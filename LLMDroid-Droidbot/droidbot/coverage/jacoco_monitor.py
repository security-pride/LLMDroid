import subprocess
import jpype
import jpype.imports
from jpype.types import JString
import threading
import time
from .base_monitor import CodeCoverageMonitor


class JacocoCVMonitor(CodeCoverageMonitor):

    def __init__(self, save_dir: str, jarpath: str,
                 ec_file_name: str, ec_file_path: str, class_file_path: str,
                 udid: str = '', wsize=10, min_growth_rate=0.05, factor=0.5
                 ):
        """
        @param ec_file_name
        """
        super().__init__(save_dir=save_dir, wsize=wsize, min_growth_rate=min_growth_rate, factor=factor)
        self.__jar_path = jarpath
        self.__udid: str = udid
        self.__save_dir: str = save_dir

        self.__ec_file_name: str = ec_file_name
        self.__ec_file_path: str = ec_file_path
        self.__class_file_path: str = class_file_path
        self.__last_coverage: float = 0.00001
        self.__coverage_lock = threading.Lock()

        self.logger.info("Starting JVM...")
        jpype.startJVM(classpath=self.__jar_path)
        self.logger.info("JVM started successfully.")

    def __send_broadcast(self):
        # adb shell am broadcast -a com.llmdroid.jacoco.COLLECT_COVERAGE --es coverageFile "coverage1.ec"
        command = ['adb',
                   'shell', 'am', 'broadcast',
                   '-a', 'com.llmdroid.jacoco.COLLECT_COVERAGE',
                   '--es', 'coverageFile', f'"{self.__ec_file_name}"'
                   ]
        if self.__udid != '':
            command.insert(1, '-s')
            command.insert(2, self.__udid)

        self.logger.debug(f"Send broadcast: {' '.join(command)}")
        subprocess.run(command, check=True)
        time.sleep(0.3)

    def _get_code_coverage(self) -> float:

        # self.__send_broadcast()
        # 定义线程事件用于同步
        event = threading.Event()

        def thread_function():
            try:
                self.logger.info("Preparing to call getMethodCoverage method.")

                # create an instance of JacocoBridge
                JacocoBridge = jpype.JClass("org.jacoco.examples.JacocoBridge")  # 替换为类的完整路径
                instance = JacocoBridge(JString(self.__udid), JString(self.__save_dir))

                # prepare parameters
                JavaFile = jpype.JClass("java.io.File")  # 导入 java.io.File 类
                ec_file_name_jstring = JString(self.__ec_file_name)  # 转换为 Java 字符串
                ec_file_path_jstring = JString(self.__ec_file_path)  # 转换为 Java 字符串
                class_file = JavaFile(self.__class_file_path)  # 创建 Java File 对象

                # 调用 Java 方法
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

        # 启动子线程
        thread = threading.Thread(target=thread_function)
        thread.start()

        # 主线程等待子线程的完成，最多等待 1 秒
        coverage: float = 0.0
        if not event.wait(1.3):  # 超过1秒，则返回上次的覆盖率
            self.logger.info("Coverage calculation took too long, returning previous coverage.")
            with self.__coverage_lock:
                coverage = self.__last_coverage
        else:
            # 子线程完成，返回新计算的覆盖率
            self.logger.info(f"New coverage: {self.__last_coverage:.5f}%")
            with self.__coverage_lock:
                coverage = self.__last_coverage

        self._save_to_file(f"{coverage:.5f}%")
        return coverage

    def __del__(self):
        if jpype.isJVMStarted():
            self.logger.info("Shutting down JVM...")
            jpype.shutdownJVM()
            self.logger.info("JVM shut down successfully.")

