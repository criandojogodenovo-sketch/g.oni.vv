// tests/test_012d_fogo.cpp — 0.12-D (D-3): O FOGO (a borda animada, o glow
// por baixo, as brasas com portão, a rampa do ícone real).
//
// Os 5 pins da sub-fase D-3, pelo caminho REAL (UiContext → endFrame → o
// rasterizador honesto do stub → glReadPixels; a FONTE ÚNICA é o ui/Fire.h
// — o FS GLSL do render/Renderer.cpp replica-o linha a linha):
//   1. fogo_rampa        — os 4 tokens do Theme são a ORDEM DO DONO
//                          (#7A1F0A→#E8530F→#FFA21F→#FFE08A; a proveniência
//                          da extração é o scripts/d3_palette.py contra o
//                          ícone real docs/iconegonevv.png)
//   2. fogo_borda_anima  — a borda do ativo é o fbm ANIMADO (t=0 ≠ t=1.7)
//                          em MISTURA ADITIVA (o pixel lido = widget + flama;
//                          o centro morto NUNCA muda)
//   3. fogo_glow_below   — o glow por baixo: 2 quads α decrescente SOB o
//                          rect (quente, morre longe, nada acima)
//   4. brasas_portao     — o PORTÃO: celebration=false emite ZERO (o editor
//                          nunca vê brasas); true emite 16 partículas
//                          determinísticas a SUBIR
//   5. fogo_toggle_baixo — «Efeitos: Baixo» mata a borda, o glow e as
//                          brasas (a cláusula D-7: sem ruído/sem brasas)
//
// MUTAÇÕES (coladas no RELATORIO-0.12-D.md §3):
//   M-D3a — o fbm morto (fbm devolve 0.5) → fogo_borda_anima vermelho
//   M-D3b — o portão removido (drawEmbers ignora celebration) →
//           brasas_portao vermelho
#include "TestFramework.h"
#include <GLES3/gl3.h>   // o stub (nada de GL real — glReadPixels do fb)
#include "glstub_fb.h"
#include "render/Renderer.h"
#include "ui/Fire.h"
#include "ui/Sdf.h"
#include "ui/Theme.h"
#include "ui/UiContext.h"
#include <cmath>
#include <vector>

using namespace vv;

namespace {

constexpr int kW = 128;
constexpr int kH = 96;

// o frame pelo caminho REAL da engine (o mesmo harness da D-1/D-2)
struct Harness {
    Renderer renderer;
    UiContext ui;
    explicit Harness(const InputState* in) {
        glstub::fb::resetState();
        glstub::fb::enabled = true;
        theme::setDensity(1.0f);
        EXPECT(renderer.init());
        renderer.resize(kW, kH);
        renderer.beginFrame();
        renderer.beginUiPass(kW, kH);
        ui.beginFrame(&renderer, in, float(kW), float(kH));
        ui.panel(0.0f, 0.0f, float(kW), float(kH), theme::kTheme.bg);
    }
    ~Harness() {
        ui.endFrame();
        renderer.endFrame();
        glstub::fb::enabled = false;
    }
};

struct Px {
    unsigned char r, g, b, a;
};
Px readPx(const std::vector<unsigned char>& buf, int x, int y) {
    const int row = kH - 1 - y;   // GL: linha 0 = fundo
    const size_t i = (size_t(row) * kW + size_t(x)) * 4u;
    return Px{buf[i], buf[i + 1], buf[i + 2], buf[i + 3]};
}
f32 lumOf(const Px& p) { return f32(p.r) + f32(p.g) + f32(p.b); }

void readBack(std::vector<unsigned char>& px) {
    px.resize(size_t(kW) * kH * 4);
    glReadPixels(0, 0, kW, kH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
}

// o delta máximo (por canal) entre dois frames dentro de um rect
f32 maxDelta(const std::vector<unsigned char>& a,
             const std::vector<unsigned char>& b, int x0, int y0, int x1,
             int y1) {
    f32 m = 0.0f;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            const Px pa = readPx(a, x, y);
            const Px pb = readPx(b, x, y);
            m = std::max(m, std::fabs(f32(pa.r) - f32(pb.r)));
            m = std::max(m, std::fabs(f32(pa.g) - f32(pb.g)));
            m = std::max(m, std::fabs(f32(pa.b) - f32(pb.b)));
        }
    }
    return m;
}

} // namespace

