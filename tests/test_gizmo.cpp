// tests/test_gizmo.cpp — 0.6.9: GIZMOS DE TRANSFORMAÇÃO.
//
// Este TU cobre por ORDEM os commits da sub-fase: MOVER (a), RODAR (b),
// ESCALAR (c). A matemática é GL-free (projeção/raio/hit-test/drag/
// snapping) e o desenho é aferido pelos batches de CPU (mesma técnica do
// test_ui). A câmara é a Camera REAL do engine (render/Camera.h — GL-free).
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include "core/Scene.h"
#include "core/Presets.h"
#include "components/Transform3D.h"
#include "platform/InputState.h"
#include "render/Camera.h"
#include "ui/EditorUi.h"
#include "ui/FontAtlas.h"
#include "ui/Gizmo.h"

using namespace vv;
using namespace vv::gizmo;
using ::test::nearEqF;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;

struct Env {
    Camera cam;                 // orbit default: yaw .6, pitch .5, dist 6
    Mat4  vp;
    ViewBasis basis;
    f32   len = 0.0f;

    Env() {
        const f32 aspect = kSW / kSH;
        vp = Mat4::mul(cam.proj(aspect), cam.view());
        basis = viewBasis(cam, aspect);
        len = gizmoLength(cam.dist);
    }

    // px,py projetados de um ponto do mundo
    bool proj(const Vec3& p, f32& sx, f32& sy) {
        return projectPoint(vp, p, kSW, kSH, sx, sy);
    }
};

} // namespace

// ---- base de vista / projeção (fundação de tudo) ------------------------------

TEST(gizmo_projeta_alvo_no_centro_e_rejeita_atras) {
    Env e;
    f32 sx = 0.0f, sy = 0.0f;
    // o alvo da câmara orbit é a origem → centro do ecrã
    EXPECT(e.proj(Vec3{0.0f, 0.0f, 0.0f}, sx, sy));
    EXPECT(nearEqF(sx, kSW * 0.5f, 2.0f));
    EXPECT(nearEqF(sy, kSH * 0.5f, 2.0f));
    // ponto ATRÁS da câmara (para lá do eye, afastando-se do alvo) → false
    const Vec3 atras = e.cam.eye() +
                       (e.cam.eye() - Vec3{0.0f, 0.0f, 0.0f});
    EXPECT(!e.proj(atras, sx, sy));
}

TEST(gizmo_raio_do_pixel_central_e_dos_cantos) {
    Env e;
    const Vec3 dCenter = screenRayDir(e.basis, kSW * 0.5f, kSH * 0.5f,
                                       kSW, kSH);
    EXPECT(dot(dCenter, e.basis.fwd) > 0.999f);   // centro ≈ forward
    // cantos: dentro do cone de 60° (dot > cos(60°) com margem)
    const Vec3 d00 = screenRayDir(e.basis, 0.0f, 0.0f, kSW, kSH);
    const Vec3 d11 = screenRayDir(e.basis, kSW, kSH, kSW, kSH);
    EXPECT(dot(d00, e.basis.fwd) > 0.5f);
    EXPECT(dot(d11, e.basis.fwd) > 0.5f);
    // right/up coerentes: pixel à direita do centro → direção com +right
    const Vec3 dR = screenRayDir(e.basis, kSW * 0.75f, kSH * 0.5f, kSW, kSH);
    EXPECT(dot(dR, e.basis.right) > 0.0f);
    // pixel abaixo do centro → direção com −up (y de ecrã é para baixo)
    const Vec3 dD = screenRayDir(e.basis, kSW * 0.5f, kSH * 0.75f, kSW, kSH);
    EXPECT(dot(dD, e.basis.up) < 0.0f);
}

// ---- MOVER: hit-test de eixo e de plano ----------------------------------------

TEST(gizmo_mover_hit_test_eixo_x) {
    Env e;
    // toque NO MEIO do eixo X projetado (0.6*len — longe da origem e dos
    // outros eixos projetados)
    f32 px = 0.0f, py = 0.0f;
    EXPECT(e.proj(Vec3{1.0f, 0.0f, 0.0f} * (e.len * 0.6f), px, py));
    const Axis hit = pickAxis(Mode::Move, e.vp, Vec3{0.0f, 0.0f, 0.0f},
                              e.len, kSW, kSH, px, py);
    EXPECT(hit == Axis::X);
    // toque longe do gizmo → None
    const Axis nada = pickAxis(Mode::Move, e.vp, Vec3{0.0f, 0.0f, 0.0f},
                                e.len, kSW, kSH, 40.0f, 60.0f);
    EXPECT(nada == Axis::None);
}

