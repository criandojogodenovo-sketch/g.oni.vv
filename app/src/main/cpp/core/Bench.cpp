// core/Bench.cpp — a parte PURA dos benchmarks (R-017). Ver Bench.h.
#include "core/Bench.h"

#include "platform/EngineLog.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cstdlib>

namespace vv::bench {

// ---- agregação ---------------------------------------------------------------

FpsAgg aggregate(const std::vector<double>& fps) {
    FpsAgg a;
    std::vector<double> s;   // só as amostras válidas (>0 fps)
    s.reserve(fps.size());
    for (double v : fps) {
        if (v > 0.0) {
            s.push_back(v);
        }
    }
    if (s.size() < 10) {
        return a;   // menos de ~10 frames não é uma medição (ok=false)
    }
    std::sort(s.begin(), s.end());
    double sum = 0.0;
    for (double v : s) {
        sum += v;
    }
    a.avg = sum / static_cast<double>(s.size());
    a.min = s.front();
    // 1% low: o fps que 99% das amostras igualam ou excedem
    // (percentil 1 ascendente; n>=10 garante índice válido)
    const size_t idx = static_cast<size_t>(
        0.01 * static_cast<double>(s.size() - 1) + 0.5);
    a.p1 = s[std::min(idx, s.size() - 1)];
    a.ok = true;
    return a;
}

// ---- pico de RSS (VmHWM — o kernel mantém o máximo) --------------------------

u64 readPeakRssKb() {
    FILE* f = std::fopen("/proc/self/status", "r");
    if (!f) {
        return 0;   // não-Android/Linux — não medido (honestidade)
    }
    char line[256];
    u64 kb = 0;
    while (std::fgets(line, sizeof(line), f)) {
        if (std::strncmp(line, "VmHWM:", 6) == 0) {
            kb = static_cast<u64>(std::strtoull(line + 6, nullptr, 10));
            break;
        }
    }
    std::fclose(f);
    return kb;
}

// ---- formatação (a fonte única do bloco) --------------------------------------

namespace {

// um double com 0 casas quando >=10, 1 casa quando <10 (o bloco fica
// estável e legível sem flutuar de tamanho a cada mudança de unidade)
void fmt1(char* out, size_t cap, const Measured& m, const char* pattern) {
    if (!m.ok) {
        std::snprintf(out, cap, "não medido");
        return;
    }
    std::snprintf(out, cap, pattern, m.value);
}

void fmtFps(char* out, size_t cap, const FpsAgg& f) {
    if (!f.ok) {
        std::snprintf(out, cap, "não medido");
        return;
    }
    std::snprintf(out, cap, "%.0f fps (min %.0f, 1%% low %.0f)",
                  f.avg, f.min, f.p1);
}

} // namespace

std::string fmtOr(const Measured& m, const char* pattern) {
    char buf[48];
    fmt1(buf, sizeof(buf), m, pattern);
    return std::string(buf);
}

std::string format(const Report& r) {
    char l1[160], l2[96], l3[128], l4[128], l5[96], l6[96], l7[96],
         l8[96], l9[160];

    // 1 — identidade (version do buildinfo; device/Android da JNI — no host
    //     o device é "" e a linha DIZ isso em vez de inventar um nome)
    std::snprintf(l1, sizeof(l1),
                  "Benchmarks · G.One VV %s (%u) · %s · Android %s",
                  r.version[0] ? r.version : "dev", r.versionCode,
                  r.device[0] ? r.device : "não medido",
                  r.sdk > 0 ? std::to_string(r.sdk).c_str() : "não medido");

    // 2 — arranques
    char warm[32], cold[32];
    fmt1(warm, sizeof(warm), r.warmStartMs, "%.0f ms");
    fmt1(cold, sizeof(cold), r.coldStartMs, "%.0f ms");
    std::snprintf(l2, sizeof(l2), "warm start: %s · cold start: %s",
                  warm, cold);

    // 3/4 — as cenas (mesma fonte de verts/draw calls que a barra de estado)
    char defF[64], scnF[64], dv[24], dd[24], sv[24], sd[24];
    fmtFps(defF, sizeof(defF), r.def);
    fmtFps(scnF, sizeof(scnF), r.scene);
    fmt1(dv, sizeof(dv), r.defVerts, "%.0f");
    fmt1(dd, sizeof(dd), r.defDc, "%.0f");
    fmt1(sv, sizeof(sv), r.sceneVerts, "%.0f");
    fmt1(sd, sizeof(sd), r.sceneDc, "%.0f");
    std::snprintf(l3, sizeof(l3),
                  "cena default: %s · %s verts · %s draw calls",
                  defF, dv, dd);
    std::snprintf(l4, sizeof(l4),
                  "cena bench (mesh importado): %s · %s verts · %s draw calls",
                  scnF, sv, sd);

    // 5 — import do glTF de referência (a escala é o round-trip do scale
    //     do nó: 2.5 sai do GLB, passa o importer, chega ao Transform3D)
    char ims[32], isc[32];
    fmt1(ims, sizeof(ims), r.importMs, "%.0f ms");
    fmt1(isc, sizeof(isc), r.importScale, "%.1f");
    std::snprintf(l5, sizeof(l5), "import glTF ref: %s (escala %s)",
                  ims, isc);

    // 6 — compressão de textura (a MESMA máquina do pipeline: ASTC se a
    //     extensão existe, senão ETC2 — formatName do formato escolhido)
    char tms[32];
    fmt1(tms, sizeof(tms), r.texMs, "%.0f ms");
    std::snprintf(l6, sizeof(l6), "texturas: %s (%s)",
                  r.texFormat[0] ? r.texFormat : "não medido", tms);

    // 7 — áudio (o probe de sempre: ok/total + backend ativo)
    char ab[32];
    if (r.audioTotal == 0) {
        std::snprintf(ab, sizeof(ab), "não medido");
        std::snprintf(l7, sizeof(l7), "áudio: %s", ab);
    } else {
        std::snprintf(l7, sizeof(l7), "áudio: %u/%u ok · %s",
                      r.audioOk, r.audioTotal,
                      r.audioBackend[0] ? r.audioBackend : "?");
    }

    // 8 — pico de RSS
    char rss[32];
    fmt1(rss, sizeof(rss), r.rssPeakMb, "%.0f MB");
    std::snprintf(l8, sizeof(l8), "memória: pico RSS %s", rss);

    // 9 — APK + projeto (o sha256 curto do APK instalado; o tamanho que
    //     o projeto ocupa no storage)
    char apk[24], sha[28], proj[32];
    fmt1(apk, sizeof(apk), r.apkMb, "%.1f MB");
    fmt1(proj, sizeof(proj), r.projMb, "%.1f MB");
    std::snprintf(sha, sizeof(sha), "%s",
                  r.apkShaOk.ok ? r.apkSha : "não medido");
    std::snprintf(l9, sizeof(l9), "APK: %s · sha256 %s · projeto: %s",
                  apk, sha, proj);

    std::string out;
    out.reserve(1024);
    const char* const lines[9] = { l1, l2, l3, l4, l5, l6, l7, l8, l9 };
    for (int i = 0; i < 9; ++i) {
        out += lines[i];
        out += (i == 8) ? "\n" : "\n";
    }
    return out;
}

// ---- GLB de referência (determinístico) ---------------------------------------
//
// grelha 16×16 (256 verts, 450 tris) com POSITION/NORMAL/TEXCOORD_0 +
// índices u16; UM nó "benchref" com scale 2.5. O container é o GLB mínimo
// (JSON chunk + BIN chunk, alinhados a 4) — o MESMO formato que o
// convert::importFile consome no device (afervado pelo FASE 12.8).

namespace {

void pushU32(std::vector<u8>& v, u32 x) {
    v.push_back(static_cast<u8>(x & 0xFF));
    v.push_back(static_cast<u8>((x >> 8) & 0xFF));
    v.push_back(static_cast<u8>((x >> 16) & 0xFF));
    v.push_back(static_cast<u8>((x >> 24) & 0xFF));
}

void pushF32(std::vector<u8>& v, f32 x) {
    const u32 b = *reinterpret_cast<const u32*>(&x);
    pushU32(v, b);
}

} // namespace

void makeReferenceGlb(std::vector<u8>& out) {
    constexpr int kGrid = 16;               // 16×16 vértices
    constexpr f32  kNodeScale = 2.5f;       // a escala que o relatório afere

    // ---- BIN: pos(3f) + nrm(3f) + uv(2f) + idx(u16) ------------------------
    std::vector<u8> bin;
    for (int iz = 0; iz < kGrid; ++iz) {
        for (int ix = 0; ix < kGrid; ++ix) {
            const f32 u = static_cast<f32>(ix) / (kGrid - 1);
            const f32 v = static_cast<f32>(iz) / (kGrid - 1);
            pushF32(bin, u);                       // x = u (0..1)
            pushF32(bin, 0.0f);                    // y = plano
            pushF32(bin, v);                       // z = v (0..1)
        }
    }
    for (int i = 0; i < kGrid * kGrid; ++i) {      // normais +Y
        pushF32(bin, 0.0f);
        pushF32(bin, 1.0f);
        pushF32(bin, 0.0f);
    }
    for (int iz = 0; iz < kGrid; ++iz) {
        for (int ix = 0; ix < kGrid; ++ix) {
            pushF32(bin, static_cast<f32>(ix) / (kGrid - 1));
            pushF32(bin, static_cast<f32>(iz) / (kGrid - 1));
        }
    }
    const int idxCount = (kGrid - 1) * (kGrid - 1) * 6;
    for (int iz = 0; iz < kGrid - 1; ++iz) {
        for (int ix = 0; ix < kGrid - 1; ++ix) {
            const int a = iz * kGrid + ix;
            const u16 quad[6] = {
                static_cast<u16>(a), static_cast<u16>(a + 1),
                static_cast<u16>(a + kGrid),
                static_cast<u16>(a + 1), static_cast<u16>(a + kGrid + 1),
                static_cast<u16>(a + kGrid) };
            for (u16 x : quad) {
                bin.push_back(static_cast<u8>(x & 0xFF));
                bin.push_back(static_cast<u8>(x >> 8));
            }
        }
    }

    const u32 posLen = static_cast<u32>(kGrid * kGrid * 12);
    const u32 nrmLen = static_cast<u32>(kGrid * kGrid * 12);
    const u32 uvLen  = static_cast<u32>(kGrid * kGrid * 8);
    const u32 idxLen = static_cast<u32>(idxCount * 2);
    const u32 oPos = 0;
    const u32 oNrm = posLen;
    const u32 oUv  = oNrm + nrmLen;
    const u32 oIdx = oUv + uvLen;

    // ---- JSON ----------------------------------------------------------------
    char j[1024];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"scene\":0,"
        "\"scenes\":[{\"nodes\":[0]}],"
        "\"nodes\":[{\"name\":\"benchref\",\"scale\":[%.1f,%.1f,%.1f],"
        "\"mesh\":0}],"
        "\"meshes\":[{\"name\":\"benchref\",\"primitives\":[{\"attributes\":"
        "{\"POSITION\":0,\"NORMAL\":1,\"TEXCOORD_0\":2},\"indices\":3}]}],"
        "\"buffers\":[{\"byteLength\":%u}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u,\"target\":34962},"
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u,\"target\":34962},"
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u,\"target\":34962},"
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":%u,\"target\":34963}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":%d,\"type\":\"VEC3\","
        "\"min\":[0,0,0],\"max\":[1,0,1]},"
        "{\"bufferView\":1,\"componentType\":5126,\"count\":%d,\"type\":\"VEC3\"},"
        "{\"bufferView\":2,\"componentType\":5126,\"count\":%d,\"type\":\"VEC2\"},"
        "{\"bufferView\":3,\"componentType\":5123,\"count\":%d,\"type\":\"SCALAR\"}]}",
        kNodeScale, kNodeScale, kNodeScale,
        static_cast<u32>(bin.size()),
        oPos, posLen, oNrm, nrmLen, oUv, uvLen, oIdx, idxLen,
        kGrid * kGrid, kGrid * kGrid, kGrid * kGrid, idxCount);

