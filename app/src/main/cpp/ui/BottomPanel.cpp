// ui/BottomPanel.cpp — painel de baixo + tab bar 32dp (0.9.0 spec E/K ·
// PASSO 1 0.9.6.14: a status bar de 24dp FOI REMOVIDA — o «FPS · TICs»
// vive no canto direito da tab bar; a versão/commit em Settings › Sobre).
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

// 0.9.6.10 (GRUPO UI · a imagem 1): o nível como ÍNDICE — as tabs da
// consola (1=info/Logs · 2=erros · 3=avisos)
int lineLevel(const std::string& ln) {
    if (lineColor(ln) == theme::kTheme.danger) {
        return 2;
    }
    if (lineColor(ln) == theme::kTheme.warn) {
        return 3;
    }
    return 1;
}

} // namespace

// 0.9.6.18 (HOTFIX D9): a versão para o MAIN (a tab Animação sem
// timeline) — o ícone é o da casa (Claquette), o caminho é o MESMO
// helper centrado+clipado das outras tabs (o drawEmptyState vive no
// namespace anónimo; ESTA é a porta com linkage externo — o header decla)
void drawDrawerEmptyState(UiContext& ui, const UiRect& rect,
                          const char* label) {
    drawEmptyState(ui, rect, icons::Icon::Clapper, label);
}

// ---- 0.9.6.18 (HOTFIX D3+D9) — O EMPTY-STATE DAS TABS DO DRAWER ----------
// O dono: «Label centrado no rect de conteúdo do drawer e CLIPADO a ele
// (o mesmo clip da lista). Nunca cruza o limite inferior nem a tab bar»
// (D3) e «estende às 4 tabs: ícone dimensionado para caber, nada desenha
// fora do drawer» (D9). O helper é o ÚNICO caminho: centrado V/H no rect,
// ScopedClip no rect (o MESMO clip da lista), ícone dimensionado ao rect.
void drawEmptyState(UiContext& ui, const UiRect& rect, icons::Icon ic,
                    const char* label) {
    if (rect.w <= 8.0f || rect.h <= 8.0f) {
        return;   // rect degenerado (drawer mínimo) — nada a desenhar
    }
    UiContext::ScopedClip clip(ui, rect);
    const TextMetrics m = ui.textMetrics();
    // o ícone CABE no rect (nunca clipado pela tab bar — o defeito D9)
    const f32 icS = std::min(theme::dp(28.0f), rect.h * 0.35f);
    const f32 gap = theme::dp(8.0f);
    const f32 block = (ui.hasFont() ? m.block() : theme::dp(14.0f)) + gap + icS;
    const f32 cy = rect.y + (rect.h - block) * 0.5f;
    icons::drawIcon(ui, ic, rect.x + (rect.w - icS) * 0.5f, cy, icS,
                    theme::kTheme.text2);
    if (ui.hasFont() && label && label[0]) {
        const f32 maxW = rect.w - theme::dp(16.0f);
        const f32 tw = ui.fontWidth(label);
        // a TRAVA (0.9.6.18 · D3): o fundo do texto NUNCA passa o fundo do
        // rect — a métrica pode divergir da escala do registo (a lição
        // R-020); o clip é a lei e a trava é a segunda defesa
        const f32 baseRaw = cy + icS + gap + m.ascent;
        const f32 base = baseRaw < rect.y + rect.h - theme::dp(2.0f) - m.descent
                             ? baseRaw
                             : rect.y + rect.h - theme::dp(2.0f) - m.descent;
        if (tw <= maxW) {
            // centrado no rect (o defeito D3 era a label desalinhada a
            // cruzar o limite inferior)
            ui.label(rect.x + (rect.w - tw) * 0.5f, base, label,
                     theme::kTheme.text2);
        } else {
            ui.labelFitted(rect.x + theme::dp(8.0f), base, label,
                           theme::kTheme.text2, maxW);
        }
    }
}

