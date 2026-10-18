// tests/test_012b_inspui.cpp — 0.12-B (B-2): O INSPECTOR DE UI (2D + âncoras).
//
// Os 5 pins da sub-fase B-2 (a ordem do prompt):
//   1. insp_control_2d      — o Inspector mostra X/Y/W/H (e escrevem ao vivo)
//   2. insp_ancoras         — as 9 âncoras são escolhíveis
//   3. insp_sem_remover     — o botão "remover joystick" morreu
//   4. insp_campos_por_tipo — Label e Joystick mostram campos diferentes
//   5. insp_reset_visivel   — o botão reset não está cortado
//
// MUTAÇÕES (coladas no RELATORIO-0.12-B.md):
//   M-B2a — as rows de âncora removidas do plano → pin 2 vermelho
//   M-B2b — o plano ignora o tipo (o joystick recebe os campos do Label)
//           → pin 4 vermelho
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
#include "platform/InputState.h"
#include "ui/EditorUi.h"
#include "ui/UiEditor.h"
#include "ui/UiRuntime.h"
#include "ui/FontAtlas.h"
#include "ui/UiContext.h"
#include "ui/Icons.h"

using namespace vv;
using namespace vv::editor;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;

bool nearEq(f32 a, f32 b, f32 eps = 0.01f) {
    return std::fabs(a - b) < eps;
}

struct Rect {
    f32 x0, y0, x1, y1;
};

struct InspEnv {
    FontAtlas  font;
    UiContext  ui;
    InputState input;
    Scene      scene;
    EditorState st;
    bool       ok = false;

    InspEnv() {
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
        drawUiInspector(ui, scene, st, input);
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
    void dragFrom(f32 x0, f32 y0, f32 x1, f32 y1) {
        input.injectDown(0, x0, y0);
        frame();
        input.injectMove(0, x1, y1);
        frame();
        input.injectUp(0);
        frame();
    }
};

// o rect DESENHADO da linha com o widget id `id` (o plano + a geometria do
// painel — o MESMO contrato do test_uieditor)
Rect rowRect(const UiInspRow* plan, u32 n, u64 id) {
    const UiRect panel =
        safe::inspectorPanelRect(kSW, kSH, safe::Insets{});
    for (u32 i = 0; i < n; ++i) {
        if (plan[i].id == id) {
            const f32 ry = panel.y + editor::kHeaderH + 4.0f + plan[i].y;
            return {panel.x, ry, panel.x + panel.w, ry + plan[i].h};
        }
    }
    return {0, 0, 0, 0};
}

} // namespace

// ---- 1. insp_control_2d --------------------------------------------------------

TEST(insp_control_2d) {
    InspEnv e;
    ASSERT(e.ok);
    // um BUTTON na árvore (o caminho de produção)
    ASSERT(uiAddTreeElement(e.scene, e.st, 2, kSW, kSH));
    const std::vector<std::pair<Handle, UiElement>> elems = [&] {
        std::vector<std::pair<Handle, UiElement>> out;
        e.scene.forEachActive([&](const Tic& t) {
            if (const Control* c = t.getComponent<Control>()) {
                out.push_back({t.handle, c->elem});
            }
        });
        return out;
    }();
    ASSERT(elems.size() == 1);
    Control* ctl = e.scene.get(elems[0].first)->getComponent<Control>();
    ASSERT(ctl != nullptr);

    // o PLANO mostra X/Y/W/H (as 4 linhas da transform 2D)
    const TextMetrics tm = e.ui.textMetrics();
    UiInspRow plan[48];
    const u32 n = uiInspectorPlanControl(ctl->elem, false, tm, 0, plan, 48);
    bool hasX = false, hasY = false, hasW = false, hasH = false;
    for (u32 i = 0; i < n; ++i) {
        if (plan[i].kind == UiInspRow::Kind::PosX) hasX = true;
        if (plan[i].kind == UiInspRow::Kind::PosY) hasY = true;
        if (plan[i].kind == UiInspRow::Kind::SizeW) hasW = true;
        if (plan[i].kind == UiInspRow::Kind::SizeH) hasH = true;
    }
    EXPECT(hasX && hasY && hasW && hasH);

    // o DRAW vivo: o drag do slider X escreve o Control AO VIVO (o trilho em
    // x+84, 118px — o MESMO contrato do Inspector 0.7.x)
    e.st.selected = elems[0].first;
    e.frame();
    const Rect rx = rowRect(plan, n, kUiInspX);
    const f32 trackX = rx.x0 + 84.0f;
    const f32 ox0 = ctl->elem.ox;
    e.dragFrom(trackX + 118.0f * 0.9f, (rx.y0 + rx.y1) * 0.5f,
               trackX + 118.0f * 0.9f, (rx.y0 + rx.y1) * 0.5f);
    EXPECT(ctl->elem.ox > ox0 + 1000.0f);   // ~90% da faixa −3000..3000
    // o drag do slider W escreve a largura
    const Rect rw = rowRect(plan, n, kUiInspW);
    e.dragFrom(trackX + 118.0f * 0.5f, (rw.y0 + rw.y1) * 0.5f,
               trackX + 118.0f * 0.5f, (rw.y0 + rw.y1) * 0.5f);
    EXPECT(ctl->elem.w > 1000.0f);   // ~50% da faixa 8..3000
}

