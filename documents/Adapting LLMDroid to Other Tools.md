# Adapting the Tool to Other Dynamic Exploration Techniques 

 ![](./imgs/workflow.png)

## Overview  

Given the significant differences in programming languages and implementations of existing testing tools, there is currently no straightforward interface to directly port LLMDroid to other tools. However, the modifications required for porting LLMDroid follow similar principles.  



Existing dynamic testing tools generally adhere to a fundamental loop:  

1. **Fetch** the current screen's view hierarchy.  
2. **Process and model** the view hierarchy (e.g., build UTG, construct pages/widgets, generate actions). Implementation varies across tools.  
3. **Select an action** based on a strategy.  
4. **Execute the action** using the testing framework.  

To port LLMDroid to another testing tool, modifications are primarily needed for **Step 2 and 3**, ensuring compatibility with the target tool's data structures.  



**Modifications for Step 2**: LLMDroid needs to adapt to the target tool's data structures. Key additions include:  

- `StateCluster`: A data structure to group similar pages, along with algorithms for page comparison (e.g., HTML conversion, page similarity calculation).  
- `LLM Agent`: Acts as a bridge between the tool and the LLM, handling queries/responses and data transformation.  
- `Code Coverage Monitoring Module`: Tracks real-time code coverage during exploration.  



**Modifications for Step 3**: LLMDroid preserves the original strategy but introduces a **state-machine mechanism** to switch between phases:  

- Autonomous Exploration: Default mode driven by the original tool's logic.  
- LLM Guidance: Sub-phases include Offline Navigation, Function Execution, and post-execution analysis.  



In summary, porting LLMDroid does not alter the original tool's core algorithms. The effort required depends on the target tool's implementation complexity. For example, adapting to Python-based tools like *Droidbot* or *Humanoid* is straightforward, while integrating with C++-based tools like *Fastbot* (with limited extensibility) demands more work.  



## Tutorial  

This section uses **LLMDroid-Droidbot (Python)** as an example to illustrate the porting process.  

### StateCluster

(The word "state" means "UI page" in this sub section.)

A `StateCluster` groups similar pages, containing metadata such as `overview`, `function list`, and `root page`:  
```python  
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
```

The similarity algorithm (in `utg_based_policy.py`) determines cluster assignments:  
```python  
    def __find_most_similar(self) -> Optional[StateCluster]:
        threshold = 0.6
        # First compare with the current cluster
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
```

### UI Page-to-HTML Conversion  
The `UI page` needs to be converted into HTML to interact with LLM. Each UI control should correspond to a unique ID to facilitate the model's responses and subsequent processing based on those responses.

Since some pages may be complex, containing a large number of controls and deeply nested structures, directly generating HTML could result in excessively long text, which may hinder the model's analysis. Therefore, it is necessary to merge irrelevant controls or structures. The detailed implementation can be found in `device_state.py`.

```python  
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
```

Example HTML output:  
```html  
<p class="FrameLayout" resource-id="action_bar_root" >
	<p class="LinearLayout" resource-id="appbar" >
		<button id=8 class="ImageButton" content-desc="Navigate up" ></button>
		<p class="TextView" >My Networks</p>
		<p class="LinearLayoutCompat" >
			<button id=11 class="TextView" resource-id="action_search" content-desc="Search" ></button>
			<button id=12 class="TextView" resource-id="action_edit" content-desc="Edit" ></button>
		</p>
	</p>
	<p class="FrameLayout" >
		<button id=14 class="RecyclerView" resource-id="list" >
			<p class="ViewGroup" resource-id="title" >Current network</p>
			<button id=17 class="ViewGroup" >
				<p class="ImageView" resource-id="icon" ></p>
				<p class="TextView" resource-id="title1" >Current Wi-Fi</p>
				<p class="TextView" resource-id="subtitle1" >Automatic selection of best available network</p>
			</button>
			<p class="ViewGroup" resource-id="title" >Monitored Networks</p>
			<p class="FrameLayout" >
				<p class="LinearLayout" resource-id="left_container" >
					<p class="TextView" resource-id="promo_title" >Monitor with Fing Desktop</p>
					<p class="TextView" resource-id="promo_body" >Don't leave your network unattended. Get continuous monitoring today.</p>
					<button id=28 class="FrameLayout" resource-id="promo_action" >Start now</button>
				</p>
				<p class="ImageView" resource-id="promo_image" ></p>
			</p>
			<p class="ViewGroup" resource-id="title" >Scanned Networks</p>
			<button id=34 class="ViewGroup" >
				<p class="ImageView" resource-id="icon" ></p>
				<p class="TextView" resource-id="title1" >Security-Pride-C502</p>
				<p class="TextView" resource-id="subtitle1" >12 hours ago</p>
				<p class="ImageView" resource-id="marker" ></p>
			</button>
			<button id=39 class="ViewGroup" >
				<p class="ImageView" resource-id="icon" ></p>
				<p class="TextView" resource-id="title1" >-</p>
				<p class="TextView" resource-id="subtitle1" >14 hours ago</p>
			</button>
		</button>
	</p>
</p>
```

