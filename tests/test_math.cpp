// tests/test_math.cpp — Mat4/Vec3: identidade, rotações, perspective sanity, lookAt.
#include "TestFramework.h"
#include "math/Math.h"

using namespace vv;

namespace {

bool nearEq(f32 a, f32 b, f32 eps = 1e-4f) {
    const f32 d = a - b;
    return d < eps && d > -eps;
}

bool vecNear(const Vec3& a, const Vec3& b, f32 eps = 1e-4f) {
    return nearEq(a.x, b.x, eps) && nearEq(a.y, b.y, eps) && nearEq(a.z, b.z, eps);
}

bool matNear(const Mat4& a, const Mat4& b, f32 eps = 1e-4f) {
    for (int i = 0; i < 16; ++i) {
        if (!nearEq(a.m[i], b.m[i], eps)) return false;
    }
    return true;
}

} // namespace

TEST(identidade_multiplicada_por_M_igual_M) {
    const Mat4 I = Mat4::identity();
    const Mat4 M = Mat4::mul(Mat4::translation(1.0f, 2.0f, 3.0f),
                             Mat4::rotY(0.7f));
    EXPECT(matNear(Mat4::mul(I, M), M));
    EXPECT(matNear(Mat4::mul(M, I), M));
}

TEST(rotY_90_gira_x_para_menos_z) {
    const Mat4 R = Mat4::rotY(1.5707963f);   // 90°
    f32 out[4];
    Mat4::transformPoint4(R, Vec3{1.0f, 0.0f, 0.0f}, out);
    EXPECT(nearEq(out[0], 0.0f));
    EXPECT(nearEq(out[1], 0.0f));
    EXPECT(nearEq(out[2], -1.0f));
    EXPECT(nearEq(out[3], 1.0f));
}

TEST(rotX_e_rotZ_90_giram_conforme_eixo) {
    f32 out[4];
    Mat4::transformPoint4(Mat4::rotX(1.5707963f), Vec3{0.0f, 1.0f, 0.0f}, out);
    EXPECT(nearEq(out[1], 0.0f) && nearEq(out[2], 1.0f));
    Mat4::transformPoint4(Mat4::rotZ(1.5707963f), Vec3{1.0f, 0.0f, 0.0f}, out);
    EXPECT(nearEq(out[0], 0.0f) && nearEq(out[1], 1.0f));
}

TEST(translation_e_scale_movem_e_escalam_pontos) {
    f32 out[4];
    Mat4::transformPoint4(Mat4::translation(2.0f, -1.0f, 0.5f),
                          Vec3{1.0f, 1.0f, 1.0f}, out);
    EXPECT(nearEq(out[0], 3.0f) && nearEq(out[1], 0.0f) && nearEq(out[2], 1.5f));
    Mat4::transformPoint4(Mat4::scale(2.0f, 3.0f, 4.0f), Vec3{1.0f, 1.0f, 1.0f}, out);
    EXPECT(nearEq(out[0], 2.0f) && nearEq(out[1], 3.0f) && nearEq(out[2], 4.0f));
    // composição: primeiro scale, depois translation
    const Mat4 T = Mat4::mul(Mat4::translation(0.0f, 1.0f, 0.0f), Mat4::scale(2.0f, 2.0f, 2.0f));
    Mat4::transformPoint4(T, Vec3{1.0f, 0.0f, 0.0f}, out);
    EXPECT(nearEq(out[0], 2.0f) && nearEq(out[1], 1.0f) && nearEq(out[2], 0.0f));
}

TEST(perspective_sanity) {
    // valores sanity: fov > 0, near < far → matriz finita e well-formed
    const Mat4 P = Mat4::perspective(1.0472f /*60°*/, 2.0f, 0.1f, 100.0f);
    for (int i = 0; i < 16; ++i) {
        EXPECT(std::isfinite(P.m[i]));
    }
    EXPECT(nearEq(P.m[11], -1.0f));                       // RH: -Z entra na câmara
    EXPECT(P.m[10] < 0.0f);                               // near<far → termo negativo
    // ponto no plano near (z = -near) → NDC z = -1; no far → NDC z = +1
    f32 out[4];
    Mat4::transformPoint4(P, Vec3{0.0f, 0.0f, -0.1f}, out);
    EXPECT(nearEq(out[2] / out[3], -1.0f));
    Mat4::transformPoint4(P, Vec3{0.0f, 0.0f, -100.0f}, out);
    EXPECT(nearEq(out[2] / out[3], 1.0f, 1e-3f));
    // aspecto: x no plano near mapeia para ±aspect no NDC
    // half-width do frustum no near = near * tan(fov/2) * aspect
    Mat4::transformPoint4(P, Vec3{0.2f, 0.0f, -0.1f}, out);
    const f32 halfW = 0.1f * 0.57735f * 2.0f;
    EXPECT(nearEq(out[0] / out[3], (0.2f / halfW), 1e-3f));
}

TEST(lookAt_camara_padrao_olha_para_origem) {
    const Mat4 V = Mat4::lookAt(Vec3{0.0f, 0.0f, 5.0f}, Vec3{0.0f, 0.0f, 0.0f},
                                Vec3{0.0f, 1.0f, 0.0f});
    f32 out[4];
    Mat4::transformPoint4(V, Vec3{0.0f, 0.0f, 0.0f}, out);   // target → (0,0,-5)
    EXPECT(nearEq(out[0], 0.0f) && nearEq(out[1], 0.0f) && nearEq(out[2], -5.0f));
    Mat4::transformPoint4(V, Vec3{0.0f, 0.0f, 5.0f}, out);   // eye → origem da vista
    EXPECT(nearEq(out[0], 0.0f) && nearEq(out[1], 0.0f) && nearEq(out[2], 0.0f));
}

TEST(orthogonality_das_rotacoes) {
    // R^T × R == I para cada eixo (rotação é ortonormal)
    const Mat4 axes[3] = { Mat4::rotX(0.9f), Mat4::rotY(-1.3f), Mat4::rotZ(2.2f) };
    for (const Mat4& R : axes) {
        Mat4 Rt = R;
        for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
                Rt.m[c * 4 + r] = R.m[r * 4 + c];   // transpose
            }
        }
        EXPECT(matNear(Mat4::mul(Rt, R), Mat4::identity(), 1e-3f));
    }
}
