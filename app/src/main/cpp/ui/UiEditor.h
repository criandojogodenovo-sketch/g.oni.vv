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
#include "platform/FileApi.h"   // 0.7.2: DirEntry do navegador
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

// 0.7.5 — algum overlay MODAL aberto? (o main decide com isto se desenha o
// chrome do editor OU o backdrop; inclui TODOS: +/MENU/Settings/seletores/
// diálogo de armazenamento/import/logs/contextual/remoção/teclado/CENAS/
// navegador/aplicar)
bool anyOverlayOpen(const EditorState& st);

// 0.7.5 — BACKDROP MODAL: fundo opaco que tapa o ecrã todo por baixo de um
// overlay modal (nada do editor/canvas UI aparece por trás/à mista).
void drawModalBackdrop(UiContext& ui, f32 sw, f32 sh);

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
        PosX, PosY, // sliders ox/oy (design px) — ocultas em FILHOS de container
        SizeW, SizeH,   // sliders w/h
        ColR, ColG, ColB,   // sliders cor 0..1
        ColA,       // 0.7.4: slider ALPHA do fundo (0..1; Label nasce 0)
        VisToggle,      // "visivel: sim/nao" (botão re-despachado)
        AnchorH,        // "ancora H: ..." (cicla esquerda/centro/direita)
        AnchorV,        // "ancora V: ..." (cicla topo/meio/fundo)
        TextBtn,        // "texto: ..." (abre o teclado)
        HexBtn,         // 0.8.6: "hex: #RRGGBB" (abre o teclado hex)
        FontScl,        // 0.8.6: slider "letra: Nx" (escala da fonte)
        TStyleBtn,      // 0.8.6: "letra estilo: normal/negrito/italico"
        ActType,        // "acao: ..." (cicla none/show/hide/toggle/scene/spawn/trans)
        ActTarget,      // "alvo: ..." (abre o teclado)
        StyleBtn,       // 0.7.1: "estilo: fade|slide" (cicla) — acao trans
        Sens,           // 0.7.3: slider de sensibilidade (joystick)
        Remove,         // "remover elemento"/"remover joystick"
        TexBtn,         // 0.7.4: "tex: ..." (Panel/Button/Image → seletor)
        ParentBtn,      // 0.7.4: "colocar em: ..." (cicla containers do canvas)
        Spacing,        // 0.7.4: slider espaçamento (Menu/containers)
        Pad,            // 0.7.4: slider padding (containers)
        AlignBtn,       // 0.7.4: "alinhamento: start/center/end" (Menu/containers)
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
// 0.7.4 — o elemento tem TEXTURA selecionável? (Panel/Button/Image — o
// campo tex: do Inspector, ref em e.image)
bool uiElementHasTexture(UiElement::Kind k);
// 0.7.4 — o elemento tem ALINHAMENTO configurável? (Menu/VBox/HBox)
bool uiElementHasAlign(UiElement::Kind k);

// 0.7.4 — mapa da escolha do "+" (modo UI) → Kind do elemento.
//   1..7 = Panel..Article (kinds 0..6); 8 = JOYSTICK (−1 — TouchControls);
//   9/10 = VBox/HBox (kinds 7/8). 0 = nada/inválido.
int uiPlusChoiceKind(int choice);

u32 uiInspectorRowCount(const UiElement& e, const TextMetrics& m);
u32 uiInspectorPlan(const UiElement& e, const TextMetrics& m, UiInspRow* rows,
                    u32 cap);
f32  uiInspectorContentHeight(const UiElement& e, const TextMetrics& m);

// 0.7.3 — plano do INSPECTOR DO JOYSTICK (TouchControls editável: pos/
// tamanho/sensibilidade/cor/remover; o MESMO contrato de y cumulativo)
u32 uiJoystickPlan(const TextMetrics& m, UiInspRow* rows, u32 cap);
f32  uiJoystickContentHeight(const TextMetrics& m);

// desenha o Inspector de UI no painel direito (substitui o de TICs quando
// há elemento selecionado em modo UI). Devolve true se algum valor mudou.
bool drawUiInspector(UiContext& ui, Scene& scene, EditorState& st,
                     const InputState& in);

// ---- criação de elementos (+ do modo UI) ----------------------------------------

// cria o elemento no canvas do TIC selecionado (cria o canvas se o TIC não
// tiver um) e seleciona-o. kind como u32 p/ o dispatch do menu (+).
// Devolve false se não há TIC selecionado (o chamador faz o toast).
// 0.7.4: com um CONTAINER selecionado (ou um FILHO dele), o elemento novo
// nasce FILHO do container (irmão do selecionado) — "colocar em" no
// Inspector move depois.
// 0.7.5: SEM TIC selecionado (em modo UI) assegura/cria o TIC DE UI próprio
// (nome "UI", só com UiCanvas — sem mesh/body) e cria lá o elemento: criar
// UI NUNCA obrigou a selecionar/criar um TIC 3D.
bool uiAddElement(Scene& scene, EditorState& st, u32 kind, f32 sw, f32 sh);

// 0.7.5 — assegura/cria o TIC DE UI próprio: procura o TIC "UI" (o cria se
// não existir), garante o UiCanvas e seleciona-o. Devolve o handle
// (invalid só se a cena recusar a criação). O TIC aparece na Hierarchy como
// qualquer outro (renomeável/duplicável/removível).
Handle ensureUiTic(Scene& scene, EditorState& st);

