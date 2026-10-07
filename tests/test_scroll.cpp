// tests/test_scroll.cpp — F4.1: primitivo de scroll (ui/ScrollMath.h, GL-free).
// Clamp de offset, drag sem passar do fim, tap-vs-drag, protocolo de claim
// (slider > scroll > botão dentro de região), clip de quads com UV,
// geometria do indicador e ATINGIBILIDADE do conteúdo dos painéis
// (ui/EditorLayout.h — Inspector com BodyComp/add TouchControls no fundo,
// Hierarchy com >10 TICs).
#include "TestFramework.h"
#include "ui/ScrollMath.h"
#include "ui/EditorLayout.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "components/Transform3D.h"
#include "components/MeshRenderer.h"
#include "components/InputMap.h"
#include "components/BodyComp.h"
#include "components/TouchControls.h"

using namespace vv;
using namespace vv::scroll;
using namespace vv::editor;
using ::test::nearEqF;

TEST(scroll_maxoffset_e_clamp) {
    // máximo: conteúdo mais alto → diferença; conteúdo menor → 0
    EXPECT(nearEqF(maxOffset(500.0f, 300.0f), 200.0f));
    EXPECT(nearEqF(maxOffset(200.0f, 300.0f), 0.0f));
    EXPECT(nearEqF(maxOffset(300.0f, 300.0f), 0.0f));

    // clamp pelos dois extremos e dentro
    EXPECT(nearEqF(clampOffset(-5.0f, 500.0f, 300.0f), 0.0f));
    EXPECT(nearEqF(clampOffset(999.0f, 500.0f, 300.0f), 200.0f));
    EXPECT(nearEqF(clampOffset(50.0f, 500.0f, 300.0f), 50.0f));

    // conteúdo não faz scroll → offset sempre 0 (sanear stale)
    EXPECT(nearEqF(clampOffset(120.0f, 200.0f, 300.0f), 0.0f));
}

TEST(scroll_drag_nao_passa_do_fim) {
    State s;
    s.offset = 0.0f;
    beginDrag(s, 100.0f, 400.0f);   // região: arrastar para CIMA aumenta o offset

    // arrasto normal: 150 px para cima → offset 150 (conteúdo 500, visível 300)
    dragTo(s, 100.0f, 250.0f, 500.0f, 300.0f);
    EXPECT(nearEqF(s.offset, 150.0f));

    // arrasto MUITO além do fim → clamp no máximo (nunca passa)
    dragTo(s, 100.0f, -1000.0f, 500.0f, 300.0f);
    EXPECT(nearEqF(s.offset, 200.0f));
    EXPECT(nearEqF(s.offset, maxOffset(500.0f, 300.0f)));

    // arrasto para baixo além do topo → clamp a 0
    dragTo(s, 100.0f, 2000.0f, 500.0f, 300.0f);
    EXPECT(nearEqF(s.offset, 0.0f));

    // arrasto horizontal não muda o offset (mas conta para tap-vs-drag)
    beginDrag(s, 100.0f, 400.0f);
    s.offset = 80.0f;   // simula offset prévio capturado no beginDrag
    s.startOffset = 80.0f;
    dragTo(s, 400.0f, 400.0f, 500.0f, 300.0f);
    EXPECT(nearEqF(s.offset, 80.0f));
    EXPECT(s.maxMove >= kTapPx);   // movimento horizontal grande = drag
}

TEST(scroll_tap_vs_drag_limiar) {
    State s;
    beginDrag(s, 50.0f, 500.0f);

    // micro-movimento (< kTapPx) continua a ser tap
    dragTo(s, 52.0f, 496.0f, 800.0f, 400.0f);
    EXPECT(isTap(s));
    EXPECT(endDrag(s));

    // movimento vertical ≥ kTapPx é drag de scroll (sem tap no release)
    beginDrag(s, 50.0f, 500.0f);
    dragTo(s, 50.0f, 500.0f - kTapPx - 1.0f, 800.0f, 400.0f);
    EXPECT(!isTap(s));
    EXPECT(!endDrag(s));   // termina sem re-despachar tap

    // movimento horizontal ≥ kTapPx também anula o tap
    beginDrag(s, 50.0f, 500.0f);
    dragTo(s, 50.0f + kTapPx + 2.0f, 499.0f, 800.0f, 400.0f);
    EXPECT(!isTap(s));

    // release sem dragTo → maxMove 0 → tap
    beginDrag(s, 10.0f, 10.0f);
    EXPECT(isTap(s));
}

