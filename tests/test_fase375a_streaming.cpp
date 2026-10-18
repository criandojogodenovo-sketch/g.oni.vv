// tests/test_fase375a_streaming.cpp — 0.11-W (3.75a): STREAMING
// PROGRESSIVO — o freeze do import morre por orçamento.
//
// O problema: o lazy de sempre carrega TODOS os blocos visíveis não
// residentes NO MESMO frame (863 leituras+decodes+uploads na cena do
// dono = segundos de freeze no 1.º frame). A cura (a spec do arquiteto,
// §3.75a): ORÇAMENTO por frame (8 blocos no caminho inteiro) + prioridade
// POR DISTÂNCIA (as CAMADAS — perto primeiro) + a PORTA na submissão (o
// não residente NÃO desenha nem carrega inline — conta «a transmitir»).
//
//   1. fase375a_sem_freeze_1a_frame — 12 blocos × 6 clones: o 1.º frame
//      carrega ≤ 8, desenha 8 grupos (48 instâncias) e conta 24
//      «a transmitir» — o frame É leve por construção;
//   2. fase375a_camadas_perto_primeiro — com a vaga a 3, os residentes
//      são EXATAMENTE os 3 mais perto do camEye (a ordem das camadas);
//   3. fase375a_budget_por_frame — o fill completa em vagas: 8 → 4 → 0
//      (Σ loads == 12; o frame 3 desenha tudo sem carregar nada);
//   4. fase375a_toggle_off — o streaming OFF devolve o caminho de sempre
//      (lazy inline: os 12 carregam no 1.º frame, ZERO draws
//      instanciados — o fallback é o MESMO do instancing OFF);
//   5. fase375a_compat_011t — o .goni da 0.11-T abre com o streaming ON
//      por default; a chave "streaming" só nasce quando false;
//   6. fase375a_blend_pending — os blends pendentes pedem vaga TAMBÉM
//      (senão nunca chegavam): o filtro é estável e a contagem bate;
//   7. fase375a_falha_nao_consome — o bloco corrupto (CRC) NÃO gasta a
//      vaga: os vizinhos vivos enchem o frame na mesma;
//   8. fase375a_stats_corretos — a contabilidade fecha: Σ loads == 12,
//      drawn + pending == visible, o dc final é o merge completo.
//
// O ORQUESTRADOR REAL (o drawTics) é aferrado pela FASE 29 do
// c33_virtual — este TU prova a POLÍTICA (o streamIn + a porta) com o
// MESMO walk do main (o bloco dist, o agrupamento, o filtro estável).
#include "TestFramework.h"

#include <GLES3/gl3.h>   // stub do hospedeiro (glstub::stats)

#include <algorithm>
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
#include "core/SceneSerializer.h"
#include "render/BlockMesh.h"
#include "render/Mesh.h"
#include "render/Renderer.h"

using namespace vv;

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

// o mesh de N blocos EM LINHA: o bloco i centrado em (i×spacing, 0, 0) —
// a distância ao camEye (na origem) cresce com i (as camadas são
// determinísticas: o entry i está a 10×i unidades)
bool makeGMeshSpaced(int nBlocks, f32 spacing, bool blend,
                     std::vector<u8>& gmesh, std::string& err) {
    MeshData m;
    m.name = "stream";
    std::vector<GMeshMatProps> props;
    if (blend) {
        GMeshMatProps p;
        p.materialIndex = 0;
        p.alphaMode = 2;   // o passe BLEND (o padrão do pin 9 da 3.5b)
        props.push_back(p);
    }
    for (int i = 0; i < nBlocks; ++i) {
        addQuadStrip(m, f32(i) * spacing, 0.0f, 1.0f, 4, 4,
                     blend ? "vidro" : (i % 2 == 0 ? "verde" : "ambar"));
    }
    return writeGMesh(m, gmesh, err, nullptr,
                      blend ? &props : nullptr, false);
}

