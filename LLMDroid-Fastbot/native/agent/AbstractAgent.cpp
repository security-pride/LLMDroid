/*
*This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
*/
/**
*@authors Jianqiang Guo, Yuhui Su, Zhao Zhang
*/

#ifndef AbstractAgent_CPP_
#define AbstractAgent_CPP_

#include "AbstractAgent.h"

#include <utility>
#include "../model/Model.h"
#include "../thirdpart/json/json.hpp"

using json = nlohmann::json;

namespace fastbotx {

    AbstractAgent::AbstractAgent(const ModelPtr &model, bool useCodeCoverage)
            : _validateFilter(validDatePriorityFilter), _graphStableCounter(0),
              _stateStableCounter(0), _activityStableCounter(0), _disableFuzz(false),
              _requestRestart(false), _currentStateBlockTimes(0),
              _algorithmType(AlgorithmType::Random),
              _graph(model->getGraph()),
              _mergedStateGraph(std::make_shared<MergedStateGraph>(_graph)),
              _gptAgent(GPTAgent(_mergedStateGraph, std::move(_promiseInt))),
              _useCodeCoverage(useCodeCoverage),
              _codeCoverageMonitor(_rateCapacity, _minGrowthRate)
    {
        this->_model = model;
        _totalMergedState = 0;
        _lastGraphOverviewTime = currentStamp();
        _startTime = currentStamp();
        _nextStageTime = _startTime + _runTime;
        ////read from json
        //std::ifstream file("/sdcard/faruzan/config.json");
        ////Check if the file is opened successfully
        //if (!file.is_open()) {
        //callJavaLogger(MAIN_THREAD, "can't open config.json");
        //exit(0);
        //}
        ////Read file content into json object
        //json config;
        //file >> config;
        //file.close();
        //_rateCapacity = config["Window"];
        //_codeCoverageMonitor.setWindowSize(_rateCapacity);
        //callJavaLogger(MAIN_THREAD, "Set Window Size: %d", _rateCapacity);
    }


    //AbstractAgent::AbstractAgent(const ModelPtr &model)
    //        : AbstractAgent() {
    //    this->_model = model;
    //}

    AbstractAgent::~AbstractAgent() {
        MLOG("~AbstractAgent has been called");
        this->_model.reset();
        this->_lastState.reset();
        this->_currentState.reset();
        this->_newState.reset();
        this->_lastAction.reset();
        this->_currentAction.reset();
        this->_newAction.reset();
        this->_validateFilter.reset();

        this->_mNewAction.reset();
        this->_mCurrentAction.reset();
    }

    MergedStatePtr AbstractAgent::findMostSimilar(ReuseStatePtr state)
    {
        MergedStatePtr origin = state->getMergedState();
        if (origin) { return origin; } 
        
        //First compare with the current MergedState
        const float threshold = _maxSimilarity;
        MergedStatePtr current = _mergedStateGraph->getCurrentNode();
        if (current == nullptr) {
            return nullptr;
        }
        ReuseStatePtr rootState = current->getRootState();
        float similarity = current->getRootState()->computeSimilarity(state);
        
        // If the similarity is less than the threshold
        // Traverse all mergedStates in mergedStateGraph and calculate similarity with the root node therein
        // Choose the one with the greatest similarity to return
        if (similarity < threshold)
        {
            float maxSimilarity = 0;
            MergedStatePtr tmp = nullptr;
            for (auto ms: _mergedStateGraph->getMergedStates())
            {
                similarity = ms->getRootState()->computeSimilarity(state);
                if (similarity > threshold && similarity > maxSimilarity) {
                    maxSimilarity = similarity;
                    tmp = ms;
                }
            }
            return tmp;
        }
        else
        {
            return current;
        }

        // If both are less than the threshold, return nullptr
        return nullptr;

    }
    
