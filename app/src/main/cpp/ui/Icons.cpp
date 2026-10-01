// ui/Icons.cpp — geometria dos 8 ícones da toolbar (0.7.6).
//
// Tudo em unidades de design 0..24 (viewBox), segmentos RETOS por linha
// (a "curva" do Rodar é uma polilinha de 10 segmentos — matemática no
// código, nada orgânico). O desenho mapeia para o rect do botão e emite
// no line batch (UiContext::drawLine) com espessura uniforme.
#include "ui/Icons.h"
#include "ui/UiContext.h"

#include <cmath>

namespace vv {
namespace icons {

namespace {

// ---- Mover: duas setas cruzadas perpendiculares (↑ e →) --------------------
// seta vertical (aponta PARA CIMA) + seta horizontal (aponta PARA A DIREITA),
// cruzadas no centro (12,12).
constexpr f32 kMovePts[] = {
    // seta ↑ : haste + ponta (2 traços)
    12.0f, 20.0f,  12.0f, 4.0f,     // haste
    12.0f, 4.0f,   8.6f, 7.8f,      // ponta esq
    12.0f, 4.0f,   15.4f, 7.8f,     // ponta dir
    // seta → : haste + ponta (2 traços)
    4.0f, 12.0f,   20.0f, 12.0f,    // haste
    20.0f, 12.0f,  16.2f, 8.6f,     // ponta cima
    20.0f, 12.0f,  16.2f, 15.4f,    // ponta baixo
};
constexpr Polyline kMoveLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
};

// ---- Rodar: seta circular parcial em volta de eixo central ------------------
// arco de ~250° (polilinha de 11 segmentos, raio 8.4) + ponta da seta no
// fim + eixo central vertical curto. Os pontos são GERADOS no arranque
// (RotArcInit abaixo — cos/sin não são constexpr): [0..11] arco,
// [12..13]+[14..15] ponta, [16..17] eixo.
f32 kRotatePts[2 * 18] = {};
constexpr Polyline kRotateLines[] = {
    {0, 12},   // arco (11 segmentos)
    {12, 2},   // ponta esq
    {14, 2},   // ponta dir
    {16, 2},   // eixo central
};

// ---- Escalar: seta diagonal dupla de/para um canto ---------------------------
// diagonal do canto inf-esq ao sup-dir com pontas nas DUAS extremidades.
constexpr f32 kScalePts[] = {
    5.0f, 19.0f,   19.0f, 5.0f,    // diagonal
    19.0f, 5.0f,   14.6f, 5.0f,    // ponta NE (traço horizontal)
    19.0f, 5.0f,   19.0f, 9.4f,    // ponta NE (traço vertical)
    5.0f, 19.0f,   9.4f, 19.0f,    // ponta SO (horizontal)
    5.0f, 19.0f,   5.0f, 14.6f,    // ponta SO (vertical)
};
constexpr Polyline kScaleLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2},
};

// ---- Snap: ímã geométrico (⊥ em U angular + os dois polos) ------------------
constexpr f32 kSnapPts[] = {
    // U angular: braço esq desce, fundo em dois joelhos, braço dir sobe
    7.0f, 3.6f,   7.0f, 12.4f,
    7.0f, 12.4f,  9.2f, 15.0f,
    9.2f, 15.0f,  14.8f, 15.0f,
    14.8f, 15.0f, 17.0f, 12.4f,
    17.0f, 12.4f, 17.0f, 3.6f,
    // polos: dois traços horizontais no TOPO dos braços (as faces do ímã)
    5.2f, 6.6f,   8.8f, 6.6f,
    15.2f, 6.6f,  18.8f, 6.6f,
};
constexpr Polyline kSnapLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2},
    {10, 2}, {12, 2},
};

// ---- Inspector: retângulo + 3 linhas internas -------------------------------
constexpr f32 kInspectorPts[] = {
    5.0f, 4.0f,   19.0f, 4.0f,     // moldura (1 polilinha fechada)
    19.0f, 4.0f,  19.0f, 20.0f,
    19.0f, 20.0f, 5.0f, 20.0f,
    5.0f, 20.0f,  5.0f, 4.0f,
    8.0f, 9.0f,   16.0f, 9.0f,     // linha 1 (título)
    8.0f, 13.0f,  16.0f, 13.0f,    // linha 2
    8.0f, 17.0f,  13.2f, 17.0f,    // linha 3 (curta)
};
constexpr Polyline kInspectorLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2},
    {8, 2}, {10, 2}, {12, 2},
};

// ---- Cena: camadas/pilha — 2 losangos sobrepostos ---------------------------
constexpr f32 kScenePts[] = {
    12.0f, 3.6f,   20.0f, 8.4f,    // camada de cima
    20.0f, 8.4f,   12.0f, 13.2f,
    12.0f, 13.2f,  4.0f, 8.4f,
    4.0f, 8.4f,    12.0f, 3.6f,
    12.0f, 10.8f,  20.0f, 15.6f,   // camada de baixo (por baixo/à frente)
    20.0f, 15.6f,  12.0f, 20.4f,
    12.0f, 20.4f,  4.0f, 15.6f,
    4.0f, 15.6f,   12.0f, 10.8f,
};
constexpr Polyline kSceneLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2},
    {8, 2}, {10, 2}, {12, 2}, {14, 2},
};

// ---- Play: triângulo ---------------------------------------------------------
constexpr f32 kPlayPts[] = {
    8.4f, 5.0f,   18.4f, 12.0f,
    18.4f, 12.0f, 8.4f, 19.0f,
    8.4f, 19.0f,  8.4f, 5.0f,
};
constexpr Polyline kPlayLines[] = {
    {0, 2}, {2, 2}, {4, 2},
};

