// ui/Icons.cpp — geometria do CONJUNTO ÚNICO DE ÍCONES OUTLINE (0.9.0).
//
// Tudo em unidades de design 0..24 (viewBox), segmentos RETOS por linha
// (as "curvas" são polilinhas de N segmentos — matemática no código, nada
// orgânico; Gear/Spinner/arcos são GERADOS no arranque do TU). O desenho
// mapeia para o rect do alvo e emite no line batch (drawLine) com espessura
// uniforme thickness = size/12 (traço 2dp num ícone de 24dp).
#include "ui/Icons.h"
#include "ui/UiContext.h"

#include <cmath>
#include <cstring>

namespace vv {
namespace icons {

namespace {

// ---- Mover: duas setas cruzadas perpendiculares (↑ e →) --------------------
constexpr f32 kMovePts[] = {
    12.0f, 20.0f,  12.0f, 4.0f,     // haste ↑
    12.0f, 4.0f,   8.6f, 7.8f,      // ponta esq
    12.0f, 4.0f,   15.4f, 7.8f,     // ponta dir
    4.0f, 12.0f,   20.0f, 12.0f,    // haste →
    20.0f, 12.0f,  16.2f, 8.6f,     // ponta cima
    20.0f, 12.0f,  16.2f, 15.4f,    // ponta baixo
};
constexpr Polyline kMoveLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
};

// ---- Rodar: arco gerado (RotArcInit) + ponta + eixo ------------------------
f32 kRotatePts[2 * 18] = {};
constexpr Polyline kRotateLines[] = {
    {0, 12}, {12, 2}, {14, 2}, {16, 2},
};

// ---- Escalar: diagonal dupla ------------------------------------------------
constexpr f32 kScalePts[] = {
    5.0f, 19.0f,   19.0f, 5.0f,
    19.0f, 5.0f,   14.6f, 5.0f,
    19.0f, 5.0f,   19.0f, 9.4f,
    5.0f, 19.0f,   9.4f, 19.0f,
    5.0f, 19.0f,   5.0f, 14.6f,
};
constexpr Polyline kScaleLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2},
};

// ---- Snap: ímã geométrico ---------------------------------------------------
constexpr f32 kSnapPts[] = {
    7.0f, 3.6f,   7.0f, 12.4f,
    7.0f, 12.4f,  9.2f, 15.0f,
    9.2f, 15.0f,  14.8f, 15.0f,
    14.8f, 15.0f, 17.0f, 12.4f,
    17.0f, 12.4f, 17.0f, 3.6f,
    5.2f, 6.6f,   8.8f, 6.6f,
    15.2f, 6.6f,  18.8f, 6.6f,
};
constexpr Polyline kSnapLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2},
    {10, 2}, {12, 2},
};

// ---- Inspector: retângulo + 3 linhas ----------------------------------------
constexpr f32 kInspectorPts[] = {
    5.0f, 4.0f,   19.0f, 4.0f,
    19.0f, 4.0f,  19.0f, 20.0f,
    19.0f, 20.0f, 5.0f, 20.0f,
    5.0f, 20.0f,  5.0f, 4.0f,
    8.0f, 9.0f,   16.0f, 9.0f,
    8.0f, 13.0f,  16.0f, 13.0f,
    8.0f, 17.0f,  13.2f, 17.0f,
};
constexpr Polyline kInspectorLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2},
    {8, 2}, {10, 2}, {12, 2},
};

// ---- Cena: camadas (2 losangos) ---------------------------------------------
constexpr f32 kScenePts[] = {
    12.0f, 3.6f,   20.0f, 8.4f,
    20.0f, 8.4f,   12.0f, 13.2f,
    12.0f, 13.2f,  4.0f, 8.4f,
    4.0f, 8.4f,    12.0f, 3.6f,
    12.0f, 10.8f,  20.0f, 15.6f,
    20.0f, 15.6f,  12.0f, 20.4f,
    12.0f, 20.4f,  4.0f, 15.6f,
    4.0f, 15.6f,   12.0f, 10.8f,
};
constexpr Polyline kSceneLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2},
    {8, 2}, {10, 2}, {12, 2}, {14, 2},
};

// ---- Play / Pause -----------------------------------------------------------
constexpr f32 kPlayPts[] = {
    8.4f, 5.0f,   18.4f, 12.0f,
    18.4f, 12.0f, 8.4f, 19.0f,
    8.4f, 19.0f,  8.4f, 5.0f,
};
constexpr Polyline kPlayLines[] = {
    {0, 2}, {2, 2}, {4, 2},
};
constexpr f32 kPausePts[] = {
    7.2f, 5.0f,   7.2f, 19.0f,
    16.8f, 5.0f,  16.8f, 19.0f,
};
constexpr Polyline kPauseLines[] = {
    {0, 2}, {2, 2},
};

// ---- 0.9.0: Hamburger ☰ -----------------------------------------------------
constexpr f32 kHamburgerPts[] = {
    4.0f, 6.5f,   20.0f, 6.5f,
    4.0f, 12.0f,  20.0f, 12.0f,
    4.0f, 17.5f,  20.0f, 17.5f,
};
constexpr Polyline kHamburgerLines[] = {
    {0, 2}, {2, 2}, {4, 2},
};

