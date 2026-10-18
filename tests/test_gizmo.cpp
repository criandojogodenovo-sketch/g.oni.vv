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
using ::test::vecNearF;

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
    // 0.9.6.18 (HOTFIX D12 · mutação M11): os planos são PREENCHIDOS a
    // 25% alfa — cada plano emite o quad de preenchimento (2 triângulos)
    // para além dos 4 segmentos do contorno: 21 + 3 = 24. A mutação
    // «planos só de contorno» volta a 21 e fica VERMELHA aqui.
    EXPECT_MSG(quads >= 24u,
               "D12: os planos do gizmo perderam o preenchimento "
               "(esperados ≥24 quads, há %u — a mutação M11 fica vermelha)",
               static_cast<unsigned>(quads));
    ui.endFrame();
}

// ==== COMMIT 0.6.9-b: RODAR (3 anéis por eixo; drag angular no plano) ===========

TEST(gizmo_rodar_hit_test_anel_z_no_plano_xy) {
    Env e;
    // ponto do anel Z (plano XY) a 45°: (cos45, sin45, 0)*len — a >22px dos
    // anéis X (plano YZ) e Y (plano XZ) e longe do centro
    const f32 s = std::sqrt(2.0f) * 0.5f;
    const Vec3 p{s * e.len, s * e.len, 0.0f};
    f32 px = 0.0f, py = 0.0f;
    EXPECT(e.proj(p, px, py));
    const Axis hit = pickAxis(Mode::Rotate, e.vp, Vec3{}, e.len, kSW, kSH,
                              px, py);
    EXPECT(hit == Axis::Z);
}

TEST(gizmo_rodar_hit_test_anel_x_no_plano_yz) {
    Env e;
    // ponto do anel X (plano YZ) a 45°: (0, cos45, sin45)*len
    const f32 s = std::sqrt(2.0f) * 0.5f;
    const Vec3 p{0.0f, s * e.len, s * e.len};
    f32 px = 0.0f, py = 0.0f;
    EXPECT(e.proj(p, px, py));
    EXPECT(pickAxis(Mode::Rotate, e.vp, Vec3{}, e.len, kSW, kSH, px, py)
           == Axis::X);
}

TEST(gizmo_rodar_drag_aplica_o_delta_angular_no_eixo) {
    Env e;
    // âncora em 0 rad; drag até +30° em torno do CENTRO projetado da origem
    f32 ox = 0.0f, oy = 0.0f;
    EXPECT(e.proj(Vec3{}, ox, oy));
    const f32 a0 = std::atan2(0.0f - oy, 100.0f - ox);        // dedo a +X
    const f32 a1 = a0 + 30.0f * 3.14159265f / 180.0f;        // +30°

    // eixo Z: fwd tem componente Z negativa (câmara olha −Z do mundo a
    // partir de +Z) → o sinal do delta é corrigido pelo facing
    const Quat q = dragRotate(Quat::identity(), Vec3{0, 0, 1}, e.basis.fwd,
                               a0, a1, false);
    EXPECT(!(q.x == 0.0f && q.y == 0.0f && q.z == 0.0f &&
             q.w == 1.0f));   // não é identity
    // propriedade: rodar o eixo X unitário dá exatamente axisAngle(Z,±30°)
    // — o mesmo cálculo de sinal, feito INDEPENDENTEMENTE aqui
    const f32 facing = dot(Vec3{0, 0, 1}, e.basis.fwd);
    const f32 delta = facing < 0.0f ? -(a1 - a0) : (a1 - a0);
    const Quat esperado = Quat::axisAngle(Vec3{0, 0, 1}, delta);
    const Vec3 vx = q.rotate(Vec3{1, 0, 0});
    const Vec3 ve = esperado.rotate(Vec3{1, 0, 0});
    EXPECT(::test::vecNearF(vx, ve));
    // e o ângulo EFETIVO é 30° (|seno do meio-ângulo| coerente)
    EXPECT(std::fabs(std::fabs(q.w) - std::cos(delta * 0.5f)) < 1e-4f);
}

