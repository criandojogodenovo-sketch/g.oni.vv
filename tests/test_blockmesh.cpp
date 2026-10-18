// tests/test_blockmesh.cpp — 0.10-M (PASSO 4) — O RENDER POR BLOCOS SOB
// PROVA.
//
// O contrato (o do dono, palavra a palavra):
//   • A ABERTURA é a TABELA (192 B + faixa da tabela + materiais — ZERO
//     bytes dos dados dos blocos; o storage CONTA as leituras);
//   • O FRUSTUM por bloco (AABB da tabela × 6 planos) — o AUDIT devolve a
//     lista dos visíveis (a câmara que vê metade, vê MESMO metade);
//   • LAZY: o 1º draw carrega os visíveis (e SÓ eles); o 2º não relê nada;
//   • LRU com ORÇAMENTOS DECLARADOS (RAM 64 MB + VRAM 192 MB da casa):
//     sob orbit contínuo a cache NUNCA passa o orçamento (o pin dos 920
//     blocos, com orçamento APERTADO no teste);
//   • O PIN DE RAM: 920 blocos num filho fork com VmHWM reiniciado — o
//     pico mede a CACHE + tabela + 1 bloco transiente, NUNCA o modelo;
//   • A CORRUPÇÃO de um bloco não derruba os outros (CRC por bloco);
//   • O EXPORT por blocos == o export do mesh inteiro (geometria).
#include "TestFramework.h"

#include <GLES3/gl3.h>   // stub do hospedeiro (glstub::stats)

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "assets/Assets.h"
#include "assets/GOwnFormats.h"
#include "assets/ObjExporter.h"
#include "assets/ObjImporter.h"
#include "core/FsStorage.h"
#include "core/ProjectStorage.h"
#include "math/Math.h"
#include "platform/EngineLog.h"
#include "render/BlockMesh.h"
#include "render/Camera.h"
#include "render/Mesh.h"
#include "render/Renderer.h"
#include "FakeStorage.h"

using namespace vv;

namespace {

// o storage que CONTA as leituras por faixa (o pin «a abertura é a tabela»
// precisa de PROVA: nenhuma leitura toca os dados dos blocos antes do lazy).
// Implementação mínima contra o ProjectStorage (o FakeStorage é `final` —
// o mapa de ficheiros é o MESMO modelo dele)
struct CountingStorage final : vv::ProjectStorage {
    std::map<std::string, std::string> files;
    mutable u32 rangeReads = 0;
    mutable u64 rangeBytes = 0;
    mutable u64 dataRegionReads = 0;   // leituras que tocam [192, tableOffset)
    u64 tableOffsetHint = 0;           // o teste arma depois do 1º open