// ---- ChevronDown v / ChevronRight > -----------------------------------------
constexpr f32 kChevronDownPts[] = {
    7.0f, 9.0f,   12.0f, 14.5f,   17.0f, 9.0f,
};
constexpr Polyline kChevronDownLines[] = {
    {0, 3},
};
constexpr f32 kChevronRightPts[] = {
    9.0f, 6.5f,   14.5f, 12.0f,   9.0f, 17.5f,
};
constexpr Polyline kChevronRightLines[] = {
    {0, 3},
};

// ---- Sliders (3 linhas + knobs defasados) -----------------------------------
constexpr f32 kSlidersPts[] = {
    4.0f, 6.5f,   20.0f, 6.5f,    // linha 1
    9.0f, 4.2f,   9.0f, 8.8f,     // knob 1
    4.0f, 12.0f,  20.0f, 12.0f,   // linha 2
    15.0f, 9.7f,  15.0f, 14.3f,   // knob 2
    4.0f, 17.5f,  20.0f, 17.5f,   // linha 3
    6.5f, 15.2f,  6.5f, 19.8f,    // knob 3
};
constexpr Polyline kSlidersLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
};

// ---- Gear (GERADO — GearInit: coroa 16 dentes + cubo octogonal) --------------
// [0..16] coroa: ângulo i·22,5°, raio alterna 9.6 (dente) / 7.4 (vale)
// [17..25] cubo: octógono r=3.2 fechado
f32 kGearPts[2 * 26] = {};
constexpr Polyline kGearLines[] = {
    {0, 17},   // coroa fechada
    {17, 9},   // cubo fechado
};

// ---- Cube (modo 3D) ----------------------------------------------------------
constexpr f32 kCubePts[] = {
    12.0f, 3.5f,  19.5f, 8.0f,    // silhueta hexagonal fechada
    19.5f, 8.0f,  19.5f, 16.0f,
    19.5f, 16.0f, 12.0f, 20.5f,
    12.0f, 20.5f, 4.5f, 16.0f,
    4.5f, 16.0f,  4.5f, 8.0f,
    4.5f, 8.0f,   12.0f, 3.5f,
    4.5f, 8.0f,   12.0f, 12.5f,   // aresta esq → centro
    12.0f, 12.5f, 19.5f, 8.0f,    // centro → aresta dir
    12.0f, 12.5f, 12.0f, 20.5f,   // centro → fundo
};
constexpr Polyline kCubeLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
    {12, 2}, {14, 2}, {16, 2},
};

// ---- Monitor (modo UI) --------------------------------------------------------
constexpr f32 kMonitorPts[] = {
    4.0f, 5.0f,   20.0f, 5.0f,    // ecrã fechado
    20.0f, 5.0f,  20.0f, 15.5f,
    20.0f, 15.5f, 4.0f, 15.5f,
    4.0f, 15.5f,  4.0f, 5.0f,
    12.0f, 15.5f, 12.0f, 18.5f,   // pé
    8.5f, 19.5f,  15.5f, 19.5f,   // base
};
constexpr Polyline kMonitorLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
};

// ---- Speaker (modo ÁUDIO) ----------------------------------------------------
constexpr f32 kSpeakerPts[] = {
    5.0f, 9.5f,   9.0f, 9.5f,     // cone fechado
    9.0f, 9.5f,   14.0f, 5.0f,
    14.0f, 5.0f,  14.0f, 19.0f,
    14.0f, 19.0f, 9.0f, 14.5f,
    9.0f, 14.5f,  5.0f, 14.5f,
    5.0f, 14.5f,  5.0f, 9.5f,
    17.0f, 9.0f,  19.5f, 12.0f,   // onda 1 (3 pts)
    19.5f, 12.0f, 17.0f, 15.0f,
};
constexpr Polyline kSpeakerLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
    {12, 3},
};

// ---- Plus --------------------------------------------------------------------
constexpr f32 kPlusPts[] = {
    12.0f, 5.0f,   12.0f, 19.0f,
    5.0f, 12.0f,   19.0f, 12.0f,
};
constexpr Polyline kPlusLines[] = {
    {0, 2}, {2, 2},
};

