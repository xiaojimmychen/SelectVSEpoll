//
// Created by hp on 10/25/18.
//

#ifndef TESTAPP_THROWEXCEPTION_H
#define TESTAPP_THROWEXCEPTION_H

#include <jni.h>


class ThrowException {
public:
    static jint ClassNotFound(JNIEnv *env, char *className);
    static jint NoSuchMethod(JNIEnv *env, char *methodName);
    static jint NoSuchField(JNIEnv *env, char *fieldName);
};


#endif //TESTAPP_THROWEXCEPTION_H
