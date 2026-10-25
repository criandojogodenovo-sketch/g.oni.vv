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
#include <cmath>

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
                bool active, f32 insetDp = 4.0f) {
    const bool pressed = ui.widgetHit(id, r.x, r.y, r.w, r.h);
    const bool held = ui.widgetActive(id);
    const bool on = active || held;
    const f32 inset = theme::dp(insetDp);
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
    // 0.12-Y (3.Y-A): 6 ferramentas (o PAN entra — a MÃOZINHA)
    constexpr u32 kRailTools = 4u;   // FASE UI D: as 4 ferramentas
    const f32 availH = view.h - 2.0f * m;
    const f32 availW = view.w - 2.0f * m;
    u32 cols = 1;
    bool fits = false;
    for (; cols < 6; ++cols) {
        const u32 rows = (kRailTools + cols - 1u) / cols;   // ceil(6/cols)
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
    const u32 rowsPerCol = (kRailTools + L.railCols - 1u) / L.railCols;
    for (u32 i = 0; i < kRailTools; ++i) {
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
    // FASE UI D: os CHIPS (mão + íman) SOB o rail — os alvos 40dp de
    // sempre (o desenho é menor: o inset 6 no toolButton); a coluna inteira
    // cabe? os chips morrem na degradação (o rail tem prioridade)
    if (L.railVisible) {
        const f32 cy = L.railPanel.y + L.railPanel.h + gap;
        // os DOIS chips têm de caber (o par: 2 alvos + o vão)
        const bool chipsFit =
            (cy + 2.0f * btn + gap <= view.y + view.h);
        for (int i = 0; i < 2; ++i) {
            L.chips[i] = chipsFit
                             ? UiRect{L.rail[0].x,
                                      cy + static_cast<f32>(i) * (btn + gap),
                                      btn, btn}
                             : UiRect{0.0f, 0.0f, 0.0f, 0.0f};
        }
    } else {
        L.chips[0] = {0.0f, 0.0f, 0.0f, 0.0f};
        L.chips[1] = {0.0f, 0.0f, 0.0f, 0.0f};
    }

    // (2) O GIZMO DO CANTO — REMOVIDO na 0.12-C (C-2 · A ORDEM DO DONO):
    // «Ferramentas do viewport numa só barra (remover o duplicado do Mover
    // se não tiver função diferente)» — o botão 40dp do topo-direito
    // mostrava O MESMO ícone da ferramenta ativa (Move/Rotate/Scale) e
    // alternava a transição selectMode↔gizmo que o RAIL JÁ tem (o botão
    // Selecionar). Sem função diferente = fachada = SAI (a cláusula da
    // campanha). O id kVpGizmoId fica APOSENTADO (o padrão D21 do [+] —
    // o pin mover_unico caça-o). Os rects ficam ZERO (o layout é honesto).
    L.gizmoBtn = {0.0f, 0.0f, 0.0f, 0.0f};
    L.gizmoPanel = {0.0f, 0.0f, 0.0f, 0.0f};

    // (3) o chip do TOPO (⋯ — FASE UI B: a fila do topo perdeu o
    // undo/redo/save, que subiram para a BARRA; a POSIÇÃO decide-se por
    // tentativa — (a) o TOPO, ao lado do rail e ANTES DA BORDA DIREITA
    // (o layout primário; 0.12-C: o gizmo do canto saiu — a fila respira
    // até à borda); (b) senão PARA BAIXO do rail (o rect estreito); (c)
    // senão ESCONDE (a degradação honesta — a fila NUNCA sai do rect; a
    // regra vale para o piso kViewportMinW=288dp da casa)
    const f32 quickW = qbtn;
    const f32 qxTop = view.x + m +
                      (L.railVisible ? railW + theme::dp(12.0f) : 0.0f);
    const bool topFits =
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
        L.quick[0] = {0.0f, 0.0f, 0.0f, 0.0f};
        L.quickPanel = {0.0f, 0.0f, 0.0f, 0.0f};
    } else {
        L.quick[0] = {qx, qy, qbtn, qbtn};
        L.quickPanel = {qx - theme::dp(4.0f), qy - theme::dp(4.0f),
                        quickW + theme::dp(8.0f), qbtn + theme::dp(8.0f)};
    }

    // (4) a LEGENDA — à direita do rail, SOB a fila do topo (o layout
    // primário); 0.12-C: sem o gizmo do canto, a legenda respira até à
    // borda direita; some quando a largura/altura não dá (a degradação
    // de sempre — e no layout "quick below" não há onde a pôr)
    const f32 legendX = view.x + m +
                        (L.railVisible ? railW + theme::dp(4.0f) : 0.0f);
    const f32 legendY = view.y + m + qbtn + theme::dp(2.0f);
    const f32 legendMaxW = view.x + view.w - m - theme::dp(4.0f) - legendX;
    L.legendVisible = L.railVisible && L.railCols == 1u && quickFits &&
                      !quickBelow && legendMaxW >= theme::dp(72.0f) &&
                      (legendY + theme::dp(16.0f) + m) <= view.y + view.h;
    L.legend = L.legendVisible
                   ? UiRect{legendX, legendY, legendMaxW, theme::dp(16.0f)}
                   : UiRect{0.0f, 0.0f, 0.0f, 0.0f};
    return L;
}

Actions draw(UiContext& ui, EditorState& st, toolbar::GizmoModeState& gz,
             const ChromeState& cs, const Camera& camera, f32 drawerH,
             f32 dt, const InputState* in) {
    (void)camera;
    Actions a;
    // 0.12-A (A-3): O CICLO DE VIDA DO TOOLTIP (a legenda de sempre virou
    // transitória — o dono: «fica preso no viewport após o toque»):
    //   • nasce no LARGAR do toque que escolheu a ferramenta (legendArm);
    //   • morre aos 1,5s (o timer decai com o dt REAL do frame);
    //   • morre no LARGAR do toque SEGUINTE (o release depois de viva).
    // O release de nascimento é o MESMO edge — o else-if garante que não
    // mata o que acaba de nascer (o pin morre_release usa um toque NOVO).
    if (in != nullptr) {
        if (st.legendArm && !in->down(0)) {
            st.legendArm = false;
            st.legendTimer = 1.5f;   // nasce no largar («após o toque»)
        } else if (st.legendTimer > 0.0f && in->released(0)) {
            st.legendTimer = 0.0f;   // o largar seguinte mata
        }
    }
    if (st.legendTimer > 0.0f) {
        st.legendTimer -= dt;
        if (st.legendTimer < 0.0f) {
            st.legendTimer = 0.0f;
        }
    }
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
    // pai) — a 60% de alfa (PASSO 3). 0.12-QoL C (A ORDEM DO DONO ·
    // Redbr1): o pai do RAIL SAI — os botões de Mover/Rodar/Escalar
    // flutuam SEM o fundo preto quadrado gigante; cada um tem o SEU chip
    // quieto (o toolButton desenha). O rect do railPanel continua a
    // existir no layout (o contrato dos pins — a banda da hierarquia);
    // só o DESENHO morre. A fila do topo (undo/redo/save) mantém o pai.
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
            st.panMode = false;   // 0.12-Y: os modos são exclusivos
            gz.mode = 0;
            st.legendArm = true;   // 0.12-A (A-3): o tooltip nasce no largar
        }
        if (toolButton(ui, toolbar::kGizmoIds[0], L.rail[1], icons::Icon::Move,
                       !st.selectMode && gz.mode == 0 && !st.panMode)) {
            st.selectMode = false;
            st.panMode = false;
            gz.mode = 0;
            st.legendArm = true;   // 0.12-A (A-3)
        }
        if (toolButton(ui, toolbar::kGizmoIds[1], L.rail[2],
                       icons::Icon::Rotate,
                       !st.selectMode && gz.mode == 1 && !st.panMode)) {
            st.selectMode = false;
            st.panMode = false;
            gz.mode = 1;
            st.legendArm = true;   // 0.12-A (A-3)
        }
        if (toolButton(ui, toolbar::kGizmoIds[2], L.rail[3],
                       icons::Icon::Scale,
                       !st.selectMode && gz.mode == 2 && !st.panMode)) {
            st.selectMode = false;
            st.panMode = false;
            gz.mode = 2;
            st.legendArm = true;   // 0.12-A (A-3)
        }
        // FASE UI D: a MÃO (Pan) e o ÍMAN vivem nos CHIPS sob o rail (os
        // alvos 40dp de sempre, o desenho 28 — os MESMOS ids 33/36, o
        // MESMO despacho; a coluna das ferramentas respira)
        if (L.chips[0].w > 0.0f) {
            // 0.12-Y (3.Y-A): a MÃOZINHA — o PAN (o target no plano da
            // câmara; o pan de 2 dedos é LIVRE e não precisa deste modo)
            if (toolButton(ui, kVpPanId, L.chips[0], icons::Icon::Hand,
                           st.panMode, 6.0f)) {
                st.panMode = true;
                st.selectMode = false;
                gz.mode = 0;   // o gizmo volta Mover ao sair do Pan
                st.legendArm = true;   // 0.12-A (A-3): o tooltip no largar
            }
            // o ÍMAN: estado ativo/inativo, SEM texto (o valor segue no
            // tooltip do gizmo — G2-9)
            const bool pressed =
                ui.widgetHit(kVpSnapValId, L.chips[1].x, L.chips[1].y,
                             L.chips[1].w, L.chips[1].h);
            const bool held = ui.widgetActive(kVpSnapValId);
            const bool on = gz.snap || held;
            const f32 inset = theme::dp(6.0f);
            const UiRect d = {L.chips[1].x + inset, L.chips[1].y + inset,
                              L.chips[1].w - 2.0f * inset,
                              L.chips[1].h - 2.0f * inset};
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
                            L.chips[1].x +
                                (L.chips[1].w - theme::dp(20.0f)) * 0.5f,
                            L.chips[1].y +
                                (L.chips[1].h - theme::dp(20.0f)) * 0.5f,
                            theme::dp(20.0f), on ? onCol : offCol);
            if (pressed) {
                gz.snap = !gz.snap;
            }
        }
        // a LEGENDA: o nome da ferramenta ATIVA (12sp, à direita do rail,
        // por baixo da fila do topo — nunca dentro do botão). 0.12-A (A-3):
        // TRANSITÓRIA — só vive enquanto o tooltip tem tempo (o largar da
        // escolha arma 1,5s; o largar seguinte mata — o C33 do dono)
        if (ui.hasFont() && L.legendVisible && st.legendTimer > 0.0f) {
            const char* name = st.panMode ? "Pan"
                               : st.selectMode ? "Selecionar"
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

    // ---- F4-Z2 (o quadro vazio com ⋮): o chip do TOPO MORREU — era um
    // DUPLICADO funcional do «Menu» da barra de topo (o MESMO fileMenu,
    // main.cpp:8074) e ao lado do rail de ferramentas parecia um quadro
    // vazio com um ⋮ sem propósito. O layout L.quick[0]/L.quickPanel
    // CONTINUA (os pins da degradação — test_wiring090/test_sentinels —
    // aferem o rect); só o DESENHO e o AÇÃO morrem, como o pai do rail
    // na 0.12-QoL C. O menu de ficheiro vive no «Menu» da barra.

    // ---- 0.12-C (C-2): o GIZMO 40dp do topo-direito FOI REMOVIDO (a
    // ordem do dono: «remover o duplicado do Mover se não tiver função
    // diferente» — o botão mostrava O MESMO ícone da ferramenta ativa e
    // alternava a transição selectMode↔gizmo que o RAIL já tem; sem
    // função diferente = fachada = SAI). O id kVpGizmoId fica APOSENTADO
    // (o padrão D21 do [+] — o pin mover_unico caça-o); os rects do
    // layout são ZERO (a mutação M-C2b repõe o botão → pin vermelho).

    // 0.9.6.19b (D21): o bloco do [+] 40dp REDONDO (fundo-direito) FOI
    // REMOVIDO — a decisão do dono (a ação vive no + da hierarquia e no
    // menu ⋯ «Novo objeto»; o canto inferior direito do viewport fica
    // LIMPO para a orbit/seleção). O pin: nenhum widget com o id 38
    // (kVpAddTicIdRetired) desenha dentro do rect do viewport.

    return a;
}