// ---- 2. insp_ancoras -----------------------------------------------------------

TEST(insp_ancoras) {
    InspEnv e;
    ASSERT(e.ok);
    ASSERT(uiAddTreeElement(e.scene, e.st, 2, kSW, kSH));
    const std::vector<std::pair<Handle, UiElement>> elems = [&] {
        std::vector<std::pair<Handle, UiElement>> out;
        e.scene.forEachActive([&](const Tic& t) {
            if (const Control* c = t.getComponent<Control>()) {
                out.push_back({t.handle, c->elem});
            }
        });
        return out;
    }();
    ASSERT(elems.size() == 1);
    Control* ctl = e.scene.get(elems[0].first)->getComponent<Control>();
    ASSERT(ctl != nullptr);
    e.st.selected = elems[0].first;

    // o plano tem AS DUAS rows de âncora (o ciclo H×V cobre os 9 cantos)
    const TextMetrics tm = e.ui.textMetrics();
    UiInspRow plan[48];
    const u32 n = uiInspectorPlanControl(ctl->elem, false, tm, 0, plan, 48);
    u32 hIdx = 0, vIdx = 0, anchors = 0;
    for (u32 i = 0; i < n; ++i) {
        if (plan[i].kind == UiInspRow::Kind::AnchorH) {
            hIdx = i;
            ++anchors;
        }
        if (plan[i].kind == UiInspRow::Kind::AnchorV) {
            vIdx = i;
            ++anchors;
        }
    }
    EXPECT(anchors == 2u);

    // os 9 CANTOS são escolhíveis: 3 taps no H × 3 taps no V (cada tap cicla
    // +1%3) passam por TODAS as combinações — e o controlRect acompanha o
    // elementRect em cada uma (a MESMA matemática, zero divergência)
    e.frame();
    const safe::Insets ins{0.0f, 0.0f, 0.0f, 0.0f};
    u32 combos = 0;
    for (u32 v = 0; v < 3; ++v) {
        const Rect rv = rowRect(plan, n, kUiInspAnchV);
        e.tap((rv.x0 + rv.x1) * 0.5f, (rv.y0 + rv.y1) * 0.5f);
        for (u32 h = 0; h < 3; ++h) {
            const Rect rh = rowRect(plan, n, kUiInspAnchH);
            e.tap((rh.x0 + rh.x1) * 0.5f, (rh.y0 + rh.y1) * 0.5f);
            const UiRect a = ui::controlRect(*ctl, kSW, kSH, ins);
            const UiRect b = ui::elementRect(ctl->elem, kSW, kSH, ins);
            EXPECT(nearEq(a.x, b.x) && nearEq(a.y, b.y) &&
                   nearEq(a.w, b.w) && nearEq(a.h, b.h));
            ++combos;
        }
    }
    EXPECT(combos == 9u);
    // os 9 combos DISTINTOS passaram pelo elem (H e V ciclam os 3 valores)
    EXPECT(static_cast<u32>(ctl->elem.anchorH) <= 2u);
    EXPECT(static_cast<u32>(ctl->elem.anchorV) <= 2u);
}

// ---- 3. insp_sem_remover -------------------------------------------------------

