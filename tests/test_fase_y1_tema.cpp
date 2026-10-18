// tests/test_fase_y1_tema.cpp — os pins da Y-1 (0.11-Y: o TEMA — a
// identidade visual G.One VV no ecrã, o par do ícone da Y-0).
//
// O contrato do prompt da Y-1:
//   • A TABELA (Theme.h — a fonte única): bg #08080A (O POÇO — o mesmo do
//     launcher), surface #151517 α0.80 / surface2 #1E1E21 α0.86 (o vidro
//     -5%, o MESMO alpha da decisão do dono), border #4A3714 α0.60 (a
//     hairline ÂMBAR — o RGB do accentDim), accent #FFB020 intacto,
//     accentDim #4A3714 intacto, danger/warn/ok intactos + OS DOIS NOVOS:
//     accent2 #FF6B00 (O FOGO) e glow #FF3D00 α0.25.
//   • O BOTÃO ATIVO é a chama: glow de foco atrás (2 quads α decrescente)
//     + o fill com o GRADIENTE VERTICAL accent→accent2 (o repouso fica o
//     vidro PANEL de sempre) — no CHOKE POINT button().
//   • A LINHA DO TOPO DO VIDRO: 1dp com o RGB do glow a 20% (o brilho do
//     cristal), desenhada a seguir a panelRounded/frameRounded (os
//     primitivos de sempre INTACTOS).
//   • A auditoria de contraste contínua HONESTA nos novos valores (os
//     pisos text ≥4,5:1 / componentes ≥3:1 — recalibrados, não frouxos).
//
// As MUTAÇÕES do relatório (M-Y1a accent2, M-Y1b bg, M-Y1c border) correm
// contra uma CÓPIA do repo via GONI_ROOT_OVERRIDE — o mesmo padrão dos
// pins y0_*/ctx_* (a fonte Theme.h É o artefago pinado; o pin lê-a).
#include "TestFramework.h"

#include "platform/InputState.h"
#include "render/QuadBatch.h"
#include "ui/Theme.h"
#include "ui/UiContext.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>

using namespace vv;   // f32/u32/theme:: à solta (o padrão dos TUs irmãos)

namespace {

std::string y1Root() {
    if (const char* e = std::getenv("GONI_ROOT_OVERRIDE")) return e;
#ifdef REPO_ROOT
    return REPO_ROOT;
#else
    return ".";
#endif
}

bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    out.assign(std::istreambuf_iterator<char>(f),
               std::istreambuf_iterator<char>());
    return true;
}

u32 countSub(const std::string& hay, const std::string& needle) {
    u32 n = 0;
    for (size_t p = hay.find(needle); p != std::string::npos;
         p = hay.find(needle, p + needle.size())) {
        ++n;
    }
    return n;
}

bool nearColor(const f32* c, f32 r, f32 g, f32 b, f32 a, f32 eps = 1e-4f) {
    return test::nearEqF(c[0], r, eps) && test::nearEqF(c[1], g, eps) &&
           test::nearEqF(c[2], b, eps) && test::nearEqF(c[3], a, eps);
}

// a forma por-COMPONENTE (para os vértices do batch)
bool nearColor2(f32 r, f32 g, f32 b, f32 a, f32 er, f32 eg, f32 eb, f32 ea,
                f32 eps = 1e-4f) {
    return test::nearEqF(r, er, eps) && test::nearEqF(g, eg, eps) &&
           test::nearEqF(b, eb, eps) && test::nearEqF(a, ea, eps);
}

} // namespace

