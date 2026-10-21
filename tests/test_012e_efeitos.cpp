// tests/test_012e_efeitos.cpp — 0.12-E (E-3): os pins dos EFEITOS 2.5D
// (a ordem do dono: o vidro α0.85 + a linha âmbar do topo; a sombra falsa
// preta 15% deslocada 3px; o gradiente fireRed→fireOrange dos ativos; a
// textura de ruído 2D em scroll lento nas bordas ativas — ZERO shaders
// matemáticos). Pelo caminho REAL (o pipeline Renderer+UiContext no
// rasterizador honesto do stub) ou pelas FONTES PURAS (Noise2D).
#include "glstub_fb.h"
#include <GLES3/gl3.h>
#include <cmath>
#include <vector>

#include "core/Types.h"
#include "platform/InputState.h"
#include "render/QuadBatch.h"
#include "render/Renderer.h"
#include "TestFramework.h"
#include "ui/Noise2D.h"
#include "ui/Sdf.h"
#include "ui/Theme.h"
#include "ui/UiContext.h"

using namespace vv;

namespace {

bool nearC(const f32* a, f32 r, f32 g, f32 b, f32 al, f32 eps = 1e-3f) {
    return std::fabs(a[0] - r) < eps && std::fabs(a[1] - g) < eps &&
           std::fabs(a[2] - b) < eps && std::fabs(a[3] - al) < eps;
}

struct Rect6 {
    f32 x0, y0, x1, y1, u0, v0, u1, v1;
    f32 r, g, b, a;
};

// reconstroi os quads do batch (6 vértices por quad; uv min/max)
void collectQuads(const QuadBatch& b, std::vector<Rect6>& out) {
    const QuadVertex* v = b.vertices();
    const u32 n = b.vertexCount();
    for (u32 i = 0; i + 5 < n; i += 6) {
        Rect6 q{1e9f, 1e9f, -1e9f, -1e9f, 1e9f, 1e9f, -1e9f, -1e9f,
                v[i].r, v[i].g, v[i].b, v[i].a};
        for (u32 k = i; k < i + 6; ++k) {
            q.x0 = v[k].x < q.x0 ? v[k].x : q.x0;
            q.y0 = v[k].y < q.y0 ? v[k].y : q.y0;
            q.x1 = v[k].x > q.x1 ? v[k].x : q.x1;
            q.y1 = v[k].y > q.y1 ? v[k].y : q.y1;
            q.u0 = v[k].u < q.u0 ? v[k].u : q.u0;
            q.v0 = v[k].v < q.v0 ? v[k].v : q.v0;
            q.u1 = v[k].u > q.u1 ? v[k].u : q.u1;
            q.v1 = v[k].v > q.v1 ? v[k].v : q.v1;
        }
        out.push_back(q);
    }
}

} // namespace

// ---- PIN 1 — A SOMBRA 2.5D (FASE 3.2) ---------------------------------------
// A ordem do dono: «um retângulo preto com 15% de opacidade deslocado 3px
// para baixo e para a direita por baixo de cada botão». O CHOKE POINT
// drawButtonVisual emite a sombra ANTES do corpo (em repouso E no ativo).
TEST(sombra_25d) {
    vv::theme::setDensity(1.0f);
    const f32 bx = 60.0f, by = 40.0f, bw = 120.0f, bh = 40.0f;

    // (a) REPOUSO: a sombra existe no offset exato (+3,+3) com o α 0.15
    {
        vv::UiContext ui;
        ui.beginFrame(nullptr, nullptr, 480.0f, 320.0f);
        ui.button(0xE301ull, bx, by, bw, bh, "OK");
        ui.endFrame();
        std::vector<Rect6> qs;
        collectQuads(ui.solidsForTest(), qs);
        bool sombra = false;
        for (const Rect6& q : qs) {
            const bool pos = test::nearEqF(q.y0, by + 3.0f, 0.05f) &&
                             test::nearEqF(q.y1, by + bh + 3.0f, 0.05f) &&
                             q.x0 >= bx + 3.0f - 0.05f &&
                             q.x1 <= bx + bw + 3.0f + 0.05f;;
            // o miolo da escadaria leva a cor INTEIRA (as peças dos cantos
            // amostram o α do span — a sonda aferra o miolo do offset)
            if (pos && test::nearEqF(q.a, 0.15f, 0.01f) &&
                test::nearEqF(q.r, 0.0f, 0.01f) && test::nearEqF(q.g, 0.0f, 0.01f) &&
                test::nearEqF(q.b, 0.0f, 0.01f)) {
                sombra = true;
            }
        }
        EXPECT_MSG(sombra,
                   "sombra_25d: o retângulo preto 15%% deslocado (+3,+3) "
                   "não foi emitido por baixo do botão em repouso");
    }
    // (b) O TOKEN existe no Theme (a FONTE ÚNICA — preto α0.15)
    EXPECT(nearC(vv::theme::kTheme.dropShadow, 0.0f, 0.0f, 0.0f, 0.15f));
    // (c) o ATIVO TAMBÉM tem sombra (o premido não perde a profundidade)
    {
        vv::InputState in;
        in.injectDown(0, bx + bw * 0.5f, by + bh * 0.5f);
        vv::UiContext ui;
        ui.beginFrame(nullptr, &in, 480.0f, 320.0f);
        ui.button(0xE302ull, bx, by, bw, bh, "OK");
        ui.endFrame();
        std::vector<Rect6> qs;
        collectQuads(ui.solidsForTest(), qs);
        bool sombra = false;
        for (const Rect6& q : qs) {
            const bool pos = test::nearEqF(q.y0, by + 3.0f, 0.05f) &&
                             test::nearEqF(q.y1, by + bh + 3.0f, 0.05f) &&
                             q.x0 >= bx + 3.0f - 0.05f;
            if (pos && test::nearEqF(q.a, 0.15f, 0.01f)) {
                sombra = true;
            }
        }
        EXPECT_MSG(sombra,
                   "sombra_25d: o botão ATIVO perdeu a sombra 2.5D (a "
                   "profundidade falsa é de TODOS os botões)");
    }
}

