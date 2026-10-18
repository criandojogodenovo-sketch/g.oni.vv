// tests/test_012c_picker.cpp — 0.12-C (C-4): O SELETOR DE FICHEIROS + OS
// ESTADOS VAZIOS.
//
// Os 7 pins C++ da sub-fase C-4 (o 8.º — empty_projetos — é o espelho
// Java: vive no gate scripts/projects_ui_check.py, o padrão da C-3):
//   1. picker_compacto   — a linha 48dp, o card dentro da safe-area, ≤8
//                          linhas visíveis e o Subir FIXO fora do scroll
//   2. picker_breadcrumb — os segmentos (rótulo da raiz + prefixos) e o
//                          SALTO pelo crumbPickOut (o toque num segmento)
//   3. picker_oculta_dot — as PASTAS OCULTAS (ponto) fora da lista (os
//                          ficheiros ponto ficam — a decisão 0.8.5 mantém)
//   4. empty_inspector   — mensagem + AÇÃO (o botão abre o plusMenu real)
//   5. empty_assets      — mensagem + botão IMPORTAR (o Actions real)
//   6. empty_hierarquia  — o convite aponta para o + e o + fica VISÍVEL
//   7. icones_unicos     — uma FUNÇÃO, um glifo (tabs, vazios, linhas do
//                          picker; a lupa deixou de ser textura)
//
// MUTAÇÕES (coladas no RELATORIO-0.12-C.md §4):
//   M-C4a — as pastas ocultas de volta (listDirEntries sem o filtro)
//           → picker_oculta_dot vermelho
//   M-C4b — o vazio dos Assets volta à PASTA (o glifo dos Ficheiros)
//           → icones_unicos vermelho
#include "TestFramework.h"
#include "ui/BottomPanel.h"
#include "ui/EditorLayout.h"
#include "ui/EditorUi.h"
#include "ui/UiContext.h"
#include "ui/UiEditor.h"
#include "platform/FileApi.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace vv;
using namespace vv::editor;
using namespace vv::editor::bottom;
using ::test::nearEqF;

namespace {

// o padrão da casa: densidade 1.0 = o layout de sempre (o harness)
struct Harness {
    FontAtlas font;
    UiContext ui;
    InputState in;

    explicit Harness(bool withFont = true) {
        theme::setDensity(1.0f);
        if (withFont) {
            const char* fontPath = FONT_FIXTURE;
            if (font.loadFromPaths(&fontPath, 1, 28.0f)) {
                ui.setFont(&font);
            }
        }
        ui.init();
        ui.setSafeArea(safe::Insets{});
    }
};

// árvore de teste em /tmp (o padrão do test_browser)
struct TempTree {
    std::string root;
    explicit TempTree(const char* sub) {
        char tmpl[] = "/tmp/goni-012c-XXXXXX";
        const char* mkd = mkdtemp(tmpl);
        root = mkd ? std::string(mkd) + "/" + sub : "/tmp/goni-012c";
        fileapi::makeDirs(root);
        fileapi::makeDirs(root + "/.thumbnails");     // a PASTA oculta
        fileapi::makeDirs(root + "/.android");        // a PASTA oculta 2
        fileapi::makeDirs(root + "/Download");
        fileapi::writeAll(root + "/modelo.obj", "o 1", 3);
        fileapi::writeAll(root + "/.nomedia", "x", 1);   // o FICHEIRO ponto
    }
    ~TempTree() {
        std::remove((root + "/modelo.obj").c_str());
        std::remove((root + "/.nomedia").c_str());
        std::remove((root + "/.thumbnails").c_str());
        std::remove((root + "/.android").c_str());
        std::remove((root + "/Download").c_str());
        std::remove(root.c_str());
        std::remove(root.substr(0, root.size() - 6).c_str());
    }
};

} // namespace

// ---- 1. picker_compacto -----------------------------------------------------

