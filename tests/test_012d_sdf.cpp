// tests/test_012d_sdf.cpp — 0.12-D (D-1): O SHADER SDF ÚNICO DE WIDGET.
//
// Os 4 pins da sub-fase D-1 (a ordem do prompt), pelo caminho REAL
// (UiContext::sdfWidget → Renderer endFrame → o rasterizador honesto do
// stub — a MESMA função ui/Sdf.h::shade que o FS GLSL replica):
//   1. sdf_widget_cantos — cantos PERFEITOS: a cobertura do arco segue a
//      distância (rampa monotónica, cov parcial no arco, 0 fora)
//   2. sdf_bisel — o pixel do topo-esquerda brilha (a normal · luz) e o
//      fundo escurece (o volume 3D)
//   3. sdf_sombra — a sombra projetada por baixo (escura junto à aresta,
//      nada longe dela)
//   4. sdf_toggle_baixo — "Efeitos: Baixo" DESLIGA especular + brilho
//      (só bisel + sombra simples — a ordem do dono)
//
// NOTA de método: o α do fb é INÚTIL contra o clear (α=1.0 do bg) — as
// sondas leem o CANAL R contra o bg (o blend SRC_ALPHA dá
// dst = src·α + dst·(1−α) → a cobertura sai do delta).
//
// MUTAÇÕES (coladas no RELATORIO-0.12-D.md §1):
//   M-D1a — o bisel removido (mul = 1.0 no shade) → sdf_bisel vermelho
//   M-D1b — o toggle ignorado (paramsFor devolve SEMPRE Alto) →
//           sdf_toggle_baixo vermelho
#include "TestFramework.h"
#include <GLES3/gl3.h>   // o stub (nada de GL real — glReadPixels do fb)
#include "glstub_fb.h"
#include "render/Renderer.h"
#include "render/SdfBatch.h"
#include "ui/Sdf.h"
#include "ui/Theme.h"
#include "ui/UiContext.h"
#include <cmath>
#include <vector>

using namespace vv;

namespace {

constexpr int kW = 128;
constexpr int kH = 96;

// o frame pelo caminho REAL da engine (o mesmo que o main faz por frame)
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

// leitura GL (origem no FUNDO) → o pixel do ECRÃ (x, y)
struct Px {
    unsigned char r, g, b, a;
};
Px readPx(const std::vector<unsigned char>& buf, int x, int y) {
    const int row = kH - 1 - y;   // GL: linha 0 = fundo
    const size_t i = (size_t(row) * kW + size_t(x)) * 4u;
    return Px{buf[i], buf[i + 1], buf[i + 2], buf[i + 3]};
}
f32 lumOf(const Px& p) { return f32(p.r) + f32(p.g) + f32(p.b); }
// a COBERTURA estimada (o canal R: src cl sobre o bg — o blend do pass dá
// dst = src·α + dst·(1−α) → α estimado pelo delta do canal)
f32 covOf(const Px& p, f32 srcR, f32 bgR) {
    const f32 d = srcR - bgR;
    return (std::fabs(d) < 1e-4f)
               ? 0.0f
               : sdf::clamp01((f32(p.r) / 255.0f - bgR) / d);
}

} // namespace

