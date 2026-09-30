#pragma once
// ui/UiEditor.h — EDITOR DE UI DEDICADO (0.7.0, F6): viewport 2D WYSIWYG,
// Inspector de elemento, teclado in-app, menu contextual e diálogos de
// gestão de TICs.
//
// VIEWPORT 2D (modo UI do separador "3D | UI"): a área central do editor
// deixa de mostrar a cena 3D e passa a mostrar o canvas do TIC selecionado
// em escala-caber (scale-to-fit: o espaço de design é o ECRÃ INTEIRO; o
// transform é partilhado por draw/hit-test/drag — nenhuma divergência).
// Tap seleciona o elemento sob o dedo (o de CIMA ganha), drag MOVE
// (WYSIWYG: ox/oy atualizados pelo delta em design px), tap no vazio
// desseleciona o elemento.
//
// TECLADO IN-APP: overlay mono com A-Z, 0-9, '_', '-', espaço, APAGA,
// OK/Cancelar — ZERO IME de sistema (frágil em NativeActivity). A geometria
// vive em keyboardLayout() (FONTE ÚNICA — o desenho e os testes partilham
// os rects; aferição de sem-sobreposição no CI).
//
// GESTÃO DE TICs: menu contextual (botão "⋮" na linha da Hierarchy —
// determinístico e aferível, cumpre o "long-press (ou ⋮)" do escopo):
// Renomear (teclado) / Remover (diálogo de confirmação) / Duplicar /
// Visibilidade (toggle). O olho na Hierarchy atalha a visibilidade.
#include "components/UiCanvas.h"
#include "core/Handle.h"
#include "ui/EditorLayout.h"
#include "ui/SafeArea.h"
#include "ui/ScrollMath.h"

