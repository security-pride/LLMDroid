# LLMDroid

## Overview

LLMDroid is a novel testing framework designed to enhance existing automated mobile GUI testing tools by leveraging LLMs more efficiently.

We applied LLMDroid to three popular open-source Android automated testing tools: Droidbot, Humanoid (based on Droidbot), and Fastbot2.

## File Structure

> LLMDroid
>
> ├── ExperimentalDataset	The directory for instrumented APKs.
>
> ​	├── configs	Pre-configured `config.json` files for dataset apps.
>
> ├── LLMDroid-Droidbot	The directory for source code of LLMDroid-Droidbot.
>
> ├── LLMDroid-Humanoid	The directory for source code of LLMDroid-Humanoid.
>
> ├── LLMDroid-Fastbot	The directory for source code of LLMDroid-Fastbot.
>
> └── JacocoBridge	The Jacoco implementation computes real-time code coverage from execution data (.ec files). The jar is invoked by LLMDroid-Droidbot during operation, but may also be used independently as a real-time coverage collection utility.

## Experimental Dataset

We selected 14 apps from GooglePlay's top charts and their recommended similar apps, and instrumented them using the [AndroLog](https://github.com/JordanSamhi/AndroLog) tool. 

The instrumented APKs can be found in the `ExperimentalDataset` directory.

Below is information about the 14 apps, including their names, categories, download counts, etc. More detailed information such as package names, instrumented log tags, total method counts, and so on can be found in the README file in the `ExperimentalDataset` directory.

|  ID  |            App Name             |   Category    | Downloads | Apk Size |
| :--: | :-----------------------------: | :-----------: | :-------: | :------: |
|  1   |       Wish: Shop and Save       |   Shopping    |   500M+   |   43MB   |
|  2   |     Twitch: Live Streaming      | Entertainment |   100M+   |  157MB   |
|  3   |            Wikipedia            |   Education   |   50M+    |  33.2MB  |
|  4   |  Quora: the knowledge platform  |   Education   |   50M+    |  10.3MB  |
|  5   |      Fing - Network Tools       |     Tools     |   50M+    |  41.5MB  |
|  6   |          Lefun Health           |    Health     |   10M+    |  142MB   |
|  7   |  Red Bull TV: Videos \& Sports  |    Sports     |   10M+    |  19.3MB  |
|  8   | Time Planner: Schedule \& Tasks |     Tools     |    5M+    |  12.5MB  |
|  9   |             NewPipe             | Entertainment |    5M+    |  13.7MB  |
|  10  |         NHK WORLD-JAPAN         |     News      |    1M+    |  7.39MB  |
|  11  |          Renpho Health          |    Health     |    1M+    |  100MB   |
|  12  | Mood Tracker: Self-Care Habits  |     Life      |   500K+   |  42.9MB  |
|  13  | CafeNovel - Fiction \& Webnovel |     Book      |   10K+    |  19.0MB  |
|  14  |             OceanEx             |  Commercial   |   10K+    |  14.9MB  |

## Experimental Results

### Tool Performance (RQ1)

The following table shows the code coverage and Activity coverage for the 14 apps tested by Monkey and three improved testing tools using the LLMDroid framework, compared to the original tools.

