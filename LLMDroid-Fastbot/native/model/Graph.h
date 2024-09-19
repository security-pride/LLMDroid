/*
 * This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
 */
/**
 * @authors Jianqiang Guo, Yuhui Su, Zhao Zhang
 */
#ifndef  Graph_H_
#define  Graph_H_

#include "State.h"
#include "Base.h"
#include "Action.h"
#include <map>
//#include "ReuseState.h"
#include "Activity.h"
#include <queue>

namespace fastbotx {
    class Activity;
    typedef std::shared_ptr<Activity> ActivityPtr;

    class ReuseState;
    typedef std::shared_ptr<ReuseState> ReuseStatePtr;

    typedef std::map<WidgetPtr, ActivityStateActionPtrSet, Comparator<Widget>> ModelActionPtrWidgetMap;
    typedef std::map<std::string, StatePtrSet> StatePtrStrMap;

    // typedef std::shared_ptr<std::queue<ActionPtr>> ActionPath;
    // typedef std::shared_ptr<std::queue<int>> StatePath;    
    //typedef std::pair<ActionPath, StatePath> Path;

    // first: parent id
    // second: action
    //typedef std::pair<int, ActionPtr> Step;
    struct Step {
        int node;   // in Graph: node --action-->
                    // in AbstractAgent: --action--> node
        ActionPtr action;
        double time; // time stamp when edge was created
    };
    struct Path {
        size_t length;
        double time;
        std::queue<Step> steps;
    };
    //typedef std::queue<Step> Path;

    std::string pathToString(Path path);


    struct ActionCounter {
    private:

        // Enum Act count
        long actCount[ActionType::ActTypeSize];
        long total;

    public:
        ActionCounter()
                : actCount{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, total(0) {
        }

        void countAction(const ActivityStateActionPtr &action) {
            actCount[action->getActionType()]++;
            total++;
        }

        long getTotal() const { return total; }
    };


    class GraphListener {
    public:
        virtual void onAddNode(StatePtr node) = 0;
    };


    typedef std::shared_ptr<GraphListener> GraphListenerPtr;
    typedef std::vector<GraphListenerPtr> GraphListenerPtrVec;

    class Graph : Node {
    public:
        Graph();

        inline size_t stateSize() const { return this->_states.size(); }

        time_t getTimestamp() const { return this->_timeStamp; }

        void addListener(const GraphListenerPtr &listener);

        // add state to graph, adjust the state or return a exists state
        StatePtr addState(StatePtr state);

        long getTotalDistri() const { return this->_totalDistri; }

        stringPtrSet getVisitedActivities() const { return this->_visitedActivities; };

        virtual ~Graph();

        /**
         * Traverse all current states according to the testing process.
         * Generate Mermaid's flowchart
         * 
         * @return std::string 
         */
        std::string generateGraphCode();

        ReuseStatePtr _firstState = nullptr;

        std::string generateGraphCodeForActivity();

        std::string generateNodeCodeForActivity();

        /**
         * find a path from current state to target state
         * @return Path. if no path found, return pair(null, null)
         * @note call from main thread
        */
        std::vector<Path> findPath(int dest, bool forceRestart);

        ReuseStatePtr findReuseStateById(int id);

    protected:
        void notifyNewStateEvents(const StatePtr &node);


    private:
        void addActionFromState(const StatePtr &node);

        std::vector<Path> Dijkstra(int source, int dest);

        Path transformPath(Path target, int source, int dest);

        std::vector<std::vector<Step>> traceback(std::vector<bool>& is_used, std::vector<std::vector<Step>>& parent, int source, int dest, int layer);

        StatePtrSet _states;      // all of the states in the graph
        stringPtrSet _visitedActivities; // a string set containing all the visited activities
        std::map<std::string, std::pair<int, double>> _activityDistri;
        long _totalDistri; // the count of reaching or accessing states, which could be new states or a state accessed before
        ModelActionPtrWidgetMap _widgetActions; //  query actions based on widget info

        ActivityStateActionPtrSet _unvisitedActions;
        ActivityStateActionPtrSet _visitedActions;

        ActionCounter _actionCounter;
        GraphListenerPtrVec _listeners;
        time_t _timeStamp;

        const static std::pair<int, double> _defaultDistri;

        //custom
        //ReuseStatePtr _firstState = nullptr;       // The first state, the starting point of the graph
        ReuseStatePtr _currentState = nullptr;       // Record the current state, that is, the position in the State diagram
        ReuseStatePtr _cursor = nullptr;           // Record the last visited node of the state graph (where the flowchart is generated)
        std::string _graphCode;
        size_t _stateIdToDraw = 0;              // Record the id of the last node that has been drawn
        ActivityPtr _firstActivity;
        ActivityPtr _currentActivity;
        std::map<std::string, ActivityPtr> _activityMap;

        

        /**
         * Build the state graph based on the incoming state
         * 
         * @param state 
         */
        void buildStateGraph(ReuseStatePtr state);

        void buildActivityGraph(ReuseStatePtr state);

        void generateNodeCode(std::string& graphCode);

        void processPaths(std::vector<Path> &paths, int source, int dest);
    };

    typedef std::shared_ptr<Graph> GraphPtr;

}

#endif  // Graph_H_
