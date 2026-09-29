// tests/test_ui.cpp — F5.0-fix: a UI REAL (FontAtlas + UiContext + EditorUi)
// corrida no hospedeiro com uma FONTE REAL a 28 px (igual ao device) e gestos
// injetados (InputState::inject*).
//
// REGRESSÃO DO C33: no device, o texto do Inspector aparecia sobreposto em
// pilhas (nome×Transform3D, tex×input, velx×tc). Causa raiz: linhas de 26 px
// para uma fonte de 28 px — o bloco de glifos invadia a linha de cima. Aqui
// desenhamos o frame completo e aferimos a geometria REAL dos glifos emitidos
// (batches de CPU): ZERO colisões glifo-a-glifo. No código antigo este teste
// apanhava 13 colisões com a mesma fonte.
#include "TestFramework.h"
#include "ui/UiContext.h"
#include "ui/EditorUi.h"
#include "ui/FontAtlas.h"
#include "ui/SafeArea.h"
#include "ui/EditorLayout.h"
#include "platform/InputState.h"
#include "core/Scene.h"
#include "core/Presets.h"
#include "components/Transform3D.h"
#include "components/MeshRenderer.h"
#include "components/InputMap.h"
#include "components/BodyComp.h"
#include "components/TouchControls.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace vv;
using namespace vv::editor;
using ::test::nearEqF;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;
constexpr f32 kFontPx = 28.0f;   // o MESMO heightPx do main.cpp no device

struct Rect { f32 x0, y0, x1, y1; };

void collectRects(const QuadBatch& b, std::vector<Rect>& out) {
    const QuadVertex* v = b.vertices();
    const u32 n = b.vertexCount();
    for (u32 i = 0; i + 5 < n; i += 6) {
        out.push_back({v[i].x, v[i].y, v[i + 2].x, v[i + 2].y});
    }
}

// pior interseção glifo×glifo (px, min(dx,dy) do par que MAIS invade).
// Glifos da MESMA palavra nunca colidem (avanço horizontal); qualquer
// colisão 2D real > ruído de AA indica sobreposição de linhas.
f32 worstGlyphPenetration(const UiContext& ui, f32& ox, f32& oy) {
    std::vector<Rect> g;
    collectRects(ui.glyphsForTest(), g);
    f32 worst = 0.0f;
    ox = oy = 0.0f;
    for (size_t i = 0; i < g.size(); ++i) {
        for (size_t j = i + 1; j < g.size(); ++j) {
            const f32 dx = std::min(g[i].x1, g[j].x1) - std::max(g[i].x0, g[j].x0);
            const f32 dy = std::min(g[i].y1, g[j].y1) - std::max(g[i].y0, g[j].y0);
            if (dx > 0.5f && dy > 0.5f) {   // interseção 2D REAL (para de AA)
                const f32 pen = std::min(dx, dy);
                if (pen > worst) {
                    worst = pen;
                    ox = dx;
                    oy = dy;
                }
            }
        }
    }
    return worst;
}

// ambiente de um frame: fonte real + UI + cena com o TIC do caso C33
struct Env {
    FontAtlas   font;
    UiContext   ui;
    InputState  input;
    Scene       scene;
    Handle      selected{};
    EditorState st;
    AssetCatalog catalog;
    bool ok = false;

    explicit Env(bool withTc, bool withCatalog) {
        const char* fontPath = FONT_FIXTURE;
        ok = font.loadFromPaths(&fontPath, 1, kFontPx);
        if (!ok) {
            return;
        }
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
        selected = createTicFromPreset(scene, PresetKind::PlayerBody3D, nullptr, nullptr);
        Tic* tic = scene.get(selected);
        if (withTc && tic) {
            tic->addComponent<TouchControls>();
        }
        if (tic) {
            if (MeshRenderer* mr = tic->getComponent<MeshRenderer>()) {
                mr->meshPath = "meshes/quad.obj";
                mr->texPath  = "textures/wood.png";
            }
        }
        st.selected = selected;
        if (withCatalog) {
            catalog.meshes   = {"quad.obj", "cube.obj"};
            catalog.textures = {"wood.png"};
        }
    }

