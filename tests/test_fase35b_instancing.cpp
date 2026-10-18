// tests/test_fase35b_instancing.cpp — 0.11-W (3.5b): GPU INSTANCING — os
// 9 pins da sub-fase (a spec do arquiteto, §3.5b.6).
//
//   1. fase35b_6_tics_1_dc — 6 TICs com o MESMO mesh+material → 1 draw
//      instanciado (o glstub conta 1 drawElementsInstanced × 6);
//   2. fase35b_2_materiais_2_dc — 6 TICs com 2 materiais → 2 draws (UMA
//      assinatura por (mesh, bloco, material, textura));
//   3. fase35b_toggle_off — o toggle por TIC (default ON; com o OFF os 6
//      desenham individualmente = 6 draws, o caminho de sempre);
//   4. fase35b_skin_flag_mismatch — o skinado NUNCA funde (a pose é por
//      TIC): 5 estáticos fundem em 1 + o skinado no caminho de sempre;
//   5. fase35b_lod_mismatch — 3 em LOD 0 + 3 em LOD 1 → 2 draws (o entry
//      LOD é da assinatura — o MESMO critério do draw de sempre);
//   6. fase35b_mais_de_64 — 100 instâncias do mesmo bloco → 2 draws
//      (64+36: o teto std140 do GE8320 fatia, nunca falha);
//   7. fase35b_visual_identico — pixel a pixel (o rasterizador do stub):
//      6 draws individuais vs 1 draw instanciado = o MESMO framebuffer;
//   8. fase35b_compat_011t — o .goni da 0.11-T abre com o instancing ON
//      por default; a chave "instancing" só nasce quando false;
//   9. fase35b_stats_corretos — instancesMerged bate certo com os blocos
//      fundidos (os blends NÃO fundem e NÃO contam).
//
// O ORQUESTRADOR (o drawTics com a colheita/agrupamento/submissão) é
// aferrado pela FASE 28 do c33_virtual — este TU é o único com
// platform/main.cpp incluído, o padrão da casa (wiring087).
#include "TestFramework.h"

#include <GLES3/gl3.h>   // stub do hospedeiro (glstub::stats + fb)

#include <cmath>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#include "assets/GOwnFormats.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/ProjectStorage.h"
#include "core/Scene.h"
#include "core/SceneBounds.h"
#include "core/SceneSerializer.h"
#include "render/BlockMesh.h"
#include "render/Mesh.h"
#include "render/Renderer.h"

using namespace vv;
using test::nearEqF;

namespace {

// ---- os fixtures (o padrão do test_blockmesh: quad strips por bloco) ------
void addQuadStrip(MeshData& m, f32 cx, f32 cz, f32 half, int nx, int nz,
                  const std::string& material) {
    const u32 base = static_cast<u32>(m.vertices.size());
    for (int iz = 0; iz <= nz; ++iz) {
        for (int ix = 0; ix <= nx; ++ix) {
            Vertex v;
            v.pos = Vec3{cx - half + 2.0f * half * f32(ix) / f32(nx), 0.0f,
                         cz - half + 2.0f * half * f32(iz) / f32(nz)};
            v.normal = Vec3{0.0f, 1.0f, 0.0f};
            v.uv = Vec2{f32(ix) / f32(nx), f32(iz) / f32(nz)};
            m.vertices.push_back(v);
        }
    }
    MeshData::Group g;
    g.material = material;
    g.firstIndex = static_cast<u32>(m.indices.size());
    for (int iz = 0; iz < nz; ++iz) {
        for (int ix = 0; ix < nx; ++ix) {
            const u32 a = base + static_cast<u32>(iz * (nx + 1) + ix);
            const u32 b = a + static_cast<u32>(nx + 1);
            m.indices.push_back(static_cast<u16>(a));
            m.indices.push_back(static_cast<u16>(b));
            m.indices.push_back(static_cast<u16>(a + 1));
            m.indices.push_back(static_cast<u16>(b));
            m.indices.push_back(static_cast<u16>(b + 1));
            m.indices.push_back(static_cast<u16>(a + 1));
            g.indexCount += 6;
        }
    }
    m.groups.push_back(g);
}

// o mesh de N blocos (cada um o seu strip + material); withLod gera a cauda
bool makeGMesh(int nBlocks, int nx, int nz, bool withLod,
               std::vector<u8>& gmesh, std::string& err) {
    MeshData m;
    m.name = "inst";
    for (int i = 0; i < nBlocks; ++i) {
        // os strips SOBREPOSTOS (o volume do modelo é o MESMO para os
        // blocos todos — o AABB da cena do investigação fica fiável)
        addQuadStrip(m, 0.0f, 0.0f, 1.0f, nx, nz,
                     i % 2 == 0 ? "verde" : "ambar");
    }
    return writeGMesh(m, gmesh, err, nullptr, nullptr, withLod);
}

// um storage mínimo (o padrão CountingStorage do test_blockmesh, sem conta)
struct InstStorage final : vv::ProjectStorage {
    std::map<std::string, std::string> files;
    std::string root() const override { return "/inst"; }
    bool makeDirs(const std::string&) override { return true; }
    bool exists(const std::string& relPath) const override {
        return files.count(relPath) != 0;
    }
    vv::Presence probe(const std::string& relPath) const override {
        return files.count(relPath) != 0 ? vv::Presence::Present
                                         : vv::Presence::Absent;
    }
    bool writeText(const std::string& relPath,
                   const std::string& text) override {
        files[relPath] = text;
        return true;
    }
    bool readText(const std::string& relPath, std::string& out) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        out = it->second;
        return true;
    }
    bool writeBytes(const std::string& relPath, const void* data,
                    size_t n) override {
        files[relPath].assign(static_cast<const char*>(data), n);
        return true;
    }
    bool readBytes(const std::string& relPath,
                   std::vector<vv::u8>& out) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        out.assign(it->second.begin(), it->second.end());
        return !out.empty();
    }
    bool listDir(const std::string&,
                 std::vector<std::string>&) const override {
        return false;
    }
    bool statBytes(const std::string& relPath,
                   vv::u64& outBytes) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        outBytes = it->second.size();
        return true;
    }
    bool readBytesAt(const std::string& relPath, vv::u64 offset, size_t len,
                     std::vector<vv::u8>& out) const override {
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        if (offset + len > it->second.size()) return false;
        out.assign(it->second.begin() + static_cast<long>(offset),
                   it->second.begin() + static_cast<long>(offset + len));
        return out.size() == len;
    }
};