    void AbstractAgent::processState(ReuseStatePtr state)
    {       
        _mCurrentState = state;
        if (_mCurrentAction) { callJavaLogger(MAIN_THREAD, "Last Action: %s\n", _mCurrentAction->toDescription().c_str()); }
        callJavaLogger(MAIN_THREAD, "State%d\n%s\n------------------\n", state->getIdi(), state->getStateDescription().c_str());
        MergedStatePtr mergedState = findMostSimilar(state);
        bool isNew = false;
        if (mergedState)
        {
            state->setMergedState(mergedState);
            
            // state belongs to the current node,
            if (mergedState == _mergedStateGraph->getCurrentNode()) {
                callJavaLogger(MAIN_THREAD, "***State%d belongs to current MergedState%d", state->getIdi(), mergedState->getId());
                mergedState->addState(state, _mCurrentAction, false, false);
            }
            //state belongs to the previous node
            else {
                callJavaLogger(MAIN_THREAD, "***State%d belongs to previous MergedState%d", state->getIdi(), mergedState->getId());
                // Add an edge to the previous MergedState to other MergedState
                MergedStatePtr preState = _mergedStateGraph->getCurrentNode();
                preState->addState(state, _mCurrentAction, false, true);
                //callJavaLogger(MAIN_THREAD,  "before mergedState->addState");
                mergedState->addState(state, _mCurrentAction, true, false);
                //callJavaLogger(MAIN_THREAD,  "before switchMode");
            }
        }
        else
        {
            isNew = true;
            MergedStatePtr preState = _mergedStateGraph->getCurrentNode();
            if (preState)
            {
                preState->addState(state, _mCurrentAction, false, true);
            }
            
            // new state
            mergedState = std::make_shared<MergedState>(state, _totalMergedState);
            _totalMergedState++;
            state->setMergedState(mergedState);
            callJavaLogger(MAIN_THREAD, "***State%d create new MergedState%d", state->getIdi(), mergedState->getId());
            // Ask about the current MergedState+action+next page
            // When entering a new page, you should pay attention to the current group of pages and the relationship between the current page and the next page.
            if (state->getActivityString()->find("com.android.") == std::string::npos) {
                GPTFunctionAnalysis({AskModel::STATE_OVERVIEW, mergedState, {}, 0, nullptr, false});
            }           
            
        }
        // automatically check whether should do graph overview and create payload
        // autoGraphOverview(mergedState);
        _mergedStateGraph->addNode(mergedState, _mCurrentAction, false);
        switchMode();
        callJavaLogger(MAIN_THREAD,  "after switchMode");
    }

    void AbstractAgent::switchMode() {
        // update code coverage every step
        double currentCodeCoverage = getCodeCoverage();
        callJavaLogger(MAIN_THREAD, "[Check] currentCodeCoverage: %f", currentCodeCoverage);
        
        if (_currentMode == Mode::EXPLORE) {
            // update threshold
            auto res = _codeCoverageMonitor.update(currentCodeCoverage);
            _currentThreshold = res.second;
            _growthRateWindow.push_back(res.first);
            if (_growthRateWindow.size() > _rateCapacity) {
                _growthRateWindow.erase(_growthRateWindow.begin());
            }

            callJavaLogger(MAIN_THREAD, "[Check] CV window size: %d, current threshold: %f", _growthRateWindow.size(), _currentThreshold);

            checkShouldWait();
            if (_shouldWait) {
                prepareForNavigation();
                return;
            }
        }

        if (_currentMode == Mode::NAVIGATE) {
            int status = guideCheck();
            if (status == 1) {
                // guide succeed at current step, but still not reach target.
                return;
            }
            else if (status == 2) {
                onNavigationOver(true);
            }
            else {
                onNavigationFailed();
                return;
            }
        }

        if (_currentMode == Mode::TEST_FUNCTION) {
            prepareTestFunction();
            return;
        }
    }

    void AbstractAgent::checkShouldWait()
    {
        bool lowGrowthRate = false;
        if (_growthRateWindow.size() == _rateCapacity) {
            callJavaLogger(MAIN_THREAD, "[Check] checking...");
            bool pass = false;
            for (double d: _growthRateWindow) {
                if (d > _currentThreshold) {
                    pass = true;
                }
            }
            if (!pass) {
                lowGrowthRate = true;
                callJavaLogger(MAIN_THREAD, "[Check] Low growth rate detected!!!");
            }
        }

        if (_useCodeCoverage) {
            callJavaLogger(MAIN_THREAD, "[Check] Using code coverage");
            if (lowGrowthRate) {
                _shouldWait = true;
            }
        }
        else {
            callJavaLogger(MAIN_THREAD, "[Check] Using time");
            double currentTime = currentStamp();
            if (currentTime > _nextStageTime) {
                _shouldWait = true;
            }
        }

    }

