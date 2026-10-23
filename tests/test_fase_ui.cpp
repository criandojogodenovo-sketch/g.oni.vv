// tests/test_fase_ui.cpp — OS PINS DA FASE UI (a ordem do dono, envio 1/5
// + 2/5): as partes A..E do redesign visual. O contrato da fase:
//   • parte A — O FICHEIRO ÚNICO DE TOKENS: a paleta BRASA do dono (os
//     hexes EXATOS), a escala de espaçamento 4/8/12/16, os raios 6-8 e a
//     regra «destaque ÚNICO» (nenhum 2.º âmbar no chrome);
//   • A SOBREPOSIÇÃO AUTOMÁTICA (a regra da fase): «nada tapa nada nem
//     sai do ecrã» — o harness constrói os layouts da casa em 3 resoluções
//     × 2 orientações e afere PARES de alvos (a fonte única do desenho é
//     a mesma função que a app consome — o padrão topbar_3_zonas).
// Os pins das partes B..E entram nos commits delas (um commit por parte).
#include "ui/Theme.h"
#include "ui/SafeArea.h"
#include "ui/ScrollMath.h"
#include "ui/Toolbar.h"
#include "ui/EditorLayout.h"
#include "ui/BottomPanel.h"
#include "TestFramework.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

using namespace vv;

namespace {

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

bool nearC(const f32 c[4], int r, int g, int b, f32 a) {
    return test::nearEqF(c[0], r / 255.0f, 1e-4f) &&
           test::nearEqF(c[1], g / 255.0f, 1e-4f) &&
           test::nearEqF(c[2], b / 255.0f, 1e-4f) &&
           test::nearEqF(c[3], a, 1e-4f);
}

// a família da brasa: R>G>B (o fogo sobe) — a regra «destaque ÚNICO»: o
// gradiente/glow são a MESMA família de matiz, nunca um 2.º destaque
bool brasaFamily(const f32 c[4]) {
    return c[0] > c[1] && c[1] > c[2];
}

// ---- O HARNESS DE SOBREPOSIÇÃO (a regra da fase: nada tapa nada) ---------
struct RectNamed {
    const char* name;
    UiRect      r;
};

// pares disjuntos: interseção de área >1px é sobreposição honesta
bool overlap(const UiRect& a, const UiRect& b) {
    const f32 ix = std::min(a.x + a.w, b.x + b.w) - std::max(a.x, b.x);
    const f32 iy = std::min(a.y + a.h, b.y + b.h) - std::max(a.y, b.y);
    return ix > 1.0f && iy > 1.0f;
}

void checkInsideAndDisjoint(const std::vector<RectNamed>& ws, const UiRect& box,
                            const char* ctx, i32 sw, i32 sh) {
    for (const RectNamed& w : ws) {
        EXPECT_MSG(w.r.x >= box.x - 0.5f && w.r.y >= box.y - 0.5f &&
                       w.r.x + w.r.w <= box.x + box.w + 0.5f &&
                       w.r.y + w.r.h <= box.y + box.h + 0.5f,
                   "ui_sobreposicao(%s @%dx%d): %s SAI do ecrã (%.0f,%.0f "
                   "%0.fx%.0f)", ctx, sw, sh, w.name, (double)w.r.x,
                   (double)w.r.y, (double)w.r.w, (double)w.r.h);
    }
    for (size_t i = 0; i < ws.size(); ++i) {
        for (size_t j = i + 1; j < ws.size(); ++j) {
            EXPECT_MSG(!overlap(ws[i].r, ws[j].r),
                       "ui_sobreposicao(%s @%dx%d): %s TAPA %s", ctx, sw, sh,
                       ws[i].name, ws[j].name);
        }
    }
}

} // namespace

// ---- PIN ui_tokens_brasa — A PALETA DO DONO + A REGRA DO DESTAQUE ÚNICO ---
// (a recalibração dos irmãos y1_tokens/toolbar já aferra os valores; este
// pin aferra O CONTRATO NOVO da fase: a brasa é a ÚNICA família de destaque
// no chrome e a escala do dono existe)
TEST(ui_tokens_brasa) {
    // os HEXES EXATOS da ordem (a parte A)
    EXPECT(nearC(theme::kTheme.bg, 11, 11, 13, 1.0f));          // #0B0B0D
    EXPECT(nearC(theme::kTheme.surface, 18, 18, 21, 0.85f));    // #121215
    EXPECT(nearC(theme::kTheme.card, 27, 27, 32, 1.0f));        // #1B1B20
    EXPECT(nearC(theme::kTheme.elevated, 38, 38, 44, 1.0f));    // #26262C
    EXPECT(nearC(theme::kTheme.border, 38, 38, 44, 1.0f));      // #26262C 1px
    EXPECT(nearC(theme::kTheme.text1, 232, 230, 227, 1.0f));    // #E8E6E3
    EXPECT(nearC(theme::kTheme.text2, 138, 138, 147, 1.0f));    // #8A8A93
    EXPECT(nearC(theme::kTheme.accent, 255, 90, 31, 1.0f));     // #FF5A1F
    EXPECT(nearC(theme::kTheme.accentDim, 43, 24, 16, 1.0f));   // #2B1810
    // os EIXOS do dono (exatos)
    EXPECT(nearC(theme::kTheme.axisX, 229, 72, 77, 1.0f));      // #E5484D
    EXPECT(nearC(theme::kTheme.axisY, 63, 185, 80, 1.0f));      // #3FB950
    EXPECT(nearC(theme::kTheme.axisZ, 59, 130, 246, 1.0f));     // #3B82F6
    // O DESTAQUE ÚNICO: a brasa e a família toda (gradiente/premido/glow)
    // têm o MESMO matiz (R>G>B) — nenhum 2.º destaque; o âmbar (#FFB020,
    // G=176) morreu — nenhum token de chrome com G ≥ 150 além do texto
    EXPECT(brasaFamily(theme::kTheme.accent));
    EXPECT(brasaFamily(theme::kTheme.accent2));
    EXPECT(brasaFamily(theme::kTheme.accentPress));
    EXPECT(brasaFamily(theme::kTheme.fireRed));
    EXPECT(brasaFamily(theme::kTheme.fireOrange));
    EXPECT(brasaFamily(theme::kTheme.glow));
    EXPECT(theme::kTheme.accent[1] < 150.0f / 255.0f);
    EXPECT(theme::kTheme.fireRed[1] < 150.0f / 255.0f);
    // A ESCALA DO DONO: 4/8/12/16 + os raios 6-8
    EXPECT(test::nearEqF(theme::kSpaceIcon, 4.0f, 1e-4f));
    EXPECT(test::nearEqF(theme::kSpace1, 8.0f, 1e-4f));
    EXPECT(test::nearEqF(theme::kSpaceMid, 12.0f, 1e-4f));
    EXPECT(test::nearEqF(theme::kSpace2, 16.0f, 1e-4f));
    EXPECT(theme::kRadiusField >= 6.0f && theme::kRadiusCard <= 8.0f);
}

