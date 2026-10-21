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
