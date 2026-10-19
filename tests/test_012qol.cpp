// tests/test_012qol.cpp — 0.12-QoL: UI POLISH, NAVEGADOR E AUTO-SAVE.
//
// Os pins da campanha (um grupo por sub-fase; o B/C/D entram nos commits
// delas):
//   A. qol_a_topbar_so_logo   — o texto "G.One" SAIU (só o ícone do G)
//      qol_a_lupa_expande     — a pesquisa recolhe-se em lupa e EXPANDE ao toque
//      qol_a_border_cinza     — as bordas dos INATIVOS são o cinza #24242A
//      qol_a_inspector_ar     — os vãos X/Y/Z 12dp + o Reset com margem
//      qol_a_cenas_x_fecha    — o X do menu de cenas fecha (bem visível)
//
// CLÁUSULA CALMA: NADA de lógica de motor — só a pele (Theme/draw/layout).
//
// MUTAÇÕES (coladas no RELATORIO-0.12-QoL.md):
//   M-QoLa1 — o texto "G.One" de volta no drawTopBar → qol_a_topbar_so_logo
//             vermelho
//   M-QoLa2 — o border de volta ao âmbar #4A3714 → qol_a_border_cinza +
//             regress_identidade_grafite_ambar vermelhos
#include "TestFramework.h"
#include "ui/EditorUi.h"
#include "ui/EditorLayout.h"
#include "ui/Toolbar.h"
#include "ui/Theme.h"
#include "ui/SafeArea.h"
#include "render/QuadBatch.h"
#include "platform/FileApi.h"
#include "core/AutoSave.h"
#include "core/FsStorage.h"
#include "core/Project.h"
#include "core/SceneSerializer.h"
#include "ui/CamGizmo.h"
#include "ui/ViewportChrome.h"
#include "render/Camera.h"
#include "render/Grid.h"
#include "components/Transform3D.h"
#include "core/Scene.h"
#include "components/CameraComp.h"
#include "ui/UiEditor.h"

#include <cmath>
#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

using namespace vv;
using namespace vv::editor;

namespace {

bool nearEq(f32 a, f32 b, f32 eps = 0.05f) {
    return std::fabs(a - b) <= eps;
}

// o padrão do test_012c_idioma (a raiz do repo com override das mutações)
std::string repoRoot() {
    if (const char* e = std::getenv("GONI_ROOT_OVERRIDE")) return e;
#ifdef REPO_ROOT
    return REPO_ROOT;
#else
    return ".";
#endif
}

bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    out.assign(std::istreambuf_iterator<char>(f),
               std::istreambuf_iterator<char>());
    return true;
}

u32 countSub(const std::string& hay, const std::string& needle) {
    u32 n = 0;
    for (size_t p = hay.find(needle); p != std::string::npos;
         p = hay.find(needle, p + needle.size())) {
        ++n;
    }
    return n;
}

// o env do draw (o padrão CnEnv do test_012b_cenas — densidade 1)
struct QolEnv {
    FontAtlas  font;
    UiContext  ui;
    InputState input;
    EditorState st;
    Scene      scene;
    bool       ok = false;

    QolEnv() {
        const char* fontPath = FONT_FIXTURE;
        ok = font.loadFromPaths(&fontPath, 1, 28.0f);
        if (!ok) {
            return;
        }
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
    }

    void begin() {
        ui.beginFrame(nullptr, &input, 1600.0f, 720.0f);
        // o modo AUDIT (o padrão da sentinela 48dp — o registo que os pins
        // leem só existe com o auditBegin ativo)
        ui.auditBegin("qol", 1600.0f, 720.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    }
    void end() {
        ui.endFrame();
        input.clearEdges();
    }
};

// o rect do painel da hierarquia (o mesmo do draw)
UiRect hierPanel(const QolEnv& e) {
    return safe::hierarchyPanelRect(1600.0f, 720.0f, safe::Insets{},
                                    e.st.drawerH, e.st.hierW, e.st.inspW);
}

} // namespace

