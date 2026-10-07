// ui/Toolbar.cpp — A BARRA ÚNICA DE CIMA 36dp (FASE 9 G2-9/G2-10 · PASSO 1).
//
// [≡ Menu][Cena ▾]    [3D][UI][ÁUDIO]    [pause][play][gear]
//
// FASE 9 (G2-10): a menu bar + tab bar fundiram-se NESTA barra — os px
// poupados vão ao viewport (safe::kToolbarH).
// PASSO 1 (0.9.6.14 · spec UI do dono): a barra é UMA LINHA de 36dp (era
// 56); os botões têm o alvo da ALTURA INTEIRA da barra (36) e o DESENHO
// em chip de 28dp centrado — a lei de ouro «desenho 32 / toque 40» no
// eixo que a barra permite (a largura); ÍCONES 20dp; texto 12sp (caption).
//
// Desenho: fundo bg #0B0E13 (chrome separado da área de trabalho — painéis
// surface #151A23); tabs de modo ao CENTRO com ícone+palavra e UNDERLINE
// accent 2dp no fundo da barra (spec D — "ÁUDIO" desenha a palavra inteira).
#include "ui/Toolbar.h"
#include "ui/EditorUi.h"   // EditorState completo (declared-only no header)
#include "ui/UiContext.h"
#include "ui/Brand.h"     // 0.9.6.18 (D6): o glifo da marca numa função

#include <cstdio>

