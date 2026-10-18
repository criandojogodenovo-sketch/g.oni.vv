// tests/bench_ram_curve.cpp — 0.12-A (A-4): A CURVA 50k…12.3M no hospedeiro
// (a investigação RAM vs GPU do salto anormal do 3.º edifício).
//
// ALVO OPCIONAL (não entra no test_core nem no APK): constrói-se SÓ com
//   cmake -DGONI_BENCH_RAM=ON ... && make bench_ram && ./bench_ram
// O QUE MEDE (a parte CPU do caminho, sem GLES real — o rasterizador é o
// stub honesto da casa com o framebuffer OFF, i.e. glDrawElements só conta):
//   • open (tabela), draw por frame (cull + sort + batching + LRU), dc/bl,
//     ram/vram residentes, EVICTIONS por frame (o THRASH da cache é o
//     mecanismo RAM visível no hospedeiro) e o RSS/pico do processo.
// O QUE NÃO MEDE: o custo da GPU (fragment/vertex shading, bandwidth) —
// esse é do device; o veredito final usa o sampler bench_ram: (o main com
// -DGONI_BENCH_RAM) + o logcat do dono. A análise vive no
// docs/RELATORIO-0.12-A-INVEST-RAM.md.
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "assets/GOwnFormats.h"
#include "core/FsStorage.h"
#include "math/Math.h"
#include "render/BlockMesh.h"
#include "render/Camera.h"
#include "render/Mesh.h"
#include "render/Renderer.h"
#include <GLES3/gl3.h>

using namespace vv;

