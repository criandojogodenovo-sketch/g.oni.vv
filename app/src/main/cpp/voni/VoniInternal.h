#pragma once
// voni/VoniInternal.h — definição do Script::Impl (partilhada entre
// VoniCompile.cpp e VoniVm.cpp). PRIVADO do módulo voni/ — NÃO incluir
// fora de voni/ (o Voni.h é a API pública).
//
// Pimpl via header interno: o Voni.h promete `struct Impl;` e os dois TUs
// que precisam de mexer no estado (compilar e correr) vêem a definição
// completa. Nada disto vaza para a engine.
//
// 0.9.5: o Impl ganha o RUNTIME dos linkers/tykers (o registo de RFs +
// o estado por tyker — sobrevive entre frames; o reset() limpa tudo).
#include "voni/Voni.h"
#include "voni/VoniAst.h"
#include "voni/VoniTykers.h"

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

    // 0.9.5 · LINKERS & TYKERS: os RFs declarados pelos linkers (partilhados
    // por todos os tykers do script) + o estado por tyker (ativação, delay,
    // params, componentes contínuos resolvidos)
    tykers::Registry            rfReg;
    std::vector<tykers::TykerState> tykerRuns;

    // 0.12-U (3.9c): o ESTADO DOS TIMERS POR NÓ AST (every/after/when) —
    // a chave é o PONTEIRO do nó (o program é estável: shared_ptr; runs
    // repetidos não recompilam) e o reset() limpa (o restart recomeça do
    // zero — o mesmo contrato das variáveis globais)
    struct TimerState {
        f64  acc = 0.0;      // o acumulador (every/after) em segundos
        bool fired = false;  // after/when já disparou (1× e morre)
    };
    std::unordered_map<const Stmt*, TimerState> timerStates;

    void reset() {
        started = false;
        running = false;
        fatal = Error::fine();
        globals.clear();
        exportOrder.clear();
        lastInstr = 0;
        rfReg.clear();
        tykerRuns.clear();
        timerStates.clear();   // 0.12-U (3.9c): o estado dos timers morre
    }
};

} // namespace voni