TEST(scroll_protocolo_de_claim_widgets) {
    // press edge dentro da região + nenhum widget reclamou → scroll reclama
    EXPECT(scrollClaims(true, false));
    // slider reclamou (prioridade da spec) → scroll NÃO reclama
    EXPECT(!scrollClaims(true, true));
    // press fora da região → nunca
    EXPECT(!scrollClaims(false, false));
    EXPECT(!scrollClaims(false, true));

    // botão DENTRO de região: só desenha (tap re-despachado pelo painel);
    // FORA: captura como sempre (toolbar, overlays, "+" do cabeçalho)
    EXPECT(!buttonCaptures(true));
    EXPECT(buttonCaptures(false));

    // slider captura sempre, dentro ou fora
    EXPECT(sliderCaptures(true));
    EXPECT(sliderCaptures(false));
}

TEST(scroll_clipquad_corta_com_uv) {
    const UiRect clip = {100.0f, 100.0f, 200.0f, 200.0f};   // 100..300 × 100..300
    Clipped out;

    // totalmente dentro → igual, UV intacto
    EXPECT(clipQuad(120.0f, 120.0f, 40.0f, 40.0f, 0.0f, 0.0f, 1.0f, 1.0f, clip, out));
    EXPECT(nearEqF(out.x, 120.0f) && nearEqF(out.y, 120.0f));
    EXPECT(nearEqF(out.w, 40.0f) && nearEqF(out.h, 40.0f));
    EXPECT(nearEqF(out.u0, 0.0f) && nearEqF(out.v1, 1.0f));

    // saída pelo topo: 30 px cortados → v0 interpola 30/60
    EXPECT(clipQuad(150.0f, 70.0f, 20.0f, 60.0f, 0.25f, 0.0f, 0.75f, 1.0f, clip, out));
    EXPECT(nearEqF(out.y, 100.0f) && nearEqF(out.h, 30.0f));
    EXPECT(nearEqF(out.v0, 30.0f / 60.0f));
    EXPECT(nearEqF(out.v1, 1.0f));

    // totalmente fora → rejeitado (não entra no batch)
    EXPECT(!clipQuad(0.0f, 0.0f, 50.0f, 50.0f, 0.0f, 0.0f, 1.0f, 1.0f, clip, out));
    EXPECT(!clipQuad(350.0f, 350.0f, 50.0f, 50.0f, 0.0f, 0.0f, 1.0f, 1.0f, clip, out));
    EXPECT(!clipQuad(150.0f, 150.0f, 0.0f, 50.0f, 0.0f, 0.0f, 1.0f, 1.0f, clip, out));

    // parcial pela direita e fundo em simultâneo
    EXPECT(clipQuad(250.0f, 250.0f, 100.0f, 100.0f, 0.0f, 0.0f, 1.0f, 1.0f, clip, out));
    EXPECT(nearEqF(out.w, 50.0f) && nearEqF(out.h, 50.0f));
    EXPECT(nearEqF(out.u1, 0.5f) && nearEqF(out.v1, 0.5f));
}

TEST(scroll_indicador_geometria) {
    const UiRect region = {0.0f, 100.0f, 300.0f, 400.0f};   // conteúdo 800
    f32 bx, by, bw, bh;

    // polegar: 400*400/800 = 200 px
    indicator(region, 800.0f, 0.0f, bx, by, bw, bh);
    EXPECT(nearEqF(bh, 200.0f));
    EXPECT(nearEqF(bw, 3.0f));
    EXPECT(nearEqF(by, 100.0f));                       // topo com offset 0
    EXPECT(nearEqF(bx, region.x + region.w - 4.0f));   // encostado à direita

    indicator(region, 800.0f, 400.0f, bx, by, bw, bh);  // offset máximo
    EXPECT(nearEqF(by, 300.0f));                       // fundo da região

    indicator(region, 800.0f, 200.0f, bx, by, bw, bh);  // meio
    EXPECT(nearEqF(by, 200.0f));

    // conteúdo enorme → polegar no mínimo 24 px
    indicator(region, 10000.0f, 0.0f, bx, by, bw, bh);
    EXPECT(nearEqF(bh, 24.0f));

    // sem overflow → t=0 (o chamador nem desenha: maxOffset == 0)
    indicator(region, 300.0f, 0.0f, bx, by, bw, bh);
    EXPECT(nearEqF(by, region.y));
}

// ---------------------------------------------------------------------------
// ATINGIBILIDADE (o bloqueio do dono no C33): o fundo do Inspector e as
// linhas >10 da Hierarchy têm de ficar alcançáveis com o scroll no máximo.
// ---------------------------------------------------------------------------

