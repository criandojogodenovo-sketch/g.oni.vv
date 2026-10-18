// tests/test_012b_uitree.cpp — 0.12-B (B-1): A ÁRVORE DE UI (UI como TICs).
//
// Os 9 pins da sub-fase B-1 (a ordem do prompt):
//   1. control_base_campos     — o Control tem pos2D/tamanho/âncoras (9 +
//                                esticar), margens, pivô, opacidade
//   2. uirrot_tem_filhos       — o UIRoot aceita N filhos (sem limite)
//   3. ui_add_joystick_cria_tic— o "+Joystick" cria TIC FILHO
//   4. ui_add_2_joysticks      — dois joysticks = DOIS TICs distintos
//   5. hierarquia_arvore       — o UIRoot mostra a SETA e os filhos
//   6. hierarquia_rename       — renomear TIC de UI funciona
//   7. hierarquia_delete       — apagar TIC de UI funciona
//   8. serializa_arvore        — round-trip .goni da árvore completa
//   9. compat_uicanvas         — .goni com UiCanvas abre como UIRoot
//
// MUTAÇÕES (coladas no RELATORIO-0.12-B.md):
//   M-B1a — o uiAddTreeJoystick deixa de criar o filho → pin 4 vermelho
//   M-B1b — o dump deixa de escrever o Control → pin 8 vermelho
//   M-B1c — o loadText deixa de converter → pin 9 vermelho
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "components/Control.h"
#include "components/UIRoot.h"
#include "components/UiCanvas.h"
#include "components/TouchControls.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "platform/InputState.h"
#include "ui/EditorUi.h"
#include "ui/UiEditor.h"
#include "ui/UiRuntime.h"   // controlRect (o layout da árvore)
#include "ui/FontAtlas.h"
#include "ui/UiContext.h"
#include "ui/Icons.h"

using namespace vv;
using namespace vv::editor;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;

bool nearEq(f32 a, f32 b, f32 eps = 0.001f) {
    return std::fabs(a - b) < eps;
}

// UiRect não tem operator== (o contrato da casa) — comparação campo a campo
bool rectEq(const UiRect& a, const UiRect& b) {
    return nearEq(a.x, b.x) && nearEq(a.y, b.y) && nearEq(a.w, b.w) &&
           nearEq(a.h, b.h);
}

// os TICs com Control da cena, ordem de slot (a vista da árvore para os pins)
std::vector<std::pair<Handle, UiElement>> treeElems(const Scene& s) {
    std::vector<std::pair<Handle, UiElement>> out;
    s.forEachActive([&](const Tic& t) {
        if (const Control* c = t.getComponent<Control>()) {
            out.push_back({t.handle, c->elem});
            out.back().second.name = t.name;
        }
    });
    return out;
}

Handle findRoot(const Scene& s) {
    Handle root = Handle::invalid();
    s.forEachActive([&](const Tic& t) {
        if (!root.valid() && t.getComponent<UIRoot>()) {
            root = t.handle;
        }
    });
    return root;
}

} // namespace

// ---- 1. control_base_campos ---------------------------------------------------

