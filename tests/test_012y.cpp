// tests/test_012y.cpp — 0.12-Y: O SALTO VISUAL E DE ASSETS.
//
// Os pins da campanha (um grupo por sub-fase; o B/C entram nos commits
// delas):
//   A. pan_camera_math         — a FONTE ÚNIDA do pan (worldPerPixel + os
//                                eixos right/camUp; direção "agarra a cena")
//      pan_modo_um_dedo        — 1 dedo no modo Pan desloca o TARGET e NÃO
//                                orbita (exclusividade com a orbit)
//      pan_dois_dedos_livre    — 2 dedos arrastam (midpoint) em QUALQUER
//                                modo, sem panMode (a ordem do dono)
//      pan_pinch_intacto       — o pinch (zoom) de sempre intacto
//      pan_rail_sexta          — a MÃOZINHA é a 6.ª ferramenta do rail
//                                (rail[4]=Pan id 33, rail[5]=Íman; o toque
//                                arma o panMode; a legenda diz "Pan")
//
// CLÁUSULA CALMA: o Pan é CÂMARA de edição — zero mexidas na V.ONI, zero
// motor de jogo (a câmara do Play segue intocada).
//
// MUTAÇÕES (coladas no RELATORIO-0.12-Y.md §1):
//   M-Ya1 — o pan INVERTIDO (target += right em vez de -=) →
//           pan_camera_math + pan_modo_um_dedo + regress_pan vermelhos
//   M-Ya2 — o pan de 2 dedos MORTO (o midpoint não alimenta o pan) →
//           pan_dois_dedos_livre + regress_pan vermelhos
#include "TestFramework.h"
#include "ui/EditorUi.h"
#include "ui/Toolbar.h"
#include "ui/Theme.h"
#include "ui/SafeArea.h"
#include "ui/ViewportChrome.h"
#include "ui/Icons.h"
#include "ui/FontAtlas.h"          // 0.12-Y-fix: TextMetrics (o plano do inspector)
#include "render/Camera.h"
#include "render/Material.h"       // 0.12-Y-fix: o bind do uBaseColorTex
#include "render/SkyRenderer.h"    // 3.Y-D: as puras do céu + o quad fullscreen
#include "render/ParticleShader.h"    // 0.12-Y-F1: as puras fx:: (o M-Yf1)
#include "render/ParticleRenderer.h"  // 0.12-Y-F1: o draw real no stub
#include "components/ParticleEmitter.h"  // 0.12-Y-F1: o componente
#include "render/Texture.h"        // 0.12-Y-fix: a textura do pin do bind
#include "ui/UiEditor.h"           // 3.Y-D: commitTextInput (os hexes 13/14/15)
#include "render/QuadBatch.h"
#include "platform/InputState.h"
#include <GLES3/gl3.h>             // 0.12-Y-fix: o stub (stats + fb do bind)

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <utility>

using namespace vv;
using namespace vv::editor;

namespace {

bool nearEq(f32 a, f32 b, f32 eps = 0.05f) {
    return std::fabs(a - b) <= eps;
}

std::string repoRoot() {
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

// 3.Y-D: o stripper de comentários vive no TestFramework.h (test:: — o
// padrão partilhado; a M-Yd1 provou que a prosa não pode satisfazer o
// contrato de fonte)

// o env do draw do chrome (o padrão QolEnv do test_012qol — densidade 1)
struct YEnv {
    FontAtlas  font;
    UiContext  ui;
    InputState input;
    EditorState st;
    toolbar::GizmoModeState gz;
    vpchrome::ChromeState cs;
    Camera       cam;
    bool         ok = false;

    YEnv() {
        const char* fontPath = FONT_FIXTURE;
        ok = font.loadFromPaths(&fontPath, 1, 28.0f);
        if (!ok) {
            return;
        }
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
    }

    void begin() {
        ui.beginFrame(nullptr, &input, 1600.0f, 720.0f);
        ui.auditBegin("y", 1600.0f, 720.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    }
    void end() {
        ui.endFrame();
        input.clearEdges();
    }
};

} // namespace

// ---- PIN pan_camera_math ------------------------------------------------------
// A FONTE ÚNICA (render/Camera.h): worldPerPixel = 2·dist·tan(fovY/2)/viewH;
// right = (cos yaw, 0, −sin yaw); camUp = (−sp·sy, cp, −sp·cy). O pan AGARRA
// a cena: dedo → direita ⇒ target anda para a ESQUERDA; dedo ↓ ⇒ target SOBE.
TEST(pan_camera_math) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    // (1) a escala: dist 6, fov 60°, view 720 px → 2·6·tan(30°)/720
    const f32 wpp = camWorldPerPixel(6.0f, 1.0472f, 720.0f);
    EXPECT_MSG(nearEq(wpp, 2.0f * 6.0f * 0.57735f / 720.0f, 1e-5f),
               "pan_camera_math: worldPerPixel fora da fonte única (%.6f)",
               (double)wpp);
    // viewH degenerado → 0 (nunca divide por zero)
    EXPECT(camWorldPerPixel(6.0f, 1.0472f, 0.0f) == 0.0f);
    // (2) os eixos: right é HORIZONTAL e unitário; camUp ortogonal ao right
    // e com y=cos(pitch) (a pitch 0 aponta (0,1,0))
    const Vec3 r = camRightVec(0.6f);
    EXPECT(nearEq(r.y, 0.0f, 1e-6f));
    EXPECT(nearEq(std::sqrt(r.x * r.x + r.z * r.z), 1.0f, 1e-5f));
    const Vec3 u0 = camUpVec(0.0f, 0.0f);
    EXPECT(nearEq(u0.x, 0.0f, 1e-6f) && nearEq(u0.y, 1.0f, 1e-6f) &&
           nearEq(u0.z, 0.0f, 1e-6f));
    const Vec3 u = camUpVec(0.6f, 0.5f);
    EXPECT(nearEq(u.y, std::cos(0.5f), 1e-5f));
    EXPECT(nearEq(dot(r, u), 0.0f, 1e-5f));   // ortogonais no plano da câmara
    // (3) o Camera::pan AGARRA a cena (a direção do editor): yaw 0 → right
    // = (+1,0,0); pan(100, 0) → target.x DIMINUI (a cena segue o dedo para
    // a direita); pan(0, 100) → target.y SOBE (o y do ecrã cresce para baixo)
    Camera c;   // yaw 0.6, pitch 0.5, dist 6, target (0,0,0)
    const f32 dyaw = c.yaw, dpitch = c.pitch, ddist = c.dist;
    c.pan(100.0f, 0.0f, 720.0f);
    EXPECT_MSG(c.target.x < 0.0f,
               "pan_camera_math: pan para a direita tem de levar o target "
               "para a ESQUERDA (agarra a cena) — target.x %.4f",
               (double)c.target.x);
    EXPECT(nearEq(c.target.y, 0.0f, 1e-5f));
    c.pan(0.0f, 100.0f, 720.0f);
    EXPECT_MSG(c.target.y > 0.0f,
               "pan_camera_math: pan para baixo tem de levar o target para "
               "CIMA (o y do ecrã cresce para baixo) — target.y %.4f",
               (double)c.target.y);
    // (4) a MAGNITUDE é a fonte única: |Δ| = px·worldPerPixel (com yaw=0 o
    // right é o +x puro — a projeção no eixo é a própria magnitude)
    Camera c2;
    c2.yaw = 0.0f;
    c2.pan(100.0f, 0.0f, 720.0f);
    EXPECT(nearEq(-c2.target.x, 100.0f * camWorldPerPixel(6.0f, c2.fovY,
                                                          720.0f), 1e-6f));
    // (5) o pan NÃO toca na rotação/zoom
    EXPECT(nearEq(c.yaw, dyaw) && nearEq(c.pitch, dpitch) &&
           nearEq(c.dist, ddist));
    // (6) o viewH degenerado não mexe em nada
    Camera c3;
    c3.pan(500.0f, 500.0f, 0.0f);
    EXPECT(c3.target.x == 0.0f && c3.target.y == 0.0f && c3.target.z == 0.0f);
}

// ---- PIN pan_modo_um_dedo -----------------------------------------------------
// «Quando o modo Pan estiver ativo, 1 dedo a arrastar move a câmara
// (desloca o ponto de foco no plano da câmara).» — e NÃO orbita.
TEST(pan_modo_um_dedo) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    const UiRect view{0.0f, 0.0f, 1080.0f, 600.0f};
    const f32 wpp = camWorldPerPixel(6.0f, 1.0472f, 600.0f);

    // (1) panMode=true: 1 dedo → o target anda para a ESQUERDA (100 px);
    // o yaw/pitch ficam EXATAMENTE onde estavam. A câmara parte de yaw=0
    // (o right é o +x puro — a projeção no eixo é a magnitude limpa)
    {
        Camera cam;
        cam.yaw = 0.0f;
        cam.pitch = 0.0f;
        OrbitState st;
        InputState in;
        in.injectDown(0, 400.0f, 300.0f);
        updateCameraOrbit(cam, st, in, view, 0, false, /*panMode=*/true);
        in.injectMove(0, 500.0f, 300.0f);   // 100 px para a direita
        updateCameraOrbit(cam, st, in, view, 0, false, true);
        in.injectUp(0);
        updateCameraOrbit(cam, st, in, view, 0, false, true);
        EXPECT_MSG(nearEq(-cam.target.x, 100.0f * wpp, 1e-5f),
                   "pan_modo_um_dedo: o target não andou 100·wpp para a "
                   "esquerda (%.5f)", (double)cam.target.x);
        EXPECT(nearEq(cam.yaw, 0.0f, 1e-6f) && nearEq(cam.pitch, 0.0f,
                                                      1e-6f));
    }
    // (2) panMode=false (o comportamento de sempre, byte a byte): o MESMO
    // gesto orbita e NÃO mexe no target
    {
        Camera cam;
        cam.yaw = 0.0f;
        cam.pitch = 0.0f;
        OrbitState st;
        InputState in;
        in.injectDown(0, 400.0f, 300.0f);
        updateCameraOrbit(cam, st, in, view, 0, false, false);
        in.injectMove(0, 500.0f, 300.0f);
        updateCameraOrbit(cam, st, in, view, 0, false, false);
        in.injectUp(0);
        updateCameraOrbit(cam, st, in, view, 0, false, false);
        EXPECT_MSG(cam.target.x == 0.0f && cam.target.y == 0.0f &&
                       cam.target.z == 0.0f,
                   "pan_modo_um_dedo: sem panMode o target NÃO pode mexer "
                   "(%.4f,%.4f)", (double)cam.target.x, (double)cam.target.y);
        EXPECT(cam.yaw != 0.0f);   // a orbit aconteceu
    }
    // (3) o vertical: 1 dedo para baixo com panMode → target SOBE
    {
        Camera cam;
        OrbitState st;
        InputState in;
        in.injectDown(0, 400.0f, 300.0f);
        updateCameraOrbit(cam, st, in, view, 0, false, true);
        in.injectMove(0, 400.0f, 400.0f);   // 100 px para baixo
        updateCameraOrbit(cam, st, in, view, 0, false, true);
        EXPECT_MSG(cam.target.y > 0.0f,
                   "pan_modo_um_dedo: dedo para baixo tem de SUBIR o target "
                   "(%.4f)", (double)cam.target.y);
    }
}

// ---- PIN pan_dois_dedos_livre -------------------------------------------------
// «Adiciona o gesto de 2 dedos a arrastar para fazer Pan em qualquer
// momento (mesmo no modo Orbit), mantendo o Pinch para Zoom.»
TEST(pan_dois_dedos_livre) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    const UiRect view{0.0f, 0.0f, 1080.0f, 600.0f};
    const f32 wpp = camWorldPerPixel(6.0f, 1.0472f, 600.0f);

    // SEM panMode (o modo Mover de sempre): os 2 dedos arrastam o target
    // pelo midpoint E mantêm a distância (sem pinch → sem zoom). A câmara
    // parte de yaw=0 (o right é o +x puro — a projeção limpa)
    Camera cam;
    cam.yaw = 0.0f;
    cam.pitch = 0.0f;
    OrbitState st;
    InputState in;
    in.injectDown(0, 400.0f, 300.0f);
    in.injectDown(1, 500.0f, 300.0f);   // d=100, mid=(450,300)
    updateCameraOrbit(cam, st, in, view, 0, false, false);   // ancora
    in.injectMove(0, 450.0f, 300.0f);
    in.injectMove(1, 550.0f, 300.0f);   // d=100 (igual), mid=(500,300)
    updateCameraOrbit(cam, st, in, view, 0, false, false);
    in.injectUp(0);
    in.injectUp(1);
    updateCameraOrbit(cam, st, in, view, 0, false, false);
    // o midpoint andou 50 px para a direita → o target anda 50·wpp para a
    // ESQUERDA; a distância NÃO mudou (d constante)
    EXPECT_MSG(nearEq(-cam.target.x, 50.0f * wpp, 1e-5f),
               "pan_dois_dedos_livre: o midpoint não arrastou o target "
               "(%.5f vs %.5f)", (double)cam.target.x, (double)(-50.0f * wpp));
    EXPECT_MSG(nearEq(cam.dist, 6.0f, 1e-5f),
               "pan_dois_dedos_livre: d constante NÃO pode fazer zoom "
               "(dist %.5f)", (double)cam.dist);
    EXPECT(nearEq(cam.yaw, 0.0f, 1e-6f));   // 2 dedos NUNCA orbitam

    // o pan vertical: os 2 dedos descem 100 px → o target SOBE 100·wpp
    {
        Camera cam2;
        cam2.yaw = 0.0f;
        cam2.pitch = 0.0f;
        OrbitState st2;
        InputState in2;
        in2.injectDown(0, 400.0f, 200.0f);
        in2.injectDown(1, 500.0f, 200.0f);
        updateCameraOrbit(cam2, st2, in2, view, 0, false, false);
        in2.injectMove(0, 400.0f, 300.0f);
        in2.injectMove(1, 500.0f, 300.0f);
        updateCameraOrbit(cam2, st2, in2, view, 0, false, false);
        EXPECT(nearEq(cam2.target.y, 100.0f * wpp, 1e-5f));
    }
}

// ---- PIN pan_pinch_intacto ----------------------------------------------------
// O pinch de sempre fica INTACTO: os dedos afastam-se → aproxima (a distância
// encolhe pela razão); o midpoint parado → NADA de pan.
TEST(pan_pinch_intacto) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    const UiRect view{0.0f, 0.0f, 1080.0f, 600.0f};
    Camera cam;
    OrbitState st;
    InputState in;
    in.injectDown(0, 400.0f, 300.0f);
    in.injectDown(1, 500.0f, 300.0f);   // d=100
    updateCameraOrbit(cam, st, in, view, 0, false, false);
    in.injectMove(0, 350.0f, 300.0f);
    in.injectMove(1, 550.0f, 300.0f);   // d=200, mid=(450,300) IGUAL
    updateCameraOrbit(cam, st, in, view, 0, false, false);
    // o zoom: dist 6 · (100/200) = 3
    EXPECT_MSG(nearEq(cam.dist, 3.0f, 1e-4f),
               "pan_pinch_intacto: o pinch tem de manter o zoom de sempre "
               "(dist %.5f)", (double)cam.dist);
    // o midpoint parado → o target não mexe
    EXPECT_MSG(cam.target.x == 0.0f && cam.target.y == 0.0f &&
                   cam.target.z == 0.0f,
               "pan_pinch_intacto: midpoint parado NÃO pode panear "
               "(%.4f,%.4f)", (double)cam.target.x, (double)cam.target.y);
    // e o 1.º frame de 2 dedos NUNCA salta (só ancora): gesto NOVO com os
    // dedos já afastados não mexe no target no 1.º frame
    {
        Camera cam2;
        OrbitState st2;
        InputState in2;
        in2.injectDown(0, 100.0f, 100.0f);
        in2.injectDown(1, 900.0f, 100.0f);
        updateCameraOrbit(cam2, st2, in2, view, 0, false, false);
        EXPECT(cam2.target.x == 0.0f && cam2.target.y == 0.0f);
        EXPECT(nearEq(cam2.dist, 6.0f, 1e-6f));   // sem zoom no 1.º frame
    }
}