// ---- PIN ui_topbar_40dp — A BARRA DO DONO (a parte B) ----------------------
// 40dp, a direita nova (undo/redo/save/play/stop/gear — subiram do
// viewport), o Pause na PLAY BAR e o «Android» nas Definições
TEST(ui_topbar_40dp_composicao) {
    // (a) a ALTURA: o token da fonte única é 40 (a ordem do dono; era 36)
    EXPECT(test::nearEqF(safe::kTopBarH, 40.0f, 1e-4f));
    EXPECT(test::nearEqF(safe::kToolbarH, safe::kTopBarH, 1e-6f));
    // (b) a DIREITA NOVA existe na barra (os ids fora das faixas antigas)
    EXPECT(editor::toolbar::kTbUndoId > editor::toolbar::kTbGearId);
    EXPECT(editor::toolbar::kTbSaveId > editor::toolbar::kTbRedoId);
    // (c) o PAUSE vive na PLAY BAR (o vizinho do Stop — o id 6) e NUNCA
    // volta à barra (o id 3 do pause está aposentado — o layout não o tem:
    // o campo pause MORREU do TopBarLayout, o compilar é a prova D21)
    EXPECT(editor::kPlayPauseId == 6u);
    EXPECT(editor::kPlayStopId == 5u);
    // (d) a FONTE: o drawTopBar já não desenha pause/plataforma (o
    // artefato pinado — GONI_ROOT_OVERRIDE nas mutações)
    std::string src;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/ui/Toolbar.cpp", src));
    EXPECT_MSG(countSub(src, "kTbPauseId") == 0,
               "ui_topbar_40dp: o kTbPauseId não desenha mais (aposentado)");
    EXPECT_MSG(countSub(src, "kTbPlatformId") == 0,
               "ui_topbar_40dp: o kTbPlatformId não desenha mais");
    EXPECT(countSub(src, "kTbUndoId") >= 1);
    EXPECT(countSub(src, "kTbRedoId") >= 1);
    EXPECT(countSub(src, "kTbSaveId") >= 1);
    // (e) o «Android» vive nas DEFINIÇÕES (a linha «plataforma»)
    std::string set;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/ui/SettingsPage.cpp",
                    set));
    EXPECT_MSG(countSub(set, "\"plataforma\"") == 1 &&
                   countSub(set, "Mobile (Android)") == 1,
               "ui_topbar_40dp: a linha da plataforma vive nas Definições");
}

// ---- PIN ui_hierarquia_40dp — AS LINHAS DO DONO (a parte C) ----------------
TEST(ui_hierarquia_40dp) {
    // (a) a LINHA: 40dp na fonte (o extern que o draw e o hit partilham)
    EXPECT(test::nearEqF(editor::kRowH, theme::dp(40.0f), 1e-4f));
    // (b) A FONTE: o olho/⋮ desenha SÓ na selecionada (o gate `if
    // (selected)`) e o hit-test gated por `controlsRow`; a indentação lê
    // a escala do dono (kSpace3=16 — o 24 literal morreu)
    std::string src;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/ui/EditorUi.cpp", src));
    EXPECT_MSG(countSub(src, "if (selected) {") >= 1 &&
                   countSub(src, "controlsRow &&") >= 2,
               "ui_hierarquia_40dp: o olho/⋮ é SÓ da linha selecionada "
               "(draw gated + hit gated)");
    EXPECT_MSG(countSub(src, "theme::dp(theme::kSpace3)") >= 2,
               "ui_hierarquia_40dp: a indentação lê a escala do dono");
    // (c) o NOME completo nas linhas não selecionadas (o budget largura
    // toda — a reticência só quando não couber)
    EXPECT(countSub(src, "(x + w - theme::dp(12.0f) - nameX)") == 1);
}

