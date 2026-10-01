// tests/test_prims.cpp — 0.8.0 (F7): PRIMITIVAS MESH PROCEDURAIS.
//
// Aferição da spec 0.8.0:
//   • CADA primitiva gera geometria VÁLIDA: vértices/índices não vazios,
//     índices múltiplos de 3 e dentro do intervalo, normais ~unitárias,
//     uv dentro de [0,1], SEM vértices degenerados (NaN);
//   • WINDING coerente: a normal de cada triângulo (cross product) CONCORDA
//     com as normais dos seus vértices (dot > 0) — o winding CCW visto de
//     fora que o GLES espera;
//   • bounding box COERENTE com os parâmetros (esfera ±r, cilindro ±r/±h/2,
//     box ±size/2, plano y=0 com ±size/2, torus, cápsula altura TOTAL,
//     cone);
//   • parâmetros respeitados (raio 2 → bounds ±2; segments conta vértices);
//   • clamps defensivos (segmentos <3, cápsula h<2r, torus tubo>r);
//   • nomes ↔ tipos round-trip; defaults sensatos;
//   • SERIALIZAÇÃO: "mesh":"prim" + bloco "prim" grava o TIPO e os
//     parâmetros; round-trip com resolver sentinela rebinda o mesh.
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "render/Mesh.h"   // sentinela do resolver de primitivas
#include "render/Primitives.h"

using namespace vv;
using namespace test;   // nearEqF/vecNearF

namespace {

// as 8 formas (esfera … cápsula)
const PrimKind kAllKinds[8] = {
    PrimKind::Sphere,   PrimKind::Cylinder, PrimKind::Cone,     PrimKind::Box,
    PrimKind::Plane,    PrimKind::Wedge,    PrimKind::Torus,    PrimKind::Capsule,
};

// normal de um triângulo (cross product dos edges)
Vec3 faceNormal(const Vertex& a, const Vertex& b, const Vertex& c) {
    const Vec3 e1{b.pos.x - a.pos.x, b.pos.y - a.pos.y, b.pos.z - a.pos.z};
    const Vec3 e2{c.pos.x - a.pos.x, c.pos.y - a.pos.y, c.pos.z - a.pos.z};
    return Vec3{e1.y * e2.z - e1.z * e2.y,
                e1.z * e2.x - e1.x * e2.z,
                e1.x * e2.y - e1.y * e2.x};
}

} // namespace

// ---- 1. geometria válida (todas as formas) ----------------------------------

TEST(prim_todas_geram_geometria_valida) {
    for (const PrimKind k : kAllKinds) {
        const PrimParams p = primDefaults(k);
        PrimMeshData m;
        makePrimMesh(p, m);
        EXPECT(m.ok());
        EXPECT(m.vertices.size() >= 3);
        EXPECT(m.indices.size() >= 3);
        EXPECT(m.indices.size() % 3 == 0);
        // índices dentro do intervalo
        bool idxOk = true;
        for (const u16 i : m.indices) {
            if (static_cast<size_t>(i) >= m.vertices.size()) {
                idxOk = false;
            }
        }
        EXPECT(idxOk);
        // normais ~unitárias e posições finitas; uv em [0,1]
        bool normsOk = true, finiteOk = true, uvOk = true;
        for (const Vertex& v : m.vertices) {
            const f32 n = std::sqrt(v.normal.x * v.normal.x +
                                    v.normal.y * v.normal.y +
                                    v.normal.z * v.normal.z);
            if (n < 0.99f || n > 1.01f) normsOk = false;
            if (!std::isfinite(v.pos.x) || !std::isfinite(v.pos.y) ||
                !std::isfinite(v.pos.z)) {
                finiteOk = false;
            }
            if (v.uv.x < -1e-4f || v.uv.x > 1.0f + 1e-4f ||
                v.uv.y < -1e-4f || v.uv.y > 1.0f + 1e-4f) {
                uvOk = false;
            }
        }
        EXPECT(normsOk);
        EXPECT(finiteOk);
        EXPECT(uvOk);
    }
}

