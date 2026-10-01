// render/Primitives.cpp — geradores das 8 primitivas procedurais (0.8.0).
//
// WINDING: CCW visto de fora (front face default do GLES). A regra usada em
// TODOS os geradores de grelha: o triângulo (a, b, d) com b−a ∝ ∂P/∂u e
// d−a ∝ ∂P/∂u + ∂P/∂v tem normal ∝ (∂P/∂u × ∂P/∂v); a orientação de cada
// grelha foi escolhida para esse produto apontar PARA FORA — e o teste do CI
// (test_prims) confirma triângulo a triângulo que a normal da face concorda
// com as normais dos vértices (dot > 0).
//
// Índice u16: os clamps (segments ≤ 64, rings ≤ 64) garantem folga larga ao
// limite de 65535 (pior caso esfera 65×65 = 4225 vértices).
#include "render/Primitives.h"
#include "render/Cube.h"
#include <cmath>

namespace vv {
namespace {

constexpr f32 kPi = 3.14159265358979323846f;

// emissor comum: acumula vértices/índices
struct Emit {
    PrimMeshData& out;
    explicit Emit(PrimMeshData& o) : out(o) {}

    u16 vert(const Vec3& p, const Vec3& n, const Vec2& uv) {
        out.vertices.push_back(Vertex{p, n, uv});
        return static_cast<u16>(out.vertices.size() - 1);
    }
    void tri(u16 a, u16 b, u16 c) {
        out.indices.push_back(a);
        out.indices.push_back(b);
        out.indices.push_back(c);
    }
    // quad (a,b,c,d) CCW → 2 triângulos
    void quad(u16 a, u16 b, u16 c, u16 d) {
        tri(a, b, c);
        tri(a, c, d);
    }
};

// ---- esfera (UV clássica: polos ±Y; linhas = phi [0..pi], colunas = th) -----
// Grelha (rings+1)×(segments+1); quad(a,b,d,c) com b na coluna seguinte e
// c/d na linha seguinte → ∂/∂th × ∂/∂phi aponta PARA FORA (sp ≥ 0).
void genSphere(const PrimParams& p, PrimMeshData& out) {
    Emit e(out);
    const i32 seg  = p.segments;
    const i32 ring = p.rings;
    for (i32 r = 0; r <= ring; ++r) {
        const f32 v   = static_cast<f32>(r) / static_cast<f32>(ring);
        f32 phi = v * kPi;
        f32 sp  = std::sin(phi), cp = std::cos(phi);
        // polos EXATOS (sin(pi)≈1e-16 em flutuante deixaria a última linha
        // "quase" no polo — triângulos degenerados com normal aleatória)
        if (r == 0)      { sp = 0.0f; cp = 1.0f; }
        else if (r == ring) { sp = 0.0f; cp = -1.0f; }
        for (i32 s = 0; s <= seg; ++s) {
            const f32 u  = static_cast<f32>(s) / static_cast<f32>(seg);
            const f32 th = u * 2.0f * kPi;
            const Vec3 n{sp * std::cos(th), cp, sp * std::sin(th)};
            e.vert(n * p.radius, n, Vec2{u, v});
        }
    }
    const i32 row = seg + 1;
    for (i32 r = 0; r < ring; ++r) {
        for (i32 s = 0; s < seg; ++s) {
            const u16 a = static_cast<u16>(r * row + s);
            const u16 b = static_cast<u16>(r * row + s + 1);
            const u16 c = static_cast<u16>((r + 1) * row + s);
            const u16 d = static_cast<u16>((r + 1) * row + s + 1);
            e.quad(a, b, d, c);
        }
    }
}

// ---- cilindro (lateral + 2 tampas em leque) --------------------------------
// Lateral: quad(a,d,c,b) — a/d na coluna s (baixo/cima), c/b na s+1: o
// produto (∂/∂y × ∂/∂th) aponta para fora. Tampas: leque com normal ±Y.
void genCylinder(const PrimParams& p, PrimMeshData& out) {
    Emit e(out);
    const i32 seg = p.segments;
    const f32 h   = p.height * 0.5f;
    const u16 bot = static_cast<u16>(out.vertices.size());
    for (i32 s = 0; s <= seg; ++s) {
        const f32 u  = static_cast<f32>(s) / static_cast<f32>(seg);
        const f32 th = u * 2.0f * kPi;
        const Vec3 n{std::cos(th), 0.0f, std::sin(th)};
        e.vert(Vec3{n.x * p.radius, -h, n.z * p.radius}, n, Vec2{u, 0.0f});
    }
    const u16 top = static_cast<u16>(out.vertices.size());
    for (i32 s = 0; s <= seg; ++s) {
        const f32 u  = static_cast<f32>(s) / static_cast<f32>(seg);
        const f32 th = u * 2.0f * kPi;
        const Vec3 n{std::cos(th), 0.0f, std::sin(th)};
        e.vert(Vec3{n.x * p.radius, h, n.z * p.radius}, n, Vec2{u, 1.0f});
    }
    for (i32 s = 0; s < seg; ++s) {
        const u16 a = bot + static_cast<u16>(s);
        const u16 b = bot + static_cast<u16>(s + 1);
        const u16 c = top + static_cast<u16>(s + 1);
        const u16 d = top + static_cast<u16>(s);
        e.quad(a, d, c, b);
    }
    // tampas: centro + aro (uv planar, normal ±Y)
    for (int up = 0; up < 2; ++up) {
        const f32 y  = up ? h : -h;
        const f32 ny = up ? 1.0f : -1.0f;
        const u16 c  = e.vert(Vec3{0.0f, y, 0.0f}, Vec3{0.0f, ny, 0.0f},
                               Vec2{0.5f, 0.5f});
        const u16 rb = static_cast<u16>(out.vertices.size());
        for (i32 s = 0; s <= seg; ++s) {
            const f32 th = (static_cast<f32>(s) / static_cast<f32>(seg)) * 2.0f * kPi;
            const Vec3 d{std::cos(th), 0.0f, std::sin(th)};
            e.vert(Vec3{d.x * p.radius, y, d.z * p.radius}, Vec3{0.0f, ny, 0.0f},
                   Vec2{d.x * 0.5f + 0.5f, d.z * 0.5f + 0.5f});
        }
        for (i32 s = 0; s < seg; ++s) {
            const u16 a = rb + static_cast<u16>(s);
            const u16 b = rb + static_cast<u16>(s + 1);
            if (up) {
                e.tri(c, b, a);   // visto de +Y: CCW
            } else {
                e.tri(c, a, b);   // visto de −Y: CCW
            }
        }
    }
}

// ---- cone (base em −h/2, ápice +h/2) ----------------------------------------
// Lateral: aro da base + coluna de ápice (duplicado por coluna — normal por
// setor, sem média); quad(a,d,c,b) como o cilindro (o triângulo interior é
// degenerado no ápice — área zero, inofensivo). Base: leque normal −Y.
void genCone(const PrimParams& p, PrimMeshData& out) {
    Emit e(out);
    const i32 seg = p.segments;
    const f32 h   = p.height * 0.5f;
    const f32 slope = p.radius / p.height;
    const f32 nl    = 1.0f / std::sqrt(1.0f + slope * slope);
    const u16 bot = static_cast<u16>(out.vertices.size());
    for (i32 s = 0; s <= seg; ++s) {
        const f32 u  = static_cast<f32>(s) / static_cast<f32>(seg);
        const f32 th = u * 2.0f * kPi;
        const f32 ct = std::cos(th), st = std::sin(th);
        const Vec3 n{ct * nl, slope * nl, st * nl};
        e.vert(Vec3{ct * p.radius, -h, st * p.radius}, n, Vec2{u, 0.0f});
    }
    const u16 apex = static_cast<u16>(out.vertices.size());
    for (i32 s = 0; s <= seg; ++s) {
        const f32 u  = static_cast<f32>(s) / static_cast<f32>(seg);
        const f32 th = u * 2.0f * kPi;
        const f32 ct = std::cos(th), st = std::sin(th);
        const Vec3 n{ct * nl, slope * nl, st * nl};
        e.vert(Vec3{0.0f, h, 0.0f}, n, Vec2{u, 1.0f});
    }
    for (i32 s = 0; s < seg; ++s) {
        const u16 a = bot + static_cast<u16>(s);
        const u16 b = bot + static_cast<u16>(s + 1);
        const u16 c = apex + static_cast<u16>(s + 1);
        const u16 d = apex + static_cast<u16>(s);
        e.quad(a, d, c, b);
    }
    // base (normal −Y, uv planar)
    {
        const u16 c = e.vert(Vec3{0.0f, -h, 0.0f}, Vec3{0.0f, -1.0f, 0.0f},
                              Vec2{0.5f, 0.5f});
        const u16 rb = static_cast<u16>(out.vertices.size());
        for (i32 s = 0; s <= seg; ++s) {
            const f32 th = (static_cast<f32>(s) / static_cast<f32>(seg)) * 2.0f * kPi;
            const Vec3 d{std::cos(th), 0.0f, std::sin(th)};
            e.vert(Vec3{d.x * p.radius, -h, d.z * p.radius}, Vec3{0.0f, -1.0f, 0.0f},
                   Vec2{d.x * 0.5f + 0.5f, d.z * 0.5f + 0.5f});
        }
        for (i32 s = 0; s < seg; ++s) {
            e.tri(c, rb + static_cast<u16>(s), rb + static_cast<u16>(s + 1));
        }
    }
}

// ---- plano (quad XZ em y=0 — chão por omissão, normal +Y) -------------------
void genPlane(const PrimParams& p, PrimMeshData& out) {
    Emit e(out);
    const f32 s = p.size * 0.5f;
    const u16 a = e.vert(Vec3{-s, 0.0f, -s}, Vec3{0.0f, 1.0f, 0.0f}, Vec2{0.0f, 0.0f});
    const u16 b = e.vert(Vec3{ s, 0.0f, -s}, Vec3{0.0f, 1.0f, 0.0f}, Vec2{1.0f, 0.0f});
    const u16 c = e.vert(Vec3{ s, 0.0f,  s}, Vec3{0.0f, 1.0f, 0.0f}, Vec2{1.0f, 1.0f});
    const u16 d = e.vert(Vec3{-s, 0.0f,  s}, Vec3{0.0f, 1.0f, 0.0f}, Vec2{0.0f, 1.0f});
    e.quad(a, d, c, b);   // visto de +Y (de cima): CCW
}

// ---- wedge / triângulo (rampa) ----------------------------------------------
// Perfil no plano (y,z): A=(−h,−s) B=(−h,+s) C=(+h,−s), extrudado em X.
// Faces: frente +X (triângulo), trás −X (triângulo), chão y=−h (quad),
// RAMPA B→C (quad inclinado — a única face "de trabalho" da rampa). A face
// z=+s é ZERO (só a aresta B) — não existe. Altura = meio do tamanho
// (cubo inscrito; h == s por construção).
void genWedge(const PrimParams& p, PrimMeshData& out) {
    Emit e(out);
    const f32 s = p.size * 0.5f;
    const f32 h = p.size * 0.5f;
    // frente (+X)
    const u16 fA = e.vert(Vec3{ s, -h, -s}, Vec3{ 1.0f, 0.0f, 0.0f}, Vec2{0.0f, 0.0f});
    const u16 fB = e.vert(Vec3{ s, -h,  s}, Vec3{ 1.0f, 0.0f, 0.0f}, Vec2{1.0f, 0.0f});
    const u16 fC = e.vert(Vec3{ s,  h, -s}, Vec3{ 1.0f, 0.0f, 0.0f}, Vec2{1.0f, 1.0f});
    e.tri(fA, fC, fB);
    // trás (−X)
    const u16 bA = e.vert(Vec3{-s, -h, -s}, Vec3{-1.0f, 0.0f, 0.0f}, Vec2{0.0f, 0.0f});
    const u16 bB = e.vert(Vec3{-s, -h,  s}, Vec3{-1.0f, 0.0f, 0.0f}, Vec2{1.0f, 0.0f});
    const u16 bC = e.vert(Vec3{-s,  h, -s}, Vec3{-1.0f, 0.0f, 0.0f}, Vec2{1.0f, 1.0f});
    e.tri(bA, bB, bC);
    // chão (y=−h, normal −Y)
    const u16 g0 = e.vert(Vec3{-s, -h, -s}, Vec3{0.0f, -1.0f, 0.0f}, Vec2{0.0f, 0.0f});
    const u16 g1 = e.vert(Vec3{ s, -h, -s}, Vec3{0.0f, -1.0f, 0.0f}, Vec2{1.0f, 0.0f});
    const u16 g2 = e.vert(Vec3{ s, -h,  s}, Vec3{0.0f, -1.0f, 0.0f}, Vec2{1.0f, 1.0f});
    const u16 g3 = e.vert(Vec3{-s, -h,  s}, Vec3{0.0f, -1.0f, 0.0f}, Vec2{0.0f, 1.0f});
    e.quad(g0, g1, g2, g3);
    // rampa: B=(−h,+s) → C=(+h,−s); normal (0, s, h) (para cima e para +Z)
    const Vec3 rn = normalized(Vec3{0.0f, s, h});
    const u16 rB0 = e.vert(Vec3{-s, -h,  s}, rn, Vec2{0.0f, 0.0f});
    const u16 rB1 = e.vert(Vec3{ s, -h,  s}, rn, Vec2{1.0f, 0.0f});
    const u16 rC1 = e.vert(Vec3{ s,  h, -s}, rn, Vec2{1.0f, 1.0f});
    const u16 rC0 = e.vert(Vec3{-s,  h, -s}, rn, Vec2{0.0f, 1.0f});
    e.quad(rB0, rB1, rC1, rC0);
}

// ---- torus (eixo Y; R principal, r tubo) ------------------------------------
// Grelha segments×rings; quad(a,b,d,c) (b na direção do TUBO, d na diagonal)
// — o produto (∂/∂th × ∂/∂ph) aponta para fora do tubo.
void genTorus(const PrimParams& p, PrimMeshData& out) {
    Emit e(out);
    const i32 seg  = p.segments;   // volta principal (ph)
    const i32 ring = p.rings;      // volta do tubo (th)
    const f32 R    = p.radius;
    const f32 r    = p.radius2;
    for (i32 i = 0; i <= seg; ++i) {
        const f32 u  = static_cast<f32>(i) / static_cast<f32>(seg);
        const f32 ph = u * 2.0f * kPi;
        const f32 cp = std::cos(ph), sp = std::sin(ph);
        for (i32 j = 0; j <= ring; ++j) {
            const f32 v  = static_cast<f32>(j) / static_cast<f32>(ring);
            const f32 th = v * 2.0f * kPi;
            const f32 ct = std::cos(th), st = std::sin(th);
            const Vec3 n{cp * ct, st, sp * ct};
            const Vec3 pos{(R + r * ct) * cp, r * st, (R + r * ct) * sp};
            e.vert(pos, n, Vec2{u, v});
        }
    }
    const i32 row = ring + 1;
    for (i32 i = 0; i < seg; ++i) {
        for (i32 j = 0; j < ring; ++j) {
            const u16 a = static_cast<u16>(i * row + j);
            const u16 b = static_cast<u16>(i * row + j + 1);
            const u16 c = static_cast<u16>((i + 1) * row + j);
            const u16 d = static_cast<u16>((i + 1) * row + j + 1);
            e.quad(a, b, d, c);
        }
    }
}

// ---- cápsula (altura TOTAL = 2r + cilindro; anéis por hemisfério) -----------
// Estrutura: polo +Y → hr anéis (hemisfério) → equador → cilindro →
// equador → hr anéis (hemisfério) → polo −Y. Fans nos polos (como as tampas
// do cilindro), quads entre anéis (winding da esfera), cilindro entre os
// equadores (winding do cilindro). hr = rings/2 (min 1).
void genCapsule(const PrimParams& p, PrimMeshData& out) {
    Emit e(out);
    const i32 seg = p.segments;
    const i32 hr  = p.rings / 2 < 1 ? 1 : p.rings / 2;
    const f32 r   = p.radius;
    const f32 hh  = (p.height * 0.5f) - r;   // meia-altura CILÍNDRICA (>= 0 pós-clamp)

    // anel a `phi` (ângulo desde +Y) com offset vertical `y0`; uv.y = phi/pi
    auto ringAt = [&](f32 phi, f32 y0) {
        const f32 sp = std::sin(phi), cp = std::cos(phi);
        const f32 vv = phi / kPi;
        for (i32 s = 0; s <= seg; ++s) {
            const f32 u  = static_cast<f32>(s) / static_cast<f32>(seg);
            const f32 th = u * 2.0f * kPi;
            const Vec3 n{sp * std::cos(th), cp, sp * std::sin(th)};
            e.vert(Vec3{n.x * r, y0 + cp * r, n.z * r}, n, Vec2{u, vv});
        }
    };
    // liga anel ACIMA (menor phi) ao anel ABAIXO (maior phi) — winding da
    // esfera: quad(above_s, above_{s+1}, below_{s+1}, below_s)
    auto linkRings = [&]() {
        const u16 below = static_cast<u16>(out.vertices.size() - (seg + 1));
        const u16 above = static_cast<u16>(below - static_cast<u16>(seg + 1));
        for (i32 s = 0; s < seg; ++s) {
            e.quad(above + static_cast<u16>(s), above + static_cast<u16>(s + 1),
                   below + static_cast<u16>(s + 1), below + static_cast<u16>(s));
        }
    };

    // polo +Y + primeiro anel (fan)
    const u16 pTop = e.vert(Vec3{0.0f, hh + r, 0.0f}, Vec3{0.0f, 1.0f, 0.0f},
                             Vec2{0.5f, 0.0f});
    ringAt((1.0f / static_cast<f32>(hr)) * (kPi * 0.5f), hh);
    {
        const u16 rb = static_cast<u16>(out.vertices.size() - (seg + 1));
        for (i32 s = 0; s < seg; ++s) {
            e.tri(pTop, rb + static_cast<u16>(s + 1), rb + static_cast<u16>(s));
        }
    }
    // hemisfério de cima: anéis 2..hr (o hr-ésimo é o EQUADOR)
    for (i32 k = 2; k <= hr; ++k) {
        ringAt((static_cast<f32>(k) / static_cast<f32>(hr)) * (kPi * 0.5f), hh);
        linkRings();
    }
    // cilindro: equador de cima (último anel) ↔ equador de baixo (novo)
    ringAt(kPi * 0.5f, -hh);
    {
        const u16 bot = static_cast<u16>(out.vertices.size() - (seg + 1));
        const u16 top = static_cast<u16>(bot - static_cast<u16>(seg + 1));
        for (i32 s = 0; s < seg; ++s) {
            e.quad(top + static_cast<u16>(s), top + static_cast<u16>(s + 1),
                   bot + static_cast<u16>(s + 1), bot + static_cast<u16>(s));
        }
    }
    // hemisfério de baixo: anéis 1..hr−1 (phi CRESCENTE do equador para o
    // polo — linkRings assume "novo anel = phi maior = mais abaixo")
    for (i32 k = 1; k <= hr - 1; ++k) {
        ringAt(kPi * 0.5f + (static_cast<f32>(k) / static_cast<f32>(hr)) * (kPi * 0.5f),
               -hh);
        linkRings();
    }
    // polo −Y + fan no último anel (k=hr−1, o mais próximo do polo; hr=1 →
    // o fan liga o polo diretamente ao equador de baixo)
    const u16 pBot = e.vert(Vec3{0.0f, -hh - r, 0.0f}, Vec3{0.0f, -1.0f, 0.0f},
                             Vec2{0.5f, 1.0f});
    {
        const u16 rb = static_cast<u16>(out.vertices.size() - (seg + 1));
        for (i32 s = 0; s < seg; ++s) {
            e.tri(pBot, rb + static_cast<u16>(s), rb + static_cast<u16>(s + 1));
        }
    }
}

} // namespace

const char* primName(PrimKind k) {
    switch (k) {
        case PrimKind::Sphere:   return "esfera";
        case PrimKind::Cylinder: return "cilindro";
        case PrimKind::Cone:     return "cone";
        case PrimKind::Box:      return "box";
        case PrimKind::Plane:    return "plano";
        case PrimKind::Wedge:    return "triangulo";
        case PrimKind::Torus:    return "torus";
        case PrimKind::Capsule:  return "capsula";
        default:                 return "esfera";
    }
}

const char* primLabel(PrimKind k) {
    return primName(k);
}

PrimKind primFromName(const std::string& name) {
    if (name == "cilindro")  return PrimKind::Cylinder;
    if (name == "cone")      return PrimKind::Cone;
    if (name == "box")       return PrimKind::Box;
    if (name == "plano")     return PrimKind::Plane;
    if (name == "triangulo") return PrimKind::Wedge;
    if (name == "torus")     return PrimKind::Torus;
    if (name == "capsula")   return PrimKind::Capsule;
    return PrimKind::Sphere;   // "esfera" e desconhecidos
}

void primClamp(PrimParams& p) {
    if (p.radius < 0.01f)  p.radius = 0.01f;
    if (p.radius2 < 0.01f) p.radius2 = 0.01f;
    if (p.size < 0.01f)    p.size = 0.01f;
    if (p.height < 0.02f)  p.height = 0.02f;
    if (p.segments < 3)    p.segments = 3;
    if (p.segments > 64)   p.segments = 64;
    if (p.rings < 2)       p.rings = 2;
    if (p.rings > 64)      p.rings = 64;
    if (p.kind == PrimKind::Capsule && p.height < 2.0f * p.radius) {
        p.height = 2.0f * p.radius;   // cápsula nunca degenera
    }
    if (p.kind == PrimKind::Torus && p.radius2 >= p.radius) {
        p.radius2 = p.radius * 0.5f;  // tubo nunca engole o raio principal
    }
}

PrimParams primDefaults(PrimKind k) {
    PrimParams p;
    p.kind = k;
    switch (k) {
        case PrimKind::Sphere:
            p.radius = 0.5f; p.segments = 16; p.rings = 12;
            break;
        case PrimKind::Cylinder:
            p.radius = 0.4f; p.height = 1.0f; p.segments = 16;
            break;
        case PrimKind::Cone:
            p.radius = 0.5f; p.height = 1.0f; p.segments = 16;
            break;
        case PrimKind::Box:
            p.size = 1.0f;
            break;
        case PrimKind::Plane:
            p.size = 2.0f;
            break;
        case PrimKind::Wedge:
            p.size = 1.0f;
            break;
        case PrimKind::Torus:
            p.radius = 0.5f; p.radius2 = 0.15f; p.segments = 20; p.rings = 10;
            break;
        case PrimKind::Capsule:
            p.radius = 0.3f; p.height = 1.0f; p.segments = 16; p.rings = 8;
            break;
        default:
            break;
    }
    primClamp(p);
    return p;
}

void makePrimMesh(const PrimParams& pIn, PrimMeshData& out) {
    out.clear();
    PrimParams p = pIn;
    primClamp(p);
    switch (p.kind) {
        case PrimKind::Sphere:   genSphere(p, out);   break;
        case PrimKind::Cylinder: genCylinder(p, out); break;
        case PrimKind::Cone:     genCone(p, out);     break;
        case PrimKind::Box: {
            // mesmo gerador do cubo da F2 (24 verts/36 índices — winding da F2)
            const CubeMeshData cube = makeCube(p.size);
            out.vertices.assign(cube.vertices.begin(), cube.vertices.end());
            out.indices.assign(cube.indices.begin(), cube.indices.end());
            break;
        }
        case PrimKind::Plane:    genPlane(p, out);    break;
        case PrimKind::Wedge:    genWedge(p, out);    break;
        case PrimKind::Torus:    genTorus(p, out);    break;
        case PrimKind::Capsule:  genCapsule(p, out);  break;
        default:                 genSphere(p, out);   break;
    }
}

void primBounds(const PrimMeshData& m, Vec3& mn, Vec3& mx) {
    mn = Vec3{1e9f, 1e9f, 1e9f};
    mx = Vec3{-1e9f, -1e9f, -1e9f};
    for (const Vertex& v : m.vertices) {
        mn.x = mn.x < v.pos.x ? mn.x : v.pos.x;
        mn.y = mn.y < v.pos.y ? mn.y : v.pos.y;
        mn.z = mn.z < v.pos.z ? mn.z : v.pos.z;
        mx.x = mx.x > v.pos.x ? mx.x : v.pos.x;
        mx.y = mx.y > v.pos.y ? mx.y : v.pos.y;
        mx.z = mx.z > v.pos.z ? mx.z : v.pos.z;
    }
    if (m.vertices.empty()) {
        mn = Vec3{0.0f, 0.0f, 0.0f};
        mx = Vec3{0.0f, 0.0f, 0.0f};
    }
}

} // namespace vv