// ---- PIN pan_rail_sexta (recalibrado FASE UI D) --------------------------------
// A MÃOZINHA e o ÍMAN desceram a CHIPS sob o rail (a ordem do dono na
// FASE UI D: a coluna é das 4 ferramentas 30-36dp); os MESMOS ids
// (kVpPanId 33 — o 38 aposentado NUNCA volta; kVpSnapValId 36); o toque
// arma o st.panMode; a legenda diz "Pan".
TEST(pan_rail_sexta) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    // (1) a geometria: o rail é a coluna das 4 FERRAMENTAS; os CHIPS (mão
    // + íman) ficam SOB o railPanel — o Pan é chips[0] (id 33)
    const UiRect view{0.0f, 0.0f, 800.0f, 600.0f};
    const vpchrome::Layout L = vpchrome::layout(view);
    ASSERT(L.railVisible);
    EXPECT_MSG(L.chips[0].w > 0.0f,
               "pan_rail_sexta: os chips existem (a mão + o íman)");
    EXPECT_MSG(L.chips[0].y > L.rail[3].y,
               "pan_rail_sexta: o Pan (chip) fica SOB o Escalar (rail[3])");
    EXPECT_MSG(nearEq(L.chips[0].w, theme::dp(vpchrome::kRailBtn)),
               "pan_rail_sexta: o chip mantém o alvo de toque 40dp");
    EXPECT_MSG(nearEq(L.chips[1].y - L.chips[0].y,
                      theme::dp(vpchrome::kRailBtn + vpchrome::kRailGap)),
               "pan_rail_sexta: o Íman desceu para chips[1]");
    EXPECT(vpchrome::kVpPanId == 33u);   // o 38 aposentado NUNCA volta
    // a coluna inteira (rail + chips) cabe no view
    EXPECT(L.chips[1].y + L.chips[1].h <= view.y + view.h + 0.5f);
    // (2) viewport BAIXO: 4 ferramentas cabem numa coluna a 220dp
    // (4×40+3×8 = 184 ≤ 220−16 — o railCols fica 1; a degradação honesta)
    {
        const vpchrome::Layout L2 = vpchrome::layout(UiRect{0.0f, 0.0f, 800.0f, 220.0f});
        EXPECT(L2.railVisible);
        EXPECT(L2.rail[3].w > 0.0f);   // as 4 ferramentas na coluna
        // os chips morrem se a altura não chegar (o rail tem prioridade)
        EXPECT(L2.chips[0].w == 0.0f);   // 184+8+88 = 280 > 204 disponíveis
    }
    // (3) o glifo Hand existe no conjunto e respeita o viewBox (o pin
    // icones_conjunto_completo_dentro_do_viewbox cobre-o automaticamente)
    const icons::IconDef& hd = icons::def(icons::Icon::Hand);
    EXPECT(hd.nPts >= 2 && hd.nLines >= 1);
    // (4) a FONTE: o draw arma o panMode no toque E a legenda diz "Pan"
    std::string src;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/ui/ViewportChrome.cpp",
                    src));
    EXPECT(countSub(src, "icons::Icon::Hand") >= 1);
    EXPECT(countSub(src, "\"Pan\"") >= 1);
    // (5) o toque: carregar no rail[4] arma o st.panMode (e desarma o
    // selectMode); carregar no Mover desarma-o
    {
        YEnv e;
        ASSERT(e.ok);
        e.begin();
        vpchrome::draw(e.ui, e.st, e.gz, e.cs, e.cam, 0.0f, 0.016f,
                       &e.input);
        e.end();
        // acha o Pan no audit (o widgetHit registra o Button)
        f32 px = 0, py = 0, pw = 0, ph = 0;
        bool pan = false;
        for (const auto& en : e.ui.auditRecord().entries) {
            if (en.kind == layout::Entry::Button && en.id == vpchrome::kVpPanId) {
                pan = true;
                px = en.x; py = en.y; pw = en.w; ph = en.h;
            }
        }
        ASSERT(pan);   // o botão Pan tem de desenhar (id kVpPanId)
        e.st.selectMode = true;   // vem do Selecionar (exclusividade)
        // o clique da casa dispara no LARGAR (o padrão down→frame→up→frame)
        e.input.injectDown(0, px + pw * 0.5f, py + ph * 0.5f);
        e.begin();
        vpchrome::draw(e.ui, e.st, e.gz, e.cs, e.cam, 0.0f, 0.016f,
                       &e.input);
        e.end();
        e.input.injectUp(0);
        e.begin();
        vpchrome::draw(e.ui, e.st, e.gz, e.cs, e.cam, 0.0f, 0.016f,
                       &e.input);
        e.end();
        EXPECT_MSG(e.st.panMode,
                   "pan_rail_sexta: o toque na MÃOZINHA tem de armar o panMode");
        EXPECT(!e.st.selectMode);
        // o Mover desarma o pan (os modos são exclusivos)
        f32 mx = 0, my = 0, mw = 0, mh = 0;
        bool mov = false;
        for (const auto& en : e.ui.auditRecord().entries) {
            if (en.kind == layout::Entry::Button &&
                en.id == toolbar::kGizmoIds[0]) {
                mov = true;
                mx = en.x; my = en.y; mw = en.w; mh = en.h;
            }
        }
        ASSERT(mov);
        e.input.injectDown(0, mx + mw * 0.5f, my + mh * 0.5f);
        e.begin();
        vpchrome::draw(e.ui, e.st, e.gz, e.cs, e.cam, 0.0f, 0.016f,
                       &e.input);
        e.end();
        e.input.injectUp(0);
        e.begin();
        vpchrome::draw(e.ui, e.st, e.gz, e.cs, e.cam, 0.0f, 0.016f,
                       &e.input);
        e.end();
        EXPECT_MSG(!e.st.panMode,
                   "pan_rail_sexta: o Mover tem de desarmar o panMode");
    }
}

// ---- PIN icon_ia_mipmaps (0.12-Y B) -------------------------------------------
// O ÍCONE GERADO POR IA (obsidiana + magma): a FONTE vive no repo
// (docs/iconegonevv-ai.png — o prompt EXATO do dono), o gerador é o
// scripts/y_gen_ai_icon.py (a safe zone 65% do contrato Y-0-FIX), os 5
// mipmaps × 3 ficheiros têm as dimensões EXATAS por densidade, o
// manifest aponta @mipmap/ic_launcher e o fundo do adaptativo é
// #0A0A0C (a ordem do dono, supera o #08080A da Y-0).
TEST(icon_ia_mipmaps) {
    // (1) as DIMENSÕES exatas dos 15 PNGs (o header IHDR: w nos bytes
    // 16..19, h nos 20..23, big-endian — parse puro, sem zlib). O LEGACY
    // (ic_launcher/round) usa o lado do launcher 48..192; o FOREGROUND
    // usa o canvas adaptativo 108 dp × densidade (108..432) — o mesmo
    // par da DENS do gerador.
    struct D { const char* dir; u32 legacy; u32 fg; };
    const D dens[] = {{"mdpi", 48u, 108u}, {"hdpi", 72u, 162u},
                      {"xhdpi", 96u, 216u}, {"xxhdpi", 144u, 324u},
                      {"xxxhdpi", 192u, 432u}};
    for (const D& d : dens) {
        const std::pair<const char*, u32> files[3] = {
            {"ic_launcher.png", d.legacy},
            {"ic_launcher_round.png", d.legacy},
            {"ic_launcher_foreground.png", d.fg}};
        for (const auto& ff : files) {
            const char* f = ff.first;
            const u32 side = ff.second;
            const std::string p = repoRoot() + "/app/src/main/res/mipmap-" +
                                  d.dir + "/" + f;
            std::string png;
            if (!readFile(p, png)) png.clear();
            EXPECT_MSG(!png.empty(),
                       "icon_ia_mipmaps: falta %s", p.c_str());
            ASSERT(png.size() > 24);   // o header IHDR cabe
            EXPECT_MSG(png.compare(0, 8, "\x89PNG\r\n\x1a\n") == 0,
                       "icon_ia_mipmaps: %s não é PNG", f);
            const u32 w = (u32((unsigned char)png[16]) << 24) |
                          (u32((unsigned char)png[17]) << 16) |
                          (u32((unsigned char)png[18]) << 8) |
                          u32((unsigned char)png[19]);
            const u32 h = (u32((unsigned char)png[20]) << 24) |
                          (u32((unsigned char)png[21]) << 16) |
                          (u32((unsigned char)png[22]) << 8) |
                          u32((unsigned char)png[23]);
            EXPECT_MSG(w == side && h == side,
                       "icon_ia_mipmaps: %s/%s é %ux%u (esperado %ux%u)",
                       d.dir, f, w, h, side, side);
        }
    }
    // (2) a FONTE IA vive no repo (o padrão D-3/Y-0: a fonte é história)
    std::string src;
    ASSERT(readFile(repoRoot() + "/docs/iconegonevv-ai.png", src));
    EXPECT(src.size() > 10000);   // um PNG real, não um placeholder
    // (3) o GERADOR: a safe zone 65% do contrato Y-0-FIX + o fundo novo
    ASSERT(readFile(repoRoot() + "/scripts/y_gen_ai_icon.py", src));
    EXPECT(countSub(src, "TILE_FILL = 0.65") == 1);
    EXPECT(countSub(src, "SAFE_ZONE_DP = 66.0") == 1);
    EXPECT(countSub(src, "(10, 10, 12)") == 1);   // o #0A0A0C
    // (4) o MANIFEST aponta o ícone (o caminho do launcher)
    ASSERT(readFile(repoRoot() + "/app/src/main/AndroidManifest.xml", src));
    EXPECT(countSub(src, "@mipmap/ic_launcher") >= 1);
    // (5) o fundo do adaptativo é #0A0A0C (a ordem do dono na 0.12-Y)
    ASSERT(readFile(repoRoot() +
                    "/app/src/main/res/values/ic_launcher_background.xml",
                    src));
    EXPECT(countSub(src, "<color name=\"ic_launcher_background\">"
                         "#0A0A0C</color>") == 1);
    // o Y-0 SUPERADO na LINHA da cor (a menção histórica no comentário
    // fica — é a nota da superação)
    EXPECT(countSub(src, "ic_launcher_background\">#08080A") == 0);
    // (6) o adaptativo compõe o foreground
    ASSERT(readFile(repoRoot() +
                    "/app/src/main/res/mipmap-anydpi-v26/ic_launcher.xml",
                    src));
    EXPECT(countSub(src, "@mipmap/ic_launcher_foreground") == 1);
}

// ---- (0.12-Y C) — os pins do PBR e do WorldEnvironment ---------------------
#include "render/Pbr.h"
#include "components/Environment.h"
#include "assets/GltfTextures.h"
#include "assets/GltfImporter.h"
#include "core/SceneSerializer.h"
#include "core/Presets.h"
#include "FakeStorage.h"

// ---- PIN pbr_fontes -----------------------------------------------------------
// As FONTE ÚNICA do lit PBR-lite (render/Pbr.h): o shininess do roughness,
// o especular Blinn-Phong, o mix metálico, a névoa exp2 e o uv equirect —
// o FS replica LINHA A LINHA (os mesmos números).
TEST(pbr_fontes) {
    // o shininess: rough 0 = 256 (quase-espelho), rough 1 = 4 (o piso),
    // rough 0.5 = 4 + 0.25·252 = 67 (o quadrado suaviza)
    EXPECT(nearEq(pbr::roughToShininess(0.0f), 256.0f, 0.01f));
    EXPECT(nearEq(pbr::roughToShininess(1.0f), 4.0f, 0.01f));
    EXPECT(nearEq(pbr::roughToShininess(0.5f), 67.0f, 0.01f));
    EXPECT(nearEq(pbr::roughToShininess(2.0f), 4.0f, 0.01f));   // clamp
    // o especular: N=H → 1; perpendicular → 0
    const Vec3 n{0.0f, 0.0f, 1.0f};
    EXPECT(nearEq(pbr::blinnSpec(n, n, 32.0f), 1.0f, 1e-5f));
    const Vec3 h90{1.0f, 0.0f, 0.0f};
    EXPECT(pbr::blinnSpec(n, h90, 32.0f) == 0.0f);
    EXPECT(pbr::blinnSpec(n, Vec3{-1.0f, 0.0f, 0.0f}, 32.0f) == 0.0f);
    // o mix metálico: dielétrico 1.0, metal 0.15
    EXPECT(nearEq(pbr::metallicDiffuseScale(0.0f), 1.0f, 1e-6f));
    EXPECT(nearEq(pbr::metallicDiffuseScale(1.0f), 0.15f, 1e-6f));
    // a névoa exp2: density 0 = MORTA (o byte a byte do lit de sempre);
    // d·ρ=1 → 1−e⁻¹ ≈ 0.632
    EXPECT(pbr::fogFactorExp2(100.0f, 0.0f) == 0.0f);
    EXPECT(nearEq(pbr::fogFactorExp2(1.0f, 1.0f), 0.6321f, 1e-3f));
    EXPECT(nearEq(pbr::fogFactorExp2(10.0f, 0.1f), 0.6321f, 1e-3f));
    // o uv equirect: +x → (0.5, 0.5) · −y (o TOPO da imagem) → v=0 ·
    // +z → u=0.75
    f32 uv[2];
    pbr::equirectUv(Vec3{1.0f, 0.0f, 0.0f}, uv);
    EXPECT(nearEq(uv[0], 0.5f, 1e-5f) && nearEq(uv[1], 0.5f, 1e-5f));
    pbr::equirectUv(Vec3{0.0f, -1.0f, 0.0f}, uv);
    EXPECT(nearEq(uv[1], 0.0f, 1e-5f));
    pbr::equirectUv(Vec3{0.0f, 0.0f, 1.0f}, uv);
    EXPECT(nearEq(uv[0], 0.75f, 1e-5f));
}

// ---- PIN env_media ----------------------------------------------------------
// O WorldEnvironment ilumina pela MÉDIA CPU do HDRI (averageColor +
// ambientFromAvg — as fontes puras do components/Environment.h).
TEST(env_media) {
    // vazio/zero → preto (sem crash)
    f32 avg[3];
    averageColor(nullptr, 0, 0, avg);
    EXPECT(avg[0] == 0.0f && avg[1] == 0.0f && avg[2] == 0.0f);
    // 1×1 vermelho puro → (1,0,0) → a luminância 0.2126 → ambient com o
    // ganho ×4 (0.85) — o piso 0.25 não ativa
    const u8 px[4] = {255, 0, 0, 255};
    averageColor(px, 1, 1, avg);
    EXPECT(nearEq(avg[0], 1.0f, 1e-6f) && avg[1] == 0.0f && avg[2] == 0.0f);
    EXPECT(nearEq(ambientFromAvg(avg), 0.2126f * 4.0f, 1e-3f));
    // branco → luminância 1 → o teto 4
    const u8 w[4] = {255, 255, 255, 255};
    averageColor(w, 1, 1, avg);
    EXPECT(ambientFromAvg(avg) == 4.0f);
    // preto → o PISO 0.25 (o HDRI escuro nunca apaga a cena)
    const u8 b[4] = {0, 0, 0, 255};
    averageColor(b, 1, 1, avg);
    EXPECT(ambientFromAvg(avg) == 0.25f);
    // 2×2 misturado → a média dos 4 (o passo cap 4096 amostra tudo)
    const u8 q[16] = {255, 255, 255, 255, 0,   0,   0,   255,
                      255, 255, 255, 255, 0,   0,   0,   255};
    averageColor(q, 2, 2, avg);
    EXPECT(nearEq(avg[0], 0.5f, 1e-6f));
}