TEST(prim_winding_concorda_com_as_normais) {
    // a normal da FACE (cross) tem de apontar para o MESMO lado que as
    // normais dos vértices — winding CCW visto de fora, triângulo a triângulo
    for (const PrimKind k : kAllKinds) {
        const PrimParams p = primDefaults(k);
        PrimMeshData m;
        makePrimMesh(p, m);
        u32 bad = 0;
        u32 zero = 0;
        for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
            const Vertex& a = m.vertices[m.indices[i]];
            const Vertex& b = m.vertices[m.indices[i + 1]];
            const Vertex& c = m.vertices[m.indices[i + 2]];
            const Vec3 fn = faceNormal(a, b, c);
            const f32 len = std::sqrt(fn.x * fn.x + fn.y * fn.y + fn.z * fn.z);
            if (len < 1e-9f) {
                ++zero;   // triângulo degenerado (ápice do cone) — inofensivo
                continue;
            }
            const f32 dots[3] = {
                fn.x * a.normal.x + fn.y * a.normal.y + fn.z * a.normal.z,
                fn.x * b.normal.x + fn.y * b.normal.y + fn.z * b.normal.z,
                fn.x * c.normal.x + fn.y * c.normal.y + fn.z * c.normal.z,
            };
            if (dots[0] <= 0.0f || dots[1] <= 0.0f || dots[2] <= 0.0f) {
                ++bad;
            }
        }
        EXPECT(bad == 0);
        (void)zero;
    }
}

// ---- 2. bounding boxes coerentes ---------------------------------------------

TEST(prim_bbox_coerente_com_parametros) {
    // ESFERA r=0.5 → ±0.5 nos três eixos
    {
        PrimMeshData m;
        makePrimMesh(primDefaults(PrimKind::Sphere), m);
        Vec3 mn, mx;
        primBounds(m, mn, mx);
        EXPECT(vecNearF(mn, Vec3{-0.5f, -0.5f, -0.5f}, 1e-3f));
        EXPECT(vecNearF(mx, Vec3{0.5f, 0.5f, 0.5f}, 1e-3f));
    }
    // CILINDRO r=0.4 h=1 → x/z ±0.4, y ±0.5
    {
        PrimMeshData m;
        makePrimMesh(primDefaults(PrimKind::Cylinder), m);
        Vec3 mn, mx;
        primBounds(m, mn, mx);
        EXPECT(nearEqF(mn.x, -0.4f, 1e-3f));
        EXPECT(nearEqF(mx.x, 0.4f, 1e-3f));
        EXPECT(nearEqF(mn.y, -0.5f, 1e-3f));
        EXPECT(nearEqF(mx.y, 0.5f, 1e-3f));
    }
    // CONE r=0.5 h=1 → base −0.5, ápice +0.5, x/z ±0.5
    {
        PrimMeshData m;
        makePrimMesh(primDefaults(PrimKind::Cone), m);
        Vec3 mn, mx;
        primBounds(m, mn, mx);
        EXPECT(nearEqF(mn.y, -0.5f, 1e-3f));
        EXPECT(nearEqF(mx.y, 0.5f, 1e-3f));
        EXPECT(nearEqF(mx.x, 0.5f, 1e-3f));
    }
    // BOX size=1 → cubo da F2 (±0.5)
    {
        PrimMeshData m;
        makePrimMesh(primDefaults(PrimKind::Box), m);
        Vec3 mn, mx;
        primBounds(m, mn, mx);
        EXPECT(vecNearF(mn, Vec3{-0.5f, -0.5f, -0.5f}, 1e-3f));
        EXPECT(vecNearF(mx, Vec3{0.5f, 0.5f, 0.5f}, 1e-3f));
        EXPECT(m.vertices.size() == 24);   // 24/36 do makeCube
        EXPECT(m.indices.size() == 36);
    }
    // PLANO size=2 em y=0 (chão por omissão)
    {
        PrimMeshData m;
        makePrimMesh(primDefaults(PrimKind::Plane), m);
        Vec3 mn, mx;
        primBounds(m, mn, mx);
        EXPECT(nearEqF(mn.y, 0.0f));
        EXPECT(nearEqF(mx.y, 0.0f));
        EXPECT(nearEqF(mn.x, -1.0f, 1e-3f));
        EXPECT(nearEqF(mx.z, 1.0f, 1e-3f));
        EXPECT(m.vertices.size() == 4);
    }
    // TRIÂNGULO/WEDGE size=1 → perfil A(−h,−s) B(−h,+s) C(+h,−s), h=s=0.5
    {
        PrimMeshData m;
        makePrimMesh(primDefaults(PrimKind::Wedge), m);
        Vec3 mn, mx;
        primBounds(m, mn, mx);
        EXPECT(vecNearF(mn, Vec3{-0.5f, -0.5f, -0.5f}, 1e-3f));
        EXPECT(vecNearF(mx, Vec3{0.5f, 0.5f, 0.5f}, 1e-3f));
        // o topo só existe em z=−0.5 (a rampa sobe para −z)
        bool topAtFront = false;
        for (const Vertex& v : m.vertices) {
            if (v.pos.y > 0.4f && v.pos.z < -0.4f) {
                topAtFront = true;
            }
        }
        EXPECT(topAtFront);
    }
    // TORUS R=0.5 tubo=0.15 → y ±0.15, x/z ±0.65 (rings=8: o anel amostra
    // th=90° EXATO — com rings=10 o topo do tubo fica entre anéis)
    {
        PrimParams p = primDefaults(PrimKind::Torus);
        p.rings = 8;
        PrimMeshData m;
        makePrimMesh(p, m);
        Vec3 mn, mx;
        primBounds(m, mn, mx);
        EXPECT(nearEqF(mn.y, -0.15f, 1e-3f));
        EXPECT(nearEqF(mx.y, 0.15f, 1e-3f));
        EXPECT(nearEqF(mx.x, 0.65f, 1e-3f));
        EXPECT(nearEqF(mn.x, -0.65f, 1e-3f));
    }
    // CÁPSULA r=0.3 h=1 → ALTURA TOTAL 1 (y ±0.5), x/z ±0.3
    {
        PrimMeshData m;
        makePrimMesh(primDefaults(PrimKind::Capsule), m);
        Vec3 mn, mx;
        primBounds(m, mn, mx);
        EXPECT(nearEqF(mn.y, -0.5f, 1e-3f));
        EXPECT(nearEqF(mx.y, 0.5f, 1e-3f));
        EXPECT(nearEqF(mx.x, 0.3f, 1e-3f));
        EXPECT(nearEqF(mn.z, -0.3f, 1e-3f));
    }
}

