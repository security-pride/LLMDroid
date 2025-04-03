import os.path
import sys
from enum import Enum
import threading
import json
from concurrent.futures import Future
import time
from queue import Queue, Empty
from typing import Optional, TYPE_CHECKING, TypedDict
from openai import OpenAI

from ..desc.action_type import ActionType
from ..utils import save_content_to_file
from .prompt import *
from ..input_event import SetTextEvent

if TYPE_CHECKING:
    from ..desc.state_cluster import StateCluster
    from ..desc.device_state import DeviceState
    from ..app import App
    from ..desc.utg import UTG
    from ..desc.widget import Widget

from ..global_log import get_logger


class QuestionMode(Enum):
    OVERVIEW = 0
    GUIDE = 1
    TEST_FUNCTION = 2
    EXPLORE = 3
    REANALYSIS = 4


class QuestionPayload:
    def __init__(self, mode, cluster: Optional['StateCluster'] = None,
                 state: Optional['DeviceState'] = None,
                 first_func_execution: bool = True):
        self.mode: QuestionMode = mode
        self.cluster: Optional['StateCluster'] = cluster
        self.state: Optional['DeviceState'] = state
        self.first_func_execution: bool = first_func_execution


class TopCluster(TypedDict):
    Overview: str
    FunctionList: list[str]


class WidgetInfo(TypedDict):
    function: str
    state: 'DeviceState'
    importance: int
    widget: 'Widget'


