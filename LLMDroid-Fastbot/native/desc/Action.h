/*
 * This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
 */
/**
 * @authors Jianqiang Guo, Yuhui Su, Zhao Zhang
 */
#ifndef Action_H_
#define Action_H_

#include "Node.h"
#include "Base.h"
#include "Widget.h"
#include "DeviceOperateWrapper.h"

#include <utility>
#include <vector>
#include <set>
#include <string>

namespace fastbotx {


    class State;

/// Base class for all Action classes
    class Action : public Node, public PriorityNode, public HashNode {
    public:
        Action();

        Action(Action& other);

        explicit Action(ActionType actionType);

        std::string toString() const override;

        virtual bool getEnabled() const { return true; }

        ActionType getActionType() const { return this->_actionType; }

        void setPriority(int priority);

        int getPriorityByActionType() const;

        bool isBack() const { return this->_actionType == ActionType::BACK; }

        bool isClick() const { return this->_actionType == ActionType::CLICK; }

        bool isNop() const { return this->_actionType == ActionType::NOP; }

        virtual bool isValid() const;

        virtual OperatePtr toOperate() const;

        uintptr_t _hashcode{};

        uintptr_t hash() const override { return _hashcode; }

        bool isModelAct() const;

        bool requireTarget() const;

        bool canStartTestApp() const;

        static std::shared_ptr<Action> NOP;
        static std::shared_ptr<Action> ACTIVATE;
        static std::shared_ptr<Action> RESTART;

        FuncGetID(Action);

        bool operator==(const Action &action);

        virtual ~Action() = default;


        virtual void setQValue(double value) { this->_qValue = static_cast<float>(value); }

        virtual double getQValue() const { return this->_qValue; }

        static int getThrottle() { return _throttle; }

        static const int DefaultValue;

        virtual std::string toDescription();

        bool hasInput() { return !_inputText.empty(); }

        std::string getInputText() { return _inputText; }

        void setInputText(std::string text) { _inputText = text; }

    protected:

        ActionType _actionType;
        static int _throttle;
        std::string _inputText;
    private:
        float _qValue;
        PropertyIDPrefix(Action);
    };

    typedef std::shared_ptr<Action> ActionPtr;

    typedef struct NetActionParameter_ {
        int throttle;
        int netActionTaskid;
        std::string algorithmString;
        std::string packageName;
        std::string token;
        std::string deviceid;
    } NetActionParam;

    class ActivityStateAction;
    class FunctionListener {
    public:
        virtual void onActionExecuted(std::shared_ptr<ActivityStateAction> action) = 0;
    };

    typedef std::shared_ptr<FunctionListener> FunctionListenerPtr;

/// Embedding an action with the whole activity state, the target widget and the action to perform.
class ActivityStateAction : public Action, public std::enable_shared_from_this<ActivityStateAction> {
    public:
        /// Embedding an action with the whole activity state, the target targetWidget and the type of action to perform.
        /// \param state
        /// \param targetWidget
        /// \param actionType
        ActivityStateAction(const std::shared_ptr<State> &state, WidgetPtr targetWidget,
                            ActionType actionType);

        ActivityStateAction(ActivityStateAction& other);

        std::weak_ptr<State> getState() const { return this->_state; }

        std::shared_ptr<Widget> getTarget() const { return this->_target; }

        bool getEnabled() const override;

        bool isValid() const override;

        // set target widget without updating hash code
        void setTarget(WidgetPtr widget) { this->_target = std::move(widget); }

        OperatePtr toOperate() const override;


        // from ResolveNode
        bool isEmpty() const;


        uintptr_t hash() const override;

        bool operator==(const ActivityStateAction &action) const;

        bool operator<(const ActivityStateAction &action) const;

        std::string toString() const override;

        std::weak_ptr<State> _state;
        std::shared_ptr<Widget> _target;
        uintptr_t _hashcode{};

        ~ActivityStateAction() override;

        void setWhichWidget(int which) { _whichWidget = which; }

        int getWhichWidget() { return _whichWidget; }

        // custom
        std::string toDescription() override;
        std::string getWidgetInfo();
        std::string getWidgetClass();

        virtual void visit(time_t timestamp) override;

        void setListener(FunctionListenerPtr listener);

    protected:
        ActivityStateAction();
        int _whichWidget = -1;
        FunctionListenerPtr _functionListener = nullptr;

    private:
    };

    typedef std::shared_ptr<ActivityStateAction> ActivityStateActionPtr;
    typedef std::vector<ActivityStateActionPtr> ActivityStateActionPtrVec;
    typedef std::set<ActivityStateActionPtr, Comparator<ActivityStateAction>> ActivityStateActionPtrSet;


}

#endif //Action_H_
