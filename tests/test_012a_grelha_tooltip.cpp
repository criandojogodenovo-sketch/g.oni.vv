// tests/test_012a_grelha_tooltip.cpp — 0.12-A (A-3): a grelha que NÃO vaza
// por trás do Inspector + o tooltip que MORRE — OS PINS.
//
// O diagnóstico (§4 do RELATORIO-0.12-A): no modo UI o painel direito vive
// DENTRO do viewRect (o inspRight do editor 3D é falso em UI — o contrato
// WYSIWYG) e o scissor do pass 3D cortava só ao viewRect — a grelha 3D
// ficava POR TRÁS do vidro do Inspector (a0.80) e vazava (o C33 do dono).
// E a legenda do gizmo ("Mover") era PERMANENTE — o dono lia-a como tooltip
// preso. Os pins aferram:
//   1. grelha_nao_vaza — o scissor3dRect no modo UI com painel NÃO se
//      sobrepõe ao rect do Inspector de UI (nenhum pixel 3D atrás do
//      vidro); fora do modo UI é o viewRect de sempre (byte a byte);
//   2. tooltip_morre_1500ms — a escolha da ferramenta arma o tooltip no
//      largar; aos 1,5s REAIS (dt somado frame a frame) os glifos sumiram;
//   3. tooltip_morre_release — o largar de um toque NOVO mata o tooltip
//      imediatamente (antes do deadline).
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <vector>

#include "platform/InputState.h"
#include "render/Camera.h"
#include "ui/EditorUi.h"
#include "ui/FontAtlas.h"
#include "ui/SafeArea.h"
#include "ui/Toolbar.h"
#include "ui/UiContext.h"
#include "ui/ViewportChrome.h"

using namespace vv;
using namespace vv::editor;
using ::test::nearEqF;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;

struct GlyphQ { f32 x0, y0, x1, y1; };

void collectGlyphs(const QuadBatch& b, std::vector<GlyphQ>& out) {
    const QuadVertex* v = b.vertices();
    const u32 n = b.vertexCount();
    for (u32 i = 0; i + 5 < n; i += 6) {
        out.push_back({v[i].x, v[i].y, v[i + 2].x, v[i + 2].y});
    }
}

// o chrome do viewport desenhado com o MESMO caminho do main (o input e o
// dt REAIS entram — o ciclo do tooltip é o código de produção)
struct Env {
    FontAtlas  font;
    UiContext  ui;
    InputState input;
    EditorState st;
    toolbar::GizmoModeState gz;
    bool ok = false;

    Env() {
        const char* fp = FONT_FIXTURE;
        ok = font.loadFromPaths(&fp, 1, 28.0f);
        if (!ok) {
            return;
        }
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
    }

    void frame(f32 dt) {
        ui.beginFrame(nullptr, &input, kSW, kSH);
        vpchrome::ChromeState cs;
        vpchrome::draw(ui, st, gz, cs, cam, 0.0f, dt, &input);
        ui.endFrame();
        input.clearEdges();
    }

    // o LAYOUT do chrome (o MESMO cálculo que o draw consome)
    vpchrome::Layout lay() {
        const UiRect vpView = safe::centerRect(
            kSW, kSH, safe::Insets{}, 0.0f, st.showInspector, st.hierW,
            st.inspW, inspectorCollapsed(st));
        return vpchrome::layout(vpView);
    }

    UiRect moverBtn() {
        return lay().rail[1];
    }

    bool hasGlyphsIn(const UiRect& r) {
        std::vector<GlyphQ> gs;
        collectGlyphs(ui.glyphsForTest(), gs);
        for (const GlyphQ& g : gs) {
            const f32 cx = (g.x0 + g.x1) * 0.5f;
            const f32 cy = (g.y0 + g.y1) * 0.5f;
            if (cx >= r.x && cx <= r.x + r.w && cy >= r.y &&
                cy <= r.y + r.h) {
                return true;
            }
        }
        return false;
    }

    void tap(f32 x, f32 y) {
        input.injectDown(0, x, y);
        frame(1.0f / 60.0f);
        input.injectUp(0);
        frame(1.0f / 60.0f);
    }

    Camera cam;
};

} // namespace

