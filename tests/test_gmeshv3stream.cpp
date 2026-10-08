// tests/test_gmeshv3stream.cpp — 0.10-M (PASSO 3) — O CONVERSOR STREAMING
// SOB PROVA: o caminho de PRODUÇÃO (convert::importFile + FsStorage real)
// produz o v3 com verificação bit a bit (a linha contrato
// «gmesh: v3 blocos=... verificado=1» no log), o conjunto de triângulos é
// O MESMO do caminho de sempre (merge em memória), as transformações dos
// nós vêm baked, a tabela agrupa por material, uma primitiva de 70k
// vértices corta em blocos (o teto u16 MORREU), o cancelamento não deixa
// estado e as texturas continuam no passe delas.
#include "TestFramework.h"

#include "assets/AssetConverter.h"
#include "assets/GltfImporter.h"
#include "assets/GmeshV3Stream.h"
#include "assets/ResourceManager.h"   // 0.10-M (PASSO 3B): o guard cedo do load
#include "core/FsStorage.h"
#include "core/Json.h"
#include "platform/EngineLog.h"
#include "platform/FileApi.h"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <map>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

using namespace vv;

bool mkDir(const std::string& p) {
    return ::mkdir(p.c_str(), 0755) == 0 || errno == EEXIST;
}

bool rmRf(const std::string& path) {
    DIR* d = opendir(path.c_str());
    if (d) {
        while (dirent* e = readdir(d)) {
            const std::string n = e->d_name;
            if (n == "." || n == "..") continue;
            const std::string full = path + "/" + n;
            struct stat st;
            if (lstat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                rmRf(full);
            } else {
                unlink(full.c_str());
            }
        }
        closedir(d);
        rmdir(path.c_str());
        return true;
    }
    unlink(path.c_str());
    return true;
}

bool fileHas(const std::string& path, const char* needle) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char buf[4096];
    bool found = false;
    while (std::fgets(buf, sizeof(buf), f)) {
        if (std::strstr(buf, needle) != nullptr) {
            found = true;
            break;
        }
    }
    std::fclose(f);
    return found;
}

// ---- o GERADOR GLB (determinístico; prims 1×mesh, nós com TRS) ------------

struct BW {
    std::vector<u8>& b;
    explicit BW(std::vector<u8>& v) : b(v) {}
    void u8_(u8 v) { b.push_back(v); }
    void u16_(u16 v) { b.push_back(static_cast<u8>(v & 0xFF));
                       b.push_back(static_cast<u8>(v >> 8)); }
    void u32_(u32 v) { u16_(static_cast<u16>(v & 0xFFFF));
                       u16_(static_cast<u16>(v >> 16)); }
    void f32_(f32 v) { u32 raw; std::memcpy(&raw, &v, 4); u32_(raw); }
    void put(const void* p, size_t n) { b.insert(b.end(), (const u8*)p, (const u8*)p + n); }
};

void vertexOf(u64 i, f32 out[3]) {
    const u64 rx = i % 997, ry = i % 991, rz = i % 983;
    out[0] = -1.0f + 2.0f * static_cast<f32>(rx) / 996.0f;
    out[1] = -1.0f + 2.0f * static_cast<f32>(ry) / 990.0f;
    out[2] = -1.0f + 2.0f * static_cast<f32>(rz) / 982.0f;
}

struct PrimSpec {
    u32 verts;
    u32 tris;
    u32 mat;
    bool u32idx;
};
struct NodeSpec {
    u32 mesh;
    f32 tx, ty, tz;
};