TEST(scroll_inspector_conteudo_e_botao_fundo_atingivel) {
    Scene s;
    const Handle h = createTicFromPreset(s, PresetKind::PlayerBody3D, nullptr, nullptr);
    Tic* tic = s.get(h);
    EXPECT(tic->getComponent<Transform3D>() != nullptr);
    EXPECT(tic->getComponent<MeshRenderer>() != nullptr);
    EXPECT(tic->getComponent<InputMap>() != nullptr);
    EXPECT(tic->getComponent<BodyComp>() != nullptr);

    // F5.0-fix: o conteúdo vem do PLANO (fonte única) com as métricas
    // fallback = sans 28 px (o caso do device). Receita completa do Player
    // (sem catálogo → mesh/tex são LABELS de 34): nome 34 + VISIVEL 36
    // (0.7.0) + transform (34 + 9×36) + mesh 34 + tex 34 + COR R/G/B 3×36
    // (0.7.0) + input 34 + body 34 + velx 36 + add TouchControls 42 = 750
    const TextMetrics m{};
    const InspProfile prof = inspectorProfile(*tic);
    InspRow plan[48];   // 0.8.0: +prim/anim
    const u32 n = inspectorPlan(prof, m, false, 0u, plan);
    const f32 contentH = inspectorContentHeight(prof, m, false, 0u);
    // 0.9.0 (spec C): secções + 3 linhas de Transform + miniaturas → 1070;
    // 0.9.2: +90 do Script; FASE 9 G2-8: Física 1 Label → 3 TwoCol
    // 0.9.6.6 (GRUPO C): o PISO dp em TODAS as linhas — 1420
    // PASSO 1 (0.9.6.14): a tabela da spec (linha 36 · secção 28 ·
    // transform 64: título 24 + caixas 32) baixa a receita para 1046
    EXPECT(nearEqF(contentH, 1046.0f));

    // cursor Y PARTILHADO: linhas sequenciais (y estritamente crescente, sem
    // reinício por secção), todas dentro do conteúdo, e o fundo do plano =
    // contentHeight (a soma REAL das alturas)
    f32 prevTop = -1.0f;
    f32 prevBottom = 0.0f;
    u32 addTcIdx = n;
    for (u32 i = 0; i < n; ++i) {
        EXPECT(plan[i].y >= prevBottom);   // começa onde a anterior acabou
        EXPECT(plan[i].y > prevTop);       // Ys DISTINTOS (nada desenha em cima)
        EXPECT(plan[i].y + plan[i].h <= contentH + 0.01f);
        prevTop = plan[i].y;
        prevBottom = plan[i].y + plan[i].h;
        if (plan[i].kind == InspRow::Kind::AddTc) addTcIdx = i;
    }
    // 0.9.2: depois do addTc vêm Anim(48)+AddAnim(42)+Script(48)+AddScript(42)
    EXPECT(addTcIdx + 5 == n);
    EXPECT(plan[n - 1].kind == InspRow::Kind::ScriptAdd);
    EXPECT(nearEqF(plan[n - 1].y + plan[n - 1].h, contentH));

    // C33 (pior caso: superfície mais baixa que a teórica) — lista 500 px:
    // sem scroll o fundo do botão fica FORA da região (o bug reportado)
    const f32 listH = 500.0f;
    const f32 contentTop = 120.0f;   // PASSO 1: y=88 + cabeçalho 28 + 4
    const f32 btnBottom0 = contentTop + plan[n - 1].y + plan[n - 1].h;
    EXPECT(btnBottom0 > contentTop + listH);

    // com o offset no MÁXIMO o botão fica inteiro dentro da região —
    // "add Animacao" (última linha, 0.8.0) clicável mesmo após scroll
    const f32 off = clampOffset(999.0f, contentH, listH);
    EXPECT(nearEqF(off, contentH - listH));
    const f32 btnTop = contentTop + plan[n - 1].y - off;
    EXPECT(btnTop >= contentTop);
    EXPECT(btnTop + plan[n - 1].h <= contentTop + listH + 0.01f);

    // com TouchControls presente o botão dá lugar à label tc
    EXPECT(tic->addComponent<TouchControls>() != nullptr);
    // 0.9.6.6 (GRUPO C): o PISO dp iguala addTc e label tc — o plano com
    // TouchControls fica IGUAL ao sem (PASSO 1: 1046): a troca não encolhe
    // PASSO 1: com TouchControls o addTc (42) dá lugar à label tc (36) —
    // 1046 − 42 + 36 = 1040
    EXPECT(nearEqF(inspectorContentHeight(inspectorProfile(*tic), m, false, 0u),
                   1040.0f));
}

TEST(scroll_hierarquia_todos_os_tics_atingeis) {
    Scene s;
    for (u32 i = 0; i < 14; ++i) {
        s.create("t");
    }
    EXPECT(s.count() == 14u);

    const f32 contentH = hierarchyContentHeight(14);
    EXPECT(nearEqF(contentH, 504.0f));   // PASSO 1: 14 × 36 (a linha da spec)

    const f32 listTop = 164.0f;   // PASSO 1: y=104 + cabeçalho 28 + pesquisa 32
    const f32 listH = 400.0f;     // teórico (scroll math pura) — FORÇA o
                                  // scroll: 14×36 = 504 caberia em 540 sem ele

    // sem scroll a linha 13 (a 14.ª) fica cortada — o comportamento antigo
    const f32 row13_0 = listTop + 13.0f * kRowH;
    EXPECT(row13_0 + kRowH > listTop + listH);

    // com o scroll no máximo TODAS as linhas cabem na região, incluindo a 13
    const f32 off = clampOffset(999.0f, contentH, listH);
    EXPECT(nearEqF(off, 104.0f));   // 504 − 400
    const f32 row13 = listTop + 13.0f * kRowH - off;
    EXPECT(row13 >= listTop);
    EXPECT(row13 + kRowH <= listTop + listH);
}

