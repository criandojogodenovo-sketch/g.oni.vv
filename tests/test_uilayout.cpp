// tests/test_uilayout.cpp — 0.7.4: PARIDADE editor↔Play + texturas de fundo
// + Label sem fundo + Menu configurável + containers VBox/HBox + serialização.
//
// O contrato novo da sub-fase (C33 pós-0.7.3):
//   • Label = SÓ TEXTO por default (alpha 0); fundo OPCIONAL (cor com alpha);
//   • Button/Panel/Image com TEXTURA de fundo (ref em e.image; tint BRANCO×
//     alpha — antes o Image tingia a textura com o LINE escuro);
//   • PARIDADE editor↔Play (princípio transversal): para CADA tipo de
//     elemento, o render do editor (drawUiViewport, escalado) e o do Play
//     (drawCanvas) produzem a MESMA UI em geometria/cor/textura/texto —
//     comparação determinística rect-a-rect (o editor é o Play × escala
//     + offset); só os OVERLAYS de edição diferem (seleção — teste com
//     selElement = −1);
//   • runs de submissão em ORDEM (z sólidos↔texturas: um botão sólido
//     desenhado DEPOIS de um painel texturizado submete-se DEPOIS dele);
//   • Menu configurável: espaçamento, fundo on/off (alpha), alinhamento;
//   • containers VBox/HBox: filhos em coluna/linha com espaçamento/pad/
//     alinhamento, SEM sobreposição, aninháveis, auto-fit no eixo de
//     conteúdo; filhos invisíveis colapsam; container invisível esconde
//     descendentes; ciclos (A pai de B, B pai de A) → órfãos de topo;
//   • hit-test e drag do editor usam o MESMO resolver (filhos de container
//     hit-testam no rect DISPPOSTO, não no ox/oy);
//   • serialização .goni de tudo (spacing/pad/align/parent + alpha + image)
//     com round-trip.
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "components/UiCanvas.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "platform/InputState.h"
#include "render/Texture.h"
#include "ui/EditorLayout.h"
#include "ui/EditorUi.h"
#include "ui/FontAtlas.h"
#include "ui/SafeArea.h"
#include "ui/UiContext.h"
#include "ui/UiEditor.h"
#include "ui/UiRuntime.h"

using namespace vv;
using namespace vv::ui;
using namespace vv::editor;
using ::test::nearEqF;

namespace {
constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;

struct Rect { f32 x0, y0, x1, y1; };
void collectRects(const QuadBatch& b, std::vector<Rect>& out) {
    const QuadVertex* v = b.vertices();
    const u32 n = b.vertexCount();
    for (u32 i = 0; i + 5 < n; i += 6) {
        out.push_back({v[i].x, v[i].y, v[i + 2].x, v[i + 2].y});
    }
}
bool rectsOverlap(const Rect& a, const Rect& b, f32 eps = 0.01f) {
    return std::min(a.x1, b.x1) - std::max(a.x0, b.x0) > eps &&
           std::min(a.y1, b.y1) - std::max(a.y0, b.y0) > eps;
}
// existe um quad EXATAMENTE neste rect? (comparação determinística)
bool hasQuad(const QuadBatch& b, f32 x, f32 y, f32 w, f32 h, f32 eps = 0.75f) {
    const QuadVertex* v = b.vertices();
    const u32 n = b.vertexCount();
    for (u32 i = 0; i + 5 < n; i += 6) {
        if (std::fabs(v[i].x - x) < eps && std::fabs(v[i].y - y) < eps &&
            std::fabs(v[i + 2].x - (x + w)) < eps &&
            std::fabs(v[i + 2].y - (y + h)) < eps) {
            return true;
        }
    }
    return false;
}
} // namespace

// ---- 1. Label = só texto por default (o fix do C33) ----------------------------

TEST(uilayout_label_sem_fundo_por_default) {
    UiCanvas c;
    const i32 i = c.addElement(UiElement::Kind::Label, kSW, kSH);
    EXPECT(i == 0);
    // default: fundo TRANSPARENTE (alpha 0) — só texto
    EXPECT(c.elements[0].color[3] == 0.0f);
    EXPECT(c.elements[0].text == "Texto");

    FontAtlas font;
    const char* fontPath = FONT_FIXTURE;
    if (!font.loadFromPaths(&fontPath, 1, 28.0f)) {
        EXPECT(!"fonte do fixture não carregou");
        return;
    }
    UiContext ui;
    ui.init();
    ui.setFont(&font);

    // sem fundo: NENHUM quad sólido no rect do label (só glifos)
    c.elements[0].ox = 100.0f; c.elements[0].oy = 100.0f;
    c.elements[0].anchorH = UiElement::AnchorH::Left;
    c.elements[0].anchorV = UiElement::AnchorV::Top;
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    drawCanvas(ui, c, kSW, kSH, safe::Insets{});
    EXPECT(!hasQuad(ui.solidsForTest(), 100.0f, 100.0f, 240.0f, 44.0f));
    EXPECT(ui.glyphsForTest().vertexCount() > 0u);   // o TEXTO está lá
    ui.endFrame();

    // fundo LIGADO (alpha > 0): o painel aparece com a cor COM ALPHA
    c.elements[0].color[3] = 0.75f;
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    drawCanvas(ui, c, kSW, kSH, safe::Insets{});
    EXPECT(hasQuad(ui.solidsForTest(), 100.0f, 100.0f, 240.0f, 44.0f));
    ui.endFrame();

    // Button mantém o fundo ESCURO por default (não regrediu)
    UiCanvas cb;
    cb.addElement(UiElement::Kind::Button, kSW, kSH);
    EXPECT(cb.elements[0].color[3] == 1.0f);
    EXPECT(cb.elements[0].color[0] < 0.5f);   // escuro (LINE), não TEXT
}

