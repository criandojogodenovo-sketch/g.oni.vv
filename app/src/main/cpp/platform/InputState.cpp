#include "platform/InputState.h"
#include <android/input.h>

namespace vv {

bool InputState::process(const AInputEvent* event) {
    if (AInputEvent_getType(event) != AINPUT_EVENT_TYPE_MOTION) {
        return false;
    }

    const int32_t fullAction = AMotionEvent_getAction(event);
    const int32_t action = fullAction & AMOTION_EVENT_ACTION_MASK;
    const size_t pointerCount = AMotionEvent_getPointerCount(event);
    const size_t actionIndex =
        (fullAction & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK)
        >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;

    switch (action) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_POINTER_DOWN:
            claim((i32)AMotionEvent_getPointerId(event, actionIndex),
                  AMotionEvent_getX(event, actionIndex),
                  AMotionEvent_getY(event, actionIndex));
            break;

        case AMOTION_EVENT_ACTION_MOVE:
            for (size_t i = 0; i < pointerCount; ++i) {
                const i32 s = slotOfId((i32)AMotionEvent_getPointerId(event, i));
                if (s >= 0) {
                    slots_[s].x = AMotionEvent_getX(event, i);
                    slots_[s].y = AMotionEvent_getY(event, i);
                }
            }
            break;

        case AMOTION_EVENT_ACTION_POINTER_UP:
            releaseId((i32)AMotionEvent_getPointerId(event, actionIndex));
            break;

        case AMOTION_EVENT_ACTION_UP:        // fim do gesto — solta todos
        case AMOTION_EVENT_ACTION_CANCEL:
            releaseAll();
            break;

        default:
            break;
    }
    return true;
}

void InputState::clearEdges() {
    for (u32 i = 0; i < kMaxPointerSlots; ++i) {
        pressed_[i] = false;
        released_[i] = false;
    }
}

i32 InputState::slotOfId(i32 id) const {
    for (u32 i = 0; i < kMaxPointerSlots; ++i) {
        if (slots_[i].down && slots_[i].id == id) {
            return (i32)i;
        }
    }
    return -1;
}

void InputState::claim(i32 id, f32 x, f32 y) {
    const i32 s = slotOfId(id);
    if (s >= 0) {   // já ativo — trata como move
        slots_[s].x = x;
        slots_[s].y = y;
        return;
    }
    for (u32 i = 0; i < kMaxPointerSlots; ++i) {
        if (!slots_[i].down) {
            slots_[i].down = true;
            slots_[i].id = id;
            slots_[i].x = x;
            slots_[i].y = y;
            pressed_[i] = true;
            return;
        }
    }
    // sem slot livre: ignora (8 toques simultâneos já é muito para a F1)
}

void InputState::releaseId(i32 id) {
    const i32 s = slotOfId(id);
    if (s < 0) {
        return;
    }
    slots_[s].down = false;
    slots_[s].id = -1;
    released_[s] = true;
}

void InputState::releaseAll() {
    for (u32 i = 0; i < kMaxPointerSlots; ++i) {
        if (slots_[i].down) {
            slots_[i].down = false;
            slots_[i].id = -1;
            released_[i] = true;
        }
    }
}

u32 InputState::activePointers() const {
    u32 n = 0;
    for (u32 i = 0; i < kMaxPointerSlots; ++i) {
        if (slots_[i].down) ++n;
    }
    return n;
}

} // namespace vv
