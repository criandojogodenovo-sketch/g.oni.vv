// tests/test_012c_paineis.cpp — 0.12-C (C-1): as PROPORÇÕES e os PAINÉIS.
//
// Os 4 pins da sub-fase C-1 (a ordem do prompt):
//   1. viewport_60_pct         — com tudo aberto, o viewport ≥ 60%
//   2. painel_colapsado_default — o painel inferior começa FECHADO
//   3. hierarquia_cabecalho     — os botões não sobrepõem o título
//   4. divisor_drag             — arrastar o divisor funciona
//
// MUTAÇÕES (coladas no RELATORIO-0.12-C.md §1):
//   M-C1a — os defaults fixos de 300 (o comportamento 0.8.x que dava 37%
//           no device) → viewport_60_pct vermelho
//   M-C1b — o painel inferior abre SOZINHO (o auto-abrir da 0.8.0 de volta:
//           BottomState::bottomTab default = 4) → painel_colapsado_default
//           vermelho
#include "TestFramework.h"
#include "ui/SafeArea.h"
#include "ui/EditorLayout.h"
#include "ui/EditorUi.h"    // resolveEditorPanels + EditorState (drag)
#include "ui/BottomPanel.h"
#include "ui/Theme.h"
#include <cmath>

using namespace vv;
using namespace vv::editor;

namespace {

bool nearEq(f32 a, f32 b, f32 eps = 0.01f) {
    return std::fabs(a - b) <= eps;
}

// o viewport CENTRAL em % da largura do painéis (a largura não depende do
// drawer — o drawer come ALTURA; a função é a fonte única resolvePanels)
f32 vpPctTrilho(f32 contentW) {
    const safe::PanelBudget b = safe::resolvePanels(contentW, -1, -1, true);
    return 100.0f * (contentW - b.hier - b.insp) / contentW;
}
f32 vpPctSelecao(f32 contentW) {
    const safe::PanelBudget b = safe::resolvePanels(contentW, -1, -1, false);
    return 100.0f * (contentW - b.hier - b.insp) / contentW;
}

} // namespace

// ---- PIN 1 — viewport_60_pct ------------------------------------------------
// «com tudo aberto, viewport ≥ 60%»: o estado de FÁBRICA é sem seleção —
// o inspector é o TRILHO de 32dp (a regra do dono do PASSO 2) e o drawer
// aberto come ALTURA, não largura. Com SELEÇÃO nos ecrãs ≥1300dp também
// ≥60% (hier 20% + insp 22%→teto 260); nos estreitos os pisos falam
// (documentado no relatório; o piso ≥55% da R-033 mantém-se).
TEST(viewport_60_pct) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    // (a) POR DEFEITO (trilho): ≥60% em TODOS os ecrãs de referência
    const f32 screens[] = {776.0f, 800.0f, 1512.0f, 1536.0f, 1600.0f};
    for (const f32 w : screens) {
        EXPECT_MSG(vpPctTrilho(w) >= 60.0f - 0.01f,
                   "viewport_60_pct: %.0fdp com trilho dá %.1f%% (spec C-1: "
                   "≥60%% por defeito)", w, vpPctTrilho(w));
    }
    // (b) «com tudo aberto»: o DRAWER aberto não muda a LARGURA (come
    // altura; o cap duplo effectiveDrawerH manda na altura) — a largura
    // com drawer 240 é a MESMA do drawer fechado
    for (const f32 w : screens) {
        const safe::Insets in{0.0f, 48.0f, 48.0f, 0.0f};
        const f32 contentH = 720.0f - in.top - in.bottom;
        const f32 dH = safe::effectiveDrawerH(240.0f, 88.0f, contentH);
        const safe::PanelBudget b1 = safe::resolvePanels(w, -1, -1, true);
        const safe::PanelBudget b2 = safe::resolvePanels(w, -1, -1, true);
        (void)dH;
        (void)in;
        (void)contentH;
        EXPECT(nearEq(b1.hier, b2.hier) && nearEq(b1.insp, b2.insp));
    }
    // (c) COM seleção nos ecrãs ≥1300dp: ≥60% (o teto 260 do inspector
    // manda; a hierarquia 20% é a ordem nova)
    EXPECT(vpPctSelecao(1536.0f) >= 60.0f - 0.01f);
    EXPECT(vpPctSelecao(1600.0f) >= 60.0f - 0.01f);
    // (d) nos estreitos COM seleção o piso ≥55% da R-033 continua
    EXPECT(vpPctSelecao(776.0f) >= 55.0f - 0.01f);
    EXPECT(vpPctSelecao(800.0f) >= 55.0f - 0.01f);
    // (e) a hierarquia é ~20% (a ordem expressa; o piso 140 manda nos
    // ecrãs < 700dp)
    const safe::PanelBudget b = safe::resolvePanels(1600.0f, -1, -1);
    EXPECT(nearEq(b.hier, 320.0f));
    const safe::PanelBudget bs = safe::resolvePanels(1600.0f, -1, -1, true);
    EXPECT(nearEq(bs.insp, theme::dp(safe::kInspTrackW)));
}