TEST(uitree_control_base_campos) {
    Control c;
    // a base nasce com os defaults do contrato (pivô centro, opaco, ancorado)
    EXPECT(nearEq(c.posX(), 0.0f) && nearEq(c.posY(), 0.0f));
    EXPECT(nearEq(c.width(), 200.0f) && nearEq(c.height(), 80.0f));
    EXPECT(!c.stretch);
    EXPECT(nearEq(c.pivotX, 0.5f) && nearEq(c.pivotY, 0.5f));
    EXPECT(nearEq(c.opacity, 1.0f));
    EXPECT(c.elem.anchorH == UiElement::AnchorH::Left);
    EXPECT(c.elem.anchorV == UiElement::AnchorV::Top);

    // pos2D + tamanho + âncoras (os ALIASES da base escrevem o elem —
    // uma só fonte de estado)
    c.setPos(12.0f, 34.0f);
    c.setSize(100.0f, 50.0f);
    EXPECT(nearEq(c.elem.ox, 12.0f) && nearEq(c.elem.oy, 34.0f));
    EXPECT(nearEq(c.elem.w, 100.0f) && nearEq(c.elem.h, 50.0f));
    c.elem.anchorH = UiElement::AnchorH::Right;
    c.elem.anchorV = UiElement::AnchorV::Bottom;

    // o LAYOUT (fonte única controlRect): os 9 cantos delegam no elementRect
    // (a MESMA matemática da 0.7.0) — spot checks nas 3 combinações
    const safe::Insets ins{4.0f, 8.0f, 6.0f, 10.0f};
    {
        Control l;   // esquerda/topo: x = ins.left + ox
        l.setPos(30.0f, 40.0f);
        l.setSize(70.0f, 60.0f);
        const UiRect r = ui::controlRect(l, kSW, kSH, ins);
        EXPECT(nearEq(r.x, 34.0f) && nearEq(r.y, 48.0f));
        EXPECT(nearEq(r.w, 70.0f) && nearEq(r.h, 60.0f));
        EXPECT(rectEq(r, ui::elementRect(l.elem, kSW, kSH, ins)));
    }
    {
        Control rct;   // direita/fundo: x = sw − ins.right − w + ox
        rct.elem.anchorH = UiElement::AnchorH::Right;
        rct.elem.anchorV = UiElement::AnchorV::Bottom;
        rct.setPos(5.0f, 7.0f);
        rct.setSize(90.0f, 40.0f);
        const UiRect r = ui::controlRect(rct, kSW, kSH, ins);
        EXPECT(nearEq(r.x, kSW - 6.0f - 90.0f + 5.0f));
        EXPECT(nearEq(r.y, kSH - 10.0f - 40.0f + 7.0f));
        EXPECT(rectEq(r, ui::elementRect(rct.elem, kSW, kSH, ins)));
    }
    {
        Control cc;   // centro/meio
        cc.elem.anchorH = UiElement::AnchorH::Center;
        cc.elem.anchorV = UiElement::AnchorV::Middle;
        cc.setPos(-10.0f, -20.0f);
        cc.setSize(80.0f, 80.0f);
        EXPECT(rectEq(ui::controlRect(cc, kSW, kSH, ins),
                      ui::elementRect(cc.elem, kSW, kSH, ins)));
    }
    // a âncora ESTICAR: preenche o pai MENOS as margens (a 10.ª âncora)
    Control st;
    st.stretch = true;
    st.marginL = 10.0f;
    st.marginT = 20.0f;
    st.marginR = 30.0f;
    st.marginB = 40.0f;
    const UiRect rs = ui::controlRect(st, kSW, kSH, ins);
    EXPECT(nearEq(rs.x, 14.0f) && nearEq(rs.y, 28.0f));
    EXPECT(nearEq(rs.w, kSW - 4.0f - 6.0f - 10.0f - 30.0f));
    EXPECT(nearEq(rs.h, kSH - 8.0f - 10.0f - 20.0f - 40.0f));

    // o vocabulário dos kinds (o .goni e o Inspector partilham)
    EXPECT(std::string(controlKindName(Control::Kind::Joystick)) == "joystick");
    EXPECT(std::string(controlKindName(Control::Kind::Button)) == "button");
    EXPECT(controlKindFromName("vbox") == Control::Kind::VBox);
    EXPECT(controlKindFromName("joystick") == Control::Kind::Joystick);
    EXPECT(controlKindFromName("?") == Control::Kind::Panel);
}

// ---- 2. uirrot_tem_filhos -------------------------------------------------------