// ---- PIN ui_divisores_persistencia — A PARTE D -----------------------------
// «Divisores arrastáveis com zona de toque 24dp + tamanhos mínimos +
// guardar/restaurar o layout entre sessões» — a FONTE ÚNICA e o
// roundtrip do layout.json (o mesmo contrato do divisor_drag)
TEST(ui_divisores_persistencia) {
    // (a) a ZONA DE TOQUE: 24dp na fonte única (a ordem do dono)
    EXPECT(test::nearEqF(editor::kDividerHitW, 24.0f, 1e-4f));
    // (b) os TAMANHOS MÍNIMOS (a gangorra do resolvePanels)
    EXPECT(test::nearEqF(safe::kHierMinW, 140.0f, 1e-4f));
    EXPECT(test::nearEqF(safe::kInspMinW, 180.0f, 1e-4f));
    EXPECT(test::nearEqF(safe::kInspMaxW, 260.0f, 1e-4f));
    EXPECT(test::nearEqF(safe::kViewportMinW, 288.0f, 1e-4f));
    // (c) A PERSISTÊNCIA: o serializeLayout leva hierW/inspW (o
    // layout.json — a sessão seguinte restaura)
    std::string src;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/ui/BottomPanel.cpp", src));
    EXPECT(countSub(src, "hierW=") >= 1);
    EXPECT(countSub(src, "inspW=") >= 1);
    // (d) o roundtrip REAL: 200/240 vão e voltam (o mesmo contrato do
    // divisor_drag — aqui aferrado ao contrato da fase)
    std::string dump = "bottomTab=0\ndrawerH=0\ninspector=1\n"
                       "inspCollapsed=0\nhierW=200\ninspW=240\n";
    f32 hw = -1.0f, iw = -1.0f;
    bool show = false;
    u32 coll = 0;
    editor::bottom::BottomState bs{};
    EXPECT(editor::bottom::parseLayout(dump, bs, show, coll, &hw, &iw));
    EXPECT(test::nearEqF(hw, 200.0f, 1e-4f));
    EXPECT(test::nearEqF(iw, 240.0f, 1e-4f));
}

// ---- PIN ui_inspector_cabecalho — A PARTE E --------------------------------
// «Cabeçalho: nome do TIC (brasa) e o tipo por baixo; pos/rot/esc em 3
// campos com barrinha colorida do eixo e valor, sem letras gigantes»
TEST(ui_inspector_cabecalho) {
    // (a) A FONTE: o cabeçalho desenha o NOME em accent quando há TIC
    // (o título «Inspector» fica para o vazio) e o TIPO vem do perfil
    std::string src;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/ui/EditorUi.cpp", src));
    EXPECT(countSub(src, "ui.labelStyled(x + kPad, nameBase, shown, theme::kTheme.accent") == 1);
    EXPECT(countSub(src, "\"Environment\"") >= 1);   // a tabela do tipo
    EXPECT(countSub(src, "headerH = theme::dp(44.0f);") == 1);
    // (b) AS BARRINHAS: o draw da barra de 3dp na cor do eixo e as LETRAS
    // mortas (o valor com o padL 12 — o espaço da barra)
    EXPECT(countSub(src, "theme::dp(3.0f),") >= 1 &&
           countSub(src, "kAxisCol[axis]);") >= 1);
    EXPECT_MSG(countSub(src, "ui.label(bx + theme::dp(6.0f), boxBase, kAxis[axis],") == 0,
               "ui_inspector_cabecalho: as letras X/Y/Z morreram (a barrinha "
               "identifica o eixo — a ordem do dono)");
    // (c) O ESTADO VAZIO: o «criar TIC» vive no vazio (o caminho do C-4)
    EXPECT(countSub(src, "kInspCreateTicId") >= 1);
    // (e) O NOME NÃO TAPA O RECOLHER (a regra da fase: nada tapa nada) —
    // o orçamento do nome reserva a célula 40dp do pin/unpin quando fixado
    EXPECT(countSub(src, "st.inspPinned ? theme::dp(40.0f) : 0.0f") == 1);
    // (d) AS SECÇÕES RECOLHÍVEIS (a ordem: «Transform, Física, Material…»)
    // — a PROVA é no PLANO (a fonte única, GL-free): os cabeçalhos ficam,
    // os campos morrem com o bit
    {
        editor::InspProfile prof{};
        prof.tr = true;
        prof.mr = true;
        prof.bc = true;
        const TextMetrics m{};
        editor::InspRow rows[64];
        const u32 nAll = editor::inspectorPlan(prof, m, false, 0u, rows);
        u32 sec = 0, trf = 0;
        for (u32 i = 0; i < nAll; ++i) {
            if (rows[i].kind == editor::InspRow::Kind::Section) ++sec;
            if (rows[i].kind == editor::InspRow::Kind::TransformRow) ++trf;
        }
        EXPECT(sec == 5);   // Transform + Malha + Material + Física + Script
        EXPECT(trf == 3);   // Pos/Rotação/Escala abertos por omissão
        const u32 nT = editor::inspectorPlan(prof, m, false,
                                             editor::kInspBitTransform, rows);
        u32 trf2 = 0;
        for (u32 i = 0; i < nT; ++i) {
            if (rows[i].kind == editor::InspRow::Kind::TransformRow) ++trf2;
        }
        EXPECT(trf2 == 0);   // o bit FECHA a secção (só o cabeçalho fica)
    }
}

