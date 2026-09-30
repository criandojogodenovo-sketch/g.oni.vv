#pragma once
// components/TouchControls.h — controlos de toque EDITÁVEIS (0.7.3; F4-D).
//
// Implementação concreta de InputSource (physics/InputSource.h): joystick
// virtual + 1 botão "jump". Tema mono. Layout DERIVÁVEL: pos (fração da
// área útil), tamanho (escala do raio) e sensibilidade (multiplicador do
// eixo) são CAMPOS do componente — editáveis no Inspector de UI do editor
// 2D (a UI do Player passa a ser esta instância) e serializados no .goni.
// O default reproduz o layout fixo da 0.6.x no ecrã de referência
// (1600×720): relX 0.09375 (=150px), relY 0.7361 (=530px), size/sens 1.
//
// O input é GL-free e host-testável: o main alimenta touchBegin/Move/End
// a partir do InputState e o PhysicsSystem lê axis()/action() via InputMap.
// O DESENHO (quad batch, só em modo Play) vive no ui/ — este ficheiro não
// toca em GL. O layout vem de layoutFor (frações da ÁREA ÚTIL — resolução
// independente desde 0.7.3; era fixo em px).
#include "core/Component.h"
#include "math/Math.h"
#include "physics/InputSource.h"

namespace vv {

class TouchControls : public Component, public InputSource {
public:
    // layout (px, origem topo-esquerda da ÁREA ÚTIL; deriva dos campos)
    struct Layout {
        f32 joyCX, joyCY, joyR;      // centro + raio do joystick
        f32 btnX, btnY, btnW, btnH;  // rect do botão jump
    };

    // ---- campos EDITÁVEIS (0.7.3; serializados quando não-default) --------
    // pos: fração da área útil [0..1] (o centro do joystick)
    f32 relX = 0.09375f;   // default = 150px num ecrã 1600 de largura
    f32 relY = 0.7361f;    // default = 530px num ecrã 720 de altura
    f32 size = 1.0f;       // escala do raio (75px base)
    f32 sens = 1.0f;       // multiplicador do eixo (clamp |a|<=1)
    f32 colR = 0.1804f, colG = 0.1804f, colB = 0.1804f;   // mono LINE

    // layout DERIVADO dos campos (o default reproduz o fixo da 0.6.x)
    Layout layoutFor(f32 aw, f32 ah) const;
    // layout FIXO da 0.6.x (compat: testes/contratos antigos)
    static Layout layout(f32 sw, f32 sh);

    // ---- alimentação de input (por frame, a partir do InputState) ----------
    // devolve true se o toque nasceu dentro de um controlo (o main usa isto
    // para NÃO entregar o gesto à câmara de orbit). USA o layout EDITÁVEL.
    bool touchBegin(u32 slot, f32 x, f32 y, f32 sw, f32 sh);
    void touchMove(u32 slot, f32 x, f32 y);
    void touchEnd(u32 slot);

    // ---- InputSource --------------------------------------------------------
    // eixo do joystick em [-1,1]² (x = direita, y = CIMA do ecrã); 0 sem
    // toque; MULTIPLICADO pela sensibilidade (clamp no círculo unitário)
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
    f32  baseX_ = 0.0f;    // centro do joystick (do layout EDITÁVEL)
    f32  baseY_ = 0.0f;
    f32  radius_ = 1.0f;
    f32  joyX_ = 0.0f;     // pos atual do dedo
    f32  joyY_ = 0.0f;
    i32  btnSlot_ = -1;
    bool btnHeld_ = false;
};

} // namespace vv