// ---- PIN 1 — fogo_rampa ------------------------------------------------------
// «extraída do ícone real»: os 4 tokens do Theme são EXATAMENTE a ordem do
// dono (a fonte da verdade é a ORDEM; a extração do PNG real é documentada
// pelo scripts/d3_palette.py — a família quente R≥G>B confirmada nas 4
// âncoras; o relatório §3 colam a saída). A rampa ESCURECE→CLAREIA: a
// luminância é estritamente crescente fire0→fire3.
TEST(fogo_rampa) {
    const theme::Theme& t = theme::kTheme;
    auto eq = [](const f32 c[4], int r, int g, int b) {
        return std::fabs(c[0] - f32(r) / 255.0f) < 0.004f &&
               std::fabs(c[1] - f32(g) / 255.0f) < 0.004f &&
               std::fabs(c[2] - f32(b) / 255.0f) < 0.004f;
    };
    EXPECT_MSG(eq(t.fire0, 122, 31, 10), "fogo_rampa: fire0 ≠ #7A1F0A");
    EXPECT_MSG(eq(t.fire1, 232, 83, 15), "fogo_rampa: fire1 ≠ #E8530F");
    EXPECT_MSG(eq(t.fire2, 255, 162, 31), "fogo_rampa: fire2 ≠ #FFA21F");
    EXPECT_MSG(eq(t.fire3, 255, 224, 138), "fogo_rampa: fire3 ≠ #FFE08A");
    // a luminância SÓBE pela rampa (a base escura → o bico amarelo)
    auto lum = [](const f32 c[4]) {
        return 0.299f * c[0] + 0.587f * c[1] + 0.114f * c[2];
    };
    EXPECT_MSG(lum(t.fire0) < lum(t.fire1) && lum(t.fire1) < lum(t.fire2) &&
                   lum(t.fire2) < lum(t.fire3),
               "fogo_rampa: a luminância não é crescente pela rampa — a "
               "ordem da chama morreu?");
    // a família quente (o que a extração do ícone real mostra): R > B
    EXPECT_MSG(t.fire0[0] > t.fire0[2] && t.fire1[0] > t.fire1[2] &&
                   t.fire2[0] > t.fire2[2] && t.fire3[0] > t.fire3[2],
               "fogo_rampa: a família saiu do quente (R > B) — a rampa não "
               "é a do «G em chama»");
}