class LLMAgent:

    MODEL_STR = 'gpt-4o'

    def __init__(self, app: 'App', utg: 'UTG'):
        self.logger = get_logger()
        self.__app: 'App' = app
        self.__utg: 'UTG' = utg
        self.__QA_file = os.path.join(self.__app.output_dir, 'LLM_QA.txt')
        with open(self.__QA_file, 'w', encoding='utf-8') as file:
            file.write(f"package: {self.__app.package_name}\n")
            file.write('=' * 20 + '\n')
        self.__start_prompt = ''
        # read from config file
        with open('./config.json', mode='r', encoding='utf-8') as file:
            config = json.load(file)
            self.logger.info(f"config.json:\n{json.dumps(config, indent=4)}\n")
            self.__app_name = config['AppName']
            self.__app_desc = config['Description']
            self.__api_key = config['ApiKey']
            self.__start_prompt = f"I'm now testing an app called {self.__app_name} on Android.\n{self.__app_desc}\n"
        # init client
        self.__client = OpenAI(api_key=self.__api_key)
        self.__init_client()

        # overview
        self.__top_valued_cluster: list['StateCluster'] = []
        self.__p2: int = 10

        self.__future: Future = None

        self.__tested_functions: set[str] = set()
        self.__target_id: int = -1
        self.__target_func: str = ''
        self.__executed_events: list[str] = []

        #
        self.__queue = Queue()
        self.__low_queue = Queue()
        self.__question_remained: int = 0
        self.__question_remained_lock = threading.Lock()

        self.__work_thread = threading.Thread(target=self.__work_loop)
        self.__work_thread.setDaemon(True)
        self.__work_thread.start()
        self.logger.info("Start child thread")

    def __init_client(self):
        # self.__client.api_key = ''
        self.__client.base_url = 'https://oneapi.xty.app/v1'
        self.__client.timeout = 30000

    def is_child_thread_alive(self) -> bool:
        return self.__work_thread.is_alive()

    def push_to_queue(self, payload: QuestionPayload):
        """
        call from main thread
        """

        if payload.mode != QuestionMode.REANALYSIS:
            with self.__question_remained_lock:
                self.__question_remained += 1
            self.__queue.put(payload)
            self.logger.info(f"Push a question to high priority queue, remains: {self.__queue.qsize()}")
        else:
            if payload.cluster in self.__top_valued_cluster[:self.__p2]:
                with self.__question_remained_lock:
                    self.__question_remained += 1
                self.__low_queue.put(payload)
                self.logger.info(f"Push a question to low priority queue, remains: {self.__low_queue.qsize()}")

    def __work_loop(self):
        while True:
            try:
                payload: Optional[QuestionPayload] = None
                try:
                    payload = self.__queue.get(timeout=1)
                    self.logger.info(f"Consumed from high priority queue")
                except Empty:
                    try:
                        payload = self.__low_queue.get(timeout=1)
                        self.logger.info(f"Consumed from low priority queue")
                    except Empty:
                        pass

                if payload:
                    if payload.mode == QuestionMode.OVERVIEW:
                        self.__ask_for_overview(payload)
                    elif payload.mode == QuestionMode.GUIDE:
                        self.__ask_for_guidance(payload)
                    elif payload.mode == QuestionMode.TEST_FUNCTION:
                        self.__ask_for_test_function(payload)
                    elif payload.mode == QuestionMode.REANALYSIS:
                        self.__ask_for_reanalysis(payload)

                    with self.__question_remained_lock:
                        self.__question_remained -= 1
            except Exception as e:
                self.logger.error(f"Child thread error: {e}")
                import traceback
                traceback.print_exc()

    def wait_until_queue_empty(self):
        self.logger.info(f"Wait until queue is empty...")
        while True:
            with self.__question_remained_lock:
                if self.__question_remained == 0:
                    self.logger.info(f"question all done")
                    break
                else:
                    self.logger.info(f"Question remains: {self.__question_remained}")
            time.sleep(3)

    def __ask_for_overview(self, payload: QuestionPayload):
        if payload.cluster is None:
            self.logger.warning("Payload's state is None, skip")

        self.logger.info(f"Ask for StateCluster's overview")
        # time.sleep(3)
        # self.logger.debug("debug: Ask for StateCluster's overview, over")
        # return
        prompt = self.__start_prompt + function_explanation + input_explanation_overview
        prompt += "\n```HTML Description\n"
        prompt += payload.cluster.to_description()[:7000] + "\n"
        prompt += "```\n"

        if len(self.__top_valued_cluster) >= 5:
            # ask gpt to maintain the M list
            prompt += required_output_overview
            count = 0
            top5: dict[str, TopCluster] = {}
            for cluster in self.__top_valued_cluster:
                if cluster.has_untested_function():
                    cluster.write_overview_top5_tojson(top5)
                    count += 1
                    if count == 5:
                        break
            prompt += f"Current State: {payload.cluster.get_id()}\n"
            prompt += f"Five other States:\n{json.dumps(top5, ensure_ascii=False, indent=4)}\n"
            prompt += required_output_overview_summary + answer_format_overview
        else:
            prompt += required_output_overview2 + required_output_overview_summary2 + answer_format_overview2

        json_resp = self.__get_response(prompt)

        # process response
        payload.cluster.update_from_overview(json_resp)
        if len(self.__top_valued_cluster) >= 5:
            top_list: list[int] = []
            key = "Top5" if "Top5" in json_resp else "Top 5"
            top_list = json_resp[key]
            # Store the first 5 elements of _topValuedMergedState
            original_first5 = self.__top_valued_cluster[:5]
            # Replace the first 5 elements of _topValuedMergedState with elements from topList
            for i, elem in enumerate(top_list):
                if isinstance(elem, int):
                    cluster = self.__utg.find_cluster_by_id(elem)
                    self.__top_valued_cluster[i] = cluster
                elif isinstance(elem, str):
                    cluster = self.__utg.find_cluster_by_id(int(elem[5:]))
                    self.__top_valued_cluster[i] = cluster
                else:
                    self.logger.warning(f"LLM's response is neither an int list nor string list")
            # Find elements in originalFirstFive that are not in topList
            cluster_to_insert = []
            for cluster in original_first5:
                def find_by_id(target) -> bool:
                    for elem in top_list:
                        if isinstance(elem, int):
                            if elem == target:
                                return True
                        elif isinstance(elem, str):
                            if target == int(elem[5:]):
                                return True
                    return False
                if find_by_id(cluster.get_id()):
                    cluster_to_insert.append(cluster)

            # Insert the elementsToInsert after the 5th element of _topValuedMergedState
            self.__top_valued_cluster = self.__top_valued_cluster[:5] + cluster_to_insert + self.__top_valued_cluster[5:]
        else:
            self.__top_valued_cluster.append(payload.cluster)

    def __ask_for_guidance(self, payload: QuestionPayload):
        self.logger.info("Ask for guidance")
        prompt = self.__start_prompt + input_explanation_guidance

        cluster_info = {}
        for cluster in self.__top_valued_cluster[:self.__p2]:
            if cluster.has_untested_function():
                cluster.write_overview_top5_tojson(cluster_info)
        # if all clusters' functions are tested, just write five functions for each cluster
        if len(cluster_info) == 0:
            self.logger.warning("[Ask for guidance] all clusters' functions are tested")
            for cluster in self.__top_valued_cluster[:self.__p2]:
                cluster.write_overview_top5_tojson(cluster_info, ignore_importance=True)
        prompt += f"\n```State Information\n{json.dumps(cluster_info, ensure_ascii=False, indent=4)}\n```\n"

        # tested functions
        prompt += required_output_guidance1 + "{"
        for func in self.__tested_functions:
            prompt += f"{func}, "
        prompt += "}" + required_output_guidance2
        prompt += answer_format_guidance

        json_resp = self.__get_response(prompt)
        self.__target_id = int(json_resp['Target State'][5:])
        self.__target_func = json_resp['Target Function']

        self.logger.info(f"Try to find path from Cluster{self.__utg.current_cluster.get_id()} to Cluster{self.__target_id}")
        # find cluster by id
        target_cluster: Optional['StateCluster'] = self.__utg.find_cluster_by_id(self.__target_id)
        if target_cluster is None:
            self.__future.set_result((-1, self.__target_func))
        else:
            # int, str
            target_state = target_cluster.get_target_state(self.__target_func)
            self.__future.set_result((target_state.get_id() if target_state else -1, self.__target_func))

    def __ask_for_test_function(self, payload: QuestionPayload):
        self.logger.info("Ask for testing function")

        prompt = self.__start_prompt + input_explanation_test
        html = payload.state.to_html()
        prompt += f"\n```Page Description\n{html}```\n"

        # function to test
        prompt += f"The target function I want to test is: {self.__target_func}\n"

        # provide executed events
        if self.__executed_events:
            joined = ',\n'.join(self.__executed_events)
            prompt += f"\nI have already executed: [{joined}]\n"

        # Ask which widget to click
        prompt += f"{required_output_test}\n{answer_format_test}\n"
        if self.__executed_events:
            prompt += answer_format_test_empty

        json_resp = self.__get_response(prompt)

        widget_id = int(json_resp['Element Id'])
        act_type = ActionType.get_type_by_value(int(json_resp['Action Type']) + ActionType.CLICK.value)

        if widget_id == -1:
            self.__future.set_result(None)
            return

        ret = payload.state.find_event_by_id_and_type(widget_id, act_type)
        if ret:
            # set text to InputEvent
            if 'Input' in json_resp:
                input_text = json_resp['Input']
                if isinstance(ret, SetTextEvent):
                    ret.text = input_text
                else:
                    self.logger.warning(f"Can't set text to event:{ret.to_description()}")
            # extract corresponding line in html
            for line in html.splitlines():
                if f"id={widget_id}" in line:
                    s = ret.to_description(html=line.split('\t')[-1])
                    self.logger.debug(s)
                    self.__executed_events.append(ret.to_description(html=line.split('\t')[-1]))
                    break
        # None type will be handled by utg_based_policy
        self.__future.set_result(ret)

    def __ask_for_reanalysis(self, payload: QuestionPayload):
        self.logger.info(f"Ask for Reanalysis of Cluster{payload.cluster.get_id()}")

        prompt: str = self.__start_prompt + input_explanation_reanalysis1
        prompt += "```Overview and Function List\n"
        prompt += json.dumps(payload.cluster.to_json(reanalysis=True), ensure_ascii=False, indent=4)
        prompt += "\n```\n"

        prompt += input_explanation_reanalysis2
        prompt += "```Controls in HTML Description\n"

        # get different widgets
        widgets_dict: dict[int, WidgetInfo] = {}
        id = 1
        root_state = payload.cluster.get_root_state()
        for state in payload.cluster.get_states():
            # allocate different id for every widget
            for widget in state.diff_widgets(root_state):
                widgets_dict[id] = WidgetInfo(function='', state=state, importance=-1, widget=widget)
                id += 1

        if len(widgets_dict) == 0:
            self.logger.warning(f"All states are exactly the same sa root state, no different widgets to analysis")
            return

        # remove duplicate ones
        unique_widgets: dict[str, list[int]] = {}
        for id in widgets_dict.keys():
            html = widgets_dict[id]['widget'].to_html(id=0)
            if html not in unique_widgets:
                unique_widgets[html] = [id]
            else:
                unique_widgets[html].append(id)

        # generate widget list in html
        for ws in unique_widgets.values():
            widget_id = ws[0]
            prompt += widgets_dict[widget_id]['widget'].to_html(id=widget_id)

        prompt += "```\n"
        # required output and answer format
        prompt += required_output_reanalysis + answer_format_reanalysis
        # rank clusters and functions

        json_resp = self.__get_response(prompt)

        # process response
        # update function list, record corresponding state, set function to widget
        # no need to set listener to action,
        # because listener has already been set when other states joined to this cluster
        payload.cluster.update_from_reanalysis(json_resp, unique_widgets, widgets_dict)
        # TODO update top clusters

    def __get_response(self, prompt: str) -> json:
        save_content_to_file(self.__QA_file, title='Prompt', content=prompt)
        begin_stamp = time.time()
        try_times = 0
        chat_completion = None
        while try_times < 5:
            try:
                chat_completion = self.__client.chat.completions.create(
                    temperature=0,
                    messages=[
                        {
                            "role": "user",
                            "content": prompt,
                        }
                    ],
                    model=LLMAgent.MODEL_STR,
                    # other params
                )
                if chat_completion:
                    break
            except Exception as e:
                self.logger.warning(f"Exception:{e}, try to ask again in 3 seconds")
                time.sleep(3)
                try_times += 1

        if try_times == 5:
            self.logger.error("Error when getting LLM's response, stop testing!")
            sys.exit()

        # get response
        response = chat_completion.choices[0].message.content
        end_stamp = time.time()
        with open(os.path.join(self.__app.output_dir, 'LLM-Interaction.txt'), 'a') as file:
            time_difference = round(end_stamp - begin_stamp, 5)
            file.write(f"{time_difference}, {len(response)}\n")
        self.logger.info(f"Get response:\n{response}")
        save_content_to_file(self.__QA_file, title='Response', content=response)
        # Cut the part between curly brackets
        pos = response.find('{')
        if pos != -1:
            response = response[pos:]
        pos = response.rfind('}')
        if pos != -1:
            response = response[:pos + 1]

        try:
            json_resp = json.loads(response)
            return json_resp
        except Exception as e:
            self.logger.warning(f"Exception({e}) occurred when transferring llm's response to json, try to get response again")
            return self.__get_response(prompt)

    def set_future(self, future):
        self.__future = future

    def add_tested_function(self):
        """
        Add target function to tested functions in agent,
        also mark the function in the corresponding cluster as tested
        """
        self.__tested_functions.add(self.__target_func)
        cluster = self.__utg.find_cluster_by_id(self.__target_id)
        if cluster:
            cluster.update_tested_function(self.__target_func)
        else:
            self.logger.warning(f"Can't find Cluster{self.__target_id} when marking function({self.__target_func}) as tested")

    def clear_executed_events(self):
        self.__executed_events.clear()
