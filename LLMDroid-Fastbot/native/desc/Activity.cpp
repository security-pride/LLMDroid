#ifndef Activity_CPP_
#define Activity_CPP_

#include "Activity.h"
#include <algorithm>

namespace fastbotx {

    ActivityGraphEdge::ActivityGraphEdge(ActionPtr action, ActivityPtr activity)
    {
        _action = action;
        _next = activity;
        _hash = std::hash<std::string>{}(action->toDescription());
        _isVisited = false;

    }

    Activity::Activity(ReuseStatePtr state)
    {
//TODO: Create a new activity based on state
        _name = *(state->getActivityString().get());
//build valuable widget based on widget in state
        fillValuableWidget(state);
        _isVisited = false;
        _edges.clear();
        _hashcode = std::hash<std::string>{}(_name);
    }

    void Activity::fillValuableWidget(ReuseStatePtr state)
    {
//TODO: Supplement activity information based on state
//_valuableWigets
        for (auto widget : state->getValuableWidgets())
        {
//Find based on location hash
            auto valuableWidget = std::find_if(_valuableWigets.begin(), _valuableWigets.end(), [widget](ValuableWidgetPtr predicate){
                return widget->getBounds()->hash2() == predicate->hash();
            });
//There is no hash for this position in valuableWidgets
            if (valuableWidget == _valuableWigets.end())
            {
//First try to search in the table to see if the component is displaced due to page sliding.
                auto it = _widgetMap.find(widget->hash());
                if (it != _widgetMap.end())
                {
                    MLOG("Activity: find valuableWidget through bound hash failed, but find one through widget's hash");
                    it->second->fillDetails(widget);
                    _widgetMap[widget->hash()] = it->second;
                }
//Neither the position hash nor the widget hash can find the corresponding valuableWidget, then create a new one
                else
                {
                    ValuableWidgetPtr newWidget= std::make_shared<ValuableWidget>(widget);
                    _valuableWigets.push_back(newWidget);
                    _widgetMap[widget->hash()] = newWidget;
                    MLOG("Activity: add a new valuable widget: %s", widget->toString().c_str());
                }
                
            }
            else
            {
//Add widget information to it
                MLOG("Activity: find valuableWidget through bound's hash");
                (*valuableWidget)->fillDetails(widget);
                _widgetMap[widget->hash()] = *valuableWidget;
            }
        }
    }

    void Activity::addSubSequentActivity(ActivityPtr dst, ActionPtr action)
    {
//MLOG("addSubSequentActivity %s action: %s", dst->getName().c_str(), action->toDescription().c_str());
        ActivityGraphEdgePtr edge = std::make_shared<ActivityGraphEdge>(action, dst);

        auto it = _edges.find(edge);
        if (it == _edges.end())
        {
            MLOG("Activity: add a brand new edge, from %s to %s", _name.c_str(), dst->_name.c_str());
            _edges.insert(edge);
        }
        else 
        {
            MLOG("Activity: edge already exist in %s, ignore it", _name.c_str());
        }
        
        
        
    }

    void Activity::sortValuableWidgets()
    {
//TODO sort ValuableWidgets by attribute top
    }

    bool compareByTop(const ValuableWidgetPtr& a, const ValuableWidgetPtr& b)
    {
        return a->getTop() < b->getTop();
    }

    std::string Activity::getBriefDescription()
    {
//sort valuable widgets();
        std::sort(_valuableWigets.begin(), _valuableWigets.end(), compareByTop);
        std::string desc = _name + "\n";
        for (ValuableWidgetPtr widget : _valuableWigets)
        {
            desc.append(widget->toDescription());
        }
        return desc;
    }

}


#endif