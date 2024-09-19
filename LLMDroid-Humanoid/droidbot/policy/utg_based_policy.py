import json
import os.path
import sys
import time
from abc import abstractmethod
from typing import Optional
from enum import Enum
from concurrent.futures import Future

from .input_policy import InputPolicy
from ..desc.action_type import ActionType
from ..input_event import KeyEvent, InputEvent
from ..desc.utg import UTG, Step, Path
from ..desc.device_state import DeviceState
from ..desc.state_cluster import StateCluster
from .llm_agent import QuestionMode, QuestionPayload, LLMAgent
from .cv_monitor import CodeCoverageMonitor


class Mode(Enum):
    EXPLORE = 0
    ASK_GUIDANCE = 1
    NAVIGATE = 2
    TEST_FUNCTION = 3


class UtgBasedInputPolicy(InputPolicy):
    """
    state-based input policy
    """

    def __init__(self, device, app, random_input):
        super(UtgBasedInputPolicy, self).__init__(device, app)
        self.random_input = random_input
        self.script = None
        self.master = None
        self.script_events = []
        self.last_event = None
        self.last_state = None
        self.current_state: Optional[DeviceState] = None
        self.utg = UTG(device=device, app=app, random_input=random_input)
        self.script_event_idx = 0
        if self.device.humanoid is not None:
            self.humanoid_view_trees = []
            self.humanoid_events = []

        # llmdroid
        self.__llm_agent = LLMAgent(app=app, utg=self.utg)
        self.__current_mode: Mode = Mode.EXPLORE

        # cv monitor
        with open('./config.json', 'r', encoding='utf-8') as file:
            config = json.load(file)
            log_identifier = config['Tag']
            total = config['TotalMethod']
        self.__cv_monitor = CodeCoverageMonitor(save_dir=app.output_dir, wsize=80, tag=log_identifier, total=total)
        self.__use_cv = True
        if self.__use_cv:
            self.__cv_monitor.start_logcat_listener()

        # guidance and navigation
        # for one guidance
        self.__navigate_target: int = -1
        self.__function_to_test: str = ''
        self.__current_path: Path = None
        self.__paths = []
        # Each time you enter navigation mode, the number of navigation failures for each target is up to 3 times.
        # Navigation attempts are made up to three times per target, depending on the number of paths calculated.
        # When all paths fail, it is counted as a failure to navigate to the target, and the value of this variable is +1.
        self.__failure_in_single_round = 0

        # for all guidance
        self.__total_guide_times = 0
        self.__successful_guide_times = 0
        self.__GUIDANCE_INTERVAL = 150
        self.__next_stage_time = time.time() + self.__GUIDANCE_INTERVAL

        self.__future: Future
        self.__reset_future()

        self.min_similarity: float = 0.50001
        self.max_similarity: float = 0.6
        self.__current_similarity_check: float = self.max_similarity

        # test function
        self.__event_by_llm: Optional[InputEvent] = None

    def generate_event(self):
        """
        generate an event
        @return:
        """

        # Get current device state
        self.current_state: DeviceState = self.device.get_current_state()

        if self.current_state is None:
            import time
            self.logger.warning("Current State is None, sleep 5s and press BACK!!!")
            time.sleep(5)
            return KeyEvent(name="BACK")

        self.__update_utg()

        # LLMDroid
        self.__process_state()

        # update last view trees for humanoid
        if self.device.humanoid is not None:
            self.humanoid_view_trees = self.humanoid_view_trees + [self.current_state.view_tree]
            if len(self.humanoid_view_trees) > 4:
                self.humanoid_view_trees = self.humanoid_view_trees[1:]

        self.__switch_mode()
        event = self.__resolve_new_action()

        # update last events for humanoid
        if self.device.humanoid is not None:
            self.humanoid_events = self.humanoid_events + [event]
            if len(self.humanoid_events) > 3:
                self.humanoid_events = self.humanoid_events[1:]

        self.last_state = self.current_state
        self.last_event = event

        event.visit()
        self.current_state.print_events()
        return event

    def __update_utg(self):
        self.current_state = self.utg.add_transition(self.last_event, self.last_state, self.current_state)

    def __process_state(self):
        self.logger.debug(f'State{self.current_state.get_id()} HTML, Activity:{self.current_state.foreground_activity}\n{self.current_state.to_html()}')

        cluster = self.__find_most_similar()
        if cluster:
            cluster.add_state(self.current_state)
            self.current_state.set_cluster(cluster)
            self.logger.info(f"State{self.current_state.get_id()} belongs to previous Cluster{cluster.get_id()}")
        else:
            cluster = StateCluster(self.current_state, len(self.utg.clusters))
            self.utg.clusters.append(cluster)
            self.current_state.set_cluster(cluster)
            self.logger.info(f"State{self.current_state.get_id()} belongs to New Cluster{cluster.get_id()}")
            # ask gpt for StateOverview
            if self.current_state.foreground_activity.startswith(self.app.package_name):
                self.__llm_agent.push_to_queue(QuestionPayload(QuestionMode.OVERVIEW, cluster=cluster))

        self.utg.current_cluster = cluster

    def __find_most_similar(self) -> Optional[StateCluster]:
        threshold = 0.6
        # First compare with the current merged state
        if self.utg.current_cluster is None:
            return None
        similarity = self.current_state.compute_similarity(self.utg.current_cluster.get_root_state())
        self.logger.debug(f"Similarity between State{self.current_state.get_id()} and CurrentCluster{self.utg.current_cluster.get_id()}'s root state is {similarity}")
        if similarity > threshold:
            return self.utg.current_cluster

        # If the similarity is less than the threshold
        # Traverse all mergedStates in mergedStateGraph and calculate similarity with the root node therein
        # Select the one with the greatest similarity to return
        max_similarity = 0
        ret: Optional[StateCluster] = None
        for cluster in self.utg.clusters:
            root_state = cluster.get_root_state()
            similarity = self.current_state.compute_similarity(root_state)
            # self.logger.debug(f"Similarity between State{self.current_state.get_id()} and Cluster{cluster.get_id()}'s root state{root_state.get_id()} is {similarity}")
            if similarity > threshold and similarity > max_similarity:
                max_similarity = similarity
                ret = cluster
        return ret

    def __check_should_wait(self) -> bool:
        low_growth_rate = False
        if self.__use_cv:
            low_growth_rate = self.__cv_monitor.check_low_growth_rate()
        else:
            # by time
            if time.time() > self.__next_stage_time:
                low_growth_rate = True
            else:
                self.logger.info(f"About {self.__next_stage_time - time.time()}s left")
        if low_growth_rate:
            self.logger.info("Low growth rate detected!")
        # return False
        return low_growth_rate

    def __guide_check(self):
        is_correct = False
        target_id = -1

        while len(self.__current_path.steps) > 0:
            current_step = self.__current_path.steps[0]
            target_id = current_step.node
            self.__current_path.steps.pop(0)
            # current step is correct
            if self.current_state.get_id() == target_id or current_step.event.get_action_type() == ActionType.STOP:
                is_correct = True
                break
            # For the first page after the restart action, even if it is not State0, it is still considered correct.
            # The corresponding next action needs to be converted
            elif current_step.event.get_action_type() == ActionType.START:
                if len(self.__current_path.steps) == 0:
                    is_correct = True
                    break
                else:
                    self.logger.info(f"[different state] After restart, try to find similar event")
                    replace = self.current_state.find_similar_event(self.__current_path.steps[0].event)
                    if replace:
                        self.__current_path.steps[0].event = replace
                        is_correct = True
                        break
            # Check similarity
            else:
                target_state = self.utg.find_state_by_id(target_id)
                if target_state is None:
                    sys.exit()
                similarity = self.current_state.compute_similarity(target_state)
                self.logger.info(f"Similarity between State{self.current_state.get_id()} and State{target_state.get_id()} is {similarity}")
                # If the requirements are met, this step is still considered successful.
                if similarity > self.__current_similarity_check:
                    if len(self.__current_path.steps) == 0:
                        is_correct = True
                        break
                    else:
                        self.logger.info(f"[different state] try to find similar event")
                        replace = self.current_state.find_similar_event(self.__current_path.steps[0].event)
                        if replace:
                            self.__current_path.steps[0].event = replace
                            is_correct = True
                            break
                # If the requirements are not met, or a replacement event is not found, the current step is skipped.
                self.logger.info(f"Target at State{target_id}, now at State{self.current_state.get_id()}, try to skip next step")

        if is_correct:
            if len(self.__current_path.steps) > 0:
                self.logger.info(f"Navigation succeed at this step")
                return 1
            else:
                self.logger.info(f"Successfully navigate to target")
                return 2
        else:
            self.logger.info(f"Navigate failed. Target at {target_id}, now at {self.current_state.get_id()}")
            return 3
        # The state corresponding to the head of the current path is the state that should be reached.
        # The next position is the action to be executed next
        # Check if current step is successful
        # Pop the head off while checking
        # Only three statuses are returned: complete success, temporary success, and temporary failure.

    def __switch_mode(self):
        if self.__current_mode == Mode.EXPLORE:
            should_wait = self.__check_should_wait()
            if should_wait:
                self.__llm_agent.wait_until_queue_empty()
                self.__current_mode = Mode.ASK_GUIDANCE
                self.logger.info("Switch to ASK_GUIDANCE mode")
            else:
                return

        # can only enter Guidance from Explore
        if self.__current_mode == Mode.ASK_GUIDANCE:
            self.logger.info("Switch to NAVIGATE mode")
            self.__prepare_for_navigate()
            # debug
            # self.__prepare_back_to_explore()
            # self.__llm_agent.add_tested_function(self.__function_to_test)
            return

        if self.__current_mode == Mode.NAVIGATE:
            status = self.__guide_check()
            # 1.If successful but not yet reaching the target, continue navigation
            if status == 1:
                # Because the head has already been popped off in the guide_check, there is no need to pop it here again.
                return
            # 2. If successful and reaching the goal
            elif status == 2:
                # Instead of return, enter the next code block
                self.__on_navigate_over(True)
            # 3. fail
            else:
                self.__on_navigate_failed()
                return

        if self.__current_mode == Mode.TEST_FUNCTION:
            self.__prepare_test_function()
            return

    def __prepare_for_navigate(self):
        self.__current_mode = Mode.NAVIGATE
        self.__total_guide_times += 1
        # ask gpt for guidance
        self.__reset_future()
        self.__llm_agent.push_to_queue(QuestionPayload(mode=QuestionMode.GUIDE))
        # get target cluster and function
        self.__navigate_target, self.__function_to_test = self.__future.result()
        # Calculate path
        paths = self.utg.get_paths(self.__navigate_target)
        # Set path
        if paths:
            self.__current_path = paths[0]
            self.__paths = paths[1:]
        else:
            self.logger.warning(f"There is no path from State{self.current_state.get_id()} to Cluster{self.__navigate_target}")
            self.__on_navigate_failed()

    def __on_navigate_failed(self):
        if self.__current_similarity_check > self.min_similarity:
            self.__current_similarity_check -= 0.05
        # Check if paths is empty
        # If not empty, replace it
        if self.__paths:
            self.__current_path = self.__paths[0]
            self.__paths.pop(0)
        # If it is empty, check the number of failures
        elif self.__failure_in_single_round < 3:
            # If less than three times, change the target. ask for guidance again
            self.__failure_in_single_round += 1
            self.__llm_agent.add_tested_function(self.__function_to_test)
            # It is considered that this function cannot be tested, and it is also marked as tested.
            cluster = self.utg.find_cluster_by_id(self.__navigate_target)
            if cluster:
                cluster.update_tested_function(self.__function_to_test)
            else:
                self.logger.warning(f"Guide failed, can't even find Cluster{self.__navigate_target}")

            self.__prepare_for_navigate()
        # If it reaches three times, it is considered completely fail.
        else:
            self.logger.info("Navigate has failed too many times")
            self.__on_navigate_over(False)

    def __on_navigate_over(self, success: bool):
        # Statistical navigation success rate
        if success:
            self.__successful_guide_times += 1
            self.__current_mode = Mode.TEST_FUNCTION
            self.logger.info("Switch to TEST_FUNCTION mode")
        else:
            self.__llm_agent.add_tested_function(self.__function_to_test)
            self.__prepare_back_to_explore()
        self.logger.info(
            f"[GUIDE STAT] {self.__successful_guide_times}/{self.__total_guide_times} success rate:{self.__successful_guide_times / self.__total_guide_times}")

        # Clear related data
        self.__navigate_target = -1
        # self.__function_to_test = ''  # still useful when testing function
        self.__current_path = None
        self.__paths = []
        self.__failure_in_single_round = 0
        self.__current_similarity_check = self.max_similarity

    def __prepare_test_function(self):
        # ask gpt to choose action
        self.__reset_future()
        self.__llm_agent.push_to_queue(QuestionPayload(mode=QuestionMode.TEST_FUNCTION, state=self.current_state))
        self.__event_by_llm = self.__future.result()

        # consider the function is tested whether succeed or not
        self.__llm_agent.add_tested_function(self.__function_to_test)
        self.utg.current_cluster.update_tested_function(self.__function_to_test)

        if self.__event_by_llm is None:
            self.logger.warning(f"LLM returns None, meaning function{self.__function_to_test} can't be tested!")

    def __prepare_back_to_explore(self):
        self.logger.info("Get ready to switch to EXPLORE mode")
        self.__current_mode = Mode.EXPLORE
        # check should wait related
        self.__cv_monitor.clear()
        # reset time
        self.__next_stage_time = time.time() + self.__GUIDANCE_INTERVAL

    def __resolve_new_action(self) -> InputEvent:
        if self.__current_mode == Mode.NAVIGATE:
            if self.__current_path and self.__current_path.steps:
                return self.__current_path.steps[0].event
            else:
                self.logger.error("In NAVIGATE mode, but current path's steps is empty, this isn't supposed to happen")
                sys.exit(-1)

        if self.__current_mode == Mode.TEST_FUNCTION:
            self.__prepare_back_to_explore()
            if self.__event_by_llm:
                self.logger.info("TEST_FUNCTION will be over after executing event by llm")
                return self.__event_by_llm
            else:
                self.logger.warning("event by llm is None, back to EXPLORE mode")

        if self.__current_mode == Mode.EXPLORE:
            event = None

            # if the previous operation is not finished, continue
            if len(self.script_events) > self.script_event_idx:
                event = self.script_events[self.script_event_idx].get_transformed_event(self)
                self.script_event_idx += 1
                if event:
                    return event

            # First try matching a state defined in the script
            if event is None and self.script is not None:
                operation = self.script.get_operation_based_on_state(self.current_state)
                if operation is not None:
                    self.script_events = operation.events
                    # restart script
                    event = self.script_events[0].get_transformed_event(self)
                    self.script_event_idx = 1
                    if event:
                        return event

            if event is None:
                event = self.generate_event_based_on_utg()
                return event

    def __reset_future(self):
        self.__future = Future()
        self.__llm_agent.set_future(self.__future)

    def debug_states(self):
        # print cluster
        with open(os.path.join(self.app.output_dir, 'debug_state.json'), 'w', encoding='utf-8') as file:
            data = {}
            for cluster in self.utg.clusters:
                data[f"Cluster{cluster.get_id()}"] = cluster.to_json()

            file.write(json.dumps(data, indent=4, ensure_ascii=False))

    @abstractmethod
    def generate_event_based_on_utg(self):
        """
        generate an event based on UTG
        :return: InputEvent
        """
        pass