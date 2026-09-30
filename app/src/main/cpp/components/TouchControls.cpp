// components/TouchControls.cpp — input dos controlos de toque (GL-free).
//
// 0.7.3: o layout é DERIVADO dos campos editáveis do componente (pos em
// frações da área útil, tamanho em escala do raio) — o default reproduz o
// layout fixo da 0.6.x no ecrã de referência 1600×720. A sensibilidade
// multiplica o eixo (clamp no círculo unitário). O botão jump mantém o
// ancorário à direita (o arrastável é o joystick). Tolerância de 1.3× no
// raio/rect para dedos imprecisos (touchscreen do C33).
#include "components/TouchControls.h"
#include <cmath>
#include <cstring>

namespace vv {

namespace {
// espelho do kStatusH do UiContext (40px) — aqui NÃO incluímos ui/ para
// manter o TouchControls GL-free (testes do CI Linux)
constexpr f32 kStatusHMirror = 40.0f;
}

TouchControls::Layout TouchControls::layout(f32 sw, f32 sh) {
    Layout l{};
    const f32 cy = sh - kStatusHMirror - 150.0f;   // centro vertical dos controlos
    l.joyCX = 150.0f;
    l.joyCY = cy;
    l.joyR  = 75.0f;
    l.btnW  = 110.0f;
    l.btnH  = 110.0f;
    l.btnX  = sw - 150.0f - l.btnW * 0.5f;
    l.btnY  = cy - l.btnH * 0.5f;
    return l;
}

TouchControls::Layout TouchControls::layoutFor(f32 aw, f32 ah) const {
    // frações da ÁREA ÚTIL (resolução-independente); raio escala com o
    // campo `size` (base 75px); o botão jump mantém o ancorário da direita
    Layout l{};
    l.joyCX = relX * aw;
    l.joyCY = relY * ah;
    l.joyR  = 75.0f * (size > 0.05f ? size : 0.05f);
    l.btnW  = 110.0f;
    l.btnH  = 110.0f;
    l.btnX  = aw - 150.0f - l.btnW * 0.5f;
    l.btnY  = l.joyCY - l.btnH * 0.5f;   // alinhado ao joystick
    return l;
}

bool TouchControls::touchBegin(u32 slot, f32 x, f32 y, f32 sw, f32 sh) {
    const Layout l = layoutFor(sw, sh);   // 0.7.3: layout EDITÁVEL
    baseX_ = l.joyCX;
    baseY_ = l.joyCY;
    radius_ = l.joyR;

    // joystick (ainda livre?) — tolerância 1.3× no raio
    const f32 dx = x - l.joyCX;
    const f32 dy = y - l.joyCY;
    if (joySlot_ < 0 && std::sqrt(dx * dx + dy * dy) <= l.joyR * 1.3f) {
        joySlot_ = static_cast<i32>(slot);
        joyX_ = x;
        joyY_ = y;
        return true;
    }
    // botão jump (ainda livre?) — tolerância 1.2× no rect
    const f32 mx = l.btnW * 0.1f;
    const f32 my = l.btnH * 0.1f;
    if (!btnHeld_ && x >= l.btnX - mx && x < l.btnX + l.btnW + mx &&
        y >= l.btnY - my && y < l.btnY + l.btnH + my) {
        btnSlot_ = static_cast<i32>(slot);
        btnHeld_ = true;
        return true;
    }
    return false;   // toque livre → pode ir para a câmara
}

void TouchControls::touchMove(u32 slot, f32 x, f32 y) {
    if (joySlot_ >= 0 && static_cast<i32>(slot) == joySlot_) {
        joyX_ = x;
        joyY_ = y;
    }
}

void TouchControls::touchEnd(u32 slot) {
    if (joySlot_ >= 0 && static_cast<i32>(slot) == joySlot_) {
        joySlot_ = -1;
        joyX_ = baseX_;
        joyY_ = baseY_;
    }
    if (btnSlot_ >= 0 && static_cast<i32>(slot) == btnSlot_) {
        btnSlot_ = -1;
        btnHeld_ = false;
    }
}

Vec2 TouchControls::axis() const {
    if (joySlot_ < 0) {
        return Vec2{0.0f, 0.0f};
    }
    Vec2 a{(joyX_ - baseX_) / radius_, -(joyY_ - baseY_) / radius_};   // y do ecrã cresce para baixo
    // 0.7.3 — SENSIBILIDADE: multiplica o vetor e volta a clamps no círculo
    // unitário (sens 1 = o comportamento 0.6.x exato)
    a.x *= sens;
    a.y *= sens;
    const f32 len = std::sqrt(a.x * a.x + a.y * a.y);
    if (len > 1.0f) {
        a.x /= len;
        a.y /= len;
    }
    return a;
}

bool TouchControls::action(const char* name) const {
    return name && std::strcmp(name, "jump") == 0 && btnHeld_;
}

} // namespace vv
