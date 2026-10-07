#pragma once
// ui/EditorUi.h — painéis do editor sobre o quad batch da F1 (F3).
//
// Layout landscape (tema mono, sem cores novas):
//   ┌─────────┬──────────────────────┬───────────┐
//   │ toolbar (3 botões: Menu/Play/Settings) — F1    │
//   ├─────────┼──────────────────────┼───────────┤
//   │ HIERARQ │  VIEWPORT (pass 3D)  │ INSPECTOR │
//   │ esquerda│   centro (câmara     │  direita  │
//   │  + bot. │    orbit da F2)      │  sliders  │
//   ├─────────┴──────────────────────┴───────────┤
//   │ status line (fps/tics/verts/dc) — F1           │
//   └────────────────────────────────────────────────┘
//
// F4.1: a lista de TICs e o Inspector são ScrollRegions (ui/ScrollMath.h +
// UiContext::beginScroll) — conteúdo mais alto que o painel chega ao fundo
// (BodyComp / add TouchControls / TICs >10). Dentro da região: drag =
// scroll, tap re-despachado pelo painel (seleção/ação), slider mantém
// prioridade. O viewport central NÃO é região de scroll (orbit/pinch
// intactos). Overlays ("+" → 4 presets; Menu → Save/Load) fecham com toque
// fora do painel. IDs: toolbar 1..3 (F1), scroll 41/42, hierarquia 40 e
// 1000+, presets 20..23, menu de ficheiro 30..31, sliders 2000+, velx 2100,
// add TouchControls 3001.
//
// F4.2: safe-area — os painéis e o viewport central passam a viver DENTRO do
// contentRect do sistema (ui/SafeArea.h; insets injetados pelo main a partir
// de android_app->contentRect). Com a altura real dos painéis, o overflow do
// Inspector é detetado e o scroll ativa no C33 (B1); gestos atrás da nav bar
// não orbitam a câmara (centerRect com insets).
//
// F5-E: ASSETS — o Inspector mostra a origem do MeshRenderer (mesh/tex) e
// abre seletores (overlay mono) com o conteúdo de meshes/ e textures/ do
// projeto; o Menu ganha "Export OBJ" (mesh do TIC selecionado → meshes/).
// O catálogo (listas de nomes) é refresh pelo main a partir do ProjectStorage.
//
// F5.2: ARMAZENAMENTO — overlays do fluxo All Files Access (diálogo
// "Precisa de acesso a todos os ficheiros?" com Permitir/Cancelar), overlay
// de IMPORT com os ficheiros de Download/Documents (File API direta) e o
// VIEWER de logs in-app (engine.log + crash-*.dump, scroll, tema mono).
// O Settings mostra o modo de armazenamento ativo e ganha "Ver logs" e
// "Acesso a ficheiros…".
//
// F5.0-fix (bug do C33: texto do Inspector sobreposto em pilhas): o layout
// do Inspector vem do PLANO (ui/EditorLayout.h) — linhas sequenciais com
// cursor Y partilhado (y += altura_linha, nenhum reinício por secção) e
// alturas derivadas das MÉTRICAS REAIS da fonte (TextMetrics do atlas; as
// linhas antigas de 26 px eram pequenas demais para a fonte de 28 px — o
// bloco de glifos invadia a linha de cima). O desenho não tem "+=" próprio:
// consome o plano e subtrai o offset do scroll; contentHeight = fundo da
// última linha; o hit-test do tap re-despachado usa o MESMO plano.
//
// F5.2: IDs — faixas exclusivas por overlay (UiContext é immediate-mode:
// dois widgets com o MESMO id no mesmo frame partilham o gesto). Toolbar
// 1..3 · hierarquia 40/1000+ · scroll 41/42/43 · presets 20..23 · menu de
// ficheiro 30..3x · SETTINGS 4400+ · assets 6000+ · IMPORT 6100+ · diálogo
// de armazenamento 6300+ · viewer de logs 6400+.
#include "core/Handle.h"
#include "ui/UiContext.h"
#include "ui/EditorLayout.h"
#include "ui/TextWindow.h"   // 0.9.1: janela de texto pesado (st.textWin)
#include "ui/ScriptEditor.h"   // 0.9.2: editor de script V.ONI
#include "ui/DocsScreen.h"     // 0.9.2: Settings → Docs
#include "platform/FileApi.h"
#include "platform/StoragePerm.h"
#include "render/Primitives.h"   // 0.8.0: PrimParams no AssetResolvers
#include "ui/Icons.h"            // FASE 9 (G2-7): hierIconFor devolve icons::Icon