// o JSON + BIN de um GLB inteiro em memória (imagens opcionais — PNGs
// embutidos como bufferViews depois das primitivas)
bool buildGlb(const std::vector<PrimSpec>& prims,
              const std::vector<NodeSpec>& nodes,
              const std::vector<std::string>& mats,
              const std::vector<std::string>& pngs,
              std::vector<u8>& glb) {
    // ---- passada 1: os offsets dos bufferViews ----
    struct V { u64 off, len; };
    std::vector<V> views;
    u64 binLen = 0;
    for (const PrimSpec& p : prims) {
        const u64 lens[4] = {u64(p.verts) * 12, u64(p.verts) * 12,
                             u64(p.verts) * 8,
                             u64(p.tris) * 3 * (p.u32idx ? 4 : 2)};
        for (int a = 0; a < 4; ++a) {
            binLen += (4 - binLen % 4) % 4;
            views.push_back({binLen, lens[a]});
            binLen += lens[a];
        }
    }
    for (const std::string& s : pngs) {
        binLen += (4 - binLen % 4) % 4;
        views.push_back({binLen, s.size()});
        binLen += s.size();
    }
    const u64 binPadded = (binLen + 3) & ~u64(3);
    // ---- passada 2: o JSON ----
    std::string j = "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,"
                    "\"scenes\":[{\"nodes\":[0]}],\"nodes\":[";
    for (size_t n = 0; n < nodes.size(); ++n) {
        j += (n ? "," : "");
        char tr[96];
        std::snprintf(tr, sizeof(tr), "\"translation\":[%.1f,%.1f,%.1f]",
                      (double)nodes[n].tx, (double)nodes[n].ty,
                      (double)nodes[n].tz);
        j += std::string("{\"mesh\":") + std::to_string(nodes[n].mesh) +
             "," + tr + ",\"name\":\"n" + std::to_string(n) + "\"}";
    }
    j += "],\"meshes\":[";
    u32 acc = 0;
    for (size_t m = 0; m < prims.size(); ++m) {
        j += (m ? "," : "");
        j += "{\"primitives\":[{\"attributes\":{\"POSITION\":" +
             std::to_string(acc) + ",\"NORMAL\":" + std::to_string(acc + 1) +
             ",\"TEXCOORD_0\":" + std::to_string(acc + 2) + "},\"indices\":" +
             std::to_string(acc + 3) + ",\"material\":" +
             std::to_string(prims[m].mat) + ",\"mode\":4}]}";
        acc += 4;
    }
    j += "],\"materials\":[";
    for (size_t mi = 0; mi < mats.size(); ++mi) {
        j += (mi ? "," : "");
        j += "{\"name\":\"" + mats[mi] +
             "\",\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.8,"
             "0.8,1]}}";
    }
    if (!pngs.empty()) {
        j += "],\"images\":[";
        for (size_t ii = 0; ii < pngs.size(); ++ii) {
            j += (ii ? "," : "");
            j += "{\"bufferView\":" + std::to_string(acc + ii) +
                 ",\"mimeType\":\"image/png\"}";
        }
    }
    j += "],\"accessors\":[";
    for (size_t k = 0; k < prims.size(); ++k) {
        const PrimSpec& p = prims[k];
        const char* sep = k ? "," : "";
        j += sep;
        j += "{\"bufferView\":" + std::to_string(k * 4) +
             ",\"componentType\":5126,\"count\":" + std::to_string(p.verts) +
             ",\"type\":\"VEC3\",\"min\":[-1,-1,-1],\"max\":[1,1,1]}";
        j += ",{\"bufferView\":" + std::to_string(k * 4 + 1) +
             ",\"componentType\":5126,\"count\":" + std::to_string(p.verts) +
             ",\"type\":\"VEC3\"}";
        j += ",{\"bufferView\":" + std::to_string(k * 4 + 2) +
             ",\"componentType\":5126,\"count\":" + std::to_string(p.verts) +
             ",\"type\":\"VEC2\"}";
        j += ",{\"bufferView\":" + std::to_string(k * 4 + 3) +
             ",\"componentType\":" + (p.u32idx ? "5125" : "5123") +
             ",\"count\":" + std::to_string(u64(p.tris) * 3) +
             ",\"type\":\"SCALAR\"}";
    }
    j += "],\"bufferViews\":[";
    for (size_t k = 0; k < views.size(); ++k) {
        j += (k ? "," : "");
        j += "{\"buffer\":0,\"byteOffset\":" +
             std::to_string(views[k].off) + ",\"byteLength\":" +
             std::to_string(views[k].len) + "}";
    }
    j += "],\"buffers\":[{\"byteLength\":" + std::to_string(binPadded) +
         "}]}";
    const u64 jsonPadded = (u64(j.size()) + 3) & ~u64(3);
    j.append(size_t(jsonPadded - j.size()), ' ');
    // ---- passada 3: o GLB ----
    const u32 total = u32(12 + 8 + jsonPadded + 8 + binPadded);
    BW o(glb);
    u32 v32 = 0x46546C67; o.put(&v32, 4);
    v32 = 2; o.put(&v32, 4);
    v32 = total; o.put(&v32, 4);
    v32 = u32(jsonPadded); o.put(&v32, 4);
    v32 = 0x4E4F534A; o.put(&v32, 4);
    o.put(j.data(), j.size());
    v32 = u32(binPadded); o.put(&v32, 4);
    v32 = 0x004E4942; o.put(&v32, 4);
    const u64 binStart = glb.size();
    glb.resize(binStart + binPadded, 0);
    u8* bin = glb.data() + binStart;
    for (size_t k = 0; k < prims.size(); ++k) {
        const PrimSpec& p = prims[k];
        u8* pos = bin + views[k * 4 + 0].off;
        u8* nrm = bin + views[k * 4 + 1].off;
        u8* uv = bin + views[k * 4 + 2].off;
        u8* idx = bin + views[k * 4 + 3].off;
        for (u32 i = 0; i < p.verts; ++i) {
            f32 v[3];
            vertexOf(i, v);
            std::memcpy(pos + i * 12, v, 12);
            const f32 n[3] = {(i % 97) / 48.0f - 1.0f,
                              (i % 89) / 44.0f - 1.0f,
                              (i % 83) / 41.0f - 1.0f};
            std::memcpy(nrm + i * 12, n, 12);
            const f32 t[2] = {f32(i % 64) / 63.0f, f32(i % 32) / 31.0f};
            std::memcpy(uv + i * 8, t, 8);
        }
        for (u32 ti = 0; ti < p.tris; ++ti) {
            for (int c = 0; c < 3; ++c) {
                const u32 gi = (3 * ti + u32(c)) % p.verts;
                if (p.u32idx) {
                    std::memcpy(idx + (u64(ti) * 3 + c) * 4, &gi, 4);
                } else {
                    const u16 g = static_cast<u16>(gi);
                    std::memcpy(idx + (u64(ti) * 3 + c) * 2, &g, 2);
                }
            }
        }
    }
    for (size_t ii = 0; ii < pngs.size(); ++ii) {
        std::memcpy(bin + views[prims.size() * 4 + ii].off,
                    pngs[ii].data(), pngs[ii].size());
    }
    return true;
}

