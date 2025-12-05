# LLMDroid-Fastbot

## Environment Setup

### Part 1

如果已经准备好下面的部分，可以跳转至Part2

> - java
> - gradle 7.6.2
> - Android SDK
>   - adb
>   - build-tools
>   - "cmake;3.18.1"
>   - "ndk;25.2.9519653"

1. 首先安装sdkman，一个sdk管理工具

```shell
curl -s "https://get.sdkman.io" | bash

# 新开一个终端,运行:
sdk help
```

2. 使用sdkman安装java 17和gradle

```shell
# 查看可安装的版本
sdk list java

#安装java
sdk install java 17.0.9-oracle

# 检查是否安装成功
java --version

# 安装gradle
sdk install gradle 7.6.2
gradle wrapper
```

3. 安装sdkmanager（可选）

   - 可以使用官方提供的sdkmanager进行安装，具体可以参考：[安装sdkmanager](https://developer.android.com/tools/sdkmanager)

     - 下载cmdline-tools，并将其拷贝到Android SDK目录下
     - 在cmdline-tools下创建新文件夹latest
     - 将cmdline-tools下的所有文件移动至latest中
     - sdkmanager位于`cmdline-tools/latest/bin/`下

   - 写入环境变量。参考：[官方文档](https://developer.android.com/tools/variables) 

     ```shell
     export ANDROID_HOME=/path/to/your/Android/sdk
     export PATH=$PATH:$ANDROID_HOME/tools:$ANDROID_HOME/tools/bin:$ANDROID_HOME/platform-tools
     ```


4. 使用sdkmanager安装cmake和 ndk（也可以使用Android Studio安装）

```shell
./sdkmanager "cmake;3.18.1"
./sdkmanager "ndk;25.2.9519653"
```

5. 运行gradle wrapper (至此已完成第一步)

```shell
./gradle wrapper
```

### Part 2

这一部分主要是准备LLMDroid需要的额外依赖，主要是openai库所依赖的`curl`和`nlohmann_json`

6. 安装vcpkg

```shell
git clone https://github.com/microsoft/vcpkg.git

cd vcpkg && ./bootstrap-vcpkg.sh
```

7. 安装build-essential

```shell
sudo apt install build-essential
```

8. 使用vcpkg安装nlohmann_json和curl

使用`--triplet`参数指定目标平台，例如 `arm64-android`、`x64-android` 或 `x86-android`。

```shell
 cd vcpkg
 ./vcpkg install nlohmann-json --triplet <target_platform>
 ./vcpkg install curl --triplet <target_platform>
```

安装完成后，`nlohmann_jsonConfig.cmake` 将位于 `vcpkg/installed/<target_platform>/share/nlohmann_json`。

9. 设置相关环境变量（推荐在`~/.bashrc`中写入）

```shell
export NDK_ROOT=$ANDROID_HOME/ndk/25.2.9519653
export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/25.2.9519653

export PATH="/usr/lib/android-sdk/cmake/3.18.1/bin:$PATH"
export PATH="$ANDROID_HOME/platform-tools/:$PATH"
```



## Build

### x86_64 构建

仓库中默认的本地编译脚本以 `arm64-v8a` 为目标。如果你需要为 `x86_64` 构建本地库（例如在某些 Android 模拟器上运行），请在编译前按下面步骤修改。

> [!NOTE]
> 仓库的原作者假定 `vcpkg` 安装在用户主目录下。如果你的 `vcpkg` 安装在其他位置，请将 `VCPKG_ROOT` 设置为你的安装路径，并相应调整下文示例。

1. 编辑 [native/CMakeLists.txt](native/CMakeLists.txt)，将 `nlohmann_json_DIR` 指向 `x64-android` 的 `vcpkg` 布局，并使用 `x86_64` 对应的库路径。

```diff
@@ -3,7 +3,7 @@
 cmake_minimum_required(VERSION 3.10)
 project(fastbot_native)
 
-set(nlohmann_json_DIR "${HOME}/vcpkg/installed/arm64-android/share/nlohmann_json")
+set(nlohmann_json_DIR "${VCPKG_ROOT}/installed/x64-android/share/nlohmann_json")
 # $ENV{HOME}/vcpkg/installed/arm64-android/share/nlohmann_json
 # $ENV{HOME}/vcpkg/buildtrees/nlohmann-json/x64-linux-rel
 #set(CURL_DIR "$ENV{HOME}/vcpkg/buildtrees/curl/arm64-android-rel/generated")
@@ -21,19 +21,19 @@ find_package(nlohmann_json CONFIG REQUIRED)
 
 add_library(lib_curl STATIC IMPORTED)
 set_target_properties(lib_curl PROPERTIES IMPORTED_LOCATION
-        ${CMAKE_CURRENT_SOURCE_DIR}/curl/lib/arm64-v8a/libcurl.a)
+        ${VCPKG_ROOT}/installed/x64-android/lib/libcurl.a)
 
 add_library(lib_crypto STATIC IMPORTED)
 set_target_properties(lib_crypto PROPERTIES IMPORTED_LOCATION
-  ${CMAKE_CURRENT_SOURCE_DIR}/curl/lib/arm64-v8a/libcrypto.a)
+  ${VCPKG_ROOT}/installed/x64-android/lib/libcrypto.a)
 
 add_library(lib_ssl STATIC IMPORTED)
 set_target_properties(lib_ssl PROPERTIES IMPORTED_LOCATION
-  ${CMAKE_CURRENT_SOURCE_DIR}/curl/lib/arm64-v8a/libssl.a)
+  ${VCPKG_ROOT}/installed/x64-android/lib/libssl.a)
 
 add_library(lib_z STATIC IMPORTED)
 set_target_properties(lib_z PROPERTIES IMPORTED_LOCATION
-  ${CMAKE_CURRENT_SOURCE_DIR}/curl/lib/arm64-v8a/libz.a)
+  ${VCPKG_ROOT}/installed/x64-android/lib/libz.a)
 
 set( LIBPATH mac)
 message(STATUS  ${CMAKE_SYSTEM_NAME})
```

2. 修改 [monkey/build.gradle](monkey/build.gradle)，将 `abiFilters` 切换为 `x86_64`，并将 `minSdkVersion` 调整为兼容目标平台的值（测试通过的最小值为 26）。

```diff
@@ -23,7 +23,7 @@ android {
     compileSdkVersion 32
 
     defaultConfig {
-        minSdkVersion 22
+        minSdkVersion 26
         targetSdkVersion 32
         versionCode 1
         versionName "1.0"
@@ -31,7 +31,7 @@ android {
         multiDexEnabled true
         ndk {
             //abiFilters 'armeabi-v7a', 'arm64-v8a', 'x86', 'x86_64'
-            abiFilters 'arm64-v8a'
+            abiFilters 'x86_64'
         }
     }
```

3. 在 [build_native.sh](build_native.sh) 中将 `ANDROID_ABI` 修改为 `x86_64`，并设置适当的 `ANDROID_PLATFORM`（例如 `android-26`）。

```diff
-cmake -DCMAKE_TOOLCHAIN_FILE=$NDK_ROOT/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DCMAKE_BUILD_TYPE=Release
+cmake -DCMAKE_TOOLCHAIN_FILE=$NDK_ROOT/build/cmake/android.toolchain.cmake -DANDROID_ABI=x86_64  -DCMAKE_BUILD_TYPE=Release -DANDROID_PLATFORM=android-26
```

在完成上述修改后，按常规方式构建即可。

### Make jar

```shell
# 首先编译出jar
cd LLMDroid-Fastbot

# 非并行模式
./gradlew clean makeJar

# 并行模式
./gradlew clean makeJar --parallel

# 需要使用dx工具将jar转变为能够在Android运行的格式
# 路径要替换成dx所在目录
# 推荐build-tools版本 > 27
$ANDROID_HOME/build-tools/30.0.1/dx --dex --output=monkeyq.jar monkey/build/libs/monkey.jar
```

### Compile .so

检查`native/CMakeLists.txt`文件

> set(nlohmann_json_DIR "$ENV{HOME}/vcpkg/installed/arm64-android/share/nlohmann_json")

可能需要修改上述路径确保其指向nlohmann_json的实际安装位置。

使用下面的命令编译.so文件

> [!NOTE]
> 如果你的目标平台是x86_64，在执行前面的gradle命令后如果在相应目录下找到了编译好的二进制文件，这一步可以跳过。

```shell
sh build_native.sh
```

## Usage

If you are simply running the LLMDroid-Fastbot tool, the steps are quite straightforward.

- Push artifacts into your device: 
    - The `faruzan` directory can be renamed as desired.
    - The `config.json` file must be placed in `/sdcard` and cannot be renamed.


```shell
adb shell mkdir /sdcard/faruzan
adb push config.json /sdcard/config.json
adb push monkeyq.jar /sdcard/faruzan/monkeyq.jar
adb push fastbot-thirdpart.jar /sdcard/faruzan/fastbot-thirdpart.jar
adb push monkey/libs/* /sdcard/faruzan
adb push framework.jar /sdcard/faruzan/framework.jar
adb push libs/* /data/local/tmp/
```



- Run LLMDroid-Fastbot with following command:

```shell
adb shell CLASSPATH=/sdcard/faruzan/monkeyq.jar:/sdcard/faruzan/framework.jar:/sdcard/faruzan/fastbot-thirdpart.jar:/sdcard/faruzan/org.jacoco.core-0.8.8.jar:/sdcard/faruzan/asm-9.2.jar:/sdcard/faruzan/asm-analysis-9.2.jar:/sdcard/faruzan/asm-commons-9.2.jar:/sdcard/faruzan/asm-tree-9.2.jar exec app_process /system/bin com.android.commands.monkey.Monkey -p $app_package_name --agent reuseq --use-code-coverage androlog --running-minutes 5 --throttle 3000 --output-directory $mobile_output_dir -v -v --bugreport
```



- Special Parameters: (Other parameters are consistent with Fastbot2, see [here](https://github.com/bytedance/Fastbot_Android) for more details)
  - `--use-code-coverage`：指定实时代码覆盖率监测方式，可以设置为下面的值。
      - `androlog`：使用androlog的方法。针对闭源app，需要使用androlog工具对apk进行插桩。并需要在`config.json`中设置`Tag`和`TotalMethod`
      - `jacoco`：使用jacoco的方法。针对开源app，需要对源码做出修改，并重新编译apk。
      - `time`：关闭实时代码覆盖率监测。用于调试，每隔指定时间就进入LLM Guidance模式。