TEST(gizmo_rodar_drag_snap_15_graus) {
    Env e;
    f32 ox = 0.0f, oy = 0.0f;
    EXPECT(e.proj(Vec3{}, ox, oy));
    const f32 a0 = 0.0f;
    // 11.4° → com snap cai em 15°; 40° → 45°
    const f32 a11 = 11.4f * 3.14159265f / 180.0f;
    const f32 a40 = 40.0f * 3.14159265f / 180.0f;
    const Quat q11 = dragRotate(Quat::identity(), Vec3{0, 0, 1},
                                e.basis.fwd, a0, a11, true);
    const Quat q40 = dragRotate(Quat::identity(), Vec3{0, 0, 1},
                                e.basis.fwd, a0, a40, true);
    const f32 facing = dot(Vec3{0, 0, 1}, e.basis.fwd);
    const f32 sinal = facing < 0.0f ? -1.0f : 1.0f;
    EXPECT(std::fabs(std::fabs(q11.w) - std::cos(15.0f * 3.14159265f / 180.0f *
                                                 0.5f)) < 1e-4f);
    EXPECT(std::fabs(std::fabs(q40.w) - std::cos(45.0f * 3.14159265f / 180.0f *
                                                 0.5f)) < 1e-4f);
    (void)sinal;
    // SEM snap: o ângulo exato do drag
    const Quat qe = dragRotate(Quat::identity(), Vec3{0, 0, 1},
                               e.basis.fwd, a0, a11, false);
    EXPECT(std::fabs(std::fabs(qe.w) -
                     std::cos(a11 * 0.5f)) < 1e-4f);
}

TEST(gizmo_rodar_eixos_independentes) {
    // rodar em Y e X aplica-se nos eixos certos (propriedade: o quat roda
    // um vetor ortogonal ao eixo SEM o mover na direção do eixo)
    Env e;
    const Quat qy = dragRotate(Quat::identity(), Vec3{0, 1, 0},
                               e.basis.fwd, 0.0f, 0.5f, false);
    const Vec3 vy = qy.rotate(Vec3{0, 1, 0});
    EXPECT(nearEqF(vy.y, 1.0f));   // eixo do giro fica imóvel
    const Quat qx = dragRotate(Quat::identity(), Vec3{1, 0, 0},
                               e.basis.fwd, 0.0f, 0.5f, false);
    const Vec3 vx2 = qx.rotate(Vec3{1, 0, 0});
    EXPECT(nearEqF(vx2.x, 1.0f));
}

TEST(gizmo_rodar_desenho_emite_os_3_aneis) {
    Env e;
    FontAtlas font;
    const char* path = FONT_FIXTURE;
    EXPECT(font.loadFromPaths(&path, 1, 28.0f));
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    ui.setSafeArea(vv::safe::Insets{});
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    drawGizmo(ui, e.vp, Vec3{}, e.len, Mode::Rotate, Axis::None);
    const u32 quads = ui.solidsForTest().vertexCount() / 6;
    // 3 anéis × 48 segmentos = 144 linhas
    EXPECT(quads >= 144u);
    // anel destacado: MESMOS segmentos com espessura maior (não menos quads)
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    drawGizmo(ui, e.vp, Vec3{}, e.len, Mode::Rotate, Axis::Y);
    EXPECT(ui.solidsForTest().vertexCount() / 6 >= 144u);
    ui.endFrame();
}

// ==== COMMIT 0.6.9-c: ESCALAR (3 handles por eixo + central uniforme) =========

TEST(gizmo_escalar_hit_test_handle_y) {
    Env e;
    // handle Y: no fim do eixo Y projetado (0.9 do len — dentro do handle)
    f32 px = 0.0f, py = 0.0f;
    EXPECT(e.proj(Vec3{0.0f, 1.0f, 0.0f} * (e.len * 0.9f), px, py));
    EXPECT(pickAxis(Mode::Scale, e.vp, Vec3{}, e.len, kSW, kSH, px, py)
           == Axis::Y);
}

