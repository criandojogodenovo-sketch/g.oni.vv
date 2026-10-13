#pragma once
// tests/Medicoes010m.h — 0.10-M: o toolkit PARTILHADO das medições da
// campanha (extraído do test_010m_medicoes.cpp no PASSO 6 — a validação
// final precisa do MESMO gerador de perfis e do MESMO medidor de pico;
// UMA fonte de verdade: o byte do perfil não diverge entre testes).
//
// O QUE VIVE AQUI (tudo o que os testes de medição partilham):
//   • os PERFIS do dono (dragão-38MB/buddha/scene/dragão-fit — cópias com
//     o MESMO tamanho de ficheiro e composição declarada; os ficheiros
//     REAIS vivem no device) e o GERADOR GLB em streaming (a RAM do
//     gerador é um chunk de 6 MB);
//   • os MEDIDORES: pico de RSS com RESET (VmHWM zerado via clear_refs)
//     e as marcas do engine.log (readMarks/spanBetween — o log tem
//     MM-DD HH:MM:SS.mmm) + o nº de ms da linha contrato «ms=<n>»;
//   • a LIMPEZA TOTAL do /tmp do filho (a lição do wiring010: /tmp nunca
//     fica sujo — nem com metade).
// O QUE NÃO VIVE AQUI: os TESTs — cada ficheiro traz os seus (o PASSO 1
// mede os perfis por fase; o PASSO 6 corre o caminho inteiro com a
// tabela final).
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

#include "core/Types.h"