// ---- o MERGE de referência (o caminho de sempre, in-memory): a MESMA
// composição de mundo do merge antigo, 1 primitiva por nó ----
void mergeReferencia(const std::vector<PrimSpec>& prims,
                     const std::vector<NodeSpec>& nodes, MeshData& m) {
    u32 base = 0;
    for (size_t n = 0; n < nodes.size(); ++n) {
        const PrimSpec& p = prims[nodes[n].mesh];
        const Mat4 world = Mat4::translation(nodes[n].tx, nodes[n].ty,
                                             nodes[n].tz);
        for (u32 i = 0; i < p.verts; ++i) {
            f32 v[3];
            vertexOf(i, v);
            const f32 nn[3] = {(i % 97) / 48.0f - 1.0f,
                               (i % 89) / 44.0f - 1.0f,
                               (i % 83) / 41.0f - 1.0f};
            const f32 t[2] = {f32(i % 64) / 63.0f, f32(i % 32) / 31.0f};
            Vertex vx;
            vx.pos = Vec3{world.m[0] * v[0] + world.m[4] * v[1] +
                              world.m[8] * v[2] + world.m[12],
                          world.m[1] * v[0] + world.m[5] * v[1] +
                              world.m[9] * v[2] + world.m[13],
                          world.m[2] * v[0] + world.m[6] * v[1] +
                              world.m[10] * v[2] + world.m[14]};
            vx.normal = Vec3{nn[0], nn[1], nn[2]};
            vx.uv = Vec2{t[0], t[1]};
            m.vertices.push_back(vx);
        }
        for (u32 ti = 0; ti < p.tris; ++ti) {
            for (int c = 0; c < 3; ++c) {
                m.indices.push_back(
                    static_cast<u16>(base + (3 * ti + u32(c)) % p.verts));
            }
        }
        base += p.verts;
    }
}

// ---- a comparação POR CONJUNTO (a spec: reordenar triângulos é lícito;
// o conjunto de vértices e o conjunto de triângulos têm de ser o mesmo) --
bool mesmoConjunto(const MeshData& a, const MeshData& b, std::string& why) {
    if (a.indices.size() != b.indices.size()) {
        why = "total de índices diverge (" +
              std::to_string(a.indices.size()) + " vs " +
              std::to_string(b.indices.size()) + ")";
        return false;
    }
    using Bits = std::array<u8, sizeof(Vertex)>;
    auto canon = [](const MeshData& m,
                    std::map<Bits, u32>& ids, std::vector<u32>& out) {
        std::vector<Bits> uniq;
        for (const Vertex& v : m.vertices) {
            Bits bits;
            std::memcpy(bits.data(), &v, sizeof(Vertex));
            auto it = ids.find(bits);
            if (it == ids.end()) {
                const u32 id = static_cast<u32>(uniq.size());
                ids.emplace(bits, id);
                uniq.push_back(bits);
                out.push_back(id);
            } else {
                out.push_back(it->second);
            }
        }
        (void)uniq;
    };
    std::map<Bits, u32> idsA, idsB;
    std::vector<u32> ca, cb;
    canon(a, idsA, ca);
    canon(b, idsB, cb);
    // os CONJUNTOS de vértices (por bits) têm de bater
    if (idsA.size() != idsB.size()) {
        why = "conjuntos de vértices com tamanhos diferentes (" +
              std::to_string(idsA.size()) + " vs " +
              std::to_string(idsB.size()) + ")";
        return false;
    }
    for (const auto& kv : idsA) {
        if (idsB.find(kv.first) == idsB.end()) {
            why = "um vértice da origem não existe no ficheiro";
            return false;
        }
    }
    // os triângulos canónicos (ids ordenados dentro do triângulo) em
    // multiset — a ORDEM é livre, o CONJUNTO não
    auto tris = [](const std::vector<u32>& c) {
        std::vector<std::array<u32, 3>> t;
        for (size_t i = 0; i + 2 < c.size(); i += 3) {
            std::array<u32, 3> tr{c[i], c[i + 1], c[i + 2]};
            std::sort(tr.begin(), tr.end());
            t.push_back(tr);
        }
        std::sort(t.begin(), t.end());
        return t;
    };
    const auto ta = tris(ca);
    const auto tb = tris(cb);
    if (ta != tb) {
        why = "o conjunto de triângulos diverge (" +
              std::to_string(ta.size()) + " vs " +
              std::to_string(tb.size()) + ")";
        return false;
    }
    return true;
}

} // namespace

// ---- O CAMINHO DE PRODUÇÃO: importFile (FsStorage real) → v3 streaming ----

