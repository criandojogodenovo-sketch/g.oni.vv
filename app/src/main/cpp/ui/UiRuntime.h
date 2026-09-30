#pragma once
// ui/UiRuntime.h — RUNTIME da UI criável (0.7.0, F6): layout ancorado,
// desenho, hit-test e ações declarativas. GL-free no núcleo (desenha via
// UiContext — os testes leem os batches de CPU).
//
// ANCORAGENS (3×3): o rect de cada elemento é (ox, oy, w, h) relativo ao
// PONTO DE ÂNCORA da safe-area (elementRect é a FONTE ÚNICA — editor e Play
// partilham a MESMA matemática; aferida em 2 resoluções no CI):
//   horizontal: esquerda (fixa à esquerda) | centro (segue o centro) | direita
//   vertical:   topo                      | meio               | fundo
//
// DRAW (Play): a UI é desenhada POR CIMA da cena (pass UI sem depth — a UI
// nunca é ocluída), antes da play bar/toast. Elementos invisíveis não
// desenham (a visibilidade é por ELEMENTO; a do TIC esconde o canvas todo).
//
// HIT-TEST (Play): o topo ganha (último canvas/elemento desenhado que
// contém o toque); só Button e itens de Menu são interativos. O toque num
// botão da UI NÃO vai para os TouchControls (o main reclama o slot).
//
// AÇÕES DECLARATIVAS: mostrar/esconder/alternar panel (por NOME, em
// QUALQUER canvas da cena), carregar cena (0.7.1) e spawn de preset —
// dependências INJETADAS em UiActionCtx (o padrão de AssetResolvers: a
// lógica é pura/afervel; o main liga os objetos reais).
#include "components/UiCanvas.h"
#include "core/Handle.h"
#include "core/Presets.h"    // PresetKind (spawn declarativo)
#include "ui/SafeArea.h"
#include "ui/ScrollMath.h"   // UiRect

namespace vv {

class Scene;
class UiContext;

namespace ui {

// ---- layout (FONTE ÚNICA — partilhada com o editor 2D) -----------------------

// rect do elemento no ecrã real (âncoras + safe-area). Com Insets{} é o
// espaço de DESIGN puro (o que o editor 2D desenha escalado).
inline UiRect elementRect(const UiElement& e, f32 sw, f32 sh,
                          const safe::Insets& ins) {
    f32 x = 0.0f;
    switch (e.anchorH) {
        case UiElement::AnchorH::Left:   x = ins.left + e.ox; break;
        case UiElement::AnchorH::Center: x = sw * 0.5f + e.ox; break;
        case UiElement::AnchorH::Right:  x = sw - ins.right - e.w + e.ox; break;
    }
    f32 y = 0.0f;
    switch (e.anchorV) {
        case UiElement::AnchorV::Top:    y = ins.top + e.oy; break;
        case UiElement::AnchorV::Middle: y = sh * 0.5f + e.oy; break;
        case UiElement::AnchorV::Bottom: y = sh - ins.bottom - e.h + e.oy; break;
    }
    return {x, y, e.w, e.h};
}

// ---- menu (composto 0.7.3 — geometria partilhada por draw + hit-test) -------
// linhas do texto do Menu: cada linha é "label>alvo" (alvo opcional — sem
// '>' o alvo é o próprio label). Devolve o nº de linhas e escreve label/alvo.
u32 menuLineCount(const UiElement& e);
bool menuLineAt(const UiElement& e, u32 row, std::string& outLabel,
                std::string& outTarget);
// rect do item `row` do menu dentro do rect base do elemento
UiRect menuItemRect(const UiElement& e, const UiRect& base, u32 row);

// ---- desenho -----------------------------------------------------------------

// desenha UM elemento num rect JÁ mapeado (o editor 2D passa o rect
// escalado do viewport; o Play passa elementRect no ecrã real — a MESMA
// função, zero divergência de desenho).
// `sel` desenha a moldura de seleção do editor (mono: frame ACCENT).
// Devolve false quando NÃO desenhou (invisível/degenerado) — o chamador
// decide o que contar.
bool drawElement(UiContext& ui, const UiElement& e, const UiRect& r,
                 bool sel = false);

// desenha o canvas inteiro no ecrã real (Play — UI por cima da cena; o
// TIC tem de estar ativo E visível). Devolve o nº de elementos DESENHADOS
// (invisíveis não contam).
u32 drawCanvas(UiContext& ui, const UiCanvas& c, f32 sw, f32 sh,
               const safe::Insets& ins);

// ---- hit-test (Play) -----------------------------------------------------------

struct CanvasHit {
    bool valid = false;
    Handle tic{};        // dono do canvas onde está o elemento
    i32  element = -1;   // índice do elemento no canvas
    i32  menuItem = -1;  // linha do Menu (−1 = não é menu)
};

// elemento interativo (Button / item de Menu) sob o toque — o TOPO ganha
// (a ordem de desenho é a ordem dos TICs/elementos; o último que contém o
// ponto fica por cima). Canvas de TIC inativo/invisível não hit-testa.
CanvasHit hitTestCanvas(const Scene& scene, f32 x, f32 y, f32 sw, f32 sh,
                        const safe::Insets& ins);

// ---- ações declarativas --------------------------------------------------------

// estilo com que a cena troca (o callback do main decide o que faz)
enum class SceneSwap : u8 { Instant = 0, Fade = 1, Slide = 2 };

// dependências do main (padrão de AssetResolvers — testes usam stubs)
struct UiActionCtx {
    // 0.7.1: cena existe? / carrega (com o estilo pedido: instantâneo nas
    // ações Scene.Load, fade/slide nas Scene.Transition)
    bool (*sceneExists)(const std::string& name, void* user) = nullptr;
    void (*loadScene)(const std::string& name, SceneSwap style,
                      void* user) = nullptr;
    // spawn: cria o TIC do preset (o main liga ao createTicFromPreset)
    Handle (*spawnPreset)(PresetKind kind, void* user) = nullptr;
    void* user = nullptr;
};

struct UiActionResult {
    bool acted = false;      // a ação correu (efeito aplicado)
    bool wantToast = false;
    char toast[96] = "";
    char log[160] = "";
};

// aplica a AÇÃO do elemento (Show/Hide/Toggle panel por nome em QUALQUER
// canvas; LoadScene/TransitionScene/Spawn via callbacks). `target` permite
// o MENU passar o alvo da LINHA em vez do alvo do elemento (nullptr = usa
// e.target).
UiActionResult applyUiAction(Scene& scene, const UiElement& e,
                             const UiActionCtx& ctx,
                             const char* targetOverride = nullptr);

} // namespace ui
} // namespace vv
