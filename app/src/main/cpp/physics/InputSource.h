#pragma once
// physics/InputSource.h — fonte abstrata de input do engine (F4-D).
//
// Um InputMap de TIC liga ZERO OU UMA fonte (InputMap::source, ponteiro
// não-dono). A F4 traz uma implementação concreta: TouchControls (joystick
// virtual + botão jump). Sem fonte ligada, o TIC não recebe input.
//
// NOTA de ciclo de vida: o ponteiro é não-dono. O caminho default da F4 não
// guarda ponteiros — o PhysicsSystem resolve a fonte DE NOVO a cada tick
// (InputMap::source explícita, ou o TouchControls irmão do mesmo TIC), logo
// fontes destruídas nunca ficam pendentes nesse fluxo.
#include "math/Math.h"

namespace vv {

struct InputSource {
    virtual ~InputSource() = default;
    // eixo de movimento em [-1,1]² (x = direita, y = frente)
    virtual Vec2 axis() const = 0;
    // ação nomeada ("jump" na F4); nomes desconhecidos devolvem false
    virtual bool action(const char* name) const = 0;
};

} // namespace vv