// ---- PIN gltf_5slots_pasta ---------------------------------------------------
// O dono: «Lê o material do glTF e extrai Albedo, Normal, Roughness e
// Metallic. Cria a pasta assets/NomeDoModelo/ e coloca lá o .gmesh e uma
// subpasta textures/ com as imagens.» — a EXTRAÇÃO dos 5 slots na pasta
// do modelo (o .gmesh na pasta é o AssetConverter — o pin aferra a fonte).
TEST(gltf_5slots_pasta) {
    FakeStorage st;
    const std::vector<u8> pngA = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
    // um PNG real do fixture (o extrator valida o decode)
    std::string fix = repoRoot() +
                      "/tests/fixtures/yellow4.png";
    std::string pngBytes;
    ASSERT(readFile(fix, pngBytes));
    const std::vector<u8> png(pngBytes.begin(), pngBytes.end());

    // o modelo SINTÉTICO: 2 meshes · 2 materiais · 3 imagens (o mesh 0 com
    // os 5 slots, o mesh 1 só baseColor)
    GltfModel model;
    MeshData mesh;
    model.meshes.push_back(mesh);
    model.meshes.push_back(mesh);
    model.meshMaterial.push_back(0);
    model.meshMaterial.push_back(1);
    GltfMaterial m0;
    m0.baseColorTex = 0;
    m0.normalTex = 1;
    m0.metalRoughTex = 2;
    GltfMaterial m1;   // só baseColor
    m1.baseColorTex = 0;
    model.materials.push_back(m0);
    model.materials.push_back(m1);
    for (int i = 0; i < 3; ++i) {
        GltfImage img;
        img.bytes = png;
        img.mime = "image/png";
        model.images.push_back(img);
    }

    GltfTexSlots slots;
    std::string err;
    EXPECT(extractGltfTexSlots(st, model, "cidade", slots, err));
    EXPECT(err.empty());
    EXPECT(slots.meshes == 2u);
    ASSERT(slots.tex.size() == 10u);   // 2 meshes × 5 slots
    // o mesh 0: os 4 slots preenchidos (baseColor/normal/metalRough; o
    // emissive/occlusion sem imagem = vazios) — TODOS na PASTA do modelo
    for (int s = 0; s < 3; ++s) {
        const std::string& rel = slots.tex[static_cast<size_t>(s)];
        EXPECT_MSG(rel.rfind("assets/cidade/textures/gltf_", 0) == 0,
                   "gltf_5slots_pasta: o slot %d saiu da PASTA DO MODELO "
                   "(%s)", s, rel.c_str());
        EXPECT(st.exists(rel));
    }
    EXPECT(slots.tex[3].empty() && slots.tex[4].empty());
    // o mesh 1: só o baseColor
    EXPECT(!slots.tex[5].empty());
    for (int s = 1; s < 5; ++s) {
        EXPECT(slots.tex[5 + static_cast<size_t>(s)].empty());
    }
    // o DEDUP: as 3 imagens IGUAIS → 1 ficheiro no disco
    std::vector<std::string> files;
    EXPECT(st.listDir("assets/cidade/textures", files));
    EXPECT(files.size() == 1u);
    // o stem vazio → a pasta "modelo" (nunca a raiz)
    GltfTexSlots slots2;
    EXPECT(extractGltfTexSlots(st, model, "", slots2, err));
    EXPECT(!slots2.tex.empty() &&
           slots2.tex[0].rfind("assets/modelo/textures/", 0) == 0);
    // a FONTE: as texturas nascem na PASTA do modelo (a ordem do dono);
    // o .gmesh continua em assets/<stem>.gmesh — a subpasta dele é a
    // campanha própria (a onda de recalibração não cabe na 3.Y-C)
    std::string src;
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/assets/GltfTextures.cpp",
                    src));
    EXPECT(countSub(src, "assets/\" +\n        (modelStem.empty()") >= 1 ||
           countSub(src, "\"/textures\"") >= 1);
    ASSERT(readFile(repoRoot() + "/app/src/main/cpp/assets/GOwnFormats.h",
                    src));
    EXPECT(countSub(src, "kBindBaseColor = 0") == 1);   // a ordem dos slots
}

// ---- PIN env_roundtrip -------------------------------------------------------
// O TIC WorldEnvironment sobrevive ao .goni: o dump/loadText preserva
// sky/fog/density/ambient/hdri/active (o serializer novo).
TEST(env_roundtrip) {
    Scene a;
    const Handle h = a.create("WorldEnvironment");
    a.get(h)->addComponent<Transform3D>();
    Environment* e = a.get(h)->addComponent<Environment>();
    e->skyColor[0] = 0.30f;
    e->skyColor[2] = 0.60f;
    e->fogColor[1] = 0.40f;
    e->fogDensity = 0.08f;
    e->ambient = 1.75f;
    e->hdriPath = "assets/hdri/atardecer.png";
    e->clampFields();
    const std::string dump = SceneSerializer::dump(a);
    EXPECT(countSub(dump, "\"Environment\"") == 1);
    EXPECT(countSub(dump, "atardecer.png") == 1);

    Scene b;
    SceneSerializer::LoadCtx ctx;   // defaults (paths vazios)
    ASSERT(SceneSerializer::loadText(b, dump, ctx));
    const Handle hf = b.find("WorldEnvironment");
    ASSERT(hf.valid());
    const Tic* t = b.get(hf);
    ASSERT(t != nullptr);
    const Environment* e2 = t->getComponent<Environment>();
    ASSERT(e2 != nullptr);
    EXPECT(nearEq(e2->skyColor[0], 0.30f, 1e-5f));
    EXPECT(nearEq(e2->skyColor[2], 0.60f, 1e-5f));
    EXPECT(nearEq(e2->fogColor[1], 0.40f, 1e-5f));
    EXPECT(nearEq(e2->fogDensity, 0.08f, 1e-6f));
    EXPECT(nearEq(e2->ambient, 1.75f, 1e-6f));
    EXPECT(e2->hdriPath == "assets/hdri/atardecer.png");
    // o preset NOVO existe (a ordem do dono: «novo preset no "+"»)
    EXPECT(std::string(presetName(PresetKind::WorldEnvironment)) ==
           "WorldEnvironment");
    Scene c;
    const Handle hp = createTicFromPreset(c, PresetKind::WorldEnvironment,
                                          nullptr, nullptr);
    const Tic* tp = c.get(hp);
    ASSERT(tp != nullptr);
    EXPECT(tp->getComponent<Environment>() != nullptr);
    EXPECT(tp->getComponent<MeshRenderer>() == nullptr);   // ESTRUTURA
    EXPECT(tp->getComponent<BodyComp>() == nullptr);       // SEM física
}

// ---- PIN inspector_worldenv (0.12-Y-fix) ------------------------------------
// A ordem do dono: «o Inspector só mostra Transform, Animação e Script» —
// a secção Environment com os 5 controlos TEM de aparecer no TIC que tem
// o componente WorldEnvironment. O plano do Inspector É a fonte do layout
// (o draw consome-o linha a linha — o pin aferre a FONTE).
TEST(inspector_worldenv) {
    Scene scene;
    const Handle h = scene.create("we");
    scene.get(h)->addComponent<Transform3D>();
    scene.get(h)->addComponent<Environment>();
    const Tic* t = scene.get(h);
    ASSERT(t != nullptr);
    const InspProfile prof = inspectorProfile(*t);
    EXPECT(prof.env);
    EXPECT(!prof.mr);   // o preset WorldEnvironment NÃO inventa Malha

    const TextMetrics tm{};
    InspRow rows[64];
    const u32 n = inspectorPlan(prof, tm, false, 0, rows);
    u32 sec = 0, sky = 0, hdri = 0, amb = 0, fogd = 0, fogc = 0;
    i32 iSky = -1, iHdri = -1, iAmb = -1, iFogd = -1, iFogc = -1;
    for (u32 i = 0; i < n; ++i) {
        const InspRow& r = rows[i];
        if (r.kind == InspRow::Kind::Section &&
            r.payload == kInspBitEnvironment) {
            ++sec;
        }
        if (r.id == kInspectorEnvSky) {
            ++sky;
            iSky = static_cast<i32>(i);
        }
        if (r.id == kInspectorEnvHdri) {
            ++hdri;
            iHdri = static_cast<i32>(i);
        }
        if (r.id == kInspectorEnvAmb) {
            ++amb;
            iAmb = static_cast<i32>(i);
        }
        if (r.id == kInspectorEnvFogD) {
            ++fogd;
            iFogd = static_cast<i32>(i);
        }
        if (r.id == kInspectorEnvFogC) {
            ++fogc;
            iFogc = static_cast<i32>(i);
        }
    }
    EXPECT_MSG(sec == 1, "esperava 1 cabecalho Environment, vi %u", sec);
    EXPECT_MSG(sky == 1 && hdri == 1 && amb == 1 && fogd == 1 && fogc == 1,
               "as 5 linhas do Environment: sky=%u hdri=%u amb=%u fogd=%u "
               "fogc=%u",
               sky, hdri, amb, fogd, fogc);
    // a ORDEM do dono: céu → hdri → ambiente → névoa → cor da névoa
    EXPECT(iSky >= 0 && iSky < iHdri && iHdri < iAmb && iAmb < iFogd &&
           iFogd < iFogc);
    // os ids NÃO colidem com a faixa da câmara (5400..5412) nem entre si
    EXPECT(kInspectorEnvSky != kInspectorEnvHdri &&
           kInspectorEnvHdri != kInspectorEnvAmb &&
           kInspectorEnvAmb != kInspectorEnvFogD &&
           kInspectorEnvFogD != kInspectorEnvFogC);

    // COLAPSADA: o bit novo esconde as 5 linhas e deixa o cabeçalho
    const u32 nc = inspectorPlan(prof, tm, false, kInspBitEnvironment, rows);
    u32 secC = 0, skyC = 0;
    for (u32 i = 0; i < nc; ++i) {
        const InspRow& r = rows[i];
        if (r.kind == InspRow::Kind::Section &&
            r.payload == kInspBitEnvironment) {
            ++secC;
        }
        if (r.id == kInspectorEnvSky) {
            ++skyC;
        }
    }
    EXPECT_MSG(secC == 1 && skyC == 0,
               "colapsada: cabecalho=%u linhas de ceu=%u (0 esperado)", secC,
               skyC);
    // o conteúdo aberto é MAIS alto que o colapsado (o scroll não corta)
    EXPECT(inspectorContentHeight(prof, tm, false, 0) >
           inspectorContentHeight(prof, tm, false, kInspBitEnvironment));

    // o TIC SEM Environment NÃO ganha a secção (o Transform de sempre)
    Scene s2;
    const Handle h2 = s2.create("plano");
    s2.get(h2)->addComponent<Transform3D>();
    const InspProfile p2 = inspectorProfile(*s2.get(h2));
    EXPECT(!p2.env);
    InspRow rows2[64];
    const u32 n2 = inspectorPlan(p2, tm, false, 0, rows2);
    for (u32 i = 0; i < n2; ++i) {
        EXPECT(!(rows2[i].kind == InspRow::Kind::Section &&
                 rows2[i].payload == kInspBitEnvironment));
    }
}

// ---- PIN pbr_texture_bind (0.12-Y-fix) --------------------------------------
// A ordem do dono: «garante que o bind() do shader envia o uBaseColorTex
// para o slot correto da GPU». O pin corre o LitMaterial REAL no stub fb:
// o uniform existe PELO NOME do contrato, setTexture(tex) liga uHasTex=1,
// mete o sampler no UNIT 0 (o slot baseColor) e binda a textura;
// setTexture(nullptr) desliga SEM bind (o lit de sempre byte a byte).
TEST(pbr_texture_bind) {
    // O CONTRATO NO FONTE (o fb auto-regista QUALQUER nome consultado —
    // a leitura do Material.cpp É a verdade do nome; a mutação do nome
    // morre AQUI antes de qualquer GL)
    std::string msrc;
    ASSERT(readFile(repoRoot() +
                        "/app/src/main/cpp/render/Material.cpp",
                    msrc));
    EXPECT_MSG(countSub(msrc, "uniform sampler2D uBaseColorTex;") >= 1,
               "o FS nao declara o sampler uBaseColorTex (o contrato do "
               "slot 0)");
    EXPECT_MSG(countSub(msrc, "glGetUniformLocation(prog_, "
                              "\"uBaseColorTex\")") >= 1,
               "o init nao consulta uBaseColorTex pelo nome");

    glstub::fb::enabled = true;
    glstub::fb::resetState();
    glstub::reset();

    LitMaterial mat;
    ASSERT(mat.init());
    mat.use();   // o programa do lit fica corrente no stub

    // o CONTRATO: o uniform uBaseColorTex EXISTE no programa (o albedo tem
    // o nome do slot 0 — o retro-guarda uTex só entra se o nome morrer)
    const GLint locBase =
        glGetUniformLocation(glstub::fb::curProg_, "uBaseColorTex");
    EXPECT_MSG(locBase >= 0,
               "uBaseColorTex nao existe no programa do lit (loc=%d)",
               static_cast<int>(locBase));

    // a textura 2x2 (o RGBA direto — o caminho curto da casa)
    Texture tex;
    const u8 px[2 * 2 * 4] = {255, 64, 32, 255, 255, 64, 32, 255,
                              255, 64, 32, 255, 255, 64, 32, 255};
    ASSERT(tex.createFromRGBA(px, 2, 2));
    ASSERT(tex.ok());

    glstub::reset();   // os contadores a partir do setTexture
    mat.setTexture(&tex);
    EXPECT_MSG(glstub::stats.lastUniform1f == 1.0f,
               "uHasTex devia ser 1.0, foi %.2f", glstub::stats.lastUniform1f);
    EXPECT_MSG(glstub::stats.lastUniform1i == 0,
               "o sampler uBaseColorTex devia ir ao unit 0, foi %d",
               glstub::stats.lastUniform1i);
    EXPECT_MSG(glstub::stats.lastBoundTexture == tex.handle(),
               "o bind de textura devia ser a do albedo (%u), foi %u",
               tex.handle(), glstub::stats.lastBoundTexture);
    EXPECT(glstub::stats.boundTextures == 1);

    // nullptr = o caminho de sempre: uHasTex 0 e NENHUM bind (o lit F2)
    glstub::reset();
    mat.setTexture(nullptr);
    EXPECT(glstub::stats.lastUniform1f == 0.0f);
    EXPECT(glstub::stats.boundTextures == 0);

    // os SAMPLERS do PBR nunca colidem: normal no 1, rough/metal no 2 (o
    // setPbr liga os seus units SEM tocar no albedo — o mesmo draw)
    Texture nrm;
    const u8 npx[2 * 2 * 4] = {};
    ASSERT(nrm.createFromRGBA(npx, 2, 2));
    Texture rmt;
    ASSERT(rmt.createFromRGBA(npx, 2, 2));
    glstub::reset();
    mat.setPbr(&nrm, &rmt, 0.5f, 0.5f);
    mat.setTexture(&tex);   // o albedo volta ao 0 DEPOIS do setPbr
    EXPECT_MSG(glstub::stats.lastBoundTexture == tex.handle(),
               "o albedo manda no unit 0 mesmo com PBR ativo (%u)",
               glstub::stats.lastBoundTexture);

    mat.destroy();
    tex.destroy();
    nrm.destroy();
    rmt.destroy();
    glstub::fb::enabled = false;
    glstub::fb::resetState();
}

