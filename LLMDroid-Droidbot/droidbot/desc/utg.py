import logging
import json
import os
import random
import datetime
import time
from typing import Optional, TypedDict

import networkx as nx
from ..input_event import InputEvent, IntentEvent
from .device_state import DeviceState
from .state_cluster import StateCluster
from ..global_log import get_logger
from ..desc.action_type import ActionType


class UTGEdge(TypedDict):
    event: InputEvent
    id: int
    time: float
    used: bool


class Step:
    def __init__(self, node: int, event: InputEvent, created_time: float):
        self.node: int = node
        self.event: InputEvent = event
        self.created_time: float = created_time


class Path:
    def __init__(self, length, latest_time, steps):
        self.length = length
        self.latest_time = latest_time
        self.steps: list[Step] = steps


class UTG(object):
    """
    UI transition graph
    """

    def __init__(self, device, app, random_input):
        # self.logger = logging.getLogger(self.__class__.__name__)
        self.logger = get_logger()
        self.device = device
        self.app = app
        self.random_input = random_input

        self.G = nx.DiGraph()
        self.G2 = nx.DiGraph()  # graph with same-structure states clustered
        self.llm_G = nx.DiGraph()   # LLMDroid's graph

        self.transitions = []
        self.effective_event_strs = set()
        self.ineffective_event_strs = set()
        self.explored_state_strs = set()
        self.reached_state_strs = set()
        self.reached_activities = set()

        # The first state is the state after KillAppEvent, usually the desktop
        self.first_state = None
        self.last_state = None

        self.start_time = datetime.datetime.now()

        self.clusters: list['StateCluster'] = []
        self.current_cluster: Optional['StateCluster'] = None

    @property
    def first_state_str(self):
        return self.first_state.state_str if self.first_state else None

    @property
    def last_state_str(self):
        return self.last_state.state_str if self.last_state else None

    @property
    def effective_event_count(self):
        return len(self.effective_event_strs)

    @property
    def num_transitions(self):
        return len(self.transitions)

    def add_transition(self, event, old_state: DeviceState, new_state: DeviceState) -> DeviceState:
        """
        after add_transition, self.last_state is current state
        """
        _ = self.add_node(old_state)
        current_state = self.add_node(new_state)
        if old_state is None:
            self.logger.info(f"Current State{current_state.get_id()}")
        else:
            self.logger.info(f"Last State{old_state.get_id()}, Current State{current_state.get_id()}")

        # make sure the states are not None
        if not old_state or not current_state:
            self.logger.warning("old state or new state is None!")
            return current_state

        event_str = event.get_event_str(old_state)
        self.transitions.append((old_state, event, current_state))

        if old_state.state_str == current_state.state_str:
            self.ineffective_event_strs.add(event_str)
            # If you previously performed the same action from old_state, you can reach other states.
            # But now the action can only reach itself (no change)
            # Indicating that the previous record is invalid and delete the previous record
            # delete the transitions including the event from utg
            edges_to_remove = []
            for new_state_str in self.G[old_state.state_str]:
                if event_str in self.G[old_state.state_str][new_state_str]["events"]:
                    other_state_id = self.G.nodes[new_state_str]["state"].get_id()
                    self.logger.info(f"Remove edge: State{old_state.get_id()} to State{other_state_id}")
                    events = self.G[old_state.state_str][new_state_str]["events"]
                    events.pop(event_str)
                    # If removing this edge causes events to be empty, it means that the edge between the two states should be deleted.
                    if len(events) == 0:
                        edges_to_remove.append((old_state.state_str, new_state_str))
            # Remove edges outside the loop to avoid interfering with iterator work
            for edge in edges_to_remove:
                self.G.remove_edge(*edge)

            if event_str in self.effective_event_strs:
                self.effective_event_strs.remove(event_str)
            return current_state

        self.effective_event_strs.add(event_str)

        if (old_state.state_str, current_state.state_str) not in self.G.edges():
            self.G.add_edge(old_state.state_str, current_state.state_str, events={})
        self.G[old_state.state_str][current_state.state_str]["events"][event_str] = UTGEdge(
            event=event, id=self.effective_event_count, time=time.time(), used=False
        )
        #     {
        #     "event": event,
        #     "id": self.effective_event_count,
        #     "time": time.time(),
        #     "used": False
        # }
        self.logger.info(f"Add Edge from State{old_state.get_id()} to State{current_state.get_id()}")

        if (old_state.structure_str, current_state.structure_str) not in self.G2.edges():
            self.G2.add_edge(old_state.structure_str, current_state.structure_str, events={})
        self.G2[old_state.structure_str][current_state.structure_str]["events"][event_str] = {
            "event": event,
            "id": self.effective_event_count
        }

        self.last_state = current_state
        self.__output_utg()
        return current_state

    def remove_transition(self, event, old_state, new_state):
        event_str = event.get_event_str(old_state)
        if (old_state.state_str, new_state.state_str) in self.G.edges():
            events = self.G[old_state.state_str][new_state.state_str]["events"]
            if event_str in events.keys():
                events.pop(event_str)
            if len(events) == 0:
                self.G.remove_edge(old_state.state_str, new_state.state_str)
        if (old_state.structure_str, new_state.structure_str) in self.G2.edges():
            events = self.G2[old_state.structure_str][new_state.structure_str]["events"]
            if event_str in events.keys():
                events.pop(event_str)
            if len(events) == 0:
                self.G2.remove_edge(old_state.structure_str, new_state.structure_str)

    def add_node(self, state: DeviceState) -> DeviceState:
        if not state:
            return None
        existed_state: DeviceState = None
        if state.state_str not in self.G.nodes():
            existed_state = state
            # set state's id
            state_id = len(self.G.nodes)
            self.logger.info(f"New State{state_id} added to graph")
            state.set_id(state_id)
            state.save2dir()
            self.G.add_node(state.state_str, state=state)
            if self.first_state is None:
                self.first_state = state
        else:
            existed_state = self.G.nodes[state.state_str]['state']
            # self.logger.info(f"State{existed_state.get_id()} already existed")

        if state.structure_str not in self.G2.nodes():
            self.G2.add_node(state.structure_str, states=[])
        self.G2.nodes[state.structure_str]['states'].append(state)

        if state.foreground_activity.startswith(self.app.package_name):
            self.reached_activities.add(state.foreground_activity)

        return existed_state

    def __output_utg(self):
        """
        Output current UTG to a js file
        """
        if not self.device.output_dir:
            return

        def list_to_html_table(dict_data):
            table = "<table class=\"table\">\n"
            for (key, value) in dict_data:
                table += "<tr><th>%s</th><td>%s</td></tr>\n" % (key, value)
            table += "</table>"
            return table

        utg_file_path = os.path.join(self.device.output_dir, "utg.js")
        utg_file = open(utg_file_path, "w")
        utg_nodes = []
        utg_edges = []
        for state_str in self.G.nodes():
            state = self.G.nodes[state_str]["state"]
            package_name = state.foreground_activity.split("/")[0]
            activity_name = state.foreground_activity.split("/")[1]
            short_activity_name = activity_name.split(".")[-1]

            state_desc = list_to_html_table([
                ("package", package_name),
                ("activity", activity_name),
                ("state_str", state.state_str),
                ("structure_str", state.structure_str),
                ("id", state.get_id()),
            ])

            utg_node = {
                "id": state_str,
                "shape": "image",
                "image": os.path.relpath(state.screenshot_path, self.device.output_dir),
                "label": short_activity_name,
                # "group": state.foreground_activity,
                "package": package_name,
                "activity": activity_name,
                "state_str": state_str,
                "structure_str": state.structure_str,
                "title": state_desc,
                "content": "\n".join([package_name, activity_name, state.state_str, state.search_content])
            }

            if state.state_str == self.first_state_str:
                utg_node["label"] += "\n<FIRST>"
                utg_node["font"] = "14px Arial red"
            if state.state_str == self.last_state_str:
                utg_node["label"] += "\n<LAST>"
                utg_node["font"] = "14px Arial red"

            utg_nodes.append(utg_node)

        for state_transition in self.G.edges():
            from_state = state_transition[0]
            to_state = state_transition[1]

            events = self.G[from_state][to_state]["events"]
            event_short_descs = []
            event_list = []

            for event_str, event_info in sorted(iter(events.items()), key=lambda x: x[1]["id"]):
                event_short_descs.append((event_info["id"], event_str))
                if self.device.adapters[self.device.minicap]:
                    view_images = ["views/view_" + view["view_str"] + ".jpg"
                                   for view in event_info["event"].get_views()]
                else:
                    view_images = ["views/view_" + view["view_str"] + ".png"
                                   for view in event_info["event"].get_views()]
                event_list.append({
                    "event_str": event_str,
                    "event_id": event_info["id"],
                    "event_type": event_info["event"].event_type,
                    "view_images": view_images
                })

            utg_edge = {
                "from": from_state,
                "to": to_state,
                "id": from_state + "-->" + to_state,
                "title": list_to_html_table(event_short_descs),
                "label": ", ".join([str(x["event_id"]) for x in event_list]),
                "events": event_list
            }

            # # Highlight last transition
            # if state_transition == self.last_transition:
            #     utg_edge["color"] = "red"

            utg_edges.append(utg_edge)

        utg = {
            "nodes": utg_nodes,
            "edges": utg_edges,

            "num_nodes": len(utg_nodes),
            "num_edges": len(utg_edges),
            "num_effective_events": len(self.effective_event_strs),
            "num_reached_activities": len(self.reached_activities),
            "test_date": self.start_time.strftime("%Y-%m-%d %H:%M:%S"),
            "time_spent": (datetime.datetime.now() - self.start_time).total_seconds(),
            "num_transitions": self.num_transitions,

            "device_serial": self.device.serial,
            "device_model_number": self.device.get_model_number(),
            "device_sdk_version": self.device.get_sdk_version(),

            "app_sha256": self.app.hashes[2],
            "app_package": self.app.package_name,
            "app_main_activity": self.app.main_activity,
            "app_num_total_activities": len(self.app.activities),
        }

        utg_json = json.dumps(utg, indent=2)
        utg_file.write("var utg = \n")
        utg_file.write(utg_json)
        utg_file.close()

    def is_event_explored(self, event, state):
        event_str = event.get_event_str(state)
        return event_str in self.effective_event_strs or event_str in self.ineffective_event_strs

    def is_state_explored(self, state):
        if state.state_str in self.explored_state_strs:
            return True
        for possible_event in state.get_possible_input():
            if not self.is_event_explored(possible_event, state):
                return False
        self.explored_state_strs.add(state.state_str)
        return True

    def is_state_reached(self, state):
        if state.state_str in self.reached_state_strs:
            return True
        self.reached_state_strs.add(state.state_str)
        return False

    def get_reachable_states(self, current_state):
        reachable_states = []
        for target_state_str in nx.descendants(self.G, current_state.state_str):
            target_state = self.G.nodes[target_state_str]["state"]
            reachable_states.append(target_state)
        return reachable_states

    def get_navigation_steps(self, from_state, to_state):
        if from_state is None or to_state is None:
            return None
        try:
            steps = []
            from_state_str = from_state.state_str
            to_state_str = to_state.state_str
            state_strs = nx.shortest_path(G=self.G, source=from_state_str, target=to_state_str)
            if not isinstance(state_strs, list) or len(state_strs) < 2:
                self.logger.warning(f"Error getting path from {from_state_str} to {to_state_str}")
            start_state_str = state_strs[0]
            for state_str in state_strs[1:]:
                edge = self.G[start_state_str][state_str]
                edge_event_strs = list(edge["events"].keys())
                if self.random_input:
                    random.shuffle(edge_event_strs)
                start_state = self.G.nodes[start_state_str]['state']
                event = edge["events"][edge_event_strs[0]]["event"]
                steps.append((start_state, event))
                start_state_str = state_str
            return steps
        except Exception as e:
            import traceback
            traceback.print_exc()
            self.logger.warning(e)
            self.logger.warning(f"Cannot find a path from {from_state.state_str} to {to_state.state_str}")
            return None

    # def get_simplified_nav_steps(self, from_state, to_state):
    #     nav_steps = self.get_navigation_steps(from_state, to_state)
    #     if nav_steps is None:
    #         return None
    #     simple_nav_steps = []
    #     last_state, last_action = nav_steps[-1]
    #     for state, action in nav_steps:
    #         if state.structure_str == last_state.structure_str:
    #             simple_nav_steps.append((state, last_action))
    #             break
    #         simple_nav_steps.append((state, action))
    #     return simple_nav_steps

    def get_G2_nav_steps(self, from_state, to_state):
        if from_state is None or to_state is None:
            return None
        from_state_str = from_state.structure_str
        to_state_str = to_state.structure_str
        try:
            nav_steps = []
            state_strs = nx.shortest_path(G=self.G2, source=from_state_str, target=to_state_str)
            if not isinstance(state_strs, list) or len(state_strs) < 2:
                return None
            start_state_str = state_strs[0]
            for state_str in state_strs[1:]:
                edge = self.G2[start_state_str][state_str]
                edge_event_strs = list(edge["events"].keys())
                start_state = random.choice(self.G2.nodes[start_state_str]['states'])
                event_str = random.choice(edge_event_strs)
                event = edge["events"][event_str]["event"]
                nav_steps.append((start_state, event))
                start_state_str = state_str
            if nav_steps is None:
                return None
            # return nav_steps
            # simplify the path
            simple_nav_steps = []
            last_state, last_action = nav_steps[-1]
            for state, action in nav_steps:
                if state.structure_str == last_state.structure_str:
                    simple_nav_steps.append((state, last_action))
                    break
                simple_nav_steps.append((state, action))
            return simple_nav_steps
        except Exception as e:
            print(e)
            return None

    def find_state_by_id(self, target_id) -> Optional[DeviceState]:
        # Traverse each node in the graph
        for node in self.G.nodes():
            # Get the node's state attribute
            state = self.G.nodes[node]['state']
            if state.get_id() == target_id:
                return state
        self.logger.error(f"Can't find State{target_id} in utg!!!")
        return None

    def find_cluster_by_id(self, target_id) -> Optional['StateCluster']:
        for cluster in self.clusters:
            if cluster.get_id() == target_id:
                return cluster
        return None

    def get_paths(self, target_state_id: int) -> list[Path]:
        # self.logger.info(f"Try to find path from Cluster{self.current_cluster.get_id()} to Cluster{target_cluster_id}")
        # # find cluster by id
        # target_cluster: Optional['StateCluster'] = self.find_cluster_by_id(target_cluster_id)
        # if target_cluster is None:
        #     self.logger.warning(f"Can't find Cluster{target_cluster_id}")
        #     return []
        self.logger.info(f"Try to find path from Current State{self.last_state.get_id()} to State{target_state_id}")
        target_state = self.find_state_by_id(target_state_id)
        if target_state:
            paths = self.generate_paths(self.last_state, target_state)
            self.logger.info(f"Found {len(paths)} paths!")
            return paths
        else:
            self.logger.warning("No path found!!!")
            return []
        # first try to find paths to root state
        # self.logger.info(f"First try to find path from Current State{self.last_state.get_id()} to Root State{target_cluster.get_root_state().get_id()}")
        # paths = self.generate_paths(self.last_state, target_cluster.get_root_state())
        # if paths:
        #     self.logger.info(f"Found {len(paths)} paths!")
        #     return paths
        # else:
        #     self.logger.info(
        #         f"No path from Current State{self.last_state.get_id()} to Root State{target_cluster.get_root_state().get_id()}")
        #     # try to find path to other states in cluster
        #     for state in target_cluster.get_states():
        #         if state == target_cluster.get_root_state():
        #             continue
        #         self.logger.info(f"Try to find path from Current State{self.last_state.get_id()} to other State{state.get_id()}")
        #         paths = self.generate_paths(self.last_state, state)
        #         if paths:
        #             self.logger.info(f"Found {len(paths)} paths!")
        #             return paths
        #         else:
        #             self.logger.info(
        #                 f"No path from Current State{self.last_state.get_id()} to State{state.get_id()}")
        #
        # self.logger.warning("No path found!!!")
        # return []

    def generate_paths(self, source_state, dest_state: DeviceState) -> list[Path]:
        paths = []

        raw_paths = nx.all_simple_paths(self.G, source=self.first_state.state_str,
                                        target=dest_state.state_str, cutoff=10)
        raw_shortest_path = nx.shortest_path(G=self.G, source=self.first_state.state_str, target=dest_state.state_str)

        shortest_path = self.convert_path(raw_shortest_path)
        shortest_length = shortest_path.length
        paths.append(shortest_path)

        # 1. If there is no path, raw paths are empty
        # 2. If source == dest, which means dest == first_state: Raw paths contain only one element, which is a list containing only the first state
        for i, raw_path in enumerate(raw_paths):
            path = self.convert_path(raw_path)
            if path.length > shortest_length:
                self.print_path(path=path, id_=i)
                paths.append(path)
            if i >= 100:
                self.logger.warning("Too many possible paths, break")
                break

        # reset 'used' to False
        for u, v, data in self.G.edges(data=True):
            for edge in data['events'].values():
                edge['used'] = False

        # Select the three paths of the shortest and latest created edge
        if len(paths) <= 1:
            return paths
        # First sort: sort by length in ascending order
        paths.sort(key=lambda path: path.length)
        # Second sorting: Arrange elements other than the first element in descending order by latest time
        # Note: Here we use slicing to exclude the first element and then sort the remaining ones
        paths[1:] = sorted(paths[1:], key=lambda path: path.latest_time, reverse=True)
        self.logger.info("***Sorted paths:***")
        for i, path in enumerate(paths[:3]):
            self.print_path(path, i)
        return paths[:3]

    def convert_path(self, raw_path: list[str]) -> Path:
        steps: list[Step] = []
        latest_time = 0
        # Traverse node pairs in a path, skipping the source node
        for i in range(1, len(raw_path)):
            # current node and next node
            current_node_str = raw_path[i - 1]
            next_node_str = raw_path[i]

            # Get the state of the next node
            next_node_state: 'DeviceState' = self.G.nodes[next_node_str]['state']

            # Get edge information
            edge_info: dict[str, UTGEdge] = self.G[current_node_str][next_node_str]['events']
            get_edge = False
            for event_str in edge_info.keys():
                value = edge_info[event_str]
                # Prioritize unused edges
                if not value['used']:
                    steps.append(Step(node=next_node_state.get_id(), event=value['event'], created_time=value['time']))
                    # Count the latest created step time of the path
                    latest_time = max(value['time'], latest_time)
                    value['used'] = True
                    get_edge = True
                    break
            # If all edges have been used, the first edge is used
            if not get_edge:
                first_key = next(iter(edge_info))
                value = edge_info[first_key]
                # Count the latest created step time of the path
                latest_time = max(value['time'], latest_time)
                steps.append(Step(node=next_node_state.get_id(), event=value['event'], created_time=value['time']))

        # add STOP event
        steps.insert(0, Step(node=self.G.nodes[raw_path[0]]['state'].get_id(),
                             event=IntentEvent(intent=self.app.get_stop_intent(),
                                               action_type=ActionType.STOP),
                             created_time=time.time()))

        path = Path(length=len(steps), latest_time=latest_time, steps=steps)
        return path

    def print_path(self, path: Path, id_: int):
        path_str = ''
        for step in path.steps:
            path_str += f" --> {step.event.to_description()} --> State{step.node}"
        self.logger.debug(f"PATH[{id_}]: len:{path.length}, t:{path.latest_time}\nsteps:{path_str}")