TEST(v3stream_roundtrip_igual_ao_caminho_legado) {
    const std::string dir = "/tmp/goni_v3s_rt_" + std::to_string(::getpid());
    rmRf(dir);
    ASSERT(mkDir(dir));
    // 2 nós com TRS (a translação TEM de vir baked) + 2 materiais
    const std::vector<PrimSpec> prims = {{100, 160, 0, false},
                                         {80, 120, 1, false}};
    const std::vector<NodeSpec> nodes = {{0, 3.0f, 4.0f, 5.0f},
                                         {1, -2.0f, 0.5f, 1.0f}};
    std::vector<u8> glb;
    ASSERT(buildGlb(prims, nodes, {"laca", "metal"}, {}, glb));
    const std::string src = dir + "/fonte.glb";
    FILE* f = std::fopen(src.c_str(), "wb");
    ASSERT(f != nullptr);
    std::fwrite(glb.data(), 1, glb.size(), f);
    std::fclose(f);
    std::printf("  [v3s-rt] glb=%zu B\n", glb.size());
    std::fflush(stdout);

    elog::init((dir + "/logs").c_str());
    FsStorage st((dir + "/proj").c_str());
    convert::Output out;
    convert::Stats stats;
    std::string err;
    const bool ok = convert::importFile(src, "fonte.glb", st, nullptr, out,
                                        stats, err, nullptr, nullptr);
    EXPECT(ok);
    if (!ok) {
        std::printf("  [v3s] err=%s\n", err.c_str());
        std::fflush(stdout);
    }
    EXPECT(out.meshes.size() == 1);
    EXPECT(fileHas(dir + "/logs/engine.log",
                   "gmesh: v3 blocos=") &&
           fileHas(dir + "/logs/engine.log", "verificado=1"));
    // o v3 lê pelo leitor de produção e monta (≤65535 verts)
    std::vector<u8> gmesh;
    EXPECT(st.readBytes(out.meshes[0], gmesh));
    MeshData got;
    EXPECT(readGMesh(gmesh.data(), gmesh.size(), got, err));
    // a REFERÊNCIA (o caminho de sempre, em memória) — o mesmo conjunto
    MeshData want;
    mergeReferencia(prims, nodes, want);
    std::string why;
    EXPECT(mesmoConjunto(want, got, why));
    if (!why.empty()) {
        std::printf("  [v3s] conjunto: %s\n", why.c_str());
        std::fflush(stdout);
    }
    elog::shutdown();
    rmRf(dir);
}

TEST(v3stream_materiais_agrupados_na_tabela) {
    const std::string dir = "/tmp/goni_v3s_mat_" + std::to_string(::getpid());
    rmRf(dir);
    ASSERT(mkDir(dir));
    // materiais A,B,A em ordem de uso — a tabela agrupa e a sequência
    // materialIndex fica NÃO-DECRESCENTE (o agrupamento por material)
    const std::vector<PrimSpec> prims = {{50, 60, 0, false},
                                         {40, 45, 1, false},
                                         {30, 36, 0, false}};
    const std::vector<NodeSpec> nodes = {{0, 0, 0, 0},
                                         {1, 1, 0, 0},
                                         {2, 2, 0, 0}};
    std::vector<u8> glb;
    ASSERT(buildGlb(prims, nodes, {"A", "B"}, {}, glb));
    const std::string src = dir + "/fonte.glb";
    FILE* f = std::fopen(src.c_str(), "wb");
    ASSERT(f != nullptr);
    std::fwrite(glb.data(), 1, glb.size(), f);
    std::fclose(f);

    elog::init((dir + "/logs").c_str());
    FsStorage st((dir + "/proj").c_str());
    convert::Output out;
    convert::Stats stats;
    std::string err;
    EXPECT(convert::importFile(src, "fonte.glb", st, nullptr, out, stats,
                               err, nullptr, nullptr));
    std::vector<u8> gmesh;
    EXPECT(st.readBytes(out.meshes[0], gmesh));
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> mats;
    EXPECT(readGMeshV3Meta(gmesh.data(), gmesh.size(), meta, blocks, mats,
                           err));
    EXPECT(meta.materialCount == 2);
    EXPECT(mats.size() == 2 && mats[0] == "A" && mats[1] == "B");
    u32 last = 0;
    bool monotonic = true;
    for (size_t i = 0; i < blocks.size(); ++i) {
        if (blocks[i].materialIndex < last) monotonic = false;
        last = blocks[i].materialIndex;
    }
    EXPECT(monotonic);
    elog::shutdown();
    rmRf(dir);
}

