#include "StateStructure.h"

namespace fastbotx {

    const ElementPtr StateStructure::getFirst()
    {
        // Initialize stack
        // Loop through elements
        while (!this->_stack.empty()) {
            this->_stack.pop();
        }
        // Depth-first traversal: add all child nodes to the stack
        for (ElementPtr child : this->_rootElement->getChildren())
        {
            this->_stack.push(child);
        }
        return this->_rootElement;
    };

    const ElementPtr StateStructure::getNext()
    {
        if (this->_stack.empty())
        {
            return nullptr;
        }
        ElementPtr target = this->_stack.top();
        this->_stack.pop();
        for (ElementPtr child : target->getChildren())
        {
            this->_stack.push(child);
        }
        return target;
    }

    const ElementPtr StateStructure::findElement(const uintptr_t target)
    {
        auto result = this->_elementMap.find(target);
        if (result != this->_elementMap.end())
        {
            return result->second;
        }
        else {
            return nullptr;
        }
    }

    void StateStructure::addTab()
    {
        for (int i = 0; i < this->tabCount; i++)
        {
            this->_stateDescription.append("\t");
        }
    }

    bool StateStructure::shouldMerge(const ElementPtr father, const ElementPtr child)
    {
        if ((child->getChildren()).empty() && 
            child->getHtmlClass() == HTML_CLASS::P && 
            father->getHtmlClass() == HTML_CLASS::BUTTON) {
            return true;
        }
        else {
            return false;
        }
    }

    bool StateStructure::insertElement(ElementPtr element)
    {
        auto res = this->_elements.insert(element);
        return res.second;
    }

    ElementPtr StateStructure::findElementById(int id)
    {
        auto found = std::find_if(_elements.begin(), _elements.end(), [id](const ElementPtr& ptr) {
            return ptr->getId() == id;
        });
        return (found != _elements.end()) ? *found : nullptr;
    }

    std::string StateStructure::generateStateDescription(int id)
    {
        if (!_stateDescription.empty()) { return _stateDescription; }
        // begining of a state
        int actionId = 0;
        this->tabCount = 0;
        this->_stateDescription = "";
        std::vector<ElementPtr> elementToMerge;
        std::vector<ElementPtr> elementNotMerge;
        std::vector<ElementPtr> checkList(1, _rootElement);
        while (!checkList.empty()) {
            ElementPtr element = checkList[0];
            checkList.erase(checkList.begin());
            std::vector<ElementPtr> children = element->getChildren();
            //callJavaLogger(MAIN_THREAD, "element to check: %s, child size: %d", element->toHTML().c_str(), children.size());
            if (children.size() == 1 && children[0]->getHtmlClass() == HTML_CLASS::P) {
                //callJavaLogger(MAIN_THREAD, "merge element above's child");
                elementToMerge.push_back(children[0]);
                checkList.push_back(children[0]);
            }
            else {
                for (const auto& child : element->getChildren())
                {
                    if (shouldMerge(element, child)) {
                        elementToMerge.push_back(child);
                    }
                    else {
                        elementNotMerge.push_back(child);
                    }
                }
            }
        }
        // root element
        this->_stateDescription.append(generateElementInfo(this->_rootElement, elementToMerge, false, actionId));
        //this->_stateDescription.append(generateActionList(this->_rootElement));
        this->tabCount++;
        // child element
        int depth = 1;
        int count = 1;
        for (const auto &child: elementNotMerge)
        {
            generateElementDescription(child, actionId, depth, count);
        }
        // end of a state
        this->_stateDescription.append(htmlEndTag[_rootElement->getHtmlClass()]).append("\n");
        return this->_stateDescription;
    };