    void AbstractAgent::autoGraphOverview(MergedStatePtr& mergedState)
    {
            // Ask for the content of the current test every once in a while
        double currentTime = currentStamp();
        if (!_guideMode && !_functionTestMode && (currentTime - _lastGraphOverviewTime) > _graphOverviewTimeWindow)
        {
            if (_mergedStateGraph->getCurrentNode() && mergedState != _mergedStateGraph->getCurrentNode()) {
                _mergedStateQueue.push(std::make_tuple(mergedState, nullptr, false));
                _transitCount++;
            }
            _mergedStateQueue.push(std::make_tuple(_mergedStateGraph->getCurrentNode(), _mCurrentAction, true));
            _mergedStateGraph->addNode(mergedState, _mCurrentAction, true);
            // MergedStateGraph has been updated at this time
            // Executed currentAction and came to mergedState
            // So currentAction belongs to the action of the previous MergedState

            std::map<MergedStatePtr, std::set<ActionPtr>> stateMap;
            bool shouldStop = false;
            do
            {
                auto top = _mergedStateQueue.front();
                MergedStatePtr lastState = std::get<0>(top);
                ActionPtr action = std::get<1>(top);
                shouldStop = std::get<2>(top);
                if (lastState)
                {
                    // Find whether the key of lastState already exists in stateMap
                    auto it = stateMap.find(lastState);
                    if (it != stateMap.end())
                    {
                        it->second.insert(action);
                    }
                    else
                    {
                        stateMap.insert({lastState, {action}});
                    }
                }
                _mergedStateQueue.pop();

            } while (shouldStop == false);
            // create payload
            callJavaLogger(MAIN_THREAD, "create graph payload!!!");
            // GPTFunctionAnalysis({AskModel::GRAPH_OVERVIEW, nullptr, stateMap, _transitCount, nullptr, false});
            // update graph overview time
            _lastGraphOverviewTime = currentTime;
            _transitCount = 0;
        }
        else
        {
            if (_mergedStateGraph->getCurrentNode() && mergedState != _mergedStateGraph->getCurrentNode()) {
                _transitCount++;
            }
            _mergedStateQueue.push(std::make_tuple(_mergedStateGraph->getCurrentNode(), _mCurrentAction, false));
            _mergedStateGraph->addNode(mergedState, _mCurrentAction, false);

        }
    }

    void AbstractAgent::GPTFunctionAnalysis(QuestionPayload payload)
    {
        _gptAgent.pushStateToQueue(payload);
        return;
    }

    void AbstractAgent::prepareForNavigation() {
        _currentMode = Mode::NAVIGATE;
        _gptAgent.waitUntilQueueEmpty();
        debugMergedStates();

        _guideTime++;
        _totalGuideTime++;

        // create payload
        resetFuture();
        GPTFunctionAnalysis({AskModel::GUIDE, nullptr, {}, 0, nullptr, false});

        _guideTarget = _futureInt.get();
        callJavaLogger(MAIN_THREAD, "[MAIN] get guide target state: %d", _guideTarget);
        //find path
        _paths = _graph->findPath(_guideTarget, true);

        // if path not found
        if (_paths.empty()) {
            callJavaLogger(MAIN_THREAD, "[warning]: no path found from R0 to R%d", _guideTarget);
            onNavigationFailed();
        }
        else {
            _currentPath = _paths[0];
            _paths.erase(_paths.begin());
        }
    }