// ---- PIN 2 — fogo_borda_anima ------------------------------------------------
// «o elemento ATIVO tem borda de fogo ANIMADA em MISTURA ADITIVA»: três
// frames pelo caminho REAL — (A) widget+borda t=0, (B) widget+borda t=1.7,
// (C) widget sós. A banda da aresta: B≠A (o fbm corre) e A>C (a flama
// ACENDE light que o widget sós não tem). O centro morto: A==B EXATO (a
// máscara morre a bevelW para dentro — o fogo não mancha o interior).
// O pixel da banda obedece à ARITMÉTICA ADITIVA lida de C + flama².
TEST(fogo_borda_anima) {
    const f32 x0 = 24.0f, y0 = 24.0f, w0 = 64.0f, h0 = 32.0f, rad = 8.0f;
    const f32 flat[4] = {0.30f, 0.32f, 0.38f, 1.0f};
    sdf::g_level = sdf::LEVEL_ALTO;

    std::vector<unsigned char> fa, fb, fc;
    {
        sdf::g_time = 0.0f;
        Harness h(nullptr);
        h.ui.sdfWidget(x0, y0, w0, h0, rad, flat, flat);
        h.ui.sdfFireBorder(x0, y0, w0, h0, rad);
        h.ui.endFrame();
        h.renderer.endFrame();
        readBack(fa);
    }
    {
        sdf::g_time = 1.7f;
        Harness h(nullptr);
        h.ui.sdfWidget(x0, y0, w0, h0, rad, flat, flat);
        h.ui.sdfFireBorder(x0, y0, w0, h0, rad);
        h.ui.endFrame();
        h.renderer.endFrame();
        readBack(fb);
    }
    {
        sdf::g_time = 0.0f;
        Harness h(nullptr);
        h.ui.sdfWidget(x0, y0, w0, h0, rad, flat, flat);
        h.ui.endFrame();
        h.renderer.endFrame();
        readBack(fc);
    }
    sdf::g_time = 0.0f;

    // a banda: o anel [−3, +3] à volta da aresta (bevelW 3 + a folga da AA);
    // o centro morto: o miolo a 8px da aresta (d > bevelW → máscara 0)
    const f32 dAnim = maxDelta(fa, fb, int(x0) - 3, int(y0) - 3,
                               int(x0 + w0) + 3, int(y0 + h0) + 3);
    EXPECT_MSG(dAnim > 10.0f,
               "fogo_borda_anima: a banda da aresta é IGUAL entre t=0 e "
               "t=1.7 (Δmáx=%.1f) — o fbm morreu, a chama não corre", dAnim);
    const f32 dFlame = maxDelta(fa, fc, int(x0) - 3, int(y0) - 3,
                                int(x0 + w0) + 3, int(y0 + h0) + 3);
    EXPECT_MSG(dFlame > 15.0f,
               "fogo_borda_anima: a borda não acende sobre o widget sós "
               "(Δmáx=%.1f) — a flama não foi emitida/aditiva", dFlame);
    const f32 dCore = maxDelta(fa, fb, int(x0) + 8, int(y0) + 8,
                               int(x0 + w0) - 8, int(y0 + h0) - 8);
    EXPECT_MSG(dCore <= 1.0f,
               "fogo_borda_anima: o centro morto mudou (Δmáx=%.1f) — a "
               "máscara do fogo está a vazar para dentro", dCore);

    // a ARITMÉTICA ADITIVA num pixel da banda: lido(A) == lido(C) + flama²
    // (o mesmo mix do FS: fr = mix(fire1, fire3, n) e o α = a máscara; se
    // o blend fosse o clássico, o destino multiplicaria por (1−α) e o lido
    // ficaria ABAIXO da previsão por C·mask² — ≥15 nos pixéis do fill)
    {
        const sdf::LevelParams lp =
            sdf::paramsFor(sdf::LEVEL_ALTO, theme::dp(1.0f));
        bool proved = false;
        for (int y = int(y0) - 2; y < int(y0 + h0) + 2 && !proved; ++y) {
            for (int x = int(x0) - 2; x < int(x0 + w0) + 2 && !proved; ++x) {
                const f32 lx = f32(x) + 0.5f - x0;
                const f32 ly = f32(y) + 0.5f - y0;
                f32 mask = 0.0f, n = 0.0f;
                vv::fire::fireBorder(lx, ly, w0 * 0.5f, h0 * 0.5f, rad,
                                     lp.bevelW, 0.0f, mask, n);
                if (mask < 0.45f) {
                    continue;   // só pixéis com sinal forte
                }
                const Px pc = readPx(fc, x, y);
                // o teto: a soma não pode saturar (o clamp comeria a prova)
                f32 fr[3];
                for (int c = 0; c < 3; ++c) {
                    fr[c] = theme::kTheme.fire1[c] * (1.0f - n) +
                            theme::kTheme.fire3[c] * n;
                }
                bool ok = true;
                for (int c = 0; c < 3 && ok; ++c) {
                    const f32 base = f32(c == 0 ? pc.r : c == 1 ? pc.g : pc.b) /
                                     255.0f;
                    const f32 want = base + fr[c] * mask * mask;
                    if (want > 0.96f) {
                        ok = false;   // saturaria — procura outro pixel
                    }
                }
                if (!ok) {
                    continue;
                }
                const Px pa = readPx(fa, x, y);
                const f32 got[3] = {f32(pa.r) / 255.0f, f32(pa.g) / 255.0f,
                                    f32(pa.b) / 255.0f};
                for (int c = 0; c < 3; ++c) {
                    const f32 base =
                        f32(c == 0 ? pc.r : c == 1 ? pc.g : pc.b) / 255.0f;
                    const f32 want = base + fr[c] * mask * mask;
                    EXPECT_MSG(std::fabs(got[c] - want) < 0.04f,
                               "fogo_borda_anima: o pixel (%d,%d) leu %.3f, "
                               "a aritmética ADITIVA prevê %.3f (mask=%.2f) — "
                               "o blend do fogo não é SRC_ALPHA/GL_ONE",
                               x, y, got[c], want, mask);
                }
                proved = true;
            }
        }
        EXPECT_MSG(proved, "fogo_borda_anima: nenhum pixel da banda com "
                           "sinal e sem saturação — a flama nem chegou ao fb");
    }
    sdf::g_level = sdf::LEVEL_ALTO;
}