// ---- PIN qol_a_topbar_so_logo ------------------------------------------------
// «Remover o texto "G.One" ao lado do logo. Deixar apenas o ícone do G.One.»
// A prova: o Menu começa ONDE o logo acaba (não há slot de nome entre eles)
// e a degradação estreita já não tem o degrau do nome (o chip é o 1.º a sair).
TEST(qol_a_topbar_so_logo) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    const safe::Insets in{};
    {
        const toolbar::TopBarLayout L =
            toolbar::topbarLayout(1600.0f, 720.0f, in, false, false);
        // SEM nome: o [Menu] encosta ao logo (com o vão de grupo de 12dp)
        EXPECT_MSG(nearEq(L.menu.x, L.logo.x + L.logo.w),
                   "qol_a_topbar_so_logo: o Menu nasce no fim do logo "
                   "(o slot do nome morreu) — menu.x %.1f vs %.1f",
                   (double)L.menu.x, (double)(L.logo.x + L.logo.w));
        EXPECT(L.showPlatform);   // o chip continua no largo
    }
    {
        // ecrã apertado: o chip sai (o degrau do NOME já não existe)
        const toolbar::TopBarLayout L =
            toolbar::topbarLayout(480.0f, 320.0f, in, false, false);
        EXPECT(!L.showPlatform);
        EXPECT(nearEq(L.logo.w, theme::dp(40.0f)));   // o logo NUNCA encolhe
    }
    // a FONTE: o drawTopBar já não escreve "G.One" (o artefato pinado)
    std::string src;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/ui/Toolbar.cpp", src));
    EXPECT(countSub(src, "\"G.One\"") == 0);
}

// ---- PIN qol_a_lupa_expande ---------------------------------------------------
// «A barra "pesquisar TIC" transforma-se num ícone de Lupa. Só expande para
// a barra de texto quando o utilizador toca nela.»
TEST(qol_a_lupa_expande) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    QolEnv e;
    ASSERT(e.ok);

    // (1) RECOLHIDA por defeito: o alvo 5500 é o CHIP da lupa 32×32 na
    // direita da linha (a coluna das ações) — não o campo inteiro
    e.begin();
    drawHierarchy(e.ui, e.scene, e.st);
    e.end();
    const UiRect panel = hierPanel(e);
    bool lupa = false;
    f32 lx = 0.0f, ly = 0.0f, lw = 0.0f, lh = 0.0f;
    for (const auto& en : e.ui.auditRecord().entries) {
        if (en.kind == layout::Entry::Button && en.id == 5500u) {
            lupa = true;
            lx = en.x;
            ly = en.y;
            lw = en.w;
            lh = en.h;
        }
    }
    EXPECT_MSG(lupa, "qol_a_lupa_expande: a lupa (id 5500) existe recolhida");
    EXPECT_MSG(nearEq(lw, theme::dp(32.0f)) && nearEq(lh, theme::dp(32.0f)),
               "qol_a_lupa_expande: o chip da lupa é 32×32 (o piso do "
               "campo) — %.1f×%.1f", (double)lw, (double)lh);
    EXPECT_MSG(nearEq(lx + lw, panel.x + panel.w - theme::dp(16.0f)),
               "qol_a_lupa_expande: a lupa encosta à coluna das ações "
               "(16dp do painel)");
    EXPECT(!e.st.hierSearchOpen);

    // (2) o TOQUE na lupa EXPANDE e pede o teclado (propósito 8) — o
    // clique da casa dispara no LARGAR (o padrão down→frame→up→frame)
    e.input.injectDown(0, lx + lw * 0.5f, ly + lh * 0.5f);
    e.begin();
    drawHierarchy(e.ui, e.scene, e.st);
    e.end();
    e.input.injectUp(0);
    e.begin();
    drawHierarchy(e.ui, e.scene, e.st);
    e.end();
    EXPECT_MSG(e.st.hierSearchOpen,
               "qol_a_lupa_expande: o toque na lupa expande");
    EXPECT(e.st.textPurpose == 8);
    EXPECT(e.st.textInput);   // o teclado in-app abriu com o campo

    // (3) EXPANDIDA: o campo ocupa a linha (o alvo 5500 é largo de novo) e
    // o × (5503) existe; escrever texto mantém a expansão
    e.st.hierSearchLen = static_cast<u32>(
        std::snprintf(e.st.hierSearch, sizeof(e.st.hierSearch), "%s", "Cas"));
    e.begin();
    drawHierarchy(e.ui, e.scene, e.st);
    e.end();
    bool campo = false, xOk = false;
    f32 fx = 0.0f, fw = 0.0f, cx = 0.0f, cy = 0.0f, cw = 0.0f;
    for (const auto& en : e.ui.auditRecord().entries) {
        if (en.kind == layout::Entry::Button && en.id == 5500u) {
            campo = true;
            fx = en.x;
            fw = en.w;
        }
        if (en.kind == layout::Entry::Button && en.id == 5503u) {
            xOk = true;
            cx = en.x;
            cy = en.y;
            cw = en.w;
        }
    }
    EXPECT_MSG(campo && nearEq(fw, panel.w - theme::dp(32.0f)),
               "qol_a_lupa_expande: o campo expandido ocupa a linha");
    EXPECT_MSG(xOk && nearEq(cw, theme::dp(28.0f)),
               "qol_a_lupa_expande: o × (id 5503) existe dentro do campo");
    // o × limpa e RECOLHE (o clique dispara no largar — down→frame→up→frame)
    e.input.injectDown(0, cx + cw * 0.5f, cy + cw * 0.5f);
    e.begin();
    drawHierarchy(e.ui, e.scene, e.st);
    e.end();
    e.input.injectUp(0);
    e.begin();
    drawHierarchy(e.ui, e.scene, e.st);
    e.end();
    EXPECT_MSG(e.st.hierSearchLen == 0 && !e.st.hierSearchOpen,
               "qol_a_lupa_expande: o × limpa e recolhe a lupa");
}

