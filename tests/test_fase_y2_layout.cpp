// tests/test_fase_y2_layout.cpp — os pins da Y-2 (0.11-Y: o LAYOUT —
// zero overflow em todo o ecrã e NADA a cruzar a tab bar de baixo).
//
// O contrato do prompt da Y-2 (o critério C33 ③ do dono): «nenhum ecrã
// com overflow; nada atravessa a tab bar». A FERRAMENTA é o validador
// puro do LayoutDump (o mesmo caminho do botão Auditoria do device) com
// as DUAS REGRAS NOVAS:
//   • CruzaTabBar — uma entrada ou vive FORA da faixa da tab bar de
//     baixo (a última faixa do contentRect, safe::kBottomTabH) ou É DELA
//     (as tabs/chips contidos); ATRAVESSAR a borda superior é o defeito.
//   • PainelFora — painel/moldura não-interativo que sangra o
//     contentRect; o VÉU que contém o contentRect inteiro é intencional.
// A VARREDURA dos ecrãs REAIS corre na FASE 31 do device virtual (o
// harness desenha cada ecrã pelo frame() de produção e valida o
// registo); AQUI a matemática é aferrada com Records plantados à mão.
//
// As MUTAÇÕES do relatório (M-Y2a: o check CruzaTabBar morto; M-Y2b: o
// PainelFora morto) correm contra o CÓDIGO compilado (rebuild + corrida
// + reverso) — o padrão M-Y1d.
#include "TestFramework.h"

#include "ui/LayoutDump.h"
#include "ui/SafeArea.h"
#include "ui/Theme.h"

using namespace vv;

namespace {

// o ecrã de referência do device (o C33: 1536×720, insets 24 topo/fundo)
constexpr f32 kSW = 1536.0f, kSH = 720.0f;
constexpr f32 kInset = 24.0f;

layout::Record baseRecord() {
    layout::Record r;
    r.screen = "pin";
    r.screenW = kSW;
    r.screenH = kSH;
    r.insetT = kInset;
    r.insetB = kInset;
    r.insetL = 0.0f;
    r.insetR = 0.0f;
    r.density = 1.0f;
    return r;
}

layout::Entry panel(f32 x, f32 y, f32 w, f32 h, bool clip = false) {
    layout::Entry e;
    e.kind = layout::Entry::Panel;
    e.x = x;
    e.y = y;
    e.w = w;
    e.h = h;
    e.clipped = clip;
    return e;
}

layout::Entry botao(f32 x, f32 y, f32 w, f32 h) {
    layout::Entry e;
    e.kind = layout::Entry::Button;
    e.id = 0x9252ull;
    e.x = x;
    e.y = y;
    e.w = w;
    e.h = h;
    return e;
}

u32 countRule(const std::vector<layout::Problem>& ps, layout::Problem::Rule r) {
    u32 n = 0;
    for (const auto& p : ps) {
        if (p.rule == r) ++n;
    }
    return n;
}

} // namespace

