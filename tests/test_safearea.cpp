// tests/test_safearea.cpp — F4.2/B1: safe-area (ui/SafeArea.h, GL-free).
// Insets a partir do contentRect do NativeActivity, TODOS os rects do layout
// dentro do contentRect, e a REGRESSÃO do bug do C33: com a altura REAL do
// painel (nav bar contabilizada) o overflow do Inspector é detetado e o
// scroll ativa — antes, com o visibleHeight inflado, maxOffset era 0.
#include "TestFramework.h"
#include "ui/SafeArea.h"
#include "ui/EditorLayout.h"   // kHeaderH — alturas de conteúdo partilhadas
#include "core/Presets.h"
#include "core/Scene.h"
#include "components/BodyComp.h"

using namespace vv;
using namespace vv::safe;
using namespace vv::editor;
using ::test::nearEqF;

TEST(safearea_insets_do_content_rect) {
    // nav bar LATERAL (direita, 48 px) + status bar no topo (24 px)
    const Insets side = insetsFromContentRect(1600.0f, 720.0f, 0, 24, 1552, 720);
    EXPECT(nearEqF(side.left, 0.0f));
    EXPECT(nearEqF(side.top, 24.0f));
    EXPECT(nearEqF(side.right, 48.0f));
    EXPECT(nearEqF(side.bottom, 0.0f));

    // nav bar EM BAIXO (3 botões em landscape, 92 px) + status no topo
    const Insets bar = insetsFromContentRect(1600.0f, 720.0f, 0, 24, 1600, 628);
    EXPECT(nearEqF(bar.top, 24.0f) && nearEqF(bar.bottom, 92.0f));
    EXPECT(nearEqF(bar.left, 0.0f) && nearEqF(bar.right, 0.0f));

    // rect zero/inválido (o glue começa a zero; nem toda a ROM envia) → sem
    // insets = comportamento antigo (superfície inteira)
    const Insets zero = insetsFromContentRect(1600.0f, 720.0f, 0, 0, 0, 0);
    EXPECT(nearEqF(zero.left, 0.0f) && nearEqF(zero.top, 0.0f));
    EXPECT(nearEqF(zero.right, 0.0f) && nearEqF(zero.bottom, 0.0f));

    // rect degenerado (largura/altura nula) → sem insets
    const Insets degen = insetsFromContentRect(1600.0f, 720.0f, 100, 100, 100, 400);
    EXPECT(nearEqF(degen.right, 0.0f));

    // rect além da superfície / negativos → clamp a 0 (nunca inset negativo)
    const Insets over = insetsFromContentRect(1600.0f, 720.0f, -20, -8, 1700, 800);
    EXPECT(nearEqF(over.left, 0.0f) && nearEqF(over.top, 0.0f));
    EXPECT(nearEqF(over.right, 0.0f) && nearEqF(over.bottom, 0.0f));

    // superfície ainda não criada (0x0) → sem insets
    const Insets nosurf = insetsFromContentRect(0.0f, 0.0f, 0, 24, 1600, 700);
    EXPECT(nearEqF(nosurf.bottom, 0.0f));
}