// ---- PIN qol_a_border_cinza ---------------------------------------------------
// «As bordas laranjas (glass) dos painéis inativos passam a cinza escuro
// (#24242A). O laranja fica APENAS para o ativo/selecionado.»
TEST(qol_a_border_cinza) {
    // o token É o cinza do ELEVADO (#24242A), opaco — a borda deixa de ser
    // vidro âmbar
    EXPECT(nearEq(theme::kTheme.border[0], 36.0f / 255.0f));
    EXPECT(nearEq(theme::kTheme.border[1], 36.0f / 255.0f));
    EXPECT(nearEq(theme::kTheme.border[2], 42.0f / 255.0f));
    EXPECT(nearEq(theme::kTheme.border[3], 1.0f));
    EXPECT(theme::kTheme.border[0] == theme::kTheme.elevated[0] &&
           theme::kTheme.border[1] == theme::kTheme.elevated[1] &&
           theme::kTheme.border[2] == theme::kTheme.elevated[2]);
    // o âmbar sobrevive APENAS no accentDim (o fill da SELEÇÃO) — intacto
    EXPECT(nearEq(theme::kTheme.accentDim[0], 74.0f / 255.0f));
    // o divisor ENTRE PAINÉIS segue o border (a fonte única — o y3 pin
    // mede o quad; aqui aferra o contrato do token)
    std::string src;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/ui/UiContext.cpp", src));
    EXPECT(countSub(src, "theme::kTheme.border[0], theme::kTheme.border[1]") >=
           1);   // o dividerGradientV lê o border
    EXPECT(countSub(src, "theme::kTheme.accentDim[0], theme::kTheme.accentDim[1]") ==
           0);   // o âmbar do divisor morreu
}

