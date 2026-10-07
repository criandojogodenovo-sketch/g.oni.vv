#include "ui/EditorUi.h"
#include "core/VoniSystem.h"   // 0.9.2: exported() das vars @+
#include "platform/EngineLog.h"  // 0.9.2: log do ScriptAdd
#include "components/ScriptComp.h"  // 0.9.2: ScriptAdd cria o componente
#include "voni/Voni.h"
#include "components/InputMap.h"
#include "components/AudioPlayer.h"   // 0.8.11: inspector/seletor de clips
#include "render/Camera.h"
#include "components/TouchControls.h"
#include <cmath>
#include "components/CameraComp.h"   // 0.7.7: inspector/menu da câmara
#include "components/BodyComp.h"     // FASE 9 (G2-7): ícone da hierarquia por corpo
#include "core/CameraUtil.h"          // 0.7.7: uma ativa por cena
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "components/AnimationPlayer.h"   // 0.8.0: add Animacao no Inspector
#include "render/Primitives.h"            // 0.8.0: seletor de primitivas
#include "render/Mesh.h"                 // 0.8.9: AABB do mesh (normalização uniforme)
#include "core/Scene.h"
#include "ui/UiEditor.h"                  // 0.8.6: uiHexFormat (linha hex)
#include "ui/Icons.h"                    // 0.9.0: conjunto outline (spec A)
#include "ui/Strings.h"                  // 0.9.6.18 (D4): a tabela localizada
#include "platform/BuildInfo.h"          // 0.8.12: badge ANTIGO dos dumps no viewer
#include <cstdio>
#include <cstring>

