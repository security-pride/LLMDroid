/*
 * This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
 */
/**
 * @authors Jianqiang Guo, Yuhui Su, Zhao Zhang
 */
#ifndef ReuseState_CPP_
#define ReuseState_CPP_

#include "ReuseState.h"

#include <utility>
#include "RichWidget.h"
#include "ActivityNameAction.h"
#include "../utils.hpp"
#include "ActionFilter.h"
#include "ValuableWidget.h"

namespace fastbotx {


    ReuseState::ReuseState()
    = default;

    ReuseState::ReuseState(stringPtr activityName)
            : ReuseState() {
        this->_activity = std::move(activityName);
        this->_hasNoDetail = false;
        this->_actionToPerform = nullptr;
    }

    void ReuseState::buildBoundingBox(const ElementPtr &element) {
        if (element->getParent().expired() &&
            !(element->getBounds() && element->getBounds()->isEmpty())) {
            if (_sameRootBounds.get()->isEmpty() && element) {
                _sameRootBounds = element->getBounds();
            }
            if (equals(_sameRootBounds, element->getBounds())) {
                this->_rootBounds = _sameRootBounds;
            } else
                this->_rootBounds = element->getBounds();
        }
    }

    void ReuseState::buildStateFromElement(WidgetPtr parentWidget, ElementPtr element) {
        buildBoundingBox(element);
        // use RichWidget build the states
        WidgetPtr widget = std::make_shared<RichWidget>(parentWidget, element);
        element->setWidget(widget);
        this->_widgets.emplace_back(widget);
        // Insert key-value pairs into the element map in the state structure:
        // The hash of the widget corresponding to the element: element pointer
        this->_stateStructure._elementMap.insert(std::make_pair(widget->hash(), element));
        this->_stateStructure.insertElement(element);
        for (const auto &childElement: element->getChildren()) {
            buildFromElement(widget, childElement);
        }
        MLOG("widget size after buildStateFromElement: %d", this->_widgets.size());
    }

    void ReuseState::buildFromElement(WidgetPtr parentWidget, ElementPtr elem) {
        buildBoundingBox(elem);
        auto element = std::dynamic_pointer_cast<Element>(elem);
        WidgetPtr widget = std::make_shared<Widget>(parentWidget, element);
        element->setWidget(widget);
        this->_widgets.emplace_back(widget);
        MLOG("[Element] class: %s resource-id: %s text: %s"
            " [widget] class: %s resource-id:%s", 
            elem->getClassname().c_str(), elem->getResourceID().c_str(), elem->getText().c_str(),
            widget->getClass().c_str(), widget->getResourceID().c_str());
        // Insert key-value pairs into the element map in the state structure
        this->_stateStructure._elementMap.insert(std::make_pair(widget->hash(), element));
        this->_stateStructure.insertElement(element);
        for (const auto &childElement: elem->getChildren()) {
            buildFromElement(widget, childElement);
        }
    }

/// @brief according to the element, or XML of this page, and the activity name,
///        create a state and the actions in this page according to the widgets inside this page.
/// @param element XML of this page
/// @param activityName activity name of this page
/// @return a newly created ReuseState according to this page
    ReuseStatePtr ReuseState::create(const ElementPtr &element, const stringPtr &activityName) {
        ReuseStatePtr statePointer = std::shared_ptr<ReuseState>(new ReuseState(activityName));//std::make_shared<ReuseState>(activityName);
        statePointer->buildState(element);
        return statePointer;
    }

    void ReuseState::buildState(const ElementPtr &element) {
        this->_stateStructure._rootElement = element;
        buildStateFromElement(nullptr, element);
        mergeWidgetsInState();
        buildHashForState();
        buildActionForState();
    }

    void ReuseState::buildHashForState() {
        //build hash
        std::string activityString = *(_activity.get());
        uintptr_t activityHash = (std::hash<std::string>{}(activityString) * 31U) << 5;
        activityHash ^= (combineHash<Widget>(_widgets, STATE_WITH_WIDGET_ORDER) << 1);
        _hashcode = activityHash;
    }