// ---- PIN qol_a_inspector_ar ---------------------------------------------------
// «Aumentar o espaçamento entre os campos X/Y/Z e entre os botões. O Reset
// deve ter margem para não colar à borda do ecrã.» A prova DESENHADA: os
// vãos entre as caixas são 12dp e o reset NUNCA cola (margem ≥ 8dp).
TEST(qol_a_inspector_ar) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    QolEnv e;
    ASSERT(e.ok);
    e.st.selected = e.scene.create("Corpo");
    e.begin();
    drawInspector(e.ui, e.scene, e.st, nullptr);
    e.end();
    // as caixas X/Y/Z: os PAINÉIS bg (o fill #0A0A0C α1) de 32dp da linha
    // Posição — os 3 primeiros pela ordem de emissão. A medição é no
    // batch de sólidos (os campos são re-despachados pelo scroll — não
    // deixam alvos no registo de auditoria).
    f32 boxX[8], boxW[8], boxY[8];
    int nB = 0;
    const UiRect panel = safe::inspectorPanelRect(1600.0f, 720.0f,
                                                  safe::Insets{});
    const QuadBatch& solids = e.ui.solidsForTest();
    for (u32 q = 0; q + 5 < solids.vertexCount() && nB < 8; q += 6) {
        const QuadVertex* v = solids.vertices() + q;
        f32 mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
        for (u32 k = 0; k < 6; ++k) {
            mnx = v[k].x < mnx ? v[k].x : mnx;
            mxx = v[k].x > mxx ? v[k].x : mxx;
            mny = v[k].y < mny ? v[k].y : mny;
            mxy = v[k].y > mxy ? v[k].y : mxy;
        }
        const f32 wq = mxx - mnx, hq = mxy - mny;
        // o fill bg do campo (r≈10/255, a=1) com a altura do campo 32dp
        if (nearEq(hq, theme::dp(32.0f), 0.5f) && wq > theme::dp(20.0f) &&
            nearEq(v[0].r, 10.0f / 255.0f, 1e-3f) &&
            nearEq(v[0].a, 1.0f, 1e-3f) && v[0].x >= panel.x) {
            boxX[nB] = mnx;
            boxW[nB] = wq;
            boxY[nB] = mny;
            ++nB;
        }
    }
    EXPECT_MSG(nB >= 3,
               "qol_a_inspector_ar: as 3 caixas X/Y/Z existem (%d)", nB);
    if (nB >= 3) {
        // as 3 primeiras são a linha Posição (mesma Y)
        EXPECT(nearEq(boxY[0], boxY[1]) && nearEq(boxY[1], boxY[2]));
        // os quads capturados são o CENTRO da escadaria do panelRounded —
        // o vão ENTRE CAIXAS medido = vão real + 2×raio do canto (4dp cada)
        const f32 gap0 = boxX[1] - (boxX[0] + boxW[0]);
        const f32 gap1 = boxX[2] - (boxX[1] + boxW[1]);
        const f32 wantGap =
            theme::dp(12.0f + 2.0f * theme::kRadiusField);   // 12 + 2×4
        EXPECT_MSG(nearEq(gap0, wantGap) && nearEq(gap1, wantGap),
                   "qol_a_inspector_ar: os vãos X|Y|Z são 12dp (QoL A; "
                   "medidos %0.f com os cantos) — %.1f / %.1f",
                   (double)(wantGap * theme::g_density), (double)gap0,
                   (double)gap1);
        // o RESET (o chip 40 depois do 3.º campo + vão 12) tem margem à
        // direita do painel — nunca cola à borda do ecrã
        const f32 linhaRight = boxX[2] + boxW[2];
        EXPECT_MSG(linhaRight + theme::dp(12.0f) + theme::dp(40.0f) <=
                       panel.x + panel.w + 0.01f,
                   "qol_a_inspector_ar: a linha + reset cabe com margem");
    }
    // a FONTE: os vãos são os kTfGap* 12 (o artefato pinado)
    std::string src;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/ui/EditorUi.cpp", src));
    EXPECT(countSub(src, "kTfGapBox   = 12.0f") == 1);
    EXPECT(countSub(src, "kTfGapReset = 12.0f") == 1);
    EXPECT(countSub(src, "kTfRowPadMin = 8.0f") == 1);
}

