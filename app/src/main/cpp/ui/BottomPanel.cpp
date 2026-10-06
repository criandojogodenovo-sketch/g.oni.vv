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

// 0.9.6.10 (GRUPO UI): a capacidade da árvore res:// e o TINT branco das
// miniaturas (a textura desenha 1:1)
constexpr u32 kTreeCap = 8;
constexpr f32 kWhiteTint[4] = {1.0f, 1.0f, 1.0f, 1.0f};

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
    // drawerH: clamp 160..400dp, passos de 8dp (spec E) — 0.9.6.1: em dp
    // REAL (R-018: os limites eram px crus)
    // GRUPO D (0.9.6.7 — o orçamento vertical): o drawer NUNCA come o
    // editor INTEIRO — deixa sempre a faixa da toolbar do viewport viva
    // (kBottomH + margens). No harness (568dp de viewport) o clamp
    // histórico (400) continua a mandar; no DEVICE (208dp de viewport) o
    // drawer default de 240dp comia TUDO (painéis a zero) — agora cede.
    f32 d = st.drawerH;
    if (d < theme::dp(safe::kDrawerMin)) {
        d = theme::dp(safe::kDrawerMin);
    }
    if (d > theme::dp(safe::kDrawerMax)) {
        d = theme::dp(safe::kDrawerMax);
    }
    const f32 vpH = safe::viewportRect(sw, sh, in).h;
    const f32 chromeFloor = theme::dp(safe::kBottomTabH) + theme::dp(16.0f);
    const f32 cap = vpH - chromeFloor;
    if (d > cap && cap > 0.0f) {
        d = cap;   // o piso da toolbar do viewport manda sobre o drawer
    }
    d = std::floor(d / theme::dp(8.0f)) * theme::dp(8.0f);
    L.drawer = {in.left, L.tabBar.y - d, sw - in.left - in.right, d};
    L.handle = {in.left, L.drawer.y, sw - in.left - in.right, theme::dp(12.0f)};
    L.drawerTop = L.drawer.y;
    const f32 third = L.tabBar.w / 4.0f;   // 0.9.6.10: 4 tabs
    L.tab[0] = {L.tabBar.x, L.tabBar.y, third, L.tabBar.h};
    L.tab[1] = {L.tabBar.x + third, L.tabBar.y, third, L.tabBar.h};
    L.tab[2] = {L.tabBar.x + 2.0f * third, L.tabBar.y, third, L.tabBar.h};
    L.tab[3] = {L.tabBar.x + 3.0f * third, L.tabBar.y, third, L.tabBar.h};
    if (st.bottomTab > 0 && st.bottomTab <= 4) {
        L.underline = L.tab[st.bottomTab - 1];
        L.underline.y = L.tabBar.y;
        L.underline.h = 2.0f;
    }
    return L;
}