namespace vv {

class VoniSystem;   // 0.9.2: a central da V.ONI (fwd — o ponteiro chega ao drawInspector)
struct Tic;         // FASE 9 (G2-7): hierIconFor (core/Scene.h)

class Scene;
class InputState;
class TouchControls;
class Camera;

namespace editor {

// FASE 9 (G2-8): ids das regiões de scroll (o reset-por-troca-de-TIC e os
// testes precisam de os REFERENCIAR — antes eram internos do .cpp)
constexpr u64 kIdScrollHier = 41;   // F4.1: região de scroll da Hierarchy
constexpr u64 kIdScrollInsp = 42;   // F4.1: região de scroll do Inspector

// re-export: a largura dos painéis agora vive em ui/SafeArea.h (fonte única)
constexpr f32 kPanelW = safe::kPanelW;

// F5-E: catálogo de assets do projeto (nomes DENTRO de meshes/ e textures/,
// ordenados — o main faz listDir no storage; vazio = pasta sem assets).
// Nota F5: sem scroll no seletor (lista capada) — mais assets = F8.
struct AssetCatalog {
    std::vector<std::string> meshes;     // nomes de ficheiros ("quad.obj")
    std::vector<std::string> textures;   // nomes de ficheiros ("wood.png")
    std::vector<std::string> audio;      // 0.8.11: clips .gi ("audio/x.gi")
};

struct EditorState {
    Handle selected = Handle::invalid();   // TIC selecionado na Hierarchy
    bool   plusMenu = false;               // overlay de criação aberto
    bool   hierMenu = false;   // 0.9.6.10 (GRUPO UI): o menu â® da Hierarquia
    bool   fileMenu = false;               // overlay Menu (Save/Load) aberto
    bool   settingsMenu = false;           // F5.1-hotfix: overlay Settings aberto
    int    assetMenu = 0;                  // F5-E: 0 fechado; 1 = seletor mesh;
                                           //       2 = seletor textura
    // F5.2: overlays de armazenamento/logs
    bool   storageDialog = false;          // diálogo All Files Access visível
    bool   importMenu = false;             // overlay IMPORT (Download/Documents)
    bool   logViewer = false;              // viewer de logs visível
    bool   logViewerJustOpened = false;    // 1 frame: auto-scroll p/ o fundo

    // 0.8.12 — GUARDA DOS PICKERS: o Inspector seta quando um toque em
    // linha de picker foi BLOQUEADO por falta de alvo válido (sem TIC com
    // MeshRenderer); o main converte o flag em toast de hint + linha de
    // log "ui: pick bloqueado (sem seleção)" NO MESMO frame. Vive no
    // estado (não numa global do main) para ser afervável na suíte.
    bool   pickBlockedHint = false;

    // FASE 9 (G2-7 — long-press no nome truncado da hierarquia): o contador
    // de frames do dedo PARADO na linha + o tip pendente (o main converte
    // em toast). ~30 frames = 0.5s @60fps.
    char   nameTip[64] = "";        // não-vazio = mostrar o nome completo
    i32    hierHoldRow  = -1;        // linha com o dedo em cima do nome
    u32    hierHoldFrames = 0;
    bool   hierHoldShown  = false;

    // FASE 9 (G2-8 — o Inspector volta ao TOPO quando o TIC selecionado
    // MUDA): o handle visto no frame anterior (scroll reset por troca)
    Handle inspPrevSelected{};

    // 0.6.8 — MODO DE UI (EDITOR ↔ PLAY). Em PLAY: viewport fullscreen +
    // TouchControls + BARRA PLAY MÍNIMA (Stop/fps/aviso); SEM toolbar,
    // SEM painéis de edição, SEM menus, orbit DESATIVADO (1 dedo =
    // controlos). O estado vive AQUI (não numa global do main) para a
    // transição completa editor→play→editor ser testável na suíte.
    bool   playMode = false;

    // ---- 0.7.0 — UI CRIÁVEL + GESTÃO DE TICs ---------------------------------
    bool   uiMode = false;          // separador "3D | UI": true = viewport 2D
    bool   audioMode = false;      // 0.8.11: separador "3D | UI | ÁUDIO"
                                    // dedicado à UI (editor WYSIWYG)
    i32    selElement = -1;         // elemento selecionado no canvas do TIC
                                    // selecionado (−1 = nenhum)
    bool   selJoystick = false;    // 0.7.3: o joystick (TouchControls) do TIC
                                    // selecionado está em edição no viewport 2D
    bool   elDrag = false;         // drag WYSIWYG em curso (viewport 2D)
    f32    elDragX = 0.0f;         // pos do dedo no frame anterior (px ecrã)
    f32    elDragY = 0.0f;
    // 0.8.6 — GIZMOS de UI (escalar/rodar, coerentes com os gizmos 3D):
    u8     elGizmoMode = 0;        // 0 = mover (drag de sempre), 1 = escalar
                                   // (handle de canto), 2 = rodar (handle ⊙)
    f32    elGizAnchorX = 0.0f;    // escalar: canto OPOSTO fixo (design px)
    f32    elGizAnchorY = 0.0f;
    f32    elGizStartW = 0.0f;
    f32    elGizStartH = 0.0f;
    f32    elGizStartAng = 0.0f;   // rodar: ângulo do dedo no press (rad)
    f32    elGizStartRot = 0.0f;   // rodar: rot do elemento no press (graus)

    // gestão de TICs — menu contextual (⋮ na Hierarchy) + diálogos
    bool   contextMenu = false;     // menu Renomear/Remover/Duplicar/Visibilidade
    Handle contextTic{};            // TIC alvo do menu contextual
    bool   removeDialog = false;    // confirmação de remoção (substitui o
                                    // "apagar" sem confirmação)
    bool   scenesMenu = false;      // 0.7.1: overlay CENAS (lista/nova/trocar)
    bool   fileBrowser = false;     // 0.7.2: overlay NAVEGADOR de ficheiros
    bool   applyAsk = false;        // 0.7.2: diálogo "aplicar ao TIC?" pós-import

