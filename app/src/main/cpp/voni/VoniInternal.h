#pragma once
// voni/VoniInternal.h — definição do Script::Impl (partilhada entre
// VoniCompile.cpp e VoniVm.cpp). PRIVADO do módulo voni/ — NÃO incluir
// fora de voni/ (o Voni.h é a API pública).
//
// Pimpl via header interno: o Voni.h promete `struct Impl;` e os dois TUs
// que precisam de mexer no estado (compilar e correr) vêem a definição
// completa. Nada disto vaza para a engine.
#include "voni/Voni.h"
#include "voni/VoniAst.h"

#include <unordered_map>

namespace voni {

struct Script::Impl {
    // AST (partilhada: runs repetidos não recompilam)
    std::shared_ptr<Program> program;   // null = não compilado

    // ciclo de vida (spec §3: top-level 1× → on moment 1× → allmoments/frame)
    bool started = false;    // runStart já correu (idempotência da central)
    bool running = false;    // started && sem erro fatal && sem stop
    Error fatal;             // 1º erro fatal (a Vm para de correr frames)

    // variáveis do script (GLOBAIS ao script — decisão 🔶 documentada:
    // v++/v# declaram na tabela global; os params de fn vivem no frame da
    // chamada e fazem shadow)
    std::unordered_map<std::string, Value> globals;

    // ordem de declaração dos @+ (o Inspector mostra por esta ordem)
    std::vector<std::string> exportOrder;

    u64 lastInstr = 0;       // instruções do último tick (budget/telemetria)

    void reset() {
        started = false;
        running = false;
        fatal = Error::fine();
        globals.clear();
        exportOrder.clear();
        lastInstr = 0;
    }
};

} // namespace voni
