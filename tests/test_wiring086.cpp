// tests/test_wiring086.cpp — 0.8.6: TESTES DE INTEGRAÇÃO DE WIRING
// (UX/layout/Inspector — hex, tipografia, gizmos de UI, sobreposição).
//
// O que os critérios da 0.8.6 exigem e esta suíte aferva (host):
//   • HEX edita a cor — uiHexParse/uiHexFormat redondos e recusas honestas;
//   • FONT SIZE/STYLE aplicam — labelStyled escala glifos, negrito dobra os
//     quads por glifo, itálico inclina os VÉRTICES TOP (no batch real);
//   • GIZMOS de UI scale/rotate — hit-test rodado, handles nos cantos,
//     reancoragem por rect nas 6 âncoras (o inverso exato de elementRect);
//   • ZERO SOBREPOSIÇÃO em TODOS os ecrãs — o chrome completo em paisagem
//     (toolbar/painéis/centro/timeline/status) par-a-par e dentro do
//     contentRect, em DUAS resoluções (C33 1600×720 e 1280×720);
//   • TUDO SERIALIZA — rot/fscale/tstyle voltam no .goni (round-trip).
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <string>
#include "components/UiCanvas.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "ui/EditorLayout.h"
#include "ui/EditorUi.h"
#include "ui/FontAtlas.h"
#include "ui/SafeArea.h"
#include "ui/Timeline.h"
#include "ui/UiContext.h"
#include "ui/UiEditor.h"
#include "ui/UiRuntime.h"

using namespace vv;
using ::test::nearEqF;

namespace {
const char* kFontPath = FONT_FIXTURE;

bool overlap(const UiRect& a, const UiRect& b) {
    return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h &&
           b.y < a.y + a.h;
}
} // namespace

// ---- 1. HEX: editar cor por código ------------------------------------------

TEST(wiring086_hex_parse_format_roundtrip) {
    f32 c[4] = {0.0f, 0.0f, 0.0f, 0.5f};
    // format → parse devolve a MESMA cor (±1/255)
    f32 src[3] = {0.1176f, 0.5098f, 0.9019f};
    char hex[12];
    editor::uiHexFormat(src, hex, sizeof(hex));
    EXPECT(std::string(hex) == "#1E82E6");
    EXPECT(editor::uiHexParse(hex, c));
    EXPECT(std::fabs(c[0] - src[0]) < 0.005f);
    EXPECT(std::fabs(c[1] - src[1]) < 0.005f);
    EXPECT(std::fabs(c[2] - src[2]) < 0.005f);
    EXPECT(nearEqF(c[3], 0.5f));   // alpha INTOCADO (hex é RGB)

    // parse aceita SEM cardinal e minúsculas (o teclado tem ambos)
    EXPECT(editor::uiHexParse("FF8000", c));
    EXPECT(nearEqF(c[0], 1.0f));
    EXPECT(nearEqF(c[1], 128.0f / 255.0f));
    EXPECT(editor::uiHexParse("#ff8000", c));

    // recusas honestas (o estado fica como estava — nunca aplica lixo)
    EXPECT(!editor::uiHexParse("", c));
    EXPECT(!editor::uiHexParse("#12345", c));
    EXPECT(!editor::uiHexParse("#1234567", c));
    EXPECT(!editor::uiHexParse("#GGHHII", c));
    EXPECT(!editor::uiHexParse(nullptr, c));
}

TEST(wiring086_hex_commit_aplica_aos_dados_do_elemento) {
    // o fluxo do commit (propósito 4): o teclado escreveu "#FF0000" no
    // buffer; o commitTextInput aplica ao color[] do elemento
    Scene scene;
    const Handle h = scene.create("UI");
    ASSERT(h.valid());
    UiCanvas* canvas = scene.get(h)->addComponent<UiCanvas>();
    ASSERT(canvas != nullptr);
    UiElement el;
    el.name = "painel";
    el.kind = UiElement::Kind::Panel;
    canvas->elements.push_back(el);

    editor::EditorState st;
    editor::openTextInput(st, 4, h, 0, "#000000");
    std::snprintf(st.textBuf, sizeof(st.textBuf), "#FF0000");
    st.textLen = 7;
    EXPECT(editor::commitTextInput(scene, st));
    EXPECT(nearEqF(canvas->elements[0].color[0], 1.0f));
    EXPECT(nearEqF(canvas->elements[0].color[1], 0.0f));
    EXPECT(nearEqF(canvas->elements[0].color[2], 0.0f));

    // hex INVÁLIDO: commit devolve false e a cor fica INTACTA
    editor::openTextInput(st, 4, h, 0, "#FF0000");
    std::snprintf(st.textBuf, sizeof(st.textBuf), "#ZZZ");
    st.textLen = 4;
    EXPECT(!editor::commitTextInput(scene, st));
    EXPECT(nearEqF(canvas->elements[0].color[0], 1.0f));   // vermelho mantido
}

