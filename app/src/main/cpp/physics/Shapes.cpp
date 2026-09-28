// physics/Shapes.cpp — interseções puras das formas da F4 (sem GL).
//
// Tudo em espaço local onde compensa: Sphere/OBB transforma a bola para o
// espaço do OBB; Capsule/Capsule reduz a segmento-segmento + raios; OBB/OBB
// usa SAT com 15 eixos (6 normais de face + 9 cruzamentos de arestas).
// Segmento↔caixa usa busca ternária (a distância a um conjunto convexo ao
// longo de um segmento é convexa — mínimo único, busca convergente).
#include "physics/Shapes.h"

namespace vv {
namespace phys {

// ---- pares com esfera ---------------------------------------------------------

bool intersects(const Sphere& a, const Sphere& b) {
    const Vec3 d = a.center - b.center;
    const f32 rr = a.r + b.r;
    return dot(d, d) <= rr * rr;
}

bool intersects(const Sphere& a, const AABB& b) {
    const Vec3 cp = closestPointOnAabb(b, a.center);
    const Vec3 d = a.center - cp;
    return dot(d, d) <= a.r * a.r;   // centro dentro → cp == center → d == 0 → true
}

bool intersects(const Sphere& a, const OBB& b) {
    const Vec3 cp = closestPointOnObb(b, a.center);
    const Vec3 d = a.center - cp;
    return dot(d, d) <= a.r * a.r;
}

bool intersects(const Sphere& a, const Capsule& b) {
    const Segment seg = capsuleSegment(b);
    const Vec3 cp = closestPointOnSegment(seg.a, seg.b, a.center);
    const Vec3 d = a.center - cp;
    const f32 rr = a.r + b.radius;
    return dot(d, d) <= rr * rr;
}

// ---- caixas --------------------------------------------------------------------

bool intersects(const AABB& a, const AABB& b) {
    return a.min.x <= b.max.x && a.max.x >= b.min.x &&
           a.min.y <= b.max.y && a.max.y >= b.min.y &&
           a.min.z <= b.max.z && a.max.z >= b.min.z;
}

bool intersects(const OBB& a, const OBB& b) {
    // SAT com 15 eixos: 3 normais de face de A + 3 de B + 9 cruzamentos.
    // Projeção raio de um OBB num eixo n: Σ |dot(eixoLocal_i, n)| · he_i.
    const Vec3 aX = a.rot.rotate(Vec3{1.0f, 0.0f, 0.0f});
    const Vec3 aY = a.rot.rotate(Vec3{0.0f, 1.0f, 0.0f});
    const Vec3 aZ = a.rot.rotate(Vec3{0.0f, 0.0f, 1.0f});
    const Vec3 bX = b.rot.rotate(Vec3{1.0f, 0.0f, 0.0f});
    const Vec3 bY = b.rot.rotate(Vec3{0.0f, 1.0f, 0.0f});
    const Vec3 bZ = b.rot.rotate(Vec3{0.0f, 0.0f, 1.0f});

    Vec3 axes[15] = {aX, aY, aZ, bX, bY, bZ,
                     cross(aX, bX), cross(aX, bY), cross(aX, bZ),
                     cross(aY, bX), cross(aY, bY), cross(aY, bZ),
                     cross(aZ, bX), cross(aZ, bY), cross(aZ, bZ)};

    const Vec3 d = b.center - a.center;
    for (Vec3& n : axes) {
        const f32 len = length(n);
        if (len < 1e-6f) {
            continue;   // eixos paralelos → cruzamento degenerado, eixo redundante
        }
        n = n * (1.0f / len);
        const f32 ra = a.halfExtents.x * std::fabs(dot(aX, n)) +
                       a.halfExtents.y * std::fabs(dot(aY, n)) +
                       a.halfExtents.z * std::fabs(dot(aZ, n));
        const f32 rb = b.halfExtents.x * std::fabs(dot(bX, n)) +
                       b.halfExtents.y * std::fabs(dot(bY, n)) +
                       b.halfExtents.z * std::fabs(dot(bZ, n));
        if (std::fabs(dot(d, n)) > ra + rb + 1e-6f) {
            return false;   // eixo separador encontrado
        }
    }
    return true;
}

// ---- cápsulas -------------------------------------------------------------------

void segSegClosest(const Segment& s1, const Segment& s2, Vec3& c1, Vec3& c2) {
    // Algoritmo clássico (Ericson, Real-Time Collision Detection) com clamps.
    const Vec3 d1 = s1.b - s1.a;   // direção de s1
    const Vec3 d2 = s2.b - s2.a;   // direção de s2
    const Vec3 r  = s1.a - s2.a;
    const f32 a = dot(d1, d1);
    const f32 e = dot(d2, d2);
    const f32 f = dot(d2, r);
    f32 t1 = 0.0f;
    f32 t2 = 0.0f;

    if (a <= 1e-12f && e <= 1e-12f) {            // ambos degenerados (pontos)
        c1 = s1.a;
        c2 = s2.a;
        return;
    }
    if (a <= 1e-12f) {                            // s1 é ponto
        t1 = 0.0f;
        t2 = f / e;
        t2 = t2 < 0.0f ? 0.0f : (t2 > 1.0f ? 1.0f : t2);
    } else {
        const f32 c = dot(d1, r);
        if (e <= 1e-12f) {                        // s2 é ponto
            t2 = 0.0f;
            t1 = -c / a;
            t1 = t1 < 0.0f ? 0.0f : (t1 > 1.0f ? 1.0f : t1);
        } else {
            const f32 b = dot(d1, d2);
            const f32 denom = a * e - b * b;
            t1 = denom > 1e-12f ? (b * f - c * e) / denom : 0.0f;   // paralelos → 0
            t1 = t1 < 0.0f ? 0.0f : (t1 > 1.0f ? 1.0f : t1);
            const f32 t1v = b * t1 + f;
            t2 = t1v / e;
            if (t2 < 0.0f) {
                t2 = 0.0f;
                t1 = -c / a;
                t1 = t1 < 0.0f ? 0.0f : (t1 > 1.0f ? 1.0f : t1);
            } else if (t2 > 1.0f) {
                t2 = 1.0f;
                t1 = (b - c) / a;
                t1 = t1 < 0.0f ? 0.0f : (t1 > 1.0f ? 1.0f : t1);
            }
        }
    }
    c1 = s1.a + d1 * t1;
    c2 = s2.a + d2 * t2;
}

bool intersects(const Capsule& a, const Capsule& b) {
    Vec3 c1, c2;
    segSegClosest(capsuleSegment(a), capsuleSegment(b), c1, c2);
    const Vec3 d = c1 - c2;
    const f32 rr = a.radius + b.radius;
    return dot(d, d) <= rr * rr;
}

void segBoxClosest(const Segment& seg, const Vec3& c, const Vec3& h,
                   const Quat& rot, Vec3& outSeg, Vec3& outBox) {
    // Espaço local da caixa: segmento e SDF ficam axis-aligned.
    const Quat inv{-rot.x, -rot.y, -rot.z, rot.w};
    const Vec3 la = inv.rotate(seg.a - c);
    const Vec3 lb = inv.rotate(seg.b - c);
    const Vec3 ab = lb - la;

    // Busca ternária do t com distância mínima ponto(t)→caixa (convexa em t).
    f32 lo = 0.0f;
    f32 hi = 1.0f;
    for (int i = 0; i < 48; ++i) {
        const f32 m1 = lo + (hi - lo) / 3.0f;
        const f32 m2 = hi - (hi - lo) / 3.0f;
        const Vec3 p1 = la + ab * m1;
        const Vec3 p2 = la + ab * m2;
        const f32 d1 = boxSDF(p1, Vec3{0.0f, 0.0f, 0.0f}, h);
        const f32 d2 = boxSDF(p2, Vec3{0.0f, 0.0f, 0.0f}, h);
        // SDF assinada < 0 dentro; para achar CONTACTO queremos a menor
        // distância euclidiana — fora, SDF == distância; dentro é 0/negativa
        // e o clamp do passo seguinte resolve. Minimizar a SDF leva ao
        // ponto mais raso — correto para primeiro contacto.
        if (d1 < d2) {
            hi = m2;
        } else {
            lo = m1;
        }
    }
    const f32 t = (lo + hi) * 0.5f;
    const Vec3 pl = la + ab * t;
    const Vec3 cl{pl.x < -h.x ? -h.x : (pl.x > h.x ? h.x : pl.x),
                  pl.y < -h.y ? -h.y : (pl.y > h.y ? h.y : pl.y),
                  pl.z < -h.z ? -h.z : (pl.z > h.z ? h.z : pl.z)};
    outSeg = rot.rotate(pl) + c;
    outBox = rot.rotate(cl) + c;
}

bool intersects(const Capsule& a, const AABB& b) {
    Vec3 s1, s2;
    segBoxClosest(capsuleSegment(a), aabbCenter(b), aabbExtent(b),
                  Quat::identity(), s1, s2);
    const Vec3 d = s1 - s2;
    return dot(d, d) <= a.radius * a.radius;
}

bool intersects(const Capsule& a, const OBB& b) {
    Vec3 s1, s2;
    segBoxClosest(capsuleSegment(a), b.center, b.halfExtents, b.rot, s1, s2);
    const Vec3 d = s1 - s2;
    return dot(d, d) <= a.radius * a.radius;
}

} // namespace phys
} // namespace vv
