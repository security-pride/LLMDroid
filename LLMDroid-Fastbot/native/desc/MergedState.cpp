#ifndef MergedState_CPP_
#define MergedState_CPP_

#include "MergedState.h"


using json = nlohmann::json;

namespace fastbotx {

    MergedStateGraphEdge::MergedStateGraphEdge(MergedStatePtr next, ActionPtr action, bool shouldStop)
    {
        _next = next;
        _action = action;
        _hash = action->hash() | next->hash();
        _isVisited = false;
        _shouldStop = shouldStop;
    }

    MergedState::MergedState(ReuseStatePtr state, int id)
    {
        _states.insert(state);
        _root = state;
        _cursor = state;
        _id = id;
        _starts.push_back(state);
        // use root state's hash
        // every MergedState's root state is unique
        _hashcode = state->hash();
    }

    void MergedState::addState(ReuseStatePtr state, ActionPtr action, bool fromOutside, bool toOutside)
    {
        // lock
        std::lock_guard<std::mutex> lock(_mergedStateMutex);
        
        // add to mini graph
        if (fromOutside) {
            _starts.push_back(state);
        }
        
        _cursor->_miniEdges.push_back({state, action, false});
        
        if (!toOutside) {
            auto success = _states.insert(state);
            _cursor = state;
            if (success.second && !_functionList.empty()) {
                _needReanalysed = true;
                updateLaterJoinedState(state);
            }
        }
        
    }

    void MergedState::addEdge(MergedStateGraphEdgePtr edge)
    {
        std::lock_guard<std::mutex> lock(_mergedStateMutex);
        _edges.push_back(edge);
        
    }

    void MergedState::addPrevious(MergedStatePtr state)
    {
        std::lock_guard<std::mutex> lock(_mergedStateMutex);
        _previous.insert(state);
    }

    void MergedState::addNext(MergedStatePtr state)
    {
        std::lock_guard<std::mutex> lock(_mergedStateMutex);
        _next.insert(state);
    }

    std::string MergedState::stateDescription()
    {
        // lock
        std::lock_guard<std::mutex> lock(_mergedStateMutex);

        std::stringstream ss;
        //ss << "[Root State" << _root->getIdi() << "]:\n";
        ss << "[Activity: " << *(_root->getActivityString()) << "]\n";
        ss << _root->getStateDescription();
        /*for (ReuseStatePtr state: _states)
        {
            if (state != _root) {
                ss << "[State" << state->getIdi() << "]:\n";
                ss << state->diffWidgets(_root);
                ss << "\n";
            }
        }*/
        //callJavaLogger(CHILD_THREAD, "[THREAD] state desc:\n%s\n", ss.str().c_str());
        return ss.str();
    }

    std::string MergedState::walk()
    {
        // lock
        std::lock_guard<std::mutex> lock(_mergedStateMutex);
        
        std::stringstream graphStream;
        // Take advantage of the feature that the in-degree of each node in the graph is equal to the out-degree
        // Simply continue searching for unvisited edges to complete the traversal
        // The advantage is that starting from one node each time can form a complete traversal chain
        // The disadvantage is that there is no guarantee that the order of traversal is consistent with the order in which the graph was originally created, and if there are repeated edges, the output will be repeated.
        for (ReuseStatePtr cursor: _starts)
        {
            graphStream << "State" << cursor->getIdi();
            MiniGraphEdge* edge = cursor->getUnvisitedMiniEdge();
            while(edge && cursor->getMergedState()->getId() == _id)
            {
                std::string actionstr = edge->action ? edge->action->toDescription() : "null";
                graphStream << " -- " << actionstr << " --> ";
                graphStream << "State" << edge->next->getIdi();
                cursor = edge->next;
                edge->isVisited = true;
                edge = cursor->getUnvisitedMiniEdge();
            }
            graphStream << "\n";
        }
        reset();
        //callJavaLogger(CHILD_THREAD, "[THREAD] mini graph:\n%s\n", graphStream.str().c_str());
        return graphStream.str();
    }

    void MergedState::reset()
    {
        for (ReuseStatePtr state: _states)
        {
            for (MiniGraphEdge& edge: state->_miniEdges)
            {
                edge.isVisited = false;
            }
        }
    }