// ---- 2. TIPOGRAFIA: fontScale/style aplicam NO BATCH ------------------------

TEST(wiring086_tipografia_escala_negrito_italico_no_batch) {
    FontAtlas font;
    ASSERT(font.loadFromPaths(&kFontPath, 1, 28.0f));
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    const f32 white[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    // baseline: "MM" normal a 1.0 → 2 quads de glifo
    ui.beginFrame(nullptr, nullptr, 1600.0f, 720.0f);
    ui.labelStyled(0.0f, 40.0f, "MM", white, 1.0f, 0);
    ui.endFrame();
    const u32 baseQuads = ui.glyphsForTest().vertexCount() / 6;
    EXPECT(baseQuads == 2u);

    // negrito: CADA glifo desenha 2× (o embutido sem segundo atlas)
    ui.beginFrame(nullptr, nullptr, 1600.0f, 720.0f);
    ui.labelStyled(0.0f, 40.0f, "MM", white, 1.0f, 1);
    ui.endFrame();
    EXPECT(ui.glyphsForTest().vertexCount() / 6 == baseQuads * 2u);

    // fontScale 2×: os glifos medem o DOBRO (o texto do elemento CRESCE)
    // (0.9.6.6 · GRUPO C: a comparação é RELATIVA — a escala absoluta
    // inclui o textK da densidade (14px de corpo a 1.0); o claim do teste
    // é o CRESCIMENTO 2×, não um número mágico de px)
    ui.beginFrame(nullptr, nullptr, 1600.0f, 720.0f);
    ui.labelStyled(0.0f, 40.0f, "MM", white, 1.0f, 0);
    ui.endFrame();
    f32 bw1 = 0.0f;
    {
        f32 x0 = 1e9f, x1 = -1e9f;
        for (u32 v = 0; v < ui.glyphsForTest().vertexCount(); ++v) {
            x0 = ui.glyphsForTest().vertices()[v].x < x0
                     ? ui.glyphsForTest().vertices()[v].x
                     : x0;
            x1 = ui.glyphsForTest().vertices()[v].x > x1
                     ? ui.glyphsForTest().vertices()[v].x
                     : x1;
        }
        bw1 = x1 - x0;   // a largura a 1× (na escala corrente)
    }
    ui.beginFrame(nullptr, nullptr, 1600.0f, 720.0f);
    ui.labelStyled(0.0f, 40.0f, "MM", white, 2.0f, 0);
    ui.endFrame();
    const u32 scaledQuads = ui.glyphsForTest().vertexCount() / 6;
    EXPECT(scaledQuads == baseQuads);
    f32 x0 = 1e9f, x1 = -1e9f;
    for (u32 v = 0; v < ui.glyphsForTest().vertexCount(); ++v) {
        x0 = ui.glyphsForTest().vertices()[v].x < x0
                 ? ui.glyphsForTest().vertices()[v].x
                 : x0;
        x1 = ui.glyphsForTest().vertices()[v].x > x1
                 ? ui.glyphsForTest().vertices()[v].x
                 : x1;
    }
    EXPECT((x1 - x0) > bw1 * 1.7f);   // "MM" a 2× mede ~o DOBRO do 1× (o
                                      // texto do elemento CRESCE com o scale)

    // itálico: os VÉRTICES TOP deslocam +0.21·h (a haste inclina p/ a direita)
    ui.beginFrame(nullptr, nullptr, 1600.0f, 720.0f);
    ui.labelStyled(0.0f, 40.0f, "M", white, 1.0f, 2);
    ui.endFrame();
    f32 minTop = 1e9f, minBot = 1e9f;
    const u32 n = ui.glyphsForTest().vertexCount();
    for (u32 v = 0; v < n; ++v) {
        const vv::QuadVertex& q = ui.glyphsForTest().vertices()[v];
        minTop = q.y < 40.0f ? std::min(minTop, q.x) : minTop;
        minBot = q.y >= 40.0f ? std::min(minBot, q.x) : minBot;
    }
    EXPECT(minTop > minBot);   // topo DESLOCADO à direita do fundo
}

// ---- 3. GIZMOS de UI: hit rodado + escala nas 6 âncoras ---------------------

TEST(wiring086_gizmo_hit_rodado_e_handles) {
    const UiRect r{100.0f, 100.0f, 200.0f, 100.0f};

    // rot 0 = rect axis-aligned
    EXPECT(ui::uiRotatedRectHit(r, 0.0f, 150.0f, 150.0f));
    EXPECT(!ui::uiRotatedRectHit(r, 0.0f, 310.0f, 150.0f));

    // rot 90°: o ponto que estava à DIREITA do centro passa para BAIXO
    // (rotação horária em ecrã com y para baixo): centro (200,150);
    // (280,150) = direita → após -90° no espaço do rect tem de CAIR dentro
    // como se fosse o topo... o teste é a INVERSA do desenho: o ponto do
    // rect rodado (canto superior-direito do rect rodado) hit-testa
    EXPECT(ui::uiRotatedRectHit(r, 90.0f, 250.0f, 70.0f));    // canto sup-dir rodado
    EXPECT(!ui::uiRotatedRectHit(r, 90.0f, 150.0f, 150.0f + 300.0f));

    // handles: os 4 cantos DISJUNTOS e centrados nos cantos do rect
    const f32 hs = 12.0f;
    UiRect c0 = ui::uiGizmoCornerRect(r, 0, hs);
    UiRect c1 = ui::uiGizmoCornerRect(r, 1, hs);
    UiRect c2 = ui::uiGizmoCornerRect(r, 2, hs);
    UiRect c3 = ui::uiGizmoCornerRect(r, 3, hs);
    EXPECT(!overlap(c0, c1) && !overlap(c0, c2) && !overlap(c0, c3));
    EXPECT(!overlap(c1, c2) && !overlap(c1, c3) && !overlap(c2, c3));
    EXPECT(nearEqF(c0.x + c0.w * 0.5f, r.x));             // TL
    EXPECT(nearEqF(c3.x + c3.w * 0.5f, r.x + r.w));       // BR
    // pega de rotação: ACIMA do topo-centro
    UiRect rh = ui::uiGizmoRotateHandleRect(r, hs);
    EXPECT(rh.y + rh.h <= r.y);
    EXPECT(nearEqF(rh.x + rh.w * 0.5f, r.x + r.w * 0.5f));
    // snap 15° (o MESMO passo do snap de rotação 3D)
    EXPECT(nearEqF(ui::uiGizmoSnapRot(20.0f), 15.0f));
    EXPECT(nearEqF(ui::uiGizmoSnapRot(40.0f), 45.0f));
    EXPECT(nearEqF(ui::uiGizmoSnapRot(90.0f), 90.0f));
}

TEST(wiring086_gizmo_escala_reancora_nas_6_ancoras) {
    const safe::Insets ins{10.0f, 20.0f, 30.0f, 40.0f};
    const f32 sw = 1600.0f, sh = 720.0f;
    const UiRect want{300.0f, 200.0f, 120.0f, 80.0f};   // rect alvo do gizmo

    for (u32 ah = 0; ah < 3; ++ah) {
        for (u32 av = 0; av < 3; ++av) {
            UiElement e;
            e.anchorH = static_cast<UiElement::AnchorH>(ah);
            e.anchorV = static_cast<UiElement::AnchorV>(av);
            editor::uiGizmoScaleToRect(e, want, sw, sh, ins);
            // elementRect (a IDA) tem de devolver EXATAMENTE o rect do gizmo
            const UiRect got = ui::elementRect(e, sw, sh, ins);
            EXPECT(nearEqF(got.x, want.x));
            EXPECT(nearEqF(got.y, want.y));
            EXPECT(nearEqF(got.w, want.w));
            EXPECT(nearEqF(got.h, want.h));
        }
    }
}

// ---- 4. SERIALIZAÇÃO: rot/fscale/tstyle voltam no .goni ---------------------

TEST(wiring086_rot_fontscale_style_roundtrip_no_goni) {
    Scene scene;
    const Handle h = scene.create("UI");
    ASSERT(h.valid());
    UiCanvas* canvas = scene.get(h)->addComponent<UiCanvas>();
    ASSERT(canvas != nullptr);
    UiElement el;
    el.name = "rotulado";
    el.kind = UiElement::Kind::Label;
    el.text = "titulo";
    el.fontScale = 1.75f;
    el.textStyle = UiElement::TextStyle::Bold;
    el.rot = -30.0f;
    canvas->elements.push_back(el);

    const std::string json = SceneSerializer::dump(scene);
    EXPECT(json.find("\"fscale\"") != std::string::npos);
    EXPECT(json.find("\"tstyle\":\"negrito\"") != std::string::npos);
    EXPECT(json.find("\"rot\":-30") != std::string::npos);

    Scene loaded;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(loaded, json, ctx));
    const Handle h2 = loaded.find("UI");
    ASSERT(h2.valid());
    UiCanvas* cv = loaded.get(h2)->getComponent<UiCanvas>();
    ASSERT(cv != nullptr && !cv->elements.empty());
    EXPECT(nearEqF(cv->elements[0].fontScale, 1.75f));
    EXPECT(cv->elements[0].textStyle == UiElement::TextStyle::Bold);
    EXPECT(nearEqF(cv->elements[0].rot, -30.0f));
}