TEST(safearea_layout_todo_dentro_do_content_rect) {
    // C33 pior caso: status topo 24 + nav bar fundo 92
    const Insets in = insetsFromContentRect(1600.0f, 720.0f, 0, 24, 1600, 628);
    const UiRect content = contentRect(1600.0f, 720.0f, in);
    EXPECT(nearEqF(content.y, 24.0f));
    EXPECT(nearEqF(content.h, 604.0f));

    // INVARIANTE central: nada de UI fora do contentRect
    EXPECT(rectInside(toolbarRect(1600.0f, 720.0f, in), content));
    EXPECT(rectInside(statusRect(1600.0f, 720.0f, in), content));
    EXPECT(rectInside(viewportRect(1600.0f, 720.0f, in), content));
    EXPECT(rectInside(hierarchyPanelRect(1600.0f, 720.0f, in), content));
    EXPECT(rectInside(inspectorPanelRect(1600.0f, 720.0f, in), content));
    EXPECT(rectInside(centerRect(1600.0f, 720.0f, in), content));

    // âncoras 0.9.0: TOP BAR 56 colada ao topo ÚTIL; a tab bar de modo
    // (48) fica LOGO abaixo; status colada ao fundo ÚTIL
    const UiRect tb = toolbarRect(1600.0f, 720.0f, in);
    EXPECT(nearEqF(tb.y, 24.0f));
    EXPECT(nearEqF(tb.h, kTopBarH));
    const UiRect mt = modeTabRect(1600.0f, 720.0f, in);
    EXPECT(nearEqF(mt.y, 24.0f + kTopBarH));
    EXPECT(nearEqF(mt.h, kModeTabH));
    const UiRect bt = bottomTabRect(1600.0f, 720.0f, in);
    EXPECT(nearEqF(bt.y + bt.h + kStatusH, 628.0f));
    const UiRect st = statusRect(1600.0f, 720.0f, in);
    EXPECT(nearEqF(st.y + st.h, 628.0f));   // 720 − 92

    // nav bar lateral: hierarquia começa no inset esquerdo; inspector acaba
    // no inset direito
    const Insets lado = insetsFromContentRect(1600.0f, 720.0f, 40, 0, 1548, 720);
    const UiRect hier = hierarchyPanelRect(1600.0f, 720.0f, lado);
    EXPECT(nearEqF(hier.x, 40.0f));
    const UiRect insp = inspectorPanelRect(1600.0f, 720.0f, lado);
    EXPECT(nearEqF(insp.x + insp.w, 1548.0f));
    EXPECT(rectInside(hier, contentRect(1600.0f, 720.0f, lado)));
    EXPECT(rectInside(insp, contentRect(1600.0f, 720.0f, lado)));

    // sem insets (rect não recebido) → rects idênticos ao comportamento antigo
    const Insets zero{};
    const UiRect tb0 = toolbarRect(1600.0f, 720.0f, zero);
    EXPECT(nearEqF(tb0.x, 0.0f) && nearEqF(tb0.y, 0.0f));
    const UiRect vp0 = viewportRect(1600.0f, 720.0f, zero);
    // 0.9.0: o viewport perde top bar 56 + tab bar 48 + tab bar de baixo 48
    // + status 24 (spec D/E)
    EXPECT(nearEqF(vp0.h, 720.0f - kToolbarH - kStatusH - kBottomTabH));
}

