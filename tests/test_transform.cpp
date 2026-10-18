// tests/test_transform.cpp — F3: Quat (identity/axisAngle/euler/rotate/toMat4)
// e Transform3D TRS. O contrato central: Quat::toMat4 tem de reproduzir as
// convenções EXATAS de Mat4::rotX/rotY/rotZ da F2 (senão o viewport roda ao
// contrário sem ninguém perceber nos testes de matriz).
#include "TestFramework.h"
#include "math/Math.h"

using namespace vv;
using ::test::matNearF;
using ::test::nearEqF;
using ::test::vecNearF;

TEST(quat_identidade_e_norma) {
    const Quat q = Quat::identity();
    EXPECT(nearEqF(q.x, 0.0f) && nearEqF(q.y, 0.0f) && nearEqF(q.z, 0.0f) && nearEqF(q.w, 1.0f));
    EXPECT(nearEqF(q.norm(), 1.0f));
    EXPECT(matNearF(q.toMat4(), Mat4::identity()));

    Quat sujo = Quat::axisAngle(Vec3{0.0f, 1.0f, 0.0f}, 0.8f);
    sujo.x *= 2.0f; sujo.y *= 2.0f; sujo.z *= 2.0f; sujo.w *= 2.0f;   // degenerado
    sujo.normalize();
    EXPECT(nearEqF(sujo.norm(), 1.0f, 1e-5f));
}

TEST(quat_axisangle_para_mat4_bate_com_mat4_dos_eixos) {
    // mesmos ângulos → mesmas matrizes (contrato com o pipeline da F2)
    const f32 angles[3] = {0.5f, -1.234f, 2.98f};
    for (const f32 a : angles) {
        EXPECT(matNearF(Quat::axisAngle(Vec3{1.0f, 0.0f, 0.0f}, a).toMat4(),
                        Mat4::rotX(a), 1e-5f));
        EXPECT(matNearF(Quat::axisAngle(Vec3{0.0f, 1.0f, 0.0f}, a).toMat4(),
                        Mat4::rotY(a), 1e-5f));
        EXPECT(matNearF(Quat::axisAngle(Vec3{0.0f, 0.0f, 1.0f}, a).toMat4(),
                        Mat4::rotZ(a), 1e-5f));
    }
}

TEST(quat_rotY_90_leva_x_para_menos_z) {
    const Quat q = Quat::axisAngle(Vec3{0.0f, 1.0f, 0.0f}, 1.5707963f);
    const Vec3 r = q.rotate(Vec3{1.0f, 0.0f, 0.0f});
    EXPECT(vecNearF(r, Vec3{0.0f, 0.0f, -1.0f}, 1e-5f));
    // e pela matriz
    f32 out[4];
    Mat4::transformPoint4(q.toMat4(), Vec3{1.0f, 0.0f, 0.0f}, out);
    EXPECT(nearEqF(out[0], 0.0f, 1e-5f) && nearEqF(out[2], -1.0f, 1e-5f));
}

TEST(quat_multiplicacao_bate_com_mul_de_matrizes) {
    // qy*qx aplicado a v == (rotY*rotX) aplicado a v (ordem idêntica)
    const Quat qy = Quat::axisAngle(Vec3{0.0f, 1.0f, 0.0f}, 0.7f);
    const Quat qx = Quat::axisAngle(Vec3{1.0f, 0.0f, 0.0f}, 0.4f);
    const Quat q  = qy * qx;
    EXPECT(nearEqF(q.norm(), 1.0f, 1e-5f));
    const Vec3 v{0.3f, -0.8f, 2.0f};

    const Vec3 viaQuat = q.rotate(v);
    f32 viaMat[4];
    Mat4::transformPoint4(Mat4::mul(Mat4::rotY(0.7f), Mat4::rotX(0.4f)), v, viaMat);
    EXPECT(nearEqF(viaQuat.x, viaMat[0], 1e-5f));
    EXPECT(nearEqF(viaQuat.y, viaMat[1], 1e-5f));
    EXPECT(nearEqF(viaQuat.z, viaMat[2], 1e-5f));

    // e via matriz do quat composto
    f32 viaQuatMat[4];
    Mat4::transformPoint4(q.toMat4(), v, viaQuatMat);
    EXPECT(nearEqF(viaQuatMat[0], viaMat[0], 1e-5f));
    EXPECT(nearEqF(viaQuatMat[1], viaMat[1], 1e-5f));
    EXPECT(nearEqF(viaQuatMat[2], viaMat[2], 1e-5f));
}

TEST(quat_euler_roundtrip_yxz) {
    // fora do polo, fromEuler→toEuler devolve os mesmos ângulos (ordem YXZ)
    const f32 pitches[3] = {-0.7f, 0.0f, 0.9f};
    const f32 yaws[3]    = {-2.5f, 0.3f, 1.2f};
    const f32 rolls[3]   = {-0.4f, 0.0f, 1.4f};
    for (const f32 p : pitches) {
        for (const f32 y : yaws) {
            for (const f32 r : rolls) {
                f32 po = 0.0f, yo = 0.0f, ro = 0.0f;
                Quat::toEuler(Quat::fromEuler(p, y, r), po, yo, ro);
                EXPECT(nearEqF(po, p, 1e-4f));
                EXPECT(nearEqF(yo, y, 1e-4f));
                EXPECT(nearEqF(ro, r, 1e-4f));
            }
        }
    }
}

TEST(quat_toMat4_vs_composicao_euler) {
    // fromEuler(p,y,r).toMat4() == rotY(y) * rotX(p) * rotZ(r) — ordem YXZ
    const f32 p = 0.32f, y = -1.1f, r = 0.65f;
    const Mat4 esperada = Mat4::mul(Mat4::rotY(y),
                                    Mat4::mul(Mat4::rotX(p), Mat4::rotZ(r)));
    EXPECT(matNearF(Quat::fromEuler(p, y, r).toMat4(), esperada, 1e-5f));
}
