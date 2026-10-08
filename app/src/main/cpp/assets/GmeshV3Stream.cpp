// assets/GmeshV3Stream.cpp — 0.10-M (PASSO 3): o conversor em streaming
// e paralelo (glTF/GLB → .gmesh v3). A spec viva é o header
// (GmeshV3Stream.h), o §streaming de docs/GMESH_formato.md e o relatório
// do PASSO 3.
//
// O CAMINHO (o relatório cita cada fase):
//   tarefas (por primitiva) → CORTE paralelo (pool de núcleos−1; cada
//   bloco fechado appenda-se ao TEMP em disco — a RAM segura o remap da
//   primitiva + UM bloco) → ASSEMBLY (ordena material→nó→bloco e escreve
//   o ficheiro final pela escrita streaming do storage, a esqueleto v3
//   PARTILHADA com o writeGMesh) → VERIFICAÇÃO (relê o final por mmap e
//   compara POR TRIÂNGULO com a origem reavaliando o MESMO bake).
#include "assets/GmeshV3Stream.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

#include <sys/mman.h>
#include <unistd.h>

#include "assets/GltfImporter.h"
#include "platform/EngineLog.h"
#include "platform/FileApi.h"
#include "platform/StorageBridge.h"   // 0.10-M: jniCacheDir — a cascata do temp (stagingWrite)