// ---- Eye / EyeOff -------------------------------------------------------------
constexpr f32 kEyePts[] = {
    4.0f, 12.0f,  6.5f, 8.5f,     // amêndoa fechada
    6.5f, 8.5f,   12.0f, 7.0f,
    12.0f, 7.0f,  17.5f, 8.5f,
    17.5f, 8.5f,  20.0f, 12.0f,
    20.0f, 12.0f, 17.5f, 15.5f,
    17.5f, 15.5f, 12.0f, 17.0f,
    12.0f, 17.0f, 6.5f, 15.5f,
    6.5f, 15.5f,  4.0f, 12.0f,
    9.7f, 12.0f,  10.5f, 10.7f,   // pupila (octógono fechado)
    10.5f, 10.7f, 13.5f, 10.7f,
    13.5f, 10.7f, 14.3f, 12.0f,
    14.3f, 12.0f, 13.5f, 13.3f,
    13.5f, 13.3f, 10.5f, 13.3f,
    10.5f, 13.3f, 9.7f, 12.0f,
};
constexpr Polyline kEyeLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2}, {12, 2}, {14, 2},
    {16, 2}, {18, 2}, {20, 2}, {22, 2}, {24, 2}, {26, 2},
};
constexpr f32 kEyeOffPts[] = {
    4.0f, 12.0f,  6.5f, 8.5f,     // amêndoa igual
    6.5f, 8.5f,   12.0f, 7.0f,
    12.0f, 7.0f,  17.5f, 8.5f,
    17.5f, 8.5f,  20.0f, 12.0f,
    20.0f, 12.0f, 17.5f, 15.5f,
    17.5f, 15.5f, 12.0f, 17.0f,
    12.0f, 17.0f, 6.5f, 15.5f,
    6.5f, 15.5f,  4.0f, 12.0f,
    4.5f, 4.5f,   19.5f, 19.5f,   // traço diagonal (cortado)
};
constexpr Polyline kEyeOffLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2}, {12, 2}, {14, 2},
    {16, 2},
};

// ---- Dots ⋮ -------------------------------------------------------------------
constexpr f32 kDotsPts[] = {
    12.0f, 5.5f,   12.0f, 7.0f,
    12.0f, 11.25f, 12.0f, 12.75f,
    12.0f, 17.0f,  12.0f, 18.5f,
};
constexpr Polyline kDotsLines[] = {
    {0, 2}, {2, 2}, {4, 2},
};

// ---- Person (cabeça + ombros) --------------------------------------------------
constexpr f32 kPersonPts[] = {
    9.2f, 7.5f,   10.2f, 5.0f,    // cabeça (octógono fechado r≈3)
    10.2f, 5.0f,  13.8f, 5.0f,
    13.8f, 5.0f,  14.8f, 7.5f,
    14.8f, 7.5f,  13.8f, 10.0f,
    13.8f, 10.0f, 10.2f, 10.0f,
    10.2f, 10.0f, 9.2f, 7.5f,
    5.0f, 20.0f,  5.0f, 17.5f,    // ombros (polilinha aberta)
    5.0f, 17.5f,  6.5f, 14.5f,
    6.5f, 14.5f,  9.5f, 13.0f,
    9.5f, 13.0f,  14.5f, 13.0f,
    14.5f, 13.0f, 17.5f, 14.5f,
    17.5f, 14.5f, 19.0f, 17.5f,
    19.0f, 17.5f, 19.0f, 20.0f,
};
constexpr Polyline kPersonLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
    {12, 2}, {14, 2}, {16, 2}, {18, 2}, {20, 2}, {22, 2},
};

// ---- Camera -------------------------------------------------------------------
constexpr f32 kCameraPts[] = {
    3.5f, 8.0f,   20.5f, 8.0f,    // corpo fechado
    20.5f, 8.0f,  20.5f, 19.0f,
    20.5f, 19.0f, 3.5f, 19.0f,
    3.5f, 19.0f,  3.5f, 8.0f,
    8.5f, 8.0f,   9.5f, 5.0f,     // mira fechada
    9.5f, 5.0f,   14.5f, 5.0f,
    14.5f, 5.0f,  15.5f, 8.0f,
    15.5f, 8.0f,  8.5f, 8.0f,
    8.9f, 13.5f,  9.7f, 11.4f,    // lente (octógono fechado)
    9.7f, 11.4f,  14.3f, 11.4f,
    14.3f, 11.4f, 15.1f, 13.5f,
    15.1f, 13.5f, 14.3f, 15.6f,
    14.3f, 15.6f, 9.7f, 15.6f,
    9.7f, 15.6f,  8.9f, 13.5f,
};
constexpr Polyline kCameraLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2},
    {8, 2}, {10, 2}, {12, 2}, {14, 2},
    {16, 2}, {18, 2}, {20, 2}, {22, 2}, {24, 2}, {26, 2},
};

// ---- Box (caixa aberta — TIC importado) ----------------------------------------
constexpr f32 kBoxPts[] = {
    5.0f, 10.0f,  19.0f, 10.0f,   // corpo fechado
    19.0f, 10.0f, 19.0f, 20.5f,
    19.0f, 20.5f, 5.0f, 20.5f,
    5.0f, 20.5f,  5.0f, 10.0f,
    5.0f, 10.0f,  7.0f, 6.0f,     // aba esq aberta
    19.0f, 10.0f, 17.0f, 6.0f,    // aba dir aberta
};
constexpr Polyline kBoxLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
};

// ---- Undo ↶ / Redo ↷ ------------------------------------------------------------
constexpr f32 kUndoPts[] = {
    19.0f, 8.0f,  14.0f, 7.0f,    // curva
    14.0f, 7.0f,  10.0f, 9.0f,
    10.0f, 9.0f,  7.0f, 13.0f,
    7.0f, 13.0f,  6.0f, 17.0f,
    6.0f, 17.0f,  9.2f, 17.4f,    // ponta
    6.0f, 17.0f,  7.3f, 13.8f,
};
constexpr Polyline kUndoLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
};
constexpr f32 kRedoPts[] = {
    5.0f, 8.0f,   10.0f, 7.0f,
    10.0f, 7.0f,  14.0f, 9.0f,
    14.0f, 9.0f,  17.0f, 13.0f,
    17.0f, 13.0f, 18.0f, 17.0f,
    18.0f, 17.0f, 14.8f, 17.4f,
    18.0f, 17.0f, 16.7f, 13.8f,
};
constexpr Polyline kRedoLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
};