namespace vv {
namespace editor {

// 0.9.6.1 (PASSO 0 · R-018) — as DEFINIÇÕES das variáveis de layout do
// EditorLayout.h (eram constexpr em dp consumidas como px: no C33, painéis/
// linhas/cabeçalhos a metade do pedido). applyDensity() atualiza TODAS de
// uma vez quando a densidade do device fica conhecida (main.cpp no arranque
// e em mudanças de AConfiguration) — os centenas de pontos de uso ficam
// intocados; nos testes (densidade 1.0) os valores são os de sempre.
// PASSO 1 (0.9.6.14 · a tabela da spec do dono): cabeçalho 28 · linha 36 ·
// campo/pesquisa 32 (eram TUDO 48 — a lei de ouro: nada ≥48 no editor)
f32 kPad       = 16.0f;
f32 kHeaderH   = 28.0f;
f32 kRowH      = 36.0f;
f32 kSearchRowH = 32.0f;
f32 kMenuW     = 340.0f;

void applyDensity() {
    kPad        = theme::dp(16.0f);
    kHeaderH    = theme::dp(28.0f);
    kRowH       = theme::dp(36.0f);
    kSearchRowH = theme::dp(32.0f);
    kMenuW      = theme::dp(340.0f);
}

namespace {
constexpr u64 kIdPlus      = 40;
// F5.2: viewer de logs usa kLogsScrollId (43, EditorLayout.h — compartilhado
// com os testes)
constexpr u64 kIdRowBase   = 1000;
constexpr u64 kIdAssetBase = 6000;   // F5-E: itens do seletor de assets
// ids do Inspector (sliders/botões) vivem em ui/EditorLayout.h — o PLANO é
// a fonte única das posições E dos ids (F5.0-fix).

// Toque (edge de press) fora do rect → fecha overlays.
bool pressedOutside(const InputState& in, f32 x, f32 y, f32 w, f32 h) {
    if (!in.pressed(0)) {
        return false;
    }
    f32 px, py;
    in.pos(0, px, py);
    return !(px >= x && px < x + w && py >= y && py < y + h);
}

// ---- 0.9.6.18 (HOTFIX D2) — O ORÇAMENTO DA LINHA TRANSFORM ----------------
// O dono: «Linha X/Y/Z clampada à largura real do painel (o padrão de
// orçamento do labelFitted): campos encolhem com minWidth; abaixo de um
// piso, o reset vira ícone inline após o Z. NADA desenha fora do rect do
// painel.» A FUNÇÃO É PURA e é a FONTE ÚNICA do draw E do re-despacho do
// tap (o padrão da casa: desenhar e tocar partilham a MESMA matemática —
// o bug do campo Z cortado nascia de as duas metades terem fórmulas
// diferentes, e o tap ainda usava gaps em px crus).
//
// As tentativas (em dp úteis = w − 2×kPad):
//   1. painel largo:  3×64 + 2×8 + 8 + 40 = 256 → caixas 64 + reset CHIP 40
//   2. encolhe:       boxW = (útil − 24 − 40)/3 com PISO 48 → o chip fica
//   3. abaixo do piso: o reset vira ÍCONE 20 inline —
//                     boxW = (útil − 24 − 20)/3 com PISO 40; no piso do
//                     painel da casa (180dp → 164 úteis) dá 40 exato
//                     (3×40+2×8+8+20 = 164). A ÚLTIMA DEFESA: se mesmo
//                     assim a linha passar do útil, as caixas encolhem —
//                     o rect do painel é a LEI (nada fora dele).
constexpr f32 kTfGapBox   = 8.0f;   // o vão ENTRE caixas
constexpr f32 kTfGapReset = 8.0f;   // o vão antes do reset
constexpr f32 kTfChipW    = 40.0f;  // o chip do reset (a lei de ouro: alvo 40)
constexpr f32 kTfIconW    = 20.0f;  // o ícone inline do reset
constexpr f32 kTfWideBox  = 64.0f;  // a caixa larga (3×64 = o painel 272 antigo)
constexpr f32 kTfMinBox   = 48.0f;  // o PISO com o chip (a spec: minWidth)
constexpr f32 kTfFloorBox = 40.0f;  // o PISO com o ícone inline

// a versão em PX (o que o draw e o dispatch consomem — os dois a MESMA,
// density incluída; a dp é a afervel)
TransformBudget transformRowBudgetPx(f32 usablePx) {
    const f32 dp1 = theme::dp(1.0f);
    TransformBudget b =
        transformRowBudget(dp1 > 0.0f ? usablePx / dp1 : usablePx);
    b.boxW *= dp1;
    b.resetW *= dp1;
    return b;
}

f32 rad2deg(f32 r) { return r * 57.29577951f; }
f32 deg2rad(f32 d) { return d * 0.01745329252f; }

// nome curto do asset p/ a linha do Inspector (basename da ref)
const char* assetBasename(const std::string& ref) {
    const size_t slash = ref.rfind('/');
    return ref.c_str() + (slash == std::string::npos ? 0 : slash + 1);
}

// Linha do Inspector: label + slider + valor; aplica em `value` via setter.
// (rowTop, rowH, tm) vêm do PLANO — a baseline é centrada nas métricas REAIS
// da fonte (F5.0-fix: o "+8" antigo deixava o bloco de 28 px invadir a linha
// de cima).
// 0.8.9: valueTappable — o VALOR (zona à direita do trilho) abre o teclado
// NUMÉRICO (campos SEM TETO: py=10 000 escreve-se, o slider fica suave no
// seu range). Desenha-se em ACCENT com sublinhado = affordance de toque.
bool sliderRow(UiContext& ui, u64 id, f32 x, f32 w, f32 rowTop, f32 rowH,
               const TextMetrics& tm, const char* labelText,
               f32 minV, f32 maxV, f32& value, const char* fmt,
               bool valueTappable = false) {
    // 0.9.6.6 (GRUPO C): rótulo/trilho em dp REAL (eram 84/118 px crus —
    // no device 2.0 o trilho saía a metade do pedido; a exata classe R-018)
    // PASSO 2 (0.9.6.15): o VALOR alinha à largura REAL do painel (w —
    // 22% afervado a [180..260]; o kPanelW=300 fixo cravava o valor FORA
    // do painel estreito — a sobreposição de glifos que o test_ui apanhou)
    const f32 labelW = theme::dp(84.0f);
    const f32 trackX = x + labelW;
    const f32 trackW = theme::dp(118.0f);
    const f32 baseline = inspBaseline(rowTop, rowH, tm);
    ui.labelFitted(x + kPad, baseline, labelText,
                   theme::TEXT, labelW - kPad - theme::dp(6.0f));   // até ao trilho
    const bool changed = ui.slider(id, trackX, rowTop, trackW, rowH, minV, maxV, value);

    char val[24];
    std::snprintf(val, sizeof(val), fmt, value);
    if (ui.hasFont()) {
        const f32 tw = ui.fontWidth(val);
        ui.label(x + w - kPad - tw, baseline, val,
                 valueTappable ? theme::ACCENT : theme::TEXT);
        if (valueTappable) {
            // sublinhado discreto: "isto é tocável" (a zona de toque é o
            // rect do valor — ver o re-despacho do tap abaixo)
            ui.panel(x + w - kPad - tw - theme::dp(6.0f),
                     rowTop + rowH - theme::dp(3.0f), tw + theme::dp(8.0f),
                     1.5f, theme::ACCENT);
        }
    }
    return changed;
}
} // namespace

TransformBudget transformRowBudget(f32 usableWdp) {
    TransformBudget b;
    if (usableWdp >= 3.0f * kTfWideBox + 2.0f * kTfGapBox + kTfGapReset +
                         kTfChipW) {
        return b;   // tentativa 1: caixas 64 + chip 40
    }
    const f32 withChip =
        (usableWdp - 2.0f * kTfGapBox - kTfGapReset - kTfChipW) / 3.0f;
    if (withChip >= kTfMinBox) {
        b.boxW = withChip;   // tentativa 2: encolhe até ao piso 48, chip fica
        return b;
    }
    // tentativa 3: o reset vira ÍCONE inline após o Z
    b.resetIcon = true;
    b.resetW = kTfIconW;
    const f32 withIcon =
        (usableWdp - 2.0f * kTfGapBox - kTfGapReset - kTfIconW) / 3.0f;
    b.boxW = withIcon >= kTfFloorBox ? withIcon : kTfFloorBox;
    // a ÚLTIMA defesa (painel sub-mínimo — impossível no clamp da casa,
    // mas a lei é o rect): a linha INTEIRA re-encolhe para caber
    const f32 total = 3.0f * b.boxW + 2.0f * kTfGapBox + kTfGapReset + b.resetW;
    if (total > usableWdp && total > 1.0f) {
        b.boxW -= (total - usableWdp) / 3.0f;
        if (b.boxW < 16.0f) {
            b.boxW = 16.0f;   // abaixo disto o campo não lê — o scissor
                              // do painel guarda o resto
        }
    }
    return b;
}


UiRect centerRect(f32 sw, f32 sh) {
    // QUALIFICADO: chamada não-qualificada era ambígua no NDK clang — o ADL
    // puxava vv::safe::centerRect (o tipo do argumento é safe::Insets) para
    // além de vv::editor::centerRect (mesma assinatura). O CI apanhou.
    return safe::centerRect(sw, sh, safe::Insets{});   // sem safe-area (compat/testes)
}

// F4.2: viewport central DENTRO do contentRect — gestos que nascem atrás da
// nav/status bar não orbitam a câmara (matemática em ui/SafeArea.h).
UiRect centerRect(f32 sw, f32 sh, const safe::Insets& in) {
    return safe::centerRect(sw, sh, in);
}

// 0.7.6 — com o painel direito opcional (G5 escondeu o Inspector)
UiRect centerRect(f32 sw, f32 sh, const safe::Insets& in, bool rightPanel) {
    return safe::centerRect(sw, sh, in, rightPanel);
}

// 0.9.0 — com o drawer do painel de baixo aberto (spec E)
UiRect centerRect(f32 sw, f32 sh, const safe::Insets& in, f32 drawerH,
                  bool rightPanel) {
    return safe::centerRect(sw, sh, in, drawerH, rightPanel);
}

// GRUPO D — com as LARGURAS DE ESTADO (divisores arrastáveis): o rect que
// o editor 3D usa; −1/−1 = defaults adaptativos. PASSO 2: inspTrack
// colapsa o inspector ao trilho (a área vai ao viewport)
UiRect centerRect(f32 sw, f32 sh, const safe::Insets& in, f32 drawerH,
                  bool rightPanel, f32 hierWdp, f32 inspWdp, bool inspTrack) {
    return safe::centerRect(sw, sh, in, drawerH, rightPanel, hierWdp, inspWdp,
                            inspTrack);
}

// a resolução do par de larguras para um EditorState (a fonte ÚNICA é a
// safe::resolvePanels — o draw dos painéis, o drag dos divisores, a
// sombra do save do layout e os testes partilham ESTA)
safe::PanelBudget resolveEditorPanels(const EditorState& st,
                                      f32 contentWdp) {
    return safe::resolvePanels(contentWdp, st.hierW, st.inspW);
}

// ---------------------------------------------------------------------------
// GRUPO D (0.9.6.7) — DIVISORES ARRASTÁVEIS hierarquia|viewport|inspector
// ---------------------------------------------------------------------------
// O padrão da pega do drawer (BottomPanel): strip VISUAL de 12dp encostado
// à borda do painel, zona de toque de 20dp (12 do strip + 8 de folga PARA
// DENTRO do painel — nunca para o lado do viewport: um toque na pega está
// SEMPRE fora do centerRect, não orbita a câmara nem agarra o gizmo). O
// press arma o drag (âncoras startX/baseW), o movimento horizontal
// redimensiona AO VIVO com clamp (kPanelMinW .. (content−kViewportMinW)/2,
// passos de 8dp — o granular do drawer), o release fixa. PERSISTE no
// layout.json (hierW/inspW) com debounce no main (a sombra acompanha).
//
// ORDEM (o coração do plano B): dividerInput corre ANTES dos painéis no UI
// pass — a pega chama dragHandle e RECLAMA active_ primeiro; o beginScroll
// do painel vê active_ != 0 e NÃO reclama (o toque na pega nunca vira
// scroll, mesmo com a região por baixo). drawPanelDividers corre DEPOIS
// dos painéis: o strip visível por cima da borda (o conteúdo do painel
// NÃO perde largura — a linha X/Y/Z do Inspector continua a caber).
namespace {

// os rects das pegas (strip + hit) — a fonte ÚNICA do input e do draw
struct DividerRects {
    UiRect stripL{}, hitL{};     // borda DIREITA da hierarquia
    UiRect stripR{}, hitR{};     // borda ESQUERDA do inspector
    bool  rightOn = false;       // o painel direito existe neste frame?
    f32   contentW = 0.0f;       // p/ o clamp (px)
    safe::PanelBudget budget{};  // as larguras EFETIVAS do frame
};

DividerRects dividerRects(UiContext& ui, const EditorState& st,
                          bool rightPanel) {
    DividerRects d;
    const f32 sw = ui.screenWidth();
    const f32 sh = ui.screenHeight();
    d.contentW = ui.contentWidthPx();
    const f32 stripW = theme::dp(kDividerStripW);
    const f32 hitW = theme::dp(kDividerHitW);
    const safe::PanelBudget b =
        safe::resolvePanels(d.contentW, st.hierW, st.inspW);
    const UiRect panels = safe::panelsRect(sw, sh, ui.safeArea(), st.drawerH);
    const UiRect hier{panels.x, panels.y, b.hier, panels.h};
    d.hitL = {hier.x + hier.w - hitW, hier.y, hitW, hier.h};
    d.stripL = {hier.x + hier.w - stripW, hier.y, stripW, hier.h};
    d.budget = b;
    if (rightPanel) {
        // PASSO 2: com o inspector no TRILHO (sem seleção) não há divisor
        // direito — não há largura a arrastar (o trilho não é pega)
        const bool track = editor::inspectorCollapsed(st);
        const UiRect insp{panels.x + panels.w - b.insp, panels.y, b.insp,
                          panels.h};
        // painel direito válido (o G5 pode escondê-lo — sem divisor então)
        d.rightOn = !track && insp.w > hitW + theme::dp(48.0f) &&
                    insp.h > 1.0f;
        d.hitR = {insp.x, insp.y, hitW, insp.h};
        d.stripR = {insp.x, insp.y, stripW, insp.h};
    }
    return d;
}

} // namespace

void dividerInput(UiContext& ui, const InputState& in, EditorState& st,
                  bool rightPanel) {
    const DividerRects d = dividerRects(ui, st, rightPanel);
    // as pegas reclamam o gesto (active_) — ANTES dos painéis (o main chama
    // na ordem certa; o dragHandle NÃO regista no audit: não é alvo de tap)
    const bool heldL = ui.dragHandle(kIdDividerL, d.hitL.x, d.hitL.y,
                                     d.hitL.w, d.hitL.h);
    const bool heldR = d.rightOn && ui.dragHandle(kIdDividerR, d.hitR.x,
                                                  d.hitR.y, d.hitR.w, d.hitR.h);
    f32 px = -1.0f, py = -1.0f;
    if (in.down(0)) {
        in.pos(0, px, py);
    }
    // press edge na pega → arma (âncoras capturadas — o padrão do drawer)
    if (in.pressed(0) && heldL) {
        st.divDragActive = true;
        st.divDragRight = false;
        st.divDragStartX = px;
        st.divDragBaseW = d.budget.hier;
    } else if (d.rightOn && in.pressed(0) && heldR) {
        st.divDragActive = true;
        st.divDragRight = true;
        st.divDragStartX = px;
        st.divDragBaseW = d.budget.insp;
    }
    // o drag: redimensiona AO VIVO com clamp + passos de 8dp (o granular
    // do drawer; o estado escreve o VALOR SNAPPED — o que o dono vê é o
    // que fica). O clamp é a GANGorra da resolvePanels (o piso do viewport
    // e o OUTRO painel contam — re-resolvido com o RAW novo a cada frame)
    if (st.divDragActive && in.down(0)) {
        const f32 dx = px - st.divDragStartX;
        // hierarquia: arrastar p/ a DIREITA alarga; inspector: p/ a ESQUERDA
        const f32 want = st.divDragRight ? (st.divDragBaseW - dx)
                                         : (st.divDragBaseW + dx);
        const f32 step = theme::dp(8.0f);
        // GRUPO D (achado ao vivo da 13.7): o snap NUNCA desce abaixo de 0
        // — um valor negativo na resolvePanels é a SEMÂNTICA de default
        // (−1), e o drag além do piso saltava o painel para o DEFAULT
        f32 snapped = std::floor(want / step) * step;
        if (snapped < 0.0f) {
            snapped = 0.0f;
        }
        const safe::PanelBudget rb = safe::resolvePanels(
            d.contentW, st.divDragRight ? st.hierW : snapped,
            st.divDragRight ? snapped : st.inspW);
        if (st.divDragRight) {
            st.inspW = rb.insp;
        } else {
            st.hierW = rb.hier;
        }
    }
    if (!in.down(0)) {
        st.divDragActive = false;
    }
}

void drawPanelDividers(UiContext& ui, const EditorState& st, bool rightPanel) {
    const DividerRects d = dividerRects(ui, st, rightPanel);
    // 0.9.6.18 (HOTFIX D11) — o dono: «Divisor = linha sólida 1dp na cor
    // border + pill de grip só durante o drag. Os pontos flutuantes
    // morrem.» O fundo do strip (bg em repouso) e os 5 traços de 2×4dp
    // (a "coluna de pontos" que flutuava sobre a grelha) MORRERAM: em
    // repouso só a LINHA 1dp existe; no drag o strip acende (accent) e a
    // PILL arredondada aparece no centro.
    const UiRect strips[2] = {d.stripL, d.stripR};
    const bool on[2] = {true, d.rightOn};
    const bool dragging[2] = {st.divDragActive && !st.divDragRight,
                              st.divDragActive && st.divDragRight};
    for (int i = 0; i < 2; ++i) {
        if (!on[i] || strips[i].w <= 0.0f || strips[i].h <= 1.0f) {
            continue;
        }
        // a LINHA SÓLIDA (1dp na cor border — o divisor em repouso)
        ui.panel(strips[i].x, strips[i].y, 1.0f, strips[i].h,
                 theme::kTheme.border);
        if (dragging[i]) {
            // o fundo ACENDE + a PILL de grip (só durante o drag)
            ui.panel(strips[i].x, strips[i].y, strips[i].w, strips[i].h,
                     theme::kTheme.accent);
            const f32 pw = theme::dp(4.0f);
            const f32 ph = theme::dp(40.0f);
            ui.panelPill(strips[i].x + (strips[i].w - pw) * 0.5f,
                         strips[i].y + (strips[i].h - ph) * 0.5f, pw, ph,
                         theme::kTheme.accentInk);
        }
    }
}

// ---------------------------------------------------------------------------
// HIERARQUIA 0.9.0 (spec B + scope funcional):
//   cabeçalho 48dp: "HIERARQUIA" 12sp text-2 + [+] 48dp
//   pesquisa 48dp:  [lupa][campo — filtra por nome] (teclado in-app propósito 8)
//   linha 48dp:     [ícone de tipo 24][nome 14sp flex][olho 24][⋮ 24] — as
//                   zonas de olho/⋮ são ALVOS de 48dp (spec A) com ícone 24
//   filhos:         recuo 24dp + conector vertical (parenting VISUAL)
//   estados:        normal · selecionado (fill accent) · escondido (olho-off
//                   + nome text-2) · premido (surface-2) · multi (frame accent)
//   multi-seleção:  toque na linha JÁ selecionada arranca o conjunto; toques
//                   seguintes alternam (decisão documentada — ver EditorUi.h)
//   vazio:          ícone + convite (spec M)
// ---------------------------------------------------------------------------
// FASE 9 (G2-7): o ícone de tipo da HIERARQUIA — o BodyComp é consultado
// PRIMEIRO (corpos têm ícone PRÓPRIO: tic_static/tic_player/tic_rigid),
// depois câmara/áudio/UI/mesh/entrada. Função PURA (afervável no CI).
icons::Icon hierIconFor(const Tic& t) {
    if (t.getComponent<CameraComp>()) {
        return icons::Icon::Camera;
    }
    if (const BodyComp* bcT = t.getComponent<BodyComp>()) {
        switch (bcT->type) {
            case BodyType::Static:    return icons::Icon::Static;
            case BodyType::Character: return icons::Icon::Person;
            case BodyType::Rigid:     return icons::Icon::Rigid;
        }
        return icons::Icon::Static;
    }
    if (t.getComponent<AudioPlayer>()) {
        return icons::Icon::Speaker;
    }
    if (t.getComponent<UiCanvas>()) {
        return icons::Icon::Monitor;
    }
    if (const MeshRenderer* mr = t.getComponent<MeshRenderer>()) {
        return mr->primOn ? icons::Icon::Cube : icons::Icon::Box;
    }
    if (t.getComponent<InputMap>() || t.getComponent<TouchControls>()) {
        return icons::Icon::Person;
    }
    return icons::Icon::Box;
}

bool drawHierarchy(UiContext& ui, Scene& scene, EditorState& st) {
    // GRUPO D: a largura é ESTADO (divisores) — default adaptativo se −1
    // (a resolução é a fonte ÚNICA: resolvePanels — gangorra dos 3 pisos)
    const UiRect panel = safe::hierarchyPanelRect(ui.screenWidth(),
                                                  ui.screenHeight(),
                                                  ui.safeArea(), st.drawerH,
                                                  st.hierW, st.inspW);
    const f32 x = panel.x;
    const f32 y = panel.y;
    const f32 w = panel.w;
    const f32 h = panel.h;

    ui.panel(x, y, w, h, theme::kTheme.surface);
    ui.panel(x + w - 1.0f, y, 1.0f, h, theme::kTheme.border);

    // ---- árvore visível: raízes primeiro, filhos recursivamente ------------
    // (o filtro de pesquisa esconde linhas que não casam; os filhos de uma
    // linha escondida também saem — a árvore continua coerente)
    struct Row {
        Handle h;
        u32    depth;
    };
    Row rows[64];
    u32 nRows = 0;
    const char* needle = st.hierSearchLen ? st.hierSearch : nullptr;
    // slots ativos recolhidos POR ÍNDICE (Scene não expõe at(i); o parent é
    // o ÍNDICE DO SLOT do pai — mesmos números que o create() grava)
    struct Slot {
        i32  idx;
        const Tic* t;
    };
    Slot slots[64];
    u32 nSlots = 0;
    scene.forEachActive([&](const Tic& t) {
        if (nSlots < 64) {
            // o índice do slot: procuramos na tabela de handles — o handle
            // CODIFICA o índice (Handle.index é o slot de sempre)
            slots[nSlots++] = {static_cast<i32>(t.handle.index), &t};
        }
    });
    // visita depth-first: filhos de cur (parent == cur) por ordem de slot
    struct Walk {
        const Slot* slots;
        u32 nSlots;
        Row* rows;
        u32* n;
        const char* needle;
        void visit(i32 cur, u32 depth) {
            if (*n >= 64) {
                return;
            }
            for (u32 i = 0; i < nSlots && *n < 64; ++i) {
                const Tic& t = *slots[i].t;
                if (slots[i].idx != t.handle.index ||
                    t.parent != cur) {
                    continue;
                }
                if (needle && *needle &&
                    std::strstr(t.name.c_str(), needle) == nullptr) {
                    visit(slots[i].idx, depth);   // filho pode casar
                    continue;
                }
                rows[(*n)++] = {t.handle, depth};
                visit(slots[i].idx, depth + 1);
            }
        }
    };
    Walk wk{slots, nSlots, rows, &nRows, needle};
    wk.visit(-1, 0);

    // ---- cabeçalho 48dp: título 12sp text-2 + [+] 48dp ----------------------
    // 0.9.6.6 (GRUPO C): o [+], o chip e o campo de pesquisa são ALVOS —
    // ≥48dp REAL (a regra da casa) e dp em tudo (eram px crus: a auditoria
    // do Grupo B media o [+] a 56×40 e o campo a 268×40 — avisos <48dp).
    {
        const TextMetrics m = ui.textMetrics();
        const f32 base = y + (kHeaderH - m.block()) * 0.5f + m.ascent;
        // 0.9.6.10 (GRUPO UI · a imagem 1): o título do painel em 16sp
        // text1 (era 12sp text2 — os painéis da referência têm títulos
        // PRÓPRIOS, não legendas)
        ui.labelStyled(x + kPad, base, "Hierarquia", theme::kTheme.text1,
                       theme::fontScale(theme::kFontSection), 0);
        // chip da MULTI-SELEÇÃO (aparece com ≥1 no conjunto): "N ×" limpa
        // PASSO 1: chip 28dp de altura (a LINHA do cabeçalho — o piso
        // kHeadFloorDp; o alvo é a linha do cabeçalho de 28dp)
        if (st.multiSelectCount > 0) {
            char chip[24];
            std::snprintf(chip, sizeof(chip), "%u x", st.multiSelectCount);
            const f32 cw = theme::dp(48.0f);
            const UiRect cr = {x + w - kPad - theme::dp(48.0f) -
                                   theme::dp(8.0f) - cw,
                               y, cw, kHeaderH};
            ui.auditRowFloorNext(layout::kHeadFloorDp);
            if (ui.button(kHierMultiClearId, cr.x, cr.y, cr.w, cr.h, chip)) {
                st.multiSelectCount = 0;   // volta à seleção simples
            }
        }
        // 0.9.6.10 (GRUPO UI · o anti-exemplo da imagem 2): o [+] era um
        // BLOCO cheio do accent (branco cegante no mono; âmbar gritaria
        // agora) — a referência tem botões QUIETOS: chip de vidro com o
        // ícone âmbar (o accent é ESTADO, não repouso — a regra spec A)
        // PASSO 1: alvo 28×28 (a LINHA do cabeçalho), desenho 24, ícone 16
        const f32 hdrBtn = kHeaderH;
        const UiRect pr = {x + w - kPad - hdrBtn, y, hdrBtn, hdrBtn};
        ui.auditRowFloorNext(layout::kHeadFloorDp);
        bool plus = false;
        if (ui.widgetHit(kIdPlus, pr.x, pr.y, pr.w, pr.h)) {
            plus = true;
        }
        const bool plusHeld = ui.widgetActive(kIdPlus);
        ui.panelRounded(pr.x, pr.y, pr.w, pr.h, theme::dp(theme::kRadiusCard),
                        plusHeld ? theme::kTheme.surface2
                                 : theme::kTheme.surface);
        ui.frameRounded(pr.x, pr.y, pr.w, pr.h, 1.0f,
                        theme::dp(theme::kRadiusCard), theme::kTheme.border);
        icons::drawIcon(ui, icons::Icon::Plus,
                        pr.x + (pr.w - theme::dp(16.0f)) * 0.5f,
                        pr.y + (pr.h - theme::dp(16.0f)) * 0.5f,
                        theme::dp(16.0f),
                        plusHeld ? theme::kTheme.accentInk
                                 : theme::kTheme.accent);
        // o ⋮ da hierarquia (a imagem 1: cada painel com o seu menu) —
        // abre o sheet com as ações reais da árvore (o main despacha)
        const UiRect dr = {pr.x - hdrBtn, y, hdrBtn, hdrBtn};
        ui.auditRowFloorNext(layout::kHeadFloorDp);
        const bool dotsHeld = ui.widgetActive(kHierDotsId);
        ui.panelRounded(dr.x, dr.y, dr.w, dr.h, theme::dp(theme::kRadiusCard),
                        (dotsHeld || st.hierMenu) ? theme::kTheme.surface2
                                                  : theme::kTheme.surface);
        ui.frameRounded(dr.x, dr.y, dr.w, dr.h, 1.0f,
                        theme::dp(theme::kRadiusCard), theme::kTheme.border);
        for (int d = 0; d < 3; ++d) {
            ui.panel(dr.x + dr.w * 0.5f - theme::dp(7.0f) +
                         static_cast<f32>(d) * theme::dp(7.0f),
                     dr.y + dr.h * 0.5f - theme::dp(2.0f), theme::dp(3.0f),
                     theme::dp(3.0f), theme::kTheme.text1);
        }
        if (ui.widgetHit(kHierDotsId, dr.x, dr.y, dr.w, dr.h)) {
            st.hierMenu = !st.hierMenu;
        }
        // ---- pesquisa 32dp (PASSO 1: o CAMPO da spec — o campo OCUPA a
        // linha kSearchRowH inteira; o piso de campo 32, LayoutDump.h) ----
        const f32 sy = y + kHeaderH;
        ui.panel(x, sy, w, 1.0f, theme::kTheme.border);
        const UiRect sfield = {x + kPad, sy, w - 2.0f * kPad, kSearchRowH};
        ui.auditRowFloorNext(layout::kFieldFloorDp);
        ui.panelRounded(sfield.x, sfield.y, sfield.w, sfield.h,
                        theme::dp(theme::kRadiusField), theme::kTheme.bg);
        ui.frameRounded(sfield.x, sfield.y, sfield.w, sfield.h, 1.0f,
                        theme::dp(theme::kRadiusField), theme::kTheme.border);
        icons::drawIcon(ui, icons::Icon::Search,
                        sfield.x + theme::dp(10.0f),
                        sfield.y + (sfield.h - theme::dp(20.0f)) * 0.5f,
                        theme::dp(20.0f), theme::kTheme.text2);
        if (ui.hasFont()) {
            const TextMetrics mm = ui.textMetrics();
            const f32 bb = sfield.y + (sfield.h - mm.block()) * 0.5f +
                           mm.ascent;
            // PASSO 2 (0.9.6.15): o placeholder DEGRADA POR ORDEM (o padrão
            // dos chips da strip R-023): «pesquisar TIC» quando o campo dá,
            // «pesquisar» no PISO 140dp da hierarquia (spec PASSO 2) — o
            // campo nunca desenha reticência no placeholder
            const f32 phBudget = sfield.w - theme::dp(40.0f) - theme::dp(12.0f);
            const char* ph = ui.fontWidth("pesquisar TIC") <= phBudget
                                 ? "pesquisar TIC"
                                 : "pesquisar";
            if (st.hierSearchLen) {
                ui.labelFitted(sfield.x + theme::dp(40.0f), bb, st.hierSearch,
                               theme::kTheme.text1, phBudget);
            } else {
                ui.labelFitted(sfield.x + theme::dp(40.0f), bb, ph,
                               theme::kTheme.text2, phBudget);
            }
        }
        // o CAMPO abre o teclado (propósito 8) — captura normal FORA do scroll
        if (ui.widgetHit(kHierSearchId, sfield.x, sfield.y, sfield.w,
                         sfield.h)) {
            st.textInput = true;
            st.textPurpose = 8;   // pesquisa da hierarquia
            st.textTic = Handle::invalid();
            std::snprintf(st.textBuf, sizeof(st.textBuf), "%s", st.hierSearch);
            st.textLen = st.hierSearchLen;
        }

        // ---- lista (scroll) ----
        // GRUPO D (plano B): SEM inset — o divisor sobrepõe a borda e
        // RECLAMA o gesto ANTES do painel (dividerInput corre primeiro no
        // main); a região mantém a largura de sempre
        const f32 listTop = y + kHeaderH + kSearchRowH;
        const UiRect listRegion = {x, listTop, w, h - kHeaderH - kSearchRowH};
        const f32 contentH = hierarchyContentHeight(nRows);
        ui.beginScroll(kIdScrollHier, listRegion, contentH);
        const f32 off = ui.scrollOffset();

        // geometria da linha (PASSO 1 · spec B): [ícone tipo 20 na zona
        // 40][nome flex][olho 40zona][⋮ 40zona] — as ZONAS de toque 40dp
        // de largura × a LINHA de 36 (o despacho do scrollTap usa AS
        // MESMAS zonas — desenho e toque NUNCA divergem; ícones 20)
        const f32 zoneW = theme::dp(40.0f);
        const f32 eyeX = x + w - zoneW - theme::dp(8.0f) -
                         zoneW;   // zona olho
        const f32 dotsX = x + w - zoneW - theme::dp(4.0f);   // zona ⋮ (até à borda)

        // FASE 9 (G2-7): houve dedo parado em algum nome neste frame?
        bool holdVivo = false;
        for (u32 r = 0; r < nRows; ++r) {
            const Tic* t = scene.get(rows[r].h);
            if (!t) {
                continue;
            }
            const f32 ry = listTop + static_cast<f32>(r) * kRowH - off;
            const f32 indent = static_cast<f32>(rows[r].depth) * theme::dp(24.0f);
            const bool selected = st.selected == t->handle;
            bool multi = false;
            for (u32 m = 0; m < st.multiSelectCount; ++m) {
                if (st.multiSelect[m] == t->handle) {
                    multi = true;
                }
            }

            // ---- fundo do estado (0.9.6.10 · GRUPO UI · a imagem 1) ----
            // a linha selecionada: FILL accentDim (o âmbar a 25% sobre
            // grafite — #4A3714) + BARRA ESQUERDA âmbar 3dp (a referência
            // exata do dono). ANTES era o fill accent CHEIO — o BLOCO
            // cegante do anti-exemplo. O texto/icones ficam text1/accent
            // (9,6:1 / 6,2:1 sobre o accentDim — os pisos passam)
            if (selected && st.multiSelectCount == 0) {
                ui.panel(x + 4.0f, ry, w - 8.0f, kRowH,
                         theme::kTheme.accentDim);
                ui.panel(x + 4.0f, ry, theme::dp(3.0f), kRowH,
                         theme::kTheme.accent);
            } else if (multi) {
                ui.panel(x + 4.0f, ry, w - 8.0f, kRowH,
                         theme::kTheme.accentDim);
                ui.panel(x + 4.0f, ry, theme::dp(3.0f), kRowH,
                         theme::kTheme.accent);
            } else if (ui.widgetActive(kIdRowBase + t->handle.index)) {
                ui.panel(x + 4.0f, ry, w - 8.0f, kRowH, theme::kTheme.surface2);
            }

            // ---- conector de filhos + recuo (parenting VISUAL) ----
            if (rows[r].depth > 0) {
                ui.panel(x + theme::dp(16.0f) + indent - theme::dp(12.0f), ry,
                         1.5f, kRowH, theme::kTheme.border);
            }

            // ---- ícone de TIPO (20dp na zona de 40 — PASSO 1) ----
            const icons::Icon ic = hierIconFor(*t);
            const f32 iconX = x + theme::dp(16.0f) + indent;
            const bool inkOn = (selected && st.multiSelectCount == 0) || multi;
            icons::drawIcon(ui, ic, iconX,
                            ry + (kRowH - theme::dp(20.0f)) * 0.5f,
                            theme::dp(20.0f),
                            inkOn ? theme::kTheme.accent
                                  : theme::kTheme.text2);

            // ---- nome 14sp (flex, truncado) ----
            if (ui.hasFont()) {
                const TextMetrics m2 = ui.textMetrics();
                const f32 base = ry + (kRowH - m2.block()) * 0.5f + m2.ascent;
                const f32 nameX = iconX + theme::dp(24.0f) + theme::dp(8.0f);
                const f32 nameW = eyeX - nameX - theme::dp(8.0f);
                // PASSO 2: o nome trunca POR DESENHO (a spec B dá o tip do
                // long-press como o acesso ao nome completo) — o validador
                // não avisa (a flag vive SÓ para a label registada)
                ui.auditFitByDesignNext();
                ui.labelFitted(nameX, base, t->name.c_str(),
                               inkOn ? theme::kTheme.text1
                                     : (t->visible ? theme::kTheme.text1
                                                   : theme::kTheme.text2),
                               nameW);
                // FASE 9 (G2-7): LONG-PRESS no nome TRUNCADO → o nome
                // completo (tip em toast; o main converte). ~30 frames
                // (0,5s @60fps) com o dedo parado na zona do nome
                const bool truncado =
                    ui.fontWidth(t->name.c_str()) > nameW;
                if (truncado &&
                    ui.pointerDownAt(nameX, ry, nameW, kRowH)) {
                    if (st.hierHoldRow == static_cast<i32>(r)) {
                        ++st.hierHoldFrames;
                    } else {
                        st.hierHoldRow = static_cast<i32>(r);
                        st.hierHoldFrames = 1;
                        st.hierHoldShown = false;
                    }
                    if (st.hierHoldFrames >= 30u && !st.hierHoldShown) {
                        std::snprintf(st.nameTip, sizeof(st.nameTip), "%s",
                                      t->name.c_str());
                        st.hierHoldShown = true;
                    }
                    holdVivo = true;
                }
            }

            // ---- olho / ⋮ (ícones 20 em zonas 40×Linha — PASSO 1; só
            // desenham — o tap é re-despachado pelo scroll) ----
            icons::drawIcon(ui, t->visible ? icons::Icon::Eye
                                           : icons::Icon::EyeOff,
                            eyeX + (zoneW - theme::dp(20.0f)) * 0.5f,
                            ry + (kRowH - theme::dp(20.0f)) * 0.5f,
                            theme::dp(20.0f),
                            inkOn ? theme::kTheme.accentInk
                                  : theme::kTheme.text2);
            icons::drawIcon(ui, icons::Icon::Dots,
                            dotsX + (zoneW - theme::dp(20.0f)) * 0.5f,
                            ry + (kRowH - theme::dp(20.0f)) * 0.5f,
                            theme::dp(20.0f),
                            inkOn ? theme::kTheme.accentInk
                                  : theme::kTheme.text2);
        }
        ui.endScroll();
        // o long-press MENTE quando o dedo sai da zona do nome — o
        // contador regressa a zero (o próximo conta de novo)
        if (!holdVivo) {
            st.hierHoldRow = -1;
            st.hierHoldFrames = 0;
            st.hierHoldShown = false;
        }

        // ---- VAZIO (spec M): ícone + convite --------------------------------
        if (nRows == 0) {
            icons::drawIcon(ui, icons::Icon::Scene,
                            x + w * 0.5f - theme::dp(20.0f),
                            listTop + theme::dp(40.0f), theme::dp(40.0f),
                            theme::kTheme.text2);
            if (ui.hasFont()) {
                const TextMetrics mv = ui.textMetrics();
                // GRUPO D: em painel ESTREITO o convite CURTO — e PASSO 2
                // (0.9.6.15): a LADDER mede o texto ao vivo (o padrão da
                // pesquisa/chips): o convite mais comprido que caiba — no
                // PISO 140dp da hierarquia sai «+ cria um TIC», nunca
                // reticência num convite
                const f32 invW = w - 2.0f * kPad;
                const char* invite;
                if (needle && *needle) {
                    invite = ui.fontWidth("nenhum TIC com esse nome") <= invW
                                 ? "nenhum TIC com esse nome"
                                 : "sem resultados";
                } else {
                    invite =
                        ui.fontWidth("sem TICs - toca em + para criar") <=
                                invW
                            ? "sem TICs - toca em + para criar"
                            : (ui.fontWidth("toca em + para criar") <= invW
                                   ? "toca em + para criar"
                                   : "+ cria um TIC");
                }
                ui.labelFitted(x + kPad,
                               listTop + theme::dp(112.0f) - mv.block() +
                                   mv.ascent,
                               invite, theme::kTheme.text2, invW);
            }
        }

        // ---- tap re-despachado (POR ID — F5.0-fix) --------------------------
        f32 tx, ty;
        if (ui.scrollTap(kIdScrollHier, tx, ty) &&
            scroll::inside(listRegion, tx, ty)) {
            const i32 sel = hierarchyRowAtTap(ty, listTop, off, nRows);
            if (sel < 0) {
                st.selected = Handle::invalid();   // vazio = desseleciona
                st.multiSelectCount = 0;
            } else {
                const Tic* t = scene.get(rows[sel].h);
                if (t) {
                    const f32 ry = listTop + static_cast<f32>(sel) * kRowH - off;
                    const f32 indent = static_cast<f32>(rows[sel].depth) * 24.0f;
                    const f32 iconX = x + 16.0f + indent;
                    const f32 eyeHit = eyeX;             // zona de 40dp
                    const f32 dotsHit = dotsX;
                    const f32 zonePx = zoneW;   // px reais da zona (dp)
                    if (ty >= ry && ty < ry + kRowH && tx >= eyeHit &&
                        tx < eyeHit + zonePx) {
                        if (Tic* tt = scene.get(t->handle)) {
                            tt->visible = !tt->visible;   // OLHO: toggle
                        }
                    } else if (ty >= ry && ty < ry + kRowH && tx >= dotsHit &&
                               tx < dotsHit + zonePx) {
                        st.contextMenu = true;           // ⋮: menu (spec L)
                        st.contextTic = t->handle;
                    } else if (ty >= ry && ty < ry + kRowH && tx >= iconX &&
                               tx < iconX + zonePx) {
                        // MULTI: o toque no ÍCONE DE TIPO alterna no conjunto
                        bool inSet = false;
                        u32 at = 0;
                        for (u32 m = 0; m < st.multiSelectCount; ++m) {
                            if (st.multiSelect[m] == t->handle) {
                                inSet = true;
                                at = m;
                            }
                        }
                        if (inSet) {
                            for (u32 m = at; m + 1 < st.multiSelectCount; ++m) {
                                st.multiSelect[m] = st.multiSelect[m + 1];
                            }
                            --st.multiSelectCount;
                        } else if (st.multiSelectCount < 16) {
                            st.multiSelect[st.multiSelectCount++] = t->handle;
                        }
                    } else {
                        st.selected = t->handle;   // corpo: seleciona
                        st.selElement = -1;
                    }
                }
            }
        }
        return plus;
    }
}

// ---------------------------------------------------------------------------
// INSPECTOR — F5.0-fix: o layout vem do PLANO (ui/EditorLayout.h) — linhas em
// ordem com y cumulativo e alturas derivadas das MÉTRICAS REAIS da fonte.
// O desenho NÃO tem nenhum "+=" próprio: consome o plano e só subtrai o
// offset do scroll. contentHeight = fundo da última linha (soma REAL).
// ---------------------------------------------------------------------------
bool drawInspector(UiContext& ui, Scene& scene, EditorState& st,
                   const AssetCatalog* catalog, const VoniSystem* voni) {
    // PASSO 2 (0.9.6.15 — spec do dono): SEM seleção o inspector É um
    // TRILHO de 32dp (safe::kInspTrackW) — só a faixa com o ícone; a área
    // restante junta-se ao viewport (o centerRect do main Já sabe — a
    // MESMA fonte: safe::resolvePanels com inspTrack). Volta ao selecionar.
    // P2-bis (0.9.6.16 — a decisão do dono): o ícone do trilho É o toggle
    // do PIN — tocar ABRE o painel E FIXA (inspPinned, persiste no
    // layout.json; a seta de recolher do cabeçalho desfaz). Sem long-press.
    st.inspTab = 0;   // D5: a 2ª tab morreu — o estado antigo volta a 0
    if (editor::inspectorCollapsed(st)) {
        const UiRect track = safe::inspectorPanelRect(
            ui.screenWidth(), ui.screenHeight(), ui.safeArea(), st.drawerH,
            st.inspW, st.hierW, /*inspTrack=*/true);
        ui.panel(track.x, track.y, track.w, track.h, theme::PANEL);
        ui.panel(track.x, track.y, 1.0f, track.h, theme::LINE);  // separador
        // o alvo do toque: a CÉLULA de 32dp de largura × 40dp de altura no
        // topo do trilho (a LEI DE OURO no toque: o desenho do ícone é 20;
        // a largura é a do TRILHO — o elemento 32dp da spec do dono, a
        // MESMA classe de piso das tabs de baixo, kFieldFloorDp)
        const f32 cellH = theme::dp(40.0f);
        const bool held = ui.widgetActive(kInspTrackPinId);
        if (held) {
            ui.panel(track.x, track.y, track.w, cellH, theme::kTheme.surface2);
        }
        icons::drawIcon(ui, icons::Icon::Inspector,
                        track.x + (track.w - theme::dp(20.0f)) * 0.5f,
                        track.y + (cellH - theme::dp(20.0f)) * 0.5f,
                        theme::dp(20.0f),
                        held ? theme::kTheme.accent : theme::kTheme.text2);
        // a flag ANTES do hit (o piso da classe CAMPO 32 — o trilho é o
        // elemento 32dp da casa, o precedente das tabs de baixo)
        ui.auditRowFloorNext(layout::kFieldFloorDp);
        if (ui.widgetHit(kInspTrackPinId, track.x, track.y, track.w,
                         cellH)) {
            st.inspPinned = true;   // abre E FIXA (P2-bis — o dono decide)
            ui.scrollSetOffset(kIdScrollInsp, 0.0f);
        }
        return false;
    }
    // F4.2: painel inteiro dentro do contentRect — a altura REAL alimenta o
    // beginScroll → o overflow do Inspector é detetado e o scroll ativa (B1)
    // GRUPO D: a largura é ESTADO (divisores) — default adaptativo se −1
    const UiRect panel = safe::inspectorPanelRect(ui.screenWidth(),
                                                   ui.screenHeight(),
                                                   ui.safeArea(), st.drawerH,
                                                   st.inspW, st.hierW);
    const f32 x = panel.x;
    const f32 y = panel.y;
    const f32 w = panel.w;
    const f32 h = panel.h;

    ui.panel(x, y, w, h, theme::PANEL);
    ui.panel(x, y, 1.0f, h, theme::LINE);   // separador esquerdo

    // FASE 9 (G2-8): o TIC selecionado MUDOU → o Inspector volta ao TOPO
    // (o scroll do TIC anterior não se arrasta para o novo — era por isso
    // que o "Transform não aparecia": ficava rolado para lá do fundo)
    if (st.inspPrevSelected != st.selected) {
        ui.scrollSetOffset(kIdScrollInsp, 0.0f);
        st.inspPrevSelected = st.selected;
    }

    // 0.9.6.18 (HOTFIX D1+D5) — O CABEÇALHO NUMA LINHA SÓ: linha 1 =
    // título à esquerda + o recolher (o par do pin) à direita. NUNCA há
    // segunda faixa de conteúdo a partilhar o y do título.
    //   • D1: título e tabs na MESMA faixa y lia-se «InspeInspector Nós»
    //     (a colisão medida no PNG do device a 180dp);
    //   • D5: a 2ª tab «Nós» MORREU — a vista listava TODOS os TICs ativos
    //     em flat (o MESMO conjunto de linhas da hierarquia, sem a árvore,
    //     sem pesquisa, sem olho, sem ⋮) — duplicado da hierarquia, a regra
    //     do dono («duplicado da hierarquia → mata a tab») aplica-se; com
    //     uma tab só a faixa de tabs inteira não existe, e o espaço da
    //     linha 1 liberta-se para o recolher do pin (que antes vivia numa
    //     linha própria por falta de espaço MEDIDO no device).
    {
        const TextMetrics mh = ui.textMetrics();
        const f32 base = y + (kHeaderH - mh.block()) * 0.5f + mh.ascent;
        ui.labelStyled(x + kPad, base, "Inspector", theme::kTheme.text1,
                       theme::fontScale(theme::kFontSection), 0);
        // o RECOLHER (o par do pin do P2-bis): célula 40×28 na DIREITA da
        // linha 1 (o D1 manda «pin/fechar à direita»), só existe ENQUANTO
        // fixado; o piso da classe cabeçalho (kHeadFloorDp)
        if (st.inspPinned) {
            const f32 cw = theme::dp(40.0f);
            const UiRect cell = {x + w - kPad - cw, y, cw, kHeaderH};
            const bool heldU = ui.widgetActive(kInspUnpinId);
            if (heldU) {
                ui.panelRounded(cell.x, cell.y, cell.w, cell.h,
                                theme::dp(theme::kRadiusField),
                                theme::kTheme.surface2);
            }
            icons::drawIcon(ui, icons::Icon::ChevronRight,
                            cell.x + (cell.w - theme::dp(20.0f)) * 0.5f,
                            cell.y + (cell.h - theme::dp(20.0f)) * 0.5f,
                            theme::dp(20.0f),
                            heldU ? theme::kTheme.accent
                                  : theme::kTheme.text2);
            // a flag ANTES do hit (a linha de cabeçalho 28 — o piso da casa)
            ui.auditRowFloorNext(layout::kHeadFloorDp);
            if (ui.widgetHit(kInspUnpinId, cell.x, cell.y, cell.w, cell.h)) {
                st.inspPinned = false;   // DESFAZ o pin (o trilho volta sem
                                         // seleção — a regra PASSO 2)
            }
        }
    }
    ui.panel(x + kPad, y + kHeaderH - 1.0f, w - 2.0f * kPad, 1.0f, theme::LINE);
    // a altura do cabeçalho EFETIVA: a linha 1 sola (a linha própria do
    // recolher MORREU com o espaço libertado pelo D5)
    const f32 headH = kHeaderH;

    // ---- 0.9.6.18 (HOTFIX D5) — a vista «Nós» FOI REMOVIDA --------------
    // Conteúdo real medido pela leitura (P-04): `scene.forEachActive` em
    // flat — o MESMO conjunto de linhas da hierarquia (ícone+nome+toque
    // seleciona) sem a árvore, sem pesquisa, sem olho, sem ⋮. A regra do
    // dono: «duplicado da hierarquia → mata a tab». O st.inspTab fica no
    // estado (retrocompatibilidade do layout.json futuro) e vale 0 —
    // forçado AQUI no topo (o trilho de baixo também sai pelo return).
    Tic* tic = scene.get(st.selected);
    if (!tic) {
        st.selected = Handle::invalid();
        // 0.9.0 (spec C/M): "Nada selecionado" 14sp text-2 CENTRADO —
        // text2 #98A2B3 sobre surface #151A23 = 6,7:1 (≥4,5:1 da auditoria);
        // o ANTIGO "(nada selecionado)" era theme::LINE (1,7:1 — INVISÍVEL,
        // o problema documentado "quase invisível")
        const TextMetrics tm0 = ui.textMetrics();
        const f32 base = y + (h - tm0.block()) * 0.5f + tm0.ascent;
        ui.labelFitted(x + kPad, base, "Nada selecionado",
                       theme::kTheme.text2, w - 2.0f * kPad);
        icons::drawIcon(ui, icons::Icon::Cursor, x + w * 0.5f - 16.0f,
                        base - tm0.block() - 40.0f, 32.0f,
                        theme::kTheme.text2);
        return false;
    }

    // ---- PLANO (fonte única): perfil → linhas sequenciais com y cumulativo
    // 0.9.0 (spec C): o plano SEGUE as secções colapsadas (só cabeçalho entra)
    const TextMetrics tm = ui.textMetrics();
    InspProfile prof = inspectorProfile(*tic);
    // 0.9.2 §4: as vars @+ vêm da run ATIVA (VoniSystem — a central)
    st.scriptVarCount = 0;
    if (prof.script && voni) {
        auto vars = voni->exported(scene, tic->handle);
        for (size_t i = 0; i < vars.size() && i < 8; ++i) {
            std::snprintf(st.scriptVars[i].name,
                          sizeof(st.scriptVars[i].name), "%s",
                          vars[i].name.c_str());
            std::snprintf(st.scriptVars[i].value,
                          sizeof(st.scriptVars[i].value), "%s",
                          voni::valueText(vars[i].value).c_str());
        }
        st.scriptVarCount = (u32)(vars.size() < 8 ? vars.size() : 8);
        prof.scriptExports = st.scriptVarCount;
    }
    const bool selectable = (catalog != nullptr);
    InspRow plan[64];
    const u32 nRows = inspectorPlan(prof, tm, selectable, st.inspCollapsed,
                                    plan);
    const f32 contentH = inspectorContentHeight(prof, tm, selectable,
                                                st.inspCollapsed);

    // região de scroll: abaixo do cabeçalho (+ a linha da seta quando
    // fixado — P2-bis)
    // GRUPO D (plano B): SEM inset — o divisor sobrepõe a borda ESQUERDA e
    // RECLAMA o gesto ANTES do painel (dividerInput corre primeiro no main)
    const f32 contentTop = y + headH + 4.0f;
    const f32 listH = h - headH - 4.0f;

    ui.beginScroll(kIdScrollInsp, {x, contentTop, w, listH}, contentH);
    const f32 off = ui.scrollOffset();

    // ---- payloads (os VALORES continuam a ser lidos dos componentes; as
    // POSIÇÕES vêm todas do plano)
    Transform3D* tr = tic->getComponent<Transform3D>();
    struct SliderSpec { const char* label; f32 min, max; const char* fmt; f32* value; };
    f32 posArr[3] = {};
    f32 rotDeg[3] = {};
    f32 sclArr[3] = {};
    if (tr) {
        posArr[0] = tr->pos.x; posArr[1] = tr->pos.y; posArr[2] = tr->pos.z;
        f32 ex = 0.0f, ey = 0.0f, ez = 0.0f;
        Quat::toEuler(tr->rot, ex, ey, ez);   // rot em graus (extrai do quat)
        rotDeg[0] = rad2deg(ex); rotDeg[1] = rad2deg(ey); rotDeg[2] = rad2deg(ez);
        sclArr[0] = tr->scale.x; sclArr[1] = tr->scale.y; sclArr[2] = tr->scale.z;
    }
    SliderSpec rows9[9] = {
        {"px", -20.0f, 20.0f, "%.2f", &posArr[0]},
        {"py", -20.0f, 20.0f, "%.2f", &posArr[1]},
        {"pz", -20.0f, 20.0f, "%.2f", &posArr[2]},
        {"rx", -180.0f, 180.0f, "%.0f", &rotDeg[0]},
        {"ry", -180.0f, 180.0f, "%.0f", &rotDeg[1]},
        {"rz", -180.0f, 180.0f, "%.0f", &rotDeg[2]},
        {"sx", 0.1f, 5.0f, "%.2f", &sclArr[0]},
        {"sy", 0.1f, 5.0f, "%.2f", &sclArr[1]},
        {"sz", 0.1f, 5.0f, "%.2f", &sclArr[2]},
    };
    const MeshRenderer* mr = tic->getComponent<MeshRenderer>();
    const InputMap* im = tic->getComponent<InputMap>();
    BodyComp* bc = tic->getComponent<BodyComp>();
    CameraComp* camEdit = tic->getComponent<CameraComp>();   // 0.7.7
    // 0.7.0: o tint é EDITÁVEL (sliders R/G/B) — ponteiro mutável
    MeshRenderer* mrEdit = tic->getComponent<MeshRenderer>();
    // 0.8.11 — o AudioPlayer do TIC (clip/preview/toggles/sliders)
    AudioPlayer* auEdit = tic->getComponent<AudioPlayer>();
    char meshLabel[64] = "";
    char texLabel[64] = "";
    char inputLine[48] = "";
    char camProjLabel[48] = "";   // 0.7.7
    char camActiveLabel[48] = "";
    char camFrustumLabel[48] = "";   // 0.7.10: toggle do gizmo
    if (camEdit) {
        std::snprintf(camProjLabel, sizeof(camProjLabel), "projeção: %s",
                      CameraComp::projectionName(camEdit->projection));
        std::snprintf(camActiveLabel, sizeof(camActiveLabel), "ativa: %s",
                      camEdit->active ? "sim" : "não");
        std::snprintf(camFrustumLabel, sizeof(camFrustumLabel), "frustum: %s",
                      camEdit->showFrustum ? "sim" : "não");
    }
    if (mr) {
        // FASE 9 (G2-8): labels de APRESENTAÇÃO (o "mesh: cube" de debug
        // morreu) — nomes legíveis, o glifo — para o vazio
        if (mr->primOn) {
            std::snprintf(meshLabel, sizeof(meshLabel), "malha: (primitiva)");
        } else if (!mr->meshPath.empty()) {
            std::snprintf(meshLabel, sizeof(meshLabel), "malha: %s",
                          assetBasename(mr->meshPath));
        } else {
            std::snprintf(meshLabel, sizeof(meshLabel), "malha: %s",
                          mr->mesh ? "cubo" : "—");
        }
        if (!mr->texPath.empty()) {
            std::snprintf(texLabel, sizeof(texLabel), "textura: %s",
                          assetBasename(mr->texPath));
        } else {
            std::snprintf(texLabel, sizeof(texLabel), "textura: %s",
                          mr->texture ? "ligada" : "—");
        }
    }
    if (im) {
        std::snprintf(inputLine, sizeof(inputLine), "entrada: %s",
                      im->source ? "fonte ligada" : "sem fonte");
    }
    if (bc) {
        // FASE 9 (G2-8): a FÍSICA em DUAS COLUNAS (tipo/forma/chão — o
        // "body: static - obb - cha…" truncado morreu); os valores vivem
        // no TwoCol desenhado com o BodyComp VIVO
        (void)bc;
    }
    // linhas de texto (Label) NA MESMA ORDEM do plano: input → body → tc
    const char* labelTexts[3];
    const f32 (*labelColors[3])[4];
    f32 labelInsets[3];
    u32 nLabels = 0;
    if (im) {
        labelTexts[nLabels] = inputLine;
        labelColors[nLabels] = &theme::TEXT;
        labelInsets[nLabels] = 12.0f;
        ++nLabels;
    }
    // (FASE 9 G2-8: o bodyLine morreu — a FÍSICA desenha-se em DUAS
    // COLUNAS pelas Kind::TwoCol do plano, com o BodyComp VIVO)
    if (im && prof.tc) {
        labelTexts[nLabels] = "tc: stick + jump";
        labelColors[nLabels] = &theme::TEXT;
        labelInsets[nLabels] = 12.0f;
        ++nLabels;
    }
    u32 labelIdx = 0;

    bool edited = false;
    bool trEdited = false;
    bool primEdited = false;   // 0.8.0: parâmetros de primitiva mudaram
    u32 sliderIdx = 0;
    u32 colorIdx = 0;   // 0.7.0: payload dos ColorSlider (0=R, 1=G, 2=B)

    // ---- desenho: UMA passagem pelo plano; nenhum cursor local
    for (u32 i = 0; i < nRows; ++i) {
        const InspRow& r = plan[i];
        const f32 ry = contentTop + r.y - off;   // topo da linha em ECRÃ
        switch (r.kind) {
        case InspRow::Kind::Name:
            ui.labelFitted(x + kPad, inspBaseline(ry, r.h, tm), tic->name.c_str(),
                           theme::ACCENT, w - 2.0f * kPad);   // B2: ellipsis
            break;
        case InspRow::Kind::VisToggle: {
            // 0.7.0 — estado de visibilidade do TIC (mesmo estado do olho
            // da Hierarchy; TIC invisível não desenha em editor nem Play).
            // 0.9.6.18 (HOTFIX D10): a caixa de TEXTO «visível: sim/não»
            // era controlo-de-texto cru — o ESTADO vive agora no ícone da
            // casa (Eye/EyeOff) com o rótulo ao lado (a spec do dono).
            const bool heldV = ui.widgetActive(r.id);
            if (heldV) {
                ui.panel(x + kPad, ry, w - 2.0f * kPad, r.h,
                         theme::kTheme.surface2);
            }
            const f32 sIc = theme::dp(20.0f);
            icons::drawIcon(ui, tic->visible ? icons::Icon::Eye
                                             : icons::Icon::EyeOff,
                            x + kPad + theme::dp(12.0f),
                            ry + (r.h - sIc) * 0.5f, sIc,
                            tic->visible ? theme::kTheme.accent
                                         : theme::kTheme.text2);
            if (ui.hasFont()) {
                ui.labelFitted(x + kPad + theme::dp(44.0f),
                               inspBaseline(ry, r.h, tm), "visível",
                               theme::kTheme.text1, w - 2.0f * kPad - theme::dp(56.0f));
            }
            // PASSO 2 (0.9.6.15): o alvo é a LINHA inteira + a flag da
            // classe linha (o padrão PASSO 1 das tabs)
            ui.auditRowFloorNext(layout::kRowFloorDp);
            ui.widgetHit(r.id, x + kPad, ry, w - 2.0f * kPad, r.h);
            break;
        }
        case InspRow::Kind::ColorSlider: {
            // 0.7.0 — cor por TIC: sliders R/G/B do tint do MeshRenderer
            if (mrEdit) {
                static const char* kColLabels[3] = {"cor R", "cor G", "cor B"};
                if (sliderRow(ui, r.id, x, w, ry, r.h, tm, kColLabels[colorIdx],
                              0.0f, 1.0f, mrEdit->tint[colorIdx], "%.2f")) {
                    edited = true;
                }
            }
            ++colorIdx;
            break;
        }
        case InspRow::Kind::MaterialThumbs: {
            // 0.9.0 (spec C): 3 MINIATURAS 64dp — [textura][albedo+lápis]
            // [preview live = textura × tint]. A textura resolve pelo MESMO
            // imgResolve do canvas (o main liga); sem textura: placeholder
            // mono (moldura + diagonais — o padrão do elemento Image).
            //
            // FASE 9 (G1-3): (a) o tint é f32[3] — as APIs de cor são
            // f32[4]; passar tint direto lia 4 bytes FORA do array (o
            // ALFA era lixo → o quadrado escuro com hex #FFFFFF); agora
            // col[4] explícito com alfa 1. (b) legendas INTEIRAS em linha
            // RESERVADA (a célula = largura útil ÷ 3): "Textura",
            // "Cor base", "Prévia" — nunca mais "text… albe… pre…".
            if (mr) {
                // PASSO 2 (0.9.6.15): TUDO em dp (R-018 — thumbS/gap eram px
                // crus: no device 2.0 as miniaturas saíam a METADE, 32dp, e
                // a legenda desenhava-SE DENTRO da linha seguinte — a 260dp
                // do PASSO 2 a legenda invadia a coluna do rótulo e os
                // glifos colidiam; o plano (inspThumbsH) e o draw coincidem
                // AGORA: dp(4)+dp(44)+legenda+dp(4))
                const f32 thumbS = theme::dp(44.0f);
                const f32 gap = theme::dp(12.0f);
                const f32 rowW = 3.0f * thumbS + 2.0f * gap;
                const f32 tx0 = x + (w - rowW) * 0.5f;
                const f32 ty0 = ry + theme::dp(4.0f);
                // o tint COMO COR RGBA (alfa 1 — o fix do quadrado escuro)
                const f32 tint4[4] = {mr->tint[0], mr->tint[1], mr->tint[2],
                                      1.0f};
                // 1) TEXTURA
                if (!ui.imageQuad(tx0, ty0, thumbS, thumbS, mr->texPath,
                                  tint4)) {
                    ui.frame(tx0, ty0, thumbS, thumbS, 1.0f,
                             theme::kTheme.border);
                    ui.drawLine(tx0, ty0, tx0 + thumbS, ty0 + thumbS, 1.0f,
                                theme::kTheme.border);
                    ui.drawLine(tx0 + thumbS, ty0, tx0, ty0 + thumbS, 1.0f,
                                theme::kTheme.border);
                }
                // 2) ALBEDO (tint) + LÁPIS (editar = tocar no hex abaixo)
                {
                    const f32 ax = tx0 + thumbS + gap;
                    ui.panelRounded(ax, ty0, thumbS, thumbS,
                                    theme::kRadiusCard, theme::kTheme.bg);
                    ui.panelRounded(ax + 4.0f, ty0 + 4.0f, thumbS - 8.0f,
                                    thumbS - 8.0f, theme::kRadiusCard, tint4);
                    ui.frameRounded(ax, ty0, thumbS, thumbS, 1.0f,
                                    theme::kRadiusCard, theme::kTheme.border);
                    icons::drawIcon(ui, icons::Icon::Rename,
                                    ax + thumbS - theme::dp(24.0f), ty0,
                                    theme::dp(20.0f), theme::kTheme.text1);
                }
                // 3) PREVIEW LIVE (textura × tint — como o TIC renderiza)
                {
                    const f32 px = tx0 + 2.0f * (thumbS + gap);
                    if (!ui.imageQuad(px, ty0, thumbS, thumbS, mr->texPath,
                                      tint4)) {
                        ui.panelRounded(px, ty0, thumbS, thumbS,
                                        theme::kRadiusCard, tint4);
                    }
                    ui.frameRounded(px, ty0, thumbS, thumbS, 1.0f,
                                    theme::kRadiusCard, theme::kTheme.border);
                }
                // legendas INTEIRAS: linha reservada por baixo, cada uma na
                // SUA célula (largura útil ÷ 3), a 12sp — "Textura",
                // "Cor base" e "Prévia" cabem INTEIROS (medido: Cor base
                // ≈ 87px @12sp ≤ 92px de célula)
                if (ui.hasFont()) {
                    const TextMetrics m3 = ui.textMetrics();
                    const f32 capScale = theme::fontScale(theme::kFontCaption);
                    const f32 capBlock = m3.block() * capScale;
                    const f32 base = ty0 + thumbS + capBlock + theme::dp(4.0f);
                    static const char* kCaps[3] = {"Textura", "Cor base",
                                                   "Prévia"};
                    const f32 cellW = w / 3.0f;
                    for (u32 t = 0; t < 3; ++t) {
                        // centrada na CÉLULA (não no thumb — a legenda pode
                        // ser mais larga que 64px sem invadir a vizinha)
                        const f32 cx = x + cellW * (0.5f + static_cast<f32>(t));
                        const f32 tw = ui.fontWidth(kCaps[t]) * capScale;
                        if (tw <= cellW - 8.0f) {
                            ui.labelStyled(cx - tw * 0.5f, base, kCaps[t],
                                           theme::kTheme.text2, capScale, 0);
                        } else {
                            // defesa (fonte gigante): ajusta SEM cortar o nome
                            ui.labelStyled(cx - cellW * 0.5f + 4.0f, base,
                                           kCaps[t], theme::kTheme.text2,
                                           capScale * (cellW - 8.0f) / tw, 0);
                        }
                    }
                }
            }
            break;
        }
        case InspRow::Kind::ColorHex: {
            // 0.8.6 — cor por CÓDIGO + 0.9.0: SWATCH 48dp (spec C — a linha
            // de cor de Malha/Material: chips R/G/B + campo hex + swatch);
            // tocar abre o teclado em modo hex (propósito 5)
            if (mrEdit) {
                char hex[12];
                uiHexFormat(mrEdit->tint, hex, sizeof(hex));
                // swatch 48 (à direita; a cor VIVA do tint — RGBA com alfa
                // 1: o tint é f32[3], as APIs de cor são f32[4]; passar o
                // array direto lia FORA — o quadrado ESCURO com #FFFFFF)
                const f32 swS = 48.0f;
                const f32 swX = x + w - kPad - swS;
                const f32 swY = ry + (r.h - swS) * 0.5f;
                const f32 tint4[4] = {mrEdit->tint[0], mrEdit->tint[1],
                                      mrEdit->tint[2], 1.0f};
                ui.panelRounded(swX, swY, swS, swS, theme::kRadiusField,
                                tint4);
                ui.frameRounded(swX, swY, swS, swS, 1.0f, theme::kRadiusField,
                                theme::kTheme.border);
                // o BOTÃO ocupa o resto da linha (hex legível à esquerda)
                char label[40];
                std::snprintf(label, sizeof(label), "hex: %s", hex);
                const f32 btnW = (swX - 8.0f) - (x + kPad);
                ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
                ui.button(r.id, x + kPad, ry + 2.0f, btnW, r.h - 4.0f, label);
            }
            break;
        }
        case InspRow::Kind::Section:
        case InspRow::Kind::CamSection:
        case InspRow::Kind::AuSection: {
            // 0.9.0 (spec C): cabeçalho 48dp COLAPSÁVEL — título 14sp +
            // chevron (baixo = aberto; direita = fechado); o toque alterna o
            // bit no EditorState::inspCollapsed (PERSISTE — spec G)
            const u32 bit = r.payload;
            const bool open = !(st.inspCollapsed & bit);
            static const char* kTitles[8] = {"Transform", "Camera", "Malha",
                                             "Material", "Física", "Áudio",
                                             "Animação", "Script"};
            u32 titleIdx = 0;
            for (u32 b = 0; b < 8; ++b) {
                if (bit == (1u << b)) {
                    titleIdx = b;
                }
            }
            if (ui.widgetActive(r.id)) {
                ui.panel(x + 4.0f, ry, w - 8.0f, r.h, theme::kTheme.surface2);
            }
            const TextMetrics m2 = ui.textMetrics();
            const f32 base = ry + (r.h - m2.block()) * 0.5f + m2.ascent;
            ui.label(x + kPad, base, kTitles[titleIdx], theme::kTheme.text1);
            icons::drawIcon(ui,
                            open ? icons::Icon::ChevronDown
                                 : icons::Icon::ChevronRight,
                            x + w - kPad - theme::dp(20.0f),
                            ry + (r.h - theme::dp(20.0f)) * 0.5f,
                            theme::dp(20.0f), theme::kTheme.text2);
            ui.panel(x + kPad, ry + r.h - 1.0f, w - 2.0f * kPad, 1.0f,
                     theme::kTheme.border);
            break;
        }
        case InspRow::Kind::TransformRow: {
            // 0.9.0 (spec C): "linhas Pos/Rotação/Escala com 3 campos
            // numéricos editáveis, raio 4dp, bordo, rótulos X/Y/Z + botão
            // que repõe a linha". Título em LINHA PRÓPRIA.
            // 0.9.6.18 (HOTFIX D2): o ORÇAMENTO vem de transformRowBudgetPx
            // (a FONTE ÚNICA do draw E do tap) — no painel de 180dp as
            // caixas encolhem a 40dp e o reset vira ÍCONE inline após o Z;
            // NADA desenha fora do rect do painel (o campo Z cortado e o
            // "R" a flutuar eram os sintomas).
            // 0.9.6.18 (HOTFIX D10): o reset deixa de ser o texto "R" nu —
            // é o ícone Reset (seta circular) da casa.
            static const char* kRowTitles[3] = {"Posição", "Rotação", "Escala"};
            const u32 rowIdx = r.payload;
            const TextMetrics m2 = ui.textMetrics();
            const f32 titleBase = ry + theme::dp(2.0f) + m2.ascent;
            ui.label(x + kPad, titleBase, kRowTitles[rowIdx],
                     theme::kTheme.text2);
            const TransformBudget tb =
                transformRowBudgetPx(w - 2.0f * kPad);
            const f32 boxW = tb.boxW;
            const f32 boxH = theme::dp(32.0f);
            const f32 boxY = ry + theme::dp(28.0f);
            const f32 boxBase = boxY + (boxH - m2.block()) * 0.5f + m2.ascent;
            f32 bx = x + kPad;
            for (u32 axis = 0; axis < 3; ++axis) {
                const u32 field = rowIdx * 3 + axis;
                char val[20];
                std::snprintf(val, sizeof(val), "%.2g",
                              rowIdx == 0 ? (&posArr[0])[axis]
                              : rowIdx == 1 ? (&rotDeg[0])[axis]
                                            : (&sclArr[0])[axis]);
                const bool held = ui.widgetActive(kInspFieldBase + field);
                ui.panelRounded(bx, boxY, boxW, boxH,
                                theme::dp(theme::kRadiusField),
                                held ? theme::kTheme.surface2
                                     : theme::kTheme.bg);
                ui.frameRounded(bx, boxY, boxW, boxH, 1.0f,
                                theme::dp(theme::kRadiusField),
                                theme::kTheme.border);
                static const char* kAxis[3] = {"X", "Y", "Z"};
                // 0.9.6.10 (GRUPO UI · a imagem 1): os rótulos X/Y/Z
                // COLORIDOS pelas cores dos EIXOS do gizmo (os tokens do
                // Theme — conteúdo, a exceção documentada de sempre)
                static const f32* const kAxisCol[3] = {
                    theme::kTheme.axisX, theme::kTheme.axisY,
                    theme::kTheme.axisZ};
                if (ui.hasFont()) {
                    ui.label(bx + theme::dp(6.0f), boxBase, kAxis[axis],
                             kAxisCol[axis]);
                    // valor ENTRE o rótulo do eixo e a borda direita —
                    // labelFitted TRUNCA (a auditoria de glifos vigia)
                    const f32 maxVw = boxW - theme::dp(6.0f) - theme::dp(14.0f) -
                                      theme::dp(8.0f);
                    char fitted[16];
                    const char* shown = val;
                    if (ui.fontWidth(val) > maxVw) {
                        textfit::ellipsize(
                            val, maxVw,
                            [&](const char* str) { return ui.fontWidth(str); },
                            fitted, sizeof(fitted));
                        shown = fitted;
                    }
                    const f32 vw = ui.fontWidth(shown);
                    ui.label(bx + boxW - theme::dp(6.0f) - vw, boxBase, shown,
                             theme::kTheme.text1);
                }
                bx += boxW + theme::dp(kTfGapBox);
            }
            // o RESET (D2+D10): CHIP 40×32 com o ícone quando cabe; ÍCONE
            // inline 20 após o Z abaixo do piso (a spec do dono)
            const f32 rW = tb.resetW;
            const f32 rX = x + kPad + 3.0f * boxW + 2.0f * theme::dp(kTfGapBox) +
                           theme::dp(kTfGapReset);
            const f32 rHeld0 = ui.widgetActive(r.id);
            if (!tb.resetIcon) {
                ui.panelRounded(rX, boxY, rW, boxH,
                                theme::dp(theme::kRadiusField),
                                rHeld0 ? theme::kTheme.accentPress
                                       : theme::kTheme.surface);
            }
            icons::drawIcon(ui, icons::Icon::Reset, rX + (rW - theme::dp(20.0f)) * 0.5f,
                            boxY + (boxH - theme::dp(20.0f)) * 0.5f,
                            theme::dp(20.0f),
                            rHeld0 ? theme::kTheme.accent
                                   : theme::kTheme.text1);
            break;
        }
        case InspRow::Kind::Slider:
            if (tr && sliderIdx < 9) {
                const SliderSpec& sp = rows9[sliderIdx];
                ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
                if (sliderRow(ui, r.id, x, w, ry, r.h, tm, sp.label, sp.min, sp.max,
                              *sp.value, sp.fmt, true)) {   // 0.8.9: valor tocável (campo numérico)
                    trEdited = true;
                    edited = true;
                }
            }
            ++sliderIdx;
            break;
        case InspRow::Kind::Velx:
            if (bc) {
                f32 vx = bc->velocity.x;
                ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
                if (sliderRow(ui, r.id, x, w, ry, r.h, tm, "velx", -60.0f, 60.0f,
                              vx, "%.1f")) {
                    bc->velocity.x = vx;
                    edited = true;
                }
            }
            break;
        case InspRow::Kind::MeshButton:
            // botão da linha INTEIRA, centrado na linha do plano (o botão
            // antigo sangrava 2 px para a linha de baixo)
            ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
            // PASSO 2 (0.9.6.15): o alvo é a LINHA inteira (o padrão PASSO 1
            // das tabs; antes o botão desenhava r.h−4 = 34dp < o piso) + a
            // flag da classe linha
            ui.auditRowFloorNext(layout::kRowFloorDp);
            ui.button(r.id, x + kPad, ry, w - 2.0f * kPad, r.h,
                      meshLabel);
            break;
        case InspRow::Kind::TexButton:
            ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
            // PASSO 2 (0.9.6.15): o alvo é a LINHA inteira (o padrão PASSO 1
            // das tabs; antes o botão desenhava r.h−4 = 34dp < o piso) + a
            // flag da classe linha
            ui.auditRowFloorNext(layout::kRowFloorDp);
            ui.button(r.id, x + kPad, ry, w - 2.0f * kPad, r.h,
                      texLabel);
            break;
        case InspRow::Kind::MeshLabel:
            ui.labelFitted(x + kPad + 12.0f, inspBaseline(ry, r.h, tm), meshLabel,
                           theme::TEXT, w - 2.0f * kPad - 12.0f);
            break;
        case InspRow::Kind::TexLabel:
            ui.labelFitted(x + kPad + 12.0f, inspBaseline(ry, r.h, tm), texLabel,
                           theme::TEXT, w - 2.0f * kPad - 12.0f);
            break;
        // ---- 0.8.9 — IMPORT: dims originais + escala original -----------------
        case InspRow::Kind::DimsLabel: {
            // AABB real do mesh carregado (dados do create — sem GL aqui)
            char dimsLine[64] = "dims: -";
            if (mr && mr->mesh) {
                const Vec3 ext = mr->mesh->boundsExtent();
                std::snprintf(dimsLine, sizeof(dimsLine),
                              "dims: %.4g x %.4g x %.4g", ext.x, ext.y, ext.z);
            }
            ui.labelFitted(x + kPad, inspBaseline(ry, r.h, tm), dimsLine,
                           theme::TEXT, w - 2.0f * kPad);
            break;
        }
        case InspRow::Kind::ScaleOrig:
            // repõe a escala {1,1,1} — o "tamanho original" do modelo (o fit
            // uniforme vive no Transform3D, a geometria nunca foi tocada)
            ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
            // PASSO 2 (0.9.6.15): o alvo é a LINHA inteira (o padrão PASSO 1
            // das tabs; antes o botão desenhava r.h−4 = 34dp < o piso) + a
            // flag da classe linha
            ui.auditRowFloorNext(layout::kRowFloorDp);
            ui.button(r.id, x + kPad, ry, w - 2.0f * kPad, r.h,
                      "escala: original");
            break;
        case InspRow::Kind::ScriptEdit: {
            // 0.9.2 §10: abre o EDITOR DE SCRIPT (portrait + IME — o main
            // faz o par inseparável ao ver o flag)
            if (ui.widgetActive(r.id)) {
                ui.panel(x + 4.0f, ry, w - 8.0f, r.h, theme::kTheme.surface2);
            }
            const TextMetrics m2 = ui.textMetrics();
            const f32 base = ry + (r.h - m2.block()) * 0.5f + m2.ascent;
            icons::drawIcon(ui, icons::Icon::Terminal, x + kPad,
                            ry + (r.h - 20.0f) * 0.5f, 20.0f,
                            theme::kTheme.accent);
            ui.label(x + kPad + 32.0f, base, "Editar script",
                     theme::kTheme.text1);
            ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
            if (ui.widgetHit(r.id, x, ry, w, r.h)) {
                st.requestScriptEditor = true;   // o main abre (com o par)
                st.scriptEditorTarget = tic->handle;
            }
            break;
        }
        case InspRow::Kind::ScriptAdd: {
            // 0.9.2 §10: cria o componente Script (vazio) no TIC
            if (ui.widgetActive(r.id)) {
                ui.panel(x + 4.0f, ry, w - 8.0f, r.h, theme::kTheme.surface2);
            }
            const TextMetrics m2 = ui.textMetrics();
            const f32 base = ry + (r.h - m2.block()) * 0.5f + m2.ascent;
            icons::drawIcon(ui, icons::Icon::Plus, x + kPad,
                            ry + (r.h - 20.0f) * 0.5f, 20.0f,
                            theme::kTheme.accent);
            ui.label(x + kPad + 32.0f, base, "Adicionar script",
                     theme::kTheme.text1);
            ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
            if (ui.widgetHit(r.id, x, ry, w, r.h) &&
                !tic->getComponent<ScriptComp>()) {
                tic->addComponent<ScriptComp>();
                elog::info("voni: componente Script criado no TIC '%s'",
                           tic->name.c_str());
            }
            break;
        }
        case InspRow::Kind::ScriptVar: {
            // 0.9.2 §4: variável @+ exportada — leitura (nome = valor ·Tipo)
            const TextMetrics m2 = ui.textMetrics();
            const f32 base = ry + (r.h - m2.block()) * 0.5f + m2.ascent;
            char line[96];
            const size_t idx = r.payload < st.scriptVarCount ? r.payload : 0;
            if (idx < st.scriptVarCount) {
                std::snprintf(line, sizeof(line), "%s = %s",
                              st.scriptVars[idx].name,
                              st.scriptVars[idx].value);
            } else {
                line[0] = '\0';
            }
            ui.label(x + kPad + 8.0f, base, line, theme::kTheme.text2);
            break;
        }
        case InspRow::Kind::Label:
            // input: → tc: — payload NA ORDEM do plano (labelIdx)
            if (labelIdx < nLabels) {
                const f32 inset = labelInsets[labelIdx];
                ui.labelFitted(x + kPad + inset, inspBaseline(ry, r.h, tm),
                               labelTexts[labelIdx], *labelColors[labelIdx],
                               w - 2.0f * kPad - inset);
                ++labelIdx;
            }
            break;
        case InspRow::Kind::TwoCol: {
            // FASE 9 (G2-8): FÍSICA em duas colunas — NOME à esquerda
            // (text2), VALOR à direita (text1); SEM truncagem (cada
            // campo curto por natureza — static/obb/sim cabem inteiros)
            if (bc) {
                const char* nome = "tipo";
                const char* valor = BodyComp::typeName(bc->type);
                if (r.payload == 1) {
                    nome = "forma";
                    valor = BodyComp::shapeName(bc->shape);
                } else if (r.payload == 2) {
                    nome = "no chão";
                    valor = bc->grounded ? "sim" : "não";
                }
                const f32 base = inspBaseline(ry, r.h, tm);
                ui.label(x + kPad, base, nome, theme::kTheme.text2);
                if (ui.hasFont()) {
                    const f32 vw = ui.fontWidth(valor);
                    ui.label(x + w - kPad - vw, base, valor,
                             theme::kTheme.text1);
                }
            }
            break;
        }
        case InspRow::Kind::AddTc:
            ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
            // PASSO 2 (0.9.6.15): o alvo é a LINHA inteira (o padrão PASSO 1
            // das tabs; antes o botão desenhava r.h−4 = 34dp < o piso) + a
            // flag da classe linha
            ui.auditRowFloorNext(layout::kRowFloorDp);
            ui.button(r.id, x + kPad, ry, w - 2.0f * kPad, r.h,
                      "add TouchControls");
            break;
        // ---- 0.7.7 — CÂMARA --------------------------------------------------

        case InspRow::Kind::CamFov:
            if (camEdit) {
                ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
                if (sliderRow(ui, r.id, x, w, ry, r.h, tm, "fov",
                              CameraComp::kMinFov, CameraComp::kMaxFov,
                              camEdit->fovY, "%.0f")) {
                    edited = true;
                }
            }
            break;
        case InspRow::Kind::CamNear:
            if (camEdit) {
                ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
                if (sliderRow(ui, r.id, x, w, ry, r.h, tm, "near",
                              CameraComp::kMinNear, 10.0f, camEdit->nearZ,
                              "%.2f")) {
                    edited = true;
                }
            }
            break;
        case InspRow::Kind::CamFar:
            if (camEdit) {
                ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
                if (sliderRow(ui, r.id, x, w, ry, r.h, tm, "far",
                              CameraComp::kMinFar, CameraComp::kMaxFar,
                              camEdit->farZ, "%.0f")) {
                    edited = true;
                }
            }
            break;
        case InspRow::Kind::CamOrtho:
            if (camEdit) {
                ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
                if (sliderRow(ui, r.id, x, w, ry, r.h, tm, "ortho", 0.5f, 50.0f,
                              camEdit->orthoSize, "%.2f")) {
                    edited = true;
                }
            }
            break;
        case InspRow::Kind::CamProj:
            ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
            // PASSO 2 (0.9.6.15): o alvo é a LINHA inteira (o padrão PASSO 1
            // das tabs; antes o botão desenhava r.h−4 = 34dp < o piso) + a
            // flag da classe linha
            ui.auditRowFloorNext(layout::kRowFloorDp);
            ui.button(r.id, x + kPad, ry, w - 2.0f * kPad, r.h,
                      camProjLabel);
            break;
        case InspRow::Kind::CamActive:
            ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
            // PASSO 2 (0.9.6.15): o alvo é a LINHA inteira (o padrão PASSO 1
            // das tabs; antes o botão desenhava r.h−4 = 34dp < o piso) + a
            // flag da classe linha
            ui.auditRowFloorNext(layout::kRowFloorDp);
            ui.button(r.id, x + kPad, ry, w - 2.0f * kPad, r.h,
                      camActiveLabel);
            break;
        case InspRow::Kind::CamFrustum:
            // 0.7.10 — toggle de visibilidade do GIZMO (o render no Play
            // NÃO muda; só o frustum do editor se esconde)
            ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
            // PASSO 2 (0.9.6.15): o alvo é a LINHA inteira (o padrão PASSO 1
            // das tabs; antes o botão desenhava r.h−4 = 34dp < o piso) + a
            // flag da classe linha
            ui.auditRowFloorNext(layout::kRowFloorDp);
            ui.button(r.id, x + kPad, ry, w - 2.0f * kPad, r.h,
                      camFrustumLabel);
            break;
        // ---- 0.8.0 (F7) — PRIMITIVA + ANIMAÇÃO ------------------------------
        case InspRow::Kind::PrimButton: {
            char primLabel[48];
            if (mr && mr->primOn) {
                std::snprintf(primLabel, sizeof(primLabel), "prim: %s",
                              primName(mr->prim.kind));
            } else {
                std::snprintf(primLabel, sizeof(primLabel), "prim: -");
            }
            ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
            // PASSO 2 (0.9.6.15): o alvo é a LINHA inteira (o padrão PASSO 1
            // das tabs; antes o botão desenhava r.h−4 = 34dp < o piso) + a
            // flag da classe linha
            ui.auditRowFloorNext(layout::kRowFloorDp);
            ui.button(r.id, x + kPad, ry, w - 2.0f * kPad, r.h,
                      primLabel);
            break;
        }
        case InspRow::Kind::PrimSlider: {
            if (mrEdit && mrEdit->primOn) {
                // payload pelo ID (o plano empurra R → Seg → Rings na ordem
                // do tipo; o box só tem R=tamanho)
                if (r.id == kInspectorPrimR) {
                    // 0.8.10: esfera → raio; box → tamanho (mesma row)
                    const bool isSize = mrEdit->prim.kind == PrimKind::Box;
                    f32* val = isSize ? &mrEdit->prim.size : &mrEdit->prim.radius;
                    ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
                    if (sliderRow(ui, r.id, x, w, ry, r.h, tm,
                                  isSize ? "tamanho" : "raio", 0.05f, 4.0f,
                                  *val, "%.2f")) {
                        primEdited = true;
                    }
                } else if (r.id == kInspectorPrimSeg) {
                    f32 seg = static_cast<f32>(mrEdit->prim.segments);
                    ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
                    if (sliderRow(ui, r.id, x, w, ry, r.h, tm, "segmentos", 3.0f,
                                  64.0f, seg, "%.0f")) {
                        mrEdit->prim.segments = static_cast<i32>(seg + 0.5f);
                        primEdited = true;
                    }
                } else if (r.id == kInspectorPrimRings) {
                    f32 rg = static_cast<f32>(mrEdit->prim.rings);
                    ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
                    if (sliderRow(ui, r.id, x, w, ry, r.h, tm, "aneis", 2.0f,
                                  64.0f, rg, "%.0f")) {
                        mrEdit->prim.rings = static_cast<i32>(rg + 0.5f);
                        primEdited = true;
                    }
                }
            }
            break;
        }
        case InspRow::Kind::AddAnim:
            ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
            // PASSO 2 (0.9.6.15): o alvo é a LINHA inteira (o padrão PASSO 1
            // das tabs; antes o botão desenhava r.h−4 = 34dp < o piso) + a
            // flag da classe linha
            ui.auditRowFloorNext(layout::kRowFloorDp);
            ui.button(r.id, x + kPad, ry, w - 2.0f * kPad, r.h,
                      "adicionar Animação");
            break;
        case InspRow::Kind::AnimLabel: {
            char animLine[64];
            u32 tracks = 0, keys = 0;
            if (const AnimationPlayer* ap = tic->getComponent<AnimationPlayer>()) {
                if (const AnimClip* c = ap->activeClipPtr()) {
                    tracks = static_cast<u32>(c->tracks.size());
                    for (const AnimTrack& t : c->tracks) {
                        keys += static_cast<u32>(t.keys.size());
                    }
                }
            }
            std::snprintf(animLine, sizeof(animLine), "anim: %u track%s · %u keys",
                          tracks, tracks == 1 ? "" : "s", keys);
            ui.labelFitted(x + kPad, inspBaseline(ry, r.h, tm), animLine,
                           theme::TEXT, w - 2.0f * kPad);
            break;
        }
        // ---- 0.8.11 — ÁUDIO --------------------------------------------------
        case InspRow::Kind::AuClip: {
            // "clip: <nome>" — o caminho completo fica no log/toast; aqui o
            // NOME limpo (sem pasta/extensão) como o seletor
            char clipLine[48];
            if (auEdit && auEdit->hasClip()) {
                const size_t slash = auEdit->clipPath.rfind('/');
                const size_t dot = auEdit->clipPath.rfind('.');
                std::snprintf(clipLine, sizeof(clipLine), "clip: %s",
                              auEdit->clipPath.substr(
                                  slash + 1,
                                  dot == std::string::npos
                                      ? std::string::npos
                                      : dot - slash - 1).c_str());
            } else {
                std::snprintf(clipLine, sizeof(clipLine), "clip: -");
            }
            ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
            // PASSO 2 (0.9.6.15): o alvo é a LINHA inteira (o padrão PASSO 1
            // das tabs; antes o botão desenhava r.h−4 = 34dp < o piso) + a
            // flag da classe linha
            ui.auditRowFloorNext(layout::kRowFloorDp);
            ui.button(r.id, x + kPad, ry, w - 2.0f * kPad, r.h,
                      clipLine);
            break;
        }
        case InspRow::Kind::AuPlay:
            // o PREVIEW: o botão faz toggle do FLAG — o main (frame) mapeia
            // o flag ao misturador (o mesmo caminho do Play; puro aqui)
            ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
            // PASSO 2 (0.9.6.15): o alvo é a LINHA inteira (o padrão PASSO 1
            // das tabs; antes o botão desenhava r.h−4 = 34dp < o piso) + a
            // flag da classe linha
            ui.auditRowFloorNext(layout::kRowFloorDp);
            ui.button(r.id, x + kPad, ry, w - 2.0f * kPad, r.h,
                      auEdit && auEdit->previewing ? "parar" : "ouvir");
            break;
        case InspRow::Kind::AuAutoplay:
            ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
            // PASSO 2 (0.9.6.15): o alvo é a LINHA inteira (o padrão PASSO 1
            // das tabs; antes o botão desenhava r.h−4 = 34dp < o piso) + a
            // flag da classe linha
            ui.auditRowFloorNext(layout::kRowFloorDp);
            ui.button(r.id, x + kPad, ry, w - 2.0f * kPad, r.h,
                      auEdit && auEdit->autoplay ? "autoplay: sim"
                                                 : "autoplay: não");
            break;
        case InspRow::Kind::AuLoop:
            ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
            // PASSO 2 (0.9.6.15): o alvo é a LINHA inteira (o padrão PASSO 1
            // das tabs; antes o botão desenhava r.h−4 = 34dp < o piso) + a
            // flag da classe linha
            ui.auditRowFloorNext(layout::kRowFloorDp);
            ui.button(r.id, x + kPad, ry, w - 2.0f * kPad, r.h,
                      auEdit && auEdit->loop ? "loop: sim" : "loop: não");
            break;
        case InspRow::Kind::AuVolume:
            if (auEdit) {
                ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
                if (sliderRow(ui, r.id, x, w, ry, r.h, tm, "volume", 0.0f, 1.0f,
                              auEdit->volume, "%.2f")) {
                    edited = true;
                }
            }
            break;
        case InspRow::Kind::AuPitch:
            if (auEdit) {
                ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
                if (sliderRow(ui, r.id, x, w, ry, r.h, tm, "pitch", 0.5f, 2.0f,
                              auEdit->pitch, "%.2f")) {
                    edited = true;
                }
            }
            break;
        case InspRow::Kind::AuPos:
            ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
            // PASSO 2 (0.9.6.15): o alvo é a LINHA inteira (o padrão PASSO 1
            // das tabs; antes o botão desenhava r.h−4 = 34dp < o piso) + a
            // flag da classe linha
            ui.auditRowFloorNext(layout::kRowFloorDp);
            ui.button(r.id, x + kPad, ry, w - 2.0f * kPad, r.h,
                      auEdit && auEdit->posicional ? "posicional: sim"
                                                   : "posicional: não");
            break;
        case InspRow::Kind::AuRint:
            if (auEdit) {
                ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
                if (sliderRow(ui, r.id, x, w, ry, r.h, tm, "r. interno", 0.1f,
                              20.0f, auEdit->raioInterno, "%.1f")) {
                    auEdit->clampFields();
                    edited = true;
                }
            }
            break;
        case InspRow::Kind::AuRext:
            if (auEdit) {
                ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha da spec
                if (sliderRow(ui, r.id, x, w, ry, r.h, tm, "r. externo", 0.5f,
                              50.0f, auEdit->raioExterno, "%.1f")) {
                    auEdit->clampFields();
                    edited = true;
                }
            }
            break;
        }
    }

    // escreve de volta no componente (rot: graus → quat YXZ)
    if (tr && trEdited) {
        tr->pos = Vec3{posArr[0], posArr[1], posArr[2]};
        tr->rot = Quat::fromEuler(deg2rad(rotDeg[0]), deg2rad(rotDeg[1]), deg2rad(rotDeg[2]));
        tr->scale = Vec3{sclArr[0], sclArr[1], sclArr[2]};
        tr->updateWorld();   // feedback imediato (TransformSystem reconfirma)
    }

    // 0.8.0 (F7) / 0.8.10 — parâmetros de primitiva mudaram: assinatura
    // nova → o mesh atual está STALE; liberta o ponteiro (PEDIDO pendente)
    // e o MAIN sobe o novo mesh no PONTO SEGURO do frame seguinte (início,
    // antes da submissão — troca determinística sem cache; o antigo morre
    // por deferred free)
    if (primEdited && mrEdit && mrEdit->primOn) {
        primClamp(mrEdit->prim);
        // 0.8.10: PEDIDO pendente — o mesh ANTIGO continua a renderizar;
        // o main sobe o novo no ponto seguro do frame seguinte e o bind
        // retira o antigo p/ cova (deferred free). Falha = mantém o antigo.
        if (!mrEdit->primPending) {
            mrEdit->primPrev = mrEdit->prim;   // label "de" da 1ª edição
        }
        mrEdit->primPending = true;
        mrEdit->primNeg = false;   // pedido NOVO = nova tentativa
    }

    ui.endScroll();

    // tap re-despachado → linhas interativas do PLANO (hit-test do rect em
    // ecrã, igual ao que foi desenhado — nunca diverge)
    f32 tx, ty;
    if (ui.scrollTap(kInspectorScrollId, tx, ty)) {
        u32 sliderTapIdx = 0;   // 0.8.9: índice do slider (mesma ordem do draw)
        for (u32 i = 0; i < nRows; ++i) {
            const InspRow& r = plan[i];
            const bool isSection =
                r.kind == InspRow::Kind::Section ||
                r.kind == InspRow::Kind::CamSection ||
                r.kind == InspRow::Kind::AuSection;
            if (r.kind != InspRow::Kind::AddTc &&
                r.kind != InspRow::Kind::MeshButton &&
                r.kind != InspRow::Kind::TexButton &&
                r.kind != InspRow::Kind::VisToggle &&
                r.kind != InspRow::Kind::ColorHex &&    // 0.8.6
                r.kind != InspRow::Kind::CamProj &&     // 0.7.7
                r.kind != InspRow::Kind::CamActive &&
                r.kind != InspRow::Kind::CamFrustum &&  // 0.7.10
                r.kind != InspRow::Kind::PrimButton &&  // 0.8.0
                r.kind != InspRow::Kind::AddAnim &&     // 0.8.0
                r.kind != InspRow::Kind::ScaleOrig &&   // 0.8.9
                r.kind != InspRow::Kind::AuClip &&      // 0.8.11: clip ▸
                r.kind != InspRow::Kind::AuPlay &&      // 0.8.11: ouvir/parar
                r.kind != InspRow::Kind::AuAutoplay &&  // 0.8.11
                r.kind != InspRow::Kind::AuLoop &&      // 0.8.11
                r.kind != InspRow::Kind::AuPos &&       // 0.8.11
                !isSection &&                           // 0.9.0: colapsar
                r.kind != InspRow::Kind::TransformRow &&// 0.9.0: caixas X/Y/Z + R
                r.kind != InspRow::Kind::Slider) {      // 0.8.9: zona do VALOR
                continue;
            }
            const f32 ry = contentTop + r.y - off;
            if (tx < x + kPad || tx >= x + w - kPad) {
                if (r.kind == InspRow::Kind::Slider) {
                    ++sliderTapIdx;   // mantém a contagem alinhada com o draw
                }
                continue;
            }
            if (ty < ry + 2.0f || ty >= ry + r.h - 2.0f) {
                if (r.kind == InspRow::Kind::Slider) {
                    ++sliderTapIdx;
                }
                continue;   // mesmo rect do botão desenhado (+2/−2)
            }
            if (isSection) {
                // 0.9.0 (spec C): COLAPSAR/ABRIR a secção (bitmask persistente)
                st.inspCollapsed ^= r.payload;
            } else if (r.kind == InspRow::Kind::TransformRow) {
                // 0.9.0 (spec C): caixa X/Y/Z → teclado numérico (purpose 6,
                // campo = rowIdx*3+axis) · reset → repõe a linha.
                // 0.9.6.18 (HOTFIX D2): a matemática vem de
                // transformRowBudgetPx — a MESMA do draw (antes eram DUAS
                // fórmulas, e o tap usava gaps em px crus: no device 2.0
                // o tap errava as caixas). A linha das caixas vive na
                // faixa Y [ry+28, ry+60) (dp: boxY ry+28, boxH 32).
                if (ty >= ry + theme::dp(28.0f) &&
                    ty < ry + theme::dp(28.0f) + theme::dp(32.0f)) {
                const TransformBudget tb2 =
                    transformRowBudgetPx(w - 2.0f * kPad);
                const f32 boxW = tb2.boxW;
                f32 bx = x + kPad;
                const f32 rResetX = x + kPad + 3.0f * boxW +
                                    2.0f * theme::dp(kTfGapBox) +
                                    theme::dp(kTfGapReset);
                bool handled = false;
                for (u32 axis = 0; axis < 3 && !handled; ++axis) {
                    if (tx >= bx && tx < bx + boxW) {
                        const u32 field = r.payload * 3 + axis;
                        char cur[20];
                        std::snprintf(cur, sizeof(cur), "%.4g",
                                      r.payload == 0 ? (&posArr[0])[axis]
                                      : r.payload == 1 ? (&rotDeg[0])[axis]
                                                       : (&sclArr[0])[axis]);
                        openTextInput(st, 6, st.selected,
                                      static_cast<i32>(field), cur);
                        handled = true;
                    }
                    bx += boxW + theme::dp(kTfGapBox);
                }
                if (!handled && tx >= rResetX && tx < x + w - kPad) {
                    // o RESET: repõe a linha ao default (0/0/0 ou 1/1/1)
                    if (tr) {
                        if (r.payload == 0) {
                            tr->pos = Vec3{0.0f, 0.0f, 0.0f};
                        } else if (r.payload == 1) {
                            tr->rot = Quat::identity();
                        } else {
                            tr->scale = Vec3{1.0f, 1.0f, 1.0f};
                        }
                        tr->updateWorld();
                    }
                }
                }
            } else if (r.kind == InspRow::Kind::AddTc) {
                tic->addComponent<TouchControls>();   // F4: cria no TIC
            } else if (r.kind == InspRow::Kind::MeshButton ||
                       r.kind == InspRow::Kind::TexButton ||
                       r.kind == InspRow::Kind::PrimButton) {
                // 0.8.12 — GUARDA DE UI: as linhas de picker (mesh/tex/prim)
                // só abrem o seletor com um TIC VIVO selecionado que tenha
                // MeshRenderer. Sem alvo: hint (o main converte o flag em
                // toast + log "ui: pick bloqueado (sem seleção)") — nunca
                // o caminho "ERRO(sem TIC com mesh selecionado)" do C33.
                if (pickerGuardBlocked(scene, st.selected)) {
                    st.pickBlockedHint = true;
                } else if (r.kind == InspRow::Kind::MeshButton) {
                    st.assetMenu = 1;                 // F5-E: seletor de meshes
                } else if (r.kind == InspRow::Kind::TexButton) {
                    st.assetMenu = 2;                 // F5-E: seletor de texturas
                } else {
                    st.assetMenu = 4;                 // 0.8.0: seletor de primitivas
                }
            } else if (r.kind == InspRow::Kind::AddAnim) {
                // 0.8.0 (F7): cria o player — a TIMELINE abre sozinha (o
                // main desenha-a quando o TIC selecionado tem player)
                tic->addComponent<AnimationPlayer>();
            } else if (r.kind == InspRow::Kind::AuClip) {
                // 0.8.11 — seletor de CLIPS (o catálogo audio/ do projeto;
                // a "importar…" do seletor abre o navegador — o main decide)
                st.assetMenu = 5;
            } else if (r.kind == InspRow::Kind::AuPlay) {
                // 0.8.11 — PREVIEW: toggle do FLAG; o frame do main mapeia
                // ao misturador (audioPreviewTick — o MESMO caminho do Play)
                if (AudioPlayer* au = tic->getComponent<AudioPlayer>()) {
                    au->previewing = !au->previewing;
                }
            } else if (r.kind == InspRow::Kind::AuAutoplay) {
                if (AudioPlayer* au = tic->getComponent<AudioPlayer>()) {
                    au->autoplay = !au->autoplay;
                }
            } else if (r.kind == InspRow::Kind::AuLoop) {
                if (AudioPlayer* au = tic->getComponent<AudioPlayer>()) {
                    au->loop = !au->loop;
                }
            } else if (r.kind == InspRow::Kind::AuPos) {
                if (AudioPlayer* au = tic->getComponent<AudioPlayer>()) {
                    au->posicional = !au->posicional;
                    au->clampFields();
                }
            } else if (r.kind == InspRow::Kind::ColorHex) {
                // 0.8.6 — teclado em MODO HEX (propósito 5) com o hex atual
                // do tint; o commit aplica R/G/B (inválido = estado intacto)
                if (mrEdit) {
                    char cur[12];
                    uiHexFormat(mrEdit->tint, cur, sizeof(cur));
                    openTextInput(st, 5, st.selected, -1, cur);
                }
            } else if (r.kind == InspRow::Kind::VisToggle) {
                tic->visible = !tic->visible;         // 0.7.0: checkbox
            } else if (r.kind == InspRow::Kind::CamProj) {
                // 0.7.7: cicla perspetiva ↔ ortográfica
                if (CameraComp* cc = tic->getComponent<CameraComp>()) {
                    cc->projection = cc->projection ==
                                     CameraComp::Projection::Perspective
                                         ? CameraComp::Projection::Orthographic
                                         : CameraComp::Projection::Perspective;
                }
            } else if (r.kind == InspRow::Kind::CamActive) {
                // 0.7.7: UMA ativa por cena — ativar desativa as outras;
                // desativar deixa a cena sem ativa (fallback da orbit)
                if (CameraComp* cc = tic->getComponent<CameraComp>()) {
                    if (cc->active) {
                        clearActiveCamera(scene, tic->handle);
                    } else {
                        setOnlyActiveCamera(scene, tic->handle);
                    }
                }
            } else if (r.kind == InspRow::Kind::CamFrustum) {
                // 0.7.10: esconder o frustum quando polui (só o GIZMO —
                // a câmara continua a valer para o render no Play)
                if (CameraComp* cc = tic->getComponent<CameraComp>()) {
                    cc->showFrustum = !cc->showFrustum;
                }
            } else if (r.kind == InspRow::Kind::ScaleOrig) {
                // 0.8.9 — IMPORT: repõe a escala original {1,1,1} (a
                // geometria nunca foi tocada — o fit vive no Transform3D)
                Transform3D* trSo = tic->getComponent<Transform3D>();
                if (trSo) {
                    trSo->scale = Vec3{1.0f, 1.0f, 1.0f};
                    trSo->updateWorld();
                } else if (Transform3D* trNew =
                               tic->addComponent<Transform3D>()) {
                    trNew->scale = Vec3{1.0f, 1.0f, 1.0f};
                }
            } else if (r.kind == InspRow::Kind::Slider) {
                // 0.8.9 — CAMPO NUMÉRICO SEM TETO: toque na zona do VALOR
                // (à direita do trilho, [x+206 .. x+w-kPad]) abre o
                // teclado numérico (propósito 6) com o valor atual; o
                // trilho continua a ser do SLIDER (gesto suave no range).
                if (tx >= x + 206.0f && tr && sliderTapIdx < 9) {
                    char cur[24];
                    const SliderSpec& sp = rows9[sliderTapIdx];
                    std::snprintf(cur, sizeof(cur), sp.fmt, *sp.value);
                    // st.textElement leva o ÍNDICE do campo (0..8)
                    openTextInput(st, 6, st.selected,
                                  static_cast<i32>(sliderTapIdx), cur);
                }
            }
            if (r.kind == InspRow::Kind::Slider) {
                ++sliderTapIdx;   // avança SEMPRE (alinhado com o draw)
            }
        }
    }

    return edited;
}

// 0.7.4 — NÚCLEO PARAMETRIZÁVEL do desenho dos TouchControls: o Play chama
// com a safe-area real (escala 1); o viewport 2D do editor chama com o
// transform do mini-ecrã (ox/oy = origem da ÁREA ÚTIL no espaço de desenho,
// aw/ah = área útil em px de DESIGN, scale = design→desenho). O mesmo código
// desenha os DOIS — PARIDADE de aparência (o C33 via um proxy simplificado
// no editor: sem botão JUMP, knob/métricas próprias).
void drawTouchControlsAt(UiContext& ui, const TouchControls& tc, f32 ox,
                         f32 oy, f32 aw, f32 ah, f32 scale) {
    const TouchControls::Layout l = tc.layoutFor(aw, ah);
    const f32 k = scale;
    const f32 jx = ox + l.joyCX * k;
    const f32 jy = oy + l.joyCY * k;
    const f32 joyR = l.joyR * k;
    // cor EDITÁVEL do joystick (default = LINE do tema mono)
    const f32 joyCol[4] = {tc.colR, tc.colG, tc.colB, 1.0f};

    // joystick: base em quadro + ponto central (mono brutalist)
    ui.frame(jx - joyR, jy - joyR, 2.0f * joyR, 2.0f * joyR, 2.0f * k,
             joyCol);
    ui.panel(jx - k, jy - k, 2.0f * k, 2.0f * k, joyCol);

    // knob quadrado: em repouso fica NO CENTRO (baseX_/baseY_ só existem
    // após o 1º toque — antes valiam 0 e o knob aparecia no canto);
    // ativo segue o dedo com clamp ao raio
    f32 kx = jx, ky = jy;
    if (tc.joystickActive()) {
        f32 dx = (tc.knobX() - tc.baseX()) * k;
        f32 dy = (tc.knobY() - tc.baseY()) * k;
        const f32 len = std::sqrt(dx * dx + dy * dy);
        if (len > joyR) {
            dx *= joyR / len;
            dy *= joyR / len;
        }
        kx += dx;
        ky += dy;
    }
    const f32 ks = 44.0f * k;
    const bool active = tc.joystickActive();
    ui.panel(kx - ks * 0.5f, ky - ks * 0.5f, ks, ks,
             active ? theme::ACCENT : theme::PANEL);
    ui.frame(kx - ks * 0.5f, ky - ks * 0.5f, ks, ks, 1.0f * k, joyCol);

    // botão JUMP: premido = invertido (tema mono)
    const bool held = tc.buttonHeld();
    const f32 bx2 = ox + l.btnX * k;
    const f32 by2 = oy + l.btnY * k;
    const f32 bw = l.btnW * k, bh = l.btnH * k;
    if (held) {
        ui.panel(bx2, by2, bw, bh, theme::TEXT);
    }
    ui.frame(bx2, by2, bw, bh, 2.0f * k, held ? theme::PANEL : theme::ACCENT);
    if (ui.hasFont()) {
        const f32 tw = ui.fontWidth("JUMP");
        const f32 thh = ui.fontHeight();
        ui.label(bx2 + (bw - tw) * 0.5f, by2 + bh * 0.5f + thh * 0.30f,
                 "JUMP", held ? theme::PANEL : theme::TEXT);
    }
}

void drawTouchControls(UiContext& ui, const TouchControls& tc, f32 sw, f32 sh) {
    // F4.2: layout recalculado para a ÁREA ÚTIL (superfície menos insets)
    // e deslocado pela origem do contentRect — nada desenhado atrás da
    // nav/status bar. 0.7.3: o layout vem dos CAMPOS EDITÁVEIS do
    // componente (pos/tamanho/cor — o joystick do Player é esta instância).
    // 0.7.4: o corpo vive em drawTouchControlsAt (partilhado com o editor).
    drawTouchControlsAt(ui, tc, ui.safeLeft(), ui.safeTop(),
                        sw - ui.safeLeft() - ui.safeRight(),
                        sh - ui.safeTop() - ui.safeBottom(), 1.0f);
}

// 0.7.0 — DESSELECCIONAR no viewport 3D: arm no press edge dentro do
// viewport central (não reclamado); limpa no release se o dedo NÃO se
// mexeu além do limiar (tap ≠ drag de orbit/gizmo). Puro e afervel.
bool viewportTapClearsSelection(EditorState& st, const InputState& in,
                                 const UiRect& view, u32 claimedMask) {
    // arm: press edge do slot 0 dentro do viewport, não reclamado (só se
    // ainda NÃO armado — um press edge persistente não re-arma com a pos
    // nova; o drag de orbit continua a ser drag)
    if (in.pressed(0) && !st.deselectArm && !(claimedMask & 1u)) {
        f32 px = 0.0f, py = 0.0f;
        in.pos(0, px, py);
        if (px >= view.x && px < view.x + view.w && py >= view.y &&
            py < view.y + view.h) {
            st.deselectArm = true;
            st.deselectX = px;
            st.deselectY = py;
        }
    }
    if (!st.deselectArm) {
        return false;
    }
    // release do slot 0: foi um tap parado dentro do viewport?
    if (!in.released(0)) {
        // dedo deslizou além do limiar → drag (orbit/gizmo), cancela o arm
        if (in.down(0)) {
            f32 px = 0.0f, py = 0.0f;
            in.pos(0, px, py);
            const f32 dx = px - st.deselectX;
            const f32 dy = py - st.deselectY;
            if (dx * dx + dy * dy > 14.0f * 14.0f) {
                st.deselectArm = false;
            }
        }
        return false;
    }
    st.deselectArm = false;
    f32 px = 0.0f, py = 0.0f;
    in.pos(0, px, py);
    const f32 dx = px - st.deselectX;
    const f32 dy = py - st.deselectY;
    if (dx * dx + dy * dy > 14.0f * 14.0f) {
        return false;   // arrastou — foi orbit/gizmo, não um tap
    }
    if (px < view.x || px >= view.x + view.w || py < view.y ||
        py >= view.y + view.h) {
        return false;
    }
    st.selected = Handle::invalid();
    st.selElement = -1;
    return true;
}

// ---- 0.8.12 — GUARDA DOS PICKERS + SOBREVIVÊNCIA DA SELEÇÃO ------------------

bool pickerGuardBlocked(const Scene& scene, Handle selected) {
    // alvo válido = TIC VIVO com MeshRenderer (preset Mesh, objetos
    // importados, bodies com mesh). Sem MeshRenderer (câmara/áudio/ui) ou
    // handle morto → bloqueado: o toque em linha de picker dá HINT, nunca
    // o caminho "ERRO(sem TIC com mesh selecionado)".
    const Tic* t = scene.get(selected);
    return t == nullptr || t->getComponent<MeshRenderer>() == nullptr;
}

Handle revalidateSelection(const Scene& scene, Handle selected,
                           const char* name) {
    // 1) handle vivo (load in-place / TIC intacto) → mantém-se
    if (scene.get(selected)) {
        return selected;
    }
    // 2) handle morto (o reload do INIT_WINDOW re-criou os TICs com
    // identidades novas) → RE-MAPEIA por NOME (o TIC continua a existir;
    // a seleção não se perde por um ciclo de lifecycle)
    if (name && name[0]) {
        return scene.find(name);
    }
    return Handle::invalid();
}

int drawPlusMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh, EditorState& st) {
    // 0.7.0: no modo UI o "+" cria ELEMENTOS (Panel/Label/Button/Image);
    // 0.7.3: + os COMPOSTOS (Menu/Card/Article) e o JOYSTICK (widget de
    // TouchControls editável). 0.7.4: + os CONTAINERS VBox/HBox (filhos
    // automáticos). No 3D cria TICs de preset (como sempre).
    // 0.8.0 (F7): o 6º preset do 3D é o TIC "Mesh" (Transform+MeshRenderer
    // com PRIMITIVA esfera default — prototipagem sem física).
    // 0.8.11: o 7º é o TIC "Audio" (Transform+AudioPlayer; o clip atribui-se
    // no Inspector/seletor — a ESTRUTURA primeiro, o som depois).
    const bool uiMode = st.uiMode;
    const int kItems = uiMode ? 10 : 7;   // 0.7.7: Camera; 0.8.0: Mesh; 0.8.11: Audio
    // PASSO 1 (0.9.6.14): as linhas do menu são alvos de 40dp (a LEI DE
    // OURO — eram 56 num passo de 64; nada ≥48 no editor)
    const f32 rowH = 40.0f;
    const f32 rowStep = 48.0f;
    const f32 w = kMenuW;
    const f32 h = kHeaderH + static_cast<f32>(kItems) * rowStep + kPad;
    // F4.2: centrado no viewport ÚTIL (dentro do contentRect)
    // 0.9.0: os overlays centram na FAIXA DO VIEWPORT (não por baixo do
    // chrome — ver overlayArea no EditorLayout.h)
    f32 ox, oy, aw, ah;
    overlayArea(sw, sh, ui.safeArea(), ox, oy, aw, ah);
    const f32 x = ox + (aw - w) * 0.5f;
    const f32 y = oy + (ah - h) * 0.5f;

    if (pressedOutside(in, x, y, w, h)) {
        st.plusMenu = false;
        return 0;
    }

    // 0.9.6.6 (GRUPO C · CANTOS SUAVIZADOS): o CARD modal com raios 8dp
    ui.panelRounded(x, y, w, h, theme::dp(theme::kRadiusCard), theme::PANEL);
    ui.frameRounded(x, y, w, h, 2.0f, theme::dp(theme::kRadiusCard),
                    theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.label(x + kPad, y + kHeaderH * 0.5f + th * 0.30f,
             uiMode ? "CRIAR ELEMENTO UI" : "CRIAR TIC", theme::TEXT);

    int chosen = 0;
    // 0.7.7: o 5º preset do 3D é o TIC de CÂMARA (Transform3D + CameraComp;
    // nasce A ativa — o main chama setOnlyActiveCamera)
    // 0.8.0 (F7): o 6º é o TIC "Mesh" (esfera procedural, SEM física)
    // 0.8.11: o 7º é o TIC "Audio" (Transform+AudioPlayer, SEM mesh)
    const char* names[7] = {"PlayerBody3D", "CharacterBody3D", "StaticBody3D",
                            "RigidBody3D", "Camera", "Mesh", "Audio"};
    // 0.7.4: + VBox/HBox (containers de layout — filhos automáticos)
    const char* elems[10] = {"Panel", "Label", "Button", "Image",
                             "Menu", "Card", "Article", "Joystick",
                             "VBox", "HBox"};
    const char* const* labels = uiMode ? elems : names;
    for (int i = 0; i < kItems; ++i) {
        if (ui.button(static_cast<u64>(20 + i), x + kPad,
                      y + kHeaderH + static_cast<f32>(i) * rowStep,
                      w - 2.0f * kPad, rowH, labels[i])) {
            chosen = i + 1;
            st.plusMenu = false;
        }
    }
    return chosen;
}

int drawFileMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh, EditorState& st,
                 f32 ax, f32 ay, bool snapOn) {
    // 0.9.6.10 (GRUPO UI · a região TOPO da imagem 1): o menu ≡ passa à
    // ESTRUTURA DE 6 SECÇÕES da barra de topo da referência — Projeto /
    // Cena / Editar / Visualizar / Ferramentas / Ajuda — com cabeçalhos
    // 12sp text2 + separadores (a MESMA organização da imagem 1, ao
    // alcance de um toque no telemóvel). As AÇÕES são as de sempre (mais
    // as do chrome do viewport promovidas a linhas: Desfazer/Refazer/
    // Duplicar/Colar — caminhos que JÁ existiam no main) + o Snap (o
    // toggle real do íman) e a Documentação V.ONI (o ecrã das Docs).
    // SHEET ANCORADO 8dp sob o botão [≡ Menu], largura 280dp, SCRIM 60%,
    // linhas 48dp ÍCONE+rótulo 14sp; COM SCROLL quando o conteúdo excede
    // o ecrã (14 linhas + 6 cabeçalhos não cabem no portrait — o sheet
    // encolhe ao disponível e a lista rola).
    constexpr int kItems = 14;
    constexpr f32 kSheetW = 280.0f;   // spec H
    constexpr f32 kRowH = 48.0f;      // spec H/A
    constexpr f32 kHdrH = 28.0f;      // cabeçalho de secção (GRUPO UI)
    // (ax/ay = rect do botão-âncora da top bar; −1/−1 = fallback centrado
    // p/ compatibilidade dos testes.)
    static const struct {
        const char*  label;
        icons::Icon  ic;
        int          section;   // 0..5 (o cabeçalho desenha ANTES da 1ª
                                // linha de cada secção)
    } kMenu[kItems] = {
        // PROJETO
        {"Sair para projetos",  icons::Icon::Back,    0},
        {"Importar…",           icons::Icon::Upload,  0},
        {"Export Downloads",    icons::Icon::Download, 0},
        // CENA
        {"Guardar cena",        icons::Icon::Save,    1},
        {"Carregar cena",       icons::Icon::Folder,  1},
        {"Export OBJ",          icons::Icon::Download, 1},
        // EDITAR
        {"Desfazer",            icons::Icon::Undo,    2},
        {"Refazer",             icons::Icon::Redo,    2},
        {"Duplicar",            icons::Icon::Duplicate, 2},
        {"Colar",               icons::Icon::Paste,   2},
        // VISUALIZAR
        {nullptr,               icons::Icon::Snap,    3},   // rótulo dinâmico
        // FERRAMENTAS
        {nullptr,               icons::Icon::Gear,    4},   // rótulo localizado
        {"Ver logs",            icons::Icon::Terminal, 4},
        // AJUDA
        {"Documentação V.ONI",  icons::Icon::Search,  5},
    };
    static const char* const kSections[6] = {
        "PROJETO", "CENA", "EDITAR", "VISUALIZAR", "FERRAMENTAS", "AJUDA"};
    // o rótulo dinâmico do Snap (linha 10 — o toggle REAL do íman)
    char snapLabel[48];
    std::snprintf(snapLabel, sizeof(snapLabel), "Snapping: %s",
                  snapOn ? "ligado" : "desligado");
    const char* labels[kItems];
    for (int i = 0; i < kItems; ++i) {
        labels[i] = kMenu[i].label ? kMenu[i].label : snapLabel;
    }
    // 0.9.6.18 (HOTFIX D4): a linha Settings vem da TABELA LOCALIZADA
    // (item 11 — «Definições» em PT, «Settings» só em locale EN; o literal
    // inglês hardcoded era o defeito)
    labels[11] = strings::tr(strings::Key::OpenSettings);

    const f32 contentH = static_cast<f32>(kItems) * kRowH +
                         6.0f * kHdrH + 8.0f;
    f32 x, y;
    // o ALTURA máxima disponível (o sheet NUNCA sai do contentRect — a
    // invariante F4.2; o resto rola lá dentro)
    const f32 maxH = sh - static_cast<f32>(ui.safeTop()) -
                     theme::dp(safe::kToolbarH) - theme::dp(8.0f) -
                     theme::dp(28.0f);
    const f32 h = contentH < maxH ? contentH : maxH;
    if (ax >= 0.0f && ay >= 0.0f) {
        x = ax;
        y = ay + 8.0f;   // 8dp sob o botão (spec H)
        // clamp ao contentRect (nada sai do ecrã — a invariante F4.2)
        const f32 maxX = ui.safeLeft() + sw - ui.safeRight() - kSheetW - 4.0f;
        if (x > maxX) {
            x = maxX > ui.safeLeft() ? maxX : ui.safeLeft();
        }
        const f32 maxY = sh - ui.safeBottom() - h - 28.0f;
        if (y > maxY) {
            y = maxY > static_cast<f32>(ui.safeTop()) + safe::kToolbarH
                    ? maxY
                    : static_cast<f32>(ui.safeTop()) + safe::kToolbarH;
        }
    } else {
        f32 ox, oy, aw, ah;
        overlayArea(sw, sh, ui.safeArea(), ox, oy, aw, ah);
        x = ox + (aw - kSheetW) * 0.5f;
        y = oy + (ah - h) * 0.5f;
    }

    // SCRIM 60% (spec H): toque fora fecha SEM ação
    ui.panel(ui.safeLeft(), static_cast<f32>(ui.safeTop()),
             sw - ui.safeLeft() - ui.safeRight(),
             sh - ui.safeTop() - ui.safeBottom(), theme::kTheme.scrim);
    if (pressedOutside(in, x, y, kSheetW, h)) {
        st.fileMenu = false;
        return 0;
    }

    ui.panelRounded(x, y, kSheetW, h, theme::kRadiusCard, theme::kTheme.surface);
    ui.frameRounded(x, y, kSheetW, h, 1.0f, theme::kRadiusCard,
                    theme::kTheme.border);

    const TextMetrics tm = ui.textMetrics();
    int chosen = 0;
    const UiRect region{x, y + 4.0f, kSheetW, h - 8.0f};
    ui.beginScroll(kMenuScrollId, region, contentH);
    const f32 off = ui.scrollOffset();
    // o y de CONTENT de cada linha (para o re-despacho do tap: dentro do
    // scroll o botão SÓ desenha — o tap nasce da região e é mapeado aqui,
    // o MESMO padrão dos cards dos Ficheiros no BottomPanel)
    f32 rowContentY[kItems];
    f32 ry = y + 4.0f;
    int lastSec = -1;
    for (int i = 0; i < kItems; ++i) {
        // o CABEÇALHO da secção (12sp text2 — a imagem 1)
        if (kMenu[i].section != lastSec) {
            lastSec = kMenu[i].section;
            if (ui.hasFont()) {
                const f32 hdrY = ry - off;
                if (hdrY + kHdrH >= y && hdrY <= y + h) {
                    ui.labelStyled(x + 16.0f,
                                   hdrY + (kHdrH - tm.block()) * 0.5f +
                                       tm.ascent,
                                   kSections[lastSec], theme::kTheme.text2,
                                   theme::fontScale(theme::kFontCaption), 0);
                }
            }
            ry += kHdrH;
        }
        rowContentY[i] = ry;   // topo da linha EM COORDENADAS DE CONTENT
        const f32 rowY = ry - off;
        ry += kRowH;
        if (rowY + kRowH < y || rowY > y + h) {
            continue;   // culled (fora do sheet)
        }
        const u64 id = kMenuRowBase + static_cast<u64>(i);
        const bool held = ui.widgetActive(id);
        if (held) {
            ui.panel(x + 4.0f, rowY, kSheetW - 8.0f, kRowH,
                     theme::kTheme.surface2);
        }
        icons::drawIcon(ui, kMenu[i].ic, x + 16.0f,
                        rowY + (kRowH - 24.0f) * 0.5f, 24.0f,
                        theme::kTheme.text2);
        if (ui.hasFont()) {
            ui.labelFitted(x + 16.0f + 24.0f + 12.0f,
                           rowY + (kRowH - tm.block()) * 0.5f + tm.ascent,
                           labels[i], theme::kTheme.text1,
                           kSheetW - 24.0f - 36.0f - 12.0f);
        }
    }
    ui.endScroll();
    // o RE-DESPACHO do tap (o padrão da casa dentro de scrolls): o tap
    // nasce da REGIÃO do sheet e mapeia para a linha pelo y de content
    f32 tx, ty;
    if (ui.scrollTap(kMenuScrollId, tx, ty)) {
        const f32 contentTapY = ty + off;
        for (int i = 0; i < kItems; ++i) {
            if (contentTapY >= rowContentY[i] &&
                contentTapY < rowContentY[i] + kRowH) {
                chosen = i + 1;
                st.fileMenu = false;
                break;
            }
        }
    }
    return chosen;
}

// ---------------------------------------------------------------------------
// 0.9.6.10 (GRUPO UI) · O MENU ⋮ DA HIERARQUIA (a imagem 1: cada painel
// com o seu menu) — um sheet pequeno ancorado sob o botão, com as ações
// REAIS da árvore: 1 = limpar a multi-seleção · 2 = o nome COMPLETO do
// TIC selecionado (o tip do long-press, sem esperar o long-press)
// ---------------------------------------------------------------------------
int drawHierMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                 EditorState& st, f32 ax, f32 ay) {
    constexpr f32 kSheetW = 260.0f;
    constexpr f32 kRowH = 48.0f;
    const f32 h = 2.0f * kRowH + 8.0f;
    f32 x = ax, y = ay + 8.0f;
    const f32 maxX = ui.safeLeft() + sw - ui.safeRight() - kSheetW - 4.0f;
    if (x > maxX) {
        x = maxX > ui.safeLeft() ? maxX : ui.safeLeft();
    }
    const f32 maxY = sh - ui.safeBottom() - h - 28.0f;
    if (y > maxY) {
        y = maxY > static_cast<f32>(ui.safeTop()) + safe::kToolbarH ? maxY
                : static_cast<f32>(ui.safeTop()) + safe::kToolbarH;
    }
    ui.panel(ui.safeLeft(), static_cast<f32>(ui.safeTop()),
             sw - ui.safeLeft() - ui.safeRight(),
             sh - ui.safeTop() - ui.safeBottom(), theme::kTheme.scrim);
    if (pressedOutside(in, x, y, kSheetW, h)) {
        st.hierMenu = false;
        return 0;
    }
    ui.panelRounded(x, y, kSheetW, h, theme::kRadiusCard,
                    theme::kTheme.surface);
    ui.frameRounded(x, y, kSheetW, h, 1.0f, theme::kRadiusCard,
                    theme::kTheme.border);
    static const struct {
        const char* label;
        icons::Icon ic;
    } kRows[2] = {
        {"Limpar seleção", icons::Icon::Check},
        {"Nome completo do TIC", icons::Icon::Question},
    };
    const TextMetrics tm = ui.textMetrics();
    int chosen = 0;
    for (int i = 0; i < 2; ++i) {
        const f32 ry = y + 4.0f + static_cast<f32>(i) * kRowH;
        const u64 id = kHierMenuRowBase + static_cast<u64>(i);
        if (ui.widgetActive(id)) {
            ui.panel(x + 4.0f, ry, kSheetW - 8.0f, kRowH,
                     theme::kTheme.surface2);
        }
        icons::drawIcon(ui, kRows[i].ic, x + 16.0f,
                        ry + (kRowH - 24.0f) * 0.5f, 24.0f,
                        theme::kTheme.text2);
        if (ui.hasFont()) {
            ui.labelFitted(x + 52.0f, ry + (kRowH - tm.block()) * 0.5f +
                                           tm.ascent,
                           kRows[i].label, theme::kTheme.text1,
                           kSheetW - 68.0f);
        }
        if (ui.widgetHit(id, x + 4.0f, ry, kSheetW - 8.0f, kRowH)) {
            chosen = i + 1;
            st.hierMenu = false;
        }
    }
    return chosen;
}

