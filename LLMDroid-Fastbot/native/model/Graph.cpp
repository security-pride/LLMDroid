/*
 * This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
 */
/**
 * @authors Jianqiang Guo, Yuhui Su, Zhao Zhang
 */
#ifndef  Graph_CPP_
#define  Graph_CPP_


#include "Graph.h"
#include "../utils.hpp"
#include <vector>
#include "ReuseState.h"
#include <stack>
#include <stdexcept>

namespace fastbotx {


    Graph::Graph()
            : _totalDistri(0), _timeStamp(0) {

    }

// first one describing the count of how many times that the state is visited.
// the second one, describing the percentage of times that this state been accessed over that of all the states.
    const std::pair<int, double> Graph::_defaultDistri = std::make_pair(0, 0.0);

    StatePtr Graph::addState(StatePtr state) {
        auto activity = state->getActivityString(); // get the activity name(activity class name) of this new state
        auto ifStateExists = this->_states.find(state); // try to find state in state caches
        if (ifStateExists ==
            this->_states.end()) // if this is a brand-new state, emplace this state inside _states cache
        {
            state->setId((int) this->_states.size());
            this->_states.emplace(state);
            MLOG("A brand-new state %d, add to _states", (int) this->_states.size());
        } else {
            MLOG("A state already exist, check if it has details");
            if ((*ifStateExists)->hasNoDetail()) {
                MLOG("This State has no details, try to fill details");
                (*ifStateExists)->fillDetails(state);
            }
            state = *ifStateExists;
        }

        this->notifyNewStateEvents(state);

        this->_visitedActivities.emplace(
                activity); // add this activity name to this set, and in this set, every name is unique.
        this->_totalDistri++;
        std::string activityStr = *(activity.get());
        if (this->_activityDistri.find(activityStr) ==
            this->_activityDistri.end()) // try to find this activity name in the map of recording how many times one activity been accessed.
        {
            this->_activityDistri[activityStr] = _defaultDistri; // if not found, use the default pair of (0, 0) to initialize.
        }
        this->_activityDistri[activityStr].first++;
        this->_activityDistri[activityStr].second =
                1.0 * this->_activityDistri[activityStr].first / this->_totalDistri;
        addActionFromState(state);

        //buildActivityGraph(std::dynamic_pointer_cast<ReuseState>(state));
        buildStateGraph(std::dynamic_pointer_cast<ReuseState>(state));
        return state;
    }


    void Graph::notifyNewStateEvents(const StatePtr &node) {
        for (const auto &listener: this->_listeners) {
            listener->onAddNode(node);
        }
    }

    void Graph::addListener(const GraphListenerPtr &listener) {
        this->_listeners.emplace_back(listener);
    }

    void Graph::addActionFromState(const StatePtr &node) {
        auto nodeActions = node->getActions();
        for (const auto &action: nodeActions) {
            auto itervisted = this->_visitedActions.find(action);
            bool visitedadd = itervisted != this->_visitedActions.end();
            auto iterunvisited = this->_unvisitedActions.find(action);
            bool unvisitedadd = !visitedadd && iterunvisited != this->_unvisitedActions.end();
            if (visitedadd || unvisitedadd) {
                action->setId((visitedadd ? (*itervisted)->getIdi() : (*iterunvisited)->getIdi()));
            } else {
                action->setId((int) this->_actionCounter.getTotal());
                this->_actionCounter.countAction(action);
            }

            if (!visitedadd && action->isVisited())
                this->_visitedActions.emplace(action);

            if (!unvisitedadd && !action->isVisited())
                this->_unvisitedActions.emplace(action);
        }
        BDLOG("unvisited action: %zu, visited action %zu", this->_unvisitedActions.size(),
              this->_visitedActions.size());
    }

    Graph::~Graph() {
        this->_states.clear();
        this->_unvisitedActions.clear();
        this->_widgetActions.clear();
    }

    void Graph::buildStateGraph(ReuseStatePtr state)
    {
        if (this->_firstState == nullptr)
        {
            MLOG("Graph:: add first state");
            this->_firstState = state;
            this->_currentState = state;
            this->_cursor = this->_firstState;
            return;
        }

        // The currentState at this time is the previous state
        // Add an edge starting from currentState and pointing to state
        MLOG("Graph: current state num: %d", this->_currentState->getIdi());
        this->_currentState->addSubSequentState(state);

        // Point current state to current state
        this->_currentState = state;
        MLOG("Graph: move current state to num: %d", this->_currentState->getIdi());
        return;
    }