// ---- PIN 1 — a TABELA (a fonte Theme.h + o kTheme compilado) ---------------
TEST(y1_tokens) {
    // (a) o kTheme COMPILADO (a verdade que a app inteira consome)
    // 0.12-C (C-3 · recalibração legítima): a ESCADA DE SUPERFÍCIES do
    // dono supera o poço da Y-1 (o padrão Y-3/D11) — o FUNDO #0A0A0C e o
    // PAINEL #141416 α0.85 (0.12-E E-3: a FASE 3 do dono — «fundo escuro
    // com alpha 0.85»; o RGB é a paleta da E-2)
    EXPECT(nearColor(theme::kTheme.bg, 10.0f / 255.0f, 10.0f / 255.0f,
                     12.0f / 255.0f, 1.0f));
    EXPECT(nearColor(theme::kTheme.surface, 20.0f / 255.0f, 20.0f / 255.0f,
                     22.0f / 255.0f, 0.85f));
    // OS DOIS NÍVEIS NOVOS da escada (o CARTÃO e o ELEVADO)
    EXPECT(nearColor(theme::kTheme.card, 27.0f / 255.0f, 27.0f / 255.0f,
                     32.0f / 255.0f, 1.0f));
    EXPECT(nearColor(theme::kTheme.elevated, 36.0f / 255.0f, 36.0f / 255.0f,
                     42.0f / 255.0f, 1.0f));
    // os 4 tons são DISTINTOS (a regra do dono)
    EXPECT(!(theme::kTheme.bg[0] == theme::kTheme.surface[0] &&
             theme::kTheme.surface[0] == theme::kTheme.card[0] &&
             theme::kTheme.card[0] == theme::kTheme.elevated[0]));
    EXPECT(nearColor(theme::kTheme.surface2, 30.0f / 255.0f, 30.0f / 255.0f,
                     33.0f / 255.0f, 0.86f));
    // a hairline ÂMBAR: o RGB É o do accentDim (o contrato do sub-tom)
    EXPECT(nearColor(theme::kTheme.border, 74.0f / 255.0f, 55.0f / 255.0f,
                     20.0f / 255.0f, 0.60f));
    EXPECT(theme::kTheme.border[0] == theme::kTheme.accentDim[0] &&
           theme::kTheme.border[1] == theme::kTheme.accentDim[1] &&
           theme::kTheme.border[2] == theme::kTheme.accentDim[2]);
    // os INTACTOS
    EXPECT(nearColor(theme::kTheme.accent, 1.0f, 176.0f / 255.0f,
                     32.0f / 255.0f, 1.0f));
    EXPECT(nearColor(theme::kTheme.accentDim, 74.0f / 255.0f, 55.0f / 255.0f,
                     20.0f / 255.0f, 1.0f));
    EXPECT(nearColor(theme::kTheme.danger, 229.0f / 255.0f, 72.0f / 255.0f,
                     77.0f / 255.0f, 1.0f));
    EXPECT(nearColor(theme::kTheme.warn, 1.0f, 138.0f / 255.0f,
                     61.0f / 255.0f, 1.0f));
    EXPECT(nearColor(theme::kTheme.ok, 70.0f / 255.0f, 167.0f / 255.0f,
                     88.0f / 255.0f, 1.0f));
    // OS DOIS NOVOS (O FOGO e O BRILHO)
    EXPECT(nearColor(theme::kTheme.accent2, 1.0f, 107.0f / 255.0f, 0.0f,
                     1.0f));
    EXPECT(nearColor(theme::kTheme.glow, 1.0f, 61.0f / 255.0f, 0.0f, 0.25f));
    // (b) a FONTE (o artefago pinado — GONI_ROOT_OVERRIDE nas mutações)
    std::string src;
    ASSERT(readFile(y1Root() + "/app/src/main/cpp/ui/Theme.h", src));
    EXPECT_MSG(countSub(src, "VV_RGB(10, 10, 12)") == 1,
               "y1_tokens: o bg VV_RGB(10,10,12) tem de aparecer EXATAMENTE "
               "1x (encontrado %u) — 0.12-C: a escada do dono",
               countSub(src, "VV_RGB(10, 10, 12)"));
    // 0.12-E E-2: o painel é o #141416 da paleta do dono (era #131316)
    EXPECT(countSub(src, "VV_RGBA(20, 20, 22, 0.85f)") == 1);
    EXPECT(countSub(src, "VV_RGBA(30, 30, 33, 0.86f)") == 1);
    EXPECT(countSub(src, "VV_RGBA(74, 55, 20, 0.60f)") == 1);
    EXPECT(countSub(src, "VV_RGB(255, 107, 0)") == 1);
    EXPECT(countSub(src, "VV_RGBA(255, 61, 0, 0.25f)") == 1);
    // os valores VELHOS da tabela morrem (o accentInk é o ÚNICO
    // VV_RGB(14, 14, 16) que fica — a tinta sobre o âmbar não muda)
    EXPECT_MSG(countSub(src, "VV_RGB(14, 14, 16)") == 1,
               "y1_tokens: VV_RGB(14,14,16) só no accentInk (encontrado %u)",
               countSub(src, "VV_RGB(14, 14, 16)"));
    EXPECT(countSub(src, "VV_RGBA(22, 22, 24") == 0);
    EXPECT(countSub(src, "VV_RGBA(32, 32, 35") == 0);
    EXPECT(countSub(src, "VV_RGBA(46, 46, 50") == 0);
}

