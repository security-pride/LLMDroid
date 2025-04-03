/*
 * This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
 */
/**
 * @authors Jianqiang Guo, Yuhui Su, Zhao Zhang, Zhengwei Lv
 */
#ifndef Action_CPP_H_
#define Action_CPP_H_

#include "Action.h"

#include <utility>
#include "State.h"


namespace fastbotx {

    Action::Action()
            : Node(), PriorityNode(), _actionType(ActionType::NOP), _qValue(0) {
    }

    Action::Action(ActionType actionType)
            : Node(), PriorityNode(), _actionType(actionType), _qValue(0) {
    }

    int Action::_throttle = 100;

    int Action::getPriorityByActionType() const {
        switch (this->_actionType) {
            case ActionType::CLICK:
                return 4;
            case ActionType::LONG_CLICK:
            case ActionType::SCROLL_TOP_DOWN:
            case ActionType::SCROLL_BOTTOM_UP:
            case ActionType::SCROLL_LEFT_RIGHT:
            case ActionType::SCROLL_RIGHT_LEFT:
                return 2;
            default:
                return 1;
        }
    }

    bool Action::isValid() const {
        return true;
    }

    bool Action::isModelAct() const {
        return this->_actionType >= ActionType::BACK &&
               this->_actionType <= ActionType::SCROLL_BOTTOM_UP_N;
    }

    bool Action::requireTarget() const {
        return this->_actionType >= ActionType::CLICK &&
               this->_actionType <= ActionType::SCROLL_BOTTOM_UP_N;
    }

    bool Action::canStartTestApp() const {
        return this->_actionType == ActionType::START ||
               this->_actionType == ActionType::RESTART ||
               this->_actionType == ActionType::CLEAN_RESTART;
    }

    bool Action::operator==(const Action &action) {
        return this->_actionType == action._actionType;
    }

    void Action::setPriority(int priority) {
        this->_priority = priority;
    }

    std::string Action::toString() const {
        std::stringstream strs;
        strs << "{id: " << this->getId() << ", act: " << actName[this->_actionType] <<
             ", value: " << this->_qValue << "}";
        return strs.str();
    }

    OperatePtr Action::toOperate() const {
        OperatePtr opt = std::make_shared<DeviceOperateWrapper>();
        opt->act = this->_actionType;
        opt->aid = this->getId();
        if (this->_visitedCount <= 1) {
            opt->throttle = static_cast<float>(randomInt(10, Action::_throttle));
        }
        return opt;
    }

    std::shared_ptr<Action> Action::NOP = std::make_shared<Action>(ActionType::NOP);
    std::shared_ptr<Action> Action::ACTIVATE = std::make_shared<Action>(ActionType::ACTIVATE);
    std::shared_ptr<Action> Action::RESTART = std::make_shared<Action>(ActionType::RESTART);

    PropertyIDPrefixImpl(Action, "g0a");
    const int Action::DefaultValue = 0;

/// Construct an empty ActivityStateAction object with no action, state or target.
    ActivityStateAction::ActivityStateAction()
            : Action(), _target(nullptr) {

    }

/// Construct an ActivityStateAction object constituted of state, targetWidget and the corresponding action type
/// \param state The StatePtr object, describing the current page
/// \param targetWidget The WidgetPtr object, describing the targetWidget on the page for acting, could be type of Widget
/// \param actionType The corresponding action type
    ActivityStateAction::ActivityStateAction(const StatePtr &state, WidgetPtr targetWidget,
                                             ActionType actionType)
            : Action(actionType), _state(state), _target(std::move(targetWidget)) {

        uintptr_t hashcode = std::hash<int>{}(this->getActionType());
        uintptr_t stateHash = this->_state.expired() ? 0x1 : this->_state.lock()->hash();
        uintptr_t targetHash = nullptr == this->_target ? 0x1 : this->_target->hash();

        this->_hashcode =
                0x9e3779b9 + (hashcode << 2) ^ (((stateHash << 4) ^ (targetHash << 3)) << 1);
    }

    bool ActivityStateAction::isValid() const {
        return (this->_target == nullptr || !this->_target->getBounds()->isEmpty());
    }

    bool ActivityStateAction::getEnabled() const {
        return (this->_target == nullptr || this->_target->getEnabled());
    }

    uintptr_t ActivityStateAction::hash() const {
        return this->_hashcode;
    }

/// check if two actions are the same
/// \param action the action to compare with
/// \return if two actions are the same return true
    bool ActivityStateAction::operator==(const ActivityStateAction &action) const {
        return this->hash() == action.hash();
    }

    bool ActivityStateAction::operator<(const ActivityStateAction &action) const {
        return this->hash() < action.hash();
    }