// ---- PIN ui_paineis_dock — A PARTE F (a barra fina + o painel encaixado) --
// «Barra inferior fina (32dp): à esquerda o botão "Painéis ▾" (contorno
// brasa quando o painel está aberto), à direita o estado em mono "60 FPS ·
// 6 TICs". Remover o "bl 2/2". Ao tocar em Painéis abre um painel
// ENCAIXADO por baixo do viewport... altura arrastável; lembra a última
// aba; altura inicial ~30%/~40%; a Consola mostra um ponto vermelho na aba
// e no botão quando há erro. Fecha ao tocar em ▾ ou em Painéis.»
TEST(ui_paineis_dock) {
    // (a) A BARRA: o botão Painéis à ESQUERDA e o estado à direita — sem
    // sobreposição em 5 ecrãs (o harness da fase)
    const struct { i32 w, h; const char* ctx; } screens[] = {
        {776, 336, "paisagem-S"}, {1600, 720, "paisagem-M"},
        {2560, 1440, "paisagem-L"}, {720, 1600, "retrato-M"},
        {1440, 2560, "retrato-L"},
    };
    for (const auto& sc : screens) {
        theme::setDensity(1.0f);
        const safe::Insets in{};
        for (int tab = 0; tab <= 4; ++tab) {
            editor::bottom::BottomState bs;
            bs.bottomTab = tab;
            bs.drawerH = 240.0f;
            const editor::bottom::Layout L =
                editor::bottom::layout((f32)sc.w, (f32)sc.h, in, bs);
            std::vector<RectNamed> ws;
            ws.push_back({"paineis", L.panelsBtn});
            ws.push_back({"fps", L.fps});
            checkInsideAndDisjoint(ws, L.tabBar, sc.ctx, sc.w, sc.h);
            // a barra INTEIRA no fundo do ecrã
            EXPECT(L.tabBar.y + L.tabBar.h <= (f32)sc.h + 0.5f);
            if (tab > 0) {
                // (b) O ENCAIXE: o viewport SOBE — o painel é a faixa acima
                // da barra (não é janela por cima): o content do painel
                // termina ONDE a barra começa e o viewport central perde a
                // altura (o MESMO effectiveDrawerH do main)
                std::vector<RectNamed> wp;
                wp.push_back({"handle", L.handle});
                wp.push_back({"tabrow", L.tabRow});
                wp.push_back({"conteudo", L.content});
                checkInsideAndDisjoint(wp, L.drawer, sc.ctx, sc.w, sc.h);
                EXPECT(L.drawer.y + L.drawer.h <= L.tabBar.y + 0.5f);
                const UiRect vp = safe::viewportRect((f32)sc.w, (f32)sc.h, in);
                // o MESMO contrato do layout (o cap da tab + o piso da
                // Animação — kTimelineH + pega + fila)
                const f32 minHp =
                    tab == 4 ? theme::dp(192.0f + editor::bottom::kDrawerHandleH +
                                         editor::bottom::kTabRowH)
                             : 0.0f;
                const f32 dEff = safe::effectiveDrawerH(
                    bs.drawerH, vp.h, (f32)sc.h - in.top - in.bottom,
                    tab == 4 ? safe::kDrawerMaxPctAnim : safe::kDrawerMaxPct,
                    minHp);
                EXPECT(test::nearEqF(L.drawer.h, dEff, 0.5f));
                // as 4 TABS na fila, disjuntas, dentro dela
                std::vector<RectNamed> wt;
                for (int i = 0; i < 4; ++i) {
                    char nm[16];
                    std::snprintf(nm, sizeof(nm), "tab%d", i + 1);
                    wt.push_back({nm, L.tab[i]});
                }
                checkInsideAndDisjoint(wt, L.tabRow, sc.ctx, sc.w, sc.h);
            }
        }
    }
    // (c) A ALTURA INICIAL por tab (a ordem: ~30% · ~40% na Animação)
    EXPECT(test::nearEqF(editor::bottom::tabDefaultHPct(1), 0.30f, 1e-4f));
    EXPECT(test::nearEqF(editor::bottom::tabDefaultHPct(2), 0.30f, 1e-4f));
    EXPECT(test::nearEqF(editor::bottom::tabDefaultHPct(3), 0.30f, 1e-4f));
    EXPECT(test::nearEqF(editor::bottom::tabDefaultHPct(4), 0.40f, 1e-4f));
    EXPECT(test::nearEqF(editor::bottom::tabDefaultH(4, 1000.0f), 400.0f,
                         1e-4f));   // clamp ao teto cru da pega
    EXPECT(test::nearEqF(editor::bottom::tabDefaultH(1, 1000.0f), 300.0f,
                         1e-4f));
    // (d) O ANIMAÇÃO CABE A TIMELINE: o piso efetivo (kTimelineH + pega +
    // fila) cabe dentro do cap de 40% num ecrã de referência
    {
        theme::setDensity(1.0f);
        const safe::Insets in{};
        editor::bottom::BottomState bs;
        bs.bottomTab = 4;
        bs.drawerH = 160.0f;   // o dono arrastou ao mínimo cru
        const editor::bottom::Layout L =
            editor::bottom::layout(1600.0f, 720.0f, in, bs);
        const f32 conteudo = L.content.h;
        EXPECT(conteudo + 0.5f >= theme::dp(192.0f));   // a timeline cabe
    }
    // (e) A FONTE: o botão Painéis com o contorno brasa aberto + o PONTO
    // VERMELHO na aba e no botão + o «· bl n/m» MORTO + o estado novo
    {
        std::string src;
        ASSERT(readFile(repoRoot() + "/app/src/main/cpp/ui/BottomPanel.cpp",
                        src));
        EXPECT_MSG(countSub(src, "kTheme.danger") >= 3,
                   "ui_paineis_dock: o PONTO VERMELHO vive na aba Consola e "
                   "no botão Painéis (2 pontos + a cor das linhas E/)");
        EXPECT_MSG(countSub(src, "\"Painéis\"") == 1,
                   "ui_paineis_dock: o rótulo do botão é «Painéis»");
        EXPECT_MSG(countSub(src, "bl %u/%u") == 0,
                   "ui_paineis_dock: o «· bl n/m» saiu do estado (a ordem)");
        EXPECT_MSG(countSub(src, "%d FPS · %u TICs") == 1,
                   "ui_paineis_dock: o estado é «60 FPS · 6 TICs» (a ordem)");
        // O ESTADO EM MONO (a ordem literal; a dívida do 2.º atlas paga na
        // H — o estado agora a usa; sem mono carregado o modo cai no sans)
        EXPECT_MSG(countSub(src, "ui.monoMode(true)") == 1 &&
                       countSub(src, "ui.monoMode(false)") == 1,
                   "ui_paineis_dock: o estado «N FPS · N TICs» não está EM "
                   "MONO (o modo por-bloco tem de envolver o label)");
        EXPECT(countSub(src, "open ? theme::kTheme.accent") >= 1);
        // (f) LEMBRAR A ÚLTIMA ABA: o roundtrip do layout.json leva lastTab
        std::string dump = "bottomTab=0\ndrawerH=240\nlastTab=3\ninspector=0\n";
        editor::bottom::BottomState bs2;
        bool show = false;
        u32 coll = 0;
        EXPECT(editor::bottom::parseLayout(dump, bs2, show, coll));
        EXPECT(bs2.lastTab == 3);   // a última aba volta
        // os ficheiros antigos (sem lastTab) ficam no default 1
        std::string velho = "bottomTab=2\ndrawerH=240\ninspector=0\n";
        editor::bottom::BottomState bs3;
        EXPECT(editor::bottom::parseLayout(velho, bs3, show, coll));
        EXPECT(bs3.lastTab == 1);
    }
}