// ---- Save (disquete) --------------------------------------------------------------
constexpr f32 kSavePts[] = {
    4.5f, 4.5f,   16.5f, 4.5f,    // corpo fechado
    16.5f, 4.5f,  19.5f, 7.5f,
    19.5f, 7.5f,  19.5f, 19.5f,
    19.5f, 19.5f, 4.5f, 19.5f,
    4.5f, 19.5f,  4.5f, 4.5f,
    9.0f, 4.5f,   15.0f, 4.5f,    // obturador fechado
    15.0f, 4.5f,  15.0f, 9.5f,
    15.0f, 9.5f,  9.0f, 9.5f,
    9.0f, 9.5f,   9.0f, 4.5f,
    8.0f, 13.5f,  16.0f, 13.5f,   // etiqueta fechada
    16.0f, 13.5f, 16.0f, 19.5f,
    16.0f, 19.5f, 8.0f, 19.5f,
    8.0f, 19.5f,  8.0f, 13.5f,
};
constexpr Polyline kSaveLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2},
    {10, 2}, {12, 2}, {14, 2},
    {16, 2}, {18, 2}, {20, 2}, {22, 2},
};

// ---- Copy (L atrás + rect) ----------------------------------------------------------
constexpr f32 kCopyPts[] = {
    15.5f, 4.5f,  4.5f, 4.5f,     // L traseiro fechado
    4.5f, 4.5f,   4.5f, 15.5f,
    4.5f, 15.5f,  6.5f, 15.5f,
    6.5f, 15.5f,  6.5f, 6.5f,
    6.5f, 6.5f,   15.5f, 6.5f,
    15.5f, 6.5f,  15.5f, 4.5f,
    8.5f, 8.5f,   19.5f, 8.5f,    // rect frontal fechado
    19.5f, 8.5f,  19.5f, 19.5f,
    19.5f, 19.5f, 8.5f, 19.5f,
    8.5f, 19.5f,  8.5f, 8.5f,
};
constexpr Polyline kCopyLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
    {12, 2}, {14, 2}, {16, 2}, {18, 2},
};

// ---- Paste (prancheta) ---------------------------------------------------------------
constexpr f32 kPastePts[] = {
    5.0f, 6.0f,   19.0f, 6.0f,    // tábua fechada
    19.0f, 6.0f,  19.0f, 20.0f,
    19.0f, 20.0f, 5.0f, 20.0f,
    5.0f, 20.0f,  5.0f, 6.0f,
    8.5f, 4.0f,   15.5f, 4.0f,    // clip fechado
    15.5f, 4.0f,  15.5f, 8.0f,
    15.5f, 8.0f,  8.5f, 8.0f,
    8.5f, 8.0f,   8.5f, 4.0f,
    8.5f, 12.0f,  15.5f, 12.0f,   // linha de conteúdo 1
    8.5f, 15.5f,  13.0f, 15.5f,   // linha de conteúdo 2
};
constexpr Polyline kPasteLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2},
    {8, 2}, {10, 2}, {12, 2}, {14, 2},
    {16, 2}, {18, 2},
};

// ---- Cursor (seta de seleção) -----------------------------------------------------------
constexpr f32 kCursorPts[] = {
    6.5f, 4.0f,   6.5f, 18.5f,
    6.5f, 18.5f,  10.6f, 14.8f,
    10.6f, 14.8f, 13.2f, 19.8f,
    13.2f, 19.8f, 15.6f, 18.7f,
    15.6f, 18.7f, 13.0f, 13.8f,
    13.0f, 13.8f, 17.8f, 13.6f,
    17.8f, 13.6f, 6.5f, 4.0f,
};
constexpr Polyline kCursorLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
};

// ---- Grid (4 quadrantes) ------------------------------------------------------------------
constexpr f32 kGridPts[] = {
    4.5f, 4.5f,   19.5f, 4.5f,
    19.5f, 4.5f,  19.5f, 19.5f,
    19.5f, 19.5f, 4.5f, 19.5f,
    4.5f, 19.5f,  4.5f, 4.5f,
    12.0f, 4.5f,  12.0f, 19.5f,
    4.5f, 12.0f,  19.5f, 12.0f,
};
constexpr Polyline kGridLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
};

// ---- Terminal (>_) --------------------------------------------------------------------------
constexpr f32 kTerminalPts[] = {
    4.0f, 5.0f,   20.0f, 5.0f,
    20.0f, 5.0f,  20.0f, 19.0f,
    20.0f, 19.0f, 4.0f, 19.0f,
    4.0f, 19.0f,  4.0f, 5.0f,
    7.0f, 9.5f,   10.5f, 12.0f,
    10.5f, 12.0f, 7.0f, 14.5f,
    13.0f, 15.0f, 17.0f, 15.0f,
};
constexpr Polyline kTerminalLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 3}, {11, 2},
};

