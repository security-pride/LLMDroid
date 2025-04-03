#ifndef MergedState_H_
#define MergedState_H_

#include "ReuseState.h"
#include "model/Graph.h"
#include "../thirdpart/json/json.hpp"
#include <memory>
#include <mutex>
#include <queue>

namespace fastbotx {

    class MergedState;
    class ReuseState;
    typedef std::shared_ptr<ReuseState> ReuseStatePtr;
    typedef std::shared_ptr<MergedState> MergedStatePtr;
    class Graph;
    typedef std::shared_ptr<Graph> GraphPtr;
    // typedef std::shared_ptr<std::queue<ActionPtr>> ActionPath;
    // typedef std::shared_ptr<std::queue<int>> StatePath;    
    // typedef std::pair<ActionPath, StatePath> Path;
    //typedef std::pair<int, ActionPtr> Step;
    struct Step;
    //typedef std::queue<Step> Path;
    struct Path;
    typedef std::vector<MergedStatePtr> MergedStateVec;
    typedef std::shared_ptr<MergedStateVec> MergedStateVecPtr;

    class MergedStateGraphEdge
    {
    public:
        MergedStatePtr _next;
        ActionPtr _action;
        uintptr_t _hash;
        bool _isVisited;
        bool _shouldStop;
    
        MergedStateGraphEdge(MergedStatePtr next, ActionPtr action, bool shouldStop);
        
        uintptr_t hash() const { return _hash; }
        inline bool operator==(const MergedStateGraphEdge &right) const {
            return this->hash() == right.hash();
        }

        // for std::sort
        inline bool operator<(const MergedStateGraphEdge &right) const {
            return this->hash() < right.hash();
        }

    };

    typedef std::shared_ptr<MergedStateGraphEdge> MergedStateGraphEdgePtr;

    struct WidgetInfo {
        std::string function;
        ReuseStatePtr state;
        int importance;
        WidgetPtr widget;
    };

    struct FunctionDetail {
        int importance;
        ReuseStatePtr state;
    };
    
    class MergedState: public HashNode, public FunctionListener, public std::enable_shared_from_this<MergedState>
    {
    public:
        MergedState(ReuseStatePtr state, int id);
        
        ReuseStatePtr getRootState() { return _root; }
        
        int getId() { return _id; }

        // call from child thread
        const std::string getOverview() { return _overview; }

        // call from child thread
        std::map<std::string, FunctionDetail> getFunctionList() { return _functionList; }

        virtual uintptr_t hash() const { return _hashcode; }

        // call from main thread
        void addPrevious(MergedStatePtr state);
        // call from main thread
        void addNext(MergedStatePtr state);
        // call from main thread
        void addEdge(MergedStateGraphEdgePtr edge);
        // call from main thread
        void addState(ReuseStatePtr state, ActionPtr action, bool fromOutside, bool toOutside);

        /**
         * @brief Get the first Unvisited Edge object,
         * @note call from main thread
         * 
         * @return MergedStateGraphEdge& 
         */
        MergedStateGraphEdgePtr getUnvisitedEdge();
        
        // call from child thread
        std::string stateDescription();
        // call from child thread
        std::string walk();
        // call from child thread
        void reset();


        void updateInfo(std::string& response);
        /**
         * @brief Update overview and function list
         * @note call from child thread
         * 
         * @param response 
         */
        void updateFromStateOverview(nlohmann::ordered_json &jsonData);


        void updateFromReanalysis(nlohmann::ordered_json &jsonResp, std::unordered_map<std::string, std::vector<int>>& uniqueWidgets, std::unordered_map<int, WidgetInfo>& WidgetInfo);

        /**
         * @brief Update the completion status of each function in the function list
         * @note call from child thread
         * 
         */
        void updateCompletedFunctions(std::map<std::string, int> completedFunctions);

        /**
         * @brief Update the completion status of a feature in the feature list
         * @note call from main thread
         * 
         */
        void updateCompletedFunction(std::string func);


        std::set<ReuseStatePtr>& getReuseStates() { return _states; }

        int getNavigationValue() { return _navigationValue; }

        /**
         * @note call from child thread
         */
        void updateNavigationValue(int total);

        /**
         * update function
         * @note call from main thread
         */
        void onActionExecuted(ActivityStateActionPtr action) override;

