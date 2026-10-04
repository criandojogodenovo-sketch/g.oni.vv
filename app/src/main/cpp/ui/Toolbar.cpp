// ui/Toolbar.cpp — A BARRA ÚNICA DE CIMA 56dp (FASE 9 G2-9/G2-10).
//
// [≡ Menu][Cena ▾]    [3D][UI][ÁUDIO]    [pause][play][gear]
//
// FASE 9 (G2-10): a menu bar (56) + tab bar (48) fundiram-se NESTA barra
// de 56dp — os ~48px poupados vão ao viewport (safe::kToolbarH = 56).
// FASE 9 (G2-9): o botão [sliders] REMOVIDO (morto desde 0.9.0 — o
// inventário G0-4; slidersPressed nunca era consumido).
//
// Desenho: fundo bg #0B0E13 (chrome separado da área de trabalho — painéis
// surface #151A23); botões-alvo 48dp centrados na altura de 56; tabs de
// modo ao CENTRO com ícone+palavra e UNDERLINE accent 2dp no fundo da
// barra (spec D — "ÁUDIO" desenha a palavra inteira).
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
constexpr f32 kMenuW     = 112.0f;  // [≡ Menu]  (ícone + palavra)
constexpr f32 kCenaW     = 100.0f;  // [Cena ▾]
constexpr f32 kTabW      = 96.0f;   // cada tab [3D]/[UI]/[ÁUDIO]
constexpr f32 kIconBtn   = 48.0f;   // [pause][play][gear]
constexpr f32 kGroupGap  = 20.0f;   // vão entre grupos

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
    if (withChevron) {   // caret ▾ (2 traços)
        const f32 cy = r.y + r.h * 0.5f;
        const f32 cx = r.x + r.w - 12.0f;
        const f32 col[4] = {theme::kTheme.text2[0], theme::kTheme.text2[1],
                            theme::kTheme.text2[2], theme::kTheme.text2[3]};
        ui.drawLine(cx - 4.0f, cy - 2.0f, cx, cy + 2.5f, 2.0f, col);
        ui.drawLine(cx, cy + 2.5f, cx + 4.0f, cy - 2.0f, 2.0f, col);
    }
    return pressed;
}

// botão de ÍCONE puro (pause/play/gear): held = fill surface2,
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
// A BARRA ÚNICA — layout puro (FASE 9 G2-10)
// ---------------------------------------------------------------------------
TopBarLayout topbarLayout(f32 sw, f32 sh, const safe::Insets& in,
                          bool uiMode, bool audioMode) {
    TopBarLayout L;
    L.bar = safe::toolbarRect(sw, sh, in);
    L.active = uiMode ? 1u : (audioMode ? 2u : 0u);
    const f32 avail = L.bar.w - 2.0f * kPadOuter;
    if (avail <= 120.0f) {
        return L;   // degenerado — só o fundo
    }
    // escala graciosa: as larguras naturais encolhem PROPORCIONALMENTE se
    // o ecrã for estreito (o gear NUNCA sai da direita)
    const f32 natW = kMenuW + 4.0f + kCenaW + 3.0f * kTabW +
                     3.0f * kIconBtn + 2.0f * 8.0f;
    f32 k = 1.0f;
    if (natW + 4.0f * kGroupGap > avail) {
        k = (avail - 4.0f * kGroupGap) / natW;
        if (k > 1.0f) {
            k = 1.0f;
        }
        if (k < 0.55f) {
            k = 0.55f;   // piso: os alvos ficam ≥26dp… o C33 (1536) nunca chega
        }
    }
    L.menu.w  = kMenuW * k;
    L.cena.w  = kCenaW * k;
    L.tab3d.w = L.tabUi.w = L.tabAudio.w = kTabW * k;
    L.pause.w = L.play.w = L.gear.w = kIconBtn * k;
    L.iconSize = 24.0f;

    const f32 by = L.bar.y + (L.bar.h - kBtnH) * 0.5f;
    const auto place = [&by](UiRect& r, f32 x) {
        r.x = x;
        r.y = by;
        r.h = kBtnH;
    };

    // ---- esquerda: [≡ Menu][Cena ▾] ----
    f32 x = L.bar.x + kPadOuter;
    place(L.menu, x);            x += L.menu.w;
    place(L.cena, x + 4.0f);     x += 4.0f + L.cena.w;

    // ---- direita: [pause][play][gear] (ancorados — os grupos do centro
    // cedem primeiro em ecrãs estreitos) ----
    f32 rgx = L.bar.x + L.bar.w - kPadOuter - L.gear.w;
    place(L.gear, rgx);          rgx -= 8.0f + L.play.w;
    place(L.play, rgx);          rgx -= 8.0f + L.pause.w;
    place(L.pause, rgx);

    // ---- centro: as tabs [3D][UI][ÁUDIO] — centradas na BARRA; se não
    // couberem entre a esquerda e a direita, comprimem-se ao espaço útil ----
    const f32 tabsW = 3.0f * L.tab3d.w;
    f32 cx = L.bar.x + (L.bar.w - tabsW) * 0.5f;
    const f32 minCx = x + kGroupGap;
    const f32 maxCx = rgx - kGroupGap - tabsW;
    if (cx < minCx) {
        cx = minCx;
    }
    if (cx > maxCx) {
        cx = maxCx;
    }
    // ainda sem espaço? as tabs encolhem ao que sobrar (alvo ≥48 quando
    // possível — o touch continua na linha inteira da barra)
    f32 tabW = L.tab3d.w;
    if (cx + tabsW > rgx - kGroupGap) {
        const f32 room = (rgx - kGroupGap) - cx;
        if (room > 3.0f * 48.0f) {
            tabW = room / 3.0f;
        } else {
            tabW = 48.0f;   // piso 48dp — o grupo da direita cede (raro)
            cx = (x + kGroupGap + rgx - kGroupGap - 3.0f * tabW) * 0.5f;
        }
    }
    L.tab3d.w = L.tabUi.w = L.tabAudio.w = tabW;
    place(L.tab3d, cx);          cx += tabW;
    place(L.tabUi, cx);          cx += tabW;
    place(L.tabAudio, cx);

    // underline do ATIVO: 2dp accent NO FUNDO da barra (spec D)
    const UiRect* tabs[3] = {&L.tab3d, &L.tabUi, &L.tabAudio};
    L.underline = *tabs[L.active];
    L.underline.y = L.bar.y + L.bar.h - 2.0f;
    L.underline.h = 2.0f;
    return L;
}