    void AbstractAgent::onNavigationFailed() {
        // _guideTime means: After detecting low growth rate,
        // we have already tried guide for _guideTime times
        callJavaLogger(MAIN_THREAD, "[MAIN] try guiding for %d times", _guideTime);

        if (_guideTime > 1 && _currentSimilarityCheck > _minSimilarity) {
            _currentSimilarityCheck -= 0.05;
        }

        if (!_paths.empty()) {
            _currentPath = _paths[0];
            _paths.erase(_paths.begin());
        }
        else if (_guideTime < 3) {
            // callJavaLogger(MAIN_THREAD, "[MAIN] try to guide again");
            _gptAgent.addTestedFunction();
            // TODO update function in mergedState
            callJavaLogger(MAIN_THREAD, "No path available, try to ask for new target");
            prepareForNavigation();
        }
        else {
            callJavaLogger(MAIN_THREAD, "[MAIN] guide failed too many times, GUIDE mode over");
            onNavigationOver(false);
        }

        // always restart and find path from R0 when asking gpt for guiding.
        // Each time the navigation target is asked, up to three paths to the target will be calculated.
        // If navigation fails, try other paths, if all paths fail, ask for a new destination

        
        return;
    }

    void AbstractAgent::onNavigationOver(bool success) {
        // Statistical navigation success rate
        if (success) {
            _successGuideTime++;
            _currentMode = Mode::TEST_FUNCTION;
            callJavaLogger(MAIN_THREAD, "Switch to TEST_FUNCTION mode");
        }
        else {
            prepareBackToExplore();
        }
        
        callJavaLogger(MAIN_THREAD, "[GUIDE STAT] %d/%d", _successGuideTime, _totalGuideTime);
        
        // Clear related data
        _guideTarget = -1;
        _paths.clear();
        _guideTime = 0;
        _currentSimilarityCheck = _maxSimilarity;
        // self.__current_path = None
        // self.__paths = []
        // self.__failure_in_single_round = 0
        // self.__current_similarity_check = self.max_similarity
    }

    int AbstractAgent::guideCheck()
    {
        bool isCorrect = false;
        int targetId = -1;

        while (!_currentPath.steps.empty())
        {
            Step currentStep = _currentPath.steps.front();
            targetId = currentStep.node;
            _currentPath.steps.pop();
            if (_mCurrentState->getIdi() == targetId) {
            // path correct for this step
                isCorrect = true;
                break;
            }
            // For the first page after the restart action, even if it is not State0, it is still considered correct
            // The corresponding next action needs to be converted
            else if (currentStep.action->getActionType() == ActionType::RESTART) {
                if (_currentPath.steps.empty()) {
                // The queue is empty, considered successful
                    isCorrect = true;
                    break;
                }
                // Replace front's action with the current state's action
                ActionPtr replace = _mCurrentState->findSimilarAction(_currentPath.steps.front().action);
                if (replace) {
                    callJavaLogger(MAIN_THREAD, "Found similar action in current state and replace next step");
                    // create a copy of replace
                    ActivityStateActionPtr tmp = std::dynamic_pointer_cast<ActivityStateAction>(replace);
                    _currentPath.steps.front().action = tmp ? std::make_shared<ActivityStateAction>(*(tmp.get())) : replace;
                    isCorrect = true;
                    break;
                }
            }
            else {  
                // Determine the similarity between two pages
                ReuseStatePtr targetState = _graph->findReuseStateById(targetId);
                float similarity = _mCurrentState->computeSimilarity(targetState);
                callJavaLogger(MAIN_THREAD, "Similarity between target%d and now%d is %f", targetId, _mCurrentState->getIdi(), similarity);
                
                // If the similarity is very high, it is still considered successful.
                if (similarity > _currentSimilarityCheck) {
                    if (_currentPath.steps.empty()) {
                        // The queue is empty, considered successful
                        isCorrect = true;
                        break;
                    }
                    // Replace front's action with the current state's action
                    ActionPtr replace = _mCurrentState->findSimilarAction(_currentPath.steps.front().action);
                    if (replace) {
                        callJavaLogger(MAIN_THREAD, "Found similar action in current state and replace next step");
                        // create a copy of replace 2
                        ActivityStateActionPtr tmp = std::dynamic_pointer_cast<ActivityStateAction>(replace);
                        _currentPath.steps.front().action = tmp ? std::make_shared<ActivityStateAction>(*(tmp.get())) : replace;
                        isCorrect = true;
                        break;
                    }
                    // If no matching action is found, it is still considered a failure.
                    else {
                        // The path does not match. Check whether the state of the following steps is the same as the current one, that is, check whether there is a step skip.
                        callJavaLogger(MAIN_THREAD, "[MAIN] target:%d, now at %d  try skip next step", targetId, _mCurrentState->getIdi());
                    }
                }
                else {
                    // The path does not match. Check whether the state of the following steps is the same as the current one, that is, check whether there is a step skip.
                    callJavaLogger(MAIN_THREAD, "[MAIN] target:%d, now at %d  try skip next step", targetId, _mCurrentState->getIdi());
                }
            }
        }

        if (isCorrect) {
            if (_currentPath.steps.empty()) {
                callJavaLogger(MAIN_THREAD, "[MAIN] successfully guide to R%d, GUIDE mode over, switch to FUNCTION TEST mode", targetId);
                return 2;
            }
            else {
                callJavaLogger(MAIN_THREAD, "[MAIN] guide succeed at this step");
                return 1;
            }
        }
        else {
            // guide failure
            callJavaLogger(MAIN_THREAD, "[MAIN] guide failed. target:%d, now at %d", targetId, _mCurrentState->getIdi());
            return 3;
        }
    }