// ---- PIN 2 — O VIDRO 0.85 + A LINHA ÂMBAR DO TOPO (FASE 3.1) ----------------
// «Nos painéis, fundo escuro com alpha 0.85 + uma linha de 1px no topo com
// a cor amber a 20% de opacidade (o reflexo da luz)».
TEST(vidro_85_topline_ambar) {
    vv::theme::setDensity(1.0f);
    // (a) o TOKEN: o surface α0.85 (o corpo do vidro — o #121215 da FASE UI)
    EXPECT(nearC(vv::theme::kTheme.surface, 18.0f / 255.0f, 18.0f / 255.0f,
                 21.0f / 255.0f, 0.85f));
    // (b) a LINHA: 1dp, no topo, A BRASA (#FF5A1F) a α0.20 — pelo emissor
    //     real (o mesmo que os painéis/menus chamam)
    {
        vv::UiContext ui;
        ui.beginFrame(nullptr, nullptr, 480.0f, 320.0f);
        ui.glassTopLine(100.0f, 50.0f, 200.0f);
        ui.endFrame();
        std::vector<Rect6> qs;
        collectQuads(ui.solidsForTest(), qs);
        bool linha = false;
        for (const Rect6& q : qs) {
            if (test::nearEqF(q.y0, 50.0f, 0.05f) &&
                test::nearEqF(q.y1 - q.y0, 1.0f, 0.05f) &&
                test::nearEqF(q.x1 - q.x0, 200.0f, 0.05f) &&
                test::nearEqF(q.r, 1.0f, 0.01f) &&
                test::nearEqF(q.g, 90.0f / 255.0f, 0.01f) &&
                test::nearEqF(q.b, 31.0f / 255.0f, 0.01f) &&
                test::nearEqF(q.a, 0.20f, 0.01f)) {
                linha = true;
            }
        }
        EXPECT_MSG(linha,
                   "vidro_85_topline: a linha 1dp do topo não saiu BRASA a "
                   "20%% (o reflexo da luz da ordem do dono)");
    }
}