// ---- 2. texturas de fundo: Button/Panel/Image + tint branco + z-order ---------

namespace {
Texture g_texA;
Texture g_texB;
const Texture* texResolver(const std::string& ref) {
    if (ref == "textures/a.png") return &g_texA;
    if (ref == "textures/b.png") return &g_texB;
    return nullptr;
}
} // namespace

TEST(uilayout_button_panel_image_com_textura) {
    u8 rgba[16];
    for (int i = 0; i < 16; ++i) {
        rgba[i] = 200;
    }
    if (!g_texA.createFromRGBA(rgba, 2, 2) || !g_texB.createFromRGBA(rgba, 2, 2)) {
        EXPECT(!"stub GL não criou as texturas");
        return;
    }
    FontAtlas font;
    const char* fontPath = FONT_FIXTURE;
    if (!font.loadFromPaths(&fontPath, 1, 28.0f)) {
        EXPECT(!"fonte do fixture não carregou");
        return;
    }
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    ui.setImageResolver(&texResolver);

    UiCanvas c;
    UiElement panel;
    panel.kind = UiElement::Kind::Panel;
    panel.name = "p";
    panel.image = "textures/a.png";   // 0.7.4: textura de fundo
    panel.ox = 100.0f; panel.oy = 80.0f; panel.w = 300.0f; panel.h = 200.0f;
    panel.anchorH = UiElement::AnchorH::Left;
    panel.anchorV = UiElement::AnchorV::Top;
    c.elements.push_back(panel);
    UiElement btn;
    btn.kind = UiElement::Kind::Button;
    btn.name = "b";
    btn.text = "OK";
    btn.image = "textures/b.png";
    btn.ox = 200.0f; btn.oy = 120.0f; btn.w = 200.0f; btn.h = 80.0f;
    btn.anchorH = UiElement::AnchorH::Left;
    btn.anchorV = UiElement::AnchorV::Top;
    c.elements.push_back(btn);

    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    const u32 drawn = drawCanvas(ui, c, kSW, kSH, safe::Insets{});
    EXPECT(drawn == 2u);
    // NOTA do glstub: glGenTextures devolve 1+i por CHAMADA — as duas
    // texturas partilham o handle 1 no hospedeiro → 1 batch com os 2 quads
    // (no device seriam 2). Os RECTS é que importam aqui:
    EXPECT(ui.imageBatchCountForTest() >= 1u);
    EXPECT(hasQuad(ui.imagesForTest(), 100.0f, 80.0f, 300.0f, 200.0f));
    // o botão (desenhado DEPOIS, SÓLIDO) submete-se DEPOIS da textura do
    // painel — o run 0 é a TEXTURA, o último run é SÓLIDO (o botão fica por
    // cima na tela — antes da 0.7.4 os sólidos submetiam-se TODOS antes)
    EXPECT(ui.runCountForTest() >= 2u);
    EXPECT(ui.runTexForTest(0) != 0u);
    EXPECT(ui.runTexForTest(ui.runCountForTest() - 1u) == 0u);
    ui.endFrame();

    // SEM resolver (CI/carga falhada): fallback HONESTO — fill sólido + frame
    ui.setImageResolver(nullptr);
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    drawCanvas(ui, c, kSW, kSH, safe::Insets{});
    EXPECT(hasQuad(ui.solidsForTest(), 100.0f, 80.0f, 300.0f, 200.0f));
    EXPECT(ui.imageBatchCountForTest() == 0u);
    ui.endFrame();
    ui.setImageResolver(&texResolver);
}

TEST(uilayout_image_placeholder_sem_imagem) {
    FontAtlas font;
    const char* fontPath = FONT_FIXTURE;
    if (!font.loadFromPaths(&fontPath, 1, 28.0f)) {
        EXPECT(!"fonte do fixture não carregou");
        return;
    }
    UiContext ui;
    ui.init();
    ui.setFont(&font);

    // SEM imagem escolhida: placeholder claro com "(sem imagem)" centrado
    UiCanvas c;
    UiElement img;
    img.kind = UiElement::Kind::Image;
    img.name = "i";
    img.ox = 120.0f; img.oy = 120.0f; img.w = 256.0f; img.h = 256.0f;
    img.anchorH = UiElement::AnchorH::Left;
    img.anchorV = UiElement::AnchorV::Top;
    c.elements.push_back(img);
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    drawCanvas(ui, c, kSW, kSH, safe::Insets{});
    EXPECT(hasQuad(ui.solidsForTest(), 120.0f, 120.0f, 256.0f, 256.0f));
    EXPECT(ui.glyphsForTest().vertexCount() > 0u);   // "(sem imagem)"
    ui.endFrame();

    // ref que NÃO resolve: placeholder HONESTO com o NOME da ref
    c.elements[0].image = "textures/nao-existe.png";
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    drawCanvas(ui, c, kSW, kSH, safe::Insets{});
    EXPECT(hasQuad(ui.solidsForTest(), 120.0f, 120.0f, 256.0f, 256.0f));
    EXPECT(ui.glyphsForTest().vertexCount() > 0u);
    ui.endFrame();
}