    // desselecionar no viewport 3D: arm no press, limpa no release se o dedo
    // não se mexeu (tap ≠ drag de orbit)
    bool   deselectArm = false;
    f32    deselectX = 0.0f, deselectY = 0.0f;

    // TECLADO IN-APP + input de texto (renomear/texto de elemento/alvo de
    // ação; NOME DE CENA na 0.7.1). Zero IME de sistema (frágil em
    // NativeActivity) — overlay mono com A-Z, 0-9, '_', '-', espaço.
    // 0.7.5: toggle de CASO abc/ABC (st.kbLower) — MINÚSCULAS disponíveis
    // (o atlas tem ambos os casos; dígitos/'_' não mudam).
    bool   textInput = false;       // overlay do teclado visível
    int    textPurpose = 0;         // 0 = renomear TIC, 1 = nome de cena,
                                    // 2 = texto de elemento, 3 = alvo de ação
    Handle textTic{};               // alvo do renomear
    i32    textElement = -1;        // alvo do texto/alvo (índice no canvas)
    char   textBuf[40] = "";       // buffer em edição
    u32    textLen = 0;
    bool   kbLower = false;         // 0.7.5: teclado em minúsculas (abc/ABC)

    // 0.7.6 — G5 da toolbar: o painel do Inspector está visível? (o botão
    // de ícone alterna; escondido, a área do painel junta-se ao viewport
    // central — ver safe::centerRect(sw,sh,in,rightPanel))
    bool   showInspector = true;

    // 0.9.0 (spec D) — modo SELECIONAR da toolbar do viewport (cursor): sem
    // gizmo; o toque no viewport seleciona TICs. Vive no estado para ser
    // afervel no CI.
    bool   selectMode = false;

    // ---- 0.9.0 (spec B + scope funcional) ----------------------------------
    // PESQUISA de TIC por nome no header da hierarquia (o buffer vive AQUI —
    // o teclado in-app propósito 8 escreve nele; vazio = sem filtro)
    char   hierSearch[32] = "";
    u32    hierSearchLen = 0;

    // MULTI-SELEÇÃO: conjunto de handles (o gizmo aplica a TODOS). Gestão:
    // toque na linha JÁ selecionada ARRANCA a multi-seleção; toques seguintes
    // alternam linhas no conjunto; conjunto vazio = seleção simples de novo.
    // (Decisão documentada: sem cronometragem de long-press — o immediate-
    // mode da lista partilha o gesto com o scroll; o toque duplo é
    // determinístico e afervável no CI.)
    Handle multiSelect[16] = {};
    u32    multiSelectCount = 0;
    // DRAG de reparenting (parenting visual): arrastar do ÍCONE DE TIPO de
    // uma linha para outra linha reparenta (drop no vazio = raiz)
    bool   hierDrag = false;
    Handle hierDragSrc{};
    Handle hierDragDst{};

    // 0.9.0 (spec C/G) — SECÇÕES COLAPSÁVEIS do Inspector (bitmask por
    // secção — ver kInspBit* no EditorLayout.h). PERSISTE no layout.json
    // (spec G) e o "Repor layout" devolve TUDO aberto.
    u32    inspCollapsed = 0;

    // 0.9.0 (spec D 🔶) — POPOVER de snap/grelha (o "sliders" da top bar e o
    // [viewport settings] da toolbar do viewport abrem o MESMO popover)
    bool   vpSettingsMenu = false;

    // PASSO 3 (0.9.6.17): a âncora do menu de ficheiro — o ⋯ do viewport
    // arma a folha ANCORADA a ele (ax/ay em px; −1 = a âncora de sempre, o
    // botão Menu da top bar). O glifo/menu são os mesmos; só a âncora muda.
    f32    menuAx = -1.0f;
    f32    menuAy = -1.0f;

    // 0.9.0 (spec E) — altura do DRAWER aberto neste frame (0 = fechado).
    // O main injeta do BottomState; os PAINÉIS (hierarquia/inspector) e o
    // viewport central encolhem por ela (safe::panelsRect).
    f32    drawerH = 0.0f;

    // 0.9.0 (spec I/G) — secções colapsáveis da PÁGINA de Settings (bitmask;
    // PERSISTE no layout.json junto com o resto do layout)
    u32    settingsCollapsed = 0;