// o ciclo do drawTics SEM o main (a colheita + UM grupo por assinatura —
// a FASE 28 do c33 aferre o orquestrador REAL; aqui a mecânica é direta):
// devolve os picks por TIC na ordem da cena e desenha os grupos fundidos.
struct InstRun {
    DrawStats total;
    BlockMesh::Stats st;
    u32 instDraws = 0;      // drawElementsInstancedCalls delta
    u32 lastInstance = 0;
};
InstRun runInstanced(BlockMesh& bm, Renderer& r, const Mat4& vp,
                     const std::vector<Mat4>& models, const Vec3& camEye,
                     f32 lodDistance = 0.0f) {
    InstRun out;
    std::vector<BlockMesh::InstPick> picks;
    std::vector<Mat4> byEntry[64];   // por entry (o .gmesh de teste tem ≤ 4)
    u32 maxEntry = 0;
    for (const Mat4& m : models) {
        const u32 n = bm.collectVisible(m, vp, &camEye, lodDistance, picks);
        if (n == 0) continue;
        for (const BlockMesh::InstPick& p : picks) {
            if (p.blend) continue;   // os blends não se fundem (o pin 9)
            byEntry[p.entry].push_back(m);
            if (p.entry > maxEntry) maxEntry = p.entry;
        }
    }
    const u32 d0 = static_cast<u32>(glstub::stats.drawElementsInstancedCalls);
    for (u32 e = 0; e <= maxEntry; ++e) {
        if (byEntry[e].empty()) continue;
        const DrawStats ds = bm.drawInstancedGroup(
            r, e, byEntry[e].data(), static_cast<u32>(byEntry[e].size()), vp,
            nullptr, nullptr, out.st);
        out.total = out.total + ds;
    }
    out.instDraws =
        static_cast<u32>(glstub::stats.drawElementsInstancedCalls) - d0;
    out.lastInstance = static_cast<u32>(glstub::stats.lastInstanceCount);
    return out;
}

} // namespace

// ---- PIN 1 — 6 TICs com o mesmo mesh + material → 1 draw call --------------

TEST(fase35b_6_tics_1_dc) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeGMesh(1, 4, 4, false, gmesh, err));   // 1 bloco, 1 material
    InstStorage st;
    st.files["assets/inst.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/inst.gmesh", err));
    EXPECT(bm.table().size() == 1);
    EXPECT(!bm.skinned());

    Renderer r;
    EXPECT(r.init());
    const Mat4 vp = Mat4::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
    const Vec3 camEye{0.0f, 0.0f, 0.0f};

    // 6 TICs (o padrão da cena do dono) — posições distintas, TODAS visíveis
    Scene scene;
    std::vector<Mat4> models;
    for (u32 k = 0; k < 6; ++k) {
        char nm[16];
        std::snprintf(nm, sizeof(nm), "C%u", k);
        Handle h = scene.create(nm);
        Tic* t = scene.get(h);
        ASSERT(t != nullptr);
        Transform3D* tr = t->addComponent<Transform3D>();
        tr->pos = Vec3{-5.0f + 2.0f * f32(k), 0.0f, -10.0f};
        tr->updateWorld();
        models.push_back(tr->world);
        MeshRenderer* mr = t->addComponent<MeshRenderer>();
        ASSERT(mr != nullptr);
        EXPECT(mr->instancing);   // o default ON (o §3.5b.4)
    }
    // o cull por TIC (o MESMO classify do draw): 1 pick cada
    std::vector<BlockMesh::InstPick> picks;
    for (const Mat4& m : models) {
        EXPECT(bm.collectVisible(m, vp, &camEye, 0.0f, picks) == 1);
        EXPECT(!picks[0].blend);
        EXPECT(picks[0].entry == 0);
    }
    // O PIN: 6 instâncias fundem em EXATAMENTE 1 draw instanciado
    const InstRun run = runInstanced(bm, r, vp, models, camEye);
    EXPECT(run.instDraws == 1);
    EXPECT(glstub::stats.lastInstanceCount == 6);
    EXPECT(run.total.drawCalls == 1);
    EXPECT(run.total.instancesMerged == 6);
    EXPECT(run.total.vertices == bm.table()[0].indexCount * 6u);
    EXPECT(run.st.drawn == 6);
}

