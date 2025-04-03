#ifndef StateStructure_H_
#define StateStructure_H_

#include "Element.h"
#include <stack>

namespace fastbotx {


    class StateStructure {
    public:
        /**
         * @brief Generate a string describing the page to gpt, obtained by traversing the ElementInState node
         *
         * @return std::string string describing the page to gpt
         */
        std::string generateStateDescription(int id);

        const ElementPtr findElement(const uintptr_t target);

        void addChildElement(ElementPtr child);

        /**
         * @brief Starting from the root node, return the ElementInState*nodes in the order of depth-first traversal.
         * Go one step down each time you call it
         * Used to fill the _actionsInState variable into the ElementInState node
         * 
         * @return const ElementInState* Return nullptr after the traversal is completed
         */
        const ElementPtr getNext();

        const ElementPtr getFirst();

        bool shouldMerge(const ElementPtr father, const ElementPtr child);

        ElementPtr _rootElement;

        std::map<uintptr_t, ElementPtr> _elementMap;

        bool insertElement(ElementPtr element);

        /**
         * @note call from child thread
        */
        ElementPtr findElementById(int id);

    private:
        std::string _stateDescription;
        std::stack<ElementPtr> _stack;
        // Record the depth of recursive traversal, used to represent the structure between components
        int tabCount = 0;
        std::set<ElementPtr> _elements;

        /**
         * Generate description of an element 
         * call generateElementInfo and generateActionList inside
         * the result contains '\\n' at the end
        */
        void generateElementDescription(const ElementPtr target, int& actionId, int depth, int& count);

        /**
         * @brief generates a list of actions that can be performed by a given Element.
         * Format:
         * Number + action performed
         * Example: which can click(0) or scroll(1)
         * @return std::string
         * @deprecated
         */
        std::string generateActionList(const ElementPtr target);

        void addTab();
        
    };
}

#endif //StateStructure_H_