    std::string root() const override { return "/count"; }
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
    bool readText(const std::string& relPath,
                  std::string& out) const override {
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
        ++rangeReads;
        rangeBytes += len;
        if (tableOffsetHint > 0 && offset >= 192 &&
            offset < tableOffsetHint) {
            ++dataRegionReads;   // tocou a região dos DADOS dos blocos
        }
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        const std::string& data = it->second;
        if (offset >= data.size() || data.size() - offset < len) {
            return false;
        }
        out.assign(data.begin() + static_cast<long>(offset),
                   data.begin() + static_cast<long>(offset + len));
        return true;
    }
};

// ---- geradores --------------------------------------------------------------

// uma FAIXA de quads (um retângulo no plano XZ, centrado em cx): vira UM
// grupo do MeshData → UM bloco com o AABB da faixa (o culling precisa de
// AABBs distintos por bloco)
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

// o modelo de N BLOCOS em faixas laterais (bloco i centrado em x=i*4):
// AABBs separados → o frustum escolhe metades
bool makeStripBlocks(int nBlocks, int nx, int nz, std::vector<u8>& gmesh,
                     std::string& err) {
    MeshData m;
    m.name = "tira";
    for (int i = 0; i < nBlocks; ++i) {
        addQuadStrip(m, f32(i) * 4.0f, 0.0f, 1.0f, nx, nz,
                     i % 2 == 0 ? "verde" : "ambar");
    }
    return writeGMesh(m, gmesh, err);
}

// ---- o mini-escritor GRANDE (total > 65 535 — o MeshData u16 não chega) ----
// O layout é o do writeGMesh (blocos GMeshV3BlockIn + gmeshV3Skeleton): o
// MESMO formato, sem o teto do mesh único — é assim que se fabrica um
// «203 MB» de laboratório (a escala muda, o formato não).
bool makeBigBlocks(int nBlocks, int vertsPerBlock, std::vector<u8>& gmesh,
                   std::string& err) {
    std::vector<GMeshV3BlockIn> bins(static_cast<size_t>(nBlocks));
    GMeshV3Meta meta;
    meta.flags = 0;
    meta.blockVertexCap = kGmeshV3BlockVertexCap;
    meta.attrs[0] = GMeshV3Attr{kAttrPosition, kAttrF32, 0, 0, 0};
    meta.attrs[1] = GMeshV3Attr{kAttrNormal, kAttrF32, 0, 0, 0};
    meta.attrs[2] = GMeshV3Attr{kAttrUv0, kAttrF32, 0, 0, 0};
    meta.attrCount = 3;
    u64 totalV = 0, totalI = 0;
    Vec3 gmn{1e30f, 1e30f, 1e30f}, gmx{-1e30f, -1e30f, -1e30f};
    for (int bi = 0; bi < nBlocks; ++bi) {
        GMeshV3BlockIn& b = bins[static_cast<size_t>(bi)];
        b.material = bi % 2 == 0 ? "verde" : "ambar";
        b.use32 = false;   // vertsPerBlock ≤ 65535
        const f32 cx = f32(bi % 40) * 4.0f;
        const f32 cz = f32(bi / 40) * 4.0f;
        b.vertices.reserve(static_cast<size_t>(vertsPerBlock));
        b.indices32.reserve(static_cast<size_t>(vertsPerBlock / 2) * 3);
        b.aabbMin = Vec3{1e30f, 1e30f, 1e30f};
        b.aabbMax = Vec3{-1e30f, -1e30f, -1e30f};
        for (int v = 0; v < vertsPerBlock; ++v) {
            Vertex vt;
            const f32 fx = f32(v % 64) / 64.0f;
            const f32 fz = f32(v / 64) / 64.0f;
            vt.pos = Vec3{cx - 1.0f + 2.0f * fx, 0.0f, cz - 1.0f + 2.0f * fz};
            vt.normal = Vec3{0.0f, 1.0f, 0.0f};
            vt.uv = Vec2{fx, fz};
            b.vertices.push_back(vt);
            b.aabbMin.x = (std::min)(b.aabbMin.x, vt.pos.x);
            b.aabbMin.z = (std::min)(b.aabbMin.z, vt.pos.z);
            b.aabbMax.x = (std::max)(b.aabbMax.x, vt.pos.x);
            b.aabbMax.z = (std::max)(b.aabbMax.z, vt.pos.z);
            if (v + 3 <= vertsPerBlock) {
                b.indices32.push_back(static_cast<u32>(v));
                b.indices32.push_back(static_cast<u32>(v + 1));
                b.indices32.push_back(static_cast<u32>(v + 2));
            }
        }
        b.aabbMin.y = -0.1f;
        b.aabbMax.y = 0.1f;
        totalV += b.vertices.size();
        totalI += b.indices32.size();
        gmn.x = (std::min)(gmn.x, b.aabbMin.x);
        gmn.y = (std::min)(gmn.y, b.aabbMin.y);
        gmn.z = (std::min)(gmn.z, b.aabbMin.z);
        gmx.x = (std::max)(gmx.x, b.aabbMax.x);
        gmx.y = (std::max)(gmx.y, b.aabbMax.y);
        gmx.z = (std::max)(gmx.z, b.aabbMax.z);
    }
    meta.vertexCount = totalV;
    meta.indexCount = totalI;
    meta.blockCount = static_cast<u64>(nBlocks);
    meta.aabbMin = gmn;
    meta.aabbMax = gmx;
    std::vector<std::string> mats;
    std::vector<u32> matIdx(bins.size(), 0);
    for (size_t i = 0; i < bins.size(); ++i) {
        u32 found = 0xFFFFFFFFu;
        for (size_t k = 0; k < mats.size(); ++k) {
            if (mats[k] == bins[i].material) {
                found = static_cast<u32>(k);
                break;
            }
        }
        if (found == 0xFFFFFFFFu) {
            mats.push_back(bins[i].material);
            found = static_cast<u32>(mats.size() - 1);
        }
        matIdx[i] = found;
    }
    meta.materialCount = mats.size();
    // a serialização de UM bloco (a MESMA do writeGMesh — stride 32)
    const u32 stride = meta.vertexStride();
    auto serializeBlock = [&](const GMeshV3BlockIn& b,
                              std::vector<u8>& blob) {
        blob.clear();
        auto u16w = [&](u16 v) {
            blob.push_back(static_cast<u8>(v & 0xFF));
            blob.push_back(static_cast<u8>(v >> 8));
        };
        auto u32w = [&](u32 v) {
            u16w(static_cast<u16>(v & 0xFFFF));
            u16w(static_cast<u16>(v >> 16));
        };
        auto f32w = [&](f32 v) {
            u32 raw;
            std::memcpy(&raw, &v, 4);
            u32w(raw);
        };
        for (const Vertex& v : b.vertices) {
            f32w(v.pos.x);
            f32w(v.pos.y);
            f32w(v.pos.z);
            f32w(v.normal.x);
            f32w(v.normal.y);
            f32w(v.normal.z);
            f32w(v.uv.x);
            f32w(v.uv.y);
        }
        for (const u32 idx : b.indices32) {
            u16w(static_cast<u16>(idx));
        }
    };
    std::vector<GMeshV3Block> entries(bins.size());
    for (size_t i = 0; i < bins.size(); ++i) {
        const GMeshV3BlockIn& b = bins[i];
        GMeshV3Block& e = entries[i];
        e.dataSize = b.vertices.size() * stride +
                     b.indices32.size() * (b.use32 ? 4ull : 2ull);
        e.vertexCount = b.vertices.size();
        e.indexCount = b.indices32.size();
        e.aabbMin = b.aabbMin;
        e.aabbMax = b.aabbMax;
        e.materialIndex = matIdx[i];
        e.indexType = b.use32 ? 1u : 0u;
        std::vector<u8> blob;
        serializeBlock(b, blob);
        e.crc32 = gcrc32(blob.data(), blob.size());
    }
    GMeshV3Skeleton sk;
    if (!gmeshV3Skeleton(meta, entries, mats, sk, err)) {
        return false;
    }
    gmesh = sk.header;
    gmesh.insert(gmesh.end(), sk.meta.begin(), sk.meta.end());
    for (size_t i = 0; i < bins.size(); ++i) {
        while (gmesh.size() < entries[i].dataOffset) gmesh.push_back(0);
        std::vector<u8> blob;
        serializeBlock(bins[i], blob);
        gmesh.insert(gmesh.end(), blob.begin(), blob.end());
    }
    while (gmesh.size() < meta.blockTableOffset) gmesh.push_back(0);
    gmesh.insert(gmesh.end(), sk.table.begin(), sk.table.end());
    gmesh.insert(gmesh.end(), sk.materials.begin(), sk.materials.end());
    return true;
}

// o VmHWM com reset (o padrão do teste de medições — o kernel zera o pico)
u64 vmHwmKb() {
    FILE* f = std::fopen("/proc/self/status", "rb");
    if (!f) return 0;
    char buf[256];
    u64 kb = 0;
    while (std::fgets(buf, sizeof(buf), f)) {
        if (std::strncmp(buf, "VmHWM:", 6) == 0) {
            kb = std::strtoull(buf + 6, nullptr, 10);
            break;
        }
    }
    std::fclose(f);
    return kb;
}
u64 vmRssKb() {
    FILE* f = std::fopen("/proc/self/status", "rb");
    if (!f) return 0;
    char buf[256];
    u64 kb = 0;
    while (std::fgets(buf, sizeof(buf), f)) {
        if (std::strncmp(buf, "VmRSS:", 6) == 0) {
            kb = std::strtoull(buf + 6, nullptr, 10);
            break;
        }
    }
    std::fclose(f);
    return kb;
}
bool resetPeakRss() {
    FILE* f = std::fopen("/proc/self/clear_refs", "wb");
    if (!f) return false;
    const bool ok = std::fputc('5', f) == '5';
    std::fclose(f);
    return ok;
}

} // namespace

// ---- (1) a abertura é a TABELA (paridade + zero dados) ---------------------

TEST(blockmesh_abre_por_faixas_e_paridade_com_o_inteiro) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeStripBlocks(8, 8, 4, gmesh, err));   // 8 blocos, 405 verts
    CountingStorage st;
    st.files["assets/tira.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());

    BlockMesh bm;
    EXPECT(bm.open(st, "assets/tira.gmesh", err));
    EXPECT(bm.ok());
    EXPECT(bm.table().size() == 8);
    // a PARIDADE com o caminho inteiro (o MESMO leitor da tabela): meta,
    // entradas e materiais batem com o readGMeshV3Meta do ficheiro inteiro
    {
        GMeshV3Meta metaW;
        std::vector<GMeshV3Block> blocksW;
        std::vector<std::string> matsW;
        EXPECT(readGMeshV3Meta(gmesh.data(), gmesh.size(), metaW, blocksW,
                               matsW, err));
        EXPECT(bm.meta().vertexCount == metaW.vertexCount);
        EXPECT(bm.meta().indexCount == metaW.indexCount);
        EXPECT(bm.meta().blockCount == metaW.blockCount);
        EXPECT(bm.meta().aabbMin.x == metaW.aabbMin.x);
        EXPECT(bm.meta().aabbMax.x == metaW.aabbMax.x);
        EXPECT(bm.table().size() == blocksW.size());
        bool same = true;
        for (size_t i = 0; i < blocksW.size(); ++i) {
            same = same && bm.table()[i].dataOffset == blocksW[i].dataOffset &&
                   bm.table()[i].dataSize == blocksW[i].dataSize &&
                   bm.table()[i].vertexCount == blocksW[i].vertexCount &&
                   bm.table()[i].aabbMin.x == blocksW[i].aabbMin.x &&
                   bm.table()[i].crc32 == blocksW[i].crc32;
        }
        EXPECT(same);
        EXPECT(bm.materials().size() == matsW.size() &&
               bm.materials()[0] == "verde");
    }
    // o HULL de bounds: o AABB do meta, EXATO (o contrato do picker/cena)
    {
        Mesh hull;
        EXPECT(bm.createHull(hull));
        EXPECT(hull.ok());
        EXPECT(hull.vertexCount() == 8);
        EXPECT(hull.boundsMin().x == bm.meta().aabbMin.x);
        EXPECT(hull.boundsMax().x == bm.meta().aabbMax.x);
        EXPECT(hull.boundsMaxExtent() == bm.boundsMaxExtent());
    }
    // O PIN DA ABERTURA: nenhum byte da região dos DADOS foi lido (só o
    // peek de 192 B + a faixa da tabela + a faixa dos materiais).
    // (contadores a ZERO antes do 2º open — o 1º já provou a paridade)
    st.rangeReads = 0;
    st.rangeBytes = 0;
    st.dataRegionReads = 0;
    st.tableOffsetHint = bm.meta().blockTableOffset;
    BlockMesh bm2;
    EXPECT(bm2.open(st, "assets/tira.gmesh", err));
    EXPECT(st.rangeReads == 3);   // peek + tabela + materiais
    EXPECT(st.dataRegionReads == 0);
    EXPECT(st.rangeBytes ==
           192 + 8 * kGmeshV3BlockEntryBytes +
               (gmesh.size() - bm2.meta().materialTableOffset));
    // nada residente, zero bytes (o import NÃO carrega — a cura do eager)
    EXPECT(bm2.lastStats().resident == 0);
    EXPECT(bm2.lastStats().ramBytes == 0);
}