// ---- 3. PARIDADE editor↔Play (o princípio transversal) ---------------------------

namespace {
// desenha o MESMO canvas nos DOIS modos e devolve os batches p/ comparação
struct BothFrames {
    std::vector<Rect> playSolids, editSolids;
    std::vector<Rect> playGlyphs, editGlyphs;
    ViewportTransform t{};
};

BothFrames renderBoth(const UiCanvas& c) {
    BothFrames out;
    FontAtlas font;
    const char* fontPath = FONT_FIXTURE;
    if (!font.loadFromPaths(&fontPath, 1, 28.0f)) {
        return out;
    }
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    const safe::Insets ins{};

    // PLAY: drawCanvas no ecrã real
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    drawCanvas(ui, c, kSW, kSH, ins);
    collectRects(ui.solidsForTest(), out.playSolids);
    collectRects(ui.glyphsForTest(), out.playGlyphs);
    ui.endFrame();

    // EDITOR: drawUiViewport (viewport 2D) — a MESMA cena/estado
    Scene scene;
    EditorState st;
    const Handle h = scene.create("HUD");
    Tic* tic = scene.get(h);
    UiCanvas* cc = tic->addComponent<UiCanvas>();
    cc->elements = c.elements;
    st.selected = h;
    st.uiMode = true;
    st.selElement = -1;   // SEM overlay de edição (só interação difere)
    InputState in;
    ui.beginFrame(nullptr, &in, kSW, kSH);
    drawUiViewport(ui, scene, st, in, kSW, kSH);
    collectRects(ui.solidsForTest(), out.editSolids);
    collectRects(ui.glyphsForTest(), out.editGlyphs);
    ui.endFrame();
    const UiRect view = safe::centerRect(kSW, kSH, ins);
    out.t = uiViewportTransform(view, kSW, kSH);
    return out;
}

// transforma rect design→ecrã do editor
Rect toEdit(const Rect& r, const ViewportTransform& t) {
    return {t.ox + r.x0 * t.scale, t.oy + r.y0 * t.scale,
            t.ox + r.x1 * t.scale, t.oy + r.y1 * t.scale};
}
// cada rect do Play existe no editor EXATAMENTE transformado? (o editor
// pode ter quads EXTRA — o fundo/moldura do viewport são chrome de edição,
// não conteúdo do canvas; o que NÃO pode é faltar ou divergir nenhum)
bool rectsMatchUnderTransform(const std::vector<Rect>& play,
                              const std::vector<Rect>& edit,
                              const ViewportTransform& t, f32 eps = 1.0f) {
    if (play.size() > edit.size()) {
        return false;
    }
    for (const Rect& p : play) {
        const Rect want = toEdit(p, t);
        bool found = false;
        for (const Rect& e : edit) {
            if (std::fabs(e.x0 - want.x0) < eps &&
                std::fabs(e.y0 - want.y0) < eps &&
                std::fabs(e.x1 - want.x1) < eps &&
                std::fabs(e.y1 - want.y1) < eps) {
                found = true;
                break;
            }
        }
        if (!found) {
            return false;
        }
    }
    return true;
}
} // namespace

