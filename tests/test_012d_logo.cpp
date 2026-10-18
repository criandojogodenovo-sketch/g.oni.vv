// tests/test_012d_logo.cpp — 0.12-D (D-4): O LOGO — o «G EM CHAMA».
//
// Os 4 pins da sub-fase D-4 (a identidade ÚNICA extraída do ícone real):
//   1. logo_rampa_viva   — cada traço leva a cor da RAMPA fire0..fire3
//                          amostrada pela ALTURA (a base escura, o bico
//                          quente; o mono âmbar do glifo antigo MORREU)
//   2. logo_ignite_porta — ignite 0..1 revela a rampa da BASE para o TOPO
//                          (0 = zero emissão; a D-6 anima o parâmetro)
//   3. logo_setas_mortas — AS SETAS MORRERAM: 16 segmentos nos dois LODs
//                          (era 28/20) e nenhum vértice além do G+capa
//                          (os eixos antigos viviam a r 8.2..11.0)
//   4. logo_lod_traco    — o LOD vive no TRAÇO (×1.2 no <32dp) — o que a
//                          contagem de segmentos já não vê
//
// MUTAÇÕES (coladas no RELATORIO-0.12-D.md §4):
//   M-D4a — a rampa morta (rampAt devolve o accent MONO — o fóssil) →
//           logo_rampa_viva vermelho
//   M-D4b — as setas de VOLTA (os 4 eixos desenhados outra vez) →
//           logo_setas_mortas vermelho
#include "TestFramework.h"
#include <GLES3/gl3.h>
#include "glstub_fb.h"
#include "render/Renderer.h"
#include "ui/Brand.h"
#include "ui/Theme.h"
#include "ui/UiContext.h"
#include <cmath>
#include <cstring>
#include <vector>

using namespace vv;

namespace {

constexpr int kW = 128;
constexpr int kH = 96;

// a amostra da RAMPA (a FÓRMULA DO PIN = a spec: fire0→fire1→fire2→fire3
// nos nós 0/1/2/3 lerp; o Brand.cpp tem de bater — a divergência é o pin
// vermelho)
void rampPin(f32 t, f32 out[4]) {
    const theme::Theme& th = theme::kTheme;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    if (t > 0.66f) {
        const f32 u = (t - 0.66f) / 0.34f;
        for (int c = 0; c < 3; ++c) {
            out[c] = th.fire2[c] + (th.fire3[c] - th.fire2[c]) * u;
        }
    } else if (t < 0.33f) {
        const f32 u = t / 0.33f;
        for (int c = 0; c < 3; ++c) {
            out[c] = th.fire0[c] + (th.fire1[c] - th.fire0[c]) * u;
        }
    } else {
        const f32 u = (t - 0.33f) / 0.33f;
        for (int c = 0; c < 3; ++c) {
            out[c] = th.fire1[c] + (th.fire2[c] - th.fire1[c]) * u;
        }
    }
    out[3] = 1.0f;
}

struct Harness {
    Renderer renderer;
    UiContext ui;
    u32 base = 0;   // os vértices ANTES do logo (o painel de fundo)
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
        base = ui.solidsForTest().vertexCount();
    }
    ~Harness() {
        ui.endFrame();
        renderer.endFrame();
        glstub::fb::enabled = false;
    }
};

struct QuadAcc {
    f32 yMean = 0.0f;
    f32 col[4] = {0, 0, 0, 0};
    f32 thick = 0.0f;      // |v0-v1| (a espessura do traço)
};
// agrupa os vértices do batch em quads (6) a partir de `from`
std::vector<QuadAcc> quadsOf(const UiContext& ui, u32 from) {
    const QuadBatch& b = ui.solidsForTest();
    const u32 n = b.vertexCount();
    std::vector<QuadAcc> out;
    for (u32 i = from; i + 5 < n; i += 6) {
        const auto* vs = b.vertices();
        QuadAcc q;
        f32 sy = 0.0f, sc[4] = {0, 0, 0, 0};
        for (u32 k = i; k < i + 6; ++k) {
            sy += vs[k].y;
            sc[0] += vs[k].r;
            sc[1] += vs[k].g;
            sc[2] += vs[k].b;
            sc[3] += vs[k].a;
        }
        q.yMean = sy / 6.0f;
        for (int c = 0; c < 4; ++c) {
            q.col[c] = sc[c] / 6.0f;
        }
        const f32 d0 = std::sqrt((vs[i].x - vs[i + 1].x) *
                                     (vs[i].x - vs[i + 1].x) +
                                 (vs[i].y - vs[i + 1].y) *
                                     (vs[i].y - vs[i + 1].y));
        q.thick = d0;
        out.push_back(q);
    }
    return out;
}

} // namespace