Actions draw(UiContext& ui, const InputState& in, EditorState& st,
             BottomState& bs, const AssetCatalog& catalog,
             const std::vector<std::string>& logLines, int fps, u32 ticCount,
             const FilesTree& tree) {
    (void)st;
    Actions a;
    const safe::Insets insets = ui.safeArea();
    const Layout L = layout(ui.screenWidth(), ui.screenHeight(), insets, bs);
    const TextMetrics tmText = ui.textMetrics();   // 0.9.6.10: o browser

    // ---- fundo da tab bar ----
    ui.panel(L.tabBar.x, L.tabBar.y, L.tabBar.w, L.tabBar.h, theme::kTheme.bg);
    ui.panel(L.tabBar.x, L.tabBar.y, 1.0f, L.tabBar.h, theme::kTheme.border);
    ui.panel(L.tabBar.x + L.tabBar.w - 1.0f, L.tabBar.y, 1.0f, L.tabBar.h,
             theme::kTheme.border);

    // ---- tabs (ícone + palavra; ativo = accent + underline 2dp) ----
    // 0.9.6.10 (GRUPO UI · a imagem 1): 4 tabs — Ficheiros (a árvore
    // res://) · Assets (a grelha de miniaturas) · Consola · Animação
    const struct {
        u64        id;
        icons::Icon ic;
        const char* word;
    } tabs[4] = {
        {kTabFilesId,   icons::Icon::Folder,   "Ficheiros"},
        {kTabAssetsId,  icons::Icon::Box,      "Assets"},
        {kTabConsoleId, icons::Icon::Terminal, "Consola"},
        {kTabAnimId,    icons::Icon::Clapper,  "Animação"},
    };
    for (int i = 0; i < 4; ++i) {
        const UiRect& r = L.tab[i];
        const bool active = bs.bottomTab == i + 1;
        const bool pressed = ui.widgetHit(tabs[i].id, r.x, r.y, r.w, r.h);
        if (ui.widgetActive(tabs[i].id)) {
            ui.panel(r.x, r.y, r.w, r.h, theme::kTheme.surface2);
        }
        // 0.9.6.6 (GRUPO C): ícone/gap em dp REAL (eram 24/8 px crus — a
        // invariância da 13.6 apanhou: o rótulo da tab deslocava 16px a 2.0)
        const f32 s = theme::dp(24.0f);
        const f32 wordW = ui.hasFont() ? ui.fontWidth(tabs[i].word) : 0.0f;
        const f32 gap = theme::dp(8.0f);
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
    // ---- 0.9.6.10 (GRUPO UI · a imagem 1) · TAB 1: A ÁRVORE res:// --------
    // A região esquerda-baixa da referência («Ficheiros: árvore res:// com
    // Assets/Cenas/…»): as PASTAS REAIS do projeto com contagem; o toque
    // ABRE o browser de Assets FILTRADO à pasta
    if (bs.bottomTab == 1) {
        const f32 rowH = theme::dp(48.0f);
        const TextMetrics tmT = ui.textMetrics();
        // o cabeçalho: "res://" + o nº de cenas do projeto
        ui.labelStyled(content.x + theme::dp(16.0f),
                       content.y + (rowH - tmT.block()) * 0.5f + tmT.ascent,
                       "res://", theme::kTheme.text1,
                       theme::fontScale(theme::kFontSection), 0);
        {
            char sc[48];
            std::snprintf(sc, sizeof(sc), "%u cena(s)", tree.sceneCount);
            ui.labelFitted(content.x + content.w - theme::dp(16.0f) -
                               ui.fontWidth(sc),
                           content.y + (rowH - tmT.block()) * 0.5f +
                               tmT.ascent,
                           sc, theme::kTheme.text2, theme::dp(120.0f));
        }
        ui.panel(content.x, content.y + rowH, content.w, 1.0f,
                 theme::kTheme.border);
        // as PASTAS (scroll quando não cabem)
        const f32 listTop = content.y + rowH;
        const UiRect listRegion = {content.x, listTop, content.w,
                                   content.y + content.h - listTop};
        const f32 contentH =
            static_cast<f32>(tree.n) * rowH + theme::dp(8.0f);
        ui.beginScroll(kTreeScrollId, listRegion, contentH);
        const f32 offT = ui.scrollOffset();
        f32 tyArr[kTreeCap];
        for (u32 i = 0; i < tree.n && i < kTreeCap; ++i) {
            tyArr[i] = listTop + static_cast<f32>(i) * rowH;
            const f32 ry = tyArr[i] - offT;
            if (ry + rowH < listTop || ry > listTop + listRegion.h) {
                continue;
            }
            const bool sel = bs.filesFolder == static_cast<int>(i);
            if (sel) {
                ui.panel(content.x + theme::dp(4.0f), ry,
                         content.w - theme::dp(8.0f), rowH,
                         theme::kTheme.accentDim);
                ui.panel(content.x + theme::dp(4.0f), ry, theme::dp(3.0f),
                         rowH, theme::kTheme.accent);
            } else if (ui.widgetActive(kTreeRowBase + i)) {
                ui.panel(content.x + theme::dp(4.0f), ry,
                         content.w - theme::dp(8.0f), rowH,
                         theme::kTheme.surface2);
            }
            icons::drawIcon(ui, icons::Icon::Folder,
                            content.x + theme::dp(20.0f),
                            ry + (rowH - theme::dp(24.0f)) * 0.5f,
                            theme::dp(24.0f),
                            sel ? theme::kTheme.accent
                                : theme::kTheme.text2);
            if (ui.hasFont()) {
                char lbl[96];
                std::snprintf(lbl, sizeof(lbl), "%s  (%s)",
                              tree.entries[i].label, tree.entries[i].dir);
                char cnt[24];
                std::snprintf(cnt, sizeof(cnt), "%u", tree.entries[i].count);
                ui.labelFitted(content.x + theme::dp(56.0f),
                               ry + (rowH - tmT.block()) * 0.5f + tmT.ascent,
                               lbl, theme::kTheme.text1,
                               content.w - theme::dp(56.0f) -
                                   theme::dp(64.0f));
                ui.labelFitted(content.x + content.w - theme::dp(16.0f) -
                                   ui.fontWidth(cnt),
                               ry + (rowH - tmT.block()) * 0.5f + tmT.ascent,
                               cnt, theme::kTheme.text2, theme::dp(48.0f));
            }
        }
        ui.endScroll();
        // o RE-DESPACHO do tap (o padrão da casa em scrolls)
        f32 txT, tyT;
        if (ui.scrollTap(kTreeScrollId, txT, tyT)) {
            const f32 cy = tyT + offT;
            for (u32 i = 0; i < tree.n && i < kTreeCap; ++i) {
                if (cy >= tyArr[i] && cy < tyArr[i] + rowH) {
                    bs.filesFolder = static_cast<int>(i);
                    bs.bottomTab = 2;   // abre o browser FILTRADO
                    elog::info("ui: pasta '%s' selecionada na arvore res://",
                               tree.entries[i].dir);
                    break;
                }
            }
        }
        if (tree.n == 0 && ui.hasFont()) {
            ui.labelFitted(content.x + theme::dp(16.0f), listTop + rowH,
                           "sem pastas (projeto sem ficheiros)",
                           theme::kTheme.text2, content.w - theme::dp(32.0f));
        }
    }
    // ---- 0.9.6.10 · TAB 2: O BROWSER DE ASSETS (a grelha da imagem 1) ----
    // A grelha de MINIATURAS REAIS (as texturas desenham a textura em si
    // pelo imageQuad; meshes/áudio ficam com o ícone de tipo até haver um
    // renderer de pré-visualização — a lista honesta do relatório) + o
    // filtro por pasta (o toque na árvore) + o TOGGLE de vista (grelha/
    // lista — a imagem 1)
    else if (bs.bottomTab == 2) {
        struct Card {
            const char* name;
            icons::Icon ic;
            int kind;   // applyAssetPick menuKind
            bool isTex; // miniatura REAL via imageQuad
        };
        Card cards[48];
        u32 nCards = 0;
        const std::string prefix =
            bs.filesFolder >= 0 && static_cast<u32>(bs.filesFolder) < tree.n
                ? std::string(tree.entries[bs.filesFolder].dir) + "/"
                : std::string();
        auto pushCard = [&](const std::string& m, icons::Icon ic, int kind,
                            bool isTex) {
            if (nCards >= 48) {
                return;
            }
            if (!prefix.empty() && m.compare(0, prefix.size(), prefix) != 0) {
                return;   // o FILTRO da pasta selecionada na árvore
            }
            cards[nCards++] = {m.c_str(), ic, kind, isTex};
        };
        for (const std::string& m : catalog.meshes) {
            pushCard(m, icons::Icon::Box, 1, false);
        }
        for (const std::string& t : catalog.textures) {
            pushCard(t, icons::Icon::Search, 2, true);
        }
        for (const std::string& g : catalog.audio) {
            pushCard(g, icons::Icon::Speaker, 5, false);
        }
        // a LINHA de topo: o chip da pasta (limpa o filtro) + o toggle
        // grelha/lista (a imagem 1: pesquisa + toggles de vista)
        const f32 chipY = content.y + theme::dp(4.0f);
        const f32 chipH = theme::dp(40.0f);
        if (!prefix.empty() && ui.hasFont()) {
            char fl[96];
            std::snprintf(fl, sizeof(fl), "%s x",
                          tree.entries[bs.filesFolder].label);
            const f32 cw = ui.fontWidth(fl) + theme::dp(24.0f);
            const bool held = ui.widgetActive(kTreeRowBase + 100);
            ui.panelRounded(content.x + theme::dp(16.0f), chipY, cw, chipH,
                            theme::kRadiusField,
                            held ? theme::kTheme.surface2
                                 : theme::kTheme.accentDim);
            ui.labelFitted(content.x + theme::dp(28.0f),
                           chipY + (chipH - tmText.block()) * 0.5f +
                               tmText.ascent,
                           fl, theme::kTheme.text1, cw - theme::dp(24.0f));
            if (ui.widgetHit(kTreeRowBase + 100, content.x + theme::dp(16.0f),
                             chipY, cw, chipH)) {
                bs.filesFolder = -1;   // limpa o filtro
            }
        } else if (ui.hasFont()) {
            ui.labelFitted(content.x + theme::dp(16.0f),
                           chipY + (chipH - tmText.block()) * 0.5f +
                               tmText.ascent,
                           "todos os ficheiros", theme::kTheme.text2,
                           theme::dp(200.0f));
        }
        {
            // o TOGGLE grelha/lista (à direita — a imagem 1)
            const f32 tw = theme::dp(96.0f);
            const f32 tx0 = content.x + content.w - theme::dp(16.0f) - tw;
            const bool heldV = ui.widgetActive(kAssetsViewId);
            ui.panelRounded(tx0, chipY, tw, chipH, theme::kRadiusField,
                            heldV ? theme::kTheme.surface2
                                  : theme::kTheme.surface);
            ui.frameRounded(tx0, chipY, tw, chipH, 1.0f, theme::kRadiusField,
                            theme::kTheme.border);
            if (ui.hasFont()) {
                const char* lbl = bs.assetsList ? "lista" : "grelha";
                ui.labelFitted(tx0 + (tw - ui.fontWidth(lbl)) * 0.5f,
                               chipY + (chipH - tmText.block()) * 0.5f +
                                   tmText.ascent,
                               lbl, theme::kTheme.text2, tw - theme::dp(8.0f));
            }
            if (ui.widgetHit(kAssetsViewId, tx0, chipY, tw, chipH)) {
                bs.assetsList = !bs.assetsList;
            }
        }
        ui.panel(content.x + theme::dp(16.0f),
                 chipY + chipH + theme::dp(2.0f), content.w - theme::dp(32.0f),
                 1.0f, theme::kTheme.border);
        const f32 gridTop = content.y + theme::dp(48.0f);
        const UiRect grid = {content.x, gridTop, content.w,
                             content.y + content.h - gridTop};
        if (!bs.assetsList) {
            // ---- a GRELHA (cards 96dp com MINIATURAS REAIS) ----
            const f32 cardS = theme::dp(96.0f);
            const f32 step = theme::dp(120.0f);
            const u32 perRow = static_cast<u32>(grid.w / step) > 0
                                   ? static_cast<u32>(grid.w / step)
                                   : 1u;
            const f32 rows = static_cast<f32>(
                (nCards + perRow - 1) / (perRow ? perRow : 1));
            const f32 contentH = rows * step + theme::dp(8.0f);
            ui.beginScroll(kFilesScrollId, grid, contentH);
            for (u32 c = 0; c < nCards; ++c) {
                const f32 cx = grid.x + theme::dp(16.0f) +
                               static_cast<f32>(c % perRow) * step;
                const f32 cy = grid.y + theme::dp(8.0f) +
                               static_cast<f32>(c / perRow) * step -
                               ui.scrollOffset();
                if (cy > grid.y + grid.h || cy + cardS < grid.y) {
                    continue;
                }
                const bool held = ui.widgetActive(kFileCardBase + c);
                ui.panelRounded(cx, cy, cardS, cardS, theme::kRadiusCard,
                                held ? theme::kTheme.surface2
                                     : theme::kTheme.bg);
                ui.frameRounded(cx, cy, cardS, cardS, 1.0f, theme::kRadiusCard,
                                theme::kTheme.border);
                // a MINIATURA: a textura EM SI (imageQuad com o resolver do
                // GpuAssets — o UI-QUAD de sempre); meshes/áudio ficam no
                // ícone de tipo (a pré-visualização 3D fica na lista honesta)
                const char* slash = std::strrchr(cards[c].name, '/');
                const char* fname = slash ? slash + 1 : cards[c].name;
                const f32 thumb = cardS - theme::dp(28.0f);
                const bool realTex =
                    cards[c].isTex &&
                    ui.imageQuad(cx + (cardS - thumb) * 0.5f,
                                 cy + (cardS - thumb - theme::dp(14.0f)) *
                                          0.5f,
                                 thumb, thumb, cards[c].name, kWhiteTint);
                if (!realTex) {
                    icons::drawIcon(ui, cards[c].ic,
                                    cx + (cardS - theme::dp(32.0f)) * 0.5f,
                                    cy + theme::dp(18.0f), theme::dp(32.0f),
                                    theme::kTheme.text2);
                }
                if (ui.hasFont()) {
                    ui.labelFitted(cx + theme::dp(6.0f),
                                   cy + cardS - theme::dp(12.0f), fname,
                                   theme::kTheme.text2,
                                   cardS - theme::dp(12.0f));
                }
            }
            ui.endScroll();
            f32 tx, ty;
            if (ui.scrollTap(kFilesScrollId, tx, ty)) {
                for (u32 c = 0; c < nCards; ++c) {
                    const f32 cx = grid.x + theme::dp(16.0f) +
                                   static_cast<f32>(c % perRow) * step;
                    const f32 cy = grid.y + theme::dp(8.0f) +
                                   static_cast<f32>(c / perRow) * step -
                                   ui.scrollOffsetForTest(kFilesScrollId);
                    if (tx >= cx && tx < cx + cardS && ty >= cy &&
                        ty < cy + cardS) {
                        a.filePick = static_cast<int>(c) + 1;
                        a.filePickKind = cards[c].kind;
                        break;
                    }
                }
            }
        } else {
            // ---- a LISTA (linhas compactas — o toggle da imagem 1) ----
            const f32 rowH = theme::dp(48.0f);
            const f32 contentH =
                static_cast<f32>(nCards) * rowH + theme::dp(8.0f);
            ui.beginScroll(kFilesScrollId, grid, contentH);
            f32 rowYs[48];
            for (u32 c = 0; c < nCards; ++c) {
                rowYs[c] = grid.y + static_cast<f32>(c) * rowH;
                const f32 ry = rowYs[c] - ui.scrollOffset();
                if (ry + rowH < grid.y || ry > grid.y + grid.h) {
                    continue;
                }
                const bool held = ui.widgetActive(kAssetsListRowBase + c);
                if (held) {
                    ui.panel(grid.x + theme::dp(4.0f), ry,
                             grid.w - theme::dp(8.0f), rowH,
                             theme::kTheme.surface2);
                }
                icons::drawIcon(ui, cards[c].ic, grid.x + theme::dp(16.0f),
                                ry + (rowH - theme::dp(24.0f)) * 0.5f,
                                theme::dp(24.0f), theme::kTheme.text2);
                if (ui.hasFont()) {
                    const char* slash = std::strrchr(cards[c].name, '/');
                    const char* fname = slash ? slash + 1 : cards[c].name;
                    ui.labelFitted(grid.x + theme::dp(56.0f),
                                   ry + (rowH - tmText.block()) * 0.5f +
                                       tmText.ascent,
                                   fname, theme::kTheme.text1,
                                   grid.w - theme::dp(160.0f));
                    ui.labelFitted(grid.x + grid.w - theme::dp(140.0f),
                                   ry + (rowH - tmText.block()) * 0.5f +
                                       tmText.ascent,
                                   cards[c].name, theme::kTheme.text2,
                                   theme::dp(132.0f));
                }
            }
            ui.endScroll();
            f32 tx, ty;
            if (ui.scrollTap(kFilesScrollId, tx, ty)) {
                const f32 cy = ty + ui.scrollOffsetForTest(kFilesScrollId);
                for (u32 c = 0; c < nCards; ++c) {
                    if (cy >= rowYs[c] && cy < rowYs[c] + rowH) {
                        a.filePick = static_cast<int>(c) + 1;
                        a.filePickKind = cards[c].kind;
                        break;
                    }
                }
            }
        }
        if (nCards == 0 && ui.hasFont()) {
            // vazio (spec M): ícone + convite
            icons::drawIcon(ui, icons::Icon::Folder,
                            content.x + content.w * 0.5f - theme::dp(20.0f),
                            content.y + theme::dp(64.0f), theme::dp(40.0f),
                            theme::kTheme.text2);
            ui.labelFitted(content.x + theme::dp(16.0f),
                           content.y + theme::dp(136.0f),
                           prefix.empty()
                               ? "sem ficheiros - importe no seletor do Inspector"
                               : "pasta vazia",
                           theme::kTheme.text2, content.w - theme::dp(32.0f));
        }
    }
    // ---- TAB 3: A CONSOLA (spec K — RECALIBRADA 0.9.6.10: era a tab 2) ----
    else if (bs.bottomTab == 3) {
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

    // bottomTab == 4 (Animação): a TIMELINE desenha o main (precisa do estado
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
    // 0.9.6.6 (GRUPO C): 12sp REAL (o comentário antigo DIZIA «12sp
    // (fontScale 12/14)» mas o código chamava o label() de CORPO — o
    // bloco de 29px do atlas cru numa banda de 24dp SANGRAVA o fundo do
    // ecrã: o ERRO medido do Grupo B). AGORA: labelFittedStyled a 12sp
    // com a baseline CENTRADA pelas métricas do CONTEXTO (textK incluído
    // — a 1.0 o bloco é 14px na banda de 24px; a 2.0 é 28px na de 48px) e
    // o fit de sempre (o texto nunca sai do rect).
    const f32 pad = theme::dp(8.0f);   // kSpace1 em dp (era px cru)
    ui.labelFittedStyled(
        r.x + pad,
        theme::centeredBaseline(ui.textMetrics().ascent,
                                ui.textMetrics().descent, r.y, r.h,
                                theme::kFontCaption),
        text, theme::kTheme.text2, r.w - 2.0f * pad,
        theme::fontScale(theme::kFontCaption), 0);
}

// ---- persistência (spec G) ----------------------------------------------------
std::string serializeLayout(const BottomState& bs, bool showInspector,
                            u32 inspCollapsed, f32 hierW, f32 inspW) {
    char buf[128];
    std::snprintf(buf, sizeof(buf),
                  "bottomTab=%d\ndrawerH=%d\ninspector=%d\ninspCollapsed=%u\n"
                  "hierW=%d\ninspW=%d\n",
                  bs.bottomTab, static_cast<int>(bs.drawerH),
                  showInspector ? 1 : 0, inspCollapsed,
                  static_cast<int>(hierW), static_cast<int>(inspW));
    return std::string(buf);
}

bool parseLayout(const std::string& data, BottomState& bs, bool& showInspector,
                 u32& inspCollapsed, f32* hierW, f32* inspW) {
    if (data.empty()) {
        return false;
    }
    int tab = -1, dh = -1, insp = -1, hw = -9999, iw = -9999;
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
        } else if (std::strncmp(p, "hierW=", 6) == 0) {
            hw = std::atoi(p + 6);
        } else if (std::strncmp(p, "inspW=", 6) == 0) {
            iw = std::atoi(p + 6);
        }
        p = std::strchr(p, '\n');
        if (!p) {
            break;
        }
        ++p;
    }
    if (tab < 0 || tab > 4 || dh < 0) {   // 0.9.6.10: 4 tabs
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
    // GRUPO D: larguras dos divisores (AUSENTES nos ficheiros antigos →
    // −1 = default adaptativo; valores fora do teto da casa são postos a
    // −1 também — o clamp VIVO por frame é a última defesa). Em PX como o
    // drawerH (re-clampado se a densidade/o ecrã mudarem)
    if (hierW) {
        *hierW = (hw > 0 && hw < 4096) ? static_cast<f32>(hw) : -1.0f;
    }
    if (inspW) {
        *inspW = (iw > 0 && iw < 4096) ? static_cast<f32>(iw) : -1.0f;
    }
    return true;
}

} // namespace bottom
} // namespace editor
} // namespace vv
