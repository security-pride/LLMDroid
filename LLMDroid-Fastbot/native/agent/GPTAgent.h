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
    typedef std::shared_ptr<std::future<int>> FutureIntPtr;

    enum AskModel
    {
        STATE_OVERVIEW, GRAPH_OVERVIEW, GUIDE, TEST_FUNCTION, GUIDE_FAILURE
    };
    
    struct QuestionPayload
    {
        AskModel type;
        MergedStatePtr from;
        std::map<MergedStatePtr, std::set<ActionPtr>> stateMap;
        int transitCount;
        ReuseStatePtr reuseState;
        bool flag; // GUIDE:guideFailed, TEST_FUNCTION:firstTime
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

        void resetPromise(PromiseIntPtr prom);

        std::string getFunctionToTest() { return _targetFunction; }
        
        /**
         * After askForTestFunction ends and the action is generated, it is called by the main thread.
         * Add the functions that GPT previously decided to test to the completed collection
         * @note call from main thread
        */
        void addTestedFunction();

    private:
        //std::atomic<int> _questionRemained;
        bool _saveToFile = true;
        std::ofstream _file;

        const char* _model_str[3] = {
            "gpt-3.5-turbo",
            "gpt-4o",
            "claude-3-opus-20240229"
        };

        std::string _startPrompt;
        std::string _apiKey;
        liboai::OpenAI _gpt;
        liboai::Conversation _conversation;
        const int _maxCachedConversation = 0;
        int _cachedConversation = 0;
        size_t _conversationLen = 0;
        std::queue<QuestionPayload> _stateQueue;
        std::mutex _mtx;
        std::condition_variable _cv;

        MergedStateGraphPtr _mergedStateGraph;
        std::string _mergedStateGraphString;

        PromiseIntPtr _promiseInt;

        std::mutex _questionMtx;
        int _questionRemained = 0;

        std::string _targetFunction; //Gpt in the guide determines the test function
        std::set<std::string> _testedFunctions; // All functions that have been implemented in the guide

        // state overview
        MergedStateVecPtr _topValuedMergedState = std::make_shared<MergedStateVec>();

        bool init();

        /**
         * @brief Read the queue cyclically, take one state each time, generate sub-task and executed sub-tasks, and call them by the sub-thread
         * Suspend waiting when the queue is empty,
         * Until the main thread adds elements to the queue
         */
        void pageAnalysisLoop();

        void askForFunction(QuestionPayload state);

        void askForStateOverview(QuestionPayload& payload);

        void askForGraphOverview(QuestionPayload& payload);

        void askForGuiding(QuestionPayload& payload);

        void askForTestFunction(QuestionPayload& payload);
        
        void processResponse(std::string response);

        void saveToFile(const std::string& prompt, const std::string& response);

        void saveToFile(const std::string& value, int type);

        nlohmann::ordered_json getResponse(std::string prompt, int type);
    };

}

#endif