TEST(uilayout_paridade_editor_play_por_tipo) {
    u8 rgba[16];
    for (int i = 0; i < 16; ++i) {
        rgba[i] = 180;
    }
    if (!g_texA.createFromRGBA(rgba, 2, 2)) {
        EXPECT(!"stub GL não criou a textura");
        return;
    }
    // um canvas com UM elemento de cada tipo base/composto (posições sem
    // sobreposição) — o hit é aferido tipo a tipo
    struct Case { UiElement::Kind kind; const char* name; bool hasText; bool hasFill; };
    const Case cases[] = {
        {UiElement::Kind::Panel, "panel", false, true},
        {UiElement::Kind::Label, "label", true, false},   // 0.7.4: sem fundo
        {UiElement::Kind::Button, "button", true, true},
        {UiElement::Kind::Image, "image", true, true},    // placeholder + texto
        {UiElement::Kind::Menu, "menu", true, true},
        {UiElement::Kind::Card, "card", true, true},
        {UiElement::Kind::Article, "article", true, true},
    };
    for (const Case& cs : cases) {
        UiCanvas c;
        UiElement e;
        e.kind = cs.kind;
        e.name = cs.name;
        e.ox = 200.0f; e.oy = 140.0f;
        e.w = 320.0f; e.h = 200.0f;
        e.anchorH = UiElement::AnchorH::Left;
        e.anchorV = UiElement::AnchorV::Top;
        if (cs.kind == UiElement::Kind::Button) e.text = "JOGAR";
        if (cs.kind == UiElement::Kind::Label)  e.text = "Texto";
        if (cs.kind == UiElement::Kind::Card)   e.text = "Titulo";
        if (cs.kind == UiElement::Kind::Article) e.text = "Linha um. Linha dois. Linha tres.";
        if (cs.kind == UiElement::Kind::Menu)   e.text = "Jogar\nSair";
        if (cs.kind == UiElement::Kind::Image)  e.image = "textures/a.png";
        c.elements.push_back(e);

        const BothFrames f = renderBoth(c);
        // SÓLIDOS: cada quad do Play existe no editor × escala + offset
        // (o editor pode ter quads a mais — o CHROME do viewport)
        if (cs.hasFill) {
            EXPECT(f.playSolids.size() > 0u);
        }
        const bool solidsOk =
            rectsMatchUnderTransform(f.playSolids, f.editSolids, f.t);
        EXPECT(solidsOk);
        // GLIFOS: o texto do editor é o do Play ESCALADO (a causa raiz do
        // "Menu com caixas no editor e texto solto no Play") — sem chrome
        // de texto no viewport, a contagem É igual
        if (cs.hasText) {
            EXPECT(f.playGlyphs.size() > 0u);
        }
        EXPECT(f.playGlyphs.size() == f.editGlyphs.size());
        const bool glyphsOk =
            rectsMatchUnderTransform(f.playGlyphs, f.editGlyphs, f.t);
        EXPECT(glyphsOk);
    }
}

TEST(uilayout_paridade_containers_e_insets) {
    // VBox com 2 filhos (Button + Label) e HBox com VBox dentro — a
    // paridade também vale com CONTAINERS e com safe-area NÃO nula
    UiCanvas c;
    UiElement box;
    box.kind = UiElement::Kind::VBox;
    box.name = "col";
    box.ox = 240.0f; box.oy = 120.0f; box.w = 300.0f; box.h = 100.0f;
    box.spacing = 12.0f; box.pad = 10.0f;
    box.align = UiElement::Align::Center;
    box.anchorH = UiElement::AnchorH::Left;
    box.anchorV = UiElement::AnchorV::Top;
    c.elements.push_back(box);
    UiElement b1;
    b1.kind = UiElement::Kind::Button;
    b1.name = "b1"; b1.text = "OK";
    b1.w = 200.0f; b1.h = 60.0f;
    b1.parent = "col";
    c.elements.push_back(b1);
    UiElement l1;
    l1.kind = UiElement::Kind::Label;
    l1.name = "l1"; l1.text = "lbl";
    l1.w = 120.0f; l1.h = 40.0f;
    l1.parent = "col";
    c.elements.push_back(l1);

    const BothFrames f = renderBoth(c);
    EXPECT(f.playSolids.size() > 0u);
    EXPECT(rectsMatchUnderTransform(f.playSolids, f.editSolids, f.t));
    EXPECT(rectsMatchUnderTransform(f.playGlyphs, f.editGlyphs, f.t));
}

// ---- 4. containers VBox/HBox -------------------------------------------------------