// um storage mínimo (o padrão CountingStorage do test_blockmesh, sem conta)
struct StreamStorage final : vv::ProjectStorage {
    std::map<std::string, std::string> files;
    std::string root() const override { return "/stream"; }
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

// a distância do camEye ao centro do AABB do bloco NORMAL transformado
// pelo model — O MESMO cálculo do blockDist do drawTics (zero drift; o
// pin da FASE 29 aferra o orquestrador real com esta fórmula)
f32 blockDist(const BlockMesh& bm, u32 entry, const Mat4& m,
              const Vec3& eye) {
    const u32 blockIdx =
        entry < static_cast<u32>(bm.table().size())
            ? entry
            : entry - static_cast<u32>(bm.table().size());
    const GMeshV3Block& blk = bm.table()[blockIdx];
    const f32 cx = (blk.aabbMin.x + blk.aabbMax.x) * 0.5f;
    const f32 cy = (blk.aabbMin.y + blk.aabbMax.y) * 0.5f;
    const f32 cz = (blk.aabbMin.z + blk.aabbMax.z) * 0.5f;
    const f32 px = m.m[0] * cx + m.m[4] * cy + m.m[8] * cz + m.m[12];
    const f32 py = m.m[1] * cx + m.m[5] * cy + m.m[9] * cz + m.m[13];
    const f32 pz = m.m[2] * cx + m.m[6] * cy + m.m[10] * cz + m.m[14];
    const f32 dx = px - eye.x;
    const f32 dy = py - eye.y;
    const f32 dz = pz - eye.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

// o walk do drawTics SEM o main (colheita → agrupamento → stream →
// submissão com a PORTA) — a FASE 29 do c33 aferra o orquestrador REAL;
// aqui a mecânica é direta (o padrão do runInstanced da 3.5b).
struct StreamRun {
    DrawStats total;
    u32 loads = 0;        // os loads do streamIn neste run
    u32 drawn = 0;        // as instâncias desenhadas (o ss.drawn dos grupos)
    u32 pending = 0;      // os «a transmitir» (grupos/blend entries)
    u32 instDraws = 0;    // delta do drawElementsInstancedCalls do stub
};
StreamRun runStreamed(BlockMesh& bm, Renderer& r, const Mat4& vp,
                      const Vec3& camEye, const std::vector<Mat4>& models,
                      bool streamingOn = true,
                      u32 budget = BlockMesh::kBlockStreamBudgetPerFrame,
                      f32 lodDistance = 0.0f) {
    StreamRun out;
    const u32 d0 =
        static_cast<u32>(glstub::stats.drawElementsInstancedCalls);
    std::vector<BlockMesh::InstPick> picks;
    struct BatchPick {
        u32 entry;
        const Mat4* model;
    };
    std::vector<BatchPick> batch;
    std::vector<std::vector<u32>> blendIdxs;
    std::vector<std::vector<f32>> blendZs;
    struct BlendRow {
        const u32* entries;
        const f32* zNdc;
        u32 count;
        const Mat4* model;
    };
    std::vector<BlendRow> blends;
    for (const Mat4& m : models) {
        if (!streamingOn) {
            // o caminho individual (o de sempre — o lazy inline)
            BlockMesh::Stats ss;
            const DrawStats ds = bm.draw(r, m, vp, nullptr, nullptr, nullptr,
                                         0, ss, &camEye, lodDistance);
            out.loads += ss.loadedThisFrame;
            out.drawn += ss.drawn;
            out.total = out.total + ds;
            continue;
        }
        const u32 n = bm.collectVisible(m, vp, &camEye, lodDistance, picks);
        if (n == 0) continue;
        u32 nBlend = 0;
        for (const BlockMesh::InstPick& p : picks) {
            if (p.blend) ++nBlend;
        }
        if (nBlend > 0) {
            blendIdxs.emplace_back();
            blendZs.emplace_back();
            for (const BlockMesh::InstPick& p : picks) {
                if (p.blend) {
                    blendIdxs.back().push_back(p.entry);
                    blendZs.back().push_back(p.zNdc);
                } else {
                    batch.push_back(BatchPick{p.entry, &m});
                }
            }
            blends.push_back(BlendRow{blendIdxs.back().data(),
                                      blendZs.back().data(), nBlend, &m});
        } else {
            for (const BlockMesh::InstPick& p : picks) {
                batch.push_back(BatchPick{p.entry, &m});
            }
        }
    }
    // o AGRUPAMENTO (a assinatura bm+entry — os pins não têm tex/tint) +
    // os pedidos de vaga dos grupos NÃO residentes (dist = MÍNIMO sobre
    // as instâncias — o MESMO walk do drawTics)
    std::stable_sort(batch.begin(), batch.end(),
                     [](const BatchPick& a, const BatchPick& b) {
                         return a.entry < b.entry;
                     });
    std::vector<BlockMesh::StreamReq> reqs;
    size_t k = 0;
    while (k < batch.size()) {
        size_t j = k + 1;
        while (j < batch.size() && batch[j].entry == batch[k].entry) ++j;
        if (!bm.resident(batch[k].entry)) {
            f32 dmin = blockDist(bm, batch[k].entry, *batch[k].model, camEye);
            for (size_t t = k + 1; t < j; ++t) {
                const f32 d =
                    blockDist(bm, batch[k].entry, *batch[t].model, camEye);
                if (d < dmin) dmin = d;
            }
            reqs.push_back(BlockMesh::StreamReq{&bm, batch[k].entry, dmin});
        }
        k = j;
    }
    for (const BlendRow& b : blends) {
        for (u32 t = 0; t < b.count; ++t) {
            if (!bm.resident(b.entries[t])) {
                reqs.push_back(BlockMesh::StreamReq{
                    &bm, b.entries[t],
                    blockDist(bm, b.entries[t], *b.model, camEye)});
            }
        }
    }
    out.loads += BlockMesh::streamIn(reqs.data(),
                                     static_cast<u32>(reqs.size()), budget);
    // a SUBMISSÃO com a PORTA (só o residente desenha)
    std::vector<Mat4> mats;
    k = 0;
    while (k < batch.size()) {
        size_t j = k + 1;
        while (j < batch.size() && batch[j].entry == batch[k].entry) ++j;
        const u32 n = static_cast<u32>(j - k);
        if (!bm.resident(batch[k].entry)) {
            out.pending += n;   // «a transmitir» — a vaga chega
            k = j;
            continue;
        }
        mats.clear();
        mats.reserve(n);
        for (size_t t = k; t < j; ++t) {
            mats.push_back(*batch[t].model);
        }
        BlockMesh::Stats ss;
        const DrawStats ds = bm.drawInstancedGroup(
            r, batch[k].entry, mats.data(), n, vp, nullptr, nullptr, ss);
        out.drawn += ss.drawn;
        out.total = out.total + ds;
        k = j;
    }
    // os blends com a PORTA (o filtro é ESTÁVEL — a ordem sobrevive)
    std::vector<u32> keep, entF;
    std::vector<f32> zF;
    for (const BlendRow& b : blends) {
        keep.clear();
        for (u32 t = 0; t < b.count; ++t) {
            if (bm.resident(b.entries[t])) keep.push_back(t);
        }
        out.pending += b.count - static_cast<u32>(keep.size());
        if (keep.empty()) continue;
        entF.clear();
        zF.clear();
        for (u32 t : keep) {
            entF.push_back(b.entries[t]);
            zF.push_back(b.zNdc[t]);
        }
        BlockMesh::Stats ss;
        const DrawStats ds = bm.drawBlendPicks(r, entF.data(), b.model,
                                               zF.data(),
                                               static_cast<u32>(entF.size()),
                                               vp, nullptr, nullptr, ss);
        out.drawn += ss.drawn;
        out.total = out.total + ds;
    }
    out.instDraws =
        static_cast<u32>(glstub::stats.drawElementsInstancedCalls) - d0;
    return out;
}

} // namespace

// ---- PIN 1 — o 1.º frame é LEVE por construção (o anti-freeze) ------------

TEST(fase375a_sem_freeze_1a_frame) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeGMeshSpaced(12, 10.0f, false, gmesh, err));   // 12 blocos
    StreamStorage st;
    st.files["assets/stream.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/stream.gmesh", err));
    EXPECT(bm.table().size() == 12);

    Renderer r;
    EXPECT(r.init());
    const Mat4 vp = Mat4::ortho(-200.0f, 200.0f, -200.0f, 200.0f, 0.1f,
                                500.0f);
    const Vec3 camEye{0.0f, 0.0f, 0.0f};
    // 6 clones (o padrão da cena do dono), TODOS visíveis
    std::vector<Mat4> models;
    for (u32 k = 0; k < 6; ++k) {
        models.push_back(
            Mat4::translation(-5.0f + 2.0f * f32(k), 0.0f, -10.0f));
    }
    // O PIN: o 1.º frame carrega SÓ a vaga (8 de 12) e desenha os 8
    // grupos residentes (48 instâncias); 24 instâncias ficam «a
    // transmitir» — NENHUM frame paga os 12 loads de uma vez.
    const StreamRun run = runStreamed(bm, r, vp, camEye, models);
    EXPECT(run.loads == BlockMesh::kBlockStreamBudgetPerFrame);   // 8
    EXPECT(run.loads == 8);
    EXPECT(run.instDraws == 8);              // 8 grupos → 8 draws instanciados
    EXPECT(run.total.drawCalls == 8);
    EXPECT(run.total.instancesMerged == 48); // 8 grupos × 6 clones
    EXPECT(run.drawn == 48);
    EXPECT(run.pending == 24);               // 4 grupos × 6 clones
    // as camadas: os residentes são os 8 MAIS PERTOS (entries 0..7 — os
    // empates ficam na ordem da tabela, o stable_sort manda)
    for (u32 i = 0; i < 12; ++i) {
        EXPECT(bm.resident(i) == (i < 8));
    }
}