TEST(scroll_hierarquia_row_sob_tap) {
    const f32 listTop = 164.0f;   // PASSO 1: +32 da linha de pesquisa
    const f32 off = 104.0f;   // máximo do teste anterior (14 TICs × 36)

    // tap no meio da linha 13 com o offset no máximo → índice 13
    EXPECT(hierarchyRowAtTap(listTop + 13.0f * kRowH - off + 10.0f,
                             listTop, off, 14) == 13);
    // topo da lista → linha 0; acima da lista e além do fim → -1
    EXPECT(hierarchyRowAtTap(listTop + 5.0f, listTop, 0.0f, 14) == 0);
    EXPECT(hierarchyRowAtTap(listTop - 1.0f, listTop, 0.0f, 14) == -1);
    EXPECT(hierarchyRowAtTap(listTop + 15.0f * kRowH, listTop, 0.0f, 14) == -1);
    // lista vazia → sempre -1
    EXPECT(hierarchyRowAtTap(listTop + 10.0f, listTop, 0.0f, 0) == -1);
}

// ---------------------------------------------------------------------------
// F5-E: as linhas de mesh/tex do Inspector (seletores de assets) ficam em
// coords de conteúdo conhecidas — atingíveis com scroll, como o botão do
// fundo. O PLANO é a fonte única usada pelo desenho E pelo hit-test do tap.
// ---------------------------------------------------------------------------

TEST(scroll_linhas_mesh_tex_atingiveis_no_scroll) {
    Scene s;
    const Handle h = createTicFromPreset(s, PresetKind::PlayerBody3D, nullptr, nullptr);
    Tic* tic = s.get(h);

    const TextMetrics m{};
    const InspProfile prof = inspectorProfile(*tic);
    InspRow plan[48];   // 0.8.0: +prim/anim
    const u32 n = inspectorPlan(prof, m, true, 0u, plan);   // seletores ativos
    u32 meshIdx = n, texIdx = n, primIdx = n;
    for (u32 i = 0; i < n; ++i) {
        if (plan[i].kind == InspRow::Kind::MeshButton) meshIdx = i;
        if (plan[i].kind == InspRow::Kind::TexButton) texIdx = i;
        if (plan[i].kind == InspRow::Kind::PrimButton) primIdx = i;   // 0.8.0
    }
    EXPECT(meshIdx < n);
    // 0.8.0: mesh → prim → tex (a linha "prim:" fica no meio — F7)
    EXPECT(primIdx == meshIdx + 1);
    EXPECT(texIdx == primIdx + 1);

    // topo da linha mesh — 0.9.6.6 (GRUPO C): 48+48+48+3×80+48 = 432
    // PASSO 1 (0.9.6.14): o plano (fonte única) põe a mesh em 320 — as
    // linhas da spec (36 · secção 28 · transform 64)
    EXPECT(nearEqF(plan[meshIdx].y, 320.0f));
    // 0.8.0: a linha "prim:" fica entre mesh e tex (a ALTURA dela —
    // simbólico desde 0.9.6.6: zero números que driftam com o piso 48dp)
    EXPECT(nearEqF(plan[texIdx].y, plan[meshIdx].y + plan[meshIdx].h +
                                    plan[primIdx].h));

    // pior caso C33 (lista 500 px): conteúdo 754 — scroll ativa
    const f32 contentH = inspectorContentHeight(prof, m, true, 0u);
    const f32 listH = 500.0f;
    EXPECT(contentH > listH);
    const f32 off = clampOffset(999.0f, contentH, listH);
    EXPECT(nearEqF(off, contentH - listH));

    // 0.9.0: o plano é mais ALTO (secções) — com o offset no MÁXIMO o mesh
    // (perto do topo) rola para FORA (comportamento correto); com o offset
    // ALINHADO nele, a linha fica inteira na região (atingível)
    const f32 contentTop = 140.0f;
    const f32 offMesh = clampOffset(plan[meshIdx].y, contentH, listH);
    const f32 meshScr = contentTop + plan[meshIdx].y - offMesh;
    EXPECT(meshScr >= contentTop);
    EXPECT(meshScr + plan[meshIdx].h <= contentTop + listH + 0.01f);
}