    // GRUPO D (0.9.6.7) — LARGURAS DOS PAINÉIS (divisores arrastáveis
    // hierarquia|viewport|inspector, o padrão da pega do drawer). Em dp;
    // −1 = default adaptativo (kPanelW em ecrãs largos, encolhe nos
    // estreitos — safe::defaultPanelW). PERSISTE no layout.json junto ao
    // resto do layout (spec G); o clamp vivo é o da safe::clampPanelW.
    f32    hierW = -1.0f;
    f32    inspW = -1.0f;
    u32    inspTab = 0;        // 0.9.6.10: 0=Inspector · 1=Nós — 0.9.6.18
                               // (D5): a vista Nós MORREU (duplicado da
                               // hierarquia); o campo fica (compat de
                               // formato) e o draw manda-o a 0
    // P2-bis (0.9.6.16 — a decisão do dono sobre «fixar aberto»): o PIN do
    // inspector. Tocar no ÍCONE do trilho (32dp, sem seleção) ABRE o painel
    // E FIXA (inspPinned=true — PERSISTE no layout.json, spec G); a seta de
    // recolher no cabeçalho DESFAZ o pin (e o painel fecha se não houver
    // seleção). Sem long-press — toggle simples. Enquanto fixado, o painel
    // mantém-se aberto MESMO sem seleção (a regra é do dono).
    bool   inspPinned = false;
    // drag dos divisores (press arma; o movimento horizontal redimensiona
    // AO VIVO com clamp; release fixa — o padrão do drawer)
    bool   divDragActive = false;
    i32    divDragSlot = -1;
    f32    divDragStartX = 0.0f;
    f32    divDragBaseW = 0.0f;
    bool   divDragRight = false;   // true = divisor do INSPECTOR

    // 0.9.1 — JANELA DE TEXTO PESADO (modal — portrait + IME do sistema;
    // a semente do editor de script 0.9.2). O gate anyOverlayOpen cobre:
    // nada do editor desenha/interage atrás dela.
    textwin::State textWin;