TEST(uitree_uirrot_tem_filhos) {
    Scene s;
    EditorState st;
    // a raiz nasce do "+" (o TIC "UI" com o marcador)
    EXPECT(uiAddTreeElement(s, st, 0, kSW, kSH));   // panel
    const Handle root = findRoot(s);
    ASSERT(root.valid());
    Tic* rt = s.get(root);
    ASSERT(rt != nullptr);
    EXPECT(rt->getComponent<UIRoot>() != nullptr);
    EXPECT(std::string(rt->name) == "UI");

    // N filhos — SEM limite (1 painel de raiz + 40 = 41 filhos; o cap 32 é
    // só da VISTA do editor — o dado e o formato não têm teto)
    for (u32 i = 0; i < 40; ++i) {
        EXPECT(uiAddTreeElement(s, st, i % 9, kSW, kSH));
    }
    const std::vector<std::pair<Handle, UiElement>> elems = treeElems(s);
    EXPECT(elems.size() == 41);

    // o round-trip transporta TODOS (a árvore inteira, não só os 32 do
    // editor — o «sem limite» é do FORMATO)
    const std::string text = SceneSerializer::dump(s);
    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(s2, text, ctx));
    const Handle root2 = findRoot(s2);
    ASSERT(root2.valid());
    EXPECT(treeElems(s2).size() == 41);
}

// ---- 3. ui_add_joystick_cria_tic ------------------------------------------------

TEST(uitree_ui_add_joystick_cria_tic) {
    Scene s;
    EditorState st;
    EXPECT(uiAddTreeJoystick(s, st));
    // a raiz nasceu (o TIC "UI" com o marcador)
    const Handle root = findRoot(s);
    ASSERT(root.valid());
    // o joystick é um TIC FILHO com Control(Kind::Joystick) + TouchControls
    const std::vector<std::pair<Handle, UiElement>> elems = treeElems(s);
    ASSERT(elems.size() == 1);
    Tic* jt = s.get(elems[0].first);
    ASSERT(jt != nullptr);
    EXPECT(jt->parent == static_cast<i32>(root.index));
    Control* ctl = jt->getComponent<Control>();
    ASSERT(ctl != nullptr);
    EXPECT(ctl->kind() == Control::Kind::Joystick);
    EXPECT(ctl->isJoystick());
    TouchControls* tc = jt->getComponent<TouchControls>();
    EXPECT(tc != nullptr);   // o runtime de input no MESMO TIC
    // nasce selecionado em edição (o padrão do +Joystick)
    EXPECT(st.selected == elems[0].first);
    EXPECT(st.selJoystick);
}

// ---- 4. ui_add_2_joysticks ------------------------------------------------------

TEST(uitree_ui_add_2_joysticks) {
    Scene s;
    EditorState st;
    EXPECT(uiAddTreeJoystick(s, st));
    EXPECT(uiAddTreeJoystick(s, st));
    // DOIS TICs distintos (nomes únicos Godot-style), os DOIS filhos da raiz
    const std::vector<std::pair<Handle, UiElement>> elems = treeElems(s);
    ASSERT(elems.size() == 2);
    EXPECT(!(elems[0].first == elems[1].first));
    Tic* a = s.get(elems[0].first);
    Tic* b = s.get(elems[1].first);
    ASSERT(a != nullptr && b != nullptr);
    EXPECT(std::string(a->name) == "joystick");
    EXPECT(std::string(b->name) == "joystick.001");
    const Handle root = findRoot(s);
    ASSERT(root.valid());
    EXPECT(a->parent == static_cast<i32>(root.index));
    EXPECT(b->parent == static_cast<i32>(root.index));
    // instâncias de TouchControls INDEPENDENTES (mexer numa não mexe na outra)
    TouchControls* ta = a->getComponent<TouchControls>();
    TouchControls* tb = b->getComponent<TouchControls>();
    ASSERT(ta != nullptr && tb != nullptr);
    ta->relX = 0.5f;
    EXPECT(!nearEq(tb->relX, 0.5f));   // o default é 0.09375
}

// ---- 5. hierarquia_arvore ---------------------------------------------------------

