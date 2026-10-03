// ui/Toolbar.cpp — TOP BAR 56dp + TAB BAR DE MODO 48dp (0.9.0, spec D).
//
// Desenho: fundo bg #0B0E13 (chrome separado da área de trabalho — painéis
// surface #151A23); botões-alvo 48dp centrados na altura de 56; separadores
// finos border entre grupos; tab bar com ícone+palavra e UNDERLINE accent
// 2dp no ativo (spec D — nunca mais "UDIO": a palavra inteira desenha).
#include "ui/Toolbar.h"
#include "ui/EditorUi.h"   // EditorState completo (declared-only no header)
#include "ui/UiContext.h"

#include <cstdio>

namespace vv {
namespace editor {
namespace toolbar {

namespace {

// ---- métricas naturais (px de design — spec A: alvos ≥48, ícone 24) ---------
constexpr f32 kPadOuter  = 12.0f;   // margem da barra aos extremos
constexpr f32 kBtnH      = 48.0f;   // ALVO de toque (spec A) dentro dos 56
constexpr f32 kMenuW     = 140.0f;  // [≡ Menu]  (ícone + palavra)
constexpr f32 kCenaW     = 124.0f;  // [Cena ▾]
constexpr f32 kIconBtn   = 48.0f;   // [pause][play][sliders][gear]
constexpr f32 kGroupGap  = 24.0f;   // vão entre grupos (8-múltiplo + sep fino)
constexpr f32 kSepH      = 28.0f;   // altura da linha fina do separador

// baseline do texto centrada no botão (métricas REAIS da fonte)
f32 textBaseline(UiContext& ui, const UiRect& r) {
    if (!ui.hasFont()) {
        return r.y + r.h * 0.5f;
    }
    const TextMetrics m = ui.textMetrics();
    return r.y + (r.h - m.block()) * 0.5f + m.ascent;
}

// botão de TEXTO + ÍCONE (Menu/Cena): inativo = texto text1 + ícone text2;
// held = fill surface2 (estado premido — spec A)
bool textIconButton(UiContext& ui, u64 id, const UiRect& r,
                    icons::Icon icon, const char* text, bool withChevron) {
    const bool pressed = ui.widgetHit(id, r.x, r.y, r.w, r.h);
    const bool held = ui.widgetActive(id);
    if (held) {
        ui.panelRounded(r.x, r.y, r.w, r.h, theme::kRadiusCard,
                        theme::kTheme.surface2);
    }
    const f32 iconS = 24.0f;
    const f32 iconX = r.x + 10.0f;
    icons::drawIcon(ui, icon, iconX, r.y + (r.h - iconS) * 0.5f, iconS,
                    held ? theme::kTheme.text1 : theme::kTheme.text2);
    if (ui.hasFont()) {
        const f32 tw = ui.fontWidth(text);
        const f32 maxW = r.w - iconS - 18.0f - (withChevron ? 14.0f : 0.0f);
        char fit[48];
        const char* shown = text;
        if (tw > maxW) {
            textfit::ellipsize(text, maxW,
                               [&](const char* s) { return ui.fontWidth(s); },
                               fit, sizeof(fit));
            shown = fit;
        }
        ui.label(iconX + iconS + 8.0f, textBaseline(ui, r), shown,
                 held ? theme::kTheme.text1 : theme::kTheme.text1);
    }
    if (withChevron) {   // caret ▾ (2 traços — o atlas é ASCII)
        const f32 cy = r.y + r.h * 0.5f;
        const f32 cx = r.x + r.w - 12.0f;
        const f32 col[4] = {theme::kTheme.text2[0], theme::kTheme.text2[1],
                            theme::kTheme.text2[2], theme::kTheme.text2[3]};
        ui.drawLine(cx - 4.0f, cy - 2.0f, cx, cy + 2.5f, 2.0f, col);
        ui.drawLine(cx, cy + 2.5f, cx + 4.0f, cy - 2.0f, 2.0f, col);
    }
    return pressed;
}

// botão de ÍCONE puro (pause/play/sliders/gear): held = fill surface2,
// ícone text1 (o accent fica para ESTADOS, não para repouso — spec A)
bool iconButton(UiContext& ui, u64 id, const UiRect& r, icons::Icon icon) {
    const bool pressed = ui.widgetHit(id, r.x, r.y, r.w, r.h);
    const bool held = ui.widgetActive(id);
    if (held) {
        ui.panelRounded(r.x, r.y, r.w, r.h, theme::kRadiusCard,
                        theme::kTheme.surface2);
    }
    const f32 s = r.h - 12.0f;
    if (s >= 12.0f) {
        icons::drawIcon(ui, icon, r.x + (r.w - s) * 0.5f,
                        r.y + (r.h - s) * 0.5f, s, theme::kTheme.text1);
    }
    return pressed;
}

} // namespace

// ---------------------------------------------------------------------------
// TOP BAR — layout puro
// ---------------------------------------------------------------------------
TopBarLayout topbarLayout(f32 sw, f32 sh, const safe::Insets& in) {
    TopBarLayout L;
    L.bar = safe::toolbarRect(sw, sh, in);
    const f32 avail = L.bar.w - 2.0f * kPadOuter;
    if (avail <= 120.0f) {
        return L;   // degenerado — só o fundo
    }
    const f32 g1 = kMenuW + kCenaW;              // esquerda
    const f32 g2 = 3.0f * kIconBtn + 2.0f * 8.0f; // centro (pause play sliders)
    const f32 g5 = kIconBtn;                      // direita (gear)
    f32 gap = kGroupGap;
    f32 total = g1 + g2 + g5 + 3.0f * gap;
    f32 k = 1.0f;
    if (total > avail) {
        gap = 12.0f;
        total = g1 + g2 + g5 + 3.0f * gap;
        k = (avail - 3.0f * gap) / (total - 3.0f * gap);
        if (k > 1.0f) {
            k = 1.0f;
        }
    }
    L.menu.w     = kMenuW * k;
    L.cena.w     = kCenaW * k;
    L.pause.w    = kIconBtn * k;
    L.play.w     = kIconBtn * k;
    L.sliders.w  = kIconBtn * k;
    L.gear.w     = kIconBtn * k;
    L.iconSize   = 24.0f;

    const f32 by = L.bar.y + (L.bar.h - kBtnH) * 0.5f;
    const auto place = [&by](UiRect& r, f32 x) {
        r.x = x;
        r.y = by;
        r.h = kBtnH;
    };
    f32 x = L.bar.x + kPadOuter;
    place(L.menu, x);  x += L.menu.w;
    place(L.cena, x + 4.0f);  x += 4.0f + L.cena.w;
    x += gap;
    // centro do grupo central = centro da barra (spec D: [pause][play][sliders])
    const f32 g2w = L.pause.w + 8.0f + L.play.w + 8.0f + L.sliders.w;
    f32 cx = L.bar.x + (L.bar.w - g2w) * 0.5f;
    if (cx < x + gap) {
        cx = x + gap;   // defesa: telas estreitas — o centro cede à esquerda
    }
    place(L.pause, cx);      cx += L.pause.w + 8.0f;
    place(L.play, cx);       cx += L.play.w + 8.0f;
    place(L.sliders, cx);
    // gear: BORDA DIREITA (nunca mexe)
    place(L.gear, L.bar.x + L.bar.w - kPadOuter - L.gear.w);
    return L;
}

TopBarActions drawTopBar(UiContext& ui, const EditorState& st) {
    (void)st;
    TopBarActions a;
    const TopBarLayout L =
        topbarLayout(ui.screenWidth(), ui.screenHeight(), ui.safeArea());

    // fundo do chrome + risca inferior (border — spec A)
    ui.panel(L.bar.x, L.bar.y, L.bar.w, L.bar.h, theme::kTheme.bg);
    ui.panel(L.bar.x, L.bar.y + L.bar.h - 1.0f, L.bar.w, 1.0f,
             theme::kTheme.border);

    if (textIconButton(ui, kTbMenuId, L.menu, icons::Icon::Hamburger, "Menu",
                       false)) {
        a.menuDropdown = true;
    }
    if (textIconButton(ui, kTbCenaId, L.cena, icons::Icon::Folder, "Cena",
                       true)) {
        a.cenaDropdown = true;
    }
    if (iconButton(ui, kTbPauseId, L.pause, icons::Icon::Pause)) {
        a.pausePressed = true;
    }
    if (iconButton(ui, kTbPlayId, L.play, icons::Icon::Play)) {
        a.playPressed = true;
    }
    if (iconButton(ui, kTbSlidersId, L.sliders, icons::Icon::Sliders)) {
        a.slidersPressed = true;
    }
    if (iconButton(ui, kTbGearId, L.gear, icons::Icon::Gear)) {
        a.gearPressed = true;
    }
    return a;
}

// ---------------------------------------------------------------------------
// TAB BAR DE MODO — layout puro
// ---------------------------------------------------------------------------
ModeTabsLayout modetabsLayout(f32 sw, f32 sh, const safe::Insets& in,
                              bool uiMode, bool audioMode) {
    ModeTabsLayout L;
    L.bar = safe::modeTabRect(sw, sh, in);
    L.active = uiMode ? 1u : (audioMode ? 2u : 0u);
    const f32 third = L.bar.w / 3.0f;
    L.tab3d   = {L.bar.x, L.bar.y, third, L.bar.h};
    L.tabUi   = {L.bar.x + third, L.bar.y, third, L.bar.h};
    L.tabAudio = {L.bar.x + 2.0f * third, L.bar.y, third, L.bar.h};
    const UiRect* tabs[3] = {&L.tab3d, &L.tabUi, &L.tabAudio};
    L.underline = *tabs[L.active];
    L.underline.y = L.bar.y + L.bar.h - 2.0f;   // underline 2dp (spec D)
    L.underline.h = 2.0f;
    return L;
}

bool drawModeTabs(UiContext& ui, EditorState& st) {
    bool changed = false;
    const ModeTabsLayout L =
        modetabsLayout(ui.screenWidth(), ui.screenHeight(), ui.safeArea(),
                       st.uiMode, st.audioMode);

    ui.panel(L.bar.x, L.bar.y, L.bar.w, L.bar.h, theme::kTheme.bg);
    ui.panel(L.bar.x, L.bar.y + L.bar.h - 1.0f, L.bar.w, 1.0f,
             theme::kTheme.border);

    const struct {
        const UiRect* r;
        u64       id;
        icons::Icon icon;
        const char* word;
        bool      active;
    } tabs[3] = {
        {&L.tab3d,    kMode3dId,   icons::Icon::Cube,     "3D",     !st.uiMode && !st.audioMode},
        {&L.tabUi,    kModeUiId,   icons::Icon::Monitor,  "UI",     st.uiMode},
        {&L.tabAudio, kModeAudioId, icons::Icon::Speaker, "ÁUDIO",  st.audioMode},
    };
    for (int i = 0; i < 3; ++i) {
        const UiRect& r = *tabs[i].r;
        const bool pressed = ui.widgetHit(tabs[i].id, r.x, r.y, r.w, r.h);
        const bool held = ui.widgetActive(tabs[i].id);
        if (held) {
            ui.panel(r.x, r.y, r.w, r.h, theme::kTheme.surface2);
        }
        const f32 s = 24.0f;
        const f32 wordW = ui.hasFont() ? ui.fontWidth(tabs[i].word) : 0.0f;
        const f32 gap = 8.0f;
        const f32 total = s + gap + wordW;
        const f32 x0 = r.x + (r.w - total) * 0.5f;
        icons::drawIcon(ui, tabs[i].icon, x0, r.y + (r.h - s) * 0.5f, s,
                        tabs[i].active ? theme::kTheme.accent
                                       : theme::kTheme.text2);
        if (ui.hasFont()) {
            const TextMetrics m = ui.textMetrics();
            ui.label(x0 + s + gap,
                     r.y + (r.h - m.block()) * 0.5f + m.ascent, tabs[i].word,
                     tabs[i].active ? theme::kTheme.accent
                                    : theme::kTheme.text2);
        }
        if (pressed) {
            changed = true;
            if (i == 0) {
                st.uiMode = false;
                st.audioMode = false;
                st.selElement = -1;
                st.elDrag = false;
            } else if (i == 1) {
                st.uiMode = true;
                st.audioMode = false;
            } else {
                st.audioMode = true;
                st.uiMode = false;
                st.selElement = -1;
                st.elDrag = false;
            }
        }
    }
    // underline accent 2dp no tab ATIVO (spec D)
    ui.panel(L.underline.x, L.underline.y, L.underline.w, L.underline.h,
             theme::kTheme.accent);
    return changed;
}

// ---------------------------------------------------------------------------
// compat 0.8.x — draw() único (top bar + tab bar; o G4 morreu)
// ---------------------------------------------------------------------------
Actions draw(UiContext& ui, EditorState& st) {
    Actions a;
    const TopBarActions t = drawTopBar(ui, st);
    a.menuDropdown  = t.menuDropdown;
    a.cenaDropdown  = t.cenaDropdown;
    a.playPressed   = t.playPressed;
    a.pausePressed  = t.pausePressed;
    a.slidersPressed = t.slidersPressed;
    a.gearPressed   = t.gearPressed;
    a.modeChanged   = drawModeTabs(ui, st);
    return a;
}

} // namespace toolbar
} // namespace editor
} // namespace vv
