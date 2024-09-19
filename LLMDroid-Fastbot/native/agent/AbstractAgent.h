/*
 * This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
 */
/**
 * @authors Jianqiang Guo, Yuhui Su, Zhao Zhang
 */

#ifndef AbstractAgent_H_
#define AbstractAgent_H_


#include "utils.hpp"
#include "Base.h"
#include "Action.h"
#include "State.h"
#include "ActionFilter.h"
#include "Graph.h"
#include "GPTAgent.h"
#include "MergedState.h"
#include "CodeCoverageMonitor.h"

namespace fastbotx {

    class Model; // forward declaration
    typedef std::shared_ptr<Model> ModelPtr;

    class AbstractAgent : public GraphListener {
    public:

        virtual int getCurrentStateBlockTimes() const { return this->_currentStateBlockTimes; }

        // resolve an action and update graph;
        virtual ActionPtr resolveNewAction();


        virtual void updateStrategy() = 0;

        virtual void moveForward(StatePtr nextState);

        virtual void moveForward2();

        /**
         * @brief Determine which MergedState should be merged into.
         * Calculate similarity with the root state of MergedState
         * 
         * @return MergedStatePtr is the most similar MergedState pointer
         * null: If there are no similar nodes, a new merged state should be created.
         */
        MergedStatePtr findMostSimilar(ReuseStatePtr state);

        void processState(ReuseStatePtr state);

        void createMergedState(ReuseStatePtr state);

        void addToMergedState(ReuseStatePtr state);

        void GPTFunctionAnalysis(QuestionPayload state);

        // override
        void onAddNode(StatePtr node) override;

        explicit AbstractAgent(const ModelPtr &model, bool useCodeCoverage);

        virtual ~AbstractAgent();

        virtual AlgorithmType getAlgorithmType() { return this->_algorithmType; }

    protected:

        //AbstractAgent();

        virtual ActivityStateActionPtr handleNullAction() const;

        virtual ActionPtr selectNewAction() = 0;

        // ！implements in  subclass
        virtual void adjustActions();
       
        /**
         * @brief Update MergedStateGraph and ask for test content every once in a while.
         * Maintain a queue to record each MergedState of the test and the actions performed in the MergedState,
         * After the time is up, take out the information in this stage and construct the payload
         * 
         * @param mergedState 
         */
        void autoGraphOverview(MergedStatePtr& mergedState);

        void checkShouldWait();

        /**
         * call from main thread
        */
        void guideMode(bool failed);

        void askForGuiding(bool failed);

        void guideCheck(ReuseStatePtr state, bool isNew);

        void debugMergedStates();

        /**
         * ask gpt to decide next action
         * @return whether this function is finished testing
         */
        bool functionTest(bool firstTime);

        void functionTestStop();

        void guideModeStop(bool success);

        void resetFuture();

        /**
         * @brief Call method in java to get code coverage's growth rate
         * @note must call from main thread
         * @return
         */
        double getCodeCoverage();

        const size_t _rateCapacity = 80;
        const double _minGrowthRate = 0.05;
        std::vector<double> _growthRateWindow;
        double _currentThreshold = 0.05;
        bool _useCodeCoverage = false;
        CodeCoverageMonitor _codeCoverageMonitor;

        GraphPtr _graph;
        MergedStateGraphPtr _mergedStateGraph; //= std::make_shared<MergedStateGraph>();
        PromiseIntPtr _promiseInt = std::make_shared<std::promise<int>>();
        std::future<int> _futureInt = _promiseInt->get_future();
        GPTAgent _gptAgent;
        
        std::vector<Path> _paths;
        Path _currentPath;
        int _totalGuideTime = 0;
        int _successGuideTime = 0;
        int _guideTime = 0; // After entering navigation mode for a single time, the number of inquiries to the large model is not the number of local navigation attempts.
        int _guideTarget = 0;
        bool _guideMode = false;
        bool _functionTestMode = false;
        ActivityStateActionPtr _actionByGPT = nullptr;

        const float _maxSimilarity = 0.6;
        float _currentSimilarityCheck = 0.6;
        const float _minSimilarity = 0.49;
        
        int _totalMergedState;
        std::queue<std::tuple<MergedStatePtr, ActionPtr, bool>> _mergedStateQueue;
        double _lastGraphOverviewTime;
        const double _graphOverviewTimeWindow = 30000.0f;
        int _transitCount = 0;

        // control guide by time
        double _startTime;
        const double _runTime = 420000.0f; // 150s
        double _nextStageTime;
        bool _shouldWait;

        ActionPtr _mCurrentAction = nullptr;
        ActionPtr _mNewAction = nullptr;
        ReuseStatePtr _mCurrentState = nullptr;

        std::weak_ptr<Model> _model;
        StatePtr _lastState;
        StatePtr _currentState; // it actually represents the last state, but will be updated by _newState
        StatePtr _newState;
        ActivityStateActionPtr _lastAction; // the executed last action for reaching the last state
        ActivityStateActionPtr _currentAction; // the executed new action for reaching the new state
        ActivityStateActionPtr _newAction; // the newly chosen action among candidate actions, for reaching new state.

//    ActionRecordPtrVec  _actionHistory;

        ActionFilterPtr _validateFilter;

        long _graphStableCounter;
        long _stateStableCounter;
        long _activityStableCounter;

        bool _disableFuzz;
        bool _requestRestart;
        bool _appActivityJustStartedFromClean{};
        bool _appActivityJustStarted{};
        bool _currentStateRecovered{};
        int _currentStateBlockTimes;

        AlgorithmType _algorithmType;
    };


    typedef std::shared_ptr<AbstractAgent> AbstractAgentPtr;
    typedef std::vector<AbstractAgentPtr> AbstractAgentPtrVec;
    typedef std::map<std::string, AbstractAgentPtr> AbstractAgentPtrStrMap;
}


#endif //AbstractAgent_H_
