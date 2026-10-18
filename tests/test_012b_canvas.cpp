// tests/test_012b_canvas.cpp — 0.12-B (B-3): O CANVAS com a moldura do device.
//
// Os 3 pins da sub-fase B-3 (a ordem do prompt):
//   1. canvas_moldura          — a proporção Android escolhida desenha a moldura
//   2. canvas_guias            — o toggle mostra as guias
//   3. canvas_toast_contraste  — o toast tem contraste ≥ 4.5
//
// MUTAÇÕES (coladas no RELATORIO-0.12-B.md):
//   M-B3a — a moldura removida → pin 1 vermelho
//   M-B3b — o toast volta ao cinza-vidro → pin 3 vermelho
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <vector>
#include "components/Control.h"
#include "components/UIRoot.h"
#include "components/UiCanvas.h"
#include "components/TouchControls.h"
#include "core/Scene.h"
#include "platform/InputState.h"
#include "ui/EditorUi.h"
#include "ui/UiEditor.h"
#include "ui/UiRuntime.h"
#include "ui/ViewportChrome.h"
#include "ui/FontAtlas.h"
#include "ui/UiContext.h"
#include "ui/Theme.h"

using namespace vv;
using namespace vv::editor;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;

bool nearEqF(f32 a, f32 b, f32 eps = 0.01f) {
    return std::fabs(a - b) < eps;
}

struct RectC {
    f32 x0, y0, x1, y1;
    f32 r = 0, g = 0, b = 0, a = 0;
};

void collectRectsC(const QuadBatch& b, std::vector<RectC>& out) {
    const QuadVertex* v = b.vertices();
    const u32 n = b.vertexCount();
    for (u32 i = 0; i + 5 < n; i += 6) {
        out.push_back({v[i].x, v[i].y, v[i + 2].x, v[i + 2].y,
                       v[i].r, v[i].g, v[i].b, v[i].a});
    }
}

struct CvEnv {
    FontAtlas  font;
    UiContext  ui;
    InputState input;
    Scene      scene;
    EditorState st;
    bool       ok = false;

    CvEnv() {
        const char* fontPath = FONT_FIXTURE;
        ok = font.loadFromPaths(&fontPath, 1, 28.0f);
        if (!ok) {
            return;
        }
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
    }

    void frame() {
        ui.beginFrame(nullptr, &input, kSW, kSH);
        drawUiViewport(ui, scene, st, input, kSW, kSH);
        ui.statusLine("s");
        ui.endFrame();
        input.clearEdges();
    }
    void tap(f32 x, f32 y) {
        input.injectDown(0, x, y);
        frame();
        input.injectUp(0);
        frame();
    }
};

} // namespace

// ---- 1. canvas_moldura ---------------------------------------------------------