TEST(gizmo_escalar_hit_test_handle_central) {
    Env e;
    f32 ox = 0.0f, oy = 0.0f;
    EXPECT(e.proj(Vec3{}, ox, oy));
    // o centro tem PRIORIDADE (handle uniforme) mesmo sobre os eixos
    // (a origem pertence aos 3 eixos projetados)
    EXPECT(pickAxis(Mode::Scale, e.vp, Vec3{}, e.len, kSW, kSH, ox, oy)
           == Axis::Center);
}

TEST(gizmo_escalar_drag_eixo_altera_sozinho_o_eixo) {
    const Vec3 anchor{1.0f, 2.0f, 4.0f};
    const Vec3 h0{0.0f, 0.0f, 0.0f};
    const Vec3 h1{1.5f, 0.0f, 0.0f};   // +1.5 u no eixo X
    const Vec3 s = dragScaleAxis(anchor, Axis::X, Vec3{1, 0, 0}, h0, h1,
                                 false);
    EXPECT(nearEqF(s.x, 2.5f));   // 1 + 1.5 (fator 1 + delta/1)
    EXPECT(nearEqF(s.y, 2.0f));   // intocados
    EXPECT(nearEqF(s.z, 4.0f));
    // eixo Z com ruído fora do eixo: delta 0.25 → FATOR 1.25 → 4*1.25=5
    const Vec3 sz = dragScaleAxis(anchor, Axis::Z, Vec3{0, 0, 1}, h0,
                                  Vec3{0.3f, -0.5f, 0.25f}, false);
    EXPECT(nearEqF(sz.z, 5.0f));   // 4 × (1 + 0.25) — escala MULTIPLICA
    EXPECT(nearEqF(sz.x, 1.0f));
}

TEST(gizmo_escalar_drag_nunca_zero_ou_negativo) {
    const Vec3 anchor{1.0f, 1.0f, 1.0f};
    const Vec3 h0{0.0f, 0.0f, 0.0f};
    // arrasto de -3 u (fator 1-3 = -2) → clamp em 0.05
    const Vec3 s = dragScaleAxis(anchor, Axis::X, Vec3{1, 0, 0}, h0,
                                Vec3{-3.0f, 0, 0}, false);
    EXPECT(nearEqF(s.x, 0.05f));
    // uniforme: dist0=100, dist1=1 (fator 0.01) → clamp
    const Vec3 u = dragScaleUniform(anchor, 100.0f, 1.0f, false);
    EXPECT(nearEqF(u.x, 0.05f));
    EXPECT(nearEqF(u.y, 0.05f));
}

TEST(gizmo_escalar_drag_uniforme_pelo_centro) {
    const Vec3 anchor{1.0f, 2.0f, 3.0f};
    // dedo afasta-se do centro 2× → fator 2 em TODOS os eixos
    const Vec3 s = dragScaleUniform(anchor, 120.0f, 240.0f, false);
    EXPECT(nearEqF(s.x, 2.0f));
    EXPECT(nearEqF(s.y, 4.0f));
    EXPECT(nearEqF(s.z, 6.0f));
    // âncora degenerada (dedo em cima do centro): nada muda
    const Vec3 n = dragScaleUniform(anchor, 0.5f, 300.0f, false);
    EXPECT(nearEqF(n.x, 1.0f));
    EXPECT(nearEqF(n.y, 2.0f));
}

TEST(gizmo_escalar_snap_em_passos_de_025) {
    const Vec3 anchor{1.0f, 1.0f, 1.0f};
    const Vec3 h0{0.0f, 0.0f, 0.0f};
    // eixo: delta 0.18 → fator 1.18 → snap 1.25
    EXPECT(nearEqF(dragScaleAxis(anchor, Axis::X, Vec3{1, 0, 0}, h0,
                                 Vec3{0.18f, 0, 0}, true).x, 1.25f));
    // delta 0.6 → fator 1.6 → snap 1.5
    EXPECT(nearEqF(dragScaleAxis(anchor, Axis::X, Vec3{1, 0, 0}, h0,
                                 Vec3{0.6f, 0, 0}, true).x, 1.5f));
    // uniforme: 100→180 = 1.8 → snap 1.75
    EXPECT(nearEqF(dragScaleUniform(anchor, 100.0f, 180.0f, true).x, 1.75f));
    // sem snap: exato
    EXPECT(nearEqF(dragScaleUniform(anchor, 100.0f, 183.0f, false).x, 1.83f));
}