// ---- PIN env_sky_type (0.12-Y 3.Y-D) -----------------------------------------
// A ordem do dono: «O COMPONENTE ENVIRONMENT adiciona as propriedades
// skyType/skyColorHorizon/skyColorZenith/sunColor/sunDirection/
// cloudCoverage/cloudSpeed» + «env_sky_type: a serialização do .goni guarda
// e carrega o skyType e as propriedades das nuvens». As puras do céu (o
// padrão Pbr.h — o FS replica LINHA A LINHA) + o componente + o roundtrip +
// o CONTRATO NO FONTE + o draw REAL no stub (o uTime é a alma — M-Yd1).
TEST(env_sky_type) {
    // (1) AS PURAS — o gradiente (o horizonte e o zénite nos extremos)
    {
        const f32 hor[3] = {0.29f, 0.56f, 0.89f};
        const f32 zen[3] = {0.12f, 0.23f, 0.54f};
        f32 out[3];
        sky::skyGradient(0.0f, hor, zen, out);
        EXPECT_MSG(out[0] == hor[0] && out[1] == hor[1] && out[2] == hor[2],
                   "env_sky_type: dir.y=0 devia ser a cor do HORIZONTE");
        sky::skyGradient(1.0f, hor, zen, out);
        EXPECT_MSG(nearEq(out[0], zen[0], 1e-5f) &&
                       nearEq(out[1], zen[1], 1e-5f) &&
                       nearEq(out[2], zen[2], 1e-5f),
                   "env_sky_type: dir.y=1 devia ser a cor do ZÉNITE");
        // 0.12-Z2 F2: abaixo do horizonte o céu escurece para o TOM DE
        // CHÃO (o horizonte × kGroundDim no fundo da transição) — a «zona
        // azul fixa que corta a grelha» do dono morreu (o ACHADO da FASE 0
        // no GLES REAL, captura B5_we_meianoite.png); em dir.y=0 é ainda o
        // HORIZONTE (a linha do horizonte fica onde sempre esteve)
        sky::skyGradient(0.0f, hor, zen, out);
        EXPECT_MSG(out[1] == hor[1],
                   "env_sky_type: dir.y=0 é o HORIZONTE (a linha do "
                   "horizonte não mexeu)");
        sky::skyGradient(-0.5f, hor, zen, out);   // abaixo do horizonte
        EXPECT_MSG(out[1] < hor[1] - 0.01f,
                   "env_sky_type: abaixo do horizonte ESCURECE (o tom de "
                   "chão — a zona azul fixa morreu; kGroundDim=0.45)");
        {
            f32 deep[3];
            sky::skyGradient(-10.0f, hor, zen, deep);
            EXPECT_MSG(nearEq(deep[0], hor[0] * 0.45f, 1e-5f) &&
                           nearEq(deep[1], hor[1] * 0.45f, 1e-5f) &&
                           nearEq(deep[2], hor[2] * 0.45f, 1e-5f),
                       "env_sky_type: o fundo do chão é horizonte×0.45 "
                       "(kGroundDim)");
        }
        sky::skyGradient(0.5f, hor, zen, out);
        EXPECT_MSG(nearEq(out[0], (hor[0] + zen[0]) * 0.5f, 1e-5f),
                   "env_sky_type: o meio do gradiente é o MIX 0.5");
        // o sol: o disco e o halo (os limites)
        EXPECT_MSG(sky::sunDisk(1.0f) == 1.0f,
                   "env_sky_type: o centro do sol é o DISCO cheio");
        EXPECT_MSG(sky::sunDisk(0.9f) == 0.0f,
                   "env_sky_type: fora do disco não há disco");
        EXPECT_MSG(nearEq(sky::sunGlow(1.0f), sky::kSunGlowGain, 1e-5f),
                   "env_sky_type: o halo no centro vale o ganho");
        EXPECT_MSG(sky::sunGlow(0.0f) == 0.0f,
                   "env_sky_type: o halo morre a 90° do sol");
        // o fade das nuvens no horizonte
        EXPECT_MSG(sky::horizonFade(0.0f) == 0.0f &&
                       sky::horizonFade(1.0f) == 1.0f,
                   "env_sky_type: as nuvens morrem no horizonte e vivem no "
                   "zénite");
        // a cobertura: 0 = céu limpo (TODO o FBM debaixo do limiar),
        // 1 = nublado (o meio do FBM é nuvem)
        EXPECT_MSG(sky::cloudAlpha(0.95f, 0.0f) == 0.0f,
                   "env_sky_type: coverage 0 = CÉU LIMPO (a nuvem morreu)");
        EXPECT_MSG(sky::cloudAlpha(0.5f, 1.0f) == 1.0f,
                   "env_sky_type: coverage 1 = NUBLADO (o céu sumiu)");
        EXPECT_MSG(sky::cloudAlpha(0.5f, 0.8f) >=
                       sky::cloudAlpha(0.5f, 0.4f),
                   "env_sky_type: mais cobertura NUNCA dá menos nuvem");
        // o FBM: os limites [0,1] + o determinismo (o mesmo uv = o mesmo
        // valor — o hash é inteiro, sem sin)
        f32 mn = 2.0f, mx = -2.0f;
        for (int i = 0; i < 64; ++i) {
            const f32 v = sky::fbm2(i * 0.137f, i * 0.291f - 3.0f);
            if (v < mn) mn = v;
            if (v > mx) mx = v;
        }
        EXPECT_MSG(mn >= 0.0f && mx <= 1.0f,
                   "env_sky_type: o FBM saiu de [0,1] (mn=%.3f mx=%.3f)",
                   (double)mn, (double)mx);
        EXPECT_MSG(sky::fbm2(1.7f, 2.3f) == sky::fbm2(1.7f, 2.3f),
                   "env_sky_type: o ruído NÃO é determinístico");
        // (2) O VENTO E O TEMPO — a M-Yd1 vive AQUI (a nuvem parada é o
        // defeito que o dono nomeou: «o shader não usa o tempo»)
        f32 w0[2], w1[2], wz[2];
        sky::windOffset(0.0f, 0.02f, w0);
        EXPECT_MSG(w0[0] == 0.0f && w0[1] == 0.0f,
                   "env_sky_type: o tempo 0 é o offset 0");
        sky::windOffset(10.0f, 0.02f, w1);
        EXPECT_MSG(w1[0] > w0[0] && w1[1] > w0[1],
                   "env_sky_type: O TEMPO NÃO EMPURRA AS NUVENS (M-Yd1 "
                   "vermelho — o céu ficou parado)");
        sky::windOffset(1000.0f, 0.0f, wz);
        EXPECT_MSG(wz[0] == 0.0f && wz[1] == 0.0f,
                   "env_sky_type: vento 0 = parado de sempre (o speed manda)");
    }
    // (3) A BASE DA CÂMARA (a extração da vista — a orbit e a de jogo)
    {
        const Mat4 ident = Mat4::identity();
        Vec3 r, u, f;
        EXPECT(sky::viewBasis(ident, r, u, f));
        EXPECT_MSG(nearEq(r.x, 1.0f, 1e-5f) && nearEq(u.y, 1.0f, 1e-5f) &&
                       nearEq(f.z, -1.0f, 1e-5f),
                   "env_sky_type: a identidade dá right/up/fwd canónicos");
        // a orbit de sempre: eye (0,0,6) → target origem olha para −Z
        const Mat4 view = Mat4::lookAt(Vec3{0.0f, 0.0f, 6.0f},
                                       Vec3{0.0f, 0.0f, 0.0f},
                                       Vec3{0.0f, 1.0f, 0.0f});
        EXPECT(sky::viewBasis(view, r, u, f));
        EXPECT_MSG(nearEq(f.z, -1.0f, 1e-4f) && nearEq(r.x, 1.0f, 1e-4f) &&
                       nearEq(u.y, 1.0f, 1e-4f),
                   "env_sky_type: a base do lookAt extrai-se das LINHAS");
        // o tan/aspect a partir da PROJEÇÃO (a fonte perspective da casa)
        const Mat4 proj = Mat4::perspective(1.0472f, 2.0f, 0.5f, 450.0f);
        EXPECT_MSG(nearEq(sky::tanFovYFromProj(proj), 0.5774f, 1e-3f),
                   "env_sky_type: tan(fovY/2) sai do m[5] da projeção");
        EXPECT_MSG(nearEq(sky::aspectFromProj(proj), 2.0f, 1e-3f),
                   "env_sky_type: o aspect sai da razão m[5]/m[0]");
        // a direção: o centro do ecrã É a frente; o canto direito abre
        Vec3 d;
        sky::viewDir(0.0f, 0.0f, r, u, f, 2.0f, 0.5774f, d);
        EXPECT_MSG(nearEq(d.x, 0.0f, 1e-5f) && nearEq(d.z, -1.0f, 1e-5f),
                   "env_sky_type: o ndc (0,0) olha para a FRENTE");
        sky::viewDir(1.0f, 0.0f, r, u, f, 2.0f, 0.5774f, d);
        EXPECT_MSG(d.x > 0.0f,
                   "env_sky_type: o ndc x=1 abre para a DIREITA da câmara");
    }
    // (4) O COMPONENTE — os defaults e a clamp (o padrão AudioPlayer)
    {
        Environment e;
        EXPECT_MSG(e.skyType == Environment::kSkyTypeSolid,
                   "env_sky_type: o default é o CÓR SÓLIDA (o C de sempre "
                   "byte a byte)");
        EXPECT_MSG(nearEq(e.cloudCoverage, 0.4f, 1e-6f) &&
                       nearEq(e.cloudSpeed, 0.02f, 1e-6f),
                   "env_sky_type: os defaults do dono (0.4 / 0.02)");
        EXPECT_MSG(nearEq(e.skyColorHorizon[0],
                          Environment::kHorizonDefault[0], 1e-7f),
                   "env_sky_type: o horizonte nasce #4A90E2");
        e.skyType = 7;                    // corrupção → o sólido seguro
        e.cloudCoverage = 1.5f;
        e.cloudSpeed = -0.1f;
        e.skyColorZenith[0] = 2.0f;
        e.sunColor[1] = -0.5f;
        // a direção do sol: só o Y leva o 2.0 (os outros a ZERO — o default
        // não é nulo; a norma tem de vir de UM eixo só para o teste falar)
        e.sunDirection[0] = 0.0f;
        e.sunDirection[1] = 2.0f;
        e.sunDirection[2] = 0.0f;
        e.clampFields();
        EXPECT_MSG(e.skyType == Environment::kSkyTypeSolid,
                   "env_sky_type: skyType SÓ tem 0 e 1 (a corrupção cai no "
                   "sólido)");
        EXPECT_MSG(nearEq(e.cloudCoverage, 1.0f, 1e-6f) &&
                       nearEq(e.cloudSpeed, 0.0f, 1e-6f),
                   "env_sky_type: cobertura teto 1, vento piso 0");
        EXPECT_MSG(nearEq(e.skyColorZenith[0], 1.0f, 1e-6f) &&
                       nearEq(e.sunColor[1], 0.0f, 1e-6f),
                   "env_sky_type: as cores vivem em [0,1]");
        EXPECT_MSG(nearEq(e.sunDirection[1], 1.0f, 1e-5f),
                   "env_sky_type: a direção do sol NORMALIZA");
        e.sunDirection[0] = 0.0f;
        e.sunDirection[1] = 0.0f;
        e.sunDirection[2] = 0.0f;
        e.clampFields();
        EXPECT_MSG(nearEq(e.sunDirection[1],
                          Environment::kSunDirDefault[1], 1e-6f),
                   "env_sky_type: o sol nulo cai no default da tarde");
        EXPECT_MSG(Environment::kSpeedMax == 0.2f,
                   "env_sky_type: o teto do vento é 0.2 (10× a brisa)");
    }
    // (5) O ROUNDTRIP (.goni) — os campos novos sobrevivem E os defaults
    // saem FORA do dump (o forward-compat da casa)
    {
        Scene a;
        const Handle h = a.create("Ceu");
        a.get(h)->addComponent<Transform3D>();
        Environment* e = a.get(h)->addComponent<Environment>();
        e->skyType = Environment::kSkyTypeProcedural;
        e->skyColorHorizon[0] = 0.15f;
        e->skyColorZenith[2] = 0.91f;
        e->sunColor[1] = 0.75f;
        e->sunDirection[0] = 0.6f;
        e->sunDirection[1] = 0.8f;
        e->sunDirection[2] = 0.0f;   // a norma é 1.0 exata — o roundtrip
                                     // transporta SEM renormalização visível
        e->cloudCoverage = 0.85f;
        e->cloudSpeed = 0.11f;
        e->clampFields();
        const std::string dump = SceneSerializer::dump(a);
        EXPECT_MSG(countSub(dump, "\"skyType\"") == 1,
                   "env_sky_type: o .goni GUARDA o skyType");
        EXPECT_MSG(countSub(dump, "\"skyHor\"") == 1 &&
                       countSub(dump, "\"skyZen\"") == 1 &&
                       countSub(dump, "\"sunCol\"") == 1 &&
                       countSub(dump, "\"sunDir\"") == 1,
                   "env_sky_type: o .goni guarda as cores do céu e do sol");
        EXPECT_MSG(countSub(dump, "\"cloudCov\"") == 1 &&
                       countSub(dump, "\"cloudSpd\"") == 1,
                   "env_sky_type: o .goni guarda AS NUVENS (a ordem do dono)");
        Scene b;
        SceneSerializer::LoadCtx ctx;
        ASSERT(SceneSerializer::loadText(b, dump, ctx));
        const Handle hf = b.find("Ceu");
        ASSERT(hf.valid());
        const Environment* e2 = b.get(hf)->getComponent<Environment>();
        ASSERT(e2 != nullptr);
        EXPECT_MSG(e2->skyType == Environment::kSkyTypeProcedural,
                   "env_sky_type: o céu carrega PROCEDURAL");
        EXPECT_MSG(nearEq(e2->skyColorHorizon[0], 0.15f, 1e-5f) &&
                       nearEq(e2->skyColorZenith[2], 0.91f, 1e-5f) &&
                       nearEq(e2->sunColor[1], 0.75f, 1e-5f),
                   "env_sky_type: as cores voltam byte a byte");
        EXPECT_MSG(nearEq(e2->cloudCoverage, 0.85f, 1e-5f) &&
                       nearEq(e2->cloudSpeed, 0.11f, 1e-5f),
                   "env_sky_type: as NUVENS voltam byte a byte");
        EXPECT_MSG(nearEq(e2->sunDirection[0], 0.6f, 1e-4f) &&
                       nearEq(e2->sunDirection[1], 0.8f, 1e-4f),
                   "env_sky_type: a direção do sol volta (normalizada)");
        // os DEFAULTS ficam FORA do dump (o .goni antigo carrega igual)
        Scene c;
        const Handle hc = c.create("limpo");
        c.get(hc)->addComponent<Transform3D>();
        c.get(hc)->addComponent<Environment>();
        const std::string dump2 = SceneSerializer::dump(c);
        EXPECT_MSG(countSub(dump2, "skyType") == 0 &&
                       countSub(dump2, "cloudCov") == 0 &&
                       countSub(dump2, "skyHor") == 0,
                   "env_sky_type: os defaults SAEM FORA do .goni "
                   "(forward-compat — o M-Yd2 da serialização)");
    }
    // (6) O CONTRATO NO FONTE (o FS replica as puras — a M-Yd1 morre AQUI
    // antes de qualquer GL; SEM comentários — a prosa não satisfaz)
    {
        std::string src;
        ASSERT(readFile(repoRoot() +
                        "/app/src/main/cpp/render/SkyRenderer.cpp",
                        src));
        const std::string code = test::stripCppComments(src);
        EXPECT_MSG(countSub(code, "uniform float uTime;") >= 1,
                   "env_sky_type: o FS declara uTime (o relógio das "
                   "nuvens)");
        EXPECT_MSG(countSub(code, "vec2(kWindDirX, kWindDirY) * (uTime * uSpeed)") >= 1,
                   "env_sky_type: O TEMPO EMPURRA O UV DAS NUVENS no FS "
                   "(a expressão do contrato — M-Yd1 morre AQUI)");
        EXPECT_MSG(countSub(code, "float fbm2(") >= 1,
                   "env_sky_type: o FS tem o FBM (a réplica das puras)");
    }
    // (7) O DRAW REAL no stub (o padrão pbr_texture_bind): os uniforms
    // existem PELO NOME e o draw manda O TEMPO (o ÚLTIMO 1f — o contrato
    // da ordem) e a direção do sol (o ÚLTIMO 3f)
    {
        glstub::fb::enabled = true;
        glstub::fb::resetState();
        glstub::reset();
        SkyRenderer s;
        ASSERT(s.init());
        s.use();   // o programa do céu fica corrente no stub (o padrão do
                   // pbr_texture_bind — a consulta registra PELO NOME)
        const GLint locTime =
            glGetUniformLocation(glstub::fb::curProg_, "uTime");
        EXPECT_MSG(locTime >= 0,
                   "env_sky_type: uTime não existe no programa do céu");
        EXPECT(glGetUniformLocation(glstub::fb::curProg_, "uSunDir") >= 0);
        EXPECT(glGetUniformLocation(glstub::fb::curProg_, "uCoverage") >= 0);

        SkyRenderer::Params p;
        p.time = 3.5f;
        p.speed = 0.02f;
        p.coverage = 0.7f;
        p.sunDir[0] = 0.6f;
        p.sunDir[1] = 0.8f;
        p.sunDir[2] = 0.0f;
        glstub::reset();
        const DrawStats st = s.draw(p);
        EXPECT_MSG(st.vertices == 4 && st.drawCalls == 1,
                   "env_sky_type: o céu é UM quad de 4 vértices");
        EXPECT_MSG(glstub::stats.lastUniform1f == 3.5f,
                   "env_sky_type: o uTime vai AO SHADER (%.2f — o tempo é "
                   "a alma; M-Yd1 morre AQUI)",
                   (double)glstub::stats.lastUniform1f);
        EXPECT_MSG(glstub::stats.lastUniform3f[0] == 0.6f &&
                       glstub::stats.lastUniform3f[1] == 0.8f,
                   "env_sky_type: a direção do sol vai ao shader (o "
                   "ÚLTIMO vec3 — a ordem do draw é o contrato)");
        p.time = 0.0f;
        glstub::reset();
        (void)s.draw(p);
        EXPECT_MSG(glstub::stats.lastUniform1f == 0.0f,
                   "env_sky_type: o tempo 0 chega ao shader como 0 (não "
                   "confundir com uniform ausente)");
        s.destroy();
        glstub::fb::enabled = false;
        glstub::fb::resetState();
    }
}

