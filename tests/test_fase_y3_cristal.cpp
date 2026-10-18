// tests/test_fase_y3_cristal.cpp — os pins da Y-3 (0.11-Y: OS CARTÕES de
// cristal — o dono: «painéis com borda brilhante e separadores com
// gradiente»). A APLICAÇÃO das primitivas da Y-1 aos painéis reais:
//   • a LINHA DO TOPO (glassTopLine — 1dp do glow a 20%) depois do painel
//     de vidro: a HIERARQUIA, o INSPECTOR, o DRAWER, o card CRIAR e os 3
//     sheets;
//   • o DIVISOR com gradiente (dividerGradientV — 1dp do âmbar a CAIR do
//     topo: accentDim α0.90 → α0.15): o divisor direito da hierarquia, o
//     esquerdo do inspector e a linha em repouso de drawPanelDividers (a
//     ordem do dono SUPERADA a linha sólida do D11 — a pill do drag fica).
//
// As MUTAÇÕes do relatório (M-Y3a: a linha do topo morta na hierarquia;
// M-Y3b: o divisor de volta à linha sólida) correm contra o CÓDIGO
// compilado (rebuild + corrida + reverso — o padrão M-Y1d/M-Y2a/b).
#include "TestFramework.h"

#include "platform/InputState.h"
#include "render/QuadBatch.h"
#include "ui/EditorUi.h"
#include "ui/FontAtlas.h"
#include "ui/SafeArea.h"
#include "ui/Theme.h"
#include "ui/UiContext.h"

#include "core/Scene.h"

using namespace vv;

namespace {

const char* kFontPath = FONT_FIXTURE;

struct QuadScan {
    const QuadBatch& b;
    explicit QuadScan(const UiContext& ui) : b(ui.solidsForTest()) {}

    u32 count() const { return b.vertexCount() / 6u; }

    // o quad i: [x,y,w,h] + as cores dos vértices de topo e de fundo
    bool quadAt(u32 i, f32& x, f32& y, f32& w, f32& h, f32 top[4],
                f32 bot[4]) const {
        const QuadVertex* v = b.vertices() + i * 6;
        f32 mnX = 1e9f, mxX = -1e9f, mnY = 1e9f, mxY = -1e9f;
        for (u32 k = 0; k < 6; ++k) {
            mnX = v[k].x < mnX ? v[k].x : mnX;
            mxX = v[k].x > mxX ? v[k].x : mxX;
            mnY = v[k].y < mnY ? v[k].y : mnY;
            mxY = v[k].y > mxY ? v[k].y : mxY;
        }
        x = mnX;
        y = mnY;
        w = mxX - mnX;
        h = mxY - mnY;
        // o 1.º vértice é o topo-esq; o 2.º o fundo-esq (a convenção do
        // quad/quadGradientV)
        top[0] = v[0].r;
        top[1] = v[0].g;
        top[2] = v[0].b;
        top[3] = v[0].a;
        bot[0] = v[1].r;
        bot[1] = v[1].g;
        bot[2] = v[1].b;
        bot[3] = v[1].a;
        return true;
    }
};

bool sameC(const f32* a, f32 r, f32 g, f32 b, f32 al, f32 eps = 1e-3f) {
    return test::nearEqF(a[0], r, eps) && test::nearEqF(a[1], g, eps) &&
           test::nearEqF(a[2], b, eps) && test::nearEqF(a[3], al, eps);
}

struct CristalEnv {
    FontAtlas  font;
    UiContext  ui;
    InputState input;
    Scene      scene;
    editor::EditorState st;
    bool ok = false;

    CristalEnv() {
        ok = font.loadFromPaths(&kFontPath, 1, 28.0f);
        if (!ok) return;
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
        theme::setDensity(1.0f);
    }

    void begin() { ui.beginFrame(nullptr, &input, 1536.0f, 720.0f); }
};

// procura no batch UM quad com o predicado dado
template <typename P>
bool findQuad(const UiContext& ui, P pred) {
    QuadScan s(ui);
    for (u32 i = 0; i < s.count(); ++i) {
        f32 x, y, w, h, top[4], bot[4];
        s.quadAt(i, x, y, w, h, top, bot);
        if (pred(x, y, w, h, top, bot)) return true;
    }
    return false;
}

} // namespace