namespace medicoes {

using namespace vv;

// OS PERFIS: os ficheiros REAIS do dono vivem no device (C33) — aqui
// correm CÓPIAS DE PERFIL com o MESMO tamanho de ficheiro e uma
// composição declarada (o relatório registra a composição; os tempos por
// fase variam com ela). O dragão-fit é o perfil que CABE no teto de
// hoje (65 535 verts) e completa o caminho inteiro — os outros três
// morrem EXATAMENTE no teto (o diagnóstico do PASSO 1).
struct Profile {
    const char* name;
    u32 meshes;        // nós/meshes do glTF
    u32 primsPerMesh;  // primitivas por mesh
    u32 vertsTotal;    // vértices do ficheiro (soma das primitivas)
    u64 trisTotal;     // triângulos do ficheiro
    bool u32Idx;       // índices u32 (5125) — senão u16 (5123)
    bool expectOk;     // true = o import TEM de completar (perfil fit)
};

// os 4 perfis (a composição vive no relatório). PASSO 1: os 3 grandes
// morriam EXATAMENTE no teto 65 535 (o diagnóstico). PASSO 3: o conversor
// STREAMING removeu a parede — TODOS completam com verificação bit a bit
// (a linha «gmesh: v3 ... verificado=1» é o veredito de cada perfil).
constexpr Profile kProfiles[] = {
    {"dragao-38MB", 1, 1, 300001, 2370000, true, true},
    {"buddha-classico", 1, 12, 543652, 10670000, false, true},
    {"scene-213MB", 80, 1, 5242880, 10000000, false, true},
    {"dragao-fit", 1, 1, 65535, 130000, false, true},
};

inline u32 vertsOfPrim(const Profile& p, u32 prim) {
    const u32 n = p.meshes * p.primsPerMesh;
    const u32 base = p.vertsTotal / n;
    return prim + 1 == n ? p.vertsTotal - base * (n - 1) : base;
}
inline u64 trisOfPrim(const Profile& p, u32 prim) {
    const u32 n = p.meshes * p.primsPerMesh;
    const u64 base = p.trisTotal / n;
    return prim + 1 == n ? p.trisTotal - base * (n - 1) : base;
}

// ---- o GERADOR GLB em streaming (RAM do gerador: um chunk de 6 MB) -----

class ChunkWriter {
public:
    explicit ChunkWriter(FILE* f) : f_(f) { buf_.reserve(kFlush); }
    ~ChunkWriter() { flush(); }
    void put(const void* p, size_t n) {
        const u8* b = static_cast<const u8*>(p);
        while (n > 0) {
            const size_t room = kFlush - buf_.size();
            const size_t take = n < room ? n : room;
            buf_.insert(buf_.end(), b, b + take);
            b += take;
            n -= take;
            if (buf_.size() == kFlush) flush();
        }
    }
    void f32v(f32 v) { put(&v, 4); }
    void u16v(u16 v) { put(&v, 2); }
    void u32v(u32 v) { put(&v, 4); }
    void zero(size_t n) {
        static const u8 z[64] = {0};
        while (n > 0) {
            const size_t take = n < 64 ? n : 64;
            put(z, take);
            n -= take;
        }
    }

private:
    static constexpr size_t kFlush = 6ull * 1024 * 1024;
    void flush() {
        if (!buf_.empty()) {
            std::fwrite(buf_.data(), 1, buf_.size(), f_);
            buf_.clear();
        }
    }
    FILE* f_;
    std::vector<u8> buf_;
};

// o i-ésimo vértice (fórmula determinística; min/max EXATOS de pos = ±1)
inline void vertexOf(u64 i, f32 out[3]) {
    const u64 rx = i % 997, ry = i % 991, rz = i % 983;
    out[0] = -1.0f + 2.0f * static_cast<f32>(rx) / 996.0f;
    out[1] = -1.0f + 2.0f * static_cast<f32>(ry) / 990.0f;
    out[2] = -1.0f + 2.0f * static_cast<f32>(rz) / 982.0f;
}

// offsets alinhados a 4: [pos, nrm, uv, idx] por primitiva, em sequência
struct Views {
    struct V {
        u64 offset;
        u64 length;
    };
    V item[4];
};
inline Views viewsOfPrim(const Profile& p, u32 prim, u64 off) {
    Views w;
    const u32 v = vertsOfPrim(p, prim);
    const u64 t = trisOfPrim(p, prim);
    const u64 lens[4] = {u64(v) * 12, u64(v) * 12, u64(v) * 8,
                         t * 3 * u64(p.u32Idx ? 4 : 2)};
    for (int a = 0; a < 4; ++a) {
        off += (4 - off % 4) % 4;
        w.item[a] = {off, lens[a]};
        off += lens[a];
    }
    return w;
}

inline bool generateGlb(const Profile& p, const std::string& path, u64& glbBytes) {
    // ---- passada 1 (analítica): offsets + tamanho do BIN ----------------
    const u32 nPrims = p.meshes * p.primsPerMesh;
    std::vector<Views> vw(nPrims);
    u64 binLen = 0;
    for (u32 k = 0; k < nPrims; ++k) {
        vw[k] = viewsOfPrim(p, k, binLen);
        const Views::V& last = vw[k].item[3];
        binLen = last.offset + last.length;
    }
    const u64 binPadded = (binLen + 3) & ~u64(3);

    // ---- passada 2: o JSON (pequeno — cabe em RAM) ----------------------
    std::string j = "{\"asset\":{\"version\":\"2.0\"},\"scene\":0,"
                    "\"scenes\":[{\"nodes\":[";
    for (u32 m = 0; m < p.meshes; ++m) {
        j += (m ? "," : "");
        j += std::to_string(m);
    }
    j += "]}],\"nodes\":[";
    for (u32 m = 0; m < p.meshes; ++m) {
        j += (m ? "," : "");
        j += "{\"mesh\":" + std::to_string(m) + ",\"name\":\"n" +
             std::to_string(m) + "\"}";
    }
    j += "],\"meshes\":[";
    u32 acc = 0, view = 0;
    for (u32 m = 0; m < p.meshes; ++m) {
        j += (m ? "," : "");
        j += "{\"name\":\"m" + std::to_string(m) + "\",\"primitives\":[";
        for (u32 q = 0; q < p.primsPerMesh; ++q) {
            const u32 k = m * p.primsPerMesh + q;
            j += (q ? "," : "");
            j += "{\"attributes\":{\"POSITION\":" + std::to_string(acc) +
                 ",\"NORMAL\":" + std::to_string(acc + 1) +
                 ",\"TEXCOORD_0\":" + std::to_string(acc + 2) +
                 "},\"indices\":" + std::to_string(acc + 3) +
                 ",\"material\":" + std::to_string(k % 3) +
                 ",\"mode\":4}";
            acc += 4;
        }
        j += "]}";
    }
    j += "],\"materials\":[";
    for (int mi = 0; mi < 3; ++mi) {
        j += (mi ? "," : "");
        j += "{\"name\":\"mat" + std::to_string(mi) +
             "\",\"pbrMetallicRoughness\":{\"baseColorFactor\":[0.8,0.8,"
             "0.8,1]}}";
    }
    j += "],\"accessors\":[";
    for (u32 k = 0; k < nPrims; ++k) {
        const u32 v = vertsOfPrim(p, k);
        const u64 t = trisOfPrim(p, k);
        const Views& w = vw[k];
        const char* sep = k ? "," : "";
        j += sep;
        j += "{\"bufferView\":" + std::to_string(view) +
             ",\"componentType\":5126,\"count\":" + std::to_string(v) +
             ",\"type\":\"VEC3\",\"min\":[-1,-1,-1],\"max\":[1,1,1]}";
        j += ",{\"bufferView\":" + std::to_string(view + 1) +
             ",\"componentType\":5126,\"count\":" + std::to_string(v) +
             ",\"type\":\"VEC3\"}";
        j += ",{\"bufferView\":" + std::to_string(view + 2) +
             ",\"componentType\":5126,\"count\":" + std::to_string(v) +
             ",\"type\":\"VEC2\"}";
        j += ",{\"bufferView\":" + std::to_string(view + 3) +
             ",\"componentType\":" + (p.u32Idx ? "5125" : "5123") +
             ",\"count\":" + std::to_string(t * 3) + ",\"type\":\"SCALAR\"}";
        view += 4;
    }
    j += "],\"bufferViews\":[";
    for (u32 k = 0; k < nPrims; ++k) {
        const Views& w = vw[k];
        for (int a = 0; a < 4; ++a) {
            const bool first = k == 0 && a == 0;
            j += first ? "" : ",";
            j += "{\"buffer\":0,\"byteOffset\":" +
                 std::to_string(w.item[a].offset) + ",\"byteLength\":" +
                 std::to_string(w.item[a].length) + "}";
        }
    }
    j += "],\"buffers\":[{\"byteLength\":" + std::to_string(binPadded) +
         "}]}";

    // o JSON chunk fica alinhado a 4 (pad com espaços)
    const u64 jsonPadded = (u64(j.size()) + 3) & ~u64(3);
    j.append(size_t(jsonPadded - j.size()), ' ');
    if (::getenv("GONI_010M_DEBUG")) {
        std::fprintf(stderr, "[010m-json] %s\n", j.c_str());
    }

    // ---- passada 3: o ficheiro (header + JSON + BIN em streaming) -------
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const u32 total = u32(12 + 8 + jsonPadded + 8 + binPadded);
    auto w32 = [&f](u32 v) { std::fwrite(&v, 4, 1, f); };
    w32(0x46546C67);          // 'glTF'
    w32(2);                   // version
    w32(total);
    w32(u32(jsonPadded));
    w32(0x4E4F534A);          // 'JSON'
    std::fwrite(j.data(), 1, j.size(), f);
    w32(u32(binPadded));
    w32(0x004E4942);          // 'BIN\0'
    {
        ChunkWriter bin(f);
        // pad inicial até ao offset da 1.ª secção (0 — sem pad) e os dados
        for (u32 k = 0; k < nPrims; ++k) {
            const Views& w = vw[k];
            const u64 cur = k == 0 ? 0 : (vw[k - 1].item[3].offset +
                                          vw[k - 1].item[3].length);
            const u64 gap = w.item[0].offset - cur;
            bin.zero(size_t(gap));
            const u32 v = vertsOfPrim(p, k);
            for (u32 i = 0; i < v; ++i) {
                f32 pos[3];
                vertexOf(i, pos);
                bin.f32v(pos[0]);
                bin.f32v(pos[1]);
                bin.f32v(pos[2]);
            }
            bin.zero(size_t(w.item[1].offset - (w.item[0].offset +
                                                w.item[0].length)));
            for (u32 i = 0; i < v; ++i) {
                // normal determinística [-1,1] (unitária não é exigida)
                bin.f32v((i % 97) / 48.0f - 1.0f);
                bin.f32v((i % 89) / 44.0f - 1.0f);
                bin.f32v((i % 83) / 41.0f - 1.0f);
            }
            bin.zero(size_t(w.item[2].offset - (w.item[1].offset +
                                                w.item[1].length)));
            for (u32 i = 0; i < v; ++i) {
                bin.f32v(f32(i % 64) / 63.0f);
                bin.f32v(f32(i % 32) / 31.0f);
            }
            bin.zero(size_t(w.item[3].offset - (w.item[2].offset +
                                                w.item[2].length)));
            const u64 t = trisOfPrim(p, k);
            if (p.u32Idx) {
                for (u64 ti = 0; ti < t; ++ti) {
                    bin.u32v(u32((3 * ti) % v));
                    bin.u32v(u32((3 * ti + 1) % v));
                    bin.u32v(u32((3 * ti + 2) % v));
                }
            } else {
                for (u64 ti = 0; ti < t; ++ti) {
                    bin.u16v(u16((3 * ti) % v));
                    bin.u16v(u16((3 * ti + 1) % v));
                    bin.u16v(u16((3 * ti + 2) % v));
                }
            }
        }
    }
    const bool ok = !std::ferror(f);
    std::fclose(f);
    glbBytes = total;
    return ok;
}

// ---- utilitários de medição --------------------------------------------

inline bool rmRf(const std::string& path) {
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

inline bool mkDir(const std::string& p) {
    return ::mkdir(p.c_str(), 0755) == 0 || errno == EEXIST;
}

// ms da marca de log mais próxima (o engine.log tem MM-DD HH:MM:SS.mmm)
struct LogMark {
    double ms;
    std::string line;
};
// o nº de BLOCOS da linha contrato «gmesh: v3 blocos=N verts=...» (0 = ausente)
inline u64 blocosFromLog(const std::string& logPath) {
    FILE* f = std::fopen(logPath.c_str(), "rb");
    if (!f) return 0;
    char buf[4096];
    u64 n = 0;
    while (std::fgets(buf, sizeof(buf), f)) {
        const char* m = std::strstr(buf, "gmesh: v3 blocos=");
        if (m != nullptr) {
            n = std::strtoull(m + 17, nullptr, 10);
            break;
        }
    }
    std::fclose(f);
    return n;
}
inline bool fileHasLine(const std::string& logPath, const char* needle) {
    FILE* f = std::fopen(logPath.c_str(), "rb");
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
inline std::vector<LogMark> readMarks(const std::string& logPath) {
    std::vector<LogMark> out;
    FILE* f = std::fopen(logPath.c_str(), "rb");
    if (!f) return out;
    char buf[4096];
    double baseMs = -1.0;
    while (std::fgets(buf, sizeof(buf), f)) {
        int mo, d, h, mi, s, ms;
        if (std::sscanf(buf, "%d-%d %d:%d:%d.%d", &mo, &d, &h, &mi, &s,
                        &ms) != 6) {
            continue;
        }
        const double now = ((h * 60.0 + mi) * 60.0 + s) * 1000.0 + ms;
        if (baseMs < 0) baseMs = now;
        out.push_back({now - baseMs, buf});
    }
    std::fclose(f);
    return out;
}
inline double spanBetween(const std::vector<LogMark>& m, const char* from,
                   const char* to) {
    bool hasA = false;
    double a = 0.0, b = -1.0;
    for (const LogMark& k : m) {
        if (!hasA && k.line.find(from) != std::string::npos) {
            a = k.ms;
            hasA = true;
        }
        if (k.line.find(to) != std::string::npos) b = k.ms;
    }
    return hasA && b >= a ? b - a : -1.0;   // b == a = 0.0 (mesma linha)
}

// PASSO 6 — o nº DE MS da linha contrato «... ms=<n>» (a 1.ª que casa o
// needle; -1.0 se a linha não existe — o chamador DECLARA, não inventa).
// As linhas contrato do import são «import: copia ms=», «gmesh:
// fase=parse|cut|assembly|verify ms=» — o MESMO número que o dono lê no
// engine.log do device (a tabela final lê o que a linha DIZ, não o que
// o relógio do hospedeiro acha).
inline double msOfLine(const std::string& logPath, const char* needle) {
    FILE* f = std::fopen(logPath.c_str(), "rb");
    if (!f) return -1.0;
    char buf[4096];
    double out = -1.0;
    while (std::fgets(buf, sizeof(buf), f)) {
        const char* m = std::strstr(buf, needle);
        if (m == nullptr) continue;
        const char* eq = std::strstr(m, "ms=");
        if (eq == nullptr) continue;
        out = std::strtod(eq + 3, nullptr);
        break;
    }
    std::fclose(f);
    return out;
}

// o pico de RSS do processo (VmHWM) com RESET via clear_refs
inline u64 currentRssKb() {
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
inline bool resetPeakRss() {
    FILE* f = std::fopen("/proc/self/clear_refs", "wb");
    if (!f) return false;
    const bool ok = std::fputc('5', f) == '5';
    std::fclose(f);
    return ok;
}

inline void childCleanup(const std::string& dir) {
    // a LIMPEZA TOTAL (a lição do wiring010: /tmp nunca fica sujo — nem
    // com metade; o rmRf recursivo leva logs, proj, fonte e a raiz)
    rmRf(dir);
}

} // namespace medicoes