TEST(v3stream_70k_verts_parte_em_blocos) {
    const std::string dir = "/tmp/goni_v3s_70k_" + std::to_string(::getpid());
    rmRf(dir);
    ASSERT(mkDir(dir));
    // UMA primitiva com 70 000 vértices e índices u32 — o teto u16 do
    // .gmesh v1 MORREU: o corte em blocos é o contrato do 0.10-M
    const std::vector<PrimSpec> prims = {{70000, 90000, 0, true}};
    const std::vector<NodeSpec> nodes = {{0, 0, 0, 0}};
    std::vector<u8> glb;
    ASSERT(buildGlb(prims, nodes, {"gigante"}, {}, glb));
    const std::string src = dir + "/fonte.glb";
    FILE* f = std::fopen(src.c_str(), "wb");
    ASSERT(f != nullptr);
    std::fwrite(glb.data(), 1, glb.size(), f);
    std::fclose(f);

    elog::init((dir + "/logs").c_str());
    FsStorage st((dir + "/proj").c_str());
    convert::Output out;
    convert::Stats stats;
    std::string err;
    EXPECT(convert::importFile(src, "fonte.glb", st, nullptr, out, stats,
                               err, nullptr, nullptr));
    std::vector<u8> gmesh;
    EXPECT(st.readBytes(out.meshes[0], gmesh));
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> mats;
    EXPECT(readGMeshV3Meta(gmesh.data(), gmesh.size(), meta, blocks, mats,
                           err));
    EXPECT(meta.vertexCount >= 70000);   // 64-bit: a soma das pools cobre
                                         // TODOS os 70000 da origem (a
                                         // duplicação entre blocos é do
                                         // corte — o conjunto é o mesmo)
    EXPECT(meta.blockCount >= 2);        // 70000 > 65535 → cortou
    bool poolsOk = true;
    for (const GMeshV3Block& b : blocks) {
        if (b.vertexCount > 65535 || b.indexType != 0) poolsOk = false;
    }
    EXPECT(poolsOk);
    EXPECT(fileHas(dir + "/logs/engine.log", "verificado=1"));
    // o RUNTIME de hoje não monta >65535 — a MENSAGEM DO PASSO 3B: a causa
    // do «fail de 203 MB» registada com nome e números (o load inteiro é a
    // parede; a cura é o render por blocos; o FICHEIRO está correto)
    MeshData cannot;
    EXPECT(!readGMesh(gmesh.data(), gmesh.size(), cannot, err));
    EXPECT(err.find("memória insuficiente ao carregar mesh") !=
           std::string::npos);
    EXPECT(err.find("cura no PASSO 4: render por blocos") != std::string::npos);
    EXPECT(err.find("70000") != std::string::npos);       // os vértices
    EXPECT(err.find("O FICHEIRO ESTÁ CORRETO") != std::string::npos);
    // 0.10-M (PASSO 3B) — O ESPIÃO DE 192 B (readBytesAt + gmeshV3PeekMeta):
    // o guard cedo vê os MESMOS números SEM ler o ficheiro inteiro — a
    // recusa no ResourceManager sai por 192 B (a mensagem chega ao dono sem
    // incendiar a RAM; a 1ª versão do guard media 2007 MB para recusar)
    {
        std::vector<u8> peek;
        EXPECT(st.readBytesAt(out.meshes[0], 0,
                              kGHeaderBytes + kGmeshV3MetaBytes, peek));
        EXPECT(peek.size() == kGHeaderBytes + kGmeshV3MetaBytes);
        GMeshV3Meta pmeta;
        std::string perr;
        EXPECT(gmeshV3PeekMeta(peek.data(), peek.size(), pmeta, perr));
        EXPECT(pmeta.vertexCount >= 70000);   // o MESMO número do caminho
        EXPECT(pmeta.blockCount >= 2);        // inteiro
        // o guard do peek dispara a MESMA mensagem (uma só fonte de verdade)
        EXPECT(pmeta.vertexCount > 65535 ||
               gmeshV3LoadEstimateBytes(pmeta) > kMeshLoadBudgetBytes);
        EXPECT(gmeshV3LoadRefusalErr(pmeta).find(
                   "memória insuficiente ao carregar mesh") !=
               std::string::npos);
        // o range SÓ: além do fim = false honesto; len 0 = true vazio
        std::vector<u8> nada;
        EXPECT(!st.readBytesAt(out.meshes[0], gmesh.size() - 2, 8, nada));
        EXPECT(st.readBytesAt(out.meshes[0], 0, 0, nada) && nada.empty());
        // o peek de UM v1 (fixtures reais) NÃO parseia como v3 — o chamador
        // cai no caminho de sempre (o guard só decide o que É v3)
        std::vector<u8> v1;
        if (FsStorage fs64(FIXTURE_DIR); fs64.readBytes(
                "gmesh_v1_esfera.gmesh", v1) && v1.size() > 8) {
            GMeshV3Meta m1;
            std::string e1;
            EXPECT(!gmeshV3PeekMeta(v1.data(), v1.size(), m1, e1));
            EXPECT(e1.find("não é um .gmesh v3") != std::string::npos);
        }
        // e o ResourceManager RECUSA cedo: 192 B, SEM o ficheiro inteiro
        ResourceManager rm;
        rm.setStorage(&st);
        std::string rerr;
        const MeshData* r = rm.mesh(out.meshes[0], rerr);
        EXPECT(r == nullptr);
        EXPECT(rerr.find("memória insuficiente ao carregar mesh") !=
               std::string::npos);
        EXPECT(rerr.find("70000") != std::string::npos);
        EXPECT(fileHas(dir + "/logs/engine.log",
                       "RECUSADO antes da leitura"));
    }
    // 0.10-M (PASSO 3B) — as LINHAS DE TEMPO por fase no log do import (a
    // linha contrato «gmesh: fase=…» — os minutos do dono com dono)
    EXPECT(fileHas(dir + "/logs/engine.log", "gmesh: fase=parse ms="));
    EXPECT(fileHas(dir + "/logs/engine.log", "gmesh: fase=cut ms="));
    EXPECT(fileHas(dir + "/logs/engine.log", "gmesh: fase=assembly ms="));
    EXPECT(fileHas(dir + "/logs/engine.log", "gmesh: fase=verify ms="));
    EXPECT(fileHas(dir + "/logs/engine.log", "import: copia ms="));
    elog::shutdown();
    rmRf(dir);
}