    std::string Graph::generateGraphCode()
    {
        // Start from the state that has not been traversed last time
        // The state directed graph is essentially a chain, and there is a path that can access all edges.
        // So you only need to follow the test process to traverse all states.
        // Possible problems: app crashes, relaunches, or page delays change, resulting in discontinuous paths
        std::string graphCode;
        if (this->_graphCode.empty())
        {
            graphCode = "flowchart LR\n";
        }

        // Generate (supplement) node code
        generateNodeCode(graphCode);
        
        std::vector<StateGraphEdge>::iterator edge;
        while (!this->_cursor->_edges.empty())
        {
            // Find an unvisited edge
            for (edge = this->_cursor->_edges.begin(); edge != this->_cursor->_edges.end(); edge++)
            {
                if (edge->remainTimes) break;
            }
            // If all edges have been visited, the traversal ends
            if (edge == this->_cursor->_edges.end())
            {
                break;
            }
            if (edge->remainTimes == 1 && !edge->isDrawn)
            {
                graphCode.append(this->_cursor->getStateNode())
                    .append("-- \"")
                    .append(edge->action->toDescription()) // add toString method in ActivityStateAction
                    .append("\" -->")
                    .append(edge->nextState->getStateNode())
                    .append("\n");
                edge->isDrawn = true;
            }                        
            edge->remainTimes--;
            this->_cursor = edge->nextState;

        }
        // The last visited state has no outgoing edges or all edges have been visited, then the traversal is completed.
        this->_graphCode.append(graphCode);
        return this->_graphCode;
    }

    void Graph::generateNodeCode(std::string& graphCode)
    {
        while (this->_stateIdToDraw <= (this->_states.size() - 1))
        {
            // find state in states, whose id=lastDrawnStateId+1
            size_t targetID = _stateIdToDraw;
            auto it = std::find_if(_states.begin(), _states.end(), [targetID](const StatePtr& s){
                return s->getIdi() == targetID;
            });
            if (it == _states.end())
            {
                MLOG("ERROR: can't find state%d in _states while state size is %d", targetID, stateSize());
                break;
            }
            // draw this state
            graphCode.append("State")
                .append(std::to_string(targetID))
                .append("[\"")
                .append((std::dynamic_pointer_cast<ReuseState>(*it))->getBriefDescription())
                .append("\"]\n");

            _stateIdToDraw++;
        }
    }

    void Graph::buildActivityGraph(ReuseStatePtr state)
    {
        std::string activityName = *((state->getActivityString()).get());
        auto it = _activityMap.find(activityName);
        if (it != _activityMap.end())
        {
            MLOG("Graph: State%d => %s already exist, try to add information", state->getIdi(), activityName.c_str());
            it->second->fillValuableWidget(state);
            MLOG("Graph: fill complete");
            _currentActivity->addSubSequentActivity(it->second, _currentState->_actionToPerform);
            _currentActivity = it->second;
        }
        else
        {
            // create a new activity based on state
            ActivityPtr activity = std::make_shared<Activity>(state);
            _activityMap[activityName] = activity;
            if (_firstActivity == nullptr)
            {
                _firstActivity = activity;
                _currentActivity = activity;
                MLOG("Graph: add first activity: %s", _currentActivity->getName().c_str());
                return;
            }
            MLOG("Graph: State%d => %s newly found, add it to graph", state->getIdi(), activityName.c_str());
            _currentActivity->addSubSequentActivity(activity, _currentState->_actionToPerform); 
            _currentActivity = activity;           
        }
        return;

    }

