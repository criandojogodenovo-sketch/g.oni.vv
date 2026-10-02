// tests/test_prims.cpp — 0.8.10: PRIMITIVAS MESH PROCEDURAIS (SÓ CUBO E ESFERA).
//
// Aferição da spec 0.8.10 (a redução decidida pelo dono — a intermitência
// do C33 vivia no caminho partilhado de cache, não nos geradores):
//   • AS DUAS formas geram geometria VÁLIDA: vértices/índices não vazios,
//     índices múltiplos de 3 e dentro do intervalo, normais ~unitárias,
//     uv dentro de [0,1], SEM vértices degenerados (NaN);
//   • WINDING coerente: a normal de cada triângulo (cross product) CONCORDA
//     com as normais dos seus vértices (dot > 0) — CCW visto de fora;
//   • bounding box COERENTE (esfera ±r, box ±size/2 com 24/36 do makeCube);
//   • parâmetros respeitados (raio 2 → bounds ±2; seg/rings contam vértices);
//   • clamps defensivos (segmentos <3, raio 0, caps 64 → nunca estoura u16);
//   • PUREZA (0.8.10): mesmos parâmetros → BYTES IDÊNTICOS (primMeshHash
//     duas chamadas, dois tipos + params não-default); params diferentes →
//     hashes DIFERENTES;
//   • nomes ↔ tipos round-trip; MIGRAÇÃO: "cilindro"/"cone"/"plano"/
//     "triangulo"/"torus"/"capsula" → primRemoved=true, primFromName=Box;
//   • SERIALIZAÇÃO: "mesh":"prim" + bloco "prim" grava TIPO+parâmetros;
//     round-trip SEM resolver (mesh null = pendente, primOn/params ficam);
//     .goni ANTIGO com prim removida carrega como box + callback chamado.
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
#include "render/Mesh.h"
#include "render/Primitives.h"

using namespace vv;
using namespace test;   // nearEqF/vecNearF

namespace {

// 0.8.10: as DUAS formas que restam
const PrimKind kAllKinds[2] = {PrimKind::Sphere, PrimKind::Box};

// os SEIS nomes removidos (cada um migra para cube no load)
const char* kRemovedNames[6] = {"cilindro", "cone", "plano", "triangulo",
                                "torus", "capsula"};

// normal de um triângulo (cross product dos edges)
Vec3 faceNormal(const Vertex& a, const Vertex& b, const Vertex& c) {
    const Vec3 e1{b.pos.x - a.pos.x, b.pos.y - a.pos.y, b.pos.z - a.pos.z};
    const Vec3 e2{c.pos.x - a.pos.x, c.pos.y - a.pos.y, c.pos.z - a.pos.z};
    return Vec3{e1.y * e2.z - e1.z * e2.y,
                e1.z * e2.x - e1.x * e2.z,
                e1.x * e2.y - e1.y * e2.x};
}

} // namespace

// ---- 1. geometria válida (as duas formas) ----------------------------------

