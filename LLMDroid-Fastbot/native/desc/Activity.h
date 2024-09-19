#ifndef  Activity_H_
#define  Activity_H_

#include "Base.h"
#include <map>
#include "ValuableWidget.h"
#include "reuse/ReuseState.h"

namespace fastbotx {

    class ActivityGraphEdge;
    typedef std::shared_ptr<ActivityGraphEdge> ActivityGraphEdgePtr;
    typedef std::set<ActivityGraphEdgePtr, Comparator<ActivityGraphEdge>> ActivityGraphEdgeSet;

    class ReuseState;
    typedef std::shared_ptr<ReuseState> ReuseStatePtr;

    class Activity;
    typedef std::shared_ptr<Activity> ActivityPtr;

    class Activity
    {
    public:

        Activity(ReuseStatePtr state);

        /**
         * Generate activity node description
         * Format:
         * ActivityName[
         * Control information
         * executable actions
         * ]
         * @return std::string 
         */
        std::string generateNodeCode();

        void fillValuableWidget(ReuseStatePtr state);

        inline bool isVisited() { return _isVisited;}

        inline void reset() { _isVisited = false; }

        inline void visit() { _isVisited = true; }

        uintptr_t hash() { return _hashcode; }

        void addSubSequentActivity(std::shared_ptr<Activity> dst, ActionPtr action);

        void sortValuableWidgets();

        /**
         * Generate node code in flowchart
         * Format:
         * Activity name
         * valuable widgets
         * Each time it is called, valuableWidgets is traversed and a description of the Activity is generated.
         * First sort the valuableWidgets (according to the screen coordinate top of the component), and generate the valuableWidget description in order from top to bottom.
         * 
         * @return std::string 
         */
        std::string getBriefDescription();

        ActivityGraphEdgeSet getEdges() { return _edges; }

        std::string getName() { return _name; }

    private:
        std::string _name;
        std::vector<ValuableWidgetPtr> _valuableWigets;
        std::map<uintptr_t, ValuableWidgetPtr> _widgetMap;
        ActivityGraphEdgeSet _edges;
        bool _isVisited;
        uintptr_t _hashcode;

    };


    class ActivityGraphEdge
    {
        public:
        ActivityGraphEdge(ActionPtr action, ActivityPtr activity);
        
        uintptr_t _hash;
        ActionPtr _action;
        ActivityPtr _next;
        bool _isVisited;

        bool operator<(const ActivityGraphEdge& other) const
        {
            return this->_hash < other._hash;
        }
    };

}


#endif