    void AbstractAgent::prepareBackToExplore() {
        callJavaLogger(MAIN_THREAD, "prepare back to EXPLORE mode");
        _currentMode = Mode::EXPLORE;

        // 'checkShouldWait' related
        _nextStageTime = _runTime + currentStamp();
        _lastGraphOverviewTime = currentStamp();
        _growthRateWindow.clear();
        _shouldWait = false;

        // consider the function is tested whether succeed or not
        // MergedStatePtr ms = state->getMergedState();
        // ms->updateCompletedFunction(_gptAgent.getFunctionToTest());
        _gptAgent.addTestedFunction();

        _executedSteps = 0;
        _gptAgent.clearExecutedEvents();

        // reanalyze mergedStates
        for (MergedStatePtr ms: _mergedStateGraph->getMergedStates()) {
            if (ms->needReanalysed()) {
                GPTFunctionAnalysis(QuestionPayload{AskModel::REANALYSIS, ms});
            }
        }
    }
    
    void AbstractAgent::onAddNode(StatePtr node) {
        _newState = node;

        if(BLOCK_STATE_TIME_RESTART != -1)
        {
            if (equals(_newState, _currentState)) {
                this->_currentStateBlockTimes++;
            } else {
                this->_currentStateBlockTimes = 0;
            }
        }
    }

    void AbstractAgent::moveForward(StatePtr nextState) {
        // update state
        _lastState = _currentState;
        _currentState = _newState;
        _newState = std::move(nextState);
        // update action
        _lastAction = _currentAction;
        _currentAction = _newAction;
        _newAction = nullptr;
    }

    void AbstractAgent::moveForward2() {
        _mCurrentAction = _mNewAction;
        _mNewAction = nullptr;
    }

    void AbstractAgent::adjustActions() {
        double totalPriority = 0;           
        for (const ActivityStateActionPtr &action: _newState->getActions()) {
            // click has priority of 4, other priority is 2, why?
            int basePriority = action->getPriorityByActionType();
            action->setPriority(basePriority);
            if (!action->requireTarget()) {
                if (!action->isVisited()) {
                    int priority = action->getPriority();
                    priority += 5;
                    action->setPriority(priority);
                }
                continue;
            }
            if (!action->isValid()) {
                continue;
            }
            int priority = action->getPriority();
            if (!action->isVisited()) {
                priority += 20;         
            }
            if (!this->_newState->isSaturated(action))          
            {
                priority += 5 * action->getPriorityByActionType();
            }

            if (priority <= 0) {
                priority = 0;
            }

            action->setPriority(priority);          
            totalPriority += (priority - action->getPriorityByActionType());
        }
        _newState->setPriority((int) totalPriority);
    }

