// tests/test_camera.cpp — câmara de orbit: clamp de pitch, clamp de zoom,
// eye derivado de yaw/pitch/dist, vista aponta para o target.
// F3.1: clamps generosos, pitch ±89°, near/far contra z-fighting.
// 0.8.9 (ESPAÇO SEM TETOS): zoom 0.01..100 000, near/far DINÂMICOS
// (setClips — defaults F3.1 mantidos p/ compat), far contém a cena.
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

TEST(limites_089_zoom_amplo_e_pitch_89) {
    // spec 0.8.9: zoom 0.01 → 100 000 (era 1..300), pitch até ~89°, yaw livre
    EXPECT(Camera::kMinDist == 0.01f);
    EXPECT(Camera::kMaxDist == 100000.0f);
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
    c.zoomBy(1e-9f);   // "aproximar infinitamente"
    EXPECT(c.dist == Camera::kMinDist);
    c.zoomBy(1e9f);    // "afastar infinitamente" (range novo 0.01→1e5 = 1e7)
    EXPECT(c.dist == Camera::kMaxDist);
}

TEST(near_far_sanity_contra_z_fighting) {
    // 0.8.9: DEFAULTS F3.1 mantidos (0.5/450 — quem não chama setClips vê o
    // comportamento de sempre); rácio ≤ ~900:1 preserva o depth de 24 bits
    Camera c;
    EXPECT(nearEq(c.nearZ, 0.5f));
    EXPECT(nearEq(c.farZ, 450.0f));
    EXPECT(c.nearZ < c.farZ);
    const f32 ratio = c.farZ / c.nearZ;
    EXPECT(ratio <= 900.0f);     // 450/0.5 = 900 exato
    EXPECT(ratio < 2000.0f);     // sanity da spec (folga)
    const Mat4 p = c.proj(16.0f / 9.0f);
    for (int i = 0; i < 16; ++i) {
        EXPECT(std::isfinite(p.m[i]));
    }
    EXPECT(nearEq(p.m[11], -1.0f));
    EXPECT(p.m[10] < 0.0f);      // near<far → termo negativo
}

TEST(clips_dinamicicos_089_far_contem_a_cena) {
    // 0.8.9 (ESPAÇO SEM TETOS): setClips com near/far derivados do zoom e do
    // AABB da cena — o contrato do main por frame. A proj lê os MEMBROS.
    Camera c;
    // zoom de trabalho: near próximo do antigo, far ≥ 450
    c.setDistance(6.0f);
    c.setClips(6.0f * 0.05f, 450.0f);
    EXPECT(nearEq(c.nearZ, 0.3f));
    EXPECT(nearEq(c.farZ, 450.0f));
    // zoom máximo + cena gigante (diagonal 150 000): far CONTÉM a cena
    c.setDistance(Camera::kMaxDist);
    c.setClips(Camera::kMaxDist * 0.05f, Camera::kMaxDist * 1.5f);
    EXPECT(c.farZ > Camera::kMaxDist);         // o alvo continua visível
    EXPECT(c.farZ / c.nearZ < 400.0f);         // rácio saudável p/ depth
    // zoom mínimo (perto do detalhe): near baixo sem degenerar
    c.setDistance(Camera::kMinDist);
    c.setClips(Camera::kMinDist * 0.05f, 450.0f);
    EXPECT(c.nearZ >= 0.0004f);                // > 0 e finito
    EXPECT(std::isfinite(c.proj(16.0f / 9.0f).m[10]));
    // defesa: near inválido mantém o default; far ≤ near sobe p/ 2×near
    Camera d;
    d.setClips(-1.0f, 0.1f);
    EXPECT(nearEq(d.nearZ, Camera::kDefaultNear));
    EXPECT(d.farZ > d.nearZ);
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
    // a 100 000 de distância o target fica dentro do frustum (far dinâmico
    // = 1.5×dist — 0.8.9: o far SEMPRE contém o que a câmara olha)
    Camera c;
    c.setDistance(Camera::kMaxDist);
    c.setClips(Camera::kMaxDist * 0.05f, Camera::kMaxDist * 1.5f);
    const Mat4 v = c.view();
    const Mat4 p = c.proj(16.0f / 9.0f);
    const Mat4 vp = Mat4::mul(p, v);
    f32 out[4];
    Mat4::transformPoint4(vp, c.target, out);
    const f32 ndcZ = out[2] / out[3];
    EXPECT(ndcZ > -1.0f && ndcZ < 1.0f);
    // e o alvo continua à frente da near plane (near = 5% dist)
    EXPECT(c.nearZ < c.dist);
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
    EXPECT(c.nearZ < c.farZ);
    EXPECT(c.fovY > 0.0f);
    const Mat4 p = c.proj(16.0f / 9.0f);
    EXPECT(nearEq(p.m[11], -1.0f));
}