// ---------------------------------------------------------------------------
// 0.7.1 — CENAS: overlay com a lista do manifesto (+ nova/trocar)
// ---------------------------------------------------------------------------

const char* sceneDisplayName(const std::string& sceneRelPath, char* out,
                             size_t outCap) {
    // basename sem extensão: "scenes/main.goni" → "main"
    size_t start = sceneRelPath.rfind('/');
    start = start == std::string::npos ? 0 : start + 1;
    size_t end = sceneRelPath.rfind('.');
    if (end == std::string::npos || end < start) {
        end = sceneRelPath.size();
    }
    const size_t len = end > start ? end - start : 0;
    std::snprintf(out, outCap, "%.*s", static_cast<int>(len),
                  sceneRelPath.c_str() + start);
    return out;
}

int drawScenesMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                   EditorState& st, const std::vector<std::string>& scenes,
                   u32 activeScene, f32 ax, f32 ay) {
    // 0.9.0 (spec H) — SHEET CENAS ANCORADO 8dp sob o botão [Cena ▾]:
    // 280dp de largura, scrim 60%, [＋ Nova cena] com FILL ACCENT no topo,
    // linhas de cena 48dp com ÍCONE + nome + CHECK na ativa, separador fino.
    // Toque fora fecha SEM ação; toque na linha troca e fecha.
    constexpr f32 kSheetW = 280.0f;   // spec H
    const f32 rowH = 48.0f;
    const u32 shown = scenes.size() < 6u ? static_cast<u32>(scenes.size()) : 6u;
    const f32 listH = static_cast<f32>(shown) * rowH;
    const f32 h = 48.0f + 56.0f + 8.0f + listH + 8.0f;
    f32 x, y;
    if (ax >= 0.0f && ay >= 0.0f) {
        x = ax;
        y = ay + 8.0f;   // 8dp sob o botão (spec H)
        const f32 maxX = ui.safeLeft() + sw - ui.safeRight() - kSheetW - 4.0f;
        if (x > maxX) {
            x = maxX > ui.safeLeft() ? maxX : ui.safeLeft();
        }
        const f32 maxY = sh - ui.safeBottom() - h - 28.0f;
        if (y > maxY) {
            y = maxY > static_cast<f32>(ui.safeTop()) + safe::kToolbarH
                    ? maxY
                    : static_cast<f32>(ui.safeTop()) + safe::kToolbarH;
        }
    } else {
        f32 ox, oy, aw, ah;
        overlayArea(sw, sh, ui.safeArea(), ox, oy, aw, ah);
        x = ox + (aw - kSheetW) * 0.5f;
        y = oy + (ah - h) * 0.5f;
    }

    // scrim + toque fora fecha SEM ação
    ui.panel(ui.safeLeft(), static_cast<f32>(ui.safeTop()),
             sw - ui.safeLeft() - ui.safeRight(),
             sh - ui.safeTop() - ui.safeBottom(), theme::kTheme.scrim);
    if (pressedOutside(in, x, y, kSheetW, h)) {
        st.scenesMenu = false;
        return 0;
    }

    ui.panelRounded(x, y, kSheetW, h, theme::kRadiusCard, theme::kTheme.surface);
    ui.frameRounded(x, y, kSheetW, h, 1.0f, theme::kRadiusCard,
                    theme::kTheme.border);

    // ＋ Nova cena — FILL ACCENT (spec H), fixo no topo
    const f32 newBtnY = y + 4.0f;
    const UiRect newBtn = {x + 8.0f, newBtnY, kSheetW - 16.0f, 48.0f};
    const bool newHeld = ui.widgetActive(6700);
    ui.panelRounded(newBtn.x, newBtn.y, newBtn.w, newBtn.h,
                    theme::kRadiusCard,
                    newHeld ? theme::kTheme.accentPress : theme::kTheme.accent);
    icons::drawIcon(ui, icons::Icon::Plus, newBtn.x + 16.0f,
                    newBtn.y + (newBtn.h - 24.0f) * 0.5f, 24.0f,
                    theme::kTheme.accentInk);
    if (ui.hasFont()) {
        const TextMetrics tm0 = ui.textMetrics();
        ui.label(newBtn.x + 16.0f + 24.0f + 12.0f,
                 newBtn.y + (newBtn.h - tm0.block()) * 0.5f + tm0.ascent,
                 "Nova cena", theme::kTheme.accentInk);
    }
    const bool newScene = ui.widgetHit(6700, newBtn.x, newBtn.y, newBtn.w,
                                       newBtn.h);

    // separador fino + lista (scroll id 44)
    ui.panel(x + 16.0f, newBtnY + 48.0f + 4.0f, kSheetW - 32.0f, 1.0f,
             theme::kTheme.border);
    const f32 listTop = newBtnY + 56.0f + 8.0f;
    const UiRect region{x, listTop, kSheetW, listH};
    const f32 contentH = static_cast<f32>(scenes.size()) * rowH;
    ui.beginScroll(44, region, contentH);
    const f32 off = ui.scrollOffset();
    const TextMetrics tm = ui.textMetrics();
    for (size_t i = 0; i < scenes.size(); ++i) {
        char name[48];
        sceneDisplayName(scenes[i], name, sizeof(name));
        const f32 ry = listTop + static_cast<f32>(i) * rowH - off;
        const bool active = i == activeScene;
        if (active) {
            ui.panel(x + 4.0f, ry, kSheetW - 8.0f, rowH, theme::kTheme.surface2);
        }
        // ícone de cena (clapper) + nome + CHECK na ativa (spec H)
        icons::drawIcon(ui, icons::Icon::Clapper, x + 16.0f,
                        ry + (rowH - 24.0f) * 0.5f, 24.0f,
                        active ? theme::kTheme.accent : theme::kTheme.text2);
        if (ui.hasFont()) {
            ui.labelFitted(x + 16.0f + 24.0f + 12.0f,
                           ry + (rowH - tm.block()) * 0.5f + tm.ascent, name,
                           active ? theme::kTheme.accent : theme::kTheme.text1,
                           kSheetW - 24.0f - 36.0f - 12.0f - 32.0f);
        }
        if (active) {
            icons::drawIcon(ui, icons::Icon::Check,
                            x + kSheetW - 16.0f - 24.0f,
                            ry + (rowH - 24.0f) * 0.5f, 24.0f,
                            theme::kTheme.accent);
        }
        ui.widgetHit(6710 + static_cast<u64>(i), x + 4.0f, ry, kSheetW - 8.0f,
                     rowH);   // só desenha (scroll re-despacha)
    }
    ui.endScroll();
    if (scenes.empty() && ui.hasFont()) {
        icons::drawIcon(ui, icons::Icon::Clapper, x + kSheetW * 0.5f - 16.0f,
                        listTop + 20.0f, 32.0f, theme::kTheme.text2);
        ui.labelFitted(x + 16.0f, listTop + 76.0f, "(sem cenas)",
                       theme::kTheme.text2, kSheetW - 32.0f);
    }

    // tap re-despachado → escolha da linha (a MESMA geometria desenhada)
    int chosen = 0;
    f32 tx = 0.0f, ty = 0.0f;
    if (newScene) {
        chosen = 1;
    } else if (ui.scrollTap(44, tx, ty)) {
        const i32 row = static_cast<i32>((ty - listTop + off) / rowH);
        if (row >= 0 && static_cast<u32>(row) < scenes.size()) {
            chosen = row + 2;
        }
    }
    if (chosen != 0) {
        st.scenesMenu = false;
    }
    return chosen;
}

int drawSettingsMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                     EditorState& st, const char* storageMode,
                     bool keepSource, f32 audioMaster) {
    // F5.1-hotfix: menu do botão Settings — mono, mesmo padrão dos overlays.
    // F5.2: 3 itens + linha do modo de armazenamento ativo.
    // 0.8.10: +2 — "fonte: manter/largar" (o setting que larga source/ do
    // import) e "reconverter assets" (reconverte tudo de source/).
    // 0.8.11: +2 — "diagnostico audio (probe)" e "volume geral" (o master).
    constexpr int kItems = 7;
    constexpr f32 kModeLineH = 30.0f;
    // PASSO 1 (0.9.6.14): alvos de 40dp num passo de 48 (eram 56/64 — a
    // LEI DE OURO: nada ≥48 no editor)
    constexpr f32 kRowH = 40.0f;
    constexpr f32 kRowStep = 48.0f;
    const bool showMode = storageMode && storageMode[0];
    const f32 h = kHeaderH + (showMode ? kModeLineH : 0.0f) +
                  static_cast<f32>(kItems) * kRowStep + kPad;
    // 0.9.0: os overlays centram na FAIXA DO VIEWPORT (não por baixo do
    // chrome — ver overlayArea no EditorLayout.h)
    f32 ox, oy, aw, ah;
    overlayArea(sw, sh, ui.safeArea(), ox, oy, aw, ah);
    const f32 x = ox + (aw - kMenuW) * 0.5f;
    const f32 y = oy + (ah - h) * 0.5f;

    if (pressedOutside(in, x, y, kMenuW, h)) {
        st.settingsMenu = false;
        return 0;
    }

    ui.panel(x, y, kMenuW, h, theme::PANEL);
    ui.frame(x, y, kMenuW, h, 2.0f, theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.label(x + kPad, y + kHeaderH * 0.5f + th * 0.30f, "SETTINGS", theme::TEXT);

    // F5.2: modo de armazenamento ativo (o item 6 do escopo — o dono vê
    // sempre QUAL modo está em uso)
    f32 itemsTop = y + kHeaderH;
    if (showMode) {
        char line[48];
        std::snprintf(line, sizeof(line), "armazenamento: %s", storageMode);
        ui.labelFitted(x + kPad, y + kHeaderH + kModeLineH * 0.5f + th * 0.30f,
                       line, theme::LINE, kMenuW - 2.0f * kPad);
        itemsTop += kModeLineH;
    }

    int chosen = 0;
    char fonte[48];
    std::snprintf(fonte, sizeof(fonte), "fonte após import: %s",
                  keepSource ? "manter" : "largar");
    char vol[48];
    std::snprintf(vol, sizeof(vol), "volume geral: %d%%",
                  static_cast<int>(audioMaster * 100.0f + 0.5f));
    const char* labels[kItems] = {"Exportar logs", "Ver logs",
                                  "Acesso a ficheiros…", fonte,
                                  "reconverter assets",
                                  "diagnostico audio (probe)", vol};
    for (int i = 0; i < kItems; ++i) {
        if (ui.button(static_cast<u64>(4400 + i), x + kPad,
                      itemsTop + static_cast<f32>(i) * kRowStep,
                      kMenuW - 2.0f * kPad, kRowH, labels[i])) {
            chosen = i + 1;
            st.settingsMenu = false;
        }
    }
    return chosen;
}

// ---------------------------------------------------------------------------
// F5.2: DIÁLOGO All Files Access — "Precisa de acesso a todos os ficheiros
// para importar/exportar projetos" + Permitir/Cancelar (tema mono).
// A mensagem é quebrada em ATÉ 3 linhas que caibam no painel (medidas com
// a fonte real — nunca sai do rect).
// ---------------------------------------------------------------------------

namespace {

// quebra por palavras (greedy) em até maxLines linhas de até cap-1 chars;
// devolve o nº de linhas usadas (texto que não couber fica na última)
int wrapText3(UiContext& ui, const char* text, f32 maxW, int maxLines,
              char out[][96]) {
    for (int i = 0; i < maxLines; ++i) {
        out[i][0] = '\0';
    }
    if (!text || !ui.hasFont() || maxLines <= 0) {
        return 0;
    }
    int line = 0;
    const char* p = text;
    while (*p && line < maxLines) {
        const char* word = p;
        while (*p && *p != ' ') ++p;          // fim da palavra
        const size_t wlen = static_cast<size_t>(p - word);
        while (*p == ' ') ++p;                // espaços entre palavras

        char candidate[96];
        if (out[line][0]) {
            std::snprintf(candidate, sizeof(candidate), "%s %.*s", out[line],
                          static_cast<int>(wlen), word);
        } else {
            std::snprintf(candidate, sizeof(candidate), "%.*s",
                          static_cast<int>(wlen), word);
        }
        if (ui.fontWidth(candidate) <= maxW || !out[line][0]) {
            std::snprintf(out[line], 96, "%s", candidate);
        } else {
            ++line;                            // a palavra não cabe → nova linha
            if (line < maxLines) {
                std::snprintf(out[line], 96, "%.*s", static_cast<int>(wlen), word);
            }
        }
        // palavra MAIOR que a linha inteira: trunca (não há em texto fixo)
    }
    int used = 0;
    for (int i = 0; i < maxLines; ++i) {
        if (out[i][0]) used = i + 1;
    }
    return used;
}

} // namespace

int drawStorageDialog(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                      EditorState& st) {
    const f32 h = storageDialogHeight();
    // 0.9.0: os overlays centram na FAIXA DO VIEWPORT (não por baixo do
    // chrome — ver overlayArea no EditorLayout.h)
    f32 ox, oy, aw, ah;
    overlayArea(sw, sh, ui.safeArea(), ox, oy, aw, ah);
    const UiRect dlg = centeredMenuRect(ox, oy, aw, ah, h);

    if (pressedOutside(in, dlg.x, dlg.y, dlg.w, dlg.h)) {
        st.storageDialog = false;   // toque fora = cancelar (sem ação)
        return 0;
    }

    ui.panel(dlg.x, dlg.y, dlg.w, dlg.h, theme::PANEL);
    ui.frame(dlg.x, dlg.y, dlg.w, dlg.h, 2.0f, theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.label(dlg.x + kPad, dlg.y + kHeaderH * 0.5f + th * 0.30f,
             "ARMAZENAMENTO", theme::TEXT);

    // mensagem EXATA do escopo, quebrada para caber
    char rows[3][96];
    wrapText3(ui,
              "Precisa de acesso a todos os ficheiros para importar/exportar "
              "projetos",
              kMenuW - 2.0f * kPad, 3, rows);
    for (int i = 0; i < 3; ++i) {
        if (rows[i][0]) {
            ui.labelFitted(dlg.x + kPad,
                           dlg.y + kHeaderH + static_cast<f32>(i) * 34.0f + 20.0f,
                           rows[i], theme::TEXT, dlg.w - 2.0f * kPad);
        }
    }

    // botões lado a lado (faixa de ids exclusiva 6300+)
    UiRect allow{}, cancel{};
    storageDialogButtons(dlg, allow, cancel);
    int chosen = 0;
    if (ui.button(6301, allow.x, allow.y, allow.w, allow.h, "Permitir")) {
        chosen = 1;
        st.storageDialog = false;
    }
    if (ui.button(6302, cancel.x, cancel.y, cancel.w, cancel.h, "Cancelar")) {
        chosen = 2;
        st.storageDialog = false;
    }
    return chosen;
}

// ---------------------------------------------------------------------------
// F5.2: overlay IMPORT — ficheiros suportados de Download/Documents (File
// API direta; o main copia o escolhido para o projeto). Cap 8 (mono).
// ---------------------------------------------------------------------------

int drawImportMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                   EditorState& st,
                   const std::vector<fileapi::Candidate>& cands) {
    // cap de linhas no overlay (sem scroll — F8)
    constexpr size_t kMaxRows = 8;
    const size_t shown = cands.size() < kMaxRows ? cands.size() : kMaxRows;

    const f32 h = importMenuHeight(static_cast<u32>(shown) + 1u);
    // 0.9.0: os overlays centram na FAIXA DO VIEWPORT (não por baixo do
    // chrome — ver overlayArea no EditorLayout.h)
    f32 ox, oy, aw, ah;
    overlayArea(sw, sh, ui.safeArea(), ox, oy, aw, ah);
    const f32 x = ox + (aw - kMenuW) * 0.5f;
    const f32 y = oy + (ah - h) * 0.5f;

    if (pressedOutside(in, x, y, kMenuW, h)) {
        st.importMenu = false;
        return 0;
    }

    ui.panel(x, y, kMenuW, h, theme::PANEL);
    ui.frame(x, y, kMenuW, h, 2.0f, theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.label(x + kPad, y + kHeaderH * 0.5f + th * 0.30f, "IMPORTAR",
             theme::TEXT);

    int chosen = 0;
    if (cands.empty()) {
        ui.labelFitted(x + kPad, y + kHeaderH + 30.0f,
                       "(nenhum obj/gltf/glb/png em Download/Documents)",
                       theme::LINE, kMenuW - 2.0f * kPad);
        return 0;
    }
    for (size_t i = 0; i < shown; ++i) {
        // rótulo: nome + tipo (m/t → mesh/textura)
        char label[80];
        std::snprintf(label, sizeof(label), "%s  [%s]", cands[i].name.c_str(),
                      cands[i].kind == 'm' ? "mesh" : "tex");
        const UiRect row = importRowRect({x, y, kMenuW, h}, static_cast<u32>(i));
        if (ui.button(6100 + static_cast<u64>(i), row.x, row.y, row.w, row.h,
                      label)) {
            chosen = static_cast<int>(i) + 1;
            st.importMenu = false;
        }
    }
    if (cands.size() > kMaxRows) {
        char more[48];
        std::snprintf(more, sizeof(more), "+%u ficheiros (cap do overlay)",
                      static_cast<unsigned>(cands.size() - kMaxRows));
        ui.labelFitted(x + kPad,
                       importRowRect({x, y, kMenuW, h},
                                     static_cast<u32>(shown)).y + 20.0f,
                       more, theme::LINE, kMenuW - 2.0f * kPad);
    }
    return chosen;
}

// ---------------------------------------------------------------------------
// F5.2: VIEWER de logs — engine.log (tail) + crash dumps com scroll (id 43),
// tema mono. Funcional SEM export: é a mesma leitura POSIX que o export faz.
// ---------------------------------------------------------------------------

void drawLogViewer(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                   EditorState& st, const std::vector<std::string>& lines,
                   const std::vector<std::string>& dumps) {
    // 0.9.0: os overlays centram na FAIXA DO VIEWPORT (não por baixo do
    // chrome — ver overlayArea no EditorLayout.h)
    f32 ox, oy, aw, ah;
    overlayArea(sw, sh, ui.safeArea(), ox, oy, aw, ah);
    // painel GRANDE central (86% × 80% da área útil — o log precisa de espaço)
    const f32 w = aw * 0.86f;
    const f32 h = ah * 0.80f;
    const f32 x = ox + (aw - w) * 0.5f;
    const f32 y = oy + (ah - h) * 0.5f;

    if (pressedOutside(in, x, y, w, h)) {
        st.logViewer = false;
        return;
    }

    // 0.9.6.6 (GRUPO C · CANTOS SUAVIZADOS): o CARD modal com raios 8dp
    ui.panelRounded(x, y, w, h, theme::dp(theme::kRadiusCard), theme::PANEL);
    ui.frameRounded(x, y, w, h, 2.0f, theme::dp(theme::kRadiusCard),
                    theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.label(x + kPad, y + kHeaderH * 0.5f + th * 0.30f,
             "LOGS (engine.log + crashes)", theme::TEXT);
    if (ui.button(6401, x + w - kPad - 96.0f, y + 4.0f, 96.0f, 36.0f, "fechar")) {
        st.logViewer = false;
        return;
    }
    ui.panel(x + kPad, y + kHeaderH - 1.0f, w - 2.0f * kPad, 1.0f, theme::LINE);

    // conteúdo: altura REAL = linhas (block da fonte) + secção de dumps
    const TextMetrics tm = ui.textMetrics();
    const f32 rowH = tm.block() + 6.0f;
    const f32 dumpsH = dumps.empty() ? 0.0f : (34.0f + static_cast<f32>(dumps.size()) * rowH);
    const f32 contentH = 34.0f + static_cast<f32>(lines.size()) * rowH + dumpsH;

    const f32 listTop = y + kHeaderH;
    const UiRect region{x, listTop, w, h - kHeaderH};
    ui.beginScroll(kLogsScrollId, region, contentH);
    const f32 off = ui.scrollOffset();

    auto baselineOf = [&](f32 rowTop) {
        return rowTop + rowH * 0.5f + tm.ascent - tm.block() * 0.5f;
    };

    f32 cy = listTop - off;
    ui.labelFitted(x + kPad, baselineOf(cy + 4.0f),
                   lines.empty() ? "(log vazio)" : "engine.log:",
                   theme::LINE, w - 2.0f * kPad);
    cy += 34.0f;
    for (const std::string& l : lines) {
        // logs são longos — labelFitted corta na largura do painel
        ui.labelFitted(x + kPad, baselineOf(cy), l.c_str(), theme::TEXT,
                       w - 2.0f * kPad);
        cy += rowH;
    }
    if (!dumps.empty()) {
        ui.labelFitted(x + kPad, baselineOf(cy + 4.0f), "crash dumps:",
                       theme::ACCENT, w - 2.0f * kPad);
        cy += 34.0f;
        for (const std::string& d : dumps) {
            // 0.8.12 — badge ANTIGO: dump de OUTRA build (vc do nome !=
            // instalado) ou pré-0.8.10 → "  [ANTIGO (build N)]". O wiring
            // existia desde a 0.8.10 (buildinfo::dumpIsFromOtherBuild) mas
            // NUNCA era chamado aqui — o dono via o dump VELHO sem rótulo
            // (crash-1790830406.dump, offsets idênticos) e não sabia.
            const std::string badge = vv::buildinfo::dumpBadge(d);
            ui.labelFitted(x + kPad, baselineOf(cy),
                           badge.empty() ? d.c_str()
                                         : (d + badge).c_str(),
                           badge.empty() ? theme::TEXT : theme::WARN,
                           w - 2.0f * kPad);
            cy += rowH;
        }
    }
    ui.endScroll();

    // F5.2: 1º frame após abrir → salta para o FIM (o recente é o que importa)
    if (st.logViewerJustOpened) {
        ui.scrollSetOffset(kLogsScrollId, contentH);   // beginScroll clampa
        st.logViewerJustOpened = false;
    }
}

// ---------------------------------------------------------------------------
// F5-E: SELETOR DE ASSETS — overlay mono com "none/cube" + ficheiros de
// meshes/ ou textures/ (cap 5 ficheiros; sem scroll no overlay — F8).
// Devolve 1-based (1 = none/cube, 2.. = ficheiros), 0 = nada este frame.
//
// 0.8.12 — none DE PRIMEIRA CLASSE no picker de MESH: 1 = none (limpa o
// slot — o TIC deixa de renderizar mesh), 2 = cube (procedural), 3.. =
// ficheiros. O de TEXTURA já tinha none em 1º; o de PRIMITIVAS idem.
//
// 0.8.10 — assetMenu == 4 é o SELETOR DE PRIMITIVAS (SÓ CUBO E ESFERA —
// decisão do dono; cilindro e as outras seis saíram). "none" + 2 botões:
// esfera, box. Devolve: 0 nada; 1 = none (desliga o prim); 2 = Sphere;
// 3 = Box. Não usa o catálogo.
// ---------------------------------------------------------------------------
int drawAssetMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                  EditorState& st, const AssetCatalog& catalog,
                  bool withImport) {
    const bool pickMesh = (st.assetMenu == 1);
    const bool pickPrim = (st.assetMenu == 4);   // 0.8.0: primitivas
    const bool pickAudio = (st.assetMenu == 5);  // 0.8.11: clips .gi
    // 0.8.11 — o seletor de CLIPS sempre oferece "importar…" (importar
    // áudio é o fluxo principal, não excecional)
    if (pickAudio) {
        withImport = true;
    }
    const std::vector<std::string>& files =
        pickMesh ? catalog.meshes
                 : (pickAudio ? catalog.audio : catalog.textures);

    if (pickPrim) {
        // ---- SELETOR DE PRIMITIVAS (2 formas + none) ----------------------
        const f32 w = kMenuW;
        const f32 h = kHeaderH + theme::dp(44.0f) + 2.0f * theme::dp(44.0f) + kPad;
        const f32 ox = ui.safeLeft();
        const f32 oy = ui.safeTop();
        const f32 aw = sw - ox - ui.safeRight();
        const f32 ah = sh - oy - ui.safeBottom();
        const f32 x = ox + (aw - w) * 0.5f;
        const f32 y = oy + (ah - h) * 0.5f;
        if (pressedOutside(in, x, y, w, h)) {
            st.assetMenu = 0;
            return 0;
        }
        ui.panel(x, y, w, h, theme::PANEL);
        ui.frame(x, y, w, h, 2.0f, theme::ACCENT);
        const f32 th = ui.fontHeight();
        ui.label(x + kPad, y + kHeaderH * 0.5f + th * 0.30f,
                 "PRIMITIVA", theme::TEXT);
        int chosen = 0;
        if (ui.button(kIdAssetBase, x + kPad, y + kHeaderH,
                      w - 2.0f * kPad, theme::dp(36.0f), "none (desligar)")) {
            chosen = 1;
            st.assetMenu = 0;
        }
        // 0.8.10: lista vertical com as DUAS formas que restam
        for (int i = 0; i < 2; ++i) {
            const f32 by = y + kHeaderH + theme::dp(44.0f) +
                           static_cast<f32>(i) * theme::dp(44.0f);
            if (ui.button(kIdAssetBase + 1 + static_cast<u64>(i), x + kPad,
                          by, w - 2.0f * kPad, theme::dp(36.0f),
                          primLabel(static_cast<PrimKind>(i)))) {
                chosen = i + 2;   // 2=Sphere, 3=Box
                st.assetMenu = 0;
            }
        }
        return chosen;
    }

    // 0.9.6 (G4 · R-014) — A LISTA DE FICHEIROS TEM SCROLL (o cap de 5
    // sem scroll escondia os imports novos: com 5+ .gmesh no projeto, o
    // glb/gltf recém-importado NUNCA aparecia no seletor — o TODO "F8
    // traz scroll" nunca chegou). TODOS os ficheiros listam; a janela
    // visível encaixa na faixa do overlay (máx. 8 linhas) e o resto
    // faz scroll — o MESMO padrão da Hierarchy/Inspector (o tap volta
    // pelo scrollTap e as linhas continuam alvos de 48dp).
    // 0.9.6.1 (PASSO 0): linha do seletor em dp REAL (R-018)
    // PASSO 1: a LINHA da spec (36 — kRowH; eram 48)
    const f32 kAssetRowH = theme::dp(36.0f);
    constexpr u64  kAssetScrollId = 50;   // slot de scroll próprio (≠ hier/insp/settings)
    const f32 w = kMenuW;
    // 0.7.4: withImport (seletor de textura de ELEMENTO de UI) acrescenta a
    // linha "importar…" que abre o NAVEGADOR 0.7.2 (escolhe de onde for)
    const f32 importH = withImport ? theme::dp(40.0f) : 0.0f;   // PASSO 1: alvo 40
    // 0.8.12 — picker de MESH: +1 linha (none + cube + ficheiros)
    f32 ox, oy, aw, ah;
    overlayArea(sw, sh, ui.safeArea(), ox, oy, aw, ah);
    const f32 fixedH = kHeaderH +
                       static_cast<f32>(pickMesh ? 2 : 1) * kAssetRowH +
                       importH + kPad;
    const f32 maxListH = ah - fixedH - theme::dp(8.0f);
    const f32 fullListH = static_cast<f32>(files.size()) * kAssetRowH;
    const f32 listH = fullListH < maxListH ? fullListH : maxListH;
    const f32 h = fixedH + listH;
    const f32 x = ox + (aw - w) * 0.5f;
    const f32 y = oy + (ah - h) * 0.5f;

    if (pressedOutside(in, x, y, w, h)) {
        st.assetMenu = 0;
        return 0;
    }

    // 0.9.6.6 (GRUPO C · CANTOS SUAVIZADOS): o CARD modal com raios 8dp
    ui.panelRounded(x, y, w, h, theme::dp(theme::kRadiusCard), theme::PANEL);
    ui.frameRounded(x, y, w, h, 2.0f, theme::dp(theme::kRadiusCard),
                    theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.label(x + kPad, y + kHeaderH * 0.5f + th * 0.30f,
             pickMesh ? "MESH" : (pickAudio ? "CLIP DE AUDIO" : "TEXTURA"),
             theme::TEXT);

    int chosen = 0;
    // 0.8.12 — picker de MESH: "none" EM PRIMEIRO LUGAR (limpa o slot — o
    // TIC deixa de renderizar mesh; o caminho seguro de deferred free é o
    // MESMO das trocas). Cube passa a 2º; ficheiros 3+.
    if (pickMesh) {
        if (ui.button(kIdAssetBase, x + kPad, y + kHeaderH,
                      w - 2.0f * kPad, theme::dp(40.0f), "none")) {
            chosen = 1;
            st.assetMenu = 0;
        }
        if (ui.button(kIdAssetBase + 8, x + kPad,
                      y + kHeaderH + kAssetRowH, w - 2.0f * kPad,
                      theme::dp(40.0f), "cube (procedural)")) {
            chosen = 2;
            st.assetMenu = 0;
        }
    } else {
        // item 0: none (textura) / none (clip de áudio)
        if (ui.button(kIdAssetBase, x + kPad, y + kHeaderH, w - 2.0f * kPad,
                      theme::dp(40.0f), "none")) {
            chosen = 1;
            st.assetMenu = 0;
        }
    }
    // ---- a LISTA DE FICHEIROS em scroll (R-014: TODOS visíveis) -----------
    const f32 listTop = y + kHeaderH +
                        static_cast<f32>(pickMesh ? 2 : 1) * kAssetRowH;
    ui.beginScroll(kAssetScrollId, UiRect{x, listTop, w, listH}, fullListH);
    const f32 off = ui.scrollOffset();
    for (size_t i = 0; i < files.size(); ++i) {
        const f32 ry = listTop + static_cast<f32>(i) * kAssetRowH - off;
        // culling: fora da janela visível não desenha
        if (ry + kAssetRowH < listTop - 1.0f || ry > listTop + listH + 1.0f) {
            continue;
        }
        // 0.8.11 — clip de áudio mostra o NOME limpo (sem pasta/extensão;
        // o catálogo guarda caminhos completos "audio/x.gi")
        char disp[96];
        const char* label = files[i].c_str();
        if (pickAudio) {
            const size_t slash = files[i].rfind('/');
            const size_t dot = files[i].rfind('.');
            std::snprintf(disp, sizeof(disp), "%s",
                          files[i].substr(slash + 1,
                                          dot == std::string::npos
                                              ? std::string::npos
                                              : dot - slash - 1).c_str());
            label = disp;
        }
        const u64 rowId =
            kIdAssetBase + 1 + static_cast<u64>(i) + (pickMesh ? 1 : 0);
        // dentro do scroll o botão é SÓ VISUAL (o tap volta pelo scrollTap
        // — o padrão da Hierarchy/Inspector: drag em qualquer sítio =
        // scroll, tap parado = escolha)
        ui.auditRowFloorNext(layout::kRowFloorDp);   // PASSO 1: a linha 36
        ui.button(rowId, x + kPad, ry, w - 2.0f * kPad, kAssetRowH, label);
    }
    ui.endScroll();
    // o TAP parado na lista (o scroll devolve a posição — o mesmo padrão
    // do Inspector): mapeia y → linha → escolha
    {
        f32 tx = 0.0f, ty = 0.0f;
        if (ui.scrollTap(kAssetScrollId, tx, ty)) {
            const f32 rel = ty - listTop + off;
            if (rel >= 0.0f) {
                const size_t idx = static_cast<size_t>(rel / kAssetRowH);
                if (idx < files.size()) {
                    // 0.8.12 — picker de MESH: ficheiros começam em 3
                    // (none=1, cube=2); tex/áudio em 2
                    chosen = static_cast<int>(idx) + (pickMesh ? 3 : 2);
                    st.assetMenu = 0;
                }
            }
        }
    }
    if (withImport) {
        // 0.7.4 — "importar…": abre o NAVEGADOR de ficheiros (0.7.2) — o
        // dono escolhe a textura de onde for (galeria incluída); o ficheiro
        // importado cai em textures/ e fica disponível no seletor
        if (ui.button(kIdAssetBase + 7, x + kPad, listTop + listH,
                      w - 2.0f * kPad, theme::dp(40.0f), "importar...")) {
            chosen = kAssetPickImport;
            st.assetMenu = 0;
        }
    }
    return chosen;
}

// ---------------------------------------------------------------------------
// F6: DISPATCH da escolha do seletor — applyAssetPick (o wiring que faltava).
// Era o bloco do main.cpp que nunca corria: drawAssetMenu fecha o seletor no
// clique (st.assetMenu = 0) e o dispatch lia g_editor.assetMenu DEPOIS →
// código morto desde a F5-E. A mesma lógica, agora PURA e afervel no CI.
// ---------------------------------------------------------------------------
AssetPickOutcome applyAssetPick(Scene& scene, Handle selected, int menuKind, int pick,
                                const AssetCatalog& catalog, const AssetResolvers& res) {
    AssetPickOutcome out;
    if (pick <= 0) {
        return out;   // nada escolhido neste frame
    }
    Tic* tic = scene.get(selected);
    if (!tic) {
        return out;   // TIC morto — sem crash, sem ação
    }

    // ---- 0.8.11: seletor de CLIPS DE ÁUDIO (menuKind 5) ---------------------
    // O AudioPlayer é DADOS puros: o pick escreve clipPath e SAI — o
    // carregamento/decode acontece no primeiro play (audioClipFor do main,
    // cache 1×); sem GL, sem engine, zero efeitos colaterais.
    if (menuKind == 5) {
        AudioPlayer* au = tic->getComponent<AudioPlayer>();
        if (!au) {
            return out;   // sem AudioPlayer no TIC — toast do chamador
        }
        if (pick == 1) {   // none → sem clip (a voz morre no próximo play)
            au->clipPath.clear();
            au->voiceId = -1;
            au->previewing = false;
            out.applied = true;
            std::snprintf(out.toast, sizeof(out.toast), "clip: none");
            std::snprintf(out.log, sizeof(out.log),
                          "audio: clip removido do TIC '%s'", tic->name.c_str());
            return out;
        }
        const size_t idx = static_cast<size_t>(pick - 2);
        if (idx >= catalog.audio.size()) {
            return out;   // fora do catálogo — sem crash
        }
        au->clipPath = catalog.audio[idx];
        au->voiceId = -1;        // voz antiga NÃO aponta o clip novo
        au->previewing = false;
        out.applied = true;
        std::snprintf(out.toast, sizeof(out.toast), "clip: %s",
                      catalog.audio[idx].c_str());
        std::snprintf(out.log, sizeof(out.log),
                      "audio: clip '%s' aplicado ao TIC '%s'",
                      catalog.audio[idx].c_str(), tic->name.c_str());
        return out;
    }

    MeshRenderer* mr = tic->getComponent<MeshRenderer>();
    if (!mr) {
        return out;   // TIC sem MeshRenderer — sem crash, sem ação
    }

    if (menuKind == 4) {
        // ---- 0.8.10: seletor de PRIMITIVAS (SÓ cubo e esfera) --------------
        // TROCA DETERMINÍSTICA: o pick ARMA o pedido (primOn + params +
        // primPending) e NÃO mexe no mesh — o ANTIGO continua a renderizar
        // até o bind no PONTO SEGURO do frame seguinte (início, antes da
        // submissão), pelo caminho ÚNICO do main (gera→valida→upload→
        // self-check→bind com deferred free). Isto é o applyAssetPick PURO:
        // zero GL, zero cache — a falha (se houver) é reportada pelo main
        // com passo+razão no log e toast no ecrã, mantendo o mesh anterior.
        if (pick == 1) {   // none → desliga o prim (mesh sai no ponto seguro)
            // 0.8.12: posse p/ cova na flush (se nossa) — NUNCA perde um
            // pendente anterior (none seguido de none no MESMO frame: o
            // mesh já saiu; o pendente de antes é a posse QUE CONTINUA válida)
            mr->primRetire = mr->mesh ? mr->mesh : mr->primRetire;
            mr->primOn = false;
            mr->mesh = nullptr;
            mr->material = nullptr;
            mr->primPending = false;
            mr->primNeg = false;
            out.applied = true;
            std::snprintf(out.toast, sizeof(out.toast), "prim: none");
            std::snprintf(out.log, sizeof(out.log),
                          "editor: primitiva desligada");
        } else if (pick >= 2 && pick <= 3) {
            const PrimKind kind = static_cast<PrimKind>(pick - 2);
            const PrimParams p = primDefaults(kind);
            if (mr->primOn) {
                mr->primPrev = mr->prim;   // 0.8.10: label "de" do log
            }
            mr->primOn = true;
            mr->prim = p;
            mr->primPending = true;      // PEDIDO — sobe no ponto seguro
            mr->primNeg = false;         // pedido novo = nova tentativa
            mr->meshPath.clear();        // uma fonte de mesh de cada vez
            out.applied = true;
            std::snprintf(out.toast, sizeof(out.toast), "prim: %s",
                          primName(kind));
            std::snprintf(out.log, sizeof(out.log),
                          "editor: primitiva %s pedida (upload no ponto seguro "
                          "do frame)", primName(kind));
        }
        return out;
    }

    if (menuKind == 1) {
        // ---- seletor de MESHES (0.8.12: none=1, cube=2, ficheiros 3+) -------
        if (pick == 1) {
            // 0.8.12 — none DE PRIMEIRA CLASSE: limpa o SLOT de mesh (o TIC
            // deixa de renderizar mesh) pelo MESMO caminho seguro das trocas
            // — a posse antiga vai para primRetire (deferred free: a cova
            // abre no início do frame SEGUINTE, no ponto seguro do main;
            // meshes de PRIM são enterrados, cube/assets de outrem ficam
            // intocados — primRetire só enterra o que é NOSSO). Fail-safe:
            // nada de GL aqui (applyAssetPick é puro), sem crash.
            mr->primRetire = mr->mesh ? mr->mesh : mr->primRetire;
            mr->mesh = nullptr;
            mr->material = nullptr;
            mr->meshPath.clear();
            mr->primOn = false;
            mr->primPending = false;
            mr->primNeg = false;
            out.applied = true;
            std::snprintf(out.toast, sizeof(out.toast), "mesh: none");
            std::snprintf(out.log, sizeof(out.log),
                          "editor: mesh none — slot limpo (TIC sem mesh; "
                          "deferred free no próximo frame)");
        } else if (pick == 2) {   // cube procedural
            // 0.8.12: posse antiga p/ cova (sem perder pendente anterior)
            mr->primRetire = mr->mesh ? mr->mesh : mr->primRetire;
            mr->mesh = res.cubeMesh;
            mr->material = res.material;
            mr->meshPath.clear();
            mr->primOn = false;   // 0.8.0: cube LIMPA o prim (fonte única)
            mr->primPending = false;
            out.applied = true;
            std::snprintf(out.toast, sizeof(out.toast), "mesh: cube");
            std::snprintf(out.log, sizeof(out.log), "editor: mesh cube aplicado");
        } else {
            const size_t idx = static_cast<size_t>(pick - 3);
            if (idx >= catalog.meshes.size()) {
                return out;   // fora do catálogo — sem crash
            }
            // 0.8.10: as entradas são CAMINHOS COMPLETOS (assets/x.gmesh
            // ou meshes/x.obj legado) — usam-se DIRETAMENTE
            const std::string rel = catalog.meshes[idx];
            // 0.8.9 — NORMALIZAÇÃO UNIFORME: só na 1ª aplicação DESTE ref a
            // ESTE TIC (re-escolher o mesmo mesh não mexe na escala que o
            // dono já afinou — muito menos re-escala um mesh que ele acabou
            // de posicionar).
            const bool firstApply = mr->meshPath != rel;
            if (Mesh* m = res.mesh ? res.mesh(rel) : nullptr) {
                // 0.8.12: posse antiga p/ cova (sem perder pendente anterior)
                mr->primRetire = mr->mesh ? mr->mesh : mr->primRetire;
                mr->mesh = m;
                mr->material = res.material;
                mr->meshPath = rel;
                mr->primOn = false;   // 0.8.0: asset LIMPA o prim (fonte única)
                mr->primPending = false;
                // 0.8.9 (fix 3 do prompt): FATOR ÚNICO s = alvo / maiorEixo,
                // aplicado aos 3 EIXOS — PROPORÇÕES PRESERVADAS (nunca o
                // escalamento eixo-a-eixo que espalmava o modelo). A escala
                // vive no Transform3D (a geometria fica intacta: o botão
                // "escala original" do Inspector repõe {1,1,1}). O AABB chega
                // como DADOS pelo resolver meshExtent (o applyAssetPick é
                // PURO — nunca desreferencia o Mesh, contrato dos stubs).
                if (firstApply && res.meshExtent) {
                    Transform3D* tr = tic->getComponent<Transform3D>();
                    if (!tr) {
                        tr = tic->addComponent<Transform3D>();
                    }
                    const Vec3 ext = res.meshExtent(rel);
                    const f32 maior = ext.x > ext.y ? (ext.x > ext.z ? ext.x : ext.z)
                                                   : (ext.y > ext.z ? ext.y : ext.z);
                    f32 s = 1.0f;
                    if (maior > 1e-6f && std::isfinite(maior)) {
                        s = kImportTargetSize / maior;
                        if (!std::isfinite(s) || s <= 0.0f) {
                            s = 1.0f;   // defesa: geometria absurda = sem fit
                        }
                    }
                    tr->scale = Vec3{s, s, s};   // UM fator, TRÊS eixos
                    tr->updateWorld();
                }
                // F5.1-B: textura embutida do glTF/GLB aplica-se logo
                // (import sem PC — o material fica referenciado)
                bool withTex = false;
                if (res.meshTextureFor) {
                    const std::string texRel = res.meshTextureFor(rel);
                    if (!texRel.empty()) {
                        std::string warn;
                        if (const Texture* tex = res.texture ? res.texture(texRel, &warn)
                                                             : nullptr) {
                            mr->texture = tex;
                            mr->texPath = texRel;
                            withTex = true;
                        }
                    }
                }
                out.applied = true;
                std::snprintf(out.toast, sizeof(out.toast), "%s",
                              withTex ? "mesh aplicado (+textura)" : "mesh aplicado");
                if (firstApply && res.meshExtent) {
                    // 0.8.9 — a linha exigida: dims originais + fator único
                    const Vec3 ext = res.meshExtent(rel);
                    const f32 maior = ext.x > ext.y ? (ext.x > ext.z ? ext.x : ext.z)
                                                   : (ext.y > ext.z ? ext.y : ext.z);
                    const f32 s = maior > 1e-6f ? kImportTargetSize / maior : 1.0f;
                    std::snprintf(out.log, sizeof(out.log),
                                  "import: dims=%.3g,%.3g,%.3g uniform "
                                  "scale=%.4g (%s)",
                                  ext.x, ext.y, ext.z, s, rel.c_str());
                } else if (withTex) {
                    std::snprintf(out.log, sizeof(out.log),
                                  "editor: mesh %s aplicado com textura %s",
                                  rel.c_str(), mr->texPath.c_str());
                } else {
                    std::snprintf(out.log, sizeof(out.log),
                                  "editor: mesh %s aplicado", rel.c_str());
                }
            } else {
                // carga falhou — o estado ANTERIOR fica intacto
                std::snprintf(out.toast, sizeof(out.toast), "falha ao carregar mesh");
                std::snprintf(out.log, sizeof(out.log),
                              "editor: mesh %s FALHOU ao carregar", rel.c_str());
            }
        }
    } else if (menuKind == 2) {
        // ---- seletor de TEXTURAS -----------------------------------------
        if (pick == 1) {   // none → liberta a referência
            mr->texture = nullptr;
            mr->texPath.clear();
            out.applied = true;
            std::snprintf(out.toast, sizeof(out.toast), "tex: none");
            std::snprintf(out.log, sizeof(out.log), "material: textura removida");
        } else {
            const size_t idx = static_cast<size_t>(pick - 2);
            if (idx >= catalog.textures.size()) {
                return out;   // fora do catálogo — sem crash
            }
            const std::string rel = catalog.textures[idx];   // caminho completo
            std::string warn;
            if (const Texture* tex = res.texture ? res.texture(rel, &warn) : nullptr) {
                mr->texture = tex;
                mr->texPath = rel;
                out.applied = true;
                std::snprintf(out.toast, sizeof(out.toast), "%s",
                              warn.empty() ? "textura aplicada" : warn.c_str());
                std::snprintf(out.log, sizeof(out.log),
                              "material: textura aplicada %s", rel.c_str());
            } else {
                // carga falhou — o estado ANTERIOR fica intacto
                std::snprintf(out.toast, sizeof(out.toast), "falha ao carregar textura");
                std::snprintf(out.log, sizeof(out.log),
                              "material: textura %s FALHOU ao carregar", rel.c_str());
            }
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// 0.7.4 — DISPATCH do seletor de TEXTURA DE ELEMENTO de UI (menuKind 3).
// Escreve SÓ a ref (e.image); a renderização resolve por frame (imgResolve
// do UiContext — o caminho do Image 0.7.0, agora partilhado). Sem GPU, puro.
// ---------------------------------------------------------------------------
UiTexPickOutcome applyUiTexPick(Scene& scene, Handle tic, i32 element, int pick,
                                const AssetCatalog& catalog) {
    UiTexPickOutcome out;
    if (pick <= 0 || pick == kAssetPickImport) {
        return out;   // nada/importar — o chamador trata o navegador
    }
    Tic* t = scene.get(tic);
    UiCanvas* canvas = t ? t->getComponent<UiCanvas>() : nullptr;
    if (!canvas || element < 0 ||
        element >= static_cast<i32>(canvas->elements.size())) {
        return out;   // TIC morto / sem canvas / elemento morto — sem crash
    }
    UiElement& e = canvas->elements[static_cast<size_t>(element)];
    if (pick == 1) {   // none → limpa a ref (volta ao fill/placeholder)
        e.image.clear();
        out.applied = true;
        std::snprintf(out.toast, sizeof(out.toast), "tex: none");
        std::snprintf(out.log, sizeof(out.log),
                      "ui: textura do elemento '%s' removida", e.name.c_str());
        return out;
    }
    const size_t idx = static_cast<size_t>(pick - 2);
    if (idx >= catalog.textures.size()) {
        return out;   // fora do catálogo — sem crash
    }
    const std::string rel = catalog.textures[idx];   // caminho completo
    e.image = rel;
    out.applied = true;
    std::snprintf(out.toast, sizeof(out.toast), "tex: %s",
                  catalog.textures[idx].c_str());
    std::snprintf(out.log, sizeof(out.log),
                  "ui: textura do elemento '%s' = %s", e.name.c_str(),
                  rel.c_str());
    return out;
}


// ---------------------------------------------------------------------------
// 0.6.8 — PLAY MODE com janela própria
// ---------------------------------------------------------------------------

void closeAllOverlays(EditorState& st) {
    st.plusMenu = false;
    st.fileMenu = false;
    st.hierMenu = false;   // 0.9.6.10 (GRUPO UI): o ⋮ da Hierarquia
    st.settingsMenu = false;
    st.assetMenu = 0;
    st.storageDialog = false;
    st.importMenu = false;
    st.logViewer = false;
    st.logViewerJustOpened = false;
    // 0.7.0/0.7.1 — os overlays da gestão de TICs/UI/cenas também fecham
    // (a SELEÇÃO e o offset dos scrolls são estado de painel e ficam)
    st.contextMenu = false;
    st.removeDialog = false;
    st.textInput = false;
    st.elDrag = false;
    st.scenesMenu = false;
    st.fileBrowser = false;   // 0.7.2: o browser também fecha ao entrar em play
    st.applyAsk = false;
}

// Orbit da câmara — extraído do main.cpp (era globais + função estática).
// A lógica é INTACTA (regra F3 do dono-do-gesto, pinch, clamps da Camera);
// o que muda: o estado vive num OrbitState puro e o PLAY desliga o orbit.
void updateCameraOrbit(Camera& cam, OrbitState& st, const InputState& in,
                       const UiRect& view, u32 claimedMask, bool playMode) {
    // 0.6.8: em PLAY o orbit está DESATIVADO — 1 dedo = controlos de toque.
    // O gesto pendente é RESETADO (um drag que começou no editor e o Play
    // entretanto não pode continuar a orbitar) e o estado fica limpo para
    // o regresso ao editor.
    if (playMode) {
        st.active = false;
        st.gestureInView = false;
        st.pinchPrev = 0.0f;
        return;
    }

    u32 active = 0;
    for (u32 s = 0; s < kMaxPointerSlots; ++s) {
        if (in.down(s) && !(claimedMask & (1u << s))) {
            ++active;
        }
    }
    if (active == 0) {
        st.gestureInView = false;
        st.active = false;
        st.pinchPrev = 0.0f;
        return;
    }

    // F3: o PRIMEIRO toque decide o dono do gesto. Se nasce num painel
    // (Hierarchy/Inspector/toolbar) ou num controlo de toque (F4), a câmara
    // não orbita — mesmo que o dedo depois atravessasse o viewport.
    if (!st.gestureInView) {
        for (u32 s = 0; s < kMaxPointerSlots; ++s) {
            if (!in.pressed(s)) continue;
            if (claimedMask & (1u << s)) break;   // nasceu num controlo
            f32 x, y;
            in.pos(s, x, y);
            if (x >= view.x && x < view.x + view.w && y >= view.y && y < view.y + view.h) {
                st.gestureInView = true;
            }
            break;   // só o primeiro pointer com edge interessa
        }
    }
    if (!st.gestureInView) {
        return;
    }

    constexpr f32 kSens = 0.0075f;   // rad/px (~0,43° por pixel) — como no main
    if (active >= 2) {
        // pinch: distância entre os dois primeiros dedos ativos (não reclamados)
        f32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        bool got0 = false, got1 = false;
        for (u32 s = 0; s < kMaxPointerSlots && !(got0 && got1); ++s) {
            if (!in.down(s)) continue;
            if (claimedMask & (1u << s)) continue;
            f32 x, y;
            in.pos(s, x, y);
            if (!got0) { x0 = x; y0 = y; got0 = true; }
            else       { x1 = x; y1 = y; got1 = true; }
        }
        if (got0 && got1) {
            const f32 d = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
            if (st.pinchPrev > 0.0f && d > 1.0f) {
                cam.zoomBy(st.pinchPrev / d);   // dedos afastam → aproxima
            }
            st.pinchPrev = d;
        }
        st.active = false;
        return;
    }
    st.pinchPrev = 0.0f;
    if (active == 1) {
        for (u32 s = 0; s < kMaxPointerSlots; ++s) {
            if (!in.down(s)) continue;
            if (claimedMask & (1u << s)) continue;
            f32 x, y;
            in.pos(s, x, y);
            if (st.active) {
                cam.orbit((x - st.x) * kSens, (y - st.y) * kSens);
            }
            st.x = x;
            st.y = y;
            st.active = true;
            break;
        }
    } else {
        st.active = false;
    }
}

bool drawPlayBar(UiContext& ui, const InputState& in, f32 sw, f32 sh, int fps) {
    // F4.2: barra na faixa da toolbar, DENTRO da safe-area (nada atrás da
    // nav/status bar). Tema mono — mesma linguagem da toolbar.
    const UiRect r = playBarRect(sw, sh, ui.safeArea());
    ui.panel(r.x, r.y, r.w, r.h, theme::PANEL);
    ui.panel(r.x, r.y + r.h - 1.0f, r.w, 1.0f, theme::LINE);

    // Stop (id 5 — faixa exclusiva; ver EditorLayout.h)
    const UiRect stop = playStopButtonRect(r);
    const bool stopClicked = ui.button(kPlayStopId, stop.x, stop.y, stop.w,
                                       stop.h, "Stop");

    if (ui.hasFont()) {
        const f32 th = ui.fontHeight();
        const f32 cy = r.y + r.h * 0.5f + th * 0.30f;
        // estado "a correr" + fps ao lado do Stop
        char run[64];
        std::snprintf(run, sizeof(run), "a correr · fps %d", fps);
        ui.label(stop.x + stop.w + 24.0f, cy, run, theme::TEXT);
        // aviso à direita: nunca sai da barra (labelFitted)
        const f32 warnW = r.w * 0.5f;
        ui.labelFitted(r.x + r.w - warnW - kPad, cy,
                       "simulação — alterações descartadas ao parar",
                       theme::LINE, warnW);
    }
    return stopClicked;
}


} // namespace editor
} // namespace vv

// (0.6.9 → 0.7.6) drawGizmoToolbar/drawModeToggle foram REMOVIDAS: o
// seletor de gizmos e o separador 3D|UI são os grupos G4/G3 da BARRA FINAL
// (ui/Toolbar.h — ícones vetoriais + segmented controls).