// ---- 5. ZERO SOBREPOSIÇÃO: o chrome completo em paisagem --------------------

TEST(wiring086_zero_sobreposicao_do_chrome_em_paisagem_c33_e_1280) {
    // os rects do EDITOR 3D (toolbar, painéis, centro, status) + TIMELINE
    // visível — composição COMPLETA, par-a-par, nas DUAS resoluções de teste
    const f32 sizes[][2] = {{1600.0f, 720.0f}, {1280.0f, 720.0f}};
    const safe::Insets in{0.0f, 0.0f, 0.0f, 40.0f};   // nav bar do C33
    for (const auto& s : sizes) {
        const f32 sw = s[0], sh = s[1];
        const UiRect bar = safe::toolbarRect(sw, sh, in);
        const UiRect status = safe::statusRect(sw, sh, in);
        const UiRect hier = safe::hierarchyPanelRect(sw, sh, in);
        const UiRect insp = safe::inspectorPanelRect(sw, sh, in);
        const UiRect center = safe::centerRect(sw, sh, in, true);
        const UiRect tl = timeline::timelineRect(sw, sh, in, true);
        // o viewport central RESPIRA a timeline (a strip vive no fundo dele)
        UiRect viewAbove = center;
        viewAbove.h -= timeline::kTimelineH;

        // todos DENTRO do contentRect
        const UiRect content = safe::contentRect(sw, sh, in);
        EXPECT(safe::rectInside(bar, content));
        EXPECT(safe::rectInside(status, content));
        EXPECT(safe::rectInside(hier, content));
        EXPECT(safe::rectInside(insp, content));
        EXPECT(safe::rectInside(center, content));
        EXPECT(safe::rectInside(tl, center));   // a timeline vive NO centro

        // par-a-par: barra, painéis e viewport ACIMA da timeline
        EXPECT(!overlap(bar, hier) && !overlap(bar, insp));
        EXPECT(!overlap(bar, viewAbove));
        EXPECT(!overlap(hier, viewAbove) && !overlap(insp, viewAbove));
        EXPECT(!overlap(hier, insp));
        EXPECT(!overlap(status, viewAbove));
        EXPECT(!overlap(tl, hier) && !overlap(tl, insp));
        EXPECT(!overlap(tl, status) && !overlap(tl, bar));

        // o HEADER da timeline (0.8.4) também sem par sobreposto
        const timeline::HeaderLayout hl = timeline::headerLayout(tl);
        const UiRect* ws[] = {&hl.play, &hl.stop, &hl.mode, &hl.slider,
                              &hl.add, &hl.clip};
        for (u32 i = 0; i < 6; ++i) {
            for (u32 j = i + 1; j < 6; ++j) {
                EXPECT(!overlap(*ws[i], *ws[j]));
            }
        }
    }
}