TEST(012c_picker_compacto) {
    theme::setDensity(1.0f);
    // a LINHA do dono: 48dp em qualquer densidade
    {
        theme::setDensity(2.0f);
        const BrowserSheetLayout ld = browserSheetLayout(
            1600.0f, 720.0f, 0.0f, 48.0f, 0.0f, 48.0f, editor::kHeaderH, 30u);
        EXPECT(nearEqF(ld.rowH, theme::dp(48.0f), 0.01f));
        theme::setDensity(1.0f);
    }
    theme::setDensity(1.0f);
    // o card DENTRO da safe-area (a invariante F4.2) nos 3 ecrãs de
    // referência e com MUITAS linhas (o ≤8 visíveis — o picker COMPACTO)
    struct Screen { f32 sw, sh, sl, st, sr, sb; };
    const Screen scr[] = {
        {1600.0f, 720.0f, 0.0f, 24.0f, 0.0f, 24.0f},   // landscape + nav
        {776.0f, 720.0f, 0.0f, 24.0f, 0.0f, 24.0f},    // device estreito
        {720.0f, 1280.0f, 0.0f, 24.0f, 0.0f, 24.0f},   // retrato
    };
    for (const Screen& s : scr) {
        const BrowserSheetLayout L = browserSheetLayout(
            s.sw, s.sh, s.sl, s.st, s.sr, s.sb, editor::kHeaderH, 50u);
        // dentro da ÁREA ÚTIL (a safe-area respeitada em TODO o lado)
        EXPECT(L.x >= s.sl - 0.01f && L.y >= s.st - 0.01f);
        EXPECT(L.x + L.w <= s.sw - s.sr + 0.01f);
        EXPECT(L.y + L.h <= s.sh - s.sb + 0.01f);
        // ≤8 linhas VISÍVEIS (a lista encolhe; o scroll revela o resto)
        const f32 listH = L.y + L.h - theme::dp(8.0f) - L.listTop;
        EXPECT(listH <= 8.0f * L.rowH + 0.01f);
        // a ORDEM vertical: breadcrumb → pesquisa → raízes → subir → lista
        EXPECT(L.crumbY > L.y && L.searchY > L.crumbY &&
               L.rootsY > L.searchY && L.upY > L.rootsY &&
               L.listTop > L.upY);
        // o SUBIR FIXO: a linha inteira dele vive FORA do scroll (antes do
        // listTop — a regra do dono; o scroll come no listTop)
        EXPECT(L.upY + L.rowH <= L.listTop + 0.01f);
        // o campo de pesquisa cabe (40dp) e dentro do card
        EXPECT(L.searchH >= theme::dp(40.0f) - 0.01f);
    }
}

// ---- 2. picker_breadcrumb ---------------------------------------------------

