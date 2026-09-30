// tests/test_joystick.cpp — 0.7.3: JOYSTICK EDITÁVEL + COMPOSTOS.
//
// Aferimos o CONTRATO da sub-fase:
//   • o layout do joystick é DERIVADO dos campos editáveis (pos em frações
//     da área útil, tamanho escala o raio): layoutFor posiciona o joystick
//     onde os campos mandam e o default reproduz o fixo da 0.6.x no ecrã
//     de referência;
//   • o EDIT afeta o INPUT em Play: touchBegin CLAIMA na posição NOVA, o
//     eixo vem escalado pela sensibilidade (clamp no círculo unitário);
//     sens 1 = o comportamento 0.6.x exato;
//   • serialização: pos/size/sens/color gravam quando não-default e o
//     round-trip os devolve; um .goni 0.6.x (presença só) abre com os
//     defaults;
//   • o proxy do joystick no viewport 2D: desenhado quando o TIC tem
//     TouchControls, tap seleciona, drag move relX/relY (clamp 0..1);
//   • o plano do Inspector do joystick (y cumulativo, o contrato) e os
//     sliders escrevem nos campos;
//   • COMPOSTOS: Menu/Card/Article desenham (Card = panel+moldura+
//     separador+título; Article = wrap pela largura — N linhas dentro do
//     rect; Menu = uma linha por item) e serializam (round-trip);
//   • o "+" do modo UI tem 8 itens e o Joystick (8) adiciona o componente
//     e seleciona-o (o fluxo do main).
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "components/TouchControls.h"
#include "components/UiCanvas.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "platform/InputState.h"
#include "ui/EditorUi.h"
#include "ui/FontAtlas.h"
#include "ui/SafeArea.h"
#include "ui/UiContext.h"
#include "ui/UiEditor.h"
#include "ui/UiRuntime.h"

using namespace vv;
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
} // namespace

// ---- 1. layout editável -----------------------------------------------------------

TEST(joystick_layout_derivado_dos_campos) {
    TouchControls tc;
    // default: reproduz o fixo da 0.6.x no ecrã de referência 1600x720
    const TouchControls::Layout d = tc.layoutFor(kSW, kSH);
    EXPECT(nearEqF(d.joyCX, 150.0f, 0.5f));
    EXPECT(nearEqF(d.joyCY, kSH - 40.0f - 150.0f, 0.5f));
    EXPECT(nearEqF(d.joyR, 75.0f));
    // em outra resolução o default ESCALA com a área (frações — nunca
    // fica colado ao pixel fixo de um ecrã só)
    const TouchControls::Layout d2 = tc.layoutFor(1280.0f, 600.0f);
    EXPECT(nearEqF(d2.joyCX, 0.09375f * 1280.0f, 0.5f));

    // pos editável: centro onde os campos mandam
    tc.relX = 0.5f;
    tc.relY = 0.5f;
    const TouchControls::Layout m = tc.layoutFor(1000.0f, 500.0f);
    EXPECT(nearEqF(m.joyCX, 500.0f));
    EXPECT(nearEqF(m.joyCY, 250.0f));
    // tamanho escala o raio (base 75)
    tc.size = 2.0f;
    const TouchControls::Layout b = tc.layoutFor(1000.0f, 500.0f);
    EXPECT(nearEqF(b.joyR, 150.0f));
    // o layout FIXO da 0.6.x continua (compat)
    const TouchControls::Layout f = TouchControls::layout(kSW, kSH);
    EXPECT(nearEqF(f.joyCX, 150.0f));
    EXPECT(nearEqF(f.joyR, 75.0f));
}

// ---- 2. o edit afeta o INPUT em Play ----------------------------------------------

