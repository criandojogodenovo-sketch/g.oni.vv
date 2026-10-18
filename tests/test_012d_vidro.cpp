// tests/test_012d_vidro.cpp — 0.12-D (D-2): O VIDRO (REAL e FALSO).
//
// Os 4 pins da sub-fase D-2 (a ordem do prompt), pelo caminho REAL:
//   1. vidro_real_blur — o painel flutuante mostra BLUR: a cena captura-se
//      (1/4 + o blur dual Kawase) e o widget SDF amostra-a; o píxel sobre
//      um quadrado brilhante ESCURECE e os vizinhos ACLAREM (o espalhar)
//   2. vidro_falso_ok — o painel fixo tem o GRADIENTE (surface→surface2)
//      + o reflexo diagonal no batch
//   3. vidro_fresnel — a BORDA é mais clara que o interior (o glassEdge)
//   4. vidro_cache — o blur SÓ corre quando preciso (a captura repete SEM
//      re-blur; a invalidação recomeça)
//
// MUTAÇÕES (coladas no RELATORIO-0.12-D.md §2):
//   M-D2a — o blur removido (o capture devolve a redução SEM o Kawase) →
//           vidro_real_blur vermelho
//   M-D2b — a cache removida (o capture recalcula sempre) → vidro_cache
//           vermelho
#include "TestFramework.h"
#include <GLES3/gl3.h>   // o stub (nada de GL real — glReadPixels do fb)
#include "glstub_fb.h"
#include "render/Renderer.h"
#include "ui/Glass.h"
#include "ui/Theme.h"
#include "ui/UiContext.h"
#include <cmath>
#include <vector>

using namespace vv;