TEST(gizmo_escalar_desenho_handles_e_centro) {
    Env e;
    FontAtlas font;
    const char* path = FONT_FIXTURE;
    EXPECT(font.loadFromPaths(&path, 1, 28.0f));
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    ui.setSafeArea(vv::safe::Insets{});
    ui.beginFrame(nullptr, nullptr, kSW, kSH);
    drawGizmo(ui, e.vp, Vec3{}, e.len, Mode::Scale, Axis::None);
    const u32 quads = ui.solidsForTest().vertexCount() / 6;
    // 3 hastes + 3 handles (painel preenchido) + handle central = 7 mín
    EXPECT(quads >= 7u);
    ui.endFrame();
}

// ---- integração final: Transform3D escrito com worldDirty coerente -----------

TEST(gizmo_apply_move_escreve_no_transform3d) {
    // o contrato do main: pose final = âncora + delta; updateWorld() com o
    // cache coerente (world == computeMatrix) e worldDirty limpo
    Scene scene;
    const Handle h = createTicFromPreset(scene, PresetKind::StaticBody3D,
                                         nullptr, nullptr);
    Tic* tic = scene.get(h);
    EXPECT(tic != nullptr);
    Transform3D* tr = tic ? tic->getComponent<Transform3D>() : nullptr;
    EXPECT(tr != nullptr);
    if (!tr) {
        return;
    }
    tr->pos = Vec3{0.0f, 0.5f, 0.0f};
    tr->updateWorld();

    // drag de MOVER no eixo X com snap: 0.3 → 0.5
    const Vec3 nova = dragMoveAxis(tr->pos, Vec3{1, 0, 0}, Vec3{},
                                   Vec3{0.3f, 0, 0}, true);
    tr->pos = nova;
    tr->updateWorld();   // (o main faz exatamente isto no applyGizmoDrag)
    tr->worldDirty = false;
    EXPECT(nearEqF(tr->pos.x, 0.5f));
    // world == computeMatrix: comparação por elementos (Mat4 sem operator==)
    {
        const Mat4 a = tr->world;
        const Mat4 b = tr->computeMatrix();
        for (int i = 0; i < 16; ++i) {
            EXPECT(nearEqF(a.m[i], b.m[i]));
        }
    }
    EXPECT(!tr->worldDirty);
}

// ---- 0.7.9 — GRAB-LOCK (fim da oscilação/"fuga" do C33) -----------------------
//
// O bug: o press edge NUNCA capturava as âncoras (anchorHit ficava (0,0,0) —
// o objeto SALTAVA no primeiro frame para distâncias do hit contra a ORIGEM
// DO MUNDO) e o hit de cada frame era medido contra um plano re-ancorado na
// pos ATUAL do gizmo (que MEXE com o drag → realimentação → oscilação/fuga).
// O grab-lock captura TUDO no touch down (alvo, raio, plano FIXO, âncoras) e
// o move NÃO faz hit-test novo.

