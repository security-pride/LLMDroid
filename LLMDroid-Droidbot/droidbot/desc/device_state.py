import copy
import math
import os
import threading
import time
import datetime
from typing import Optional

from ..utils import md5, safe_dict_get
from ..input_event import UIEvent, TouchEvent, LongTouchEvent, ScrollEvent, SetTextEvent, KeyEvent, InputEvent
from .widget import Widget
from .action_type import *
from ..global_log import get_logger
from .state_cluster import StateCluster


class DeviceState(object):
    """
    the state of the current device
    """

    def __init__(self, device, views, foreground_activity, activity_stack, background_services,
                 tag=None, screenshot_path=None):
        self.logger = get_logger()
        self.device = device
        self.foreground_activity: str = foreground_activity
        self.activity_stack = activity_stack if isinstance(activity_stack, list) else []
        self.background_services = background_services
        if tag is None:
            from datetime import datetime
            tag = datetime.now().strftime("%Y-%m-%d_%H%M%S")
        self.tag = tag
        self.screenshot_path = screenshot_path

        self.views = self.__parse_views(views)
        self.view_tree = {}
        self.__assemble_view_tree(self.view_tree, self.views)
        self.__generate_view_strs()

        # init widget from view
        self.__widgets: list[Widget] = []
        self.__merged_widgets = {}
        root = self.__init_widgets()
        self.__root_widget = views[root]['widget']

        self.state_str = self.__get_state_str()
        self.structure_str = self.__get_content_free_state_str()
        self.search_content = self.__get_search_content()
        self.text_representation = self.get_text_representation()
        self.possible_events = []
        self.width = device.get_width(refresh=True)
        self.height = device.get_height(refresh=False)

        self.__html_desc: str = ''
        self.__tab_count: int = 0
        self.__html_tag_count: int = 0
        self.__id = -1
        self.__cluster: StateCluster = None
        self.__lock = threading.Lock()

    @property
    def activity_short_name(self):
        return self.foreground_activity.split('.')[-1]

    def to_dict(self):
        state = {'tag': self.tag,
                 'state_str': self.state_str,
                 'state_str_content_free': self.structure_str,
                 'foreground_activity': self.foreground_activity,
                 'activity_stack': self.activity_stack,
                 'background_services': self.background_services,
                 'width': self.width,
                 'height': self.height,
                 'views': self.to_html()}
        return state

    def to_json(self):
        import json
        return json.dumps(self.to_dict(), ensure_ascii=False, indent=2)

    def __parse_views(self, raw_views):
        views = []
        if not raw_views or len(raw_views) == 0:
            return views

        for view_dict in raw_views:
            # # Simplify resource_id
            # resource_id = view_dict['resource_id']
            # if resource_id is not None and ":" in resource_id:
            #     resource_id = resource_id[(resource_id.find(":") + 1):]
            #     view_dict['resource_id'] = resource_id
            views.append(view_dict)
        return views

    def __assemble_view_tree(self, root_view, views):
        if not len(self.view_tree):  # bootstrap
            if not len(views):  # to fix if views is empty
                return
            self.view_tree = copy.deepcopy(views[0])
            if 'widget' in self.view_tree:
                del self.view_tree['widget']
            self.__assemble_view_tree(self.view_tree, views)
        else:
            children = list(enumerate(root_view["children"]))
            if not len(children):
                return
            for i, j in children:
                root_view["children"][i] = copy.deepcopy(self.views[j])
                if 'widget' in root_view["children"][i]:
                    del root_view["children"][i]['widget']
                self.__assemble_view_tree(root_view["children"][i], views)

    def __generate_view_strs(self):
        for view_dict in self.views:
            self.__get_view_str(view_dict)
            # self.__get_view_structure(view_dict)

    @staticmethod
    def __calculate_depth(views):
        root_view = None
        for view in views:
            if DeviceState.__safe_dict_get(view, 'parent') == -1:
                root_view = view
                break
        DeviceState.__assign_depth(views, root_view, 0)

    @staticmethod
    def __assign_depth(views, view_dict, depth):
        view_dict['depth'] = depth
        for view_id in DeviceState.__safe_dict_get(view_dict, 'children', []):
            DeviceState.__assign_depth(views, views[view_id], depth + 1)

    def __get_state_str(self):
        state_str_raw = self.__get_state_str_raw()
        return md5(state_str_raw)

    def __get_state_str_raw(self):
        if self.device.humanoid is not None:
            import json
            from xmlrpc.client import ServerProxy
            proxy = ServerProxy("http://%s/" % self.device.humanoid)
            return proxy.render_view_tree(json.dumps({
                "view_tree": self.view_tree,
                "screen_res": [self.device.display_info["width"],
                               self.device.display_info["height"]]
            }))
        else:
            view_signatures = set()
            for view in self.views:
                if self.__safe_dict_get(view, 'visible', False):
                    view_signature = DeviceState.__get_view_signature(view)
                    if view_signature:
                        view_signatures.add(view_signature)
            return "%s{%s}" % (self.foreground_activity, ",".join(sorted(view_signatures)))

    def __get_content_free_state_str(self):
        if self.device.humanoid is not None:
            import json
            from xmlrpc.client import ServerProxy
            proxy = ServerProxy("http://%s/" % self.device.humanoid)
            state_str = proxy.render_content_free_view_tree(json.dumps({
                "view_tree": self.view_tree,
                "screen_res": [self.device.display_info["width"],
                               self.device.display_info["height"]]
            }))
        else:
            view_signatures = set()
            for view in self.views:
                if self.__safe_dict_get(view, 'visible', False):
                    view_signature = DeviceState.__get_content_free_view_signature(view)
                    if view_signature:
                        view_signatures.add(view_signature)
            state_str = "%s{%s}" % (self.foreground_activity, ",".join(sorted(view_signatures)))
        import hashlib
        return hashlib.md5(state_str.encode('utf-8')).hexdigest()

    def __get_search_content(self):
        """
        get a text for searching the state
        :return: str
        """
        words = [",".join(self.__get_property_from_all_views("resource_id")),
                 ",".join(self.__get_property_from_all_views("text"))]
        return "\n".join(words)

    def __get_property_from_all_views(self, property_name):
        """
        get the values of a property from all views
        :return: a list of property values
        """
        property_values = set()
        for view in self.views:
            property_value = DeviceState.__safe_dict_get(view, property_name, None)
            if property_value:
                property_values.add(property_value)
        return property_values

    def save2dir(self, output_dir=None):
        try:
            if output_dir is None:
                if self.device.output_dir is None:
                    return
                else:
                    output_dir = os.path.join(self.device.output_dir, "states")
            if not os.path.exists(output_dir):
                os.makedirs(output_dir)

            # save screenshot
            if self.device.adapters[self.device.minicap]:
                dest_screenshot_path = "%s/screen_%s.jpg" % (output_dir, self.tag)
            else:
                dest_screenshot_path = "%s/screen_%s.png" % (output_dir, self.tag)
            import shutil
            shutil.copyfile(self.screenshot_path, dest_screenshot_path)
            os.remove(self.screenshot_path)
            self.screenshot_path = dest_screenshot_path
            # save json file
            dest_state_json_path = os.path.join(output_dir, f"state_{self.__id}.json")
            self.logger.debug(f"sava state to {dest_state_json_path}")
            # dest_state_json_path = "%s/state_%s.json" % (output_dir, self.__id)
            state_json_file = open(dest_state_json_path, "w", encoding='utf-8')
            state_json_file.write(self.to_json())
            state_json_file.close()

            # from PIL.Image import Image
            # if isinstance(self.screenshot_path, Image):
            #     self.screenshot_path.save(dest_screenshot_path)
        except Exception as e:
            self.logger.warning(e)

    def save_view_img(self, view_dict, output_dir=None):
        try:
            if output_dir is None:
                if self.device.output_dir is None:
                    return
                else:
                    output_dir = os.path.join(self.device.output_dir, "views")
            if not os.path.exists(output_dir):
                os.makedirs(output_dir)
            view_str = view_dict['view_str']
            if self.device.adapters[self.device.minicap]:
                view_file_path = "%s/view_%s.jpg" % (output_dir, view_str)
            else:
                view_file_path = "%s/view_%s.png" % (output_dir, view_str)
            if os.path.exists(view_file_path):
                return
            from PIL import Image
            # Load the original image:
            view_bound = view_dict['bounds']
            original_img = Image.open(self.screenshot_path)
            # view bound should be in original image bound
            view_img = original_img.crop((min(original_img.width - 1, max(0, view_bound[0][0])),
                                          min(original_img.height - 1, max(0, view_bound[0][1])),
                                          min(original_img.width, max(0, view_bound[1][0])),
                                          min(original_img.height, max(0, view_bound[1][1]))))
            view_img.convert("RGB").save(view_file_path)
        except Exception as e:
            self.device.logger.warning(e)

    def is_different_from(self, another_state):
        """
        compare this state with another
        @param another_state: DeviceState
        @return: boolean, true if this state is different from other_state
        """
        return self.state_str != another_state.state_str

    @staticmethod
    def __get_view_signature(view_dict):
        """
        get the signature of the given view
        @param view_dict: dict, an element of list DeviceState.views
        @return:
        """
        if 'signature' in view_dict:
            return view_dict['signature']

        view_text = DeviceState.__safe_dict_get(view_dict, 'text', "None")
        if view_text is None or len(view_text) > 50:
            view_text = "None"

        signature = "[class]%s[resource_id]%s[text]%s[%s,%s,%s]" % \
                    (DeviceState.__safe_dict_get(view_dict, 'class', "None"),
                     DeviceState.__safe_dict_get(view_dict, 'resource_id', "None"),
                     view_text,
                     DeviceState.__key_if_true(view_dict, 'enabled'),
                     DeviceState.__key_if_true(view_dict, 'checked'),
                     DeviceState.__key_if_true(view_dict, 'selected'))
        view_dict['signature'] = signature
        return signature

    @staticmethod
    def __get_content_free_view_signature(view_dict):
        """
        get the content-free signature of the given view
        @param view_dict: dict, an element of list DeviceState.views
        @return:
        """
        if 'content_free_signature' in view_dict:
            return view_dict['content_free_signature']
        content_free_signature = "[class]%s[resource_id]%s" % \
                                 (DeviceState.__safe_dict_get(view_dict, 'class', "None"),
                                  DeviceState.__safe_dict_get(view_dict, 'resource_id', "None"))
        view_dict['content_free_signature'] = content_free_signature
        return content_free_signature

    def __get_view_str(self, view_dict):
        """
        get a string which can represent the given view
        @param view_dict: dict, an element of list DeviceState.views
        @return:
        """
        if 'view_str' in view_dict:
            return view_dict['view_str']
        view_signature = DeviceState.__get_view_signature(view_dict)
        parent_strs = []
        for parent_id in self.get_all_ancestors(view_dict):
            parent_strs.append(DeviceState.__get_view_signature(self.views[parent_id]))
        parent_strs.reverse()
        child_strs = []
        for child_id in self.get_all_children(view_dict):
            child_strs.append(DeviceState.__get_view_signature(self.views[child_id]))
        child_strs.sort()
        view_str = "Activity:%s\nSelf:%s\nParents:%s\nChildren:%s" % \
                   (self.foreground_activity, view_signature, "//".join(parent_strs), "||".join(child_strs))
        import hashlib
        view_str = hashlib.md5(view_str.encode('utf-8')).hexdigest()
        view_dict['view_str'] = view_str
        return view_str

    def __get_view_structure(self, view_dict):
        """
        get the structure of the given view
        :param view_dict: dict, an element of list DeviceState.views
        :return: dict, representing the view structure
        """
        if 'view_structure' in view_dict:
            return view_dict['view_structure']
        width = DeviceState.get_view_width(view_dict)
        height = DeviceState.get_view_height(view_dict)
        class_name = DeviceState.__safe_dict_get(view_dict, 'class', "None")
        children = {}

        root_x = view_dict['bounds'][0][0]
        root_y = view_dict['bounds'][0][1]

        child_view_ids = self.__safe_dict_get(view_dict, 'children')
        if child_view_ids:
            for child_view_id in child_view_ids:
                child_view = self.views[child_view_id]
                child_x = child_view['bounds'][0][0]
                child_y = child_view['bounds'][0][1]
                relative_x, relative_y = child_x - root_x, child_y - root_y
                children["(%d,%d)" % (relative_x, relative_y)] = self.__get_view_structure(child_view)

        view_structure = {
            "%s(%d*%d)" % (class_name, width, height): children
        }
        view_dict['view_structure'] = view_structure
        return view_structure

    @staticmethod
    def __key_if_true(view_dict, key):
        return key if (key in view_dict and view_dict[key]) else ""

    @staticmethod
    def __safe_dict_get(view_dict, key, default=None):
        value = view_dict[key] if key in view_dict else None
        return value if value is not None else default

    @staticmethod
    def get_view_center(view_dict):
        """
        return the center point in a view
        @param view_dict: dict, an element of DeviceState.views
        @return: a pair of int
        """
        bounds = view_dict['bounds']
        return (bounds[0][0] + bounds[1][0]) / 2, (bounds[0][1] + bounds[1][1]) / 2

    @staticmethod
    def get_view_width(view_dict):
        """
        return the width of a view
        @param view_dict: dict, an element of DeviceState.views
        @return: int
        """
        bounds = view_dict['bounds']
        return int(math.fabs(bounds[0][0] - bounds[1][0]))

    @staticmethod
    def get_view_height(view_dict):
        """
        return the height of a view
        @param view_dict: dict, an element of DeviceState.views
        @return: int
        """
        bounds = view_dict['bounds']
        return int(math.fabs(bounds[0][1] - bounds[1][1]))

    def get_all_ancestors(self, view_dict):
        """
        Get temp view ids of the given view's ancestors
        :param view_dict: dict, an element of DeviceState.views
        :return: list of int, each int is an ancestor node id
        """
        result = []
        parent_id = self.__safe_dict_get(view_dict, 'parent', -1)
        if 0 <= parent_id < len(self.views):
            result.append(parent_id)
            result += self.get_all_ancestors(self.views[parent_id])
        return result

    def get_all_children(self, view_dict):
        """
        Get temp view ids of the given view's children
        :param view_dict: dict, an element of DeviceState.views
        :return: set of int, each int is a child node id
        """
        children = self.__safe_dict_get(view_dict, 'children')
        if not children:
            return set()
        children = set(children)
        for child in children:
            children_of_child = self.get_all_children(self.views[child])
            children.union(children_of_child)
        return children

    def get_app_activity_depth(self, app):
        """
        Get the depth of the app's activity in the activity stack
        :param app: App
        :return: the depth of app's activity, -1 for not found
        """
        depth = 0
        for activity_str in self.activity_stack:
            if app.package_name in activity_str:
                return depth
            depth += 1
        return -1

    def get_possible_input(self):
        """
        Get a list of possible input events for this state
        :return: list of InputEvent
        """
        if self.possible_events:
            return [] + self.possible_events
        possible_events = []
        enabled_view_ids = []
        touch_exclude_view_ids = set()
        for view_dict in self.views:
            # exclude navigation bar if exists
            if self.__safe_dict_get(view_dict, 'enabled') and \
                    self.__safe_dict_get(view_dict, 'visible') and \
                    self.__safe_dict_get(view_dict, 'resource_id') not in \
                    ['android:id/navigationBarBackground',
                     'android:id/statusBarBackground']:
                enabled_view_ids.append(view_dict['temp_id'])
        # enabled_view_ids.reverse()

        for view_id in enabled_view_ids:
            if self.__safe_dict_get(self.views[view_id], 'clickable'):
                possible_events.append(TouchEvent(view=self.views[view_id]))
                touch_exclude_view_ids.add(view_id)
                touch_exclude_view_ids.union(self.get_all_children(self.views[view_id]))

        for view_id in enabled_view_ids:
            if self.__safe_dict_get(self.views[view_id], 'scrollable'):
                possible_events.append(ScrollEvent(view=self.views[view_id], direction="up"))
                possible_events.append(ScrollEvent(view=self.views[view_id], direction="down"))
                possible_events.append(ScrollEvent(view=self.views[view_id], direction="left"))
                possible_events.append(ScrollEvent(view=self.views[view_id], direction="right"))

        for view_id in enabled_view_ids:
            if self.__safe_dict_get(self.views[view_id], 'checkable'):
                possible_events.append(TouchEvent(view=self.views[view_id]))
                touch_exclude_view_ids.add(view_id)
                touch_exclude_view_ids.union(self.get_all_children(self.views[view_id]))

        for view_id in enabled_view_ids:
            if self.__safe_dict_get(self.views[view_id], 'long_clickable'):
                possible_events.append(LongTouchEvent(view=self.views[view_id]))

        for view_id in enabled_view_ids:
            if self.__safe_dict_get(self.views[view_id], 'editable'):
                possible_events.append(SetTextEvent(view=self.views[view_id], text="Hello World"))
                touch_exclude_view_ids.add(view_id)
                # TODO figure out what event can be sent to editable views
                pass

        # Set click events for child components of clickable components
        for view_id in enabled_view_ids:
            if view_id in touch_exclude_view_ids:
                continue
            children = self.__safe_dict_get(self.views[view_id], 'children')
            if children and len(children) > 0:
                continue
            possible_events.append(TouchEvent(view=self.views[view_id]))

        # For old Android navigation bars
        # possible_events.append(KeyEvent(name="MENU"))

        self.possible_events = possible_events
        return [] + possible_events

    def get_text_representation(self, merge_buttons=False):
        """
        Get a text representation of current state
        """
        enabled_view_ids = []
        for view_dict in self.views:
            # exclude navigation bar if exists
            if self.__safe_dict_get(view_dict, 'visible') and \
                    self.__safe_dict_get(view_dict, 'resource_id') not in \
                    ['android:id/navigationBarBackground',
                     'android:id/statusBarBackground']:
                enabled_view_ids.append(view_dict['temp_id'])

        text_frame = "<p id=@ text='&' attr=null bounds=null>#</p>"
        btn_frame = "<button id=@ text='&' attr=null bounds=null>#</button>"
        checkbox_frame = "<checkbox id=@ text='&' attr=null bounds=null>#</checkbox>"
        input_frame = "<input id=@ text='&' attr=null bounds=null>#</input>"
        scroll_frame = "<scrollbar id=@ attr=null bounds=null></scrollbar>"

        view_descs = []
        indexed_views = []
        # available_actions = []
        removed_view_ids = []

        for view_id in enabled_view_ids:
            if view_id in removed_view_ids:
                continue
            # print(view_id)
            view = self.views[view_id]
            clickable = self._get_self_ancestors_property(view, 'clickable')
            scrollable = self.__safe_dict_get(view, 'scrollable')
            checkable = self._get_self_ancestors_property(view, 'checkable')
            long_clickable = self._get_self_ancestors_property(view, 'long_clickable')
            editable = self.__safe_dict_get(view, 'editable')
            actionable = clickable or scrollable or checkable or long_clickable or editable
            checked = self.__safe_dict_get(view, 'checked', default=False)
            selected = self.__safe_dict_get(view, 'selected', default=False)
            content_description = self.__safe_dict_get(view, 'content_description', default='')
            view_text = self.__safe_dict_get(view, 'text', default='')
            view_class = self.__safe_dict_get(view, 'class').split('.')[-1]
            bounds = self.__safe_dict_get(view, 'bounds')
            view_bounds = f'{bounds[0][0]},{bounds[0][1]},{bounds[1][0]},{bounds[1][1]}'
            if not content_description and not view_text and not scrollable:  # actionable?
                continue

            # text = self._merge_text(view_text, content_description)
            # view_status = ''
            view_local_id = str(len(view_descs))
            if editable:
                view_desc = input_frame.replace('@', view_local_id).replace('#', view_text)
                if content_description:
                    view_desc = view_desc.replace('&', content_description)
                else:
                    view_desc = view_desc.replace(" text='&'", "")
                # available_actions.append(SetTextEvent(view=view, text='HelloWorld'))
            elif checkable:
                view_desc = checkbox_frame.replace('@', view_local_id).replace('#', view_text)
                if content_description:
                    view_desc = view_desc.replace('&', content_description)
                else:
                    view_desc = view_desc.replace(" text='&'", "")
                # available_actions.append(TouchEvent(view=view))
            elif clickable:  # or long_clickable
                if merge_buttons:
                    # below is to merge buttons, led to bugs
                    clickable_ancestor_id = self._get_ancestor_id(view=view, key='clickable')
                    if not clickable_ancestor_id:
                        clickable_ancestor_id = self._get_ancestor_id(view=view, key='checkable')
                    clickable_children_ids = self._extract_all_children(id=clickable_ancestor_id)
                    if view_id not in clickable_children_ids:
                        clickable_children_ids.append(view_id)
                    view_text, content_description = self._merge_text(clickable_children_ids)
                    checked = self._get_children_checked(clickable_children_ids)
                    # end of merging buttons
                view_desc = btn_frame.replace('@', view_local_id).replace('#', view_text)
                if content_description:
                    view_desc = view_desc.replace('&', content_description)
                else:
                    view_desc = view_desc.replace(" text='&'", "")
                # available_actions.append(TouchEvent(view=view))
                if merge_buttons:
                    for clickable_child in clickable_children_ids:
                        if clickable_child in enabled_view_ids and clickable_child != view_id:
                            removed_view_ids.append(clickable_child)
            elif scrollable:
                # print(view_id, 'continued')
                view_desc = scroll_frame.replace('@', view_local_id)
                # available_actions.append(ScrollEvent(view=view, direction='DOWN'))
                # available_actions.append(ScrollEvent(view=view, direction='UP'))
            else:
                view_desc = text_frame.replace('@', view_local_id).replace('#', view_text)
                if content_description:
                    view_desc = view_desc.replace('&', content_description)
                else:
                    view_desc = view_desc.replace(" text='&'", "")
                # available_actions.append(TouchEvent(view=view))

            allowed_actions = ['touch']
            special_attrs = []
            if editable:
                allowed_actions.append('set_text')
            if checkable:
                allowed_actions.extend(['select', 'unselect'])
                allowed_actions.remove('touch')
            if scrollable:
                allowed_actions.extend(['scroll up', 'scroll down'])
                allowed_actions.remove('touch')
            if long_clickable:
                allowed_actions.append('long_touch')
            if checked or selected:
                special_attrs.append('selected')
            view['allowed_actions'] = allowed_actions
            view['special_attrs'] = special_attrs
            view['local_id'] = view_local_id
            if len(special_attrs) > 0:
                special_attrs = ','.join(special_attrs)
                view_desc = view_desc.replace("attr=null", f"attr={special_attrs}")
            else:
                view_desc = view_desc.replace(" attr=null", "")
            view_desc = view_desc.replace("bounds=null", f"bound_box={view_bounds}")
            view_descs.append(view_desc)
            view['desc'] = view_desc.replace(f' id={view_local_id}', '').replace(f' attr={special_attrs}', '')
            indexed_views.append(view)

        # prefix = 'The current state has the following UI elements: \n' #views and corresponding actions, with action id in parentheses:\n '
        state_desc = '\n'.join(view_descs)
        activity = self.foreground_activity.split('/')[-1]
        # print(views_without_id)
        return state_desc, activity, indexed_views

    def _get_self_ancestors_property(self, view, key, default=None):
        all_views = [view] + [self.views[i] for i in self.get_all_ancestors(view)]
        for v in all_views:
            value = self.__safe_dict_get(v, key)
            if value:
                return value
        return default

    def _merge_text(self, children_ids):
        texts, content_descriptions = [], []
        for childid in children_ids:
            if not self.__safe_dict_get(self.views[childid], 'visible') or \
                    self.__safe_dict_get(self.views[childid], 'resource_id') in \
                    ['android:id/navigationBarBackground',
                     'android:id/statusBarBackground']:
                # if the successor is not visible, then ignore it!
                continue

            text = self.__safe_dict_get(self.views[childid], 'text', default='')
            if len(text) > 50:
                text = text[:50]

            if text != '':
                # text = text + '  {'+ str(childid)+ '}'
                texts.append(text)

            content_description = self.__safe_dict_get(self.views[childid], 'content_description', default='')
            if len(content_description) > 50:
                content_description = content_description[:50]

            if content_description != '':
                content_descriptions.append(content_description)

        merged_text = '<br>'.join(texts) if len(texts) > 0 else ''
        merged_desc = '<br>'.join(content_descriptions) if len(content_descriptions) > 0 else ''
        return merged_text, merged_desc

    def __init_widgets(self) -> int:
        # create widgets
        first_visible = -1
        for i, view in enumerate(self.views):
            if self.__safe_dict_get(view, 'visible'):
                if first_visible == -1:
                    first_visible = i
                self.__widgets.append(Widget(view))
        if first_visible == -1:
            self.logger.error("Can't find ROOT(first visible) widget!!!")

        # merge widgets
        final_widgets = []
        for widget in self.__widgets:
            hashcode = widget.get_hash()
            # add widget with same hash into merged_widgets
            if hashcode in self.__merged_widgets.keys():
                widget.set_position(len(self.__merged_widgets[hashcode]))
                self.__merged_widgets[hashcode].append(widget)
            else:
                final_widgets.append(widget)
                widget.set_position(-1)
                self.__merged_widgets[hashcode] = []
        # every widget in widgets has unique hash
        self.__widgets = final_widgets
        return first_visible

    def __should_merge(self, father: Widget, child: Widget):
        if len(child.get_children()) == 0 and \
                child.get_html_class() == HtmlClass.P and \
                father.get_html_class() == HtmlClass.BUTTON:
            return True
        else:
            return False

    def __add_tab(self) -> None:
        for i in range(self.__tab_count):
            self.__html_desc += '\t'

    def __generate_html_recursive(self, parent: Widget):
        if not parent.get_visible():
            return

        if self.__tab_count >= 25 or self.__html_tag_count >= 100:
            return

        self.__html_tag_count += 1
        self.__tab_count += 1
        self.__add_tab()

        widget_to_merge = []
        widget_not_merge = []

        check_list: list[Widget] = [parent]
        # for id in parent.get_children():
        #     w = safe_dict_get(self.views[id], 'widget', None)
        #     if w:
        #         check_list.append(w)

        while len(check_list) != 0:
            widget = check_list[0]
            check_list.remove(widget)
            # [self.views[id]['widget'] for id in widget.get_children()]
            child_widgets = []
            for id in widget.get_children():
                w = safe_dict_get(self.views[id], 'widget', None)
                if w:
                    child_widgets.append(w)
            # Merge nested situations to reduce depth
            # There is only one child node, and the child node is of normal type
            if len(child_widgets) == 1 and child_widgets[0].get_html_class() == HtmlClass.P:
                widget_to_merge.append(child_widgets[0])
                # self.logger.debug(f'[to html] merge widget: {child_widgets[0].to_html()[:-1]}')
                # Keep checking down
                check_list.append(child_widgets[0])
            else:
                # Do not merge when there are more than 1 child nodes
                if len(child_widgets) > 1:
                    widget_not_merge += [widget for widget in child_widgets]
                else:
                    # There may be no child nodes, or only one child node
                    # Perform a general check, that is, whether button and child node p can merge text
                    for child in child_widgets:
                        # child_view = self.views[id]['widget']
                        if self.__should_merge(widget, child):
                            widget_to_merge.append(child)
                        else:
                            widget_not_merge.append(child)

        has_child: bool = True if widget_not_merge else False
        self.__html_desc += parent.to_html(widget_to_merge, has_child)

        for widget in widget_not_merge:
            self.__generate_html_recursive(widget)

        if has_child:
            self.__add_tab()
            self.__html_desc += parent.get_html_class().end_tag + '\n'
        self.__tab_count -= 1

    def to_html(self) -> str:
        """
        return state description in html format.
        only generate html once
        """
        with self.__lock:
            if self.__html_desc:
                return self.__html_desc

            self.__tab_count = -1
            self.__generate_html_recursive(self.__root_widget)

            return self.__html_desc

    def set_id(self, value) -> None:
        self.__id = value

    def get_id(self) -> int:
        return self.__id

    def compute_similarity(self, other: 'DeviceState') -> float:
        matched_count = 0
        to_compare = self.__widgets if len(self.__widgets) > len(other.__widgets) else other.__widgets
        candidates = other.__widgets if len(self.__widgets) > len(other.__widgets) else self.__widgets
        for candidate in candidates:
            # Is there a widget with the same hash as candidate in find_if to_compare?
            for other_widget in to_compare:
                if candidate.get_hash() == other_widget.get_hash():
                    matched_count += 1
                    break
        return (2 * matched_count) / (len(self.__widgets) + len(other.__widgets))

    def get_cluster(self) -> 'StateCluster':
        return self.__cluster

    def set_cluster(self, cluster: 'StateCluster'):
        self.__cluster = cluster

    def get_all_widgets(self) -> list[Widget]:
        all_widgets = []
        for widget in self.__widgets:
            all_widgets.append(widget)
            if widget.get_hash() in self.__merged_widgets:
                all_widgets.extend(self.__merged_widgets[widget.get_hash()])
        return all_widgets

    def find_widget_by_id(self, widget_id: int) -> Optional[Widget]:
        """
        Given the id of a widget, return the corresponding widget
        The widget_id should be the id of the widget inside the state, and should not come from the widget_id in other states.
        """
        for widget in self.get_all_widgets():
            if widget_id == widget.get_id():
                return widget
        return None

    def find_similar_widget(self, widget: Widget) -> Optional[Widget]:
        """
        Given a widget in another state, return similar widget in the current state
        First look for widget with the same hash, and then try to select widget with the same position.
        """
        hash_to_match = widget.get_hash()
        pos_to_match = widget.get_position()
        for w in self.__widgets:
            if w.get_hash() == hash_to_match:
                if pos_to_match == -1:
                    return w
                # similar widget may be in merged_widgets
                elif hash_to_match in self.__merged_widgets:
                    # find in merged_widgets
                    if pos_to_match < len(self.__merged_widgets[hash_to_match]):
                        return self.__merged_widgets[hash_to_match][pos_to_match]
                    elif len(self.__merged_widgets[hash_to_match]) > 0:
                        return self.__merged_widgets[hash_to_match][-1]
                    else:
                        return w
                else:
                    return w

        return None

    def print_events(self):
        events = []
        for event in self.get_possible_input():
            if event.get_visit_count() >= 1:
                events.append(event)
        self.logger.info("*******************visited event:")
        self.logger.info([event.to_description() for event in events])
        self.logger.info("*********************************")

    def find_similar_event(self, event: InputEvent) -> Optional[InputEvent]:
        # ActionType.CLICK.value <= event.get_action_type().value <= ActionType.SCROLL_RIGHT_LEFT
        if isinstance(event, UIEvent):
            action_type = event.get_action_type()
            widget = event.get_target()
            widget_hash = widget.get_hash()
            which_widget = widget.get_position()
            # Filter out all events with the same action type and the same target hash
            candidates = []
            for event in self.get_possible_input():
                if isinstance(event, UIEvent):
                    if event.get_target() and event.get_target().get_hash() == widget_hash and event.get_action_type() == action_type:
                        candidates.append(event)
            candidates = sorted(candidates, key=lambda x: abs(x.get_target().get_position() - which_widget))
            ret = None
            # Prioritize the widget with the same pos, and if it does not exist, select the widget closest to it.
            for candidate in candidates:
                if which_widget == candidate.get_target().get_position():
                    ret = candidate
                    break
            if ret is None and candidates:
                ret = candidates[0]
            return ret
        else:
            return event

    def find_event_by_id_and_type(self, widget_id: int, act_type: ActionType) -> Optional['InputEvent']:
        for event in self.get_possible_input():
            if (isinstance(event, UIEvent)
                    and event.get_action_type() == act_type
                    and event.get_target().get_id() == widget_id):
                return event
        self.logger.warning(f"State{self.__id}: No qualified event matched by widget{widget_id} and {act_type}")
        return None

    def find_events_by_widget(self, widget: 'Widget') -> list['UIEvent']:
        """
        find all relevant events by widget
        """
        ret = []
        for event in self.get_possible_input():
            if isinstance(event, UIEvent) and event.get_target() == widget:
                ret.append(event)
        return ret

    def diff_widgets(self, target: 'DeviceState') -> list['Widget']:
        # return widget with different hash
        # ignore Layout
        if target == self:
            return []
        res: list['Widget'] = []
        for widget in self.__widgets:
            # find_if
            condition = lambda x, y: x.get_hash() == y.get_hash()
            found: Optional[Widget] = next((item for item in target.__widgets if condition(x=widget, y=item)), None)
            if not found and widget.get_class().lower().find("layout") == -1:
                res.append(widget)
        return res