Layout layout(f32 sw, f32 sh, const safe::Insets& in, const BottomState& st) {
    Layout L;
    L.tabBar = safe::bottomTabRect(sw, sh, in);
    // P-08 (0.9.6.12 · GRUPO J1 · R-022): a altura efetiva vem da FONTE
    // ÚNICA (safe::effectiveDrawerH) — o MESMO valor que o main usa para os
    // rects do centro (currentDrawerH). Antes o cap vivia AQUI e o main lia
    // o drawer CRU: no device o viewRect colapsava a 0 e a transform
    // toolbar desenhava-se por cima da top bar (os defeitos 1+2 do dono).
    const f32 vpH = safe::viewportRect(sw, sh, in).h;
    const f32 contentH = sh - in.top - in.bottom;   // PASSO 2: o cap de 35%
    const f32 d = safe::effectiveDrawerH(st.drawerH, vpH, contentH);
    L.drawer = {in.left, L.tabBar.y - d, sw - in.left - in.right, d};
    L.handle = {in.left, L.drawer.y, sw - in.left - in.right,
                theme::dp(kDrawerHandleH)};   // PASSO 2: pega 24dp (era 12)
    L.drawerTop = L.drawer.y;
    // PASSO 1 (0.9.6.14): as 4 tabs vivem à ESQUERDA do canto direito
    // «FPS · TICs» (a reserva kFpsW; a status bar de 24dp morreu)
    L.fps = {L.tabBar.x + L.tabBar.w - theme::dp(kFpsW), L.tabBar.y,
             theme::dp(kFpsW), L.tabBar.h};
    const f32 tabsW = L.tabBar.w - L.fps.w;
    const f32 third = tabsW / 4.0f;
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

    // ---- tabs (ícone 20 + palavra 12sp; ativo = accent + underline 2dp) ----
    // 0.9.6.10 (GRUPO UI · a imagem 1): 4 tabs — Ficheiros (a árvore
    // res://) · Assets (a grelha de miniaturas) · Consola · Animação
    // PASSO 1 (0.9.6.14): a faixa é 32dp — o alvo é a CÉLULA inteira (o
    // piso de linha 32, LayoutDump.h); ícone 20, palavra caption 12sp
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
        ui.auditRowFloorNext(layout::kFieldFloorDp);   // linha de 32dp
        const bool pressed = ui.widgetHit(tabs[i].id, r.x, r.y, r.w, r.h);
        if (ui.widgetActive(tabs[i].id)) {
            ui.panel(r.x, r.y, r.w, r.h, theme::kTheme.surface2);
        }
        const f32 s = theme::dp(20.0f);   // PASSO 1: ícone 20 (era 24)
        const f32 wordW = ui.hasFont() ? ui.fontWidth(tabs[i].word) : 0.0f;
        const f32 gap = theme::dp(6.0f);
        const f32 x0 = r.x + (r.w - (s + gap + wordW)) * 0.5f;
        icons::drawIcon(ui, tabs[i].ic, x0, r.y + (r.h - s) * 0.5f, s,
                        active ? theme::kTheme.accent : theme::kTheme.text2);
        if (ui.hasFont()) {
            // PASSO 1: a palavra da tab em CAPTION 12sp
            ui.labelStyled(x0 + s + gap, textBaseline(ui, r), tabs[i].word,
                           active ? theme::kTheme.accent
                                  : theme::kTheme.text2,
                           theme::fontScale(theme::kFontCaption), 0);
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

    // ---- PASSO 1 (0.9.6.14): o «FPS n · TICs n» no CANTO DIREITO da
    // tab bar (a status bar de 24dp foi REMOVIDA — a spec: a faixa extra
    // sai; a versão/commit vivem em Settings › Sobre). Caption 12sp
    // text-2, right-aligned na reserva, o fit trunca honestamente.
    if (ui.hasFont()) {
        char fpsTxt[40];
        std::snprintf(fpsTxt, sizeof(fpsTxt), "FPS %d · TICs %u", fps,
                      ticCount);
        const TextMetrics tmF = ui.textMetrics();
        const f32 padF = theme::dp(8.0f);
        const f32 maxW = L.fps.w - 2.0f * padF;
        const f32 tw = ui.fontWidth(fpsTxt) *
                       theme::fontScale(theme::kFontCaption);
        const f32 xF = tw > maxW ? L.fps.x + padF
                                 : L.fps.x + L.fps.w - padF - tw;
        ui.labelStyled(xF,
                       theme::centeredBaseline(tmF.ascent, tmF.descent,
                                               L.fps.y, L.fps.h,
                                               theme::kFontCaption),
                       fpsTxt, theme::kTheme.text2,
                       theme::fontScale(theme::kFontCaption), 0);
    }

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
        // grip: 9 traços verticais centrados na pega de 24dp (PASSO 2 —
        // y+9..y+15; eram y+3..y+9 na pega de 12)
        for (int g = 0; g < 9; ++g) {
            const f32 gx = L.handle.x + L.handle.w * 0.5f - 32.0f +
                           static_cast<f32>(g) * 8.0f;
            ui.drawLine(gx, L.handle.y + 9.0f, gx, L.handle.y + 15.0f, 2.0f,
                        col);
        }
    }

    // conteúdo do drawer por tab (rect RECALCULADO — o drag pode ter mudado
    // a altura NESTE frame; a pega fica no topo do drawer novo)
    const Layout L2 = layout(ui.screenWidth(), ui.screenHeight(), insets, bs);
    const UiRect content = {L2.drawer.x, L2.drawer.y + theme::dp(kDrawerHandleH),
                            L2.drawer.w,
                            L2.drawer.h - theme::dp(kDrawerHandleH)};
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
            // 0.9.6.18 (HOTFIX D3): o empty-state centrado+clipado no rect
            // de conteúdo (a label antiga desenhava a listTop+rowH FIXO —
            // cruzava o limite inferior com o drawer curto e a linha âmbar
            // do tab atravessava-a)
            drawEmptyState(ui, content, icons::Icon::Folder,
                           "sem pastas (projeto sem ficheiros)");
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
            // 0.9.6.18 (HOTFIX D10): o texto cru «grelha»/«lista» MORREU —
            // o ESTADO vive nos ícones da casa (Grid = 4 quadrantes /
            // List = 3 linhas com pontos), o ativo em accent
            const f32 tw = theme::dp(64.0f);
            const f32 tx0 = content.x + content.w - theme::dp(16.0f) - tw;
            const bool heldV = ui.widgetActive(kAssetsViewId);
            ui.panelRounded(tx0, chipY, tw, chipH, theme::kRadiusField,
                            heldV ? theme::kTheme.surface2
                                  : theme::kTheme.surface);
            ui.frameRounded(tx0, chipY, tw, chipH, 1.0f, theme::kRadiusField,
                            theme::kTheme.border);
            // os DOIS ícones lado a lado (o toggle mostra o par — o ATIVO
            // acende; o toque alterna)
            const f32 sIc = theme::dp(20.0f);
            const f32 icY = chipY + (chipH - sIc) * 0.5f;
            icons::drawIcon(ui, icons::Icon::Grid,
                            tx0 + tw * 0.30f - sIc * 0.5f, icY, sIc,
                            !bs.assetsList ? theme::kTheme.accent
                                           : theme::kTheme.text2);
            icons::drawIcon(ui, icons::Icon::List,
                            tx0 + tw * 0.70f - sIc * 0.5f, icY, sIc,
                            bs.assetsList ? theme::kTheme.accent
                                          : theme::kTheme.text2);
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
            // 0.9.6.18 (HOTFIX D9): o empty-state CENTRADO no rect da
            // grelha e clipado a ele (a label antiga vivia a content.y+136
            // FIXO — saía do drawer por baixo da tab bar; o ícone de pasta
            // gigante 40dp clipava). O ícone dimensiona-se ao rect.
            drawEmptyState(ui, grid, icons::Icon::Folder,
                           prefix.empty()
                               ? "sem ficheiros - importe no seletor do Inspector"
                               : "pasta vazia");
        }
    }
    // ---- TAB 3: A CONSOLA (spec K — RECALIBRADA 0.9.6.10: era a tab 2) ----
    else if (bs.bottomTab == 3) {
        // ---- CONSOLA (spec K · 0.9.6.10 GRUPO UI — a imagem 1): as chips
        // INTERNAS Logs/Erros/Avisos — PASSO 2 (0.9.6.15): de CABEÇALHO 28dp
        // (era 36; o piso kHeadFloorDp, flagado antes do hit) e a chip
        // «Consola» SAIU (duplicava o nome da TAB do dock — spec do dono);
        // a vista COMPLETA do log é a chip «Logs» (filtro −1), Erros/Avisos
        // filtram por nível (2/3) — cores por severidade (erro=danger ·
        // aviso=warn LARANJA · info=text2) + o CAMPO DE COMANDO com o botão
        // enviar (os comandos REAIS: limpar/ajuda/play/stop/snap — o main
        // corre)
        {
            const f32 tabY = content.y + theme::dp(kConChipPad);
            const f32 tabH = theme::dp(kConChipH);
            static const struct {
                const char* label;
                int filter;   // -1 = tudo (Logs — o log inteiro)
            } kConTabs[3] = {
                {"Logs", -1}, {"Erros", 2}, {"Avisos", 3},
            };
            f32 tx = content.x + theme::dp(16.0f);
            for (int i = 0; i < 3; ++i) {
                const f32 tw = ui.hasFont()
                                   ? ui.fontWidth(kConTabs[i].label) +
                                         theme::dp(24.0f)
                                   : theme::dp(88.0f);
                const bool active = bs.consoleTab == i;
                if (active) {
                    ui.panelRounded(tx, tabY, tw, tabH,
                                    theme::kRadiusField,
                                    theme::kTheme.accentDim);
                } else if (ui.widgetActive(kConsoleTabBase +
                                           static_cast<u64>(i))) {
                    ui.panelRounded(tx, tabY, tw, tabH,
                                    theme::kRadiusField,
                                    theme::kTheme.surface2);
                }
                if (ui.hasFont()) {
                    ui.labelFitted(
                        tx + theme::dp(12.0f),
                        tabY + (tabH - tmText.block()) * 0.5f + tmText.ascent,
                        kConTabs[i].label,
                        active ? theme::kTheme.text1 : theme::kTheme.text2,
                        tw - theme::dp(16.0f));
                }
                // PASSO 2: a flag ANTES do hit — chip de CABEÇALHO 28dp
                ui.auditRowFloorNext(layout::kHeadFloorDp);
                if (ui.widgetHit(kConsoleTabBase + static_cast<u64>(i), tx,
                                 tabY, tw, tabH)) {
                    bs.consoleTab = i;   // a chip da consola
                }
                tx += tw + theme::dp(8.0f);
            }
            // auto-scroll + Export (à direita — os de sempre; PASSO 2:
            // a MESMA linha de cabeçalho 28dp — flag kHeadFloorDp)
            {
                const f32 aw = theme::dp(88.0f);
                const f32 ax0 = content.x + content.w - theme::dp(16.0f) -
                                2.0f * aw - theme::dp(8.0f);
                ui.auditRowFloorNext(layout::kHeadFloorDp);
                if (ui.widgetHit(kAutoScrollId, ax0, tabY, aw, tabH)) {
                    bs.consoleAutoScroll = !bs.consoleAutoScroll;
                }
                if (ui.hasFont()) {
                    const char* al = bs.consoleAutoScroll ? "auto: sim"
                                                          : "auto: não";
                    ui.labelFitted(ax0 + (aw - ui.fontWidth(al)) * 0.5f,
                                   tabY + (tabH - tmText.block()) * 0.5f +
                                       tmText.ascent,
                                   al, theme::kTheme.text2, aw);
                }
                ui.auditRowFloorNext(layout::kHeadFloorDp);
                if (ui.widgetHit(kExportId, ax0 + aw + theme::dp(8.0f), tabY,
                                 aw, tabH)) {
                    a.exportPressed = true;
                }
                if (ui.hasFont()) {
                    ui.labelFitted(
                        ax0 + aw + theme::dp(8.0f) +
                            (aw - ui.fontWidth("export")) * 0.5f,
                        tabY + (tabH - tmText.block()) * 0.5f + tmText.ascent,
                        "export", theme::kTheme.text1, aw);
                }
            }
            ui.panel(content.x + theme::dp(16.0f),
                     tabY + tabH + theme::dp(1.0f),
                     content.w - theme::dp(32.0f), 1.0f,
                     theme::kTheme.border);
            // ---- o CAMPO DE COMANDO (a imagem 1: «Digite um comando…» +
            // enviar) — o toque abre o teclado da casa (propósito 9); o
            // main CORRE os comandos reais (limpar/ajuda/play/stop/snap)
            // P2-bis (0.9.6.16 — a decisão do dono): SÓ existe quando a
            // lista com ele ainda cumpre o piso de 60% (conCmdVisible — a
            // fonte ÚNICA); no drawer pequeno o campo SAI — chips 28dp + a
            // lista no resto (sem exceções — a regra da consola)
            const bool conShowCmd = conCmdVisible(content.h /
                                                  theme::dp(1.0f));
            const f32 cmdH = theme::dp(kConCmdH);
            const f32 cmdY = content.y + content.h - cmdH -
                             theme::dp(kConCmdPad);
            if (conShowCmd) {
                const UiRect cmdR = {content.x + theme::dp(16.0f), cmdY,
                                     content.w - theme::dp(32.0f) -
                                         theme::dp(56.0f) - theme::dp(8.0f),
                                     cmdH};
                ui.panelRounded(cmdR.x, cmdR.y, cmdR.w, cmdR.h,
                                theme::kRadiusField, theme::kTheme.bg);
                ui.frameRounded(cmdR.x, cmdR.y, cmdR.w, cmdR.h, 1.0f,
                                theme::kRadiusField, theme::kTheme.border);
                icons::drawIcon(ui, icons::Icon::Terminal,
                                cmdR.x + theme::dp(10.0f),
                                cmdR.y + (cmdR.h - theme::dp(20.0f)) * 0.5f,
                                theme::dp(20.0f), theme::kTheme.text2);
                if (ui.hasFont()) {
                    ui.labelFitted(
                        cmdR.x + theme::dp(40.0f),
                        cmdR.y + (cmdR.h - tmText.block()) * 0.5f + tmText.ascent,
                        "Digite um comando… (limpar/ajuda/play/stop/snap)",
                        theme::kTheme.text2, cmdR.w - theme::dp(48.0f));
                }
                // o botão ENVIAR (▶ — a imagem 1)
                const UiRect sendR = {cmdR.x + cmdR.w + theme::dp(8.0f), cmdY,
                                      theme::dp(56.0f), cmdH};
                const bool sendHeld = ui.widgetActive(kCmdFieldId);
                ui.panelRounded(sendR.x, sendR.y, sendR.w, sendR.h,
                                theme::kRadiusField,
                                sendHeld ? theme::kTheme.surface2
                                         : theme::kTheme.accentDim);
                icons::drawIcon(ui, icons::Icon::Play,
                                sendR.x + (sendR.w - theme::dp(20.0f)) * 0.5f,
                                sendR.y + (sendR.h - theme::dp(20.0f)) * 0.5f,
                                theme::dp(20.0f), theme::kTheme.accent);
                if (ui.widgetHit(kCmdFieldId, cmdR.x, cmdR.y, cmdR.w, cmdR.h) ||
                    ui.widgetHit(kCmdFieldId, sendR.x, sendR.y, sendR.w,
                                 sendR.h)) {
                    a.commandPressed = true;   // o main abre o teclado (prop. 9)
                }
            }   // fim do conShowCmd (P2-bis — o campo cede à regra dos 60%)
            // as linhas (12sp mono; timestamp + cor por severidade — o
            // formato do engine.log já traz "MM-DD HH:MM:SS.mmm I/GONI:")
            // P2-bis: a região da lista é ≥60% do conteúdo (a FONTE da
            // regra: kConChipPad + chips 28 + kConListGap no topo; o campo
            // de comando (40+4) SÓ no fundo quando conCmdVisible)
            const f32 listTop = tabY + tabH + theme::dp(kConListGap);
            const f32 listBot = conShowCmd
                                    ? cmdY - theme::dp(kConCmdPad)
                                    : content.y + content.h;
            const UiRect listRegion = {
                content.x, listTop, content.w,
                listBot > listTop ? listBot - listTop : 0.0f};
            const TextMetrics m = ui.textMetrics();
            const f32 rowH = m.block() + theme::dp(4.0f);
            // PASSO 2: o filtro vem da TABELA das chips (Logs=-1 tudo ·
            // Erros=2 · Avisos=3) — o índice deixou de SER o nível quando a
            // chip «Consola» saiu
            const int wantLevel = (bs.consoleTab >= 0 && bs.consoleTab < 3)
                                      ? kConTabs[bs.consoleTab].filter
                                      : -1;
            u32 shown = 0;
            for (const std::string& ln : logLines) {
                if (wantLevel < 0 || lineLevel(ln) == wantLevel) {
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
                if (wantLevel >= 0 && lineLevel(ln) != wantLevel) {
                    continue;
                }
                const f32 ly = listTop + static_cast<f32>(idx) * rowH - off;
                ++idx;
                if (ly > listTop + listRegion.h || ly + rowH < listTop) {
                    continue;
                }
                const bool exp =
                    bs.consoleExpanded == static_cast<int>(idx - 1);
                if (exp) {
                    ui.panel(content.x, ly, content.w, rowH,
                             theme::kTheme.surface2);
                }
                ui.labelFitted(content.x + theme::dp(8.0f),
                               ly + m.ascent + theme::dp(2.0f), ln.c_str(),
                               lineColor(ln), content.w - theme::dp(16.0f));
            }
            ui.endScroll();
            f32 tx2, ty2;
            if (ui.scrollTap(kConsoleScrollId, tx2, ty2)) {
                // toque EXPANDE a linha (spec K): a linha inteira
                const i32 row = static_cast<i32>((ty2 - listTop + off) / rowH);
                bs.consoleExpanded = (row == bs.consoleExpanded) ? -1 : row;
            }
            if (shown == 0 && ui.hasFont()) {
                // 0.9.6.18 (HOTFIX D9): o empty-state da consola também é
                // CENTRADO+CLIPADO na região da lista (a label antiga
                // desenhava a listTop+rowH FIXO — com a lista curta
                // flutuava no topo, fora do centro do drawer)
                drawEmptyState(ui, listRegion, icons::Icon::Terminal,
                               bs.consoleTab == 1
                                   ? "sem erros"
                                   : (bs.consoleTab == 2 ? "sem avisos"
                                                         : "(vazio)"));
            }
        }
    }

    // bottomTab == 4 (Animação): a TIMELINE desenha o main (precisa do estado
    // da timeline/cena — ver o hook no main; o rect do drawer é passado lá)
    return a;
}