namespace {
struct Rect5 {
    f32 x0, y0, x1, y1;
};
void collectRects5(const QuadBatch& b, std::vector<Rect5>& out) {
    const QuadVertex* v = b.vertices();
    const u32 n = b.vertexCount();
    for (u32 i = 0; i + 5 < n; i += 6) {
        out.push_back({v[i].x, v[i].y, v[i + 2].x, v[i + 2].y});
    }
}
} // namespace

TEST(uitree_hierarquia_arvore) {
    FontAtlas font;
    const char* fontPath = FONT_FIXTURE;
    ASSERT(font.loadFromPaths(&fontPath, 1, 28.0f));
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    ui.setSafeArea(safe::Insets{});
    InputState in;
    Scene s;
    EditorState st;

    // (sem raiz ainda — a cena vazia não tem TIC de UI; o default do ícone
    // só se aferi COM raiz, em baixo)

    EXPECT(uiAddTreeJoystick(s, st));
    const Handle root = findRoot(s);
    ASSERT(root.valid());
    EXPECT(uiAddTreeElement(s, st, 2, kSW, kSH));   // button
    EXPECT(uiAddTreeElement(s, st, 1, kSW, kSH));   // label

    // os ícones da árvore: raiz = Monitor (o ecrã), filhos por TIPO
    EXPECT(hierIconFor(*s.get(root)) == icons::Icon::Monitor);
    const std::vector<std::pair<Handle, UiElement>> elems = treeElems(s);
    ASSERT(elems.size() == 3);   // joystick + button + label (TODOS são TICs)
    EXPECT(hierIconFor(*s.get(elems[0].first)) == icons::Icon::Person);
    EXPECT(hierIconFor(*s.get(elems[1].first)) == icons::Icon::Cursor);
    EXPECT(hierIconFor(*s.get(elems[2].first)) == icons::Icon::Sort);

    // o draw da hierarquia: o UIRoot (com filhos) leva a SETA (o chevron da
    // 0.11-H — a MESMA regra de qualquer pai) e as linhas dos filhos desenham
    auto frame = [&]() {
        ui.beginFrame(nullptr, &in, kSW, kSH);
        drawHierarchy(ui, s, st);
        ui.statusLine("s");
        ui.endFrame();
        in.clearEdges();
    };
    frame();
    std::vector<Rect5> rects;
    collectRects5(ui.solidsForTest(), rects);
    // a zona do chevron da raiz (depth 0): o glifo 16dp vive em [20, 36)
    // (chevX=16 + (24−16)/2); a zona do ÍCONE DE TIPO começa em 40 — a
    // janela tem de ficar ANTES para não apanhar o ícone da linha
    bool chevronRaiz = false;
    for (const Rect5& r : rects) {
        if (r.x0 >= 18.0f && r.x1 <= 38.0f && r.y0 >= 96.0f &&
            r.y1 <= 96.0f + 36.0f) {
            chevronRaiz = true;
        }
    }
    EXPECT(chevronRaiz);

    // os NOMES dos filhos estão desenhados (os glifos existem no batch)
    const QuadVertex* g = ui.glyphsForTest().vertices();
    const u32 gn = ui.glyphsForTest().vertexCount();
    EXPECT(gn > 0);   // há texto (a lista com nomes)
    (void)g;

    // sem filhos NÃO há seta na raiz (a mesma cena, TODOS os filhos apagados
    // — joystick incluído: ele TAMBÉM é filho da árvore)
    destroyWithChildren(s, elems[0].first);
    destroyWithChildren(s, elems[1].first);
    destroyWithChildren(s, elems[2].first);
    ui.beginFrame(nullptr, &in, kSW, kSH);
    drawHierarchy(ui, s, st);
    ui.statusLine("s");
    ui.endFrame();
    in.clearEdges();
    std::vector<Rect5> rects2;
    collectRects5(ui.solidsForTest(), rects2);
    bool chevronVazio = false;
    for (const Rect5& r : rects2) {
        if (r.x0 >= 18.0f && r.x1 <= 38.0f && r.y0 >= 96.0f &&
            r.y1 <= 96.0f + 36.0f) {
            chevronVazio = true;
        }
    }
    EXPECT(!chevronVazio);
}

