// tests/stub/android_native_app_glue.h — stub de hospedeiro: estruturas do
// glue do NDK suficientes para COMPILAR platform/main.cpp no CI (check de
// paridade de sintaxe — nunca liga; no device usa o glue real do NDK).
#pragma once
#include <stdint.h>
#include "android/native_window.h"
#include "android/input.h"

struct JavaVM;
struct _JNIEnv;
typedef _JNIEnv JNIEnv;
struct ANativeActivity;

struct android_app {
    ANativeActivity* activity;
    ANativeWindow* window;
    ARect contentRect;
    int destroyRequested;
    void (*onAppCmd)(android_app*, int32_t);
    int32_t (*onInputEvent)(android_app*, AInputEvent*);
};

struct ANativeActivity {
    JavaVM* vm;
    JNIEnv* env;
    void* clazz;                 // jobject da Activity (JNI)
    const char* internalDataPath;
    const char* externalDataPath;
};

struct android_poll_source {
    int32_t id;
    android_app* app;
    void (*process)(android_app*, android_poll_source*);
};

enum {
    APP_CMD_INPUT_CHANGED,
    APP_CMD_INIT_WINDOW,
    APP_CMD_TERM_WINDOW,
    APP_CMD_WINDOW_RESIZED,
    APP_CMD_WINDOW_REDRAW_NEEDED,
    APP_CMD_CONTENT_RECT_CHANGED,
    APP_CMD_GAINED_FOCUS,
    APP_CMD_LOST_FOCUS,
    APP_CMD_CONFIG_CHANGED,
    APP_CMD_LOW_MEMORY,
    APP_CMD_START,
    APP_CMD_RESUME,
    APP_CMD_SAVE_STATE,
    APP_CMD_PAUSE,
    APP_CMD_STOP,
    APP_CMD_DESTROY,
};

typedef struct ALooper ALooper;
inline int ALooper_pollAll(int, int*, int*, void**) { return -1; }
