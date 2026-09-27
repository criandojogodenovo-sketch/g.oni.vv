#pragma once
// platform/InputState.h — estado de entrada multi-touch (down/move/up) da AInputQueue.
// Cada toque é mapeado por pointer id → slot fixo (até 8 simultâneos).
// Edges (pressed/released) valem por um frame e são limpas em clearEdges()
// ao fim do frame, depois que a UI as consome.
#include "core/Types.h"

struct AInputEvent;

namespace vv {

constexpr u32 kMaxPointerSlots = 8;

struct PointerSlot {
    bool down = false;
    f32  x = 0.0f;
    f32  y = 0.0f;
    i32  id = -1;   // pointer id do Android (-1 = slot livre)
};

class InputState {
public:
    // callback do glue (onInputEvent): processa um evento de toque; true = consumido
    bool process(const AInputEvent* event);

    // limpa as edges do frame (chamar ao fim do frame, após a UI consumir)
    void clearEdges();

    // consulta
    bool down(u32 slot) const     { return slots_[slot].down; }
    bool pressed(u32 slot) const  { return pressed_[slot]; }
    bool released(u32 slot) const { return released_[slot]; }
    void pos(u32 slot, f32& x, f32& y) const { x = slots_[slot].x; y = slots_[slot].y; }
    u32  activePointers() const;

private:
    i32  slotOfId(i32 id) const;
    void claim(i32 id, f32 x, f32 y);
    void releaseId(i32 id);
    void releaseAll();

    PointerSlot slots_[kMaxPointerSlots];
    bool pressed_[kMaxPointerSlots]  = {};
    bool released_[kMaxPointerSlots] = {};
};

} // namespace vv