// ---- Pause: duas barras verticais ---------------------------------------------
constexpr f32 kPausePts[] = {
    7.2f, 5.0f,   7.2f, 19.0f,
    16.8f, 5.0f,  16.8f, 19.0f,
};
constexpr Polyline kPauseLines[] = {
    {0, 2}, {2, 2},
};

// tabela final (índice = Icon)
const IconDef kDefs[] = {
    {kMovePts,       12, kMoveLines,       6},
    {kRotatePts,     18, kRotateLines,     4},
    {kScalePts,      10, kScaleLines,      5},
    {kSnapPts,       14, kSnapLines,       7},
    {kInspectorPts,  14, kInspectorLines,  7},
    {kScenePts,      16, kSceneLines,      8},
    {kPlayPts,        6, kPlayLines,       3},
    {kPausePts,       4, kPauseLines,      2},
};
static_assert(sizeof(kDefs) / sizeof(kDefs[0]) ==
              static_cast<size_t>(Icon::Count), "tabela de ícones incompleta");

// o arco do Rodar é gerado UMA vez (constexpr não teria std::cos) — corre
// no arranque do TU, antes de main()/testes. Ângulos com y do viewBox para
// BAIXO (crescem no sentido dos ponteiros na tela); a PONTA da seta fica
// dentro do viewBox (canto inferior-esquerdo) — aferido no CI.
struct RotArcInit {
    RotArcInit() {
        // arco: centro (12,12), raio 8.2, de -55° a 180°
        const f32 a0 = -55.0f * 0.01745329252f;
        const f32 a1 = 180.0f * 0.01745329252f;
        const f32 r = 8.2f;
        for (int i = 0; i < 12; ++i) {
            const f32 a = a0 + (a1 - a0) * (static_cast<f32>(i) / 11.0f);
            kRotatePts[2 * i]     = 12.0f + r * std::cos(a);
            kRotatePts[2 * i + 1] = 12.0f + r * std::sin(a);
        }
        // ponta da seta no FIM do arco (a1 = 180°, lado esquerdo): dois
        // traços de (tip - t*head) ± n*head — t = tangente no fim, n = sua
        // perpendicular. Com a1=180° o resultado fica (0.8..6.8, 15.0) —
        // DENTRO do viewBox 0..24 (o teste aferia falhas anteriores)
        const f32 tipx = 12.0f + r * std::cos(a1);
        const f32 tipy = 12.0f + r * std::sin(a1);
        const f32 tx = -std::sin(a1), ty = std::cos(a1);
        const f32 nx = -ty, ny = tx;
        const f32 head = 3.0f;
        kRotatePts[24] = tipx - tx * head + nx * head;
        kRotatePts[25] = tipy - ty * head + ny * head;
        kRotatePts[26] = tipx;
        kRotatePts[27] = tipy;
        kRotatePts[28] = tipx - tx * head - nx * head;
        kRotatePts[29] = tipy - ty * head - ny * head;
        kRotatePts[30] = tipx;
        kRotatePts[31] = tipy;
        // eixo central (traço vertical curto — o eixo em volta do qual se
        // roda; estava no initializer antigo e passa a ser escrito AQUI)
        kRotatePts[32] = 12.0f;
        kRotatePts[33] = 8.6f;
        kRotatePts[34] = 12.0f;
        kRotatePts[35] = 15.4f;
    }
};
const RotArcInit kRotArcInit;

} // namespace

const IconDef& def(Icon icon) {
    const u32 i = static_cast<u32>(icon);
    return i < static_cast<u32>(Icon::Count) ? kDefs[i] : kDefs[0];
}

u32 segmentCount(Icon icon) {
    const IconDef& d = def(icon);
    u32 n = 0;
    for (u32 i = 0; i < d.nLines; ++i) {
        if (d.lines[i].count >= 2) {
            n += d.lines[i].count - 1;
        }
    }
    return n;
}

f32 totalLength(Icon icon) {
    const IconDef& d = def(icon);
    f32 len = 0.0f;
    for (u32 i = 0; i < d.nLines; ++i) {
        const Polyline& p = d.lines[i];
        for (u16 k = p.first + 1; k < p.first + p.count; ++k) {
            const f32 dx = d.pts[2 * k] - d.pts[2 * (k - 1)];
            const f32 dy = d.pts[2 * k + 1] - d.pts[2 * (k - 1) + 1];
            len += std::sqrt(dx * dx + dy * dy);
        }
    }
    return len;
}

void drawIcon(UiContext& ui, Icon icon, f32 x, f32 y, f32 size,
              const f32 color[4]) {
    if (size < 2.0f) {
        return;   // degenerado — nada a desenhar
    }
    const IconDef& d = def(icon);
    const f32 k = size / 24.0f;
    // espessura UNIFORME: ~2 px num ícone de 24 (regra do conjunto)
    const f32 thickness = size / 12.0f;
    const f32 over = thickness * 0.5f;   // o traço sangra meio-width
    for (u32 i = 0; i < d.nLines; ++i) {
        const Polyline& p = d.lines[i];
        for (u16 q = p.first + 1; q < p.first + p.count; ++q) {
            const f32 ax = x + d.pts[2 * (q - 1)] * k;
            const f32 ay = y + d.pts[2 * (q - 1) + 1] * k;
            const f32 bx = x + d.pts[2 * q] * k;
            const f32 by = y + d.pts[2 * q + 1] * k;
            ui.drawLine(ax, ay, bx, by, thickness, color);
        }
    }
    (void)over;
}

} // namespace icons
} // namespace vv
