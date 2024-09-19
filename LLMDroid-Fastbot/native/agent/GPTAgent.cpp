#include "GPTAgent.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <algorithm>
#include "../thirdpart/json/json.hpp"
#include <atomic>
#include <stdexcept>

using json = nlohmann::json;

namespace fastbotx {

    GPTAgent::GPTAgent(MergedStateGraphPtr& graph, PromiseIntPtr prom):
    _file("/sdcard/gpt.txt", std::ios::out | std::ios::trunc),
    _questionRemained(0)
    {
        _mergedStateGraph = graph;
        _promiseInt = std::move(prom);
        // read from json
        std::ifstream file("/sdcard/faruzan/config.json");
        // Check if the file is opened successfully
        if (!file.is_open()) {
            callJavaLogger(MAIN_THREAD, "can't open config.json");
            exit(0);
        }
        // Read file content into json object
        json config;
        file >> config;
        file.close();
        // Read field value
        std::string appName = config["AppName"];
        std::string description = config["Description"];
        std::string apiKey = config["ApiKey"];
        if (!appName.empty() && !description.empty()) {
            _startPrompt = "I'm now testing an app called " + appName + " on Android.\n" + description + "\n";
            _apiKey = apiKey;
            callJavaLogger(MAIN_THREAD, "App:%s\nDesc: %s\nkey: %s", appName.c_str(), description.c_str(), _apiKey.c_str());
        }
        else {
            callJavaLogger(MAIN_THREAD, "empty json");
            exit(0);
        }
        init();
    }

    GPTAgent::~GPTAgent()
    {
        if (_file.is_open()) {
            _file.close();
        }
        return;
    }

    bool GPTAgent::init()
    {
        _gpt.auth.SetMaxTimeout(300000);
        callJavaLogger(0, "Start child thread!!!");
        std::thread child(&GPTAgent::pageAnalysisLoop, this);
        child.detach();
        return true;
    }

    void GPTAgent::pushStateToQueue(QuestionPayload payload)
    {
        {
            // Protect access to queues using mutex locks
            std::lock_guard<std::mutex> lock(_mtx);
            this->_stateQueue.push(payload);
            std::stringstream ss;
            if (payload.from) { ss << "from: MergedState" << payload.from->getId();}
            callJavaLogger(0, "[MAIN] push {%s} to queue, remains: %d", ss.str().c_str(), _stateQueue.size());
        }
        // Notify the waiting thread that an element has been added to the queue
        _cv.notify_one();
    }

    void GPTAgent::waitUntilQueueEmpty()
    {
        callJavaLogger(MAIN_THREAD, "[MAIN] wait until queue is empty");
        int questionRemained = 0;
        while(true)
        {
            std::unique_lock<std::mutex> questionCountLock(_questionMtx);
            questionRemained = _questionRemained;//_questionRemained.load();
            questionCountLock.unlock();

            if (questionRemained == 0) {
                callJavaLogger(MAIN_THREAD, "[MAIN] question all done");
                break;
            }
            else {
                callJavaLogger(MAIN_THREAD, "[MAIN] question remains: %d, sleep 3 seconds", questionRemained);
                std::this_thread::sleep_for(std::chrono::seconds(3));
            }
        }
        
    }

