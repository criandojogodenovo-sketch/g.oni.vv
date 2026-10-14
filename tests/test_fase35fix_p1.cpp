// tests/test_fase35fix_p1.cpp — 0.11-T-FIX (PERNA 1) — OS PINS DO CONTORNO.
//
// A campanha: docs/RELATORIO-0.11-T-FIX-ABERTURA.md (a corrupção de heap
// do corte paralelo do streaming — o crash §4.1 do RELATORIO-0.11-T-3.5A).
// Os 6 pins da Perna 1:
//   1. fix_p1_teto_workers      — o teto de 4 (o contorno 1.1)
//   2. fix_p1_teto_workers_min  — o piso (1 e nunca 0)
//   3. fix_p1_onprogress_seguro — o callback de progresso com 8 workers
//                                  forçados × 1000 imports sem corrupção
//                                  (o mutex da 1.2)
//   4. fix_p1_madvise_so_mmap   — o dropRange NUNCA corre em heap; corre
//                                  em mmap de ficheiro e de memfd (a 1.3)
//   5. fix_p1_compat_011s       — .goni e .gmesh da 0.11-S byte a byte
//   6. fix_p1_streaming_ok      — o data-uri (o teste que crashava) 10×
// Sentinela R-046: regress_streaming_paralelo (test_sentinels.cpp).
// Mutações: M-FIX1a (teto), M-FIX1b (madvise), M-FIX1c (mutex).
// memfd_create vive na glibc (sys/mman.h com _GNU_SOURCE — sem
// redeclaração própria: a glibc declara com noexcept e a duplicata
// conflictua; o mesmo princípio do FakeSafIo.h)
#ifndef _GNU_SOURCE
#define _GNU_SOURCE 1
#endif
#include "TestFramework.h"

#include "assets/AssetConverter.h"
#include "assets/GOwnFormats.h"
#include "assets/GmeshV3Stream.h"   // o v3StreamWorkerCount (os pins 1/2)
#include "components/MeshRenderer.h"
#include "core/FsStorage.h"
#include "core/SceneSerializer.h"
#include "core/Scene.h"
#include "platform/EngineLog.h"
#include "platform/FileApi.h"

#include <cerrno>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include <sys/mman.h>   // memfd_create (a realidade do provider SAF)

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

// ---- o GERADOR do .gltf com data: URI (determinístico; N prims) -----------
// A vítima do crash (safstream_data_uri_sem_ficheiro_tambem_streama) é o
// caminho HEAP (o buffer 0 embutido no JSON — binDroppable=false). O
// gerador produz esse MESMO caminho com N primitivas (N tarefas = corte
// com workers a competir de verdade).

struct UriPrim {
    u32 verts;
    u32 tris;
};

static void uriVertex(u64 i, f32 out[3]) {
    const u64 rx = i % 997, ry = i % 991, rz = i % 983;
    out[0] = -1.0f + 2.0f * static_cast<f32>(rx) / 996.0f;
    out[1] = -1.0f + 2.0f * static_cast<f32>(ry) / 990.0f;
    out[2] = -1.0f + 2.0f * static_cast<f32>(rz) / 982.0f;
}

