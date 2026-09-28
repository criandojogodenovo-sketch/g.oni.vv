// tests/test_camera.cpp — câmara de orbit: clamp de pitch, clamp de zoom,
// eye derivado de yaw/pitch/dist, vista aponta para o target.
// F3.1: clamps generosos (zoom 1..300, pitch ±89°), near/far 0.5/450 contra
// z-fighting, e vista quase top-down sem inversão.
#include "TestFramework.h"
#include "render/Camera.h"
#include <cmath>

using namespace vv;

namespace {

bool nearEq(f32 a, f32 b, f32 eps = 1e-4f) {
    const f32 d = a - b;
    return d < eps && d > -eps;
}

} // namespace

TEST(limites_f31_zoom_1_a_300_e_pitch_89) {
    // spec F3.1: zoom mín ~1.0, zoom máx 300, pitch até ~89°, yaw livre
    EXPECT(Camera::kMinDist == 1.0f);
    EXPECT(Camera::kMaxDist == 300.0f);
    EXPECT(nearEq(Camera::kMaxPitch, 89.0f * 3.14159265f / 180.0f, 1e-3f));
    EXPECT(nearEq(Camera::kMinPitch, -Camera::kMaxPitch));
    EXPECT(Camera::kMinPitch > -1.5707963f);   // nunca chega a −90°
    EXPECT(Camera::kMaxPitch <  1.5707963f);   // nem a +90° (lookAt degenera)
}

TEST(clamp_de_pitch_nao_inverte) {
    Camera c;
    c.orbit(0.0f, 100.0f);
    EXPECT(c.pitch == Camera::kMaxPitch);
    c.orbit(0.0f, -200.0f);
    EXPECT(c.pitch == Camera::kMinPitch);
    // mesmo com many deltas grandes, mantém-se no intervalo aberto ±90°
    for (int i = 0; i < 100; ++i) {
        c.orbit(0.1f, 0.3f);
        EXPECT(c.pitch >= Camera::kMinPitch && c.pitch <= Camera::kMaxPitch);
        EXPECT(std::fabs(c.pitch) < 1.5707963f);
    }
}

TEST(clamp_de_zoom_mantem_distancia_no_intervalo) {
    Camera c;
    c.zoomBy(1e-6f);   // "aproximar infinitamente"
    EXPECT(c.dist == Camera::kMinDist);
    c.zoomBy(1e6f);    // "afastar infinitamente"
    EXPECT(c.dist == Camera::kMaxDist);
}

TEST(near_far_sanity_contra_z_fighting) {
    // spec F3.1: near 0.5, far = zoom máx × 1.5 (≈450); rácio ≤ ~900:1
    // para preservar a precisão do depth de 24 bits
    Camera c;
    EXPECT(nearEq(c.zNear, 0.5f));
    EXPECT(nearEq(c.zFar, Camera::kMaxDist * 1.5f, 1e-3f));
    EXPECT(c.zNear < c.zFar);
    const f32 ratio = c.zFar / c.zNear;
    EXPECT(ratio <= 900.0f);     // 450/0.5 = 900 exato
    EXPECT(ratio < 2000.0f);     // sanity da spec (folga)
    const Mat4 p = c.proj(16.0f / 9.0f);
    for (int i = 0; i < 16; ++i) {
        EXPECT(std::isfinite(p.m[i]));
    }
    EXPECT(nearEq(p.m[11], -1.0f));
    EXPECT(p.m[10] < 0.0f);      // near<far → termo negativo
}

TEST(top_down_quase_total_sem_inversao) {
    Camera c;
    c.setDistance(12.0f);
    // 89°: quase de cima — o lookAt com up (0,1,0) ainda é válido
    c.orbit(0.0f, 100.0f);
    EXPECT(nearEq(c.pitch, Camera::kMaxPitch));
    const Vec3 e = c.eye();
    EXPECT(e.y > 0.0f);          // câmara continua acima do chão
    const Mat4 v = c.view();
    f32 out[4];
    Mat4::transformPoint4(v, c.target, out);
    EXPECT(nearEq(out[0], 0.0f, 1e-3f));
    EXPECT(nearEq(out[1], 0.0f, 1e-3f));
    EXPECT(nearEq(out[2], -12.0f, 1e-2f));   // target à frente, sem espelhar
    // simétrico para baixo
    c.orbit(0.0f, -200.0f);
    EXPECT(c.eye().y < 0.0f);
    const Mat4 v2 = c.view();
    Mat4::transformPoint4(v2, c.target, out);
    EXPECT(nearEq(out[2], -12.0f, 1e-2f));
}

TEST(alvo_visivel_no_zoom_maximo) {
    // a 300 de distância o target fica dentro do frustum (far = 450)
    Camera c;
    c.setDistance(Camera::kMaxDist);
    const Mat4 v = c.view();
    const Mat4 p = c.proj(16.0f / 9.0f);
    const Mat4 vp = Mat4::mul(p, v);
    f32 out[4];
    Mat4::transformPoint4(vp, c.target, out);
    const f32 ndcZ = out[2] / out[3];
    EXPECT(ndcZ > -1.0f && ndcZ < 1.0f);
    // e o alvo continua à frente da near plane (near 0.5 < dist 300)
    EXPECT(c.zNear < c.dist);
}

TEST(eye_dista_dist_do_target) {
    Camera c;
    c.yaw = 0.8f;
    c.pitch = -0.3f;
    c.setDistance(7.5f);
    const Vec3 e = c.eye();
    EXPECT(nearEq(length(e - c.target), 7.5f, 1e-3f));
}

TEST(vista_aponta_para_o_target) {
    Camera c;
    c.yaw = 2.1f;
    c.pitch = 0.6f;
    c.setDistance(4.0f);
    const Mat4 v = c.view();
    f32 out[4];
    Mat4::transformPoint4(v, c.target, out);
    // target no centro da vista: x=y=0 e a distância à frente
    EXPECT(nearEq(out[0], 0.0f, 1e-3f));
    EXPECT(nearEq(out[1], 0.0f, 1e-3f));
    EXPECT(nearEq(out[2], -4.0f, 1e-3f));
}

TEST(orbit_nao_muda_distancia_nem_target) {
    Camera c;
    c.setDistance(6.0f);
    const Vec3 t0 = c.target;
    for (int i = 0; i < 50; ++i) {
        c.orbit(0.07f, -0.05f);
    }
    EXPECT(nearEq(c.dist, 6.0f));
    EXPECT(nearEq(c.target.x, t0.x) && nearEq(c.target.y, t0.y) &&
           nearEq(c.target.z, t0.z));
}

TEST(perspective_da_camara_tem_sanity) {
    Camera c;
    EXPECT(c.zNear < c.zFar);
    EXPECT(c.fovY > 0.0f);
    const Mat4 p = c.proj(16.0f / 9.0f);
    EXPECT(nearEq(p.m[11], -1.0f));
}
