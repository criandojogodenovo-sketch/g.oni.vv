// ui/Brand.cpp — A MARCA (0.12-D · D-4): a implementação do «G em chama».
//
// A GEOMETRIA (viewBox 0..24, centro 12,12) — a MESMA de sempre:
//   • o G: arco r=6.2 com a abertura à DIREITA (gerado UMA vez — o mesmo
//     padrão dos geradores do Icons.cpp) + a barra horizontal até perto do
//     centro + o espelho vertical na ponta direita da barra.
// AS SETAS MORRERAM (o D-4: identidade única — o ícone real nunca as teve).
//
// A CHAMA: cada traço leva a cor da RAMPA DO FOGO do Theme amostrada pela
// ALTURA do seu ponto médio no viewBox (t: 0 na base = fire0 #7A1F0A, 1 no
// topo = fire3 #FFE08A — o que o ícone real faz). O `ignite` revela a
// rampa da BASE para o TOPO (t < ignite → a cor da rampa; t ≥ ignite → o
// mono accentDim APAGADO) — a D-6 anima o parâmetro na splash.
#include "ui/Brand.h"
#include "ui/UiContext.h"
#include "ui/Theme.h"

#include <cmath>

namespace vv {
namespace editor {
namespace brand {

namespace {

constexpr f32 kC = 12.0f;       // centro
constexpr f32 kGArcR = 6.2f;    // raio do arco do G
constexpr f32 kDeg = 0.01745329252f;

// o arco do G: 15 pontos (14 segmentos) de 55° a 305° passando por 180°
// (a abertura fica à direita, entre -55° e 55°)
f32 gArcPts[2 * 15] = {};
struct GArcInit {
    GArcInit() {
        const f32 a0 = 55.0f * kDeg;
        const f32 a1 = 305.0f * kDeg;
        for (int i = 0; i < 15; ++i) {
            const f32 a = a0 + (a1 - a0) * (static_cast<f32>(i) / 14.0f);
            gArcPts[2 * i]     = kC + kGArcR * std::cos(a);
            gArcPts[2 * i + 1] = kC + kGArcR * std::sin(a);
        }
    }
};
const GArcInit kGArcInit;

// a barra + o espelho do G (3 pontos, 2 segmentos). 0.12-D (D-4) — o
// ACHADO da auditoria: a tabela antiga tinha 4 pontos (o ponto 2
// REPETIA o ponto 0) e o loop desenhava 3 segmentos — o do meio
// RE-TRACEAVA a barra ao contrário (overdraw puro, um quad desperdiçado
// por frame; a contagem «2 segmentos» do header mentia — o Java já
// desenhava 2). A geometria certa:
constexpr f32 kGBarPts[] = {
    18.8f, 12.0f,   11.4f, 12.0f,   // a barra (entra do lado da abertura)
    18.8f, 7.6f,                    // o espelho vertical (sobe do fim da barra)
};

// a amostra da RAMPA do Theme pela ALTURA t 0..1 (0 = fire0 na base,
// 1 = fire3 no topo; 3 lerp pelos 4 pontos — o MESMO gradiente do
// drawEmbers da D-3, a MESMA família do ícone real)
void rampAt(f32 t, f32 out[4]) {
    const theme::Theme& th = theme::kTheme;
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    if (t > 0.66f) {
        const f32 u = (t - 0.66f) / 0.34f;
        for (int c = 0; c < 3; ++c) {
            out[c] = th.fire2[c] + (th.fire3[c] - th.fire2[c]) * u;
        }
    } else {
        const f32 u = t / 0.66f;
        // fire0 → fire1 (0..0.33) → fire2 (0.33..0.66) — o lerp em 2 nós
        if (u < 0.5f) {
            const f32 v = u / 0.5f;
            for (int c = 0; c < 3; ++c) {
                out[c] = th.fire0[c] + (th.fire1[c] - th.fire0[c]) * v;
            }
        } else {
            const f32 v = (u - 0.5f) / 0.5f;
            for (int c = 0; c < 3; ++c) {
                out[c] = th.fire1[c] + (th.fire2[c] - th.fire1[c]) * v;
            }
        }
    }
    out[3] = 1.0f;
}

// a cor de um traço: t = a altura normalizada do ponto médio (0 base →
// 1 topo); ignite revela da base para o topo (t < ignite → a rampa;
// fora → o APAGADO accentDim)
void strokeColor(f32 tMid, f32 ignite, f32 out[4]) {
    if (tMid < ignite) {
        rampAt(tMid, out);
    } else {
        out[0] = theme::kTheme.accentDim[0];
        out[1] = theme::kTheme.accentDim[1];
        out[2] = theme::kTheme.accentDim[2];
        out[3] = theme::kTheme.accentDim[3];
    }
}

} // namespace

void drawLogo(UiContext& ui, f32 x, f32 y, f32 size, f32 ignite) {
    if (size < 4.0f || ignite <= 0.0f) {
        return;   // degenerado — nada a desenhar (ignite 0 = nem o esqueleto)
    }
    if (ignite > 1.0f) ignite = 1.0f;
    const f32 k = size / 24.0f;
    // o LOD pelo TAMANHO PEDIDO em dp (o size do chamador é px de ecrã):
    // ≥32dp = traço 1.0 · <32dp = traço ×1.2 (o peso no pequeno)
    const f32 dp1 = theme::dp(1.0f);
    const f32 sizeDp = dp1 > 0.0f ? size / dp1 : size;
    const f32 t = size / 12.0f * (sizeDp >= kLodFullMinDp ? 1.0f : 1.2f);
    f32 col[4];

    // o G (arco gerado + barra/espelho) — a cor por traço pela ALTURA
    for (int i = 1; i < 15; ++i) {
        const f32 yMid =
            (gArcPts[2 * (i - 1) + 1] + gArcPts[2 * i + 1]) * 0.5f;
        strokeColor(1.0f - yMid / 24.0f, ignite, col);
        ui.drawLine(x + gArcPts[2 * (i - 1)] * k,
                    y + gArcPts[2 * (i - 1) + 1] * k,
                    x + gArcPts[2 * i] * k, y + gArcPts[2 * i + 1] * k, t,
                    col);
    }
    for (int i = 1; i < 3; ++i) {
        const f32 yMid =
            (kGBarPts[2 * (i - 1) + 1] + kGBarPts[2 * i + 1]) * 0.5f;
        strokeColor(1.0f - yMid / 24.0f, ignite, col);
        ui.drawLine(x + kGBarPts[2 * (i - 1)] * k,
                    y + kGBarPts[2 * (i - 1) + 1] * k,
                    x + kGBarPts[2 * i] * k, y + kGBarPts[2 * i + 1] * k, t,
                    col);
    }
}

u32 segmentCountFor(f32 sizeDp) {
    // G: 14 (arco) + 2 (barra/espelho) — AS SETAS MORRERAM nos dois LODs
    (void)sizeDp;
    return 14u + 2u;
}

} // namespace brand
} // namespace editor
} // namespace vv
