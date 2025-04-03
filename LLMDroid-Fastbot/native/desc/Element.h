/*
 * This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
 */
/**
 * @authors Jianqiang Guo, Yuhui Su, Zhao Zhang
 */
#ifndef Element_H_
#define Element_H_

#include "../Base.h"
#include <string>
#include <utility>
#include <vector>
#include <memory>
#include <functional>

namespace tinyxml2 {
    class XMLElement;

    class XMLDocument;
}


namespace fastbotx {

    class Widget;
    typedef std::shared_ptr<Widget> WidgetPtr;
    class Element;
    typedef std::shared_ptr<Element> ElementPtr;

    class Xpath {
    public:
        Xpath();

        explicit Xpath(const std::string &xpathString);

        std::string clazz;
        std::string resourceID;
        std::string text;
        std::string contentDescription;
        int index;
        bool operationAND;

        std::string toString() const { return _xpathStr; }

    private:
        std::string _xpathStr;
    };

    typedef std::shared_ptr<Xpath> XpathPtr;


///sdcard/max.feedmodel/structure
////sdcard/max.tree.pruning
// GUITreeNode
    typedef std::pair<int, fastbotx::ActionType> ActionInState;

    class Element : public Serializable {
    public:
        Element(int id);

        bool matchXpathSelector(const XpathPtr &xpathSelector) const;

        void deleteElement();


        bool isWebView() const;

        bool isEditText() const;

        const std::vector<std::shared_ptr<Element> > &
        getChildren() const { return this->_children; }

        // recursive get elements depends func
        void recursiveElements(const std::function<bool(std::shared_ptr<Element>)> &func,
                               std::vector<std::shared_ptr<Element>> &result) const;

        void recursiveDoElements(const std::function<void(std::shared_ptr<Element>)> &doFunc);

        std::weak_ptr<Element> getParent() const { return this->_parent; }

        const std::string &getClassname() const { return this->_classname; }

        const std::string &getResourceID() const { return this->_resourceID; }

        const std::string getClassnameTrunc() const;

        const std::string getResourceIDTrunc() const;

        const std::string &getText() const { return this->_text; }

        const std::string &getContentDesc() const { return this->_contentDesc; }

        const std::string &getPackageName() const { return this->_packageName; }

        RectPtr getBounds() const { return this->_bounds; };

        int getIndex() const { return this->_index; }

        bool getClickable() const { return this->_clickable; }

        bool getLongClickable() const { return this->_longClickable; }

        bool getCheckable() const { return this->_checkable; }

        bool getScrollable() const { return this->_scrollable; }

        bool getEnable() const { return this->_enabled; }

        ScrollType getScrollType() const;

        // reset properties, in Preference
        void reSetResourceID(const std::string &resourceID) { this->_resourceID = resourceID; }

        void reSetContentDesc(const std::string &content) { this->_contentDesc = content; }

        void reSetText(const std::string &text) { this->_text = text; }

        void reSetIndex(const int &index) { this->_index = index; }

        void reSetClassname(const std::string &className) { this->_classname = className; }

        void reSetClickable(bool clickable) { this->_clickable = clickable; }

        void reSetScrollable(bool scrollable) { this->_scrollable = scrollable; }

        void reSetEnabled(bool enable) { this->_enabled = enable; }

        void reSetBounds(RectPtr rect) { this->_bounds = std::move(rect); }

        void reSetParent(const std::shared_ptr<Element> &parent) { this->_parent = parent; }

        void reAddChild(const std::shared_ptr<Element> &child) {
            this->_children.emplace_back(child);
        }

        std::string toJson() const;

        std::string toXML() const;

        void fromJson(const std::string &jsonData);

        std::string toString() const override;

        static std::shared_ptr<Element> createFromXml(const std::string &xmlContent);

        static std::shared_ptr<Element> createFromXml(const tinyxml2::XMLDocument &doc);

        long hash(bool recursive = true);

        std::string validText;

        virtual ~Element();

        const std::vector<ActionInState>& getActions() const {
            return this->_actionsInState;
        }

        void addAction(ActionInState action);

        HTML_CLASS getHtmlClass();

        std::string toHTML(const std::vector<ElementPtr>& elementToMerge, bool noChild, int actionId = -1);

        std::string getHtmlSpecialAttribute(HTML_CLASS html_class);

        void setId(int id) { _id = id; }

        int getId() { return _id; }

        void setWidget(WidgetPtr widget) { _widget = widget; }

        WidgetPtr getWidget() { return _widget; }

    protected:
        void fromXMLNode(const tinyxml2::XMLElement *xmlNode,
                         const std::shared_ptr<Element> &parentOfNode, int& count);

        void fromXml(const tinyxml2::XMLDocument &nodeOfDoc,
                     const std::shared_ptr<Element> &parentOfNode, int& count);

        void recursiveToXML(tinyxml2::XMLElement *xml, const Element *elm) const;

        std::string _resourceID;
        std::string _classname;
        std::string _packageName;
        std::string _text;
        std::string _contentDesc;
        std::string _inputText;
        std::string _activity;

        bool _enabled;
        bool _checked;
        bool _checkable;
        bool _clickable;
        bool _focusable;
        bool _scrollable;
        bool _longClickable;
        int _childCount;
        bool _focused;
        int _index;
        bool _password;
        bool _selected;
        bool _isEditable;

        RectPtr _bounds;
        std::vector<std::shared_ptr<Element> > _children;
        std::weak_ptr<Element> _parent;

        std::vector<ActionInState> _actionsInState;
        int _id;
        WidgetPtr _widget;

        // a construct helper
        static bool _allClickableFalse;
    };


}

#endif //Element_H_