    void frame() {
        ui.beginFrame(nullptr, &input, kSW, kSH);
        bool clicks[3] = {false, false, false};
        ui.toolbar(clicks);
        drawHierarchy(ui, scene, st);
        drawInspector(ui, scene, st, withCatalog_());
        ui.statusLine("status");
        ui.endFrame();
        input.clearEdges();
    }

private:
    const AssetCatalog* withCatalog_() const {
        return catalog.meshes.empty() ? nullptr : &catalog;
    }
};

} // namespace

// métricas REAIS do bake: ascent/descent coerentes com uma sans a 28 px —
// são estes números que alimentam as alturas de linha do plano
TEST(ui_fonte_real_metricas_do_bake) {
    FontAtlas f;
    const char* path = FONT_FIXTURE;
    EXPECT(f.loadFromPaths(&path, 1, kFontPx));
    EXPECT(f.ok());
    EXPECT(nearEqF(f.height(), kFontPx));
    EXPECT(f.ascent() > 14.0f && f.ascent() < 26.0f);    // sans 28 px
    EXPECT(f.descent() > 2.0f && f.descent() < 12.0f);
    const TextMetrics m{f.ascent(), f.descent()};
    EXPECT(m.block() >= 22.0f && m.block() <= 30.0f);
    // o bloco TEM de caber nas linhas novas (o bug: linhas 26 px < bloco 28)
    EXPECT(inspTextRowH(m) >= m.block() + 4.0f);
    EXPECT(inspButtonRowH(m) >= m.block() + 6.0f);
    // baseline centrada: bloco inteiro DENTRO da linha
    const f32 rowH = inspTextRowH(m);
    const f32 b = inspBaseline(10.0f, rowH, m);
    EXPECT(b - m.ascent >= 10.0f);
    EXPECT(b + m.descent <= 10.0f + rowH);
}

// REGRESSÃO DO C33: frame completo do editor com fonte real a 28 px —
// ZERO colisões glifo-a-glifo (o código antigo apanhava 13: tex×input 7 px,
// velx×tc 3 px, etc.). Com o pior caso do dono: Player + tc + assets.
TEST(ui_inspector_sem_sobreposicao_glifos_c33) {
    Env e(/*withTc=*/true, /*withCatalog=*/true);
    EXPECT(e.ok);
    e.frame();

    f32 ox = 0.0f, oy = 0.0f;
    const f32 worst = worstGlyphPenetration(e.ui, ox, oy);
    EXPECT(worst <= 2.0f);   // ≤ ruído de AA (antigo: 7.4 px)
    // e há texto DE VERDADE (a aferição não é vazia)
    EXPECT(e.ui.glyphsForTest().vertexCount() > 6 * 100);

    // o painel do Inspector mostra as linhas do caso do dono em Ys distintos
    const Tic* tic = e.scene.get(e.selected);
    const TextMetrics tm = e.ui.textMetrics();
    const InspProfile prof = inspectorProfile(*tic);
    InspRow plan[20];
    const u32 n = inspectorPlan(prof, tm, true, plan);
    for (u32 i = 1; i < n; ++i) {
        EXPECT(plan[i].y > plan[i - 1].y);   // sequencial, sem reinício
    }
}

// o mesmo, SEM TouchControls (variante com o botão "add TouchControls") e
// SEM catálogo (mesh/tex só leitura) — as duas outras formas do painel
TEST(ui_inspector_sem_sobreposicao_variacoes) {
    {
        Env e(false, false);
        EXPECT(e.ok);
        e.frame();
        f32 ox = 0.0f, oy = 0.0f;
        EXPECT(worstGlyphPenetration(e.ui, ox, oy) <= 2.0f);
    }
    {
        Env e(false, true);
        EXPECT(e.ok);
        e.frame();
        f32 ox = 0.0f, oy = 0.0f;
        EXPECT(worstGlyphPenetration(e.ui, ox, oy) <= 2.0f);
    }
}