namespace {

double nowMs() {
    struct timeval tv;
    ::gettimeofday(&tv, nullptr);
    return double(tv.tv_sec) * 1000.0 + double(tv.tv_usec) / 1000.0;
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

// o gerador do test_blockmesh (makeBigBlocks) — o MESMO formato, copiado
// para o bench não depender de um TU de teste (o bloco tem vertsPerBlock
// verts e ~vertsPerBlock-2 tris: 1 tri por vértice, a malha da casa)
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
        b.use32 = false;
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

struct Row {
    u32 tri = 0;
    u32 blocks = 0;
    double openMs = 0.0;
    double firstFrameMs = 0.0;   // o cold frame (materializa até ao teto)
    double steadyMs = 0.0;       // a média dos frames seguintes (ORBIT)
    double staticMs = 0.0;       // a média com a câmara PARADA (o
                                 // discriminador do mecanismo)
    u32 evictPerFrame = 0;       // o THRASH em orbit
    u32 evictStaticPerFrame = 0; // o THRASH parado
    u32 dc = 0, blDrawn = 0, visible = 0;
    u64 vramBytes = 0, ramBytes = 0;
    double rssPeakMb = 0.0;
};

// corre UM tamanho: escreve o .gmesh em disco (a RAM do gerador morre),
// abre pela tabela, desenha FAR (tudo visível) com os orçamentos DE
// PRODUÇÃO (64+192 MB) — o pior caso honesto da cache
bool runSize(u32 tris, Row& out) {
    constexpr int kVertsPerBlock = 600;
    constexpr int kTrisPerBlock = kVertsPerBlock - 2;   // 1 tri por ~1 vert
    const int nBlocks = static_cast<int>((tris + kTrisPerBlock - 1) /
                                         kTrisPerBlock);
    out.blocks = static_cast<u32>(nBlocks);

    const std::string dir = "/tmp/goni_bench_ram_" + std::to_string(::getpid());
    ::mkdir(dir.c_str(), 0755);
    {
        std::vector<u8> gmesh;
        std::string err;
        if (!makeBigBlocks(nBlocks, kVertsPerBlock, gmesh, err)) {
            std::printf("  ] gerador falhou em %u tris: %s\n", tris,
                        err.c_str());
            return false;
        }
        FsStorage stW(dir);
        const bool okW = stW.writeBytes("assets/curva.gmesh", gmesh.data(),
                                        gmesh.size());
        const u64 genRss = vmRssKb();
        std::vector<u8>().swap(gmesh);   // a RAM do gerador morre
        if (!okW) {
            return false;
        }
        out.rssPeakMb = double(genRss) / 1024.0;   // referência do gerador
    }

    FsStorage st(dir);
    BlockMesh bm;
    std::string err;
    const double t0 = nowMs();
    if (!bm.open(st, "assets/curva.gmesh", err)) {
        std::printf("  ] open falhou em %u tris: %s\n", tris, err.c_str());
        return false;
    }
    out.openMs = nowMs() - t0;

    // os ORÇAMENTOS DE PRODUÇÃO (a casa declara 64+192 MB — kBlockCache*)
    bm.setBudgets(64ull * 1024 * 1024, 192ull * 1024 * 1024);

    Renderer r;
    if (!r.init()) {
        return false;
    }
    Camera cam;
    cam.fovY = 1.0472f;
    // o ENQUADRAMENTO pelo AABB do meta: o modelo INTEIRO no frustum (o
    // cenário do dono — os edifícios vistos de fora; a curva escala com o
    // modelo e não com uma janela fixa)
    const Vec3 mn = bm.meta().aabbMin, mx = bm.meta().aabbMax;
    cam.target = Vec3{(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f,
                      (mn.z + mx.z) * 0.5f};
    const f32 ex = mx.x - mn.x, ey = mx.y - mn.y, ez = mx.z - mn.z;
    const f32 radius = 0.5f * std::sqrt(ex * ex + ey * ey + ez * ez);
    cam.dist = radius * 2.2f + 1.0f;
    cam.setClips(0.1f, radius * 8.0f + 100.0f);   // o far TEM de cobrir o
                                                  // modelo inteiro
    glstub::fb::enabled = false;   // o rasterizador NÃO rasteriza (só conta)

    constexpr int kFrames = 12;
    double sum = 0.0;
    u32 evict0 = 0;
    for (int f = 0; f < kFrames; ++f) {
        cam.yaw = 0.6f * std::sin(f * 0.2f);
        cam.pitch = 0.35f * std::cos(f * 0.15f);
        const Mat4 vp = Mat4::mul(cam.proj(1.0f), cam.view());
        const double a = nowMs();
        BlockMesh::Stats stt;
        bm.draw(r, Mat4::identity(), vp, nullptr, nullptr, nullptr, 0, stt);
        const double ms = nowMs() - a;
        if (f == 0) {
            out.firstFrameMs = ms;
            evict0 = stt.evictedTotal;
        } else {
            sum += ms;
            out.blDrawn = stt.drawn;
            out.visible = stt.visible;
            out.vramBytes = stt.vramBytes;
            out.ramBytes = stt.ramBytes;
        }
        const u64 rss = vmRssKb();
        if (double(rss) / 1024.0 > out.rssPeakMb) {
            out.rssPeakMb = double(rss) / 1024.0;
        }
    }
    out.steadyMs = sum / double(kFrames - 1);
    out.evictPerFrame =
        (bm.lastStats().evictedTotal - evict0) / u32(kFrames - 1);
    out.tri = tris;

    // o DISCRIMINADOR: a câmara ESTÁTICA (sem orbit) — com o conjunto
    // visível FIXO, a LRU estabiliza (o thrash era do MUNDO que muda) ou
    // continua (o pedido de materializar TODO o visível cicliza a cache
    // mesmo parado)? O orçamento é o veredito do mecanismo.
    cam.yaw = 0.0f;
    cam.pitch = 0.0f;
    const u32 evictStatic0 = bm.lastStats().evictedTotal;
    double sumS = 0.0;
    for (int f = 0; f < 5; ++f) {
        const Mat4 vp = Mat4::mul(cam.proj(1.0f), cam.view());
        const double a = nowMs();
        BlockMesh::Stats stt;
        bm.draw(r, Mat4::identity(), vp, nullptr, nullptr, nullptr, 0, stt);
        sumS += nowMs() - a;
        if (double(vmRssKb()) / 1024.0 > out.rssPeakMb) {
            out.rssPeakMb = double(vmRssKb()) / 1024.0;
        }
    }
    out.staticMs = sumS / 5.0;
    out.evictStaticPerFrame =
        (bm.lastStats().evictedTotal - evictStatic0) / 5u;
    (void)out.dc;   // o dc real sai do st3d do device; no stub o drawCalls
                    // do Stats de bloco não soma os draws fundidos — o bl
                    // e as evictions são os números honestos daqui
    return true;
}

} // namespace

int main() {
    std::printf("== bench_ram — a curva 50k..12.3M (hospedeiro, CPU-side) ==\n");
    std::printf(
        "%9s %8s %9s %10s %10s %10s %8s %9s %9s %9s %9s\n", "tri", "blocos",
        "open_ms", "frame0_ms", "orbit_ms", "parado_ms", "bl", "vram_MB",
        "evc_orb", "evc_par", "rss_MB");
    const u32 sizes[] = {50000, 200000, 500000, 1000000, 2000000, 4100000,
                         8200000, 12300000};
    for (const u32 tris : sizes) {
        Row row;
        if (!runSize(tris, row)) {
            std::printf("%9u FALHOU\n", tris);
            continue;
        }
        std::printf(
            "%9u %8u %9.1f %10.1f %10.2f %10.2f %8u %9.1f %9u %9u %9.1f\n",
            row.tri, row.blocks, row.openMs, row.firstFrameMs, row.steadyMs,
            row.staticMs, row.blDrawn,
            double(row.vramBytes) / (1024.0 * 1024.0), row.evictPerFrame,
            row.evictStaticPerFrame, row.rssPeakMb);
        std::fflush(stdout);
        // o ficheiro da iteração morre com a pasta (limpeza por tamanho)
        const std::string dir =
            "/tmp/goni_bench_ram_" + std::to_string(::getpid());
        std::string cmd = "rm -rf " + dir;
        (void)system(cmd.c_str());
    }
    return 0;
}
