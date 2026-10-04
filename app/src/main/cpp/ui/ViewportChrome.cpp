// ui/ViewportChrome.cpp — stack vertical + toolbar inferior + triad.
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
        ui.panelRounded(r.x, r.y, r.w, r.h, theme::kRadiusCard,
                        theme::kTheme.surface2);
    } else if (enabled) {
        // repouso: chip surface com bordo (o alvo é visível — nunca "quase
        // invisível", o problema documentado da 0.8.x)
        ui.panelRounded(r.x, r.y, r.w, r.h, theme::kRadiusCard,
                        theme::kTheme.surface);
        ui.frameRounded(r.x, r.y, r.w, r.h, 1.0f, theme::kRadiusCard,
                        theme::kTheme.border);
    } else {
        ui.panelRounded(r.x, r.y, r.w, r.h, theme::kRadiusCard,
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
    const f32 s = 24.0f;
    icons::drawIcon(ui, icon, r.x + (r.w - s) * 0.5f, r.y + (r.h - s) * 0.5f,
                    s, col);
    return pressed && enabled;
}

// botão da TOOLBAR INFERIOR (G1-1): SÓ ÍCONE quando inativo (48dp); o
// ATIVO ganha o NOME (fill accent + palavra — o único rótulo da barra)
bool toolButton(UiContext& ui, u64 id, const UiRect& r, icons::Icon icon,
                const char* word, bool active) {
    const bool pressed = ui.widgetHit(id, r.x, r.y, r.w, r.h);
    const bool held = ui.widgetActive(id);
    const bool on = active || held;
    if (on) {
        ui.panelRounded(r.x, r.y, r.w, r.h, theme::kRadiusCard,
                        theme::kTheme.accent);
    } else {
        ui.panelRounded(r.x, r.y, r.w, r.h, theme::kRadiusCard,
                        theme::kTheme.surface);
        ui.frameRounded(r.x, r.y, r.w, r.h, 1.0f, theme::kRadiusCard,
                        theme::kTheme.border);
    }
    const f32 s = 24.0f;
    const f32 col[4] = {on ? theme::kTheme.accentInk[0] : theme::kTheme.text1[0],
                        on ? theme::kTheme.accentInk[1] : theme::kTheme.text1[1],
                        on ? theme::kTheme.accentInk[2] : theme::kTheme.text1[2],
                        1.0f};
    if (active && word && word[0]) {
        // ATIVO: ícone + palavra (o único rótulo da barra — G1-1)
        const f32 wordW = ui.hasFont() ? ui.fontWidth(word) : 0.0f;
        const f32 gap = 8.0f;
        const f32 total = s + gap + wordW;
        const f32 x0 = r.x + (r.w - total) * 0.5f;
        icons::drawIcon(ui, icon, x0, r.y + (r.h - s) * 0.5f, s, col);
        if (ui.hasFont()) {
            const TextMetrics m = ui.textMetrics();
            ui.label(x0 + s + gap, r.y + (r.h - m.block()) * 0.5f + m.ascent,
                     word, col);
        }
    } else {
        icons::drawIcon(ui, icon, r.x + (r.w - s) * 0.5f, r.y + (r.h - s) * 0.5f,
                        s, col);
    }
    return pressed;
}

} // namespace

Layout layout(const UiRect& view) {
    Layout L;
    L.view = view;
    // ---- stack vertical à esquerda (undo/redo/save/dup/paste) ----
    f32 y = view.y + 8.0f;
    const f32 x = view.x + 8.0f;
    for (int i = 0; i < 5; ++i) {
        L.stack[i] = {x, y, kStackBtn, kStackBtn};
        y += kStackBtn + kStackGap;
    }
    // ---- triad 64dp canto superior direito (removido no G2-10) ----
    L.triad = {view.x + view.w - kTriad - 8.0f, view.y + 8.0f, kTriad, kTriad};
    // ---- toolbar inferior: SÓ ÍCONES, âncora = canto inferior ESQUERDO
    // do rect da viewport (G1-1); o ATIVO ganha o nome (mais largo).
    // Total: ativo 132 + 3×48 + íman 48 + 4 gaps 8 = 368 ≤ viewport útil.
    const f32 by = view.y + view.h - kBottomH - 8.0f;
    f32 bx = view.x + 8.0f;
    // as larguras dependem de QUEM está ativo — o draw resolve o estado;
    // o layout usa a pior caso (um ativo por vez, sempre o MESMO total)
    const f32 w[5] = {kToolActiveW, kToolBtn, kToolBtn, kToolBtn, kToolBtn};
    L.selectBtn = {bx, by, w[0], kBottomH};  bx += w[0] + 8.0f;
    L.moveBtn   = {bx, by, w[1], kBottomH};  bx += w[1] + 8.0f;
    L.rotateBtn = {bx, by, w[2], kBottomH};  bx += w[2] + 8.0f;
    L.scaleBtn  = {bx, by, w[3], kBottomH};  bx += w[3] + 8.0f;
    L.snapBtn   = {bx, by, kToolBtn, kBottomH};
    // "+" no canto inferior DIREITO da viewport (G1-1)
    L.addTicBtn = {view.x + view.w - 56.0f - 8.0f, by, 56.0f, kBottomH};
    return L;
}

