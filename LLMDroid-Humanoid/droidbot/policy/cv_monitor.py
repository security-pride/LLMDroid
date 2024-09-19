import logging
import time
from ..global_log import get_logger

import re
import threading
import subprocess
import os
import math


def get_log_type(line):
    match = re.search(r"(\w+?)=", line)
    return match.group(1) if match else None


def extract_content(line, prefix):
    match = re.search(r"=(.*)", line)
    return match.group(1) if match else None


class CodeCoverageMonitor:

    def __init__(self, save_dir: str = '', total=62491, tag: str = "FING_SUPER_LOG",
                 wsize=10, min_growth_rate=0.05, factor=0.5):

        self.logger = get_logger()
        self.__file_path = os.path.join(save_dir, 'codecoverage.txt')

        self.method_count = 1
        self.last_method_count = 1
        self.rate = 0.0
        self.__total = total
        self.__log_tag = tag
        self.summary = {}
        self.visited_components = set()

        # monitor
        self.__WINDOW_SIZE: int = wsize
        self.__MIN_GROWTH_RATE: float = min_growth_rate
        self.__FACTOR: float = factor
        self.__MIN_THRESHOLD = 0.01
        self.__window = (0, self.__WINDOW_SIZE)
        self.__cv_history: list[float] = []
        self.__adjusted_threshold = self.__MIN_GROWTH_RATE
        self.__growth_rate_sum = 0.0
        self.__gr_to_check: list[float] = []

        try:
            with open(self.__file_path, 'w') as file:
                file.write("code coverage\n")
                local_time = time.localtime(time.time())
                formatted_local_time = time.strftime('%Y-%m-%d %H:%M:%S', local_time)
                file.write(f"start time: {formatted_local_time}\n")
        except IOError as e:
            self.logger.error(e)
        self.logger.info(f"[CodeCoverageMonitor] total methods: {total}, TAG: {tag}")
        try:
            subprocess.run(["adb", "logcat", "-c"], check=True)
            self.logger.info("[CodeCoverageMonitor] clear adb log cache")
        except subprocess.CalledProcessError as e:
            self.logger.error(e)

    def __increment(self, key):
        with threading.Lock():
            self.method_count += 1  # update count

    def __increment_method(self, method):
        self.__increment_component("methods", method)

    def __increment_component(self, type_, component):
        if f"{type_}{component}" not in self.visited_components:
            self.visited_components.add(f"{type_}{component}")
            self.__increment(type_)

    def __analyze_line(self, line):
        log_type = get_log_type(line)
        if log_type == "METHOD":
            self.__increment_method(extract_content(line, "METHOD="))

    def print_summary(self):
        for key, value in self.summary.items():
            print(f"{key} : {value}")

    def get_summary(self):
        return self.summary

    def get_method_summary(self):
        return self.summary.get("methods")

    def start_logcat_listener(self):
        def listener():
            while True:
                try:
                    subprocess.run(["adb", "logcat", "-c"], check=True)
                    self.logger.info("[CodeCoverageMonitor] clear adb log cache")
                    cmd = ["adb", "logcat", "-s", self.__log_tag]
                    process = subprocess.Popen(cmd, stdout=subprocess.PIPE, text=True)
                    for line in process.stdout:
                        self.__analyze_line(line)
                    self.logger.warning(
                        "[CodeCoverageMonitor] ******************** !! thread stop !! *************************")
                    self.logger.warning(
                        "[CodeCoverageMonitor] ******************** !! restart logcat !! **********************")
                except subprocess.CalledProcessError as e:
                    print(e)

        thread = threading.Thread(target=listener)
        thread.setDaemon(True)
        thread.start()

    def __compute_increment(self) -> float:
        with threading.Lock():
            current_method_count = self.method_count
            # code coverage growth rate
            self.rate = ((current_method_count - self.last_method_count) / self.last_method_count)
            # code coverage percentage
            percentage: float = (current_method_count / self.__total) * 100
            self.last_method_count = current_method_count
        str_ = f"[{self.__log_tag}] {percentage:8.5f}% ({current_method_count}/{self.__total}) --> {self.rate:8.5f}"
        self.logger.info(str_)
        try:
            with open(self.__file_path, 'a') as file:
                file.write(str_ + "\n")
        except IOError as e:
            print(e)
        return percentage

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


    def check_low_growth_rate(self) -> bool:
        # update current code coverage and adjust threshold
        self.__update(self.__compute_increment())

        if len(self.__gr_to_check) == self.__WINDOW_SIZE:
            reverse = self.__gr_to_check[::-1]
            for i, gr in enumerate(reverse):
                if gr > self.__adjusted_threshold:
                    self.logger.info(f"[CV_Monitor Check] {i} to the end")
                    return False
            return True
        else:
            return False

    def clear(self):
        self.__gr_to_check.clear()


if __name__ == "__main__":
    monitor = CodeCoverageMonitor()
    monitor.start_logcat_listener()

    while True:
        low = monitor.check_low_growth_rate()
        print(low)
        time.sleep(3)