// ---- PIN 1 — sdf_widget_cantos ----------------------------------------------
// «cantos perfeitos (comparar com o atual)»: a escadaria de quads é BINÁRIA
// (cobre por dentro, sem meia-cobertura no arco); o SDF dá a cobertura
// ANALÍTICA — a cobertura lida segue clamp01(0.5 − d), a própria distância.
// NOTA de método honesta: a sonda de cobertura usa o canto INFERIOR-DIREITO
// (o escuro — sem especular: o brilho aditivo da banda de topo satura a
// estimativa pelo canal R; o bisel/especular são provados pelo sdf_bisel).
TEST(sdf_widget_cantos) {
    const f32 x0 = 20.0f, y0 = 20.0f, w0 = 60.0f, h0 = 24.0f, rad = 8.0f;
    // cl médio (o bisel multiplica; a saturação do canal estraga a estimativa)
    const f32 cl[4] = {0.45f, 0.47f, 0.50f, 1.0f};
    sdf::g_level = sdf::LEVEL_ALTO;
    Harness h(nullptr);
    h.ui.sdfWidget(x0, y0, w0, h0, rad, cl, cl);
    h.ui.endFrame();
    h.renderer.endFrame();
    std::vector<unsigned char> px(size_t(kW) * kH * 4);
    glReadPixels(0, 0, kW, kH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());

    const f32 bgR = theme::kTheme.bg[0];
    // o SDF analítico (a FONTE ÚNICA) prevê a distância de cada pixel
    const f32 hw = w0 * 0.5f, hh = h0 * 0.5f;
    auto distAt = [&](int sx, int sy) {
        const f32 lx = f32(sx) + 0.5f - x0;
        const f32 ly = f32(sy) + 0.5f - y0;
        return sdf::roundRectSdf(lx - hw, ly - hh, hw, hh, rad);
    };
    auto expectCov = [&](int sx, int sy, const char* what) {
        const f32 d = distAt(sx, sy);
        const f32 want = sdf::clamp01(0.5f - d);
        const f32 got = covOf(readPx(px, sx, sy), cl[0], bgR);
        EXPECT_MSG(std::fabs(got - want) < 0.18f,
                   "sdf_widget_cantos: %s em (%d,%d) leu cov=%.2f, o SDF "
                   "prevê %.2f (d=%.2f)", what, sx, sy, got, want, d);
    };
    // (a) o INTERIOR do arco do canto inferior-direito: cobertura cheia
    expectCov(int(x0 + w0) - 4, int(y0 + h0) - 4, "interior do arco BR");
    // (b) o pixel EXATAMENTE no arco 45° do canto BR: cobertura PARCIAL —
    // o canto PERFEITO que a escadaria binária nunca deu
    {
        const f32 off = rad * 0.7071f;   // 45° no arco de raio 8
        const int ax = int(x0 + w0 - rad + off);
        const int ay = int(y0 + h0 - rad + off);
        const f32 d = distAt(ax, ay);
        const f32 want = sdf::clamp01(0.5f - d);
        const f32 got = covOf(readPx(px, ax, ay), cl[0], bgR);
        EXPECT_MSG(want > 0.05f && want < 0.95f &&
                       std::fabs(got - want) < 0.25f,
                   "sdf_widget_cantos: o pixel no arco 45° BR (%d,%d) leu "
                   "cov=%.2f (SDF %.2f) — esperada a cobertura parcial do "
                   "SDF", ax, ay, got, want);
    }
    // (c) FORA do arco (o canto morto BR): o fundo
    expectCov(int(x0 + w0) - 1, int(y0 + h0) - 1, "fora do arco BR");
    // (d) a diagonal do canto BR É a matemática do SDF: cada pixel lido é
    // previsto pela FUNÇÃO ÚNICA shade() (a mesma que o FS GLSL replica) +
    // o blend do pass — o pipeline inteiro (emissão → batch → raster →
    // blend) obedece à distância; a escadaria do panelRounded desvia em
    // vários destes pixéis (a comparação com o atual)
    {
        const sdf::LevelParams lp = sdf::paramsFor(sdf::LEVEL_ALTO, 1.0f);
        sdf::Shade sh;
        sh.hw = hw;
        sh.hh = hh;
        sh.radius = rad;
        sh.top[0] = cl[0]; sh.top[1] = cl[1];
        sh.top[2] = cl[2]; sh.top[3] = cl[3];
        sh.bot[0] = cl[0]; sh.bot[1] = cl[1];
        sh.bot[2] = cl[2]; sh.bot[3] = cl[3];
        sh.p = lp;
        int matched = 0;
        for (int i = 0; i < 6; ++i) {
            const int sx = int(x0 + w0) - 1 - i;
            const int sy = int(y0 + h0) - 1 - i;
            const f32 lx = f32(sx) + 0.5f - x0;
            const f32 ly = f32(sy) + 0.5f - y0;
            f32 out[4];
            sdf::shade(sh, lx, ly, 1.0f, out);
            const f32 predR =
                out[0] * out[3] + bgR * (1.0f - out[3]);   // o blend do pass
            const f32 got = f32(readPx(px, sx, sy).r) / 255.0f;
            if (std::fabs(got - predR) < 0.06f) {
                ++matched;
            } else {
                EXPECT_MSG(false,
                           "sdf_widget_cantos: o pixel diagonal BR (%d,%d) "
                           "leu R=%.3f, o SDF prevê %.3f — o pipeline não "
                           "obedece à distância", sx, sy, got, predR);
            }
        }
        EXPECT_MSG(matched == 6,
                   "sdf_widget_cantos: %d/6 pixéis da diagonal BR seguem o "
                   "SDF — o canto perfeito morreu?", matched);
    }
    // (e) a aresta reta também é AA (meio-pixel na borda esquerda)
    expectCov(int(x0) - 1, 32, "AA da aresta esquerda");
    sdf::g_level = sdf::LEVEL_ALTO;
}

