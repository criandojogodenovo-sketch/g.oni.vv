// ui/ViewportChrome.cpp — o chrome do viewport (FASE 9 → PASSO 3 → 0.9.6.19b).
//
// PASSO 3 (0.9.6.17 — a spec do dono): o rail ESQUERDO de ferramentas, a
// fila do topo-esquerdo (undo/redo/save/⋯), o gizmo 40dp no topo-direito e
// a legenda — TUDO a 60% de alfa, nada full-width, nada sobreposto. A strip
// [Cena][Perspetiva][Global] (a barra que atravessava a largura) MORREU —
// os chips eram SEM FUNÇÃO desde o inventário do PASSO 0.
// 0.9.6.19b (D21 · A DECISÃO DO DONO): o [+] REDONDO do fundo-direito MORREU
// — era redundante (o + da hierarquia e o item «Novo objeto» do menu ⋯ fazem
// o mesmo) e INTERCEPTAVA toques de orbit/seleção no canto. Os alvos de 40dp
// do chrome passam a N−1 (10).
//
// A GEOMETRIA (a fonte única é layout(), abaixo):
//   • rail 1 coluna (ecrãs altos): a fila do topo vive À DIREITA do rail;
//   • rail 2+ colunas (viewports baixos — a degradação do stack antigo):
//     a fila do topo desce PARA BAIXO do rail (encostada à esquerda);
//   • viewport sub-mínimo: o rail ESCONDE (a fila do topo e os cantos
//     ficam — cabem sempre no piso kViewportMinH da casa);
//   • a legenda só existe no layout primário (rail 1 coluna) e quando a
//     largura dá — a degradação honesta da legenda de sempre.
#include "ui/ViewportChrome.h"
#include "ui/EditorUi.h"
#include "ui/UiContext.h"
#include "render/Camera.h"

#include <cstdio>