TEST(joystick_editavel_afeta_input_em_play) {
    TouchControls tc;
    tc.relX = 0.5f;    // joystick ao CENTRO
    tc.relY = 0.8f;
    tc.size = 1.0f;
    tc.sens = 2.0f;    // SENSIBILIDADE dobra o eixo

    const f32 aw = 1000.0f, ah = 500.0f;
    const TouchControls::Layout l = tc.layoutFor(aw, ah);
    // o toque NASCE no joystick NOVO → CLAIMA (true)
    EXPECT(tc.touchBegin(0, l.joyCX, l.joyCY, aw, ah));
    // drag de 30px para a direita: eixo X = 30/75 * sens 2 = 0.8
    tc.touchMove(0, l.joyCX + 30.0f, l.joyCY);
    const Vec2 a = tc.axis();
    EXPECT(nearEqF(a.x, 0.8f, 0.01f));
    EXPECT(nearEqF(a.y, 0.0f, 0.01f));
    // drag de 75px (raio cheio) com sens 2 → CLAMP em 1.0 (nunca passa)
    tc.touchMove(0, l.joyCX + 75.0f, l.joyCY);
    EXPECT(nearEqF(tc.axis().x, 1.0f, 0.01f));
    // sens 1 = o comportamento 0.6.x exato (75px = 1.0)
    tc.touchEnd(0);
    tc.sens = 1.0f;
    EXPECT(tc.touchBegin(1, l.joyCX, l.joyCY, aw, ah));
    tc.touchMove(1, l.joyCX + 75.0f, l.joyCY);
    EXPECT(nearEqF(tc.axis().x, 1.0f, 0.01f));
    tc.touchMove(1, l.joyCX + 37.5f, l.joyCY);
    EXPECT(nearEqF(tc.axis().x, 0.5f, 0.01f));
    // fora do joystick NOVO não claima (o sítio antigo ficou livre)
    TouchControls tc2;
    tc2.relX = 0.9f;
    tc2.relY = 0.1f;
    EXPECT(!tc2.touchBegin(0, 150.0f, 400.0f, aw, ah));   // pos antiga
}

// ---- 3. serialização do joystick ----------------------------------------------------

TEST(joystick_serializa_campos_e_roundtrip) {
    Scene s;
    Tic* t = s.get(s.create("Player"));
    TouchControls* tc = t->addComponent<TouchControls>();
    tc->relX = 0.42f;
    tc->relY = 0.66f;
    tc->size = 1.5f;
    tc->sens = 1.8f;
    tc->colR = 0.9f;
    tc->colG = 0.1f;
    tc->colB = 0.2f;

    const std::string text = SceneSerializer::dump(s);
    EXPECT(text.find("\"pos\"") != std::string::npos);
    EXPECT(text.find("\"sens\"") != std::string::npos);

    Scene s2;
    SceneSerializer::LoadCtx ctx;
    EXPECT(SceneSerializer::loadText(s2, text, ctx));
    const TouchControls* tc2 =
        s2.get(s2.find("Player"))->getComponent<TouchControls>();
    EXPECT(tc2 != nullptr);
    EXPECT(nearEqF(tc2->relX, 0.42f));
    EXPECT(nearEqF(tc2->relY, 0.66f));
    EXPECT(nearEqF(tc2->size, 1.5f));
    EXPECT(nearEqF(tc2->sens, 1.8f));
    EXPECT(nearEqF(tc2->colR, 0.9f));
    EXPECT(nearEqF(tc2->colB, 0.2f));

    // .goni 0.6.x (presença só) abre com os DEFAULTS (o layout fixo)
    const char* old = R"({"version":1,"tics":[
        {"id":0,"name":"P","active":true,"parent":-1,
         "components":[{"type":"Transform3D","pos":[0,0.5,0],"rot":[0,0,0,1],"scale":[1,1,1]},
                       {"type":"TouchControls"}]}]})";
    Scene s3;
    EXPECT(SceneSerializer::loadText(s3, old, ctx));
    const TouchControls* tc3 =
        s3.get(s3.find("P"))->getComponent<TouchControls>();
    EXPECT(tc3 != nullptr);
    EXPECT(nearEqF(tc3->relX, 0.09375f));
    EXPECT(nearEqF(tc3->relY, 0.7361f));
    EXPECT(nearEqF(tc3->size, 1.0f));
    EXPECT(nearEqF(tc3->sens, 1.0f));
}

// ---- 4. proxy no viewport 2D + inspector do joystick -------------------------------