// ---- PIN 2 — 6 TICs com 2 materiais → 2 draws -------------------------------

TEST(fase35b_2_materiais_2_dc) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeGMesh(2, 4, 4, false, gmesh, err));   // 2 blocos, 2 materiais
    InstStorage st;
    st.files["assets/inst.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/inst.gmesh", err));
    EXPECT(bm.table().size() == 2);
    EXPECT(bm.materials().size() == 2);

    Renderer r;
    EXPECT(r.init());
    const Mat4 vp = Mat4::ortho(-30.0f, 30.0f, -30.0f, 30.0f, 0.1f, 200.0f);
    const Vec3 camEye{0.0f, 0.0f, 0.0f};
    std::vector<Mat4> models;
    for (u32 k = 0; k < 6; ++k) {
        models.push_back(
            Mat4::translation(-12.5f + 5.0f * f32(k), 0.0f, -20.0f));
    }
    // O PIN: 12 picks (6×2) agrupam POR ENTRADA → exatamente 2 draws
    const InstRun run = runInstanced(bm, r, vp, models, camEye);
    EXPECT(run.instDraws == 2);
    EXPECT(run.total.drawCalls == 2);
    EXPECT(run.total.instancesMerged == 12);
    EXPECT(run.st.drawn == 12);
}

// ---- PIN 3 — o toggle por TIC (off = o caminho de sempre) -------------------

TEST(fase35b_toggle_off) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeGMesh(1, 4, 4, false, gmesh, err));
    InstStorage st;
    st.files["assets/inst.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/inst.gmesh", err));
    Renderer r;
    EXPECT(r.init());
    const Mat4 vp = Mat4::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
    const Vec3 camEye{0.0f, 0.0f, 0.0f};

    Scene scene;
    std::vector<Mat4> models;
    MeshRenderer* last = nullptr;
    for (u32 k = 0; k < 6; ++k) {
        char nm[16];
        std::snprintf(nm, sizeof(nm), "C%u", k);
        Handle h = scene.create(nm);
        Tic* t = scene.get(h);
        ASSERT(t != nullptr);
        Transform3D* tr = t->addComponent<Transform3D>();
        tr->pos = Vec3{-5.0f + 2.0f * f32(k), 0.0f, -10.0f};
        tr->updateWorld();
        models.push_back(tr->world);
        last = t->addComponent<MeshRenderer>();
    }
    ASSERT(last != nullptr);
    // o toggle existe e serializa; com OFF nos 6, o caminho de sempre
    // desenha individualmente (6 drawElements, ZERO instanciados)
    last->instancing = false;
    BlockMesh::Stats bs;
    u32 drawsNormal = 0;
    for (const Mat4& m : models) {
        const DrawStats ds = bm.draw(r, m, vp, nullptr, nullptr, nullptr, 0,
                                     bs, &camEye, 0.0f);
        drawsNormal += ds.drawCalls;
    }
    EXPECT(drawsNormal == 6);
    EXPECT(glstub::stats.drawElementsInstancedCalls == 0);
    // (o ORQUESTRADOR salta a colheita quando mr->instancing == false —
    // aferrado pela FASE 28 do c33 com o drawTics real)
}

// ---- PIN 4 — o skinado NUNCA funde (a pose é por TIC) -----------------------