    std::string Graph::generateGraphCodeForActivity()
    {
        for (auto it : _activityMap) {
            it.second->reset();
        }
        
        std::string graphCode = "flowchart LR\n";

        graphCode.append(generateNodeCodeForActivity());

        // draw edges
        std::set<ActivityPtr> unvisitedActivities;
        ActivityPtr targetActivity;
        // start from firstActivity
        unvisitedActivities.insert(_firstActivity);
        do
        {
            // retrieve an element from set and remove it
            auto it = (unvisitedActivities.begin());
            targetActivity = *it;
            unvisitedActivities.erase(*it);
            if (targetActivity->isVisited()) {
                continue;
            }
            // visit targetActivity and draw edges
            targetActivity->visit();
            for (ActivityGraphEdgePtr edge : targetActivity->getEdges())
            {
                //edge->_isVisited = true;
                unvisitedActivities.insert(edge->_next);
                // draw edge
                graphCode.append(targetActivity->getName())
                    .append("-- \"")
                    .append(edge->_action->toDescription())
                    .append("\" -->")
                    .append(edge->_next->getName())
                    .append("\n");

            }
        } while (!unvisitedActivities.empty());
        return graphCode;
    }

    std::string Graph::generateNodeCodeForActivity()
    {
        std::string code;
        for (auto it : _activityMap)
        {
            code.append(it.first)
                .append("[\"\n")
                .append(it.second->getBriefDescription())
                .append("\"]\n");
        }
        return code;
    }

    std::vector<Path> Graph::findPath(int dest, bool forceRestart)
    {
        ReuseStatePtr destination = findReuseStateById(dest);
        if (!destination) {
            return std::vector<Path>();
        }
               
        int source = _currentState->getIdi();
        callJavaLogger(MAIN_THREAD, "[GRAPH] try to find a path from ReuseState%d(M%d) to ReuseState%d(M%d)",
            source, _currentState->getMergedState()->getId(),
            dest, destination->getMergedState()->getId());

        if (!forceRestart) {
            std::vector<Path> forwardPath = Dijkstra(source, dest);
            // The state of each step of the path calculated by dijkstra is the source state
            // That is, the meaning of Step at this time is State --action -->
            // But in AbstracAgent, the form of processing --action -->State is more convenient
            // So we need to do some conversion
            if (!forwardPath.empty()) // || source == dest
            {
                processPaths(forwardPath, source, dest);
                return forwardPath;
            }
        }
        else {
            callJavaLogger(MAIN_THREAD, "[GRAPH] Find path from R0");
            std::vector<Path> originPath = Dijkstra(0, dest);
            if (!originPath.empty()) //  || dest == 0
            {
                processPaths(originPath, 0, dest);
                return originPath;
            }
        }

        callJavaLogger(MAIN_THREAD, "[GRAPH] no path found!");
        return std::vector<Path>();

    }

    void Graph::processPaths(std::vector<Path>& paths, int source, int dest) {
        // sort by length
        std::sort(paths.begin(), paths.end(), [](const Path& a, const Path& b) {
            return a.length < b.length;
        });
        // sort by time
        if (paths.size() >= 2) {
            std::sort(paths.begin() + 1, paths.end(), [](const Path& a, const Path& b) {
                return a.time > b.time;
            });
        }
        // only preserve 3 elements
        if (paths.size() > 3) {
            paths.resize(3);
        }
        // transform every path
        for (int i = 0; i < paths.size(); i++) {
            callJavaLogger(MAIN_THREAD, "[GRAPH] PATH %d, time %f, length %d:\n%s\n",
                           i,  paths[i].time, paths[i].length, pathToString(paths[i]).c_str());
            paths[i] = transformPath(paths[i], source, dest);
        }
    }

    Path Graph::transformPath(Path origin, int source, int dest)
    {
        Path res;
        std::stringstream ss;
        ss << "State" << _currentState->getIdi();
        // The current state is not 0, but the path starts from 0, indicating that a restart is required.
        if (_currentState->getIdi() != 0 && source == 0) {
            res.steps.push(Step{0, Action::RESTART, 0.0});
            ss << "-- RESTART -->State0";
        }
        // The current state is 0, and the path starts from 0, no operation is required.
        // But if an empty queue is returned directly, it will be difficult for AbstractAgent to handle, so a no-op is added.
        else if (_currentState->getIdi() == 0 && source == 0) {
            res.steps.push(Step{0, Action::NOP, 0.0});
            ss << "-- NOP -->State0";
        }

        while (!origin.steps.empty())
        {
            Step step = origin.steps.front();
            origin.steps.pop();
            // If the path contains a restart operation, clear the queue directly and start with the restart operation.
            if (step.action->getActionType() == ActionType::RESTART) {
                while (!res.steps.empty()) {
                    res.steps.pop();
                }
                ss.str("");
                ss << "State" << _currentState->getIdi();
            }
            step.node = (!origin.steps.empty()) ? origin.steps.front().node : dest;
            res.steps.push(step);
            ss << "-- " << step.action->toDescription() << " -->State" << step.node;
        }
        
        callJavaLogger(MAIN_THREAD, "[transformed path]\n%s\n", ss.str().c_str());
        res.length = res.steps.size();
        res.time = origin.time;
        return res;
    }