// ---- PIN 3 — fogo_glow_below -------------------------------------------------
// «o glow por baixo»: 2 quads α decrescente SOB o rect (a reflexão da
// chama no «chão»). Quente (o vermelho do glow domina o azul), MORRE
// longe e NADA acima do rect (o glow só vive por baixo).
TEST(fogo_glow_below) {
    const f32 x0 = 24.0f, y0 = 24.0f, w0 = 64.0f, h0 = 32.0f;
    sdf::g_level = sdf::LEVEL_ALTO;
    Harness h(nullptr);
    const u32 before = h.ui.solidsForTest().vertexCount();
    h.ui.fireGlowBelow(x0, y0, w0, h0);
    const u32 emitted = h.ui.solidsForTest().vertexCount() - before;
    EXPECT_MSG(emitted == 12,
               "fogo_glow_below: o emissor lançou %u vértices, os 2 quads "
               "α decrescente são 12 — o padrão honesto da casa morreu?",
               emitted);
    h.ui.endFrame();
    h.renderer.endFrame();
    std::vector<unsigned char> px;
    readBack(px);

    const int cx = int(x0 + w0 * 0.5f);
    const Px near_ = readPx(px, cx, int(y0 + h0) + 2);   // o pé do glow
    const Px mid = readPx(px, cx, int(y0 + h0) + 6);
    const Px far_ = readPx(px, cx, int(y0 + h0) + 11);   // além do d2=12? não
    const Px above = readPx(px, cx, int(y0) - 4);        // ACIMA do rect
    // o glow é QUENTE: o vermelho domina (o glow #FF3D00)
    EXPECT_MSG(f32(near_.r) > f32(near_.b) + 20.0f,
               "fogo_glow_below: o pé do glow não é quente (r=%d b=%d) — "
               "a cor do glow morreu?", near_.r, near_.b);
    // decai com a distância (α decrescente — os 2 quads)
    EXPECT_MSG(lumOf(near_) > lumOf(mid) && lumOf(mid) > lumOf(far_),
               "fogo_glow_below: o glow não decai (pé %.0f, meio %.0f, longe "
               "%.0f) — o gradiente α morreu?", lumOf(near_), lumOf(mid),
               lumOf(far_));
    // nada ACIMA (o glow só vive por baixo)
    EXPECT_MSG(std::fabs(f32(above.r) - theme::kTheme.bg[0] * 255.0f) < 2.0f,
               "fogo_glow_below: há luz ACIMA do rect (r=%d) — o glow vazou "
               "para cima", above.r);
    sdf::g_level = sdf::LEVEL_ALTO;
}

// ---- PIN 4 — brasas_portao ---------------------------------------------------
// «10-20 partículas a subir, SÓ na splash e no ecrã de projetos, NUNCA no
// editor»: O PORTÃO é a celebração — false emite ZERO (o editor chama o
// frame inteiro e não nasce UMA brasa); true emite 16 (o meio do intervalo
// do prompt). As brasas são PARAMÉTRICAS: o mesmo t dá as MESMAS brasas
// (determinismo) e SOBEM (y decresce dentro do período).
TEST(brasas_portao) {
    sdf::g_level = sdf::LEVEL_ALTO;
    Harness h(nullptr);
    const u32 before = h.ui.solidsForTest().vertexCount();
    // O PORTÃO: o EDITOR (celebration=false) — NEM UMA brasa
    h.ui.drawEmbers(0.0f, 0.0f, 200.0f, 180.0f, 3.0f, /*celebration=*/false);
    EXPECT_MSG(h.ui.solidsForTest().vertexCount() == before,
               "brasas_portao: o portão está ABERTO (o editor emitiu "
               "brasas) — a cláusula «nunca no editor» morreu");
    // a celebração: 16 partículas × 6 vértices
    h.ui.drawEmbers(0.0f, 0.0f, 200.0f, 180.0f, 3.0f, /*celebration=*/true);
    const u32 emitted = h.ui.solidsForTest().vertexCount() - before;
    EXPECT_MSG(emitted == 16u * 6u,
               "brasas_portao: a celebração emitiu %u vértices, as 16 "
               "brasas são 96 — a contagem do prompt (10-20) morreu?",
               emitted);
    // o determinismo + a subida (a função pura do ui/Fire.h)
    f32 a[4], b[4], c[4];
    vv::fire::ember(5, 2.5f, 200.0f, 180.0f, a);
    vv::fire::ember(5, 2.5f, 200.0f, 180.0f, b);
    for (int i = 0; i < 4; ++i) {
        EXPECT_MSG(std::fabs(a[i] - b[i]) < 1e-5f,
                   "brasas_portao: a brasa 5 não é determinística no t "
                   "(campo %d: %.3f vs %.3f)", i, a[i], b[i]);
    }
    vv::fire::ember(5, 3.0f, 200.0f, 180.0f, c);
    EXPECT_MSG(c[1] < a[1],
               "brasas_portao: a brasa não SOBE (y t=3.0 %.1f ≥ y t=2.5 "
               "%.1f) — o movimento das brasas morreu", c[1], a[1]);
    // o tamanho 1.5..4px e o x dentro do rect (com a folga do wobble ±12)
    EXPECT_MSG(a[2] >= 1.5f && a[2] <= 4.0f,
               "brasas_portao: o tamanho da brasa (%.2f) saiu do 1.5..4",
               a[2]);
    EXPECT_MSG(a[0] >= -12.0f && a[0] <= 212.0f,
               "brasas_portao: o x da brasa (%.1f) saiu do rect com o "
               "wobble", a[0]);
    sdf::g_level = sdf::LEVEL_ALTO;
}