// ---- PIN inspector_sky (0.12-Y 3.Y-D) -----------------------------------------
// A ordem do dono: «Quando o TIC WorldEnvironment for selecionado, mostra
// um dropdown para skyType. Se skyType == 1 (Procedural), mostra os campos
// de cor, cobertura e velocidade das nuvens». O PLANO é a fonte (o mesmo
// contrato do inspector_worldenv) — o M-Yd2 mata a CONDIÇÃO e o modo
// sólido ganha nuvens que não devia ter.
TEST(inspector_sky) {
    const TextMetrics tm{};
    Scene scene;
    const Handle h = scene.create("ceu");
    scene.get(h)->addComponent<Transform3D>();
    Environment* env = scene.get(h)->addComponent<Environment>();
    ASSERT(env != nullptr);

    // (1) MODO SÓLIDO (skyType 0): o dropdown entra, AS NUVENS NÃO (o
    // M-Yd2 vive exatamente aqui)
    {
        const InspProfile prof = inspectorProfile(*scene.get(h));
        EXPECT(prof.env);
        EXPECT_MSG(prof.envSkyType == 0,
                   "inspector_sky: o perfil transporta o TIPO de céu");
        InspRow rows[64];
        const u32 n = inspectorPlan(prof, tm, false, 0, rows);
        u32 type = 0, hor = 0, zen = 0, sunc = 0, cov = 0, spd = 0;
        i32 iType = -1, iSky = -1;
        for (u32 i = 0; i < n; ++i) {
            const InspRow& r = rows[i];
            if (r.id == kInspectorEnvSkyType) {
                ++type;
                iType = static_cast<i32>(i);
            }
            if (r.id == kInspectorEnvHor) ++hor;
            if (r.id == kInspectorEnvZen) ++zen;
            if (r.id == kInspectorEnvSunC) ++sunc;
            if (r.id == kInspectorEnvCov) ++cov;
            if (r.id == kInspectorEnvSpd) ++spd;
            if (r.id == kInspectorEnvSky) iSky = static_cast<i32>(i);
        }
        EXPECT_MSG(type == 1,
                   "inspector_sky: o dropdown do tipo falta (vi %u)", type);
        EXPECT_MSG(hor == 0 && zen == 0 && sunc == 0 && cov == 0 && spd == 0,
                   "inspector_sky: NO MODO SÓLIDO as nuvens NÃO entram "
                   "(hor=%u zen=%u sol=%u cov=%u spd=%u — M-Yd2 vermelho)",
                   hor, zen, sunc, cov, spd);
        EXPECT_MSG(iType >= 0 && iType < iSky,
                   "inspector_sky: o tipo vem ANTES do céu sólido");
        // a contagem da FÓRMULA bate com o plano (as duas fontes uma só)
        EXPECT_MSG(inspectorRowCount(prof, false, 0) == n,
                   "inspector_sky: a fórmula do rowCount divergiu do plano "
                   "(%u vs %u)",
                   inspectorRowCount(prof, false, 0), n);
    }
    // (2) MODO PROCEDURAL (skyType 1): as 5 linhas na ORDEM do dono
    {
        env->skyType = Environment::kSkyTypeProcedural;
        const InspProfile prof = inspectorProfile(*scene.get(h));
        EXPECT_MSG(prof.envSkyType == 1,
                   "inspector_sky: o perfil segue o componente (procedural)");
        InspRow rows[64];
        const u32 n = inspectorPlan(prof, tm, false, 0, rows);
        i32 iType = -1, iHor = -1, iZen = -1, iSun = -1, iCov = -1,
            iSpd = -1, iSky = -1;
        for (u32 i = 0; i < n; ++i) {
            const InspRow& r = rows[i];
            if (r.id == kInspectorEnvSkyType) iType = static_cast<i32>(i);
            if (r.id == kInspectorEnvHor) iHor = static_cast<i32>(i);
            if (r.id == kInspectorEnvZen) iZen = static_cast<i32>(i);
            if (r.id == kInspectorEnvSunC) iSun = static_cast<i32>(i);
            if (r.id == kInspectorEnvCov) iCov = static_cast<i32>(i);
            if (r.id == kInspectorEnvSpd) iSpd = static_cast<i32>(i);
            if (r.id == kInspectorEnvSky) iSky = static_cast<i32>(i);
        }
        EXPECT_MSG(iType >= 0 && iHor >= 0 && iZen >= 0 && iSun >= 0 &&
                       iCov >= 0 && iSpd >= 0,
                   "inspector_sky: faltam linhas no procedural "
                   "(type=%d hor=%d zen=%d sol=%d cov=%d spd=%d)",
                   iType, iHor, iZen, iSun, iCov, iSpd);
        // a ORDEM do dono: tipo → horizonte → zénite → sol → nuvens →
        // vento → e o resto da secção (céu/hdri/ambiente/névoa)
        EXPECT_MSG(iType < iHor && iHor < iZen && iZen < iSun &&
                       iSun < iCov && iCov < iSpd && iSpd < iSky,
                   "inspector_sky: a ORDEM procedural saiu da especificação");
        EXPECT_MSG(inspectorRowCount(prof, false, 0) == n,
                   "inspector_sky: a fórmula divergiu do plano procedural");
        // os ids NÃO colidem (a faixa 5425..5430 é própria)
        EXPECT(kInspectorEnvSkyType != kInspectorEnvHor &&
               kInspectorEnvHor != kInspectorEnvZen &&
               kInspectorEnvZen != kInspectorEnvSunC &&
               kInspectorEnvSunC != kInspectorEnvCov &&
               kInspectorEnvCov != kInspectorEnvSpd);
        // (3) COLAPSADA: o bit8 esconde TUDO (o dropdown incluído)
        const u32 nc = inspectorPlan(prof, tm, false, kInspBitEnvironment,
                                     rows);
        u32 envRows = 0;
        for (u32 i = 0; i < nc; ++i) {
            if (rows[i].id >= kInspectorEnvSkyType &&
                rows[i].id <= kInspectorEnvSpd) {
                ++envRows;
            }
        }
        EXPECT_MSG(envRows == 0,
                   "inspector_sky: colapsada ainda mostra %u linhas de céu",
                   envRows);
    }
    // (4) O COMMIT dos hexes (13/14/15) — o teclado escreve o componente
    {
        EditorState st;
        st.textTic = h;
        f32 rgba[4] = {0.0f, 0.0f, 0.0f, 1.0f};
        // o horizonte
        std::snprintf(st.textBuf, sizeof(st.textBuf), "#112233");
        st.textLen = 7;
        st.textPurpose = 13;
        EXPECT(commitTextInput(scene, st));
        uiHexParse("#112233", rgba);
        EXPECT_MSG(nearEq(env->skyColorHorizon[0], rgba[0], 1e-5f) &&
                       nearEq(env->skyColorHorizon[2], rgba[2], 1e-5f),
                   "inspector_sky: o hex 13 NÃO chega ao horizonte");
        // o zénite
        std::snprintf(st.textBuf, sizeof(st.textBuf), "#445566");
        st.textLen = 7;
        st.textPurpose = 14;
        EXPECT(commitTextInput(scene, st));
        uiHexParse("#445566", rgba);
        EXPECT_MSG(nearEq(env->skyColorZenith[1], rgba[1], 1e-5f),
                   "inspector_sky: o hex 14 NÃO chega ao zénite");
        // o sol
        std::snprintf(st.textBuf, sizeof(st.textBuf), "#FFDDAA");
        st.textLen = 7;
        st.textPurpose = 15;
        EXPECT(commitTextInput(scene, st));
        uiHexParse("#FFDDAA", rgba);
        EXPECT_MSG(nearEq(env->sunColor[0], 1.0f, 1e-5f) &&
                       nearEq(env->sunColor[2], rgba[2], 1e-5f),
                   "inspector_sky: o hex 15 NÃO chega ao sol");
        // inválido = estado intacto (o contrato do hex)
        const f32 antes = env->skyColorHorizon[0];
        std::snprintf(st.textBuf, sizeof(st.textBuf), "zzz");
        st.textLen = 3;
        st.textPurpose = 13;
        EXPECT(!commitTextInput(scene, st));
        EXPECT_MSG(env->skyColorHorizon[0] == antes,
                   "inspector_sky: o hex inválido ESTRAGOU o horizonte");
    }
}