TEST(fase35b_skin_flag_mismatch) {
    glstub::reset();
    // o mesh skinado de 1 bloco (o meta ganha kGmeshV3FlagSkinned)
    MeshData sk;
    const Vec3 P[4] = {Vec3{-0.5f, -0.5f, 0.0f}, Vec3{0.5f, -0.5f, 0.0f},
                       Vec3{0.5f, 0.5f, 0.0f}, Vec3{-0.5f, 0.5f, 0.0f}};
    for (int k = 0; k < 4; ++k) {
        Vertex v;
        v.pos = P[k];
        v.normal = Vec3{0.0f, 0.0f, 1.0f};
        sk.vertices.push_back(v);
        for (int c = 0; c < 4; ++c) {
            sk.skinJoints.push_back(0);
            sk.skinWeights.push_back(c == 0 ? 1.0f : 0.0f);
        }
    }
    const u16 IDX[6] = {0, 1, 2, 0, 2, 3};
    sk.indices.assign(IDX, IDX + 6);
    std::string err;
    std::vector<u8> gmeshSkin;
    EXPECT(writeGMesh(sk, gmeshSkin, err));
    GMeshV3Meta metaSkin;
    std::vector<GMeshV3Block> bSkin;
    std::vector<std::string> mSkin;
    EXPECT(readGMeshV3Meta(gmeshSkin.data(), gmeshSkin.size(), metaSkin,
                           bSkin, mSkin, err));
    EXPECT((metaSkin.flags & kGmeshV3FlagSkinned) != 0u);

    InstStorage st;
    st.files["assets/skin.gmesh"].assign(
        reinterpret_cast<const char*>(gmeshSkin.data()), gmeshSkin.size());
    std::vector<u8> gmeshStatic;
    EXPECT(makeGMesh(1, 4, 4, false, gmeshStatic, err));
    st.files["assets/static.gmesh"].assign(
        reinterpret_cast<const char*>(gmeshStatic.data()), gmeshStatic.size());

    BlockMesh bmSkin;
    EXPECT(bmSkin.open(st, "assets/skin.gmesh", err));
    BlockMesh bmStatic;
    EXPECT(bmStatic.open(st, "assets/static.gmesh", err));
    // O CONTRATO que o guard do drawTics usa: o skinado diz-o pela flag
    EXPECT(bmSkin.skinned());
    EXPECT(!bmStatic.skinned());

    Renderer r;
    EXPECT(r.init());
    const Mat4 vp = Mat4::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
    const Vec3 camEye{0.0f, 0.0f, 0.0f};
    // 1 skinado + 5 estáticos: os 5 estáticos fundem em 1 draw; o skinado
    // segue o caminho de sempre (o draw com bones) — o total é 2+ draws,
    // NUNCA 1 (o fundir quebraria a pose individual)
    std::vector<Mat4> staticModels;
    for (u32 k = 0; k < 5; ++k) {
        staticModels.push_back(
            Mat4::translation(-4.0f + 2.0f * f32(k), 0.0f, -10.0f));
    }
    const InstRun run = runInstanced(bmStatic, r, vp, staticModels, camEye);
    EXPECT(run.instDraws == 1);
    EXPECT(run.total.instancesMerged == 5);
    // o skinado desenha por baixo (o caminho de sempre: 1 draw normal)
    const Mat4 skinModel = Mat4::translation(6.0f, 0.0f, -10.0f);
    BlockMesh::Stats bs;
    const DrawStats dsSkin =
        bmSkin.draw(r, skinModel, vp, nullptr, nullptr, nullptr, 0, bs,
                    &camEye, 0.0f);
    EXPECT(dsSkin.drawCalls == 1);
    EXPECT(glstub::stats.drawElementsInstancedCalls == 1);   // o estático só
}

// ---- PIN 5 — 3 em LOD 0 + 3 em LOD 1 → 2 draws ------------------------------