    void ReuseState::buildActionForState() {
        MLOG("[buildActionForState]: widget size: %d", this->_widgets.size());
        for (const auto &widget: _widgets) {
            if (widget->getBounds() == nullptr) {
                BLOGE("NULL Bounds happened");
                continue;
            }
            ElementPtr element = this->_stateStructure.findElement(widget->hash());
            if (element == nullptr)
            {
                MLOG("ReuseState: can't find corresponding element to this widget\n"
                    "[widget]\n\tclass: %s\n\tresource-id:%s", widget->getClass().c_str(), widget->getResourceID().c_str());
            }
            for (auto action: widget->getActions()) {
                ActivityNameActionPtr activityNameAction = std::shared_ptr<ActivityNameAction>
                        (new ActivityNameAction(nullptr, getActivityString(), widget, action));
                // Appends a new element to the end of the container.
                // emplace_back() constructs the object in-place at the end of the list,
                // potentially improving performance by avoiding a copy operation,
                // while push_back() adds a copy of the object to the end of the list.
                MLOG("[widget] [class]: %s [resource-id]: %s [action]: %s", 
                    widget->getClass().c_str(), widget->getResourceID().c_str(), activityNameAction->toDescription().c_str());
                _actions.emplace_back(activityNameAction);
                // create a ValuableWidget
                _valuableWidgets.push_back(widget);
                if (element != nullptr)
                {
                    ActionInState actionInState = std::pair<uintptr_t, ActionType>(this->_actions.size() - 1, action);
                    element->addAction(actionInState);
                }
            }
            // if widget doesn't have actions but have text or content-desc, also considered valuable
            if (!widget->hasAction() && !widget->getTextualInfo().empty())
            {
                _valuableWidgets.push_back(widget);
            }
        }
        _backAction = std::make_shared<ActivityNameAction>(nullptr, getActivityString(), nullptr,
                                                           ActionType::BACK);
        _actions.emplace_back(_backAction);
    }

    void ReuseState::mergeWidgetsInState() {
        WidgetPtrSet mergedWidgets;
        int mergedCount = mergeWidgetAndStoreMergedOnes(mergedWidgets);
        if (mergedCount != 0) {
            BDLOG("build state merged  %d widget", mergedCount);
            _widgets.assign(mergedWidgets.begin(), mergedWidgets.end());
        }
    }

    const std::string ReuseState::getStateDescription()
    {
        return this->_stateStructure.generateStateDescription(this->_id);
    }

    void ReuseState::addSubSequentState(ReuseStatePtr state)
    {
        ActionPtr action = this->_actionToPerform;
        if (action == nullptr) {
            action = std::make_shared<Action>(ActionType::NOP);
            MLOG("ReuseState: A state has _actionToPerform = nullptr!");
            MLOG("ReuseState: Problem state: %s", state->toString().c_str());
        }
        // check if action has already existed
        // use new state and actionToPerform as a new token
        uintptr_t edgeHash = action->hash() + state->hash();
        auto it = this->_existedEdges.find(edgeHash);

        // if edgeHash exist in _edges, just increase the value of remainTimes
        if (it != this->_existedEdges.end()) {
            MLOG("edge already exist in state%d's edges", getIdi());
            auto edge = std::find_if(_edges.begin(), _edges.end(), [edgeHash](StateGraphEdge& edge) { return edgeHash == edge.hash; });
            if (edge == _edges.end()) {
                MLOG("ERROR: edgeHash exist in set but not found in _edges!");
            }
            else {
                edge->remainTimes++;
            }            
        } 
        else {
            callJavaLogger(MAIN_THREAD, "Graph: state%d add an edge to state%d: %s", this->_id, state->_id, action->toDescription().c_str());
            ActivityStateActionPtr tmp = std::dynamic_pointer_cast<ActivityStateAction>(action);
            int whichWidget = -1;
            if (tmp) {
                whichWidget = tmp->getWhichWidget();
            }
            this->_edges.emplace_back(StateGraphEdge{action, state, 1, false, edgeHash, whichWidget, currentStamp()});
            this->_existedEdges.insert(edgeHash);
        }

        

    }

    void ReuseState::addPreviousState(StatePtr state)
    {
        std::shared_ptr<ReuseState> preState = std::dynamic_pointer_cast<ReuseState>(state);
        
    }


    std::string ReuseState::getStateNode()
    {
        std::string node;
        node.append("State")
            .append(std::to_string(this->_id));
        return node;
    }