// ---- PIN 2 — os ALIASES ligados bit-a-bit (a promessa da fonte única) ------
TEST(y1_aliases) {
    for (int i = 0; i < 4; ++i) {
        EXPECT(theme::BG[i] == theme::kTheme.bg[i]);
        EXPECT(theme::PANEL[i] == theme::kTheme.surface[i]);
        EXPECT(theme::LINE[i] == theme::kTheme.border[i]);
        EXPECT(theme::ACCENT2[i] == theme::kTheme.accent2[i]);
        EXPECT(theme::GLOW[i] == theme::kTheme.glow[i]);
    }
}

// ---- PIN 3 — a auditoria de contraste HONESTA nos valores novos -----------
TEST(y1_contraste) {
    // texto ≥4,5:1 sobre surface sólida
    EXPECT(theme::contrastOnSurface(theme::kTheme.text1) >= 4.5f);
    EXPECT(theme::contrastOnSurface(theme::kTheme.text2) >= 4.5f);
    // componentes ≥3:1 sobre surface
    EXPECT(theme::contrastOnSurface(theme::kTheme.accent) >= 3.0f);
    EXPECT(theme::contrastOnSurface(theme::kTheme.danger) >= 3.0f);
    EXPECT(theme::contrastOnSurface(theme::kTheme.warn) >= 3.0f);
    EXPECT(theme::contrastOnSurface(theme::kTheme.ok) >= 3.0f);
    // o vidro REAL (surface α0.80 sobre o POÇO): o que flutua segura 4,5
    EXPECT(theme::contrastOnGlassReal(theme::kTheme.text1) >= 4.5f);
    EXPECT(theme::contrastOnGlassReal(theme::kTheme.accent) >= 4.5f);
    // a paleta V.ONI no poço
    EXPECT(theme::contrastOnBg(theme::kTheme.voniComment) >= 4.5f);
    EXPECT(theme::contrastOnBg(theme::kTheme.voniUser) >= 4.5f);
    // o TEXTO DO BOTÃO ATIVO: a tinta é o BG sobre o gradiente inteiro —
    // no accent2 (o pior caso do degradê, mais claro que o BG) ≥4,5
    EXPECT(theme::contrastRatio(theme::kTheme.bg, theme::kTheme.accent) >=
           4.5f);
    EXPECT(theme::contrastRatio(theme::kTheme.bg, theme::kTheme.accent2) >=
           4.5f);
}

