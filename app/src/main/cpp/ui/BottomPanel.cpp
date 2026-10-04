// ui/BottomPanel.cpp — painel de baixo + status bar 24dp (0.9.0, spec E/K).
//
// Tema: theme::kTheme (tabela spec A) — tabs ativas com texto accent +
// underline 2dp, drawer surface, pega com traços (grip), consola mono 12sp
// colorida por nível (parse do prefixo do engine.log: I/W/E), cards 96dp
// com ícone de tipo (cubo/lupa→imageQuad/speaker/pasta p/ cenas).
#include "ui/BottomPanel.h"
#include "ui/EditorUi.h"
#include "ui/Timeline.h"
#include "ui/UiContext.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace vv {
namespace editor {
namespace bottom {

namespace {

f32 textBaseline(UiContext& ui, const UiRect& r) {
    if (!ui.hasFont()) {
        return r.y + r.h * 0.5f;
    }
    const TextMetrics m = ui.textMetrics();
    return r.y + (r.h - m.block()) * 0.5f + m.ascent;
}

// nível de uma linha do engine.log: prefixo "… I/" W/ E/ (o formato de
// sempre) — I=text-2, W=warn, E=danger (spec K)
const f32* lineColor(const std::string& ln) {
    if (ln.find(" E/") != std::string::npos ||
        ln.find("ERROR") != std::string::npos) {
        return theme::kTheme.danger;
    }
    if (ln.find(" W/") != std::string::npos ||
        ln.find("WARN") != std::string::npos) {
        return theme::kTheme.warn;
    }
    return theme::kTheme.text2;
}

// nível da linha entra no filtro [erros]?
bool isErrorLine(const std::string& ln) {
    return lineColor(ln) == theme::kTheme.danger;
}

} // namespace

Layout layout(f32 sw, f32 sh, const safe::Insets& in, const BottomState& st) {
    Layout L;
    L.tabBar = safe::bottomTabRect(sw, sh, in);
    L.status = safe::statusRect(sw, sh, in);
    // drawerH: clamp 160..400, passos de 8 (spec E)
    f32 d = st.drawerH;
    if (d < safe::kDrawerMin) {
        d = safe::kDrawerMin;
    }
    if (d > safe::kDrawerMax) {
        d = safe::kDrawerMax;
    }
    d = std::floor(d / 8.0f) * 8.0f;
    L.drawer = {in.left, L.tabBar.y - d, sw - in.left - in.right, d};
    L.handle = {in.left, L.drawer.y, sw - in.left - in.right, 12.0f};
    L.drawerTop = L.drawer.y;
    const f32 third = L.tabBar.w / 3.0f;
    L.tab[0] = {L.tabBar.x, L.tabBar.y, third, L.tabBar.h};
    L.tab[1] = {L.tabBar.x + third, L.tabBar.y, third, L.tabBar.h};
    L.tab[2] = {L.tabBar.x + 2.0f * third, L.tabBar.y, third, L.tabBar.h};
    if (st.bottomTab > 0 && st.bottomTab <= 3) {
        L.underline = L.tab[st.bottomTab - 1];
        L.underline.y = L.tabBar.y;
        L.underline.h = 2.0f;
    }
    return L;
}

Actions draw(UiContext& ui, const InputState& in, EditorState& st,
             BottomState& bs, const AssetCatalog& catalog,
             const std::vector<std::string>& logLines, int fps, u32 ticCount) {
    (void)st;
    Actions a;
    const safe::Insets insets = ui.safeArea();
    const Layout L = layout(ui.screenWidth(), ui.screenHeight(), insets, bs);

    // ---- fundo da tab bar ----
    ui.panel(L.tabBar.x, L.tabBar.y, L.tabBar.w, L.tabBar.h, theme::kTheme.bg);
    ui.panel(L.tabBar.x, L.tabBar.y, 1.0f, L.tabBar.h, theme::kTheme.border);
    ui.panel(L.tabBar.x + L.tabBar.w - 1.0f, L.tabBar.y, 1.0f, L.tabBar.h,
             theme::kTheme.border);

    // ---- tabs (ícone + palavra; ativo = accent + underline 2dp) ----
    const struct {
        u64        id;
        icons::Icon ic;
        const char* word;
    } tabs[3] = {
        {kTabFilesId,   icons::Icon::Folder,   "Ficheiros"},
        {kTabConsoleId, icons::Icon::Terminal, "Consola"},
        {kTabAnimId,    icons::Icon::Clapper,  "Animação"},
    };
    for (int i = 0; i < 3; ++i) {
        const UiRect& r = L.tab[i];
        const bool active = bs.bottomTab == i + 1;
        const bool pressed = ui.widgetHit(tabs[i].id, r.x, r.y, r.w, r.h);
        if (ui.widgetActive(tabs[i].id)) {
            ui.panel(r.x, r.y, r.w, r.h, theme::kTheme.surface2);
        }
        const f32 s = 24.0f;
        const f32 wordW = ui.hasFont() ? ui.fontWidth(tabs[i].word) : 0.0f;
        const f32 gap = 8.0f;
        const f32 x0 = r.x + (r.w - (s + gap + wordW)) * 0.5f;
        icons::drawIcon(ui, tabs[i].ic, x0, r.y + (r.h - s) * 0.5f, s,
                        active ? theme::kTheme.accent : theme::kTheme.text2);
        if (ui.hasFont()) {
            ui.label(x0 + s + gap, textBaseline(ui, r), tabs[i].word,
                     active ? theme::kTheme.accent : theme::kTheme.text2);
        }
        if (pressed) {
            // tocar na tab ATIVA fecha o drawer (toggle — spec E)
            bs.bottomTab = active ? 0 : (i + 1);
        }
    }
    if (bs.bottomTab > 0) {
        ui.panel(L.underline.x, L.underline.y, L.underline.w, L.underline.h,
                 theme::kTheme.accent);
    }

    // ---- status bar 24dp (SEM abreviaturas — spec E) ----
    drawStatusBar(ui, ui.screenWidth(), ui.screenHeight(), insets, fps,
                  ticCount);

    // ---- drawer ----
    if (bs.bottomTab == 0) {
        return a;
    }
    ui.panel(L.drawer.x, L.drawer.y, L.drawer.w, L.drawer.h,
             theme::kTheme.surface);
    ui.panel(L.drawer.x, L.drawer.y + L.drawer.h - 1.0f, L.drawer.w, 1.0f,
             theme::kTheme.border);

    // pega de arrasto (12dp com traços verticais — o grip). DRAG REAL:
    // press na pega arma; o movimento vertical do dedo redimensiona o
    // drawer AO VIVO (160..400 — passos de 8 no layout; release fixa)
    {
        f32 px = -1.0f, py = -1.0f;
        if (in.down(0)) {
            in.pos(0, px, py);
        }
        const f32 hitTop = L.handle.y - 8.0f;
        const f32 hitBot = L.handle.y + L.handle.h + 8.0f;
        if (in.pressed(0) && px >= L.handle.x && px < L.handle.x + L.handle.w &&
            py >= hitTop && py < hitBot) {
            bs.dragActive = true;
            bs.dragStartY = py;
            bs.dragBaseH = bs.drawerH;
        }
        if (bs.dragActive && in.down(0)) {
            const f32 want = bs.dragBaseH + (bs.dragStartY - py);
            bs.drawerH = want < safe::kDrawerMin ? safe::kDrawerMin
                        : want > safe::kDrawerMax ? safe::kDrawerMax
                                                  : want;
        }
        if (!in.down(0)) {
            bs.dragActive = false;
        }
        ui.panel(L.handle.x, L.handle.y, L.handle.w, L.handle.h,
                 bs.dragActive ? theme::kTheme.surface2 : theme::kTheme.bg);
        const f32 col[4] = {theme::kTheme.text2[0], theme::kTheme.text2[1],
                            theme::kTheme.text2[2], 1.0f};
        for (int g = 0; g < 9; ++g) {
            const f32 gx = L.handle.x + L.handle.w * 0.5f - 32.0f +
                           static_cast<f32>(g) * 8.0f;
            ui.drawLine(gx, L.handle.y + 3.0f, gx, L.handle.y + 9.0f, 2.0f, col);
        }
    }

    // conteúdo do drawer por tab (rect RECALCULADO — o drag pode ter mudado
    // a altura NESTE frame; a pega fica no topo do drawer novo)
    const Layout L2 = layout(ui.screenWidth(), ui.screenHeight(), insets, bs);
    const UiRect content = {L2.drawer.x, L2.drawer.y + 12.0f, L2.drawer.w,
                            L2.drawer.h - 12.0f};
    if (bs.bottomTab == 1) {
        // ---- FICHEIROS: grelha de cards 96dp (spec K) ----
        struct Card {
            const char* name;
            icons::Icon ic;
            int kind;   // applyAssetPick menuKind
        };
        Card cards[48];
        u32 nCards = 0;
        for (const std::string& m : catalog.meshes) {
            if (nCards < 48) {
                cards[nCards++] = {m.c_str(), icons::Icon::Box, 1};
            }
        }
        for (const std::string& t : catalog.textures) {
            if (nCards < 48) {
                cards[nCards++] = {t.c_str(), icons::Icon::Search, 2};
            }
        }
        for (const std::string& g : catalog.audio) {
            if (nCards < 48) {
                cards[nCards++] = {g.c_str(), icons::Icon::Speaker, 5};
            }
        }
        // grelha: cards de 96dp, 8 por linha (~88dp de passo em 1536)
        const f32 cardS = 96.0f;
        const f32 step = 120.0f;
        const u32 perRow =
            static_cast<u32>(content.w / step) > 0 ? static_cast<u32>(content.w / step)
                                                   : 1u;
        const f32 rows = static_cast<f32>((nCards + perRow - 1) / (perRow ? perRow : 1));
        const f32 contentH = rows * step + 8.0f;
        ui.beginScroll(kFilesScrollId, content, contentH);
        for (u32 c = 0; c < nCards; ++c) {
            const f32 cx = content.x + 16.0f +
                           static_cast<f32>(c % perRow) * step;
            const f32 cy = content.y + 8.0f +
                           static_cast<f32>(c / perRow) * step -
                           ui.scrollOffset();
            if (cy > content.y + content.h || cy + cardS < content.y) {
                continue;
            }
            const bool held = ui.widgetActive(kFileCardBase + c);
            ui.panelRounded(cx, cy, cardS, cardS, theme::kRadiusCard,
                            held ? theme::kTheme.surface2 : theme::kTheme.bg);
            ui.frameRounded(cx, cy, cardS, cardS, 1.0f, theme::kRadiusCard,
                            theme::kTheme.border);
            icons::drawIcon(ui, cards[c].ic, cx + (cardS - 32.0f) * 0.5f,
                            cy + 18.0f, 32.0f, theme::kTheme.text2);
            if (ui.hasFont()) {
                ui.labelFitted(cx + 6.0f, cy + cardS - 10.0f, cards[c].name,
                               theme::kTheme.text2, cardS - 12.0f);
            }
        }
        ui.endScroll();
        f32 tx, ty;
        if (ui.scrollTap(kFilesScrollId, tx, ty)) {
            for (u32 c = 0; c < nCards; ++c) {
                const f32 cx = content.x + 16.0f +
                               static_cast<f32>(c % perRow) * step;
                const f32 cy = content.y + 8.0f +
                               static_cast<f32>(c / perRow) * step -
                               ui.scrollOffsetForTest(kFilesScrollId);
                if (tx >= cx && tx < cx + cardS && ty >= cy && ty < cy + cardS) {
                    a.filePick = static_cast<int>(c) + 1;
                    a.filePickKind = cards[c].kind;
                    break;
                }
            }
        }
        if (nCards == 0 && ui.hasFont()) {
            // vazio (spec M): ícone + convite
            icons::drawIcon(ui, icons::Icon::Folder,
                            content.x + content.w * 0.5f - 20.0f,
                            content.y + 24.0f, 40.0f, theme::kTheme.text2);
            ui.labelFitted(content.x + 16.0f, content.y + 96.0f,
                           "sem ficheiros - importe no seletor do Inspector",
                           theme::kTheme.text2, content.w - 32.0f);
        }
    } else if (bs.bottomTab == 2) {
        // ---- CONSOLA (spec K): linhas coloridas + chips + auto-scroll ----
        const f32 chipY = content.y + 4.0f;
        const f32 chipH = 32.0f;
        auto chip = [&](u64 id, const char* label, bool on, f32 x) {
            const f32 w = 96.0f;
            if (on) {
                ui.panelRounded(x, chipY, w, chipH, theme::kRadiusField,
                                theme::kTheme.accent);
            } else {
                ui.panelRounded(x, chipY, w, chipH, theme::kRadiusField,
                                theme::kTheme.bg);
                ui.frameRounded(x, chipY, w, chipH, 1.0f, theme::kRadiusField,
                                theme::kTheme.border);
            }
            if (ui.hasFont()) {
                const f32 tw = ui.fontWidth(label);
                ui.label(x + (w - tw) * 0.5f, textBaseline(ui, {x, chipY, w, chipH}),
                         label, on ? theme::kTheme.accentInk : theme::kTheme.text2);
            }
            return ui.widgetHit(id, x, chipY, w, chipH);
        };
        if (chip(kChipAllId, "todos", !bs.consoleOnlyErrors, content.x + 16.0f)) {
            bs.consoleOnlyErrors = false;
        }
        if (chip(kChipErrId, "erros", bs.consoleOnlyErrors, content.x + 120.0f)) {
            bs.consoleOnlyErrors = true;
        }
        // auto-scroll toggle + Export
        if (ui.widgetHit(kAutoScrollId, content.x + 240.0f, chipY, 140.0f, chipH)) {
            bs.consoleAutoScroll = !bs.consoleAutoScroll;
        }
        if (ui.hasFont()) {
            ui.label(content.x + 248.0f, textBaseline(ui, {0, chipY, 0, chipH}),
                     bs.consoleAutoScroll ? "auto: sim" : "auto: não",
                     theme::kTheme.text2);
        }
        if (ui.widgetHit(kExportId, content.x + content.w - 112.0f, chipY, 96.0f,
                         chipH)) {
            a.exportPressed = true;
        }
        if (ui.hasFont()) {
            ui.label(content.x + content.w - 104.0f,
                     textBaseline(ui, {0, chipY, 0, chipH}), "export",
                     theme::kTheme.text1);
        }
        ui.panel(content.x + 16.0f, chipY + chipH + 2.0f, content.w - 32.0f, 1.0f,
                 theme::kTheme.border);
        // linhas (12sp mono — a fonte é a de sempre; mono por cultura)
        const f32 listTop = chipY + chipH + 8.0f;
        const UiRect listRegion = {content.x, listTop, content.w,
                                   content.y + content.h - listTop};
        const TextMetrics m = ui.textMetrics();
        const f32 rowH = m.block() + 4.0f;
        // filtro
        u32 shown = 0;
        for (const std::string& ln : logLines) {
            if (!bs.consoleOnlyErrors || isErrorLine(ln)) {
                ++shown;
            }
        }
        const f32 contentH = static_cast<f32>(shown) * rowH;
        if (bs.consoleAutoScroll) {
            ui.scrollSetOffset(kConsoleScrollId, contentH);
        }
        ui.beginScroll(kConsoleScrollId, listRegion, contentH);
        const f32 off = ui.scrollOffset();
        u32 idx = 0;
        for (const std::string& ln : logLines) {
            if (bs.consoleOnlyErrors && !isErrorLine(ln)) {
                continue;
            }
            const f32 ly = listTop + static_cast<f32>(idx) * rowH - off;
            ++idx;
            if (ly > listTop + listRegion.h || ly + rowH < listTop) {
                continue;
            }
            const bool exp = bs.consoleExpanded == static_cast<int>(idx - 1);
            if (exp) {
                ui.panel(content.x, ly, content.w, rowH, theme::kTheme.surface2);
            }
            ui.labelFitted(content.x + 8.0f, ly + m.ascent + 2.0f, ln.c_str(),
                           lineColor(ln), content.w - 16.0f);
        }
        ui.endScroll();
        f32 tx, ty;
        if (ui.scrollTap(kConsoleScrollId, tx, ty)) {
            // toque EXPANDE a linha (spec K): a linha inteira, texto completo
            const i32 row = static_cast<i32>((ty - listTop + off) / rowH);
            bs.consoleExpanded = (row == bs.consoleExpanded) ? -1 : row;
        }
        if (shown == 0 && ui.hasFont()) {
            ui.labelFitted(content.x + 16.0f, listTop + rowH,
                           bs.consoleOnlyErrors ? "sem erros" : "(vazio)",
                           theme::kTheme.text2, content.w - 32.0f);
        }
    }
    // bottomTab == 3 (Animação): a TIMELINE desenha o main (precisa do estado
    // da timeline/cena — ver o hook no main; o rect do drawer é passado lá)
    return a;
}

void drawStatusBar(UiContext& ui, f32 sw, f32 sh, const safe::Insets& in,
                   int fps, u32 ticCount) {
    const UiRect r = safe::statusRect(sw, sh, in);
    ui.panel(r.x, r.y, r.w, r.h, theme::kTheme.bg);
    ui.panel(r.x, r.y, r.w, 1.0f, theme::kTheme.border);
    if (!ui.hasFont()) {
        return;
    }
    char text[48];
    std::snprintf(text, sizeof(text), "FPS %d · TICs %u", fps, ticCount);
    // 12sp (fontScale 12/14) text-2 — a ÚNICA linha (spec E)
    ui.label(r.x + theme::kSpace1, r.y + (r.h - ui.textMetrics().block()) * 0.5f +
                                  ui.textMetrics().ascent,
             text, theme::kTheme.text2);
}

// ---- persistência (spec G) ----------------------------------------------------
std::string serializeLayout(const BottomState& bs, bool showInspector,
                            u32 inspCollapsed) {
    char buf[96];
    std::snprintf(buf, sizeof(buf),
                  "bottomTab=%d\ndrawerH=%d\ninspector=%d\ninspCollapsed=%u\n",
                  bs.bottomTab, static_cast<int>(bs.drawerH),
                  showInspector ? 1 : 0, inspCollapsed);
    return std::string(buf);
}

bool parseLayout(const std::string& data, BottomState& bs, bool& showInspector,
                 u32& inspCollapsed) {
    if (data.empty()) {
        return false;
    }
    int tab = -1, dh = -1, insp = -1;
    unsigned collapsed = 0xFFFFFFFFu;
    const char* p = data.c_str();
    while (*p) {
        if (std::strncmp(p, "bottomTab=", 10) == 0) {
            tab = std::atoi(p + 10);
        } else if (std::strncmp(p, "drawerH=", 8) == 0) {
            dh = std::atoi(p + 8);
        } else if (std::strncmp(p, "inspector=", 10) == 0) {
            insp = std::atoi(p + 10);
        } else if (std::strncmp(p, "inspCollapsed=", 14) == 0) {
            collapsed = static_cast<unsigned>(std::strtoul(p + 14, nullptr, 10));
        }
        p = std::strchr(p, '\n');
        if (!p) {
            break;
        }
        ++p;
    }
    if (tab < 0 || tab > 3 || dh < 0) {
        return false;   // ilegível → defaults ("Repor layout")
    }
    bs.bottomTab = tab;
    if (dh >= static_cast<int>(safe::kDrawerMin) &&
        dh <= static_cast<int>(safe::kDrawerMax)) {
        bs.drawerH = static_cast<f32>(dh);
    }
    if (insp == 0 || insp == 1) {
        showInspector = insp == 1;
    }
    if (collapsed != 0xFFFFFFFFu) {
        inspCollapsed = collapsed;
    }
    return true;
}

} // namespace bottom
} // namespace editor
} // namespace vv