// 0.10-M (PASSO 3B) — a ESTIMATURA PURA do load (o contrato aferível sem
// ficheiro: verts×sizeof(Vertex) + índices×2 + pele 20 B/vértice). O
// orçamento declarado (256 MB) é o que separa «carrega» de «recusa com a
// mensagem do PASSO 4».
TEST(v3stream_estimativa_de_load_e_orcamento) {
    // estático pequeno: 1000 verts + 3000 índices = 32000 + 6000 B
    GMeshV3Meta m1;
    m1.vertexCount = 1000;
    m1.indexCount = 3000;
    EXPECT(gmeshV3LoadEstimateBytes(m1) == 1000 * sizeof(Vertex) + 6000);
    // skinned (flags bit0): +20 B por vértice
    GMeshV3Meta m2 = m1;
    m2.flags = 1;
    EXPECT(gmeshV3LoadEstimateBytes(m2) ==
           1000 * (sizeof(Vertex) + 20) + 6000);
    // o caso do dono em Miniatura: 6,5 M verts estático ≈ 229 MB (a classe
    // do scene-213MB: ~5,2 M verts + 30 M índices = ~227 MB — acima não por
    // vértices (65535) mas o MESMO tipo de parede: o mesh único em RAM)
    GMeshV3Meta m3;
    m3.vertexCount = 65000;          // ≤65535: passa o teto de vértices…
    m3.indexCount = 150ull * 1000 * 1000;  // …mas 150 M de índices
    EXPECT(gmeshV3LoadEstimateBytes(m3) ==
           65000 * sizeof(Vertex) + 150ull * 1000 * 1000 * 2);
    EXPECT(gmeshV3LoadEstimateBytes(m3) > kMeshLoadBudgetBytes);
    // o orçamento é o número da casa (256 MB — o teto de range R-032)
    EXPECT(kMeshLoadBudgetBytes == 256ull * 1024 * 1024);
}

TEST(v3stream_cancelar_nao_deixa_estado) {
    const std::string dir = "/tmp/goni_v3s_can_" + std::to_string(::getpid());
    rmRf(dir);
    ASSERT(mkDir(dir));
    const std::vector<PrimSpec> prims = {{30000, 60000, 0, false}};
    const std::vector<NodeSpec> nodes = {{0, 0, 0, 0}};
    std::vector<u8> glb;
    ASSERT(buildGlb(prims, nodes, {"m"}, {}, glb));
    const std::string src = dir + "/fonte.glb";
    FILE* f = std::fopen(src.c_str(), "wb");
    ASSERT(f != nullptr);
    std::fwrite(glb.data(), 1, glb.size(), f);
    std::fclose(f);

    // o cancelamento DIRETO no conversor (a 1.ª chamada do progresso)
    elog::init((dir + "/logs").c_str());
    FsStorage st((dir + "/proj").c_str());
    st.makeDirs("assets");
    GltfModel model;
    std::vector<u8> json;
    std::string err;
    ASSERT(fileapi::readAll(src.c_str(), json));
    // o JSON chunk do GLB (o container é binário — o JSON vive depois do
    // header de 12 B + o header do chunk de 8 B)
    u32 jsonLen = 0;
    std::memcpy(&jsonLen, json.data() + 12, 4);
    ASSERT(jsonLen > 0 && 20ull + jsonLen <= json.size());
    Json doc;
    EXPECT(Json::parse(reinterpret_cast<const char*>(json.data() + 20),
                       jsonLen, doc));
    // o BIN chunk (o parse precisa do buffer 0 — o fixture é pequeno,
    // cabe em RAM; o PRODUÇÃO mapeia o ficheiro, aqui é só o parse)
    u32 binLenChunk = 0;
    std::memcpy(&binLenChunk, json.data() + 20 + jsonLen, 4);
    ASSERT(28ull + jsonLen + binLenChunk <= json.size());
    std::vector<u8> binChunk(json.data() + 20 + jsonLen + 8,
                             json.data() + 20 + jsonLen + 8 + binLenChunk);
    {
        const bool parsed =
            parseGltf(reinterpret_cast<const char*>(json.data() + 20),
                      jsonLen, binChunk, GltfBufferResolver{}, model, err,
                      nullptr, true);
        EXPECT_MSG(parsed, "o parse streaming do fixture falhou: %s",
                   err.c_str());
    }
    EXPECT(!model.primRefs.empty());
    unsigned long long mapLen = 0;
    u8* map = static_cast<u8*>(
        fileapi::mapFile64(src.c_str(), 0, glb.size(), &mapLen));
    ASSERT(map != nullptr);
    // o CONTRATO do conversor: `bin` é o buffer 0 com a base NO INÍCIO DO
    // CHUNK BIN (os offsets dos accessors são daí) — não o ficheiro inteiro
    const u64 binBase = 20ull + jsonLen + 8ull;
    convert::Output out;
    convert::Stats stats;
    V3StreamResult res;
    bool (*cb)(void*, u64, u64) = [](void*, u64, u64) { return false; };
    const bool ok =
        convertGltfToV3(model, doc, map + binBase, binLenChunk, glb.size(),
                        "fonte", st, out, stats, res, err, cb, nullptr);
    fileapi::unmapFile64(map, mapLen);
    EXPECT(!ok);
    EXPECT(stats.canceled);
    // SEM estado: nem o .gmesh, nem o temporário no .staging
    EXPECT(st.probe("assets/fonte.gmesh") == Presence::Absent);
    std::vector<std::string> staging;
    if (st.listDir(".staging", staging)) {
        EXPECT(staging.empty());
    }
    elog::shutdown();
    rmRf(dir);
}

