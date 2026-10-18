// tests/test_012c_chrome.cpp — 0.12-C (C-2): MENUS, sobreposições, barra
// de topo.
//
// Os 4 pins da sub-fase C-2 (a ordem do prompt):
//   1. menu_scroll    — o menu com 15 itens TEM scroll (nunca cortado)
//   2. menu_ancorado  — o popover SAI do botão que o abriu
//   3. topbar_3_zonas — as 3 zonas da barra não se sobrepõem
//   4. mover_unico    — só há UM botão Mover no viewport
//
// MUTAÇÕES (coladas no RELATORIO-0.12-C.md §2):
//   M-C2a — o menu CORTADO (a altura máxima morta: h = contentH sempre)
//           → menu_scroll vermelho
//   M-C2b — o segundo Mover volta (o gizmo 40dp do canto reposto)
//           → mover_unico vermelho
#include "TestFramework.h"
#include "ui/EditorLayout.h"
#include "ui/SafeArea.h"
#include "ui/Toolbar.h"
#include "ui/ViewportChrome.h"
#include "ui/Theme.h"
#include <cmath>

using namespace vv;
using namespace vv::editor;

namespace {

bool nearEq(f32 a, f32 b, f32 eps = 0.01f) {
    return std::fabs(a - b) <= eps;
}

// o contentH REAL do menu principal (o MESMO cálculo do drawFileMenu:
// 15 itens × 48dp + 6 cabeçalhos × 28dp + 8dp de respiro)
f32 menuContentH() {
    const f32 rowH = theme::dp(48.0f);
    const f32 hdrH = theme::dp(28.0f);
    return 15.0f * rowH + 6.0f * hdrH + theme::dp(8.0f);
}

} // namespace

// ---- PIN 1 — menu_scroll -----------------------------------------------------
// «menu com 15 itens tem scroll»: num ecrã de referência (720dp) o sheet
// NUNCA passa a altura máxima disponível (o resto rola — o D14) e o
// maxOffset é positivo; num ecrã alto o menu cabe INTEIRO (sem scroll).
TEST(menu_scroll) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    const f32 contentH = menuContentH();
    // 15×48 + 6×28 + 8 = 896dp de conteúdo
    EXPECT(nearEq(contentH, 896.0f));
    // o ecrã de referência 1600×720 (sem insets): maxH = 720−36−8−32 = 644
    {
        const MenuSheetLayout m =
            menuSheetLayout(1600.0f, 720.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                            contentH, 100.0f, 40.0f, theme::dp(280.0f));
        EXPECT(m.h < contentH - 0.01f);          // o sheet é MENOR que o conteúdo
        EXPECT(m.h <= 720.0f - 36.0f - 8.0f - 32.0f + 0.01f);   // nunca sai
        EXPECT(m.maxOffset > 0.0f);              // há O QUE rolar
        EXPECT(nearEq(m.maxOffset, contentH - m.h));
        // o fundo do sheet nunca cruza a tab bar (o D14)
        EXPECT(m.y + m.h <= 720.0f - 32.0f + 0.01f);
    }
    // num ecrã ALTO o menu cabe inteiro (sem scroll)
    {
        const MenuSheetLayout m =
            menuSheetLayout(1600.0f, 1400.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                            contentH, 100.0f, 40.0f, theme::dp(280.0f));
        EXPECT(nearEq(m.h, contentH));
        EXPECT(nearEq(m.maxOffset, 0.0f));
    }
    // e num ecrã RETRATO estreito (o device do dono a 336dp de altura)
    {
        const MenuSheetLayout m =
            menuSheetLayout(776.0f, 336.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                            contentH, 100.0f, 40.0f, theme::dp(280.0f));
        EXPECT(m.h < contentH - 0.01f);
        EXPECT(m.maxOffset > 0.0f);
        EXPECT(m.y + m.h <= 336.0f - 32.0f + 0.01f);
    }
}

// ---- PIN 2 — menu_ancorado ----------------------------------------------------
// «o popover sai do botão que o abriu»: com a âncora (ax/ay do botão) o
// sheet nasce 8dp SOB o botão e com clamp honesto; sem âncora é centrado
// (o compat dos testes). O CENAS e o ⋮ da hierarquia partilham o contrato
// (o MESMO padrão ax/ay — ver drawScenesMenu/drawHierMenu).
TEST(menu_ancorado) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    const f32 contentH = menuContentH();
    // (a) o sheet SAI DE BAIXO do botão: y = ay + 8dp, x = ax (com o
    // conteúdo 896dp o h = 644 e o clamp do fundo é y ≤ 44 — uma âncora
    // de 20dp fica SEM clamp: y = 28)
    {
        const MenuSheetLayout m =
            menuSheetLayout(1600.0f, 720.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                            contentH, 100.0f, 20.0f, theme::dp(280.0f));
        EXPECT(nearEq(m.x, 100.0f));
        EXPECT(nearEq(m.y, 28.0f));   // 20 + 8 (o clamp não ativou)
    }
    // (a2) uma âncora BAIXA ativa o clamp vertical (o fundo nunca passa
    // o topo da tab bar — o D14)
    {
        const MenuSheetLayout m =
            menuSheetLayout(1600.0f, 720.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                            contentH, 100.0f, 600.0f, theme::dp(280.0f));
        EXPECT(nearEq(m.y, 720.0f - 644.0f - 32.0f));
    }
    // (b) o clamp horizontal: uma âncora à direita extrema NÃO deixa o
    // sheet sair do ecrã
    {
        const MenuSheetLayout m =
            menuSheetLayout(1600.0f, 720.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                            contentH, 1580.0f, 40.0f, theme::dp(280.0f));
        EXPECT(m.x + theme::dp(280.0f) <= 1600.0f + 0.01f);
    }
    // (c) o clamp vertical: uma âncora perto do fundo NÃO cruza a tab bar
    {
        const MenuSheetLayout m =
            menuSheetLayout(1600.0f, 720.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                            contentH, 100.0f, 600.0f, theme::dp(280.0f));
        EXPECT(m.y + m.h <= 720.0f - 32.0f + 0.01f);
        EXPECT(m.y >= 36.0f - 0.01f);   // nunca por cima da barra de topo
    }
    // (d) sem âncora (−1): centrado na área útil (o compat)
    {
        const MenuSheetLayout m =
            menuSheetLayout(1600.0f, 720.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                            contentH, -1.0f, -1.0f, theme::dp(280.0f));
        EXPECT(m.x > 0.0f && m.x + theme::dp(280.0f) < 1600.0f);
        EXPECT(m.y > 0.0f);
    }
}