// REGRESSÃO B1 (o bug do dono no C33): Inspector com receita completa do
// Player (contentHeight 536). Com o visibleHeight INFLADO pela nav bar o
// scroll não ativava (maxOffset 0); com a altura REAL (painel dentro do
// contentRect) o overflow é detetado → scroll ativa → BodyComp/velx/add
// TouchControls alcançáveis.
TEST(safearea_inspector_scroll_ativa_com_nav_bar) {
    // histórico (F4.2): com a receita ANTIGA (536 px) e o painel inflado o
    // scroll morria — maxOffset 0
    const f32 contentOld = 536.0f;
    const f32 listOld = (720.0f - kToolbarH - kStatusH) - kHeaderH - 4.0f;
    EXPECT(nearEqF(listOld, 652.0f));   // PASSO 1: kToolbarH=36, kHeaderH=28
    EXPECT(nearEqF(scroll::maxOffset(contentOld, listOld), 0.0f));   // bug antigo

    // F5.0-fix → 0.9.0: o plano tem SECÇÕES COLAPSÁVEIS de 48dp (spec C) e
    // as 3 linhas de Transform (X/Y/Z + R) — a receita do Player completo
    // sem TouchControls e sem catálogo sobe para 998 (o desenho segue o
    // MESMO plano — fonte única)
    InspProfile prof{};
    prof.tr = true;
    prof.mr = true;
    prof.im = true;
    prof.bc = true;
    prof.tc = false;
    prof.cam = false;
    prof.prim = false;        // 0.8.0: sem primitiva (só a linha "prim:")
    prof.anim = false;
    prof.canAnim = true;      // 0.8.0: tem Transform3D → linha add Animacao
    const TextMetrics m{};
    const f32 contentH = inspectorContentHeight(prof, m, false, 0u);
    // 0.9.2: + secção Script; FASE 9 G2-8: Física em 3 linhas TwoCol.
    // 0.9.6.6 (GRUPO C): as linhas ganharam o PISO dp (eram bloco+px crus)
    // PASSO 1 (0.9.6.14): a tabela da spec (linha 36 · secção 28 ·
    // transform 64) — o plano do Player completo fica em 1046
    EXPECT(nearEqF(contentH, 1046.0f));

    // DEPOIS: painel dentro do contentRect [0,24,·,628] (status 24 + nav 92)
    const Insets in = insetsFromContentRect(1600.0f, 720.0f, 0, 24, 1600, 628);
    const UiRect panel = inspectorPanelRect(1600.0f, 720.0f, in);
    // FASE 9 (G2-10): 476 = 428 + 48; PASSO 1 (0.9.6.14): 536 = 428 + 36
    // (barra 36, tabs de baixo 32, status 0 — o viewport/painel ganham)
    EXPECT(nearEqF(panel.h, 536.0f));
    const f32 listH = panel.h - kHeaderH - 4.0f;
    EXPECT(nearEqF(listH, 504.0f));   // 536 − 28 (cabeçalho) − 4
    const f32 mo = scroll::maxOffset(contentH, listH);
    EXPECT(mo > 0.0f);                        // scroll ATIVA
    EXPECT(nearEqF(mo, contentH - listH));

    // com o offset no máximo, a ÚLTIMA linha do plano (add TouchControls)
    // fica INTEIRA dentro da lista
    InspRow plan[64];   // 0.9.0: plano com secções colapsáveis
    const u32 n = inspectorPlan(prof, m, false, 0u, plan);
    EXPECT(n > 0);
    const InspRow& last = plan[n - 1];
    EXPECT(last.kind == InspRow::Kind::ScriptAdd);   // 0.9.2: add script no fundo
    const f32 contentTop = panel.y + kHeaderH + 4.0f;
    const f32 off = scroll::clampOffset(999.0f, contentH, listH);
    const f32 btnTop = contentTop + last.y - off;
    EXPECT(btnTop >= contentTop);
    EXPECT(btnTop + last.h <= contentTop + listH + 0.01f);   // botão completo
}

// Hierarchy com insets: com >10 TICs todas as linhas continuam alcançáveis
// (e a região encolhida pela nav bar continua a fazer scroll).
// PASSO 1 (0.9.6.14): com linhas de 36dp, 14 TICs (504) JÁ CABEM na região
// de 532 — o cenário passa a 20 TICs (720) para FORÇAR o scroll com insets.
TEST(safearea_hierarchy_com_insets_todos_os_tics) {
    Scene s;
    for (u32 i = 0; i < 20; ++i) {
        s.create("t");
    }
    const f32 contentH = hierarchyContentHeight(20);
    EXPECT(nearEqF(contentH, 720.0f));   // PASSO 1: 20 × 36 (a linha da spec)

    const Insets in = insetsFromContentRect(1600.0f, 720.0f, 0, 24, 1600, 628);
    const UiRect panel = hierarchyPanelRect(1600.0f, 720.0f, in);
    // PASSO 1: cabeçalho 28 + LINHA DE PESQUISA 32 (spec B/PASSO 1)
    const f32 listTop = panel.y + kHeaderH + kSearchRowH;
    const f32 listH = panel.h - kHeaderH - kSearchRowH;
    EXPECT(nearEqF(listH, 476.0f));   // panel 536 (60..596) − 28 − 32

    const f32 off = scroll::clampOffset(999.0f, contentH, listH);
    EXPECT(nearEqF(off, 244.0f));    // 720 − 476
    // última linha (a 20.ª) inteira dentro da região com o offset no máximo
    const f32 row19 = listTop + 19.0f * kRowH - off;
    EXPECT(row19 >= listTop);
    EXPECT(row19 + kRowH <= listTop + listH);
    // tap na última linha resolve o índice certo
    EXPECT(hierarchyRowAtTap(listTop + 19.0f * kRowH - off + 10.0f,
                             listTop, off, 20) == 19);
}