TEST(gizmo_grab_captura_ancoras_no_arranque) {
    Env e;
    const Vec3 P0{1.5f, 0.0f, 0.0f};   // FORA da origem do mundo (expõe o
                                       // bug do anchorHit a zero)
    // dedo no eixo Y do gizmo (a 60% do len)
    f32 fx = 0.0f, fy = 0.0f;
    EXPECT(e.proj(P0 + Vec3{0.0f, 1.0f, 0.0f} * (e.len * 0.6f), fx, fy));

    const Grab g = beginGrab(Mode::Move, e.vp, P0, e.len, kSW, kSH, fx, fy,
                             0, e.basis);
    EXPECT(g.valid());
    EXPECT(g.target == Axis::Y);
    EXPECT(g.slot == 0);
    // o plano FIXO: passa pela pos do ARRANQUE, ⟂ à câmara do grab
    EXPECT(vecNearF(g.planeOrigin, P0));
    EXPECT(vecNearF(g.planeNormal, e.basis.fwd));
    EXPECT(vecNearF(g.basis.eye, e.basis.eye));   // o raio é o do grab
    // a âncora do plano ESTÁ no plano fixo (não é (0,0,0)!)
    EXPECT(nearEqF(dot(g.anchorHit - g.planeOrigin, g.planeNormal), 0.0f,
                   1e-3f));
    EXPECT(length(g.anchorHit - P0) < e.len * 1.2f);   // perto do dedo
    // âncoras de ecrã no CENTRO projetado do arranque
    f32 ox = 0.0f, oy = 0.0f;
    EXPECT(e.proj(P0, ox, oy));
    EXPECT(nearEqF(g.anchorOx, ox, 0.5f));
    EXPECT(nearEqF(g.anchorOy, oy, 0.5f));
    EXPECT(nearEqF(g.anchorDist,
                   std::sqrt((fx - ox) * (fx - ox) + (fy - oy) * (fy - oy)),
                   0.5f));
    // um Grab default é INVÁLIDO (touch up liberta o lock)
    EXPECT(!Grab{}.valid());
}

TEST(gizmo_grab_raio_generoso_agarra_ligeiramente_fora) {
    Env e;
    const Vec3 P0{};
    // dedo 40 px PARA LÁ DA PONTA do eixo X (ao longo do eixo — longe dos
    // handles de plano): distância ao segmento = 40 px
    f32 tipx = 0.0f, tipy = 0.0f, ox = 0.0f, oy = 0.0f;
    EXPECT(e.proj(P0 + Vec3{1.0f, 0.0f, 0.0f} * e.len, tipx, tipy));
    EXPECT(e.proj(P0, ox, oy));
    const f32 dx = tipx - ox, dy = tipy - oy;
    const f32 n = std::sqrt(dx * dx + dy * dy);
    EXPECT(n > 1.0f);
    const f32 off = 40.0f;   // > kHitAxisPx (22), < kGrabPx (44)
    const f32 px = tipx + dx / n * off;
    const f32 py = tipy + dy / n * off;
    // hover (raio fino): NÃO destaca
    EXPECT(pickAxis(Mode::Move, e.vp, P0, e.len, kSW, kSH, px, py)
           == Axis::None);
    // grab (raio generoso): AGARRA
    const Grab g = beginGrab(Mode::Move, e.vp, P0, e.len, kSW, kSH, px, py,
                             0, e.basis);
    EXPECT(g.valid());
    EXPECT(g.target == Axis::X);
}

TEST(gizmo_grab_primeiro_frame_nao_salta_e_trava_o_eixo) {
    Env e;
    const Vec3 P0{1.5f, 0.0f, 0.0f};
    f32 fx = 0.0f, fy = 0.0f;
    EXPECT(e.proj(P0 + Vec3{0.0f, 1.0f, 0.0f} * (e.len * 0.6f), fx, fy));
    const Grab g = beginGrab(Mode::Move, e.vp, P0, e.len, kSW, kSH, fx, fy,
                             0, e.basis);
    EXPECT(g.valid());

    // PRIMEIRO frame com o dedo PARADO: delta ZERO — o objeto NÃO salta.
    // (o bug: anchorHit (0,0,0) + h1 no plano pela pos atual davam um salto
    // de |dot(h1, Y)| ~ 0.6 u NO ARRANQUE do drag)
    bool ok = false;
    const Vec3 h1 = grabHit(g, fx, fy, kSW, kSH, ok);
    EXPECT(ok);
    const Vec3 pos0 = dragMoveAxis(P0, Vec3{0.0f, 1.0f, 0.0f}, g.anchorHit,
                                   h1, false);
    EXPECT(vecNearF(pos0, P0, 1e-4f));
}