// 0.7.4 — desliga um elemento do container (parent = "") CONSERVANDO a
// posição visual: as âncoras passam a esquerda/topo e ox/oy derivam do rect
// RESOLVIDO atual (o elemento fica exatamente onde estava). Puro/afervel.
void uiDetachElement(UiCanvas& c, i32 element, f32 sw, f32 sh,
                     const safe::Insets& ins);

// ---- 0.8.6 — GIZMOS de UI: matemática PURA de escalar/rodar -----------------

// ângulo do ponteiro em torno de (cx,cy), em GRAUS (atan2; -180..180)
inline f32 uiGizmoAngleAt(f32 cx, f32 cy, f32 px, f32 py) {
    return std::atan2(py - cy, px - cx) * 57.2957795131f;
}

// reancora o elemento para o RECT de design dado (w/h novos + ox/oy
// derivados da âncora — o inverso de elementRect; o MESMO padrão do
// uiDetachElement). Puro — os testes aferem as 6 combinações de âncoras.
void uiGizmoScaleToRect(UiElement& e, const UiRect& newRect,
                        f32 sw, f32 sh, const safe::Insets& ins);

// ---- gestão de TICs (menu contextual + diálogos) ---------------------------------

// menu contextual: devolve 0 nada / 1 Renomear / 2 Remover / 3 Duplicar /
// 4 Visibilidade / 5 Alinhar a vista (0.7.7 — só com hasCamera: copia a
// pose da orbit de edição para o transform da câmara). Fecha com toque
// fora (muta st).
int drawContextMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                    EditorState& st, const char* ticName, bool ticVisible,
                    bool hasCamera = false);

// diálogo de confirmação de remoção: 0 nada / 1 confirmar / 2 cancelar.
int drawRemoveDialog(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                     EditorState& st, const char* ticName);

// duplica o TIC (todos os componentes por valor; nome único com sufixo
// Godot-style ".001"). Devolve o handle novo (invalid se o alvo morreu).
Handle duplicateTic(Scene& scene, Handle h);

// ---- teclado in-app + input de texto ---------------------------------------------

// geometria do teclado (FONTE ÚNICA — desenho e testes partilham os rects):
// 4 linhas de teclas (A..I / J..R / S..Z + '_' / 0..9) + linha de baixo
// (ESPACO, abc/ABC, '-', APAGA, OK, CANCELAR) + linha do buffer. 10 colunas.
// 0.7.5: tecla de CASO (abc/ABC) entre o espaço e o '-' — minúsculas.
struct KeyboardLayout {
    UiRect dialog{};
    UiRect buffer{};
    UiRect key[4][10]{};
    u32    keyCount[4] = {9, 9, 9, 10};
    UiRect space{}, caseKey{}, dash{}, back{}, ok{}, cancel{};
    // rótulos das teclas (estáticos: "A".."Z", '_', "0".."9");
    // 0.7.5: `lower` devolve MINÚSCULAS (o toggle abc/ABC do teclado)
    static const char* keyLabel(u32 row, u32 col, bool lower = false);
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
// alvo de ação / cor hex de elemento / cor hex de TIC). Devolve true se
// aplicou (o nome vazio NÃO se aplica; hex inválido NÃO se aplica).
// O propósito 1 (nome de cena) fica para o CHAMADOR (0.7.1 — precisa do
// projeto, não da cena).
bool commitTextInput(Scene& scene, EditorState& st);

// válidos no teclado: letras MAIÚSCULAS (o atlas é mono brutalist — nomes
// da engine são PascalCase; o buffer fica uniforme), dígitos, '_', '-',
// espaço. Filtro defensivo (o teclado só OFERECE estas teclas).
bool uiTextCharAllowed(char c);

// 0.8.6 — COR POR CÓDIGO: "#RRGGBB" ↔ RGB 0..1 (pure, afervel no CI)
void uiHexFormat(const f32 rgb[3], char* out, u32 cap);
bool uiHexParse(const char* s, f32 out[4]);

// ---- 0.7.2: navegador de ficheiros + aplicar-após-import ------------------

// rótulo do CAMINHO atual (mostra ONDE procura; quando não cabe corta o
// INÍCIO e guarda o FIM: "...DCIM/Camera")
std::string browserPathLabel(const std::string& cwd, f32 maxW,
                             f32 (*measure)(const std::string&, void*),
                             void* user);

// mensagem de pasta VAZIA / sem acesso COM O CAMINHO (nunca um toast cego)
std::string browserEmptyMessage(const std::string& cwd, bool opendirFailed);

// overlay NAVEGADOR (0.7.2): raízes (Raiz/Download/Docs/Camera/Pictures) +
// [^ Subir] + lista com scroll (id 45 — diretorias primeiro) + caminho no
// TOPO. Devolve: 0 nada; 1..5 = raiz i; 6 = subir; 7.. = entrada (pick−7).
// Fecha com "fechar" ou toque fora (muta st.fileBrowser).
int drawFileBrowser(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                    EditorState& st, const std::string& cwd,
                    const std::vector<fileapi::DirEntry>& entries,
                    bool opendirFailed);

// diálogo APLICAR-APÓS-IMPORT: 0 nada / 1 = Sim (aplica) / 2 = Nao.
// Fecha com toque fora (= Nao).
int drawApplyDialog(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                    EditorState& st, const char* fileName, const char* ticName);

} // namespace editor
} // namespace vv