TEST(gizmo_mover_hit_test_eixo_y_e_z) {
    Env e;
    f32 px = 0.0f, py = 0.0f;
    EXPECT(e.proj(Vec3{0.0f, 1.0f, 0.0f} * (e.len * 0.6f), px, py));
    EXPECT(pickAxis(Mode::Move, e.vp, Vec3{}, e.len, kSW, kSH, px, py)
           == Axis::Y);
    EXPECT(e.proj(Vec3{0.0f, 0.0f, 1.0f} * (e.len * 0.6f), px, py));
    EXPECT(pickAxis(Mode::Move, e.vp, Vec3{}, e.len, kSW, kSH, px, py)
           == Axis::Z);
}

TEST(gizmo_mover_hit_test_quad_de_plano_xy) {
    Env e;
    // centro do quad do plano XY: a 62% do len em X e Y
    const Vec3 c = Vec3{1.0f, 1.0f, 0.0f} * (e.len * 0.62f);
    f32 px = 0.0f, py = 0.0f;
    EXPECT(e.proj(c, px, py));
    const Axis hit = pickAxis(Mode::Move, e.vp, Vec3{}, e.len, kSW, kSH,
                              px, py);
    EXPECT(hit == Axis::XY);
}

// ---- MOVER: drag ao longo do eixo ----------------------------------------------

TEST(gizmo_mover_drag_eixo_x_altera_sozinho_o_x) {
    const Vec3 anchor{1.0f, 2.0f, 3.0f};
    // hits sintéticos: arrasto de 1.5 u ao longo de X (com ruído fora do eixo
    // — o drag projeta no eixo, o ruído é ignorado)
    const Vec3 h0{0.0f, 0.0f, 0.0f};
    const Vec3 h1{1.5f, 0.4f, -0.2f};
    const Vec3 p = dragMoveAxis(anchor, Vec3{1.0f, 0.0f, 0.0f}, h0, h1, false);
    EXPECT(nearEqF(p.x, 2.5f));    // 1.0 + 1.5
    EXPECT(nearEqF(p.y, 2.0f));    // intocado
    EXPECT(nearEqF(p.z, 3.0f));
}

TEST(gizmo_mover_drag_eixo_y_e_z) {
    const Vec3 anchor{0.0f, 0.0f, 0.0f};
    const Vec3 h0{0.0f, 0.0f, 0.0f};
    EXPECT(nearEqF(dragMoveAxis(anchor, Vec3{0, 1, 0}, h0,
                                Vec3{0, -2.0f, 0}, false).y, -2.0f));
    EXPECT(nearEqF(dragMoveAxis(anchor, Vec3{0, 0, 1}, h0,
                                Vec3{0, 0, 4.25f}, false).z, 4.25f));
}

TEST(gizmo_mover_drag_snap_ao_grid) {
    const Vec3 anchor{0.0f, 0.0f, 0.0f};
    const Vec3 h0{0.0f, 0.0f, 0.0f};
    const Vec3 X{1.0f, 0.0f, 0.0f};
    // delta 0.3 → 0.5 (grid 0.5)
    EXPECT(nearEqF(dragMoveAxis(anchor, X, h0, Vec3{0.3f, 0, 0}, true).x,
                   0.5f));
    // delta 0.74 → 0.5; delta 0.8 → 1.0
    EXPECT(nearEqF(dragMoveAxis(anchor, X, h0, Vec3{0.74f, 0, 0}, true).x,
                   0.5f));
    EXPECT(nearEqF(dragMoveAxis(anchor, X, h0, Vec3{0.8f, 0, 0}, true).x,
                   1.0f));
    // negativo: -0.26 → -0.5
    EXPECT(nearEqF(dragMoveAxis(anchor, X, h0, Vec3{-0.26f, 0, 0}, true).x,
                   -0.5f));
    // SEM snap: exato
    EXPECT(nearEqF(dragMoveAxis(anchor, X, h0, Vec3{0.33f, 0, 0}, false).x,
                   0.33f));
}