TEST(v3stream_texturas_continuam_no_passe_delas) {
    const std::string dir = "/tmp/goni_v3s_tex_" + std::to_string(::getpid());
    rmRf(dir);
    ASSERT(mkDir(dir));
    // um PNG da casa embutido — o passe de TEXTURAS corre no caminho
    // streaming (o modelo não volta ao parse de sempre por causa dela)
    std::vector<u8> png;
    {
        FILE* pf = std::fopen(FIXTURE_DIR "/red16.png", "rb");
        ASSERT(pf != nullptr);
        u8 buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof(buf), pf)) > 0) {
            png.insert(png.end(), buf, buf + n);
        }
        std::fclose(pf);
    }
    ASSERT(png.size() > 8);
    const std::vector<PrimSpec> prims = {{24, 24, 0, false}};
    const std::vector<NodeSpec> nodes = {{0, 0, 0, 0}};
    std::vector<u8> glb;
    ASSERT(buildGlb(prims, nodes, {"texturizado"}, {std::string(
                                                       png.begin(),
                                                       png.end())},
                     glb));
    const std::string src = dir + "/fonte.glb";
    FILE* f = std::fopen(src.c_str(), "wb");
    ASSERT(f != nullptr);
    std::fwrite(glb.data(), 1, glb.size(), f);
    std::fclose(f);

    elog::init((dir + "/logs").c_str());
    FsStorage st((dir + "/proj").c_str());
    convert::Output out;
    convert::Stats stats;
    std::string err;
    EXPECT(convert::importFile(src, "fonte.glb", st, nullptr, out, stats,
                               err, nullptr, nullptr));
    EXPECT(out.meshes.size() == 1);
    EXPECT(out.textures.size() == 1);
    EXPECT(stats.textures == 1);
    EXPECT(fileHas(dir + "/logs/engine.log", "verificado=1"));
    elog::shutdown();
    rmRf(dir);
}

// ---- 0.10-M PASSO 3 · A REGRESSÃO DO C33 (FASE 12.8/12.9 vermelhas) ----
// O cenário EXATO do harness: FakeStorage (raiz /fake virtual — o
// makeDirs "aceita" e o fopen morre ENOENT) + glTF SEM nodes (o robo.glb:
// a casa legada escrevia os meshes crus — o streaming faz o MESMO com
// node=-1 identidade). O temp tem de cair na CASCATA (projeto → cache
// dir da app) e o import completar com verificado=1.
#include "FakeStorage.h"
#include <jni.h>
#include "platform/StorageBridge.h"

extern "C" void Java_vv_goni_VvActivity_nativeRegisterActivity(
        JNIEnv*, jclass, jobject activity, jstring origin);

namespace {
jobject kReproActivity = reinterpret_cast<jobject>(static_cast<intptr_t>(0xE101));
jclass  kReproCls      = reinterpret_cast<jclass>(static_cast<intptr_t>(0xE102));

void registersWithCacheDir(const std::string& dir) {
    g_jni.reset();
    while (vv::storage::pollResult()) {
    }
    Java_vv_goni_VvActivity_nativeRegisterActivity(
        g_jni.env, kReproCls, kReproActivity,
        g_jni.newString("test-v3stream"));
    g_jni.manager_result = true;
    fileapi::makeDirs(dir);
    g_jni.cache_dir = dir;
}

// o GLB do 12.8 (triângulo pos+norm+uv+idx — SEM nodes/scenes no JSON)
bool buildRoboGlb(std::vector<u8>& glb) {
    const f32 pos[9] = {0, 0, 0, 1, 0, 0, 0, 1, 0};
    const f32 nrm[9] = {0, 0, 1, 0, 0, 1, 0, 0, 1};
    const f32 uv[6] = {0, 0, 1, 0, 0, 1};
    const u16 idx[3] = {0, 1, 2};
    std::vector<u8> bin;
    auto pushF = [&bin](const f32* v, int n) {
        for (int i = 0; i < n; ++i) {
            const u32 b = *reinterpret_cast<const u32*>(&v[i]);
            bin.push_back((u8)(b & 0xFF));
            bin.push_back((u8)((b >> 8) & 0xFF));
            bin.push_back((u8)((b >> 16) & 0xFF));
            bin.push_back((u8)((b >> 24) & 0xFF));
        }
    };
    pushF(pos, 9);
    pushF(nrm, 9);
    pushF(uv, 6);
    for (int i = 0; i < 3; ++i) {
        bin.push_back((u8)(idx[i] & 0xFF));
        bin.push_back((u8)(idx[i] >> 8));
    }
    char j[900];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":%u}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
        "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36},"
        "{\"buffer\":0,\"byteOffset\":72,\"byteLength\":24},"
        "{\"buffer\":0,\"byteOffset\":96,\"byteLength\":6}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":1,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC2\"},"
        "{\"bufferView\":3,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":"
        "{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},"
        "\"indices\":3}]}]}",
        (u32)bin.size());
    std::string json = j;
    while (json.size() % 4 != 0) json += ' ';
    std::vector<u8> binPad = bin;
    while (binPad.size() % 4 != 0) binPad.push_back(0);
    auto u32push = [&glb](u32 v) {
        glb.push_back((u8)(v & 0xFF));
        glb.push_back((u8)((v >> 8) & 0xFF));
        glb.push_back((u8)((v >> 16) & 0xFF));
        glb.push_back((u8)((v >> 24) & 0xFF));
    };
    u32push(0x46546C67u);
    u32push(2);
    u32push(12 + 8 + (u32)json.size() + 8 + (u32)binPad.size());
    u32push((u32)json.size());
    u32push(0x4E4F534Au);
    glb.insert(glb.end(), json.begin(), json.end());
    u32push((u32)binPad.size());
    u32push(0x004E4942u);
    glb.insert(glb.end(), binPad.begin(), binPad.end());
    return true;
}
} // namespace

