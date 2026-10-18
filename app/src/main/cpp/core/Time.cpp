#include "Time.h"
#include <chrono>

namespace vv {

double nowSeconds() {
    using clock = std::chrono::steady_clock;
    return std::chrono::duration<double>(clock::now().time_since_epoch()).count();
}

u32 Time::beginFrame(double realDt) {
    if (realDt < 0.0) {
        realDt = 0.0;
    }
    accumulator += realDt;

    // Guard anti-spiral: joga fora o excesso além de N passos por frame.
    const double maxAccum = fixedDt * static_cast<double>(maxStepsPerFrame);
    if (accumulator > maxAccum) {
        accumulator = maxAccum;
    }

    // eps: evita que arredondamento de ponto flutuante derrube o passo
    // quando o acumulador fica exatamente no limite do clamp.
    u32 steps = static_cast<u32>(accumulator / fixedDt + 1e-9);
    if (steps > maxStepsPerFrame) {
        steps = maxStepsPerFrame;
    }
    return steps;
}

void Time::endStep() {
    accumulator -= fixedDt;
    if (accumulator < 0.0) {
        accumulator = 0.0;
    }
    ++totalSteps;
}

double Time::alpha() const {
    return (fixedDt > 0.0) ? accumulator / fixedDt : 0.0;
}

} // namespace vv