// ---- (2) o frustum por bloco (o AUDIT do culling) ---------------------------

TEST(blockmesh_frustum_por_bloco_audit) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeStripBlocks(8, 8, 4, gmesh, err));
    FakeStorage st;
    st.files["assets/tira.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/tira.gmesh", err));

    const Mat4 model = Mat4::identity();
    // A GEOMETRIA DO AUDIT (determinística): 8 blocos em x=4i±1; a câmara
    // DE FRENTE (eye no +z, yaw=0) centrada no MEIO dos blocos 0..3 —
    // T=(6,0,0), centro do range visível [-1,13] — com meia-largura 8 no
    // plano do alvo: tan(fovY/2)·D = 8 com fovY=1.0 → D≈14.64. O canto
    // esquerdo do bloco 0 está a 7 do centro (< 8−folga → VISIBLE); o
    // canto direito do bloco 4 está a 9 (> 8+folga → CULLED); a
    // profundidade z±1 come ±0.55 e NÃO vira o veredicto
    {
        Camera cam;
        cam.target = Vec3{6.0f, 0.0f, 0.0f};
        cam.yaw = 0.0f;
        cam.pitch = 0.0f;
        cam.dist = 14.642f;
        cam.fovY = 1.0f;
        const Mat4 vp = Mat4::mul(cam.proj(1.0f), cam.view());
        std::vector<u32> vis;
        const u32 n = bm.visibleBlocks(model, vp, vis);
        EXPECT(n == 4);   // os 4 da esquerda (0..3), NENHUM da direita
        bool onlyLeft = true;
        for (u32 i : vis) {
            onlyLeft = onlyLeft && i < 4;
        }
        EXPECT(onlyLeft);
    }
    // a câmara com o ALVO LATERALMENTE DESLOCADO (200 unidades à direita):
    // o modelo INTEIRO cai fora dos planos laterais — NADA visível
    {
        Camera cam;
        cam.target = Vec3{200.0f, 0.0f, 0.0f};
        cam.yaw = 0.0f;
        cam.pitch = 0.0f;
        cam.dist = 8.0f;
        cam.fovY = 0.5f;
        const Mat4 vp = Mat4::mul(cam.proj(1.0f), cam.view());
        std::vector<u32> vis;
        EXPECT(bm.visibleBlocks(model, vp, vis) == 0);
    }
    // a câmara LONGE e de frente: TUDO visível
    {
        Camera cam;
        cam.target = Vec3{14.0f, 0.0f, 0.0f};   // o centro do modelo
        cam.yaw = 0.0f;
        cam.pitch = 0.0f;
        cam.dist = 60.0f;
        cam.fovY = 1.0472f;
        const Mat4 vp = Mat4::mul(cam.proj(1.0f), cam.view());
        std::vector<u32> vis;
        EXPECT(bm.visibleBlocks(model, vp, vis) == 8);
    }
    // o MODEL TRANSFORMADO: o modelo MOVIDO para x=100..130 continua
    // TODO visível com a câmara larga centrada nele (o AABB do bloco é
    // transformado pelos 8 cantos — OBB conservativo)
    {
        Camera cam;
        cam.target = Vec3{114.0f, 0.0f, 0.0f};
        cam.yaw = 0.0f;
        cam.pitch = 0.0f;
        cam.dist = 60.0f;
        cam.fovY = 1.0472f;
        const Mat4 vp = Mat4::mul(cam.proj(1.0f), cam.view());
        Mat4 model2 = Mat4::translation(100.0f, 0.0f, 0.0f);
        std::vector<u32> vis;
        EXPECT(bm.visibleBlocks(model2, vp, vis) == 8);
    }
    // os PLANOS em si (o pin da fórmula — identidade = o cubo [-1,1]³)
    {
        f32 planes[6][4];
        BlockMesh::frustumPlanes(Mat4::identity(), planes);
        // o ponto (2, 0, 0) está FORA do left (x=1)… o plano left deixa
        // de ter x+1 ≥ 0: (2)+1 = 3 ≥ 0 — DENTRO do left, FORA do right
        bool outRight = planes[1][0] * 2.0f + planes[1][3] < 0.0f;
        EXPECT(outRight);   // right: -x+1 < 0 quando x > 1
        bool inAtOrigin = true;
        for (int p = 0; p < 6; ++p) {
            inAtOrigin = inAtOrigin && planes[p][3] >= 0.0f;
        }
        EXPECT(inAtOrigin);   // a origem está DENTRO dos 6
    }
}

// ---- (3) lazy + draw real (stub GL) + contabilidade --------------------------