TEST(canvas_moldura) {
    // a MATEMÁTICA pura: os presets e o rect centrado (o lado maior casa)
    EXPECT(kUiDeviceAspectCount == 4u);
    EXPECT(nearEqF(uiDeviceAspectValue(0), 0.0f, 0.0001f));
    EXPECT(nearEqF(uiDeviceAspectValue(1), 16.0f / 9.0f, 0.0001f));
    EXPECT(nearEqF(uiDeviceAspectValue(2), 2.0f, 0.0001f));
    EXPECT(nearEqF(uiDeviceAspectValue(3), 20.0f / 9.0f, 0.0001f));
    EXPECT(std::string(uiDeviceAspectName(0)) == "livre");
    EXPECT(std::string(uiDeviceAspectName(1)) == "16:9");

    // o design 1600×720 (20:9 ≈ 2.222):
    // 16:9 (1.778) é MAIS ESTREITO → a moldura tem a ALTURA toda (720),
    // a largura 720×16/9 = 1280, centrada (x=160)
    const UiRect f1 = uiDeviceFrameRect(1, kSW, kSH);
    EXPECT(nearEqF(f1.x, 160.0f, 0.5f));
    EXPECT(nearEqF(f1.y, 0.0f, 0.5f));
    EXPECT(nearEqF(f1.w, 1280.0f, 0.5f));
    EXPECT(nearEqF(f1.h, 720.0f, 0.5f));
    EXPECT(nearEqF(f1.w / f1.h, 16.0f / 9.0f, 0.01f));
    // 18:9 (2.0) → largura 1440, x=80
    const UiRect f2 = uiDeviceFrameRect(2, kSW, kSH);
    EXPECT(nearEqF(f2.x, 80.0f, 0.5f));
    EXPECT(nearEqF(f2.w, 1440.0f, 0.5f));
    // 20:9 == o design → a moldura é o design INTEIRO
    const UiRect f3 = uiDeviceFrameRect(3, kSW, kSH);
    EXPECT(nearEqF(f3.x, 0.0f, 0.5f) && nearEqF(f3.w, kSW, 0.5f));
    EXPECT(nearEqF(f3.y, 0.0f, 0.5f) && nearEqF(f3.h, kSH, 0.5f));
    // livre (0) → o design inteiro
    const UiRect f0 = uiDeviceFrameRect(0, kSW, kSH);
    EXPECT(nearEqF(f0.w, kSW, 0.5f) && nearEqF(f0.h, kSH, 0.5f));

    // o DRAW: com a 16:9 escolhida, o véu escuro FORA da moldura existe (os
    // 2 bandos laterais com alfa 0.55) e a borda da moldura é o ACCENT
    CvEnv e;
    ASSERT(e.ok);
    ASSERT(uiAddTreeElement(e.scene, e.st, 2, kSW, kSH));   // um button
    e.st.uiDeviceAspect = 1;
    e.frame();
    std::vector<RectC> rects;
    collectRectsC(e.ui.solidsForTest(), rects);
    // o rect do viewport (o MESMO centerRect do draw) e a escala
    const UiRect view = safe::centerRect(kSW, kSH, safe::Insets{}, 0.0f,
                                         e.st.showInspector, e.st.hierW,
                                         e.st.inspW);
    const ViewportTransform t = uiViewportTransform(view, kSW, kSH);
    const f32 frameX0 = t.ox + 160.0f * t.scale;
    // o bando ESQUERDO: [t.ox, frameX0) com alfa ~0.55
    bool veilEsq = false;
    for (const RectC& r : rects) {
        if (nearEqF(r.x0, t.ox, 1.0f) && nearEqF(r.x1, frameX0, 1.0f) &&
            nearEqF(r.a, 0.55f, 0.02f)) {
            veilEsq = true;
        }
    }
    EXPECT(veilEsq);
    // livre → SEM véu
    e.st.uiDeviceAspect = 0;
    e.frame();
    rects.clear();
    collectRectsC(e.ui.solidsForTest(), rects);
    for (const RectC& r : rects) {
        EXPECT(!(nearEqF(r.a, 0.55f, 0.02f) && r.x1 - r.x0 > 20.0f));
    }
}

// ---- 2. canvas_guias -----------------------------------------------------------