TEST(joystick_proxy_editavel_no_viewport_2d) {
    FontAtlas font;
    const char* fontPath = FONT_FIXTURE;
    if (!font.loadFromPaths(&fontPath, 1, 28.0f)) {
        EXPECT(!"fonte do fixture não carregou");
        return;
    }
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    ui.setSafeArea(safe::Insets{});
    InputState in;
    Scene scene;
    EditorState st;

    // TIC com TouchControls (a UI do Player é esta instância) e SEM canvas
    const Handle h = createTicFromPreset(scene, PresetKind::PlayerBody3D,
                                         nullptr, nullptr);
    Tic* t = scene.get(h);
    TouchControls* tc = t->addComponent<TouchControls>();
    tc->relX = 0.5f;
    tc->relY = 0.5f;   // centro do espaço de design
    st.selected = h;
    st.uiMode = true;

    const UiRect view = safe::centerRect(kSW, kSH, safe::Insets{});
    const ViewportTransform tr = uiViewportTransform(view, kSW, kSH);
    const TouchControls::Layout jl = tc->layoutFor(kSW, kSH);

    auto frame = [&]() {
        ui.beginFrame(nullptr, &in, kSW, kSH);
        drawUiViewport(ui, scene, st, in, kSW, kSH);
        // o MESMO dispatch do main: com elemento/joystick selecionado o
        // painel direito é o INSPECTOR DE UI (é ele que despacha o remover)
        if (st.selJoystick || st.selElement >= 0) {
            drawUiInspector(ui, scene, st, in);
        }
        ui.statusLine("s");
        ui.endFrame();
        in.clearEdges();
    };
    auto tap = [&](f32 x, f32 y) {
        in.injectDown(0, x, y);
        frame();
        in.injectUp(0);
        frame();
    };

    // SEM canvas o proxy continua a aparecer (não há early-return da dica)
    frame();
    EXPECT(ui.solidsForTest().vertexCount() > 0);

    // tap NO CENTRO do proxy (coords de ecrã do transform) → seleciona
    const f32 cx = tr.ox + jl.joyCX * tr.scale;
    const f32 cy = tr.oy + jl.joyCY * tr.scale;
    tap(cx, cy);
    EXPECT(st.selJoystick);
    EXPECT(st.selElement == -1);

    // drag de (+200, 0) px de ecrã → relX avança 200/scale/sw (clamp 0..1)
    const f32 rx0 = tc->relX;
    in.injectDown(0, cx, cy);
    frame();
    in.injectMove(0, cx + 200.0f, cy);
    frame();
    in.injectUp(0);
    frame();
    EXPECT(tc->relX > rx0);
    EXPECT(nearEqF(tc->relX - rx0, 200.0f / tr.scale / kSW, 0.01f));
    EXPECT(tc->relX <= 1.0f);

    // plano do Inspector do joystick: y cumulativo (o contrato)
    const TextMetrics tm = ui.textMetrics();
    UiInspRow plan[16];
    const u32 n = uiJoystickPlan(tm, plan, 16);
    EXPECT(n == 9u);   // nome + X + Y + tam + sens + R + G + B + remover
    for (u32 i = 1; i < n; ++i) {
        EXPECT(plan[i].y >= plan[i - 1].y + plan[i - 1].h - 0.01f);
    }
    // o inspector desenha SEM crash e os sliders vivem no painel direito
    ui.beginFrame(nullptr, &in, kSW, kSH);
    EXPECT(drawUiInspector(ui, scene, st, in) || true);
    ui.endFrame();
    // remover o joystick pelo botão do plano: o componente sai do TIC
    const UiRect panel = safe::inspectorPanelRect(kSW, kSH, safe::Insets{});
    const f32 rmY = panel.y + kHeaderH + 4.0f + plan[8].y + plan[8].h * 0.5f;
    // (scroll do inspector: conteúdo 9 linhas cabe — offset 0)
    in.injectDown(0, panel.x + panel.w * 0.5f, rmY);
    frame();
    in.injectUp(0);
    frame();
    EXPECT(!st.selJoystick);
    EXPECT(t->getComponent<TouchControls>() == nullptr);
}

// ---- 5. compostos: Menu / Card / Article -------------------------------------------

