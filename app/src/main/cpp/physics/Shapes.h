#pragma once
// physics/Shapes.h — formas de colisão da F4 (puras, sem GL — host-testáveis).
//
// Quatro formas da spec:
//   Sphere  {center, r}                          — bola (preset RigidBody3D)
//   AABB    {min, max}                           — caixa alinhada aos eixos do MUNDO
//   OBB     {center, halfExtents, rotQuat}       — caixa orientada (parede rotacionável)
//   Capsule {center, radius, halfHeight}         — eixo Y LOCAL do dono (character)
//
// Convenção de espaços: as structs guardam parâmetros LOCAIS (relativos ao
// Transform3D do TIC dono). O PhysicsSystem deriva as formas de MUNDO por
// tick (center = pos + rot·offset; raios escalados pelo scale do TIC; OBB
// world rot = rotTIC × rotQuat; eixo da capsule = Y local do TIC rotacionado).
//
// Todas as funções aqui são determinísticas e não tocam em GL/Android —
// a suíte do CI Linux (test_shapes.cpp) valida interseções, sweep e CCD.
#include "core/Types.h"
#include "math/Math.h"
#include <cmath>

namespace vv {
namespace phys {

// ---- formas -----------------------------------------------------------------

struct Sphere {
    Vec3 center{0.0f, 0.0f, 0.0f};
    f32  r = 0.5f;
};

struct AABB {
    Vec3 min{-0.5f, -0.5f, -0.5f};
    Vec3 max{ 0.5f,  0.5f,  0.5f};
};

struct OBB {
    Vec3 center{0.0f, 0.0f, 0.0f};
    Vec3 halfExtents{0.5f, 0.5f, 0.5f};
    Quat rot = Quat::identity();   // orientação no espaço do shape
};

struct Capsule {
    Vec3 center{0.0f, 0.0f, 0.0f};
    f32  radius     = 0.3f;
    f32  halfHeight = 0.25f;   // meia-altura do segmento interno (eixo Y local)
};

// Segmento interno de uma capsule (eixo Y do shape; a rotação do TIC é
// aplicada pelo PhysicsSystem — ver sweepCapsuleSegBox para o eixo arbitrário).
struct Segment {
    Vec3 a{0.0f, 0.0f, 0.0f};
    Vec3 b{0.0f, 0.0f, 0.0f};
};

inline Segment capsuleSegment(const Capsule& c) {
    return Segment{Vec3{c.center.x, c.center.y - c.halfHeight, c.center.z},
                   Vec3{c.center.x, c.center.y + c.halfHeight, c.center.z}};
}

// ---- helpers de geometria ----------------------------------------------------

inline Vec3 aabbCenter(const AABB& b) {
    return Vec3{(b.min.x + b.max.x) * 0.5f,
                (b.min.y + b.max.y) * 0.5f,
                (b.min.z + b.max.z) * 0.5f};
}

inline Vec3 aabbExtent(const AABB& b) {   // meia-extensão
    return Vec3{(b.max.x - b.min.x) * 0.5f,
                (b.max.y - b.min.y) * 0.5f,
                (b.max.z - b.min.z) * 0.5f};
}

// Ponto → espaço local de um OBB (rotação inversa + translação).
inline Vec3 obbToLocal(const OBB& o, const Vec3& p) {
    const Quat inv{-o.rot.x, -o.rot.y, -o.rot.z, o.rot.w};   // conjugado (unitário)
    return inv.rotate(p - o.center);
}

// Ponto local do OBB → mundo.
inline Vec3 obbToWorld(const OBB& o, const Vec3& p) {
    return o.rot.rotate(p) + o.center;
}

// Distância assinada de um PONTO a um AABB (negativa dentro).
// Exata fora (euclidiana, cantos incluídos); dentro devolve a distância
// negativa ao plano mais próximo (convenção clássica de SDF de caixa).
inline f32 boxSDF(const Vec3& p, const Vec3& c, const Vec3& h) {
    const f32 qx = std::fabs(p.x - c.x) - h.x;
    const f32 qy = std::fabs(p.y - c.y) - h.y;
    const f32 qz = std::fabs(p.z - c.z) - h.z;
    const f32 outX = qx > 0.0f ? qx : 0.0f;
    const f32 outY = qy > 0.0f ? qy : 0.0f;
    const f32 outZ = qz > 0.0f ? qz : 0.0f;
    const f32 outside = std::sqrt(outX * outX + outY * outY + outZ * outZ);
    f32 m = qx > qy ? qx : qy;
    if (qz > m) m = qz;
    const f32 inside = m < 0.0f ? m : 0.0f;
    return outside + inside;
}

inline f32 pointAabbSDF(const AABB& b, const Vec3& p) {
    return boxSDF(p, aabbCenter(b), aabbExtent(b));
}

inline f32 pointObbSDF(const OBB& o, const Vec3& p) {
    return boxSDF(obbToLocal(o, p), Vec3{0.0f, 0.0f, 0.0f}, o.halfExtents);
}

// Ponto mais próximo de `p` num AABB (clamp) — para normais de depenetração.
inline Vec3 closestPointOnAabb(const AABB& b, const Vec3& p) {
    return Vec3{p.x < b.min.x ? b.min.x : (p.x > b.max.x ? b.max.x : p.x),
                p.y < b.min.y ? b.min.y : (p.y > b.max.y ? b.max.y : p.y),
                p.z < b.min.z ? b.min.z : (p.z > b.max.z ? b.max.z : p.z)};
}

inline Vec3 closestPointOnObb(const OBB& o, const Vec3& p) {
    const Vec3 l = obbToLocal(o, p);
    const Vec3 cl{l.x < -o.halfExtents.x ? -o.halfExtents.x :
                     (l.x > o.halfExtents.x ? o.halfExtents.x : l.x),
                  l.y < -o.halfExtents.y ? -o.halfExtents.y :
                     (l.y > o.halfExtents.y ? o.halfExtents.y : l.y),
                  l.z < -o.halfExtents.z ? -o.halfExtents.z :
                     (l.z > o.halfExtents.z ? o.halfExtents.z : l.z)};
    return obbToWorld(o, cl);
}

// Ponto mais próximo de `p` num segmento ab→b (t clampado a [0,1]).
inline Vec3 closestPointOnSegment(const Vec3& a, const Vec3& b, const Vec3& p) {
    const Vec3 ab = b - a;
    const f32 denom = dot(ab, ab);
    if (denom < 1e-12f) {
        return a;   // segmento degenerado (ponto)
    }
    const f32 t = dot(p - a, ab) / denom;
    const f32 tc = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return a + ab * tc;
}

// ---- interseções puras (F4-A.2 — bool, sem alocação, determinísticas) --------
bool intersects(const Sphere& a, const Sphere& b);
bool intersects(const Sphere& a, const AABB& b);
bool intersects(const Sphere& a, const OBB& b);
bool intersects(const Sphere& a, const Capsule& b);
bool intersects(const AABB& a, const AABB& b);
bool intersects(const OBB& a, const OBB& b);       // SAT 15 eixos (6 face + 9 aresta)
bool intersects(const Capsule& a, const Capsule& b);
bool intersects(const Capsule& a, const AABB& b);  // base da depenetração do sistema
bool intersects(const Capsule& a, const OBB& b);

// Pontos mais próximos entre dois segmentos (clampados) — base de Capsule/Capsule.
void segSegClosest(const Segment& s1, const Segment& s2, Vec3& c1, Vec3& c2);

// Pontos mais próximos entre segmento e caixa (OBB generalizada; AABB = rot
// identidade). Busca ternária — a distância a um conjunto convexo ao longo de
// um segmento é convexa, logo o mínimo é único.
void segBoxClosest(const Segment& seg, const Vec3& c, const Vec3& h,
                   const Quat& rot, Vec3& outSeg, Vec3& outBox);

// ---- sweep contínuo (F4-A.3) + CCD adaptativo ---------------------------------

struct SweepResult {
    bool hit = false;
    f32  toi = 1.0f;        // fração do delta no primeiro contacto [0,1]
    Vec3 normal{0.0f, 0.0f, 0.0f};   // normal de contacto (aponta contra o movimento)
};

// sweep(shapeA, delta, shapeB) → TOI + normal, para sphere e capsule contra
// AABB/OBB (as duas famílias da spec). O núcleo (sweepSegBox) aceita um
// segmento arbitrário — o PhysicsSystem passa a capsule já rotacionada.
SweepResult sweep(const Sphere& a, const Vec3& delta, const AABB& b);
SweepResult sweep(const Sphere& a, const Vec3& delta, const OBB& b);
SweepResult sweep(const Capsule& a, const Vec3& delta, const AABB& b);
SweepResult sweep(const Capsule& a, const Vec3& delta, const OBB& b);

// CCD adaptativo: se |delta| > raio_eff (r da forma), divide o sweep em
// ceil(|delta|/raio_eff) substeps (máx 8) — nunca tunela paredes finas.
// Núcleo: avanço conservador dentro de cada substep (o passo (d−r)/|delta|
// é seguro porque cada ponto do segmento anda exatamente |delta|).
SweepResult sweepSegBox(const Segment& seg, f32 r, const Vec3& delta,
                        const Vec3& c, const Vec3& h, const Quat& rot);

// ---- depenetração (MTD — mínimo vetor de separação) ----------------------------
// normal aponta de B para A (empurra A para fora de B). Usada pelo
// PhysicsSystem em sobreposições discretas e pelo repel Character↔Character.
struct Contact {
    bool hit = false;
    Vec3 normal{0.0f, 0.0f, 0.0f};
    f32  depth = 0.0f;        // profundidade de penetração (hit=true)
    f32  separation = 0.0f;   // distância entre superfícies (hit=false; 0 se tocando)
};

Contact depenetrate(const Sphere& a, const Sphere& b);
Contact depenetrate(const Sphere& a, const AABB& b);
Contact depenetrate(const Sphere& a, const OBB& b);
Contact depenetrate(const Capsule& a, const Capsule& b);
Contact depenetrate(const Capsule& a, const AABB& b);
Contact depenetrate(const Capsule& a, const OBB& b);

} // namespace phys
} // namespace vv