|      App Name       |   Monkey   |  Droidbot  | LLMDroid-Droidbot |    Impr    |  Humanoid  | LLMDroid-Huamnoid |  Improve   |  Fastbot   | LLMDroid-Fastbot |    Impr    |
| :-----------------: | :--------: | :--------: | :---------------: | :--------: | :--------: | :---------------: | :--------: | :--------: | :--------------: | :--------: |
| Wish: Shop and Save |   6.97%    |   11.53%   |      15.00%       |   40.68%   |   14.88%   |      17.57%       |   18.04%   |   9.58%    |      16.23%      |   69.37%   |
|       Twitch        |   10.05%   |   14.41%   |      19.90%       |   38.13%   |   18.46%   |      20.02%       |   8.49%    |   16.22%   |      24.64%      |   51.97%   |
|      Wikipedia      |   12.08%   |   9.73%    |      14.13%       |   45.28%   |   16.08%   |      18.98%       |   18.06%   |   16.78%   |      20.71%      |   23.39%   |
|        Quora        |   2.87%    |   2.53%    |       3.81%       |   50.58%   |   3.65%    |       4.14%       |   13.37%   |   3.79%    |      4.21%       |   11.04%   |
|        Fing         |   18.59%   |   16.32%   |      20.12%       |   23.25%   |   19.02%   |      21.93%       |   15.30%   |   16.50%   |      24.80%      |   50.30%   |
|    Lefun Health     |   2.87%    |   2.71%    |       2.96%       |   9.34%    |   2.18%    |       2.58%       |   18.18%   |   3.67%    |      3.96%       |   17.53%   |
|     Red Bull TV     |   1.47%    |   5.29%    |       6.39%       |   20.84%   |   5.27%    |       6.17%       |   17.15%   |   4.35%    |      5.88%       |   35.43%   |
|    Time Planner     |   15.39%   |   9.57%    |      14.69%       |   53.39%   |   12.69%   |      15.45%       |   21.77%   |   17.35%   |      21.98%      |   26.67%   |
|       NewPipe       |   16.75%   |   20.67%   |      24.37%       |   17.89%   |   23.04%   |      33.14%       |   43.81%   |   22.06%   |      33.86%      |   42.68%   |
|   NHK WORLD-JAPAN   |   6.77%    |   17.87%   |      18.09%       |   1.22%    |   17.23%   |      22.83%       |   32.53%   |   20.46%   |      23.03%      |   12.56%   |
|    Renpho Health    |   2.31%    |   3.14%    |       3.67%       |   16.89%   |   2.99%    |       3.03%       |   1.32%    |   3.50%    |      4.01%       |   14.61%   |
|    Mood Tracker     |   4.76%    |   4.09%    |       5.00%       |   22.29%   |   4.71%    |       5.71%       |   21.36%   |   7.78%    |      8.05%       |   3.52%    |
|      CafeNovel      |   4.20%    |   4.11%    |       4.81%       |   16.97%   |   5.01%    |       5.41%       |   57.90%   |   5.49%    |      5.81%       |   5.87%    |
|       OceanEx       |   6.40%    |   8.52%    |      10.17%       |   19.29%   |   9.44%    |      10.61%       |   12.35%   |   9.81%    |      12.38%      |   26.10%   |
|       **ACC**       | **8.86%**  | **9.21%**  |    **11.69%**     | **26.90%** | **11.05%** |    **13.04%**     | **21.29%** | **11.36%** |    **14.80%**    | **30.30%** |
|       **AAC**       | **11.34%** | **15.90%** |    **22.46%**     | **41.29%** | **19.16%** |    **23.66%**     | **23.51%** | **24.40%** |    **30.05%**    | **23.13%** |

Impr.：Improvement

ACC: Average Code Coverage

AAC: Average Activity Coverage

### Performance and Cost under Different LLMs (RQ2)

#### Code Coverage