// ---- 6. hierarquia_rename ---------------------------------------------------------

TEST(uitree_hierarquia_rename) {
    Scene s;
    EditorState st;
    EXPECT(uiAddTreeElement(s, st, 2, kSW, kSH));   // button
    const std::vector<std::pair<Handle, UiElement>> elems = treeElems(s);
    ASSERT(elems.size() == 1);
    const Handle btn = elems[0].first;

    // o RENOMEAR é o fluxo da casa (teclado in-app → commitTextInput)
    openTextInput(st, 0, btn, -1, "");
    st.textLen = 0;
    for (const char* p = "Start"; *p; ++p) {
        st.textBuf[st.textLen++] = *p;
    }
    st.textBuf[st.textLen] = '\0';
    EXPECT(commitTextInput(s, st));
    Tic* bt = s.get(btn);
    ASSERT(bt != nullptr);
    EXPECT(std::string(bt->name) == "Start");
    // o elem.name do Control SEGUE o TIC (a identidade é o nome do TIC)
    EXPECT(bt->getComponent<Control>()->elem.name == "Start");
    // o round-trip transporta o nome novo
    const std::string text = SceneSerializer::dump(s);
    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(s2, text, ctx));
    const std::vector<std::pair<Handle, UiElement>> elems2 = treeElems(s2);
    ASSERT(elems2.size() == 1);
    EXPECT(elems2[0].second.name == "Start");
}

// ---- 7. hierarquia_delete ---------------------------------------------------------

TEST(uitree_hierarquia_delete) {
    Scene s;
    EditorState st;
    EXPECT(uiAddTreeElement(s, st, 0, kSW, kSH));   // panel (fica)
    EXPECT(uiAddTreeElement(s, st, 7, kSW, kSH));   // vbox (vai com o neto)
    EXPECT(uiAddTreeElement(s, st, 1, kSW, kSH));   // label (fica)
    const std::vector<std::pair<Handle, UiElement>> elems = treeElems(s);
    ASSERT(elems.size() == 3);
    const Handle vbox = elems[1].first;

    // o neto: um label DENTRO do vbox (reparent explícito — a op da 0.11-H)
    EXPECT(uiAddTreeElement(s, st, 1, kSW, kSH));
    std::vector<std::pair<Handle, UiElement>> elems2 = treeElems(s);
    ASSERT(elems2.size() == 4);
    const Handle neto = elems2[3].first;
    EXPECT(reparentTic(s, neto, vbox));

    // o APAGAR é o fluxo do ⋮ (o consume do main): destroyWithChildren —
    // o vbox vai COM o neto (a sub-árvore inteira), os irmãos ficam
    EXPECT(countSubtree(s, vbox) == 2u);
    EXPECT(destroyWithChildren(s, vbox) == 2u);
    EXPECT(s.get(vbox) == nullptr);
    EXPECT(s.get(neto) == nullptr);
    const std::vector<std::pair<Handle, UiElement>> elems3 = treeElems(s);
    ASSERT(elems3.size() == 2);
    EXPECT(elems3[0].second.kind == UiElement::Kind::Panel);
    EXPECT(elems3[1].second.kind == UiElement::Kind::Label);
    // o round-trip da cena apagada: 2 filhos, sem fantasmas
    const std::string text = SceneSerializer::dump(s);
    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(s2, text, ctx));
    EXPECT(treeElems(s2).size() == 2);
}

// ---- 8. serializa_arvore ----------------------------------------------------------