    /*void MergedState::updateInfo(std::string& response)
    {
        std::lock_guard<std::mutex> lock(_mergedStateMutex);
        
        // extract result from response
        json jsonData = json::parse(response);
        std::string overview = jsonData["Overview"];
        std::vector<std::string> functionList = jsonData["Function List"];
        std::vector<std::string> completedFunctions = jsonData["Completed Functions"];
        std::string currentFunction = jsonData["Current Function"];
        bool isCompleted = jsonData["IsCompleted"];

        _overview = overview;

        for (std::string function: functionList)
        {
            auto it = _functionList.find(function);
            if (it == _functionList.end()) {
                _functionList.insert(std::make_pair(function, false));
            }
        }

        for (std::string function: completedFunctions)
        {
            auto it = _functionList.find(function);
            if (it != _functionList.end()) {
                it->second = true;
            }
            else {
                _functionList.insert(std::make_pair(function, true));
            }
        }

        auto it = _functionList.find(currentFunction);
        if (it != _functionList.end()) {
                it->second = isCompleted;
            }
        else {
            _functionList.insert(std::make_pair(currentFunction, isCompleted));
        }

        return;
    }*/

    void MergedState::updateFromStateOverview(nlohmann::ordered_json &jsonData)
    {
        std::lock_guard<std::mutex> lock(_mergedStateMutex);

        std::string overview = jsonData["Overview"];
        std::vector<std::pair<std::string, int>> functionList;
        nlohmann::ordered_json jsonFunctionList = jsonData["Function List"];
        for (auto it = jsonFunctionList.begin(); it != jsonFunctionList.end(); ++it) {
            if (it.value().is_number()) {
                functionList.push_back(std::make_pair(it.key(), it.value()));
                callJavaLogger(CHILD_THREAD, "[Number]%s", it.key().c_str());
            }
            else if (it.value().is_array()) {
                functionList.push_back(std::make_pair(it.key(), it.value().front()));
                callJavaLogger(CHILD_THREAD, "[Vector]%s", it.key().c_str());
            }
            else {
                callJavaLogger(CHILD_THREAD, "[ErrorType]%s", it.key().c_str());
            }
        }

        _overview = overview;

        int size = functionList.size();
        for (int i = 0; i < size; i++) {
            auto it = _functionList.find(functionList[i].first);
            if (it == _functionList.end()) {
                _functionList.insert(std::make_pair(functionList[i].first, FunctionDetail{size - i, _root}));
            }
        }

        filterFunctionList();

        updateNavigationCount();

        setFunctionToWidget(functionList);
        callJavaLogger(CHILD_THREAD, "setFunctionToWidget complete!");
        // set listener for actions
        // update _functionList according to action's visit count
        updateCompletedFunctions2();
        callJavaLogger(CHILD_THREAD, "updateFromStateOverview complete!");
        return;
    }

    void MergedState::updateCompletedFunctions(std::map<std::string, int> completedFunctions)
    {
        for (auto it: completedFunctions)
        {
            auto found = _functionList.find(it.first);
            if (found != _functionList.end()) {
                found->second.importance = it.second;
            }
            else {
                _functionList.insert(std::make_pair(it.first, FunctionDetail{it.second, _root}));
            }
        }
    }

    MergedStateGraphEdgePtr MergedState::getUnvisitedEdge()
    {
        std::lock_guard<std::mutex> lock(_mergedStateMutex);
        for (auto edge: _edges)
        {
            if (edge->_isVisited == false)
            {
                return edge;
            }
        }
        return nullptr;
    }

    void MergedState::updateCompletedFunction(std::string func)
    {
        auto it = _functionList.find(func);
        if (it == _functionList.end()) {
            _functionList.insert(std::make_pair(func, FunctionDetail{0, _root}));
        }
        else {
            (*it).second.importance = 0;
        }
    }

    void MergedState::updateNavigationValue(int total)
    {
        int weight = total - _id;
        _navigationValue = weight * _navigationCount;
    }

    void MergedState::filterFunctionList() {
        std::set<std::string> keyToDelete;

        // First, traverse all keys, find the keys wrapped in **, and extract their original forms and add them to the set.
        for (const auto& pair : _functionList) {
            size_t startPos = pair.first.find("**");
            if (startPos != std::string::npos) {
                size_t endPos = pair.first.rfind("**");
                if (endPos != std::string::npos && endPos > startPos) {
                    std::string strippedKey = pair.first.substr(startPos + 2, endPos - startPos - 2);
                    if (_functionList.find(strippedKey) != _functionList.end()) {
                        keyToDelete.insert(strippedKey);
                    }
                }
            }
        }

        for (auto it: keyToDelete) {
            _functionList.erase(it);
        }

    }