    // ---- 0.9.2 — V.ONI ----------------------------------------------------
    // editor de script (modal portrait+IME — o par inseparável do textWin)
    scriptwin::State scriptWin;
    // ecrã de Docs (Settings → Docs; landscape, com pesquisa in-app)
    docswin::State docsScreen;
    // pedido do Inspector para abrir o editor (o main consome com o par
    // portrait+IME e limpa o flag)
    bool requestScriptEditor = false;
    Handle scriptEditorTarget{};
    // vars @+ do TIC selecionado (o drawInspector preenche por frame a
    // partir do VoniSystem — exibição do Inspector §4)
    u32 scriptVarCount = 0;
    struct ScriptVarView {
        char name[32] = "";
        char value[48] = "";
    };
    ScriptVarView scriptVars[8];
};

// Rect do viewport central (entre os painéis) — usado para o gate da câmara.
// Versão SEM safe-area (insets zero) mantida para compat/testes.
UiRect centerRect(f32 sw, f32 sh);
// F4.2: com insets do sistema — gestos atrás da nav/status bar não orbitam.
UiRect centerRect(f32 sw, f32 sh, const safe::Insets& in);
// 0.7.6: com o painel DIREITO opcional (G5 escondeu o Inspector — a área
// dele junta-se ao viewport central; o esquerdo fica sempre).
UiRect centerRect(f32 sw, f32 sh, const safe::Insets& in, bool rightPanel);
// 0.9.0: com o DRAWER do painel de baixo (spec E — o viewport central
// encolhe pela altura do drawer aberto)
UiRect centerRect(f32 sw, f32 sh, const safe::Insets& in, f32 drawerH,
                  bool rightPanel);
// GRUPO D: COM AS LARGURAS DE ESTADO dos painéis (divisores arrastáveis)
// — o rect que o EDITOR 3D usa (scissor/orbit/chrome acompanham);
// PASSO 2: inspTrack colapsa o inspector ao trilho de 32dp
UiRect centerRect(f32 sw, f32 sh, const safe::Insets& in, f32 drawerH,
                  bool rightPanel, f32 hierWdp, f32 inspWdp,
                  bool inspTrack = false);
// resolve o PAR de larguras efetivas de um EditorState (a fonte ÚNICA é a
// safe::resolvePanels — gangorra dos três pisos; o draw dos painéis, o drag
// dos divisores, a sombra do save e os testes partilham ESTA)
safe::PanelBudget resolveEditorPanels(const EditorState& st, f32 contentWdp);

// ---- 0.9.6.18 (HOTFIX D2) — O ORÇAMENTO DA LINHA TRANSFORM -----------------
// A fonte ÚNICA do draw E do re-despacho do tap (o campo Z cortado nascia
// de DUAS fórmulas). Pura/afervel — a sentinela R-035 afirma os pisos e o
// teto «nada fora do rect». Em DP (a versão px é interna ao TU).
//   1. painel largo (≥256dp úteis): caixas 64 + reset CHIP 40
//   2. encolhe: caixas (útil−24−40)/3 com PISO 48 — o chip fica
//   3. abaixo do piso 48: reset ÍCONE inline 20 — caixas (útil−24−20)/3
//      com PISO 40 (no painel 180dp dá 40 exato)
struct TransformBudget {
    f32  boxW = 64.0f;      // a caixa X/Y/Z (dp)
    f32  resetW = 40.0f;    // o alvo do reset (40 chip · 20 ícone inline)
    bool resetIcon = false; // true = reset ÍCONE inline após o Z
};
TransformBudget transformRowBudget(f32 usableWdp);

// PASSO 2 (0.9.6.15 — spec do dono): SEM seleção o INSPECTOR colapsa ao
// trilho de 32dp (safe::kInspTrackW) — a área junta-se ao viewport e volta
// ao selecionar um TIC (na hierarquia ou na aba Nós). Puro/afervel: os
// rects (safe), o draw (drawInspector), o divisor direito e os testes
// partilham ESTA pergunta — a fonte ÚNICA do colapso. A MULTI-seleção
// conta como seleção (o gizmo vive nela).
// P2-bis (0.9.6.16): o PIN manda sobre a ausência de seleção — fixado, o
// painel fica aberto MESMO sem seleção (a regra do dono). A seta de
// recolher limpa o pin e o trilho volta (se não houver seleção).
inline bool inspectorCollapsed(const EditorState& st) {
    return !st.inspPinned && st.selected == Handle::invalid() &&
           st.multiSelectCount == 0;
}

// GRUPO D (0.9.6.7) — DIVISORES ARRASTÁVEIS hierarquia|viewport|inspector
// (o padrão da pega do drawer: strip 12dp + hit 24dp — PASSO 2, press arma,
// o move redimensiona AO VIVO com clamp, release fixa; persiste no
// layout.json). dividerInput corre ANTES dos painéis no UI pass (a pega
// RECLAMA o gesto primeiro — o toque na pega nunca vira scroll/orbit/gizmo);
// drawPanelDividers corre DEPOIS (o strip visível por cima da borda).
// PASSO 2: com o inspector no TRILHO (inspectorCollapsed) não há divisor
// direito — não há largura a arrastar.
void dividerInput(UiContext& ui, const InputState& in, EditorState& st,
                  bool rightPanel);
void drawPanelDividers(UiContext& ui, const EditorState& st, bool rightPanel);

// Painel esquerdo: lista de TICs COM SCROLL (todas as entradas, sem corte) +
// botão "+" no cabeçalho. Tap numa linha seleciona (re-despacho do scroll).
// Devolve true apenas no frame em que "+" é clicado (main abre o menu).
// FASE 9 (G2-7): o ícone de tipo da linha da HIERARQUIA (BodyComp
// primeiro — tic_static/tic_player/tic_rigid; depois câmara/áudio/UI/
// mesh/entrada). Pura e afervável.
icons::Icon hierIconFor(const Tic& t);
bool drawHierarchy(UiContext& ui, Scene& scene, EditorState& st);

// Painel direito: componentes do TIC selecionado COM SCROLL — Transform3D
// com 9 sliders (pos/rot graus/scale), MeshRenderer (mesh + tex com seletor
// de assets, F5-E), InputMap, BodyComp (tipo·forma·chão + velx) e botão
// "add TouchControls" no fundo, sempre alcançável. Já atualiza
// tr->updateWorld() ao editar.
// `catalog` (não-dono, pode ser null) habilita os SELETORES: sem catálogo as
// linhas de mesh/tex são só leitura (modo degradação, usado nos testes).
// Devolve true se algum slider alterou valores neste frame.
bool drawInspector(UiContext& ui, Scene& scene, EditorState& st,
                   const AssetCatalog* catalog = nullptr,
                   const VoniSystem* voni = nullptr);

// F4: controlos de toque (só em modo Play, só se algum TIC ativo tem o
// componente) — joystick quadrado + botão JUMP no quad batch, tema mono.
// F4.2: posicionados dentro da safe-area (o layout fixo do TouchControls é
// recalculado para a área útil e deslocado pelos insets — nada atrás da
// nav bar).
// 0.7.4: o corpo vive em drawTouchControlsAt (PARAMETRIZÁVEL — o viewport
// 2D do editor desenha o MESMO joystick com o transform do mini-ecrã;
// paridade de aparência editor↔Play). ox/oy = origem da ÁREA ÚTIL no
// espaço de desenho; aw/ah = área útil em px de DESIGN; scale = design→desenho.
void drawTouchControls(UiContext& ui, const TouchControls& tc, f32 sw, f32 sh);
void drawTouchControlsAt(UiContext& ui, const TouchControls& tc, f32 ox,
                         f32 oy, f32 aw, f32 ah, f32 scale);

// ---- 0.6.8: PLAY MODE com janela própria ------------------------------------

// Estado do gesto de orbit ENTRE frames (era globais do main — agora é
// puro/testável: o main cria um OrbitState e passa por referência).
struct OrbitState {
    bool active = false;         // 1 dedo em orbit
    f32  x = 0.0f, y = 0.0f;     // último ponto do dedo de orbit
    f32  pinchPrev = 0.0f;      // distância entre dedos no frame anterior
    bool gestureInView = false;  // F3: gesto nasce só no viewport central
};

// Orbit da câmara (extraído do main p/ ser afervel no CI).
// Regra F3: o PRIMEIRO toque decide o dono do gesto (nascido no viewport
// central e não reclamado pelos TouchControls). 1 dedo = yaw/pitch,
// ≥2 dedos = pinch. 0.6.8: playMode=true DESATIVA o orbit (1 dedo =
// controlos) e RESETA o gesto pendente — o estado fica limpo p/ o regresso.
void updateCameraOrbit(Camera& cam, OrbitState& st, const InputState& in,
                       const UiRect& view, u32 claimedMask, bool playMode);

// Fecha TODOS os overlays/menus (transição EDITOR→PLAY: em play nada de
// edição fica aberto; ao parar, os PAINÉIS voltam exatamente — os offsets
// de scroll e a seleção vivem fora destas flags e ficam intactos).
void closeAllOverlays(EditorState& st);

// Barra PLAY mínima (topo, mesma altura da toolbar, dentro da safe-area):
// botão Stop à esquerda + "a correr" + fps + aviso "simulação — alterações
// descartadas ao parar" à direita. Devolve true no frame em que Stop é
// clicado (o main sai do play: PlaySnapshot restore + painéis repostos).
bool drawPlayBar(UiContext& ui, const InputState& in, f32 sw, f32 sh, int fps);

// ---- 0.7.0: UI CRIÁVEL + GESTÃO DE TICs (ui/UiEditor.h tem o resto) ----------
// (0.7.6: o separador "3D | UI" e o grupo Mover/Rodar/Escalar/Snap passaram
// a GRUPOS G3/G4 da BARRA FINAL — ui/Toolbar.h; as funções antigas
// drawModeToggle/drawGizmoToolbar foram removidas)

// DESSELECCIONAR no viewport 3D: arm no press edge dentro do viewport,
// limpa a seleção no release se o dedo não se mexeu (tap ≠ drag de orbit).
// Puro e afervel no CI (o main chama por frame com a máscara de claims).
bool viewportTapClearsSelection(EditorState& st, const InputState& in,
                                 const UiRect& view, u32 claimedMask);

// ---- 0.8.12 — GUARDA DOS PICKERS + SOBREVIVÊNCIA DA SELEÇÃO ------------------
// GUARDA: true quando o TIC selecionado NÃO é alvo válido para os pickers
// de mesh/prim/tex do MeshRenderer (handle morto, TIC sem MeshRenderer —
// câmara/áudio/ui ficam de fora). O Inspector NÃO abre o picker e o
// dispatch NÃO aplica sem alvo: em vez do caminho "ERRO(sem TIC com mesh
// selecionado)" (que não diz ao dono o que fazer), o toque dá HINT
// "seleciona um TIC com mesh" + a linha "ui: pick bloqueado (sem seleção)".
// Puro e afervel no CI.
bool pickerGuardBlocked(const Scene& scene, Handle selected);

// RE-VALIDAÇÃO pós-lifecycle: o INIT_WINDOW recarrega a cena e os handles
// dos TICs MORREM (identidades novas); a seleção RE-MAPEIA por NOME (o TIC
// continua a existir — só o handle é novo). Se o handle ainda está vivo
// (load in-place), mantém-se. Handle inválido só se o TIC desapareceu.
// Puro e afervel no CI (o main chama após o load do INIT_WINDOW).
Handle revalidateSelection(const Scene& scene, Handle selected,
                           const char* name);

// ---- 0.6.9 → 0.7.6: GIZMOS DE TRANSFORMAÇÃO ----------------------------------
// (o seletor de modo/snap é o GRUPO G4 da barra final — struct GizmoModeState
// e tudo o resto vive em ui/Toolbar.h; a antiga drawGizmoToolbar desapareceu)

// Overlays. Devolvem a escolha do frame:
//   drawPlusMenu → 0 nada, 1..4 = PresetKind (1=Player, 2=Character,
//                  3=Static, 4=Rigid); 0.7.3: modo UI 1..7 = elementos
//                  (Panel..Article), 8 = Joystick; 0.7.4: 9/10 = VBox/HBox
//   drawFileMenu → 0 nada; 1..14 — 0.9.6.10 (GRUPO UI · a imagem 1): o
//                  menu de 6 SECÇÕES (Projeto/Cena/Editar/Visualizar/
//                  Ferramentas/Ajuda): 1 Sair p/ projetos · 2 Importar… ·
//                  3 Export Downloads · 4 Guardar cena · 5 Carregar cena ·
//                  6 Export OBJ · 7 Desfazer · 8 Refazer · 9 Duplicar ·
//                  10 Colar · 11 Snapping (toggle real) · 12 Settings ·
//                  13 Ver logs · 14 Documentação V.ONI. 0.9.0 (spec H):
//                  SHEET ANCORADO 8dp sob o botão (ax/ay; −1 = centrado,
//                  compat com os testes); 0.9.6.10: COM SCROLL quando o
//                  conteúdo excede o ecrã e o RÓTULO do Snap dinâmico
//                  (snapOn — o estado real do íman)
//   drawAssetMenu → 0 nada; >0 = item 1-based do seletor ativo
//                    (st.assetMenu: 1 = meshes → 1 = "cube", 2.. = ficheiros;
//                     2 = texturas → 1 = "none", 2.. = ficheiros; 0.7.4:
//                     3 = textura de ELEMENTO de UI → 1 = none, 2.. =
//                     ficheiros, kAssetPickImport = "importar…" (navegador))
//     withImport (0.7.4) — acrescenta a linha "importar…" (só menuKind 3)
// Todos fecham com toque fora do painel (mutam st) e centrados na safe-area.
int drawPlusMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh, EditorState& st);
// 0.9.6.10 (GRUPO UI): as linhas do menu de 6 secções — ids 7400+ (a
// faixa 30..43 antiga colidia com os kVp*Id 31..36 do viewport) e o
// scroll do sheet (kMenuScrollId 7410 — faixa exclusiva)
constexpr u64 kMenuRowBase   = 7400;
constexpr u64 kMenuScrollId  = 7410;
constexpr u64 kInspTabBase   = 7430;   // 0.9.6.10: tabs Inspector/Nós —
                                       // 0.9.6.18 (D5): a 2ª tab MORREU
                                       // (duplicado da hierarquia); o id
                                       // fica RESERVADO na faixa (os
                                       // formatos antigos de layout podem
                                       // referi-lo; nenhuma vista desenha)
// P2-bis (0.9.6.16): o PAR do pin — o ícone do TRILHO abre E fixa (o
// touch é a célula toda de 32dp de largura × 40dp de altura — a LEI DE
// OURO no toque; a largura é a do trilho, o elemento 32dp da spec do
// dono — a mesma classe de piso das tabs de baixo, kFieldFloorDp).
// 0.9.6.18 (D1): a SETA de recolher vive agora na LINHA 1 do cabeçalho
// (à DIREITA — o contrato «título à esquerda + pin/fechar à direita»);
// a linha própria de 28dp do P2-bis morreu (o espaço foi libertado pela
// remoção das tabs do D5).
constexpr u64 kInspTrackPinId = 7432;  // o ícone do trilho (abre + fixa)
constexpr u64 kInspUnpinId    = 7433;  // a seta de recolher (desfaz o pin)
constexpr f32 kInspUnpinCellW = 40.0f; // dp — a célula do recolher na linha
                                       // 1 (o alvo do toque, 40×28; o piso
                                       // da classe cabeçalho kHeadFloorDp)
constexpr u64 kHierDotsId    = 7420;   // ⋮ da Hierarquia (GRUPO UI)
constexpr u64 kHierMenuRowBase = 7421; // linhas do menu ⋮ (2)
int drawFileMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                 EditorState& st, f32 ax = -1.0f, f32 ay = -1.0f,
                 bool snapOn = false);
