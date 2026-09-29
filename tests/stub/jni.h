// tests/stub/jni.h — stub de hospedeiro: assinaturas mínimas de JNI para o
// CHECK DE SINTAXE de platform/SafBridge.cpp e SafIoJni.cpp no CI (o NDK
// tem o jni.h real; estes TU não são linkados no hospedeiro).
#pragma once
#include <cstdint>

typedef int32_t jint;
typedef int64_t jlong;
typedef uint8_t jboolean;
typedef int8_t jbyte;
typedef uint16_t jchar;
typedef int16_t jshort;
typedef float jfloat;
typedef double jdouble;
typedef jint jsize;
typedef void* jmethodID;

#define JNI_OK 0
#define JNI_ERR (-1)
#define JNI_VERSION_1_6 0x00010006
#define JNI_TRUE 1
#define JNI_FALSE 0
#define JNIEXPORT
#define JNICALL

struct _jobject;
typedef _jobject* jobject;
typedef _jobject* jclass;
typedef _jobject* jstring;
typedef _jobject* jbyteArray;
typedef _jobject* jobjectArray;

typedef struct {
    const char* name;
    const char* signature;
    void* fnPtr;
} JNINativeMethod;

struct JNIEnv {
    jclass GetObjectClass(jobject) { return nullptr; }
    jclass FindClass(const char*) { return nullptr; }
    jint RegisterNatives(jclass, const JNINativeMethod*, jint) { return JNI_OK; }
    jmethodID GetMethodID(jclass, const char*, const char*) { return nullptr; }
    jmethodID GetStaticMethodID(jclass, const char*, const char*) { return nullptr; }
    jobject CallObjectMethod(jobject, jmethodID, ...) { return nullptr; }
    jint CallIntMethod(jobject, jmethodID, ...) { return 0; }
    jobject CallStaticObjectMethod(jclass, jmethodID, ...) { return nullptr; }
    jboolean CallStaticBooleanMethod(jclass, jmethodID, ...) { return JNI_FALSE; }
    void CallVoidMethod(jobject, jmethodID, ...) {}
    jobject NewGlobalRef(jobject o) { return o; }
    void DeleteGlobalRef(jobject) {}
    void DeleteLocalRef(jobject) {}
    jstring NewStringUTF(const char*) { return nullptr; }
    const char* GetStringUTFChars(jstring, jboolean*) { return nullptr; }
    void ReleaseStringUTFChars(jstring, const char*) {}
    jboolean ExceptionCheck() { return JNI_FALSE; }
    void ExceptionClear() {}
    jsize GetArrayLength(jbyteArray) { return 0; }
    jbyteArray NewByteArray(jsize) { return nullptr; }
    void GetByteArrayRegion(jbyteArray, jsize, jsize, jbyte*) {}
    void SetByteArrayRegion(jbyteArray, jsize, jsize, const jbyte*) {}
    jobject GetObjectArrayElement(jobjectArray, jsize) { return nullptr; }
};

struct JavaVM {
    jint GetEnv(void**, jint) { return JNI_OK; }
};