namespace vv {

namespace {

// o chunk da leitura do temporário → escrita final (a RAM não cresce com
// o ficheiro; o mesmo espírito do kChunkBytes do conversor)
constexpr size_t kStreamChunk = 1ull * 1024 * 1024;
// a janela do MADV_DONTNEED no mmap da fonte (a higiene de RSS: as
// páginas limpas do mmap contam no pico — a casa larga o que já usou)
constexpr u64 kDropWindow = 32ull * 1024 * 1024;
// o cadência do progresso/cancelar dentro do corte (em triângulos)
constexpr u64 kProgressStride = 16384;

// ---- o accessor resolvido (offsets desde o INÍCIO do buffer 0) -------------
struct AccInfo {
    u64 offset = 0;
    u64 count = 0;
    u32 elemSize = 0;
    u32 stride = 0;
    u32 compSize = 0;
    bool ok = false;
};

bool resolveAccessor(const Json& jacc, const Json& jviews, u64 binLen,
                     u32 wantComps, u32 wantCompSize, AccInfo& out,
                     std::string& err) {
    const Json* jOff = jacc.find("byteOffset");
    const u64 accOff =
        jOff && jOff->type == Json::Type::Number
            ? static_cast<u64>(jOff->number)
            : 0;
    const Json* jCount = jacc.find("count");
    const u64 count =
        jCount && jCount->type == Json::Type::Number
            ? static_cast<u64>(jCount->number)
            : 0;
    i32 compType = 5126;
    if (const Json* t = jacc.find("componentType");
        t && t->type == Json::Type::Number) {
        compType = static_cast<i32>(t->number);
    }
    u32 cs = 0;
    switch (compType) {
        case 5120: case 5121: cs = 1; break;
        case 5122: case 5123: cs = 2; break;
        case 5125: case 5126: cs = 4; break;
        default:
            err = "accessor com componentType " + std::to_string(compType) +
                  " (desconhecido)";
            return false;
    }
    u32 nc = 0;
    if (const Json* t = jacc.find("type"); t && t->type == Json::Type::String) {
        const std::string& ty = t->string;
        nc = ty == "SCALAR" ? 1 : ty == "VEC2" ? 2 : ty == "VEC3" ? 3
                                  : ty == "VEC4"   ? 4
                                                   : 0;
    }
    if (nc != wantComps || cs != wantCompSize) {
        err = "accessor com layout inesperado (" + std::to_string(nc) +
              " comps de " + std::to_string(cs) + " B)";
        return false;
    }
    const Json* jbv = jacc.find("bufferView");
    if (!jbv || jbv->type != Json::Type::Number) {
        err = "accessor de geometria sem bufferView";
        return false;
    }
    const i64 bi = static_cast<i64>(jbv->number);
    if (bi < 0 || bi >= static_cast<i64>(jviews.items.size())) {
        err = "bufferView " + std::to_string(bi) + " fora do documento";
        return false;
    }
    const Json& view = jviews.items[static_cast<size_t>(bi)];
    const Json* jvOff = view.find("byteOffset");
    const u64 vOff = jvOff && jvOff->type == Json::Type::Number
                         ? static_cast<u64>(jvOff->number)
                         : 0;
    const Json* jvLen = view.find("byteLength");
    const u64 vLen = jvLen && jvLen->type == Json::Type::Number
                         ? static_cast<u64>(jvLen->number)
                         : 0;
    u32 stride = 0;
    if (const Json* s = view.find("byteStride");
        s && s->type == Json::Type::Number) {
        stride = static_cast<u32>(s->number);
    }
    if (stride != 0 && stride < cs * nc) {
        err = "byteStride " + std::to_string(stride) +
              " menor que o elemento (" + std::to_string(cs * nc) + ")";
        return false;
    }
    if (stride == 0) stride = cs * nc;
    if (accOff > vLen) {
        err = "byteOffset do accessor (" + std::to_string(accOff) +
              ") excede o bufferView (" + std::to_string(vLen) + ")";
        return false;
    }
    if (vOff + vLen > binLen) {
        err = "o bufferView acaba além do buffer (" +
              std::to_string(vOff + vLen) + " > " + std::to_string(binLen) +
              ")";
        return false;
    }
    if (count > 0 && (count - 1) * stride + cs * nc > vLen - accOff) {
        err = "o accessor acaba além do bufferView (count " +
              std::to_string(count) + ")";
        return false;
    }
    out.offset = vOff + accOff;
    out.count = count;
    out.elemSize = cs * nc;
    out.stride = stride;
    out.compSize = cs;
    out.ok = true;
    return true;
}

// o bake (a MESMA multiplicação do merge antigo — e da verificação)
inline void bakePos(const Vec3& p, const Mat4& M, f32 out[3]) {
    out[0] = M.m[0] * p.x + M.m[4] * p.y + M.m[8] * p.z + M.m[12];
    out[1] = M.m[1] * p.x + M.m[5] * p.y + M.m[9] * p.z + M.m[13];
    out[2] = M.m[2] * p.x + M.m[6] * p.y + M.m[10] * p.z + M.m[14];
}
inline void bakeNrm(const Vec3& n, const Mat4& R, f32 out[3]) {
    out[0] = R.m[0] * n.x + R.m[4] * n.y + R.m[8] * n.z;
    out[1] = R.m[1] * n.x + R.m[5] * n.y + R.m[9] * n.z;
    out[2] = R.m[2] * n.x + R.m[6] * n.y + R.m[10] * n.z;
}

// ---- o bloco convertido (em RAM só ENTRE o fecho e o append) ---------------
struct BlockOut {
    std::vector<Vertex> pool;
    std::vector<u16> idx;
    Vec3 aabbMin{}, aabbMax{};
    u32 materialOrder = 0;
    u32 taskIdx = 0;
    u32 blockIdx = 0;
    u64 tStart = 0;
    u64 triCount = 0;
};

// o registo do bloco no temp (o assembly ordena por material→task→idx)
struct BlockRec {
    u64 tempOff = 0;
    u32 poolVerts = 0;
    u32 idxCount = 0;
    u32 materialOrder = 0;
    u32 taskIdx = 0;
    u32 blockIdx = 0;
    u64 tStart = 0;
    u64 triCount = 0;
    Vec3 aabbMin{}, aabbMax{};
    u32 crc = 0;
};

// a higiene de RSS: larga (MADV_DONTNEED) as páginas limpas de um range
// do mmap da fonte/final — as páginas de ficheiro contam no pico (VmHWM)
// mesmo sem ser memória do algoritmo; a casa larga o que já usou. O
// endereço alinha-se à página (o madvise exige).
void dropRange(const u8* base, u64 off, u64 len) {
    if (len == 0) return;
    const long pz = ::sysconf(_SC_PAGESIZE);
    if (pz <= 0) return;
    const uintptr_t addr = reinterpret_cast<uintptr_t>(base) + off;
    const uintptr_t aligned = addr & ~static_cast<uintptr_t>(pz - 1);
    const size_t span = static_cast<size_t>(addr + len - aligned);
    ::madvise(reinterpret_cast<void*>(aligned), span, MADV_DONTNEED);
}

// o corte de UMA primitiva (os vértices leem-se do mmap pelo índice).
// Cada bloco FECHADO vai ao sink (o corte appenda ao temp; a verificação
// compara com o ficheiro final) — a RAM segura o remap + UM bloco.
// `drop` (higiene de RSS): a marca de água do span já percorrido.
struct TaskCtx {
    AccInfo pos, nrm, uv, idx;
    bool hasNrm = false, hasUv = false, hasIdx = false;
    const u8* base = nullptr;
    Mat4 world = Mat4::identity();
    Mat4 worldRot = Mat4::identity();
    u32 materialOrder = 0;
    u32 taskIdx = 0;
    u32 cap = kGmeshV3BlockVertexCap;
    u32 posCount = 0;
    u64 trisTotal = 0;
    u64 spanOff = 0;    // o span da fonte desta tarefa (higiene de RSS)
    u64 spanLen = 0;
    std::string material;
};

using BlockSink = bool (*)(void* user, BlockOut& b, std::string& err);

bool cutPrimitive(TaskCtx* c, std::atomic<u64>& trisDone, u64 trisTotal,
                  std::atomic<bool>& canceled, u32 taskIdx,
                  BlockSink sink, void* sinkUser,
                  bool (*onProgress)(void*, u64, u64), void* user,
                  std::string& err) {
    BlockOut cur;
    cur.materialOrder = c->materialOrder;
    cur.taskIdx = taskIdx;
    u32 blockIdx = 0;   // o índice do bloco DENTRO da tarefa (a tabela
                        // ordena material→tarefa→bloco; a verificação
                        // re-corta na MESMA sequência)
    std::vector<u32> remap(c->posCount, 0xFFFFFFFFu);
    std::vector<u32> orig;
    u64 t = 0;
    u64 dropped = 0;   // a marca de água do drop (relativa ao span)
    auto flush = [&]() -> bool {
        if (cur.pool.empty()) return true;
        // a pool sai em ordem ASCENDENTE do índice original (a regra v3:
        // o round-trip fica idêntico e o ficheiro é determinístico)
        std::vector<u32> order(cur.pool.size());
        for (u32 i = 0; i < order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](u32 a, u32 b) {
            return orig[a] < orig[b];
        });
        std::vector<u32> newPos(cur.pool.size());
        std::vector<Vertex> sorted(cur.pool.size());
        for (u32 n = 0; n < order.size(); ++n) {
            newPos[order[n]] = n;
            sorted[n] = cur.pool[order[n]];
        }
        for (u16& li : cur.idx) li = static_cast<u16>(newPos[li]);
        cur.pool = std::move(sorted);
        cur.triCount = t - cur.tStart;
        // a AABB do bloco (sobre a pool JÁ baked — min/max exatos, a
        // ordem não muda o resultado)
        Vec3 mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
        for (const Vertex& v : cur.pool) {
            mn.x = (std::min)(mn.x, v.pos.x);
            mn.y = (std::min)(mn.y, v.pos.y);
            mn.z = (std::min)(mn.z, v.pos.z);
            mx.x = (std::max)(mx.x, v.pos.x);
            mx.y = (std::max)(mx.y, v.pos.y);
            mx.z = (std::max)(mx.z, v.pos.z);
        }
        cur.aabbMin = mn;
        cur.aabbMax = mx;
        cur.blockIdx = blockIdx++;
        if (!sink(sinkUser, cur, err)) {
            return false;
        }
        cur = BlockOut{};
        cur.materialOrder = c->materialOrder;
        cur.taskIdx = taskIdx;
        cur.tStart = t;
        std::fill(remap.begin(), remap.end(), 0xFFFFFFFFu);
        orig.clear();
        return true;
    };
    while (t < c->trisTotal) {
        if (canceled.load(std::memory_order_relaxed)) {
            err = "cancelado";
            return false;
        }
        u64 corner[3];
        u32 fresh = 0;
        for (int k = 0; k < 3; ++k) {
            u64 gi;
            if (c->hasIdx) {
                const u8* p = c->base + c->idx.offset +
                              (t * 3 + static_cast<u64>(k)) * c->idx.stride;
                if (c->idx.compSize == 4) {
                    u32 v;
                    std::memcpy(&v, p, 4);
                    gi = v;
                } else {
                    u16 v;
                    std::memcpy(&v, p, 2);
                    gi = v;
                }
            } else {
                gi = t * 3 + static_cast<u64>(k);
            }
            if (gi >= c->posCount) {
                err = "índice do glTF fora da pool da primitiva (" +
                      std::to_string(gi) + " ≥ " +
                      std::to_string(c->posCount) +
                      "; hasIdx=" + std::to_string(c->hasIdx ? 1 : 0) +
                      " idxOff=" + std::to_string(c->idx.offset) +
                      " idxCount=" + std::to_string(c->idx.count) + ")";
                return false;
            }
            corner[k] = gi;
            if (remap[gi] == 0xFFFFFFFFu) ++fresh;
        }
        if (cur.pool.size() + fresh > c->cap && !cur.pool.empty()) {
            if (!flush()) return false;
        }
        for (int k = 0; k < 3; ++k) {
            const u64 gi = corner[k];
            if (remap[gi] == 0xFFFFFFFFu) {
                remap[gi] = static_cast<u32>(cur.pool.size());
                orig.push_back(static_cast<u32>(gi));
                Vertex v;
                const u8* p = c->base + c->pos.offset + gi * c->pos.stride;
                Vec3 pv;
                std::memcpy(&pv, p, 12);
                bakePos(pv, c->world, &v.pos.x);
                if (c->hasNrm) {
                    Vec3 nv;
                    std::memcpy(&nv, c->base + c->nrm.offset +
                                         gi * c->nrm.stride,
                                12);
                    bakeNrm(nv, c->worldRot, &v.normal.x);
                } else {
                    v.normal = Vec3{0.0f, 1.0f, 0.0f};
                }
                if (c->hasUv) {
                    std::memcpy(&v.uv.x, c->base + c->uv.offset +
                                             gi * c->uv.stride,
                                8);
                } else {
                    v.uv = Vec2{0.0f, 0.0f};
                }
                cur.pool.push_back(v);
            }
            cur.idx.push_back(static_cast<u16>(remap[gi]));
        }
        ++t;
        if ((t & (kProgressStride - 1)) == 0) {
            // o progresso é GLOBAL (triângulos feitos por TODOS os
            // workers — a barra é monotónica, a ordem das threads não)
            trisDone.fetch_add(kProgressStride, std::memory_order_relaxed);
            if (onProgress != nullptr &&
                !onProgress(user, trisDone.load(std::memory_order_relaxed),
                            trisTotal)) {
                canceled.store(true, std::memory_order_relaxed);
            }
            // a higiene de RSS dentro de tarefas grandes: o span da
            // fonte já percorrido larga-se (páginas limpas — reler é
            // barato; o pico fica nos blocos, não no ficheiro)
            const u64 curOff =
                c->pos.offset + (t * 3) * c->pos.stride;
            if (curOff > c->spanOff + dropped + kDropWindow &&
                curOff + kDropWindow < c->spanOff + c->spanLen) {
                const u64 upto = curOff - c->spanOff;
                dropRange(c->base, c->spanOff + dropped, upto - dropped);
                dropped = upto;
            }
        }
    }
    // o resto do progresso (a cauda abaixo da cadência)
    const u64 done = trisDone.fetch_add(c->trisTotal % kProgressStride,
                                        std::memory_order_relaxed);
    (void)done;
    if (!flush()) return false;
    // a tarefa inteira passou — o span dela larga-se TODO
    dropRange(c->base, c->spanOff, c->spanLen);
    return true;
}

// o ficheiro temporário (os blocos appendam-se com mutex; o assembly
// ordena e escreve o ficheiro FINAL com o layout canónico). A casa usa
// .staging/ do projeto (a convenção do stagingWrite) — o caminho é
// ABSOLUTO (o streaming só corre com raiz real).
struct TempStore {
    std::string abs;
    FILE* f = nullptr;

