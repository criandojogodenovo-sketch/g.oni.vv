#pragma once
// ui/Icons.h — CONJUNTO ÚNICO DE ÍCONES OUTLINE (0.9.0, spec A).
//
// Cada ícone é um conjunto de POLILINHAS em viewBox 0..24 (unidades de
// design), desenhadas pelo LINE BATCH existente (UiContext::drawLine). SEM
// parser de SVG, SEM raster, SEM atlas novo: geometria pura → segmentos de
// ecrã com espessura uniforme (thickness = size/12 — traço uniforme do
// conjunto outline, 24dp dentro de alvos de 48dp).
//
// A LISTA (spec A — por função; 8 herdados da 0.7.6 + 41 novos):
//   hamburger · chevron (baixo/direita) · pause · play · sliders · gear ·
//   cubo · monitor · speaker · plus · olho · olho-off · ⋮ vertical ·
//   pessoa · câmara · box (variante aberta) · undo · redo · save · copy ·
//   paste · cursor · mover · rodar · escalar · grelha · terminal · clapper ·
//   pasta · lupa · ordenar · upload · download · mic · stop · record ·
//   check · warn · interrogação · spinner · lixo · renomear · duplicar ·
//   atribuir · back · snap (ímã) · inspector (lista) · cena (camadas)
//
// REGRAS (afervadas no CI — test_wiring090):
//   • traço UNIFORME (thickness = size/12) — conjunto outline;
//   • TODOS os pontos dentro do viewBox 0..24 (nunca sai do alvo);
//   • UNICIDADE: nenhum ícone partilha a sequência de segmentos com outro
//     (a auditoria hashes os segmentos normalizados);
//   • legível a 24dp; zero emoji, zero preenchimentos orgânicos;
//   • Gear/Spinner são GERADOS no arranque do TU (cos/sin não-constexpr),
//     como o arco do Rodar desde a 0.7.6.
#include "core/Types.h"

namespace vv {

class UiContext;

namespace icons {

enum class Icon : u8 {
    // ---- 0.7.6 (herdados — MESMOS índices de sempre) ----
    Move = 0,       // mover (setas cruzadas)
    Rotate,         // rodar (arco + seta)
    Scale,          // escalar (diagonal dupla)
    Snap,           // ímã (toggle de snapping)
    Inspector,      // painel com 3 linhas
    Scene,          // camadas (2 losangos)
    Play,           // triângulo
    Pause,          // 2 barras
    // ---- 0.9.0 (spec A — o conjunto completo) ----
    Hamburger,      // ☰ menu
    ChevronDown,    // v (expande/colapsa)
    ChevronRight,   // > (fechado/avança)
    Sliders,        // 3 linhas com knobs (viewport settings)
    Gear,           // definições
    Cube,           // modo 3D / TIC de mesh
    Monitor,        // modo UI
    Speaker,        // modo ÁUDIO / clip de áudio
    Plus,           // adicionar
    Eye,            // visível
    EyeOff,         // escondido
    Dots,           // ⋮ vertical (menu contextual)
    Person,         // TIC player/personagem
    Camera,         // TIC câmara
    Box,            // TIC importado (caixa aberta)
    Undo,           // ↶ desfazer
    Redo,           // ↷ refazer
    Save,           // disquete
    Copy,           // 2 rects (L atrás)
    Paste,          // prancheta
    Cursor,         // selecionar (seta)
    Grid,           // grelha (4 quadrantes)
    Terminal,       // consola (>_)
    Clapper,        // animação (claquete)
    Folder,         // ficheiros
    Search,         // lupa
    Sort,           // ordenar (linhas + seta)
    Upload,         // importar (bandeja + seta ↑)
    Download,       // exportar (bandeja + seta ↓)
    Mic,            // microfone
    Stop,           // ■ quadrado
    Record,         // ● círculo
    Check,          // ✓
    Warn,           // ⚠ triângulo !
    Question,       // ? em círculo
    Spinner,        // arco parcial (a carregar)
    Trash,          // lixeira
    Rename,         // lápis
    Duplicate,      // rect + plus (duplicar ≠ copy)
    Assign,         // seta para dentro da caixa (atribuir)
    Back,           // ← voltar
    // ---- FASE 9 (G2-7 — hierarquia consulta o BodyComp) ----
    Static,         // corpo ESTÁTICO (bloco assente no chão — tic_static)
    Rigid,          // corpo RÍGIDO (bola com rasto de queda — tic_rigid)
    // ---- 0.9.6 (G3 — o teclado próprio do editor de script) ----
    Keyboard,       // teclado (rect + 2 filas de teclas)
    // ---- 0.9.6.1 (G2-6c — a tecla APAGA do teclado próprio) ----
    Erase,          // backspace (pentagono + × — o rótulo APA… truncava)
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

// 0.9.0 — ícone pelo NOME funcional (spec A lista por função; o valor
// devolve <0 se o nome não existe). Partilhado com os testes de unicidade.
i32 iconByName(const char* name);

} // namespace icons
} // namespace vv
