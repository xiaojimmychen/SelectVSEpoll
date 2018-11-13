//
// Created by hp on 10/25/18.
//

#include "NativeLog.h"
#include "ThrowException.h"

NativeLog::NativeLog(JNIEnv *env) {
    jclass jclass1 = env->FindClass("android/util/Log");
    if (jclass1 == nullptr && env->ExceptionCheck()) {
        ThrowException::ClassNotFound(env, "android/util/Log");
    }
    jclassLog = (jclass)env->NewGlobalRef(jclass1);

    jmethodIDLogV = env->GetStaticMethodID(jclassLog, "v", "(Ljava/lang/String;Ljava/lang/String;)I");
    if (jmethodIDLogV == nullptr && env->ExceptionCheck()) {
        ThrowException::NoSuchMethod(env, "v");
    }

    jmethodIDLogI = env->GetStaticMethodID(jclassLog, "i", "(Ljava/lang/String;Ljava/lang/String;)I");
    if (jmethodIDLogI == nullptr && env->ExceptionCheck()) {
        ThrowException::NoSuchMethod(env, "i");
    }

    jmethodIDLogD = env->GetStaticMethodID(jclassLog, "d", "(Ljava/lang/String;Ljava/lang/String;)I");
    if (jmethodIDLogD == nullptr && env->ExceptionCheck()) {
        ThrowException::NoSuchMethod(env, "d");
    }

    jmethodIDLogE = env->GetStaticMethodID(jclassLog, "e", "(Ljava/lang/String;Ljava/lang/String;)I");
    if (jmethodIDLogE == nullptr && env->ExceptionCheck()) {
        ThrowException::NoSuchMethod(env, "e");
    }

    jmethodIDLogW = env->GetStaticMethodID(jclassLog, "w", "(Ljava/lang/String;Ljava/lang/String;)I");
    if (jmethodIDLogW == nullptr && env->ExceptionCheck()) {
        ThrowException::NoSuchMethod(env, "w");
    }
}

int NativeLog::d(JNIEnv *env, const char *LOG_TAG, const char *Msg) {
    jstring jstringLogTag = env->NewStringUTF(LOG_TAG);
    jstring jstringMsg = env->NewStringUTF(Msg);

    return env->CallStaticIntMethod(jclassLog, jmethodIDLogD, jstringLogTag, jstringMsg);
}

int NativeLog::v(JNIEnv *env, const char *LOG_TAG, const char *Msg) {
    jstring jstringLogTag = env->NewStringUTF(LOG_TAG);
    jstring jstringMsg = env->NewStringUTF(Msg);

    return env->CallStaticIntMethod(jclassLog, jmethodIDLogV, jstringLogTag, jstringMsg);
}

int NativeLog::i(JNIEnv *env, const char *LOG_TAG, const char *Msg) {
    jstring jstringLogTag = env->NewStringUTF(LOG_TAG);
    jstring jstringMsg = env->NewStringUTF(Msg);

    return env->CallStaticIntMethod(jclassLog, jmethodIDLogI, jstringLogTag, jstringMsg);
}

int NativeLog::w(JNIEnv *env, const char *LOG_TAG, const char *Msg) {
    jstring jstringLogTag = env->NewStringUTF(LOG_TAG);
    jstring jstringMsg = env->NewStringUTF(Msg);

    return env->CallStaticIntMethod(jclassLog, jmethodIDLogW, jstringLogTag, jstringMsg);
}

int NativeLog::e(JNIEnv *env, const char *LOG_TAG, const char *Msg) {
    jstring jstringLogTag = env->NewStringUTF(LOG_TAG);
    jstring jstringMsg = env->NewStringUTF(Msg);

    return env->CallStaticIntMethod(jclassLog, jmethodIDLogE, jstringLogTag, jstringMsg);
}