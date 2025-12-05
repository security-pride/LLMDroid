# LLMDroid-Fastbot

## Environment Setup

### Part 1

If you have already prepared the following components, you can skip to Part 2.

> - Java  
> - Gradle 7.6.2  
> - Android SDK  
>   - adb  
>   - build-tools  
>   - "cmake;3.18.1"  
>   - "ndk;25.2.9519653"  

1. First, install sdkman, an SDK management tool.

```shell
curl -s "https://get.sdkman.io" | bash

# Open a new terminal and run:
sdk help
```

2. Use sdkman to install Java 17 and Gradle.

```shell
# List available versions
sdk list java

# Install Java
sdk install java 17.0.9-oracle

# Verify installation
java --version

# Install Gradle
sdk install gradle 7.6.2
gradle wrapper
```

3. Install sdkmanager (optional)

   - You can use the official sdkmanager for installation. Refer to: [Install sdkmanager](https://developer.android.com/tools/sdkmanager)  
     - Download cmdline-tools and copy it to the Android SDK directory.  
     - Create a new folder named `latest` under cmdline-tools.  
     - Move all files from cmdline-tools into `latest`.  
     - The sdkmanager is located at `cmdline-tools/latest/bin/`.  

   - Set environment variables. Refer to: [Official Documentation](https://developer.android.com/tools/variables)  

     ```shell
     export ANDROID_HOME=/path/to/your/Android/sdk
     export PATH=$PATH:$ANDROID_HOME/tools:$ANDROID_HOME/tools/bin:$ANDROID_HOME/platform-tools
     ```

4. Use sdkmanager to install cmake and ndk (or install via Android Studio).

```shell
./sdkmanager "cmake;3.18.1"
./sdkmanager "ndk;25.2.9519653"
```

5. Run gradle wrapper (Part 1 is now complete).

```shell
./gradle wrapper
```

### Part 2

This section prepares additional dependencies required by LLMDroid, mainly the `curl` and `nlohmann_json` libraries needed by the OpenAI library.

6. Install vcpkg.

```shell
git clone https://github.com/microsoft/vcpkg.git

cd vcpkg && ./bootstrap-vcpkg.sh
```

7. Install build-essential.

```shell
sudo apt install build-essential
```

8. Use vcpkg to install nlohmann_json and curl.

Set `--triplet` to the platform you want to build for, such as `arm64-android`, `x64-android`, or `x86-android`.

```shell
cd vcpkg
./vcpkg install nlohmann-json --triplet <target_platform>
./vcpkg install curl --triplet <target_platform>
```

After installation, `nlohmann_jsonConfig.cmake` will be available at
`vcpkg/installed/<target_platform>/share/nlohmann_json`.

9. Set the relevant environment variables (recommended to add to `~/.bashrc`).

```shell
export NDK_ROOT=$ANDROID_HOME/ndk/25.2.9519653
export ANDROID_NDK_HOME=$ANDROID_HOME/ndk/25.2.9519653

export PATH="/usr/lib/android-sdk/cmake/3.18.1/bin:$PATH"
export PATH="$ANDROID_HOME/platform-tools/:$PATH"
```

## Build

### x86_64 Build

The scripts in this repo are written for `arm64-v8a` by default.
If you want to build for `x86_64` (for example, for Android emulators), update the files below before building.

> [!NOTE]
> The original authors assume `vcpkg` is installed in the user’s `home` directory, but yours might not be.
> In the steps below, `VCPKG_ROOT` means your actual `vcpkg` install path.

1. [native/CMakeLists.txt](native/CMakeLists.txt)

Change the target platform in all paths from `arm64-android` to `x64-android`.

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
2. [monkey/build.gradle](monkey/build.gradle)

Switch ABI to `x86_64` and raise the minimum SDK to 26 (lowest version tested to be compilable).

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

3. [build_native.sh](build_native.sh)

Change target ABI and specify the Android API level.

```diff
-cmake -DCMAKE_TOOLCHAIN_FILE=$NDK_ROOT/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DCMAKE_BUILD_TYPE=Release
+cmake -DCMAKE_TOOLCHAIN_FILE=$NDK_ROOT/build/cmake/android.toolchain.cmake -DANDROID_ABI=x86_64  -DCMAKE_BUILD_TYPE=Release -DANDROID_PLATFORM=android-26
```

### Make jar

```shell
# First, build the jar
cd LLMDroid-Fastbot

# non-parallel mode
./gradlew clean makeJar

# parallel mode
./gradlew clean makeJar --parallel

# Use the dx tool to convert the jar into a format compatible with Android
# Replace the path with the actual location of dx
# Recommended build-tools version > 27
$ANDROID_HOME/build-tools/30.0.1/dx --dex --output=monkeyq.jar monkey/build/libs/monkey.jar
```

### Compile .so

Check the `native/CMakeLists.txt` file.

> set(nlohmann_json_DIR "$ENV{HOME}/vcpkg/installed/arm64-android/share/nlohmann_json")

You may need to modify the path above to ensure it points to the actual installation location of nlohmann_json.

Use the following command to compile the .so file.

> [!NOTE]
> If you're building the library for x86_64, you can likely skip this step as long as the previous Gradle command already produced the library file.

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