// ---- PIN 3 — A BORDA DE RUÍDO EM SCROLL (FASE 3.4, caminho REAL) ------------
// «uma textura 2D de ruído em scroll lento (UV offset)»: o botão ATIVO
// emite as 4 bandas no batch de imagens com a textura de ruído do
// Renderer; o UV offset é noise2d::fireScrollOffset(sdf::g_time) — o
// MESMO t = o MESMO offset (o determinismo da animação).
TEST(fogo2d_scroll) {
    vv::theme::setDensity(1.0f);
    glstub::fb::resetState();
    glstub::fb::enabled = true;
    const f32 bx = 60.0f, by = 40.0f, bw = 120.0f, bh = 40.0f;

    auto heldFrame = [&](f32 time, f32* u0Out) {
        vv::Renderer r;
        ASSERT(r.init());
        r.resize(480, 320);
        r.beginFrame();
        r.beginUiPass(480, 320);
        sdf::g_time = time;
        vv::InputState in;
        in.injectDown(0, bx + bw * 0.5f, by + bh * 0.5f);
        vv::UiContext ui;
        ui.init();
        ui.beginFrame(&r, &in, 480.0f, 320.0f);
        ui.button(0xE303ull, bx, by, bw, bh, "OK");
        ui.endFrame();
        r.endFrame();
        // o slot 0 é a textura de ruído (o ÚNICO quad texturizado do frame)
        ASSERT(ui.imageBatchCountForTest() >= 1u);
        ASSERT(r.noiseTexture() != 0u);
        std::vector<Rect6> qs;
        collectQuads(ui.imagesForTest(), qs);
        // a banda do TOPO: o quad com y0 == by e altura 2dp
        bool banda = false;
        for (const Rect6& q : qs) {
            if (test::nearEqF(q.y0, by, 0.05f) &&
                test::nearEqF(q.y1 - q.y0, theme::dp(2.0f), 0.05f)) {
                *u0Out = q.u0;
                banda = true;
            }
        }
        EXPECT_MSG(banda,
                   "fogo2d_scroll: a banda do topo não foi emitida no "
                   "batch de imagens (o ativo sem a borda de ruído)");
        glstub::fb::resetState();
        glstub::fb::enabled = true;
    };

    f32 u0t0 = -1.0f, u0t2 = -1.0f;
    heldFrame(0.0f, &u0t0);
    heldFrame(2.0f, &u0t2);
    // o offset do t=2.0: 2 s × 6 px/s / 64 px = 0.1875 (o SCROLL LENTO)
    EXPECT_MSG(test::nearEqF(u0t0, 0.0f, 1e-3f),
               "fogo2d_scroll: o t=0 devia dar offset 0 (leu %.4f)", u0t0);
    EXPECT_MSG(test::nearEqF(u0t2, 2.0f * 6.0f / 64.0f, 1e-3f),
               "fogo2d_scroll: o t=2.0 devia dar offset 0.1875 (leu %.4f)",
               u0t2);
    glstub::fb::enabled = false;
}

// ---- PIN 4 — O RUÍDO PURO (a FONTE ÚNICA — determinismo + tile) -------------
TEST(fogo2d_determinismo) {
    // (a) determinístico: o mesmo pixel = o mesmo valor
    EXPECT(noise2d::noisePixel(3, 5) == noise2d::noisePixel(3, 5));
    // (b) TILEÁVEL: o pixel (x+64,y) É o pixel (x,y) — a emenda do
    //     GL_REPEAT é perfeita por construção (o hash embrulha na célula)
    for (int x = 0; x < 64; x += 7) {
        for (int y = 0; y < 64; y += 11) {
            EXPECT(noise2d::noisePixel(x, y) ==
                   noise2d::noisePixel(x + 64, y));
            EXPECT(noise2d::noisePixel(x, y) ==
                   noise2d::noisePixel(x, y + 64));
        }
    }
    // (c) o SCROLL LENTO: 6 px/s na textura de 64 — o wrap volta ao mesmo
    //     offset (a volta completa 64/6 s) e o offset vive em [0,1)
    EXPECT(test::nearEqF(noise2d::fireScrollOffset(0.0f), 0.0f, 1e-6f));
    EXPECT(test::nearEqF(noise2d::fireScrollOffset(2.0f), 0.1875f, 1e-4f));
    const f32 volta = 64.0f / 6.0f;
    EXPECT(test::nearEqF(noise2d::fireScrollOffset(volta),
                   noise2d::fireScrollOffset(0.0f), 1e-4f));
    for (f32 t = 0.0f; t < 40.0f; t += 1.3f) {
        const f32 off = noise2d::fireScrollOffset(t);
        EXPECT(off >= 0.0f && off < 1.0f);
    }
    // (d) o grão é BRASA (manchas, não sal): os vizinhos distantes 1px
    //     nunca saltam mais de ~64 níveis (o smoothstep suaviza)
    const int dx[4] = {1, -1, 0, 0};
    const int dy[4] = {0, 0, 1, -1};
    for (int x = 8; x < 56; x += 5) {
        for (int y = 8; y < 56; y += 5) {
            for (int k = 0; k < 4; ++k) {
                const int d = std::abs(int(noise2d::noisePixel(x, y)) -
                                       int(noise2d::noisePixel(x + dx[k],
                                                               y + dy[k])));
                EXPECT(d <= 64);
            }
        }
    }
}