// ---- PIN qol_a_cenas_x_fecha --------------------------------------------------
// «Garantir que o botão "Nova Cena" e o "X" para fechar estão bem visíveis
// e com o espaçamento correto.» O X (48×48) vive à direita do Nova cena e
// o toque fecha SEM ação.
TEST(qol_a_cenas_x_fecha) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    QolEnv e;
    ASSERT(e.ok);
    const std::vector<std::string> scenes = {"scenes/main.goni",
                                             "scenes/nivel.goni"};

    // (1) a geometria: Nova cena + X na MESMA linha, o X 48×48 à direita
    e.begin();
    drawScenesMenu(e.ui, e.input, 1600.0f, 720.0f, e.st, scenes, 0);
    e.end();
    f32 nx = 0, ny = 0, nw = 0, nh = 0, cx = 0, cy = 0, cw = 0, ch = 0;
    bool nova = false, xOk = false;
    for (const auto& en : e.ui.auditRecord().entries) {
        if (en.kind == layout::Entry::Button && en.id == 6700u) {
            nova = true;
            nx = en.x;
            ny = en.y;
            nw = en.w;
            nh = en.h;
        }
        if (en.kind == layout::Entry::Button && en.id == 6799u) {
            xOk = true;
            cx = en.x;
            cy = en.y;
            cw = en.w;
            ch = en.h;
        }
    }
    ASSERT(nova);
    EXPECT_MSG(xOk, "qol_a_cenas_x_fecha: o X (id 6799) existe");
    EXPECT_MSG(nearEq(cw, theme::dp(48.0f)) && nearEq(ch, theme::dp(48.0f)),
               "qol_a_cenas_x_fecha: o X é 48×48 (bem visível)");
    EXPECT_MSG(nearEq(cy, ny) && cx >= nx + nw,
               "qol_a_cenas_x_fecha: o X na mesma linha, à direita do "
               "Nova cena, sem sobrepor");
    EXPECT(e.st.scenesMenu == false);   // o draw não abre o menu (o main arma)

    // (2) o toque no X fecha o menu aberto SEM ação (o retorno é −1; o
    // clique dispara no largar — down→frame→up→frame)
    e.st.scenesMenu = true;
    e.input.injectDown(0, cx + cw * 0.5f, cy + ch * 0.5f);
    {
        e.ui.beginFrame(nullptr, &e.input, 1600.0f, 720.0f);
        drawScenesMenu(e.ui, e.input, 1600.0f, 720.0f, e.st, scenes, 0);
        e.ui.endFrame();
        e.input.clearEdges();
    }
    e.input.injectUp(0);
    const int pick = [&] {
        e.ui.beginFrame(nullptr, &e.input, 1600.0f, 720.0f);
        const int p = drawScenesMenu(e.ui, e.input, 1600.0f, 720.0f, e.st,
                                     scenes, 0);
        e.ui.endFrame();
        e.input.clearEdges();
        return p;
    }();
    EXPECT_MSG(pick == -1,
               "qol_a_cenas_x_fecha: o X fecha sem escolher cena");
    EXPECT(!e.st.scenesMenu);
}