// 0.9.6.10 (GRUPO UI): o menu ⋮ da Hierarquia (1 = limpar multi-seleção ·
// 2 = nome completo do selecionado; fechado ao toque fora)
int drawHierMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                 EditorState& st, f32 ax, f32 ay);
int drawAssetMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh, EditorState& st,
                  const AssetCatalog& catalog, bool withImport = false);

// ---- F6: DISPATCH da escolha do seletor (wiring material/textura) ------------
//
// FIX do C33 (0.6.9): tocar "tex:" → escolher o PNG importado → NADA acontecia
// (estado ficava "tex: none", cubo sem textura, engine.log sem linha de
// aplicação NEM de erro). CAUSA RAIZ: drawAssetMenu fecha o seletor NO CLIQUE
// (st.assetMenu = 0 antes do return) e o dispatch no main.cpp lia
// g_editor.assetMenu DEPOIS da chamada → sempre 0 → o bloco `if (pick > 0)`
// era CÓDIGO MORTO desde a F5-E (0.5.0): a escolha nunca chegava ao
// MeshRenderer. O mesmo no seletor de mesh.
//
// A lógica de aplicação vive AGORA aqui — função PURA (GL-free, resolvers
// injetados, afervel no CI; o padrão de StoragePerm.h/JniAttach.h). O
// chamador captura o menuKind ANTES de drawAssetMenu e aplica o pick depois.
// O render é immediate-mode (drawTics lê mr->texture a CADA frame → o bind
// acontece no frame seguinte sem flags dirty); a persistência já gravava
// texPath no .goni (SceneSerializer) — só o wiring é que estava partido.