// ---- PIN 2 — as CAMADAS: perto primeiro -------------------------------------

TEST(fase375a_camadas_perto_primeiro) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeGMeshSpaced(12, 10.0f, false, gmesh, err));
    StreamStorage st;
    st.files["assets/stream.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/stream.gmesh", err));
    Renderer r;
    EXPECT(r.init());
    const Mat4 vp = Mat4::ortho(-200.0f, 200.0f, -200.0f, 200.0f, 0.1f,
                                500.0f);
    const Vec3 camEye{0.0f, 0.0f, 0.0f};
    std::vector<Mat4> models{Mat4::identity()};   // 1 TIC (a matemática limpa)
    // vaga 3: os residentes são EXATAMENTE os 3 mais perto (0,1,2 —
    // distância 0, 10, 20; os blocos estão EM LINHA a 10×i)
    const StreamRun run =
        runStreamed(bm, r, vp, camEye, models, true, 3);
    EXPECT(run.loads == 3);
    EXPECT(bm.resident(0) && bm.resident(1) && bm.resident(2));
    for (u32 i = 3; i < 12; ++i) {
        EXPECT(!bm.resident(i));
    }
    // o eye DESLOCADO inverte a ordem: com o camEye no bloco 11, os mais
    // perto são 11,10,9 (as camadas seguem o olho, não a tabela)
    BlockMesh bm2;
    EXPECT(bm2.open(st, "assets/stream.gmesh", err));
    const Vec3 eyeFar{110.0f, 0.0f, 0.0f};
    const StreamRun run2 =
        runStreamed(bm2, r, vp, eyeFar, models, true, 3);
    EXPECT(run2.loads == 3);
    EXPECT(bm2.resident(11) && bm2.resident(10) && bm2.resident(9));
    EXPECT(!bm2.resident(0));
}