// ---- PIN env_sky_clouds (0.12-Y-E) --------------------------------------------
// A ordem do dono: «nuvens procedurais mobile-friendly (FBM no FS, ZERO
// raymarching), cloudCoverage/cloudSpeed/cloudHeight no Environment, o UV
// desliza com o uTime». A ALTURA nova (o plano da projeção) + as ESTRELAS
// (o campo estocástico atrás do branch uniforme) + o componente + o
// contrato no FS + o draw real (a ordem do uTime INTACTA — o pin da D).
TEST(env_sky_clouds) {
    // (1) O UV COM A ALTURA — a réplica de sky::cloudUv: a altura escala o
    // plano (2× = 2× o uv base) e o TEMPO continua a empurrar (M-Ye1)
    {
        f32 uv0[2], uv1[2], uvh[2], uvt[2];
        sky::cloudUv(0.3f, 0.8f, -0.5f, 1.0f, 0.0f, 0.02f, uv0);
        sky::cloudUv(0.3f, 0.8f, -0.5f, 2.0f, 0.0f, 0.02f, uvh);
        EXPECT_MSG(nearEq(uvh[0], uv0[0] * 2.0f, 1e-4f) &&
                       nearEq(uvh[1], uv0[1] * 2.0f, 1e-4f),
                   "env_sky_clouds: a ALTURA NÃO ESCALA O PLANO das nuvens "
                   "(uv %.3f×%.3f vs %.3f×%.3f)",
                   (double)uvh[0], (double)uvh[1], (double)uv0[0],
                   (double)uv0[1]);
        sky::cloudUv(0.3f, 0.8f, -0.5f, 1.0f, 10.0f, 0.02f, uvt);
        EXPECT_MSG(uvt[0] > uv0[0] && uvt[1] > uv0[1],
                   "env_sky_clouds: O TEMPO NÃO EMPURRA O UV (a nuvem "
                   "parada — M-Ye1 vermelho)");
        // o piso da projeção: dir.y~0 não explode (o kPlaneEps de sempre)
        sky::cloudUv(0.3f, 0.01f, -0.5f, 1.0f, 0.0f, 0.0f, uv1);
        EXPECT_MSG(std::isfinite(uv1[0]) && std::isfinite(uv1[1]) &&
                       std::fabs(uv1[0]) < 100.0f,
                   "env_sky_clouds: o horizonte EXPLODIU o uv (o piso "
                   "kPlaneEps morreu)");
        // a altura 1 = o céu da D byte a byte (o kCloudScale entra 1×)
        f32 uvD[2];
        sky::cloudUv(0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, uvD);
        EXPECT_MSG(nearEq(uvD[0], sky::kCloudScale * 0.0f, 1e-6f),
                   "env_sky_clouds: a altura 1 não é o plano da D");
    }
    // (2) AS ESTRELAS — determinísticas, esparsas (o threshold mata ~72%
    // das células) e CHEIAS no centro do ponto (o offset vem do hash)
    {
        EXPECT_MSG(sky::starField(0.3f, 0.4f, 0.5f) ==
                       sky::starField(0.3f, 0.4f, 0.5f),
                   "env_sky_clouds: o campo de estrelas NÃO é "
                   "determinístico (o hash morreu)");
        // a dispersão: num varrimento amplo HÁ células vazias E estrelas
        u32 vazias = 0, vivas = 0;
        f32 mx = 0.0f;
        for (int i = 1; i < 24; ++i) {
            for (int j = 1; j < 24; ++j) {
                const Vec3 d = normalized(Vec3{
                    static_cast<f32>(i) * 0.173f - 2.0f,
                    static_cast<f32>(j) * 0.091f + 0.05f,
                    -0.62f});
                const f32 s = sky::starField(d.x, d.y, d.z);
                if (s <= 0.0f) {
                    ++vazias;
                } else {
                    ++vivas;
                    if (s > mx) mx = s;
                }
            }
        }
        EXPECT_MSG(vazias > 0 && vivas > 0,
                   "env_sky_clouds: o campo de estrelas não é ESTOCÁSTICO "
                   "(vazias=%u vivas=%u — o threshold morreu)", vazias,
                   vivas);
        EXPECT_MSG(mx <= 1.0f,
                   "env_sky_clouds: a estrela saiu de [0,1] (mx=%.3f)",
                   (double)mx);
        // o centro de uma célula COM estrela vale ~1 (o ponto inteiro)
        bool achou = false;
        for (int x = -8; x < 8 && !achou; ++x) {
            for (int y = 4; y < 40 && !achou; ++y) {
                for (int z = -8; z < 8 && !achou; ++z) {
                    const u32 h = sky::hash3u(x, y, z);
                    if ((h & 0xFFu) >= 184u) {
                        const f32 ox = 0.2f + 0.6f *
                            static_cast<f32>((h >> 8u) & 0xFFu) / 255.0f;
                        const f32 oy = 0.2f + 0.6f *
                            static_cast<f32>((h >> 16u) & 0xFFu) / 255.0f;
                        const f32 oz = 0.2f + 0.6f *
                            static_cast<f32>((h >> 24u) & 0xFFu) / 255.0f;
                        const f32 sx = (static_cast<f32>(x) + ox) /
                                       sky::kStarDensity;
                        const f32 sy = (static_cast<f32>(y) + oy) /
                                       sky::kStarDensity;
                        const f32 sz = (static_cast<f32>(z) + oz) /
                                       sky::kStarDensity;
                        EXPECT_MSG(nearEq(sky::starField(sx, sy, sz), 1.0f,
                                          1e-4f),
                                   "env_sky_clouds: o CENTRO da estrela não "
                                   "vale 1 (o raio morreu)");
                        achou = true;
                    }
                }
            }
        }
        EXPECT_MSG(achou,
                   "env_sky_clouds: nenhuma célula acima do threshold "
                   "(o campo inteiro morreu)");
    }
    // (3) O COMPONENTE — os defaults do ciclo + a clamp (o relógio dá a
    // volta, a altura vive em [0.25,4])
    {
        Environment e;
        EXPECT_MSG(nearEq(e.timeOfDay, 12.0f, 1e-6f) &&
                       nearEq(e.dayCycleSpeed, 0.0f, 1e-6f) &&
                       !e.autoCycle &&
                       nearEq(e.cloudHeight, 1.0f, 1e-6f),
                   "env_sky_clouds: os defaults do ciclo (12h / 0 / off / "
                   "1.0 — a identidade da D)");
        e.timeOfDay = 25.5f;    // a volta: 25.5h é 1.5h
        e.cloudHeight = 9.0f;
        e.dayCycleSpeed = -0.3f;
        e.clampFields();
        EXPECT_MSG(nearEq(e.timeOfDay, 1.5f, 1e-5f),
                   "env_sky_clouds: o relógio NÃO dá a volta em 24h "
                   "(25.5 → %.3f)", (double)e.timeOfDay);
        EXPECT_MSG(nearEq(e.cloudHeight, 4.0f, 1e-6f) &&
                       nearEq(e.dayCycleSpeed, 0.0f, 1e-6f),
                   "env_sky_clouds: a altura teto 4 e a velocidade piso 0");
        e.timeOfDay = -2.0f;
        e.cloudHeight = 0.05f;
        e.clampFields();
        EXPECT_MSG(nearEq(e.timeOfDay, 22.0f, 1e-5f),
                   "env_sky_clouds: o relógio negativo NÃO modula "
                   "(-2 → %.3f)", (double)e.timeOfDay);
        EXPECT_MSG(nearEq(e.cloudHeight, 0.25f, 1e-6f),
                   "env_sky_clouds: a altura piso 0.25");
        EXPECT_MSG(Environment::kTodMax == 24.0f &&
                       Environment::kCycleMax == 1.0f,
                   "env_sky_clouds: os tetos do ciclo (24h, vel 1.0)");
    }
    // (4) O CONTRATO NO FONTE (o FS replica as puras — sem comentários)
    {
        std::string src;
        ASSERT(readFile(repoRoot() +
                        "/app/src/main/cpp/render/SkyRenderer.cpp",
                        src));
        const std::string code = test::stripCppComments(src);
        EXPECT_MSG(countSub(code, "uniform float uHeight;") >= 1,
                   "env_sky_clouds: o FS perdeu o uniform uHeight");
        EXPECT_MSG(countSub(code, "uniform float uStars;") >= 1,
                   "env_sky_clouds: o FS perdeu o uniform uStars");
        EXPECT_MSG(countSub(code, "(kCloudScale * uHeight)") >= 1,
                   "env_sky_clouds: a ALTURA saiu do UV das nuvens no FS "
                   "(a expressão do contrato)");
        EXPECT_MSG(countSub(code, "float starField(vec3 dir, float thresh)") >= 1,
                   "env_sky_clouds: o FS perdeu o campo de estrelas");
        EXPECT_MSG(countSub(code, "if (uStars > 0.001)") >= 1,
                   "env_sky_clouds: o branch UNIFORME das estrelas morreu "
                   "(o dia paga o campo inteiro)");
    }
    // (5) O DRAW REAL no stub — os uniforms novos EXISTEM e a ordem do
    // uTime (o ÚLTIMO 1f — o contrato do pin da D) sobrevive aos 2 novos
    {
        glstub::fb::enabled = true;
        glstub::fb::resetState();
        glstub::reset();
        SkyRenderer s;
        ASSERT(s.init());
        s.use();
        const GLint locH =
            glGetUniformLocation(glstub::fb::curProg_, "uHeight");
        const GLint locS =
            glGetUniformLocation(glstub::fb::curProg_, "uStars");
        EXPECT_MSG(locH >= 0 && locS >= 0,
                   "env_sky_clouds: uHeight/uStars não existem no programa");
        SkyRenderer::Params p;
        p.height = 2.5f;
        p.stars = 0.8f;
        p.time = 7.25f;
        const DrawStats st = s.draw(p);
        EXPECT_MSG(st.vertices == 4 && st.drawCalls == 1,
                   "env_sky_clouds: o céu deixou de ser UM quad");
        EXPECT_MSG(glstub::stats.lastUniform1f == 7.25f,
                   "env_sky_clouds: o uTime deixou de ser o ÚLTIMO 1f do "
                   "draw (%.2f — a ordem da D é o contrato)",
                   (double)glstub::stats.lastUniform1f);
        s.destroy();
        glstub::fb::enabled = false;
        glstub::fb::resetState();
    }
}

// ---- PIN env_daynight_cycle (0.12-Y-E) ----------------------------------------
// A ordem do dono: «timeOfDay 0..24 + dayCycleSpeed; o shader calcula o
// ÂNGULO do sol; o gradiente e a luz interpolam entre 00:00 noite (escuro,
// estrelas, luar), 06:00 amanhecer (laranja/rosa), 12:00 meio-dia (azul,
// sol no zénite) e 18:00 pôr do sol (vermelho/laranja); a DIRECTIONAL roda
// em sincronia (mais fraca e alaranjada ao pôr do sol)». As PURAS + a
// identidade do meio-dia (o byte a byte da D) + o wiring no main + o lit.
TEST(env_daynight_cycle) {
    const f32 hor[3] = {0.29f, 0.56f, 0.89f};
    const f32 zen[3] = {0.12f, 0.23f, 0.54f};
    const f32 sun[3] = {1.0f, 0.87f, 0.67f};
    // (1) O ÂNGULO DO SOL — 06h nasce a leste, 12h no zénite, 18h a oeste,
    // 00h sob o mundo; SEMPRE normalizada
    {
        f32 d[3];
        sky::sunDirFromTime(6.0f, d);
        EXPECT_MSG(d[0] > 0.85f && std::fabs(d[1]) < 0.35f,
                   "env_daynight_cycle: às 06h o sol NÃO NASCE a leste "
                   "(%.2f, %.2f)", (double)d[0], (double)d[1]);
        sky::sunDirFromTime(12.0f, d);
        EXPECT_MSG(d[1] > 0.85f,
                   "env_daynight_cycle: às 12h o sol NÃO está no zénite "
                   "(y=%.2f)", (double)d[1]);
        sky::sunDirFromTime(18.0f, d);
        EXPECT_MSG(d[0] < -0.85f,
                   "env_daynight_cycle: às 18h o sol NÃO se põe a oeste "
                   "(x=%.2f)", (double)d[0]);
        sky::sunDirFromTime(0.0f, d);
        EXPECT_MSG(d[1] < 0.0f,
                   "env_daynight_cycle: à meia-noite o sol NÃO está sob o "
                   "mundo (y=%.2f)", (double)d[1]);
        const f32 len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        EXPECT_MSG(nearEq(len, 1.0f, 1e-5f),
                   "env_daynight_cycle: a direção do sol não NORMALIZA");
    }
    // (2) A GRADE — a identidade no meio-dia (o byte a byte da D), a noite
    // ESCURA com estrelas (o teste do dono: 20h), o calor do amanhecer e
    // do pôr do sol, o luar fraco, a continuidade
    {
        sky::SkyGrade g;
        sky::dayNightGrade(12.0f, hor, zen, sun, g);
        EXPECT_MSG(g.hor[0] == hor[0] && g.hor[1] == hor[1] &&
                       g.hor[2] == hor[2] && g.zen[0] == zen[0] &&
                       g.zen[2] == zen[2] && g.sun[1] == sun[1],
                   "env_daynight_cycle: o meio-dia NÃO é a identidade (as "
                   "cores do utilizador tinham de voltar byte a byte)");
        EXPECT_MSG(g.stars == 0.0f,
                   "env_daynight_cycle: estrelas ao MEIO-DIA (o gate morreu)");
        // O TESTE DO DONO: 20h = céu ESCURO + estrelas
        sky::dayNightGrade(20.0f, hor, zen, sun, g);
        const f32 lum = 0.2126f * g.hor[0] + 0.7152f * g.hor[1] +
                        0.0722f * g.hor[2];
        EXPECT_MSG(lum < 0.16f,
                   "env_daynight_cycle: às 20h o céu NÃO fica escuro "
                   "(lum=%.3f — M-Ye2 vermelho)", (double)lum);
        EXPECT_MSG(g.stars > 0.5f,
                   "env_daynight_cycle: às 20h NÃO há estrelas (%.2f — o "
                   "gate da noite morreu)", (double)g.stars);
        EXPECT_MSG(0.2126f * g.light[0] + 0.7152f * g.light[1] +
                       0.0722f * g.light[2] < 0.35f,
                   "env_daynight_cycle: o LUAR da noite não é fraco");
        // o amanhecer/pôr do sol: o horizonte mais QUENTE que azul (r > b)
        sky::dayNightGrade(6.0f, hor, zen, sun, g);
        EXPECT_MSG(g.hor[0] > g.hor[2],
                   "env_daynight_cycle: às 06h o horizonte não é laranja/"
                   "rosa (r=%.2f b=%.2f)", (double)g.hor[0], (double)g.hor[2]);
        sky::dayNightGrade(18.0f, hor, zen, sun, g);
        EXPECT_MSG(g.hor[0] > g.hor[2],
                   "env_daynight_cycle: às 18h o horizonte não é vermelho/"
                   "laranja (r=%.2f b=%.2f)", (double)g.hor[0],
                   (double)g.hor[2]);
        EXPECT_MSG(g.stars > 0.0f && g.stars < 0.5f,
                   "env_daynight_cycle: as estrelas do crepúsculo saíram do "
                   "meio-termo (%.2f)", (double)g.stars);
        // a escada das estrelas: noite > crepúsculo > meio-dia = 0
        sky::SkyGrade gn, gd, gz;
        sky::dayNightGrade(0.0f, hor, zen, sun, gn);
        sky::dayNightGrade(6.0f, hor, zen, sun, gd);
        sky::dayNightGrade(12.0f, hor, zen, sun, gz);
        EXPECT_MSG(gn.stars > gd.stars && gd.stars > gz.stars,
                   "env_daynight_curve: a escada das estrelas inverteu");
        // a continuidade: os vizinhos do meio-dia e o fecho do anel 0≈24
        sky::SkyGrade ga, gb;
        sky::dayNightGrade(11.9f, hor, zen, sun, ga);
        sky::dayNightGrade(12.1f, hor, zen, sun, gb);
        EXPECT_MSG(nearEq(ga.hor[0], gb.hor[0], 0.06f) &&
                       nearEq(ga.stars, gb.stars, 0.06f),
                   "env_daynight_cycle: a grade SALTA no meio-dia (a "
                   "interpolação suave morreu)");
        sky::dayNightGrade(0.0f, hor, zen, sun, ga);
        sky::dayNightGrade(24.0f, hor, zen, sun, gb);
        EXPECT_MSG(nearEq(ga.hor[0], gb.hor[0], 1e-5f) &&
                       nearEq(ga.stars, gb.stars, 1e-5f),
                   "env_daynight_cycle: o anel 0h/24h não fecha");
    }
    // (3) O WIRING NO FONTE — o main consome a grade + o sol + a luz (as
    // puras chegam ao frame; SEM comentários — a prosa não satisfaz)
    {
        std::string src;
        ASSERT(readFile(repoRoot() + "/app/src/main/cpp/platform/main.cpp",
                        src));
        const std::string code = test::stripCppComments(src);
        EXPECT_MSG(countSub(code, "sky::dayNightGrade(") >= 1,
                   "env_daynight_cycle: o main deixou de consumir a GRADE "
                   "(M-Ye2 — o tempo deixou de mudar a cor)");
        EXPECT_MSG(countSub(code, "sky::sunDirFromTime(") >= 1,
                   "env_daynight_cycle: o sol deixou de rodar pelo relógio");
        EXPECT_MSG(countSub(code, "setLight(") >= 1,
                   "env_daynight_cycle: a DIRECTIONAL deixou de receber o "
                   "sol (a luz não sincroniza)");
        // a identidade da luz de sempre (o kLightDir do lit preservado)
        EXPECT_MSG(countSub(code, "0.4545f, 0.7435f, 0.2891f") >= 1,
                   "env_daynight_cycle: a identidade da luz de sempre "
                   "desapareceu do main (o lit sem Environment diverge)");
    }
    // (4) O LIT NO FONTE — os uniforms novos + a identidade (a cor 1.0 não
    // muda o lit de sempre)
    {
        std::string src;
        ASSERT(readFile(repoRoot() + "/app/src/main/cpp/render/Material.cpp",
                        src));
        const std::string code = test::stripCppComments(src);
        EXPECT_MSG(countSub(code, "uniform vec3 uLightDir;") >= 1 &&
                       countSub(code, "uniform vec3 uLightCol;") >= 1,
                   "env_daynight_cycle: o lit perdeu os uniforms da "
                   "directional (uLightDir/uLightCol)");
        EXPECT_MSG(countSub(code,
                            "uLightCol * (alb * mdiff * diff") >= 1,
                   "env_daynight_cycle: a COR da luz não entra na difusa/"
                   "especular (a expressão do contrato morreu)");
        EXPECT_MSG(countSub(code, "dot(n, uLightDir)") >= 1,
                   "env_daynight_cycle: a difusa não aponta para "
                   "uLightDir (o sol parado no lit)");
    }
}