    ~TempStore() { close(); }
    void close() {
        if (f != nullptr) {
            std::fclose(f);
            f = nullptr;
        }
    }
    bool open(const std::string& p, std::string& err) {
        abs = p;
        f = std::fopen(p.c_str(), "w+b");
        if (f == nullptr) {
            err = "não consegui criar o ficheiro temporário '" + p +
                  "' (errno " + std::to_string(errno) + ")" +
                  (errno == ENOSPC ? " — DISCO CHEIO" : "");
            return false;
        }
        return true;
    }
    bool append(const BlockOut& b, BlockRec& rec, std::string& err) {
        // serializa os bytes do bloco (pool f32 + índices u16 — o MESMO
        // layout que o escritor v3 emite: pos, normal, uv)
        std::vector<u8> blob;
        blob.reserve(b.pool.size() * 32 + b.idx.size() * 2);
        for (const Vertex& v : b.pool) {
            u8 raw[32];
            std::memcpy(raw, &v.pos, 12);
            std::memcpy(raw + 12, &v.normal, 12);
            std::memcpy(raw + 24, &v.uv, 8);
            blob.insert(blob.end(), raw, raw + 32);
        }
        for (const u16 i : b.idx) {
            blob.push_back(static_cast<u8>(i & 0xFF));
            blob.push_back(static_cast<u8>((i >> 8) & 0xFF));
        }
        std::lock_guard<std::mutex> lk(mu);
        // std::ftell — a casa usa long (LP64: 64-bit em arm64/x86_64,
        // os alvos da engine; o temporário vive no mesmo volume)
        const long pos = std::ftell(f);
        if (pos < 0) {
            err = "ftello do temporário falhou";
            return false;
        }
        if (std::fwrite(blob.data(), 1, blob.size(), f) != blob.size()) {
            err = "a escrita do temporário falhou (errno " +
                  std::to_string(errno) + ")" +
                  (errno == ENOSPC ? " — DISCO CHEIO (o espaço livre é o "
                                     "limite, não o modelo)" : "");
            return false;
        }
        rec.tempOff = static_cast<u64>(pos);
        rec.poolVerts = static_cast<u32>(b.pool.size());
        rec.idxCount = static_cast<u32>(b.idx.size());
        rec.crc = gcrc32(blob.data(), blob.size());
        rec.aabbMin = b.aabbMin;
        rec.aabbMax = b.aabbMax;
        rec.materialOrder = b.materialOrder;
        rec.taskIdx = b.taskIdx;
        rec.blockIdx = b.blockIdx;
        rec.tStart = b.tStart;
        rec.triCount = b.triCount;
        return true;
    }
    std::mutex mu;
};

// o sink do CORTE: appenda o bloco ao temporário (o mutex ordena o
// disco; a ORDEM final é decidida pelo assembly)
struct CutCtx {
    TempStore* temp = nullptr;
    std::vector<BlockRec>* recs = nullptr;   // por tarefa (o worker dono)
};

bool cutSink(void* user, BlockOut& b, std::string& err) {
    CutCtx* c = static_cast<CutCtx*>(user);
    BlockRec rec;
    if (!c->temp->append(b, rec, err)) {
        return false;
    }
    c->recs->push_back(rec);
    return true;
}

// o sink da VERIFICAÇÃO: o bloco re-cortado compara-se com o bloco do
// FICHEIRO FINAL (lido pelo leitor v3 de produção) — bit a bit. O
// primeiro desvio nomeia o bloco, o campo e os valores (o relatório
// cita-o); o ficheiro divergente não fica no projeto.
struct VerifyCtx {
    const u8* map = nullptr;
    u64 mapLen = 0;
    // O drop de páginas (MADV_DONTNEED) só é LEGÍTIMO num mmap de
    // ficheiro (as páginas re-lêem do disco). Num readback do HEAP
    // (storage virtual) o madvise zera páginas INTEIRAS — vizinhos do
    // heap incluídos — e os destrutores seguintes libertam ponteiros
    // selvagens (o crash do C33-repro). false = NUNCA dropa.
    bool droppable = false;
    const GMeshV3Meta* meta = nullptr;
    const std::vector<GMeshV3Block>* entries = nullptr;
    const std::vector<u32>* taskStart = nullptr;
    u32 taskIdx = 0;
    std::string* dev = nullptr;
    u32 blockIdxSeen = 0;
};

bool verifySink(void* user, BlockOut& b, std::string& err) {
    VerifyCtx* c = static_cast<VerifyCtx*>(user);
    const u32 pos = (*c->taskStart)[c->taskIdx] + c->blockIdxSeen;
    ++c->blockIdxSeen;
    if (pos >= c->entries->size()) {
        *c->dev = "a tarefa " + std::to_string(c->taskIdx) +
                  " produziu mais blocos do que a tabela tem";
        return false;
    }
    const GMeshV3Block& e = (*c->entries)[pos];
    const std::string where =
        "bloco " + std::to_string(pos) + " (tarefa " +
        std::to_string(c->taskIdx) + ")";
    if (e.vertexCount != b.pool.size() || e.indexCount != b.idx.size()) {
        *c->dev = where + ": as contagens divergem (ficheiro verts=" +
                  std::to_string(e.vertexCount) + " idx=" +
                  std::to_string(e.indexCount) + " · origem verts=" +
                  std::to_string(b.pool.size()) + " idx=" +
                  std::to_string(b.idx.size()) + ")";
        return false;
    }
    MeshData part;
    if (!readGMeshV3Block(c->map, static_cast<size_t>(c->mapLen), *c->meta,
                          e, part, err)) {
        err = where + " não materializa: " + err;
        return false;
    }
    if (part.vertices.size() != b.pool.size() ||
        std::memcmp(part.vertices.data(), b.pool.data(),
                    b.pool.size() * sizeof(Vertex)) != 0) {
        for (size_t i = 0; i < b.pool.size(); ++i) {
            if (std::memcmp(&part.vertices[i], &b.pool[i], sizeof(Vertex)) !=
                0) {
                *c->dev = where + ", vértice " + std::to_string(i) +
                          ": o ficheiro diverge da origem (float32 bit a "
                          "bit)";
                break;
            }
        }
        return false;
    }
    if (part.indices.size() != b.idx.size() ||
        std::memcmp(part.indices.data(), b.idx.data(),
                    b.idx.size() * sizeof(u16)) != 0) {
        for (size_t i = 0; i < b.idx.size(); ++i) {
            if (part.indices[i] != b.idx[i]) {
                *c->dev = where + ", triângulo " + std::to_string(i / 3) +
                          ": os índices divergem (ficheiro " +
                          std::to_string(part.indices[i]) + " vs origem " +
                          std::to_string(b.idx[i]) + ")";
                break;
            }
        }
        return false;
    }
    if (std::memcmp(&e.aabbMin, &b.aabbMin, sizeof(Vec3)) != 0 ||
        std::memcmp(&e.aabbMax, &b.aabbMax, sizeof(Vec3)) != 0) {
        *c->dev = where + ": a AABB diverge da origem";
        return false;
    }
    // a higiene de RSS: o range deste bloco no ficheiro final larga-se
    // (páginas limpas do mmap — o pico fica num bloco, não no ficheiro)
    // SÓ no caminho mmap: o readback do heap NÃO droppa (ver VerifyCtx)
    if (c->droppable) {
        dropRange(c->map, e.dataOffset, e.dataSize);
    }
    return true;
}

} // namespace

