// tests/test_fase35a_lod_cull.cpp — 0.11-T (FASE 3.5a): SKINNED CULL + LOD
// POR DISTÂNCIA — os 5 pins da sub-fase (a spec do dono).
//
//   1. fase35a_skinned_cull — 10 personagens skinados, 5 fora do frustum:
//      skinned: culled=5 (o AnimationSystem salta advance+apply — o tempo
//      do clip congela); sem frustum injetado o custo é ZERO (o de sempre);
//   2. fase35a_lod_gera — o import (writeGMesh withLod) gera a cauda GLOD
//      com o LOD a ½ dos vértices (parity-weld: a grelha 32×32 → 512);
//   3. fase35a_lod_usa — o TIC com LOD on desenha a versão CERTA à
//      distância (perto → normal; longe → a entrada LOD, ½ verts);
//   4. fase35a_lod_off_default — TIC novo tem LOD off (o default jogável
//      manda); o "lodDist" só nasce no .goni quando != 0;
//   5. fase35a_compat_011s — o .goni da 0.11-S abre sem LOD e fica byte a
//      byte; o .gmesh SEM cauda GLOD desenha normal com o LOD "ligado".
#include "TestFramework.h"

#include <GLES3/gl3.h>   // stub do hospedeiro (glstub::stats)

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

#include "assets/GOwnFormats.h"
#include "components/AnimationPlayer.h"
#include "components/MeshRenderer.h"
#include "components/SkeletonComp.h"
#include "components/Transform3D.h"
#include "core/AnimationSystem.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "render/BlockMesh.h"
#include "render/Mesh.h"
#include "render/Renderer.h"
#include "FakeStorage.h"

using namespace vv;
using test::nearEqF;

namespace {

// ---- a GRELHA (o fixture do LOD): row-major cols×rows verts, 2 tris por
// célula — os pares de índices consecutivos são ADJACENTES (a distância do
// passo), logo o parity-weld solda TODOS os ímpares: o LOD tem EXATAMENTE
// metade dos vértices (o pin 2 aferre o número exato).
void makeGridMeshData(u32 cols, u32 rows, f32 step, MeshData& out) {
    out = MeshData{};
    const u32 n = cols * rows;
    out.vertices.resize(n);
    for (u32 j = 0; j < rows; ++j) {
        for (u32 i = 0; i < cols; ++i) {
            Vertex& v = out.vertices[static_cast<size_t>(j * cols + i)];
            v.pos = Vec3{static_cast<f32>(i) * step, 0.0f,
                         static_cast<f32>(j) * step};
            v.normal = Vec3{0.0f, 1.0f, 0.0f};
            v.uv = Vec2{static_cast<f32>(i) / static_cast<f32>(cols - 1),
                        static_cast<f32>(j) / static_cast<f32>(rows - 1)};
        }
    }
    out.indices.reserve((cols - 1) * (rows - 1) * 6);
    for (u32 j = 0; j + 1 < rows; ++j) {
        for (u32 i = 0; i + 1 < cols; ++i) {
            const u32 a = j * cols + i;
            const u32 b = a + 1;
            const u32 c = a + cols;
            const u32 d = c + 1;
            out.indices.push_back(static_cast<u16>(a));
            out.indices.push_back(static_cast<u16>(c));
            out.indices.push_back(static_cast<u16>(b));
            out.indices.push_back(static_cast<u16>(b));
            out.indices.push_back(static_cast<u16>(c));
            out.indices.push_back(static_cast<u16>(d));
        }
    }
}

// o personagem MÍNIMO skinado do cull: 2 triângulos, bounds [−0.5, 0.5]³,
// 1 joint com influência total (o skinVertex do CI usa a mesma receita)
void makeSkinnedMeshData(MeshData& out) {
    out = MeshData{};
    const Vec3 P[4] = {Vec3{-0.5f, -0.5f, 0.0f}, Vec3{0.5f, -0.5f, 0.0f},
                       Vec3{0.5f, 0.5f, 0.0f}, Vec3{-0.5f, 0.5f, 0.0f}};
    for (int k = 0; k < 4; ++k) {
        Vertex v;
        v.pos = P[k];
        v.normal = Vec3{0.0f, 0.0f, 1.0f};
        out.vertices.push_back(v);
        for (int c = 0; c < 4; ++c) {
            out.skinJoints.push_back(0);
            out.skinWeights.push_back(c == 0 ? 1.0f : 0.0f);
        }
    }
    const u16 IDX[6] = {0, 1, 2, 0, 2, 3};
    out.indices.assign(IDX, IDX + 6);
}

// o clip de 1 track TicPos (0 → 1 s: y sobe 0→1) — o apply move o TIC e o
// advance mexe o `time`; o pin aferre o tempo CONGELADO dos culled
void addWalkClip(AnimationPlayer& pl) {
    AnimClip& clip = *pl.editClip();
    clip.name = "andar";
    AnimTrack& tr = *pl.addTrack(AnimTarget::TicPos);
    AnimKey k0;
    k0.t = 0.0f;
    k0.v[1] = 0.0f;
    AnimKey k1;
    k1.t = 1.0f;
    k1.v[1] = 1.0f;
    tr.keys.push_back(k0);
    tr.keys.push_back(k1);
    pl.activeClip = 0;
    pl.playing = true;
    pl.time = 0.0f;
}

} // namespace

