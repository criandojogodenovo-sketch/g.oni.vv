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

// 0.9.6 (G1) — ECRÃS CHEIOS: os overlays que ocupam o ecrã TODO (Settings/
// Docs/Editor de script/janela de texto). Estão DENTRO do anyOverlayOpen;
// este predicado separa-os dos DIÁLOGOS flutuantes porque as regras de
// CAMADA são mais fortes: capturam TODO o toque (nada chega à cena/painéis
// por trás), escondem tudo o que desenha sobre a cena (glifos de TIC de
// áudio/câmara/luz, gizmos, marcadores) E a barra de baixo + status saem
// do ecrã (a camada fica: cena < painéis < modais < teclado).
bool fullscreenOverlayOpen(const EditorState& st);

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
        Sec,        // 0.12-B (B-2): cabeçalho de secção colapsável (payload
                    // = o índice 0..2 no EditorState::inspUiCollapsed)
        PosX, PosY, // sliders ox/oy (design px) — ocultas em FILHOS de container
        SizeW, SizeH,   // sliders w/h
        MargL, MargT, MargR, MargB,  // 0.12-B: margens do esticar (0..300)
        PivotX, PivotY,              // 0.12-B: pivô (0..1)
        Rot,                          // 0.12-B: rotação (graus, −180..180)
        StretchTgl,                   // 0.12-B: "esticar: sim/nao" (a 10.ª âncora)
        Opacity,                      // 0.12-B: opacidade (0..1)
        Dead,                         // 0.12-B: joystick — zona morta (0..0.4)
        Radius,                       // 0.12-B: joystick — raio (px; escreve size)
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
        Remove,         // "remover elemento" (a árvore: APAGA O TIC)
        TexBtn,         // 0.7.4: "tex: ..." (Panel/Button/Image → seletor)
        ParentBtn,      // 0.7.4: "colocar em: ..." (cicla containers)
        Spacing,        // 0.7.4: slider espaçamento (Menu/containers)
        Pad,            // 0.7.4: slider padding (containers)
        AlignBtn,       // 0.7.4: "alinhamento: start/center/end" (Menu/containers)
    };
    Kind kind;
    f32  y;
    f32  h;
    u64  id;
    u32  payload = 0;   // 0.12-B: extra por kind (a secção do cabeçalho)
    bool reset = false; // 0.12-B: a linha tem BOTÃO DE RESET (a zona à direita)
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

// ---- 0.12-B (B-2) — o plano do INSPECTOR DE CONTROLO (a árvore): 3
// SECÇÕES COLAPSÁVEIS (Transform/Aparência/Comportamento — o bit i de
// `collapsed` = a secção i fechada), os campos da base (margens/pivô/
// rotação/esticar/opacidade) e os CAMPOS POR TIPO (texto/letra p/ Label e
// Button; raio/zona morta/sens p/ Joystick). SEM "remover joystick" (o
// apagar é o TIC — a Hierarchy/"remover elemento"). FONTE ÚNICA partilhada
// com os pins (o MESMO contrato de y cumulativo).
u32 uiInspectorPlanControl(const UiElement& e, bool joystick,
                           const TextMetrics& m, u8 collapsed,
                           UiInspRow* rows, u32 cap);
f32  uiInspectorContentHeightControl(const UiElement& e, bool joystick,
                                     const TextMetrics& m, u8 collapsed);

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
// 0.12-B: DEPRECATED no fluxo de produção (a árvore é a via nova —
// ensureUiRoot abaixo); mantém-se para a compat 0.7.x (testes/legado).
Handle ensureUiTic(Scene& scene, EditorState& st);

// ---- 0.12-B (B-1) — a ÁRVORE de UI: os elementos são TICs filhos do UIRoot
//
// assegura a raiz: o primeiro TIC ativo com UIRoot; sem raiz, cria o TIC
// "UI" com o marcador (o UiCanvas já não nasce — deprecated; a compat do
// serializer promove os antigos). Seleciona a raiz (padrão SELEÇÃO-ÚNICA).
Handle ensureUiRoot(Scene& scene, EditorState& st);
// cria o elemento-NA-ÁRVORE (kind 0..8 = Panel..HBox, o mapeamento de
// uiPlusChoiceKind): TIC filho da raiz + Control com os MESMOS defaults de
// criação do addElement (tamanho/cor/texto — fonte única). false se a cena
// recusou (o chamador faz o toast honesto).
bool uiAddTreeElement(Scene& scene, EditorState& st, u32 kind, f32 sw,
                      f32 sh);