// ---- PIN 3 — topbar_3_zonas ----------------------------------------------------
// «as 3 zonas não se sobrepõem»: ESQUERDA (logo+nome+Menu+Cena) · CENTRO
// (as tabs 3D/UI/Áudio) · DIREITA (play/pause/stop/Android/gear) — nos
// ecrãs de referência e na degradação (o nome e a plataforma SAEM antes
// de as zonas tocarem).
TEST(topbar_3_zonas) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    const safe::Insets in{};
    struct Scr { f32 w; f32 h; };
    const Scr screens[] = {{1600.0f, 720.0f}, {1536.0f, 720.0f},
                           {776.0f, 336.0f},  {800.0f, 336.0f}};
    for (const Scr& s : screens) {
        const toolbar::TopBarLayout L = toolbar::topbarLayout(s.w, s.h, in, false, false);
        // (a) a zona da ESQUERDA termina antes da do CENTRO
        const f32 leftEnd = L.cena.x + L.cena.w;
        const f32 centerStart = L.tab3d.x;
        EXPECT_MSG(leftEnd <= centerStart + 0.01f,
                   "topbar_3_zonas %.0fdp: a esquerda (%.0f) cruza o centro "
                   "(%.0f)", s.w, leftEnd, centerStart);
        // (b) o CENTRO termina antes da DIREITA
        const f32 centerEnd = L.tabAudio.x + L.tabAudio.w;
        const f32 rightStart = L.play.x;
        EXPECT_MSG(centerEnd <= rightStart + 0.01f,
                   "topbar_3_zonas %.0fdp: o centro (%.0f) cruza a direita "
                   "(%.0f)", s.w, centerEnd, rightStart);
        // (c) a DIREITA fica ancorada à borda: o gear é o extremo direito
        EXPECT(L.gear.x + L.gear.w <= L.bar.x + L.bar.w + 0.01f);
        // (d) as 3 tabs do centro têm a MESMA largura e são contíguas
        EXPECT(nearEq(L.tab3d.w, L.tabUi.w));
        EXPECT(nearEq(L.tabUi.x, L.tab3d.x + L.tab3d.w));
        EXPECT(nearEq(L.tabAudio.x, L.tabUi.x + L.tabUi.w));
    }
    // (e) a degradação honesta: num ecrã apertado o NOME e o CHIP saem —
    // as zonas continuam SEM tocar
    {
        const toolbar::TopBarLayout L = toolbar::topbarLayout(480.0f, 320.0f, in, false, false);
        EXPECT(!L.showName);
        const f32 leftEnd = L.cena.x + L.cena.w;
        EXPECT(leftEnd <= L.tab3d.x + 0.01f);
        EXPECT(L.tabAudio.x + L.tabAudio.w <= L.play.x + 0.01f);
    }
}

// ---- PIN 4 — mover_unico --------------------------------------------------------
// «só há um botão Mover»: o rail tem as 5 ferramentas (Selecionar/Mover/
// Rodar/Escalar/Íman) e o GIZMO 40dp do canto (que mostrava o MESMO
// ícone da ferramenta ativa) SAIU — a cláusula anti-superficialidade da
// campanha. A mutação M-C2b repõe o botão → este pin fica vermelho.
TEST(mover_unico) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    // o rect do device real: 456×244dp (o inspector aberto, 776 de content)
    const UiRect view{160.0f, 36.0f, 456.0f, 244.0f};
    const vpchrome::Layout L = vpchrome::layout(view);
    // (a) o rail completo existe (as 5 ferramentas — o Mover É o rail[1])
    EXPECT(L.railVisible);
    EXPECT(nearEq(L.rail[1].w, theme::dp(vpchrome::kRailBtn)));
    EXPECT(L.rail[1].x > L.rail[0].x || L.rail[1].y > L.rail[0].y);
    // (b) o GIZMO do canto NÃO EXISTE mais (os rects ZERO)
    EXPECT(L.gizmoBtn.w == 0.0f && L.gizmoBtn.h == 0.0f);
    EXPECT(L.gizmoPanel.w == 0.0f);
    // (c) a fila do topo (undo/redo/save/⋯) existe e NUNCA sai do rect
    EXPECT(L.quick[0].w > 0.0f);
    for (int i = 0; i < 4; ++i) {
        EXPECT(safe::rectInside(L.quick[i], view));
    }
    // (d) as barras flutuantes ficam DENTRO do rect (nada transborda)
    EXPECT(safe::rectInside(L.railPanel, view));
    EXPECT(safe::rectInside(L.quickPanel, view));
    // (e) num viewport SUB-mínimo o canto fica TOTALMENTE limpo
    {
        const vpchrome::Layout lt =
            vpchrome::layout(UiRect{160.0f, 36.0f, 140.0f, 208.0f});
        EXPECT(lt.gizmoBtn.w == 0.0f);
    }
}