// ---- PIN 1 — o skinned cull (10 personagens, 5 fora do frustum) ------------

TEST(fase35a_skinned_cull) {
    glstub::reset();
    Scene scene;
    MeshData md;
    makeSkinnedMeshData(md);
    Mesh skinned;
    EXPECT(skinned.createSkinned(md.vertices.data(), 4, md.indices.data(), 6,
                                 md.skinJoints.data(), md.skinWeights.data()));
    // 10 personagens: 5 À FRENTE (z=−10, x=−8..+8 — dentro do ortho ±10) e
    // 5 ATRÁS da câmara (z=+10 — TODOS os cantos fora do plano near)
    Handle hs[10];
    for (u32 k = 0; k < 10; ++k) {
        char nm[24];
        std::snprintf(nm, sizeof(nm), "P%u", k);
        hs[k] = scene.create(nm);
        Tic* t = scene.get(hs[k]);
        ASSERT(t != nullptr);
        Transform3D* tr = t->addComponent<Transform3D>();
        const bool front = k < 5;
        tr->pos = Vec3{front ? -8.0f + 4.0f * static_cast<f32>(k)
                             : -8.0f + 4.0f * static_cast<f32>(k - 5),
                       0.0f, front ? -10.0f : 10.0f};
        tr->updateWorld();
        MeshRenderer* mr = t->addComponent<MeshRenderer>();
        mr->mesh = &skinned;   // o teste é o dono (o padrão dos prims)
        t->addComponent<SkeletonComp>();
        AnimationPlayer* pl = t->addComponent<AnimationPlayer>();
        addWalkClip(*pl);
    }
    // o ortho que vê a FRENTE (z∈[−100,−0.1] em coordenadas de câmara — os
    // z=+10 ficam ATRÁS do plano near: TODOS os cantos fora → culled)
    const Mat4 vp = Mat4::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
    f32 planes[6][4];
    BlockMesh::frustumPlanes(vp, planes);

    AnimationSystem sys;
    sys.enabled = true;
    sys.frustum = planes;
    sys.frustumValid = true;
    const f32 dt = 1.0f / 60.0f;
    sys.tick(scene, dt);

    // O PIN: 5 fora do frustum → skinned: culled=5 (o número do dono)
    EXPECT(sys.skinnedCulled == 5);
    EXPECT(sys.culledOwners.size() == 5);
    // os culled NÃO animaram (o tempo do clip congelou em 0); os visíveis
    // avançaram dt (o advance correu — o par do contrato)
    auto& players = scene.components().animators();
    u32 frozen = 0, advanced = 0;
    for (u32 i = 0; i < players.size(); ++i) {
        const AnimationPlayer& pl = players.at(i);
        bool isCulled = false;
        for (const Handle& h : sys.culledOwners) {
            isCulled = isCulled || (h == players.owner(i));
        }
        if (isCulled) {
            EXPECT(pl.time == 0.0f);   // congelado (nem advance nem apply)
            ++frozen;
        } else {
            EXPECT(nearEqF(pl.time, dt));   // avançou o passo
            ++advanced;
        }
    }
    EXPECT(frozen == 5);
    EXPECT(advanced == 5);
    // CUSTO ZERO: sem frustum injetado o teste nem corre (o de sempre)
    sys.frustumValid = false;
    sys.tick(scene, dt);
    EXPECT(sys.skinnedCulled == 0);
    EXPECT(sys.culledOwners.empty());
}