// "+Joystick" na árvore: TIC filho com Control(Kind::Joystick) +
// TouchControls (o runtime de input no MESMO TIC — a fonte da física nunca
// muda). Nasce selecionado em edição (o padrão do +Joystick).
bool uiAddTreeJoystick(Scene& scene, EditorState& st);

// 0.7.4 — desliga um elemento do container (parent = "") CONSERVANDO a
// posição visual: as âncoras passam a esquerda/topo e ox/oy derivam do rect
// RESOLVIDO atual (o elemento fica exatamente onde estava). Puro/afervel.
void uiDetachElement(UiCanvas& c, i32 element, f32 sw, f32 sh,
                     const safe::Insets& ins);

// ---- 0.12-B (B-3) — o CANVAS com a moldura do device: matemática PURA -------
//
// O seletor de proporção ANDROID (as chips do topo do modo UI): 0 = livre
// (o design de sempre — o ecrã inteiro), 1..3 = os presets 16:9 / 18:9 /
// 20:9. A MOLDURA é o rect da proporção escolhida, CENTRADO no espaço de
// design (o lado MAIOR casa com o design — o letterbox do device preview);
// a área FORA dela escurece (o que o device não vê). Nada disto desenha em
// Play — é chrome do editor.
constexpr u32 kUiDeviceAspectCount = 4;   // livre + 3 presets
const char* uiDeviceAspectName(u32 i);    // "livre" / "16:9" / "18:9" / "20:9"
f32  uiDeviceAspectValue(u32 i);          // 0 (livre) ou w/h (16/9, 2, 20/9)
// a moldura: o rect da proporção `aspect` centrado no design sw×sh (o
// lado maior casa; degenerado → o design inteiro)
UiRect uiDeviceFrameRect(u32 aspect, f32 sw, f32 sh);
// a geometria das CHIPS do topo do viewport (FONTE ÚNICA — o draw e os
// testes partilham os rects; a mesma casa do keyboardLayout)
struct UiCanvasChips {
    UiRect aspect{}, guides{}, grid{};   // em px DE ECRÃ (o viewport)
};
UiCanvasChips uiCanvasChipsRect(const UiRect& view);

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
// 0.11-H (FASE 1): `subtreeCount` = o nº de TICs que vão morrer (o root
// incluído — o main conta com countSubtree); >1 muda o texto para
// «Remover 'X' e a sub-árvore (N TICs)?» (o apagar de UM TIC mantém o
// texto de sempre, byte a byte).
int drawRemoveDialog(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                     EditorState& st, const char* ticName,
                     u32 subtreeCount = 1);

// duplica o TIC (todos os componentes por valor; nome único com sufixo
// Godot-style ".001"). Devolve o handle novo (invalid se o alvo morreu).
Handle duplicateTic(Scene& scene, Handle h);

// ---- 0.11-H (FASE 1) — OPERAÇÕES DE ÁRVORE da hierarquia --------------------
// O contrato do parêntelo/children já existia (Tic.parent + TransformSystem
// + serializer); o que faltava era a EDIÇÃO da árvore. Tudo PURO sobre a
// Scene (sem GL, sem storage) — afervel no CI (test_fase1_hierarquia).

// `h` é descendente de `ancestor` (ou o próprio)? Caminho pelos pais com
// guard de ciclo (um parent em ciclo termina no teto da capacidade — nunca
// pendura). Pura/afervel.
bool isDescendant(const Scene& scene, Handle ancestor, Handle h);

// O reparent de `src` para `newParent` é legal? `newParent` invalid = RAIZ
// (legal com src vivo). Recusas: src morto; newParent == src; newParent
// DENTRO da sub-árvore de src (mexe para o próprio descendente = ciclo).
// Pura/afervel — a MESMA função do drop do drag e do consumo no main.
bool canReparentTo(const Scene& scene, Handle src, Handle newParent);

// executa o reparent: MUDA SÓ o pai (o TRS local fica — o TransformSystem
// recomputa os mundos no próximo tick; decisão documentada: a matriz da
// casa não tem inverse/decompose, o «manter transform mundial» fica para
// uma fase futura se o dono quiser). false se canReparentTo recusa.
bool reparentTic(Scene& scene, Handle src, Handle newParent);