// ---- PIN 3 — o fill completa em VAGAS (8 → 4 → 0) ---------------------------

TEST(fase375a_budget_por_frame) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeGMeshSpaced(12, 10.0f, false, gmesh, err));
    StreamStorage st;
    st.files["assets/stream.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/stream.gmesh", err));
    Renderer r;
    EXPECT(r.init());
    const Mat4 vp = Mat4::ortho(-200.0f, 200.0f, -200.0f, 200.0f, 0.1f,
                                500.0f);
    const Vec3 camEye{0.0f, 0.0f, 0.0f};
    std::vector<Mat4> models{Mat4::identity()};
    u32 sumLoads = 0;
    // frame 1: a vaga inteira (8), 4 a transmitir
    {
        const StreamRun run = runStreamed(bm, r, vp, camEye, models);
        EXPECT(run.loads == 8);
        EXPECT(run.pending == 4);
        EXPECT(run.drawn == 8);
        sumLoads += run.loads;
    }
    // frame 2: a cauda (4) + os 8 residentes desenham outra vez → o frame
    // fica COMPLETO (12 desenhados, 0 a transmitir)
    {
        const StreamRun run = runStreamed(bm, r, vp, camEye, models);
        EXPECT(run.loads == 4);
        EXPECT(run.pending == 0);
        EXPECT(run.drawn == 12);
        EXPECT(run.instDraws == 12);
        sumLoads += run.loads;
    }
    // frame 3: nada a carregar (o steady state — os 12 desenham de novo)
    {
        const StreamRun run = runStreamed(bm, r, vp, camEye, models);
        EXPECT(run.loads == 0);
        EXPECT(run.pending == 0);
        EXPECT(run.drawn == 12);
        sumLoads += run.loads;
    }
    EXPECT(sumLoads == 12);   // Σ loads == os blocos do ficheiro (nada
                              // carregado 2×, nada ficado por carregar)
}

