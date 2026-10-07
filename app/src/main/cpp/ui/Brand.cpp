// ui/Brand.cpp — A MARCA (0.9.6.18 · HOTFIX D6): a implementação do glifo.
//
// A GEOMETRIA (viewBox 0..24, centro 12,12):
//   • o G: arco r=6.2 com a abertura à DIREITA (gerado UMA vez — o mesmo
//     padrão dos geradores do Icons.cpp) + a barra horizontal até perto do
//     centro + o espelho vertical na ponta direita da barra;
//   • as 4 setas (LOD completo): eixo radial r 8.2→11.0 a N/E/S/W com
//     pontas em V (2 traços a ±32°) — DENTRO do viewBox (o tip 23.0 +
//     meio-traço 1.0 = 24.0 exato, nada clipa);
//   • os 4 ticks (LOD simplificado): os MESMOS eixos sem ponta.
// O traço é o do conjunto (size/12) — a versão simplificada desenha com
// ×1.2 (a spec: «traço ~20% mais grosso»).
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

// a barra + o espelho do G (3 pontos, 2 segmentos)
constexpr f32 kGBarPts[] = {
    18.8f, 12.0f,   11.4f, 12.0f,   // a barra (entra do lado da abertura)
    18.8f, 12.0f,   18.8f, 7.6f,    // o espelho vertical (sobe do fim)
};

// as 4 setas: ângulos N/E/S/W (ecrã: 270°=cima, 0°=direita, 90°=baixo,
// 180°=esquerda). Cada seta = eixo (2 pts) + 2 traços de ponta.
constexpr f32 kArrowAng[4] = {270.0f, 0.0f, 90.0f, 180.0f};
constexpr f32 kShaftIn = 8.2f;    // raio onde o eixo nasce
constexpr f32 kTip = 11.0f;       // raio da PONTA (11+meio-traço = 24 exato)
constexpr f32 kHeadBack = 2.0f;   // recuo da ponta (raio)
constexpr f32 kHeadHalf = 32.0f * kDeg;   // meia-abertura da ponta

} // namespace

void drawIcon(UiContext& ui, f32 x, f32 y, f32 size, const f32 color[4]) {
    if (size < 4.0f) {
        return;   // degenerado — nada a desenhar
    }
    const f32 k = size / 24.0f;
    // o LOD pelo TAMANHO PEDIDO em dp (o size do chamador é px de ecrã):
    // ≥32dp = completa · <32dp = simplificada (traço ×1.2)
    const f32 dp1 = theme::dp(1.0f);
    const f32 sizeDp = dp1 > 0.0f ? size / dp1 : size;
    const bool full = sizeDp >= kLodFullMinDp;
    const f32 t = size / 12.0f * (full ? 1.0f : 1.2f);

    // o G (arco gerado + barra/espelho)
    for (int i = 1; i < 15; ++i) {
        ui.drawLine(x + gArcPts[2 * (i - 1)] * k, y + gArcPts[2 * (i - 1) + 1] * k,
                    x + gArcPts[2 * i] * k, y + gArcPts[2 * i + 1] * k, t,
                    color);
    }
    for (int i = 1; i < 4; ++i) {
        ui.drawLine(x + kGBarPts[2 * (i - 1)] * k, y + kGBarPts[2 * (i - 1) + 1] * k,
                    x + kGBarPts[2 * i] * k, y + kGBarPts[2 * i + 1] * k, t,
                    color);
    }

    // as 4 setas (com pontas) ou os 4 ticks (simplificado)
    for (int a = 0; a < 4; ++a) {
        const f32 ang = kArrowAng[a];
        const f32 ca = std::cos(ang * kDeg), sa = std::sin(ang * kDeg);
        const f32 ax0 = kC + kShaftIn * ca, ay0 = kC + kShaftIn * sa;
        const f32 ax1 = kC + kTip * ca,     ay1 = kC + kTip * sa;
        ui.drawLine(x + ax0 * k, y + ay0 * k, x + ax1 * k, y + ay1 * k, t,
                    color);
        if (full) {
            // as PONTAS: 2 traços do tip para dentro a ±32°
            for (int s = -1; s <= 1; s += 2) {
                const f32 ha = ang * kDeg + static_cast<f32>(s) * kHeadHalf;
                const f32 hx = kC + (kTip - kHeadBack) * std::cos(ha);
                const f32 hy = kC + (kTip - kHeadBack) * std::sin(ha);
                ui.drawLine(x + ax1 * k, y + ay1 * k, x + hx * k, y + hy * k,
                            t, color);
            }
        }
    }
}

u32 segmentCountFor(f32 sizeDp) {
    // G: 14 (arco) + 2 (barra/espelho) · setas completas: 4 eixos + 8 pontas
    //    simplificadas: 4 ticks
    return sizeDp >= kLodFullMinDp ? (14u + 2u + 12u) : (14u + 2u + 4u);
}

} // namespace brand
} // namespace editor
} // namespace vv