// ---- PIN 1 — logo_rampa_viva -------------------------------------------------
// «a identidade extraída do ícone real»: cada quad do glifo leva a cor da
// rampa na SUA altura (o t replicado da spec) e o mono âmbar do glifo
// antigo MORREU (nenhum quad accent exato; a base escurece, o topo acende).
TEST(logo_rampa_viva) {
    const f32 x0 = 24.0f, y0 = 16.0f, sz = 48.0f;   // 48dp → o LOD completo
    Harness h(nullptr);
    editor::brand::drawLogo(h.ui, x0, y0, sz, 1.0f);
    const std::vector<QuadAcc> qs = quadsOf(h.ui, h.base);
    EXPECT_MSG(qs.size() == 16u,
               "logo_rampa_viva: %zu quads, o G em chama é 16 segmentos "
               "(14 arco + 2 barra)", qs.size());
    const f32 k = sz / 24.0f;
    int hot = 0, dark = 0;
    for (const QuadAcc& q : qs) {
        const f32 yVB = (q.yMean - y0) / k;
        const f32 t = 1.0f - yVB / 24.0f;
        f32 want[4];
        rampPin(t, want);
        EXPECT_MSG(std::fabs(q.col[0] - want[0]) < 0.03f &&
                       std::fabs(q.col[1] - want[1]) < 0.03f &&
                       std::fabs(q.col[2] - want[2]) < 0.03f,
                   "logo_rampa_viva: o quad a yVB=%.1f (t=%.2f) leu "
                   "(%.3f,%.3f,%.3f), a rampa prevê (%.3f,%.3f,%.3f) — a "
                   "identidade não segue o ícone real",
                   yVB, t, q.col[0], q.col[1], q.col[2], want[0], want[1],
                   want[2]);
        const f32 lum =
            0.299f * q.col[0] + 0.587f * q.col[1] + 0.114f * q.col[2];
        if (lum > 0.62f) ++hot;      // quente (fire2/fire3)
        if (lum < 0.45f) ++dark;     // escuro (a base fire0→fire1)
    }
    EXPECT_MSG(hot > 0, "logo_rampa_viva: nenhum traço QUENTE no topo — a "
                        "chama não acende");
    EXPECT_MSG(dark > 0, "logo_rampa_viva: nenhum traço ESCURO na base — a "
                         "base do fogo sumiu");
}

// ---- PIN 2 — logo_ignite_porta -----------------------------------------------
// «a splash da D-6 anima o ignitar»: ignite=0 NÃO emite nada (apagada);
// ignite=0.35 revela a rampa SÓ nos traços com t<0.35 (a base apanha fogo
// primeiro, o resto fica no accentDim); o mesmo t dá o MESMO glifo
// (determinismo — a animação é paramétrica).
TEST(logo_ignite_porta) {
    const f32 x0 = 24.0f, y0 = 16.0f, sz = 48.0f;
    {
        Harness h(nullptr);
        editor::brand::drawLogo(h.ui, x0, y0, sz, 0.0f);
        EXPECT_MSG(h.ui.solidsForTest().vertexCount() == h.base,
                   "logo_ignite_porta: ignite=0 emitiu vértices — o glifo "
                   "apagado devia ser INVISÍVEL");
    }
    std::vector<QuadAcc> a, b;
    {
        Harness h(nullptr);
        editor::brand::drawLogo(h.ui, x0, y0, sz, 0.35f);
        a = quadsOf(h.ui, h.base);
    }
    {
        Harness h(nullptr);
        editor::brand::drawLogo(h.ui, x0, y0, sz, 0.35f);
        b = quadsOf(h.ui, h.base);
    }
    EXPECT_MSG(a.size() == 16u && b.size() == 16u,
               "logo_ignite_porta: o glifo parcial perdeu segmentos "
               "(%zu/%zu de 16)", a.size(), b.size());
    const f32 k = sz / 24.0f;
    int vivos = 0, apagados = 0;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
        // o determinismo: o mesmo t = o MESMO quad (a animação é pura)
        EXPECT_MSG(std::fabs(a[i].col[0] - b[i].col[0]) < 1e-5f,
                   "logo_ignite_porta: o glifo não é determinístico no "
                   "mesmo t (quad %zu)", i);
        const f32 t = 1.0f - ((a[i].yMean - y0) / k) / 24.0f;
        if (t < 0.35f) {
            ++vivos;
            f32 want[4];
            rampPin(t, want);
            EXPECT_MSG(std::fabs(a[i].col[1] - want[1]) < 0.03f,
                       "logo_ignite_porta: o quad vivo (t=%.2f) não leva a "
                       "rampa (g=%.3f, previsto %.3f)", t, a[i].col[1],
                       want[1]);
        } else {
            ++apagados;
            EXPECT_MSG(std::fabs(a[i].col[0] - theme::kTheme.accentDim[0]) <
                               1e-4f &&
                           std::fabs(a[i].col[1] -
                                     theme::kTheme.accentDim[1]) < 1e-4f,
                       "logo_ignite_porta: o quad apagado (t=%.2f) não é o "
                       "accentDim (r=%.3f)", t, a[i].col[0]);
        }
    }
    EXPECT_MSG(vivos > 0 && apagados > 0,
               "logo_ignite_porta: ignite=0.35 não dividiu o glifo "
               "(vivos=%d apagados=%d) — a porta não abre a meio", vivos,
               apagados);
}

