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

    // receita completa do Player: nome 30 + transform 350 + mesh 26 + tex 26
    // (F5-E) + input 26 + body 62 + add TouchControls 42 = 562
    const f32 contentH = inspectorContentHeight(*tic);
    EXPECT(nearEqF(contentH, 562.0f));

    // C33 (pior caso: superfície mais baixa que a teórica) — lista 500 px:
    // sem scroll o fundo do botão fica FORA da região (o bug reportado)
    const f32 listH = 500.0f;
    const f32 contentTop = 140.0f;   // y=88 + cabeçalho 48 + 4
    const f32 btnBottom0 = contentTop + inspectorAddTcTop(contentH) + 34.0f;
    EXPECT(btnBottom0 > contentTop + listH);

    // com o offset no MÁXIMO o botão fica inteiro dentro da região —
    // "add TouchControls" clicável mesmo após scroll
    const f32 off = clampOffset(999.0f, contentH, listH);
    EXPECT(nearEqF(off, contentH - listH));
    const f32 btnTop = contentTop + inspectorAddTcTop(contentH) - off;
    EXPECT(btnTop >= contentTop);
    EXPECT(btnTop + 34.0f <= contentTop + listH);

    // com TouchControls presente o botão dá lugar à label (42 → 26)
    EXPECT(tic->addComponent<TouchControls>() != nullptr);
    EXPECT(nearEqF(inspectorContentHeight(*tic), 546.0f));
}

TEST(scroll_hierarquia_todos_os_tics_atingeis) {
    Scene s;
    for (u32 i = 0; i < 14; ++i) {
        s.create("t");
    }
    EXPECT(s.count() == 14u);

    const f32 contentH = hierarchyContentHeight(14);
    EXPECT(nearEqF(contentH, 728.0f));   // 14 × 52

    const f32 listTop = 136.0f;   // y=88 + cabeçalho 48
    const f32 listH = 540.0f;     // painel 592 − cabeçalho (C33 teórico)

    // sem scroll a linha 13 (a 14.ª) fica cortada — o comportamento antigo
    const f32 row13_0 = listTop + 13.0f * kRowH;
    EXPECT(row13_0 + kRowH > listTop + listH);

    // com o scroll no máximo TODAS as linhas cabem na região, incluindo a 13
    const f32 off = clampOffset(999.0f, contentH, listH);
    EXPECT(nearEqF(off, 188.0f));
    const f32 row13 = listTop + 13.0f * kRowH - off;
    EXPECT(row13 >= listTop);
    EXPECT(row13 + kRowH <= listTop + listH);
}

TEST(scroll_hierarquia_row_sob_tap) {
    const f32 listTop = 136.0f;
    const f32 off = 188.0f;   // máximo do teste anterior (14 TICs)

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
// fundo. inspectorMeshTop é a fonte única usada pelo hit-test do tap.
// ---------------------------------------------------------------------------

TEST(scroll_linhas_mesh_tex_atingiveis_no_scroll) {
    Scene s;
    const Handle h = createTicFromPreset(s, PresetKind::PlayerBody3D, nullptr, nullptr);
    Tic* tic = s.get(h);

    // topo da linha mesh (Player tem Transform3D): nome + transform
    const f32 meshTop = inspectorMeshTop(*tic);
    EXPECT(nearEqF(meshTop, 30.0f + 26.0f + 9.0f * 36.0f + 2.0f));
    const f32 texTop = meshTop + 26.0f;
    EXPECT(nearEqF(texTop + 26.0f, meshTop + 52.0f));   // blocos adjacentes

    // pior caso C33 (lista 500 px): conteúdo 562 — scroll ativa
    const f32 contentH = inspectorContentHeight(*tic);
    const f32 listH = 500.0f;
    EXPECT(contentH > listH);
    const f32 off = clampOffset(999.0f, contentH, listH);
    EXPECT(nearEqF(off, contentH - listH));

    // com o offset no máximo, o topo da linha mesh continua dentro da lista
    const f32 contentTop = 140.0f;
    const f32 meshScr = contentTop + meshTop - off;
    EXPECT(meshScr >= contentTop);
    EXPECT(meshScr + 26.0f <= contentTop + listH);
}