TEST(canvas_guias) {
    CvEnv e;
    ASSERT(e.ok);
    // a SAFE AREA NÃO-NULA: as guias desenham ao INSET dela (distintas da
    // borda da moldura — o detetor aferra a POSIÇÃO, não só a forma)
    const safe::Insets sina{10.0f, 12.0f, 14.0f, 16.0f};
    e.ui.setSafeArea(sina);
    ASSERT(uiAddTreeElement(e.scene, e.st, 2, kSW, kSH));
    const UiRect view = safe::centerRect(kSW, kSH, sina, 0.0f,
                                         e.st.showInspector, e.st.hierW,
                                         e.st.inspW);
    const UiCanvasChips chips = uiCanvasChipsRect(view);
    e.frame();

    // o toggle pelas CHIPS (o gesto do dono): o tap alterna
    EXPECT(!e.st.uiSafeGuides);
    e.tap(chips.guides.x + chips.guides.w * 0.5f,
          chips.guides.y + chips.guides.h * 0.5f);
    EXPECT(e.st.uiSafeGuides);
    // com a moldura LIGADA as guias desenham: a guia ESQUERDA vive na
    // coluna de design x = ins.left = 10 (DENTRO da moldura 16:9 — que
    // começa em x=160; a borda dela está em OUTRA coluna)
    e.st.uiDeviceAspect = 1;
    e.frame();
    std::vector<RectC> rects;
    collectRectsC(e.ui.solidsForTest(), rects);
    const ViewportTransform t = uiViewportTransform(view, kSW, kSH);
    // a guia ESQUERDA vive ao INSET DA MOLURA: design x = fr.x(160) + 10 = 170
    const f32 guiaX = t.ox + (160.0f + 10.0f) * t.scale;
    bool guiaExiste = false;
    for (const RectC& r : rects) {
        if (r.x1 - r.x0 <= 2.0f && r.y1 - r.y0 > 100.0f &&
            nearEqF(r.x0, guiaX, 2.0f)) {
            guiaExiste = true;   // a hairline VERTICAL na coluna do inset
        }
    }
    EXPECT(guiaExiste);
    // o tap de novo → as guias MORREM (a borda da moldura fica — noutra
    // coluna: a distintação é a POSIÇÃO)
    e.tap(chips.guides.x + chips.guides.w * 0.5f,
          chips.guides.y + chips.guides.h * 0.5f);
    EXPECT(!e.st.uiSafeGuides);
    e.frame();
    rects.clear();
    collectRectsC(e.ui.solidsForTest(), rects);
    guiaExiste = false;
    for (const RectC& r : rects) {
        if (r.x1 - r.x0 <= 2.0f && r.y1 - r.y0 > 100.0f &&
            nearEqF(r.x0, guiaX, 2.0f)) {
            guiaExiste = true;
        }
    }
    EXPECT(!guiaExiste);
    // a GRELHA: o toggle próprio (as hairlines de 40px dentro da moldura)
    e.tap(chips.grid.x + chips.grid.w * 0.5f,
          chips.grid.y + chips.grid.h * 0.5f);
    EXPECT(e.st.uiAlignGrid);
    e.st.uiDeviceAspect = 1;
    e.frame();
    rects.clear();
    collectRectsC(e.ui.solidsForTest(), rects);
    // as linhas VERTICAIS da grelha: a cada 40px de design DENTRO da moldura
    u32 gridLines = 0;
    for (const RectC& r : rects) {
        const f32 dx = (r.x0 - t.ox) / (t.scale > 0.0f ? t.scale : 1.0f);
        if (r.x1 - r.x0 <= 2.0f && r.y1 - r.y0 > 50.0f &&
            nearEqF(std::fmod(dx, 40.0f), 0.0f, 1.0f)) {
            ++gridLines;
        }
    }
    EXPECT(gridLines >= 2u);   // 200, 240, … (múltiplos de 40 na moldura 16:9)
    (void)chips;
}

// ---- 3. canvas_toast_contraste -------------------------------------------------

TEST(canvas_toast_contraste) {
    // a FONTE ÚNICA do toast (o draw do main consome estas funções):
    // OPACO + o rácio WCAG do par ≥ 4.5 (o piso do prompt)
    EXPECT(nearEqF(editor::vpchrome::toastBgAlpha(), 1.0f, 0.0001f));
    f32 bg[4], tx[4];
    editor::vpchrome::toastColors(bg, tx);
    // o bg é o TOKEN elevated (0.12-C: o topo da escada de superfícies —
    // o B-3 dizia surface2 e a C-3 dá o token próprio) COM o alfa 1
    EXPECT(nearEqF(bg[0], theme::kTheme.elevated[0], 0.0001f));
    EXPECT(nearEqF(bg[1], theme::kTheme.elevated[1], 0.0001f));
    EXPECT(nearEqF(bg[2], theme::kTheme.elevated[2], 0.0001f));
    EXPECT(nearEqF(bg[3], 1.0f, 0.0001f));
    // o texto é o text1 OPACO (o fade antigo misturava a cena)
    EXPECT(nearEqF(tx[0], theme::kTheme.text1[0], 0.0001f));
    EXPECT(nearEqF(tx[3], 1.0f, 0.0001f));
    // O RÁCIO (o pin do prompt): o WCAG (L1+0.05)/(L2+0.05) do par
    const f32 ratio = editor::vpchrome::toastContrastRatio();
    EXPECT(ratio >= 4.5f);
    // o sanity da matemática: branco sobre preto = 21 (o teto WCAG)
    EXPECT(ratio <= 21.0f);
}