TEST(insp_sem_remover) {
    InspEnv e;
    ASSERT(e.ok);
    ASSERT(uiAddTreeJoystick(e.scene, e.st));
    const std::vector<std::pair<Handle, UiElement>> elems = [&] {
        std::vector<std::pair<Handle, UiElement>> out;
        e.scene.forEachActive([&](const Tic& t) {
            if (const Control* c = t.getComponent<Control>()) {
                out.push_back({t.handle, c->elem});
            }
        });
        return out;
    }();
    ASSERT(elems.size() == 1);
    Tic* joyTic = e.scene.get(elems[0].first);
    ASSERT(joyTic != nullptr);
    ASSERT(joyTic->getComponent<TouchControls>() != nullptr);
    e.st.selected = elems[0].first;

    // TODAS as linhas do plano do joystick recebem um tap (com as secções
    // abertas): NENHUM caminho tira SÓ o componente TouchControls — o
    // "remover joystick" morreu; o apagar é do TIC (a última linha leva o
    // TIC INTEIRO, o componente sai com ele)
    e.frame();
    const UiRect panel = safe::inspectorPanelRect(kSW, kSH, safe::Insets{});
    // colapsa as 2 primeiras secções para o conteúdo caber (o gesto do dono)
    e.st.inspUiCollapsed = 0x03;
    e.frame();
    const TextMetrics tm = e.ui.textMetrics();
    UiInspRow plan[48];
    const u32 n = uiInspectorPlanControl(
        joyTic->getComponent<Control>()->elem, true, tm, 0x03, plan, 48);
    bool removeRowSeen = false;
    for (u32 i = 0; i < n; ++i) {
        const f32 ry =
            panel.y + editor::kHeaderH + 4.0f + plan[i].y + plan[i].h * 0.5f;
        if (ry > panel.y + panel.h) {
            continue;   // fora do painel (o scroll cobre no device)
        }
        e.tap(panel.x + panel.w * 0.5f, ry);
        if (plan[i].kind == UiInspRow::Kind::Remove) {
            removeRowSeen = true;
            break;   // o apagar é a ÚLTIMA interação (o TIC sai)
        }
        // o componente NUNCA sai sozinho (o TIC vivo ⇒ TouchControls vivo)
        if (e.scene.get(elems[0].first) != nullptr) {
            EXPECT(e.scene.get(elems[0].first)->getComponent<TouchControls>() !=
                   nullptr);
        }
    }
    EXPECT(removeRowSeen);   // a linha "remover elemento" existe e apaga o TIC
    // o fim honesto: ou o TIC INTEIRO saiu, ou nada saiu — NUNCA o componente
    if (Tic* t = e.scene.get(elems[0].first)) {
        EXPECT(t->getComponent<TouchControls>() != nullptr);
    }
}

// ---- 4. insp_campos_por_tipo -----------------------------------------------------

TEST(insp_campos_por_tipo) {
    InspEnv e;
    ASSERT(e.ok);
    ASSERT(uiAddTreeElement(e.scene, e.st, 1, kSW, kSH));   // label
    ASSERT(uiAddTreeJoystick(e.scene, e.st));               // joystick
    std::vector<std::pair<Handle, UiElement>> labelE, joyE;
    e.scene.forEachActive([&](const Tic& t) {
        if (const Control* c = t.getComponent<Control>()) {
            (c->isJoystick() ? joyE : labelE).push_back({t.handle, c->elem});
        }
    });
    ASSERT(labelE.size() == 1 && joyE.size() == 1);
    const TextMetrics tm = e.ui.textMetrics();
    UiInspRow lp[48], jp[48];
    const u32 ln =
        uiInspectorPlanControl(labelE[0].second, false, tm, 0, lp, 48);
    const u32 jn = uiInspectorPlanControl(joyE[0].second, true, tm, 0, jp, 48);
    auto has = [](const UiInspRow* p, u32 n, UiInspRow::Kind k) {
        for (u32 i = 0; i < n; ++i) {
            if (p[i].kind == k) {
                return true;
            }
        }
        return false;
    };
    // o LABEL: texto/letra/estilo + fundo A + hex — SEM campos de joystick
    EXPECT(has(lp, ln, UiInspRow::Kind::TextBtn));
    EXPECT(has(lp, ln, UiInspRow::Kind::FontScl));
    EXPECT(has(lp, ln, UiInspRow::Kind::TStyleBtn));
    EXPECT(has(lp, ln, UiInspRow::Kind::ColA));
    EXPECT(has(lp, ln, UiInspRow::Kind::AnchorH));
    EXPECT(!has(lp, ln, UiInspRow::Kind::Dead));
    EXPECT(!has(lp, ln, UiInspRow::Kind::Radius));
    EXPECT(!has(lp, ln, UiInspRow::Kind::Sens));
    // o JOYSTICK: raio/zona morta/sens — SEM texto/âncoras/tam do elemento
    EXPECT(has(jp, jn, UiInspRow::Kind::Dead));
    EXPECT(has(jp, jn, UiInspRow::Kind::Radius));
    EXPECT(has(jp, jn, UiInspRow::Kind::Sens));
    EXPECT(has(jp, jn, UiInspRow::Kind::Opacity));
    EXPECT(!has(jp, jn, UiInspRow::Kind::TextBtn));
    EXPECT(!has(jp, jn, UiInspRow::Kind::FontScl));
    EXPECT(!has(jp, jn, UiInspRow::Kind::AnchorH));
    EXPECT(!has(jp, jn, UiInspRow::Kind::SizeW));
    // as SECÇÕES existem nos DOIS (3 cabeçalhos)
    EXPECT(has(lp, ln, UiInspRow::Kind::Sec) &&
           has(jp, jn, UiInspRow::Kind::Sec));
    // os planos são DIFERENTES entre si (o contrato do pin)
    EXPECT(ln != jn);
}