// ---- PIN 4 — as PRIMITIVAS do cristal (linha do topo + glow de foco) ------
TEST(y1_primitivas) {
    vv::theme::setDensity(1.0f);
    vv::InputState in;
    vv::UiContext ui;
    ui.beginFrame(nullptr, &in, 480.0f, 320.0f);

    // A LINHA DO TOPO: exatamente 1 quad (6 vértices), h = dp(1), o RGB do
    // glow com α0.20
    {
        const u32 before = ui.solidsForTest().vertexCount();
        ui.glassTopLine(100.0f, 50.0f, 200.0f);
        const u32 after = ui.solidsForTest().vertexCount();
        EXPECT_MSG(after - before == 6u,
                   "y1_primitivas: a glassTopLine emite UM quad (6 vértices; "
                   "emitidos %u)", after - before);
        const vv::QuadVertex* v = ui.solidsForTest().vertices() + before;
        f32 mnY = 1e9f, mxY = -1e9f, mnX = 1e9f, mxX = -1e9f;
        for (u32 i = 0; i < 6; ++i) {
            mnY = v[i].y < mnY ? v[i].y : mnY;
            mxY = v[i].y > mxY ? v[i].y : mxY;
            mnX = v[i].x < mnX ? v[i].x : mnX;
            mxX = v[i].x > mxX ? v[i].x : mxX;
            // 0.12-E (E-3 · FASE 3.1): a linha do topo é o ÂMBAR a 20%
            // (a ordem do dono — era o RGB do glow #FF3D00 da Y-1)
            EXPECT(nearColor2(v[i].r, v[i].g, v[i].b, v[i].a, 1.0f,
                              176.0f / 255.0f, 32.0f / 255.0f, 0.20f));
        }
        EXPECT(test::nearEqF(mxY - mnY, 1.0f, 1e-3f));   // 1dp (densidade 1)
        EXPECT(test::nearEqF(mnX, 100.0f, 1e-3f) &&
               test::nearEqF(mxX, 300.0f, 1e-3f));
        EXPECT(test::nearEqF(mnY, 50.0f, 1e-3f));   // a linha no TOPO dado
    }

    // O GLOW DE FOCO: 2 quads (12 vértices), α DECRESCENTE para fora
    // (interior 4dp @ glow.a×0.8 = 0.20; exterior 8dp @ glow.a×0.4 = 0.10)
    {
        const u32 before = ui.solidsForTest().vertexCount();
        ui.focusGlow(200.0f, 150.0f, 80.0f, 40.0f);
        const u32 after = ui.solidsForTest().vertexCount();
        EXPECT_MSG(after - before == 12u,
                   "y1_primitivas: o focusGlow emite 2 quads (12 vértices; "
                   "emitidos %u)", after - before);
        const vv::QuadVertex* v = ui.solidsForTest().vertices() + before;
        // o 1.º quad é o EXTERIOR (o mais transparente), o 2.º o interior
        auto quadBounds = [&](u32 base, f32& mnX, f32& mxX, f32& mnY,
                              f32& mxY, f32& a) {
            mnX = mnY = 1e9f;
            mxX = mxY = -1e9f;
            a = v[base].a;
            for (u32 i = base; i < base + 6; ++i) {
                mnX = v[i].x < mnX ? v[i].x : mnX;
                mxX = v[i].x > mxX ? v[i].x : mxX;
                mnY = v[i].y < mnY ? v[i].y : mnY;
                mxY = v[i].y > mxY ? v[i].y : mxY;
                EXPECT(nearColor2(v[i].r, v[i].g, v[i].b, v[i].a, 1.0f,
                                  61.0f / 255.0f, 0.0f, v[base].a));
            }
        };
        f32 x0, x1, y0, y1, aOut, aIn;
        quadBounds(0, x0, x1, y0, y1, aOut);
        EXPECT(test::nearEqF(aOut, 0.10f, 1e-4f));   // o exterior a 0.10
        EXPECT(test::nearEqF(x0, 200.0f - 8.0f, 1e-3f) &&
               test::nearEqF(y0, 150.0f - 8.0f, 1e-3f) &&
               test::nearEqF(x1, 280.0f + 8.0f, 1e-3f) &&
               test::nearEqF(y1, 190.0f + 8.0f, 1e-3f));   // exp 8dp
        quadBounds(6, x0, x1, y0, y1, aIn);
        EXPECT(test::nearEqF(aIn, 0.20f, 1e-4f));   // o interior a 0.20
        EXPECT(test::nearEqF(x0, 200.0f - 4.0f, 1e-3f) &&
               test::nearEqF(y0, 150.0f - 4.0f, 1e-3f) &&
               test::nearEqF(x1, 280.0f + 4.0f, 1e-3f) &&
               test::nearEqF(y1, 190.0f + 4.0f, 1e-3f));   // exp 4dp
        EXPECT(aIn > aOut);   // α DECRESCENTE para fora (o degrau do brilho)
    }
    ui.endFrame();
}