bool buildDataUriGltf(const std::vector<UriPrim>& prims,
                      const std::string& path) {
    std::vector<u8> bin;
    struct V { u64 off, len; };
    std::vector<V> views;
    auto align4 = [&bin]() {
        while (bin.size() % 4 != 0) bin.push_back(0);
    };
    for (const UriPrim& p : prims) {
        align4();
        views.push_back({bin.size(), u64(p.verts) * 12});
        for (u32 i = 0; i < p.verts; ++i) {
            f32 v[3];
            uriVertex(i, v);
            u32 raw;
            for (int c = 0; c < 3; ++c) {
                std::memcpy(&raw, &v[c], 4);
                bin.push_back(u8(raw & 0xFF)); bin.push_back(u8(raw >> 8));
                bin.push_back(u8(raw >> 16)); bin.push_back(u8(raw >> 24));
            }
        }
        align4();
        views.push_back({bin.size(), u64(p.verts) * 12});
        for (u32 i = 0; i < p.verts; ++i) {
            const f32 n[3] = {f32(i % 97) / 48.0f - 1.0f,
                              f32(i % 89) / 44.0f - 1.0f,
                              f32(i % 83) / 41.0f - 1.0f};
            u32 raw;
            for (int c = 0; c < 3; ++c) {
                std::memcpy(&raw, &n[c], 4);
                bin.push_back(u8(raw & 0xFF)); bin.push_back(u8(raw >> 8));
                bin.push_back(u8(raw >> 16)); bin.push_back(u8(raw >> 24));
            }
        }
        align4();
        views.push_back({bin.size(), u64(p.verts) * 8});
        for (u32 i = 0; i < p.verts; ++i) {
            const f32 t[2] = {f32(i % 64) / 63.0f, f32(i % 32) / 31.0f};
            u32 raw;
            for (int c = 0; c < 2; ++c) {
                std::memcpy(&raw, &t[c], 4);
                bin.push_back(u8(raw & 0xFF)); bin.push_back(u8(raw >> 8));
                bin.push_back(u8(raw >> 16)); bin.push_back(u8(raw >> 24));
            }
        }
    }
    for (const UriPrim& p : prims) {
        align4();
        views.push_back({bin.size(), u64(p.tris) * 3 * 2});
        for (u32 ti = 0; ti < p.tris; ++ti) {
            for (int c = 0; c < 3; ++c) {
                const u16 g = u16((3 * ti + u32(c)) % p.verts);
                bin.push_back(u8(g & 0xFF));
                bin.push_back(u8(g >> 8));
            }
        }
    }
    static const char kTbl[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string b64;
    b64.reserve(((bin.size() + 2) / 3) * 4);
    for (size_t i = 0; i < bin.size(); i += 3) {
        const u32 n0 = bin[i];
        const u32 n1 = i + 1 < bin.size() ? bin[i + 1] : 0;
        const u32 n2 = i + 2 < bin.size() ? bin[i + 2] : 0;
        const u32 v = (n0 << 16) | (n1 << 8) | n2;
        b64.push_back(kTbl[(v >> 18) & 63]);
        b64.push_back(kTbl[(v >> 12) & 63]);
        b64.push_back(i + 1 < bin.size() ? kTbl[(v >> 6) & 63] : '=');
        b64.push_back(i + 2 < bin.size() ? kTbl[v & 63] : '=');
    }
    std::string json =
        "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,"
        "\"scenes\":[{\"nodes\":[0]}],\"nodes\":[{\"mesh\":0}],"
        "\"meshes\":[{\"primitives\":[";
    for (size_t p = 0; p < prims.size(); ++p) {
        if (p != 0) json += ",";
        json += "{\"attributes\":{\"POSITION\":" +
                std::to_string(p * 3) + ",\"NORMAL\":" +
                std::to_string(p * 3 + 1) + ",\"TEXCOORD_0\":" +
                std::to_string(p * 3 + 2) + "},\"indices\":" +
                std::to_string(prims.size() * 3 + p) + ",\"mode\":4}";
    }
    json += "]}],\"accessors\":[";
    for (size_t p = 0; p < prims.size(); ++p) {
        json += std::string(p != 0 ? "," : "") +
                "{\"bufferView\":" + std::to_string(p * 3) +
                ",\"componentType\":5126,\"count\":" +
                std::to_string(prims[p].verts) + ",\"type\":\"VEC3\"}," +
                "{\"bufferView\":" + std::to_string(p * 3 + 1) +
                ",\"componentType\":5126,\"count\":" +
                std::to_string(prims[p].verts) + ",\"type\":\"VEC3\"}," +
                "{\"bufferView\":" + std::to_string(p * 3 + 2) +
                ",\"componentType\":5126,\"count\":" +
                std::to_string(prims[p].verts) + ",\"type\":\"VEC2\"}";
    }
    for (size_t p = 0; p < prims.size(); ++p) {
        // a vírgula é SEMPRE: o array de accessors já tem os 3 por prim
        json += ",{\"bufferView\":" +
                std::to_string(prims.size() * 3 + p) +
                ",\"componentType\":5123,\"count\":" +
                std::to_string(prims[p].tris * 3) +
                ",\"type\":\"SCALAR\"}";
    }
    json += "],\"bufferViews\":[";
    for (size_t v = 0; v < views.size(); ++v) {
        json += std::string(v != 0 ? "," : "") +
                "{\"buffer\":0,\"byteOffset\":" +
                std::to_string(views[v].off) + ",\"byteLength\":" +
                std::to_string(views[v].len) + "}";
    }
    json += "],\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,";
    json += b64;
    json += "\"}]}";
    FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return false;
    const bool ok =
        std::fwrite(json.data(), 1, json.size(), f) == json.size();
    std::fclose(f);
    return ok;
}

// o callback ADVERSÁRIO do pin 3: deteta SOBREPOSIÇÃO — se duas invocações
// alguma vez correm em paralelo (o mutex removido), ambas veem inFlight==1
// e a sobreposição fica registrada; o total não-atómico acumula perdas
// (a aritmética do detector). O append ao std::string partilhado é o
// alvo da corrupção de heap (a hipótese 1 da ABERTURA). SEM sobreposição
// (o mutex da 1.2), tudo é exato.
struct ProgStress {
    std::string log;
    std::atomic<u64> calls{0};       // atómico — o contador NÃO é o alvo
    u64 total = 0;                   // NÃO atómico — o alvo da corrida
    u32 inFlight = 0;                // NÃO atómico — o detector de sobreposição
    u64 overlaps = 0;
};
bool progStressCb(void* user, u64 done, u64 total) {
    auto* s = static_cast<ProgStress*>(user);
    if (s->inFlight != 0) ++s->overlaps;   // duas entradas em paralelo
    s->inFlight = 1;
    for (int i = 0; i < 8; ++i) s->total += 1;   // perde sob corrida
    s->log.push_back('x');
    if (s->log.size() >= 512) {
        if (s->log.find_first_not_of('x') != std::string::npos) {
            ++s->overlaps;   // o string partilhado corrompeu
        }
        s->log.clear();
    }
    std::this_thread::sleep_for(std::chrono::microseconds(200));
    s->inFlight = 0;
    s->calls.fetch_add(1);
    (void)done;
    (void)total;
    return true;
}

bool importOnce(const std::string& src, const std::string& projDir,
                ProgStress* stress) {
    rmRf(projDir);
    if (!mkDir(projDir)) return false;
    elog::init((projDir + "/logs").c_str());
    FsStorage st((projDir + "/proj").c_str());
    convert::Output out;
    convert::Stats stats;
    std::string err;
    const bool ok = convert::importFile(src, "modelo.gltf", st, nullptr, out,
                                        stats, err,
                                        stress ? &progStressCb : nullptr,
                                        stress);
    const bool verified = fileHas(projDir + "/logs/engine.log",
                                  "verificado=1");
    elog::shutdown();
    rmRf(projDir);
    if (!(ok && verified && out.meshes.size() == 1)) {
        std::printf("  [import] ok=%d verified=%d meshes=%zu err=%.240s\n",
                    ok ? 1 : 0, verified ? 1 : 0, out.meshes.size(),
                    err.c_str());
        std::fflush(stdout);
    }
    return ok && verified && out.meshes.size() == 1;
}

} // namespace