// ---- PIN qol_b_navegador_compacto ---------------------------------------------
// «O navegador de ficheiros parece botões gigantes → lista compacta.»
// A prova DESENHADA: as raízes e o Subir deixam de emitir o gradiente
// âmbar do choke point button() (o fill é surface/borda — os quads do
// card), a seta (>) existe nas linhas dos FICHEIROS também e o breadcrumb
// desenha o separador « / ».
TEST(qol_b_navegador_compacto) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    QolEnv e;
    ASSERT(e.ok);
    std::vector<fileapi::DirEntry> entries;
    fileapi::DirEntry d1;
    d1.name = "Download";
    d1.isDir = true;
    fileapi::DirEntry f1;
    f1.name = "modelo.obj";
    f1.kind = 'm';
    entries.push_back(d1);
    entries.push_back(f1);
    e.st.fileBrowser = true;
    e.begin();
    (void)drawFileBrowser(e.ui, e.input, 1600.0f, 720.0f, e.st, "/Download",
                          entries, false);
    e.end();
    // os ALVOS continuam nos ids de sempre (a geometria não mudou — o
    // picker_compacto da C-4 segue aferrado): 6 raízes + Subir + fechar
    u32 roots = 0, subir = 0, fechar = 0;
    for (const auto& en : e.ui.auditRecord().entries) {
        if (en.kind == layout::Entry::Button) {
            if (en.id >= 6850u && en.id < 6856u) {
                ++roots;
            }
            if (en.id == 6860u) {
                ++subir;
            }
            if (en.id == 6899u) {
                ++fechar;
            }
        }
    }
    EXPECT_MSG(roots == 6u && subir == 1u && fechar == 1u,
               "qol_b_navegador_compacto: os alvos de sempre existem "
               "(raízes %u, subir %u, fechar %u)", roots, subir, fechar);
    // o FILHO VISUAL: ZERO quads do gradiente âmbar (#FFB020→#FF6B00 — o
    // fill do button) nas BANDAS das raízes e do Subir; o fill lá é
    // surface (#141416 α0.85). (Fora destas bandas o accent é legítimo —
    // o thumb do scrollbar, por exemplo.)
    const BrowserSheetLayout L = browserSheetLayout(
        1600.0f, 720.0f, 0.0f, 0.0f, 0.0f, 0.0f, theme::dp(28.0f), 2u);
    const QuadBatch& solids = e.ui.solidsForTest();
    u32 accentFill = 0;
    for (u32 q = 0; q + 5 < solids.vertexCount(); q += 6) {
        const QuadVertex* v = solids.vertices() + q;
        f32 mny = 1e9f;
        for (u32 k = 0; k < 6; ++k) {
            mny = v[k].y < mny ? v[k].y : mny;
        }
        const bool banda = (mny >= L.rootsY - 1.0f &&
                            mny <= L.upY + L.rowH + 1.0f);
        // o gradiente do button: topo âmbar puro (r=1, g≈0.69, a=1)
        if (banda && nearEq(v[0].r, 1.0f, 1e-3f) &&
            nearEq(v[0].g, 176.0f / 255.0f, 2e-3f) &&
            nearEq(v[0].a, 1.0f, 1e-3f)) {
            ++accentFill;
        }
    }
    EXPECT_MSG(accentFill == 0u,
               "qol_b_navegador_compacto: ZERO botões âmbar gigantes nas "
               "bandas raízes/Subir (%u sobraram)", accentFill);
    // a FONTE: as raízes/subir desenham com panelRounded + frameRounded
    // (o padrão quieto) e o breadcrumb tem o separador « / »
    std::string src;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/ui/UiEditor.cpp", src));
    EXPECT(countSub(src, "ui.fontWidth(\" / \")") == 1);
    EXPECT(countSub(src, "\"^ Subir\"") == 0);   // o botão gigante morreu
}

// ---- PIN qol_b_ponto_fora -----------------------------------------------------
// «Ocultar ficheiros/pastas que começam com "."». A prova REAL no FileApi
// (o R-057 é a sentinela; este pin aferra a superfície do draw: a lista
// que chega ao browser já não tem pontos).
TEST(qol_b_ponto_fora) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    char tmpl[] = "/tmp/goni-qolb-XXXXXX";
    const char* mkd = mkdtemp(tmpl);
    ASSERT(mkd != nullptr);
    const std::string root = mkd;
    fileapi::makeDirs(root);
    fileapi::makeDirs(root + "/.git");
    fileapi::makeDirs(root + "/Pasta");
    fileapi::writeAll(root + "/.oculto.txt", "x", 1);
    fileapi::writeAll(root + "/visivel.goni", "{}", 2);
    std::vector<fileapi::DirEntry> out;
    ASSERT(fileapi::listDirEntries(root, out));
    EXPECT(out.size() == 2u);   // a Pasta + o visivel.goni
    for (const fileapi::DirEntry& en : out) {
        EXPECT(en.name[0] != '.');
    }
    EXPECT(out[0].isDir && out[0].name == "Pasta");
    EXPECT(!out[1].isDir && out[1].name == "visivel.goni");
    // limpeza
    ::remove((root + "/.oculto.txt").c_str());
    ::remove((root + "/visivel.goni").c_str());
    ::rmdir((root + "/.git").c_str());
    ::rmdir((root + "/Pasta").c_str());
    ::rmdir(root.c_str());
}

