// render/Primitives.cpp — geradores das primitivas procedurais (0.8.0;
// 0.8.10: SÓ esfera e box — decisão do dono, ver Primitives.h).
//
// WINDING: CCW visto de fora (front face default do GLES). A regra usada em
// TODOS os geradores de grelha: o triângulo (a, b, d) com b−a ∝ ∂P/∂u e
// d−a ∝ ∂P/∂u + ∂P/∂v tem normal ∝ (∂P/∂u × ∂P/∂v); a orientação da grelha
// foi escolhida para esse produto apontar PARA FORA — e o teste do CI
// (test_prims) confirma triângulo a triângulo que a normal da face concorda
// com as normais dos vértices (dot > 0).
//
// Índice u16: os clamps (segments ≤ 64, rings ≤ 64) garantem folga larga ao
// limite de 65535 (pior caso esfera 65×65 = 4225 vértices).
//
// 0.8.10 — PUREZA: nenhum gerador lê estado global, relógio, RNG ou ordem
// anterior; a saída é função EXCLUSIVA dos parâmetros clampados (o hash do
// CI aperia isso a cada corrida).
#include "render/Primitives.h"
#include "render/Cube.h"
#include <cmath>
#include <cstring>

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

} // namespace

const char* primName(PrimKind k) {
    switch (k) {
        case PrimKind::Sphere: return "esfera";
        case PrimKind::Box:    return "box";
        default:               return "esfera";
    }
}

const char* primLabel(PrimKind k) {
    return primName(k);
}

PrimKind primFromName(const std::string& name) {
    if (name == "box") return PrimKind::Box;
    if (primRemoved(name)) {
        return PrimKind::Box;   // defesa: prim removida → migração p/ cube
    }
    return PrimKind::Sphere;   // "esfera" e desconhecidos
}

bool primRemoved(const std::string& name) {
    return name == "cilindro" || name == "cone" || name == "plano" ||
           name == "triangulo" || name == "torus" || name == "capsula";
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
}

PrimParams primDefaults(PrimKind k) {
    PrimParams p;
    p.kind = k;
    switch (k) {
        case PrimKind::Sphere:
            p.radius = 0.5f; p.segments = 16; p.rings = 12;
            break;
        case PrimKind::Box:
            p.size = 1.0f;
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
        case PrimKind::Sphere:
            genSphere(p, out);
            break;
        case PrimKind::Box: {
            // mesmo gerador do cubo da F2 (24 verts/36 índices — winding da F2)
            const CubeMeshData cube = makeCube(p.size);
            out.vertices.assign(cube.vertices.begin(), cube.vertices.end());
            out.indices.assign(cube.indices.begin(), cube.indices.end());
            break;
        }
        default:
            genSphere(p, out);
            break;
    }
}

u64 primMeshHash(const PrimMeshData& m) {
    // FNV-1a 64-bit: começa nos verts (pos/normal/uv bit-a-bit — floats
    // determinísticos geram os MESMOS bits) e termina nos índices. Ordem
    // fixa → o hash é estável entre chamadas, processos e plataformas
    // little-endian (o teste de pureza do CI compara exatamente isto).
    u64 h = 1469598103934665603ull;
    auto mix = [&h](const void* data, size_t n) {
        const u8* p = static_cast<const u8*>(data);
        for (size_t i = 0; i < n; ++i) {
            h ^= p[i];
            h *= 1099511628211ull;
        }
    };
    for (const Vertex& v : m.vertices) {
        mix(&v.pos.x, sizeof(f32) * 3);
        mix(&v.normal.x, sizeof(f32) * 3);
        mix(&v.uv.x, sizeof(f32) * 2);
    }
    if (!m.indices.empty()) {
        mix(m.indices.data(), m.indices.size() * sizeof(u16));
    }
    return h;
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
