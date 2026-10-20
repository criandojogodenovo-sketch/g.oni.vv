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
#include "render/Camera.h"
#include "render/QuadBatch.h"
#include "platform/InputState.h"

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

// ---- PIN pan_rail_sexta -------------------------------------------------------
// A MÃOZINHA é a 6.ª FERRAMENTA do rail: rail[4] = Pan (id 33 — o 38
// aposentado NUNCA volta), rail[5] = Íman; o toque arma o st.panMode; a
// legenda diz "Pan"; a degradação em colunas continua honesta.
TEST(pan_rail_sexta) {
    theme::setDensity(1.0f);
    editor::applyDensity();
    // (1) a geometria: 6 botões numa coluna (viewport alto); o Pan fica
    // ENTRE o Escalar (rail[3]) e o Íman (rail[5]) — "junto ao
    // Mover/Rodar/Escalar" (a ordem do dono)
    const UiRect view{0.0f, 0.0f, 800.0f, 600.0f};
    const vpchrome::Layout L = vpchrome::layout(view);
    ASSERT(L.railVisible);
    EXPECT(nearEq(L.rail[4].w, theme::dp(vpchrome::kRailBtn)));
    EXPECT_MSG(L.rail[4].y > L.rail[3].y,
               "pan_rail_sexta: o Pan (rail[4]) fica SOB o Escalar");
    EXPECT_MSG(nearEq(L.rail[4].y - L.rail[3].y,
                      theme::dp(vpchrome::kRailBtn + vpchrome::kRailGap)),
               "pan_rail_sexta: o passo do rail é 48dp");
    EXPECT_MSG(nearEq(L.rail[5].y - L.rail[4].y,
                      theme::dp(vpchrome::kRailBtn + vpchrome::kRailGap)),
               "pan_rail_sexta: o Íman desceu para rail[5]");
    EXPECT(vpchrome::kVpPanId == 33u);   // o 38 aposentado NUNCA volta
    // o rail inteiro cabe no view (6·40 + 5·8 = 280dp + margens)
    EXPECT(L.rail[5].y + L.rail[5].h <= view.y + view.h + 0.5f);
    // (2) viewport BAIXO: a degradação em 2 colunas (3 rows × 2 cols)
    {
        const vpchrome::Layout L2 = vpchrome::layout(UiRect{0.0f, 0.0f, 800.0f, 220.0f});
        EXPECT(L2.railVisible);
        EXPECT(L2.railCols >= 2u);
        EXPECT(L2.rail[5].w > 0.0f);   // as 6 ferramentas cabem em 2 colunas
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