// ---- 3. parâmetros respeitados ------------------------------------------------

TEST(prim_parametros_respeitados) {
    // esfera raio 2 → bounds ±2
    {
        PrimParams p = primDefaults(PrimKind::Sphere);
        p.radius = 2.0f;
        PrimMeshData m;
        makePrimMesh(p, m);
        Vec3 mn, mx;
        primBounds(m, mn, mx);
        EXPECT(nearEqF(mx.x, 2.0f, 1e-3f));
        EXPECT(nearEqF(mn.y, -2.0f, 1e-3f));
    }
    // esfera segments=8, rings=6 → (6+1)×(8+1) = 63 vértices
    {
        PrimParams p = primDefaults(PrimKind::Sphere);
        p.segments = 8;
        p.rings = 6;
        PrimMeshData m;
        makePrimMesh(p, m);
        EXPECT(m.vertices.size() == 63);
        EXPECT(m.indices.size() == 6 * 8 * 6);   // 6·8 quads × 6 índices
    }
    // cilindro altura 3 → y ±1.5
    {
        PrimParams p = primDefaults(PrimKind::Cylinder);
        p.height = 3.0f;
        PrimMeshData m;
        makePrimMesh(p, m);
        Vec3 mn, mx;
        primBounds(m, mn, mx);
        EXPECT(nearEqF(mx.y, 1.5f, 1e-3f));
    }
    // torus tubo 0.3, R 0.5 → y ±0.3 (rings=8 → amostra th=90°)
    {
        PrimParams p = primDefaults(PrimKind::Torus);
        p.rings = 8;
        p.radius2 = 0.3f;
        PrimMeshData m;
        makePrimMesh(p, m);
        Vec3 mn, mx;
        primBounds(m, mn, mx);
        EXPECT(nearEqF(mx.y, 0.3f, 1e-3f));
    }
    // determinismo: mesma assinatura → MESMOS arrays
    {
        PrimParams p = primDefaults(PrimKind::Torus);
        PrimMeshData a, b;
        makePrimMesh(p, a);
        makePrimMesh(p, b);
        EXPECT(a.vertices.size() == b.vertices.size());
        EXPECT(a.indices.size() == b.indices.size());
        bool same = true;
        for (size_t i = 0; i < a.vertices.size(); ++i) {
            if (!vecNearF(a.vertices[i].pos, b.vertices[i].pos, 1e-6f)) {
                same = false;
            }
        }
        EXPECT(same);
    }
}

