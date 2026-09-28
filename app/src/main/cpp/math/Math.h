#pragma once
// math/Math.h — álgebra linear mínima do engine (F2).
// Column-major (layout compatível com glUniformMatrix4fv sem transpose),
// ângulos em radianos internamente, sistema right-handed, clip space -1..1.
// F1 mantém Mat4::ortho (UI). F2 acrescenta Vec3 e o pipeline 3D (perspective/lookAt).
#include <cmath>
#include "core/Types.h"

namespace vv {

struct Vec3 {
    f32 x = 0.0f;
    f32 y = 0.0f;
    f32 z = 0.0f;

    Vec3() = default;
    constexpr Vec3(f32 ix, f32 iy, f32 iz) : x(ix), y(iy), z(iz) {}

    constexpr Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    constexpr Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    constexpr Vec3 operator-() const { return {-x, -y, -z}; }
    constexpr Vec3 operator*(f32 s) const { return {x * s, y * s, z * s}; }

    constexpr Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
};

inline f32 dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y,
            a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

inline f32 length(const Vec3& v) { return std::sqrt(dot(v, v)); }

inline Vec3 normalized(const Vec3& v) {
    const f32 len = length(v);
    return len > 1e-8f ? v * (1.0f / len) : Vec3{0.0f, 0.0f, 0.0f};
}

// Matriz 4x4 column-major: elemento (linha r, coluna c) = m[c*4 + r].
// Multiplicação no padrão OpenGL: v' = (A*B)*v aplica primeiro B, depois A.
struct Mat4 {
    f32 m[16];

    static Mat4 identity() {
        Mat4 r{};
        r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f;
        return r;
    }

    static Mat4 mul(const Mat4& a, const Mat4& b) {
        Mat4 r{};
        for (i32 c = 0; c < 4; ++c) {
            for (i32 rw = 0; rw < 4; ++rw) {
                f32 s = 0.0f;
                for (i32 k = 0; k < 4; ++k) {
                    s += a.m[k * 4 + rw] * b.m[c * 4 + k];
                }
                r.m[c * 4 + rw] = s;
            }
        }
        return r;
    }

    static Mat4 translation(f32 x, f32 y, f32 z) {
        Mat4 r = identity();
        r.m[12] = x;
        r.m[13] = y;
        r.m[14] = z;
        return r;
    }

    static Mat4 scale(f32 x, f32 y, f32 z) {
        Mat4 r{};
        r.m[0] = x;
        r.m[5] = y;
        r.m[10] = z;
        r.m[15] = 1.0f;
        return r;
    }

    static Mat4 rotX(f32 radians) {
        Mat4 r = identity();
        const f32 c = std::cos(radians);
        const f32 s = std::sin(radians);
        r.m[5] = c;  r.m[6]  = s;
        r.m[9] = -s; r.m[10] = c;
        return r;
    }

    static Mat4 rotY(f32 radians) {
        Mat4 r = identity();
        const f32 c = std::cos(radians);
        const f32 s = std::sin(radians);
        r.m[0] = c;  r.m[2]  = -s;
        r.m[8] = s;  r.m[10] = c;
        return r;
    }

    static Mat4 rotZ(f32 radians) {
        Mat4 r = identity();
        const f32 c = std::cos(radians);
        const f32 s = std::sin(radians);
        r.m[0] = c;  r.m[1] = s;
        r.m[4] = -s; r.m[5] = c;
        return r;
    }

    // Projeção perspectiva (right-handed, câmara olha para -Z, clip -1..1).
    static Mat4 perspective(f32 fovyRadians, f32 aspect, f32 zNear, f32 zFar) {
        Mat4 r{};
        const f32 f = 1.0f / std::tan(fovyRadians * 0.5f);
        r.m[0]  = f / aspect;
        r.m[5]  = f;
        r.m[10] = (zFar + zNear) / (zNear - zFar);
        r.m[11] = -1.0f;
        r.m[14] = (2.0f * zFar * zNear) / (zNear - zFar);
        return r;
    }

    // Vista de câmara: eye → center, up = referência "para cima".
    static Mat4 lookAt(const Vec3& eye, const Vec3& center, const Vec3& up) {
        const Vec3 f = normalized(center - eye);          // frente da câmara
        const Vec3 s = normalized(cross(f, up));          // direita
        const Vec3 u = cross(s, f);                       // cima real
        Mat4 r = identity();
        r.m[0]  =  s.x; r.m[4]  =  s.y; r.m[8]  =  s.z;
        r.m[1]  =  u.x; r.m[5]  =  u.y; r.m[9]  =  u.z;
        r.m[2]  = -f.x; r.m[6]  = -f.y; r.m[10] = -f.z;
        r.m[12] = -dot(s, eye);
        r.m[13] = -dot(u, eye);
        r.m[14] =  dot(f, eye);
        return r;
    }

    // Transforma um ponto (w=1); devolve também w para validar projeções nos testes.
    static void transformPoint4(const Mat4& t, const Vec3& p, f32 out[4]) {
        out[0] = t.m[0] * p.x + t.m[4] * p.y + t.m[8]  * p.z + t.m[12];
        out[1] = t.m[1] * p.x + t.m[5] * p.y + t.m[9]  * p.z + t.m[13];
        out[2] = t.m[2] * p.x + t.m[6] * p.y + t.m[10] * p.z + t.m[14];
        out[3] = t.m[3] * p.x + t.m[7] * p.y + t.m[11] * p.z + t.m[15];
    }

    // F1: projeção ortográfica da UI (y para baixo, origem topo-esquerda).
    static Mat4 ortho(f32 left, f32 right, f32 bottom, f32 top, f32 zNear, f32 zFar) {
        Mat4 r{};
        const f32 rl = right - left;
        const f32 tb = top - bottom;
        const f32 fn = zFar - zNear;
        r.m[0]  = 2.0f / rl;
        r.m[1]  = 0.0f;
        r.m[2]  = 0.0f;
        r.m[3]  = 0.0f;
        r.m[4]  = 0.0f;
        r.m[5]  = 2.0f / tb;
        r.m[6]  = 0.0f;
        r.m[7]  = 0.0f;
        r.m[8]  = 0.0f;
        r.m[9]  = 0.0f;
        r.m[10] = -2.0f / fn;
        r.m[11] = 0.0f;
        r.m[12] = -(right + left) / rl;
        r.m[13] = -(top + bottom) / tb;
        r.m[14] = -(zFar + zNear) / fn;
        r.m[15] = 1.0f;
        return r;
    }
};

} // namespace vv