TEST(012c_picker_breadcrumb) {
    theme::setDensity(1.0f);
    // (a) os segmentos: o 1.º é o RÓTULO da raiz casada; os restantes os
    // componentes; TODO path é PREFIXO do cwd (o salto nunca sai do sítio)
    const std::vector<BrowserCrumb> cs =
        browserCrumbs("/storage/emulated/0/Download/sub");
    EXPECT(cs.size() == 3u);
    if (cs.size() == 3u) {
        EXPECT(cs[0].label == "Raiz");
        EXPECT(cs[0].path == "/storage/emulated/0");
        EXPECT(cs[1].label == "Download");
        EXPECT(cs[1].path == "/storage/emulated/0/Download");
        EXPECT(cs[2].label == "sub");
        EXPECT(cs[2].path == "/storage/emulated/0/Download/sub");
        for (const BrowserCrumb& c : cs) {
            EXPECT(cs[2].path.compare(0, c.path.size(), c.path) == 0);
        }
    }
    // (b) o SALTO no draw: tocar no 1.º segmento (Raiz) sai pelo
    // crumbPickOut == 1 (o main despacha o browserOpen do prefixo — a
    // FONTE ÚNICA browserCrumbs desenha e o main recalcula os MESMOS pares)
    // o padrão do test_browser: o tap é down + frame + up + frame (o
    // widgetHit dispara no LARGAR — o padrão da casa)
    Harness h;
    InputState& in = h.in;
    UiContext& ui = h.ui;
    EditorState st;
    const std::vector<fileapi::DirEntry> entries = {
        {"sub", "/storage/emulated/0/Download/sub", true, 0},
    };
    const BrowserSheetLayout L =
        browserSheetLayout(1600.0f, 720.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                           editor::kHeaderH, 1u);
    st.fileBrowser = true;
    int crumbPick = 0;
    u32 rowIdx = 0;
    ui.beginFrame(nullptr, &in, 1600.0f, 720.0f);
    in.injectDown(0, L.x + theme::dp(28.0f), L.crumbY + L.crumbH * 0.5f);
    drawFileBrowser(ui, in, 1600.0f, 720.0f, st,
                    "/storage/emulated/0/Download/sub", entries, false,
                    &crumbPick, &rowIdx);
    ui.endFrame();
    in.clearEdges();
    in.injectUp(0);
    ui.beginFrame(nullptr, &in, 1600.0f, 720.0f);
    drawFileBrowser(ui, in, 1600.0f, 720.0f, st,
                    "/storage/emulated/0/Download/sub", entries, false,
                    &crumbPick, &rowIdx);
    ui.endFrame();
    in.clearEdges();
    EXPECT(crumbPick == 1);   // o segmento «Raiz» tocado
    EXPECT(rowIdx == 0);      // nenhuma linha escolhida
    EXPECT(st.fileBrowser);   // o breadcrumb NÃO fecha o picker
}

// ---- 3. picker_oculta_dot ---------------------------------------------------

TEST(012c_picker_oculta_dot) {
    TempTree t("dot");
    std::vector<fileapi::DirEntry> out;
    if (!fileapi::listDirEntries(t.root, out)) {
        EXPECT(!"opendir falhou na árvore de teste");
        return;
    }
    // as PASTAS OCULTAS (.thumbnails/.android) FORA da lista (a ordem do
    // dono na C-4) — a única diretoria visível é a normal
    u32 nDirs = 0, nDotDirs = 0;
    bool hasDotFile = false, hasObj = false;
    for (const fileapi::DirEntry& e : out) {
        if (e.isDir) {
            ++nDirs;
            if (!e.name.empty() && e.name[0] == '.') {
                ++nDotDirs;
            }
        } else {
            hasDotFile = hasDotFile || e.name == ".nomedia";
            hasObj = hasObj || e.name == "modelo.obj";
        }
    }
    EXPECT(nDirs == 1u);        // só a «Download»
    EXPECT(nDotDirs == 0u);     // ZERO pastas ocultas — o pin
    EXPECT(hasObj);             // o .obj continua
    EXPECT(hasDotFile);         // os FICHEIROS ponto ficam (0.8.5 intacta)
}

// ---- 4. empty_inspector -----------------------------------------------------

TEST(012c_empty_inspector) {
    Harness h;
    InputState& in = h.in;
    UiContext& ui = h.ui;
    Scene scene;              // VAZIA — o estado vazio do Inspector
    EditorState st;
    st.inspPinned = true;     // o painel ABERTO sem seleção (o pin P2-bis)
    AssetCatalog catalog;
    const safe::Insets inz{};
    const UiRect panel =
        safe::inspectorPanelRect(1600.0f, 720.0f, inz, 0.0f, -1.0f, -1.0f);
    // o botão «+ criar TIC»: a MESMA fórmula do draw (180×40 centrado, a
    // 24dp SOB o meio do painel)
    const f32 bw = theme::dp(180.0f), bh = theme::dp(40.0f);
    const f32 bx = panel.x + (panel.w - bw) * 0.5f;
    const f32 by = panel.y + panel.h * 0.5f + theme::dp(24.0f);

    ui.beginFrame(nullptr, &in, 1600.0f, 720.0f);
    drawInspector(ui, scene, st, &catalog, nullptr);
    ui.endFrame();
    in.clearEdges();
    EXPECT(!st.plusMenu);   // antes do toque o menu está fechado

    // o TOQUE no botão abre o MESMO plusMenu de presets do + da hierarquia
    // (a AÇÃO real — nunca um convite que manda procurar o + pelo ecrã);
    // o tap da casa: down + frame + up + frame (o widgetHit dispara no
    // LARGAR)
    st.inspPinned = true;
    ui.beginFrame(nullptr, &in, 1600.0f, 720.0f);
    in.injectDown(0, bx + bw * 0.5f, by + bh * 0.5f);
    drawInspector(ui, scene, st, &catalog, nullptr);
    ui.endFrame();
    in.clearEdges();
    in.injectUp(0);
    ui.beginFrame(nullptr, &in, 1600.0f, 720.0f);
    drawInspector(ui, scene, st, &catalog, nullptr);
    ui.endFrame();
    in.clearEdges();
    EXPECT(st.plusMenu);
}