// ---- PIN qol_c_frustum_sob_selecao --------------------------------------------
// «A teia de linhas da câmara fica INVISÍVEL por defeito. Só aparece se o
// utilizador selecionar a câmara na Hierarquia.» (a sentinela R-058 aferra;
// este pin mede o DESENHO no env do QoL)
TEST(qol_c_frustum_sob_selecao) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    QolEnv e;
    ASSERT(e.ok);
    const Handle h = e.scene.create("Camera");
    Tic* t = e.scene.get(h);
    t->addComponent<Transform3D>();
    t->addComponent<CameraComp>();
    const Camera orbit;
    const Mat4 vp = Mat4::mul(orbit.proj(1600.0f / 720.0f), orbit.view());
    const Vec3 eye = orbit.eye();
    // SEM seleção: ZERO arestas longas (a teia); o glifo mudo segue
    e.begin();
    camgizmo::drawAll(e.ui, e.scene, vp, 1600.0f, 720.0f, Handle{}, eye);
    e.end();
    {
        const QuadBatch& solids = e.ui.solidsForTest();
        u32 longos = 0;
        for (u32 q = 0; q + 5 < solids.vertexCount(); q += 6) {
            const QuadVertex* v = solids.vertices() + q;
            f32 mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
            for (u32 k = 0; k < 6; ++k) {
                mnx = v[k].x < mnx ? v[k].x : mnx;
                mxx = v[k].x > mxx ? v[k].x : mxx;
                mny = v[k].y < mny ? v[k].y : mny;
                mxy = v[k].y > mxy ? v[k].y : mxy;
            }
            const f32 ext = (mxx - mnx) > (mxy - mny) ? (mxx - mnx)
                                                      : (mxy - mny);
            if (ext > theme::dp(40.0f)) {
                ++longos;
            }
        }
        EXPECT_MSG(longos == 0u,
                   "qol_c_frustum_sob_selecao: a teia invisível por defeito "
                   "(%u arestas longas)", longos);
    }
    // COM seleção: a teia (as arestas longas) volta — é o modo de edição
    e.begin();
    camgizmo::drawAll(e.ui, e.scene, vp, 1600.0f, 720.0f, h, eye);
    e.end();
    {
        const QuadBatch& solids = e.ui.solidsForTest();
        u32 longos = 0;
        for (u32 q = 0; q + 5 < solids.vertexCount(); q += 6) {
            const QuadVertex* v = solids.vertices() + q;
            f32 mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
            for (u32 k = 0; k < 6; ++k) {
                mnx = v[k].x < mnx ? v[k].x : mnx;
                mxx = v[k].x > mxx ? v[k].x : mxx;
                mny = v[k].y < mny ? v[k].y : mny;
                mxy = v[k].y > mxy ? v[k].y : mxy;
            }
            const f32 ext = (mxx - mnx) > (mxy - mny) ? (mxx - mnx)
                                                      : (mxy - mny);
            if (ext > theme::dp(40.0f)) {
                ++longos;
            }
        }
        // 12 arestas na cena; as que ficam ATRÁS do olho não projetam (o
        // edge() salta) — o piso é a teia VISÍVEL (8 neste enquadramento)
        EXPECT_MSG(longos >= 6u,
                   "qol_c_frustum_sob_selecao: a teia volta COM a câmara "
                   "selecionada (%u arestas)", longos);
    }
}