// ---- PIN 4 — o toggle OFF devolve o caminho de sempre (byte a byte) ---------

TEST(fase375a_toggle_off) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeGMeshSpaced(12, 10.0f, false, gmesh, err));
    StreamStorage st;
    st.files["assets/stream.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/stream.gmesh", err));
    Renderer r;
    EXPECT(r.init());
    const Mat4 vp = Mat4::ortho(-200.0f, 200.0f, -200.0f, 200.0f, 0.1f,
                                500.0f);
    const Vec3 camEye{0.0f, 0.0f, 0.0f};
    std::vector<Mat4> models;
    for (u32 k = 0; k < 6; ++k) {
        models.push_back(
            Mat4::translation(-5.0f + 2.0f * f32(k), 0.0f, -10.0f));
    }
    // o streaming OFF: o draw INDIVIDUAL de sempre — os 12 blocos carregam
    // INLINE no 1.º frame (o freeze de sempre, assumido), zero draws
    // instanciados, zero «a transmitir» (a porta não existe neste caminho)
    const StreamRun run =
        runStreamed(bm, r, vp, camEye, models, false);
    EXPECT(run.loads == 12);
    EXPECT(run.drawn == 72);
    EXPECT(run.pending == 0);
    EXPECT(run.instDraws == 0);
    for (u32 i = 0; i < 12; ++i) {
        EXPECT(bm.resident(i));   // tudo residente no fim do 1.º frame
    }
}

// ---- PIN 5 — o .goni da 0.11-T abre com streaming ON por default ------------

TEST(fase375a_compat_011t) {
    glstub::reset();
    // (a) um TIC novo nasce ON (o default que cura o freeze)
    {
        Scene scene;
        Handle h = scene.create("A");
        Tic* t = scene.get(h);
        ASSERT(t != nullptr);
        MeshRenderer* mr = t->addComponent<MeshRenderer>();
        ASSERT(mr != nullptr);
        EXPECT(mr->streaming);
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
        EXPECT(dump1.find("streaming") == std::string::npos);
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
        mr->streaming = false;
        dump2 = SceneSerializer::dump(scene);
        EXPECT(dump2.find("\"streaming\":false") != std::string::npos);
    }
    // (d) o load do (c) restaura OFF; o load do (b) (a forma 0.11-T) fica
    //     ON por default — e o round-trip do (b) é byte a byte
    {
        Scene s1;
        SceneSerializer::LoadCtx ctx;
        EXPECT(SceneSerializer::loadText(s1, dump1, ctx));
        const auto& mrs1 = s1.components().meshRenderers();
        EXPECT(mrs1.size() == 1);
        EXPECT(mrs1.at(0).streaming);   // ausente = ON (o default)
        Scene s2;
        EXPECT(SceneSerializer::loadText(s2, dump2, ctx));
        const auto& mrs2 = s2.components().meshRenderers();
        EXPECT(mrs2.size() == 1);
        EXPECT(!mrs2.at(0).streaming);   // false restaura false
        // o round-trip da forma 0.11-T: save(load(dump1)) == dump1
        Scene s3;
        EXPECT(SceneSerializer::loadText(s3, dump1, ctx));
        const std::string again = SceneSerializer::dump(s3);
        EXPECT(again == dump1);   // byte a byte (o pin do compat)
    }
}

// ---- PIN 6 — os blends pendentes pedem vaga TAMBÉM --------------------------

TEST(fase375a_blend_pending) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeGMeshSpaced(3, 10.0f, true, gmesh, err));   // 3 blocos BLEND
    StreamStorage st;
    st.files["assets/blend.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/blend.gmesh", err));
    EXPECT(bm.table().size() == 3);
    Renderer r;
    EXPECT(r.init());
    const Mat4 vp = Mat4::ortho(-200.0f, 200.0f, -200.0f, 200.0f, 0.1f,
                                500.0f);
    const Vec3 camEye{0.0f, 0.0f, 0.0f};
    std::vector<Mat4> models{Mat4::identity()};
    // frame 1 (vaga 2): os 2 mais perto carregam e desenham (1 draw por
    // bloco — os blends NUNCA se fundem), o 3.º fica «a transmitir»
    {
        const StreamRun run =
            runStreamed(bm, r, vp, camEye, models, true, 2);
        EXPECT(run.loads == 2);
        EXPECT(run.drawn == 2);
        EXPECT(run.pending == 1);
        EXPECT(run.total.instancesMerged == 0);   // blends não fundem
    }
    // frame 2: o 3.º chega pela vaga e desenha — o passe fica completo
    {
        const StreamRun run =
            runStreamed(bm, r, vp, camEye, models, true, 2);
        EXPECT(run.loads == 1);
        EXPECT(run.drawn == 3);
        EXPECT(run.pending == 0);
    }
}