    void GPTAgent::pageAnalysisLoop()
    {
        while (true)
        {
            callJavaLogger(1, "[THREAD] before get lock");
            std::unique_lock<std::mutex> lock(_mtx); // Use unique_lock to support waiting on condition variables
            _cv.wait(lock, [this](){ return !_stateQueue.empty(); }); // Waiting queue is not empty
            QuestionPayload payload = _stateQueue.front();
            _stateQueue.pop();
            //std::stringstream ss;
            //if (payload.from) { ss << "from: MergedState" << payload.from->getId();}
            callJavaLogger(1, "[THREAD]pop one payload from queue, remains: %d", _stateQueue.size());
            lock.unlock();
            
            //_questionRemained.fetch_add(1);
            std::unique_lock<std::mutex> questionCountLock(_questionMtx);
            _questionRemained++;
            questionCountLock.unlock();
            
            switch(payload.type)
            {
                case AskModel::STATE_OVERVIEW:
                {
                    askForStateOverview(payload);
                    break;
                }
                case AskModel::GRAPH_OVERVIEW:
                {
                    askForGraphOverview(payload);
                    break;
                }
                case AskModel::GUIDE:
                {
                    askForGuiding(payload);
                    break;
                }
                case AskModel::TEST_FUNCTION:
                {
                    askForTestFunction(payload);
                    break;
                }
                default: {
                    break;
                }
            }// end switch

            //_questionRemained.fetch_sub(1);
            std::unique_lock<std::mutex> questionCountLock2(_questionMtx);
            _questionRemained--;
            questionCountLock2.unlock();
        }        
    }

    void GPTAgent::saveToFile(const std::string& prompt, const std::string& response)
    {
        if (_file.is_open()) {
            _file << "---------------------------------------" << std::endl;
            _file << "Prompt:\n" << prompt << std::endl;
            _file << "Response:\n" << response << std::endl;
            _file << "---------------------------------------" << std::endl;
            callJavaLogger(CHILD_THREAD, "Saved to /sdcard/gpt.txt");
        }
    }

    void GPTAgent::saveToFile(const std::string& value, int type)
    {
        if (_file.is_open()) {
            _file << "---------------------------------------" << std::endl;
            if (type == 0) {
                _file << "Prompt:\n" << value << std::endl;
            }
            else {
                _file << "Response:\n" << value << std::endl;
            }           
            _file << "---------------------------------------" <<  value.length() << std::endl;
            callJavaLogger(CHILD_THREAD, "Saved to /sdcard/gpt.txt");
        }
    }