namespace vv {
namespace editor {
namespace vpchrome {

namespace {

// PASSO 3: a ALFA do chrome vive no HEADER (chromeCol — afervel pela
// sentinela). Os glifos DESATIVADOS ficam nos 0.4 de sempre (já
// translúcidos por desenho — multiplicá-los de novo os tornava invisíveis).
static void colA(const f32* c, f32 out[4]) {
    chromeCol(c, out);
}

// botão do RAIL (PASSO 1 → PASSO 3): alvo 40×40, DESENHO em chip 32×32
// centrado; ícone 20; disabled = text2 40% (o seu alfa de sempre)
bool railButton(UiContext& ui, u64 id, const UiRect& r, icons::Icon icon,
                bool enabled) {
    const bool pressed = ui.widgetHit(id, r.x, r.y, r.w, r.h);
    const bool held = ui.widgetActive(id);
    const f32 inset = theme::dp(4.0f);   // o desenho 32 dentro do alvo 40
    const UiRect d = {r.x + inset, r.y + inset, r.w - 2.0f * inset,
                      r.h - 2.0f * inset};
    f32 surface[4], surface2[4], border[4], iconCol[4];
    colA(theme::kTheme.surface, surface);
    colA(theme::kTheme.surface2, surface2);
    colA(theme::kTheme.border, border);
    if (held && enabled) {
        ui.panelRounded(d.x, d.y, d.w, d.h, theme::dp(theme::kRadiusCard),
                        surface2);
    } else if (enabled) {
        // repouso: chip surface com bordo (o alvo é visível — nunca "quase
        // invisível", o problema documentado da 0.8.x)
        ui.panelRounded(d.x, d.y, d.w, d.h, theme::dp(theme::kRadiusCard),
                        surface);
        ui.frameRounded(d.x, d.y, d.w, d.h, 1.0f,
                        theme::dp(theme::kRadiusCard), border);
    }
    if (!enabled) {
        iconCol[0] = theme::kTheme.text2[0];
        iconCol[1] = theme::kTheme.text2[1];
        iconCol[2] = theme::kTheme.text2[2];
        iconCol[3] = 0.4f;
    } else {
        colA(theme::kTheme.text1, iconCol);
    }
    const f32 s = theme::dp(20.0f);   // PASSO 1: ícone 20 no chip 32
    icons::drawIcon(ui, icon, r.x + (r.w - s) * 0.5f, r.y + (r.h - s) * 0.5f,
                    s, iconCol);
    return pressed && enabled;
}

// botão de ferramenta (0.9.6.1 · G1-2 → PASSO 3): SÓ ÍCONE — o nome vive
// na LEGENDA (à direita do rail). Alvo 40, desenho 32, ícone 20.
bool toolButton(UiContext& ui, u64 id, const UiRect& r, icons::Icon icon,
                bool active) {
    const bool pressed = ui.widgetHit(id, r.x, r.y, r.w, r.h);
    const bool held = ui.widgetActive(id);
    const bool on = active || held;
    const f32 inset = theme::dp(4.0f);
    const UiRect d = {r.x + inset, r.y + inset, r.w - 2.0f * inset,
                      r.h - 2.0f * inset};
    f32 accent[4], surface[4], border[4], onCol[4], offCol[4];
    colA(theme::kTheme.accent, accent);
    colA(theme::kTheme.surface, surface);
    colA(theme::kTheme.border, border);
    colA(theme::kTheme.accentInk, onCol);
    colA(theme::kTheme.text1, offCol);
    if (on) {
        ui.panelRounded(d.x, d.y, d.w, d.h, theme::dp(theme::kRadiusCard),
                        accent);
    } else {
        ui.panelRounded(d.x, d.y, d.w, d.h, theme::dp(theme::kRadiusCard),
                        surface);
        ui.frameRounded(d.x, d.y, d.w, d.h, 1.0f,
                        theme::dp(theme::kRadiusCard), border);
    }
    const f32 s = theme::dp(20.0f);   // PASSO 1: ícone 20
    icons::drawIcon(ui, icon, r.x + (r.w - s) * 0.5f,
                    r.y + (r.h - s) * 0.5f, s, on ? onCol : offCol);
    return pressed;
}

// o chip de vidro do PASSO 3 (a receita do pai spec G — fill + bordo +
// highlight no topo — TUDO a 60%: o pai é translúcido, a cena lê-se)
void glassPanel(UiContext& ui, const UiRect& r, f32 radiusDp) {
    if (r.w <= 0.0f || r.h <= 0.0f) {
        return;
    }
    f32 fill[4], edge[4], top[4];
    colA(theme::kTheme.surface2, fill);
    colA(theme::kTheme.glassEdge, edge);
    colA(theme::kTheme.glassTop, top);
    ui.panelRounded(r.x, r.y, r.w, r.h, theme::dp(radiusDp), fill);
    ui.frameRounded(r.x, r.y, r.w, r.h, 1.0f, theme::dp(radiusDp), edge);
    // o HIGHLIGHT do topo do vidro (a spec G: #FFFFFF0A)
    ui.panelRounded(r.x + theme::dp(2.0f), r.y + theme::dp(1.0f),
                    r.w - theme::dp(4.0f), theme::dp(2.0f), theme::dp(1.0f),
                    top);
}

} // namespace

Layout layout(const UiRect& view) {
    Layout L;
    L.view = view;
    // 0.9.6.1 (PASSO 0 · R-018): todos os alvos daqui são dp REAL
    const f32 m = theme::dp(8.0f);
    const f32 btn = theme::dp(kRailBtn);
    const f32 gap = theme::dp(kRailGap);
    const f32 qbtn = theme::dp(kQuickBtn);
    const f32 qgap = theme::dp(kQuickGap);

    // (1) o RAIL esquerdo (col-major; a degradação em colunas é a do stack
    // antigo: o MENOR nº de colunas que caiba na altura útil)
    const f32 availH = view.h - 2.0f * m;
    const f32 availW = view.w - 2.0f * m;
    u32 cols = 1;
    bool fits = false;
    for (; cols < 5; ++cols) {
        const u32 rows = (5u + cols - 1u) / cols;   // ceil(5/cols)
        const f32 needH = static_cast<f32>(rows) * btn +
                          static_cast<f32>(rows - 1u) * gap;
        const f32 needW = static_cast<f32>(cols) * btn +
                          static_cast<f32>(cols - 1u) * gap;
        if (needH <= availH && needW <= availW) {
            fits = true;
            break;
        }
    }
    L.railVisible = fits;
    L.railCols = fits ? cols : 1u;
    const u32 rowsPerCol = (5u + L.railCols - 1u) / L.railCols;
    for (u32 i = 0; i < 5; ++i) {
        if (!L.railVisible) {
            L.rail[i] = {0.0f, 0.0f, 0.0f, 0.0f};
            continue;
        }
        const u32 col = i / rowsPerCol;
        const u32 row = i % rowsPerCol;
        L.rail[i] = {view.x + m + static_cast<f32>(col) * (btn + gap),
                     view.y + m + static_cast<f32>(row) * (btn + gap),
                     btn, btn};
    }
    const f32 railW = static_cast<f32>(L.railCols) * btn +
                      static_cast<f32>(L.railCols - 1u) * gap;
    const f32 railH = static_cast<f32>(rowsPerCol) * btn +
                      static_cast<f32>(rowsPerCol - 1u) * gap;
    if (L.railVisible) {
        L.railPanel = {L.rail[0].x - theme::dp(4.0f),
                       L.rail[0].y - theme::dp(4.0f),
                       railW + theme::dp(8.0f), railH + theme::dp(8.0f)};
    } else {
        L.railPanel = {0.0f, 0.0f, 0.0f, 0.0f};
    }

    // (2) o GIZMO (topo-direito) — 40dp, a spec PASSO 3. Fica mesmo com o
    // rail escondido. 0.9.6.19b (D21): o [+] do fundo-direito FOI REMOVIDO
    // (o canto fica LIMPO — a orbit/drag nessa zona sem interceptação).
    L.gizmoBtn = {view.x + view.w - m - btn, view.y + m, btn, btn};
    L.gizmoPanel = {L.gizmoBtn.x - theme::dp(4.0f),
                    L.gizmoBtn.y - theme::dp(4.0f), btn + theme::dp(8.0f),
                    btn + theme::dp(8.0f)};

    // (3) a fila do TOPO (undo/redo/save/⋯): a POSIÇÃO decide-se por
    // tentativa — (a) o TOPO, ao lado do rail e antes do gizmo (o layout
    // primário; vale mesmo com o rail em 2 colunas — o device com o
    // drawer aberto tem 88dp de sobra); (b) senão PARA BAIXO do rail (o
    // rect estreito); (c) senão ESCONDE (a degradação honesta — a fila
    // NUNCA sai do rect nem pisa o gizmo; a regra vale para o piso
    // kViewportMinW=288dp da casa)
    const f32 quickW = 4.0f * qbtn + 3.0f * qgap;
    const f32 qxTop = view.x + m +
                      (L.railVisible ? railW + theme::dp(12.0f) : 0.0f);
    const bool topFits =
        (qxTop + quickW <= L.gizmoPanel.x - theme::dp(4.0f)) &&
        (qxTop + quickW + m <= view.x + view.w);
    const f32 qyBelow = view.y + m + railH + m;
    const bool belowFits =
        L.railVisible &&
        (qyBelow + qbtn + m <= view.y + view.h) &&
        (view.x + m + quickW + m <= view.x + view.w);
    bool quickBelow = false;
    f32 qy = view.y + m;
    f32 qx = qxTop;
    if (topFits) {
        qy = view.y + m;
    } else if (belowFits) {
        quickBelow = true;
        qy = qyBelow;
        qx = view.x + m;
    }
    const bool quickFits = topFits || belowFits;
    if (!quickFits) {
        for (int i = 0; i < 4; ++i) {
            L.quick[i] = {0.0f, 0.0f, 0.0f, 0.0f};
        }
        L.quickPanel = {0.0f, 0.0f, 0.0f, 0.0f};
    } else {
        for (int i = 0; i < 4; ++i) {
            L.quick[i] = {qx + static_cast<f32>(i) * (qbtn + qgap), qy, qbtn,
                          qbtn};
        }
        L.quickPanel = {qx - theme::dp(4.0f), qy - theme::dp(4.0f),
                        quickW + theme::dp(8.0f), qbtn + theme::dp(8.0f)};
    }

    // (4) a LEGENDA — à direita do rail, SOB a fila do topo (o layout
    // primário); some quando a largura/altura não dá (a degradação de
    // sempre — e no layout "quick below" não há onde a pôr com dignidade)
    const f32 legendX = view.x + m +
                        (L.railVisible ? railW + theme::dp(4.0f) : 0.0f);
    const f32 legendY = view.y + m + qbtn + theme::dp(2.0f);
    const f32 legendMaxW = L.gizmoPanel.x - theme::dp(4.0f) - legendX;
    L.legendVisible = L.railVisible && L.railCols == 1u && quickFits &&
                      !quickBelow && legendMaxW >= theme::dp(72.0f) &&
                      (legendY + theme::dp(16.0f) + m) <= view.y + view.h;
    L.legend = L.legendVisible
                   ? UiRect{legendX, legendY, legendMaxW, theme::dp(16.0f)}
                   : UiRect{0.0f, 0.0f, 0.0f, 0.0f};
    return L;
}

Actions draw(UiContext& ui, EditorState& st, toolbar::GizmoModeState& gz,
             const ChromeState& cs, const Camera& camera, f32 drawerH) {
    (void)camera;
    Actions a;
    // G1-1: o rect da viewport com o drawerH REAL — o [+] acompanha o
    // painel de baixo (aberto = sobe; fechado = desce ao fundo)
    // GRUPO D: larguras de ESTADO (divisores) — o chrome acompanha os
    // painéis. PASSO 3: o flag do TRILHO entra AQUI (o chrome ancorava ao
    // rect do inspector ABERTO mesmo sem seleção — o gizmo/scissor do
    // main usavam o rect REAL; as duas medidas voltam a coincidir)
    const UiRect vpView = safe::centerRect(
        ui.screenWidth(), ui.screenHeight(), ui.safeArea(), drawerH,
        st.showInspector, st.hierW, st.inspW, editor::inspectorCollapsed(st));
    const Layout L = layout(vpView);

    // ---- OS PAIS DE VIDRO (a regra do pai-painelinho — nada flutua sem
    // pai) — a 60% de alfa (PASSO 3) ----
    glassPanel(ui, L.railPanel, theme::kRadiusCard);
    glassPanel(ui, L.quickPanel, theme::kRadiusCard);
    glassPanel(ui, L.gizmoPanel, kGizmoBtnDp);
    // 0.9.6.19b (D21): o pai de vidro do [+] (plusPanel) FOI REMOVIDO

    // ---- o RAIL esquerdo: as ferramentas (a ordem da spec do dono) ----
    if (L.railVisible) {
        // Selecionar = SEM gizmo; gz.mode para o gizmo da 0.6.9; o
        // st.selectMode é o cursor de seleção por toque
        if (toolButton(ui, kVpSelectId, L.rail[0], icons::Icon::Cursor,
                       st.selectMode)) {
            st.selectMode = true;
            gz.mode = 0;
        }
        if (toolButton(ui, toolbar::kGizmoIds[0], L.rail[1], icons::Icon::Move,
                       !st.selectMode && gz.mode == 0)) {
            st.selectMode = false;
            gz.mode = 0;
        }
        if (toolButton(ui, toolbar::kGizmoIds[1], L.rail[2],
                       icons::Icon::Rotate,
                       !st.selectMode && gz.mode == 1)) {
            st.selectMode = false;
            gz.mode = 1;
        }
        if (toolButton(ui, toolbar::kGizmoIds[2], L.rail[3],
                       icons::Icon::Scale,
                       !st.selectMode && gz.mode == 2)) {
            st.selectMode = false;
            gz.mode = 2;
        }
        // o ÍMAN: estado ativo/inativo, SEM texto (o valor segue no
        // tooltip do gizmo — G2-9)
        {
            const bool pressed = ui.widgetHit(kVpSnapValId, L.rail[4].x,
                                              L.rail[4].y, L.rail[4].w,
                                              L.rail[4].h);
            const bool held = ui.widgetActive(kVpSnapValId);
            const bool on = gz.snap || held;
            const f32 inset = theme::dp(4.0f);
            const UiRect d = {L.rail[4].x + inset, L.rail[4].y + inset,
                              L.rail[4].w - 2.0f * inset,
                              L.rail[4].h - 2.0f * inset};
            f32 accent[4], surface[4], border[4], onCol[4], offCol[4];
            colA(theme::kTheme.accent, accent);
            colA(theme::kTheme.surface, surface);
            colA(theme::kTheme.border, border);
            colA(theme::kTheme.accentInk, onCol);
            colA(theme::kTheme.text1, offCol);
            ui.panelRounded(d.x, d.y, d.w, d.h, theme::dp(theme::kRadiusCard),
                            on ? accent : surface);
            if (!on) {
                ui.frameRounded(d.x, d.y, d.w, d.h, 1.0f,
                                theme::dp(theme::kRadiusCard), border);
            }
            icons::drawIcon(ui, icons::Icon::Snap,
                            L.rail[4].x + (L.rail[4].w - theme::dp(20.0f)) *
                                              0.5f,
                            L.rail[4].y + (L.rail[4].h - theme::dp(20.0f)) *
                                              0.5f,
                            theme::dp(20.0f), on ? onCol : offCol);
            if (pressed) {
                gz.snap = !gz.snap;
            }
        }
        // a LEGENDA: o nome da ferramenta ATIVA (12sp, à direita do rail,
        // por baixo da fila do topo — nunca dentro do botão)
        if (ui.hasFont() && L.legendVisible) {
            const char* name = st.selectMode ? "Selecionar"
                               : gz.mode == 0 ? "Mover"
                               : gz.mode == 1 ? "Rodar"
                                              : "Escalar";
            f32 txt[4];
            colA(theme::kTheme.text2, txt);
            const TextMetrics tm = ui.textMetrics();
            ui.labelStyled(L.legend.x,
                           L.legend.y +
                               (L.legend.h - tm.block()) * 0.5f + tm.ascent,
                           name, txt,
                           theme::fontScale(theme::kFontCaption), 0);
        }
    }

    // ---- a fila do TOPO: desfazer/refazer/guardar/⋯ ----
    if (L.quick[0].w > 0.0f) {
        if (railButton(ui, kVpUndoId, L.quick[0], icons::Icon::Undo,
                       cs.canUndo)) {
            a.undoPressed = true;
        }
        if (railButton(ui, kVpRedoId, L.quick[1], icons::Icon::Redo,
                       cs.canRedo)) {
            a.redoPressed = true;
        }
        if (railButton(ui, kVpSaveId, L.quick[2], icons::Icon::Save, true)) {
            a.savePressed = true;
        }
        // o ⋯: abre o menu de ficheiro ANCORADO a ele (a folha de sempre;
        // o glifo é o ⋮ da casa — o mesmo do menu da hierarquia)
        if (railButton(ui, kVpMenuId, L.quick[3], icons::Icon::Dots, true)) {
            a.menuPressed = true;
            st.menuAx = L.quick[3].x;
            st.menuAy = L.quick[3].y + L.quick[3].h;
        }
    }

    // ---- o GIZMO 40dp (topo-direito): o atalho mostrar/esconder o gizmo
    // (a transição selectMode↔gizmo que JÁ existe — zero lógica nova; a
    // interpretação do «gizmo 40dp» da spec, documentada no relatório) ----
    {
        const bool live = !st.selectMode;   // o gizmo vive fora do cursor
        const icons::Icon ic = live ? (gz.mode == 0   ? icons::Icon::Move
                                       : gz.mode == 1 ? icons::Icon::Rotate
                                                      : icons::Icon::Scale)
                                    : icons::Icon::Cursor;
        const bool pressed =
            ui.widgetHit(kVpGizmoId, L.gizmoBtn.x, L.gizmoBtn.y,
                         L.gizmoBtn.w, L.gizmoBtn.h);
        const bool held = ui.widgetActive(kVpGizmoId);
        const f32 inset = theme::dp(4.0f);
        const UiRect d = {L.gizmoBtn.x + inset, L.gizmoBtn.y + inset,
                          L.gizmoBtn.w - 2.0f * inset,
                          L.gizmoBtn.h - 2.0f * inset};
        f32 accent[4], surface[4], border[4], onCol[4], offCol[4];
        colA(theme::kTheme.accent, accent);
        colA(theme::kTheme.surface, surface);
        colA(theme::kTheme.border, border);
        colA(theme::kTheme.accentInk, onCol);
        colA(theme::kTheme.text2, offCol);
        if (live || held) {
            ui.panelRounded(d.x, d.y, d.w, d.h, theme::dp(theme::kRadiusCard),
                            accent);
        } else {
            ui.panelRounded(d.x, d.y, d.w, d.h, theme::dp(theme::kRadiusCard),
                            surface);
            ui.frameRounded(d.x, d.y, d.w, d.h, 1.0f,
                            theme::dp(theme::kRadiusCard), border);
        }
        icons::drawIcon(ui, ic,
                        L.gizmoBtn.x + (L.gizmoBtn.w - theme::dp(20.0f)) * 0.5f,
                        L.gizmoBtn.y + (L.gizmoBtn.h - theme::dp(20.0f)) * 0.5f,
                        theme::dp(20.0f), live ? onCol : offCol);
        if (pressed) {
            st.selectMode = !st.selectMode;   // a transição existente
        }
    }

    // 0.9.6.19b (D21): o bloco do [+] 40dp REDONDO (fundo-direito) FOI
    // REMOVIDO — a decisão do dono (a ação vive no + da hierarquia e no
    // menu ⋯ «Novo objeto»; o canto inferior direito do viewport fica
    // LIMPO para a orbit/seleção). O pin: nenhum widget com o id 38
    // (kVpAddTicIdRetired) desenha dentro do rect do viewport.

    return a;
}

} // namespace vpchrome
} // namespace editor
} // namespace vv