// ---- PIN 2 — o import gera o LOD a ½ (a cauda GLOD) ------------------------

TEST(fase35a_lod_gera) {
    glstub::reset();
    MeshData grid;
    makeGridMeshData(32, 32, 1.0f, grid);   // 1024 verts, 1922 tris
    EXPECT(grid.vertices.size() == 1024);
    std::string err;

    // COM LOD (o caminho do import): a cauda GLOD nasce
    std::vector<u8> withLod;
    EXPECT(writeGMesh(grid, withLod, err, nullptr, nullptr, /*withLod=*/true));
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> mats;
    std::vector<GMeshV3Block> lod;
    EXPECT(readGMeshV3Meta(withLod.data(), withLod.size(), meta, blocks, mats,
                           err, nullptr, nullptr, &lod));
    EXPECT((meta.flags & kGmeshV3FlagLOD) != 0u);
    EXPECT(blocks.size() == 1);
    EXPECT(lod.size() == blocks.size());   // 1:1 POR ORDEM (o contrato)
    // A CONTA DO PIN: o LOD tem EXATAMENTE metade dos vértices (a grelha
    // row-major solda TODOS os ímpares ao vizinho par — 1024 → 512)
    EXPECT(lod[0].vertexCount == 512);
    EXPECT(lod[0].indexCount > 0);
    EXPECT(lod[0].indexCount < blocks[0].indexCount);   // também decimou tris
    // a entrada valida e MATERIALIZA (o CRC da cauda + o CRC do blob)
    MeshData lodMd;
    EXPECT(gmeshV3MaterializeBlock(withLod.data() + lod[0].dataOffset,
                                   static_cast<size_t>(lod[0].dataSize),
                                   meta, lod[0], lodMd, err));
    EXPECT(lodMd.vertices.size() == 512);
    // o AABB do LOD está CONTIDO no do bloco normal (os mantidos são os
    // originais — a decimação nunca expande)
    EXPECT(lod[0].aabbMin.x >= blocks[0].aabbMin.x - 1e-4f);
    EXPECT(lod[0].aabbMax.x <= blocks[0].aabbMax.x + 1e-4f);

    // SEM LOD (o default do escritor): byte a byte com o pré-3.5a
    std::vector<u8> noLod;
    EXPECT(writeGMesh(grid, noLod, err));
    GMeshV3Meta meta2;
    std::vector<GMeshV3Block> blocks2;
    std::vector<std::string> mats2;
    std::vector<GMeshV3Block> lod2;
    EXPECT(readGMeshV3Meta(noLod.data(), noLod.size(), meta2, blocks2, mats2,
                           err, nullptr, nullptr, &lod2));
    EXPECT((meta2.flags & kGmeshV3FlagLOD) == 0u);
    EXPECT(lod2.empty());
    EXPECT(noLod.size() < withLod.size());
}

// ---- PIN 3 — o TIC com LOD on desenha a versão certa à distância ----------

