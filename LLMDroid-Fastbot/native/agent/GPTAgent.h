#ifndef GPTAgent_H_
#define GPTAgent_H_

#include "Base.h"
#include "liboai.h"
#include "ReuseState.h"
#include <queue>
#include "MergedState.h"
#include "prompt.h"
#include <atomic>
#include <future>

#define GPT_3 0
#define GPT_4 1
#define CLAUDE 2

namespace fastbotx {

    typedef std::shared_ptr<std::promise<int>> PromiseIntPtr;
    typedef std::future<int> FutureInt;
    typedef std::shared_ptr<std::promise<std::string>> PromiseStrPtr;
    typedef std::future<std::string> FutureStr;
    typedef std::shared_ptr<std::promise<ActivityStateActionPtr>> PromiseActionPtr;
    typedef std::future<ActivityStateActionPtr> FutureAction;

    enum class AskModel
    {
        STATE_OVERVIEW, GRAPH_OVERVIEW, GUIDE, TEST_FUNCTION, GUIDE_FAILURE, REANALYSIS
    };
    
    struct QuestionPayload
    {
        AskModel type;
        MergedStatePtr from = nullptr;
        std::map<MergedStatePtr, std::set<ActionPtr>> stateMap;
        int transitCount = 0;
        ReuseStatePtr reuseState = nullptr;
        bool flag = false; // GUIDE:guideFailed, TEST_FUNCTION:firstTime
    };
    
    
    /**
     * @brief Responsible for interacting with GPT.
     *
     * Call the test thread the main thread, and the thread that interacts with gpt is called the child thread
     * 
     */
    class GPTAgent
    {
    public:
        GPTAgent(MergedStateGraphPtr& graph, PromiseIntPtr prom);
        ~GPTAgent();

        /**
         * @brief Add a page to be asked to the queue, called by the main thread.
         * Lock the queue and push a state to it
         * 
         * @param state 
         */
        void pushStateToQueue(QuestionPayload state);

        void waitUntilQueueEmpty();

        void resetPromise(PromiseIntPtr promInt, PromiseActionPtr promAction);

        std::string getFunctionToTest() { return _targetFunction; }
        
        /**
         * After askForTestFunction ends and the action is generated, it is called by the main thread.
         * Add the functions that GPT previously decided to test to the completed collection
         * @note call from main thread
        */
        void addTestedFunction();

        void clearExecutedEvents();

    private:
        //std::atomic<int> _questionRemained;
        bool _saveToFile = true;
        std::ofstream _file;
        std::ofstream _interactionFile;

        std::string _model_str = "gpt-4o-mini";

        std::string _startPrompt;
        std::string _apiKey;
        liboai::OpenAI _gpt;
        liboai::Conversation _conversation;
        const int _maxCachedConversation = 0;
        int _cachedConversation = 0;
        size_t _conversationLen = 0;
        std::queue<QuestionPayload> _stateQueue;
        std::queue<QuestionPayload> _lowQueue;
        std::mutex _mtx;
        std::condition_variable _cv;

        MergedStateGraphPtr _mergedStateGraph;
        std::string _mergedStateGraphString;

        PromiseIntPtr _promiseInt;
        PromiseStrPtr _promiseStr;
        PromiseActionPtr _promiseAction;

        std::mutex _questionMtx;
        int _questionRemained = 0;

        std::string _targetFunction; //Gpt in the guide determines the test function
        int _targetMergedStateId;
        std::set<std::string> _testedFunctions; // All functions that have been implemented in the guide
        std::vector<std::string> _executedFunctions;

        // state overview
        const unsigned long _P2 = 10;
        MergedStateVecPtr _topValuedMergedState = std::make_shared<MergedStateVec>();

        bool init();

        /**
         * @brief Read the queue cyclically, take one state each time, generate sub-task and executed sub-tasks, and call them by the sub-thread
         * Suspend waiting when the queue is empty,
         * Until the main thread adds elements to the queue
         */
        void pageAnalysisLoop();

        void askForStateOverview(QuestionPayload& payload);

        void askForGuiding(QuestionPayload& payload);

        void askForTestFunction(QuestionPayload& payload);

        void askForReanalysis(QuestionPayload& payload);

        void saveToFile(const std::string& prompt, const std::string& response);

        void saveToFile(const std::string& value, int type);

        nlohmann::ordered_json getResponse(const std::string& prompt, AskModel type);
    
        void addExecutedEvent(const std::string& html, int widget_id, ActionPtr act);
    };

}

#endif