TEST(prim_clamps_defensivos) {
    // segmentos <3 → 3
    {
        PrimParams p = primDefaults(PrimKind::Sphere);
        p.segments = 0;
        primClamp(p);
        EXPECT(p.segments >= 3);
    }
    // cápsula h < 2r → h = 2r (nunca degenera)
    {
        PrimParams p = primDefaults(PrimKind::Capsule);
        p.radius = 0.5f;
        p.height = 0.5f;   // < 2·0.5
        primClamp(p);
        EXPECT(nearEqF(p.height, 1.0f));
        PrimMeshData m;
        makePrimMesh(p, m);
        Vec3 mn, mx;
        primBounds(m, mn, mx);
        EXPECT(nearEqF(mx.y, 0.5f, 1e-3f));   // altura TOTAL = 2r
    }
    // torus tubo >= R → tubo = R/2
    {
        PrimParams p = primDefaults(PrimKind::Torus);
        p.radius = 0.4f;
        p.radius2 = 0.9f;   // engole
        primClamp(p);
        EXPECT(nearEqF(p.radius2, 0.2f));
    }
    // dimensões negativas/zero → mínimo positivo
    {
        PrimParams p = primDefaults(PrimKind::Sphere);
        p.radius = -5.0f;
        primClamp(p);
        EXPECT(p.radius > 0.0f);
    }
    // segmentos 100 → 64 (cap: nunca estoura u16)
    {
        PrimParams p = primDefaults(PrimKind::Sphere);
        p.segments = 100;
        p.rings = 100;
        primClamp(p);
        EXPECT(p.segments <= 64);
        EXPECT(p.rings <= 64);
        PrimMeshData m;
        makePrimMesh(p, m);
        EXPECT(m.vertices.size() <= 65535);
    }
}

// ---- 4. nomes ↔ tipos ----------------------------------------------------------

TEST(prim_nomes_roundtrip) {
    for (const PrimKind k : kAllKinds) {
        EXPECT(std::strcmp(primName(k), primName(primFromName(primName(k)))) == 0);
        EXPECT(primFromName(primName(k)) == k);
    }
    // desconhecido → esfera (defesa)
    EXPECT(primFromName("hipercubo") == PrimKind::Sphere);
    // nomes PT (a UI é PT)
    EXPECT(std::strcmp(primName(PrimKind::Sphere), "esfera") == 0);
    EXPECT(std::strcmp(primName(PrimKind::Cylinder), "cilindro") == 0);
    EXPECT(std::strcmp(primName(PrimKind::Cone), "cone") == 0);
    EXPECT(std::strcmp(primName(PrimKind::Box), "box") == 0);
    EXPECT(std::strcmp(primName(PrimKind::Plane), "plano") == 0);
    EXPECT(std::strcmp(primName(PrimKind::Wedge), "triangulo") == 0);
    EXPECT(std::strcmp(primName(PrimKind::Torus), "torus") == 0);
    EXPECT(std::strcmp(primName(PrimKind::Capsule), "capsula") == 0);
}

TEST(prim_defaults_sensatos) {
    for (const PrimKind k : kAllKinds) {
        const PrimParams p = primDefaults(k);
        EXPECT(p.kind == k);
        EXPECT(p.radius > 0.0f && p.radius <= 2.0f);     // escala de cena 1-10 u
        EXPECT(p.height > 0.0f && p.height <= 3.0f);
        EXPECT(p.segments >= 3 && p.segments <= 32);     // suave mas leve
        EXPECT(p.rings >= 2 && p.rings <= 32);
        EXPECT(p.size > 0.0f && p.size <= 4.0f);
    }
}

// ---- 5. serialização do tipo + parâmetros --------------------------------------

TEST(prim_serializacao_tipo_e_params_no_goni) {
    Scene s;
    const Handle h = s.create("Bola");
    Tic* tic = s.get(h);
    tic->addComponent<Transform3D>();
    MeshRenderer* mr = tic->addComponent<MeshRenderer>();
    mr->primOn = true;
    mr->prim = primDefaults(PrimKind::Sphere);
    mr->prim.radius = 0.75f;   // params NÃO-default

    const std::string text = SceneSerializer::dump(s);
    EXPECT(std::strstr(text.c_str(), "\"mesh\":\"prim\"") != nullptr);
    EXPECT(std::strstr(text.c_str(), "\"type\":\"esfera\"") != nullptr);
    EXPECT(std::strstr(text.c_str(), "0.75") != nullptr);
}

