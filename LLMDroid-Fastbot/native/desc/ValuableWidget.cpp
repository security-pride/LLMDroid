#ifndef ValuableWidget_CPP_
#define ValuableWidget_CPP_

#include "ValuableWidget.h"

namespace fastbotx {

    ValuableWidget::ValuableWidget(WidgetPtr widget)
    {
        
        fillDetails(widget);

        // get top value of bounds
        RectPtr rect = widget->getBounds();
        _top = rect ? rect->top : 0;

        //computeHash
        _hashcode = widget->getBounds()->hash2();
    }

    void ValuableWidget::fillDetails(WidgetPtr widget)
    {
        _widgets.insert(widget);
        _classes.insert(widget->getClass());
        if (widget->hasAction())
        {
            std::set<ActionType> tmp = widget->getActions();
            _actions.insert(tmp.begin(), tmp.end());
        }
        else
        {
            _info.insert(widget->getTextualInfo());
        }
    }

    void ValuableWidget::computeHash(RectPtr rect)
    {

        // Generate a hash based on the widget's bound
        // uintptr_t' length: 8bytes 64bits
        // top、bottom、left、right each occupy 16bit
        uintptr_t hashcode1 = (uintptr_t)(rect->top) << 48;
        uintptr_t hashcode2 = (uintptr_t)(rect->bottom) << 32;
        uintptr_t hashcode3 = (uintptr_t)(rect->left) << 16;
        uintptr_t hashcode4 = (uintptr_t)(rect->right);
        this->_hashcode = hashcode1 | hashcode2 | hashcode3 | hashcode4;
    }

    std::string ValuableWidget::toDescription()
    {
        std::string desc = generateClass();
        desc.append(std::to_string(_widgets.size()));
        std::string res_id = generateResId();
        if (!res_id.empty()) {
            desc.append(res_id);
        }
        if (!_actions.empty())
        {
            desc.append("(");
            for (auto widget : _widgets)
            {
                std::string tmp = widget->getDescriptionInfo();
                if (!tmp.empty())
                {
                    desc.append(tmp).append(" ");
                }
            }
            desc.append(")");
            desc.append(generateAction());
        }
        if (!_info.empty())
        {
            desc.append("(");
            for (auto info : _info)
            {
                desc.append(info).append(" ");
            }
            desc.append(")");
        }
        desc.append("\n");
        return desc;
    }

    std::string ValuableWidget::generateClass()
    {
        std::string str;
        for (auto clazz : _classes) {
            str.append(clazz).append(" ");
        }
        return str;
    }

    std::string ValuableWidget::generateAction()
    {
        std::string str = " which can ";
        for (auto action: _actions)
        {
            str.append(actName[action]).append(" ");
        }
        return str;
    }

    std::string ValuableWidget::generateResId()
    {
        std::string str;
        std::string res_id = (*(_widgets.begin()))->getResourceID();
        if (res_id.empty()) {
            return str;
        }
        str.append("(resource-id:").append(res_id).append(")");
        return str;
    }


    ValuableWidget::~ValuableWidget()
    {
        return;
    }

}

#endif