// ---- PIN 1 — a regra CRUZA_TAB_BAR (a faixa OU dela OU intocado) ----------
TEST(y2_validador_tab_bar) {
    // a faixa: contentRect y ∈ [24 .. 696]; a tab bar é [696-32 .. 696]
    const f32 bandY0 = kSH - kInset - safe::kBottomTabH;   // 664

    // (a) o DEFEITO: um painel que atravessa a borda superior da faixa
    {
        layout::Record r = baseRecord();
        r.add(panel(100.0f, bandY0 - 10.0f, 300.0f, 24.0f));   // 654..678
        const auto ps = layout::validate(r);
        EXPECT_MSG(countRule(ps, layout::Problem::CruzaTabBar) == 1,
                   "y2_tab_bar: o painel que cruza a borda da faixa tem de "
                   "dar EXATAMENTE 1 CruzaTabBar (deu %u)",
                   countRule(ps, layout::Problem::CruzaTabBar));
    }
    // (b) a TAB PRÓPRIA: contida na faixa = autorizada (0 problemas)
    {
        layout::Record r = baseRecord();
        r.add(panel(100.0f, bandY0 + 4.0f, 120.0f, 24.0f));   // dentro
        r.add(botao(240.0f, bandY0 + 2.0f, 80.0f, 28.0f));    // tab interativa
        const auto ps = layout::validate(r);
        EXPECT_MSG(countRule(ps, layout::Problem::CruzaTabBar) == 0,
                   "y2_tab_bar: o conteúdo CONTIDO na faixa é a tab bar "
                   "própria (deu %u CruzaTabBar)",
                   countRule(ps, layout::Problem::CruzaTabBar));
    }
    // (c) ACIMA da faixa: intocado (0)
    {
        layout::Record r = baseRecord();
        r.add(panel(100.0f, bandY0 - 40.0f, 300.0f, 36.0f));   // termina 4px acima
        const auto ps = layout::validate(r);
        EXPECT(countRule(ps, layout::Problem::CruzaTabBar) == 0);
    }
    // (d) o conteúdo de SCROLL cortado na borda da região: POR DESENHO
    {
        layout::Record r = baseRecord();
        r.add(panel(100.0f, bandY0 - 10.0f, 300.0f, 24.0f, /*clip=*/true));
        const auto ps = layout::validate(r);
        EXPECT(countRule(ps, layout::Problem::CruzaTabBar) == 0);
    }
    // (e) o botão INTERATIVO que cruza: A REGRA APLICA A TODOS (e o
    // ForaDoEcra de sempre continua a contar o resto)
    {
        layout::Record r = baseRecord();
        r.add(botao(100.0f, bandY0 - 10.0f, 200.0f, 24.0f));
        const auto ps = layout::validate(r);
        EXPECT(countRule(ps, layout::Problem::CruzaTabBar) == 1);
    }
}

// ---- PIN 2 — a regra PAINEL_FORA (o overflow do não-interativo) -----------
TEST(y2_validador_overflow) {
    const f32 cx = kInset;   // contentRect [24..1512] x [24..696]
    const f32 cy = kInset;
    const f32 cw = kSW - 2.0f * kInset;
    const f32 ch = kSH - 2.0f * kInset;

    // (a) o DEFEITO: um painel que sangra a borda DIREITA do contentRect
    {
        layout::Record r = baseRecord();
        r.add(panel(cx + 10.0f, 100.0f, cw - 20.0f, 40.0f));   // ok (dentro)
        r.add(panel(100.0f, 200.0f, cw + 10.0f, 40.0f));    // sangra 10px
        const auto ps = layout::validate(r);
        EXPECT_MSG(countRule(ps, layout::Problem::PainelFora) == 1,
                   "y2_overflow: o painel que sangra a borda dá 1 "
                   "PainelFora (deu %u)",
                   countRule(ps, layout::Problem::PainelFora));
    }
    // (b) o VÉU de propósito: contém o contentRect INTEIRO = intencional
    {
        layout::Record r = baseRecord();
        r.add(panel(0.0f, 0.0f, kSW, kSH));   // o scrim sobre a superfície
        const auto ps = layout::validate(r);
        EXPECT(countRule(ps, layout::Problem::PainelFora) == 0);
    }
    // (c) o frame que sangra o TOPO (o cabeçalho a subir para a inset)
    {
        layout::Record r = baseRecord();
        layout::Entry f = panel(100.0f, cy - 6.0f, 400.0f, 40.0f);
        f.kind = layout::Entry::Frame;
        r.add(f);
        const auto ps = layout::validate(r);
        EXPECT(countRule(ps, layout::Problem::PainelFora) == 1);
    }
    // (d) a moldura DENTRO do contentRect: limpa
    {
        layout::Record r = baseRecord();
        layout::Entry f = panel(cx + 10.0f, cy + 10.0f, 400.0f, 40.0f);
        f.kind = layout::Entry::Frame;
        r.add(f);
        const auto ps = layout::validate(r);
        EXPECT(countRule(ps, layout::Problem::PainelFora) == 0);
        EXPECT(countRule(ps, layout::Problem::ForaDoEcra) == 0);
    }
    // (e) a densidade do registo manda: a 2.0 a faixa é 64px
    {
        layout::Record r = baseRecord();
        r.density = 2.0f;
        const f32 bandY0 = kSH - kInset - 2.0f * safe::kBottomTabH;   // 632
        // encostada à faixa de 1.0 (termina em 664) — a 2.0 É cruzamento
        r.add(panel(100.0f, bandY0 - 8.0f, 300.0f, 8.0f));   // 624..632 ✓ borda
        layout::Record r2 = baseRecord();
        r2.density = 2.0f;
        r2.add(panel(100.0f, bandY0 - 40.0f, 300.0f, 40.0f));   // 592..632
        // o (e1): termina NA borda — contida na faixa nova? não: termina
        // EM bandY0 → fora da faixa (tocar a borda não é cruzar)
        const auto ps1 = layout::validate(r);
        EXPECT(countRule(ps1, layout::Problem::CruzaTabBar) == 0);
        // o (e2): UM pixel dentro da faixa (termina em bandY0+1)
        layout::Record r3 = baseRecord();
        r3.density = 2.0f;
        r3.add(panel(100.0f, bandY0 - 39.0f, 300.0f, 40.0f));   // ..633
        const auto ps3 = layout::validate(r3);
        EXPECT_MSG(countRule(ps3, layout::Problem::CruzaTabBar) == 1,
                   "y2_tab_bar: a densidade 2.0 alarga a faixa para 64px "
                   "(deu %u)",
                   countRule(ps3, layout::Problem::CruzaTabBar));
        (void)cx;
        (void)cw;
        (void)ch;
    }
}