TEST(uilayout_vbox_hbox_dispoem_filhos_sem_sobreposicao) {
    UiCanvas c;
    UiElement box;
    box.kind = UiElement::Kind::VBox;
    box.name = "col";
    box.ox = 100.0f; box.oy = 100.0f; box.w = 300.0f; box.h = 50.0f;   // h AUTO
    box.spacing = 10.0f; box.pad = 12.0f;
    box.align = UiElement::Align::Start;
    box.anchorH = UiElement::AnchorH::Left;
    box.anchorV = UiElement::AnchorV::Top;
    c.elements.push_back(box);
    UiElement a;
    a.kind = UiElement::Kind::Button; a.name = "a"; a.text = "A";
    a.w = 200.0f; a.h = 60.0f; a.parent = "col";
    c.elements.push_back(a);
    UiElement b;
    b.kind = UiElement::Kind::Label; b.name = "b"; b.text = "B";
    b.w = 160.0f; b.h = 40.0f; b.parent = "col";
    c.elements.push_back(b);

    ui::CanvasLayout lay[8];
    resolveCanvasLayout(c, kSW, kSH, safe::Insets{}, lay, 8);
    // filhos reconhecidos
    EXPECT(lay[1].parentIdx == 0 && lay[2].parentIdx == 0);
    EXPECT(lay[1].laid && lay[2].laid);
    // AUTO-FIT: h do VBox = pad*2 + 60 + 40 + spacing = 24 + 100 + 10 = 134
    EXPECT(nearEqF(lay[0].rect.h, 134.0f, 0.01f));
    EXPECT(nearEqF(lay[0].rect.w, 300.0f, 0.01f));   // w manual mantém
    // coluna: b logo abaixo de a COM o vão — sem sobreposição
    EXPECT(nearEqF(lay[1].rect.x, 112.0f, 0.01f));          // pad Start
    EXPECT(nearEqF(lay[1].rect.y, 112.0f, 0.01f));
    EXPECT(nearEqF(lay[2].rect.y, 112.0f + 60.0f + 10.0f, 0.01f));
    const Rect ra{lay[1].rect.x, lay[1].rect.y,
                  lay[1].rect.x + lay[1].rect.w, lay[1].rect.y + lay[1].rect.h};
    const Rect rb{lay[2].rect.x, lay[2].rect.y,
                  lay[2].rect.x + lay[2].rect.w, lay[2].rect.y + lay[2].rect.h};
    EXPECT(!rectsOverlap(ra, rb));

    // align CENTER: filho centrado no eixo transversal
    c.elements[0].align = UiElement::Align::Center;
    resolveCanvasLayout(c, kSW, kSH, safe::Insets{}, lay, 8);
    EXPECT(nearEqF(lay[1].rect.x + lay[1].rect.w * 0.5f,
                   lay[0].rect.x + lay[0].rect.w * 0.5f, 0.01f));
    // align END: encostado à direita (com pad)
    c.elements[0].align = UiElement::Align::End;
    resolveCanvasLayout(c, kSW, kSH, safe::Insets{}, lay, 8);
    EXPECT(nearEqF(lay[1].rect.x + lay[1].rect.w,
                   lay[0].rect.x + lay[0].rect.w - 12.0f, 0.01f));

    // HBOX aninhado DENTRO do VBox (nesting): 2 filhos lado a lado
    c.elements[0].align = UiElement::Align::Start;
    UiElement row;
    row.kind = UiElement::Kind::HBox;
    row.name = "row"; row.w = 100.0f; row.h = 44.0f;
    row.spacing = 8.0f; row.pad = 4.0f;
    row.parent = "col";
    c.elements.push_back(row);
    UiElement p1;
    p1.kind = UiElement::Kind::Panel; p1.name = "p1";
    p1.w = 90.0f; p1.h = 36.0f; p1.parent = "row";
    c.elements.push_back(p1);
    UiElement p2;
    p2.kind = UiElement::Kind::Panel; p2.name = "p2";
    p2.w = 60.0f; p2.h = 36.0f; p2.parent = "row";
    c.elements.push_back(p2);

    ui::CanvasLayout lay2[8];
    resolveCanvasLayout(c, kSW, kSH, safe::Insets{}, lay2, 8);
    // VBox h cresce com o HBox (h manual 44): 24(pad) + 60 + 10 + 40 + 10
    // + 44 = 188
    EXPECT(nearEqF(lay2[0].rect.h, 188.0f, 0.01f));
    EXPECT(nearEqF(lay2[3].rect.w, 4.0f * 2.0f + 90.0f + 60.0f + 8.0f, 0.01f));
    EXPECT(lay2[3].parentIdx == 0);   // HBox é filho do VBox
    EXPECT(lay2[4].parentIdx == 3 && lay2[5].parentIdx == 3);
    const Rect rp1{lay2[4].rect.x, lay2[4].rect.y,
                   lay2[4].rect.x + lay2[4].rect.w, lay2[4].rect.y + lay2[4].rect.h};
    const Rect rp2{lay2[5].rect.x, lay2[5].rect.y,
                   lay2[5].rect.x + lay2[5].rect.w, lay2[5].rect.y + lay2[5].rect.h};
    EXPECT(!rectsOverlap(rp1, rp2));
    EXPECT(rp2.x0 >= rp1.x1 - 0.01f);   // lado a lado (p2 depois de p1)
}

TEST(uilayout_container_visibilidade_e_ciclos) {
    UiCanvas c;
    UiElement box;
    box.kind = UiElement::Kind::VBox;
    box.name = "col";
    box.ox = 100.0f; box.oy = 100.0f; box.w = 300.0f; box.h = 400.0f;
    box.anchorH = UiElement::AnchorH::Left;
    box.anchorV = UiElement::AnchorV::Top;
    c.elements.push_back(box);
    UiElement a;
    a.kind = UiElement::Kind::Button; a.name = "a";
    a.w = 100.0f; a.h = 50.0f; a.parent = "col";
    c.elements.push_back(a);
    UiElement b;
    b.kind = UiElement::Kind::Button; b.name = "b";
    b.w = 100.0f; b.h = 50.0f; b.parent = "col";
    c.elements.push_back(b);

    ui::CanvasLayout lay[8];
    resolveCanvasLayout(c, kSW, kSH, safe::Insets{}, lay, 8);
    EXPECT(lay[1].shown && lay[2].shown);

    // filho INVISÍVEL colapsa (não ocupa lugar): b escondido → a sozinho
    c.elements[2].visible = false;
    resolveCanvasLayout(c, kSW, kSH, safe::Insets{}, lay, 8);
    EXPECT(!lay[2].shown);
    EXPECT(nearEqF(lay[0].rect.h, 8.0f * 2.0f + 50.0f, 0.01f));   // só a
    EXPECT(nearEqF(lay[1].rect.y, 108.0f, 0.01f));
    c.elements[2].visible = true;

    // container INVISÍVEL esconde os DESCENDENTES em cascata
    c.elements[0].visible = false;
    resolveCanvasLayout(c, kSW, kSH, safe::Insets{}, lay, 8);
    EXPECT(!lay[0].shown && !lay[1].shown && !lay[2].shown);
    c.elements[0].visible = true;

    // CICLO (a pai de col, col pai de a): guard — ambos órfãos de TOPO,
    // sem crash, rects das âncoras próprias
    c.elements[0].parent = "a";
    resolveCanvasLayout(c, kSW, kSH, safe::Insets{}, lay, 8);
    EXPECT(!lay[0].laid || !lay[1].laid);   // o ciclo quebrou (órfãos)
    // pai INEXISTENTE: órfão de topo (âncoras próprias)
    c.elements[0].parent.clear();
    c.elements[1].parent = "ghost";
    resolveCanvasLayout(c, kSW, kSH, safe::Insets{}, lay, 8);
    EXPECT(!lay[1].laid);
    EXPECT(nearEqF(lay[1].rect.x, 0.0f, 0.01f));   // âncora Left com ox=0
}

