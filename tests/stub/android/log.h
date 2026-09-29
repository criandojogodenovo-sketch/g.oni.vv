// android/log.h — stub de hospedeiro para os testes de UI no CI.
#pragma once
#include <cstdio>
enum { ANDROID_LOG_INFO = 4, ANDROID_LOG_WARN = 5, ANDROID_LOG_ERROR = 6 };
inline int __android_log_print(int, const char*, const char* fmt, ...) {
    (void)fmt;
    return 0;
}
#define ALOGI(...) __android_log_print(ANDROID_LOG_INFO, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, __VA_ARGS__)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, __VA_ARGS__)