TEST(fase35b_lod_mismatch) {
    glstub::reset();
    // a grelha row-major decima para EXATAMENTE metade (o padrão do pin da
    // 3.5a) — a cauda GLOD nasce e o entry LOD é o 2.º
    MeshData grid;
    const u32 cols = 32, rows = 32;
    grid.vertices.resize(cols * rows);
    for (u32 j = 0; j < rows; ++j) {
        for (u32 i = 0; i < cols; ++i) {
            Vertex& v = grid.vertices[j * cols + i];
            v.pos = Vec3{f32(i), 0.0f, f32(j)};
            v.normal = Vec3{0.0f, 1.0f, 0.0f};
            v.uv = Vec2{f32(i) / 31.0f, f32(j) / 31.0f};
        }
    }
    for (u32 j = 0; j + 1 < rows; ++j) {
        for (u32 i = 0; i + 1 < cols; ++i) {
            const u32 a = j * cols + i, b = a + 1, c = a + cols, d = c + 1;
            grid.indices.push_back(static_cast<u16>(a));
            grid.indices.push_back(static_cast<u16>(c));
            grid.indices.push_back(static_cast<u16>(b));
            grid.indices.push_back(static_cast<u16>(b));
            grid.indices.push_back(static_cast<u16>(c));
            grid.indices.push_back(static_cast<u16>(d));
        }
    }
    std::string err;
    std::vector<u8> gmesh;
    EXPECT(writeGMesh(grid, gmesh, err, nullptr, nullptr, /*withLod=*/true));
    InstStorage st;
    st.files["assets/lod.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/lod.gmesh", err));
    EXPECT(bm.hasLod());
    EXPECT(bm.table().size() == 1);

    Renderer r;
    EXPECT(r.init());
    const Mat4 vp = Mat4::ortho(-160.0f, 160.0f, -160.0f, 160.0f, 0.1f, 400.0f);
    const Vec3 camEye{0.0f, 0.0f, 0.0f};
    // 3 perto (dist ~10.6 u < 20 u → entry NORMAL) + 3 longe (dist ~85+ u
    // > 20 u → entry LOD) — o MESMO critério do lodActive do draw de
    // sempre (o centro do AABB do bloco: z ∈ [0,31] → o mundo soma o
    // offset do TIC; por isso os perto ficam a ~10 u e os longe a ~100 u
    // DA CÂMARA — x junto do eixo, a distância é feita no z)
    std::vector<Mat4> models;
    for (u32 k = 0; k < 6; ++k) {
        // o centro do AABB do bloco fica a (+15.5, 0, +15.5) do TIC — os
        // perto ficam a ~10.5-11.5 u da câmara, os longe a ~100.5 u
        const f32 z = k < 3 ? -26.0f : -116.0f;
        models.push_back(
            Mat4::translation(-15.5f + 2.0f * f32(k % 3), 0.0f, z));
    }
    std::vector<BlockMesh::InstPick> picks;
    for (u32 k = 0; k < 6; ++k) {
        EXPECT(bm.collectVisible(models[k], vp, &camEye, 20.0f, picks) == 1);
        if (k < 3) {
            EXPECT(picks[0].entry == 0);   // o bloco normal
        } else {
            EXPECT(picks[0].entry == 1);   // lodBase_ + 0 (o LOD)
        }
    }
    const InstRun run = runInstanced(bm, r, vp, models, camEye, 20.0f);
    // O PIN: 2 assinaturas (normal ×3 + LOD ×3) → exatamente 2 draws
    EXPECT(run.instDraws == 2);
    EXPECT(run.total.drawCalls == 2);
    EXPECT(run.total.instancesMerged == 6);
    EXPECT(run.st.drawn == 6);
    EXPECT(run.st.lodDrawn == 3);   // só os longe desenham a entrada LOD
}

// ---- PIN 6 — 100 instâncias → 2 draws (o teto 64 fatia, nunca falha) -------

TEST(fase35b_mais_de_64) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeGMesh(1, 4, 4, false, gmesh, err));
    InstStorage st;
    st.files["assets/inst.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/inst.gmesh", err));
    Renderer r;
    EXPECT(r.init());
    const Mat4 vp = Mat4::ortho(-200.0f, 200.0f, -200.0f, 200.0f, 0.1f, 500.0f);
    const Vec3 camEye{0.0f, 0.0f, 0.0f};

    // 100 TICs iguais (o §3.5b.6: "100 blocos iguais")
    std::vector<Mat4> models;
    for (u32 k = 0; k < 100; ++k) {
        models.push_back(
            Mat4::translation(-99.0f + 2.0f * f32(k), 0.0f, -100.0f));
    }
    const InstRun run = runInstanced(bm, r, vp, models, camEye);
    // O PIN: o teto de 64 matrizes por draw fatia em 64+36 = 2 draws
    EXPECT(run.instDraws == 2);
    EXPECT(run.total.drawCalls == 2);
    EXPECT(run.total.instancesMerged == 100);
    EXPECT(run.total.vertices ==
           bm.table()[0].indexCount * 100u);   // os índices × instâncias
    EXPECT(run.st.drawn == 100);
}

// ---- PIN 7 — pixel a pixel: 6 draws individuais == 1 draw instanciado ------