// ---- PIN 1 — a HIERARQUIA é o cristal (topo brilhante + divisor a cair) --
TEST(y3_cristal_hierarquia) {
    CristalEnv e;
    ASSERT(e.ok);
    const UiRect p =
        safe::hierarchyPanelRect(1536.0f, 720.0f, safe::Insets{}, 0.0f);
    e.begin();
    editor::drawHierarchy(e.ui, e.scene, e.st);

    // (a) a LINHA DO TOPO: o quad 1dp no TOPO do painel com o RGB do glow
    // a α0.20, com a LARGURA do painel
    // 0.12-E (E-1): a recalibração D-2 reverte — o vidro falso MORREU com
    // o SDF abortado; o cristal volta à linha 1dp do topo
    const bool topo = findQuad(e.ui, [&](f32 x, f32 y, f32 w, f32 h,
                                         const f32 top[4], const f32 bot[4]) {
        return test::nearEqF(y, p.y, 0.05f) &&
               test::nearEqF(h, 1.0f, 0.05f) &&
               test::nearEqF(x, p.x, 0.05f) &&
               test::nearEqF(w, p.w, 0.05f) &&
               sameC(top, 1.0f, 176.0f / 255.0f, 32.0f / 255.0f, 0.20f);
    });
    EXPECT_MSG(topo,
               "y3_hierarquia: a LINHA DO TOPO do cristal (1dp do âmbar a "
               "20% na largura do painel — FASE 3 do E) não foi "
               "encontrada no batch");

    // (b) o DIVISOR direito: o quad 1dp de largura na borda direita do
    // painel com o GRADIENTE (topo accentDim α0.90 → fundo α0.15)
    const f32 divX = p.x + p.w - 1.0f;
    const bool div = findQuad(e.ui, [&](f32 x, f32 y, f32 w, f32 h,
                                        const f32 top[4], const f32 bot[4]) {
        return test::nearEqF(x, divX, 0.05f) &&
               test::nearEqF(w, 1.0f, 0.05f) &&
               test::nearEqF(y, p.y, 0.05f) &&
               test::nearEqF(h, p.h, 0.05f) &&
               sameC(top, 74.0f / 255.0f, 55.0f / 255.0f, 20.0f / 255.0f,
                     0.90f) &&
               sameC(bot, 74.0f / 255.0f, 55.0f / 255.0f, 20.0f / 255.0f,
                     0.15f);
    });
    EXPECT_MSG(div,
               "y3_hierarquia: o DIVISOR com gradiente (accentDim a0.90 "
               "-> a0.15, 1dp na borda direita) não foi encontrado");

    // (c) A LINHA SÓLIDA MORREU: nenhum quad 1dp-wide full-height com cor
    // ÚNICA de border (a linha sólida do D11 é superada — o gradiente é
    // o separador)
    const bool solida = findQuad(e.ui, [&](f32 x, f32, f32 w, f32 h,
                                           const f32 top[4],
                                           const f32 bot[4]) {
        return test::nearEqF(w, 1.0f, 0.05f) && h > p.h * 0.9f &&
               sameC(top, 74.0f / 255.0f, 55.0f / 255.0f, 20.0f / 255.0f,
                     0.60f) &&
               sameC(bot, 74.0f / 255.0f, 55.0f / 255.0f, 20.0f / 255.0f,
                     0.60f);
    });
    EXPECT_MSG(!solida,
               "y3_hierarquia: a linha SÓLIDA do divisor (uniforme a0.60) "
               "ainda existe — a Y-3 manda o gradiente");
    e.ui.endFrame();
}

// ---- PIN 2 — o INSPECTOR segue o MESMO contrato (topo + divisor esq.) ----
TEST(y3_cristal_inspector) {
    CristalEnv e;
    ASSERT(e.ok);
    e.st.selected = e.scene.create("Alvo");
    const UiRect p =
        safe::inspectorPanelRect(1536.0f, 720.0f, safe::Insets{}, 0.0f);
    e.begin();
    editor::drawInspector(e.ui, e.scene, e.st, nullptr);

    // a LINHA DO TOPO (o MESMO contrato da hierarquia — o vidro falso da
    // D-2 morreu com o SDF; o cristal 1dp do glow a 20% volta)
    const bool topo = findQuad(e.ui, [&](f32 x, f32 y, f32 w, f32 h,
                                         const f32 top[4], const f32 bot[4]) {
        return test::nearEqF(y, p.y, 0.05f) &&
               test::nearEqF(h, 1.0f, 0.05f) &&
               test::nearEqF(x, p.x, 0.05f) &&
               test::nearEqF(w, p.w, 0.05f) &&
               sameC(top, 1.0f, 176.0f / 255.0f, 32.0f / 255.0f, 0.20f);
    });
    EXPECT_MSG(topo, "y3_inspector: a linha do topo do cristal em falta");
    const bool div = findQuad(e.ui, [&](f32 x, f32 y, f32 w, f32 h,
                                        const f32 top[4], const f32 bot[4]) {
        return test::nearEqF(x, p.x, 0.05f) &&
               test::nearEqF(w, 1.0f, 0.05f) &&
               test::nearEqF(h, p.h, 0.05f) &&
               sameC(top, 74.0f / 255.0f, 55.0f / 255.0f, 20.0f / 255.0f,
                     0.90f) &&
               sameC(bot, 74.0f / 255.0f, 55.0f / 255.0f, 20.0f / 255.0f,
                     0.15f);
    });
    EXPECT_MSG(div, "y3_inspector: o divisor com gradiente em falta");
    e.ui.endFrame();
}

// ---- PIN 3 — o DIVISOR EM REPOUSO de drawPanelDividers é o gradiente ----
TEST(y3_cristal_divisores) {
    CristalEnv e;
    ASSERT(e.ok);
    e.begin();
    // o divisor ESQUERDO existe sempre (o strip da hierarquia)
    const safe::PanelBudget b =
        safe::resolvePanels(1536.0f, -1.0f, -1.0f, false);
    const f32 stripX = b.hier - theme::dp(12.0f);   // kDividerStripW
    editor::drawPanelDividers(e.ui, e.st, false);
    const bool div = findQuad(e.ui, [&](f32 x, f32 y, f32 w, f32 h,
                                        const f32 top[4], const f32 bot[4]) {
        return test::nearEqF(x, stripX, 0.05f) &&
               test::nearEqF(w, 1.0f, 0.05f) &&
               test::nearEqF(y, safe::panelsRect(1536.0f, 720.0f,
                                                 safe::Insets{}, 0.0f).y,
                             0.05f) &&
               sameC(top, 74.0f / 255.0f, 55.0f / 255.0f, 20.0f / 255.0f,
                     0.90f) &&
               sameC(bot, 74.0f / 255.0f, 55.0f / 255.0f, 20.0f / 255.0f,
                     0.15f);
    });
    EXPECT_MSG(div,
               "y3_divisores: a linha em repouso do drawPanelDividers tem "
               "de ser o GRADIENTE (a0.90 -> a0.15) — a sólida do D11 "
               "voltou?");
    e.ui.endFrame();
}
