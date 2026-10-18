#pragma once
// components/InputMap.h — mapa de entrada do TIC (F3; fonte na F4).
//
// F4: o InputMap liga ZERO OU UMA fonte de input (physics/InputSource.h).
// O PhysicsSystem lê axis()/action() da fonte ligada a cada tick; sem fonte
// (source == nullptr E sem TouchControls irmão), o TIC não recebe input.
// O campo é um ponteiro NÃO-DONO — o fluxo default resolve a fonte de novo
// a cada tick (nunca fica pendente; ver InputSource.h).
#include "core/Component.h"

namespace vv {

struct InputSource;

class InputMap : public Component {
public:
    const InputSource* source = nullptr;   // fonte ligada (não-dono; pode ser nula)
};

} // namespace vv
