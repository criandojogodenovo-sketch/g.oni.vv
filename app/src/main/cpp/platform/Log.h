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

#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,  "GONI_VV", __VA_ARGS__))
#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, "GONI_VV", __VA_ARGS__))
