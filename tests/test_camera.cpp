// tests/test_camera.cpp — câmara de orbit: clamp de pitch, clamp de zoom,
// eye derivado de yaw/pitch/dist, vista aponta para o target.
#include "TestFramework.h"
#include "render/Camera.h"

using namespace vv;

namespace {

bool nearEq(f32 a, f32 b, f32 eps = 1e-4f) {
    const f32 d = a - b;
    return d < eps && d > -eps;
}

} // namespace

TEST(clamp_de_pitch_nao_inverte) {
    Camera c;
    c.orbit(0.0f, 100.0f);
    EXPECT(c.pitch == Camera::kMaxPitch);
    c.orbit(0.0f, -200.0f);
    EXPECT(c.pitch == Camera::kMinPitch);
    // mesmo com many deltas grandes, mantém-se no intervalo
    for (int i = 0; i < 100; ++i) {
        c.orbit(0.1f, 0.3f);
        EXPECT(c.pitch >= Camera::kMinPitch && c.pitch <= Camera::kMaxPitch);
    }
}

TEST(clamp_de_zoom_mantem_distancia_no_intervalo) {
    Camera c;
    c.zoomBy(1e-6f);   // "aproximar infinitamente"
    EXPECT(c.dist == Camera::kMinDist);
    c.zoomBy(1e6f);    // "afastar infinitamente"
    EXPECT(c.dist == Camera::kMaxDist);
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
