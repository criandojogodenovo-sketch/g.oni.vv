// tests/test_time.cpp — Time/accumulator: passos fixos determinísticos + guard anti-spiral.
#include "TestFramework.h"
#include "core/Time.h"

using namespace vv;
using vv::Time;

TEST(now_monotonico) {
    const double t0 = vv::nowSeconds();
    const double t1 = vv::nowSeconds();
    EXPECT(t1 >= t0);
    EXPECT(t1 - t0 < 1.0);   // duas leituras seguidas não distam 1s
}

TEST(passos_fixos_deterministicos) {
    Time a;
    Time b;
    // mesma sequência de realDt em duas instâncias → mesmos passos (determinismo)
    const double seq[] = { 1.0 / 60.0, 1.0 / 60.0, 2.5 / 60.0,
                           1.0 / 60.0, 0.5 / 60.0, 3.3 / 60.0 };
    for (const double dt : seq) {
        const u32 sa = a.beginFrame(dt);
        const u32 sb = b.beginFrame(dt);
        EXPECT(sa == sb);
        for (u32 i = 0; i < sa; ++i) { a.endStep(); }
        for (u32 i = 0; i < sb; ++i) { b.endStep(); }
    }
    EXPECT(a.totalSteps == b.totalSteps);
    // 1/60 + 1/60 + 2.5/60 + 1/60 + 0.5/60 + 3.3/60 → 9 passos inteiros de 1/60
    EXPECT(a.totalSteps == 9u);
}

TEST(dt_fixo_consome_um_passo_por_frame) {
    Time t;
    for (int i = 0; i < 120; ++i) {
        const u32 steps = t.beginFrame(1.0 / 60.0);
        EXPECT(steps == 1u);
        t.endStep();
    }
    EXPECT(t.totalSteps == 120u);
    EXPECT(t.accumulator >= 0.0 && t.accumulator < t.fixedDt);
}

TEST(guard_anti_spiral_limita_passos) {
    Time t;   // maxStepsPerFrame = 5 por padrão
    const u32 steps = t.beginFrame(10.0);   // frame "travado" de 10s
    EXPECT(steps <= t.maxStepsPerFrame);
    EXPECT(steps == 5u);
    for (u32 i = 0; i < steps; ++i) { t.endStep(); }
    EXPECT(t.totalSteps == 5u);   // NÃO tentou "recuperar" os 600 frames atrasados
    EXPECT(t.accumulator <= t.fixedDt * static_cast<double>(t.maxStepsPerFrame) + 1e-9);
}

TEST(frame_muito_longo_seguinte_normaliza) {
    Time t;
    t.beginFrame(10.0);
    for (u32 i = 0; i < 5; ++i) { t.endStep(); }   // consome o clamp do frame travado
    const u32 steps = t.beginFrame(1.0 / 60.0);    // frame normal seguinte
    EXPECT(steps == 1u);
    t.endStep();
    EXPECT(t.totalSteps == 6u);
}

TEST(alpha_no_intervalo_zero_um) {
    Time t;
    const u32 steps = t.beginFrame(2.5 / 60.0);
    for (u32 i = 0; i < steps; ++i) { t.endStep(); }
    EXPECT(t.alpha() >= 0.0 && t.alpha() <= 1.0);
}

TEST(dt_negativo_tratado_como_zero) {
    Time t;
    const u32 steps = t.beginFrame(-1.0);
    EXPECT(steps == 0u);
    EXPECT(t.accumulator == 0.0);
}