TEST(uilayout_hit_test_e_drag_usam_o_resolver) {
    Scene s;
    Tic* t = s.get(s.create("HUD"));
    UiCanvas* c = t->addComponent<UiCanvas>();
    UiElement box;
    box.kind = UiElement::Kind::VBox;
    box.name = "col";
    box.ox = 400.0f; box.oy = 300.0f; box.w = 300.0f; box.h = 400.0f;
    box.anchorH = UiElement::AnchorH::Left;
    box.anchorV = UiElement::AnchorV::Top;
    c->elements.push_back(box);
    UiElement btn;
    btn.kind = UiElement::Kind::Button; btn.name = "btn"; btn.text = "OK";
    // ox/oy IRRELEVANTES dentro do container (posição = do layout)
    btn.ox = 0.0f; btn.oy = 0.0f;
    btn.w = 200.0f; btn.h = 60.0f;
    btn.parent = "col";
    c->elements.push_back(btn);

    // o filho fica em (408, 308) — o hit-test ACERTA no rect DISPPOSTO
    CanvasHit hit = hitTestCanvas(s, 500.0f, 330.0f, kSW, kSH, safe::Insets{});
    EXPECT(hit.valid);
    EXPECT(hit.element == 1);
    // e NÃO hit-testa no ox/oy cru (0,0)
    hit = hitTestCanvas(s, 10.0f, 10.0f, kSW, kSH, safe::Insets{});
    EXPECT(!hit.valid);

    // detach conserva a posição visual: o filho vira topo NO SÍTIO onde está
    uiDetachElement(*c, 1, kSW, kSH, safe::Insets{});
    EXPECT(c->elements[1].parent.empty());
    EXPECT(c->elements[1].anchorH == UiElement::AnchorH::Left);
    EXPECT(c->elements[1].anchorV == UiElement::AnchorV::Top);
    EXPECT(nearEqF(c->elements[1].ox, 408.0f, 0.01f));
    EXPECT(nearEqF(c->elements[1].oy, 308.0f, 0.01f));
    // e o hit continua no MESMO sítio (agora por âncoras próprias)
    hit = hitTestCanvas(s, 500.0f, 330.0f, kSW, kSH, safe::Insets{});
    EXPECT(hit.valid && hit.element == 1);
}

TEST(uilayout_add_element_nasce_filho_do_container_selecionado) {
    Scene s;
    Tic* t = s.get(s.create("HUD"));
    UiCanvas* c = t->addComponent<UiCanvas>();
    UiElement box;
    box.kind = UiElement::Kind::VBox;
    box.name = "col";
    box.w = 300.0f; box.h = 400.0f;
    c->elements.push_back(box);

    EditorState st;
    st.selected = s.find("HUD");
    st.selElement = 0;   // o VBox selecionado
    EXPECT(uiAddElement(s, st, static_cast<u32>(UiElement::Kind::Button),
                        kSW, kSH));
    EXPECT(c->elements.size() == 2u);
    EXPECT(c->elements[1].parent == "col");   // nasceu FILHO

    // com um FILHO selecionado, o novo nasce IRMÃO (mesmo container)
    EXPECT(uiAddElement(s, st, static_cast<u32>(UiElement::Kind::Label),
                        kSW, kSH));
    EXPECT(c->elements.size() == 3u);
    EXPECT(c->elements[2].parent == "col");

    // sem container na seleção: topo (comportamento 0.7.0)
    st.selElement = -1;
    EXPECT(uiAddElement(s, st, static_cast<u32>(UiElement::Kind::Panel),
                        kSW, kSH));
    EXPECT(c->elements[3].parent.empty());
}

// ---- 5. Menu configurável ----------------------------------------------------------