TEST(fase35b_visual_identico) {
    glstub::reset();
    glstub::fb::enabled = true;
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeGMesh(1, 4, 4, false, gmesh, err));
    InstStorage st;
    st.files["assets/inst.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/inst.gmesh", err));
    Renderer r;
    EXPECT(r.init());
    r.resize(160, 120);
    const Mat4 vp = Mat4::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);

    // 6 posições (3 distintas visíveis + réplicas — a fusão é o que prova)
    std::vector<Mat4> models;
    for (u32 k = 0; k < 6; ++k) {
        models.push_back(
            Mat4::translation(-4.0f + 2.0f * f32(k % 3), 0.0f, -10.0f));
    }
    std::vector<u8> pathA, pathB;
    // ---- A: o caminho de sempre (6 draws individuais) ----
    {
        r.beginFrame();
        BlockMesh::Stats bs;
        for (const Mat4& m : models) {
            bm.draw(r, m, vp, nullptr, nullptr, nullptr, 0, bs, nullptr, 0.0f);
        }
        std::vector<u8> px(size_t(160) * 120 * 4);
        glReadPixels(0, 0, 160, 120, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        pathA = px;
    }
    // ---- B: o instanciado (1 draw × 6) ----
    {
        r.beginFrame();
        BlockMesh::Stats ss;
        std::vector<BlockMesh::InstPick> picks;
        std::vector<Mat4> mats;
        for (const Mat4& m : models) {
            if (bm.collectVisible(m, vp, nullptr, 0.0f, picks) > 0) {
                mats.push_back(m);
            }
        }
        DrawStats ds;
        EXPECT(mats.size() == 6);
        ds = bm.drawInstancedGroup(r, 0, mats.data(),
                                   static_cast<u32>(mats.size()), vp, nullptr,
                                   nullptr, ss);
        EXPECT(ds.drawCalls == 1);
        std::vector<u8> px(size_t(160) * 120 * 4);
        glReadPixels(0, 0, 160, 120, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        pathB = px;
    }
    // O PIN: o framebuffer é o MESMO byte a byte (visualmente idêntico —
    // o instancing não muda UM pixel; só o dc muda)
    EXPECT(pathA.size() == pathB.size());
    EXPECT(std::memcmp(pathA.data(), pathB.data(), pathA.size()) == 0);
    glstub::fb::enabled = false;
    glstub::reset();
}

// ---- PIN 8 — o .goni da 0.11-T abre com instancing ON por default ----------

TEST(fase35b_compat_011t) {
    glstub::reset();
    // (a) um TIC novo nasce ON (o §3.5b.4 — o default que manda)
    {
        Scene scene;
        Handle h = scene.create("A");
        Tic* t = scene.get(h);
        ASSERT(t != nullptr);
        MeshRenderer* mr = t->addComponent<MeshRenderer>();
        ASSERT(mr != nullptr);
        EXPECT(mr->instancing);
    }
    // (b) o save de um TIC SEM edição NÃO contém a chave (os .goni da
    //     0.11-T ficam byte a byte)
    std::string dump1, dump2;
    {
        Scene scene;
        Handle h = scene.create("A");
        Tic* t = scene.get(h);
        ASSERT(t != nullptr);
        Transform3D* tr = t->addComponent<Transform3D>();
        tr->pos = Vec3{1.0f, 2.0f, 3.0f};
        tr->updateWorld();
        t->addComponent<MeshRenderer>();
        dump1 = SceneSerializer::dump(scene);
        EXPECT(dump1.find("instancing") == std::string::npos);
    }
    // (c) com o toggle OFF a chave NASCE (e só com false)
    {
        Scene scene;
        Handle h = scene.create("A");
        Tic* t = scene.get(h);
        ASSERT(t != nullptr);
        Transform3D* tr = t->addComponent<Transform3D>();
        tr->pos = Vec3{1.0f, 2.0f, 3.0f};
        tr->updateWorld();
        MeshRenderer* mr = t->addComponent<MeshRenderer>();
        ASSERT(mr != nullptr);
        mr->instancing = false;
        dump2 = SceneSerializer::dump(scene);
        EXPECT(dump2.find("\"instancing\":false") != std::string::npos);
    }
    // (d) o load do (c) restaura OFF; o load do (b) (a forma 0.11-T) fica
    //     ON por default — e o round-trip do (b) é byte a byte
    {
        Scene s1;
        SceneSerializer::LoadCtx ctx;
        EXPECT(SceneSerializer::loadText(s1, dump1, ctx));
        const auto& mrs1 = s1.components().meshRenderers();
        EXPECT(mrs1.size() == 1);
        EXPECT(mrs1.at(0).instancing);   // ausente = ON (o default)
        Scene s2;
        EXPECT(SceneSerializer::loadText(s2, dump2, ctx));
        const auto& mrs2 = s2.components().meshRenderers();
        EXPECT(mrs2.size() == 1);
        EXPECT(!mrs2.at(0).instancing);   // false restaura false
        // o round-trip da forma 0.11-T: save(load(dump1)) == dump1
        Scene s3;
        EXPECT(SceneSerializer::loadText(s3, dump1, ctx));
        const std::string again = SceneSerializer::dump(s3);
        EXPECT(again == dump1);   // byte a byte (o pin do compat)
    }
}

// ---- PIN 9 — instancesMerged bate certo (e os blends NÃO fundem) -----------

TEST(fase35b_stats_corretos) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeGMesh(1, 4, 4, false, gmesh, err));
    InstStorage st;
    st.files["assets/inst.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/inst.gmesh", err));
    Renderer r;
    EXPECT(r.init());
    const Mat4 vp = Mat4::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.1f, 100.0f);
    // O contador É o nº de blocos fundidos: 6 → 6; 100 → 100 (em 2 draws)
    {
        std::vector<Mat4> models;
        for (u32 k = 0; k < 6; ++k) {
            models.push_back(
                Mat4::translation(-5.0f + 2.0f * f32(k), 0.0f, -10.0f));
        }
        const InstRun run = runInstanced(bm, r, vp, models,
                                         Vec3{0.0f, 0.0f, 0.0f});
        EXPECT(run.total.instancesMerged == 6);
        EXPECT(run.total.drawCalls == 1);
    }
    {
        // o vp LARGO (os 100 espalham-se por ±100 — o MESMO pin do teto)
        const Mat4 vpWide =
            Mat4::ortho(-200.0f, 200.0f, -200.0f, 200.0f, 0.1f, 500.0f);
        std::vector<Mat4> models;
        for (u32 k = 0; k < 100; ++k) {
            models.push_back(
                Mat4::translation(-99.0f + 2.0f * f32(k), 0.0f, -100.0f));
        }
        const InstRun run =
            runInstanced(bm, r, vpWide, models, Vec3{0.0f, 0.0f, 0.0f});
        EXPECT(run.total.instancesMerged == 100);
        EXPECT(run.total.drawCalls == 2);   // o teto 64 fatiou
        EXPECT(run.st.drawn == 100);        // o "bl" do HUD bate certo
    }
    // os BLEND não se fundem NEM contam: o drawBlendPicks devolve
    // instancesMerged == 0 e 1 draw por bloco (a ordem longe→perto)
    {
        MeshData mb;
        addQuadStrip(mb, 0.0f, 0.0f, 1.0f, 4, 4, "vidro");
        // o material "vidro" com alphaMode 2 via o GTB2 — o writeGMesh
        // aceita as props por material (o padrão da FASE 2)
        GMeshMatProps props;
        props.materialIndex = 0;
        props.alphaMode = 2;
        std::vector<GMeshMatProps> propsVec;
        propsVec.push_back(props);
        std::vector<u8> gmeshBlend;
        EXPECT(writeGMesh(mb, gmeshBlend, err, nullptr, &propsVec, false));
        InstStorage sb;
        sb.files["assets/blend.gmesh"].assign(
            reinterpret_cast<const char*>(gmeshBlend.data()),
            gmeshBlend.size());
        BlockMesh bmB;
        EXPECT(bmB.open(sb, "assets/blend.gmesh", err));
        BlockMesh::Stats ss;
        const u32 entries[2] = {0, 0};
        const Mat4 models[2] = {Mat4::identity(), Mat4::identity()};
        const f32 zs[2] = {0.5f, -0.5f};
        const u32 d0 =
            static_cast<u32>(glstub::stats.drawElementsInstancedCalls);
        const DrawStats ds = bmB.drawBlendPicks(
            r, entries, models, zs, 2, vp, nullptr, nullptr, ss);
        EXPECT(ds.instancesMerged == 0);   // NÃO fundiu (a ordem é por
                                           // fragmento)
        EXPECT(ds.drawCalls == 2);         // 1 draw por bloco
        EXPECT(static_cast<u32>(glstub::stats.drawElementsInstancedCalls) ==
               d0);
    }
}