### LLM Agent  
LLMAgent acts as a proxy between the testing tool and the LLM, responsible for converting test-generated data into natural language based on prompt templates (see `prompt.py`) to interact with the LLM and extract answers from its responses.

Specifically, its tasks include:

- **During the Autonomous Exploration phase**:
    - Receiving the `UI page` and performing **GUI Summary** via the LLM.
- **During the LLM Guidance phase**:
    - Generating a **Top Valued Clusters List** and requesting navigation targets from the LLM.
    - After reaching the target page, providing the page to the LLM and requesting the next action.
    - After function execution is completed, submitting previously queried clusters to the LLM for **follow-up cluster queries**.

To avoid blocking the testing process during autonomous exploration due to LLM interactions, **LLMAgent runs in a separate thread**. Its core logic is outlined below—detailed implementation can be found in `llm_agent.py`.

```python  
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
```

### Code Coverage Monitoring  
This module tracks code coverage growth and triggers phase transitions.

Base class (`CodeCoverageMonitor`) implements threshold adjustments and low-growth-rate detection:  

```python  
class CodeCoverageMonitor(metaclass=ABCMeta):
    @abstractmethod
    def _get_code_coverage(self) -> float:
        pass

    def __update(self, current_cv: float):
        self.__cv_history.append(current_cv)
        n = len(self.__cv_history)
        current_growth_rate = 0.0
        if n >= 2:
            # gn = (xn - xn-1) / xn-1
            current_growth_rate = (current_cv - self.__cv_history[-2]) / self.__cv_history[-2]
            self.__growth_rate_sum += min(10.0, current_growth_rate)
            # update growth rate list to check
            self.__gr_to_check.append(current_growth_rate)
            if len(self.__gr_to_check) > self.__WINDOW_SIZE:
                self.__gr_to_check.pop(0)
            self.logger.info(f"[CV_Monitor]({len(self.__gr_to_check)}) growth rate: {current_growth_rate}, sum:{self.__growth_rate_sum}")
        # Adjust the threshold when the number of collected growth rates is greater than the window value
        if n >= self.__WINDOW_SIZE:
            # G
            baseline = self.__growth_rate_sum / (n - 1)
            # delta_g = gn - G
            delta_g = current_growth_rate - baseline

            # Tn = T0 * exp(k * delta_g)
            adjusted = self.__adjusted_threshold * math.exp(self.__FACTOR * delta_g)
            self.__adjusted_threshold = max(adjusted, self.__MIN_THRESHOLD)
            self.logger.info(f"[CV_Monitor] G:{baseline:8.5f}, delta_g:{delta_g:8.5f}, adjusted_threshold:{self.__adjusted_threshold:8.5f}")

    def update_code_coverage(self):
        self.__current_coverage = self._get_code_coverage()

    def check_low_growth_rate(self) -> bool:
        # update current code coverage and adjust threshold
        self.__update(self.__current_coverage)

        if len(self.__gr_to_check) == self.__WINDOW_SIZE:
            reverse = self.__gr_to_check[::-1]
            for i, gr in enumerate(reverse):
                if gr > self.__adjusted_threshold:
                    self.logger.info(f"[CV_Monitor Check] {i} to the end")
                    return False
            return True
        else:
            return False
```

