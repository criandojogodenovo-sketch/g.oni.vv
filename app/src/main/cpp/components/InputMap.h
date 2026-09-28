#pragma once
// components/InputMap.h — mapa de entrada do TIC (F3).
//
// PLACEHOLDER — NÃO É IMPLEMENTAÇÃO FINAL.
// Motivo: o preset PlayerBody3D da spec nasce com InputMap(vazio); bindings
// de ação (eixo/botão → ação) só fazem sentido com física/personagem — F4
// preenche os campos. Hoje o componente existe, é registrado no registry e
// serializa (presença round-trip), mas não tem dados nem comportamento.
#include "core/Component.h"

namespace vv {

class InputMap : public Component {
public:
    // F4: bindings ação → tecla/pointer. Vazio na F3 por design (CLÁUSULA CALMA).
};

} // namespace vv