// ---- 1 · O TETO (o contorno 1.1: clamp(cores−1, 1, 4)) ---------------------
TEST(fix_p1_teto_workers) {
    EXPECT(v3StreamWorkerCount(8) == 4);    // o C33 (8 cores) → 4, não 7
    EXPECT(v3StreamWorkerCount(9) == 4);
    EXPECT(v3StreamWorkerCount(16) == 4);   // o teto NÃO cresce
    EXPECT(v3StreamWorkerCount(1024) == 4);
    EXPECT(v3StreamWorkerCount(3) == 2);    // abaixo do teto: núcleos−1
    EXPECT(v3StreamWorkerCount(5) == 4);    // 5−1 = 4: exatamente no teto
}

// ---- 2 · O PISO (nunca 0; o contrato M3 do PASSO 3 continua) ---------------
TEST(fix_p1_teto_workers_min) {
    EXPECT(v3StreamWorkerCount(2) == 1);   // 2 cores → 1 worker
    EXPECT(v3StreamWorkerCount(1) == 1);   // single-core → sequencial
    EXPECT(v3StreamWorkerCount(0) == 1);   // desconhecido → 1
}

// ---- 3 · O CALLBACK SEGURO (o mutex da 1.2 sob 8 workers forçados) ---------
TEST(fix_p1_onprogress_seguro) {
    const std::string dir =
        "/tmp/goni_fixp1_prog_" + std::to_string(::getpid());
    rmRf(dir);
    ASSERT(mkDir(dir));
    // 3 primitivas × 17000 tris: CADA tarefa cruza o kProgressStride
    // (16384) — os WORKERS disparam o callback de verdade (o fixture
    // pequeno nunca chegava lá: só as chamadas single-thread do
    // assembly/verify corriam — a lição da 1.ª versão do pin)
    ASSERT(setenv("GONI_STREAM_WORKERS", "8", 1) == 0);
    const std::vector<UriPrim> prims = {{2400, 17000}, {2400, 17000},
                                        {2400, 17000}};
    const std::string src = dir + "/stress.gltf";
    ASSERT(buildDataUriGltf(prims, src));
    ProgStress stress;
    bool all = true;
    for (int i = 0; i < 1000 && all; ++i) {
        if (!importOnce(src, dir + "/proj", &stress)) {
            std::printf("  [prog] iteração %d falhou\n", i);
            std::fflush(stdout);
            all = false;
        }
    }
    unsetenv("GONI_STREAM_WORKERS");   // higiene — os outros pins não herdam
    EXPECT(all);
    EXPECT(stress.calls.load() > 0);   // o callback correu DE VERDADE
    EXPECT(stress.overlaps == 0);      // ZERO sobreposições (o mutex manda)
    EXPECT(stress.total == stress.calls.load() * 8);   // zero perdas
    rmRf(dir);
}