    void GPTAgent::askForStateOverview(QuestionPayload& payload)
    {
        if (!payload.from) 
        {
            callJavaLogger(CHILD_THREAD, "[THREAD] payload.from is null, skip");
            return;
        }
        callJavaLogger(CHILD_THREAD, "[THREAD] ask for MergedState's overview and funtion list");

        std::stringstream promptstream;
        promptstream << _startPrompt << _functionExplanationPrompt << _inputExplanationPrompt_state;
        // If a new state has been added to the merged state here, it will be asked along with the new one.
        promptstream << "\n```HTML Description\n";
        std::string stateDesc = payload.from->stateDescription();
        if (stateDesc.length() > 7000) {
            stateDesc = safe_utf8_substr(stateDesc, 0, 7000);
        }
        promptstream << stateDesc << "```\n";

        if (_topValuedMergedState->size() >= 5) {
            // ask gpt to maintain the M list
            promptstream << _requiredOutputPrompt_state3;
            // M list
            nlohmann::ordered_json top5;
            int count = 0;
            for (auto it = _topValuedMergedState->begin(); it < _topValuedMergedState->end() && count < 5; ++it) {
                // M: overview, top 5 function to json
                if ((*it)->hasUntestedFunctions()) {
                    (*it)->writeOverviewAndTop5Tojson(top5);
                    count++;
                }

            }
            promptstream << "Current: State" << payload.from->getId() << "\n";
            promptstream << "Five other pages:\n" << top5.dump(4) << "\n";
            promptstream << _requiredOutputPrompt_state_summary3 << _anwserFormatPrompt_state3;
        }
        else {
            promptstream << _requiredOutputPrompt_state2 <<  _requiredOutputPrompt_state_summary2 << _anwserFormatPrompt_state2;
        }

        nlohmann::ordered_json jsonResponse = getResponse(promptstream.str(), GPT_4);

        // process response
        payload.from->updateFromStateOverview(jsonResponse);
        if (_topValuedMergedState->size() >= 5) {
            // update M list from response
            std::vector<int> topList;
            std::string key = jsonResponse.contains("Top5") ? "Top5" : "Top 5";
            try {
                topList = jsonResponse[key].get<std::vector<int>>();
            }
            catch (const std::exception& e) {
                callJavaLogger(CHILD_THREAD, "\t\t\t\t[WARNING] GPT's response is not an int list, try to resolve as string list");
                std::vector<std::string> strs = jsonResponse[key];
                for (auto str: strs) {
                    topList.push_back(std::stoi(str.substr(5)));
                }
            }

            // Store the first 5 elements of _topValuedMergedState
            std::vector<MergedStatePtr> originalFirstFive(_topValuedMergedState->begin(), _topValuedMergedState->begin() + 5);
            // Replace the first 5 elements of _topValuedMergedState with elements from topList
            for (size_t i = 0; i < topList.size(); ++i) {
                MergedStatePtr mergedState = _mergedStateGraph->findMergedStateById(topList[i]);
                if (mergedState) {
                    (*_topValuedMergedState)[i] = mergedState;
                }
            }
            // Find elements in originalFirstFive that are not in topList
            std::vector<MergedStatePtr> elementsToInsert;
            for (const auto& elem : originalFirstFive) {
                int id = elem->getId();
                if (std::find(topList.begin(), topList.end(), id) == topList.end()) {
                    elementsToInsert.push_back(elem);
                }
            }
            // Insert the elementsToInsert after the 5th element of _topValuedMergedState
            _topValuedMergedState->insert(_topValuedMergedState->begin() + 5, elementsToInsert.begin(), elementsToInsert.end());

            /*for (int i = 0; i < topList.size(); i++) {
                if (topList[i] == (*_topValuedMergedState)[i]->getId()) { continue; }
                MergedStatePtr mergedState = _mergedStateGraph->findMergedStateById(topList[i]);
                MergedStatePtr tmp = (*_topValuedMergedState)[i];
                (*_topValuedMergedState)[i] = mergedState;
                _topValuedMergedState->insert(_topValuedMergedState->begin() + 4 , tmp);
            }*/
        }
        else {
            _topValuedMergedState->push_back(payload.from);
        }
        callJavaLogger(CHILD_THREAD, "askForStateOverview complete!");
    }

    void GPTAgent::askForGraphOverview(QuestionPayload& payload)
    {
        callJavaLogger(CHILD_THREAD, "[THREAD] ask for recent completed functions");
        // Provides visited MergedStates for this time period
        // Each MergedStates contains overview, actions executed during the time period, and function list
        std::stringstream promptstream;
        promptstream << _startPrompt << _inputExplanationPrompt_graphoverview;
        json jsonObject;
        for (auto it: payload.stateMap)
        {
            std::string key = "State" + std::to_string(it.first->getId());
            jsonObject[key]["Overview"] = it.first->getOverview();
            jsonObject[key]["Function List"] = it.first->getFunctionList();

            std::vector<std::string> actionStr;
            std::transform(it.second.begin(), it.second.end(), std::back_inserter(actionStr),
                [](const ActionPtr& ptr) { 
                    return ptr ? ptr->toDescription() : std::string("");
                });
            jsonObject[key]["Actions"] = actionStr;
        }
        
        promptstream << "```State Informations\n" << jsonObject.dump(4) << "\n```\n";
        //callJavaLogger(CHILD_THREAD, "[THREAD] after generate state informations\n%s\n", promptstream.str().c_str());

        //callJavaLogger(CHILD_THREAD, "[THREAD] current%d", _mergedStateGraph->getCurrentNode()->getId());
        std::string utg = _mergedStateGraph->temporalWalk(payload.transitCount);
        _mergedStateGraph->appendUtgString(utg);
        promptstream << "\n```UTG\n" << utg << "```\n";
        callJavaLogger(CHILD_THREAD, "[THREAD] after temporalWalk");

        promptstream << _requiredOutputPrompt_graph << _answerFormatPrompt_graph;
        
        std::string prompt = promptstream.str();
        std::string response = getResponse(prompt, GPT_3);

        // process response
        // update every MergedState's function list
        json jsonResponse;
        try {
            jsonResponse = json::parse(response);
        }
        catch (nlohmann::json::parse_error& e) {
            callJavaLogger(CHILD_THREAD, "[Exception] %s, ask for response again", e.what());
            response = getResponse(prompt, GPT_3);
            jsonResponse = json::parse(response);
        }

        for (auto it = jsonResponse.begin(); it != jsonResponse.end(); ++it) {
            int id = std::stoi(it.key().substr(5));
            MergedStatePtr state = _mergedStateGraph->findMergedStateById(id);
            if (state) {
                state->updateCompletedFunctions(it.value()["Function List"].get<std::map<std::string, int>>());
            }
        }

        return;
    }