    std::string pathToString(Path path)
    {
        callJavaLogger(MAIN_THREAD, "[MAIN] path to string");
        std::stringstream ss;
        std::queue<Step> copiedPath(path.steps);
        while (!copiedPath.empty())
        {
            //callJavaLogger(MAIN_THREAD, "[MAIN] hhh");
            std::string act = copiedPath.front().action ? copiedPath.front().action->toDescription(): "";
            
            ss << "State" << copiedPath.front().node;
            ss << "-- " << act << " -->";
            copiedPath.pop();
        }

        return ss.str();

    }

    ReuseStatePtr Graph::findReuseStateById(int id)
    {
        auto result = std::find_if(_states.begin(), _states.end(), [id](StatePtr state) {
            return state->getIdi() == id;
        });
        if (result != _states.end()) {
            return std::dynamic_pointer_cast<ReuseState>(*result);
        }
        else {
            callJavaLogger(MAIN_THREAD, "[MAIN] findReuseStateById: can't find id %d", id);
            return nullptr;
        }
    }
    

    std::vector<Path> Graph::Dijkstra(int source, int dest)
    {
         // Initialize the distance array, all set to infinity
        int stateNum = _states.size();
        std::vector<int> dist(stateNum, std::numeric_limits<int>::max());

        std::vector<std::vector<Step>> parent(stateNum, std::vector<Step>());
        //1, std::make_pair(-1, nullptr)
        // Set the distance from the source point to itself to 0
        dist[source] = 0;

        // Priority queue, sorted by distance
        // first: source到second表示的ReuseState的距离
        // second: ReuseState的id
        std::priority_queue<std::pair<int, int>, std::vector<std::pair<int, int>>, std::greater<std::pair<int, int>>> pq;

        // Add the source point to the priority queue
        pq.push({0, source});

        callJavaLogger(MAIN_THREAD, "[Dijkstra] path compute begin: source%d dest%d", source, dest);
        while (!pq.empty()) {
            // Remove the element with the smallest distance from the queue
            int u = pq.top().second;
            pq.pop();
            ReuseStatePtr u_state = findReuseStateById(u);

            // Traverse all edges of u
            for (auto v_edge : u_state->getEdges()) {
                int v = (v_edge.nextState)->getIdi();
                currentStamp();
                // create a copy of this action
                ActivityStateActionPtr tmp = std::dynamic_pointer_cast<ActivityStateAction>(v_edge.action);
                ActivityStateActionPtr action_copy = tmp ? std::make_shared<ActivityStateAction>(*(tmp.get())) : nullptr;
                // set correct Target Widget to action_copy
                if (tmp) {
                    int currentWidget = tmp->getWhichWidget();
                    // action's currentWidget differs from original widget recorded in edge
                    // meaning action's targetWidget has been changed since it was added into graph
                    if (v_edge.whichWidget != currentWidget) {
                        callJavaLogger(MAIN_THREAD, "action's currentWidget%d differs from originWidget%d recorded in edge", currentWidget, v_edge.whichWidget);
                        // action BACK's target is nullptr
                        // but action BACK's currentWidget must be -1, so we can ignore this situation
                        WidgetPtr realTarget = u_state->findWidgetByHashAndLocation(tmp->getTarget()->hash(), v_edge.whichWidget);
                        if (realTarget) {
                            callJavaLogger(MAIN_THREAD, "successfully set correct widget to the action below:");
                            action_copy->setWhichWidget(v_edge.whichWidget);
                            action_copy->setTarget(realTarget);
                        }
                    }
                }
                // If the path through u to v is shorter, update the distance and add to the queue
                bool loose = false;
                if (dist[u] + 1 < dist[v]) {
                    dist[v] = dist[u] + 1;
                    pq.push({dist[v], v});
                    loose = true;
                }
                // record v's parent node and action
                // u==v or when Parent[v] already contains u, it will not be added to minimize the amount of calculation during traceback.
                if (u != v)
                {
                    auto found = std::find_if(parent[v].begin(), parent[v].end(), [u](const Step& s) {
                        return s.node == u;
                    });
                    // make sure the node in shortest path is always the first one parent[v]
                    if (found == parent[v].end()) {
                        // insert to first
                        if (loose) {
                            if (action_copy) {
                                parent[v].insert(parent[v].begin(), Step{u, action_copy, v_edge.createdTime});
                            } else {
                                parent[v].insert(parent[v].begin(), Step{u, v_edge.action, v_edge.createdTime});
                            }
                        }
                        else {
                            if (action_copy) {
                                parent[v].push_back(Step{u, action_copy, v_edge.createdTime});
                            } else {
                                parent[v].push_back(Step{u, v_edge.action, v_edge.createdTime});
                            }
                        }
                        callJavaLogger(MAIN_THREAD, "parent[%d] first:%d act:%s", v, u,
                                       parent[v].back().action->toDescription().c_str());
                    }
                    else if (loose){
                        // move to first
                        std::rotate(parent[v].begin(), found, found + 1);
                    }
                }
            }
        }
        callJavaLogger(MAIN_THREAD, "[Dijkstra] path compute finished, begin to traceback");

        std::vector<bool> is_used = std::vector<bool>(stateNum, false);
        std::vector<std::vector<Step>> all_paths = traceback(is_used, parent, source, dest, 0);
        callJavaLogger(MAIN_THREAD, "[Dijkstra] traceback complete, begin to generate paths, total %d raw paths", all_paths.size());
        std::vector<Path> ret;
        int num = 0;
        try {
            for (auto path: all_paths) {
                if (path.empty()) {
                    callJavaLogger(MAIN_THREAD, "[Dijkstra] warning path is empty");
                }
                auto latest = std::max_element(path.begin(), path.end(), [](const Step& a, const Step& b) {
                    return a.time < b.time;
                });
                double latest_time = 0.0;
                if (latest != path.end()) {
                    latest_time = latest->time;
                }
                ret.emplace_back(Path{path.size(), latest_time, std::queue<Step>(std::deque<Step>(path.begin(), path.end()))});
                if (++num >= 100) { break; }
            }
        }
        catch (std::exception& e){
            callJavaLogger(MAIN_THREAD, "[Dijkstra] exception: %s", e.what());
        }
        callJavaLogger(MAIN_THREAD, "[Dijkstra] done");
        return ret;
    }