// ---- PIN env_serialize (0.12-Y-E) ---------------------------------------------
// A ordem do dono: «o ficheiro de cena (.goni) guarda e carrega corretamente
// o timeOfDay e as propriedades das nuvens». O roundtrip dos 4 campos novos
// + os defaults FORA do dump (o forward-compat da casa — o .goni da D
// carrega byte a byte).
TEST(env_serialize) {
    // (1) O ROUNDTRIP dos campos novos (não-defaults entram no dump)
    {
        Scene a;
        const Handle h = a.create("Ciclo");
        a.get(h)->addComponent<Transform3D>();
        Environment* e = a.get(h)->addComponent<Environment>();
        e->timeOfDay = 20.5f;
        e->dayCycleSpeed = 0.4f;
        e->autoCycle = true;
        e->cloudHeight = 2.5f;
        e->cloudCoverage = 0.7f;
        e->clampFields();
        const std::string dump = SceneSerializer::dump(a);
        EXPECT_MSG(countSub(dump, "\"tod\"") == 1,
                   "env_serialize: o .goni NÃO guarda o timeOfDay");
        EXPECT_MSG(countSub(dump, "\"cycSpd\"") == 1 &&
                       countSub(dump, "\"cycOn\"") == 1,
                   "env_serialize: o .goni NÃO guarda o auto ciclo "
                   "(velocidade/estado)");
        EXPECT_MSG(countSub(dump, "\"cloudH\"") == 1,
                   "env_serialize: o .goni NÃO guarda a altura das nuvens");
        Scene b;
        SceneSerializer::LoadCtx ctx;
        ASSERT(SceneSerializer::loadText(b, dump, ctx));
        const Handle hf = b.find("Ciclo");
        ASSERT(hf.valid());
        const Environment* e2 = b.get(hf)->getComponent<Environment>();
        ASSERT(e2 != nullptr);
        EXPECT_MSG(nearEq(e2->timeOfDay, 20.5f, 1e-5f) &&
                       nearEq(e2->dayCycleSpeed, 0.4f, 1e-5f) &&
                       e2->autoCycle &&
                       nearEq(e2->cloudHeight, 2.5f, 1e-5f) &&
                       nearEq(e2->cloudCoverage, 0.7f, 1e-5f),
                   "env_serialize: o ciclo não volta do .goni (tod=%.2f "
                   "spd=%.2f on=%d h=%.2f)",
                   (double)e2->timeOfDay, (double)e2->dayCycleSpeed,
                   e2->autoCycle ? 1 : 0, (double)e2->cloudHeight);
    }
    // (2) Os DEFAULTS ficam FORA do dump (o meio-dia/parado/desligado é o
    // silêncio — o .goni antigo carrega igual)
    {
        Scene c;
        const Handle hc = c.create("limpo");
        c.get(hc)->addComponent<Transform3D>();
        c.get(hc)->addComponent<Environment>();
        const std::string dump2 = SceneSerializer::dump(c);
        EXPECT_MSG(countSub(dump2, "\"tod\"") == 0 &&
                       countSub(dump2, "\"cycSpd\"") == 0 &&
                       countSub(dump2, "\"cycOn\"") == 0 &&
                       countSub(dump2, "\"cloudH\"") == 0,
                   "env_serialize: os defaults do ciclo INVADIRAM o .goni "
                   "(o forward-compat morreu)");
        // e o carregamento de um dump SEM os campos cai na identidade
        Scene d;
        SceneSerializer::LoadCtx ctx2;
        ASSERT(SceneSerializer::loadText(d, dump2, ctx2));
        const Handle hd = d.find("limpo");
        const Environment* e3 = d.get(hd)->getComponent<Environment>();
        ASSERT(e3 != nullptr);
        EXPECT_MSG(nearEq(e3->timeOfDay, 12.0f, 1e-6f) && !e3->autoCycle &&
                       nearEq(e3->cloudHeight, 1.0f, 1e-6f),
                   "env_serialize: os defaults ausentes não caem na "
                   "identidade do meio-dia");
    }
    // (3) A corrupção do relógio modula (o clampFields no load — 30h é 6h)
    {
        Scene a;
        const Handle h = a.create("corrupto");
        a.get(h)->addComponent<Transform3D>();
        Environment* e = a.get(h)->addComponent<Environment>();
        e->timeOfDay = 30.0f;
        const std::string dump = SceneSerializer::dump(a);
        Scene b;
        SceneSerializer::LoadCtx ctx;
        ASSERT(SceneSerializer::loadText(b, dump, ctx));
        const Environment* e2 =
            b.get(b.find("corrupto"))->getComponent<Environment>();
        ASSERT(e2 != nullptr);
        EXPECT_MSG(nearEq(e2->timeOfDay, 6.0f, 1e-5f),
                   "env_serialize: o relógio corrompido não modula "
                   "(30h → %.2f)", (double)e2->timeOfDay);
    }
}

// ---- PIN inspector_daynight (0.12-Y-E) ----------------------------------------
// A ordem do dono: «Na secção Environment do Inspector, slider timeOfDay
// (0..24), checkbox/botão Auto Cycle, sliders cloudCoverage/cloudSpeed/
// cloudHeight». O PLANO (a fonte única do Inspector): as 4 linhas novas
// SÓ no procedural, na ORDEM, e a fórmula do rowCount em casa.
TEST(inspector_daynight) {
    const TextMetrics tm{};
    Scene scene;
    const Handle h = scene.create("ciclo");
    scene.get(h)->addComponent<Transform3D>();
    Environment* env = scene.get(h)->addComponent<Environment>();
    ASSERT(env != nullptr);

    // (1) MODO SÓLIDO: NENHUMA das 4 linhas novas (o M-Yd2 mantém a regra)
    {
        InspRow rows[64];
        const u32 n = inspectorPlan(inspectorProfile(*scene.get(h)), tm,
                                    false, 0, rows);
        u32 tod = 0, cyc = 0, cycs = 0, hgt = 0;
        for (u32 i = 0; i < n; ++i) {
            if (rows[i].id == kInspectorEnvTod) ++tod;
            if (rows[i].id == kInspectorEnvCyc) ++cyc;
            if (rows[i].id == kInspectorEnvCycS) ++cycs;
            if (rows[i].id == kInspectorEnvHgt) ++hgt;
        }
        EXPECT_MSG(tod == 0 && cyc == 0 && cycs == 0 && hgt == 0,
                   "inspector_daynight: NO MODO SÓLIDO o ciclo NÃO entra "
                   "(tod=%u cyc=%u cycs=%u hgt=%u)", tod, cyc, cycs, hgt);
    }
    // (2) MODO PROCEDURAL: as 4 linhas na ORDEM do dono (hora → ciclo →
    // vel. → altura) DEPOIS das 5 da D e ANTES do céu sólido
    {
        env->skyType = Environment::kSkyTypeProcedural;
        const InspProfile prof = inspectorProfile(*scene.get(h));
        InspRow rows[64];
        const u32 n = inspectorPlan(prof, tm, false, 0, rows);
        i32 iTod = -1, iCyc = -1, iCycS = -1, iHgt = -1, iSpd = -1,
            iSky = -1;
        u32 tod = 0, cyc = 0, cycs = 0, hgt = 0;
        for (u32 i = 0; i < n; ++i) {
            const InspRow& r = rows[i];
            if (r.id == kInspectorEnvTod) { ++tod; iTod = (i32)i; }
            if (r.id == kInspectorEnvCyc) { ++cyc; iCyc = (i32)i; }
            if (r.id == kInspectorEnvCycS) { ++cycs; iCycS = (i32)i; }
            if (r.id == kInspectorEnvHgt) { ++hgt; iHgt = (i32)i; }
            if (r.id == kInspectorEnvSpd) iSpd = (i32)i;
            if (r.id == kInspectorEnvSky) iSky = (i32)i;
        }
        EXPECT_MSG(tod == 1 && cyc == 1 && cycs == 1 && hgt == 1,
                   "inspector_daynight: as 4 linhas novas faltam ou "
                   "duplicam (tod=%u cyc=%u cycs=%u hgt=%u)", tod, cyc,
                   cycs, hgt);
        EXPECT_MSG(iSpd >= 0 && iTod > iSpd && iCyc > iTod &&
                       iCycS > iCyc && iHgt > iCycS && iSky > iHgt,
                   "inspector_daynight: a ORDEM do dono morreu (spd=%d "
                   "tod=%d cyc=%d cycs=%d hgt=%d sky=%d)", iSpd, iTod,
                   iCyc, iCycS, iHgt, iSky);
        // a contagem da FÓRMULA bate com o plano (as duas fontes uma só)
        EXPECT_MSG(inspectorRowCount(prof, false, 0) == n,
                   "inspector_daynight: a fórmula do rowCount divergiu do "
                   "plano (%u vs %u)", inspectorRowCount(prof, false, 0),
                   n);
        // os IDS não colidem com os vizinhos da D (a colisão mata o toque)
        EXPECT_MSG(kInspectorEnvTod != kInspectorEnvSpd &&
                       kInspectorEnvCyc != kInspectorEnvHgt &&
                       kInspectorEnvCycS != kInspectorEnvSky,
                   "inspector_daynight: os ids novos colidem");
    }
}

// ---- PIN particle_emitter_cria (0.12-Y-F1) ------------------------------------
// A ordem do dono: «O CPU apenas gera uma vez um buffer com "seeds" e a
// posição inicial; o Vertex Shader recebe o uTime e calcula a posição
// final em tempo real. Zero física no CPU». As PURAS (a FONTE ÚNICA
// render/ParticleShader.h) + o componente + o preset + o VS (a fórmula
// EXATA do prompt, SEM comentários) + o draw real no stub (1 call, N
// instâncias, o uTime ÚLTIMO 1f).
TEST(particle_emitter_cria) {
    // (1) O COMPONENTE — a fábrica do jorro (os defaults) + a clamp
    {
        ParticleEmitter pe;
        EXPECT_MSG(pe.maxParticles == 500 && nearEq(pe.lifetime, 2.0f, 1e-6f) &&
                       pe.active && pe.texturePath.empty() &&
                       nearEq(pe.startSize, 0.25f, 1e-6f) &&
                       nearEq(pe.endSize, 0.05f, 1e-6f),
                   "particle_emitter_cria: os defaults da fábrica (500/2s/"
                   "sprite procedural)");
        pe.maxParticles = 99999;
        pe.lifetime = -1.0f;
        pe.startSize = 99.0f;
        pe.gravity[1] = -99.0f;
        pe.clampFields();
        EXPECT_MSG(pe.maxParticles == 2000 &&
                       nearEq(pe.lifetime, 0.1f, 1e-6f) &&
                       nearEq(pe.startSize, 4.0f, 1e-6f) &&
                       nearEq(pe.gravity[1], -20.0f, 1e-6f),
                   "particle_emitter_cria: a clamp (2000/0.1s/4/−20)");
        EXPECT_MSG(ParticleEmitter::kMaxParticlesMax == 2000 &&
                       ParticleEmitter::kLifetimeMin == 0.1f,
                   "particle_emitter_cria: os tetos da casa");
    }
    // (2) O PRESET — «ParticleEmitter» cria o TIC com Transform +
    // ParticleEmitter (SÓ isso — SEM mesh, SEM física)
    {
        Scene scene;
        const Handle h = createTicFromPreset(scene, PresetKind::ParticleEmitter,
                                             nullptr, nullptr);
        ASSERT(h.valid());
        const Tic* t = scene.get(h);
        EXPECT_MSG(t->name.find("ParticleEmitter") == 0,
                   "particle_emitter_cria: o TIC nasce com o nome do preset "
                   "('%s')", t->name.c_str());
        EXPECT_MSG(t->getComponent<Transform3D>() != nullptr,
                   "particle_emitter_cria: o preset traz o Transform");
        const ParticleEmitter* pe = t->getComponent<ParticleEmitter>();
        EXPECT_MSG(pe != nullptr,
                   "particle_emitter_cria: o preset traz o ParticleEmitter");
        EXPECT_MSG(t->getComponent<MeshRenderer>() == nullptr &&
                       t->getComponent<BodyComp>() == nullptr,
                   "particle_emitter_cria: o preset NÃO traz mesh nem corpo");
        EXPECT_MSG(nearEq(pe->velocity[1], 3.0f, 1e-6f) &&
                       nearEq(pe->gravity[1], -4.0f, 1e-6f),
                   "particle_emitter_cria: o jorro de fábrica (sobe e cai)");
        // o nome único: o 2.º nasce .001 (o padrão Godot da casa)
        const Handle h2 = createTicFromPreset(scene, PresetKind::ParticleEmitter,
                                              nullptr, nullptr);
        ASSERT(h2.valid());
        EXPECT_MSG(scene.get(h2)->name != t->name,
                   "particle_emitter_cria: o 2.º emissor tem nome único");
    }
    // (3) AS SEEDS — determinísticas, na esfera do raio kSpawnRadius, a
    // semente em [0,1) e o buffer INTEIRO gerado de uma vez
    {
        f32 a[4], b[4];
        fx::seedFor(7u, a);
        fx::seedFor(7u, b);
        EXPECT_MSG(a[0] == b[0] && a[3] == b[3],
                   "particle_emitter_cria: a seed NÃO é determinística");
        u32 fora = 0;
        f32 seeds[ParticleEmitter::kMaxParticlesMax * 4];
        fx::generateSeeds(ParticleEmitter::kMaxParticlesMax, seeds);
        for (u32 i = 0; i < ParticleEmitter::kMaxParticlesMax; ++i) {
            const f32* s = seeds + i * 4;
            const f32 len = std::sqrt(s[0] * s[0] + s[1] * s[1] +
                                      s[2] * s[2]);
            if (len > fx::kSpawnRadius + 1e-4f) {
                ++fora;
            }
            if (s[3] < 0.0f || s[3] >= 1.0f) {
                ++fora;
            }
        }
        EXPECT_MSG(fora == 0,
                   "particle_emitter_cria: %u seeds fora do contrato "
                   "(esfera %.2f, semente [0,1))", fora,
                   (double)fx::kSpawnRadius);
    }
    // (4) A MATEMÁTICA — o mod do GLSL (NUNCA negativo), a balística
    // (p = p0 + v·t + ½g·t²), o progresso e as interpolações (o M-Yf1
    // vive AQUI: sem o termo do tempo as partículas morrem paradas)
    {
        EXPECT_MSG(nearEq(fx::glslMod(-2.0f, 24.0f), 22.0f, 1e-5f),
                   "particle_emitter_cria: o mod NÃO é o do GLSL (negativo "
                   "não modula)");
        EXPECT_MSG(nearEq(fx::localTime(5.0f, 0.0f, 2.0f), 1.0f, 1e-5f),
                   "particle_emitter_cria: o tempo local (5 mod 2 = 1)");
        // a semente DESFAZ o tempo: sementes diferentes = fases diferentes
        // (0.1·100=10 → 10 mod 2 = 0; 0.13·100=13 → 13 mod 2 = 1)
        EXPECT_MSG(std::fabs(fx::localTime(0.0f, 0.1f, 2.0f) -
                             fx::localTime(0.0f, 0.13f, 2.0f)) > 1e-3f,
                   "particle_emitter_cria: as sementes NÃO desafazem o "
                   "tempo (todas nascem juntas)");
        const f32 p0[3] = {1.0f, 2.0f, 3.0f};
        const f32 vel[3] = {0.0f, 3.0f, 0.0f};
        const f32 grav[3] = {0.0f, -4.0f, 0.0f};
        f32 p[3];
        fx::particlePos(p0, vel, grav, 1.0f, p);
        EXPECT_MSG(nearEq(p[0], 1.0f, 1e-5f) && nearEq(p[1], 3.0f, 1e-5f) &&
                       nearEq(p[2], 3.0f, 1e-5f),
                   "particle_emitter_cria: a balística (1+0, 2+3−2, 3+0)");
        EXPECT_MSG(nearEq(fx::lifeProgress(0.5f, 2.0f), 0.25f, 1e-6f),
                   "particle_emitter_cria: o progresso da vida");
        EXPECT_MSG(nearEq(fx::particleSize(0.25f, 0.05f, 0.5f), 0.15f,
                          1e-6f),
                   "particle_emitter_cria: o tamanho interpola");
        const f32 c0[4] = {1, 0, 0, 1};
        const f32 c1[4] = {0, 0, 1, 0};
        f32 cc[4];
        fx::particleColor(c0, c1, 0.5f, cc);
        EXPECT_MSG(nearEq(cc[0], 0.5f, 1e-6f) && nearEq(cc[2], 0.5f, 1e-6f),
                   "particle_emitter_cria: a cor interpola");
        // A PROVA DO M-Yf1: o mesmo tempo local em tempos DIFERENTES move
        // a partícula (o uTime vivo = a nuvem anda)
        f32 pa[3], pb[3];
        fx::particlePos(p0, vel, grav, fx::localTime(1.0f, 0.0f, 20.0f), pa);
        fx::particlePos(p0, vel, grav, fx::localTime(4.0f, 0.0f, 20.0f), pb);
        EXPECT_MSG(std::fabs(pb[1] - pa[1]) > 1e-3f,
                   "particle_emitter_cria: o TEMPO NÃO move a partícula "
                   "(parada — M-Yf1 vermelho)");
    }
    // (5) O VS NO FONTE — a fórmula EXATA do prompt, SEM comentários
    // (o stripCppComments; o M-Yf1 morre AQUI pela 2.ª vez)
    {
        std::string src;
        ASSERT(readFile(repoRoot() +
                        "/app/src/main/cpp/render/ParticleShader.h",
                        src));
        const std::string code = test::stripCppComments(src);
        EXPECT_MSG(countSub(code,
                            "float localTime = mod(uTime + (seed * 100.0), "
                            "uLifetime);") == 1,
                   "particle_emitter_cria: a fórmula do TEMPO LOCAL saiu do "
                   "VS (a linha do prompt)");
        EXPECT_MSG(countSub(code, "(uVel * localTime)") == 1 &&
                       countSub(code, "(0.5 * uGrav * localTime * localTime)")
                           == 1,
                   "particle_emitter_cria: a BALÍSTICA saiu do VS");
        EXPECT_MSG(countSub(code,
                            "float lifeProgress = localTime / uLifetime;")
                       == 1,
                   "particle_emitter_cria: o PROGRESSO saiu do VS");
        EXPECT_MSG(countSub(code, "glDrawArraysInstanced") == 0 &&
                       countSub(code, "uRight * (aCorner.x * sz)") == 1,
                   "particle_emitter_cria: o billboard saiu do VS (a base "
                   "da câmara é a do céu)");
    }
    // (6) O DRAW REAL no stub — UM draw call, N instâncias, o divisor do
    // seeds e o uTime ÚLTIMO 1f (o contrato da casa)
    {
        glstub::fb::enabled = true;
        glstub::fb::resetState();
        glstub::reset();
        ParticleRenderer pr;
        ASSERT(pr.init());
        EXPECT_MSG(glstub::stats.lastAttribDivisorIndex == 1 &&
                       glstub::stats.lastAttribDivisorValue == 1,
                   "particle_emitter_cria: o divisor dos seeds (loc 1, 1 "
                   "por INSTÂNCIA) não ficou armado");
        ParticleRenderer::Params pp;
        pp.maxParticles = 500;
        pp.time = 7.25f;
        const DrawStats st = pr.draw(pp);
        EXPECT_MSG(st.drawCalls == 1 && st.vertices == 2000u,
                   "particle_emitter_cria: o draw NÃO é 1 call × 2000 "
                   "vértices (calls=%u verts=%u)", st.drawCalls, st.vertices);
        EXPECT_MSG(glstub::stats.drawArraysInstancedCalls == 1 &&
                       glstub::stats.lastInstanceCount == 500,
                   "particle_emitter_cria: o GLES 3.0 não viu 1 "
                   "glDrawArraysInstanced com 500 instâncias");
        EXPECT_MSG(glstub::stats.lastUniform1f == 7.25f,
                   "particle_emitter_cria: o uTime deixou de ser o ÚLTIMO "
                   "1f do draw (%.2f — o contrato da casa)",
                   (double)glstub::stats.lastUniform1f);
        pr.destroy();
        glstub::fb::enabled = false;
        glstub::fb::resetState();
    }
}

