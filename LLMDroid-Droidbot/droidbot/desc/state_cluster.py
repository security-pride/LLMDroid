import threading

from ..global_log import get_logger
from abc import ABC, abstractmethod
from typing import TYPE_CHECKING, TypedDict, Optional
from ..policy.llm_agent import TopCluster

if TYPE_CHECKING:
    from .device_state import DeviceState
    from ..input_event import UIEvent


class FunctionDetail(TypedDict):
    widget_id: int
    importance: int
    state: 'DeviceState'


class ActionListener(ABC):
    @abstractmethod
    def on_action_executed(self, event: 'UIEvent'):
        pass


class StateCluster(ActionListener):
    def __init__(self, state: 'DeviceState', total: int):
        self.logger = get_logger()
        self.__root_state = state
        self.__states: set['DeviceState'] = set()
        self.__states.add(state)
        self.__id = total

        self.__overview: str = ''
        # key: function(str)
        # value: {importance: int(0 means tested), widget_id: corresponding widget's id}
        self.__functions: dict[str, FunctionDetail] = {}
        self.__analysed = False
        self.__lock = threading.Lock()
        self.__listener_lock = threading.Lock()
        self.__need_reanalysed: bool = False

    def get_root_state(self) -> 'DeviceState':
        return self.__root_state

    def to_json(self, reanalysis: bool = False):
        ret = {
            "Overview": self.__overview,
            "Function List": [func for func in self.__functions.keys()]
        }
        if not reanalysis:
            ret["id"] = self.__id
            ret["root"] = f"State{self.__root_state.get_id()}"
            ret["states"] = [state.get_id() for state in self.__states],
            functions = {}
            for func in self.__functions:
                functions[func] = self.__functions[func]['importance']
            ret["Function List"] = functions
        return ret

    def on_action_executed(self, event: 'UIEvent'):
        """
        call from main/child thread
        """
        # event->widget->function marked as done
        if event.get_visit_count() > 0:
            widget = event.get_target()
            if widget:
                function = widget.get_function()
                with self.__listener_lock:
                    if function:
                        if function in self.__functions:
                            self.__functions[function]['importance'] = 0
                            self.logger.info(f'Function:{function} is tested by performing {event.to_description()}')
                        else:
                            self.logger.warning(f'event({event.to_description()}) has no corresponding function')
            else:
                self.logger.warning(f'UI Event({event.to_description()}) has no target widget!')

    def update_tested_function(self, function: str):
        """
        call from main thread
        Update functions completed by llm testing (test function mode)
        """
        with self.__lock:
            if function in self.__functions:
                self.__functions[function]['importance'] = 0
            else:
                self.__functions[function] = FunctionDetail(widget_id=-1, importance=0, state=self.__root_state)

    def update_from_overview(self, answer):
        """
        call from child thread
        """
        with self.__lock:
            self.__overview = answer['Overview']
            # key:function, value: widget's id
            function_list: dict = answer['Function List']

            # Iterate through all keys of function list
            for i, key in enumerate(function_list.keys()):
                self.__functions[key] = FunctionDetail(widget_id=function_list[key], importance=len(function_list) - i,
                                                       state=self.__root_state)

            self.__set_function_to_widget(function_list)
            self.__update_completed_functions()
            self.__analysed = True

    def __set_function_to_widget(self, function_list):
        for function in function_list.keys():
            widget_id = function_list[function]
            # Set function for all widgets in the root state
            widget = self.__root_state.find_widget_by_id(widget_id)
            if widget:
                widget.set_function(function)
                # find similar widget in other states and set function
                for state in self.__states:
                    if state == self.__root_state:
                        continue
                    other_widget = state.find_similar_widget(widget)
                    if other_widget:
                        other_widget.set_function(function)
                    else:
                        self.logger.info(f"({function}:{widget_id}) can't find widget in State{state.get_id()}")
            else:
                self.logger.warning(
                    f"({function}:{widget_id}) can't find widget in Root State{self.__root_state.get_id()}")

    def __update_completed_functions(self):
        """
        For all states currently included, set listeners for actions and update completed functions.
        At this time, we have just obtained the analysis results of llm on the cluster.
        """
        for state in self.__states:
            for event in state.get_possible_input():
                # first call on_action_executed
                # to prevent the situation that on_action_executed was called twice after set_listener
                self.on_action_executed(event)
                event.set_listener(self)

    def __update_later_joined_state(self, state: 'DeviceState'):
        """
        call from main thread
        Set the function of the widget in state
        """
        self.logger.info(f"Update later joined State{state.get_id()}...")
        for widget in self.__root_state.get_all_widgets():
            function = widget.get_function()
            if not function:
                continue
            target = state.find_similar_widget(widget)
            if target:
                target.set_function(function)
                self.logger.info(
                    f"Successfully set function:{function} to widget({target.to_html()[:-1]}) in State{state.get_id()}")
            else:
                self.logger.info(f"widget({widget.to_html()[:-1]}) doesn't have similar one in State{state.get_id()}")

        # set listener to event
        for event in state.get_possible_input():
            event.set_listener(self)

    def update_from_reanalysis(self, json_resp, unique_widgets: dict[str, list[int]], widgets_dict):
        with self.__lock:
            try:
                for id_str in json_resp.keys():
                    id = int(id_str)
                    function = json_resp[id_str]
                    if function not in self.__functions:
                        # consider the importance as 1
                        self.__functions[function] = FunctionDetail(widget_id=-1, importance=1,
                                                                    state=widgets_dict[id]['state'])

                    widget_ids = unique_widgets[widgets_dict[id]['widget'].to_html(id=0)]
                    for widget_id in widget_ids:
                        # set function to widget
                        widget = widgets_dict[widget_id]['widget']
                        widget.set_function(function)
                        # mark this function as done, if the corresponding event has been executed
                        for event in widgets_dict[widget_id]['state'].find_events_by_widget(widget):
                            self.on_action_executed(event)
                        self.logger.info(f"[Reanalysis] Successfully set function({function}) to {widget.to_html()}")
            except Exception as e:
                self.logger.error(e)
            self.__need_reanalysed = False

    def add_state(self, state: 'DeviceState'):
        """
        call from main thread
        """
        with self.__lock:
            if state not in self.__states:
                self.__states.add(state)
                # For the state added later (after the current cluster has been analyzed by llm)
                if self.__analysed:
                    self.__need_reanalysed = True
                    self.__update_later_joined_state(state)

    def get_id(self) -> int:
        return self.__id

    def get_states(self) -> set['DeviceState']:
        return self.__states

    def need_reanalysed(self) -> bool:
        # add lock
        with self.__lock:
            return self.__need_reanalysed

    def to_description(self) -> str:
        """
        call from child thread
        Activity + HTML + \\n
        """
        desc = f"[Activity: {self.__root_state.foreground_activity}]\n"
        # Lock in to html to ensure thread safety
        desc += self.__root_state.to_html()
        return desc

    def has_untested_function(self) -> bool:
        """
        call from child thread
        """
        with self.__lock:
            for function in self.__functions.keys():
                if self.__functions[function]['importance'] > 0:
                    return True
            return False

    def write_overview_top5_tojson(self, top5, ignore_importance: bool = False):
        """
        call from child thread
        Write its own overview and top 5 important functions
        """
        with self.__lock:
            key = f"State{self.__id}"
            # Sort and eliminate functions with importance 0 (already executed)
            sorted_functions = self.__sort_functions_by_value()
            final_functions = []
            for func in sorted_functions[:5]:
                if self.__functions[func]['importance'] > 0 or ignore_importance:
                    final_functions.append(func)

            top5[key] = TopCluster(Overview=self.__overview, FunctionList=final_functions)

    def __sort_functions_by_value(self) -> list[str]:
        """
        call from child thread
        """
        sorted_keys = [key for key, detail in sorted(self.__functions.items(),
                                                     key=lambda item: item[1]['importance'], reverse=True)]
        return sorted_keys

    def get_target_state(self, function: str) -> Optional['DeviceState']:
        if function in self.__functions:
            state = self.__functions[function]['state']
            return state
        else:
            self.logger.warning(f"function{function} doesn't belong to any state in Cluster{self.__id}")
            return None