TEST(prim_serializacao_roundtrip_com_resolver) {
    Scene s;
    const Handle h = s.create("Donut");
    Tic* tic = s.get(h);
    tic->addComponent<Transform3D>();
    MeshRenderer* mr = tic->addComponent<MeshRenderer>();
    mr->primOn = true;
    mr->prim = primDefaults(PrimKind::Torus);
    mr->prim.radius2 = 0.22f;

    const std::string text = SceneSerializer::dump(s);

    // resolver sentinela: devolve ponteiros únicos por assinatura
    static Mesh meshA;   // stub GL: create() nunca chamado, ids a 0 — serve
    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ctx.resolvePrim = [](const PrimParams& p) -> Mesh* {
        // o resolver VÊ os params do ficheiro (tipo+parâmetros)
        EXPECT(p.kind == PrimKind::Torus);
        EXPECT(nearEqF(p.radius2, 0.22f, 1e-4f));
        return &meshA;
    };
    ASSERT(SceneSerializer::loadText(s2, text, ctx));
    Tic* back = s2.get(s2.find("Donut"));
    ASSERT(back != nullptr);
    MeshRenderer* mr2 = back->getComponent<MeshRenderer>();
    ASSERT(mr2 != nullptr);
    EXPECT(mr2->primOn);
    EXPECT(mr2->prim.kind == PrimKind::Torus);
    EXPECT(nearEqF(mr2->prim.radius2, 0.22f, 1e-4f));
    EXPECT(mr2->mesh == &meshA);   // rebind pelo resolver
    EXPECT(mr2->material != nullptr ? true : (ctx.material == nullptr));
}

TEST(prim_serializacao_sem_resolver_entra_na_mesma) {
    // sem resolver: primOn+params FICAM, mesh null (rebind possível) — a
    // política forward-compat do serializer
    Scene s;
    Tic* tic = s.get(s.create("X"));
    tic->addComponent<Transform3D>();
    MeshRenderer* mr = tic->addComponent<MeshRenderer>();
    mr->primOn = true;
    mr->prim = primDefaults(PrimKind::Cone);
    const std::string text = SceneSerializer::dump(s);
    Scene s2;
    SceneSerializer::LoadCtx ctx;   // SEM resolvePrim
    ASSERT(SceneSerializer::loadText(s2, text, ctx));
    if (Tic* back = s2.get(s2.find("X"))) {
        if (const MeshRenderer* mr2 = back->getComponent<MeshRenderer>()) {
            EXPECT(mr2->primOn);
            EXPECT(mr2->prim.kind == PrimKind::Cone);
            EXPECT(mr2->mesh == nullptr);
        }
    }
}

TEST(prim_serializacao_params_parciais_default_do_tipo) {
    // bloco "prim" SEM parâmetros → defaults DO TIPO (torus 0.5/0.15/20/10)
    const char* txt = R"({"version":1,"tics":[
      {"id":0,"name":"T","active":true,"parent":-1,"components":[
        {"type":"MeshRenderer","mesh":"prim","prim":{"type":"torus"}}]}]})";
    Scene s;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(s, txt, ctx));
    Tic* tic = s.get(s.find("T"));
    ASSERT(tic != nullptr);
    const MeshRenderer* mr = tic->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    EXPECT(mr->primOn);
    EXPECT(mr->prim.kind == PrimKind::Torus);
    const PrimParams def = primDefaults(PrimKind::Torus);
    EXPECT(nearEqF(mr->prim.radius, def.radius));
    EXPECT(nearEqF(mr->prim.radius2, def.radius2));
    EXPECT(mr->prim.segments == def.segments);
}

TEST(prim_prim_e_cube_sao_exclusivos) {
    // escolher prim limpa meshPath; escolher cube/file limpa primOn —
    // uma fonte de mesh de cada vez (contrato do applyAssetPick/serializer)
    Scene s;
    Tic* tic = s.get(s.create("A"));
    tic->addComponent<Transform3D>();
    MeshRenderer* mr = tic->addComponent<MeshRenderer>();
    mr->primOn = true;
    mr->prim = primDefaults(PrimKind::Sphere);
    mr->meshPath = "meshes/velho.obj";   // incoerente? o dump decide por primOn
    const std::string text = SceneSerializer::dump(s);
    EXPECT(std::strstr(text.c_str(), "velho.obj") == nullptr);   // prim GANHA
    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(s2, text, ctx));
    if (Tic* back = s2.get(s2.find("A"))) {
        if (const MeshRenderer* mr2 = back->getComponent<MeshRenderer>()) {
            EXPECT(mr2->primOn);
            EXPECT(mr2->meshPath.empty());
        }
    }
}
