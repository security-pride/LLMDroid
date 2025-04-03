#ifndef Prompt_H_
#define Prompt_H_

#include <string>


//////////////////////////////////////////////////////////////////////////////
// State Overview
//////////////////////////////////////////////////////////////////////////////

const std::string _functionExplanationPrompt = R"(
An app's page contains many controls to display information to users and provide interactive interfaces.
Users can interact with the controls to perform a "Function", such as navigating to other tabs by clicking a navigation bar icon or accessing the settings page.
)";

const std::string _inputExplanationPrompt_state = R"(
I will provide an HTML description of an app's page, including the components and their structural information.
In the HTML description of this page,
I use five types of HTML tags, namely <button>, <checkbox>, <scroller>, <input>, and <p>, which represent elements that can be clicked, checked, swiped, edited, and any other views respectively.
Each HTML element has the following attributes:
id(the unique id of this component), class(the class name of this component), resource-id (the resource-id of this Android component), content-desc (the content description of this component), text (the text of this component), direction (if this component is scrollable, indicating its scroll direction), value (the text that has been input to the text box).
)";

const std::string _requiredOutputPrompt_state2 = R"(
Based on the HTML description of this page, your tasks include:

1. Page Overview: Summarize the current page, concluding what kind of information the page mainly presents to users, and what this page is primarily used for.
2. Function Analysis: Identify the functions present on the page, listing their corresponding element IDs. Prioritize these functions by importance. A function's importance increases if it triggers a new page or results in more code being executed. Specifically:
    - Navigation-related functions are crucial. These functions correspond to buttons usually located in menus, navigation drawers, or Tabs. These buttons are typically used to guide users to switch between different pages and enter pages with different functions. These buttons usually have the following characteristics:
        1. They are usually located at the top or bottom of the page.
        2. They usually appear in groups, possibly wrapped in a ScrollView.
        3. In the HTML description, their resource-id attributes may be the same or similar, and the resource-id may also contain “tab”. Their class should be the same; their text attributes have a similar format and are highly general.
    - functions central to the page's main purpose, like video playback on a video page (play, like, subscribe, comment) or settings adjustments on a settings page.
    - Any other functions you believe could trigger new pages or enhance code coverage.
)";

const std::string _requiredOutputPrompt_state3 = R"(
Based on the HTML description of this page, your tasks include:

1. Page Overview: Summarize the current page, concluding what kind of information the page mainly presents to users, and what this page is primarily used for.
2. Function Analysis: Identify the functions present on the page, listing their corresponding element IDs. Prioritize these functions by importance. A function's importance increases if it triggers a new page or results in more code being executed. Specifically:
    - Navigation-related functions are crucial. These functions correspond to buttons usually located in menus, navigation drawers, or Tabs. These buttons are typically used to guide users to switch between different pages and enter pages with different functions. These buttons usually have the following characteristics:
        1. They are usually located at the top or bottom of the page.
        2. They usually appear in groups, possibly wrapped in a ScrollView.
        3. In the HTML description, their resource-id attributes may be the same or similar, and the resource-id may also contain “tab”. Their class should be the same; their text attributes have a similar format and are highly general.
    - functions central to the page's main purpose, like video playback on a video page (play, like, subscribe, comment) or settings adjustments on a settings page.
    - Any other functions you believe could trigger new pages or enhance code coverage.
3. Page Importance Ranking: Assess this page's significance relative to the entire app, considering its content and functions in relation to the app’s category and main functions. For example, if this page is a homepage or one of the main pgaes or includes core functions, it's considered more important.
    - I will also provide descriptions and function lists for five other pages. Compare the importance of these pages with the current one and rank the top five most important pages.
)";

const std::string _requiredOutputPrompt_state_summary2 = R"(
In summary, your response should include:

1. A concise summary of the page, within 30 words.
2. A list of the page's functions, including their element IDs, sorted by importance.
)";

const std::string _requiredOutputPrompt_state_summary3 = R"(
In summary, your response should include:

1. A concise summary of the page, within 30 words.
2. A list of the page's functions, including their element IDs, sorted by importance. If you believe the current page is empty or has no function, you can return an empty function list.
3. A ranking of the top five most important pages among the current and the other five pages.
)";