namespace vv {

class Scene;
class UiContext;
class InputState;

namespace editor {
struct EditorState;   // definido em ui/EditorUi.h (dependência só de declaração)
} // namespace editor

namespace editor {

// ---- viewport 2D --------------------------------------------------------------

// transform do espaço de design (ecrã inteiro) → rect do viewport central:
// escala uniforme que CABE (nunca corta) + centrado. Pure — os testes
// aferem que o canvas inteiro fica DENTRO do viewport.
struct ViewportTransform {
    f32 scale = 1.0f;   // design px → ecrã px
    f32 ox = 0.0f;      // origem do espaço de design no ecrã
    f32 oy = 0.0f;
};
inline ViewportTransform uiViewportTransform(const UiRect& view, f32 sw,
                                             f32 sh) {
    ViewportTransform t;
    t.scale = (sw > 0.0f && sh > 0.0f)
                  ? (view.w / sw < view.h / sh ? view.w / sw : view.h / sh)
                  : 1.0f;
    t.ox = view.x + (view.w - sw * t.scale) * 0.5f;
    t.oy = view.y + (view.h - sh * t.scale) * 0.5f;
    return t;
}
// ecrã px → design px (o inverso — hit-test/drag do WYSIWYG)
inline f32 uiViewportToDesignX(const ViewportTransform& t, f32 px) {
    return (px - t.ox) / (t.scale > 0.0f ? t.scale : 1.0f);
}
inline f32 uiViewportToDesignY(const ViewportTransform& t, f32 py) {
    return (py - t.oy) / (t.scale > 0.0f ? t.scale : 1.0f);
}

// desenha o viewport 2D (bg + moldura do ecrã + elementos do canvas do TIC
// selecionado + seleção) e processa o gesto WYSIWYG (tap seleciona, drag
// move, tap no vazio desseleciona). Sem canvas → hint mono.
void drawUiViewport(UiContext& ui, Scene& scene, EditorState& st,
                    const InputState& in, f32 sw, f32 sh);

// ---- Inspector de elemento ------------------------------------------------------

// plano do Inspector de UI (linhas sequenciais com y cumulativo — o MESMO
// contrato do plano do Inspector de TICs, F5.0-fix). Fonte única partilhada
// com os testes de sem-sobreposição.
struct UiInspRow {
    enum class Kind : u8 {
        Name,       // nome + tipo do elemento (accent)
        PosX, PosY, // sliders ox/oy (design px)
        SizeW, SizeH,   // sliders w/h
        ColR, ColG, ColB,   // sliders cor 0..1
        VisToggle,      // "visivel: sim/nao" (botão re-despachado)
        AnchorH,        // "ancora H: ..." (cicla esquerda/centro/direita)
        AnchorV,        // "ancora V: ..." (cicla topo/meio/fundo)
        TextBtn,        // "texto: ..." (abre o teclado)
        ActType,        // "acao: ..." (cicla none/show/hide/toggle/scene/spawn)
        ActTarget,      // "alvo: ..." (abre o teclado)
        Remove,         // "remover elemento"
    };
    Kind kind;
    f32  y;
    f32  h;
    u64  id;
};

// o elemento tem TEXTO editável? (Label/Button/Card/Article/Menu)
bool uiElementHasText(UiElement::Kind k);
// o elemento tem AÇÃO on-click? (Button/Menu)
bool uiElementHasAction(UiElement::Kind k);

u32 uiInspectorRowCount(const UiElement& e, const TextMetrics& m);
u32 uiInspectorPlan(const UiElement& e, const TextMetrics& m, UiInspRow* rows,
                    u32 cap);
f32  uiInspectorContentHeight(const UiElement& e, const TextMetrics& m);

// desenha o Inspector de UI no painel direito (substitui o de TICs quando
// há elemento selecionado em modo UI). Devolve true se algum valor mudou.
bool drawUiInspector(UiContext& ui, Scene& scene, EditorState& st,
                     const InputState& in);

// ---- criação de elementos (+ do modo UI) ----------------------------------------

// cria o elemento no canvas do TIC selecionado (cria o canvas se o TIC não
// tiver um) e seleciona-o. kind como u32 p/ o dispatch do menu (+).
// Devolve false se não há TIC selecionado (o chamador faz o toast).
bool uiAddElement(Scene& scene, EditorState& st, u32 kind, f32 sw, f32 sh);

// ---- gestão de TICs (menu contextual + diálogos) ---------------------------------

// menu contextual: devolve 0 nada / 1 Renomear / 2 Remover / 3 Duplicar /
// 4 Visibilidade. Fecha com toque fora (muta st).
int drawContextMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                    EditorState& st, const char* ticName, bool ticVisible);

// diálogo de confirmação de remoção: 0 nada / 1 confirmar / 2 cancelar.
int drawRemoveDialog(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                     EditorState& st, const char* ticName);

// duplica o TIC (todos os componentes por valor; nome único com sufixo
// Godot-style ".001"). Devolve o handle novo (invalid se o alvo morreu).
Handle duplicateTic(Scene& scene, Handle h);

// ---- teclado in-app + input de texto ---------------------------------------------

// geometria do teclado (FONTE ÚNICA — desenho e testes partilham os rects):
// 4 linhas de teclas (A..I / J..R / S..Z + '_' / 0..9) + linha de baixo
// (ESPACO, '-', APAGA, OK, CANCELAR) + linha do buffer. 10 colunas.
struct KeyboardLayout {
    UiRect dialog{};
    UiRect buffer{};
    UiRect key[4][10]{};
    u32    keyCount[4] = {9, 9, 9, 10};
    UiRect space{}, dash{}, back{}, ok{}, cancel{};
    // rótulos das teclas (estáticos: "A".."Z", '_', "0".."9")
    static const char* keyLabel(u32 row, u32 col);
};
KeyboardLayout keyboardLayout(f32 sw, f32 sh, const safe::Insets& ins);

// overlay do teclado + diálogo: devolve 0 nada / 1 OK / 2 Cancelar.
// O buffer vive em EditorState (st.textBuf/textLen).
int drawTextInput(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                  EditorState& st, const char* title);

// abre o input de texto com um valor inicial (helper — zera o buffer e
// copia até 39 chars)
void openTextInput(EditorState& st, int purpose, Handle tic, i32 element,
                   const char* initial);

// aplica o buffer conforme o propósito (renomear TIC / texto de elemento /
// alvo de ação). Devolve true se aplicou (o nome vazio NÃO se aplica).
// O propósito 1 (nome de cena) fica para o CHAMADOR (0.7.1 — precisa do
// projeto, não da cena).
bool commitTextInput(Scene& scene, EditorState& st);

// válidos no teclado: letras MAIÚSCULAS (o atlas é mono brutalist — nomes
// da engine são PascalCase; o buffer fica uniforme), dígitos, '_', '-',
// espaço. Filtro defensivo (o teclado só OFERECE estas teclas).
bool uiTextCharAllowed(char c);

} // namespace editor
} // namespace vv
