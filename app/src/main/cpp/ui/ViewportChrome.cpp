// ui/ViewportChrome.cpp — stack vertical + toolbar inferior + triad (0.9.0).
//
// Tudo lido de theme::kTheme (tabela spec A): botões alvo 48dp (stack) e
// 56dp (toolbar inferior rotulada), ícones 24dp, ativo = fill accent +
// accentInk, disabled = text2 a 40% (o alvo continua ≥48dp — desativado NÃO
// encolhe), chip [snap: <valor>] com raio 4dp. O triad projeta os eixos
// unitários pela VIEW da câmara (X vermelho, Y verde, Z azul — as cores de
// eixo documentadas dos gizmos 3D).
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

// botão da TOOLBAR INFERIOR: 56dp rotulado (ícone + palavra), ativo = fill
// accent + accentInk (spec D)
bool modeButton(UiContext& ui, u64 id, const UiRect& r, icons::Icon icon,
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
    const f32 wordW = ui.hasFont() ? ui.fontWidth(word) : 0.0f;
    const f32 gap = 8.0f;
    const f32 total = s + gap + wordW;
    const f32 x0 = r.x + (r.w - total) * 0.5f;
    const f32 col[4] = {on ? theme::kTheme.accentInk[0] : theme::kTheme.text1[0],
                        on ? theme::kTheme.accentInk[1] : theme::kTheme.text1[1],
                        on ? theme::kTheme.accentInk[2] : theme::kTheme.text1[2],
                        1.0f};
    icons::drawIcon(ui, icon, x0, r.y + (r.h - s) * 0.5f, s, col);
    if (ui.hasFont()) {
        const TextMetrics m = ui.textMetrics();
        ui.label(x0 + s + gap, r.y + (r.h - m.block()) * 0.5f + m.ascent, word,
                 col);
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
    // ---- triad 64dp canto superior direito ----
    L.triad = {view.x + view.w - kTriad - 8.0f, view.y + 8.0f, kTriad, kTriad};
    // ---- toolbar inferior (56dp, 8dp acima do fundo) ----
    const f32 by = view.y + view.h - kBottomH - 8.0f;
    f32 bx = view.x + 8.0f;
    L.selectBtn = {bx, by, 128.0f, kBottomH};  bx += 128.0f + 8.0f;
    L.moveBtn   = {bx, by, 112.0f, kBottomH};  bx += 112.0f + 8.0f;
    L.rotateBtn = {bx, by, 112.0f, kBottomH};  bx += 112.0f + 8.0f;
    L.scaleBtn  = {bx, by, 120.0f, kBottomH};  bx += 120.0f + 8.0f;
    L.snapChip  = {bx, by, 116.0f, kBottomH};  bx += 116.0f + 8.0f;
    L.settingsBtn = {bx, by, kBottomH, kBottomH};  bx += kBottomH + 8.0f;
    L.addTicBtn = {bx, by, 152.0f, kBottomH};
    return L;
}

Actions draw(UiContext& ui, EditorState& st, toolbar::GizmoModeState& gz,
             const ChromeState& cs, const Camera& camera) {
    Actions a;
    const Layout L = layout(safe::centerRect(
        ui.screenWidth(), ui.screenHeight(), ui.safeArea(), 0.0f,
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
    if (modeButton(ui, kVpSelectId, L.selectBtn, icons::Icon::Cursor,
                   "Selecionar", st.selectMode)) {
        st.selectMode = true;
        gz.mode = 0;
    }
    if (modeButton(ui, toolbar::kGizmoIds[0], L.moveBtn, icons::Icon::Move,
                   "Mover", !st.selectMode && gz.mode == 0)) {
        st.selectMode = false;
        gz.mode = 0;
    }
    if (modeButton(ui, toolbar::kGizmoIds[1], L.rotateBtn, icons::Icon::Rotate,
                   "Rodar", !st.selectMode && gz.mode == 1)) {
        st.selectMode = false;
        gz.mode = 1;
    }
    if (modeButton(ui, toolbar::kGizmoIds[2], L.scaleBtn, icons::Icon::Scale,
                   "Escalar", !st.selectMode && gz.mode == 2)) {
        st.selectMode = false;
        gz.mode = 2;
    }

    // chip [snap: <valor>] — toque cicla o valor (0.1/0.25/0.5/1/off)
    {
        const bool pressed = ui.widgetHit(kVpSnapValId, L.snapChip.x,
                                          L.snapChip.y, L.snapChip.w,
                                          L.snapChip.h);
        ui.panelRounded(L.snapChip.x, L.snapChip.y, L.snapChip.w, L.snapChip.h,
                        theme::kRadiusField, theme::kTheme.surface);
        ui.frameRounded(L.snapChip.x, L.snapChip.y, L.snapChip.w, L.snapChip.h,
                        1.0f, theme::kRadiusField,
                        gz.snap ? theme::kTheme.accent : theme::kTheme.border);
        if (ui.hasFont()) {
            char label[32];
            std::snprintf(label, sizeof(label), gz.snap ? "snap: %.2g"
                                                        : "snap: off",
                          cs.snapValue);
            const f32 tw = ui.fontWidth(label);
            ui.label(L.snapChip.x + (L.snapChip.w - tw) * 0.5f,
                     textBaseline(ui, L.snapChip), label,
                     gz.snap ? theme::kTheme.accent : theme::kTheme.text2);
        }
        if (pressed) {
            gz.snap = !gz.snap;
        }
    }

    // [viewport settings] 🔶 — abre o MESMO popover do "sliders" da top bar
    if (modeButton(ui, kVpSettingsId, L.settingsBtn, icons::Icon::Sliders, "",
                   false)) {
        a.settingsPressed = true;
    }

    // [Adicionar TIC] — o plus-menu de sempre
    if (modeButton(ui, kVpAddTicId, L.addTicBtn, icons::Icon::Plus,
                   "Adicionar TIC", false)) {
        a.addTicPressed = true;
    }

    // ---- TRIAD de orientação (canto sup-dir): eixos unitários projetados
    // pela VIEW da câmara — X vermelho, Y verde, Z azul (documentado) ----
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