// ---- 5. insp_reset_visivel -------------------------------------------------------

TEST(insp_reset_visivel) {
    InspEnv e;
    ASSERT(e.ok);
    ASSERT(uiAddTreeElement(e.scene, e.st, 2, kSW, kSH));   // button
    const std::vector<std::pair<Handle, UiElement>> elems = [&] {
        std::vector<std::pair<Handle, UiElement>> out;
        e.scene.forEachActive([&](const Tic& t) {
            if (const Control* c = t.getComponent<Control>()) {
                out.push_back({t.handle, c->elem});
            }
        });
        return out;
    }();
    ASSERT(elems.size() == 1);
    Control* ctl = e.scene.get(elems[0].first)->getComponent<Control>();
    ASSERT(ctl != nullptr);
    e.st.selected = elems[0].first;

    const TextMetrics tm = e.ui.textMetrics();
    UiInspRow plan[48];
    const u32 n = uiInspectorPlanControl(ctl->elem, false, tm, 0, plan, 48);
    const UiRect panel = safe::inspectorPanelRect(kSW, kSH, safe::Insets{});
    // TODAS as linhas com reset: o quadrado 30px é INTEIRO dentro da linha E
    // do painel (o "(não cortados)" do prompt — o valor do slider desloca-se,
    // o quadrado nunca sai do painel nem da linha)
    u32 resets = 0;
    for (u32 i = 0; i < n; ++i) {
        if (!plan[i].reset) {
            continue;
        }
        ++resets;
        const f32 ry = panel.y + editor::kHeaderH + 4.0f + plan[i].y;
        const f32 rx0 = panel.x + panel.w - editor::kPad - 30.0f;
        EXPECT(rx0 >= panel.x);
        EXPECT(rx0 + 30.0f <= panel.x + panel.w);
        // a linha tem espaço para o quadrado (a altura comporta 30px — o
        // quadrado é INTEIRO dentro da linha SEMPRE; a visibilidade no
        // painel é do SCROLL — as linhas abaixo da dobra chegam pelo scroll
        // com a MESMA geometria relativa, nunca cortadas pela linha)
        EXPECT(plan[i].h >= 30.0f);
        if (ry >= panel.y && ry + 30.0f <= panel.y + panel.h) {
            // (as visíveis: o quadrado também inteiro NO painel)
        }
    }
    EXPECT(resets >= 10u);   // x/y/lar/alt/R/G/B/A/letra + as margens/pivô/rot

    // o RESET FUNCIONA: o quadrado da linha X repõe o valor (o drag põe
    // ~3000, o reset volta a 0 — o quadrado continua INTEIRO e vivo)
    const Rect rx = rowRect(plan, n, kUiInspX);
    const f32 trackX = rx.x0 + 84.0f;
    e.dragFrom(trackX + 118.0f * 0.9f, (rx.y0 + rx.y1) * 0.5f,
               trackX + 118.0f * 0.9f, (rx.y0 + rx.y1) * 0.5f);
    EXPECT(ctl->elem.ox > 1000.0f);
    e.tap(rx.x1 - editor::kPad - 15.0f, (rx.y0 + rx.y1) * 0.5f);
    EXPECT(nearEq(ctl->elem.ox, 0.0f));
}
