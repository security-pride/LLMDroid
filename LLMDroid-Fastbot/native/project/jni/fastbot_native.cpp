/*
 * This code is licensed under the Fastbot license. You may obtain a copy of this license in the LICENSE.txt file in the root directory of this source tree.
 */
/**
 * @authors Jianqiang Guo, Yuhui Su
 */
#include "fastbot_native.h"
#include "Model.h"
#include "ModelReusableAgent.h"
#include "utils.hpp"

#ifdef __cplusplus
extern "C" {
#endif

static fastbotx::ModelPtr _fastbot_model = nullptr;


void callJavaLoggerStaticMethod(JNIEnv* env, const char* str) {
    // 1. Get the corresponding Java class object
    jclass loggerClass = env->FindClass("com/android/commands/monkey/utils/Logger");
    //MLOG("%s", loggerClass);
    if (loggerClass == nullptr) {
        // Handling the case where the class is not found
        MLOG("callJavaLoggerStaticMethod");
        return;
    }


    // 2. Get the corresponding static method ID
    jmethodID printlnMethod = env->GetStaticMethodID(loggerClass, "println", "(Ljava/lang/Object;)V");
    if (printlnMethod == nullptr) {
        // Handle the situation when the method is not found
        return;
    }

    // 3. Call this static method
    jstring message = env->NewStringUTF(str);
    env->CallStaticVoidMethod(loggerClass, printlnMethod, message);

    // Release local reference
    env->DeleteLocalRef(message);
}

void initJavaLogger(JNIEnv* env)
{
    fastbotx::jnienv = env;
    // 1. Get the corresponding Java class object
    fastbotx::loggerClass = fastbotx::jnienv->FindClass("com/android/commands/monkey/utils/Logger");
    if (fastbotx::loggerClass == nullptr) {
        // Handling the case where the class is not found
        MLOG("can't find logger class");
        return;
    }
    // 2. Get the corresponding static method ID
    fastbotx::printlnMethod = fastbotx::jnienv->GetStaticMethodID(fastbotx::loggerClass, "println", "(Ljava/lang/Object;)V");
    if (fastbotx::printlnMethod == nullptr) {
        // Handle the situation when the method is not found
        return;
    }
}

void initCodeCoverage()
{
    // 1. Get the corresponding Java class object
    fastbotx::codeCoverageClass = fastbotx::jnienv->FindClass("com/android/commands/monkey/utils/CodeCoverage");
    if (fastbotx::codeCoverageClass == nullptr) {
        // Handling the case where the class is not found
        fastbotx::callJavaLogger(MAIN_THREAD, "can't find CodeCoverage class");
        return;
    }
    // 2. Get the corresponding static method ID
    fastbotx::getCoverageMethod = fastbotx::jnienv->GetStaticMethodID(fastbotx::codeCoverageClass, "getCoverage", "()D");
    if (fastbotx::getCoverageMethod == nullptr) {
        // Handle the situation when the method is not found
        fastbotx::callJavaLogger(MAIN_THREAD, "can't find CodeCoverage class's getCodeCoverage method");
        return;
    }
}

JNIEXPORT jint JNI_OnLoad(JavaVM* vm, void *reserved)
{
    JNIEnv* env = NULL; //It is implemented in jni env during registration, so it must be obtained first
    fastbotx::jvm = vm;

    // Get jni env from java vm, generally use version 1.4
    if(vm->GetEnv((void**)&env, JNI_VERSION_1_4) != JNI_OK) 
        return -1;

    // It is important here, the version must be returned, otherwise the load will fail.
    return JNI_VERSION_1_4; 
}

//getAction
jstring JNICALL Java_com_bytedance_fastbot_AiClient_b0bhkadf(JNIEnv *env, jobject, jstring activity,
                                                             jstring xmlDescOfGuiTree) {
    if (nullptr == _fastbot_model) {
        _fastbot_model = fastbotx::Model::create();
    }
    const char *xmlDescriptionCString = env->GetStringUTFChars(xmlDescOfGuiTree, nullptr);
    const char *activityCString = env->GetStringUTFChars(activity, nullptr);
    std::string xmlString = std::string(xmlDescriptionCString);
    std::string activityString = std::string(activityCString);
    std::string operationString = _fastbot_model->getOperate(xmlString, activityString);
    LOGD("do action opt is : %s", operationString.c_str());
    env->ReleaseStringUTFChars(xmlDescOfGuiTree, xmlDescriptionCString);
    env->ReleaseStringUTFChars(activity, activityCString);
    //callJavaLoggerStaticMethod(env);
    return env->NewStringUTF(operationString.c_str());
}

// for single device, just addAgent as empty device //InitAgent
void JNICALL Java_com_bytedance_fastbot_AiClient_fgdsaf5d(JNIEnv *env, jobject, jint agentType,
                                                          jstring packageName, jint deviceType, jboolean useCodeCoverage) {
    initJavaLogger(env);
    fastbotx::callJavaLogger(MAIN_THREAD, "*************************************Java logger is ready");
    initCodeCoverage();

    if (nullptr == _fastbot_model) {
        _fastbot_model = fastbotx::Model::create();
    }
    auto algorithmType = (fastbotx::AlgorithmType) agentType;
    auto agentPointer = _fastbot_model->addAgent("", algorithmType, useCodeCoverage,
                                                 (fastbotx::DeviceType) deviceType);
    const char *packageNameCString = "";
    if (env)
        packageNameCString = env->GetStringUTFChars(packageName, nullptr);
    _fastbot_model->setPackageName(std::string(packageNameCString));

    BLOG("init agent with type %d, %s,  %d", agentType, packageNameCString, deviceType);
    if (algorithmType == fastbotx::AlgorithmType::Reuse) {
        auto reuseAgentPtr = std::dynamic_pointer_cast<fastbotx::ModelReusableAgent>(agentPointer);
        reuseAgentPtr->loadReuseModel(std::string(packageNameCString));
        if (env)
            env->ReleaseStringUTFChars(packageName, packageNameCString);
    }

    //initJavaLogger(env);
    
}

// load ResMapping
void JNICALL
Java_com_bytedance_fastbot_AiClient_jdasdbil(JNIEnv *env, jobject, jstring resMappingFilepath) {
    if (nullptr == _fastbot_model) {
        _fastbot_model = fastbotx::Model::create();
    }
    const char *resourceMappingPath = env->GetStringUTFChars(resMappingFilepath, nullptr);
    auto preference = _fastbot_model->getPreference();
    if (preference) {
        preference->loadMixResMapping(std::string(resourceMappingPath));
    }
    env->ReleaseStringUTFChars(resMappingFilepath, resourceMappingPath);
}

// to check if a point is in black widget area
jboolean JNICALL
Java_com_bytedance_fastbot_AiClient_nkksdhdk(JNIEnv *env, jobject, jstring activity, jfloat pointX,
                                             jfloat pointY) {
    bool isShield = false;
    if (nullptr == _fastbot_model) {
        BLOGE("%s", "model null, check point failed!");
        return isShield;
    }
    const char *activityStr = env->GetStringUTFChars(activity, nullptr);
    auto preference = _fastbot_model->getPreference();
    if (preference) {
        isShield = preference->checkPointIsInBlackRects(std::string(activityStr),
                                                        static_cast<int>(pointX),
                                                        static_cast<int>(pointY));
    }
    env->ReleaseStringUTFChars(activity, activityStr);
    return isShield;
}

jstring JNICALL Java_com_bytedance_fastbot_AiClient_getNativeVersion(JNIEnv *env, jclass clazz) {
    return env->NewStringUTF(FASTBOT_VERSION);
}

#ifdef __cplusplus
}
#endif