Actions draw(UiContext& ui, EditorState& st, toolbar::GizmoModeState& gz,
             const ChromeState& cs, const Camera& camera, f32 drawerH) {
    Actions a;
    // G1-1: o rect da viewport com o drawerH REAL — a toolbar acompanha o
    // painel de baixo (aberto = sobe; fechado = desce ao fundo da viewport)
    const Layout L = layout(safe::centerRect(
        ui.screenWidth(), ui.screenHeight(), ui.safeArea(), drawerH,
        st.showInspector));

    // ---- stack vertical ----
    if (stackButton(ui, kVpUndoId, L.stack[0], icons::Icon::Undo, cs.canUndo)) {
        a.undoPressed = true;
    }
    if (stackButton(ui, kVpRedoId, L.stack[1], icons::Icon::Redo, cs.canRedo)) {
        a.redoPressed = true;
    }
    if (stackButton(ui, kVpSaveId, L.stack[2], icons::Icon::Save, true)) {
        a.savePressed = true;
    }
    if (stackButton(ui, kVpDupId, L.stack[3], icons::Icon::Duplicate, true)) {
        a.dupPressed = true;
    }
    if (stackButton(ui, kVpPasteId, L.stack[4], icons::Icon::Paste,
                    cs.canPaste)) {
        a.pastePressed = true;
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

    // snap: BOTÃO DE ÍMAN (G2-9 no mock, aplicado com a toolbar nova) —
    // estado ativo/inativo, SEM texto (o valor segue no tooltip do gizmo)
    {
        const bool pressed =
            ui.widgetHit(kVpSnapValId, L.snapBtn.x, L.snapBtn.y, L.snapBtn.w,
                         L.snapBtn.h);
        const bool held = ui.widgetActive(kVpSnapValId);
        const bool on = gz.snap || held;
        ui.panelRounded(L.snapBtn.x, L.snapBtn.y, L.snapBtn.w, L.snapBtn.h,
                        theme::kRadiusCard,
                        on ? theme::kTheme.accent : theme::kTheme.surface);
        if (!on) {
            ui.frameRounded(L.snapBtn.x, L.snapBtn.y, L.snapBtn.w, L.snapBtn.h,
                            1.0f, theme::kRadiusCard, theme::kTheme.border);
        }
        const f32 col[4] = {on ? theme::kTheme.accentInk[0]
                               : theme::kTheme.text1[0],
                            on ? theme::kTheme.accentInk[1]
                               : theme::kTheme.text1[1],
                            on ? theme::kTheme.accentInk[2]
                               : theme::kTheme.text1[2],
                            1.0f};
        icons::drawIcon(ui, icons::Icon::Snap,
                        L.snapBtn.x + (L.snapBtn.w - 24.0f) * 0.5f,
                        L.snapBtn.y + (L.snapBtn.h - 24.0f) * 0.5f, 24.0f,
                        col);
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
                        L.addTicBtn.h, theme::kRadiusCard,
                        held ? theme::kTheme.accentPress
                             : theme::kTheme.accent);
        const f32 col[4] = {theme::kTheme.accentInk[0],
                            theme::kTheme.accentInk[1],
                            theme::kTheme.accentInk[2], 1.0f};
        icons::drawIcon(ui, icons::Icon::Plus,
                        L.addTicBtn.x + (L.addTicBtn.w - 24.0f) * 0.5f,
                        L.addTicBtn.y + (L.addTicBtn.h - 24.0f) * 0.5f, 24.0f,
                        col);
        if (pressed) {
            a.addTicPressed = true;
        }
    }

    // ---- TRIAD de orientação (canto sup-dir; REMOVIDO no G2-10 — os
    // "pontinhos fantasma" do dono; mantido até o grupo G2) ----
    {
        const Mat4 v = camera.view();
        // direções dos eixos NO ESPAÇO DA CÂMARA: linhas da view (rotação)
        const f32 cx = L.triad.x + L.triad.w * 0.5f;
        const f32 cy = L.triad.y + L.triad.h * 0.5f;
        const f32 len = L.triad.w * 0.42f;
        const struct {
            f32 ax, ay, az;      // eixo no mundo
            f32 col[4];
            const char* letter;
        } axes[3] = {
            {1.0f, 0.0f, 0.0f, {0.90f, 0.32f, 0.30f, 1.0f}, "X"},
            {0.0f, 1.0f, 0.0f, {0.42f, 0.76f, 0.42f, 1.0f}, "Y"},
            {0.0f, 0.0f, 1.0f, {0.35f, 0.52f, 0.90f, 1.0f}, "Z"},
        };
        for (int i = 0; i < 3; ++i) {
            // view: rotação transposta aplicada ao eixo (a view transforma
            // mundo→câmara; os eixos seguem a MESMA orientação)
            const f32 dx = v.m[0] * axes[i].ax + v.m[4] * axes[i].ay +
                           v.m[8] * axes[i].az;
            const f32 dy = v.m[1] * axes[i].ax + v.m[5] * axes[i].ay +
                           v.m[9] * axes[i].az;
            const f32 dz = v.m[2] * axes[i].ax + v.m[6] * axes[i].ay +
                           v.m[10] * axes[i].az;
            (void)dz;
            // projeção ortográfica simples no plano do triad (dy INVERTIDO:
            // o ecrã cresce para baixo)
            const f32 px = cx + dx * len;
            const f32 py = cy - dy * len;
            ui.drawLine(cx, cy, px, py, 3.0f, axes[i].col);
            if (ui.hasFont()) {
                const f32 lx = px + (dx >= 0 ? 4.0f : -18.0f);
                ui.label(lx, py + 5.0f, axes[i].letter, axes[i].col);
            }
        }
        // marco central (a origem do triad)
        const f32 dot[4] = {theme::kTheme.text2[0], theme::kTheme.text2[1],
                            theme::kTheme.text2[2], 1.0f};
        ui.panel(cx - 2.0f, cy - 2.0f, 4.0f, 4.0f, dot);
    }
    return a;
}

} // namespace vpchrome
} // namespace editor
} // namespace vv