// ---- PIN particle_serializa (0.12-Y-F1) ---------------------------------------
// A ordem do dono: «O .goni guarda e carrega as propriedades». O roundtrip
// + os defaults FORA do dump (a fábrica do jorro é silêncio) + a corrupção
// clampada.
TEST(particle_serializa) {
    // (1) O ROUNDTRIP dos campos (os não-defaults entram)
    {
        Scene a;
        const Handle h = a.create("Jorro");
        a.get(h)->addComponent<Transform3D>();
        ParticleEmitter* pe = a.get(h)->addComponent<ParticleEmitter>();
        pe->maxParticles = 1200;
        pe->lifetime = 3.5f;
        pe->startColor[0] = 0.2f;
        pe->endColor[2] = 0.8f;
        pe->startSize = 0.5f;
        pe->endSize = 0.1f;
        pe->velocity[1] = 6.0f;
        pe->gravity[1] = -9.0f;
        pe->texturePath = "textures/particle_smoke.png";
        pe->clampFields();
        const std::string dump = SceneSerializer::dump(a);
        EXPECT_MSG(countSub(dump, "\"ParticleEmitter\"") == 1 &&
                       countSub(dump, "\"maxP\"") == 1 &&
                       countSub(dump, "\"life\"") == 1,
                   "particle_serializa: o .goni NÃO guarda o emissor "
                   "(tipo/máx/vida)");
        EXPECT_MSG(countSub(dump, "\"colA\"") == 1 &&
                       countSub(dump, "\"colB\"") == 1 &&
                       countSub(dump, "\"szA\"") == 1 &&
                       countSub(dump, "\"szB\"") == 1,
                   "particle_serializa: o .goni NÃO guarda as cores/tamanhos");
        EXPECT_MSG(countSub(dump, "\"vel\"") == 1 &&
                       countSub(dump, "\"grav\"") == 1,
                   "particle_serializa: o .goni NÃO guarda vel/grav");
        EXPECT_MSG(countSub(dump, "\"tex\"") == 1 &&
                   dump.find("particle_smoke.png") != std::string::npos,
                   "particle_serializa: o .goni NÃO guarda a textura");
        Scene b;
        SceneSerializer::LoadCtx ctx;
        ASSERT(SceneSerializer::loadText(b, dump, ctx));
        const Handle hf = b.find("Jorro");
        ASSERT(hf.valid());
        const ParticleEmitter* pe2 =
            b.get(hf)->getComponent<ParticleEmitter>();
        ASSERT(pe2 != nullptr);
        EXPECT_MSG(pe2->maxParticles == 1200 &&
                       nearEq(pe2->lifetime, 3.5f, 1e-5f) &&
                       nearEq(pe2->startColor[0], 0.2f, 1e-5f) &&
                       nearEq(pe2->endColor[2], 0.8f, 1e-5f) &&
                       nearEq(pe2->startSize, 0.5f, 1e-5f) &&
                       nearEq(pe2->endSize, 0.1f, 1e-5f) &&
                       nearEq(pe2->velocity[1], 6.0f, 1e-5f) &&
                       nearEq(pe2->gravity[1], -9.0f, 1e-5f) &&
                       pe2->texturePath == "textures/particle_smoke.png",
                   "particle_serializa: o emissor não volta do .goni");
    }
    // (2) Os DEFAULTS ficam FORA do dump (a fábrica do jorro é silêncio —
    // o .goni antigo carrega igual)
    {
        Scene c;
        const Handle hc = c.create("limpo");
        c.get(hc)->addComponent<Transform3D>();
        c.get(hc)->addComponent<ParticleEmitter>();
        const std::string dump2 = SceneSerializer::dump(c);
        EXPECT_MSG(countSub(dump2, "\"maxP\"") == 0 &&
                       countSub(dump2, "\"life\"") == 0 &&
                       countSub(dump2, "\"colA\"") == 0 &&
                       countSub(dump2, "\"tex\"") == 0,
                   "particle_serializa: os defaults INVADIRAM o .goni");
        Scene d;
        SceneSerializer::LoadCtx ctx2;
        ASSERT(SceneSerializer::loadText(d, dump2, ctx2));
        const ParticleEmitter* pe3 =
            d.get(d.find("limpo"))->getComponent<ParticleEmitter>();
        ASSERT(pe3 != nullptr);
        EXPECT_MSG(pe3->maxParticles == 500 &&
                       nearEq(pe3->lifetime, 2.0f, 1e-6f),
                   "particle_serializa: os ausentes não caem na fábrica");
    }
    // (3) A CORRUPÇÃO clampada (99999 partículas → 2000; vida −1 → 0.1)
    {
        Scene a;
        const Handle h = a.create("corrupto");
        a.get(h)->addComponent<Transform3D>();
        ParticleEmitter* pe = a.get(h)->addComponent<ParticleEmitter>();
        pe->maxParticles = 99999;
        pe->lifetime = -1.0f;
        const std::string dump = SceneSerializer::dump(a);
        Scene b;
        SceneSerializer::LoadCtx ctx;
        ASSERT(SceneSerializer::loadText(b, dump, ctx));
        const ParticleEmitter* pe2 =
            b.get(b.find("corrupto"))->getComponent<ParticleEmitter>();
        ASSERT(pe2 != nullptr);
        EXPECT_MSG(pe2->maxParticles == 2000 &&
                       nearEq(pe2->lifetime, 0.1f, 1e-5f),
                   "particle_serializa: a corrupção não clampou (%d/%.2f)",
                   pe2->maxParticles, (double)pe2->lifetime);
    }
}

// ---- PIN particle_inspector (0.12-Y-F1) ---------------------------------------
// A ordem do dono: «Secção "Particle Emitter" quando o TIC tem o
// componente; sliders para vida/tamanho/cores/gravidade; um botão
// "textura" que abre o Navegador». O PLANO: a secção + 8 linhas SÓ com o
// componente, na ordem, a fórmula em casa e os ids não colidem.
TEST(particle_inspector) {
    const TextMetrics tm{};
    Scene scene;
    const Handle h = scene.create("jorro");
    scene.get(h)->addComponent<Transform3D>();
    scene.get(h)->addComponent<ParticleEmitter>();
    ASSERT(scene.get(h)->getComponent<ParticleEmitter>() != nullptr);

    // (1) O TIC SEM o componente: NENHUMA linha de partículas
    {
        Scene vazio;
        const Handle hv = vazio.create("v");
        vazio.get(hv)->addComponent<Transform3D>();
        InspRow rows[64];
        const u32 n = inspectorPlan(inspectorProfile(*vazio.get(hv)), tm,
                                    false, 0, rows);
        u32 pe = 0;
        for (u32 i = 0; i < n; ++i) {
            if (rows[i].id == kInspectorPeMax || rows[i].id == kInspectorPeTex)
                ++pe;
        }
        EXPECT_MSG(pe == 0,
                   "particle_inspector: SEM o componente a secção ENTROU "
                   "(%u linhas)", pe);
    }
    // (2) COM o componente: a secção + as 8 linhas na ORDEM do dono
    {
        const InspProfile prof = inspectorProfile(*scene.get(h));
        EXPECT_MSG(prof.pe,
                   "particle_inspector: o perfil não vê o componente");
        InspRow rows[64];
        const u32 n = inspectorPlan(prof, tm, false, 0, rows);
        i32 iSec = -1, iMax = -1, iLife = -1, iSzA = -1, iSzB = -1,
            iColA = -1, iColB = -1, iGrav = -1, iTex = -1;
        u32 max = 0, life = 0, tex = 0;
        for (u32 i = 0; i < n; ++i) {
            const InspRow& r = rows[i];
            if (r.id == kInspectorPeMax) { ++max; iMax = (i32)i; }
            if (r.id == kInspectorPeLife) { ++life; iLife = (i32)i; }
            if (r.id == kInspectorPeSizeA) iSzA = (i32)i;
            if (r.id == kInspectorPeSizeB) iSzB = (i32)i;
            if (r.id == kInspectorPeColA) iColA = (i32)i;
            if (r.id == kInspectorPeColB) iColB = (i32)i;
            if (r.id == kInspectorPeGrav) iGrav = (i32)i;
            if (r.id == kInspectorPeTex) { ++tex; iTex = (i32)i; }
            if (r.kind == InspRow::Kind::PeSection) iSec = (i32)i;
        }
        EXPECT_MSG(max == 1 && life == 1 && tex == 1,
                   "particle_inspector: as linhas faltam ou duplicam "
                   "(máx=%u vida=%u tex=%u)", max, life, tex);
        EXPECT_MSG(iSec >= 0 && iMax > iSec && iLife > iMax && iSzA > iLife &&
                       iSzB > iSzA && iColA > iSzB && iColB > iColA &&
                       iGrav > iColB && iTex > iGrav,
                   "particle_inspector: a ORDEM do dono morreu (sec=%d "
                   "máx=%d vida=%d szA=%d szB=%d colA=%d colB=%d grav=%d "
                   "tex=%d)", iSec, iMax, iLife, iSzA, iSzB, iColA, iColB,
                   iGrav, iTex);
        EXPECT_MSG(inspectorRowCount(prof, false, 0) == n,
                   "particle_inspector: a fórmula do rowCount divergiu do "
                   "plano (%u vs %u)", inspectorRowCount(prof, false, 0), n);
        // os ids não colidem (a colisão mata o toque)
        EXPECT_MSG(kInspectorPeMax != kInspectorPeLife &&
                       kInspectorPeColA != kInspectorPeColB &&
                       kInspectorPeGrav != kInspectorPeTex,
                   "particle_inspector: os ids colidem");
    }
    // (3) O CONTRATO no fonte — o toggle do hex (16/17) e a intenção do
    // navegador (o M-Yf2 morre AQUI: o slider que não escreve a
    // propriedade é um adorno)
    {
        std::string ui;
        ASSERT(readFile(repoRoot() + "/app/src/main/cpp/ui/EditorUi.cpp",
                        ui));
        const std::string code = test::stripCppComments(ui);
        EXPECT_MSG(countSub(code, "case InspRow::Kind::PeLife:") == 1 &&
                       countSub(code, "peEdit->lifetime, \"%.1fs\"") == 1,
                   "particle_inspector: o slider da VIDA deixou de escrever "
                   "a propriedade (a linha EXATA do sliderRow — M-Yf2 "
                   "vermelho; a cópia morta não satisfaz)");
        EXPECT_MSG(countSub(code, "peEdit->clampFields()") >= 1,
                   "particle_inspector: os sliders deixaram de clampar");
        EXPECT_MSG(countSub(code, "st.partTexPick = true;") == 1,
                   "particle_inspector: o botão textura deixou de armar a "
                   "intenção do navegador");
        std::string mainSrc;
        ASSERT(readFile(repoRoot() + "/app/src/main/cpp/platform/main.cpp",
                        mainSrc));
        const std::string mainCode = test::stripCppComments(mainSrc);
        EXPECT_MSG(countSub(mainCode,
                                "g_partTexPickPending &&") >= 1 &&
                       countSub(mainCode,
                                "browserImportParticleTex(") >= 1,
                   "particle_inspector: o intercept do pick da textura "
                   "morreu no main");
        std::string ue;
        ASSERT(readFile(repoRoot() + "/app/src/main/cpp/ui/UiEditor.cpp",
                        ue));
        const std::string ueCode = test::stripCppComments(ue);
        EXPECT_MSG(countSub(ueCode, "case 16:") == 1 &&
                       countSub(ueCode, "case 17:") == 1,
                   "particle_inspector: os hexes 16/17 morreram (as cores "
                   "não voltam do teclado)");
    }
}
