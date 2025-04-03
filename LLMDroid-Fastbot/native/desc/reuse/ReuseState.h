        /*
        *This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
        */
        /**
        *@authors Jianqiang Guo, Yuhui Su, Zhao Zhang
        */
#ifndef ReuseState_H_
#define ReuseState_H_

#include "State.h"
#include "RichWidget.h"
#include <vector>
#include "../StateStructure.h"
#include "../ValuableWidget.h"
#include "MergedState.h"



namespace fastbotx {

        //typedef std::pair<ActivityStateActionPtr, StatePtr> StateGraphEdge;

    struct StateGraphEdge;
    class ReuseState;
    typedef std::shared_ptr<ReuseState> ReuseStatePtr;
    class MergedState;
    typedef std::shared_ptr<MergedState> MergedStatePtr;

    struct MiniGraphEdge;

        //This class is for build a state which holds all RichWidgets and their associated actions and so on.
class ReuseState : public State, public std::enable_shared_from_this<ReuseState> {
    public:
        static std::shared_ptr<ReuseState>
        create(const ElementPtr &element, const stringPtr &activityName);

        //custom
        const std::string getStateDescription();
        void addSubSequentState(std::shared_ptr<ReuseState> state);
        void addPreviousState(StatePtr state);
        float computeSimilarity(std::shared_ptr<ReuseState> state);

        const std::vector<StateGraphEdge>& getEdges() { return _edges; }
        
        std::vector<StateGraphEdge> _edges;     
        std::string getBriefDescription();
        std::string getStateName();

        ActionPtr _actionToPerform;     

        /**
         * Generate nodes in flowchart
         * Format: State5[brief description]
         *
         * @return std::string
        */
        std::string getStateNode();

        std::vector<WidgetPtr> getValuableWidgets() {return _valuableWidgets;}
        
        void setMergedState(MergedStatePtr mergedState) { _mergedState = mergedState; }
        MergedStatePtr getMergedState() { return _mergedState; }

        //mini graph
        std::vector<MiniGraphEdge> _miniEdges;
        void addMiniEdge(MiniGraphEdge edge);
        MiniGraphEdge* getUnvisitedMiniEdge();
        std::vector<WidgetPtr> diffWidgets(ReuseStatePtr target);

        /**
         *find similar action in current state to replace next step
         *@note call from main thread -guideCheck
        */
        ActionPtr findSimilarAction(ActionPtr target);

        /**
         * find corresponding action using element id and type
         * @param elementId
         * @param actionType
         * @return
         * @note call from child thread -(askForFunction)when processing gpt's response
        */
        int findActionByElementId(int elementId, int actionType);

        ElementPtr findElementById(int id);

        /**
         * locate target widget in _widgets or _mergedWidgets
         * @param target
         * @return -1: in _widgets, >0: index in _mergedWidgets, -2 -3: not found
         * @note call from child thread
        */
        int findWhichWidget(WidgetPtr target);

        /**
         * find widget using hash and location in _widgets/_mergedWidgets.
         * usually used to find the specific widget with same hash but different location.
         * @param hash
         * @param location
         * @return WidgetPtr if found, nullptr if not found
         * @note call from main/child thread -when finding similar widget in other state
        */
        WidgetPtr findWidgetByHashAndLocation(uintptr_t hash, int location);

        /**
         * Get all widgets including merged ones
         * @return
         * @note call from main thread
        */
        std::vector<WidgetPtr> getAllWidgets();

        std::vector<ActivityStateActionPtr> findActionsByWidget(WidgetPtr widget);
    protected:
        virtual void buildStateFromElement(WidgetPtr parentWidget, ElementPtr element);

        virtual void buildHashForState();

        virtual void buildActionForState();

        virtual void mergeWidgetsInState();

        explicit ReuseState(stringPtr activityName);

        ReuseState();

        virtual void buildState(const ElementPtr &element);

        virtual void buildBoundingBox(const ElementPtr &element);

    private:
        void buildFromElement(WidgetPtr parentWidget, ElementPtr elem) override;

        /**
         *using widget's hash and actionType to find action in _actions
         *@param widgetHash
         *@param actionType
         *@return
        */
        ActivityStateActionPtr findActionByWidget(uintptr_t widgetHash, ActionType actionType);

        //custom
        StateStructure _stateStructure;
        std::string _briefDescription;
        std::set<uintptr_t> _existedEdges;
        MergedStatePtr _mergedState;
        //std::vector<StatePtr> _preivousStates;
        //ActivityStateActionPtrVec _actionsToHere;
        std::vector<WidgetPtr> _valuableWidgets;
        
    };

    struct MiniGraphEdge
    {
        ReuseStatePtr next;
        ActionPtr action;
        bool isVisited;
    };


    struct StateGraphEdge {
        ActionPtr action;
        ReuseStatePtr nextState;
        int remainTimes;        
        bool isDrawn;       
        uintptr_t hash;
        int whichWidget;
        double createdTime;
    };

}


#endif      
