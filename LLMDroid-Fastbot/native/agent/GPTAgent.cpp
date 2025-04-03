#include "GPTAgent.h"
#include <thread>
#include <chrono>
#include <fstream>
#include <algorithm>
#include "../thirdpart/json/json.hpp"
#include <atomic>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <iomanip>

using json = nlohmann::json;

namespace fastbotx {

    GPTAgent::GPTAgent(MergedStateGraphPtr& graph, PromiseIntPtr prom):
    _file("/sdcard/gpt.txt", std::ios::out | std::ios::trunc),
    _interactionFile("/sdcard/LLM-Interaction-Fastbot.txt", std::ios::out | std::ios::trunc),
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
        try {
            std::string appName = config["AppName"];
            std::string description = config["Description"];
            std::string apiKey = config["ApiKey"];
            if (config.contains("Model")) {
                _model_str = config["Model"];
                callJavaLogger(MAIN_THREAD, "Set model_str to %s", _model_str.c_str());
            }
            if (config.contains("BaseUrl")) {
                _gpt.ChatCompletion->set_base_url(config["BaseUrl"]);
                callJavaLogger(MAIN_THREAD, "Set base_url to %s", config["BaseUrl"].get<std::string>().c_str());
            }
            if (!appName.empty() && !description.empty()) {
                _startPrompt = "I'm now testing an app called " + appName + " on Android.\n" + description + "\n";
                _apiKey = apiKey;
                callJavaLogger(MAIN_THREAD, "App:%s\nDesc: %s\nkey: %s", appName.c_str(), description.c_str(), _apiKey.c_str());
            }
            else {
                callJavaLogger(MAIN_THREAD, "The value of `AppName` and `Description` are missing in json");
                exit(0);
            }
        }
        catch (const std::exception& e) {
            callJavaLogger(CHILD_THREAD, "[Exception]: %s", e.what());
            exit(0);
        }