// ---- 5. empty_assets --------------------------------------------------------

TEST(012c_empty_assets) {
    Harness h;
    InputState& in = h.in;
    UiContext& ui = h.ui;
    EditorState st;
    BottomState bs;
    bs.bottomTab = 2;   // a tab ASSETS
    AssetCatalog catalog;   // VAZIO — o estado vazio dos Assets
    const std::vector<std::string> logs;
    const FilesTree tree{};

    // a geometria do botão IMPORTAR (a MESMA fórmula do draw: 180×40
    // ANCORADO ao fundo da grelha — existe em TODA altura de drawer)
    const safe::Insets inz{};
    const bottom::Layout bl = bottom::layout(1600.0f, 720.0f, inz, bs);
    const f32 gridTop = bl.drawer.y + theme::dp(bottom::kDrawerHandleH) +
                        theme::dp(48.0f);
    const f32 gridBottom = bl.drawer.y + bl.drawer.h;
    const f32 bw = theme::dp(180.0f), bh = theme::dp(40.0f);
    const f32 bx = bl.drawer.x + (bl.drawer.w - bw) * 0.5f;
    const f32 by = gridBottom - bh - theme::dp(8.0f);

    ui.beginFrame(nullptr, &in, 1600.0f, 720.0f);
    const bottom::Actions a0 = bottom::draw(ui, in, st, bs, catalog, logs,
                                            60, 0, tree);
    ui.endFrame();
    in.clearEdges();
    EXPECT(!a0.importPressed);   // antes do toque, nada

    // o TOQUE no IMPORTAR — a ação REAL (o main abre o navegador); o tap
    // da casa: down + frame + up + frame (o widgetHit dispara no LARGAR)
    ui.beginFrame(nullptr, &in, 1600.0f, 720.0f);
    in.injectDown(0, bx + bw * 0.5f, by + bh * 0.5f);
    const bottom::Actions aD = bottom::draw(ui, in, st, bs, catalog, logs,
                                            60, 0, tree);
    ui.endFrame();
    in.clearEdges();
    in.injectUp(0);
    ui.beginFrame(nullptr, &in, 1600.0f, 720.0f);
    const bottom::Actions a1 = bottom::draw(ui, in, st, bs, catalog, logs,
                                            60, 0, tree);
    ui.endFrame();
    in.clearEdges();
    EXPECT(a1.importPressed);
}

// ---- 6. empty_hierarquia ----------------------------------------------------