    void GPTAgent::askForGuiding(QuestionPayload& payload)
    {
        callJavaLogger(CHILD_THREAD, "[THREAD] ask for guiding");
        std::stringstream promptstream;
        promptstream << _startPrompt << _inputExplanationPrompt_guide;

        // update navigation value
        /*auto mergedStates = _mergedStateGraph->getMergedStates();
        size_t total = mergedStates.size();
        for (auto it: mergedStates) {
            it->updateNavigationValue(total);
        }*/

        // sort by value
        /*std::vector<MergedStatePtr> vec;
        vec.assign(mergedStates.begin(), mergedStates.end());
        std::sort(vec.begin(), vec.end(), [](MergedStatePtr a, MergedStatePtr b){
            return a->getNavigationValue() > b->getNavigationValue();
        });*/

        nlohmann::ordered_json jsonData;
        int end = (_topValuedMergedState->size() > 8) ? 8 : _topValuedMergedState->size();
        int count = 0;
        for (int i = 0; i < _topValuedMergedState->size(); i++) {
            if ((*_topValuedMergedState)[i]->hasUntestedFunctions()) {
                (*_topValuedMergedState)[i]->writeOverviewAndTop5Tojson(jsonData);
                count++;
            }
            if (count >= end) {
                break;
            }
        }
        count = 0;
        if (jsonData.empty()) {
            for (int i = 0; i < _topValuedMergedState->size(); i++) {
                if ((*_topValuedMergedState)[i]->hasUntestedFunctions()) {
                    (*_topValuedMergedState)[i]->writeOverviewAndTop5Tojson(jsonData, true);
                    count++;
                }
                if (count >= end) {
                    break;
                }
            }
        }
        promptstream << "\n```State Informations\n" << jsonData.dump(4) << "\n```\n";

        // tested function
        promptstream << _requiredOutputPrompt_guide_part1 << "{";
        for (auto it: _testedFunctions) {
            promptstream << it << ", ";
        }
        promptstream << "}" << _requiredOutputPrompt_guide_part2;
        promptstream << _answerFormatPrompt_guide;

        // ask
        nlohmann::ordered_json jsonResponse = getResponse(promptstream.str(), GPT_4);

        // process response
        std::string targetState = jsonResponse["Target State"];
        _targetFunction =  jsonResponse["Target Function"];
        int targetId = std::stoi(targetState.substr(5));


        _promiseInt->set_value(targetId);
    }