// sub-árvore de `root` (o root PRIMEIRO, pais antes de filhos, ordem de
// slot dentro de cada nível; guard de ciclo). Vazia se o root morreu.
std::vector<Handle> collectSubtree(const Scene& scene, Handle root);

// nº de TICs na sub-árvore (root incluído) — o contador do diálogo de
// remoção. 0 se o root morreu.
u32 countSubtree(const Scene& scene, Handle root);

// apaga a sub-árvore INTEIRA (root incluído; filhos primeiro NUNCA deixa
// parent pendurado para slot reutilizável — a corrupção latente do delete
// simples morre). Devolve o nº apagado. SEM undo (o op do UndoStack cobre
// UM TIC — sub-árvore não cabe; o diálogo diz «não há desfazer»).
u32 destroyWithChildren(Scene& scene, Handle root);

// deep clone: duplica root + TODA a sub-árvore (novos handles, nomes únicos
// Godot-style, estrutura espelhada, componentes por valor — a mesma
// política do duplicateTic). Devolve o handle do clone RAIZ (invalid se o
// root morreu). SEM undo (o duplicar de sempre também não empurra).
Handle duplicateWithChildren(Scene& scene, Handle root);

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

// 0.12-C (C-4) — O BREADCRUMB do picker (a FONTE ÚNICA; o draw e os pins
// partilham): os SEGMENTOS do caminho atual — o 1.º é a RAIZ navegável
// (o rótulo do kBrowserRoots que a casou; caminhos fora das raízes usam o
// basename), os seguintes os componentes. O path de cada segmento é um
// PREFIXO do cwd — tocar nele SALTA para a pasta (o main despacha).
struct BrowserCrumb {
    std::string label;   // "Raiz" / "Download" / "sub"
    std::string path;    // o prefixo absoluto (browserOpen nele)
};
std::vector<BrowserCrumb> browserCrumbs(const std::string& cwd);

// 0.12-C (C-4) — O FILTRO da pesquisa do picker (puro): os ÍNDICES (no
// vetor ORIGINAL) das entradas cujo nome contém a consulta
// (case-insensitive; consulta vazia = TODAS, pela ordem de entrada —
// diretorias primeiro, a ordem do listDirEntries intacta)
std::vector<u32> browserFilterEntries(const std::vector<fileapi::DirEntry>&
                                          entries,
                                      const char* query);

// mensagem de pasta VAZIA / sem acesso COM O CAMINHO (nunca um toast cego)
std::string browserEmptyMessage(const std::string& cwd, bool opendirFailed);

// overlay NAVEGADOR (0.7.2): breadcrumb SALTA para a pasta do segmento +
// pesquisa (propósito 10 — filtra a lista) + raízes (Raiz/Download/Docs/
// Camera/Pictures/Music) + [^ Subir] FIXO + lista com scroll (id 45 —
// diretorias primeiro, as linhas com ÍCONE/nome/seta da C-4).
// Devolve: 0 nada; 1..kBrowserRootCount = raiz i; kBrowserRootCount+1 =
// subir; kBrowserRootCount+2.. = entrada VISÍVEL i (o índice no ORIGINAL
// vai em *rowIndexOut). *crumbPickOut recebe o SEGMENTO tocado (0 = nada).
// Fecha com "fechar" ou toque fora (muta st.fileBrowser).
int drawFileBrowser(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                    EditorState& st, const std::string& cwd,
                    const std::vector<fileapi::DirEntry>& entries,
                    bool opendirFailed, int* crumbPickOut = nullptr,
                    u32* rowIndexOut = nullptr);

// diálogo APLICAR-APÓS-IMPORT: 0 nada / 1 = Sim (aplica) / 2 = Nao.
// Fecha com toque fora (= Nao).
// 0.12-QoL D — o pop-up da RECUPERAÇÃO do auto-save: «Encontrámos
// trabalho não guardado. Desejas recuperar?» com [Recuperar]/[Descartar]
// (o padrão do drawApplyDialog — o card centrado com véu; o toque fora
// devolve 2 = NÃO destrutivo: o autosave fica no disco para a próxima).
int drawAutosaveDialog(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                       EditorState& st);

int drawApplyDialog(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                    EditorState& st, const char* fileName, const char* ticName);

} // namespace editor
} // namespace vv