// ---- PIN 2 — sdf_bisel --------------------------------------------------------
// «pixel com brilho no topo-esquerda»: a banda do bisel na aresta de TOPO
// fica MAIS CLARA que o interior (a normal aponta para a luz) e a de FUNDO
// mais escura — o volume 3D do shader único.
TEST(sdf_bisel) {
    const f32 x0 = 24.0f, y0 = 24.0f, w0 = 64.0f, h0 = 32.0f;
    const f32 flat[4] = {0.30f, 0.32f, 0.38f, 1.0f};
    sdf::g_level = sdf::LEVEL_ALTO;
    Harness h(nullptr);
    h.ui.sdfWidget(x0, y0, w0, h0, 6.0f, flat, flat);
    h.ui.endFrame();
    h.renderer.endFrame();
    std::vector<unsigned char> px(size_t(kW) * kH * 4);
    glReadPixels(0, 0, kW, kH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());

    const int cx = int(x0 + w0 * 0.5f);
    const int cy = int(y0 + h0 * 0.5f);
    const Px top = readPx(px, cx, int(y0));       // a aresta de topo
    const Px mid = readPx(px, cx, cy);            // o interior
    const Px bot = readPx(px, cx, int(y0 + h0) - 1);   // a aresta de fundo
    const f32 lumTop = lumOf(top);
    const f32 lumMid = lumOf(mid);
    const f32 lumBot = lumOf(bot);
    // o TOPO brilha (o bisel + o especular; a luz vem de cima)
    EXPECT_MSG(lumTop > lumMid + 30.0f,
               "sdf_bisel: a aresta de topo (%.0f) não brilha sobre o "
               "interior (%.0f) — o bisel morreu?", lumTop, lumMid);
    // o FUNDO escurece (o volume — o contrário do topo)
    EXPECT_MSG(lumBot < lumMid - 10.0f,
               "sdf_bisel: a aresta de fundo (%.0f) devia escurecer sob o "
               "interior (%.0f)", lumBot, lumMid);
    sdf::g_level = sdf::LEVEL_ALTO;
}