// ---- 0.12-B (B-3) — o toast OPACO (a fonte única das cores) ----------------

f32 toastBgAlpha() {
    return 1.0f;   // opaco: o contraste não depende da cena por baixo
}

void toastColors(f32 outBg[4], f32 outText[4]) {
    // os TOKENS da casa: 0.12-C (C-3) o toast é O ELEVADO (o topo da
    // escada de superfícies — SÓLIDO, o alfa 1 do B-3 mantém; o
    // M-B3b devolve o cinza-vidro)
    outBg[0] = theme::kTheme.elevated[0];
    outBg[1] = theme::kTheme.elevated[1];
    outBg[2] = theme::kTheme.elevated[2];
    outBg[3] = toastBgAlpha();
    outText[0] = theme::kTheme.text1[0];
    outText[1] = theme::kTheme.text1[1];
    outText[2] = theme::kTheme.text1[2];
    outText[3] = 1.0f;
}

namespace {
// a luminância RELATIVA WCAG (a linearização sRGB: c<=0.03928 → c/12.92;
// caso contrário ((c+0.055)/1.055)^2.4) — o MESMO rácio dos pins
f32 relLum(f32 c) {
    if (c <= 0.03928f) {
        return c / 12.92f;
    }
    return std::pow((c + 0.055f) / 1.055f, 2.4f);
}
} // namespace

f32 toastContrastRatio() {
    f32 bg[4], tx[4];
    toastColors(bg, tx);
    const f32 lb = 0.2126f * relLum(bg[0]) + 0.7152f * relLum(bg[1]) +
                   0.0722f * relLum(bg[2]);
    const f32 lt = 0.2126f * relLum(tx[0]) + 0.7152f * relLum(tx[1]) +
                   0.0722f * relLum(tx[2]);
    const f32 hi = lt > lb ? lt : lb;
    const f32 lo = lt > lb ? lb : lt;
    return (hi + 0.05f) / (lo + 0.05f);
}

} // namespace vpchrome
} // namespace editor
} // namespace vv