// scroll por cima: drag vertical dentro da região → offset cresce ATÉ ao
// máximo e o ÚLTIMO campo do plano (tc / addTc) fica inteiro na região
TEST(ui_scroll_revela_ultimo_campo) {
    Env e(true, true);
    EXPECT(e.ok);

    Tic* tic = e.scene.get(e.selected);
    const TextMetrics tm = e.ui.textMetrics();
    const InspProfile prof = inspectorProfile(*tic);
    const f32 contentH = inspectorContentHeight(prof, tm, true);

    const UiRect panel = safe::inspectorPanelRect(kSW, kSH, safe::Insets{});
    const f32 contentTop = panel.y + kHeaderH + 4.0f;
    const f32 listH = panel.h - kHeaderH - 4.0f;
    EXPECT(contentH > listH);   // o caso C33 tem de transbordar

    // frame 1: press edge dentro da região (margem ESQUERDA do painel —
    // fora do trilho dos sliders, para o scroll reclamar o gesto)
    e.input.injectDown(0, panel.x + 30.0f, panel.y + panel.h * 0.5f);
    e.frame();
    // frame 2: arrasto para CIMA (revela o fundo)
    e.input.injectMove(0, panel.x + 30.0f, panel.y - 999.0f);
    e.frame();
    // frame 3: solta (drag ≥ kTapPx → sem tap re-despachado)
    e.input.injectUp(0);
    e.frame();

    const f32 off = e.ui.scrollOffsetForTest(kInspectorScrollId);
    EXPECT(nearEqF(off, scroll::maxOffset(contentH, listH)));
    EXPECT(off > 0.0f);

    // a ÚLTIMA linha do plano fica INTEIRA dentro da região com o offset
    InspRow plan[20];
    const u32 n = inspectorPlan(prof, tm, true, plan);
    const InspRow& last = plan[n - 1];
    const f32 lastTop = contentTop + last.y - off;
    EXPECT(lastTop >= contentTop);
    EXPECT(lastTop + last.h <= contentTop + listH + 0.01f);
    // e o indicador de scroll é desenhado (barra fina à direita da região)
    std::vector<Rect> solids;
    collectRects(e.ui.solidsForTest(), solids);
    bool indicator = false;
    for (const Rect& r : solids) {
        if (r.x0 >= panel.x + panel.w - 6.0f && (r.x1 - r.x0) <= 4.0f &&
            r.y1 > r.y0) {
            indicator = true;
        }
    }
    EXPECT(indicator);
}

// sliders DENTRO da região: drag horizontal num slider muda o valor e NÃO
// faz scroll (o slider mantém a prioridade de captura — regra da spec)
TEST(ui_slider_captura_dentro_do_scroll) {
    Env e(true, true);
    EXPECT(e.ok);

    Tic* tic = e.scene.get(e.selected);
    Transform3D* tr = tic->getComponent<Transform3D>();
    const f32 px0 = tr->pos.x;

    const UiRect panel = safe::inspectorPanelRect(kSW, kSH, safe::Insets{});
    const f32 contentTop = panel.y + kHeaderH + 4.0f;
    // linha "px" (primeiro slider): y = nome 34 + secção 34 (métricas reais
    // da Liberation ~ iguais ao fallback) — derivado do PLANO com as métricas
    // reais para não divergir do desenho
    const TextMetrics tm = e.ui.textMetrics();
    InspRow plan[20];
    const u32 n = inspectorPlan(inspectorProfile(*tic), tm, true, plan);
    f32 pxY = -1.0f;
    for (u32 i = 0; i < n; ++i) {
        if (plan[i].kind == InspRow::Kind::Slider && plan[i].id == kInspectorSliderBase) {
            pxY = plan[i].y;
        }
    }
    EXPECT(pxY > 0.0f);
    const f32 rowScr = contentTop + pxY;

    // press NO TRILHO do slider px (track x = painel + 84, w = 118)
    e.input.injectDown(0, panel.x + 84.0f + 59.0f, rowScr + 18.0f);
    e.frame();
    // arrasto horizontal para a DIREITA (valor sobe; vertical quase nulo)
    e.input.injectMove(0, panel.x + 84.0f + 59.0f + 80.0f, rowScr + 18.0f);
    e.frame();
    e.input.injectUp(0);
    e.frame();

    EXPECT(tr->pos.x > px0 + 10.0f);   // o slider seguiu o dedo
    EXPECT(nearEqF(e.ui.scrollOffsetForTest(kInspectorScrollId), 0.0f));   // o scroll NÃO reclamou
}