// ---- PIN qol_c_rail_flutuante -------------------------------------------------
// «Os botões de Mover/Rodar/Escalar flutuam sem o fundo preto quadrado
// gigante.» O pai de vidro do RAIL morreu; os chips individuais ficam.
TEST(qol_c_rail_flutuante) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    QolEnv e;
    ASSERT(e.ok);
    editor::toolbar::GizmoModeState gz;
    vpchrome::ChromeState cs;
    Camera camera;
    e.begin();
    vpchrome::draw(e.ui, e.st, gz, cs, camera, 0.0f, 1.0f / 60.0f, nullptr);
    e.end();
    // o LAYOUT mantém o rect do rail (o contrato dos pins C-2) — mas o
    // DESENHO do pai morreu: ZERO quads do vidro do pai (surface2 α~0.52
    // a 60%) na banda do rail que NÃO seja um chip de botão (surface
    // α~0.51 — o RGB distingue: 30 vs 20)
    const UiRect view = safe::centerRect(1600.0f, 720.0f, safe::Insets{},
                                         0.0f, e.st.showInspector, e.st.hierW,
                                         e.st.inspW,
                                         inspectorCollapsed(e.st));
    const vpchrome::Layout L = vpchrome::layout(view);
    EXPECT(L.railVisible);   // o rail existe (só o pai é que morreu)
    const QuadBatch& solids = e.ui.solidsForTest();
    u32 vidroPai = 0;
    for (u32 q = 0; q + 5 < solids.vertexCount(); q += 6) {
        const QuadVertex* v = solids.vertices() + q;
        f32 mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
        for (u32 k = 0; k < 6; ++k) {
            mnx = v[k].x < mnx ? v[k].x : mnx;
            mxx = v[k].x > mxx ? v[k].x : mxx;
            mny = v[k].y < mny ? v[k].y : mny;
            mxy = v[k].y > mxy ? v[k].y : mxy;
        }
        // um quad do PAI cobriria a largura do rail inteiro (btn+8dp)
        const f32 wq = mxx - mnx;
        const bool noRail = (mny >= L.railPanel.y - 1.0f &&
                             mny <= L.railPanel.y + L.railPanel.h + 1.0f &&
                             mnx >= L.railPanel.x - 1.0f &&
                             mnx <= L.railPanel.x + L.railPanel.w + 1.0f);
        const bool larguraDoPai = wq >= L.railPanel.w - 0.5f;
        if (noRail && larguraDoPai) {
            ++vidroPai;
        }
    }
    EXPECT_MSG(vidroPai == 0u,
               "qol_c_rail_flutuante: o pai de vidro do rail não desenha "
               "(%u quads do pai)", vidroPai);
}

// ---- PIN qol_c_grid_fade_suave ------------------------------------------------
// «O grid deve ter um fade suave com a distância.» A FONTE ÚNICA
// gridFadeForDist (a sentinela R-058 aferra; este pin aferra a janela
// de TRABALHO que o dono vê: o caída 2.2× com o piso 60).
TEST(qol_c_grid_fade_suave) {
    f32 st0 = 0.0f, en0 = 0.0f;
    Grid::gridFadeForDist(6.0f, st0, en0);
    // o feel de trabalho: 12..72 (era 12..42 — a janela dobra)
    EXPECT(nearEq(st0, 12.0f));
    EXPECT(nearEq(en0, 72.0f));
    // o caída NUNCA desce abaixo de 60 (a suavidade é o contrato)
    f32 st1 = 0.0f, en1 = 0.0f;
    for (f32 f = 0.5f; f <= 4000.0f; f *= 2.0f) {
        Grid::gridFadeForDist(f, st1, en1);
        EXPECT_MSG(en1 - st1 >= 60.0f - 0.01f,
                   "qol_c_grid_fade_suave: o caída em foco %.1f é %.1f "
                   "(< 60 — a borda dura voltou)", (double)f,
                   (double)(en1 - st1));
        EXPECT(st1 >= 12.0f - 0.01f);   // o piso do início
    }
    // a FONTE: o draw consome a função (o artefato pinado)
    std::string src;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/render/Grid.cpp", src));
    EXPECT(countSub(src, "gridFadeForDist(focus, start, end)") == 1);
}
