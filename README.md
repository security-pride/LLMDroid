# LLMDroid

## Overview

LLMDroid is a novel testing framework designed to enhance existing automated mobile GUI testing tools by leveraging LLMs more efficiently.

We applied LLMDroid to three popular open-source Android automated testing tools: Droidbot, Humanoid (based on Droidbot), and Fastbot.

## File Structure

> LLMDroid
>
> ├── ExperimentalDataset	The directory for instrumented APKs.
>
> ├── LLMDroid-Droidbot	The directory for source code of LLMDroid-Droidbot.
>
> ├── LLMDroid-Humanoid	The directory for source code of LLMDroid-Humanoid.
>
> └── LLMDroid-Fastbot	The directory for source code of LLMDroid-Fastbot.

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

The following table shows the code coverage and Activity coverage for the 14 apps tested by Monkey and three improved testing tools using the LLMDroid framework, compared to the original tools.

|      App Name       |  Monkey  | Droidbot | LLMDroid-Droidbot |  Impr  | Humanoid | LLMDroid-Huamnoid | Improve | Fastbot | LLMDroid-Fastbot |  Impr  |
| :-----------------: | :------: | :------: | :---------------: | :----: | :------: | :---------------: | :-----: | :-----: | :--------------: | :----: |
| Wish: Shop and Save |  6.97%   |  11.53%  |      15.00%       | 40.68% |  14.88%  |      17.57%       | 18.04%  |  9.58%  |      16.23%      | 69.37% |
|       Twitch        |  10.05%  |  14.41%  |      19.90%       | 38.13% |  18.46%  |      20.02%       |  8.49%  | 16.22%  |      24.64%      | 51.97% |
|      Wikipedia      |  12.08%  |  9.73%   |      14.13%       | 45.28% |  16.08%  |      18.98%       | 18.06%  | 16.78%  |      20.71%      | 23.39% |
|        Quora        |  2.87%   |  2.53%   |       3.81%       | 50.58% |  3.65%   |       4.14%       | 13.37%  |  3.79%  |      4.21%       | 11.04% |
|        Fing         |  18.59%  |  16.32%  |      20.12%       | 23.25% |  19.02%  |      21.93%       | 15.30%  | 16.50%  |      24.80%      | 50.30% |
|    Lefun Health     | 2.87151% |  2.71%   |       2.96%       | 9.34%  |  2.18%   |       2.58%       | 18.18%  |  3.67%  |      3.96%       | 17.53% |
|     Red Bull TV     |  1.47%   |  5.29%   |       6.39%       | 20.84% |  5.27%   |       6.17%       | 17.15%  |  4.35%  |      5.88%       | 35.43% |
|    Time Planner     |  15.39%  |  9.57%   |      14.69%       | 53.39% |  12.69%  |      15.45%       | 21.77%  | 17.35%  |      21.98%      | 26.67% |
|       NewPipe       |  16.75%  |  20.67%  |      24.37%       | 17.89% |  23.04%  |      33.14%       | 43.81%  | 22.06%  |      33.86%      | 42.68% |
|   NHK WORLD-JAPAN   |  6.77%   |  17.87%  |      18.09%       | 1.22%  |  17.23%  |      22.83%       | 32.53%  | 20.46%  |      23.03%      | 12.56% |
|    Renpho Health    |  2.31%   |  3.14%   |       3.67%       | 16.89% |  2.99%   |       3.03%       |  1.32%  |  3.50%  |      4.01%       | 14.61% |
|    Mood Tracker     |  4.76%   |  4.09%   |       5.00%       | 22.29% |  4.71%   |       5.71%       | 21.36%  |  7.78%  |      8.05%       | 3.52%  |
|      CafeNovel      |  4.20%   |  4.11%   |       4.81%       | 16.97% |  5.01%   |       5.41%       | 57.90%  |  5.49%  |      5.81%       | 5.87%  |
|       OceanEx       | % 6.40%  |  8.52%   |      10.17%       | 19.29% |  9.44%   |      10.61%       | 12.35%  |  9.81%  |      12.38%      | 26.10% |
|         ACC         |  8.86%   |  9.21%   |      11.69%       | 26.90% |  11.05%  |      13.04%       | 21.29%  | 11.36%  |      14.80%      | 30.30% |
|         AAC         |  11.34%  |  15.90%  |      22.46%       | 41.29% |  19.16%  |      23.66%       | 23.51%  | 24.40%  |      30.05%      | 23.13% |

Impr.：Improvement

ACC: Average Code Coverage, AAC: Average Activity Coverage

### 