| **APP**                        | **LLMDroid-Fastbot-3.5** | **LLMDroid-Fastbot-4o-mini** | **LLMDroid-Fastbot-4o** | **DroidAgent-4o+mini** | **DroidAgent-4o+3.5** | **GPTDroid-3.5** | **GPTDroid-4o-mini** |
| ------------------------------ | ------------------------ | ---------------------------- | ----------------------- | ---------------------- | --------------------- | ---------------- | -------------------- |
| Twitch                         | 22.23%                   | 23.19%                       | 23.81%                  | 16.10%                 | 14.83%                | 11.76%           | 14.06%               |
| NewPipe                        | 21.58%                   | 22.91%                       | 28.28%                  | 26.06%                 | 23.94%                | 13.99%           | 12.66%               |
| Fing                           | 23.99%                   | 24.61%                       | 25.66%                  | 20.15%                 | 18.24%                | 12.97%           | 12.41%               |
| Wiki                           | 17.11%                   | 18.04%                       | 19.09%                  | 14.72%                 | 13.53%                | 7.03%            | 7.40%                |
| Cafa Novel                     | 5.96%                    | 5.84%                        | 5.77%                   | 4.76%                  | 4.56%                 | 2.91%            | 2.78%                |
| Ocean ex                       | 7.99%                    | 9.85%                        | 10.67%                  | 7.31%                  | 7.30%                 | 5.16%            | 4.97%                |
| Mood Tracker: Self-Care Habits | 7.64%                    | 7.82%                        | 7.96%                   | 4.98%                  | 5.06%                 | 3.97%            | 4.39%                |
| Wish: Shop and Save            | 16.10%                   | 15.41%                       | 16.33%                  | 15.93%                 | 17.15%                | 11.47%           | 10.40%               |
| Quora: the knowledge platform  | 3.86%                    | 4.26%                        | 4.23%                   | 3.10%                  | 3.79%                 | 3.12%            | 2.88%                |
| Time Planner: Schedule & Tasks | 19.43%                   | 19.81%                       | 20.50%                  | 14.34%                 | 15.63%                | 7.65%            | 8.13%                |
| Red Bull TV: Videos & Sports   | 5.69%                    | 5.87%                        | 6.86%                   | 4.11%                  | 3.88%                 | 4.54%            | 4.29%                |
| NHK WORLD-JAPAN                | 22.47%                   | 23.02%                       | 22.94%                  | 15.47%                 | 16.71%                | 15.20%           | 15.75%               |
| Lefun Health                   | 3.64%                    | 3.39%                        | 3.64%                   | 2.37%                  | 2.77%                 | 2.03%            | 1.88%                |
| Renpho Health                  | 4.40%                    | 4.50%                        | 4.90%                   | 3.02%                  | 3.39%                 | 2.16%            | 2.27%                |
| **Average**                    | **13.01%**               | **13.47%**                   | **14.33%**              | **10.89%**             | **10.77%**            | **7.43%**        | **7.45%**            |



#### API Cost

(Unit: USD)

| **APP**                        | **LLMDroid-Fastbot-3.5** | **LLMDroid-Fastbot-4o-mini** | **LLMDroid-Fastbot-4o** | **DroidAgent-4o+mini** | **DroidAgent-4o+3.5** | **GPTDroid-3.5** | **GPTDroid-4o-mini** |
| ------------------------------ | ------------------------ | ---------------------------- | ----------------------- | ---------------------- | --------------------- | ---------------- | -------------------- |
| Twitch                         | 0.1400                   | 0.0901                       | 0.8578                  | 0.7965                 | 1.9814                | 0.3182           | 0.0603               |
| NewPipe                        | 0.0807                   | 0.0368                       | 0.5183                  | 0.9958                 | 1.4734                | 0.1779           | 0.0548               |
| Fing                           | 0.0843                   | 0.0262                       | 0.5520                  | 0.9465                 | 1.8882                | 0.1951           | 0.0611               |
| Wiki                           | 0.1078                   | 0.0391                       | 0.7145                  | 1.013                  | 1.8584                | 0.0793           | 0.0547               |
| Cafa Novel                     | 0.0746                   | 0.0190                       | 0.3517                  | 0.9394                 | 1.4708                | 0.1903           | 0.0628               |
| Ocean ex                       | 0.0832                   | 0.0268                       | 0.3133                  | 1.0035                 | 1.5318                | 0.1653           | 0.0478               |
| Mood Tracker: Self-Care Habits | 0.0872                   | 0.0273                       | 0.4379                  | 0.874                  | 1.6947                | 0.2905           | 0.0773               |
| Wish: Shop and Save            | 0.1135                   | 0.0336                       | 0.5229                  | 0.9702                 | 1.837                 | 0.2862           | 0.0811               |
| Quora: the knowledge platform  | 0.0743                   | 0.0350                       | 0.3927                  | 0.5768                 | 1.7273                | 0.3261           | 0.0521               |
| Time Planner: Schedule & Tasks | 0.0853                   | 0.0319                       | 0.466                   | 0.7288                 | 1.6257                | 0.1597           | 0.0666               |
| Red Bull TV: Videos & Sports   | 0.0463                   | 0.0237                       | 0.3734                  | 3.6221                 | 2.8841                | 0.1422           | 0.0454               |
| NHK WORLD-JAPAN                | 0.0570                   | 0.0244                       | 0.5297                  | 0.8453                 | 1.4472                | 0.1630           | 0.0386               |
| Lefun Health                   | 0.0847                   | 0.0216                       | 0.3682                  | 0.8662                 | 1.1283                | 0.1945           | 0.0603               |
| Renpho Health                  | 0.0978                   | 0.0311                       | 0.5177                  | 1.0249                 | 1.5455                | 0.2078           | 0.0666               |
| **Average**                    | **0.0869**               | **0.0333**                   | **0.4940**              | **1.0859**             | **1.7210**            | **0.2069**       | **0.0593**           |