Code coverage monitoring is implemented through two concrete classes: `JacocoCVMonitor` and `AndroLogCVMonitor`, which extend the abstract CodeCoverageMonitor base class. Both subclasses implement their specific coverage collection logic by overriding the `_get_code_coverage()` method.
1. **JaCoCo**:  
```python  
class JacocoCVMonitor(CodeCoverageMonitor):
    def _get_code_coverage(self) -> float:

        # self.__send_broadcast()
        # Define thread event for synchronization
        event = threading.Event()

        def thread_function():
            try:
                self.logger.info("Preparing to call getMethodCoverage method.")

                # create an instance of JacocoBridge
                JacocoBridge = jpype.JClass("org.jacoco.examples.JacocoBridge")  # Replace with full class path
                instance = JacocoBridge(JString(self.__udid), JString(self.__save_dir))

                # prepare parameters
                JavaFile = jpype.JClass("java.io.File")  # Import java.io.File class
                ec_file_name_jstring = JString(self.__ec_file_name)  # Convert to Java string
                ec_file_path_jstring = JString(self.__ec_file_path)  # Convert to Java string
                class_file = JavaFile(self.__class_file_path)  # Create Java File object

                # Call Java method
                result = instance.getMethodCoverage(ec_file_name_jstring, ec_file_path_jstring, class_file)
                result *= 100
                with self.__coverage_lock:
                    self.__last_coverage = result
                self.logger.info(f"Result from getMethodCoverage: {result:.5f}%")

            except jpype.JException as ex:
                if isinstance(ex, jpype.JClass("java.io.IOException")):
                    self.logger.warning(f"Caught an IOException from Java: {ex}")
                else:
                    self.logger.error(f"Java Exception occurred: {ex}")
            except Exception as ex:
                self.logger.error(f"Python Exception occurred: {ex}")
            finally:
                event.set()  # notify main thread to update coverage

        # Start sub-thread
        thread = threading.Thread(target=thread_function)
        thread.start()

        # Main thread waits for sub-thread completion, with 1 second timeout
        coverage: float = 0.0
        if not event.wait(1.3):  # If timeout exceeds 1 second, return previous coverage
            self.logger.info("Coverage calculation took too long, returning previous coverage.")
            with self.__coverage_lock:
                coverage = self.__last_coverage
        else:
            # Sub-thread completed, return newly calculated coverage
            self.logger.info(f"New coverage: {self.__last_coverage:.5f}%")
            with self.__coverage_lock:
                coverage = self.__last_coverage

        self._save_to_file(f"{coverage:.5f}%")
        return coverage
```
2. **AndroLog**:  
```python  
class AndroLogCVMonitor(CodeCoverageMonitor):
    def _get_code_coverage(self) -> float:
        """
        compute current code coverage and growth rate
        save result to file
        @return current code coverage
        """
        with threading.Lock():
            current_method_count = self.method_count
            # code coverage growth rate
            self.rate = ((current_method_count - self.last_method_count) / self.last_method_count)
            # code coverage percentage
            percentage: float = (current_method_count / self.__total) * 100
            self.last_method_count = current_method_count
        str_ = f"[{self.__log_tag}] {percentage:8.5f}% ({current_method_count}/{self.__total}) --> {self.rate:8.5f}"
        self.logger.info(str_)
        self._save_to_file(str_)
        return percentage
```

### State Transition Logic  
This component serves as the core of LLMDroid's logic control, responsible for orchestrating module interactions and managing phase transitions. 

Below is the modified `generate_event` loop in LLMDroid-Droidbot:  

```python  
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
        if not self.__llm_agent.is_child_thread_alive():
            raise Exception("LLM Agent terminated")
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
        self.logger.info(f"Next event: {event.to_description()}")
        return event
```

After LLMDroid adaptation, the following logic is added:

- `self.__process_state()`: Determines which cluster the UI page belongs to and performs GUI Summary
- `self.__switch_mode()`: Performs state transitions according to the following logic
- `event = self.__resolve_new_action()`: Generates the test case to be executed

------

The state transition logic operates as follows:

**Initial State:** Autonomous Exploration

- **A. Preprocessing:** (Different logic based on current state)

    1. **Autonomous Exploration:**
        - After obtaining the current screen, the tool first determines whether the page belongs to a new cluster or an existing one. If it's a new cluster, the LLMAgent is invoked to perform GUI summary.
        - Retrieves current code coverage and checks for low-growth conditions. If low growth is detected, transitions to **A2**; otherwise proceeds to **B1**.
    2. **LLM Guidance:**
        - (Upon entering from Autonomous Exploration) Requests test guidance from the LLM via LLMAgent. After obtaining the target, computes the path locally and proceeds to **B2**.
        - Verifies whether the current UI page matches the planned path, performs step skipping/failure handling operations, then proceeds to **B2**.
        - If the target screen is reached, transitions to **B3**.

    - **Function Execution:** (For non-first entries) Queries the LLMAgent to confirm whether the target function is completed. If completed, transitions to **B1**; otherwise proceeds to **B3**.

- **B. Action Selection:** (Logic varies by current state)

    1. **Autonomous Exploration:** Next action is determined by the original tool's logic
    2. **Navigation:** Pops the current action from the path
    3. **Function Execution:** Queries the LLMAgent to obtain the next action

- **C. Action Execution:** Executes the selected action



For implementation details, see `utg_based_policy.py: __switch_mode()`.