    std::string json = j;
    while (json.size() % 4 != 0) {
        json += ' ';
    }
    while (bin.size() % 4 != 0) {
        bin.push_back(0);
    }

    // ---- o container GLB ------------------------------------------------------
    out.clear();
    out.reserve(12 + 8 + json.size() + 8 + bin.size());
    pushU32(out, 0x46546C67u);   // 'glTF'
    pushU32(out, 2);             // versão
    pushU32(out, static_cast<u32>(12 + 8 + json.size() + 8 + bin.size()));
    pushU32(out, static_cast<u32>(json.size()));
    pushU32(out, 0x4E4F534Au);   // 'JSON'
    out.insert(out.end(), json.begin(), json.end());
    pushU32(out, static_cast<u32>(bin.size()));
    pushU32(out, 0x004E4942u);   // 'BIN'
    out.insert(out.end(), bin.begin(), bin.end());
}

// ---- marcas de arranque --------------------------------------------------------

namespace {
struct Marks {
    std::chrono::steady_clock::time_point create{};
    std::chrono::steady_clock::time_point resume{};
    std::chrono::steady_clock::time_point firstFrame{};
    std::chrono::steady_clock::time_point frameAfterResume{};
    bool seenCreate = false;
    bool seenResume = false;
    bool seenFirst = false;
    bool seenFrameAfterResume = false;
};
Marks g_marks;   // thread da UI escreve (onCreate/onResume/swaps); o bench
                 // lê na MESMA thread — sem atomics necessários (o frame e o
                 // lifecycle são serializados pelo looper do Android)
} // namespace