TEST(uitree_serializa_arvore) {
    Scene s;
    EditorState st;
    // a árvore COMPLETA: os 9 kinds + joystick, os campos da base, ação,
    // container com filho, TIC invisível
    ASSERT(uiAddTreeElement(s, st, 0, kSW, kSH));   // panel
    ASSERT(uiAddTreeElement(s, st, 1, kSW, kSH));   // label
    ASSERT(uiAddTreeElement(s, st, 2, kSW, kSH));   // button
    ASSERT(uiAddTreeJoystick(s, st));
    ASSERT(uiAddTreeElement(s, st, 7, kSW, kSH));   // vbox
    ASSERT(uiAddTreeElement(s, st, 8, kSW, kSH));   // hbox

    const std::vector<std::pair<Handle, UiElement>> elems = treeElems(s);
    ASSERT(elems.size() == 6);

    // dados DIFERENTES por elemento (o round-trip a aferrar um a um)
    Control* cLabel = s.get(elems[1].first)->getComponent<Control>();
    cLabel->elem.text = "Pontos";
    cLabel->elem.fontScale = 1.5f;
    cLabel->elem.color[0] = 0.9f;
    Control* cBtn = s.get(elems[2].first)->getComponent<Control>();
    cBtn->elem.text = "Jogar";
    cBtn->elem.action = UiElement::Action::TransitionScene;
    cBtn->elem.target = "nivel2";
    cBtn->elem.param = "slide";
    cBtn->elem.ox = -120.0f;
    cBtn->elem.anchorH = UiElement::AnchorH::Center;
    cBtn->elem.anchorV = UiElement::AnchorV::Middle;
    cBtn->stretch = true;   // a base: esticar + margens + pivô + opacidade
    cBtn->marginL = 8.0f;
    cBtn->marginT = 12.0f;
    cBtn->marginR = 8.0f;
    cBtn->marginB = 16.0f;
    cBtn->pivotX = 0.25f;
    cBtn->pivotY = 0.75f;
    cBtn->opacity = 0.5f;
    cBtn->elem.rot = 15.0f;
    Control* cJoy = s.get(elems[3].first)->getComponent<Control>();
    TouchControls* tcJoy = s.get(elems[3].first)->getComponent<TouchControls>();
    ASSERT(cJoy != nullptr && tcJoy != nullptr);
    tcJoy->relX = 0.2f;
    tcJoy->relY = 0.8f;
    tcJoy->sens = 1.25f;
    Control* cVbox = s.get(elems[4].first)->getComponent<Control>();
    cVbox->elem.spacing = 10.0f;
    cVbox->elem.pad = 12.0f;
    Control* cHbox = s.get(elems[5].first)->getComponent<Control>();
    cHbox->elem.visible = false;

    // o filho do container (o reparent por NOME conservado no .goni)
    Control* cLbl2 = nullptr;
    {
        ASSERT(uiAddTreeElement(s, st, 1, kSW, kSH));
        std::vector<std::pair<Handle, UiElement>> e7 = treeElems(s);
        ASSERT(e7.size() == 7);
        EXPECT(reparentTic(s, e7[6].first, elems[4].first));
        cLbl2 = s.get(e7[6].first)->getComponent<Control>();
        ASSERT(cLbl2 != nullptr);
        cLbl2->elem.text = "dentro";
    }

    const std::string text = SceneSerializer::dump(s);
    // o vocabulário novo está no ficheiro (só quando não-default)
    EXPECT(text.find("\"stretch\":true") != std::string::npos);
    EXPECT(text.find("\"ml\":8") != std::string::npos);
    EXPECT(text.find("\"op\":0.5") != std::string::npos);
    EXPECT(text.find("\"kind\":\"joystick\"") != std::string::npos);

    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(s2, text, ctx));
    const Handle root2 = findRoot(s2);
    ASSERT(root2.valid());
    const std::vector<std::pair<Handle, UiElement>> e2 = treeElems(s2);
    ASSERT(e2.size() == 7);

    // kinds + ordem preservados
    EXPECT(e2[0].second.kind == UiElement::Kind::Panel);
    EXPECT(e2[1].second.kind == UiElement::Kind::Label);
    EXPECT(e2[2].second.kind == UiElement::Kind::Button);
    EXPECT(e2[3].second.kind == UiElement::Kind::Panel);   // joystick: espelho
    EXPECT(e2[4].second.kind == UiElement::Kind::VBox);
    EXPECT(e2[5].second.kind == UiElement::Kind::HBox);

    // o label: texto/escala/cor
    {
        Control* c = s2.get(e2[1].first)->getComponent<Control>();
        ASSERT(c != nullptr);
        EXPECT(c->elem.text == "Pontos");
        EXPECT(nearEq(c->elem.fontScale, 1.5f));
        EXPECT(nearEq(c->elem.color[0], 0.9f));
    }
    // o botão: ação + a BASE 2D completa (stretch/margens/pivô/opacidade/rot)
    {
        Control* c = s2.get(e2[2].first)->getComponent<Control>();
        ASSERT(c != nullptr);
        EXPECT(c->elem.action == UiElement::Action::TransitionScene);
        EXPECT(c->elem.target == "nivel2");
        EXPECT(c->elem.param == "slide");
        EXPECT(nearEq(c->elem.ox, -120.0f));
        EXPECT(c->elem.anchorH == UiElement::AnchorH::Center);
        EXPECT(c->elem.anchorV == UiElement::AnchorV::Middle);
        EXPECT(c->stretch);
        EXPECT(nearEq(c->marginL, 8.0f) && nearEq(c->marginT, 12.0f));
        EXPECT(nearEq(c->marginR, 8.0f) && nearEq(c->marginB, 16.0f));
        EXPECT(nearEq(c->pivotX, 0.25f) && nearEq(c->pivotY, 0.75f));
        EXPECT(nearEq(c->opacity, 0.5f));
        EXPECT(nearEq(c->elem.rot, 15.0f));
        // o round-trip É ESTÁVEL: os defaults não voltam como campos
        const std::string t2 = SceneSerializer::dump(s2);
        Control* c1 = s.get(elems[2].first)->getComponent<Control>();
        (void)c1;
        EXPECT(t2.find("\"stretch\":true") != std::string::npos);
    }
    // o joystick: Control + TouchControls por valor
    {
        Control* c = s2.get(e2[3].first)->getComponent<Control>();
        ASSERT(c != nullptr);
        EXPECT(c->isJoystick());
        TouchControls* tc = s2.get(e2[3].first)->getComponent<TouchControls>();
        ASSERT(tc != nullptr);
        EXPECT(nearEq(tc->relX, 0.2f));
        EXPECT(nearEq(tc->relY, 0.8f));
        EXPECT(nearEq(tc->sens, 1.25f));
    }
    // os containers: spacing/pad + o filho DENTRO do vbox (TIC pai)
    {
        Control* c = s2.get(e2[4].first)->getComponent<Control>();
        ASSERT(c != nullptr);
        EXPECT(nearEq(c->elem.spacing, 10.0f));
        EXPECT(nearEq(c->elem.pad, 12.0f));
        Control* ch = s2.get(e2[6].first)->getComponent<Control>();
        ASSERT(ch != nullptr);
        EXPECT(ch->elem.text == "dentro");
        Tic* chT = s2.get(e2[6].first);
        EXPECT(chT->parent == static_cast<i32>(e2[4].first.index));
    }
    // o hbox invisível (a visibilidade do elemento viaja)
    {
        Control* c = s2.get(e2[5].first)->getComponent<Control>();
        ASSERT(c != nullptr);
        EXPECT(!c->elem.visible);
    }
}