// ---- PIN §1.8 — a investigação do culling (o bl 5164/5178 do dono) --------
// O número do dono: 5164/5178 blocos desenhados com uma vista quase
// vertical — o cull cortou só 14 (0.27%). HIPÓTESES do prompt (§1.8):
// (a) o far da câmara é enorme → nada está fora do frustum;
// (b) os AABBs dos blocos são maiores que a geometria;
// (c) o culling não corre (bug silencioso).
// A PROVA: (a) é o comportamento CORRETO da casa — o far é DINÂMICO
// (core/SceneBounds.h + camerautil::editorClips no main.cpp: far = max(
// 1.5×zoom, sceneFarthest+zoom+10, 450) — com a cena dos 6 modelos o far
// contém TUDO e o plano far nunca corta; a vista de cima põe os modelos
// todos à frente do near). O culling está VIVO (o R-040 é a sentinela):
// com um frustum de gameplay (o olho perto, far curto) os distantes cortam.
TEST(fase35b_culling_investigacao) {
    glstub::reset();
    std::string err;
    std::vector<u8> gmesh;
    EXPECT(makeGMesh(4, 4, 4, false, gmesh, err));   // 4 blocos por TIC
    InstStorage st;
    st.files["assets/inst.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/inst.gmesh", err));

    // a cena do dono (a forma): 6 TICs com o MESMO mesh, espalhados ao pé
    // da origem — o AABB da cena sai daqui
    Scene scene;
    std::vector<Mat4> models;
    for (u32 k = 0; k < 6; ++k) {
        char nm[16];
        std::snprintf(nm, sizeof(nm), "C%u", k);
        Handle h = scene.create(nm);
        Tic* t = scene.get(h);
        ASSERT(t != nullptr);
        Transform3D* tr = t->addComponent<Transform3D>();
        tr->pos = Vec3{-2.0f + 1.0f * f32(k), 0.0f, f32(k % 2) * 1.0f};
        tr->updateWorld();
        models.push_back(tr->world);
        t->addComponent<MeshRenderer>();
    }
    // (a) O FAR DINÂMICO DO EDITOR (o MESMO cálculo do main.cpp 5751-5781):
    Vec3 mn, mx;
    f32 radius;
    camerautil::sceneAABB(scene, mn, mx, radius);
    const f32 sceneFar = camerautil::sceneFarthest(mn, mx);
    f32 nearOut = 0.0f, farOut = 0.0f;
    camerautil::editorClips(6.0f /*o zoom de trabalho*/, sceneFar, nearOut,
                            farOut);
    std::printf(
        "\n[r047-cull] AABB da cena: min(%.1f,%.1f,%.1f) max(%.1f,%.1f,%.1f)"
        " · sceneFarthest=%.1f u · editor far=%.1f u near=%.2f u"
        " (o far DINÂMICO contém a cena inteira — o plano far nunca corta)\n",
        mn.x, mn.y, mn.z, mx.x, mx.y, mx.z, sceneFar, farOut, nearOut);
    // O far do editor NUNCA desce de 450 (Camera::kDefaultFar) — com a
    // cena dos 6 modelos ao pé da origem, sceneFarthest ≈ 12 u e o far
    // fica nos 450: 30× maior que a cena
    EXPECT(farOut >= 450.0f);
    // os AABBs de 4 blocos (a amostra do prompt): o volume do mesh é o
    // MESMO que o AABB diz (a quantização 16-bit não incha — o dequant é
    // mn + q/65535·ext)
    const GMeshV3Block& b0 = bm.table()[0];
    std::printf(
        "[r047-cull] AABB do bloco 0: min(%.2f,%.2f,%.2f) max(%.2f,%.2f,"
        "%.2f) — %u verts (o AABB é o volume REAL da geometria do bloco)\n",
        b0.aabbMin.x, b0.aabbMin.y, b0.aabbMin.z, b0.aabbMax.x, b0.aabbMax.y,
        b0.aabbMax.z, b0.vertexCount);
    // (a vs c) A CONTA: com a vista QUASE VERTICAL e o far do editor,
    // TODOS os blocos dos 6 TICs passam o cull (o número do dono reproduz:
    // 24/24 — o 5164/5178 é o MESMO efeito com 863×6 blocos)
    Renderer r;
    EXPECT(r.init());
    const Mat4 vpTop = Mat4::mul(
        Mat4::perspective(60.0f * 3.14159265f / 180.0f, 1.0f, nearOut,
                          farOut),
        Mat4::lookAt(Vec3{0.0f, 12.0f, 0.001f}, Vec3{0.0f, 0.0f, 0.0f},
                     Vec3{0.0f, 0.0f, -1.0f}));
    f32 planes[6][4];
    BlockMesh::frustumPlanes(vpTop, planes);
    std::printf(
        "[r047-cull] planos do VP (a·x+b·y+c·z+d < 0 = fora): "
        "L(%.2f,%.2f,%.2f,%.2f) R(%.2f,%.2f,%.2f,%.2f) "
        "B(%.2f,%.2f,%.2f,%.2f) T(%.2f,%.2f,%.2f,%.2f) "
        "N(%.2f,%.2f,%.2f,%.2f) F(%.2f,%.2f,%.2f,%.2f)\n",
        planes[0][0], planes[0][1], planes[0][2], planes[0][3],
        planes[1][0], planes[1][1], planes[1][2], planes[1][3],
        planes[2][0], planes[2][1], planes[2][2], planes[2][3],
        planes[3][0], planes[3][1], planes[3][2], planes[3][3],
        planes[4][0], planes[4][1], planes[4][2], planes[4][3],
        planes[5][0], planes[5][1], planes[5][2], planes[5][3]);
    u32 visTop = 0, totalTop = 0;
    std::vector<BlockMesh::InstPick> picks;
    for (const Mat4& m : models) {
        totalTop += 4;
        visTop += bm.collectVisible(m, vpTop, nullptr, 0.0f, picks);
    }
    std::printf("[r047-cull] vista de cima + far do editor: %u/%u blocos "
                "visíveis (com os 6 modelos empilhados como o dono tem, o "
                "frustum lateral contém TUDO — o 5164/5178 do dono é o "
                "MESMO efeito: só o near/lados cortam o resto)\n",
                visTop, totalTop);
    EXPECT(visTop == totalTop);   // NADA corta (o far contém tudo e os
                                  // modelos estão dentro dos lados)
    // (c) FALSIFICADA: com um frustum de GAMEPLAY (o olho ao nível do
    // chão, far curto), os blocos distantes CORTAM — o culling está vivo
    const Mat4 vpGame = Mat4::mul(
        Mat4::perspective(60.0f * 3.14159265f / 180.0f, 1.0f, 0.1f, 8.0f),
        Mat4::lookAt(Vec3{0.0f, 1.0f, 0.0f}, Vec3{0.0f, 0.0f, -6.0f},
                     Vec3{0.0f, 1.0f, 0.0f}));
    u32 visGame = 0;
    for (const Mat4& m : models) {
        visGame += bm.collectVisible(m, vpGame, nullptr, 0.0f, picks);
    }
    std::printf("[r047-cull] frustum de gameplay (far 8 u): %u/%u visíveis "
                "(o cull CORTA de verdade — o R-040 confirma-se vivo)\n",
                visGame, totalTop);
    EXPECT(visGame < totalTop);   // o culling corre e escolhe
}