const std::string _anwserFormatPrompt_state2 = R"(
Your anwser should be in json form. Here are the key elements to include:
- "Overview": A string that provides a summary of the page.
- "Function List": An object consisting of key-value pairs that list the functions in order of importance. The key is a string describing the function, and the value is an integer representing the element ID, which can be obtained from the 'id' attribute of the elements in the HTML description.
Note that the key must not be changed!
An example is given below, where "navigate to 'News'" and "navigate to 'My'" are the navigation-related functions you believed.
{
  "Overview": "Main page of the app, providing buttons to navigate to other tabs, and functions for searching and playing videos.",
  "Function List": {
    "navigate to 'News'": 29,
    "navigate to 'My'": 28,
    "play a video": 15,
    ...
  }
}
)";

const std::string _anwserFormatPrompt_state3 = R"(
Your anwser should be in json form. Here are the key elements to include:
- "Overview": A string that provides a summary of the page.
- "Function List": An object consisting of key-value pairs that list the functions in order of importance. The key is a string describing the function, and the value is an integer representing the element ID, which can be obtained from the 'id' attribute of the elements in the HTML description.
- "Top5": An array of integers indicating the indices of the top five most important pages, where the index is the number behind "State".
Note that the key must not be changed!
An example is given below, where "navigate to 'News'" and "navigate to 'My'" are the navigation-related functions you believed.
{
  "Overview": "Main page of the app, providing buttons to navigate to other tabs, and functions for searching and playing videos.",
  "Function List": {
    "navigate to 'News'": 29,
    "navigate to 'My'": 28,
    "play a video": 15,
    ...
  },
  "Top5": [1, 3, 2, 7, 4]
}
)";



//////////////////////////////////////////////////////////////////////////////
// Guide
//////////////////////////////////////////////////////////////////////////////

const std::string _inputExplanationPrompt_guide = R"(
After a period of testing, we have identified some pages (referred to as States below) and had you analyze their roles and functionalities. Based on this, I also asked you to rank these States in terms of their importance to the overall app.
Below is a list of States you ranked from highest to lowest importance in previous discussions. Each State includes its Overview and FunctionList, with FunctionList containing the five most important untested functions of that page.
)";
// 3. The UI transition graph (UTG) for these pages. The UTG represents the transition relationships between these states. Each node in the UTG is labeled as "State + Number," where the number after "State" represents the sequence number of that state. The edges in the UTG are represented as "-- Action -->", where the action indicates that after performing that action on a control in the previous state, the app transitions to the next state. For example, "State15 -- CLICK (text:"More") --> State16" means that after clicking a control with the text "More" in State15, the app transitions to State16.


const std::string _requiredOutputPrompt_guide_part1 = R"(
Based on the information above, please decide: Which State should we go next, and what function would be most appropriate to test in the target State?
Your main objective should be to explore new pages and enhance code coverage by executing this function.
Specifically, you can follow these strategies:
1. Do not select function that has been chosen before:)";

const std::string _requiredOutputPrompt_guide_part2 = R"(
2. Do not choose functions related to login or registration.
3. Prioritize choosing functions related to navigation.
4. Choose other function which can trigger transition or lead to undiscovered pages.
5. If there are no navigation-related functions, you can choose a core function from the higher-ranked pages, like video playback on a video page (play, like, subscribe, comment) or settings adjustments on a settings page.
)";
//These functions often belong to elements like Menu buttons, Navigation Drawer or Tabs. These elements can provide access to different pages and features, or allow users to switch between different content areas.
//4. Attempt to access the settings page and adjust settings you deem important. The settings page typically contains advanced options, configurations, and preferences of an application. Testing this page allows for changing the app's settings, effectively increasing the code coverage.The entrance to the settings interface is typically found under 'Account' or 'My' pages.

const std::string _answerFormatPrompt_guide = R"(
Your anwser should be in json form. Here are the key elements to include:
- "Target State": The State you want to go to, which contains the functionality you want to test.
- "Target Function": The function you want to test in the "Target State". This function must be chosen from the provided "Function List" of the corresponding State and cannot be made up.

Please note that the key must not be changed. You should only give me one choice!
Your final output should only contain the json result and no more. An example is given below:
{
    "Target State": "State2",
    "Target Function": "navigate to 'News'"
}
)";


//////////////////////////////////////////////////////////////////////////////
// Test Function
//////////////////////////////////////////////////////////////////////////////