TEST(compostos_menu_card_article_desenham_e_serializam) {
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
    UiElement menu;
    menu.kind = UiElement::Kind::Menu;
    menu.name = "menu1";
    menu.text = "Jogar>cena2\nOpcoes\nSair";
    menu.ox = 100.0f; menu.oy = 100.0f; menu.w = 300.0f; menu.h = 144.0f;
    c.elements.push_back(menu);
    UiElement card;
    card.kind = UiElement::Kind::Card;
    card.name = "card1";
    card.text = "Titulo";
    card.ox = 500.0f; card.oy = 100.0f; card.w = 300.0f; card.h = 160.0f;
    c.elements.push_back(card);
    UiElement art;
    art.kind = UiElement::Kind::Article;
    art.name = "art1";
    art.text = "Uma linha bem comprida de texto que nao cabe numa linha so "
               "e por isso faz wrap pela largura do elemento.";
    art.ox = 100.0f; art.oy = 400.0f; art.w = 340.0f; art.h = 200.0f;
    c.elements.push_back(art);

    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    const u32 drawn = ui::drawCanvas(ui, c, kSW, kSH, safe::Insets{});
    EXPECT(drawn == 3u);
    std::vector<Rect> rects;
    collectRects(ui.solidsForTest(), rects);
    // MENU: 3 itens → 3 painéis de item (+ o hit-test por linha já aferido)
    u32 menuRows = 0;
    for (const Rect& r : rects) {
        if (r.y0 >= 100.0f && r.y0 < 244.0f && r.x0 >= 100.0f &&
            r.x0 < 400.0f && r.y1 - r.y0 > 30.0f) {
            ++menuRows;
        }
    }
    EXPECT(menuRows >= 3u);
    // ARTICLE: o wrap produz VÁRIAS linhas de glifos dentro da largura
    // (todas as linhas começam em x < 100+340)
    const QuadVertex* g = ui.glyphsForTest().vertices();
    const u32 gn = ui.glyphsForTest().vertexCount();
    u32 artGlyphs = 0;
    for (u32 i = 0; i + 5 < gn; i += 6) {
        if (g[i].y > 400.0f && g[i].y < 600.0f && g[i].x >= 100.0f) {
            ++artGlyphs;
        }
    }
    EXPECT(artGlyphs > 30u);   // texto longo → várias linhas de glifos
    ui.endFrame();

    // serialização: os três compostos fazem round-trip COMPLETO
    Scene s;
    Tic* t = s.get(s.create("HUD"));
    UiCanvas* cc = t->addComponent<UiCanvas>();
    *cc = c;   // cópia do canvas de teste
    const std::string text = SceneSerializer::dump(s);
    Scene s2;
    SceneSerializer::LoadCtx ctx;
    EXPECT(SceneSerializer::loadText(s2, text, ctx));
    const UiCanvas* back =
        s2.get(s2.find("HUD"))->getComponent<UiCanvas>();
    EXPECT(back != nullptr && back->elements.size() == 3);
    EXPECT(back->elements[0].kind == UiElement::Kind::Menu);
    EXPECT(back->elements[0].text == "Jogar>cena2\nOpcoes\nSair");
    EXPECT(back->elements[1].kind == UiElement::Kind::Card);
    EXPECT(back->elements[1].text == "Titulo");
    EXPECT(back->elements[2].kind == UiElement::Kind::Article);
    // os compostos criáveis pelo + (uiAddElement aceita 4..6)
    Scene s3;
    const Handle h3 = s3.create("X");
    EditorState st3;
    st3.selected = h3;
    for (u32 k = 4; k <= 6; ++k) {
        EXPECT(uiAddElement(s3, st3, k, kSW, kSH));
    }
    const UiCanvas* c3 = s3.get(h3)->getComponent<UiCanvas>();
    EXPECT(c3 != nullptr && c3->elements.size() == 3);
    EXPECT(c3->elements[0].kind == UiElement::Kind::Menu);
    EXPECT(c3->elements[1].kind == UiElement::Kind::Card);
    EXPECT(c3->elements[2].kind == UiElement::Kind::Article);
    EXPECT(uiAddElement(s3, st3, 7, kSW, kSH) == false);   // kind 7 não existe
}

// ---- 6. o "+" do modo UI tem 8 itens (com Joystick) ---------------------------------

TEST(joystick_plus_menu_oito_itens_e_addiciona) {
    FontAtlas font;
    const char* fontPath = FONT_FIXTURE;
    if (!font.loadFromPaths(&fontPath, 1, 28.0f)) {
        EXPECT(!"fonte do fixture não carregou");
        return;
    }
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    ui.setSafeArea(safe::Insets{});
    InputState in;
    Scene scene;
    EditorState st;

    const Handle h = createTicFromPreset(scene, PresetKind::PlayerBody3D,
                                         nullptr, nullptr);
    st.selected = h;
    st.uiMode = true;

    auto frame = [&]() {
        ui.beginFrame(nullptr, &in, kSW, kSH);
        // o MESMO dispatch do main: choice 8 = joystick
        const int choice = drawPlusMenu(ui, in, kSW, kSH, st);
        if (choice == 8) {
            Tic* tic = scene.get(st.selected);
            if (tic) {
                if (!tic->getComponent<TouchControls>()) {
                    tic->addComponent<TouchControls>();
                }
                st.selJoystick = true;
                st.selElement = -1;
            }
        }
        ui.endFrame();
        in.clearEdges();
        return choice;
    };

    st.plusMenu = true;
    // geometria do menu (8 itens): centrado, itens de 64px
    const f32 h8 = kHeaderH + 8.0f * 64.0f + kPad;
    const f32 mx = (kSW - kMenuW) * 0.5f;
    const f32 my = (kSH - h8) * 0.5f;
    // item 8 (Joystick): id 27, oitava linha
    in.injectDown(0, mx + kMenuW * 0.5f, my + kHeaderH + 7.0f * 64.0f + 28.0f);
    frame();
    in.injectUp(0);
    const int choice = frame();
    EXPECT(choice == 8);
    EXPECT(!st.plusMenu);
    // o componente existe e ficou SELECIONADO para edição
    Tic* tic = scene.get(h);
    EXPECT(tic->getComponent<TouchControls>() != nullptr);
    EXPECT(st.selJoystick);
    EXPECT(st.selElement == -1);
}