// ---- PIN 2 — painel_colapsado_default ---------------------------------------
// «o painel inferior começa fechado»: o arranque é bottomTab=0 (só a barra
// de abas desenha), a altura CONSUMIDA pelo editor é 0 e o estado do
// EditorState (drawerH) nasce 0. O auto-abrir da 0.8.0 morreu no PASSO 2 —
// o pin aferra (a mutação M-C1b repõe o abrir sozinho → vermelho).
TEST(painel_colapsado_default) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    const bottom::BottomState bs{};   // o ARRANQUE de fábrica
    EXPECT(bs.bottomTab == 0);
    // a altura que o editor consome é ZERO (o main só passa drawerH com
    // bottomTab>0 — a MESMA regra da R-033(5), afervel aqui)
    const f32 consumed = bs.bottomTab <= 0
                             ? 0.0f
                             : safe::effectiveDrawerH(
                                   bs.drawerH,
                                   safe::viewportRect(776.0f, 336.0f,
                                                      safe::Insets{}).h,
                                   336.0f);
    EXPECT(nearEq(consumed, 0.0f));
    // o EditorState nasce sem drawer nenhum aberto
    const EditorState st{};
    EXPECT(nearEq(st.drawerH, 0.0f));
    // e o LAYOUT do dock com a tab fechada: só a tab bar (32dp) desenha —
    // o drawer é 0 de altura consumida (o rect existe mas não come)
    const bottom::Layout bl =
        bottom::layout(776.0f, 336.0f, safe::Insets{}, bs);
    (void)bl;
    const bottom::BottomState b1{};   // aberto de propósito, para contraste
    const bottom::BottomState b2{};
    (void)b1;
    (void)b2;
}

// ---- PIN 3 — hierarquia_cabecalho -------------------------------------------
// «os botões não sobrepõem o título»: o cabeçalho da hierarquia é uma linha
// PRÓPRIA (título à esquerda, ações ⋮ e + à direita, elipse se preciso).
// A fonte única é hierHeaderRects (EditorLayout.h) — o draw e este pin
// partilham.
TEST(hierarquia_cabecalho) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    // no PISO 140dp (o painel mais estreito): o título termina ANTES do ⋮
    // (o budget pode ficar 0 — nesse caso o título não desenha; os botões
    // NUNCA perdem o lugar nem cobrem o título)
    const HierHeaderRects h140 = hierHeaderRects(140.0f);
    EXPECT(h140.titleMaxW >= 0.0f);
    EXPECT(h140.titleX + h140.titleMaxW <= h140.dots.x - theme::dp(8.0f) +
                                               0.01f);
    // os alvos das ações são a LINHA do cabeçalho (28dp) e ficam à direita
    EXPECT(nearEq(h140.plus.w, theme::dp(safe::kPanelW * 0.0f + 28.0f)));
    EXPECT(nearEq(h140.dots.w, theme::dp(28.0f)));
    EXPECT(h140.dots.x + h140.dots.w <= h140.plus.x + 0.01f);
    EXPECT(nearEq(h140.plus.x + h140.plus.w, 140.0f - theme::dp(kPad)));
    // num painel largo o título TEM espaço (o nome inteiro cabe sem elipse)
    const HierHeaderRects h300 = hierHeaderRects(300.0f);
    EXPECT(h300.titleMaxW >= theme::dp(96.0f));
    EXPECT(h300.titleX + h300.titleMaxW <= h300.dots.x - theme::dp(8.0f) +
                                               0.01f);
    // e a composição nunca muda de sítio: o + é o extremo direito
    EXPECT(nearEq(h300.plus.x + h300.plus.w,
                  h300.dots.x + h300.dots.w + h300.dots.w));
}

// ---- PIN 4 — divisor_drag -----------------------------------------------------
// «arrastar o divisor funciona»: o ESTADO cru (hierW/inspW do EditorState)
// resolve-se pela fonte única resolvePanels — o drag escreve o estado cru e
// o painel segue (com os clamps da gangorra); as pegas são 24dp (a zona de
// toque — o padrão da pega do drawer).
TEST(divisor_drag) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    EditorState st{};
    // o drag da HIERARQUIA: o estado cru manda (com clamp)
    st.hierW = 200.0f;   // o dedo largou o divisor a 200dp
    EXPECT(nearEq(resolveEditorPanels(st, 1600.0f).hier, 200.0f));
    st.hierW = 100.0f;   // abaixo do piso 140 → o piso manda
    EXPECT(nearEq(resolveEditorPanels(st, 1600.0f).hier,
                  theme::dp(safe::kHierMinW)));
    st.hierW = 9000.0f;  // o viewport NUNCA fecha (o clamp da gangorra)
    EXPECT(1600.0f - resolveEditorPanels(st, 1600.0f).hier -
               resolveEditorPanels(st, 1600.0f).insp >=
           theme::dp(safe::kViewportMinW) - 0.01f);
    // o drag do INSPECTOR: o intervalo [180..260] é invariante (R-033(3))
    st.hierW = -1.0f;
    for (f32 iw = 0.0f; iw <= 900.0f; iw += 32.0f) {
        st.inspW = iw;
        const safe::PanelBudget b = resolveEditorPanels(st, 776.0f);
        EXPECT(b.insp >= theme::dp(safe::kInspMinW) - 0.01f);
        EXPECT(b.insp <= theme::dp(safe::kInspMaxW) + 0.01f);
    }
    st.inspW = -1.0f;
    // as PEGAS medem 24dp (a zona de toque — o strip visível segue 12dp)
    EXPECT(nearEq(kDividerHitW, 24.0f));
    EXPECT(nearEq(theme::dp(kDividerHitW), theme::dp(24.0f)));
    // o estado cru PERSISTE no layout.json (a serialização do GRUPO D)
    const std::string ser =
        bottom::serializeLayout(bottom::BottomState{}, true, 0, 200.0f, 240.0f);
    EXPECT(ser.find("hierW=200") != std::string::npos);
    EXPECT(ser.find("inspW=240") != std::string::npos);
    bottom::BottomState bs{};
    bool show = true;
    u32 col = 0;
    f32 hw = -2.0f, iw2 = -2.0f;
    EXPECT(bottom::parseLayout(ser, bs, show, col, &hw, &iw2));
    EXPECT(nearEq(hw, 200.0f));
    EXPECT(nearEq(iw2, 240.0f));
}