// ---- PIN ui_sheets_conteudos — A PARTE G (as folhas contidas) -------------
// «Menu, Navegador de ficheiros, lista de Cenas e todos os pop-ups: nunca
// cortados nem por baixo da barra inferior, altura máxima 85% do ecrã,
// scroll com indicador visível, botão fechar sempre acessível. Navegador:
// cabeçalho fixo, caminho, pesquisa e lista com scroll.»
TEST(ui_sheets_conteudos) {
    // (a) O TETO DE 85%: menuSheetLayout E browserSheetLayout respeitam-no
    // em 5 ecrãs, mesmo com conteúdo gigante
    const struct { i32 w, h; const char* ctx; } screens[] = {
        {776, 336, "paisagem-S"}, {1600, 720, "paisagem-M"},
        {2560, 1440, "paisagem-L"}, {720, 1600, "retrato-M"},
        {1440, 2560, "retrato-L"},
    };
    for (const auto& sc : screens) {
        theme::setDensity(1.0f);
        const f32 sw = (f32)sc.w, sh = (f32)sc.h;
        // o menu (conteúdo 3000px — nunca cabe; o teto fala)
        const editor::MenuSheetLayout ms = editor::menuSheetLayout(
            sw, sh, 0.0f, 0.0f, 0.0f, 0.0f, 3000.0f, 8.0f, 48.0f,
            theme::dp(280.0f));
        EXPECT_MSG(ms.h <= sh * editor::kSheetMaxPct + 0.5f,
                   "ui_sheets_conteudos(%s): o menu passa do teto de 85%% "
                   "(h=%.0f teto=%.0f)", sc.ctx, (double)ms.h,
                   (double)(sh * editor::kSheetMaxPct));
        EXPECT(ms.h > 0.0f);
        // o navegador (a reserva da barra: o fundo do card NUNCA desce por
        // baixo da barra de baixo)
        const f32 reserva = theme::dp(safe::kBottomTabH);
        const editor::BrowserSheetLayout lb = editor::browserSheetLayout(
            sw, sh, 0.0f, 0.0f, 0.0f, 0.0f, theme::dp(56.0f), 64u, reserva);
        EXPECT_MSG(lb.y + lb.h <= sh - reserva + 0.5f,
                   "ui_sheets_conteudos(%s): o navegador desce POR BAIXO da "
                   "barra (fundo=%.0f limite=%.0f)", sc.ctx,
                   (double)(lb.y + lb.h), (double)(sh - reserva));
        EXPECT_MSG(lb.h <= sh * editor::kSheetMaxPct + 0.5f,
                   "ui_sheets_conteudos(%s): o navegador passa do teto de "
                   "85%%", sc.ctx);
        // o Cabeçalho FIXO: o breadcrumb/pesquisa/raízes/subir têm yFixos
        // e a lista começa DEPOIS deles (a ordem vertical do C-4 intocada)
        EXPECT(lb.listTop > lb.upY);
        EXPECT(lb.upY > lb.rootsY);
        EXPECT(lb.rootsY > lb.searchY);
        EXPECT(lb.searchY > lb.crumbY);
    }
    // (b) A FONTE: os botões FECHAR vivem nos três (o menu 7411 novo, as
    // Cenas 6799, o navegador 6899) e o INDICADOR de scroll é o da casa
    {
        std::string src;
        ASSERT(readFile(repoRoot() + "/app/src/main/cpp/ui/EditorUi.cpp",
                        src));
        EXPECT_MSG(countSub(src, "kMenuCloseId") >= 2,
                   "ui_sheets_conteudos: o X de fechar vive no MENU (a faixa "
                   "fixa de 40dp — a ordem «fechar sempre acessível»)");
        ASSERT(readFile(repoRoot() + "/app/src/main/cpp/ui/UiContext.cpp",
                        src));
        EXPECT_MSG(countSub(src, "scroll::indicator") >= 1,
                   "ui_sheets_conteudos: o INDICADOR de scroll é o da casa "
                   "(o thumb da região — visível quando há o que rolar)");
    }
}