TEST(v3stream_c33_fakestorage_cascata_e_sem_nodes) {
    const std::string dir = "/tmp/goni_v3s_c33_" + std::to_string(::getpid());
    rmRf(dir);
    ASSERT(mkDir(dir));
    const std::string cache = dir + "/appcache";
    registersWithCacheDir(cache);
    elog::init((dir + "/logs").c_str());
    std::vector<u8> glb;
    ASSERT(buildRoboGlb(glb));
    const std::string src = dir + "/robo.glb";
    FILE* f = std::fopen(src.c_str(), "wb");
    ASSERT(f != nullptr);
    std::fwrite(glb.data(), 1, glb.size(), f);
    std::fclose(f);

    FakeStorage st;   // raiz /fake VIRTUAL — a condição exata do harness
    convert::Output out;
    convert::Stats stats;
    std::string err;
    const bool ok = convert::importFile(src, "robo.glb", st, nullptr, out,
                                        stats, err, nullptr, nullptr);
    if (!ok) {
        std::printf("  [c33] err=%s\n", err.c_str());
        std::fflush(stdout);
    }
    EXPECT(ok);
    EXPECT(out.meshes.size() == 1);
    EXPECT(out.meshes[0] == "assets/robo.gmesh");
    // o temp caiu no CACHE DIR (o passo 1 da cascata morre ENOENT na
    // raiz virtual — o log prova a queda honesta)
    EXPECT(fileHas(dir + "/logs/engine.log",
                   "a tentar o cache dir da app"));
    // o round-trip v3 completo com a verificação bit a bit
    EXPECT(fileHas(dir + "/logs/engine.log", "verificado=1"));
    // o mesh convertido ABRE pelo leitor de produção: 1 triângulo
    std::vector<u8> gmesh;
    EXPECT(st.readBytes(out.meshes[0], gmesh));
    MeshData got;
    EXPECT(readGMesh(gmesh.data(), gmesh.size(), got, err));
    EXPECT(got.vertices.size() == 3);
    EXPECT(got.indices.size() == 3);
    elog::shutdown();
    rmRf(dir);
}

// ---- M3 · O POOL NUNCA FICA A 0 (single-core cai para 1: sequencial,
// mais lento, CORRETO — a mutação "pool a 0 sem fallback" fica vermelha)
TEST(v3stream_worker_count_nunca_zero) {
    EXPECT(v3StreamWorkerCount(0) == 1);   // desconhecido → 1
    EXPECT(v3StreamWorkerCount(1) == 1);   // single-core → 1
    EXPECT(v3StreamWorkerCount(2) == 1);   // núcleos−1
    EXPECT(v3StreamWorkerCount(8) == 7);   // núcleos−1
    EXPECT(v3StreamWorkerCount(16) == 15);
}

// ---- M2 · O OFFSET DO mmap NÃO ALINHADO (o Android arm64 exige página):
// o mapFile64 alinha o offset PARA BAIXO internamente e devolve base+within
// — qualquer offset 64-bit lê os bytes EXATOS do ficheiro. A mutação "o
// alinhamento morre" faz o mmap devolver EINVAL → vermelho.
TEST(mmap64_offset_nao_alinhado_le_bytes_exatos) {
    const std::string path =
        "/tmp/goni_v3_mmap_off_" + std::to_string(::getpid()) + ".bin";
    constexpr size_t kSize = 9 * 1024 + 123;   // ~3 páginas + resto
    {
        std::vector<u8> f(kSize);
        for (size_t i = 0; i < f.size(); ++i) {
            f[i] = static_cast<u8>((i * 7 + 13) & 0xFF);
        }
        FILE* fp = std::fopen(path.c_str(), "wb");
        ASSERT(fp != nullptr);
        std::fwrite(f.data(), 1, f.size(), fp);
        std::fclose(fp);
    }
    struct Probe {
        u64 off, len;
    };
    const Probe probes[] = {
        {4097, 4000},    // dentro da 2.ª página (não alinhado)
        {8191, 2},       // 1 byte antes da fronteira de página
        {8192, 100},     // exatamente na fronteira
        {511, 4097},     // atravessa uma fronteira inteira
    };
    for (const Probe& pr : probes) {
        unsigned long long mapped = 0;
        void* p = fileapi::mapFile64(path.c_str(), pr.off, pr.len, &mapped);
        ASSERT(p != nullptr);
        // os bytes têm de ser EXATAMENTE os do ficheiro [off, off+len)
        const u8* m = static_cast<const u8*>(p);
        bool exact = true;
        for (u64 i = 0; i < pr.len && exact; ++i) {
            const u8 want = static_cast<u8>(((pr.off + i) * 7 + 13) & 0xFF);
            exact = m[i] == want;
        }
        EXPECT(exact);
        fileapi::unmapFile64(p, mapped);
    }
    std::remove(path.c_str());
}