TEST(blockmesh_lazy_primeira_visibilidade) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeStripBlocks(8, 8, 4, gmesh, err));
    CountingStorage st;
    st.files["assets/tira.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());

    Renderer r;
    EXPECT(r.init());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/tira.gmesh", err));
    st.tableOffsetHint = bm.meta().blockTableOffset;

    const Camera cam;
    const Mat4 model = Mat4::identity();
    // o modelo em x=0..28: a câmara de frente, LONGE — vê tudo
    Camera camAll;
    camAll.target = Vec3{14.0f, 0.0f, 0.0f};
    camAll.dist = 60.0f;
    const Mat4 vpAll = Mat4::mul(camAll.proj(1.0f), camAll.view());

    BlockMesh::Stats stt;
    const u32 readsBefore = st.rangeReads;   // (o open já leu 3)
    const DrawStats ds1 = bm.draw(r, model, vpAll, nullptr, nullptr, nullptr,
                                  0, stt);
    // o 1º frame: carrega os 8 (SÓ os visíveis — aqui são todos)
    EXPECT(stt.totalBlocks == 8);
    EXPECT(stt.visible == 8);
    EXPECT(stt.drawn == 8);
    EXPECT(stt.loadedThisFrame == 8);
    EXPECT(stt.resident == 8);
    EXPECT(ds1.drawCalls == 8);   // 1 dc por bloco (a honestidade do HUD)
    EXPECT(st.rangeReads == readsBefore + 8);   // o lazy: 1 leitura por bloco
    EXPECT(glstub::stats.drawElementsCalls == 8);

    // o 2º frame: NADA relê (a cache entregou — o caminho quente)
    const u32 readsAfter1 = st.rangeReads;
    BlockMesh::Stats stt2;
    const DrawStats ds2 = bm.draw(r, model, vpAll, nullptr, nullptr, nullptr,
                                  0, stt2);
    EXPECT(stt2.drawn == 8);
    EXPECT(stt2.loadedThisFrame == 0);
    EXPECT(st.rangeReads == readsAfter1);   // ZERO leituras novas
    EXPECT(ds2.drawCalls == 8);
    EXPECT(stt2.vramBytes == stt.vramBytes);   // a contabilidade estável

    // metade visível: o 1º frame da vista ESQUERDA carrega SÓ os 4 (os
    // 4 da direita ficam por carregar — o contrato do lazy)
    BlockMesh bm3;
    EXPECT(bm3.open(st, "assets/tira.gmesh", err));
    Camera camHalf;
    camHalf.target = Vec3{6.0f, 0.0f, 0.0f};   // o MEIO dos blocos 0..3
    camHalf.yaw = 0.0f;
    camHalf.pitch = 0.0f;
    camHalf.dist = 14.642f;
    camHalf.fovY = 1.0f;
    const Mat4 vpHalf = Mat4::mul(camHalf.proj(1.0f), camHalf.view());
    BlockMesh::Stats stt3;
    bm3.draw(r, model, vpHalf, nullptr, nullptr, nullptr, 0, stt3);
    EXPECT(stt3.visible == 4);
    EXPECT(stt3.drawn == 4);
    EXPECT(stt3.loadedThisFrame == 4);
    EXPECT(stt3.resident == 4);   // os outros 4 NUNCA subiram
}

// ---- (4) LRU: orçamento contido sob orbit (o pin) ----------------------------

TEST(blockmesh_lru_orcamento_contido_sob_orbit) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    // 920 BLOCOS (o número do dono): 920 faixas de 5×4 quads (30 verts)
    EXPECT(makeStripBlocks(920, 5, 4, gmesh, err));
    FakeStorage st;
    st.files["assets/grade.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());

    Renderer r;
    EXPECT(r.init());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/grade.gmesh", err));
    EXPECT(bm.table().size() == 920);

    // o orçamento APERTADO de laboratório: cabe ~6 blocos em VRAM (a casa
    // declara 64+192 MB; o pin prova o MECANISMO com 192 KB)
    const u64 vramBudget = 192ull * 1024;
    const u64 ramBudget = 64ull * 1024;
    bm.setBudgets(ramBudget, vramBudget);

    // o ORBIT: 60 frames com a câmara a varrer o modelo (yaw muda por
    // frame — o conjunto visível muda, a cache tem de evictar)
    Camera cam;
    cam.target = Vec3{1838.0f, 0.0f, 0.0f};   // o centro (920×4/2)
    cam.dist = 400.0f;
    cam.fovY = 1.0472f;
    u64 maxVram = 0, maxRam = 0;
    u32 framesDrawn = 0;
    for (int f = 0; f < 60; ++f) {
        cam.yaw = 0.6f * std::sin(f * 0.21f);
        cam.pitch = 0.4f * std::cos(f * 0.13f);
        const Mat4 vp = Mat4::mul(cam.proj(1.0f), cam.view());
        BlockMesh::Stats stt;
        bm.draw(r, Mat4::identity(), vp, nullptr, nullptr, nullptr, 0, stt);
        if (stt.drawn > 0) {
            ++framesDrawn;
        }
        if (stt.vramBytes > maxVram) maxVram = stt.vramBytes;
        if (stt.ramBytes > maxRam) maxRam = stt.ramBytes;
        // O PIN: a cache NUNCA passa o orçamento (por frame, sob orbit)
        EXPECT_MSG(stt.vramBytes <= vramBudget,
                   "frame %d: vram %llu > orçamento %llu", f,
                   static_cast<unsigned long long>(stt.vramBytes),
                   static_cast<unsigned long long>(vramBudget));
        EXPECT_MSG(stt.ramBytes <= ramBudget,
                   "frame %d: ram %llu > orçamento %llu", f,
                   static_cast<unsigned long long>(stt.ramBytes),
                   static_cast<unsigned long long>(ramBudget));
    }
    EXPECT(framesDrawn == 60);          // todos os frames desenharam
    EXPECT(bm.lastStats().evictedTotal > 0);   // a eviction CORREU de verdade
    // e com folga honesta: o pico máximo ~ o orçamento (não 10× abaixo —
    // senão o teste não estava a provar nada sobre o limite)
    EXPECT(maxVram > vramBudget / 2);
}

// ---- (5) o pin de RAM (920 blocos, filho fork, VmHWM) ------------------------