// ---- PIN 3 — as DUAS regras NO rules-degrestos: a ordem e a soma ---------
TEST(y2_validador_soma) {
    // um ecrã com TUDO errado: os dois defeitos NOVOS contam-se ao lado
    // das regras de sempre (o validador continua a devolver o pacote todo)
    layout::Record r = baseRecord();
    const f32 bandY0 = kSH - kInset - safe::kBottomTabH;
    r.add(panel(100.0f, bandY0 - 10.0f, 300.0f, 24.0f));     // CruzaTabBar
    r.add(panel(100.0f, 100.0f, kSW - 100.0f + 20.0f, 40.0f));  // PainelFora
    const auto ps = layout::validate(r);
    EXPECT(countRule(ps, layout::Problem::CruzaTabBar) == 1);
    EXPECT(countRule(ps, layout::Problem::PainelFora) == 1);
    // as regras de SEMPRE continuam vivas (o interativo fora do ecrã)
    r.add(botao(kSW - 10.0f, 100.0f, 60.0f, 40.0f));   // metade fora
    const auto ps2 = layout::validate(r);
    EXPECT(countRule(ps2, layout::Problem::ForaDoEcra) >= 1);
    // o describe/report não crasham com as regras novas (o caminho do log)
    for (const auto& p : ps2) {
        const std::string d = layout::describe(r, p);
        EXPECT(!d.empty());
    }
    const std::string rep = layout::report(r, ps2);
    // o relatório do ecrã diz as regras NOVAS em texto humano (o caminho
    // do log do device — a linha describe de cada problema)
    EXPECT(rep.find("CRUZA a tab bar") != std::string::npos);
    EXPECT(rep.find("SANGRA o contentRect") != std::string::npos);
    // e os NOMES das regras existem no contrato (o validador os expõe)
    EXPECT(std::string(layout::Problem().ruleName()) == "fora_do_ecra");
    layout::Problem pc;
    pc.rule = layout::Problem::CruzaTabBar;
    EXPECT(std::string(pc.ruleName()) == "cruza_tab_bar");
    layout::Problem pf;
    pf.rule = layout::Problem::PainelFora;
    EXPECT(std::string(pf.ruleName()) == "painel_fora");
}