TEST(uilayout_menu_espacamento_fundo_alinhamento) {
    FontAtlas font;
    const char* fontPath = FONT_FIXTURE;
    if (!font.loadFromPaths(&fontPath, 1, 28.0f)) {
        EXPECT(!"fonte do fixture não carregou");
        return;
    }
    UiContext ui;
    ui.init();
    ui.setFont(&font);

    UiCanvas c;
    UiElement m;
    m.kind = UiElement::Kind::Menu;
    m.name = "m";
    m.text = "Jogar\nSair";
    m.ox = 100.0f; m.oy = 100.0f; m.w = 300.0f; m.h = 200.0f;
    m.anchorH = UiElement::AnchorH::Left;
    m.anchorV = UiElement::AnchorV::Top;
    c.elements.push_back(m);

    // spacing 0 = 0.7.3 exato (h/n por linha)
    const UiRect base{100.0f, 100.0f, 300.0f, 200.0f};
    UiRect r0 = menuItemRect(c.elements[0], base, 0);
    EXPECT(nearEqF(r0.h, 100.0f, 0.01f));
    // spacing 20: rowH = (200 − 20)/2 = 90; 2ª linha em y = 100 + 90 + 20
    c.elements[0].spacing = 20.0f;
    r0 = menuItemRect(c.elements[0], base, 0);
    const UiRect r1 = menuItemRect(c.elements[0], base, 1);
    EXPECT(nearEqF(r0.h, 90.0f, 0.01f));
    EXPECT(nearEqF(r1.y, 100.0f + 90.0f + 20.0f, 0.01f));
    EXPECT(nearEqF(r1.h, 90.0f, 0.01f));

    // fundo ON (alpha 1): 2 painéis de linha; fundo OFF (alpha 0): NENHUM
    c.elements[0].color[3] = 1.0f;
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    drawCanvas(ui, c, kSW, kSH, safe::Insets{});
    EXPECT(hasQuad(ui.solidsForTest(), 100.0f, 100.0f, 300.0f, 90.0f));
    EXPECT(hasQuad(ui.solidsForTest(), 100.0f, 210.0f, 300.0f, 90.0f));
    ui.endFrame();
    c.elements[0].color[3] = 0.0f;   // fundo OFF — só texto
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    drawCanvas(ui, c, kSW, kSH, safe::Insets{});
    EXPECT(!hasQuad(ui.solidsForTest(), 100.0f, 100.0f, 300.0f, 90.0f));
    EXPECT(ui.glyphsForTest().vertexCount() > 0u);   // texto continua
    ui.endFrame();

    // alinhamento CENTER: o texto da linha fica CENTRADO (glifo inicial
    // dentro da metade direita da largura quando start o punha à esquerda)
    c.elements[0].color[3] = 1.0f;
    c.elements[0].align = UiElement::Align::Center;
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    drawCanvas(ui, c, kSW, kSH, safe::Insets{});
    const QuadVertex* gv = ui.glyphsForTest().vertices();
    const u32 gn = ui.glyphsForTest().vertexCount();
    EXPECT(gn > 0u);
    if (gn > 0u) {
        const f32 rowMidX = 100.0f + 150.0f;   // centro da linha
        EXPECT(gv[0].x > rowMidX - 120.0f);    // não está colado à esquerda
        // (o rótulo "Jogar" ~ 6 chars × ~15px ≈ 90px < 300 → centrado fica
        // bem à direita do x+10 do alinhamento start)
    }
    ui.endFrame();
}

// ---- 6. serialização round-trip (containers + texturas + alpha) -------------------

TEST(uilayout_serializacao_roundtrip_containers) {
    Scene s;
    Tic* t = s.get(s.create("HUD"));
    UiCanvas* c = t->addComponent<UiCanvas>();
    UiElement box;
    box.kind = UiElement::Kind::VBox;
    box.name = "col";
    box.ox = 24.0f; box.oy = 48.0f; box.w = 320.0f; box.h = 180.0f;
    box.spacing = 14.0f; box.pad = 6.0f;
    box.align = UiElement::Align::End;
    box.color[3] = 0.5f;   // fundo translúcido
    box.anchorH = UiElement::AnchorH::Right;
    box.anchorV = UiElement::AnchorV::Bottom;
    c->elements.push_back(box);
    UiElement btn;
    btn.kind = UiElement::Kind::Button;
    btn.name = "btn";
    btn.text = "JOGAR";
    btn.image = "textures/wood.png";   // 0.7.4: textura de fundo
    btn.w = 240.0f; btn.h = 72.0f;
    btn.parent = "col";
    btn.color[3] = 0.0f;   // alpha do Label-style preservado
    c->elements.push_back(btn);

    const std::string text = SceneSerializer::dump(s);
    Scene s2;
    SceneSerializer::LoadCtx ctx;
    EXPECT(SceneSerializer::loadText(s2, text, ctx));

    const UiCanvas* c2 = s2.get(s2.find("HUD"))->getComponent<UiCanvas>();
    EXPECT(c2 != nullptr && c2->elements.size() == 2);
    EXPECT(c2->elements[0].kind == UiElement::Kind::VBox);
    EXPECT(nearEqF(c2->elements[0].spacing, 14.0f));
    EXPECT(nearEqF(c2->elements[0].pad, 6.0f));
    EXPECT(c2->elements[0].align == UiElement::Align::End);
    EXPECT(nearEqF(c2->elements[0].color[3], 0.5f));
    EXPECT(c2->elements[1].parent == "col");
    EXPECT(c2->elements[1].image == "textures/wood.png");
    EXPECT(nearEqF(c2->elements[1].color[3], 0.0f));

    // 2º round-trip textual estável (defaults não voltam como campos)
    const std::string text2 = SceneSerializer::dump(s2);
    EXPECT(text2.find("\"spacing\":14") != std::string::npos);
    EXPECT(text2.find("\"parent\":\"col\"") != std::string::npos);
    EXPECT(text2.find("\"image\":\"textures/wood.png\"") != std::string::npos);
    // cena 0.7.3 (sem os campos novos) continua a abrir (forward-compat)
    Scene s3;
    const char* old = R"({"version":1,"tics":[
        {"id":0,"name":"HUD","active":true,"parent":-1,
         "components":[{"type":"UiCanvas","elements":[
            {"kind":"menu","name":"m","text":"Jogar\nSair",
             "x":10,"y":10,"w":300,"h":200,"color":[0.1,0.1,0.1,1],
             "visible":true,"ah":"left","av":"top"}]}]}]})";
    EXPECT(SceneSerializer::loadText(s3, old, ctx));
    const UiCanvas* c3 = s3.get(s3.find("HUD"))->getComponent<UiCanvas>();
    EXPECT(c3 != nullptr && c3->elements.size() == 1);
    EXPECT(c3->elements[0].spacing == 0.0f);   // default 0.7.3
    EXPECT(c3->elements[0].pad == 8.0f);
    EXPECT(c3->elements[0].parent.empty());
}