void markCreate() {
    g_marks.create = std::chrono::steady_clock::now();
    g_marks.seenCreate = true;
}

void markResume() {
    g_marks.resume = std::chrono::steady_clock::now();
    g_marks.seenResume = true;
    g_marks.seenFrameAfterResume = false;   // espera o PRÓXIMO swap
}

// chamado a CADA apresentação: barato (dois compares) e idempotente —
// guarda a primeira do processo (cold) e a primeira após cada onResume (warm)
void markFirstFrame() {
    const auto n = std::chrono::steady_clock::now();
    if (!g_marks.seenFirst) {
        g_marks.firstFrame = n;
        g_marks.seenFirst = true;
    }
    if (g_marks.seenResume && !g_marks.seenFrameAfterResume) {
        g_marks.frameAfterResume = n;
        g_marks.seenFrameAfterResume = true;
    }
}

Measured latestColdMs() {
    Measured m;
    if (!g_marks.seenCreate || !g_marks.seenFirst ||
        g_marks.firstFrame < g_marks.create) {
        return m;   // não medido (o 1º frame veio antes da marca? não inventa)
    }
    m.value = std::chrono::duration<double, std::milli>(
                  g_marks.firstFrame - g_marks.create).count();
    m.ok = true;
    return m;
}

Measured latestWarmMs() {
    Measured m;
    if (!g_marks.seenResume || !g_marks.seenFrameAfterResume ||
        g_marks.frameAfterResume < g_marks.resume) {
        return m;   // nunca saiu do foreground desde o arranque → não medido
    }
    m.value = std::chrono::duration<double, std::milli>(
                  g_marks.frameAfterResume - g_marks.resume).count();
    m.ok = true;
    return m;
}

void resetMarksForTest() {
    g_marks = Marks{};
}

} // namespace vv::bench