// ---- Clapper (claquete) ----------------------------------------------------------------------
constexpr f32 kClapperPts[] = {
    4.0f, 6.5f,   20.0f, 6.5f,    // régua superior fechada
    20.0f, 6.5f,  20.0f, 10.0f,
    20.0f, 10.0f, 4.0f, 10.0f,
    4.0f, 10.0f,  4.0f, 6.5f,
    4.0f, 10.0f,  20.0f, 10.0f,   // corpo fechado
    20.0f, 10.0f, 20.0f, 19.5f,
    20.0f, 19.5f, 4.0f, 19.5f,
    4.0f, 19.5f,  4.0f, 10.0f,
    7.5f, 6.5f,   9.5f, 10.0f,    // listras diagonais da régua
    12.0f, 6.5f,  14.0f, 10.0f,
    16.5f, 6.5f,  18.5f, 10.0f,
};
constexpr Polyline kClapperLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2},
    {8, 2}, {10, 2}, {12, 2}, {14, 2},
    {16, 2}, {18, 2}, {20, 2},
};

// ---- Folder (pasta) ---------------------------------------------------------------------------
constexpr f32 kFolderPts[] = {
    4.0f, 6.0f,   9.0f, 6.0f,
    9.0f, 6.0f,   11.0f, 8.5f,
    11.0f, 8.5f,  20.0f, 8.5f,
    20.0f, 8.5f,  20.0f, 19.0f,
    20.0f, 19.0f, 4.0f, 19.0f,
    4.0f, 19.0f,  4.0f, 6.0f,
};
constexpr Polyline kFolderLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
};

// ---- Search (lupa) ------------------------------------------------------------------------------
constexpr f32 kSearchPts[] = {
    10.5f, 5.0f,  6.7f, 6.6f,     // aro (octógono fechado)
    6.7f, 6.6f,   5.0f, 10.5f,
    5.0f, 10.5f,  6.7f, 14.4f,
    6.7f, 14.4f,  10.5f, 16.0f,
    10.5f, 16.0f, 14.3f, 14.4f,
    14.3f, 14.4f, 16.0f, 10.5f,
    16.0f, 10.5f, 14.3f, 6.6f,
    14.3f, 6.6f,  10.5f, 5.0f,
    14.6f, 14.6f, 19.5f, 19.5f,   // cabo
};
constexpr Polyline kSearchLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2}, {12, 2}, {14, 2},
    {16, 2},
};

// ---- Sort (ordenar) -------------------------------------------------------------------------------
constexpr f32 kSortPts[] = {
    4.0f, 6.5f,   13.0f, 6.5f,    // linhas decrescentes
    4.0f, 11.5f,  10.5f, 11.5f,
    4.0f, 16.5f,  8.0f, 16.5f,
    18.0f, 5.0f,  18.0f, 19.0f,   // haste
    14.8f, 15.8f, 18.0f, 19.0f,   // ponta
    18.0f, 19.0f, 21.2f, 15.8f,
};
constexpr Polyline kSortLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
};

// ---- Upload / Download (bandeja + seta) -------------------------------------------------------------
constexpr f32 kUploadPts[] = {
    4.0f, 15.0f,  4.0f, 19.5f,    // bandeja (aberta)
    4.0f, 19.5f,  20.0f, 19.5f,
    20.0f, 19.5f, 20.0f, 15.0f,
    12.0f, 15.5f, 12.0f, 4.5f,    // haste ↑
    8.8f, 7.5f,   12.0f, 4.5f,    // ponta
    12.0f, 4.5f,  15.2f, 7.5f,
};
constexpr Polyline kUploadLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
};
constexpr f32 kDownloadPts[] = {
    4.0f, 15.0f,  4.0f, 19.5f,
    4.0f, 19.5f,  20.0f, 19.5f,
    20.0f, 19.5f, 20.0f, 15.0f,
    12.0f, 4.5f,  12.0f, 15.5f,   // haste ↓
    8.8f, 12.5f,  12.0f, 15.5f,
    12.0f, 15.5f, 15.2f, 12.5f,
};
constexpr Polyline kDownloadLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
};

// ---- Mic ----------------------------------------------------------------------------------------------
constexpr f32 kMicPts[] = {
    9.0f, 4.0f,   15.0f, 4.0f,    // cápsula fechada
    15.0f, 4.0f,  15.0f, 11.0f,
    15.0f, 11.0f, 13.5f, 13.0f,
    13.5f, 13.0f, 10.5f, 13.0f,
    10.5f, 13.0f, 9.0f, 11.0f,
    9.0f, 11.0f,  9.0f, 4.0f,
    5.5f, 10.5f,  5.5f, 12.5f,    // U (aberto)
    5.5f, 12.5f,  7.0f, 15.5f,
    7.0f, 15.5f,  12.0f, 17.0f,
    12.0f, 17.0f, 17.0f, 15.5f,
    17.0f, 15.5f, 18.5f, 12.5f,
    18.5f, 12.5f, 18.5f, 10.5f,
    12.0f, 17.0f, 12.0f, 20.0f,   // haste
    8.5f, 20.0f,  15.5f, 20.0f,   // base
};
constexpr Polyline kMicLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
    {12, 2}, {14, 2}, {16, 2}, {18, 2}, {20, 2},
    {22, 2}, {24, 2},
};