// ---- PIN 3 — logo_setas_mortas ------------------------------------------------
// «os glifos antigos das setas fora»: 16 segmentos nos DOIS LODs (o glifo
// antigo tinha 28 no completo / 20 no simplificado) e NENHUM vértice além
// do G+capa (r ≤ 8.0 vb — os eixos das setas viviam a 8.2..11.0).
TEST(logo_setas_mortas) {
    EXPECT_MSG(editor::brand::segmentCountFor(24.0f) == 16u &&
                   editor::brand::segmentCountFor(32.0f) == 16u &&
                   editor::brand::segmentCountFor(96.0f) == 16u,
               "logo_setas_mortas: a contagem de segmentos não é 16 nos "
               "LODs — as setas voltaram?");
    const f32 szs[2] = {24.0f, 96.0f};   // o simplificado E o completo
    for (const f32 sz : szs) {
        Harness h(nullptr);
        editor::brand::drawLogo(h.ui, 16.0f, 8.0f, sz, 1.0f);
        const std::vector<QuadAcc> qs = quadsOf(h.ui, h.base);
        EXPECT_MSG(qs.size() == 16u,
                   "logo_setas_mortas: %zu quads a size=%.0f — as setas "
                   "(+12 segmentos no completo) parecem vivas", qs.size(),
                   sz);
        const f32 k = sz / 24.0f;
        const f32 cx = 16.0f + 12.0f * k, cy = 8.0f + 12.0f * k;
        // os VÉRTICES todos dentro do G+capa: r ≤ 8.0 vb (as setas antigas
        // chegavam a 11+capa)
        const QuadBatch& bd = h.ui.solidsForTest();
        const auto* vs = bd.vertices();
        for (u32 i = h.base; i + 5 < bd.vertexCount(); ++i) {
            const f32 dx = (vs[i].x - cx) / k;
            const f32 dy = (vs[i].y - cy) / k;
            const f32 r = std::sqrt(dx * dx + dy * dy);
            EXPECT_MSG(r <= 8.0f,
                       "logo_setas_mortas: um vértice a r=%.2f vb (o G+capa "
                       "termina a ~7.8; os eixos das setas começavam a 8.2) "
                       "— o glifo antigo sobreviveu", r);
        }
    }
}

// ---- PIN 4 — logo_lod_traco ---------------------------------------------------
// «o LOD vive no TRAÇO»: a espessura medida nos quads — 24dp desenha com
// ×1.2 (2.4dp) e 32dp com 1.0 (2.667dp) — o que a contagem de segmentos
// (igual nos dois, as setas mortas) já não distingue.
TEST(logo_lod_traco) {
    auto probe = [&](f32 sz) -> f32 {
        Harness h(nullptr);
        editor::brand::drawLogo(h.ui, 16.0f, 8.0f, sz, 1.0f);
        const std::vector<QuadAcc> qs = quadsOf(h.ui, h.base);
        EXPECT(qs.size() == 16u);
        return qs[0].thick;   // |v0-v1| = a espessura do traço
    };
    const f32 small = probe(24.0f);
    const f32 full = probe(32.0f);
    EXPECT_MSG(std::fabs(small - 2.4f) < 0.05f,
               "logo_lod_traco: o traço do pequeno mede %.3f (2.4 = "
               "24/12×1.2) — o LOD do traço morreu?", small);
    EXPECT_MSG(std::fabs(full - 32.0f / 12.0f) < 0.05f,
               "logo_lod_traco: o traço do completo mede %.3f (2.667 = "
               "32/12)", full);
    EXPECT_MSG(full > small,
               "logo_lod_traco: o completo (%.3f) devia ser MAIS GROSSO que "
               "o pequeno ×1.2 relativo (%.3f)", full, small);
}