// ---- PIN ui_sobreposicao_topbar — A BARRA NÃO TAPA NADA NEM SAI ----------
// 3 resoluções × 2 orientações (a regra da fase). Os alvos vêm da FONTE
// ÚNICA topbarLayout — o MESMO desenho que a app faz.
TEST(ui_sobreposicao_topbar) {
    // 3 resoluções × 2 orientações, a partir de 640dp de largura — a
    // banda INFERIOR (<480dp, o retrato-S) é a DEGRADAÇÃO da parte B: a
    // barra nova encolhe Menu/Cena ao ícone lá (o achado fica no
    // relatório; a sobreposição extrema é curada com a barra nova)
    const struct {
        i32 w, h;
        const char* ctx;
    } screens[] = {
        {776, 336, "paisagem-S"}, {1600, 720, "paisagem-M"},
        {2560, 1440, "paisagem-L"}, {720, 1600, "retrato-M"},
        {1440, 2560, "retrato-L"},
    };
    for (const auto& sc : screens) {
        theme::setDensity(1.0f);
        const safe::Insets in{};   // sem safe-area: o pior caso geométrico
        const editor::toolbar::TopBarLayout L =
            editor::toolbar::topbarLayout((f32)sc.w, (f32)sc.h, in, false,
                                          false);
        std::vector<RectNamed> ws;
        ws.push_back({"logo", L.logo});
        ws.push_back({"menu", L.menu});
        ws.push_back({"cena", L.cena});
        ws.push_back({"tab3d", L.tab3d});
        ws.push_back({"tabUi", L.tabUi});
        ws.push_back({"tabAudio", L.tabAudio});
        ws.push_back({"undo", L.undo});
        ws.push_back({"redo", L.redo});
        ws.push_back({"save", L.save});
        ws.push_back({"play", L.play});
        ws.push_back({"stop", L.stop});
        ws.push_back({"gear", L.gear});
        // FASE UI B: o Pause e o chip «Android» SAÍRAM da barra (a play
        // bar / as Definições — a ordem do dono); a sobreposição afere a
        // direita NOVA: undo/redo/save/play/stop/gear
        // o underline do ativo DESENHA sob a tab ativa (é 2dp no fundo da
        // barra, deliberadamente sobreposto) — fora da aferição
        checkInsideAndDisjoint(ws, L.bar, sc.ctx, sc.w, sc.h);
        // e a barra INTEIRA está dentro do ecrã
        EXPECT(L.bar.x >= 0.0f && L.bar.y >= 0.0f &&
               L.bar.x + L.bar.w <= (f32)sc.w + 0.5f &&
               L.bar.y + L.bar.h <= (f32)sc.h + 0.5f);
    }
}

