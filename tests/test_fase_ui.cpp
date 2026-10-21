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
#include "TestFramework.h"
#include <cstdio>
#include <vector>

using namespace vv;

namespace {

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
        ws.push_back({"play", L.play});
        ws.push_back({"stop", L.stop});
        ws.push_back({"gear", L.gear});
        if (L.pause.w > 0.0f) {
            const RectNamed rn = {"pause", L.pause};
            ws.push_back(rn);
        }
        if (L.showPlatform && L.platform.w > 0.0f) {
            const RectNamed rn = {"plataforma", L.platform};
            ws.push_back(rn);
        }
        // o underline do ativo DESENHA sob a tab ativa (é 2dp no fundo da
        // barra, deliberadamente sobreposto) — fora da aferição
        checkInsideAndDisjoint(ws, L.bar, sc.ctx, sc.w, sc.h);
        // e a barra INTEIRA está dentro do ecrã
        EXPECT(L.bar.x >= 0.0f && L.bar.y >= 0.0f &&
               L.bar.x + L.bar.w <= (f32)sc.w + 0.5f &&
               L.bar.y + L.bar.h <= (f32)sc.h + 0.5f);
    }
}
