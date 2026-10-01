#pragma once
// ui/Icons.h — 8 ÍCONES VETORIAIS PRÓPRIOS da toolbar (0.7.6).
//
// Cada ícone é um conjunto de POLILINHAS em viewBox 0..24 (unidades de
// design), desenhadas pelo LINE BATCH existente (UiContext::drawLine — o
// MESMO dos gizmos 3D projetados). SEM parser de SVG, SEM raster, SEM
// atlas novo: geometria pura → segmentos de ecrã com espessura uniforme.
// (SVGs fonte em assets/ serviriam só como referência de design — a
// geometria vive AQUI, em código, aferível no CI.)
//
// O CONJUNTO (esp. 0.7.6):
//   Mover     — duas setas cruzadas perpendiculares (↑→)
//   Rodar     — seta circular parcial em volta de eixo central
//   Escalar   — seta diagonal dupla de/para um canto
//   Snap      — ímã geométrico (só segmentos retos, zero curvas orgânicas)
//   Inspector — retângulo com 3 linhas horizontais internas
//   Cena      — camadas/pilha (2 losangos sobrepostos)
//   Play      — triângulo
//   Pause     — duas barras verticais
//
// REGRAS (aferidas no CI):
//   • mesmo peso visual entre os 8 (contagem/comprimento de traços
//     comparáveis; o triângulo do Play compensa com massa);
//   • geometria simples, legível a 24/32 px;
//   • stroke-width UNIFORME dentro de cada ícone (thickness = size/12);
//   • TODOS os pontos dentro do viewBox 0..24 → o ícone desenhado num rect
//     nunca lhe sai (testado em 24 e 32 px);
//   • cor de marca (theme::kTheme.brand) sobre a barra; estado ativo
//     inverte (fundo brand + ícone brandInk) — isso é da TOOLBAR, não daqui.
#include "core/Types.h"

namespace vv {

class UiContext;

namespace icons {

enum class Icon : u8 {
    Move = 0,
    Rotate,
    Scale,
    Snap,
    Inspector,
    Scene,
    Play,
    Pause,
    Count
};

// uma polilinha = range [first, first+count) no array de pontos do ícone
struct Polyline {
    u16 first;   // índice do primeiro ponto
    u16 count;   // nº de pontos (≥2; count-1 segmentos)
};

// definição geométrica de um ícone (pares x,y em unidades 0..24)
struct IconDef {
    const f32*        pts;      // [2*nPts] (x0,y0, x1,y1, …)
    u32               nPts;
    const Polyline*   lines;
    u32               nLines;
};

// a definição de cada ícone (FONTE ÚNICA — desenho e testes partilham)
const IconDef& def(Icon icon);

// nº de SEGMENTOS do ícone (soma das polilinhas)
u32 segmentCount(Icon icon);

// comprimento TOTAL das polilinhas em unidades de design (peso visual)
f32 totalLength(Icon icon);

// desenha o ícone centrado num quadrado (x,y,size) — mapeia 0..24 → rect,
// espessura uniforme thickness = size/12, cor dada. Emite no line batch do
// UiContext (drawLine). Nada de clip/estado — puro immediate mode.
void drawIcon(UiContext& ui, Icon icon, f32 x, f32 y, f32 size,
              const f32 color[4]);

// transformação de um ponto do viewBox para o rect (partilhado com testes)
inline f32 mapX(f32 dx, f32 x, f32 size) { return x + dx * (size / 24.0f); }
inline f32 mapY(f32 dy, f32 y, f32 size) { return y + dy * (size / 24.0f); }

} // namespace icons
} // namespace vv