    std::string ReuseState::getBriefDescription()
    {
        if (!this->_briefDescription.empty()) {
            return this->_briefDescription;
        }
        // generate one
        std::string desc;
        size_t dotPosition = (this->_activity)->find_last_of('.');
        if (dotPosition != std::string::npos) {
            // Extract the substring starting from the character after '.'
            desc = this->_activity->substr(dotPosition + 1);
        }
        else {
            desc = *(this->_activity);
        }
        for (int i = 0; i < this->_actions.size(); i++)
        {
            desc.append("\n(")
                .append(std::to_string(i))
                .append(")")
                .append(this->_actions[i]->toDescription());
        }

        return desc;
    }

    float ReuseState::computeSimilarity(ReuseStatePtr target)
    {
        size_t matchedCount = 0;
        bool bigger = target->_widgets.size() > _widgets.size();
        WidgetPtrVec toCompare = bigger ? target->_widgets : _widgets;
        WidgetPtrVecMap toCompareMap = bigger ? target->_mergedWidgets : _mergedWidgets;
        WidgetPtrVec candidates = bigger ? _widgets : target->_widgets;

        for (WidgetPtr candidate : candidates)
        {
            // first find in _widgets
            std::vector<WidgetPtr>::iterator inWidgets = std::find_if(toCompare.begin(), toCompare.end(), [candidate](WidgetPtr ptr) {
                return candidate->getMyHashcode() == ptr->getMyHashcode();
            });
            bool inMergedWidgets = false;
            // also find in _mergedWidgets
            if (toCompareMap.find(candidate->getMyHashcode()) != toCompareMap.end()) {
                WidgetPtrVec mergedWidgets = toCompareMap.at(candidate->getMyHashcode());
                std::vector<WidgetPtr>::iterator found = std::find_if(mergedWidgets.begin(), mergedWidgets.end(), [candidate](WidgetPtr ptr) {
                    return candidate->getMyHashcode() == ptr->getMyHashcode();
                });
                if (found != mergedWidgets.end()) { inMergedWidgets = true; }
            }
            if (inWidgets != toCompare.end() || inMergedWidgets) {
                matchedCount++;
            }
        }
        return (static_cast<float>(matchedCount * 2) / (toCompare.size() + candidates.size()));
    }

    MiniGraphEdge* ReuseState::getUnvisitedMiniEdge()
    {
        for (int i = 0; i < _miniEdges.size(); i++)
        {
            if (_miniEdges[i].isVisited == false) {
                return &(_miniEdges[i]);
            }
        }
        return nullptr;
    }

    std::vector<WidgetPtr> ReuseState::diffWidgets(ReuseStatePtr target)
    {
        // use myHash
        std::vector<WidgetPtr> ret;
        for (WidgetPtr widget: _widgets)
        {
            auto found = std::find_if(target->_widgets.begin(), target->_widgets.end(), [widget](WidgetPtr root_w){
                return widget->getMyHashcode() == root_w->getMyHashcode();
            });
            if (found == target->_widgets.end()) {
                ret.push_back(widget);
            }
        }
        return ret;
    }

    ActivityStateActionPtr ReuseState::findActionByWidget(uintptr_t widgetHash, ActionType actionType)
    {
        ActivityStateActionPtr ret = nullptr;
        for (auto it: _actions)
        {
            if ( it->getTarget() && it->getTarget()->hash() == widgetHash && it->getActionType() == actionType) {
                ret = it;
                break;
            }
        }       
        return ret;
    } 