const std::string _inputExplanationPrompt_functionTest = R"(
The app's current page is provided using HTML, including the components and their structural information.
I use five types of HTML tags, namely <button>, <checkbox>, <scroller>, <input>, and <p>, which represent elements that can be clicked, checked, swiped, edited, and any other views respectively.
Each HTML element has the following attributes:
id(the unique id of this component), class(the class name of this component), resource-id (the resource-id of this Android component), content-desc (the content description of this component), text (the text of this component), direction (if this component is scrollable, indicating its scroll direction), value (the text that has been input to the text box).
)";

const std::string _requiredOutputPrompt_functionTest = R"(
What action should I perform next to test the target function?
)";

const std::string _answerFormatPrompt_functionTest = R"(
Your response should include the selected element's id and the action to be performed on that element.
The available types of actions include: click (0), long press (1), swipe from top to bottom (2), swipe from bottom to top (3), swipe from left to right (4), swipe from right to left (5) and input text (6).
Your answer should be in json form.
The key "Element Id" represents the value of the id attribute of the corresponding tag in the HTML description of the element you have chosen.
The key "Action Type" represents the type of action you want to perform on the element, please use the number in the parentheses of the action type.
The key "Input" represents the text you want to input to the target element, the value should be generated by you. This key is only needed when the value of "Action Type" is 6.
If you believe the target function is finished testing and no more action is needed, the value of "Element Id" should be -1, the value of "Action Type" should be 0.
Please note that the key must not be changed. The output should be pure json string starting with "{", NOT begin with "```json", and must not contain comments.
An example is given below, indicates that you have selected the 2nd element and performed operation 4 on it, which is a scroll from left to right.
{
    "Element Id": 2,
    "Action Type": 4
}
Another example is also given below to demonstrate the situation that requires input, indicates that you have selected the 13th element and input "apple" to it.
{
    "Element Id": 13,
    "Action Type": 6,
    "Input": "apple"
}
)";

const std::string _answerFormatPrompt_functionTestEmpty = R"(
If you believe that the current page is the page that should be reached after executing the target function, or if the current page lacks the corresponding element to complete the target function, your response should be as follows:
{
    "Element Id": -1,
    "Action Type": 0
}
)";


//////////////////////////////////////////////////////////////////////////////
// Reanalysis
//////////////////////////////////////////////////////////////////////////////

const std::string inputExplanationReanalysis1 = R"(
You have previously analyzed a page and summarized its Overview and Function list.
)";

const std::string inputExplanationReanalysis2 = R"(
Now, you are provided with a set of similar pages containing controls not present in the previous page. Your task is to analyze the potential functions corresponding to these controls.

The controls are provided in HTML format, consisting of five types of HTML tags: <button>, <checkbox>, <scroller>, <input>, and <p>, which represent elements that can be clicked, checked, swiped, edited, and other views, respectively.
Each HTML element has the following attributes: id (the unique ID of this component), class (the class name of this component), resource-id (the resource ID of this Android component), content-desc (the content description of this component), text (the text of this component), direction (if this component is scrollable, indicating its scroll direction)
value (the text that has been input to the text box)
)";

const std::string requiredOutputReanalysis = R"(
Based on the HTML components, the page's Overview, and the existing Function List, your tasks are as follows:

1. Analyze the functions corresponding to the controls that have an id attribute. Cross-reference these functions with the existing function list, prioritizing matches to ensure consistency.
2. Rank the importance of these functions. A function's importance increases if it triggers a new page or results in more code being executed. Specifically:
	- Navigation-related functions are crucial.
	- Functions central to the page's main purpose, such as video playback on a video page (play, like, subscribe, comment) or settings adjustments on a settings page.
	- Any other functions you believe could trigger new pages or enhance code coverage.
)";

const std::string answerFormatReanalysis = R"(
You should always respond using the correct JSON format.
The key is the control's `id` attribute, which must be a string representation of an integer.
The value is the corresponding function of that control.
The closer a key-value pair is to the top, the higher the importance of its function.
If there is no `id` attribute in html controls, just return an empty json.
Please note that the output should be pure json string starting with "{", NOT begin with "```json", and must not contain comments.

An example is given below to demonstrate the situation that Control 5 has the function "navigate to 'News'" and is the highest in importance; Control 3 has the function "navigate to 'My'", which is second highest; Control 9 has the function "play a video", and so on.
{
    "5": "navigate to 'News'",
    "3": "navigate to 'My'",
    "9": "play a video",
    ...
}
)";