// ---- 7. runs de submissão: z-order sólidos↔texturas -------------------------------

TEST(uilayout_runs_intercalam_solidos_e_texturas) {
    u8 rgba[16];
    for (int i = 0; i < 16; ++i) {
        rgba[i] = 160;
    }
    if (!g_texA.createFromRGBA(rgba, 2, 2)) {
        EXPECT(!"stub GL não criou a textura");
        return;
    }
    UiContext ui;
    ui.init();
    ui.setImageResolver(&texResolver);

    // painel TEXTURIZADO desenhado PRIMEIRO, botão SÓLIDO desenhado DEPOIS:
    // o run do botão tem de vir DEPOIS do run da textura (o botão fica por
    // cima na tela). Antes da 0.7.4 os sólidos submetiam-se TODOS antes das
    // imagens — o botão desaparecia sob a textura do painel.
    UiElement panel;
    panel.kind = UiElement::Kind::Panel;
    panel.image = "textures/a.png";
    UiElement btn;
    btn.kind = UiElement::Kind::Button;
    btn.text = "OK";

    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    drawElement(ui, panel, {0.0f, 0.0f, 400.0f, 300.0f});
    drawElement(ui, btn, {100.0f, 100.0f, 200.0f, 80.0f});
    EXPECT(ui.runCountForTest() >= 2u);
    EXPECT(ui.runTexForTest(0) != 0u);   // 1º run = textura do painel
    // o ÚLTIMO run é SÓLIDO (o botão desenhado depois submete-se depois)
    const u32 last = ui.runCountForTest() - 1u;
    EXPECT(ui.runTexForTest(last) == 0u);
    // a ordem dos vértices acompanha: os quads do botão estão no batch de
    // sólidos DEPOIS dos quads do painel... (sólidos = painel-frame? não —
    // o painel texturizado só emite o run da textura + frame sólido antes)
    ui.endFrame();
    ui.setImageResolver(nullptr);
}

// ---- 8. seletor de textura de elemento (menuKind 3) -------------------------------

TEST(uilayout_applyuitexpick_escreve_a_ref) {
    Scene s;
    Tic* t = s.get(s.create("HUD"));
    UiCanvas* c = t->addComponent<UiCanvas>();
    UiElement img;
    img.kind = UiElement::Kind::Image;
    img.name = "i";
    c->elements.push_back(img);

    AssetCatalog cat;
    cat.textures = {"textures/wood.png", "textures/brick.png"};

    // escolhe o 1º ficheiro (pick 2) → ref "textures/wood.png"
    UiTexPickOutcome out = applyUiTexPick(s, s.find("HUD"), 0, 2, cat);
    EXPECT(out.applied);
    EXPECT(c->elements[0].image == "textures/wood.png");
    EXPECT(std::string(out.log).find("wood.png") != std::string::npos);

    // none (pick 1) → limpa
    out = applyUiTexPick(s, s.find("HUD"), 0, 1, cat);
    EXPECT(out.applied);
    EXPECT(c->elements[0].image.empty());

    // "importar…" (kAssetPickImport) → NÃO aplica (o chamador abre o browser)
    out = applyUiTexPick(s, s.find("HUD"), 0, kAssetPickImport, cat);
    EXPECT(!out.applied);

    // alvos mortos sem crash: TIC inexistente, elemento fora do array
    EXPECT(!applyUiTexPick(s, Handle{}, 0, 2, cat).applied);
    EXPECT(!applyUiTexPick(s, s.find("HUD"), 99, 2, cat).applied);
    EXPECT(!applyUiTexPick(s, s.find("HUD"), 0, 99, cat).applied);
}