    void StateStructure::generateElementDescription(const ElementPtr target, int& actionId, int depth, int& count)
    {
        if (depth >= 25 || count >= 100) {
            callJavaLogger(MAIN_THREAD, "[StateStructure] depth: %d, count: %d, stop generating", depth, count);
            return;
        }
        addTab();
        std::vector<ElementPtr> elementToMerge;
        std::vector<ElementPtr> elementNotMerge;
        std::vector<ElementPtr> checkList(1, target);

        while (!checkList.empty()) {
            ElementPtr element = checkList[0];
            checkList.erase(checkList.begin());
            std::vector<ElementPtr> children = element->getChildren();
            //callJavaLogger(MAIN_THREAD, "element to check: %s, child size: %d", element->toHTML().c_str(), children.size());
            if (children.size() == 1 && children[0]->getHtmlClass() == HTML_CLASS::P) {
                //callJavaLogger(MAIN_THREAD, "merge element above's child");
                elementToMerge.push_back(children[0]);
                checkList.push_back(children[0]);
            }
            else {
                if (children.size() > 1) {
                    for (const auto& child : children) {
                        elementNotMerge.push_back(child);
                    }
                }
                else {
                    for (const auto& child : children)
                    {
                        if (shouldMerge(element, child)) {
                            elementToMerge.push_back(child);
                        }
                        else {
                            elementNotMerge.push_back(child);
                        }
                    }
                }
            }
        }
        bool noChild = elementNotMerge.empty();
        // Write the text part of the element
        count++;
        this->_stateDescription.append(generateElementInfo(target, elementToMerge, noChild, actionId));
        // Write the executable action for this element
        // this->_stateDescription.append(generateActionList(target));
        // recursion
        this->tabCount++;
        for (const auto &child: elementNotMerge)
        {
            generateElementDescription(child, actionId, depth + 1, count);
        }
        this->tabCount--;
        if (!noChild) {
            addTab();
            this->_stateDescription.append(htmlEndTag[target->getHtmlClass()]).append("\n");
        }        
    };

    std::string StateStructure::generateElementInfo(const ElementPtr target, std::vector<ElementPtr>& elementToMerge, bool noChild, int& actionId)
    {
        std::stringstream infoStr;

        HTML_CLASS html_class = target->getHtmlClass();

        infoStr <<  "<" << htmlClass[html_class] << " ";

        if (html_class != HTML_CLASS::P) {
            infoStr << "id=" << target->getId() << " ";
            // actionId++;
        }
        
        // Get the view type based on class name
        std::string className = target->getClassnameTrunc();
        if (!className.empty()) {
            infoStr << "class=\"" << className << "\" ";
        }
        else {
            std::string childClassName;
            for (const auto& child : elementToMerge) {
                childClassName = child->getClassnameTrunc();
                if (!childClassName.empty()) {
                    infoStr << "class=\"" << childClassName << "\" ";
                    break;
                }
            }
        }
        
        // resource-id
        std::string resource_id = target->getResourceIDTrunc();
        if (!resource_id.empty()) {
            infoStr << "resource-id=\"" << resource_id << "\" ";
        }
        else {
            std::string childResourceID;
            for (const auto& child : elementToMerge) {
                childResourceID = child->getResourceIDTrunc();
                if (!childResourceID.empty()) {
                    infoStr << "resource-id=\"" << childResourceID << "\" ";
                    break;
                }
            }
        }

        // content-desc(label)
        std::string description = target->getContentDesc();
        if (!description.empty()){
            infoStr << "content-desc=\"" << description << "\" ";
        }

        // Add special attributes
        infoStr << target->getHtmlSpecialAttribute(html_class);

        infoStr << ">";
        
        // text
        std::string text = target->getText();
        bool fatherEmpty = text.empty();
        if (!fatherEmpty){
            infoStr << text;
        }
        bool firstFlag = true;
        for (const auto& child : elementToMerge) {
            std::string childText = child->getText();
            if (!childText.empty()) {
                if (firstFlag && fatherEmpty) {
                    infoStr << childText;
                    firstFlag = false;
                }
                else {
                    infoStr << " <br> " << childText;
                }
            }
        }

        if (noChild) { infoStr << htmlEndTag[html_class]; }
        infoStr << "\n";

        return infoStr.str();
    };

    std::string StateStructure::generateActionList(const ElementPtr target)
    {
        std::string actionList = "\n";
        const std::vector<ActionInState> actionsInState = target->getActions();
        size_t size = actionsInState.size();
        if (size <= 0)
        {
            return actionList;
        }
        else 
        {
            actionList = "which can ";
        }
        for (int i = 0; i < size; i++)
        {
            actionList.append(fastbotx::actName[actionsInState[i].second] + "(" 
                + std::to_string(actionsInState[i].first) + ") ");
        }
        actionList.append("\n");
        return actionList;
    };
}