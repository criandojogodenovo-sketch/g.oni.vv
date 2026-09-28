// tests/test_scroll.cpp — F4.1: primitivo de scroll (ui/ScrollMath.h, GL-free).
// Clamp de offset, drag sem passar do fim, tap-vs-drag, protocolo de claim
// (slider > scroll > botão dentro de região), clip de quads com UV e
// geometria do indicador.
#include "TestFramework.h"
#include "ui/ScrollMath.h"

using namespace vv;
using namespace vv::scroll;
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