namespace {

constexpr int kW = 160;
constexpr int kH = 120;

struct PxStub {
    unsigned char r, g, b, a;
};

struct Harness {
    Renderer renderer;
    UiContext ui;
    explicit Harness() {
        glstub::fb::resetState();
        glstub::fb::enabled = true;
        theme::setDensity(1.0f);
        EXPECT(renderer.init());
        renderer.resize(kW, kH);
        renderer.beginFrame();
        renderer.beginUiPass(kW, kH);
        ui.beginFrame(&renderer, nullptr, float(kW), float(kH));
    }
    void read(std::vector<unsigned char>& px) {
        ui.endFrame();
        renderer.endFrame();
        px.resize(size_t(kW) * kH * 4);
        glReadPixels(0, 0, kW, kH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        glstub::fb::enabled = false;
    }
};

PxStub readAt(const std::vector<unsigned char>& px, int x, int y) {
    const int row = kH - 1 - y;   // GL: linha 0 = fundo
    const size_t i = (size_t(row) * kW + size_t(x)) * 4u;
    return PxStub{px[i], px[i + 1], px[i + 2], px[i + 3]};
}

} // namespace

// ---- PIN 1 — vidro_real_blur -------------------------------------------------
// «o painel flutuante mostra blur»: um quadrado brilhante NOVO no fundo;
// o vidro por cima mostra a cena ESPALHADA (o centro mais escuro que a
// fonte, os vizinhos mais claros que o fundo) — o Kawase dual no píxel.
TEST(vidro_real_blur) {
    // (a) a FUNÇÃO PURA: o delta espalha (o pino da matemática partilhada)
    {
        const int w = 32, h = 32;
        std::vector<u8> src(size_t(w) * h * 4, 0), tmp(src), dst(src);
        // o fundo cinza + UM pixel branco no centro
        for (size_t i = 0; i < src.size(); i += 4) {
            src[i] = src[i + 1] = src[i + 2] = 40;
            src[i + 3] = 255;
        }
        const size_t c = (size_t(16) * w + 16) * 4;
        src[c] = src[c + 1] = src[c + 2] = 255;
        glass::kawaseBlurDual(src.data(), w, h, tmp.data(), dst.data());
        auto at = [&](int x, int y) {
            return (int)dst[(size_t(y) * w + size_t(x)) * 4];
        };
        // o centro DESCE (o delta espalhou) e os vizinhos SOBEM
        EXPECT_MSG(at(16, 16) < 255 && at(16, 16) > 60,
                   "vidro_real_blur: o centro do delta leu %d — o Kawase "
                   "não espalhou", at(16, 16));
        EXPECT_MSG(at(12, 16) > 40, "vidro_real_blur: o vizinho a 4px leu "
                   "%d — o blur não chegou lá", at(12, 16));
        EXPECT_MSG(at(0, 0) == 40, "vidro_real_blur: o canto longe mudou "
                   "(%d) — o blur vazou para lá do raio", at(0, 0));
    }
    // (b) o CAMINHO REAL: a captura + o widget de vidro sobre o quadrado
    {
        glstub::fb::resetState();
        glstub::fb::enabled = true;
        theme::setDensity(1.0f);
        Renderer r;
        ASSERT(r.init());
        r.resize(kW, kH);
        r.beginFrame();
        // a "cena": o fundo escuro + o quadrado brilhante 16×16 no centro
        glClearColor(0.04f, 0.04f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        {
            UiContext ui;
            ui.init();
            ui.beginFrame(&r, nullptr, float(kW), float(kH));
            const f32 sq[4] = {0.9f, 0.9f, 0.92f, 1.0f};
            ui.panel(64.0f, 44.0f, 16.0f, 16.0f, sq);   // o quadrado
            ui.endFrame();
            r.endFrame();
        }
        // a CAPTURA do viewport (o fb ainda é a cena — o frame seguinte)
        const u32 gtex = r.captureGlass(0, 0, kW, kH);
        EXPECT(gtex != 0);
        // o frame do vidro: o painel flutuante por cima do quadrado
        r.beginFrame();
        r.beginUiPass(kW, kH);
        UiContext ui;
        ui.init();
        ui.beginFrame(&r, nullptr, float(kW), float(kH));
        ui.panel(0, 0, float(kW), float(kH), theme::kTheme.bg);
        ui.sdfWidgetGlass(40.0f, 30.0f, 80.0f, 50.0f, 6.0f,
                          theme::kTheme.glassInk, gtex);
        ui.endFrame();
        r.endFrame();
        std::vector<unsigned char> px(size_t(kW) * kH * 4);
        glReadPixels(0, 0, kW, kH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        glstub::fb::enabled = false;
        // o pixel SOBRE o quadrado (72,52) — dentro do painel: o blur
        // ESPALHOU o brilho → o centro desceu do 0.9 e os vizinhos subiram
        auto lum = [&](int x, int y) {
            const PxStub p = readAt(px, x, y);
            return float(p.r) + float(p.g) + float(p.b);
        };
        const f32 lSq = lum(72, 52);       // o centro do quadrado
        const f32 lNext = lum(72, 66);     // 14px abaixo (ainda no painel)
        const f32 lBg = lum(20, 100);      // o fundo longe do quadrado
        EXPECT_MSG(lSq < 700.0f,
                   "vidro_real_blur: o centro do quadrado sob o vidro leu "
                   "%.0f — o blur não suavizou a fonte", lSq);
        EXPECT_MSG(lNext > lBg + 6.0f,
                   "vidro_real_blur: o vizinho fora do quadrado (%.0f) não "
                   "acendeu contra o fundo (%.0f) — o blur não espalhou",
                   lNext, lBg);
    }
}

// ---- PIN 2 — vidro_falso_ok ---------------------------------------------------
// «o painel fixo tem gradiente»: o corpo é o GRADIENTE surface→surface2
// (os vértices do batch) e o REFLEXO DIAGONAL existe (os veios brancos).
TEST(vidro_falso_ok) {
    theme::setDensity(1.0f);
    UiContext ui;
    ui.init();
    ui.beginFrame(nullptr, nullptr, 400.0f, 300.0f);
    ui.glassPanelFake(20.0f, 20.0f, 300.0f, 200.0f);
    ui.endFrame();
    // (a) o corpo: o quad com o GRADIENTE (topo surface, fundo surface2)
    bool foundGrad = false, foundStreak = false;
    const QuadVertex* v = ui.solidsForTest().vertices();
    const u32 n = ui.solidsForTest().vertexCount();
    for (u32 i = 0; i + 5 < n; i += 6) {
        f32 mnX = 1e9f, mxX = -1e9f, mnY = 1e9f, mxY = -1e9f;
        for (u32 k = i; k < i + 6; ++k) {
            mnX = v[k].x < mnX ? v[k].x : mnX;
            mxX = v[k].x > mxX ? v[k].x : mxX;
            mnY = v[k].y < mnY ? v[k].y : mnY;
            mxY = v[k].y > mxY ? v[k].y : mxY;
        }
        if (test::nearEqF(mnX, 20.0f, 0.05f) &&
            test::nearEqF(mxX, 320.0f, 0.05f) &&
            test::nearEqF(mnY, 20.0f, 0.05f) &&
            test::nearEqF(mxY, 220.0f, 0.05f)) {
            // o corpo: o topo surface (19,19,22 α0.80), o fundo surface2
            bool topOk = false, botOk = false;
            for (u32 k = i; k < i + 6; ++k) {
                if (test::nearEqF(v[k].y, mnY, 0.05f)) {
                    topOk = test::nearEqF(v[k].r, 19.0f / 255.0f, 0.01f) &&
                            test::nearEqF(v[k].a, 0.80f, 0.01f);
                }
                if (test::nearEqF(v[k].y, mxY, 0.05f)) {
                    botOk = test::nearEqF(v[k].r, 30.0f / 255.0f, 0.01f) &&
                            test::nearEqF(v[k].a, 0.86f, 0.01f);
                }
            }
            if (topOk && botOk) foundGrad = true;
        }
        // o reflexo: um quad diagonal (os cantos NÃO axis-aligned — o x e
        // o y variam em contra-fase) com o branco fraco
        if (v[i].r > 0.99f && v[i].a > 0.01f && v[i].a < 0.07f) {
            const f32 dx1 = std::fabs(v[i].x - v[i + 1].x);
            const f32 dy1 = std::fabs(v[i].y - v[i + 1].y);
            if (dx1 > 0.5f && dy1 > 0.5f) foundStreak = true;
        }
    }
    EXPECT_MSG(foundGrad,
               "vidro_falso_ok: o corpo com o GRADIENTE surface→surface2 "
               "não foi encontrado");
    EXPECT_MSG(foundStreak,
               "vidro_falso_ok: o REFLEXO DIAGONAL (os veios brancos) não "
               "foi encontrado");
}

// ---- PIN 3 — vidro_fresnel ----------------------------------------------------
// «a borda é mais clara»: o glassEdge de 1px do painel falso lê-se no fb
// mais claro que o interior a 8px de distância.
TEST(vidro_fresnel) {
    glstub::fb::resetState();
    glstub::fb::enabled = true;
    theme::setDensity(1.0f);
    Renderer r;
    ASSERT(r.init());
    r.resize(kW, kH);
    r.beginFrame();
    r.beginUiPass(kW, kH);
    UiContext ui;
    ui.init();
    ui.beginFrame(&r, nullptr, float(kW), float(kH));
    ui.panel(0, 0, float(kW), float(kH), theme::kTheme.bg);
    ui.glassPanelFake(30.0f, 30.0f, 90.0f, 60.0f);
    ui.endFrame();
    r.endFrame();
    std::vector<unsigned char> px(size_t(kW) * kH * 4);
    glReadPixels(0, 0, kW, kH, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    glstub::fb::enabled = false;
    auto lum = [&](int x, int y) {
        const PxStub p = readAt(px, x, y);
        return float(p.r) + float(p.g) + float(p.b);
    };
    const f32 lEdge = lum(30, 60);     // a borda esquerda (x=30)
    const f32 lInner = lum(38, 60);    // o interior (8px para dentro)
    EXPECT_MSG(lEdge > lInner + 4.0f,
               "vidro_fresnel: a borda (%.0f) não é mais clara que o "
               "interior (%.0f) — a Fresnel morreu?", lEdge, lInner);
}

// ---- PIN 4 — vidro_cache ------------------------------------------------------
// «o blur só corre quando preciso»: duas capturas SEM invalidação = UMA
// passagem de blur; a invalidação recomeça (o contador conta-as).
TEST(vidro_cache) {
    glstub::fb::resetState();
    glstub::fb::enabled = true;
    theme::setDensity(1.0f);
    Renderer r;
    ASSERT(r.init());
    r.resize(kW, kH);
    r.beginFrame();
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r.endFrame();
    // a 1.ª captura: suja de fábrica → corre
    const u32 t1 = r.captureGlass(0, 0, kW, kH);
    EXPECT(r.glassCapturesForTest() == 1u);
    EXPECT(t1 != 0);
    // a 2.ª/3.ª SEM invalidação: a CACHE responde — ZERO blur novo
    r.beginFrame();
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r.endFrame();
    const u32 t2 = r.captureGlass(0, 0, kW, kH);
    EXPECT_MSG(r.glassCapturesForTest() == 1u,
               "vidro_cache: a captura sem invalidação re-blurou (%u) — a "
               "cache morreu?", r.glassCapturesForTest());
    EXPECT(t2 == t1);
    // a invalidação: o próximo capture RECORRE (o que está atrás mudou)
    r.glassInvalidate();
    r.beginFrame();
    glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    r.endFrame();
    r.captureGlass(0, 0, kW, kH);
    EXPECT_MSG(r.glassCapturesForTest() == 2u,
               "vidro_cache: a captura após a invalidação não correu (%u)",
               r.glassCapturesForTest());
    // e a DECISÃO PURA: o estado da câmara/da cena muda → suja
    {
        glass::CacheState a;
        a.camYaw = 0.5f;
        a.camPitch = 0.4f;
        a.camDist = 6.0f;
        a.sceneVersion = 3;
        a.capX = 0;
        a.capY = 0;
        a.capW = 100;
        a.capH = 100;
        a.surfaceW = 100;
        a.surfaceH = 100;
        glass::CacheState b = a;
        EXPECT(!glass::glassCacheDirty(a, b));
        b.camYaw += 0.01f;
        EXPECT(glass::glassCacheDirty(a, b));
        b = a;
        b.sceneVersion = 4;
        EXPECT(glass::glassCacheDirty(a, b));
    }
    glstub::fb::enabled = false;
}