    void MergedState::updateNavigationCount() {
        int navigateFunctionNum = 0;
        for (auto it: _functionList) {
            if(it.first.find("navigate") != std::string::npos) {
                navigateFunctionNum++;
            }
        }
        _navigationCount = navigateFunctionNum;
    }

    void MergedState::updateCompletedFunctions2() {
        for (ReuseStatePtr state: _states) {
            for (ActivityStateActionPtr action: state->getActions()) {
                action->setListener(shared_from_this());
                updateCompletedFunction2(CHILD_THREAD, action);
            }
        }
    }

    void MergedState::setFunctionToWidget(const std::vector<std::pair<std::string, int>>& functionList) {

        for (int i = 0; i < functionList.size(); i++) {
            if (_functionList.find(functionList[i].first) == _functionList.end()) {
                continue;
            }
            // find element
            ElementPtr element = _root->findElementById(functionList[i].second);
            if (!element) {
                callJavaLogger(CHILD_THREAD, "can't find element%d to function%s", functionList[i].second, functionList[i].first.c_str());
                continue;
            }
            // update function to widget
            WidgetPtr widget = element->getWidget();
            std::string function = functionList[i].first;
            widget->setFunction(function);
            callJavaLogger(CHILD_THREAD, "successfully set function: %s to root's widget", function.c_str());

            int whichWidget = _root->findWhichWidget(widget);
            if (whichWidget < -1) {
                // This situation doesn't suppose to happen
                callJavaLogger(CHILD_THREAD, "[setFunctionToWidget] can't find widget in root%d", _root->getIdi());
                continue;
            }

            // find similar widget in other states and set function
            for (ReuseStatePtr state: _states) {
                if (state == _root) { continue; }
                WidgetPtr similarWidget = state->findWidgetByHashAndLocation(widget->hash(), whichWidget);
                if (similarWidget) {
                    similarWidget->setFunction(function);
                    callJavaLogger(CHILD_THREAD, "successfully set function: %s to R%d's widget", function.c_str(), state->getIdi());
                }
                else {
                    callJavaLogger(CHILD_THREAD, "widget:%s doesn't have similar one in R%d", widget->toHTML().c_str(), state->getIdi());
                }
            }
        }

    }

    void MergedState::onActionExecuted(ActivityStateActionPtr action) {
        updateCompletedFunction2(MAIN_THREAD, action);
    }

    void MergedState::updateCompletedFunction2(int caller, ActivityStateActionPtr action) {
        if (action->getVisitedCount() > 0) {
            WidgetPtr widget = action->getTarget();
            // action BACK has no target widget
            if (!widget) { return; }
            std::string function = widget->getFunction();
            if (!function.empty()) {
                auto it = _functionList.find(function);
                if (it != _functionList.end()) {
                    std::unique_lock<std::mutex> uniqueLock(_functionListMutex);
                    it->second.importance = 0;
                    uniqueLock.unlock();
                    callJavaLogger(caller, "Function:%s is tested by perform: %s", function.c_str(), action->toDescription().c_str());
                }
            }
        }
    }

    void MergedState::writeOverviewAndTop5Tojson(nlohmann::ordered_json &top5, bool ignoreImportance) {
        std::string key = "State" + std::to_string(_id);
        top5[key]["Overview"] = _overview;
        auto sortedFunctions = sortFunctionsByValue(ignoreImportance);
        if (sortedFunctions.size() > 5) {
            sortedFunctions.resize(5);
        }
        top5[key]["FunctionList"] = sortedFunctions;
    }

    nlohmann::ordered_json MergedState::toJson() {
        nlohmann::ordered_json data;
        data["Overview"] = _overview;
        std::vector<std::string> functions;
        for (auto& it: _functionList) {
            functions.push_back(it.first);
        }
        data["Function List"] = functions;
        return data;
    }