## Experimental Environment

Here is the experimental environment we have tested.

### Operating System

- Ubuntu 20.04
- Also tested on Windows 10/11

### Python

- LLMDroid-Droidbot and LLMDroid-Humanoid is compatible with **Python >= 3.9**

### Android Environment

- Command Line Tool Used: ADB, AAPT, AAPT2



## Usage

### Config

First, you need to prepare a `config.json` file before you test an app. 

```json
{
  "AppName": "Fing",
  "Description": "This app is a networking toolset app that can scan devices on the network, assess network status, and analyze network security. It also provides many useful utilities such as ping, port scanning, and speed test.",
  "ApiKey": "",
  "TotalMethod": 62491,
  "Tag": "FING_SUPER_LOG"
}
```

- **AppName**: The app you want to test.
- **Description**: A brief introduction to the app, including its main features and purposes. This helps the LLM better understand the task and improve the testing effectiveness.
- **ApiKey**: The API Key used to invoke the LLM.
- **TotalMethod**: The total number of methods in the app after instrumentation. For apps in the Dataset, this can be found in the table within `ExperimentalDataset`. For other apps, it can be obtained through the AndroLog tool after instrumentation is completed.
- **Tag**: The log tag specified during the app’s instrumentation, used for real-time code coverage statistics. For apps in the Dataset, this can be found in the table within `ExperimentalDataset`. For other apps, it can be specified before using the AndroLog tool for instrumentation.
- **ClassFilePath**: The **`.class`** files generated during APK compilation. You can find them in the `app\\build\\intermediates\\javac\\debug\\classes `directory under your app project. You may copy this directory to another location, as long as LLMDroid can access it.
- **EcFilePath**: The directory where the coverage file is generated during runtime when using Jacoco instrumentation. This must match the location specified when modifying the app's source code. It is recommended to use the path returned by `getExternalFilesDir(null).getPath()`, typically `/storage/emulated/0/Android/data/<package name>/files`.
- **Model: ** The model used, defaults to `gpt-4o-mini`.
- **BaseUrl:** The base URL for API calls. This parameter, along with the "Model" parameter, allows you to call non-OpenAI models as long as the third-party service supports the OpenAI API specification.



**Note:**

- **AndroLog-instrumented apps**: Set `Tag` + `TotalMethod`.  (`ClassFilePath` and `EcFilePath` are not required)
- **Jacoco-instrumented apps**: Set `ClassFilePath` + `EcFilePath`.  (`Tag` and `TotalMethod` are not required)
- Config templates are available in `ExperimentalDataset/configs` (rename to `config.json` before use).



### LLMDroid-Droidbot

- Install required modules

```shell
pip install openai androguard networkx Pillow
```

- Make sure the `config.json` file is under the root path of LLMDroid-Droidbot.

