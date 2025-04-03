import time
from .base_monitor import CodeCoverageMonitor

import re
import threading
import subprocess
import os



def get_log_type(line):
    match = re.search(r"(\w+?)=", line)
    return match.group(1) if match else None


def extract_content(line, prefix):
    match = re.search(r"=(.*)", line)
    return match.group(1) if match else None


class AndroLogCVMonitor(CodeCoverageMonitor):
    # TODO 支持指定udid
    def __init__(self, save_dir: str = '', total=62491, tag: str = "FING_SUPER_LOG",
                 wsize=10, min_growth_rate=0.05, factor=0.5):

        super().__init__(save_dir=save_dir, wsize=wsize, min_growth_rate=min_growth_rate, factor=factor)

        self.method_count = 1
        self.last_method_count = 1
        self.rate = 0.0
        self.__total = total
        self.__log_tag = tag
        self.summary = {}
        self.visited_components = set()


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



if __name__ == "__main__":
    monitor = AndroLogCVMonitor()
    monitor.start_logcat_listener()

    while True:
        low = monitor.check_low_growth_rate()
        print(low)
        time.sleep(3)