// ---- PIN 3 — sdf_sombra -------------------------------------------------------
// «sombra por baixo»: logo abaixo da aresta do widget a cor puxa para
// baixo (a sombra preta composta); longe dela o fundo está INTACTO.
TEST(sdf_sombra) {
    const f32 x0 = 24.0f, y0 = 20.0f, w0 = 64.0f, h0 = 28.0f;
    const f32 flat[4] = {0.25f, 0.27f, 0.33f, 1.0f};
    sdf::g_level = sdf::LEVEL_ALTO;
    Harness h(nullptr);
    h.ui.sdfWidget(x0, y0, w0, h0, 6.0f, flat, flat);
    h.ui.endFrame();
    h.renderer.endFrame();
    std::vector<unsigned char> px(size_t(kW) * kH * 4);
    glReadPixels(0, 0, kW, kH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());

    const int cx = int(x0 + w0 * 0.5f);
    const Px bg = readPx(px, cx, kH - 8);            // o fundo longe
    const Px under = readPx(px, cx, int(y0 + h0) + 2);   // a sombra de contato
    const Px farAway = readPx(px, cx, int(y0 + h0) + 12);  // além da penumbra
    const f32 lumBg = lumOf(bg);
    const f32 lumUnder = lumOf(under);
    const f32 lumFar = lumOf(farAway);
    EXPECT_MSG(lumUnder < lumBg - 4.0f,
               "sdf_sombra: o pixel por baixo (%.0f) devia ser mais escuro "
               "que o fundo (%.0f) — a sombra morreu?", lumUnder, lumBg);
    EXPECT_MSG(std::fabs(lumFar - lumBg) < 6.0f,
               "sdf_sombra: longe da aresta (%.0f) o fundo (%.0f) devia "
               "estar intacto — a penumbra não pode vazar", lumFar, lumBg);
    sdf::g_level = sdf::LEVEL_ALTO;
}

// ---- PIN 4 — sdf_toggle_baixo -------------------------------------------------
// «modo Baixo desliga os efeitos»: o ESPECULAR é o que o toggle mata (o
// bisel fica — é o piso de todos os níveis). O MESMO pixel (na banda do
// especular) comparado entre ALTO e BAIXO: em Alto brilha, em Baixo é o
// bisel puro. Só bisel + sombra simples — a ordem expressa do dono.
TEST(sdf_toggle_baixo) {
    const f32 x0 = 24.0f, y0 = 24.0f, w0 = 64.0f, h0 = 32.0f;
    const f32 flat[4] = {0.30f, 0.32f, 0.38f, 1.0f};
    auto probe = [&](u8 level) -> f32 {
        sdf::g_level = level;
        Harness h(nullptr);
        h.ui.sdfWidget(x0, y0, w0, h0, 6.0f, flat, flat);
        h.ui.endFrame();
        h.renderer.endFrame();
        std::vector<unsigned char> px(size_t(kW) * kH * 4);
        glReadPixels(0, 0, kW, kH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        const int cx = int(x0 + w0 * 0.5f);
        return f32(readPx(px, cx, int(y0) + 1).r);   // a banda do especular
    };
    // os PARÂMETROS: Alto tem spec+glow; Baixo NÃO (a fonte única)
    const sdf::LevelParams hi = sdf::paramsFor(sdf::LEVEL_ALTO, 1.0f);
    const sdf::LevelParams lo = sdf::paramsFor(sdf::LEVEL_BAIXO, 1.0f);
    EXPECT(hi.spec && hi.glow && hi.specAdd > 0.0f);
    EXPECT(!lo.spec && !lo.glow && lo.specAdd == 0.0f);
    // o PIXEL: o mesmo sítio nos dois níveis — o especular é a diferença
    const f32 rHi = probe(sdf::LEVEL_ALTO);
    const f32 rLo = probe(sdf::LEVEL_BAIXO);
    EXPECT_MSG(rHi - rLo > 20.0f,
               "sdf_toggle_baixo: o especular não aparece em Alto (r=%.0f "
               "vs Baixo r=%.0f)", rHi, rLo);
    // e a sombra simples do Baixo é MAIS SECA (soft 1.5 vs 3.5)
    EXPECT(lo.shadowSoft < hi.shadowSoft && lo.shadowDy < hi.shadowDy);
    sdf::g_level = sdf::LEVEL_ALTO;
}
