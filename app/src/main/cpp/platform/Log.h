#pragma once
// platform/Log.h — macros de log Android (diagnóstico F1).
// Tag única "GONI_VV" para filtrar no logcat.
#include <android/log.h>

// native_app_glue também define LOGI/LOGE — garantimos as nossas.
#ifdef LOGI
#undef LOGI
#endif
#ifdef LOGE
#undef LOGE
#endif
#ifdef LOGW
#undef LOGW
#endif

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,  "GONI_VV", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "GONI_VV", __VA_ARGS__))
// 0.12-Z2 F1: o WARNING do swap falhado (o device: EGL_CONTEXT_LOST)
#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN,  "GONI_VV", __VA_ARGS__))