    bool ActivityStateAction::isEmpty() const {
        auto rect = this->getTarget()->getBounds();
        return rect->isEmpty();
    }

    ActivityStateAction::~ActivityStateAction() {
        this->_state.reset();
        this->_target = nullptr;
    }

    OperatePtr ActivityStateAction::toOperate() const {
        auto opt = Action::toOperate(); // call base virtual method
        opt->sid = this->getState().expired() ? "" : this->getState().lock()->getId();
        if (this->getTarget()) {
            opt->pos = *(this->getTarget()->getBounds());
            opt->editable = this->getTarget()->isEditable();
        }
        return opt;
    }

    std::string ActivityStateAction::toString() const {
        std::stringstream strs;
        strs << "{" << Action::toString() <<
             ", state: " << (this->_state.expired() ? "" : this->_state.lock()->getId()) <<
             ", node: " << (this->_target ? this->_target->toString() : "") << "}";
        return strs.str();
    }

    std::string Action::toDescription(const std::string& html)
    {
        return std::string(actName[this->_actionType]);
    }

    Action::Action(Action &other): _actionType(other._actionType), _inputText(other._inputText),
    _qValue(other._qValue) {

    }

    std::string ActivityStateAction::toDescription(const std::string& html)
    {
        if (!this->_target)
        {
            return std::string(actName[this->_actionType]);
        }
        std::string description;
        // Try to get text, content-desc, resource-id
        std::string info = getWidgetInfo();

        // Get the target type based on class name
        std::string viewType;
        std::string className = this->_target->getClass();//getWidgetClass();
        size_t dotPosition = className.find_last_of('.');
        if (dotPosition != std::string::npos) {
            // Extract the substring starting from the character after '.'
            viewType = className.substr(dotPosition + 1);
        }

        std::string subject = html.empty() ? viewType + "(" + info + ")" : html;
        // scroll: scroll ScrollView from top to down
        if (this->_actionType >= ActionType::SCROLL_TOP_DOWN && 
                this->_actionType<= ActionType::SCROLL_BOTTOM_UP_N)
        {
            description = "scroll " + subject;
            switch (this->_actionType)
            {
                case ActionType::SCROLL_TOP_DOWN: {
                    description.append(" from top to down");
                    break;
                }
                case ActionType::SCROLL_BOTTOM_UP: {
                    description.append(" from bottom to top");
                    break;
                }
                case ActionType::SCROLL_LEFT_RIGHT: {
                    description.append(" from left to right");
                    break;
                }
                case ActionType::SCROLL_RIGHT_LEFT: {
                    description.append(" from right to left");
                    break;
                }
                case ActionType::SCROLL_BOTTOM_UP_N: {
                    description.append(" from bottom to top");
                    break;
                }
            }
        }
        // input: Input xxx to (res-id:search_text)
        else if (hasInput()) {
            description = "Input '" + _inputText + "' to ";
            description += subject;
        }
        // click or other action: click Button with text: hello
        else {
            description = actName[this->_actionType] + " " + subject;
        }
        
        return description;
    }

    std::string ActivityStateAction::getWidgetInfo()
    {
        std::string info;
        WidgetPtr widget = this->_target;
        while (info.empty())
        {
            info = widget->getText();
            if (info.empty()) {
                info = widget->getContentDesc();
                if (info.empty()) {
                    info = widget->getResourceID();
                    if (!info.empty()) {
                        info.insert(0, "resource-id: ");
                    }
                    else {
                        widget = widget->getParent();
                        if (!widget) { break; }
                    }
                }
                else {
                    info.insert(0, "content-desc: ");
                }
            }
            else {
                info.insert(0, "text: ");
            }
        }
        if (info.empty()) {
            info = "no info";
        }
        return info;
    }

    std::string ActivityStateAction::getWidgetClass()
    {
        std::string className;
        WidgetPtr widget = this->_target;
        while (className.empty())
        {
            if (!widget) { break; }
            className = widget->getClass();
            widget = widget->getParent();
        }
        return className;
    }

    void ActivityStateAction::visit(time_t timestamp) {
        Node::visit(timestamp);
        if (_functionListener) {
            _functionListener->onActionExecuted(shared_from_this());
        }
    }

    void ActivityStateAction::setListener(FunctionListenerPtr listener) {
        _functionListener = listener;
    }

    ActivityStateAction::ActivityStateAction(ActivityStateAction &other): Action(other),
    _hashcode(other._hashcode), _whichWidget(other._whichWidget),
    _functionListener(other._functionListener), _state(other._state), _target(other._target) {
    }

}

#endif // Action_CPP_H_