// ---- persistência (spec G) ----------------------------------------------------
std::string serializeLayout(const BottomState& bs, bool showInspector,
                            u32 inspCollapsed, f32 hierW, f32 inspW,
                            bool inspPinned) {
    char buf[144];
    std::snprintf(buf, sizeof(buf),
                  "bottomTab=%d\ndrawerH=%d\ninspector=%d\ninspCollapsed=%u\n"
                  "hierW=%d\ninspW=%d\ninspPinned=%d\n",
                  bs.bottomTab, static_cast<int>(bs.drawerH),
                  showInspector ? 1 : 0, inspCollapsed,
                  static_cast<int>(hierW), static_cast<int>(inspW),
                  inspPinned ? 1 : 0);
    return std::string(buf);
}

bool parseLayout(const std::string& data, BottomState& bs, bool& showInspector,
                 u32& inspCollapsed, f32* hierW, f32* inspW, bool* inspPinned) {
    if (data.empty()) {
        return false;
    }
    int tab = -1, dh = -1, insp = -1, hw = -9999, iw = -9999, pin = 0;
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
        } else if (std::strncmp(p, "inspPinned=", 11) == 0) {
            pin = std::atoi(p + 11);   // P2-bis: o pin do inspector
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
    // P2-bis: o pin (AUSENTE nos ficheiros antigos → false — o formato é
    // retrocompatível; só o 1 explícito fixa)
    if (inspPinned) {
        *inspPinned = pin == 1;
    }
    return true;
}

} // namespace bottom
} // namespace editor
} // namespace vv
