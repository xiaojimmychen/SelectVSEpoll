//
// Created by hp on 10/25/18.
//

#ifndef TESTAPP_LOG_H
#define TESTAPP_LOG_H

#include <jni.h>
#include <stdio.h>

class NativeLog {
private:
    jmethodID jmethodIDLogV;
    jmethodID jmethodIDLogD;
    jmethodID jmethodIDLogI;
    jmethodID jmethodIDLogW;
    jmethodID jmethodIDLogE;
    jclass jclassLog;

public:
    NativeLog() {}
    NativeLog(JNIEnv *env);
    int v(JNIEnv *env, const char *LOG_TAG, const char *Msg);
    int d(JNIEnv *env, const char *LOG_TAG, const char *Msg);
    int i(JNIEnv *env, const char *LOG_TAG, const char *Msg);
    int w(JNIEnv *env, const char *LOG_TAG, const char *Msg);
    int e(JNIEnv *env, const char *LOG_TAG, const char *Msg);
};

#endif //TESTAPP_LOG_H
