#pragma once
// components/TouchControls.h — controlos de toque mínimos e FIXOS (F4-D).
//
// Implementação concreta de InputSource (physics/InputSource.h): joystick
// virtual à esquerda + 1 botão "jump" à direita. Tema mono, layout fixo,
// NÃO-CRIÁVEL (sem editor de botões/cores/textos — UI criável é F6).
//
// O input é GL-free e host-testável: o main alimenta touchBegin/Move/End a
// partir do InputState e o PhysicsSystem lê axis()/action() via InputMap.
// O DESENHO (quad batch, só em modo Play) vive no ui/ — este ficheiro não
// toca em GL. Layout calculado de (sw,sh) — fonte única para hit-test e draw.
#include "core/Component.h"
#include "math/Math.h"
#include "physics/InputSource.h"

namespace vv {

class TouchControls : public Component, public InputSource {
public:
    // layout fixo (px, origem topo-esquerda; respeita status line)
    struct Layout {
        f32 joyCX, joyCY, joyR;      // centro + raio do joystick
        f32 btnX, btnY, btnW, btnH;  // rect do botão jump
    };
    static Layout layout(f32 sw, f32 sh);

    // ---- alimentação de input (por frame, a partir do InputState) ----------
    // devolve true se o toque nasceu dentro de um controlo (o main usa isto
    // para NÃO entregar o gesto à câmara de orbit)
    bool touchBegin(u32 slot, f32 x, f32 y, f32 sw, f32 sh);
    void touchMove(u32 slot, f32 x, f32 y);
    void touchEnd(u32 slot);

    // ---- InputSource --------------------------------------------------------
    // eixo do joystick em [-1,1]² (x = direita, y = CIMA do ecrã); 0 sem toque
    Vec2 axis() const override;
    // "jump" = botão premido; outros nomes → false
    bool action(const char* name) const override;

    // ---- estado para o desenho (ui/) ----------------------------------------
    bool joystickActive() const { return joySlot_ >= 0; }
    f32  knobX() const { return joyX_; }   // pos do dedo (px ecrã)
    f32  knobY() const { return joyY_; }
    bool buttonHeld() const { return btnHeld_; }
    // o slot está reclamado por um controlo (câmara não pode usar o dedo)
    bool ownsSlot(u32 slot) const {
        return joySlot_ == static_cast<i32>(slot) || btnSlot_ == static_cast<i32>(slot);
    }
    f32  baseX() const { return baseX_; }  // centro do joystick no layout
    f32  baseY() const { return baseY_; }

private:
    i32  joySlot_ = -1;
    f32  baseX_ = 0.0f;    // centro do joystick (fixo, do layout)
    f32  baseY_ = 0.0f;
    f32  radius_ = 1.0f;
    f32  joyX_ = 0.0f;     // pos atual do dedo
    f32  joyY_ = 0.0f;
    i32  btnSlot_ = -1;
    bool btnHeld_ = false;
};

} // namespace vv