//const std::string StartPrompt = R"(I'm now testing an app called Souhu Video on android.
//Sohu Video is a platform that provides a variety of video content, including news, entertainment, sports, movies, TV series, animation, cars, technology, etc. Users can watch, upload, search, comment, collect, and share videos online. They can also subscribe verified uploaders and popular topics.
//)";
//// This app is a networking toolset app that can scan devices on the network, assess network status, and analyze network security. It also provides many useful utilities such as ping, port scanning, and speed test.
//
//const std::string _subTaskPrompt = "A 'function' represents a specific function that can be performed on an app page, which may involve one or multiple touch events. "
//                                   "For example, the main page of a video playback app may include the following sub tasks: "
//                                   "search, play a video, navigate to 'other' page, access settings, log in, etc. "
//                                   "I will provide a description of an app page, and you need to analyze its content to generate a list of sub tasks included on that page. "
//                                   "Additionally, I will also provide an existing sub task list. "
//                                   "If a sub task from this page already exists in the list, please use the elements from the sub task list to answer\n";
//
//const std::string _inputExplanationPrompt = R"(
//I will provide the following three types of information:
//1. A set of similar pages described in HTML format. In the HTML description of this set of pages, the root state will provide a complete description of the page (including the page structure and control information), while the descriptions of other pages will only include the controls that are newly added compared to the root state.
//2. The UI transition graph (UTG) for this set of pages. The UTG represents the transition relationships between this group of pages. Each node in the UTG is labeled as "State + Number," where the number after "State" represents the sequence number of that state. The edges in the UTG are represented as "-- Action -->", where the action indicates that after performing that action on a control in the previous state, the app transitions to the next state. For example, "State15 -- CLICK (text:"More") --> State16" means that after clicking a control with the text "More" in State15, the app transitions to State16.
//3. Function list. A list of functions represented in key-value pairs, indicating the functionalities present in the current MergedState and whether they have been completed. The values in this list are entirely based on your previous responses. If the list of functions is empty, it means that the MergedState is newly created, and you have not yet generated a function list for it.
//)";
//
//const std::string _inputExplanationPrompt_graphoverview = R"(
//After a period of testing, several states have been discovered.
//I will provide the following two types of information:
//1. Overview of each State.A general description of the State.
//2. List of actions performed within the State. Actions performed within the State that do not lead to page transitions, keeping the app on the same State.
//3. Function list. A list of functions represented in key-value pairs, where the key is the name of the function and the value is the rating of the completion of that function. The score ranges from 0 to 5, where 0 indicates that the function has not been tested yet, and 5 indicates that the function has been thoroughly tested.
//4. The UI transition graph (UTG) for these pages. The UTG represents the transition relationships between these states. Each node in the UTG is labeled as "State + Number," where the number after "State" represents the sequence number of that state. The edges in the UTG are represented as "-- Action -->", where the action indicates that after performing that action on a control in the previous state, the app transitions to the next state. For example, "State15 -- CLICK (text:"More") --> State16" means that after clicking a control with the text "More" in State15, the app transitions to State16.
//)";
//
//const std::string _requiredOutputPrompt_graph = R"(
//Combining the overviews, the function list, actions that have been executed of each state, and the transition relationships between states in the UTG diagram, please analyze which functionalities have been completed in which states during this period.
//Because some functionalities may involve a series of operations, you need to assess the completion status of each function. Please rate the completion level of each function in the function list for each state. The score ranges from 0 to 5, where 0 indicates that the function has not been tested yet, and 5 indicates that the function has been thoroughly tested.
//Note that in the provided function lists for each State, the completion level of each function is already included. This rating comes from your previous evaluations, so your ratings should be based on these, and the completion scores for each function should not be lower than originally assessed.
//Scores must be integers, and your output should include each function in the Function List for each state.
//)";
//
//const std::string _answerFormatPrompt_graph = R"(
//Your anwser should be in json form. Please note that the key must not be changed.
//The output should be pure json string starting with "{", NOT begin with "```json", and must not contain comments.
//An example is given below:
//{
//    "State12":
//    {
//        "Function List": {
//            "search": 3,
//            "navigate to 'News'": 0,
//            "navigate to 'My'": 5,
//            "play a video": 0
//        }
//    },
//    "State15":
//    {
//        "Function List": {
//            "log in": 1,
//            "create an account": 0
//        }
//    }
//}
//)";

#endif