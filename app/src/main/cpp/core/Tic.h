#pragma once
// core/Tic.h — entidade/nó da VV (inspiração Godot/itsmagic).
// F1: identidade + estado mínimo. Composição (TIC = ator dono de componentes)
// entra na F3 — nenhum componente aqui (CLÁUSULA CALMA).
#include <string>
#include "Handle.h"

namespace vv {

struct Tic {
    Handle      handle{};      // handle generacional deste TIC
    std::string name;          // nome de busca (F1: primeiro ativo com o nome)
    bool        active = false;
    i32         parent = -1;   // índice do slot do TIC pai; -1 = sem pai
                               // F1: índice cru — resolução/composição definida na F3
};

} // namespace vv