// ---- PIN 1 — a grelha NÃO vaza por trás do Inspector (o scissor) ------------
TEST(grelha_nao_vaza) {
    constexpr safe::Insets kIns{};
    // (a) o EDITOR 3D é byte a byte: o scissor É o viewRect
    {
        const UiRect view{200.0f, 104.0f, 900.0f, 500.0f};
        const UiRect s = safe::scissor3dRect(view, /*uiMode=*/false, true,
                                             kSW, kSH, kIns, 0.0f, -1.0f,
                                             -1.0f);
        EXPECT(nearEqF(s.x, view.x));
        EXPECT(nearEqF(s.y, view.y));
        EXPECT(nearEqF(s.w, view.w));
        EXPECT(nearEqF(s.h, view.h));
    }
    // (b) o modo UI SEM painel direito: a mesma área (nada muda)
    {
        const UiRect view = safe::centerRect(kSW, kSH, kIns, 0.0f, false,
                                             -1.0f, -1.0f, false);
        const UiRect s = safe::scissor3dRect(view, true, false, kSW, kSH,
                                             kIns, 0.0f, -1.0f, -1.0f);
        EXPECT(nearEqF(s.x, view.x));
        EXPECT(nearEqF(s.w, view.w));
    }
    // (c) o modo UI COM painel direito: o scissor TERMINA antes do painel —
    // nenhum pixel 3D por trás do vidro (o contrato do C33)
    {
        const UiRect view = safe::centerRect(kSW, kSH, kIns, 0.0f, false,
                                             -1.0f, -1.0f, false);
        const UiRect panel = safe::inspectorPanelRect(kSW, kSH, kIns, 0.0f,
                                                      -1.0f, -1.0f);
        EXPECT(panel.w > 1.0f);   // o painel existe (o pressuposto)
        const UiRect s = safe::scissor3dRect(view, true, true, kSW, kSH,
                                             kIns, 0.0f, -1.0f, -1.0f);
        EXPECT(s.w < view.w);   // a largura do painel saiu do scissor
        // a aresta direita do scissor não entra NO rect do painel
        EXPECT(s.x + s.w <= panel.x + 0.5f);
        // e o scissor continua a cobrir o resto (a vista 2D não encolheu
        // mais do que o painel pedido)
        EXPECT(s.w >= view.w - panel.w - 1.0f);
    }
}

// ---- PIN 2 — o tooltip morre aos 1,5s (dt REAL, frame a frame) --------------
TEST(tooltip_morre_1500ms) {
    Env e;
    ASSERT(e.ok);
    const UiRect btn = e.moverBtn();
    EXPECT(btn.w > 1.0f);   // o rail existe (o pressuposto do layout)

    // o toque de escolha: down → frame (o widget RECLAMA o gesto) → up →
    // frame (o widgetHit dispara no LARGAR — arm) → frame (o ciclo nasce)
    e.input.injectDown(0, btn.x + btn.w * 0.5f, btn.y + btn.h * 0.5f);
    e.frame(1.0f / 60.0f);
    e.input.injectUp(0);
    e.frame(1.0f / 60.0f);
    e.frame(1.0f / 60.0f);   // o frame seguinte ao largar: o tooltip NASCE
    EXPECT(e.st.legendTimer > 1.4f && e.st.legendTimer <= 1.5f);   // nasceu
    {
        const vpchrome::Layout L = e.lay();
        EXPECT(L.legendVisible);          // o layout tem a área da legenda
        EXPECT(e.hasGlyphsIn(L.legend));  // o «Mover» ESTÁ no ecrã
    }

    // 1,0s depois (60 frames): AINDA VIVO (o timer não é instantâneo)
    for (int i = 0; i < 60; ++i) {
        e.frame(1.0f / 60.0f);
    }
    EXPECT(e.st.legendTimer > 0.0f);
    EXPECT(e.st.legendTimer < 0.6f);   // ~0,5s restantes

    // até aos 1,5s+ (mais 60 frames de 1/60 = 1,0s): MORREU pelo timer
    for (int i = 0; i < 60; ++i) {
        e.frame(1.0f / 60.0f);
    }
    EXPECT(nearEqF(e.st.legendTimer, 0.0f, 0.01f));
    {
        const vpchrome::Layout L = e.lay();
        EXPECT(!e.hasGlyphsIn(L.legend));   // e os glifos SUMIRAM
    }
}

// ---- PIN 3 — o largar de um toque NOVO mata o tooltip ------------------------
TEST(tooltip_morre_release) {
    Env e;
    ASSERT(e.ok);
    const UiRect btn = e.moverBtn();

    // nasce igual ao pin 2 (down → up → o frame do nascimento)
    e.input.injectDown(0, btn.x + btn.w * 0.5f, btn.y + btn.h * 0.5f);
    e.frame(1.0f / 60.0f);
    e.input.injectUp(0);
    e.frame(1.0f / 60.0f);
    e.frame(1.0f / 60.0f);
    EXPECT(e.st.legendTimer > 1.4f);   // vivo

    // um toque NOVO em qualquer lado (o meio do viewport): o LARGAR mata
    e.input.injectDown(0, 800.0f, 400.0f);
    e.frame(1.0f / 60.0f);
    EXPECT(e.st.legendTimer > 0.0f);   // o down sozinho não mata
    e.input.injectUp(0);
    e.frame(1.0f / 60.0f);
    EXPECT(nearEqF(e.st.legendTimer, 0.0f, 0.001f));   // morreu no largar

    // e o release de nascimento NÃO mata (o MESMO edge do toque de escolha):
    // segunda escolha, o tooltip nasce e sobrevive ao próprio largar
    e.input.injectDown(0, btn.x + btn.w * 0.5f, btn.y + btn.h * 0.5f);
    e.frame(1.0f / 60.0f);
    e.input.injectUp(0);
    e.frame(1.0f / 60.0f);
    e.frame(1.0f / 60.0f);
    EXPECT(e.st.legendTimer > 1.4f);   // nasceu de novo (o ciclo recomeça)
}