TEST(prim_geram_geometria_valida) {
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
        for (size_t i = 0; i + 2 < m.indices.size(); i += 3) {
            const Vertex& a = m.vertices[m.indices[i]];
            const Vertex& b = m.vertices[m.indices[i + 1]];
            const Vertex& c = m.vertices[m.indices[i + 2]];
            const Vec3 fn = faceNormal(a, b, c);
            const f32 len = std::sqrt(fn.x * fn.x + fn.y * fn.y + fn.z * fn.z);
            if (len < 1e-9f) {
                continue;   // triângulo degenerado — inofensivo
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
    // BOX size=1 → cubo da F2 (±0.5; 24 verts/36 índices)
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
    // box tamanho 3 → ±1.5
    {
        PrimParams p = primDefaults(PrimKind::Box);
        p.size = 3.0f;
        PrimMeshData m;
        makePrimMesh(p, m);
        Vec3 mn, mx;
        primBounds(m, mn, mx);
        EXPECT(nearEqF(mx.x, 1.5f, 1e-3f));
        EXPECT(nearEqF(mn.z, -1.5f, 1e-3f));
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
    // dimensões negativas/zero → mínimo positivo
    {
        PrimParams p = primDefaults(PrimKind::Sphere);
        p.radius = -5.0f;
        primClamp(p);
        EXPECT(p.radius > 0.0f);
    }
    {
        PrimParams p = primDefaults(PrimKind::Box);
        p.size = 0.0f;
        primClamp(p);
        EXPECT(p.size > 0.0f);
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

// ---- 3b. PUREZA (0.8.10): mesmos params → bytes idênticos --------------------

TEST(prim_pureza_mesmos_params_mesmo_hash) {
    // os DOIS tipos, defaults e params não-default: duas gerações
    // INDEPENDENTES → hash IGUAL (o gerador é função pura dos parâmetros)
    const PrimParams cases[] = {
        primDefaults(PrimKind::Sphere),
        primDefaults(PrimKind::Box),
    };
    for (const PrimParams& p : cases) {
        PrimMeshData a, b;
        makePrimMesh(p, a);
        makePrimMesh(p, b);
        EXPECT(a.vertices.size() == b.vertices.size());
        EXPECT(a.indices.size() == b.indices.size());
        EXPECT(primMeshHash(a) == primMeshHash(b));
    }
    // params NÃO-default (o slider do dono mexeu no raio)
    {
        PrimParams p = primDefaults(PrimKind::Sphere);
        p.radius = 0.83f;
        p.segments = 24;
        p.rings = 17;
        PrimMeshData a, b;
        makePrimMesh(p, a);
        makePrimMesh(p, b);
        EXPECT(primMeshHash(a) == primMeshHash(b));
    }
}

TEST(prim_pureza_params_diferentes_hash_diferente) {
    PrimMeshData a, b, c;
    PrimParams sph = primDefaults(PrimKind::Sphere);
    makePrimMesh(sph, a);
    PrimParams box = primDefaults(PrimKind::Box);
    makePrimMesh(box, b);
    sph.radius = 0.9f;   // um pixel de slider
    makePrimMesh(sph, c);
    EXPECT(primMeshHash(a) != primMeshHash(b));
    EXPECT(primMeshHash(a) != primMeshHash(c));
}

// ---- 4. nomes ↔ tipos + MIGRAÇÃO ----------------------------------------------

TEST(prim_nomes_roundtrip) {
    for (const PrimKind k : kAllKinds) {
        EXPECT(std::strcmp(primName(k), primName(primFromName(primName(k)))) == 0);
        EXPECT(primFromName(primName(k)) == k);
    }
    // desconhecido → esfera (defesa)
    EXPECT(primFromName("hipercubo") == PrimKind::Sphere);
    // nomes PT (a UI é PT)
    EXPECT(std::strcmp(primName(PrimKind::Sphere), "esfera") == 0);
    EXPECT(std::strcmp(primName(PrimKind::Box), "box") == 0);
    // contador: PRIMKIND SÃO DUAS (enum encolheu de 8)
    EXPECT(static_cast<int>(PrimKind::Count) == 2);
}

TEST(prim_migracao_nomes_removidos) {
    // cada um dos SEIS nomes removidos: primRemoved=true, primFromName→Box
    for (const char* name : kRemovedNames) {
        EXPECT(primRemoved(name));
        EXPECT(primFromName(name) == PrimKind::Box);   // defesa
    }
    // os vivos NÃO são "removidos"
    EXPECT(!primRemoved("esfera"));
    EXPECT(!primRemoved("box"));
    EXPECT(!primRemoved("coisaestranha"));
}

TEST(prim_defaults_sensatos) {
    for (const PrimKind k : kAllKinds) {
        const PrimParams p = primDefaults(k);
        EXPECT(p.kind == k);
        EXPECT(p.radius > 0.0f && p.radius <= 2.0f);     // escala de cena 1-10 u
        EXPECT(p.segments >= 3 && p.segments <= 64);     // suave mas leve
        EXPECT(p.rings >= 2 && p.rings <= 64);
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

TEST(prim_serializacao_roundtrip_sem_resolver_pendente) {
    // 0.8.10: SEM resolver (o resolvePrim morreu) — primOn+params ficam,
    // mesh null = PEDIDO PENDENTE (o main sobe no ponto seguro do frame)
    Scene s;
    const Handle h = s.create("Bola");
    Tic* tic = s.get(h);
    tic->addComponent<Transform3D>();
    MeshRenderer* mr = tic->addComponent<MeshRenderer>();
    mr->primOn = true;
    mr->prim = primDefaults(PrimKind::Sphere);
    mr->prim.radius = 0.83f;
    mr->prim.segments = 24;

    const std::string text = SceneSerializer::dump(s);
    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(s2, text, ctx));
    Tic* back = s2.get(s2.find("Bola"));
    ASSERT(back != nullptr);
    MeshRenderer* mr2 = back->getComponent<MeshRenderer>();
    ASSERT(mr2 != nullptr);
    EXPECT(mr2->primOn);
    EXPECT(mr2->prim.kind == PrimKind::Sphere);
    EXPECT(nearEqF(mr2->prim.radius, 0.83f, 1e-4f));
    EXPECT(mr2->prim.segments == 24);
    EXPECT(mr2->mesh == nullptr);   // pendente — sobe no main
    EXPECT(!mr2->primNeg);          // load = pedido novo
}

TEST(prim_serializacao_goni_antigo_prim_removida_migra) {
    // .goni de um projeto 0.8.9 com "cilindro": carrega como BOX (cube) com
    // o callback onPrimMigrated a disparar — NUNCA crash, nunca silêncio
    const char* txt = R"({"version":1,"tics":[
      {"id":0,"name":"T","active":true,"parent":-1,"components":[
        {"type":"MeshRenderer","mesh":"prim","prim":{"type":"cilindro",
         "r":0.4,"h":1.0,"seg":16}}]}]})";
    Scene s;
    SceneSerializer::LoadCtx ctx;
    int calls = 0;
    std::string lastName;
    ctx.onPrimMigrated = [&](const char* removedName) {
        ++calls;
        lastName = removedName;
    };
    ASSERT(SceneSerializer::loadText(s, txt, ctx));
    Tic* tic = s.get(s.find("T"));
    ASSERT(tic != nullptr);
    const MeshRenderer* mr = tic->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    EXPECT(mr->primOn);
    EXPECT(mr->prim.kind == PrimKind::Box);   // MIGRADO
    EXPECT(mr->mesh == nullptr);              // pendente como qualquer prim
    EXPECT(calls == 1);
    EXPECT(lastName == "cilindro");
    // re-save: grava "box" (a prim migrada persiste na forma nova)
    const std::string text2 = SceneSerializer::dump(s);
    EXPECT(std::strstr(text2.c_str(), "\"type\":\"box\"") != nullptr);
    EXPECT(std::strstr(text2.c_str(), "cilindro") == nullptr);
}

TEST(prim_serializacao_params_parciais_default_do_tipo) {
    // bloco "prim" SEM parâmetros → defaults DO TIPO (esfera 0.5/16/12)
    const char* txt = R"({"version":1,"tics":[
      {"id":0,"name":"T","active":true,"parent":-1,"components":[
        {"type":"MeshRenderer","mesh":"prim","prim":{"type":"esfera"}}]}]})";
    Scene s;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(s, txt, ctx));
    Tic* tic = s.get(s.find("T"));
    ASSERT(tic != nullptr);
    const MeshRenderer* mr = tic->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    EXPECT(mr->primOn);
    EXPECT(mr->prim.kind == PrimKind::Sphere);
    const PrimParams def = primDefaults(PrimKind::Sphere);
    EXPECT(nearEqF(mr->prim.radius, def.radius));
    EXPECT(mr->prim.segments == def.segments);
    EXPECT(mr->prim.rings == def.rings);
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