    std::vector<std::string> MergedState::sortFunctionsByValue(bool ignoreImportance) {
        // Create a vector to store the reversed form of value and key
        std::vector<std::pair<int, std::string>> pairs;

        std::unique_lock<std::mutex> uniqueLock(_functionListMutex);
        for (const auto& kvp : _functionList) {
            if (kvp.second.importance > 0 || ignoreImportance) {
                pairs.push_back(std::make_pair(kvp.second.importance, kvp.first));
            }
        }
        uniqueLock.unlock();

        // Sort vector in descending order
        std::sort(pairs.begin(), pairs.end(), [](const std::pair<int, std::string>& a, const std::pair<int, std::string>& b) {
            return a.first > b.first;
        });

        // Extract sorted keys
        std::vector<std::string> sortedKeys;
        for (const auto& pair : pairs) {
            sortedKeys.push_back(pair.second);
        }

        return sortedKeys;
    }

    void MergedState::updateLaterJoinedState(ReuseStatePtr state) {
        // set function to widget
        for (WidgetPtr rootWidget: _root->getAllWidgets()) {
            std::string function = rootWidget->getFunction();
            if (function.empty()) { continue; }
            int whichWidget = _root->findWhichWidget(rootWidget);
            if (whichWidget < -1) {
                // This situation doesn't suppose to happen
                callJavaLogger(MAIN_THREAD, "[updateLaterJoinedState] can't find widget in root%d", _root->getIdi());
                continue;
            }
            // find similar widget in state and set function
            WidgetPtr similarWidget = state->findWidgetByHashAndLocation(rootWidget->hash(), whichWidget);
            if (similarWidget) {
                similarWidget->setFunction(function);
                callJavaLogger(MAIN_THREAD, "successfully set function: %s to widget: %s", function.c_str(), similarWidget->toHTML().c_str());
            }
            else {
                callJavaLogger(MAIN_THREAD, "widget:%s doesn't have similar one in R%d", rootWidget->toHTML().c_str(), state->getIdi());
            }
        }

        // set listener to actions
        for (ActivityStateActionPtr action: state->getActions()) {
            action->setListener(shared_from_this());
        }
    }

    bool MergedState::hasUntestedFunctions() {
        // when ask for guiding, main thread is blocked
        // so there is no need to lock _functionList
        bool flag = false;
        for (auto it: _functionList) {
            if (it.second.importance > 0) {
                flag = true;
                break;
            }
        }
        return flag;
    }

    void MergedState::updateFromReanalysis(nlohmann::ordered_json &jsonResp, std::unordered_map<std::string, std::vector<int>>& uniqueWidgets, std::unordered_map<int, WidgetInfo>& widgetDict) {
        std::lock_guard<std::mutex> lock(_mergedStateMutex);
        
        for (auto& it: jsonResp.items()) {
            try {
                int id = std::stoi(it.key());
                std::string function = it.value();
                if (_functionList.find(function) == _functionList.end()) {
                    _functionList.insert(std::make_pair(function, FunctionDetail{1, widgetDict.at(id).state}));
                }
                std::vector<int> widgetIds = uniqueWidgets[widgetDict.at(id).widget->toHTML({}, false, 0)];
                for (int widgetId: widgetIds) {
                    WidgetPtr widget = widgetDict[widgetId].widget;
                    widget->setFunction(function);

                    for (auto& action: widgetDict[widgetId].state->findActionsByWidget(widget)) {
                        updateCompletedFunction2(CHILD_THREAD, action);
                    }
                }
            }
            catch (const std::exception& e) {
                callJavaLogger(CHILD_THREAD, "[Exception]: %s, skip this kv-pair", e.what());
            }
        }
        _needReanalysed = false;
    }

    bool MergedState::needReanalysed() {
        std::lock_guard<std::mutex> lock(_mergedStateMutex);
        return _needReanalysed;
    }



    /////////////////////////////////////////
    // MergedStateGraph
    /////////////////////////////////////////

    MergedStateGraph::MergedStateGraph(GraphPtr graph)
    {
        callJavaLogger(MAIN_THREAD, "MergedStateGraph init");
        _root = nullptr;
        _cursor = nullptr;
        _gptCursor = nullptr;
        _lastState = nullptr;
        _graph = graph;
    }  