// ---- Stop ■ / Record ● --------------------------------------------------------------------------------
constexpr f32 kStopPts[] = {
    7.0f, 7.0f,   17.0f, 7.0f,
    17.0f, 7.0f,  17.0f, 17.0f,
    17.0f, 17.0f, 7.0f, 17.0f,
    7.0f, 17.0f,  7.0f, 7.0f,
};
constexpr Polyline kStopLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2},
};
// Record: círculo GERADO (12-gon r=6 fechado — SpinInit escreve [0..12])
f32 kRecordPts[2 * 13] = {};
constexpr Polyline kRecordLines[] = {
    {0, 13},
};

// ---- Check ✓ / Warn ⚠ ----------------------------------------------------------------------------------
constexpr f32 kCheckPts[] = {
    5.0f, 13.0f,  10.0f, 18.0f,
    10.0f, 18.0f, 19.0f, 6.5f,
};
constexpr Polyline kCheckLines[] = {
    {0, 3},
};
constexpr f32 kWarnPts[] = {
    12.0f, 4.0f,  20.5f, 19.0f,   // triângulo fechado
    20.5f, 19.0f, 3.5f, 19.0f,
    3.5f, 19.0f,  12.0f, 4.0f,
    12.0f, 9.0f,  12.0f, 14.0f,   // exclamação
    12.0f, 16.6f, 12.0f, 17.4f,
};
constexpr Polyline kWarnLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2},
};

// ---- Question (? em círculo) ----------------------------------------------------------------------------
constexpr f32 kQuestionPts[] = {
    12.0f, 3.5f,  5.2f, 7.0f,     // aro (octógono fechado r=8)
    5.2f, 7.0f,   5.2f, 17.0f,
    5.2f, 17.0f,  12.0f, 20.5f,
    12.0f, 20.5f, 18.8f, 17.0f,
    18.8f, 17.0f, 18.8f, 7.0f,
    18.8f, 7.0f,  12.0f, 3.5f,
    9.2f, 9.6f,   9.2f, 8.2f,     // gancho do ?
    9.2f, 8.2f,   11.0f, 6.6f,
    11.0f, 6.6f,  13.6f, 6.8f,
    13.6f, 6.8f,  15.0f, 8.6f,
    15.0f, 8.6f,  15.0f, 10.2f,
    15.0f, 10.2f, 12.8f, 12.4f,
    12.8f, 12.4f, 12.8f, 14.6f,
    12.8f, 17.4f, 12.8f, 18.2f,   // ponto do ?
};
constexpr Polyline kQuestionLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
    {12, 2}, {14, 2}, {16, 2}, {18, 2}, {20, 2}, {22, 2},
    {24, 2},
};

// ---- Spinner (GERADO — arco 300° r=8) ---------------------------------------------------------------------
f32 kSpinnerPts[2 * 12] = {};
constexpr Polyline kSpinnerLines[] = {
    {0, 12},
};

// ---- Trash (lixeira) ---------------------------------------------------------------------------------------
constexpr f32 kTrashPts[] = {
    9.5f, 4.5f,  14.5f, 4.5f,     // pega fechada
    14.5f, 4.5f, 14.5f, 7.0f,
    14.5f, 7.0f, 9.5f, 7.0f,
    9.5f, 7.0f,  9.5f, 4.5f,
    5.0f, 7.0f,  19.0f, 7.0f,     // tampa
    6.5f, 7.0f,  7.4f, 19.5f,     // corpo traseiro (trapezoidal aberto)
    7.4f, 19.5f, 16.6f, 19.5f,
    16.6f, 19.5f, 17.5f, 7.0f,
    10.0f, 10.5f, 10.0f, 16.0f,   // listras
    14.0f, 10.5f, 14.0f, 16.0f,
};
constexpr Polyline kTrashLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2},
    {8, 2}, {10, 2}, {12, 2}, {14, 2},
    {16, 2}, {18, 2},
};

// ---- Rename (lápis) ------------------------------------------------------------------------------------------
constexpr f32 kRenamePts[] = {
    4.5f, 19.5f,  5.5f, 15.8f,    // corpo fechado
    5.5f, 15.8f,  16.2f, 5.0f,
    16.2f, 5.0f,  19.0f, 7.8f,
    19.0f, 7.8f,  8.3f, 18.5f,
    8.3f, 18.5f, 4.5f, 19.5f,
    14.4f, 6.8f,  17.2f, 9.6f,    // corte da ponta
};
constexpr Polyline kRenameLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
};

// ---- Duplicate (rect + plus) ------------------------------------------------------------------------------------
constexpr f32 kDuplicatePts[] = {
    8.5f, 8.5f,   19.5f, 8.5f,    // rect frontal fechado
    19.5f, 8.5f,  19.5f, 19.5f,
    19.5f, 19.5f, 8.5f, 19.5f,
    8.5f, 19.5f,  8.5f, 8.5f,
    14.0f, 11.5f, 14.0f, 16.5f,   // plus interior
    11.5f, 14.0f, 16.5f, 14.0f,
};
constexpr Polyline kDuplicateLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2},
};