// (Texture/Mesh vêm já forward-declarados em namespace vv via MeshRenderer.h
// no cadeia de includes; LitMaterial está completo via render/Material.h)

// Resolvers do device — o main liga-os ao GpuAssets/ResourceManager/cubo
// procedural/material lit (injeção → os testes usam stubs).
// 0.8.0 (F7): `prim` resolve PRIMITIVAS PROCEDURAIS pelo cache do main
// (assinatura = tipo+parâmetros → Mesh* único partilhado).
// 0.8.9 — alvo da NORMALIZAÇÃO UNIFORME do import: o maior eixo do AABB do
// mesh fica com ESTE tamanho (2 unidades ≈ cubo da engine); fator ÚNICO nos
// 3 eixos — proporções preservadas, NUNCA espalmado.
constexpr f32 kImportTargetSize = 2.0f;
struct AssetResolvers {
    Mesh* (*mesh)(const std::string& ref) = nullptr;   // GpuAssets::mesh
    const Texture* (*texture)(const std::string& relPath, std::string* warn) = nullptr;
    std::string (*meshTextureFor)(const std::string& ref) = nullptr;  // glTF embutida
    Mesh*       cubeMesh = nullptr;      // cubo procedural do main
    LitMaterial* material = nullptr;    // lit do renderer
    // 0.8.10: o resolver `prim` MORREU com o cache — o pick ARMA o pedido
    // (primOn+params+mesh=null) e o main sobe no ponto seguro do frame.
    // 0.8.9: EXTENSÃO do AABB do mesh COMO DADOS (o applyAssetPick é PURO e
    // NUNCA desreferencia o Mesh — contrato dos testes com stubs-ponteiro;
    // o main liga-a ao boundsExtent do Mesh real). Null = sem normalização
    // (o chamador decide não normalizar).
    Vec3 (*meshExtent)(const std::string& ref) = nullptr;
};

// Resultado de uma escolha (feedbacks ficam pelo chamador: toast + engine.log)
struct AssetPickOutcome {
    bool applied = false;   // a escolha CHEGOU ao MeshRenderer
    char toast[64] = "";    // "" → sem toast
    char log[160] = "";     // "" → sem linha no engine.log
};