    std::vector<std::vector<Step>>
    Graph::traceback(std::vector<bool>& is_used, std::vector<std::vector<Step>> &parent, int source, int dest, int layer) {
        if (source == dest) {
            return std::vector<std::vector<Step>>(1);
        }
        // Limit the recursion depth of backtracking
        if (layer > 10) {
            return std::vector<std::vector<Step>>();
        }
        // Check whether adding the current node will cause loop formation
        if (is_used[dest]) {
            return std::vector<std::vector<Step>>();
        }
        else {
            is_used[dest] = true;
        }
        std::vector<std::vector<Step>> res;
        std::vector<Step> precursors = parent[dest];
        for (auto precursor: precursors) {
            // recursive
            std::vector<std::vector<Step>> possible_paths = traceback(is_used, parent, source, precursor.node, layer + 1);
            if (layer <= 1) {
//                callJavaLogger(MAIN_THREAD, "[Layer%d] %d: traceback(%d, %d) complete",
//                               layer, dest, source, precursor.node);
            }
            for (auto current_path: possible_paths) {
                // pre_node --action--> current node (dest)
                //    ^         ^
                // precursor.first/second
                current_path.push_back(precursor);
                res.push_back(current_path);
            }
        }
        // restore
        is_used[dest] = false;
        return res;
    }
}

#endif //Graph_CPP_