TEST(gizmo_mover_drag_plano_xy_xz_yz) {
    const Vec3 anchor{5.0f, 5.0f, 5.0f};
    const Vec3 h0{0.0f, 0.0f, 0.0f};
    // plano XY (n=Z): só x,y mudam
    Vec3 p = dragMovePlane(anchor, Vec3{0.0f, 0.0f, 1.0f}, h0,
                           Vec3{1.2f, 0.7f, 9.0f}, false);
    EXPECT(nearEqF(p.x, 6.2f));
    EXPECT(nearEqF(p.y, 5.7f));
    EXPECT(nearEqF(p.z, 5.0f));    // z intocado
    // plano XZ (n=Y)
    p = dragMovePlane(anchor, Vec3{0.0f, 1.0f, 0.0f}, h0,
                      Vec3{1.0f, 8.0f, -2.0f}, false);
    EXPECT(nearEqF(p.x, 6.0f));
    EXPECT(nearEqF(p.y, 5.0f));
    EXPECT(nearEqF(p.z, 3.0f));
    // plano YZ (n=X)
    p = dragMovePlane(anchor, Vec3{1.0f, 0.0f, 0.0f}, h0,
                      Vec3{-4.0f, 0.5f, 0.25f}, false);
    EXPECT(nearEqF(p.x, 5.0f));
    EXPECT(nearEqF(p.y, 5.5f));
    EXPECT(nearEqF(p.z, 5.25f));
}

// ---- MOVER: gestos — drag em gizmo NÃO orbita a câmara -------------------------

TEST(gizmo_mover_drag_em_gizmo_nao_orbita) {
    Env e;
    // o mecanismo do main: press no eixo X do gizmo → pickAxis != None →
    // o slot é CLAIMED → updateCameraOrbit ignora esse dedo
    f32 px = 0.0f, py = 0.0f;
    EXPECT(e.proj(Vec3{1.0f, 0.0f, 0.0f} * (e.len * 0.6f), px, py));
    const Axis hit = pickAxis(Mode::Move, e.vp, Vec3{}, e.len, kSW, kSH,
                              px, py);
    EXPECT(hit != Axis::None);   // o dedo está MESMO no gizmo
    const u32 claimed = (hit != Axis::None) ? 1u : 0u;

    InputState in;
    vv::editor::OrbitState orbit;
    const f32 yaw0 = e.cam.yaw;
    in.injectDown(0, px, py);
    vv::editor::updateCameraOrbit(e.cam, orbit, in,
                                  vv::editor::centerRect(kSW, kSH,
                                                         vv::safe::Insets{}),
                                  claimed, false);
    in.injectMove(0, px + 200.0f, py + 100.0f);
    vv::editor::updateCameraOrbit(e.cam, orbit, in,
                                  vv::editor::centerRect(kSW, kSH,
                                                         vv::safe::Insets{}),
                                  claimed, false);
    EXPECT(nearEqF(e.cam.yaw, yaw0));   // câmara intacta (slot do gizmo)

    // contraste: SEM claim o mesmo drag orbita
    Camera cam2;
    vv::editor::OrbitState orbit2;
    InputState in2;
    in2.injectDown(0, 800.0f, 360.0f);
    vv::editor::updateCameraOrbit(cam2, orbit2, in2,
                                  vv::editor::centerRect(kSW, kSH,
                                                         vv::safe::Insets{}),
                                  0, false);
    in2.injectMove(0, 900.0f, 400.0f);
    vv::editor::updateCameraOrbit(cam2, orbit2, in2,
                                  vv::editor::centerRect(kSW, kSH,
                                                         vv::safe::Insets{}),
                                  0, false);
    EXPECT(cam2.yaw != 0.60f);   // orbitou (a máscara funciona ao contrário)
}

// ---- visibilidade (nunca em PLAY) ----------------------------------------------

TEST(gizmo_nunca_em_play_e_exige_selecao) {
    EXPECT(!visible(/*playMode=*/true, /*hasSelection=*/true));    // play: NUNCA
    EXPECT(!visible(/*playMode=*/true, /*hasSelection=*/false));
    EXPECT(!visible(/*playMode=*/false, /*hasSelection=*/false));  // sem seleção
    EXPECT(visible(/*playMode=*/false, /*hasSelection=*/true));    // editor + TIC
}

// ---- desenho (MOVER): setas + planos emitem linhas/quads ------------------------

TEST(gizmo_mover_desenho_emite_linhas) {
    Env e;
    FontAtlas font;
    const char* path = FONT_FIXTURE;
    EXPECT(font.loadFromPaths(&path, 1, 28.0f));
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    ui.setSafeArea(vv::safe::Insets{});
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    drawGizmo(ui, e.vp, Vec3{}, e.len, Mode::Move, Axis::None);
    const u32 quads = ui.solidsForTest().vertexCount() / 6;
    // 3 setas (haste + 2 traços da ponta = 3 linhas cada) + 3 planos
    // (4 segmentos cada) = 9 + 12 = 21 quads no mínimo
    EXPECT(quads >= 21u);
    ui.endFrame();
}