    ActionPtr AbstractAgent::resolveNewAction() {
        //update priority
        this->adjustActions();

        if (_currentMode == Mode::NAVIGATE) {
            if (!_currentPath.steps.empty()) {
                // convert the copy to real one
                ActionPtr nextAction = _currentPath.steps.front().action;
                ActivityStateActionPtr tmp = std::dynamic_pointer_cast<ActivityStateAction>(nextAction);
                ActionPtr action = tmp ? _mCurrentState->findSimilarAction(nextAction) : nextAction;
                // debug
                if (!action) {
                    callJavaLogger(MAIN_THREAD, "failed when convert the copy to real one: %s\n",
                                _currentPath.steps.front().action->toDescription().c_str());
                    exit(0);
                }

                _mNewAction = action;
                return action;
            }
            else {
                callJavaLogger(MAIN_THREAD, "In NAVIGATE mode, but current path's steps is empty, this isn't supposed to happen");
                exit(0);
            }
        }

        if (_currentMode == Mode::TEST_FUNCTION) {
            if (_actionByGPT) {
                _newAction = _actionByGPT;
                _mNewAction = std::dynamic_pointer_cast<Action>(_actionByGPT);
                callJavaLogger(MAIN_THREAD, "About to execute event chosen by llm");
                return _mNewAction;
            }
            else {
                callJavaLogger(MAIN_THREAD, "event by llm is None, back to EXPLORE mode");
                prepareBackToExplore();
            }
        }

        ActionPtr action = this->selectNewAction();
        _newAction = std::dynamic_pointer_cast<ActivityStateAction>(action);
        _mNewAction = action;
        
        return action;
    }

    ActivityStateActionPtr AbstractAgent::handleNullAction() const {
        ActivityStateActionPtr action = this->_newState->randomPickAction(this->_validateFilter);
        if (nullptr != action) {
            ActivityStateActionPtr resolved = this->_newState->resolveAt(action,
                                                                         this->_model.lock()->getGraph()->getTimestamp());
            if (nullptr != resolved) {
                return resolved;
            }
        }
        BDLOGE("handle null action error!!!!!");
        return nullptr;
    }

    void AbstractAgent::debugMergedStates()
    {
        nlohmann::json jsonData;
        for (auto ms: _mergedStateGraph->getMergedStates())
        {
            std::string key = "MergedState" + std::to_string(ms->getId());
            jsonData[key]["RootState"] = "R" + std::to_string(ms->getRootState()->getIdi());
            std::vector<std::string> stateList;
            auto it = ms->getReuseStates();
            std::transform(it.begin(), it.end(), std::back_inserter(stateList),
                [](const ReuseStatePtr& ptr) { 
                    return "State" + std::to_string(ptr->getIdi());
                });
            jsonData[key]["States"] = stateList;
            jsonData[key]["overview"] = ms->getOverview();
        }
        std::string str = jsonData.dump(4);
        callJavaLogger(MAIN_THREAD, "[DEBUG] save MergedState to file");

        std::ofstream file("/sdcard/MergedState.txt", std::ios::out | std::ios::trunc);
        if (file.is_open()) {
            file << str << std::endl;
        }
    }

    void AbstractAgent::prepareTestFunction()
    {
        if (_executedSteps < 5) {
            _executedSteps++;

            resetFuture();
            ReuseStatePtr state = std::dynamic_pointer_cast<ReuseState>(_newState);          
            GPTFunctionAnalysis({AskModel::TEST_FUNCTION, nullptr, {}, 0, state, false});

            _actionByGPT = _futureAction.get();           
        }
        else {
            _actionByGPT = nullptr;
            callJavaLogger(MAIN_THREAD, "TEST FUNCTION for over 5 steps, quit!");
        }
        
    }

    void AbstractAgent::resetFuture()
    {
        PromiseIntPtr promInt = std::make_shared<std::promise<int>>();
        _futureInt = promInt->get_future();
        PromiseActionPtr promAction = std::make_shared<std::promise<ActivityStateActionPtr>>();
        _futureAction = promAction->get_future();
        _gptAgent.resetPromise(promInt, promAction);
    }

    double AbstractAgent::getCodeCoverage() {
        jdouble rate = jnienv->CallStaticDoubleMethod(codeCoverageClass, getCoverageMethod);
        return rate;
    }

}

#endif          