// ---- PIN 7 — o bloco corrupto NÃO consome a vaga ----------------------------

TEST(fase375a_falha_nao_consome) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeGMeshSpaced(4, 10.0f, false, gmesh, err));
    StreamStorage st;
    st.files["assets/stream.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/stream.gmesh", err));
    // corromper o bloco 0 (o MAIS PERTO — o pior caso): um flip no meio
    // da faixa de dados → o CRC32 do bloco não bate (o materialize
    // recusa — «CHECKSUM CORROMPIDO»)
    {
        const GMeshV3Block& blk = bm.table()[0];
        std::string& bytes = st.files["assets/stream.gmesh"];
        EXPECT(blk.dataOffset + blk.dataSize <= bytes.size());
        const size_t mid = static_cast<size_t>(blk.dataOffset) +
                           static_cast<size_t>(blk.dataSize / 2);
        bytes[mid] = static_cast<char>(bytes[mid] ^ 0xFF);
    }
    Renderer r;
    EXPECT(r.init());
    const Mat4 vp = Mat4::ortho(-200.0f, 200.0f, -200.0f, 200.0f, 0.1f,
                                500.0f);
    const Vec3 camEye{0.0f, 0.0f, 0.0f};
    std::vector<Mat4> models{Mat4::identity()};
    // vaga 2: o corrupto (o mais perto) FALHA e NÃO gasta a vaga — os
    // blocos 1 e 2 (os vivos mais pertos) enchem o frame; se a falha
    // gastasse a vaga, só o bloco 1 carregava (loaded==1 = vermelho)
    const StreamRun run =
        runStreamed(bm, r, vp, camEye, models, true, 2);
    EXPECT(run.loads == 2);
    EXPECT(bm.resident(1) && bm.resident(2));
    EXPECT(!bm.resident(0));
    EXPECT(!bm.resident(3));
    EXPECT(run.drawn == 2);
    EXPECT(run.pending == 2);   // o corrupto (retry de sempre) + o 3
}

// ---- PIN 8 — a contabilidade fecha (Σ loads == blocos; o merge final) -------

TEST(fase375a_stats_corretos) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeGMeshSpaced(12, 10.0f, false, gmesh, err));
    StreamStorage st;
    st.files["assets/stream.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/stream.gmesh", err));
    Renderer r;
    EXPECT(r.init());
    const Mat4 vp = Mat4::ortho(-200.0f, 200.0f, -200.0f, 200.0f, 0.1f,
                                500.0f);
    const Vec3 camEye{0.0f, 0.0f, 0.0f};
    std::vector<Mat4> models;
    for (u32 k = 0; k < 6; ++k) {
        models.push_back(
            Mat4::translation(-5.0f + 2.0f * f32(k), 0.0f, -10.0f));
    }
    u32 sumLoads = 0;
    u32 sumPending = 0;
    StreamRun run{};
    for (int frame = 0; frame < 4; ++frame) {
        run = runStreamed(bm, r, vp, camEye, models);
        sumLoads += run.loads;
        sumPending += run.pending;
    }
    // a conta fecha: os 12 blocos carregaram EXATAMENTE 1× cada (Σ loads
    // == 12), o fill durou 2 frames (8 + 4), o steady state desenha o
    // merge completo (12 grupos × 6 clones) e não sobra nada pendente
    EXPECT(sumLoads == 12);
    EXPECT(sumPending == 24);   // só o frame 1 teve pendentes (4×6)
    EXPECT(run.loads == 0);
    EXPECT(run.pending == 0);
    EXPECT(run.drawn == 72);
    EXPECT(run.instDraws == 12);
    EXPECT(run.total.drawCalls == 12);
    EXPECT(run.total.instancesMerged == 72);
}