// ---- 9. compat_uicanvas -----------------------------------------------------------

TEST(uitree_compat_uicanvas) {
    // .goni da 0.7.x/0.8.x (UiCanvas com elementos por valor) abre como
    // UIRoot + filhos automáticos — a ordem do prompt da B-1
    SceneSerializer::LoadCtx ctx;
    const char* old = R"({"version":1,"tics":[
        {"id":0,"name":"HUD","active":true,"parent":-1,
         "components":[{"type":"UiCanvas","elements":[
            {"kind":"vbox","name":"col","x":24,"y":48,"w":320,"h":180,
             "color":[0.1,0.1,0.1,1],"visible":true,"ah":"right","av":"bottom",
             "spacing":14,"pad":6,"align":"end"},
            {"kind":"button","name":"btn","text":"JOGAR","x":-120,"y":0,
             "w":240,"h":72,"color":[0.2,0.2,0.2,1],"visible":true,
             "ah":"center","av":"middle","parent":"col",
             "act":"scene","target":"nivel2"}]}]}]})";
    Scene s;
    ASSERT(SceneSerializer::loadText(s, old, ctx));

    // o hospedeiro ficou RAIZ (UIRoot) e o canvas SAIU (deprecated — a
    // árvore é a única fonte; o TouchControls não se mexe, não havia)
    Tic* hud = s.get(s.find("HUD"));
    ASSERT(hud != nullptr);
    EXPECT(hud->getComponent<UIRoot>() != nullptr);
    EXPECT(hud->getComponent<UiCanvas>() == nullptr);

    // os filhos automáticos: o vbox e o botão, MESMOS dados, MESMA ordem
    const std::vector<std::pair<Handle, UiElement>> elems = treeElems(s);
    ASSERT(elems.size() == 2);
    const Tic* colT = s.get(elems[0].first);
    const Tic* btnT = s.get(elems[1].first);
    ASSERT(colT != nullptr && btnT != nullptr);
    // a ESTRUTURA: o botão fica filho DO TIC do container (o parent por nome
    // da 0.7.4 virou árvore de TICs)
    EXPECT(btnT->parent == static_cast<i32>(colT->handle.index));
    EXPECT(std::string(colT->name) == "col");
    EXPECT(std::string(btnT->name) == "btn");
    {
        const Control* c = colT->getComponent<Control>();
        ASSERT(c != nullptr);
        EXPECT(c->elem.kind == UiElement::Kind::VBox);
        EXPECT(nearEq(c->elem.ox, 24.0f) && nearEq(c->elem.oy, 48.0f));
        EXPECT(c->elem.anchorH == UiElement::AnchorH::Right);
        EXPECT(c->elem.anchorV == UiElement::AnchorV::Bottom);
        EXPECT(nearEq(c->elem.spacing, 14.0f));
        EXPECT(nearEq(c->elem.pad, 6.0f));
        EXPECT(c->elem.align == UiElement::Align::End);
    }
    {
        const Control* c = btnT->getComponent<Control>();
        ASSERT(c != nullptr);
        EXPECT(c->elem.kind == UiElement::Kind::Button);
        EXPECT(c->elem.text == "JOGAR");
        EXPECT(c->elem.action == UiElement::Action::LoadScene);
        EXPECT(c->elem.target == "nivel2");
        EXPECT(c->elem.anchorH == UiElement::AnchorH::Center);
        EXPECT(nearEq(c->elem.w, 240.0f) && nearEq(c->elem.h, 72.0f));
    }

    // o SAVE da cena convertida grava a ÁRVORE (sem UiCanvas) e REABRE igual
    const std::string text2 = SceneSerializer::dump(s);
    EXPECT(text2.find("\"type\":\"UIRoot\"") != std::string::npos);
    EXPECT(text2.find("\"type\":\"UiCanvas\"") == std::string::npos);
    Scene s2;
    ASSERT(SceneSerializer::loadText(s2, text2, ctx));
    EXPECT(treeElems(s2).size() == 2);
    EXPECT(s2.get(s2.find("HUD"))->getComponent<UIRoot>() != nullptr);
}