// ---- Assign (seta para dentro da caixa) ---------------------------------------------------------------------------
constexpr f32 kAssignPts[] = {
    4.5f, 12.5f,  4.5f, 19.5f,    // caixa fechada
    4.5f, 19.5f,  11.5f, 19.5f,
    11.5f, 19.5f, 11.5f, 12.5f,
    11.5f, 12.5f, 4.5f, 12.5f,
    19.5f, 16.0f, 10.5f, 16.0f,   // seta ←
    13.3f, 13.2f, 10.5f, 16.0f,
    10.5f, 16.0f, 13.3f, 18.8f,
};
constexpr Polyline kAssignLines[] = {
    {0, 2}, {2, 2}, {4, 2}, {6, 2}, {8, 2}, {10, 2}, {12, 2},
};

// ---- Back ← ---------------------------------------------------------------------------------------------------------
constexpr f32 kBackPts[] = {
    19.0f, 12.0f, 5.0f, 12.0f,
    8.8f, 8.2f,   5.0f, 12.0f,
    5.0f, 12.0f,  8.8f, 15.8f,
};
constexpr Polyline kBackLines[] = {
    {0, 2}, {2, 2}, {4, 3},
};

// tabela final (índice = Icon)
const IconDef kDefs[] = {
    {kMovePts,          12, kMoveLines,          6},
    {kRotatePts,        18, kRotateLines,        4},
    {kScalePts,         10, kScaleLines,         5},
    {kSnapPts,          14, kSnapLines,          7},
    {kInspectorPts,     14, kInspectorLines,     7},
    {kScenePts,         16, kSceneLines,         8},
    {kPlayPts,           6, kPlayLines,          3},
    {kPausePts,          4, kPauseLines,         2},
    {kHamburgerPts,      6, kHamburgerLines,     3},
    {kChevronDownPts,    3, kChevronDownLines,   1},
    {kChevronRightPts,   3, kChevronRightLines,  1},
    {kSlidersPts,       12, kSlidersLines,       6},
    {kGearPts,          26, kGearLines,           2},
    {kCubePts,          18, kCubeLines,          9},
    {kMonitorPts,       12, kMonitorLines,       6},
    {kSpeakerPts,       15, kSpeakerLines,       7},
    {kPlusPts,           4, kPlusLines,          2},
    {kEyePts,           28, kEyeLines,          14},
    {kEyeOffPts,        18, kEyeOffLines,        9},
    {kDotsPts,           6, kDotsLines,          3},
    {kPersonPts,        24, kPersonLines,       12},
    {kCameraPts,        28, kCameraLines,       14},
    {kBoxPts,           12, kBoxLines,           6},
    {kUndoPts,          12, kUndoLines,          6},
    {kRedoPts,          12, kRedoLines,          6},
    {kSavePts,          24, kSaveLines,         12},
    {kCopyPts,          20, kCopyLines,         10},
    {kPastePts,         20, kPasteLines,        10},
    {kCursorPts,        12, kCursorLines,        6},
    {kGridPts,          12, kGridLines,          6},
    {kTerminalPts,      14, kTerminalLines,      6},
    {kClapperPts,       22, kClapperLines,      11},
    {kFolderPts,        12, kFolderLines,        6},
    {kSearchPts,        18, kSearchLines,        9},
    {kSortPts,          12, kSortLines,          6},
    {kUploadPts,        12, kUploadLines,        6},
    {kDownloadPts,      12, kDownloadLines,      6},
    {kMicPts,           26, kMicLines,          13},
    {kStopPts,           8, kStopLines,          4},
    {kRecordPts,        13, kRecordLines,        1},
    {kCheckPts,          3, kCheckLines,         1},
    {kWarnPts,          10, kWarnLines,          5},
    {kQuestionPts,      26, kQuestionLines,     13},
    {kSpinnerPts,       12, kSpinnerLines,       1},
    {kTrashPts,         20, kTrashLines,        10},
    {kRenamePts,        12, kRenameLines,        6},
    {kDuplicatePts,     12, kDuplicateLines,     6},
    {kAssignPts,        14, kAssignLines,        7},
    {kBackPts,           6, kBackLines,          3},
};
static_assert(sizeof(kDefs) / sizeof(kDefs[0]) ==
              static_cast<size_t>(Icon::Count), "tabela de ícones incompleta");

// o arco do Rodar é gerado UMA vez (constexpr não teria std::cos) — corre
// no arranque do TU, antes de main()/testes.
struct RotArcInit {
    RotArcInit() {
        const f32 a0 = -55.0f * 0.01745329252f;
        const f32 a1 = 180.0f * 0.01745329252f;
        const f32 r = 8.2f;
        for (int i = 0; i < 12; ++i) {
            const f32 a = a0 + (a1 - a0) * (static_cast<f32>(i) / 11.0f);
            kRotatePts[2 * i]     = 12.0f + r * std::cos(a);
            kRotatePts[2 * i + 1] = 12.0f + r * std::sin(a);
        }
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
        kRotatePts[32] = 12.0f;
        kRotatePts[33] = 8.6f;
        kRotatePts[34] = 12.0f;
        kRotatePts[35] = 15.4f;
    }
};
const RotArcInit kRotArcInit;