// ---- PIN 5 — fogo_toggle_baixo ------------------------------------------------
// «Efeitos: Baixo = sem ruído, sem brasas» (a cláusula D-7): em Baixo o
// emissor da borda NEMEMITE (a borda É o ruído animado), o glow morre e as
// brasas TAMBÉM — o ativo continua legível pelo fill accent→accent2 do
// widget (o que a D-1 deixou em todos os níveis).
TEST(fogo_toggle_baixo) {
    const f32 x0 = 24.0f, y0 = 24.0f, w0 = 64.0f, h0 = 32.0f, rad = 8.0f;
    auto borderVerts = [&](u8 level) -> u32 {
        sdf::g_level = level;
        Harness h(nullptr);
        const u32 before = h.ui.sdfForTest().vertexCount();
        h.ui.sdfFireBorder(x0, y0, w0, h0, rad);
        return h.ui.sdfForTest().vertexCount() - before;
    };
    EXPECT_MSG(borderVerts(sdf::LEVEL_ALTO) == 6u,
               "fogo_toggle_baixo: em Alto a borda não emite o quad "
               "(6 vértices) — o fogo do ativo morreu");
    EXPECT_MSG(borderVerts(sdf::LEVEL_BAIXO) == 0u,
               "fogo_toggle_baixo: em Baixo a borda AINDA emite — o toggle "
               "«sem ruído» da D-7 morreu");

    auto emberVerts = [&](u8 level) -> u32 {
        sdf::g_level = level;
        Harness h(nullptr);
        const u32 before = h.ui.solidsForTest().vertexCount();
        h.ui.drawEmbers(0.0f, 0.0f, 200.0f, 180.0f, 1.0f, true);
        return h.ui.solidsForTest().vertexCount() - before;
    };
    EXPECT_MSG(emberVerts(sdf::LEVEL_ALTO) == 96u,
               "fogo_toggle_baixo: em Alto as brasas sumiram (96 vértices)");
    EXPECT_MSG(emberVerts(sdf::LEVEL_BAIXO) == 0u,
               "fogo_toggle_baixo: em Baixo as brasas AINDA emitem — «sem "
               "brasas» da D-7 morreu");

    auto glowVerts = [&](u8 level) -> u32 {
        sdf::g_level = level;
        Harness h(nullptr);
        const u32 before = h.ui.solidsForTest().vertexCount();
        h.ui.fireGlowBelow(24.0f, 24.0f, 64.0f, 32.0f);
        return h.ui.solidsForTest().vertexCount() - before;
    };
    EXPECT_MSG(glowVerts(sdf::LEVEL_ALTO) == 12u,
               "fogo_toggle_baixo: em Alto o glow sumiu (12 vértices)");
    EXPECT_MSG(glowVerts(sdf::LEVEL_BAIXO) == 0u,
               "fogo_toggle_baixo: em Baixo o glow AINDA emite — o brilho "
               "extra devia morrer no toggle");
    sdf::g_level = sdf::LEVEL_ALTO;
}