TEST(012c_empty_hierarquia) {
    theme::setDensity(1.0f);
    // (a) o convite aponta para o + em TODOS os degraus da ladder (a
    // FONTE ÚNICA hierEmptyInvite — o draw e o pin partilham)
    EXPECT(std::string(hierEmptyInvite(false, true, true)) ==
           "sem TICs - toca em + para criar");
    EXPECT(std::string(hierEmptyInvite(false, false, true)) ==
           "toca em + para criar");
    EXPECT(std::string(hierEmptyInvite(false, false, false)) ==
           "+ cria um TIC");
    EXPECT(std::string(hierEmptyInvite(true, true, false)) ==
           "nenhum TIC com esse nome");
    EXPECT(std::string(hierEmptyInvite(true, false, false)) ==
           "sem resultados");
    // (b) o + fica VISÍVEL com a cena vazia: o rect dele existe (w>0) e
    // vive DENTRO do painel no piso 140dp e no harness 300dp
    for (f32 w : {140.0f, 220.0f, 300.0f}) {
        const HierHeaderRects hh = hierHeaderRects(w);
        EXPECT(hh.plus.w > 0.0f && hh.plus.h > 0.0f);
        EXPECT(hh.plus.x >= 0.0f && hh.plus.x + hh.plus.w <= w + 0.01f);
        // o + é o EXTREMO direito e o ⋮ fica imediatamente à esquerda
        EXPECT(nearEqF(hh.plus.x + hh.plus.w, w - kPad, 0.01f));
        EXPECT(nearEqF(hh.dots.x + hh.dots.w, hh.plus.x, 0.01f));
    }
}

// ---- 7. icones_unicos -------------------------------------------------------

TEST(012c_icones_unicos) {
    // (a) as 4 TABS do drawer: glifos TODOS distintos (uma função, um
    // glifo — Ficheiros ≠ Assets ≠ Consola ≠ Animação)
    const icons::Icon tabs[4] = {icons::Icon::Folder, icons::Icon::Box,
                                 icons::Icon::Terminal,
                                 icons::Icon::Clapper};
    for (int i = 0; i < 4; ++i) {
        for (int j = i + 1; j < 4; ++j) {
            EXPECT_MSG(tabs[i] != tabs[j],
                       "icones_unicos: as tabs %d e %d partilham o glifo", i,
                       j);
        }
    }
    // (b) o VAZIO usa o glifo da PRÓPRIA função (a FONTE ÚNICA
    // emptyStateIcon): o vazio dos Assets era a PASTA — o glifo dos
    // Ficheiros — a duplicação que a C-4 cura (a mutação M-C4b reverte)
    EXPECT(emptyStateIcon(1) == icons::Icon::Folder);
    EXPECT(emptyStateIcon(2) == icons::Icon::Box);
    EXPECT(emptyStateIcon(2) != emptyStateIcon(1));
    EXPECT(emptyStateIcon(4) == icons::Icon::Clapper);
    // (c) as LINHAS do picker: pasta/mesh/imagem/áudio/desconhecido —
    // glifos TODOS distintos e a LUPA FORA (a lupa voltou a ser SÓ
    // pesquisar — o campo de pesquisa da C-4)
    const icons::Icon rows[5] = {icons::Icon::Folder, icons::Icon::Box,
                                 icons::Icon::Image, icons::Icon::Speaker,
                                 icons::Icon::Question};
    for (int i = 0; i < 5; ++i) {
        for (int j = i + 1; j < 5; ++j) {
            EXPECT(rows[i] != rows[j]);
        }
        EXPECT(rows[i] != icons::Icon::Search);   // a lupa é SÓ o campo
    }
    // (d) o glifo NOVO Image existe e é ÚNICO no conjunto (o mesmo
    // contrato do test_wiring090 — os segmentos nunca se repetem)
    const icons::IconDef& di = icons::def(icons::Icon::Image);
    EXPECT(di.nPts > 0 && di.nLines > 0);
    for (u8 i = 0; i < static_cast<u8>(icons::Icon::Count); ++i) {
        const icons::Icon ic = static_cast<icons::Icon>(i);
        if (ic == icons::Icon::Image) {
            continue;
        }
        EXPECT(icons::segmentCount(ic) != icons::segmentCount(
                   icons::Icon::Image) ||
               icons::totalLength(ic) !=
                   icons::totalLength(icons::Icon::Image));
    }
    // (e) o ícone pelo NOME (a fonte única iconByName conhece o novo)
    EXPECT(icons::iconByName("imagem") ==
           static_cast<i32>(icons::Icon::Image));
}