// Aplica a escolha do seletor no MeshRenderer do TIC selecionado.
//   menuKind: 1 = seletor de meshes, 2 = seletor de texturas (o valor de
//             EditorState::assetMenu CAPTURADO ANTES do drawAssetMenu),
//             4 = seletor de PRIMITIVAS (0.8.0)
//   pick:     1 = cube (mesh) / none (textura) / none (primitiva); 2.. =
//             ficheiro do catálogo (kinds 1/2) OU PrimKind 0..7 (kind 4)
// Regras: escolher textura → texture + texPath + log "material: textura
// aplicada <ref>"; remover (pick 1) → liberta a referência + log "material:
// textura removida"; carga que falha → estado ANTERIOR intacto + toast de
// falha; TIC morto/sem MeshRenderer/pick fora do catálogo → outcome vazio.
// 0.8.0: escolher PRIMITIVA → primOn+params (defaults do tipo) + mesh do
// resolver + log "editor: primitiva <nome> aplicada"; escolher cube/file
// LIMPA o prim (uma fonte de mesh de cada vez); escolher none (kind 4)
// desliga o prim (mesh null — "prim: -" no Inspector).
AssetPickOutcome applyAssetPick(Scene& scene, Handle selected, int menuKind, int pick,
                                const AssetCatalog& catalog, const AssetResolvers& res);

// ---- 0.7.4: DISPATCH da escolha do seletor de TEXTURA DE ELEMENTO (menuKind 3)
//
// O C33: "o elemento Image não tem como escolher/importar a imagem" (e
// Button/Panel também não tinham textura de fundo). O seletor abre no
// Inspector de UI (linha tex:) e a escolha escreve na REF do elemento
// (e.image = "textures/<ficheiro>"); a RENDERIZAÇÃO resolve a ref POR FRAME
// pelo mesmo imgResolve do UiContext (o caminho do elemento Image 0.7.0 —
// agora partilhado por Panel/Button/Image). Sem GPU aqui: é PURE DATA.
//   pick 1 = none (limpa a ref); 2.. = ficheiro do catálogo textures/;
//   kAssetPickImport = o chamador abre o NAVEGADOR (0.7.2) — não aplica nada
struct UiTexPickOutcome {
    bool applied = false;
    char toast[64] = "";
    char log[160] = "";
};
UiTexPickOutcome applyUiTexPick(Scene& scene, Handle tic, i32 element, int pick,
                                const AssetCatalog& catalog);

// F5.1-hotfix: menu do botão Settings → 0 nada, 1 = "Exportar logs"
// (copia logs/ e crash dumps para Downloads/GOneVV/logs via MediaStore).
// F5.2: 2 = "Ver logs" (viewer in-app), 3 = "Acesso a ficheiros…" (abre as
// definições do sistema); storageMode (não-nulo) desenha a linha
// "armazenamento: …" com o modo ativo.
// 0.8.10: 4 = fonte manter/largar, 5 = reconverter assets.
// 0.8.11: 6 = "diagnostico audio (probe)" (50 ciclos + tabela no log),
// 7 = "volume geral: NN%" (cicla 0→25→50→75→100; master do misturador).
int drawSettingsMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                     EditorState& st, const char* storageMode = "",
                     bool keepSource = true, f32 audioMaster = 1.0f);

// F5.2: DIÁLOGO All Files Access — 0 nada, 1 = "Permitir" (o main lança o
// intent das definições), 2 = "Cancelar". Fecha com toque fora.
int drawStorageDialog(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                      EditorState& st);

// 0.7.1 — OVERLAY CENAS: lista as cenas do projeto (a ATIVA com frame
// ACCENT; scroll id 44) + botão "+ Nova cena". Devolve:
//   0 = nada este frame; 1 = "+ Nova cena"; 2.. = trocar para a cena
//   (pick-2 = índice 0-based em `scenes`)
// `scenes` são os caminhos relativos do manifesto ("scenes/main.goni") —
// o nome mostrado é o basename sem extensão.
int drawScenesMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                   EditorState& st, const std::vector<std::string>& scenes,
                   u32 activeScene, f32 ax = -1.0f, f32 ay = -1.0f);

// nome de exibição da cena ("scenes/main.goni" → "main") — partilhado
// com os testes (FONTE ÚNICA do rótulo).
const char* sceneDisplayName(const std::string& sceneRelPath, char* out,
                             size_t outCap);

// F5.2: overlay IMPORT — candidatos de Download/Documents (File API direta).
// Devolve 0 nada; i+1 = candidato i escolhido. Cap 8 linhas (mono, sem
// scroll — F8). vazio → mensagem "nenhum ficheiro suportado".
int drawImportMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                   EditorState& st, const std::vector<fileapi::Candidate>& cands);

// F5.2: VIEWER de logs — engine.log (tail) + crash dumps, scroll (id 43),
// tema mono, fecha com toque fora ou botão "fechar". Auto-scroll para o
// fundo no 1º frame (st.logViewerJustOpened).
void drawLogViewer(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                   EditorState& st, const std::vector<std::string>& lines,
                   const std::vector<std::string>& dumps);

} // namespace editor

} // namespace vv