TEST(fase35a_lod_usa) {
    glstub::reset();
    MeshData grid;
    makeGridMeshData(32, 32, 1.0f, grid);
    std::string err;
    std::vector<u8> bytes;
    EXPECT(writeGMesh(grid, bytes, err, nullptr, nullptr, /*withLod=*/true));
    FakeStorage st;
    st.files["assets/grade.gmesh"].assign(
        reinterpret_cast<const char*>(bytes.data()), bytes.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/grade.gmesh", err));
    EXPECT(bm.hasLod());
    EXPECT(bm.lodBlocks().size() == 1);
    EXPECT(bm.lodBlocks()[0].vertexCount == 512);

    Renderer r;
    const Mat4 model = Mat4::translation(0.0f, 0.0f, -20.0f);
    const Mat4 vp = Mat4::ortho(-50.0f, 50.0f, -50.0f, 50.0f, 0.1f, 400.0f);

    // PERTO (a câmara a 5.5 u do centro; lodDistance=20): o NORMAL desenha
    {
        BlockMesh::Stats bs;
        const Vec3 camPos{0.0f, 0.0f, -10.0f};
        const DrawStats ds = bm.draw(r, model, vp, nullptr, nullptr, nullptr,
                                     0, bs, &camPos, 20.0f);
        EXPECT(bs.drawn == 1);
        EXPECT(bs.lodDrawn == 0);   // dentro da distância — o normal
        EXPECT(ds.vertices == grid.indices.size());   // o mesh INTEIRO
    }
    // LONGE (a câmara a ~104 u; lodDistance=20): a ENTRADA LOD desenha
    {
        BlockMesh::Stats bs;
        const Vec3 camPos{0.0f, 0.0f, 100.0f};
        const DrawStats ds = bm.draw(r, model, vp, nullptr, nullptr, nullptr,
                                     0, bs, &camPos, 20.0f);
        EXPECT(bs.drawn == 1);
        EXPECT(bs.lodDrawn == 1);   // a versão CERTA à distância
        // A PROVA DOS VÉRTICES: o desenho consumiu o LOD (os índices da
        // entrada LOD — a metade; o ds.vertices conta ÍNDICES desenhados)
        EXPECT(ds.vertices == bm.lodBlocks()[0].indexCount);
        EXPECT(ds.vertices < grid.indices.size());   // menos de metade do tris
    }
    // O DEFAULT OFF (o pin 4 no nível do draw): lodDistance=0 NUNCA liga
    {
        BlockMesh::Stats bs;
        const Vec3 camPos{0.0f, 0.0f, 100.0f};
        bm.draw(r, model, vp, nullptr, nullptr, nullptr, 0, bs, &camPos,
                0.0f);
        EXPECT(bs.drawn == 1);
        EXPECT(bs.lodDrawn == 0);   // off — o normal desenha mesmo longe
    }
    // A ENTRADA LOD MATERIALIZA (o que o draw lazy subiu é o mesh a ½)
    {
        MeshData lodMd;
        const u32 lodEntry =
            static_cast<u32>(bm.table().size());   // lodBase == table size
        EXPECT(bm.materializeBlock(lodEntry, lodMd, err));
        EXPECT(lodMd.vertices.size() == 512);
    }
}

// ---- PIN 4 — o default é OFF e o "lodDist" só viaja quando != 0 ------------

TEST(fase35a_lod_off_default) {
    glstub::reset();
    // o TIC NOVO: lodDistance == 0 (o default jogável manda)
    MeshRenderer fresh;
    EXPECT(fresh.lodDistance == 0.0f);

    // a cena SEM lod: o .goni NÃO tem a chave (os ficheiros ficam byte a byte)
    Scene s1;
    const Handle h = s1.create("Bloco");
    Tic* t = s1.get(h);
    t->addComponent<Transform3D>();
    t->addComponent<MeshRenderer>();
    const std::string text1 = SceneSerializer::dump(s1);
    EXPECT(std::strstr(text1.c_str(), "lodDist") == nullptr);   // ausente

    // com o LOD ligado (120 u): a chave nasce e VOLTA no load
    MeshRenderer* mr = t->getComponent<MeshRenderer>();
    mr->lodDistance = 120.0f;
    const std::string text2 = SceneSerializer::dump(s1);
    EXPECT(std::strstr(text2.c_str(), "lodDist") != nullptr);
    Scene s2;
    SceneSerializer::LoadCtx ctx;
    EXPECT(SceneSerializer::loadText(s2, text2, ctx));
    const MeshRenderer* mr2 = s2.get(s2.find("Bloco"))->getComponent<MeshRenderer>();
    ASSERT(mr2 != nullptr);
    EXPECT(nearEqF(mr2->lodDistance, 120.0f));

    // o guard do loader: negativo/zero NÃO ligam (o default honesto)
    const std::string bad =
        "{\"version\":1,\"tics\":[{\"id\":0,\"name\":\"X\",\"parent\":-1,"
        "\"components\":[{\"type\":\"MeshRenderer\",\"mesh\":\"cube\","
        "\"lodDist\":-5}]}]}";
    Scene s3;
    EXPECT(SceneSerializer::loadText(s3, bad, ctx));
    const MeshRenderer* mr3 =
        s3.get(s3.find("X"))->getComponent<MeshRenderer>();
    ASSERT(mr3 != nullptr);
    EXPECT(mr3->lodDistance == 0.0f);

    // o round-trip SEM lod é byte a byte (o save(load(goni)) do contrato)
    Scene s4;
    EXPECT(SceneSerializer::loadText(s4, text1, ctx));
    const std::string text4 = SceneSerializer::dump(s4);
    EXPECT(text1 == text4);
}

// ---- PIN 5 — o .goni/.gmesh da 0.11-S abrem sem LOD (compat) ---------------

TEST(fase35a_compat_011s) {
    glstub::reset();
    // (a) o .gmesh SEM cauda GLOD (o escritor pré-3.5a): abre, e o draw com
    // o LOD "ligado" desenha o NORMAL (a cauda não existe — honesto)
    MeshData grid;
    makeGridMeshData(32, 32, 1.0f, grid);
    std::string err;
    std::vector<u8> bytes;
    EXPECT(writeGMesh(grid, bytes, err));   // SEM withLod
    FakeStorage st;
    st.files["assets/velho.gmesh"].assign(
        reinterpret_cast<const char*>(bytes.data()), bytes.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/velho.gmesh", err));
    EXPECT(!bm.hasLod());   // sem cauda — o ficheiro antigo não tem LOD
    {
        Renderer r;
        const Mat4 model = Mat4::translation(0.0f, 0.0f, -20.0f);
        const Mat4 vp = Mat4::ortho(-50.0f, 50.0f, -50.0f, 50.0f, 0.1f, 400.0f);
        BlockMesh::Stats bs;
        const Vec3 camPos{0.0f, 0.0f, 100.0f};
        bm.draw(r, model, vp, nullptr, nullptr, nullptr, 0, bs, &camPos,
                100.0f);
        EXPECT(bs.drawn == 1);
        EXPECT(bs.lodDrawn == 0);   // nada de LOD — o normal desenhou
    }
    // (b) o .goni da 0.11-S (sem "lodDist" em lado nenhum) abre com o LOD
    // off em TODOS os MeshRenderers. A BASE é a forma CANÓNICA do dump (o
    // que a 0.11-S gravava — o JSON canónico; um fixture à mão divergia no
    // "active"/"mesh" que o dump normaliza) — e o round-trip é byte a byte.
    const std::string goni011sRaw =
        "{\"version\":1,\"tics\":[{\"id\":0,\"name\":\"Corpo\",\"parent\":-1,"
        "\"components\":[{\"type\":\"Transform3D\",\"pos\":[1,2,3],"
        "\"rot\":[0,0,0,1],\"scale\":[1,1,1]},{\"type\":\"MeshRenderer\","
        "\"mesh\":\"cube\",\"tint\":[1,0.5,0.25]}]}]}";
    Scene sRaw;
    SceneSerializer::LoadCtx ctx;
    EXPECT(SceneSerializer::loadText(sRaw, goni011sRaw, ctx));
    const std::string goni011s = SceneSerializer::dump(sRaw);   // o canónico
    EXPECT(std::strstr(goni011s.c_str(), "lodDist") == nullptr);   // sem LOD
    Scene s;
    EXPECT(SceneSerializer::loadText(s, goni011s, ctx));
    const Tic* t = s.get(s.find("Corpo"));
    ASSERT(t != nullptr);
    const MeshRenderer* mr = t->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    EXPECT(mr->lodDistance == 0.0f);   // OFF — a cena abre como abria
    EXPECT(nearEqF(mr->tint[1], 0.5f));   // o resto intacto
    // (c) o save(load(goni)) é byte a byte (o pin de compat da sub-fase)
    const std::string out = SceneSerializer::dump(s);
    EXPECT(out == goni011s);
}
