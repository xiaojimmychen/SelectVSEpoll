//
// Created by hp on 10/25/18.
//

#include "ThrowException.h"
#include <string.h>

jint ThrowException::ClassNotFound(JNIEnv *env, char *className) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    jclass jclassClassNotFound = env->FindClass("java/lang/ClassNotFoundException");
    return env->ThrowNew(jclassClassNotFound, className);
}

jint ThrowException::NoSuchMethod(JNIEnv *env, char *methodName) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    jclass jclassNotSuchMethod = env->FindClass("java/lang/NoSuchMethodException");
    return env->ThrowNew(jclassNotSuchMethod, methodName);
}

jint ThrowException::NoSuchField(JNIEnv *env, char *fieldName) {
    env->ExceptionDescribe();
    env->ExceptionClear();
    jclass jclassNoSuchField = env->FindClass("java/lang/NoSuchFieldException");
    return env->ThrowNew(jclassNoSuchField, fieldName);
}