// ---- PIN 5 — O BOTÃO ATIVO É A CHAMA (gradiente accent→accent2) ------------
TEST(y1_gradiente_botao) {
    vv::theme::setDensity(1.0f);
    const f32 bx = 60.0f, by = 40.0f, bw = 120.0f, bh = 40.0f;
    const f32 cx = bx + bw * 0.5f, cy = by + bh * 0.5f;

    // (a) PREMIDO: o fill é o gradiente (topo accent, fundo accent2)
    {
        vv::InputState in;
        in.injectDown(0, cx, cy);   // a aresta de press DENTRO do botão
        vv::UiContext ui;
        ui.beginFrame(nullptr, &in, 480.0f, 320.0f);
        const u32 before = ui.solidsForTest().vertexCount();
        const bool pressed = ui.button(0x9151ull, bx, by, bw, bh, "OK");
        const u32 after = ui.solidsForTest().vertexCount();
        // o frame do PRESS captura (o release é que devolve true — o
        // contrato do widgetHit); o visual de held é provado pelo
        // gradiente a seguir
        EXPECT(!pressed);
        EXPECT(after > before + 6u);
        // a coluna CENTRAL da escadaria: o quad que cruza a altura INTEIRA
        // (topo == accent exato, fundo == accent2 exato — o span completo)
        bool foundCentral = false, foundMid = false;
        for (u32 i = before; i + 5 < after; i += 6) {
            const vv::QuadVertex* v = ui.solidsForTest().vertices();
            f32 mnY = 1e9f, mxY = -1e9f;
            for (u32 k = i; k < i + 6; ++k) {
                mnY = v[k].y < mnY ? v[k].y : mnY;
                mxY = v[k].y > mxY ? v[k].y : mxY;
            }
            const bool fullSpan = test::nearEqF(mnY, by, 0.05f) &&
                                  test::nearEqF(mxY, by + bh, 0.05f);
            if (!fullSpan) continue;
            foundCentral = true;
            // o topo do quad é o FIRERED exato, o fundo o FIREORANGE
            // exato (0.12-E E-3 · FASE 3.3: o gradiente da paleta FOGO
            // AVERMELHADO do dono — o accent→accent2 da Y-1 morreu)
            for (u32 k = i; k < i + 6; ++k) {
                if (test::nearEqF(v[k].y, mnY, 0.05f)) {
                    EXPECT(nearColor2(v[k].r, v[k].g, v[k].b, v[k].a,
                                      217.0f / 255.0f, 43.0f / 255.0f,
                                      4.0f / 255.0f, 1.0f));
                }
                if (test::nearEqF(v[k].y, mxY, 0.05f)) {
                    EXPECT(nearColor2(v[k].r, v[k].g, v[k].b, v[k].a,
                                      255.0f / 255.0f, 94.0f / 255.0f,
                                      0.0f, 1.0f));
                }
            }
        }
        EXPECT_MSG(foundCentral,
                   "y1_gradiente: a coluna central do fill em span completo "
                   "não foi encontrada no batch");
        // o gradiente É CONTÍNUO através das peças: as bandas laterais e
        // os degraus dos cantos são AMOSTRADOS do span completo — algum
        // vértice do batch tem cor INTERMÉDIA entre fireRed e fireOrange
        // (0.12-E E-3: g entre 43 e 94, r entre 217 e 255 — a interpolação
        // real da paleta FOGO AVERMELHADO)
        for (u32 i = before; i < after; ++i) {
            const vv::QuadVertex* v = ui.solidsForTest().vertices();
            if (v[i].r > 217.0f / 255.0f + 0.005f &&
                v[i].r < 255.0f / 255.0f - 0.005f &&
                v[i].g > 43.0f / 255.0f + 0.005f &&
                v[i].g < 94.0f / 255.0f - 0.005f &&
                v[i].b > 0.0f && v[i].b < 4.0f / 255.0f &&
                test::nearEqF(v[i].a, 1.0f, 1e-4f)) {
                foundMid = true;
            }
        }
        EXPECT_MSG(foundMid,
                   "y1_gradiente: nenhum vértice INTERMÉDIO do degradê "
                   "(o gradiente não é contínuo através das peças)");
        ui.endFrame();
    }

    // (b) EM REPOUSO: SEM gradiente (nenhum vértice accent2 no fill) —
    // o vidro PANEL de sempre
    {
        vv::InputState in;
        vv::UiContext ui;
        ui.beginFrame(nullptr, &in, 480.0f, 320.0f);
        const u32 before = ui.solidsForTest().vertexCount();
        ui.button(0x9151ull, bx, by, bw, bh, "OK");
        const u32 after = ui.solidsForTest().vertexCount();
        bool accent2Found = false;
        for (u32 i = before; i < after; ++i) {
            const vv::QuadVertex* v = ui.solidsForTest().vertices();
            if (test::nearEqF(v[i].g, 107.0f / 255.0f, 1e-4f) &&
                test::nearEqF(v[i].b, 0.0f, 1e-4f)) {
                accent2Found = true;
            }
        }
        EXPECT_MSG(!accent2Found,
                   "y1_gradiente: o botão EM REPOUSO não pode ter o fogo "
                   "(accent2 só no estado ativo)");
        ui.endFrame();
    }
}