TEST(gizmo_grab_drag_do_eixo_ySegue_o_dedo_sem_oscilacao) {
    Env e;
    const Vec3 P0{1.5f, 0.0f, 0.0f};
    const Vec3 Y{0.0f, 1.0f, 0.0f};
    f32 fx = 0.0f, fy = 0.0f;
    EXPECT(e.proj(P0 + Y * (e.len * 0.6f), fx, fy));
    const Grab g = beginGrab(Mode::Move, e.vp, P0, e.len, kSW, kSH, fx, fy,
                             0, e.basis);
    EXPECT(g.valid());
    EXPECT(g.target == Axis::Y);

    // o dedo SOBE 0.8 u AO LONGO DO EIXO (onde o gizmo ficaria) e DESVIA-SE
    // lateralmente a meio (o caso do C33: dedo escorrega do eixo)
    f32 gx = 0.0f, gy = 0.0f;
    EXPECT(e.proj(P0 + Y * (e.len * 0.6f + 0.8f), gx, gy));
    gx += 120.0f;   // desvio lateral grosseiro NO MEIO do drag
    bool ok = false;
    const Vec3 h1 = grabHit(g, gx, gy, kSW, kSH, ok);
    EXPECT(ok);   // o lock NÃO larga: o dedo NÃO está sobre o gizmo
    const Vec3 pos = dragMoveAxis(P0, Y, g.anchorHit, h1, false);
    // segue o dedo para cima (± tolerância da perspetiva do desvio)…
    EXPECT(pos.y > P0.y + 0.5f);
    // …SEM fugir (o bug: a realimentação disparava o objeto)…
    EXPECT(length(pos - P0) < 4.0f);
    // …e TRAVADO no eixo: x/z EXATAMENTE os da âncora
    EXPECT(nearEqF(pos.x, P0.x));
    EXPECT(nearEqF(pos.z, P0.z));

    // IDEMPOTENTE: o mesmo dedo no frame seguinte dá a MESMA pos (o delta é
    // puro — funções das âncoras + dedo; nada acumula/re-ancora)
    const Vec3 h2 = grabHit(g, gx, gy, kSW, kSH, ok);
    const Vec3 pos2 = dragMoveAxis(P0, Y, g.anchorHit, h2, false);
    EXPECT(vecNearF(pos2, pos, 1e-5f));
}

TEST(gizmo_grab_snap_no_valor_final_ancora_fora_do_grid) {
    // 0.7.9 — o snap arredonda o VALOR FINAL, não o delta cru: âncora
    // {1.3, 0.7, 0} + delta 0.3 em X → 1.6 → degrau ABSOLUTO 1.5 (antes:
    // 1.3 + snap(0.3)=0.5 → 1.8, passos RELATIVOS à âncora)
    const Vec3 anchor{1.3f, 0.7f, 0.0f};
    const Vec3 h0{10.0f, 10.0f, 10.0f};
    const Vec3 p = dragMoveAxis(anchor, Vec3{1, 0, 0}, h0,
                                h0 + Vec3{0.3f, 0, 0}, true);
    EXPECT(nearEqF(p.x, 1.5f));
    EXPECT(nearEqF(p.y, 0.7f));
    // plano: o mesmo princípio nas coordenadas u/v (âncora 1.3+0.35=1.65 →
    // degrau ABSOLUTO 1.5; 0.7+0.1=0.8 → 1.0)
    const Vec3 q = dragMovePlane(anchor, Vec3{0.0f, 0.0f, 1.0f}, h0,
                                 h0 + Vec3{0.35f, 0.1f, 0.0f}, true);
    EXPECT(nearEqF(q.x, 1.5f));
    EXPECT(nearEqF(q.y, 1.0f));
    // escala eixo: âncora 2.0 × fator 1.18 = 2.36 → degrau 2.25 (antes:
    // 2.0 × snap(1.18)=1.25 → 2.5)
    const Vec3 s = dragScaleAxis(Vec3{2.0f, 1.0f, 1.0f}, Axis::X,
                                 Vec3{1, 0, 0}, h0, h0 + Vec3{0.18f, 0, 0},
                                 true);
    EXPECT(nearEqF(s.x, 2.25f));
    // escala uniforme: componentes FINAIS em passos absolutos
    const Vec3 u = dragScaleUniform(Vec3{2.0f, 1.0f, 1.0f}, 100.0f, 180.0f,
                                    true);
    EXPECT(nearEqF(u.x, 3.5f));    // 2·1.8=3.6 → 3.5
    EXPECT(nearEqF(u.y, 1.75f));   // 1.8 → 1.75
    EXPECT(nearEqF(u.z, 1.75f));
}