    void MergedStateGraph::addNode(MergedStatePtr mergedState, ActionPtr action, bool timeup)
    {
        std::lock_guard<std::mutex> lock(_mergedStateGraphMutex);
        if (mergedState == _cursor) { return; }

        // Add an edge to the graph
        _mergedStates.insert(mergedState);
        if (_root == nullptr) {
            _root = _cursor = mergedState;
            _gptCursor = mergedState;
            callJavaLogger(MAIN_THREAD, "***first add node: MergedState%d", _gptCursor->getId());
        }
        else {
            _cursor->addNext(mergedState);
            mergedState->addPrevious(_cursor);
            // add edge
            _cursor->addEdge(std::make_shared<MergedStateGraphEdge>(mergedState, action, timeup));
            callJavaLogger(MAIN_THREAD, "add edge from MergedState%d to MergedState%d timeup %d", _cursor->getId(), mergedState->getId(), timeup);
            // move forward
            _lastState = _cursor;
            _cursor = mergedState;
        }
    }

    MergedStatePtr MergedStateGraph::findMergedStateById(int id)
    {
        std::lock_guard<std::mutex> lock(_mergedStateGraphMutex);

        auto it = std::find_if(_mergedStates.begin(), _mergedStates.end(),
            [id](auto state) { return id == state->getId(); });
        if (it != _mergedStates.end()) {
            return *it;
        }
        else {
            callJavaLogger(CHILD_THREAD, "[THREAD] findMergedStateById: can't find id %d", id);
            return nullptr;
        }
    }

    std::string MergedStateGraph::temporalWalk(int transitCount)
    {
        if (transitCount == 0) { return "No transition during this period."; }
        std::lock_guard<std::mutex> lock(_mergedStateGraphMutex);
        // begin with _gptCursor
        std::stringstream ss;
        ss << "State" << _gptCursor->getId();
        std::string ret;
        ret.append("State").append(std::to_string(_gptCursor->getId()));

        callJavaLogger(CHILD_THREAD, "[THREAD] temporal walk begin %d", transitCount);
        // walk until next edge's stop flag is true
        MergedStateGraphEdgePtr edge;
        int count = 0;
        do
        {
            edge = _gptCursor->getUnvisitedEdge();
            
            if (edge)
            {
                int id = edge->_next->getId();
//                callJavaLogger(CHILD_THREAD, "[THREAD] edge.next%d", id);
//                callJavaLogger(CHILD_THREAD, "[THREAD] transit %d", count);
//                callJavaLogger(CHILD_THREAD, "[THREAD] %s", (edge->_action ? edge->_action->toDescription() : std::string("")).c_str());
                
                //ss << " -- " << edge->_action->toDescription();
                //ss << " --> " << "State" << id;

                ret.append(" -- ").append(edge->_action->toDescription());
                //callJavaLogger(CHILD_THREAD, "[THREAD] 111 %s", ret.c_str());
                ret.append(" --> State");
                ret.append(std::to_string(id));
                //callJavaLogger(CHILD_THREAD, "[THREAD] 222 %s", ret.c_str());
                
                // callJavaLogger(CHILD_THREAD, "[THREAD] temporal walk: %s", ret.c_str());
                edge->_isVisited = true;
                _gptCursor = edge->_next;
                ++count;
            }
        } while (edge && (count < transitCount));
        
        ret.append("\n");
        return ret;     
    }

    void MergedStateGraph::appendUtgString(std::string value)
    {
        _utgString.append(value).append("\n");
    }

    std::vector<Path> MergedStateGraph::findPaths(int id, bool forceRestart)
    {
        callJavaLogger(MAIN_THREAD, "[MAIN] try to find a paths from M%d to M%d", _cursor->getId(), id);
        MergedStatePtr destination = findMergedStateById(id);
        if (!destination) {
            return std::vector<Path>();
        }
               
        // Try root first
        int rootId = destination->getRootState()->getIdi();
        std::vector<Path> paths = _graph->findPath(rootId, forceRestart);
        
        // If there is no path to root, try to other reuse states under m
        if (paths.empty()) {
            for (auto it: destination->getReuseStates()) {
                if (it->getIdi() != rootId) {
                    paths = _graph->findPath(it->getIdi(), forceRestart);
                    // found one
                    if (!paths.empty()) {
                        return paths;
                    }
                }
            }
        }
        return paths;
    }

    ReuseStatePtr MergedState::getTargetState(std::string function) {
        if (_functionList.find(function) != _functionList.end()) {
            return _functionList[function].state;
        }
        else {
            callJavaLogger(CHILD_THREAD, "function{%s} doesn't belong to any state in MergedState{%d}", function.c_str(), _id);
            return nullptr;
        }
    }
    
}

#endif