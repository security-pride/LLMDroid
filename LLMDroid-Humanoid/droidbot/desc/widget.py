from ..utils import safe_dict_get
from .action_type import *


class Widget:
    def __init__(self, view):
        # compatibility
        self.__view = view
        view['widget'] = self

        self.__init_from_view(view)

        # position in State's widgets or merged_widgets
        self.__position: int = -1

        # action
        self.__function: str = ''

        # compute hash
        hashcode1 = hash(self.__clazz)
        hashcode2 = hash(self.__resource_id)
        hashcode3 = hash(self.__bounds[1][0] - self.__bounds[0][0])
        hashcode4 = hash(self.__bounds[1][1] - self.__bounds[0][1])
        hashcode5 = hash(self.__action_mask)  # action type
        hashcode6 = hash(self.get_scroll_type().value)
        self.__hash = ((hashcode1 ^ (hashcode2 << 5)) >> 3) ^ \
                      (((127 * hashcode3 << 1) ^ (256 * hashcode4 << 4)) >> 2) ^ \
                      (((hashcode5 << 6) ^ (hashcode6 << 7)) >> 4)

    def __init_from_view(self, view):
        # operate type
        self.__enabled = safe_dict_get(view, 'enabled', False)
        self.__visible = safe_dict_get(view, 'visible', False)
        self.__clickable = safe_dict_get(view, 'clickable', False)
        self.__checkable = safe_dict_get(view, 'checkable', False)
        self.__long_clickable = safe_dict_get(view, 'long_clickable', False)
        self.__scrollable = safe_dict_get(view, 'scrollable', False)
        self.__editable = safe_dict_get(view, 'editable', False)

        # basic attribute
        self.__id = view['temp_id']
        self.__clazz: str = safe_dict_get(view, 'class', '')
        self.__resource_id: str = safe_dict_get(view, 'resource_id', '')
        self.__text: str = safe_dict_get(view, 'text', '')
        self.__content_desc: str = safe_dict_get(view, 'content_description', '')
        # [[left, top], [right, bottom]]
        self.__bounds = view['bounds']

        # set children and father
        self.__children = safe_dict_get(view, 'children', [])
        self.parent = safe_dict_get(view, 'parent', -1)

        # set action mask
        self.__action_mask = OperateType.NONE.value
        if self.get_enable():
            self.__enable_operate(OperateType.Enable)
        if self.get_clickable():
            self.__enable_operate(OperateType.Clickable)
        if self.get_checkable():
            self.__enable_operate(OperateType.Checkable)
        if self.get_long_clickable():
            self.__enable_operate(OperateType.LongClickable)
        if self.get_scrollable():
            self.__enable_operate(OperateType.Scrollable)
        if self.get_editable():
            self.__enable_operate(OperateType.Editable)

        # TODO: set child clickable if father is clickable

    def get_position(self) -> int:
        return self.__position

    def set_position(self, pos):
        self.__position = pos

    def get_id(self) -> int:
        return self.__id

    def get_brief_description(self) -> str:
        info = "({attr}:{value})"
        if self.__text:
            info = info.format(attr='text', value=self.__text)
        elif self.__content_desc:
            info = info.format(attr='content-desc', value=self.__content_desc)
        elif self.__resource_id:
            info = info.format(attr='res-id', value=self.get_resource_id())
        else:
            info = ''
        return f"{self.get_class()}{info}"


    def get_class(self) -> str:
        return self.__clazz.split('.')[-1]

    def get_resource_id(self) -> str:
        return self.__resource_id.split('/')[-1]

    def get_content_desc(self) -> str:
        return self.__content_desc

    def get_text(self) -> str:
        return self.__text

    def get_function(self) -> str:
        return self.__function

    def set_function(self, function: str):
        self.__function = function

    def get_hash(self) -> int:
        return self.__hash

    def get_enable(self) -> bool:
        return self.__enabled

    def get_visible(self) -> bool:
        return self.__visible

    def get_clickable(self) -> bool:
        return self.__clickable

    def get_checkable(self) -> bool:
        return self.__checkable

    def get_long_clickable(self) -> bool:
        return self.__long_clickable

    def get_scrollable(self) -> bool:
        return self.__scrollable

    def get_editable(self) -> bool:
        return self.__editable

    def get_children(self) -> list[int]:
        return self.__children

    def __enable_operate(self, operate: OperateType):
        self.__action_mask |= operate.value

    def get_html_class(self) -> HtmlClass:
        if self.get_checkable():
            return HtmlClass.CHECKBOX
        if self.get_editable():
            return HtmlClass.INPUT
        if self.get_scrollable():
            return HtmlClass.SCROLLER
        if self.get_clickable():
            return HtmlClass.BUTTON
        return HtmlClass.P

    def get_scroll_type(self) -> ScrollType:
        if not self.__scrollable:
            return ScrollType.NONE
        if (self.__clazz in ["android.widget.ScrollView",
                             "android.widget.ListView",
                             "android.widget.ExpandableListView",
                             "android.support.v17.leanback.widget.VerticalGridView",
                             "android.support.v7.widget.RecyclerView",
                             "androidx.recyclerview.widget.RecyclerView"]):
            return ScrollType.Vertical
        elif (self.__clazz in ["android.widget.HorizontalScrollView",
                               "android.support.v17.leanback.widget.HorizontalGridView",
                               "android.support.v4.view.ViewPager"]):
            return ScrollType.Horizontal
        elif "ScrollView" in self.__clazz:
            return ScrollType.ALL
        else:
            return ScrollType.ALL  # ALL ?

    def to_html(self, widget_to_merge: 'list[Widget]' = None, has_child: bool = False) -> str:
        if widget_to_merge is None:
            widget_to_merge = []

        html_class = self.get_html_class()
        result: str = html_class.start_tag + ' '

        result += f'id="{self.__id}" '

        # class
        class_name = self.get_class()
        if class_name:
            result += f'"class={class_name}" '

        res_id = self.get_resource_id()
        if res_id:
            result += f'"resource-id={res_id}" '
        else:
            for widget in widget_to_merge:
                res_id = widget.get_resource_id()
                if res_id:
                    result += f'"resource-id={res_id}" '
                    break

        content_desc = self.get_content_desc()
        if content_desc:
            result += f'"content-desc={content_desc}" '

        # add special attribute
        if html_class == HtmlClass.SCROLLER:
            scroll_type = self.get_scroll_type()
            if scroll_type == ScrollType.ALL:
                result += 'direction="vertical, horizontal" '
            elif scroll_type == ScrollType.Horizontal:
                result += 'direction="horizontal" '
            elif scroll_type == ScrollType.Vertical:
                result += 'direction="vertical" '
        if html_class == HtmlClass.INPUT:
            result += 'input="?" '

        result += '>'

        # text
        text = self.__text
        if text:
            result += text
        first_flag = True
        for widget in widget_to_merge:
            child_text = widget.get_text()
            if child_text:
                if first_flag and not text:
                    result += child_text
                    first_flag = False
                else:
                    result += f' <br> {child_text}'

        if not has_child:
            result += html_class.end_tag
        return result + '\n'