    ActionPtr ReuseState::findSimilarAction(ActionPtr origin)
    {
        if (origin->getActionType() == ActionType::BACK) {
            auto found = std::find_if(_actions.begin(), _actions.end(),[](ActivityStateActionPtr elem) {
                return elem->getActionType() == ActionType::BACK;
            });
            return *found;
        }
        if (!origin->requireTarget()) {
            callJavaLogger(MAIN_THREAD, "[action:%s] doesn't require target, no need to find", origin->toDescription().c_str());
            return origin;
        }
        callJavaLogger(MAIN_THREAD, "try to convert ActionPtr to ActivityStateActionPtr");
        ActivityStateActionPtr action = std::dynamic_pointer_cast<ActivityStateAction>(origin);
        if (action->getTarget() == nullptr) {
            callJavaLogger(MAIN_THREAD, "[action:%s] has no target, FAILED to find", origin->toDescription().c_str());
            return nullptr;
        }
        uintptr_t h = action->getTarget()->hash();
        // First look for it in widgets
        auto found = std::find_if(_widgets.begin(), _widgets.end(),
            [h](WidgetPtr widget) {
                return h == widget->hash();
            });
        // If there is no corresponding action in the widget, there is no corresponding action and it fails.
        if (found == _widgets.end())
        {
            callJavaLogger(MAIN_THREAD, "[action:%s]'s target widget didn't exist in State%d, FAILED to find", origin->toDescription().c_str(), _id);
            return nullptr;
        }
        // Find the corresponding action from actions
        ActivityStateActionPtr ret = findActionByWidget(h, origin->getActionType());
        if (!ret) {
            callJavaLogger(MAIN_THREAD, "No corresponding action found");
            return nullptr;
        }

        // If the original action is of input type, the current action must also be set to the original input content.
        if (origin->hasInput()) {
            ret->setInputText(origin->getInputText());
        }

        // Now that the current state has corresponding widgets and actions, you need to determine which widget the target widget of the original action corresponds to the current action.
        int originIndex = action->getWhichWidget();

        auto targetWidgets = this->_mergedWidgets.find(h);
        if (targetWidgets == this->_mergedWidgets.end()) {
            // There is no duplicate in mergedWidgets but there is in widgets, indicating that the widget is not duplicated.
            // That is, if there is only one widget for a similar action, just return it directly
            callJavaLogger(MAIN_THREAD, "[action:%s]'s target widget didn't have duplicate widgets, directly return one in _widgets", action->toDescription().c_str());
            return ret;
        }

        // If there are widgets in the merged widgets, you need to modify the target of the current action according to the serial number of the original action.
        int total = (int) (this->_mergedWidgets.at(h).size());
        if (originIndex == -1) {
            callJavaLogger(MAIN_THREAD, "[action:%s]'s target widget is set to -1, accroding to origin index", action->toDescription().c_str());
            ret->setTarget(*found);
            ret->setWhichWidget(originIndex);
            return ret;
        }
        else if (originIndex < total) {
            callJavaLogger(MAIN_THREAD, "[action:%s]'s target widget is set to %d, accroding to origin index", action->toDescription().c_str(), originIndex);
            ret->setTarget(this->_mergedWidgets.at(h)[originIndex]);
            ret->setWhichWidget(originIndex);
            return ret;
        }
        else {
            callJavaLogger(MAIN_THREAD, "[action:%s]'s target widget is set to %d, accroding to origin index", action->toDescription().c_str(), total - 1);
            ret->setTarget(this->_mergedWidgets.at(h)[total - 1]);
            ret->setWhichWidget(total - 1);
            return ret;
        }
    }

    int ReuseState::findActionByElementId(int elementId, int actionType)
    {
        // find element
        ElementPtr element = _stateStructure.findElementById(elementId);
        if (!element) {
            callJavaLogger(CHILD_THREAD, "can't find id:%d in State%d's elements", elementId, _id);
            return -1;
        }

        // get widget
        WidgetPtr widget = element->getWidget();
        callJavaLogger(CHILD_THREAD, "element -> widget:");
        callJavaLogger(CHILD_THREAD, "%s", widget->toHTML().c_str());

        // Determine whether the widget is in widgets or merged widgets. If it is in merged widgets, get its index
        auto it = std::find_if(_widgets.begin(), _widgets.end(), [widget](const WidgetPtr& ptr) {
            return ptr->hash() == widget->hash();
        });
        if (it == _widgets.end()) {
            callJavaLogger(CHILD_THREAD, "0 widget neither in _widgets nor in _mergedWidgets");
            exit(0);
        }
        else {
            callJavaLogger(CHILD_THREAD, "widget:%zu, one in widgets:%d", widget->hash(), (it-_widgets.begin()));
        }

        int whichWidget = findWhichWidget(widget);
        if (whichWidget == -1) {
            callJavaLogger(CHILD_THREAD, "found element->widget in _widgets");
        }
        else if (whichWidget == -2) {
            callJavaLogger(CHILD_THREAD, "1 widget neither in _widgets nor in _mergedWidgets");
            exit(0);
        }
        else if (whichWidget == -3) {
            callJavaLogger(CHILD_THREAD, "2 widget neither in _widgets nor in _mergedWidgets");
            exit(0);
        }
        else {
            callJavaLogger(CHILD_THREAD, "found element->widget in _mergedWidgets %d", whichWidget);
        }
        // Locate the action and its index in actions based on the widget's hash and action type.
        //ActivityStateActionPtr action = findActionByWidget(widget->hash(), widget->getActionType());
        auto action = std::find_if(_actions.begin(), _actions.end(), [widget, actionType](ActivityStateActionPtr& ptr){
            if (!ptr->getTarget()) return false;
            return widget->hash() == ptr->getTarget()->hash() && ptr->getActionType() == actionType;
        });
        if (action == _actions.end()) {
            callJavaLogger(CHILD_THREAD, "No corresponding action found");
            // LLM may return an element doesn't have action
            return -1;
        }
        else {
            // Set the target of the action to the target widget and update the flag in the action
            int index = action - _actions.begin();
            (*action)->setWhichWidget(whichWidget);
            (*action)->setTarget(widget);
            callJavaLogger(CHILD_THREAD, "set target element->widget %d", whichWidget);
            return index;
        }

    }