    void GPTAgent::askForTestFunction(QuestionPayload& payload)
    {
        callJavaLogger(CHILD_THREAD, "[THREAD] ask for testing function");
        std::stringstream promptstream;
        promptstream << _startPrompt << _inputExplanationPrompt_functionTest;
        // Provide a detailed description of the page (including action number)
        // To extend to mergedWidget
        promptstream << "\n```Page Description\n" 
            << (payload.reuseState)->getStateDescription()
            << "```\n";

        // Function to be tested
        promptstream << "The function I want to test is : " << _targetFunction << "\n";

        // Ask which control to click
        promptstream << _requiredOutputPrompt_functionTest << "\n" << _answerFormatPrompt_functionTest;

        // ask
        nlohmann::ordered_json jsonResponse = getResponse(promptstream.str(), GPT_4);

        // process response
        int elementId = jsonResponse["Element Id"];
        int actionType = ActionType::CLICK + jsonResponse["Action Type"].get<int>();

        if (elementId == -1) {
            _promiseInt->set_value(-1);
            return;
        }

        // Find action based on number
        // If the widget comes from mergedWidgets, set the target of the action
        // Return the id of the action in actions
        int ret = payload.reuseState->findActionByElementId(elementId, actionType);

        _promiseInt->set_value(ret);

    }

    nlohmann::ordered_json GPTAgent::getResponse(std::string prompt, int type)
    {
        //std::this_thread::sleep_for(std::chrono::seconds(5));
        saveToFile(prompt, 0);
        callJavaLogger(CHILD_THREAD, "[THREAD]prompt:\n%s\n-----prompt end %d-----", prompt.c_str(), prompt.length());
        callJavaLogger(CHILD_THREAD, "[THREAD]Start Asking...");
        //std::this_thread::sleep_for(std::chrono::seconds(3));
        //callJavaLogger(1, "[THREAD]Get response");
        
        _conversation.AddUserData(prompt);
        
        if (_gpt.auth.SetKey(_apiKey))
        {
            int try_times = 0;
            while (try_times < 5) {
                try {
                    liboai::Response rawResponse = _gpt.ChatCompletion->create(_model_str[type], _conversation, 0.0);
                    bool success = _conversation.Update(rawResponse);
                    if (success) { break; }
                } catch (const std::exception& e) {
                    // Catch any exception from std::exception and its derived classes
                    callJavaLogger(CHILD_THREAD, "[Exception]: %s", e.what());
                    // try again
                    callJavaLogger(CHILD_THREAD, "\t\t\t\t[WARNING] GPT chat got an exception, try to ask again in 3 seconds");
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                    try_times++;
                }
            }
            if (try_times == 5) {
                callJavaLogger(CHILD_THREAD, "[ERROR]: error when getting GPT's response");
                exit(0);
            }
            
        }
        else
        {
            callJavaLogger(CHILD_THREAD, "!!!Set key failed!!!");
            exit(0);
        }
        
        _cachedConversation++;
        std::string response = _conversation.GetLastResponse();
        callJavaLogger(1, "[THREAD]Get response\n%s\n", response.c_str());


        if (_saveToFile){
            saveToFile(response, 1);
        }

        if (_cachedConversation > _maxCachedConversation) {
            if (_conversation.PopFirstUserData() && _conversation.PopFirstResponse()) {
                callJavaLogger(CHILD_THREAD, "[THREAD] Reach max cached conversation, pop the earliest one");
                _cachedConversation--;
            }
            else {
                callJavaLogger(CHILD_THREAD, "[THREAD] Pop conversation failed!");
            }            
        }

        // Intercept the following string starting from the "{" character
        size_t pos = response.find("{");
        if (pos != std::string::npos) {
            response = response.substr(pos);
        }
        pos = response.rfind("}");
        if (pos != std::string::npos) {
            response = response.substr(0, pos + 1);
        }

        nlohmann::ordered_json jsonResponse;
        try {
            jsonResponse = nlohmann::ordered_json::parse(response);
        }
        catch (nlohmann::json::parse_error& e) {
            callJavaLogger(CHILD_THREAD, "[Exception] %s, ask for response again", e.what());
            return getResponse(prompt, type);
        }
        return jsonResponse;
    }

    void GPTAgent::resetPromise(PromiseIntPtr prom)
    {
        _promiseInt = prom;
    }

    void GPTAgent::addTestedFunction()
    {
        _testedFunctions.insert(_targetFunction);
    }

}