// ---- o CONVERSOR (a spec do PASSO 3 inteira) --------------------------------

// o POOL do corte: núcleos−1 e NUNCA 0 (o contrato está no header —
// single-core/desconhecido cai para 1: sequencial, mais lento, CORRETO)
u32 v3StreamWorkerCount(const u32 hardwareThreads) {
    return hardwareThreads > 1u ? hardwareThreads - 1u : 1u;
}

bool convertGltfToV3(const GltfModel& model, const Json& doc,
                     const u8* bin, u64 binLen, u64 fileBytes,
                     const std::string& stem, ProjectStorage& st,
                     convert::Output& out, convert::Stats& stats,
                     V3StreamResult& result, std::string& err,
                     bool (*onProgress)(void*, u64, u64), void* user) {
    result = V3StreamResult{};
    if (bin == nullptr || binLen == 0) {
        err = "o conversor streaming precisa do buffer mmapado (o chamador "
              "mmapa o GLB/irmão .bin — sem ficheiro real não há streaming)";
        return false;
    }
    if (model.primRefs.empty()) {
        err = "o conversor streaming recebeu um modelo sem primitivas";
        return false;
    }
    const Json* jviews = doc.find("bufferViews");
    const Json* jaccs = doc.find("accessors");
    if (jviews == nullptr || jaccs == nullptr) {
        err = "glTF sem bufferViews/accessors";
        return false;
    }

    // ---- 1. as TAREFAS por primitiva (a ordem dos nós; material
    // agrupado). A resolução de accessor que falha por LIMITES degrada
    // (o contrato R-014: a primitiva sai com W — o resto entra); a
    // estrutura inválida (índices não múltiplos de 3) é FATAL, como no
    // caminho de sempre.
    std::vector<TaskCtx> tasks;
    tasks.reserve(model.primRefs.size());
    std::map<std::string, u32> matOrder;
    std::vector<std::string> matNames;
    u32 droppedPrims = 0;
    std::string lastDropCause;   // a causa da ÚLTIMA queda (o contrato
                                 // R-032: a comparação completa nunca
                                 // truncada — a mesma do caminho de sempre)
    for (size_t i = 0; i < model.primRefs.size(); ++i) {
        const GltfPrimRef& r = model.primRefs[i];
        TaskCtx t;
        t.taskIdx = static_cast<u32>(tasks.size());
        t.cap = kGmeshV3BlockVertexCap;
        t.posCount = r.posCount;
        std::string aerr;
        bool ok = true;
        if (r.posAcc < 0 ||
            r.posAcc >= static_cast<i32>(jaccs->items.size()) ||
            !resolveAccessor(jaccs->items[static_cast<size_t>(r.posAcc)],
                             *jviews, binLen, 3, 4, t.pos, aerr)) {
            ok = false;
            aerr = "POSITION: " + aerr;
        }
        if (ok && r.nrmAcc >= 0) {
            if (r.nrmAcc >= static_cast<i32>(jaccs->items.size()) ||
                !resolveAccessor(
                    jaccs->items[static_cast<size_t>(r.nrmAcc)], *jviews,
                    binLen, 3, 4, t.nrm, aerr)) {
                ok = false;
                aerr = "NORMAL: " + aerr;
            } else {
                t.hasNrm = true;
            }
        }
        if (ok && r.uvAcc >= 0) {
            if (r.uvAcc >= static_cast<i32>(jaccs->items.size()) ||
                !resolveAccessor(
                    jaccs->items[static_cast<size_t>(r.uvAcc)], *jviews,
                    binLen, 2, 4, t.uv, aerr)) {
                ok = false;
                aerr = "TEXCOORD_0: " + aerr;
            } else {
                t.hasUv = true;
            }
        }
        if (ok && r.idxAcc >= 0) {
            u32 comp = 5123;
            if (r.idxAcc < static_cast<i32>(jaccs->items.size())) {
                if (const Json* ct =
                        jaccs->items[static_cast<size_t>(r.idxAcc)]
                            .find("componentType");
                    ct && ct->type == Json::Type::Number) {
                    comp = static_cast<u32>(ct->number);
                }
            }
            if (r.idxAcc >= static_cast<i32>(jaccs->items.size()) ||
                !resolveAccessor(
                    jaccs->items[static_cast<size_t>(r.idxAcc)], *jviews,
                    binLen, 1, comp == 5125 ? 4u : 2u, t.idx, aerr)) {
                ok = false;
                aerr = "indices: " + aerr;
            } else if (t.idx.count % 3 != 0) {
                // estrutura inválida — FATAL (o mesmo veredito do merge)
                err = "primitiva " + std::to_string(i) +
                      ": o accessor de índices tem " +
                      std::to_string(t.idx.count) +
                      " entradas — não é múltiplo de 3 (triângulos)";
                return false;
            } else {
                t.hasIdx = true;
            }
        }
        if (ok && !t.hasIdx && t.posCount % 3 != 0) {
            err = "primitiva " + std::to_string(i) + " não indexada com " +
                  std::to_string(t.posCount) +
                  " vértices — não é múltiplo de 3 (triângulos)";
            return false;
        }
        if (!ok) {
            ++droppedPrims;
            // a linha diag do view (o MESMO contrato R-032 do caminho de
            // sempre: off/len/acc + declared/real/file — nunca truncada)
            std::string diag;
            const i32 accIdx =
                aerr.compare(0, 9, "POSITION:") == 0
                    ? r.posAcc
                    : aerr.compare(0, 7, "NORMAL:") == 0
                          ? r.nrmAcc
                          : aerr.compare(0, 11, "TEXCOORD_0:") == 0
                                ? r.uvAcc
                                : r.idxAcc;
            if (accIdx >= 0 &&
                accIdx < static_cast<i32>(jaccs->items.size())) {
                const Json& ja = jaccs->items[static_cast<size_t>(accIdx)];
                u64 accOff = 0;
                if (const Json* o2 = ja.find("byteOffset");
                    o2 && o2->type == Json::Type::Number) {
                    accOff = static_cast<u64>(o2->number);
                }
                i32 vIdx = -1;
                if (const Json* bv = ja.find("bufferView");
                    bv && bv->type == Json::Type::Number) {
                    vIdx = static_cast<i32>(bv->number);
                }
                u64 vOff = 0, vLen2 = 0;
                if (vIdx >= 0 &&
                    vIdx < static_cast<i32>(jviews->items.size())) {
                    const Json& vw = jviews->items[static_cast<size_t>(vIdx)];
                    if (const Json* o3 = vw.find("byteOffset");
                        o3 && o3->type == Json::Type::Number) {
                        vOff = static_cast<u64>(o3->number);
                    }
                    if (const Json* l3 = vw.find("byteLength");
                        l3 && l3->type == Json::Type::Number) {
                        vLen2 = static_cast<u64>(l3->number);
                    }
                }
                u64 declared = 0;
                const Json* jbufs = doc.find("buffers");
                if (jbufs && !jbufs->items.empty()) {
                    if (const Json* bl3 = jbufs->items[0].find("byteLength");
                        bl3 && bl3->type == Json::Type::Number) {
                        declared = static_cast<u64>(bl3->number);
                    }
                }
                char db[256];
                std::snprintf(db, sizeof(db),
                              "glb: view%d buffer0 off=%llu len=%llu "
                              "acc=%llu declared=%llu real=%llu file=%llu",
                              vIdx, static_cast<unsigned long long>(vOff),
                              static_cast<unsigned long long>(vLen2),
                              static_cast<unsigned long long>(accOff),
                              static_cast<unsigned long long>(declared),
                              static_cast<unsigned long long>(binLen),
                              static_cast<unsigned long long>(fileBytes));
                diag = db;
            }
            elog::warn("import: primitiva %zu LARGADA no caminho streaming "
                       "(%s) — %s (o RESTO do modelo entra)",
                       i, aerr.c_str(),
                       diag.empty() ? aerr.c_str() : diag.c_str());
            lastDropCause = diag.empty() ? aerr : diag;
            continue;
        }
        t.material =
            r.material >= 0 &&
                    r.material < static_cast<i32>(model.materials.size())
                ? model.materials[static_cast<size_t>(r.material)].name
                : "";
        if (matOrder.find(t.material) == matOrder.end()) {
            matOrder[t.material] = static_cast<u32>(matNames.size());
            matNames.push_back(t.material);
        }
        t.materialOrder = matOrder[t.material];
        // o BASE do buffer (os offsets dos accessors são daqui) — o mmap
        // do chamador, já no início do chunk BIN (GLB) ou do irmão .bin
        t.base = bin;
        // a transformação MUNDO do nó (a MESMA composição do merge
        // antigo — a verificação reavalia EXATAMENTE esta multiplicação)
        if (r.node >= 0 &&
            r.node < static_cast<i32>(model.nodes.size())) {
            const i32 nodeIdx = r.node;
            Mat4 pw = Mat4::identity();
            Mat4 rw = Mat4::identity();
            std::vector<u32> chain;
            i32 k = nodeIdx;
            while (k >= 0 && k < static_cast<i32>(model.nodes.size())) {
                chain.push_back(static_cast<u32>(k));
                k = model.nodes[static_cast<size_t>(k)].parent;
            }
            for (size_t c2 = chain.size(); c2-- > 0;) {
                const GltfNode& n2 = model.nodes[chain[c2]];
                const Mat4 local =
                    Mat4::mul(Mat4::translation(n2.translation.x,
                                                n2.translation.y,
                                                n2.translation.z),
                              Mat4::mul(n2.rotation.toMat4(),
                                        Mat4::scale(n2.scale.x, n2.scale.y,
                                                    n2.scale.z)));
                pw = Mat4::mul(pw, local);
                rw = Mat4::mul(rw, n2.rotation.toMat4());
            }
            t.world = pw;
            t.worldRot = rw;
        }
        t.trisTotal = t.hasIdx ? t.idx.count / 3 : t.pos.count / 3;
        // o span da fonte desta tarefa (a higiene de RSS larga-o no fim)
        u64 mn = t.pos.offset, mx = t.pos.offset + u64(t.posCount) * t.pos.stride;
        auto spanOf = [&mn, &mx](const AccInfo& a) {
            const u64 lo = a.offset;
            const u64 hi = a.offset + a.count * a.stride;
            mn = (std::min)(mn, lo);
            mx = (std::max)(mx, hi);
        };
        if (t.hasNrm) spanOf(t.nrm);
        if (t.hasUv) spanOf(t.uv);
        if (t.hasIdx) spanOf(t.idx);
        t.spanOff = mn;
        t.spanLen = mx > mn ? mx - mn : 0;
        tasks.push_back(std::move(t));
    }
    if (tasks.empty()) {
        err = "glTF: TODAS as primitivas ficaram fora dos limites do "
              "buffer — " + lastDropCause;
        return false;
    }
    stats.primWarn += droppedPrims;
    u64 totalTris = 0;
    for (const TaskCtx& t : tasks) totalTris += t.trisTotal;
    if (totalTris == 0) {
        err = "o modelo não tem triângulos (primitivas vazias)";
        return false;
    }
    elog::info("asset: v3 streaming '%s' — %u tarefa(s), %llu triângulo(s), "
               "%u material(is), %u primitiva(s) largada(s)",
               stem.c_str(), static_cast<u32>(tasks.size()),
               static_cast<unsigned long long>(totalTris),
               static_cast<u32>(matNames.size()), droppedPrims);

    // ---- 2. o TEMP (os blocos appendam-se; o assembly ordena) ---------
    // A CASCATA do stagingWrite (0.8.12 — a convenção da casa; o C33
    // apanhou a falta: o FakeStorage do harness "aceita" o makeDirs mas
    // o fopen morre ENOENT na raiz virtual /fake): (1) .staging/ do
    // projeto quando a raiz é um caminho POSIX REAL; (2) o cache dir da
    // app via JNI — o único sítio GUARANTIDO no Android sem permissões;
    // (3) erro LEGÍVEL com os caminhos tentados. Cada falha LOGA a
    // causa. /tmp NUNCA (o gate do CI vigia).
    TempStore temp;
    char nmTmp[64];
    std::snprintf(nmTmp, sizeof(nmTmp), "/goni_v3stream_%d.tmp",
                  static_cast<int>(::getpid()));
    const std::string root = st.root();
    bool haveTemp = false;
    std::string tried;
    if (!root.empty() && root.rfind("content://", 0) != 0) {
        st.makeDirs(".staging");   // o real cria; o virtual "aceita" —
                                   // o fopen é o veredito, não o makeDirs
        const std::string p = joinRelPath(root, ".staging") + nmTmp;
        std::string oerr;
        if (temp.open(p, oerr)) {
            haveTemp = true;
            elog::info("asset: v3 temp em '%s' (projeto)", p.c_str());
        } else {
            elog::warn("asset: v3 temp em '%s' FALHOU (%s) — a tentar o "
                       "cache dir da app",
                       p.c_str(), fileapi::errnoText().c_str());
            tried = ".staging/ do projeto (raiz '" + root + "': " + oerr +
                    ")";
        }
    }
    if (!haveTemp) {
        const std::string cache = storage::jniCacheDir();
        if (!cache.empty()) {
            const std::string p = cache + "/.staging" + nmTmp;
            std::string oerr;
            if (fileapi::makeDirs(cache + "/.staging") &&
                temp.open(p, oerr)) {
                haveTemp = true;
                elog::info("asset: v3 temp em '%s' (cache dir)",
                           p.c_str());
            } else {
                elog::warn("asset: v3 temp no cache dir '%s' FALHOU (%s)",
                           p.c_str(), fileapi::errnoText().c_str());
                tried += tried.empty() ? "" : " e ";
                tried += "o cache dir '" + cache + "' (" + oerr + ")";
            }
        } else {
            tried += tried.empty() ? "" : " e ";
            tried +=
                "o cache dir da app (a ponte Java não deu cacheDirPath)";
        }
    }
    if (!haveTemp) {
        err = "o temporário do streaming não abriu: nem " + tried +
              " aceitou a escrita — o espaço e as permissões são o "
              "limite, não o modelo";
        return false;
    }
    // a limpeza é GARANTIDA em TODAS as saídas (a lição do wiring010: o
    // disco da casa nunca fica sujo — nem em erro, nem em cancelamento)
    struct TempGuard {
        TempStore& t;
        ~TempGuard() {
            t.close();
            ::remove(t.abs.c_str());
        }
    } tg{temp};

    // ---- 3. o CORTE paralelo (núcleos−1; o disco ordena, o assembly
    // decide a ordem final). A RAM de pico = remap da primitiva em voo +
    // UM bloco por worker (a fórmula declarada no relatório).
    std::atomic<size_t> nextTask{0};
    std::atomic<bool> canceled{false};
    std::atomic<u64> trisDone{0};
    std::mutex resMu;
    std::string workerErr;
    std::vector<std::vector<BlockRec>> recs(tasks.size());
    const u32 nWorkers =
        v3StreamWorkerCount(std::thread::hardware_concurrency());
    if (nWorkers == 1) {
        elog::info("asset: v3 corte em MODO SEQUENCIAL (1 worker — o "
                   "host não deu núcleos úteis; mais lento, correto)");
    }
    const auto tCut = std::chrono::steady_clock::now();
    auto worker = [&]() {
        try {
            for (;;) {
                if (canceled.load(std::memory_order_relaxed)) return;
                const size_t i = nextTask.fetch_add(1);
                if (i >= tasks.size()) return;
                CutCtx cut{&temp, &recs[i]};
                std::string werr;
                if (!cutPrimitive(&tasks[i], trisDone, totalTris, canceled,
                                  tasks[i].taskIdx, &cutSink, &cut,
                                  onProgress, user, werr)) {
                    std::lock_guard<std::mutex> lk(resMu);
                    if (workerErr.empty()) workerErr = werr;
                    canceled.store(true, std::memory_order_relaxed);
                    return;
                }
            }
        } catch (const std::bad_alloc&) {
            std::lock_guard<std::mutex> lk(resMu);
            if (workerErr.empty()) {
                workerErr = "sem memória para o corte (a RAM é o limite "
                            "declarado; o modelo não tem teto de "
                            "vértices — divide primitivas muito grandes)";
            }
            canceled.store(true, std::memory_order_relaxed);
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lk(resMu);
            if (workerErr.empty()) workerErr = e.what();
            canceled.store(true, std::memory_order_relaxed);
        }
    };
    {
        std::vector<std::thread> threads;
        threads.reserve(nWorkers);
        for (u32 w = 0; w < nWorkers; ++w) threads.emplace_back(worker);
        for (std::thread& th : threads) th.join();
    }
    if (!workerErr.empty()) {
        if (workerErr == "cancelado") {
            stats.canceled = true;
            err = "cancelado (o import foi interrompido)";
        } else {
            err = "a conversão streaming falhou: " + workerErr;
        }
        return false;
    }
    result.cutMs = std::chrono::duration<double, std::milli>(
                       std::chrono::steady_clock::now() - tCut)
                       .count();

    // ---- 4. o ASSEMBLY (ordena material→nó→bloco; escreve o final pela
    // escrita streaming do storage — a ESQUELETO v3 partilhada com o
    // writeGMesh garante o MESMO layout byte a byte)
    const auto tAsm = std::chrono::steady_clock::now();
    std::vector<BlockRec> sorted;
    {
        size_t total = 0;
        for (const auto& r : recs) total += r.size();
        sorted.reserve(total);
        for (size_t tsk = 0; tsk < recs.size(); ++tsk) {
            for (const BlockRec& r : recs[tsk]) sorted.push_back(r);
        }
        recs.clear();
        recs.shrink_to_fit();
    }
    std::sort(sorted.begin(), sorted.end(),
              [](const BlockRec& a, const BlockRec& b) {
                  if (a.materialOrder != b.materialOrder) {
                      return a.materialOrder < b.materialOrder;
                  }
                  if (a.taskIdx != b.taskIdx) return a.taskIdx < b.taskIdx;
                  return a.blockIdx < b.blockIdx;
              });
    if (sorted.empty()) {
        err = "o modelo não produziu blocos (primitivas sem triângulos)";
        return false;
    }
    // o mapa (taskIdx → 1.ª posição na ordem final) — a verificação
    // re-corta por tarefa e precisa do sítio de cada bloco
    std::vector<u32> taskStart(tasks.size(), 0xFFFFFFFFu);
    for (size_t p = 0; p < sorted.size(); ++p) {
        const u32 tsk = sorted[p].taskIdx;
        if (taskStart[tsk] == 0xFFFFFFFFu) taskStart[tsk] = static_cast<u32>(p);
    }
    // os totais + a AABB global + as entradas da tabela
    GMeshV3Meta meta;
    meta.flags = 0;   // o streaming não tem skin (a pele segue o caminho
                      // de sempre — decisão do PASSO 3, no relatório)
    meta.blockVertexCap = kGmeshV3BlockVertexCap;
    meta.attrs[0] = GMeshV3Attr{kAttrPosition, kAttrF32, 0, 0, 0};
    meta.attrs[1] = GMeshV3Attr{kAttrNormal, kAttrF32, 0, 0, 0};
    meta.attrs[2] = GMeshV3Attr{kAttrUv0, kAttrF32, 0, 0, 0};
    meta.attrCount = 3;
    std::vector<GMeshV3Block> entries(sorted.size());
    u64 totalV = 0, totalI = 0;
    Vec3 gmn{1e30f, 1e30f, 1e30f}, gmx{-1e30f, -1e30f, -1e30f};
    for (size_t p = 0; p < sorted.size(); ++p) {
        const BlockRec& r = sorted[p];
        GMeshV3Block& e = entries[p];
        e.dataSize = u64(r.poolVerts) * 32 + u64(r.idxCount) * 2;
        e.vertexCount = r.poolVerts;
        e.indexCount = r.idxCount;
        e.aabbMin = r.aabbMin;
        e.aabbMax = r.aabbMax;
        e.materialIndex = r.materialOrder;
        e.indexType = 0;   // a pool ≤ cap (65535) → índices locais u16
        e.crc32 = r.crc;
        totalV += r.poolVerts;
        totalI += r.idxCount;
        gmn.x = (std::min)(gmn.x, r.aabbMin.x);
        gmn.y = (std::min)(gmn.y, r.aabbMin.y);
        gmn.z = (std::min)(gmn.z, r.aabbMin.z);
        gmx.x = (std::max)(gmx.x, r.aabbMax.x);
        gmx.y = (std::max)(gmx.y, r.aabbMax.y);
        gmx.z = (std::max)(gmx.z, r.aabbMax.z);
    }
    meta.vertexCount = totalV;
    meta.indexCount = totalI;
    meta.materialCount = matNames.size();
    meta.aabbMin = gmn;
    meta.aabbMax = gmx;
    GMeshV3Skeleton sk;
    if (!gmeshV3Skeleton(meta, entries, matNames, sk, err)) {
        return false;
    }
    // o LIMITE final no log (o MESMO marcador do caminho de sempre — as
    // fases do relatório 0.10-M leem estes timestamps)
    {
        const f32 dims[3] = {gmx.x - gmn.x, gmx.y - gmn.y, gmx.z - gmn.z};
        const f32 maxDim =
            (std::max)(dims[0], (std::max)(dims[1], dims[2]));
        elog::info("asset: limites finais min(%.3f %.3f %.3f) max(%.3f "
                   "%.3f %.3f) — dimensões %.3f x %.3f x %.3f (unidades)",
                   (double)gmn.x, (double)gmn.y, (double)gmn.z,
                   (double)gmx.x, (double)gmx.y, (double)gmx.z,
                   (double)dims[0], (double)dims[1], (double)dims[2]);
        if (maxDim > 10000.0f) {
            elog::warn("asset: o modelo importado mede %.1f unidades — "
                       "grande de mais para a câmara padrão (aproxima ou "
                       "reduz a escala; NADA foi escalado automaticamente)",
                       (double)maxDim);
        } else if (maxDim > 0.0f && maxDim < 0.001f) {
            elog::warn("asset: o modelo importado mede %.4f unidades — "
                       "pequeno de mais para a câmara padrão (afasta ou "
                       "aumenta a escala; NADA foi escalado automaticamente)",
                       (double)maxDim);
        }
    }
    // ---- a escrita STREAMING (chunks de 1 MB do temporário — o
    // ficheiro final NUNCA está inteiro em RAM)
    const std::string outRel = "assets/" + stem + ".gmesh";
    const int wh = st.openWriteStream(outRel);
    if (wh <= 0) {
        err = "não consegui abrir '" + outRel +
              "' para escrita streaming (o armazenamento é o limite)";
        return false;
    }
    u64 written = 0;
    const u64 outFileBytes = meta.materialTableOffset + sk.materials.size();
    auto failWrite = [&](const std::string& why) {
        st.closeWriteStream(wh);
        st.remove(outRel);   // sem estado parcial (a regra da casa)
        err = why;
        return false;
    };
    auto wchunk = [&](const void* p, size_t n, const char* what) -> bool {
        if (n == 0) return true;
        if (!st.writeStreamChunk(wh, p, n)) {
            return false;
        }
        written += n;
        if (onProgress != nullptr && (written & (kStreamChunk - 1)) < n) {
            if (!onProgress(user, written, outFileBytes)) {
                canceled.store(true, std::memory_order_relaxed);
            }
        }
        (void)what;
        return true;
    };
    if (canceled.load(std::memory_order_relaxed) ||
        (onProgress != nullptr && !onProgress(user, 0, outFileBytes))) {
        return failWrite("cancelado (o import foi interrompido)");
    }
    if (!wchunk(sk.header.data(), sk.header.size(), "header") ||
        !wchunk(sk.meta.data(), sk.meta.size(), "meta")) {
        return failWrite("a escrita de '" + outRel +
                         "' falhou no cabeçalho (errno " +
                         std::to_string(errno) + ")");
    }
    // os blocos: o temporário lê-se POR CHUNKS e o CRC re-confere em
    // voo (um temporário corrompido morre AQUI, nomeado — nunca no
    // device a meio de um render)
    static const u8 kZero[16] = {0};
    std::vector<u8> blob;
    blob.reserve(static_cast<size_t>(kGmeshV3BlockVertexCap) * 32 +
                 kGmeshV3BlockVertexCap * 6);
    u64 cursor = kGHeaderBytes + kGmeshV3MetaBytes;
    for (size_t p = 0; p < sorted.size(); ++p) {
        const BlockRec& r = sorted[p];
        if (entries[p].dataOffset > cursor) {
            const u64 pad = entries[p].dataOffset - cursor;
            if (!wchunk(kZero, static_cast<size_t>(pad), "pad") ||
                canceled.load(std::memory_order_relaxed)) {
                return failWrite("cancelado (o import foi interrompido)");
            }
        }
        cursor = entries[p].dataOffset + entries[p].dataSize;
        if (std::fseek(temp.f, static_cast<long>(r.tempOff), SEEK_SET) != 0) {
            return failWrite("o seek no temporário falhou (bloco #" +
                             std::to_string(p) + ")");
        }
        blob.resize(static_cast<size_t>(entries[p].dataSize));
        size_t got = 0;
        while (got < blob.size()) {
            const size_t want =
                (std::min)(kStreamChunk, blob.size() - got);
            const size_t n = std::fread(blob.data() + got, 1, want, temp.f);
            if (n == 0) {
                return failWrite("o temporário acabou cedo (bloco #" +
                                 std::to_string(p) + ", errno " +
                                 std::to_string(errno) + ")");
            }
            got += n;
        }
        if (gcrc32(blob.data(), blob.size()) != r.crc) {
            return failWrite("o bloco #" + std::to_string(p) +
                             " corrompeu no temporário (o CRC diverge) — "
                             "o disco é o suspeito, não o modelo");
        }
        if (!wchunk(blob.data(), blob.size(), "bloco")) {
            return failWrite("a escrita de '" + outRel +
                             "' falhou no bloco #" + std::to_string(p) +
                             " (errno " + std::to_string(errno) +
                             (errno == ENOSPC
                                  ? " — DISCO CHEIO (o espaço livre é o "
                                    "limite, não o modelo)"
                                  : "") +
                             ")");
        }
        if (canceled.load(std::memory_order_relaxed)) {
            return failWrite("cancelado (o import foi interrompido)");
        }
    }
    // o PAD de 16-alinhamento antes da tabela (o MESMO vão que o
    // writeGMesh deixa — a esqueleto decidiu, a emissão respeita)
    if (meta.blockTableOffset > cursor) {
        if (!wchunk(kZero, static_cast<size_t>(meta.blockTableOffset - cursor),
                    "pad")) {
            return failWrite("a escrita de '" + outRel +
                             "' falhou no alinhamento (errno " +
                             std::to_string(errno) + ")");
        }
    }
    if (!wchunk(sk.table.data(), sk.table.size(), "tabela") ||
        !wchunk(sk.materials.data(), sk.materials.size(), "materiais")) {
        return failWrite("a escrita de '" + outRel +
                         "' falhou na tabela (errno " +
                         std::to_string(errno) + ")");
    }
    st.closeWriteStream(wh);
    // o temporário MORRE AQUI (a verificação só precisa da fonte e do
    // final — o disco não segura 2× o modelo durante o re-corte)
    temp.close();
    ::remove(temp.abs.c_str());
    result.assembleMs = std::chrono::duration<double, std::milli>(
                            std::chrono::steady_clock::now() - tAsm)
                            .count();
    elog::info("asset: %s verts=%llu idx=%llu blocos=%llu — registado na "
               "lista como %s",
               outRel.c_str(), static_cast<unsigned long long>(totalV),
               static_cast<unsigned long long>(totalI),
               static_cast<unsigned long long>(sorted.size()),
               outRel.c_str());

    // ---- 5. a VERIFICAÇÃO (obrigatória — a spec do dono): relê o
    // final e compara POR TRIÂNGULO com a origem reavaliando o MESMO
    // bake (bit a bit). A VIA do readback: o final com caminho POSIX
    // REAL vem por mmap (zero RAM); num storage VIRTUAL (o harness —
    // raiz sem fopen) o final relê PELO STORAGE (o caminho do runtime)
    // — a comparação é a MESMA, bit a bit.
    const auto tVer = std::chrono::steady_clock::now();
    const std::string outAbs = joinRelPath(root, outRel);
    u64 outBytes = 0;
    const bool finalOnDisk = fileapi::fileSize(outAbs.c_str(), outBytes);
    unsigned long long vLen = 0;
    u8* vmap = nullptr;
    std::vector<u8> vbytes;   // o readback pelo storage (via virtual)
    if (finalOnDisk) {
        // mapFile64 fala `unsigned long long` (FileApi) — acompanha
        vmap = static_cast<u8*>(
            fileapi::mapFile64(outAbs.c_str(), 0, outBytes, &vLen));
        if (vmap == nullptr) {
            err = "o mmap do ficheiro final falhou: " + outAbs;
            return false;
        }
        stats.outputBytes += outBytes;
    } else {
        elog::warn("asset: o final '%s' não tem caminho POSIX (storage "
                   "virtual) — a verificação relê PELO STORAGE",
                   outRel.c_str());
        if (!st.readBytes(outRel, vbytes) || vbytes.empty()) {
            err = "o ficheiro final '" + outRel +
                  "' não relê do storage após a escrita";
            return false;
        }
        vLen = vbytes.size();
        stats.outputBytes += vbytes.size();
    }
    const u8* vbase = vmap != nullptr ? vmap : vbytes.data();
    std::string dev;
    {
        GMeshV3Meta vmeta;
        std::vector<GMeshV3Block> ventries;
        std::vector<std::string> vmats;
        if (!readGMeshV3Meta(vbase, static_cast<size_t>(vLen), vmeta,
                             ventries, vmats, err)) {
            fileapi::unmapFile64(vmap, vLen);
            st.remove(outRel);
            err = "o ficheiro que acabei de escrever não relê: " + err;
            return false;
        }
        if (vmeta.vertexCount != meta.vertexCount ||
            vmeta.indexCount != meta.indexCount ||
            vmeta.blockCount != meta.blockCount ||
            vmeta.materialCount != meta.materialCount ||
            vmats != matNames) {
            dev = "as contagens do ficheiro final divergem do assembly";
        }
        if (dev.empty()) {
            // o re-corte POR TAREFA (a MESMA rotina; o sink compara)
            std::atomic<u64> verDone{0};
            for (size_t tsk = 0; tsk < tasks.size(); ++tsk) {
                if (taskStart[tsk] == 0xFFFFFFFFu) continue;
                VerifyCtx vc;
                vc.map = vbase;
                vc.droppable = vmap != nullptr;
                vc.mapLen = vLen;
                vc.meta = &vmeta;
                vc.entries = &ventries;
                vc.taskStart = &taskStart;
                vc.taskIdx = static_cast<u32>(tsk);
                vc.dev = &dev;
                std::string werr;
                if (!cutPrimitive(&tasks[tsk], verDone, totalTris,
                                  canceled, tasks[tsk].taskIdx,
                                  &verifySink, &vc, onProgress, user,
                                  werr)) {
                    if (dev.empty()) {
                        dev = werr == "cancelado"
                                  ? "cancelado"
                                  : "o re-corte da tarefa " +
                                        std::to_string(tsk) + " falhou: " +
                                        werr;
                    }
                    break;
                }
                // o span da fonte desta tarefa larga-se (higiene de RSS)
                dropRange(bin, tasks[tsk].spanOff, tasks[tsk].spanLen);
            }
        }
    }
    if (vmap != nullptr) {
        fileapi::unmapFile64(vmap, vLen);
    }
    if (dev.empty() && canceled.load(std::memory_order_relaxed)) {
        st.remove(outRel);
        stats.canceled = true;
        err = "cancelado (o import foi interrompido)";
        return false;
    }
    if (!dev.empty()) {
        elog::error("gmesh: v3 verificação FALHOU — %s", dev.c_str());
        st.remove(outRel);   // um ficheiro VERIFICADAMENTE errado não fica
        stats.canceled = dev == "cancelado";
        err = dev == "cancelado" ? "cancelado (o import foi interrompido)"
                                 : "a verificação do .gmesh v3 falhou: " +
                                       dev;
        return false;
    }
    result.verified = true;
    result.verifyMs = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - tVer)
                          .count();
    result.blocks = sorted.size();
    result.verts = totalV;
    result.tris = totalI / 3;
    result.firstDeviation.clear();
    // A LINHA CONTRATO da spec do dono
    elog::info("gmesh: v3 blocos=%llu verts=%llu tris=%llu verificado=1",
               static_cast<unsigned long long>(result.blocks),
               static_cast<unsigned long long>(result.verts),
               static_cast<unsigned long long>(result.tris));
    // os stats (a saturação u32 é honesta — o warn acompanha; as
    // contagens REAIS vivem no formato, em 64 bits)
    constexpr u64 kU32Max = 0xFFFFFFFFull;
    stats.verts = totalV > kU32Max ? kU32Max : static_cast<u32>(totalV);
    stats.indices = totalI > kU32Max ? kU32Max : static_cast<u32>(totalI);
    if (totalV > kU32Max) {
        elog::warn("asset: %llu vértices excedem o contador do toast "
                   "(u32) — as contagens do FICHEIRO estão corretas (64 "
                   "bits no formato; a linha gmesh: v3 acima)",
                   static_cast<unsigned long long>(totalV));
    }
    stats.meshes = 1;
    out.meshes.push_back(outRel);
    return true;
}

} // namespace vv