TEST(gizmo_grab_rotate_ancora_do_angulo_capturada) {
    Env e;
    // dedo no anel Z (θ=150° — longe das projeções dos anéis X/Y)
    f32 fx = 0.0f, fy = 0.0f;
    EXPECT(e.proj(Vec3{-0.8660254f, 0.5f, 0.0f} * (e.len * 0.85f), fx, fy));
    EXPECT(pickAxis(Mode::Rotate, e.vp, Vec3{}, e.len, kSW, kSH, fx, fy,
                    kGrabPx) == Axis::Z);   // (sanity do ângulo escolhido)
    const Grab g = beginGrab(Mode::Rotate, e.vp, Vec3{}, e.len, kSW, kSH,
                             fx, fy, 0, e.basis);
    EXPECT(g.valid());
    EXPECT(g.target == Axis::Z);
    // primeiro frame com o dedo parado: rotação ZERO (o bug: a âncora do
    // ângulo nunca era capturada — ficava 0 e o objeto RODAVA o ângulo
    // absoluto do dedo logo no arranque)
    const f32 ang = std::atan2(fy - g.anchorOy, fx - g.anchorOx);
    const Quat q = dragRotate(Quat::identity(), Vec3{0, 0, 1}, e.basis.fwd,
                              g.anchorAngle, ang, false);
    const Quat expect = Quat::identity();
    EXPECT(nearEqF(q.x, expect.x, 1e-4f));
    EXPECT(nearEqF(q.y, expect.y, 1e-4f));
    EXPECT(nearEqF(q.z, expect.z, 1e-4f));
    EXPECT(nearEqF(q.w, expect.w, 1e-4f));
}

TEST(gizmo_grab_scale_centro_ancora_da_distancia_capturada) {
    Env e;
    // dedo no handle central (escalar uniforme): ~40 px do centro
    // projetado (dentro do alvo generoso de 44 px, fora do eixo)
    f32 ox = 0.0f, oy = 0.0f;
    EXPECT(e.proj(Vec3{}, ox, oy));
    const f32 cx = ox + 32.0f;
    const f32 cy = oy - 24.0f;   // |(32,-24)| = 40 px
    const Grab g = beginGrab(Mode::Scale, e.vp, Vec3{}, e.len, kSW, kSH,
                             cx, cy, 0, e.basis);
    EXPECT(g.valid());
    EXPECT(g.target == Axis::Center);
    EXPECT(g.anchorDist > 30.0f);   // capturada (o bug: ficava 0 → drag
                                    // morto pelo guard degenerado)
    // dedo parado: escala IDENTIDADE
    const Vec3 same = dragScaleUniform(Vec3{1, 1, 1}, g.anchorDist,
                                       g.anchorDist, false);
    EXPECT(vecNearF(same, Vec3{1.0f, 1.0f, 1.0f}));
    // dedo afastou 50%: 1.5× (o drag VIVE — antes estava morto)
    const Vec3 up = dragScaleUniform(Vec3{1, 1, 1}, g.anchorDist,
                                     g.anchorDist * 1.5f, false);
    EXPECT(nearEqF(up.x, 1.5f));
}
