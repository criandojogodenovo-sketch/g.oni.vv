#pragma once
// core/Time.h — relógio monotônico + loop de timestep fixo (padrão clássico).
// Uso por frame:
//   const u32 steps = time.beginFrame(realDt);
//   for (u32 s = 0; s < steps; ++s) { update(time.fixedDt); time.endStep(); }
// Se realDt explodir (frame longo/pausa), beginFrame joga fora o excesso
// (guard anti-spiral): o jogo atrasa, mas nunca espirala.
#include "Types.h"

namespace vv {

// Relógio monotônico do sistema, em segundos (steady_clock).
double nowSeconds();

struct Time {
    double fixedDt          = 1.0 / 60.0;  // passo fixo (60 Hz)
    double accumulator      = 0.0;
    u32    maxStepsPerFrame = 5;            // guard anti-spiral
    u64    totalSteps       = 0;

    // Acumula realDt, aplica o guard e devolve quantos passos fixos rodar.
    u32 beginFrame(double realDt);

    // Consome um passo fixo (chamar após cada update).
    void endStep();

    // Fração do próximo passo acumulada — interpolação futura (F2+).
    double alpha() const;
};

} // namespace vv