TEST(blockmesh_pin_920_blocos_ram_pico_orcamento) {
    // o filho mede o pico DEPOIS de apagar o ficheiro da RAM do gerador:
    // o que sobra é a TABELA + a cache (≤ orçamento) + 1 bloco transiente
    const pid_t pid = ::fork();
    ASSERT(pid >= 0);
    if (pid == 0) {
        // ---- FILHO ---------------------------------------------------------
        glstub::reset();
        Renderer r;
        if (!r.init()) {
            ::_exit(30);
        }
        // o ficheiro GRANDE em disco (FsStorage real — a RAM do ficheiro
        // não conta para o pico do render): 920 blocos × 600 verts
        // (552 000 verts — o dobro do dragão do dono) ≈ 19 MB de .gmesh
        const std::string dir = "/tmp/goni_p4_" + std::to_string(::getpid());
        ::mkdir(dir.c_str(), 0755);
        {
            std::vector<u8> gmesh;
            std::string err;
            if (!makeBigBlocks(920, 600, gmesh, err)) {
                ::_exit(31);
            }
            FsStorage stW(dir);
            if (!stW.writeBytes("assets/grade.gmesh", gmesh.data(),
                                gmesh.size())) {
                ::_exit(32);
            }
        }   // o gmesh inteiro MORRE AQUI (antes do reset do pico)
        FsStorage st(dir);
        BlockMesh bm;
        std::string err;
        if (!bm.open(st, "assets/grade.gmesh", err)) {
            ::_exit(33);
        }
        if (bm.table().size() != 920) {
            ::_exit(34);
        }
        // o ORÇAMENTO APERTADO de laboratório: 2 MB de cache CPU / 4 MB de
        // VRAM (a casa declara 64+192 MB — o pin prova que o pico segue o
        // ORÇAMENTO, não o modelo: 19 MB de dados, 2 MB de teto de cache)
        bm.setBudgets(2ull * 1024 * 1024, 4ull * 1024 * 1024);
        if (!resetPeakRss()) {
            ::_exit(35);   // sem /proc — o host do CI tem; falha honesta
        }
        const u64 baseRss = vmRssKb();
        Camera cam;
        cam.target = Vec3{78.0f, 0.0f, 44.0f};
        cam.fovY = 1.0472f;
        // FASE A — O ORBIT REALÍSTICO do dono: perto do detalhe (dist 40),
        // o frustum vê ~15% dos 920 blocos; a cache estabiliza DENTRO do
        // orçamento sem churn — é o cenário do device com o modelo de
        // 203 MB na mão
        cam.dist = 40.0f;
        for (int f = 0; f < 30; ++f) {
            cam.yaw = 0.6f * std::sin(f * 0.2f);
            cam.pitch = 0.4f * std::cos(f * 0.15f);
            const Mat4 vp = Mat4::mul(cam.proj(1.0f), cam.view());
            BlockMesh::Stats stt;
            bm.draw(r, Mat4::identity(), vp, nullptr, nullptr, nullptr, 0,
                    stt);
            if (stt.vramBytes > bm.vramBudget() ||
                stt.ramBytes > bm.ramBudget()) {
                ::printf("  ] pino: frame %d PASSOU o orçamento (vram=%llu "
                         "ram=%llu)\n",
                         f, static_cast<unsigned long long>(stt.vramBytes),
                         static_cast<unsigned long long>(stt.ramBytes));
                std::fflush(stdout);
                ::_exit(36);
            }
        }
        const u64 peakA = vmHwmKb();
        const u64 deltaA = peakA > baseRss ? peakA - baseRss : 0;
        // FASE B — O PIOR CASO honesto: a câmara LONGE (dist 120) com o
        // frustum a engolir os 920 blocos TODOS: o conjunto visível é
        // MAIOR que o orçamento → há churn (recarrega/evicta por frame,
        // com o aviso no log) e o pico sobe pelos transientes do churn —
        // mas continua ABAIXO do modelo inteiro (o caminho do mesh único
        // arderia os 19 MB de uma vez; aqui nem o pior caso chega lá)
        if (!resetPeakRss()) {
            ::_exit(38);
        }
        const u64 baseB = vmRssKb();
        cam.dist = 120.0f;
        for (int f = 0; f < 20; ++f) {
            cam.yaw = 0.6f * std::sin(f * 0.3f);
            cam.pitch = 0.4f * std::cos(f * 0.2f);
            const Mat4 vp = Mat4::mul(cam.proj(1.0f), cam.view());
            BlockMesh::Stats stt;
            bm.draw(r, Mat4::identity(), vp, nullptr, nullptr, nullptr, 0,
                    stt);
            if (stt.vramBytes > bm.vramBudget() ||
                stt.ramBytes > bm.ramBudget()) {
                ::printf("  ] pino (B): frame %d PASSOU o orçamento "
                         "(vram=%llu ram=%llu)\n",
                         f, static_cast<unsigned long long>(stt.vramBytes),
                         static_cast<unsigned long long>(stt.ramBytes));
                std::fflush(stdout);
                ::_exit(36);
            }
        }
        const u64 peakB = vmHwmKb();
        const u64 deltaB = peakB > baseB ? peakB - baseB : 0;
        // a PROVA (os dois números no relatório):
        //   A: pico ≤ orçamento de cache (2 MB) + tabela (75 KB) + 1 bloco
        //      transiente (~23 KB) + folga do allocator (2 MB) — 5 MB de teto
        //   B: pico ≤ os 19 MB que o mesh único arderia (o churn nunca
        //      alcança o caminho de sempre)
        std::printf(
            "  ] pino 920 blocos: A(orbit realista) base=%llu KB pico=%llu "
            "KB delta=%llu KB — B(frustum com TUDO) base=%llu KB pico=%llu "
            "KB delta=%llu KB (orçamento ram=2 MB vram=4 MB; dados do "
            "modelo=19 MB — o pico segue o ORÇAMENTO, nunca o modelo)\n",
            static_cast<unsigned long long>(baseRss),
            static_cast<unsigned long long>(peakA),
            static_cast<unsigned long long>(deltaA),
            static_cast<unsigned long long>(baseB),
            static_cast<unsigned long long>(peakB),
            static_cast<unsigned long long>(deltaB));
        std::fflush(stdout);
        if (deltaA > 5ull * 1024) {
            ::_exit(37);
        }
        if (deltaB > 19ull * 1024) {
            ::_exit(39);
        }
        // a limpeza (a lição do wiring010: /tmp nunca fica sujo)
        ::remove((dir + "/assets/grade.gmesh").c_str());
        ::remove((dir + "/assets").c_str());
        ::remove(dir.c_str());
        ::_exit(0);
    }
    int status = 0;
    ::waitpid(pid, &status, 0);
    EXPECT(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        std::printf("  ] o filho do pino saiu com %d\n",
                    WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    }
}

// ---- (6) a corrupção de UM bloco não derruba os outros -----------------------

TEST(blockmesh_corrupcao_de_bloco_isolada) {
    glstub::reset();
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeStripBlocks(6, 8, 4, gmesh, err));
    // corromper UM byte dos DADOS do bloco 2 (dentro da região de dados:
    // o 1º bloco começa em 192; o bloco 2 em dataOffset da tabela)
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> mats;
    EXPECT(readGMeshV3Meta(gmesh.data(), gmesh.size(), meta, blocks, mats,
                           err));
    gmesh[static_cast<size_t>(blocks[2].dataOffset) + 10] ^= 0xFF;

    FakeStorage st;
    st.files["assets/ruim.gmesh"].assign(reinterpret_cast<const char*>(
                                             gmesh.data()),
                                         gmesh.size());
    Renderer r;
    EXPECT(r.init());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/ruim.gmesh", err));   // a tabela está BOM
    Camera cam;
    cam.target = Vec3{10.0f, 0.0f, 0.0f};
    cam.dist = 60.0f;
    const Mat4 vp = Mat4::mul(cam.proj(1.0f), cam.view());
    BlockMesh::Stats stt;
    bm.draw(r, Mat4::identity(), vp, nullptr, nullptr, nullptr, 0, stt);
    // 5 desenharam, 1 recusou (CRC) — os OUTROS não morrem por causa dele
    EXPECT(stt.totalBlocks == 6);
    EXPECT(stt.visible == 6);
    EXPECT(stt.drawn == 5);
}

// ---- (7) o export por blocos == o export inteiro (geometria) -----------------

TEST(blockmesh_export_por_tabela_parity_com_inteiro) {
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeStripBlocks(6, 8, 4, gmesh, err));
    FakeStorage st;
    st.files["assets/tira.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/tira.gmesh", err));

    // o STREAM: um bloco de cada vez (a TABELA manda)
    ObjStreamBlock stream;
    objStreamBegin(stream, "tira");
    MeshData part;
    for (size_t bi = 0; bi < bm.table().size(); ++bi) {
        EXPECT(bm.materializeBlock(bi, part, err));
        const GMeshV3Block& blk = bm.table()[bi];
        objStreamAppend(stream, part, "bloco " + std::to_string(bi),
                        bm.materials()[static_cast<size_t>(
                            blk.materialIndex)]);
    }
    EXPECT(stream.blocks == 6);
    EXPECT(stream.verts == bm.meta().vertexCount);
    EXPECT(stream.tris == bm.meta().indexCount / 3);

    // o INTEIRO (o caminho de sempre — este mesh cabe no mesh único)
    MeshData whole;
    EXPECT(readGMesh(gmesh.data(), gmesh.size(), whole, err));
    const std::string objWhole = exportObj(whole);

    // a GEOMETRIA é a MESMA: o parse dos dois devolve contagens iguais
    MeshData a, b;
    std::string perr;
    EXPECT(parseObj(stream.out.data(), stream.out.size(), a, perr));
    EXPECT(parseObj(objWhole.data(), objWhole.size(), b, perr));
    EXPECT(a.vertices.size() == b.vertices.size());
    EXPECT(a.indices.size() == b.indices.size());
    EXPECT(a.vertices.size() == bm.meta().vertexCount);
    // e o multiconjunto de POSIÇÕES bate (o round-trip por blocos não
    // perde nem duplica vértice nenhum)
    {
        u64 checks = 0, hits = 0;
        for (const Vertex& va : a.vertices) {
            for (const Vertex& vb2 : b.vertices) {
                if (std::fabs(va.pos.x - vb2.pos.x) < 1e-6f &&
                    std::fabs(va.pos.y - vb2.pos.y) < 1e-6f &&
                    std::fabs(va.pos.z - vb2.pos.z) < 1e-6f) {
                    ++hits;
                    break;
                }
            }
            ++checks;
        }
        EXPECT(checks == hits);
    }
}