    ElementPtr ReuseState::findElementById(int id) {
        return _stateStructure.findElementById(id);
    }

    int ReuseState::findWhichWidget(WidgetPtr target)
    {
        int whichWidget;
        auto found = std::find_if(_widgets.begin(), _widgets.end(), [target](const WidgetPtr& ptr) {
            return ptr.get() == target.get();
        });
        if (found != _widgets.end()) {
            whichWidget = -1;
            //callJavaLogger(CHILD_THREAD, "found element->widget in _widgets");
        }
        else {
            uintptr_t hash = target->hash();
            if (_mergedWidgets.find(hash) == _mergedWidgets.end()) {
                //callJavaLogger(CHILD_THREAD, "1 widget neither in _widgets nor in _mergedWidgets");
                return -2;
            }
            WidgetPtrVec mergedOnes = _mergedWidgets.at(hash);
            found = std::find_if(mergedOnes.begin(), mergedOnes.end(), [target](const WidgetPtr& ptr) {
                return ptr.get() == target.get();
            });
            if (found == mergedOnes.end()) {
                //callJavaLogger(CHILD_THREAD, "2 widget neither in _widgets nor in _mergedWidgets");
                return -3;
            }
            whichWidget = found - mergedOnes.begin();
            //callJavaLogger(CHILD_THREAD, "found element->widget in _mergedWidgets %d", whichWidget);
        }
        return whichWidget;
    }

    WidgetPtr ReuseState::findWidgetByHashAndLocation(uintptr_t hash, int location) {
        // first find in widgets
        auto found = std::find_if(_widgets.begin(), _widgets.end(), [hash](WidgetPtr ptr) {
            return ptr->hash() == hash;
        });
        if (found == _widgets.end()) {
            return nullptr;
        }
        // if location = -1, return one in widget
        if (location == -1) {
            return *found;
        }
        else {
            if (_mergedWidgets.find(hash) != _mergedWidgets.end()) {
                // location > merged widgets' size, return the last one
                if (location >= _mergedWidgets.at(hash).size()) {
                    return _mergedWidgets.at(hash).back();
                }
                // return the one at the location
                else {
                    return _mergedWidgets.at(hash)[location];
                }
            }
            else {
                return nullptr;
            }
        }
    }

    std::vector<WidgetPtr> ReuseState::getAllWidgets() {
        WidgetPtrVec ret(_widgets);
        for (WidgetPtr widget: _widgets) {
            if (_mergedWidgets.find(widget->hash()) != _mergedWidgets.end()) {
                WidgetPtrVec& tmp = _mergedWidgets.at(widget->hash());
                ret.insert(ret.end(), tmp.begin(), tmp.end());
            }
        }
        return ret;
    }

    std::vector<ActivityStateActionPtr> ReuseState::findActionsByWidget(WidgetPtr widget) {
        std::vector<ActivityStateActionPtr> ret;
        for (auto it: _actions)
        {
            if ( it->getTarget() && it->getTarget()->hash() == widget->hash()) {
                ret.push_back(it);
                break;
            }
        }       
        return ret;
    }

} // namespace fastbot


#endif // ReuseState_CPP_
