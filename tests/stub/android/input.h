// android/input.h — stub de hospedeiro: só o mínimo que platform/InputState.cpp
// usa (a suíte de UI injeta gestos pelos hooks de teste, não por AInputEvent).
#pragma once
#include <cstdint>
#include <cstddef>
using std::size_t;   // InputState.cpp usa size_t global
struct AInputEvent;
enum { AINPUT_EVENT_TYPE_MOTION = 1 };
enum {
    AMOTION_EVENT_ACTION_DOWN = 0,
    AMOTION_EVENT_ACTION_UP = 1,
    AMOTION_EVENT_ACTION_MOVE = 2,
    AMOTION_EVENT_ACTION_POINTER_DOWN = 5,
    AMOTION_EVENT_ACTION_POINTER_UP = 6,
    AMOTION_EVENT_ACTION_CANCEL = 3,
};
enum {
    AMOTION_EVENT_ACTION_MASK = 0xff,
    AMOTION_EVENT_ACTION_POINTER_INDEX_MASK = 0xff00,
    AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT = 8,
};
inline int32_t AInputEvent_getType(const AInputEvent*) { return 0; }
inline int32_t AMotionEvent_getAction(const AInputEvent*) { return 0; }
inline size_t AMotionEvent_getPointerCount(const AInputEvent*) { return 0; }
inline int32_t AMotionEvent_getPointerId(const AInputEvent*, size_t) { return 0; }
inline float AMotionEvent_getX(const AInputEvent*, size_t) { return 0.0f; }
inline float AMotionEvent_getY(const AInputEvent*, size_t) { return 0.0f; }