// ---- (8) o .gmesh v1/v2 NÃO abre por blocos (o caminho de sempre) ------------

TEST(blockmesh_v1_recusado_o_caminho_de_sempre) {
    const char* kV1 = "tests/fixtures/gmesh_v1_esfera.gmesh";
    FILE* f = std::fopen(kV1, "rb");
    if (!f) {
        std::printf("  ] fixture v1 ausente — salta (o CI commita-a)\n");
        return;
    }
    std::vector<u8> bytes;
    {
        char buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
            bytes.insert(bytes.end(), buf, buf + n);
        }
        std::fclose(f);
    }
    FakeStorage st;
    st.files["meshes/v1.gmesh"].assign(reinterpret_cast<const char*>(
                                           bytes.data()),
                                       bytes.size());
    BlockMesh bm;
    std::string err;
    EXPECT(!bm.open(st, "meshes/v1.gmesh", err));
    EXPECT(!bm.ok());
    EXPECT(err.find("não é um .gmesh v3") != std::string::npos);
}

// ---- (9) skinned por blocos (o MESMO contrato de skin do mesh único) ---------

TEST(blockmesh_skinned_por_blocos) {
    glstub::reset();
    // 2 blocos com pele (joints/weights por vértice): o MeshData skinned
    // de sempre, com 2 grupos → 2 blocos
    MeshData m;
    m.name = "pele";
    for (int g = 0; g < 2; ++g) {
        const u32 base = static_cast<u32>(m.vertices.size());
        for (int v = 0; v < 24; ++v) {
            Vertex vt;
            vt.pos = Vec3{f32(g) * 2.0f + f32(v % 4) * 0.25f,
                          f32(v / 4) * 0.25f, 0.0f};
            vt.normal = Vec3{0.0f, 0.0f, 1.0f};
            vt.uv = Vec2{0.0f, 0.0f};
            m.vertices.push_back(vt);
            for (int c = 0; c < 4; ++c) {
                m.skinJoints.push_back(static_cast<u8>(c % 4));
                m.skinWeights.push_back(0.25f);
            }
        }
        MeshData::Group gr;
        gr.firstIndex = static_cast<u32>(m.indices.size());
        for (int q = 0; q + 1 < 4; ++q) {
            const u32 a = base + static_cast<u32>(q * 4);
            m.indices.push_back(static_cast<u16>(a));
            m.indices.push_back(static_cast<u16>(a + 5));
            m.indices.push_back(static_cast<u16>(a + 1));
            m.indices.push_back(static_cast<u16>(a + 5));
            m.indices.push_back(static_cast<u16>(a + 6));
            m.indices.push_back(static_cast<u16>(a + 1));
            gr.indexCount += 6;
        }
        m.groups.push_back(gr);
    }
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(writeGMesh(m, gmesh, err));
    FakeStorage st;
    st.files["assets/pele.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());
    Renderer r;
    EXPECT(r.init());
    BlockMesh bm;
    EXPECT(bm.open(st, "assets/pele.gmesh", err));
    EXPECT(bm.skinned());
    Camera cam;
    cam.target = Vec3{1.0f, 0.0f, 0.0f};
    cam.dist = 8.0f;
    const Mat4 vp = Mat4::mul(cam.proj(1.0f), cam.view());
    BlockMesh::Stats stt;
    const DrawStats ds = bm.draw(r, Mat4::identity(), vp, nullptr, nullptr,
                                 nullptr, 0, stt);
    EXPECT(stt.drawn == 2);
    EXPECT(ds.drawCalls == 2);
}

// ---- 0.10.6 (SAF-SEAM) — A FONTE DA ABERTURA (fd) + A QUARENTENA ------------
//
// O contrato do dono: a abertura por blocos usa a MESMA cascata do
// conversor — openReadFd / mapFd64; se o provider recusar o mmap, pread
// de faixas NO MESMO fd; NUNCA um caminho POSIX sob content://. Em falha
// o log diz QUAL das 3 leituras, o errno e o tamanho VISTO; a tabela que
// não valida (peek v3 verde) põe o asset em QUARENTENA (.corrupt) com a
// mensagem «asset corrompido, reimporta» — nunca a bola silenciosa.
namespace {

// o storage que serve FICHEIROS por fd E conta os ranges (a prova de que
// o mmap-fd serviu AS 3 LEITURAS: zero readBytesAt na abertura)
struct FdCountStorage final : vv::ProjectStorage {
    std::map<std::string, std::string> files;
    mutable u32 rangeReads = 0;

    std::string root() const override { return "/fdcount"; }
    bool makeDirs(const std::string&) override { return true; }
    bool exists(const std::string& relPath) const override {
        return files.count(relPath) != 0;
    }
    vv::Presence probe(const std::string& relPath) const override {
        return files.count(relPath) != 0 ? vv::Presence::Present
                                         : vv::Presence::Absent;
    }
    bool writeText(const std::string& relPath, const std::string& text) override {
        files[relPath] = text;
        return true;
    }
    bool readText(const std::string& relPath,
                  std::string& out) const override {
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
    bool readBytes(const std::string&, std::vector<vv::u8>&) const override {
        return false;
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
        ++rangeReads;   // ZERO disto na abertura por mmap-fd (o pin)
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        const std::string& data = it->second;
        if (offset >= data.size() || data.size() - offset < len) {
            return false;
        }
        out.assign(data.begin() + static_cast<long>(offset),
                   data.begin() + static_cast<long>(offset + len));
        return true;
    }
    // o fd do bridge: um MEMFD com os bytes (o mmap POR FD funciona — o
    // mesmo padrão do FakeStorage)
    bool openReadFd(const std::string& relPath, int* outFd,
                    std::string& err) override {
        if (outFd != nullptr) {
            *outFd = -1;
        }
        const auto it = files.find(relPath);
        if (it == files.end()) {
            err = "o ficheiro '" + relPath + "' não vive no storage fake";
            return false;
        }
        const int fd = ::memfd_create("vv-fdcount", 0);
        if (fd < 0) {
            err = "memfd_create falhou no fake";
            return false;
        }
        if (!it->second.empty()) {
            const ssize_t w = ::write(fd, it->second.data(), it->second.size());
            (void)w;
        }
        ::lseek(fd, 0, SEEK_SET);
        *outFd = fd;
        return true;
    }
};

// o storage que serve um PIPE no openReadFd (o provider de cloud que
// entrega stream: fstat size 0, pread ESPIPE — a recusa REAL do SO)
struct PipeFdStorage final : vv::ProjectStorage {
    std::map<std::string, std::string> files;
    mutable u32 rangeReads = 0;

    std::string root() const override { return "/pipefd"; }
    bool makeDirs(const std::string&) override { return true; }
    bool exists(const std::string& relPath) const override {
        return files.count(relPath) != 0;
    }
    vv::Presence probe(const std::string& relPath) const override {
        return files.count(relPath) != 0 ? vv::Presence::Present
                                         : vv::Presence::Absent;
    }
    bool writeText(const std::string& relPath, const std::string& text) override {
        files[relPath] = text;
        return true;
    }
    bool readText(const std::string& relPath,
                  std::string& out) const override {
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
    bool readBytes(const std::string&, std::vector<vv::u8>&) const override {
        return false;
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
        ++rangeReads;
        const auto it = files.find(relPath);
        if (it == files.end()) return false;
        const std::string& data = it->second;
        if (offset >= data.size() || data.size() - offset < len) {
            return false;
        }
        out.assign(data.begin() + static_cast<long>(offset),
                   data.begin() + static_cast<long>(offset + len));
        return true;
    }
    // o «fd do bridge» é um PIPE: o fstat dá 0 B (stream) e o pread leva
    // ESPIPE — a degradação para ranges é a RECUSA REAL do SO
    bool openReadFd(const std::string& relPath, int* outFd,
                    std::string& err) override {
        if (outFd != nullptr) {
            *outFd = -1;
        }
        const auto it = files.find(relPath);
        if (it == files.end()) {
            err = "o ficheiro '" + relPath + "' não vive no storage fake";
            return false;
        }
        int fds[2];
        if (::pipe(fds) != 0) {
            err = "pipe falhou no fake";
            return false;
        }
        if (!it->second.empty()) {
            const ssize_t w = ::write(fds[1], it->second.data(),
                                      it->second.size());
            (void)w;
        }
        ::close(fds[1]);   // o writer fecha — EOF depois dos bytes
        *outFd = fds[0];
        return true;
    }
};

// o storage cujo readBytesAt FALHA na faixa da TABELA (I/O — o provider
// engasga): a falha é REPORTADA com qual-leitura/tamanho; NÃO é quarentena
// (I/O não é corrupção — o ficheiro FICA no sítio)
struct FailTableStorage final : vv::ProjectStorage {
    std::map<std::string, std::string> files;
    u64 failFromOff = 256;   // falha a partir deste offset (a tabela)

    std::string root() const override { return "/failtable"; }
    bool makeDirs(const std::string&) override { return true; }
    bool exists(const std::string& relPath) const override {
        return files.count(relPath) != 0;
    }
    vv::Presence probe(const std::string& relPath) const override {
        return files.count(relPath) != 0 ? vv::Presence::Present
                                         : vv::Presence::Absent;
    }
    bool writeText(const std::string& relPath, const std::string& text) override {
        files[relPath] = text;
        return true;
    }
    bool readText(const std::string& relPath,
                  std::string& out) const override {
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
    bool readBytes(const std::string&, std::vector<vv::u8>&) const override {
        return false;
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
        if (offset >= failFromOff) {
            return false;   // o provider engasga NA faixa da tabela
        }
        const std::string& data = it->second;
        if (offset >= data.size() || data.size() - offset < len) {
            return false;
        }
        out.assign(data.begin() + static_cast<long>(offset),
                   data.begin() + static_cast<long>(offset + len));
        return true;
    }
    // sem openReadFd (o default honesto) — os ranges de sempre
};

bool bmLogHas(const char* dir, const char* needle) {
    FILE* f = std::fopen((std::string(dir) + "/logs/engine.log").c_str(),
                         "rb");
    if (!f) {
        return false;
    }
    std::string data;
    char buf[4096];
    size_t r;
    while ((r = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        data.append(buf, r);
    }
    std::fclose(f);
    return data.find(needle) != std::string::npos;
}

// o elog::init cria o diretório (e os pais) — o padrão do test_gmeshv3stream
std::string bmLogInit(const char* tag) {
    const std::string dir =
        "/tmp/goni_bm_safseam_" + std::to_string(::getpid()) + "_" + tag;
    elog::init((dir + "/logs").c_str());
    return dir;
}

void bmLogTerm(const std::string& dir) {
    elog::shutdown();
    // o rmRf do padrão (dirent + remove)
    DIR* d = ::opendir((dir + "/logs").c_str());
    if (d) {
        while (dirent* e = ::readdir(d)) {
            const std::string n = e->d_name;
            if (n != "." && n != "..") {
                ::remove((dir + "/logs/" + n).c_str());
            }
        }
        ::closedir(d);
    }
    ::remove((dir + "/logs").c_str());
    ::remove(dir.c_str());
}

}   // namespace

// (SAF-SEAM 1) A ABERTURA POR FD DO BRIDGE: o mmap POR FD serve as 3
// leituras (peek 192 B + tabela + materiais) — ZERO readBytesAt, e a
// linha contrato do load nomeia a FONTE (fonte=mmap-fd, o par do
// «asset: v3 fonte=…» do conversor). Seria VERMELHO na abertura por
// caminho/ranges de sempre (a mutação M-SS1).
TEST(blockmesh_fonte_mmapfd_o_fd_do_bridge_serve_as_3_leituras) {
    glstub::reset();
    const std::string dir = bmLogInit("mmapfd");
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeStripBlocks(8, 8, 4, gmesh, err));
    FdCountStorage st;
    st.files["assets/tira.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());

    BlockMesh bm;
    EXPECT(bm.open(st, "assets/tira.gmesh", err));
    EXPECT(bm.ok());
    EXPECT(bm.table().size() == 8);
    // O PIN: o mmap POR FD serviu TUDO — nenhum range pelo storage
    EXPECT(st.rangeReads == 0);
    // a linha contrato nomeia a FONTE (o par do conversor)
    EXPECT(bmLogHas(dir.c_str(), "fonte=mmap-fd"));
    EXPECT(bmLogHas(dir.c_str(), "gmesh: fase=load"));
    // o hull continua EXATO (o contrato do picker intacto)
    {
        Mesh hull;
        EXPECT(bm.createHull(hull));
        EXPECT(hull.ok());
        EXPECT(hull.vertexCount() == 8);
    }
    bmLogTerm(dir);
}

// (SAF-SEAM 2) O PROVIDER QUE ENTREGA STREAM (o pipe): o fstat do fd dá
// 0 B e o pread leva a RECUSA REAL do SO — a abertura DEGRADA para os
// ranges do storage (VERDE na mesma; o log diz a degradação honesta).
TEST(blockmesh_pipe_do_provider_degrada_para_ranges) {
    glstub::reset();
    const std::string dir = bmLogInit("pipe");
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeStripBlocks(8, 8, 4, gmesh, err));
    PipeFdStorage st;
    st.files["assets/tira.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());

    BlockMesh bm;
    EXPECT(bm.open(st, "assets/tira.gmesh", err));
    EXPECT(bm.ok());
    EXPECT(bm.table().size() == 8);
    // os ranges serviram as 3 leituras (o mmap/pread não deu)
    EXPECT(st.rangeReads == 3);
    // o AVISO do fd com 0 B + a degradação LOGADA (a causa honesta)
    EXPECT(bmLogHas(dir.c_str(), "veio com 0 B"));
    EXPECT(bmLogHas(dir.c_str(), "a degradar para RANGES pelo storage"));
    // a linha contrato: a fonte acabou nos ranges
    EXPECT(bmLogHas(dir.c_str(), "fonte=ranges"));
    bmLogTerm(dir);
}

// (SAF-SEAM 3) A TABELA TRUNCADA (o peek v3 VERDE, o fim do ficheiro não
// acompanha): QUARENTENA — o asset sai do catálogo (rename .corrupt, os
// bytes ficam p/ forense) e o err manda REIMPORTAR. NUNCA a bola
// silenciosa: o ficheiro não volta a aparecer como se nada fosse.
TEST(blockmesh_tabela_truncada_quarentena_corrupt) {
    glstub::reset();
    const std::string dir = bmLogInit("trunc");
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeStripBlocks(8, 8, 4, gmesh, err));
    // o blockTableOffset da META (o peek valida; a faixa da tabela vai
    // além do fim — truncado a meio da tabela)
    {
        GMeshV3Meta meta;
        std::vector<GMeshV3Block> blocks;
        std::vector<std::string> mats;
        EXPECT(readGMeshV3Meta(gmesh.data(), gmesh.size(), meta, blocks,
                               mats, err));
        const u64 tableLen =
            static_cast<u64>(blocks.size()) * kGmeshV3BlockEntryBytes;
        const u64 cut = meta.blockTableOffset + tableLen / 2;
        gmesh.resize(static_cast<size_t>(cut));
    }
    FakeStorage st;
    st.files["assets/tira.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());

    BlockMesh bm;
    EXPECT(!bm.open(st, "assets/tira.gmesh", err));
    EXPECT(!bm.ok());
    // a MENSAGEM CLARA (a spec do dono, palavra a palavra)
    EXPECT(err.find("asset corrompido, reimporta") != std::string::npos);
    EXPECT(err.find("tabela") != std::string::npos);
    // a QUARENTENA: o nome saiu do catálogo; os bytes ficam p/ forense
    EXPECT(!st.exists("assets/tira.gmesh"));
    EXPECT(st.exists("assets/tira.gmesh.corrupt"));
    {
        std::vector<u8> quar;
        EXPECT(st.readBytes("assets/tira.gmesh.corrupt", quar));
        EXPECT(quar.size() == gmesh.size());
    }
    EXPECT(bmLogHas(dir.c_str(), "ASSET CORROMPIDO"));
    EXPECT(bmLogHas(dir.c_str(), "quarentena"));
    bmLogTerm(dir);
}

// (SAF-SEAM 3b) OS MATERIAIS ALÉM DO FIM: o meta PROMETE uma tabela de
// materiais que começa depois do fim (patch do materialTableOffset +
// checksum FNV recalculado — o peek segue VERDE, a promessa é falsa).
// A MESMA quarentena: o asset sai do catálogo com a causa nos materiais.
TEST(blockmesh_materiais_alem_do_fim_quarentena) {
    glstub::reset();
    const std::string dir = bmLogInit("matoff");
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeStripBlocks(8, 8, 4, gmesh, err));
    // o patch: materialTableOffset (meta offset 80 → ficheiro 112) aponta
    // ALÉM do fim; o checksum do header (offset 20, FNV1a dos 160 B do
    // meta) recalcula-se — o PEEK segue verde (a corrupção é a PROMESSA)
    {
        const u64 beyond = gmesh.size() + 999;
        for (int b = 0; b < 8; ++b) {
            gmesh[112 + b] = static_cast<u8>((beyond >> (8 * b)) & 0xFF);
        }
        const u64 sum = gfnv1a(gmesh.data() + kGHeaderBytes,
                               kGmeshV3MetaBytes);
        for (int b = 0; b < 8; ++b) {
            gmesh[20 + b] = static_cast<u8>((sum >> (8 * b)) & 0xFF);
        }
        GMeshV3Meta meta;
        std::string perr;
        EXPECT(gmeshV3PeekMeta(gmesh.data(),
                               kGHeaderBytes + kGmeshV3MetaBytes, meta,
                               perr));   // o peek continua VERDE
        EXPECT(meta.materialTableOffset == beyond);
    }
    FakeStorage st;
    st.files["assets/tira.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());

    BlockMesh bm;
    EXPECT(!bm.open(st, "assets/tira.gmesh", err));
    EXPECT(err.find("asset corrompido, reimporta") != std::string::npos);
    EXPECT(err.find("materiais") != std::string::npos);
    EXPECT(err.find("alem do fim") != std::string::npos ||
           err.find("além do fim") != std::string::npos);
    EXPECT(!st.exists("assets/tira.gmesh"));
    EXPECT(st.exists("assets/tira.gmesh.corrupt"));
    bmLogTerm(dir);
}

// (SAF-SEAM 3c) O PEEK QUE NÃO VALIDA NÃO É QUARENTENA: um v1 VÁLIDO (ou
// lixo) que morre no peek segue para o caminho do mesh ÚNICO — o
// ficheiro FICA no sítio (renomear um v1 válido de <192 B matia-o).
TEST(blockmesh_peek_invalido_nao_e_quarentena) {
    glstub::reset();
    FakeStorage st;
    // lixo de 64 B: o peek falha — o ficheiro NÃO é renomeado
    st.files["assets/lixo.gmesh"] = std::string(64, '\x5A');
    BlockMesh bm;
    std::string err;
    EXPECT(!bm.open(st, "assets/lixo.gmesh", err));
    EXPECT(err.find("asset corrompido") == std::string::npos);
    EXPECT(st.exists("assets/lixo.gmesh"));
    EXPECT(!st.exists("assets/lixo.gmesh.corrupt"));
}

// (SAF-SEAM 4) A FALHA DE I/O (o provider engasga na faixa da tabela):
// o err diz QUAL das 3 leituras falhou e o tamanho VISTO — e NÃO é
// quarentena (I/O não é corrupção: o ficheiro fica p/ RETENTAR).
TEST(blockmesh_falha_de_io_diz_qual_leitura_e_tamanho_sem_quarentena) {
    glstub::reset();
    const std::string dir = bmLogInit("iotable");
    std::vector<u8> gmesh;
    std::string err;
    EXPECT(makeStripBlocks(8, 8, 4, gmesh, err));
    FailTableStorage st;
    st.files["assets/tira.gmesh"].assign(
        reinterpret_cast<const char*>(gmesh.data()), gmesh.size());

    BlockMesh bm;
    EXPECT(!bm.open(st, "assets/tira.gmesh", err));
    // QUAL leitura: a TABELA (o peek de 192 B passou)
    EXPECT(err.find("faixa da TABELA") != std::string::npos);
    // O TAMANHO VISTO (a linha de sempre truncava a causa)
    EXPECT(err.find("ficheiro visto com") != std::string::npos);
    EXPECT(err.find(std::to_string(gmesh.size())) != std::string::npos);
    // I/O ≠ corrupção: SEM quarentena (o ficheiro fica no sítio)
    EXPECT(st.exists("assets/tira.gmesh"));
    EXPECT(!st.exists("assets/tira.gmesh.corrupt"));
    EXPECT(err.find("asset corrompido") == std::string::npos);
    bmLogTerm(dir);
}