// 0.9.0 — GEAR: coroa dentada (16 dentes alternando raio 9.6/7.4) + cubo
// octogonal r=3.2. Tudo DENTRO do viewBox (9.6+traço/2 < 12 do centro).
struct GearInit {
    GearInit() {
        // coroa: 17 pontos (o último repete o 1º — polilinha fechada)
        for (int i = 0; i <= 16; ++i) {
            const f32 a = static_cast<f32>(i) * 22.5f * 0.01745329252f;
            const f32 r = (i % 2 == 0) ? 9.6f : 7.4f;
            kGearPts[2 * i]     = 12.0f + r * std::cos(a);
            kGearPts[2 * i + 1] = 12.0f + r * std::sin(a);
        }
        // cubo: octógono fechado (9 pontos; começa no ângulo 22.5° para os
        // vértices ficarem entre os dentes)
        for (int i = 0; i <= 8; ++i) {
            const f32 a = (22.5f + static_cast<f32>(i) * 45.0f) * 0.01745329252f;
            kGearPts[2 * (17 + i)]     = 12.0f + 3.2f * std::cos(a);
            kGearPts[2 * (17 + i) + 1] = 12.0f + 3.2f * std::sin(a);
        }
    }
};
const GearInit kGearInit;

// 0.9.0 — RECORD (círculo 12-gon fechado) e SPINNER (arco 300°):
struct SpinInit {
    SpinInit() {
        // Record: 13 pontos (12-gon fechado)
        for (int i = 0; i <= 12; ++i) {
            const f32 a = static_cast<f32>(i) * 30.0f * 0.01745329252f - 90.0f;
            kRecordPts[2 * i]     = 12.0f + 6.0f * std::cos(a);
            kRecordPts[2 * i + 1] = 12.0f + 6.0f * std::sin(a);
        }
        // Spinner: arco de 300° (12 pontos, r=8) — a "falta" de 60° é o
        // movimento percebido (todo spinner outline tem um vão)
        const f32 a0 = -150.0f * 0.01745329252f;
        const f32 a1 = 150.0f * 0.01745329252f;
        for (int i = 0; i < 12; ++i) {
            const f32 a = a0 + (a1 - a0) * (static_cast<f32>(i) / 11.0f);
            kSpinnerPts[2 * i]     = 12.0f + 8.0f * std::cos(a);
            kSpinnerPts[2 * i + 1] = 12.0f + 8.0f * std::sin(a);
        }
    }
};
const SpinInit kSpinInit;

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

i32 iconByName(const char* name) {
    if (!name) {
        return -1;
    }
    static const struct { Icon ic; const char* nm; } kNames[] = {
        {Icon::Move, "mover"}, {Icon::Rotate, "rodar"},
        {Icon::Scale, "escalar"}, {Icon::Snap, "snap"},
        {Icon::Inspector, "inspector"}, {Icon::Scene, "cena"},
        {Icon::Play, "play"}, {Icon::Pause, "pause"},
        {Icon::Hamburger, "hamburger"}, {Icon::ChevronDown, "chevron"},
        {Icon::ChevronRight, "chevron-direita"}, {Icon::Sliders, "sliders"},
        {Icon::Gear, "gear"}, {Icon::Cube, "cubo"}, {Icon::Monitor, "monitor"},
        {Icon::Speaker, "speaker"}, {Icon::Plus, "plus"},
        {Icon::Eye, "olho"}, {Icon::EyeOff, "olho-off"},
        {Icon::Dots, "dots"}, {Icon::Person, "pessoa"},
        {Icon::Camera, "camara"}, {Icon::Box, "box"},
        {Icon::Undo, "undo"}, {Icon::Redo, "redo"}, {Icon::Save, "save"},
        {Icon::Copy, "copy"}, {Icon::Paste, "paste"},
        {Icon::Cursor, "cursor"}, {Icon::Grid, "grelha"},
        {Icon::Terminal, "terminal"}, {Icon::Clapper, "clapper"},
        {Icon::Folder, "pasta"}, {Icon::Search, "lupa"},
        {Icon::Sort, "ordenar"}, {Icon::Upload, "upload"},
        {Icon::Download, "download"}, {Icon::Mic, "mic"},
        {Icon::Stop, "stop"}, {Icon::Record, "record"},
        {Icon::Check, "check"}, {Icon::Warn, "warn"},
        {Icon::Question, "interrogacao"}, {Icon::Spinner, "spinner"},
        {Icon::Trash, "lixo"}, {Icon::Rename, "renomear"},
        {Icon::Duplicate, "duplicar"}, {Icon::Assign, "atribuir"},
        {Icon::Back, "back"},
    };
    for (const auto& e : kNames) {
        if (std::strcmp(e.nm, name) == 0) {
            return static_cast<i32>(e.ic);
        }
    }
    return -1;
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
}

} // namespace icons
} // namespace vv