// ---- 4 · O MADVISE SÓ EM MMAP (a defesa por construção da 1.3) -------------
TEST(fix_p1_madvise_so_mmap) {
    // (a) HEAP: o rangeIsFileBacked diz NÃO e o dropRange é NO-OP — o
    // conteúdo fica EXATO (um madvise DONTNEED em anónima ZEROEAVA as
    // páginas — o crash F3 §2e)
    std::vector<u8> heap(3 * 4096, 0);
    for (size_t i = 0; i < heap.size(); ++i) {
        heap[i] = static_cast<u8>((i * 31 + 7) & 0xFF);
    }
    std::vector<u8> heapCopia = heap;
    EXPECT(!fileapi::rangeIsFileBacked(heap.data(), heap.size()));
    fileapi::dropRange(heap.data(), 0, heap.size());
    EXPECT(heap == heapCopia);   // byte a byte — o madvise NÃO correu
    // (b) MMAP de ficheiro REAL: file-backed; o drop corre e o conteúdo
    // re-lê do ficheiro (o MESMO byte a byte — a higiene legítima)
    const std::string path =
        "/tmp/goni_fixp1_drop_" + std::to_string(::getpid()) + ".bin";
    constexpr size_t kSize = 64 * 1024;
    {
        std::vector<u8> f(kSize);
        for (size_t i = 0; i < f.size(); ++i) {
            f[i] = static_cast<u8>((i * 13 + 5) & 0xFF);
        }
        FILE* fp = std::fopen(path.c_str(), "wb");
        ASSERT(fp != nullptr);
        std::fwrite(f.data(), 1, f.size(), fp);
        std::fclose(fp);
    }
    {
        unsigned long long mapped = 0;
        u8* m = static_cast<u8*>(
            fileapi::mapFile64(path.c_str(), 0, kSize, &mapped));
        ASSERT(m != nullptr);
        EXPECT(fileapi::rangeIsFileBacked(m, kSize));
        for (size_t i = 0; i < kSize; ++i) {
            m[i] = static_cast<u8>(m[i] | 0);   // toca as páginas (fault in)
        }
        fileapi::dropRange(m, 0, kSize);
        bool igual = true;
        for (size_t i = 0; i < kSize && igual; ++i) {
            igual = m[i] == static_cast<u8>((i * 13 + 5) & 0xFF);
        }
        EXPECT(igual);   // re-lê do ficheiro — bit a bit
        fileapi::unmapFile64(m, mapped);
    }
    // (c) MEMFD (a realidade do FakeSafIo/provider SAF): file-backed — o
    // drop é legítimo AQUI também
    {
        const int fd = ::memfd_create("vv-fixp1-pin", 0);
        ASSERT(fd >= 0);
        ASSERT(::ftruncate(fd, static_cast<off_t>(kSize)) == 0);
        unsigned long long mapped = 0;
        u8* m = static_cast<u8*>(
            fileapi::mapFd64(fd, 0, kSize, &mapped));
        if (m != nullptr) {
            EXPECT(fileapi::rangeIsFileBacked(m, kSize));
            fileapi::unmapFile64(m, mapped);
        }
        // (o mapFd64 recusar num sandbox exótico não é falha do pin — o
        // ASSERT acima é a existência do memfd)
        ::close(fd);
    }
    unlink(path.c_str());
}