// ---- PIN ui_codigo_editor — A PARTE H (o EDITOR DE CÓDIGO) -----------------
// A ordem do dono: «o TIC a que pertence + os exportáveis no topo; as abas
// dos scripts abertos com a ativa em brasa; desfazer/refazer e copiar/colar
// na mesma família; fundo #0B0B0D; linha atual mais clara». O pin aferra:
//   (a) os EXPORTÁVEIS (o parser puro dos #@+ / ++@+ — a spec §4);
//   (b) o UNDO/REDO por evento (o texto/del/enter/tab empilham UM passo;
//       o redo morre com a mutação nova; o teto 100);
//   (c) as ABAS (o abrir com o editor aberto NÃO descarta — o snapshot
//       acorda no switchTab; o teto kMaxTabs derruba a mais antiga);
//   (d) os IDS fora da banda das teclas (a colisão latente do 6570 morta);
//   (e) A SOBREPOSIÇÃO: a tira de abas sob o cabeçalho, o corpo sob a tira
//       e as abas ≥44dp sem sair do ecrã (2 retratos — o editor é retrato).
//   (f) A FONTE: o follow só-no-evento (os trackers no State) e o mono do
//       2.º atlas (o modo cai para o sans sem a fonte — degradação honesta).
TEST(ui_codigo_editor) {
    namespace sw = editor::scriptwin;
    theme::setDensity(1.0f);
    editor::applyDensity();

    // (a) OS EXPORTÁVEIS: a ordem do fonte, sem duplicados, teto 6
    {
        const std::vector<std::string> ex = sw::exportedVarsOf(
            "v#@+velocidade:Num=5.5\n"
            "v#vida:Int=100\n"          // v# SEM @+ — NÃO é exportável
            "x = #@+velocidade\n"        // duplicado — morre
            "v++@+combustivel=1.0\n"     // a 2.ª forma da spec §4
            "v#@+ultimo\n");
        EXPECT(ex.size() == 3);
        EXPECT(ex[0] == "velocidade");
        EXPECT(ex[1] == "combustivel");
        EXPECT(ex[2] == "ultimo");
        EXPECT(sw::exportedVarsOf("central main { }\n").empty());
        // o teto 6: sete exportáveis → os 6 primeiros (o subtítulo é 1 linha)
        const std::vector<std::string> m7 = sw::exportedVarsOf(
            "#@+a\n#@+b\n#@+c\n#@+d\n#@+e\n#@+f\n#@+g\n");
        EXPECT(m7.size() == 6 && m7[5] == "f");
    }

    // (b) O UNDO/REDO POR EVENTO (o caminho PÚBLICO applyEvent)
    {
        Scene scene;
        const Handle tic = scene.create("ator");
        scene.get(tic)->addComponent<ScriptComp>();
        sw::State st;
        sw::open(st, scene, tic);   // o esqueleto; o undo nasce vazio
        EXPECT(st.undo.empty() && st.redo.empty());
        const std::string base = st.buf;
        const u32 caret0 = st.caret;
        ime::Event ev;
        ev.isText = true;
        ev.text = "xy";
        EXPECT(sw::applyEvent(st, ev));
        EXPECT(st.buf.size() == base.size() + 2);
        // o UNDO devolve o par exato (buffer + caret)
        EXPECT(sw::undoStep(st));
        EXPECT(st.buf == base && st.caret == caret0);
        // a mutação NOVA com o redo PENDENTE MATA-O (o clássico — o
        // M-UIh deixa o redo sobreviver aqui)
        EXPECT(sw::applyEvent(st, ev));
        EXPECT(st.buf.size() == base.size() + 2);
        EXPECT_MSG(st.redo.empty(),
                   "ui_codigo_editor: o redo SOBREVIVEU à mutação nova "
                   "(o passo morto volta a valer)");
        // o undo devolve o passo (e alimenta o redo com o estado corrente)
        EXPECT(sw::undoStep(st));
        EXPECT(st.buf == base && st.caret == caret0);
        EXPECT(!sw::undoStep(st));   // a pilha esvaziou
        // o REDO repõe o "xy" que o undo tinha retirado (o par clássico)
        EXPECT(sw::redoStep(st));
        EXPECT(st.caret == caret0 + 2);
    }

    // (c) AS ABAS: abrir com o editor aberto dorme o ativo; a troca acorda
    {
        Scene scene;
        const Handle a = scene.create("Jogador");
        scene.get(a)->addComponent<ScriptComp>();
        scene.get(a)->getComponent<ScriptComp>()->source = "central a { }\n";
        const Handle b = scene.create("Inimigo");
        scene.get(b)->addComponent<ScriptComp>();
        scene.get(b)->getComponent<ScriptComp>()->source = "central b { }\n";
        sw::State st;
        sw::open(st, scene, a);
        st.caret = 4;   // o estado editado do A
        sw::open(st, scene, b);   // A DORME, B acorda no fim do fonte dele
        EXPECT(sw::tabTotal(st) == 2u && st.activeTab == 1u);
        EXPECT(st.buf == "central b { }\n" && st.ticName ==
                   std::string("Inimigo"));
        // a troca devolve o A INTEIRO (buffer + caret + nome)
        EXPECT(sw::switchTab(st, 0));
        EXPECT(st.buf == "central a { }\n" && st.caret == 4u &&
                   st.ticName == std::string("Jogador"));
        EXPECT(!sw::switchTab(st, 0));   // a mesma aba não troca
        EXPECT(!sw::switchTab(st, 9));   // fora da faixa não troca
        // o teto: a 7.ª abertura derruba a mais antiga (fica com 6)
        for (u32 k = 0; k < 6; ++k) {
            const Handle h = scene.create("extra" + std::to_string(k));
            scene.get(h)->addComponent<ScriptComp>();
            sw::open(st, scene, h);
        }
        EXPECT(sw::tabTotal(st) == sw::State::kMaxTabs);
        EXPECT(!sw::switchTab(st, 6));   // nada além do teto
    }

    // (d) OS IDS fora da banda das teclas (o ⋯ a 6570 COLIDIA com a 11.ª
    // tecla em páginas largas — a colisão latente morre no H)
    EXPECT(sw::kHeaderDotsId > sw::kSymKeyBase + 13u);
    EXPECT(sw::kUndoId > sw::kHeaderOverflowBase + 6u);
    EXPECT(sw::kTabBase > sw::kPasteId);

    // (e+f) A SOBREPOSIÇÃO da tira de abas + a FONTE do follow/mono
    {
        std::string src;
        ASSERT(readFile(repoRoot() +
                        "/app/src/main/cpp/ui/ScriptEditor.cpp", src));
        // o FOLLOW SÓ-NO-EVENTO: o scrollSetOffset vive DENTRO do if
        // (follow) — o follow absoluto de todos os frames morreu (a cura
        // do «scroll luta com o dedo» — a causa do tremor reportada)
        EXPECT_MSG(countSub(src, "st.followCaret = st.caret;") == 1,
                   "ui_codigo_editor: o follow só-no-evento não está na "
                   "fonte (o scrollSetOffset tem de viver no if follow)");
        // o MONO do código: o modo liga no corpo e desliga no fim
        EXPECT(countSub(src, "ui.monoMode(true)") == 1);
        EXPECT(countSub(src, "ui.monoMode(false)") == 1);
        // 0.9.6.11 (cursor): o hit-test do toque mede no atlas do CÓDIGO
        // (fontWidthCode — o MESMO do corpo em mono; o fontWidth global
        // media o SANS depois do monoMode(false) e o x→coluna derivava
        // 1-2 caracteres) e o «- caretInset()» morreu do chamador (o
        // enviesamento de 2dp resolvia uma banda à esquerda de cada
        // fronteira UM carácter cedo — a regra do meio fica exata)
        EXPECT_MSG(countSub(src, "ui.fontWidthCode(one)") == 1,
                   "ui_codigo_cursor: o toque tem de medir pelo atlas do "
                   "CÓDIGO (fontWidthCode) — o fontWidth global diverge do "
                   "corpo desenhado em mono");
        EXPECT_MSG(countSub(src, "tx - xCode2 - caretInset()") == 0,
                   "ui_codigo_cursor: o -caretInset() do toque enviesa o "
                   "x 2dp à esquerda (cursor um carácter cedo na banda)");
        // a BANDA da linha corrente existe (o fundo mais claro do dono)
        EXPECT(countSub(src, "theme::kTheme.surface);") >= 1);

        const struct { i32 w, h; const char* ctx; } screens[] = {
            {720, 1600, "retrato-M"}, {1440, 2560, "retrato-L"},
        };
        for (const auto& sc : screens) {
            const char* fp = FONT_FIXTURE;
            FontAtlas font;
            ASSERT(font.loadFromPaths(&fp, 1, 28.0f));
            UiContext ui;
            ui.init();
            ui.setFont(&font);
            ui.setMonoFont(&font);   // o fixture como mono (a mesma fonte —
                                     // o contrato do MODO é o que afere)
            ui.setSafeArea(safe::Insets{});
            InputState input;
            editor::EditorState st;
            Scene scene;
            const Handle a = scene.create("Jogador");
            scene.get(a)->addComponent<ScriptComp>();
            const Handle b = scene.create("Inimigo");
            scene.get(b)->addComponent<ScriptComp>();
            sw::open(st.scriptWin, scene, a);
            sw::open(st.scriptWin, scene, b);   // 2 abas
            EXPECT(ui.monoActive() == false);  // sem monoMode ligado
            ui.monoMode(true);
            EXPECT(ui.monoActive() == true);   // o mono aceito
            ui.monoMode(false);
            ui.auditBegin("script-h", (f32)sc.w, (f32)sc.h, 0, 0, 0, 0, 1.0f);
            ui.beginFrame(nullptr, &input, (f32)sc.w, (f32)sc.h);
            sw::draw(ui, input, st.scriptWin, (f32)sc.w, (f32)sc.h,
                     1.0f / 60.0f);
            ui.endFrame();
            ui.auditEnd();
            const layout::Record& rec = ui.auditRecord();
            // as ABAS no registo: as 2 com toque ≥40dp de alto, dentro do
            // ecrã, disjuntas entre si e SOB o cabeçalho (a tira inteira)
            f32 stripTop = 1e9f, stripBot = -1.0f;
            UiRect tabs[2];
            u32 nTabs = 0;
            for (const auto& en : rec.entries) {
                if (en.kind == layout::Entry::Button &&
                    en.id >= sw::kTabBase &&
                    en.id < sw::kTabBase + sw::State::kMaxTabs) {
                    EXPECT_MSG(en.h >= 40.0f - 0.5f && en.w >= 44.0f - 0.5f,
                               "ui_codigo_editor(%s): a aba é pequena "
                               "(%.0f×%.0f — o toque mínimo da fase)",
                               sc.ctx, (double)en.w, (double)en.h);
                    EXPECT(en.x >= -0.5f &&
                           en.x + en.w <= (f32)sc.w + 0.5f);
                    EXPECT(en.y >= editor::scriptwin::kTopH - 0.5f);
                    if (nTabs < 2) tabs[nTabs] = {en.x, en.y, en.w, en.h};
                    ++nTabs;
                    stripTop = stripTop < en.y ? stripTop : en.y;
                    stripBot = stripBot > en.y + en.h ? stripBot : en.y + en.h;
                }
            }
            EXPECT_MSG(nTabs == 2,
                       "ui_codigo_editor(%s): as 2 abas no registo (%u)",
                       sc.ctx, nTabs);
            if (nTabs == 2) {
                // disjuntas (o gap 4dp entre elas — nada tapa nada)
                const bool disj =
                    tabs[0].x + tabs[0].w <= tabs[1].x + 0.5f ||
                    tabs[1].x + tabs[1].w <= tabs[0].x + 0.5f;
                EXPECT_MSG(disj, "ui_codigo_editor(%s): as abas tocam-se",
                           sc.ctx);
            }
            // a tira sob o cabeçalho e o CORPO não sobe para ela: o scroll
            // do código começa onde a tira acaba (a fonte: body.y do draw)
            EXPECT(stripTop >= editor::scriptwin::kTopH - 0.5f);
            EXPECT(stripBot <= (f32)sc.h);
            // o VALIDADOR da casa: 0 problemas no frame inteiro do editor
            const auto probs = layout::validate(rec);
            EXPECT_MSG(probs.empty(),
                       "ui_codigo_editor(%s): o validador apanhou %zu "
                       "problemas", sc.ctx, probs.size());
        }
    }
}
