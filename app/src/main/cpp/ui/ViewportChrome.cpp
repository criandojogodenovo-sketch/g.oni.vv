// ui/ViewportChrome.cpp — stack vertical + toolbar inferior (FASE 9).
//
// FASE 9 (G1-1): toolbar ancorada ao RETÂNGULO DA VIEWPORT (drawerH real),
// só ícones (nome só no ativo), snap = íman, "+" no canto inferior
// direito, botão de settings REMOVIDO (morto — inventário G0-4).
#include "ui/ViewportChrome.h"
#include "ui/EditorUi.h"
#include "ui/UiContext.h"
#include "render/Camera.h"

#include <cstdio>

namespace vv {
namespace editor {
namespace vpchrome {

namespace {

f32 textBaseline(UiContext& ui, const UiRect& r) {
    if (!ui.hasFont()) {
        return r.y + r.h * 0.5f;
    }
    const TextMetrics m = ui.textMetrics();
    return r.y + (r.h - m.block()) * 0.5f + m.ascent;
}

// botão do STACK: 48dp, ícone 24 centrado; disabled = text2 40% (mesmo alvo)
bool stackButton(UiContext& ui, u64 id, const UiRect& r, icons::Icon icon,
                 bool enabled) {
    const bool pressed = ui.widgetHit(id, r.x, r.y, r.w, r.h);
    const bool held = ui.widgetActive(id);
    if (held && enabled) {
        ui.panelRounded(r.x, r.y, r.w, r.h, theme::dp(theme::kRadiusCard),
                        theme::kTheme.surface2);
    } else if (enabled) {
        // repouso: chip surface com bordo (o alvo é visível — nunca "quase
        // invisível", o problema documentado da 0.8.x)
        ui.panelRounded(r.x, r.y, r.w, r.h, theme::dp(theme::kRadiusCard),
                        theme::kTheme.surface);
        ui.frameRounded(r.x, r.y, r.w, r.h, 1.0f, theme::dp(theme::kRadiusCard),
                        theme::kTheme.border);
    } else {
        ui.panelRounded(r.x, r.y, r.w, r.h, theme::dp(theme::kRadiusCard),
                        theme::kTheme.bg);
    }
    f32 col[4] = {theme::kTheme.text1[0], theme::kTheme.text1[1],
                  theme::kTheme.text1[2], 1.0f};
    if (!enabled) {
        col[0] = theme::kTheme.text2[0];
        col[1] = theme::kTheme.text2[1];
        col[2] = theme::kTheme.text2[2];
        col[3] = 0.4f;
    }
    const f32 s = theme::dp(24.0f);
    icons::drawIcon(ui, icon, r.x + (r.w - s) * 0.5f, r.y + (r.h - s) * 0.5f,
                    s, col);
    return pressed && enabled;
}

// botão da TOOLBAR INFERIOR (0.9.6.1 · G1-2): SÓ ÍCONE — os 4 botões são
// IGUAIS de 48dp (a spec do dono: "4 botões iguais de 48dp, só ícone"). O
// NOME da ferramenta ativa passou para a LEGENDA ACIMA da barra (o "Escalar"
// de 48px estendia-se POR CIMA dos botões vizinhos — o layout só dava
// largura larga ao Selecionar e o draw pintava a palavra em QUALQUER ativo)
bool toolButton(UiContext& ui, u64 id, const UiRect& r, icons::Icon icon,
                const char* word, bool active) {
    (void)word;   // o nome vive na legenda acima da barra (draw abaixo)
    const bool pressed = ui.widgetHit(id, r.x, r.y, r.w, r.h);
    const bool held = ui.widgetActive(id);
    const bool on = active || held;
    if (on) {
        ui.panelRounded(r.x, r.y, r.w, r.h, theme::dp(theme::kRadiusCard),
                        theme::kTheme.accent);
    } else {
        ui.panelRounded(r.x, r.y, r.w, r.h, theme::dp(theme::kRadiusCard),
                        theme::kTheme.surface);
        ui.frameRounded(r.x, r.y, r.w, r.h, 1.0f, theme::dp(theme::kRadiusCard),
                        theme::kTheme.border);
    }
    const f32 s = theme::dp(24.0f);
    const f32 col[4] = {on ? theme::kTheme.accentInk[0] : theme::kTheme.text1[0],
                        on ? theme::kTheme.accentInk[1] : theme::kTheme.text1[1],
                        on ? theme::kTheme.accentInk[2] : theme::kTheme.text1[2],
                        1.0f};
    icons::drawIcon(ui, icon, r.x + (r.w - s) * 0.5f,
                    r.y + (r.h - s) * 0.5f, s, col);
    return pressed;
}

} // namespace

Layout layout(const UiRect& view) {
    Layout L;
    L.view = view;
    // 0.9.6.1 (PASSO 0 · R-018): todos os alvos daqui são dp REAL — eram px
    // crus (o dono media botões de ferramentas com 48px de altura no device)
    const f32 stackBtn = theme::dp(kStackBtn);
    const f32 stackGap = theme::dp(kStackGap);
    const f32 margin = theme::dp(8.0f);
    // GRUPO D (0.9.6.7 — A BARRA DE TOQUE CABE NO ORÇAMENTO): o chrome
    // ADAPTa-se ao rect da viewport (o device de 800dp deixava o viewport a
    // ~200dp: o stack de 5×48+4×8+8 = 280dp TRANBORDAVA o fundo por cima
    // da toolbar e o [+] caía POR CIMA dos botões de ferramenta — o
    // kViewportMinW dos divisores garante ≥320dp de largura; a ALTURA
    // adapta-se AQUI, em colunas, SEMPRE com alvos de 48dp inteiros).
    //
    // (1) o [+] : canto inferior direito SE cabe ao lado da toolbar
    // (toolbar = 5×48 + 4×8 = 272 + margens); senão sobe para o canto
    // superior direito (onde o triad esteve até à FASE 9).
    const f32 toolbarW = 5.0f * theme::dp(kToolBtn) + 4.0f * stackGap;
    const bool plusBottom = view.w >= toolbarW + margin + theme::dp(56.0f) +
                                       margin + margin;
    L.plusTopRight = !plusBottom;
    // (2) o stack: COLUNAS suficientes para a altura útil (a faixa da
    // toolbar em baixo come 56dp + 8 de folga; o topo tem 8 de margem).
    // O menor nº de colunas que caiba — 1 coluna nos ecrãs largos (o
    // layout de sempre, ZERO mudança onde cabe), 2/3 nos curtos.
    const f32 availH = view.h - margin - theme::dp(kBottomH) - margin;
    const f32 availW = view.w - 2.0f * margin -
                       (L.plusTopRight ? theme::dp(56.0f) + margin : 0.0f);
    u32 cols = 1;
    bool fits = false;
    for (; cols < 5; ++cols) {
        const u32 rowsPerCol = (5u + cols - 1u) / cols;   // ceil(5/cols)
        const f32 needH = static_cast<f32>(rowsPerCol) * stackBtn +
                          static_cast<f32>(rowsPerCol - 1u) * stackGap;
        const f32 needW = static_cast<f32>(cols) * stackBtn +
                          static_cast<f32>(cols - 1u) * stackGap;
        if (needH <= availH && needW <= availW) {
            fits = true;
            break;
        }
    }
    if (!fits) {
        // nem 4 colunas couberam — a ÚLTIMA tentativa: 5 colunas de 1 linha
        // SEM a reserva do [+] (com ele no canto, a reserva pode ter sobrado)
        const f32 needW = 5.0f * stackBtn + 4.0f * stackGap;
        if (stackBtn <= availH && needW <= view.w - 2.0f * margin) {
            cols = 5;
            fits = true;
        }
    }
    // DEGRADAÇÃO HONESTA: nem o mínimo coube (viewport sub-toolbar — ex. o
    // drawer comido ao device) → o stack ESCONDE (toolbar+viewport mandam;
    // os rects ficam degenerados e o draw salta)
    L.stackVisible = fits;
    L.stackCols = fits ? cols : 1;
    // ---- stack (coluna-major: undo/redo/save/dup/paste, preenchendo
    // coluna a coluna — a ordem de leitura de sempre; degenerado quando
    // ESCONDIDO — o draw salta) ----
    const u32 rowsPerCol = (5u + L.stackCols - 1u) / L.stackCols;
    for (u32 i = 0; i < 5; ++i) {
        if (!L.stackVisible) {
            L.stack[i] = {0.0f, 0.0f, 0.0f, 0.0f};
            continue;
        }
        const u32 col = i / rowsPerCol;
        const u32 row = i % rowsPerCol;
        L.stack[i] = {view.x + margin +
                          static_cast<f32>(col) * (stackBtn + stackGap),
                      view.y + margin +
                          static_cast<f32>(row) * (stackBtn + stackGap),
                      stackBtn, stackBtn};
    }
    // FASE 9 (G2-10): o TRIAD foi REMOVIDO — os "pontinhos fantasma" do
    // dono (canto sup-dir do viewport, fora do mock); a orientação vive
    // no gizmo 3D e na câmara.
    // ---- toolbar inferior: SÓ ÍCONES, âncora = canto inferior ESQUERDO
    // do rect da viewport (G1-1). Uma fileira (garantida pelo
    // kViewportMinW = 320dp ≥ 272+16 da toolbar).
    const f32 botH = theme::dp(kBottomH);
    const f32 toolW = theme::dp(kToolBtn);
    const f32 by = view.y + view.h - botH - theme::dp(8.0f);
    f32 bx = view.x + theme::dp(8.0f);
    L.selectBtn = {bx, by, toolW, botH};  bx += toolW + theme::dp(8.0f);
    L.moveBtn   = {bx, by, toolW, botH};  bx += toolW + theme::dp(8.0f);
    L.rotateBtn = {bx, by, toolW, botH};  bx += toolW + theme::dp(8.0f);
    L.scaleBtn  = {bx, by, toolW, botH};  bx += toolW + theme::dp(8.0f);
    L.snapBtn   = {bx, by, toolW, botH};
    // "+" — inferior direito se cabe; senão o canto SUPERIOR direito
    if (plusBottom) {
        L.addTicBtn = {view.x + view.w - theme::dp(56.0f) - margin, by,
                       theme::dp(56.0f), botH};
    } else {
        L.addTicBtn = {view.x + view.w - theme::dp(56.0f) - margin,
                       view.y + margin, theme::dp(56.0f), botH};
    }
    return L;
}

Actions draw(UiContext& ui, EditorState& st, toolbar::GizmoModeState& gz,
             const ChromeState& cs, const Camera& camera, f32 drawerH) {
    Actions a;
    // G1-1: o rect da viewport com o drawerH REAL — a toolbar acompanha o
    // painel de baixo (aberto = sobe; fechado = desce ao fundo da viewport)
    // GRUPO D: larguras de ESTADO (divisores) — o chrome acompanha os painéis
    const Layout L = layout(safe::centerRect(
        ui.screenWidth(), ui.screenHeight(), ui.safeArea(), drawerH,
        st.showInspector, st.hierW, st.inspW));

    // ---- stack vertical (pulado quando o layout ESCONDE — degradação) ----
    if (L.stackVisible) {
        if (stackButton(ui, kVpUndoId, L.stack[0], icons::Icon::Undo,
                        cs.canUndo)) {
            a.undoPressed = true;
        }
        if (stackButton(ui, kVpRedoId, L.stack[1], icons::Icon::Redo,
                        cs.canRedo)) {
            a.redoPressed = true;
        }
        if (stackButton(ui, kVpSaveId, L.stack[2], icons::Icon::Save, true)) {
            a.savePressed = true;
        }
        // FASE 9 (G2-11): o 4.º ícone era DUPLICATE (rect+plus — o
        // "quadrado com ponto" do dono) → agora é o COPY padrão (2
        // quadrados sobrepostos; a AÇÃO continua duplicar — só o GLIFO)
        if (stackButton(ui, kVpDupId, L.stack[3], icons::Icon::Copy, true)) {
            a.dupPressed = true;
        }
        if (stackButton(ui, kVpPasteId, L.stack[4], icons::Icon::Paste,
                        cs.canPaste)) {
            a.pastePressed = true;
        }
    }

    // ---- toolbar inferior: modos (Selecionar = SEM gizmo; gz.mode para o
    // gizmo da 0.6.9; o st.selectMode é o cursor de seleção por toque) ----
    if (toolButton(ui, kVpSelectId, L.selectBtn, icons::Icon::Cursor,
                   "Selecionar", st.selectMode)) {
        st.selectMode = true;
        gz.mode = 0;
    }
    if (toolButton(ui, toolbar::kGizmoIds[0], L.moveBtn, icons::Icon::Move,
                   "Mover", !st.selectMode && gz.mode == 0)) {
        st.selectMode = false;
        gz.mode = 0;
    }
    if (toolButton(ui, toolbar::kGizmoIds[1], L.rotateBtn, icons::Icon::Rotate,
                   "Rodar", !st.selectMode && gz.mode == 1)) {
        st.selectMode = false;
        gz.mode = 1;
    }
    if (toolButton(ui, toolbar::kGizmoIds[2], L.scaleBtn, icons::Icon::Scale,
                   "Escalar", !st.selectMode && gz.mode == 2)) {
        st.selectMode = false;
        gz.mode = 2;
    }
    // 0.9.6.1 (G1-2) · A LEGENDA: o nome da ferramenta ATIVA numa strip
    // pequena ACIMA da barra — nunca dentro do botão (nada se sobrepõe)
    {
        const char* name = st.selectMode ? "Selecionar"
                           : gz.mode == 0 ? "Mover"
                           : gz.mode == 1 ? "Rodar"
                                          : "Escalar";
        const f32 legendY = L.selectBtn.y - theme::dp(6.0f) -
                            theme::dp(12.0f);   // 12sp acima do topo da barra
        if (ui.hasFont()) {
            ui.labelStyled(L.selectBtn.x + theme::dp(2.0f), legendY, name,
                           theme::kTheme.text2,
                           theme::fontScale(theme::kFontCaption), 0);
        }
    }

    // snap: BOTÃO DE ÍMAN (G2-9 no mock, aplicado com a toolbar nova) —
    // estado ativo/inativo, SEM texto (o valor segue no tooltip do gizmo)
    {
        const bool pressed =
            ui.widgetHit(kVpSnapValId, L.snapBtn.x, L.snapBtn.y, L.snapBtn.w,
                         L.snapBtn.h);
        const bool held = ui.widgetActive(kVpSnapValId);
        const bool on = gz.snap || held;
        ui.panelRounded(L.snapBtn.x, L.snapBtn.y, L.snapBtn.w, L.snapBtn.h,
                        theme::dp(theme::kRadiusCard),
                        on ? theme::kTheme.accent : theme::kTheme.surface);
        if (!on) {
            ui.frameRounded(L.snapBtn.x, L.snapBtn.y, L.snapBtn.w, L.snapBtn.h,
                            1.0f, theme::dp(theme::kRadiusCard),
                            theme::kTheme.border);
        }
        const f32 col[4] = {on ? theme::kTheme.accentInk[0]
                               : theme::kTheme.text1[0],
                            on ? theme::kTheme.accentInk[1]
                               : theme::kTheme.text1[1],
                            on ? theme::kTheme.accentInk[2]
                               : theme::kTheme.text1[2],
                            1.0f};
        icons::drawIcon(ui, icons::Icon::Snap,
                        L.snapBtn.x + (L.snapBtn.w - theme::dp(24.0f)) * 0.5f,
                        L.snapBtn.y + (L.snapBtn.h - theme::dp(24.0f)) * 0.5f,
                        theme::dp(24.0f), col);
        if (pressed) {
            gz.snap = !gz.snap;
        }
    }

    // [+] Adicionar TIC — o plus-menu de sempre, no canto inferior DIREITO
    // da viewport (G1-1)
    {
        const bool pressed =
            ui.widgetHit(kVpAddTicId, L.addTicBtn.x, L.addTicBtn.y,
                         L.addTicBtn.w, L.addTicBtn.h);
        const bool held = ui.widgetActive(kVpAddTicId);
        ui.panelRounded(L.addTicBtn.x, L.addTicBtn.y, L.addTicBtn.w,
                        L.addTicBtn.h, theme::dp(theme::kRadiusCard),
                        held ? theme::kTheme.accentPress
                             : theme::kTheme.accent);
        const f32 col[4] = {theme::kTheme.accentInk[0],
                            theme::kTheme.accentInk[1],
                            theme::kTheme.accentInk[2], 1.0f};
        icons::drawIcon(ui, icons::Icon::Plus,
                        L.addTicBtn.x + (L.addTicBtn.w - theme::dp(24.0f)) * 0.5f,
                        L.addTicBtn.y + (L.addTicBtn.h - theme::dp(24.0f)) * 0.5f,
                        theme::dp(24.0f), col);
        if (pressed) {
            a.addTicPressed = true;
        }
    }

    // FASE 9 (G2-10): o TRIAD de orientação foi REMOVIDO (os
    // "pontinhos fantasma" do dono — fora do mock).

    return a;
}

} // namespace vpchrome
} // namespace editor
} // namespace vv