// ---- 5 · O COMPAT 0.11-S (.goni e .gmesh byte a byte) ----------------------
TEST(fix_p1_compat_011s) {
    // (a) o .goni da forma 0.11-S (sem "lodDist", sem nada novo): o
    // round-trip load→dump é BYTE A BYTE e o MeshRenderer abre com os
    // defaults de sempre (o fix não tocou em formato nenhum)
    const std::string goni011sRaw =
        "{\"version\":1,\"tics\":[{\"id\":0,\"name\":\"Corpo\",\"parent\":-1,"
        "\"components\":[{\"type\":\"Transform3D\",\"pos\":[1,2,3],"
        "\"rot\":[0,0,0,1],\"scale\":[1,1,1]},{\"type\":\"MeshRenderer\","
        "\"mesh\":\"cube\",\"tint\":[1,0.5,0.25]}]}]}";
    Scene sRaw;
    SceneSerializer::LoadCtx ctx;
    EXPECT(SceneSerializer::loadText(sRaw, goni011sRaw, ctx));
    const std::string goni011s = SceneSerializer::dump(sRaw);
    EXPECT(std::strstr(goni011s.c_str(), "lodDist") == nullptr);
    Scene s;
    EXPECT(SceneSerializer::loadText(s, goni011s, ctx));
    const Tic* t = s.get(s.find("Corpo"));
    ASSERT(t != nullptr);
    const MeshRenderer* mr = t->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    EXPECT(mr->lodDistance == 0.0f);
    EXPECT(test::nearEqF(mr->tint[1], 0.5f));
    EXPECT(SceneSerializer::dump(s) == goni011s);   // byte a byte
    // (b) o .gmesh: write→read→write é DETERMINÍSTICO (o escritor v3/v2
    // de sempre — o fix não mudou um byte do formato)
    MeshData grid;
    grid.vertices.push_back(Vertex{Vec3{0, 0, 0}, Vec3{0, 1, 0},
                                   Vec2{0, 0}});
    grid.vertices.push_back(Vertex{Vec3{1, 0, 0}, Vec3{0, 1, 0},
                                   Vec2{1, 0}});
    grid.vertices.push_back(Vertex{Vec3{0, 1, 0}, Vec3{0, 1, 0},
                                   Vec2{0, 1}});
    grid.indices = {0, 1, 2};
    std::string err;
    std::vector<u8> bytes1;
    EXPECT(writeGMesh(grid, bytes1, err));
    MeshData lido;
    EXPECT(readGMesh(bytes1.data(), bytes1.size(), lido, err));
    EXPECT(lido.vertices.size() == 3);
    EXPECT(lido.indices.size() == 3);
    std::vector<u8> bytes2;
    EXPECT(writeGMesh(lido, bytes2, err));
    EXPECT(bytes1 == bytes2);   // byte a byte
}

// ---- 6 · O STREAMING 10× (o cenário que crashava, sem crash) ---------------
TEST(fix_p1_streaming_ok) {
    const std::string dir =
        "/tmp/goni_fixp1_str_" + std::to_string(::getpid());
    rmRf(dir);
    ASSERT(mkDir(dir));
    // o MESMO caminho da vítima (data: URI — o bin HEAP) com 2 primitivas
    const std::vector<UriPrim> prims = {{8000, 5000}, {8000, 5000}};
    const std::string src = dir + "/embutido.gltf";
    ASSERT(buildDataUriGltf(prims, src));
    for (int i = 0; i < 10; ++i) {
        if (!importOnce(src, dir + "/proj", nullptr)) {
            std::printf("  [streaming] iteração %d falhou\n", i);
            std::fflush(stdout);
            EXPECT(false);
            break;
        }
    }
    rmRf(dir);
}