TopBarActions drawTopBar(UiContext& ui, EditorState& st) {
    TopBarActions a;
    const TopBarLayout L =
        topbarLayout(ui.screenWidth(), ui.screenHeight(), ui.safeArea(),
                     st.uiMode, st.audioMode);

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

    // ---- tabs de modo AO CENTRO (G2-10): ícone+palavra, underline no ativo --
    const struct {
        const UiRect* r;
        u64          id;
        icons::Icon  icon;
        const char*  word;
        bool         active;
    } tabs[3] = {
        {&L.tab3d,    kMode3dId,    icons::Icon::Cube,     "3D",
         !st.uiMode && !st.audioMode},
        {&L.tabUi,    kModeUiId,    icons::Icon::Monitor,  "UI",    st.uiMode},
        {&L.tabAudio, kModeAudioId, icons::Icon::Speaker,  "ÁUDIO", st.audioMode},
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
            a.modeChanged = true;
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
    // underline accent 2dp no tab ATIVO (fundo da barra — spec D)
    ui.panel(L.underline.x, L.underline.y, L.underline.w, L.underline.h,
             theme::kTheme.accent);

    if (iconButton(ui, kTbPauseId, L.pause, icons::Icon::Pause)) {
        a.pausePressed = true;
    }
    if (iconButton(ui, kTbPlayId, L.play, icons::Icon::Play)) {
        a.playPressed = true;
    }
    if (iconButton(ui, kTbGearId, L.gear, icons::Icon::Gear)) {
        a.gearPressed = true;
    }
    return a;
}

// ---------------------------------------------------------------------------
// compat — draw() único (a barra única; G2-9: sliders REMOVIDO)
// ---------------------------------------------------------------------------
Actions draw(UiContext& ui, EditorState& st) {
    Actions a;
    const TopBarActions t = drawTopBar(ui, st);
    a.menuDropdown = t.menuDropdown;
    a.cenaDropdown = t.cenaDropdown;
    a.playPressed  = t.playPressed;
    a.pausePressed = t.pausePressed;
    a.gearPressed  = t.gearPressed;
    a.modeChanged  = t.modeChanged;
    return a;
}

} // namespace toolbar
} // namespace editor
} // namespace vv