// tap re-despachado: tap nos botões do PLANO aciona a ação certa (seletores
// de assets F5-E e "add TouchControls") — dentro da região com scroll
TEST(ui_tap_redespachado_botoes_do_plano) {
    Env e(false, true);   // SEM TouchControls → o botão do fundo existe
    EXPECT(e.ok);

    const Tic* tic = e.scene.get(e.selected);
    const TextMetrics tm = e.ui.textMetrics();
    const InspProfile prof = inspectorProfile(*tic);
    InspRow plan[20];
    const u32 n = inspectorPlan(prof, tm, true, plan);
    const UiRect panel = safe::inspectorPanelRect(kSW, kSH, safe::Insets{});
    const f32 contentTop = panel.y + kHeaderH + 4.0f;

    auto rowOf = [&](InspRow::Kind k) -> const InspRow* {
        for (u32 i = 0; i < n; ++i) {
            if (plan[i].kind == k) return &plan[i];
        }
        return nullptr;
    };
    const InspRow* mesh = rowOf(InspRow::Kind::MeshButton);
    const InspRow* tex = rowOf(InspRow::Kind::TexButton);
    const InspRow* addTc = rowOf(InspRow::Kind::AddTc);
    EXPECT(mesh && tex && addTc);

    // tap 1-frame (down+up antes da UI) no botão mesh → abre seletor MESH
    e.input.injectDown(0, panel.x + panel.w * 0.5f, contentTop + mesh->y + 4.0f);
    e.input.injectUp(0);
    e.frame();
    EXPECT(e.st.assetMenu == 1);

    // tap no botão tex → seletor TEXTURA
    e.input.injectDown(0, panel.x + panel.w * 0.5f, contentTop + tex->y + 4.0f);
    e.input.injectUp(0);
    e.frame();
    EXPECT(e.st.assetMenu == 2);

    // tap no botão "add TouchControls" → cria o componente no TIC
    e.input.injectDown(0, panel.x + panel.w * 0.5f, contentTop + addTc->y + 10.0f);
    e.input.injectUp(0);
    e.frame();
    EXPECT(e.scene.get(e.selected)->getComponent<TouchControls>() != nullptr);

    // tap numa área SEM widget (meio da linha body → label) não faz nada
    e.st.assetMenu = 0;
    const f32 bodyY = [&]() -> f32 {
        for (u32 i = 0; i < n; ++i) {
            if (plan[i].kind == InspRow::Kind::Label) return plan[i].y;
        }
        return 0.0f;
    }();
    e.input.injectDown(0, panel.x + panel.w - 4.0f, contentTop + bodyY + 4.0f);
    e.input.injectUp(0);
    e.frame();
    EXPECT(e.st.assetMenu == 0);
    // (o gesto nasceu DENTRO da região → foi de scroll/tap sem alvo)
}

// o plano e o conteúdo respeitam a área ÚTIL com insets do C33 (nav bar à
// direita): o painel encolhe e o scroll compensa — nada sai do contentRect
TEST(ui_inspector_com_insets_scroll_compensa) {
    Env e(true, true);
    EXPECT(e.ok);
    const safe::Insets in = safe::insetsFromContentRect(kSW, kSH, 0, 24, kSW - 42, 628);
    e.ui.setSafeArea(in);
    e.frame();

    const UiRect panel = safe::inspectorPanelRect(kSW, kSH, in);
    EXPECT(safe::rectInside(panel, safe::contentRect(kSW, kSH, in)));
    f32 ox = 0.0f, oy = 0.0f;
    EXPECT(worstGlyphPenetration(e.ui, ox, oy) <= 2.0f);   // sem sobreposição
}