        init();
    }

    GPTAgent::~GPTAgent()
    {
        if (_file.is_open()) {
            _file.close();
        }
        if (_interactionFile.is_open()) {
            _interactionFile.close();
        }
        return;
    }

    bool GPTAgent::init()
    {
        _gpt.auth.SetMaxTimeout(300000);
        callJavaLogger(MAIN_THREAD, "Start child thread!!!");
        std::thread child(&GPTAgent::pageAnalysisLoop, this);
        child.detach();
        return true;
    }

    void GPTAgent::pushStateToQueue(QuestionPayload payload)
    {
        if (payload.type == AskModel::REANALYSIS) {
            // need to protect _topValuedMergedState
            std::unique_lock<std::mutex> lock(_mtx); 
            int targetId = payload.from->getId();
            auto end = _topValuedMergedState->begin() + std::min(_P2 + 1ul, _topValuedMergedState->size());
            auto found = std::find_if(_topValuedMergedState->begin(), end,
                                      [targetId](MergedStatePtr& ms){
                                          return targetId == ms->getId();
                                      });
            if (found != end) {
                // std::lock_guard<std::mutex> lock(_mtx);
                std::unique_lock<std::mutex> questionCountLock(_questionMtx);
                _questionRemained++;

                this->_lowQueue.push(payload);
                callJavaLogger(MAIN_THREAD, "[MAIN] push M%d to low priority queue, remains: %d", payload.from->getId(), _lowQueue.size());
                lock.unlock();
                _cv.notify_one();
            }
        }
        else {
            {
                std::unique_lock<std::mutex> questionCountLock(_questionMtx);
                _questionRemained++;
                // Protect access to queues using mutex locks
                std::lock_guard<std::mutex> lock(_mtx);
                this->_stateQueue.push(payload);
                std::stringstream ss;
                if (payload.from) { ss << "from: MergedState" << payload.from->getId();}
                callJavaLogger(MAIN_THREAD, "[MAIN] push {%s} to high priority queue, remains: %d", ss.str().c_str(), _stateQueue.size());
            }
            _cv.notify_one();
        }
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
            //callJavaLogger(1, "[THREAD] before get lock");
            QuestionPayload payload;
            {
                std::unique_lock<std::mutex> lock(_mtx);
                if (_cv.wait_for(lock, std::chrono::seconds(1), [this]() { return !_stateQueue.empty() || !_lowQueue.empty(); })) {
                    if (!_stateQueue.empty()) {
                        payload = _stateQueue.front();
                        _stateQueue.pop();
                        callJavaLogger(CHILD_THREAD, "[THREAD]pop one payload from high priority queue, remains: %d", _stateQueue.size());
                    }
                    else if (!_lowQueue.empty()) {
                        payload = _lowQueue.front();
                        _lowQueue.pop();
                        callJavaLogger(CHILD_THREAD, "[THREAD]pop one payload from low priority queue, remains: %d", _lowQueue.size());
                    }
                }
                else {
                    continue; // No payload available, retry
                }
            } // Lock is automatically released here
            
            switch(payload.type)
            {
                case AskModel::STATE_OVERVIEW:
                {
                    askForStateOverview(payload);
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
                case AskModel::REANALYSIS:
                {
                    askForReanalysis(payload);
                    break;
                }
                default: {
                    break;
                }
            }// end switch

            //_questionRemained.fetch_sub(1);
            //std::unique_lock<std::mutex> questionCountLock2(_questionMtx);
            std::unique_lock<std::mutex> questionCountLock(_questionMtx);
            _questionRemained--;
            //questionCountLock2.unlock();
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
        std::unique_lock<std::mutex> lock(_mtx);
        
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

        nlohmann::ordered_json jsonResponse = getResponse(promptstream.str(), AskModel::STATE_OVERVIEW);

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

    void GPTAgent::askForGuiding(QuestionPayload& payload)
    {
        callJavaLogger(CHILD_THREAD, "[THREAD] ask for guiding");
        std::stringstream promptstream;
        promptstream << _startPrompt << _inputExplanationPrompt_guide;

        nlohmann::ordered_json jsonData;
        int end = (_topValuedMergedState->size() > _P2) ? _P2 : _topValuedMergedState->size();
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
                (*_topValuedMergedState)[i]->writeOverviewAndTop5Tojson(jsonData, true);
                count++;
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
        nlohmann::ordered_json jsonResponse = getResponse(promptstream.str(), AskModel::GUIDE);

        // process response
        std::string targetState = jsonResponse["Target State"];
        _targetFunction =  jsonResponse["Target Function"];
        _targetMergedStateId = std::stoi(targetState.substr(5));

        MergedStatePtr destination = _mergedStateGraph->findMergedStateById(_targetMergedStateId);
        if (!destination) {
            _promiseInt->set_value(-1);
        }
        else {
            ReuseStatePtr targetState = destination->getTargetState(_targetFunction);
            _promiseInt->set_value((targetState? targetState->getIdi(): -1));
        }   
    }

    void GPTAgent::askForTestFunction(QuestionPayload& payload)
    {
        callJavaLogger(CHILD_THREAD, "[THREAD] ask for testing function");
        std::stringstream promptstream;
        promptstream << _startPrompt << _inputExplanationPrompt_functionTest;
        // Provide a detailed description of the page (including action number)
        // To extend to mergedWidget
        std::string html = (payload.reuseState)->getStateDescription();
        promptstream << "\n```Page Description\n" 
            << html
            << "```\n";

        // Function to be tested
        promptstream << "The target function I want to test is : " << _targetFunction << "\n";

        // executed functions
        if (!_executedFunctions.empty()) {
            promptstream << "I've already I have already executed: [";
            for (int i = 0; i < _executedFunctions.size(); i++) {
                if (i != 0) { promptstream << ",\n"; }
                promptstream << _executedFunctions[i];
            }
            promptstream << "]\n";
        }
        
        // Ask which control to click
        promptstream << _requiredOutputPrompt_functionTest << "\n" << _answerFormatPrompt_functionTest;
        if (!_executedFunctions.empty()) {
            promptstream << _answerFormatPrompt_functionTestEmpty;
        }

        // ask
        nlohmann::ordered_json jsonResponse = getResponse(promptstream.str(), AskModel::TEST_FUNCTION);

        // process response
        int elementId = jsonResponse["Element Id"];
        int actionType = ActionType::CLICK + jsonResponse["Action Type"].get<int>();
        // Special handling in Fastbot where the input corresponds to the click type.
        if (jsonResponse["Action Type"].get<int>() == 6) {
            actionType = ActionType::CLICK;
        }

        if (elementId == -1) {
            _promiseAction->set_value(nullptr);
            return;
        }

        // Find action based on number
        // If the widget comes from mergedWidgets, change the target widget of the action
        // Directly return actionPtr, with inputText set, and add executed event in here, using line in html
        int actionId = payload.reuseState->findActionByElementId(elementId, actionType);
        ActivityStateActionPtr ret = nullptr;
        if (actionId == -1) {
            // _actionByGPT = state->getActions()[0];
            ret = nullptr;
            callJavaLogger(CHILD_THREAD, "LLM returns None, meaning function %s is either finished testing or can't be tested", _targetFunction.c_str());
        }
        else {
            ret = (payload.reuseState)->getActions()[actionId];
            // set inputText to action
            if (jsonResponse.contains("Input")) {
                ret->setInputText(jsonResponse["Input"].get<std::string>());
            }
            addExecutedEvent(html, elementId, ret);
        }
        _promiseAction->set_value(ret);
    }

    void GPTAgent::askForReanalysis(QuestionPayload& payload) {
        callJavaLogger(CHILD_THREAD, "Ask for Reanalysis of MergedState%d", payload.from->getId());
        std::stringstream prompt;

        prompt << _startPrompt << inputExplanationReanalysis1;
        prompt << "```Overview and Function List\n";
        nlohmann::ordered_json data = payload.from->toJson();
        prompt << data.dump(4);
        prompt << "\n```\n";

        prompt << inputExplanationReanalysis2 << "```Controls in HTML Description\n";

        // create widgetsDict
        std::unordered_map<int, WidgetInfo> widgetsDict;
        int id = 1;
        auto rootState = payload.from->getRootState();
        for (const auto& state : payload.from->getReuseStates()) {
            for (const auto& widget : state->diffWidgets(rootState)) {
                widgetsDict[id] = WidgetInfo{"", state, -1, widget};
                id++;
            }
        }

        if (widgetsDict.empty()) {
            callJavaLogger(CHILD_THREAD, "All states are exactly the same as root state, no different widgets to analysis");
            return;
        }

        // remove duplicate widgets
        std::unordered_map<std::string, std::vector<int>> uniqueWidgets;
        for (const auto& widgetPair : widgetsDict) {
            int id = widgetPair.first;
            const auto& widgetInfo = widgetPair.second;
            std::string html = widgetInfo.widget->toHTML({}, false, 0);
            if (uniqueWidgets.find(html) == uniqueWidgets.end()) {
                uniqueWidgets[html] = {id};
            } else {
                uniqueWidgets[html].push_back(id);
            }
        }

        // generate widget list in html
        for (const auto& item : uniqueWidgets) {
            int widgetId = item.second[0];
            prompt << widgetsDict[widgetId].widget->toHTML({}, true, widgetId);
        }

        prompt << "```\n";
        prompt << requiredOutputReanalysis + answerFormatReanalysis;

        nlohmann::ordered_json json_resp = getResponse(prompt.str(), AskModel::REANALYSIS);

        payload.from->updateFromReanalysis(json_resp, uniqueWidgets, widgetsDict);

    }
    
    nlohmann::ordered_json GPTAgent::getResponse(const std::string& prompt, AskModel type)
    {
        saveToFile(prompt, 0);
        callJavaLogger(CHILD_THREAD, "[THREAD]prompt:\n%s\n-----prompt end %d-----", prompt.c_str(), prompt.length());
        callJavaLogger(CHILD_THREAD, "[THREAD]Start Asking...");
        
        _conversation.AddUserData(prompt);
        double beginStamp = 0;
        double endStamp = 0;
        liboai::Response rawResponse;
        
        if (_gpt.auth.SetKey(_apiKey))
        {
            int try_times = 0;
            beginStamp = currentStamp();
            while (try_times < 5) {
                try {
                    rawResponse = _gpt.ChatCompletion->create(_model_str, _conversation, 0.0);
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
            endStamp = currentStamp();
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

        double timeCost = (endStamp - beginStamp) / 1000.0;
        nlohmann::json rawJson = rawResponse.raw_json;

        using UnderlyingType = typename std::underlying_type<AskModel>::type;
        _interactionFile << std::fixed << std::setprecision(5) <<
                timeCost << ", " <<
                _model_str << ", " <<
                rawJson["usage"]["prompt_tokens"] << ", " <<
                rawJson["usage"]["completion_tokens"] << ", " <<
                static_cast<UnderlyingType>(type) << std::endl;

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
        size_t pos = response.find('{');
        if (pos != std::string::npos) {
            response = response.substr(pos);
        }
        pos = response.rfind('}');
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

    void GPTAgent::resetPromise(PromiseIntPtr promInt, PromiseActionPtr promAction)
    {
        _promiseAction = std::move(promAction);
        _promiseInt = std::move(promInt);
    }

    void GPTAgent::addTestedFunction()
    {
        _testedFunctions.insert(_targetFunction);
        // add update tested function to mergedState(target)
        MergedStatePtr ms = _mergedStateGraph->findMergedStateById(_targetMergedStateId);
        if (ms) {
            ms->updateCompletedFunction(_targetFunction);
        }
        else {
            callJavaLogger(MAIN_THREAD, "Can't find MergedState%d when marking function(%s) as tested", _targetMergedStateId, _targetFunction.c_str());
        }
    }

    void GPTAgent::addExecutedEvent(const std::string& html, int widget_id, ActionPtr act) {
        std::istringstream stream(html);
        std::string line;
        std::string target = "id=" + std::to_string(widget_id);
        
        while (std::getline(stream, line)) {
            if (line.find(target) != std::string::npos) {
                std::istringstream line_stream(line);
                std::string cell;
                std::string last_cell;
                
                while (std::getline(line_stream, cell, '\t')) {
                    last_cell = cell;
                }
                
                _executedFunctions.push_back(act->toDescription(last_cell));
                break;
            }
        }
    }

    void GPTAgent::clearExecutedEvents() {
        _executedFunctions.clear();
    }

}