        /**
         * write overview,
         * and top5 functions in _functionList
         * @param top5 the target json object to write into
         * @note call from child thread
         */
        void writeOverviewAndTop5Tojson(nlohmann::ordered_json& top5, bool ignoreImportance = false);

        nlohmann::ordered_json toJson();

        /**
         * set function to new state's widgets,
         * set listener to actions.
         * No need to update completion status because the state is newly found and no action has been performed yet(call by main thread, main thread is blocked now)
         * @param state
         * @note call from main thread
         */
        void updateLaterJoinedState(ReuseStatePtr state);

        /**
         * check whether _functionList has untested function.
         * @return
         * @note call from child thread - askForGuiding
         */
        bool hasUntestedFunctions();

        bool needReanalysed();

        ReuseStatePtr getTargetState(std::string function);

    private:

        void updateNavigationCount();

        void updateCompletedFunctions2();

        /**
         * after getting gpt's response,
         * first set listener for actions,
         * then set action's corresponding function's status to complete
         * @param action
         * @note call from main/child thread
         */
        void updateCompletedFunction2(int caller, ActivityStateActionPtr action);

        /**
         * @note call from child thread
         */
        void setFunctionToWidget(const std::vector<std::pair<std::string, int>>& functionList);

        /**
         * @note call from child thread
         */
        void filterFunctionList();

        /**
         * @note call from child thread
         * @return
         */
        std::vector<std::string> sortFunctionsByValue(bool ignoreImportance);

        int _id; 
        std::set<std::string> _subtasks;
        std::set<ReuseStatePtr> _states;
        ReuseStatePtr _root;    
        uintptr_t _hashcode;

        // mini graph
        std::vector<ReuseStatePtr> _starts;
        ReuseStatePtr _cursor;
        std::mutex _mergedStateMutex;

        // Coarse-grained graph
        std::set<MergedStatePtr> _previous;
        std::set<MergedStatePtr> _next;
        std::vector<MergedStateGraphEdgePtr> _edges;

        std::string _overview;
        std::map<std::string, FunctionDetail> _functionList;
        std::mutex _functionListMutex;

        int _navigationValue = 0;
        int _navigationCount = 0;

        bool _needReanalysed = false;
    };

    

    class MergedStateGraph
    {
    public:
        MergedStateGraph(GraphPtr graph);

        /**
         * @brief When state does not belong to the current MergedState,
         * Add an edge from the current node of the graph pointing to the node to which state belongs,
         * and update currentNode
         * 
         * @param mergedState next MergedState
         * @param action The action currently executed by MergedState
         * @param timeup phase flag of new edge
         * @note call from main thread
         */
        void addNode(MergedStatePtr mergedState, ActionPtr action, bool timeup);

        MergedStatePtr getCurrentNode() { return _cursor; }

        MergedStatePtr getLastNode() { return _lastState; }

        /**
         * @brief Get the Merged State By Id 
         * 
         * @param id 
         * @return MergedStatePtr if not found return nullptr
         */
        MergedStatePtr findMergedStateById(int id);

        /**
         * call from child thread
        */
        std::string temporalWalk(int transitCount);

        std::set<MergedStatePtr>& getMergedStates() { return _mergedStates; }

        /**
         * call from child thread
        */
        void appendUtgString(std::string value);

        const std::string& getUtgString() { return _utgString; }

        /**
         * find a path from current MergedState to target MergedState
         * @return Path. if no path found, return pair(null, null)
         * @note call from main thread
        */
        std::vector<Path> findPaths(int id, bool forceRestart);
    
    private:
        std::mutex _mergedStateGraphMutex;
        
        MergedStatePtr _root;

        MergedStatePtr _cursor; // Used to update the graph and record the current node position

        MergedStatePtr _gptCursor; // Used to describe a graph within a time period to gpt

        MergedStatePtr _lastState;

        std::set<MergedStatePtr> _mergedStates;

        std::set<std::string> _allSubtasks;

        std::set<std::string> _performedSubtask;

        std::set<std::string> getPerformedSubtask() { return _performedSubtask; }

        std::string _utgString;

        GraphPtr _graph;
    };

    typedef std::shared_ptr<MergedStateGraph> MergedStateGraphPtr;

}

#endif