- Make sure the `JacocoBridge.jar` file is under the root path of LLMDroid-Droidbot.

- Enter the `LLMDroid-Droidbot` directory and run the following comand:

```shell
python start.py -d <AVD_SERIAL> -a <APK_FILE> -o <result_dir> -timeout 3600 -interval 3 -count 100000 -keep_app -keep_env -policy dfs_greedy -grant_perm
```

(Parameters are same as Droidbot)	



### LLMDroid-Humanoid

- Install required modules

```shell
pip install openai androguard networkx Pillow
```

- Make sure the `config.json` file is under the root path of LLMDroid-Humanoid.
- Make sure the `JacocoBridge.jar` file is under the root path of LLMDroid-Humanoid.
- Deploy and start the Humanoid agent. (For more details, see [Humanoid](https://github.com/the-themis-benchmarks/Humanoid))

- Enter the `LLMDroid-Humanoid` directory and run the following comand:

```shell
python start.py -d <AVD_SERIAL> -a <APK_FILE> -o <result_dir> -timeout 3600 -interval 3 -count 100000 -keep_app -keep_env -policy dfs_greedy -grant_perm -humanoid 192.168.50.133:50405
```

(Parameters are same as Humanoid)	

The only difference from LLMDroid-Droidbot is the addition of the parameter `-humanoid`, which indicates the IP address and the listening port of the humanoid agent. 



### LLMDroid-Fastbot

If you are simply running the LLMDroid-Fastbot tool, the steps are quite straightforward.

- Push artifacts into your device:  
    - The `faruzan` directory can be renamed as desired.  
    - The `config.json` file must be placed in `/sdcard` and cannot be renamed.  
    - If you are using a Jacoco-instrumented APK, you need to copy the **classes files** to the `/sdcard` directory so that LLMDroid-Fastbot can access them during runtime. Additionally, ensure that the **`ClassFilePath`** in `config.json` is correctly set to this location.

```shell
adb shell mkdir /sdcard/faruzan
adb push config.json /sdcard/config.json
adb push monkey/libs/* /sdcard/faruzan
adb push monkeyq.jar /sdcard/faruzan/monkeyq.jar
adb push fastbot-thirdpart.jar /sdcard/faruzan/fastbot-thirdpart.jar
adb push framework.jar /sdcard/faruzan/framework.jar
adb push libs/* /data/local/tmp/
```

- Run LLMDroid-Fastbot with the following command:

```shell
adb shell CLASSPATH=/sdcard/faruzan/monkeyq.jar:/sdcard/faruzan/framework.jar:/sdcard/faruzan/fastbot-thirdpart.jar:/sdcard/faruzan/org.jacoco.core-0.8.8.jar:/sdcard/faruzan/asm-9.2.jar:/sdcard/faruzan/asm-analysis-9.2.jar:/sdcard/faruzan/asm-commons-9.2.jar:/sdcard/faruzan/asm-tree-9.2.jar exec app_process /system/bin com.android.commands.monkey.Monkey -p $app_package_name --agent reuseq --use-code-coverage androlog --running-minutes 5 --throttle 3000 --output-directory $mobile_output_dir -v -v --bugreport
```

- Special Parameters: (Other parameters are consistent with Fastbot2. For more details, refer to the [Fastbot_Android](https://github.com/bytedance/Fastbot_Android).)  
    - `--use-code-coverage`: Specifies the real-time code coverage monitoring method. The following values are supported:  
        - `androlog`: Uses the androlog method. For closed-source apps, the APK must be instrumented using the androlog tool. Additionally, `Tag` and `TotalMethod` must be set in `config.json`.  
        - `jacoco`: Uses the jacoco method. For open-source apps, the source code must be modified and recompiled.  
        - `time`: Disables real-time code coverage monitoring. Used for debugging, where LLM Guidance mode is triggered at specified intervals.



However, if you wish to modify the code and compile it, you will need to install additional dependencies. For detailed steps, please refer to the README file under LLMDroid-Fastbot.