namespace vv {
namespace editor {
namespace toolbar {

namespace {

// ---- métricas naturais (PASSO 1 · 0.9.6.14: alvo = a linha inteira da
// barra de 36dp; desenho em chip 28dp; ícones 20; texto caption 12sp) -----
// 0.9.6.1 (PASSO 0 · R-018): os valores são DP e multiplicam pela densidade
// AQUI (na fonte do layout da barra) — antes eram px crus: no C33 os alvos
// media ~24dp reais e o "Cena" truncava a "C…"
// 0.9.6.10 (GRUPO UI · região TOPO da imagem 1): o LOGO (G âmbar) e
// o NOME "G.One" à EXTREMA esquerda; o STOP (■) ao lado do play/pause;
// o CHIP de plataforma antes do gear (o alvo REAL: Android)
f32 kPadOuter()  { return theme::dp(12.0f); }   // margem da barra aos extremos
f32 kBtnH()      { return theme::dp(safe::kTopBarH); }  // alvo = a linha toda
f32 kChipH()     { return theme::dp(28.0f); }  // o DESENHO do botão (na barra 36)
f32 kLogoW()     { return theme::dp(40.0f); }  // o G âmbar (alvo inteiro)
f32 kNameW()     { return theme::dp(48.0f); }  // "G.One" 14sp
f32 kMenuW()     { return theme::dp(84.0f); }  // [≡ Menu]  (ícone + palavra)
f32 kCenaW()     { return theme::dp(92.0f); }  // [Cena ▾] — largura para
                                                // o rótulo inteiro
f32 kTabW()      { return theme::dp(72.0f); }  // cada tab [3D]/[UI]/[ÁUDIO]
f32 kIconBtn()   { return theme::dp(40.0f); }  // [play][pause][stop][gear]
f32 kPlatW()     { return theme::dp(76.0f); }  // o chip [Android ▾]
f32 kGroupGap()  { return theme::dp(12.0f); }  // vão entre grupos
f32 kIconS()     { return theme::dp(20.0f); }  // PASSO 1: os ícones da barra

// baseline do texto centrada no botão (métricas REAIS da fonte)
f32 textBaseline(UiContext& ui, const UiRect& r) {
    if (!ui.hasFont()) {
        return r.y + r.h * 0.5f;
    }
    const TextMetrics m = ui.textMetrics();
    return r.y + (r.h - m.block()) * 0.5f + m.ascent;
}

// botão de TEXTO + ÍCONE (Menu/Cena): inativo = texto text1 + ícone text2;
// held = fill surface2 NO CHIP de desenho (28dp centrado no alvo da linha)
bool textIconButton(UiContext& ui, u64 id, const UiRect& r,
                    icons::Icon icon, const char* text, bool withChevron) {
    // PASSO 1: o alvo é a LINHA da barra (36dp — o piso kRowFloorDp)
    ui.auditRowFloorNext(layout::kRowFloorDp);
    const bool pressed = ui.widgetHit(id, r.x, r.y, r.w, r.h);
    const bool held = ui.widgetActive(id);
    // PASSO 1: o DESENHO vive num chip 4dp mais baixo de cada lado (o alvo
    // é a linha inteira de 36dp)
    const UiRect chip = {r.x, r.y + (r.h - kChipH()) * 0.5f, r.w, kChipH()};
    if (held) {
        ui.panelRounded(chip.x, chip.y, chip.w, chip.h, theme::kRadiusCard,
                        theme::kTheme.surface2);
    }
    const f32 iconS = kIconS();
    const f32 iconX = r.x + theme::dp(10.0f);
    icons::drawIcon(ui, icon, iconX, r.y + (r.h - iconS) * 0.5f, iconS,
                    held ? theme::kTheme.text1 : theme::kTheme.text2);
    if (ui.hasFont()) {
        const f32 tw = ui.fontWidth(text);
        const f32 maxW = r.w - iconS - theme::dp(18.0f) -
                         (withChevron ? theme::dp(14.0f) : 0.0f);
        char fit[48];
        const char* shown = text;
        if (tw > maxW) {
            textfit::ellipsize(text, maxW,
                               [&](const char* s) { return ui.fontWidth(s); },
                               fit, sizeof(fit));
            shown = fit;
        }
        // PASSO 1: o texto da barra é CAPTION 12sp (o secundário da spec)
        ui.labelStyled(iconX + iconS + theme::dp(8.0f),
                       textBaseline(ui, r), shown,
                       theme::kTheme.text1,
                       theme::fontScale(theme::kFontCaption), 0);
    }
    if (withChevron) {   // caret ▾ (2 traços)
        const f32 cy = r.y + r.h * 0.5f;
        const f32 cx = r.x + r.w - theme::dp(12.0f);
        const f32 col[4] = {theme::kTheme.text2[0], theme::kTheme.text2[1],
                            theme::kTheme.text2[2], theme::kTheme.text2[3]};
        ui.drawLine(cx - theme::dp(4.0f), cy - theme::dp(2.0f), cx,
                    cy + theme::dp(2.5f), theme::dp(2.0f), col);
        ui.drawLine(cx, cy + theme::dp(2.5f), cx + theme::dp(4.0f),
                    cy - theme::dp(2.0f), theme::dp(2.0f), col);
    }
    return pressed;
}

// botão de ÍCONE puro (pause/play/gear): held = fill surface2 NO CHIP,
// ícone text1 (o accent fica para ESTADOS, não para repouso — spec A)
bool iconButton(UiContext& ui, u64 id, const UiRect& r, icons::Icon icon) {
    // PASSO 1: o alvo é a LINHA da barra (36dp — o piso kRowFloorDp)
    ui.auditRowFloorNext(layout::kRowFloorDp);
    const bool pressed = ui.widgetHit(id, r.x, r.y, r.w, r.h);
    const bool held = ui.widgetActive(id);
    const UiRect chip = {r.x, r.y + (r.h - kChipH()) * 0.5f, r.w, kChipH()};
    if (held) {
        ui.panelRounded(chip.x, chip.y, chip.w, chip.h, theme::kRadiusCard,
                        theme::kTheme.surface2);
    }
    const f32 s = kIconS();
    if (s >= theme::dp(12.0f)) {
        icons::drawIcon(ui, icon, r.x + (r.w - s) * 0.5f,
                        r.y + (r.h - s) * 0.5f, s, theme::kTheme.text1);
    }
    return pressed;
}

} // namespace

// ---------------------------------------------------------------------------
// A BARRA ÚNICA — layout puro (FASE 9 G2-10 · GRUPO UI região TOPO)
// ---------------------------------------------------------------------------
TopBarLayout topbarLayout(f32 sw, f32 sh, const safe::Insets& in,
                          bool uiMode, bool audioMode) {
    TopBarLayout L;
    L.bar = safe::toolbarRect(sw, sh, in);
    L.active = uiMode ? 1u : (audioMode ? 2u : 0u);
    const f32 avail = L.bar.w - 2.0f * kPadOuter();
    if (avail <= 160.0f) {
        return L;   // degenerado — só o fundo
    }
    // escala graciosa: as larguras naturais encolhem PROPORCIONALMENTE se
    // o ecrã for estreito. Os botões de ÍCONE (play/pause/stop/gear + o
    // LOGO) NUNCA ENCOLHEM (o alvo 40dp é o PISO); o k divide
    // só Menu/Cena/tabs/nome/plataforma. A DEGRADAÇÃO HONESTA (GRUPO UI):
    // o NOME "G.One" some primeiro (k<0.78 — o logo fica, é a
    // identidade), o CHIP da plataforma depois (k<0.62 — é informativo,
    // o alvo REAL é sempre Android; fica na lista honesta do relatório).
    const f32 nIcon = 5;   // play/pause/stop/gear + logo
    f32 natFlex = kMenuW() + theme::dp(4.0f) + kCenaW() + 3.0f * kTabW() +
                  kPlatW() + kNameW();
    const f32 natW = natFlex + nIcon * kIconBtn() + 2.0f * theme::dp(8.0f);
    auto scaleK = [&](f32 flexW) {
        const f32 iconsW = nIcon * kIconBtn();
        f32 k = (avail - iconsW - 4.0f * kGroupGap() - 2.0f * theme::dp(8.0f) -
                 theme::dp(4.0f)) /
                flexW;
        return k > 1.0f ? 1.0f : (k < 0.55f ? 0.55f : k);
    };
    f32 k = 1.0f;
    if (natW + 4.0f * kGroupGap() > avail) {
        k = scaleK(natFlex);
    }
    L.showName = k >= 0.78f;
    L.showPlatform = k >= 0.62f;
    if (!L.showName) {
        natFlex -= kNameW();
        k = scaleK(natFlex);
    }
    if (!L.showPlatform) {
        natFlex -= kPlatW();
        k = scaleK(natFlex);
    }
    L.menu.w  = kMenuW() * k;
    L.cena.w  = kCenaW() * k;
    L.tab3d.w = L.tabUi.w = L.tabAudio.w = kTabW() * k;
    L.platform.w = kPlatW() * k;
    L.name.w  = kNameW();
    L.logo.w  = kIconBtn();
    // os botões de ÍCONE nunca encolhem (o piso 40dp da casa)
    L.play.w = L.pause.w = L.stop.w = L.gear.w = kIconBtn();
    L.iconSize = kIconS();

    const f32 by = L.bar.y + (L.bar.h - kBtnH()) * 0.5f;
    const auto place = [&by](UiRect& r, f32 x) {
        r.x = x;
        r.y = by;
        r.h = kBtnH();
    };

    // ---- esquerda: [G][G.One][≡ Menu][Cena ▾] ----
    f32 x = L.bar.x + kPadOuter();
    place(L.logo, x);            x += L.logo.w;
    if (L.showName) {
        place(L.name, x);        x += L.name.w;
    }
    place(L.menu, x);            x += L.menu.w;
    place(L.cena, x + theme::dp(4.0f));     x += theme::dp(4.0f) + L.cena.w;

    // ---- direita: [play][pause][stop][Android ▾][gear] (ancorados) ----
    f32 rgx = L.bar.x + L.bar.w - kPadOuter() - L.gear.w;
    place(L.gear, rgx);          rgx -= theme::dp(8.0f);
    if (L.showPlatform) {
        rgx -= L.platform.w;
        place(L.platform, rgx);  rgx -= theme::dp(8.0f);
    }
    rgx -= L.stop.w;
    place(L.stop, rgx);          rgx -= theme::dp(8.0f);
    rgx -= L.pause.w;
    place(L.pause, rgx);         rgx -= theme::dp(8.0f);
    rgx -= L.play.w;
    place(L.play, rgx);

    // ---- centro: as tabs [3D][UI][ÁUDIO] — centradas na BARRA; se não
    // couberem entre a esquerda e a direita, comprimem-se ao espaço útil ----
    const f32 tabsW = 3.0f * L.tab3d.w;
    f32 cx = L.bar.x + (L.bar.w - tabsW) * 0.5f;
    const f32 minCx = x + kGroupGap();
    const f32 maxCx = rgx - kGroupGap() - tabsW;
    if (cx < minCx) {
        cx = minCx;
    }
    if (cx > maxCx) {
        cx = maxCx;
    }
    // ainda sem espaço? as tabs encolhem ao que sobrar (alvo ≥40 quando
    // possível — o touch continua na linha inteira da barra)
    f32 tabW = L.tab3d.w;
    if (cx + tabsW > rgx - kGroupGap()) {
        const f32 room = (rgx - kGroupGap()) - cx;
        if (room > 3.0f * kIconBtn()) {
            tabW = room / 3.0f;
        } else {
            tabW = kIconBtn();   // piso 40dp — o grupo da direita cede
            cx = (x + kGroupGap() + rgx - kGroupGap() - 3.0f * tabW) * 0.5f;
        }
    }
    L.tab3d.w = L.tabUi.w = L.tabAudio.w = tabW;
    place(L.tab3d, cx);          cx += tabW;
    place(L.tabUi, cx);          cx += tabW;
    place(L.tabAudio, cx);

    // underline do ATIVO: 2dp accent NO FUNDO da barra (spec D)
    const UiRect* tabs[3] = {&L.tab3d, &L.tabUi, &L.tabAudio};
    L.underline = *tabs[L.active];
    L.underline.y = L.bar.y + L.bar.h - theme::dp(2.0f);
    L.underline.h = theme::dp(2.0f);
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

    // ---- A MARCA (0.9.6.18 · HOTFIX D6): o tile âmbar com o "G" liso
    // lia-se como PLACEHOLDER (o dono: «o launcher mostra o ícone real»).
    // O slot vira LOCKUP: o glifo G-com-4-setas (âmbar, SEM FUNDO sobre o
    // grafite da barra) pela ÚNICA função da casa brand::drawIcon — o
    // chip de fundo MORREU (tiles com letra única proibidos como marca) —
    // e o wordmark "G.One" ao lado (some no aperto, como sempre). O toque
    // continua a não fazer nada (é a marca, não um botão). O glifo aqui é
    // ~24dp de desenho no alvo 40 — o LOD simplificado (traço ×1.2), o
    // MESMO que o launcher usa em pequeno (a paridade dos 4 sítios).
    {
        const UiRect& r = L.logo;
        // o glifo 24dp centrado no alvo 40 (sem fundo — âmbar sobre
        // grafite, nunca âmbar sobre âmbar)
        const f32 gs = theme::dp(24.0f);
        editor::brand::drawIcon(ui, r.x + (r.w - gs) * 0.5f,
                                r.y + (r.h - gs) * 0.5f, gs,
                                theme::kTheme.accent);
        if (L.showName && ui.hasFont()) {
            // o NOME ao lado do glifo (14sp text1 — some no aperto)
            const TextMetrics m = ui.textMetrics();
            ui.label(L.name.x + theme::dp(6.0f),
                     L.name.y + (L.name.h - m.block()) * 0.5f + m.ascent,
                     "G.One", theme::kTheme.text1);
        }
    }

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
        // PASSO 1: o alvo da tab é a LINHA da barra (36dp — kRowFloorDp)
        ui.auditRowFloorNext(layout::kRowFloorDp);
        const bool pressed = ui.widgetHit(tabs[i].id, r.x, r.y, r.w, r.h);
        const bool held = ui.widgetActive(tabs[i].id);
        if (held) {
            ui.panel(r.x, r.y, r.w, r.h, theme::kTheme.surface2);
        }
        const f32 s = kIconS();
        const f32 wordW = ui.hasFont() ? ui.fontWidth(tabs[i].word) : 0.0f;
        const f32 gap = theme::dp(8.0f);
        const f32 total = s + gap + wordW;
        const f32 x0 = r.x + (r.w - total) * 0.5f;
        icons::drawIcon(ui, tabs[i].icon, x0, r.y + (r.h - s) * 0.5f, s,
                        tabs[i].active ? theme::kTheme.accent
                                       : theme::kTheme.text2);
        if (ui.hasFont()) {
            const TextMetrics m = ui.textMetrics();
            // PASSO 1: a palavra da tab em CAPTION 12sp
            ui.labelStyled(x0 + s + gap,
                           r.y + (r.h - m.block()) * 0.5f + m.ascent,
                           tabs[i].word,
                           tabs[i].active ? theme::kTheme.accent
                                          : theme::kTheme.text2,
                           theme::fontScale(theme::kFontCaption), 0);
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

    if (iconButton(ui, kTbPlayId, L.play, icons::Icon::Play)) {
        a.playPressed = true;
    }
    if (iconButton(ui, kTbPauseId, L.pause, icons::Icon::Pause)) {
        a.pausePressed = true;
    }
    // 0.9.6.10 (GRUPO UI · a imagem 1): o STOP explícito — o ■ ao lado do
    // play/pause (o main sai do modo play; em editor fica esbatido — o
    // estado é o mesmo do play/pause defensivo de sempre)
    if (iconButton(ui, kTbStopId, L.stop, icons::Icon::Stop)) {
        a.stopPressed = true;
    }
    // o CHIP da plataforma (a imagem 1: o seletor): o alvo REAL da build —
    // "Android" (o toque informa; NÃO há alvos falsos para escolher — a
    // lista honesta do relatório diz exactamente isto)
    if (L.showPlatform) {
        const UiRect& r = L.platform;
        const bool held = ui.widgetActive(kTbPlatformId);
        ui.panelRounded(r.x, r.y, r.w, r.h, theme::dp(theme::kRadiusField),
                        held ? theme::kTheme.surface2 : theme::kTheme.surface);
        ui.frameRounded(r.x, r.y, r.w, r.h, 1.0f,
                        theme::dp(theme::kRadiusField), theme::kTheme.border);
        if (ui.hasFont()) {
            const TextMetrics m = ui.textMetrics();
            const f32 tw = ui.fontWidth("Android");
            const f32 gap = theme::dp(4.0f);
            const f32 total = tw + gap + theme::dp(8.0f);
            const f32 x0 = r.x + (r.w - total) * 0.5f;
            // PASSO 1: o chip em CAPTION 12sp
            ui.labelStyled(x0, r.y + (r.h - m.block()) * 0.5f + m.ascent,
                           "Android", theme::kTheme.text2,
                           theme::fontScale(theme::kFontCaption), 0);
            // o caret ▾ do chip
            const f32 cy = r.y + r.h * 0.5f;
            const f32 cx = x0 + tw + gap + theme::dp(4.0f);
            const f32 col[4] = {theme::kTheme.text2[0], theme::kTheme.text2[1],
                                theme::kTheme.text2[2], 1.0f};
            ui.drawLine(cx - theme::dp(3.0f), cy - theme::dp(2.0f), cx,
                        cy + theme::dp(2.0f), theme::dp(2.0f), col);
            ui.drawLine(cx, cy + theme::dp(2.0f), cx + theme::dp(3.0f),
                        cy - theme::dp(2.0f), theme::dp(2.0f), col);
        }
        // PASSO 1: o chip também é da LINHA (36dp — kRowFloorDp)
        ui.auditRowFloorNext(layout::kRowFloorDp);
        if (ui.widgetHit(kTbPlatformId, r.x, r.y, r.w, r.h)) {
            a.platformPressed = true;
        }
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
    a.stopPressed  = t.stopPressed;
    a.platformPressed = t.platformPressed;
    a.gearPressed  = t.gearPressed;
    a.modeChanged  = t.modeChanged;
    return a;
}

} // namespace toolbar
} // namespace editor
} // namespace vv
