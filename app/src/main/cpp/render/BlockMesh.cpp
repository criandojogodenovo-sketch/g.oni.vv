// render/BlockMesh.cpp — 0.10-M (PASSO 4): a implementação do render por
// blocos (abertura por faixas, frustum por bloco, lazy load, LRU com
// orçamentos, hull de bounds).
//
// A ABERTURA é pura até ao último byte: peek (192 B) → meta validada →
// faixa da tabela (blockCount×80 B) → faixa dos materiais. NENHUM dado de
// bloco é lido — o `import` que aplica um mesh de 972 MB ao TIC custa
// ~192 B + tabela (~75 KB para 920 blocos) + materiais.
#include "render/BlockMesh.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>

#include "core/ProjectStorage.h"
#include "platform/EngineLog.h"
#include "platform/Log.h"
#include "render/Mesh.h"
#include "render/Renderer.h"
#include "render/Texture.h"

namespace vv {

// teto da faixa de materiais lida de uma vez (nomes 4-alinhados — 1 MB é
// ~10k materiais de ~100 chars: muito além de qualquer modelo real; um
// ficheiro que precise de mais é recusado com erro legível)
constexpr size_t kMaterialsRangeMax = 1ull * 1024 * 1024;

u64 blockMeshVramBytes(const MeshData& block) {
    u64 n = static_cast<u64>(block.vertices.size()) * sizeof(Vertex) +
            static_cast<u64>(block.indices.size()) * 2u;
    if (block.skinned()) {
        n += static_cast<u64>(block.vertices.size()) * 20u;   // 4 u8 + 4 f32
    }
    return n;
}

BlockMesh::~BlockMesh() {
    close();
}

void BlockMesh::close() {
    destroyGpu();
    entries_.clear();
    blocks_.clear();
    materials_.clear();
    storage_ = nullptr;
    path_.clear();
    opened_ = false;
    frame_ = 0;
    evictedTotal_ = 0;
    uploadFails_ = 0;
    uploadDead_ = false;
    last_ = Stats{};
    churnWarned_ = false;
}

void BlockMesh::destroyGpu() {
    for (Entry& e : entries_) {
        if (e.gpu) {
            e.gpu->destroy();
            e.gpu.reset();
        }
        e.vramBytes = 0;
    }
}

bool BlockMesh::open(ProjectStorage& st, const std::string& relPath,
                     std::string& err) {
    close();
    const auto t0 = std::chrono::steady_clock::now();

    // (1) o PEEK de sempre (192 B) — o MESMO guard do load inteiro
    std::vector<u8> peek;
    if (!st.readBytesAt(relPath, 0, kGHeaderBytes + kGmeshV3MetaBytes,
                        peek)) {
        err = "ficheiro não encontrado (ou faixa do header ilegível): " +
              relPath;
        return false;
    }
    if (!gmeshV3PeekMeta(peek.data(), peek.size(), meta_, err)) {
        err = relPath + ": " + err;
        return false;
    }

    // (2) a TABELA pela faixa EXATA (nunca os dados)
    {
        const u64 tableLen =
            meta_.blockCount * static_cast<u64>(kGmeshV3BlockEntryBytes);
        if (tableLen > kMaterialsRangeMax * 64ull) {
            // defesa: blocos demais para a faixa declarada (920 blocos =
            // 73,6 KB; o teto absurdo trava lixo antes de alocar)
            err = "tabela de blocos absurda (" +
                  std::to_string(meta_.blockCount) + " entradas)";
            return false;
        }
        std::vector<u8> tableBytes;
        if (!st.readBytesAt(relPath, meta_.blockTableOffset,
                            static_cast<size_t>(tableLen), tableBytes)) {
            err = "faixa da tabela de blocos ilegível (offset " +
                  std::to_string(meta_.blockTableOffset) + ", " +
                  std::to_string(tableLen) + " B): " + relPath;
            return false;
        }
        if (!gmeshV3ReadTableOnly(tableBytes.data(), tableBytes.size(), meta_,
                                  blocks_, err)) {
            err = relPath + ": " + err;
            return false;
        }
    }

    // (3) os MATERIAIS pela faixa deles: com statBytes é a cauda EXATA;
    //    sem stat (SAF), prova faixas decrescentes até os nomes caberem
    {
        std::vector<u8> matBytes;
        bool got = false;
        u64 fileBytes = 0;
        if (st.statBytes(relPath, fileBytes) &&
            fileBytes >= meta_.materialTableOffset) {
            const u64 tail = fileBytes - meta_.materialTableOffset;
            if (tail > kMaterialsRangeMax) {
                err = "tabela de materiais absurda (" + std::to_string(tail) +
                      " B — o .gmesh v3 da casa escreve nomes)";
                return false;
            }
            got = st.readBytesAt(relPath, meta_.materialTableOffset,
                                 static_cast<size_t>(tail), matBytes);
        } else {
            // SAF (sem stat): 1 MB → 64 KB → 4 KB → 256 B; o reader para
            // em materialCount nomes — bytes a mais no fim são inofensivos
            static const size_t kProbes[] = {1024u * 1024u, 64u * 1024u,
                                             4u * 1024u, 256u};
            for (size_t probe : kProbes) {
                if (st.readBytesAt(relPath, meta_.materialTableOffset, probe,
                                   matBytes)) {
                    got = true;
                    break;
                }
            }
        }
        if (!got || !gmeshV3ReadMaterialsOnly(matBytes.data(),
                                              matBytes.size(), meta_,
                                              materials_, err)) {
            err = relPath + ": faixa dos materiais ilegível — " + err;
            return false;
        }
    }

    storage_ = &st;
    path_ = relPath;
    entries_.resize(blocks_.size());   // Entry move-only: resize constrói
    opened_ = true;

    // a linha contrato do LOAD (o par das fases do conversor): a abertura
    // É o load do runtime por blocos — a tabela, nunca os dados
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - t0)
                          .count();
    const u64 dataBytes = [&]() {
        u64 n = 0;
        for (const GMeshV3Block& b : blocks_) {
            n += b.dataSize;
        }
        return n;
    }();
    elog::info(
        "gmesh: fase=load ms=%.1f (por BLOCOS: tabela de %u blocos, %zu B "
        "lidos; dados=%llu B por carregar em lazy)",
        ms, static_cast<unsigned>(blocks_.size()),
        kGHeaderBytes + kGmeshV3MetaBytes +
            blocks_.size() * kGmeshV3BlockEntryBytes,
        static_cast<unsigned long long>(dataBytes));
    return true;
}

bool BlockMesh::createHull(Mesh& out) const {
    if (!opened_) {
        return false;
    }
    // 8 cantos do AABB global; 12 triângulos (36 índices) — um Mesh VÁLIDO
    // cujos bounds são EXATAMENTE o AABB do meta (o create calcula-os dos
    // vértices — os cantos são o próprio AABB)
    Vertex v[8];
    int vi = 0;
    for (int c = 0; c < 8; ++c) {
        v[vi].pos = Vec3{c & 1 ? meta_.aabbMax.x : meta_.aabbMin.x,
                         c & 2 ? meta_.aabbMax.y : meta_.aabbMin.y,
                         c & 4 ? meta_.aabbMax.z : meta_.aabbMin.z};
        v[vi].normal = Vec3{0.0f, 1.0f, 0.0f};
        v[vi].uv = Vec2{0.0f, 0.0f};
        ++vi;
    }
    static const u16 kHullIdx[36] = {
        0, 1, 3, 0, 3, 2,   // -Z
        5, 4, 6, 5, 6, 7,   // +Z
        4, 0, 2, 4, 2, 6,   // -X
        1, 5, 7, 1, 7, 3,   // +X
        2, 3, 7, 2, 7, 6,   // +Y
        4, 5, 1, 4, 1, 0    // -Y
    };
    return out.create(v, 8, kHullIdx, 36);
}

// ---- frustum (Gribb-Hart sobre as colunas do VP; coluna-major) -----------
// plano = linha3 ± linha_i do clip; fora = a·x+b·y+c·z+d < 0
void BlockMesh::frustumPlanes(const Mat4& vp, f32 out[6][4]) {
    const f32* m = vp.m;
    // linhas da matriz (coluna-major): linha r = (m[r], m[4+r], m[8+r], m[12+r])
    for (int p = 0; p < 6; ++p) {
        const int i = p >> 1;             // 0..2
        const f32 sign = (p & 1) ? -1.0f : 1.0f;   // par: +, ímpar: -
        f32 a = m[3] + sign * m[i];
        f32 b = m[7] + sign * m[4 + i];
        f32 c = m[11] + sign * m[8 + i];
        f32 d = m[15] + sign * m[12 + i];
        const f32 len = std::sqrt(a * a + b * b + c * c);
        if (len > 1e-12f) {
            const f32 inv = 1.0f / len;
            a *= inv;
            b *= inv;
            c *= inv;
            d *= inv;
        }
        out[p][0] = a;
        out[p][1] = b;
        out[p][2] = c;
        out[p][3] = d;
    }
}

bool BlockMesh::blockVisible(const Mat4& model, const GMeshV3Block& blk,
                             const f32 planes[6][4]) {
    // o AABB do bloco (8 cantos) transformado pelo model — testado DIRETO
    // contra cada plano: conservative-exact para OBB (rodado incluído)
    Vec3 c[8];
    for (int k = 0; k < 8; ++k) {
        const Vec3 local{k & 1 ? blk.aabbMax.x : blk.aabbMin.x,
                         k & 2 ? blk.aabbMax.y : blk.aabbMin.y,
                         k & 4 ? blk.aabbMax.z : blk.aabbMin.z};
        // model * local (coluna-major)
        const f32* m = model.m;
        c[k] = Vec3{m[0] * local.x + m[4] * local.y + m[8] * local.z + m[12],
                    m[1] * local.x + m[5] * local.y + m[9] * local.z + m[13],
                    m[2] * local.x + m[6] * local.y + m[10] * local.z +
                        m[14]};
    }
    for (int p = 0; p < 6; ++p) {
        const f32 a = planes[p][0], b = planes[p][1], c2 = planes[p][2],
                  d = planes[p][3];
        int outside = 0;
        for (int k = 0; k < 8; ++k) {
            if (a * c[k].x + b * c[k].y + c2 * c[k].z + d < 0.0f) {
                ++outside;
            }
        }
        if (outside == 8) {
            return false;   // o bloco INTEIRO está fora deste plano
        }
    }
    return true;
}

u32 BlockMesh::visibleBlocks(const Mat4& model, const Mat4& vp,
                             std::vector<u32>& out) const {
    out.clear();
    if (!opened_) {
        return 0;
    }
    f32 planes[6][4];
    frustumPlanes(vp, planes);
    for (size_t i = 0; i < blocks_.size(); ++i) {
        if (blockVisible(model, blocks_[i], planes)) {
            out.push_back(static_cast<u32>(i));
        }
    }
    return static_cast<u32>(out.size());
}

// ---- lazy load -------------------------------------------------------------

bool BlockMesh::materializeBlock(size_t index, MeshData& out,
                                  std::string& err) const {
    out = MeshData{};
    if (!opened_ || !storage_ || index >= blocks_.size()) {
        err = "bloco #" + std::to_string(index) + " fora da tabela";
        return false;
    }
    const GMeshV3Block& blk = blocks_[index];
    std::vector<u8> range;
    const auto t0 = std::chrono::steady_clock::now();
    if (!storage_->readBytesAt(path_, blk.dataOffset,
                               static_cast<size_t>(blk.dataSize), range)) {
        err = "faixa do bloco #" + std::to_string(index) +
              " ilegível (offset " + std::to_string(blk.dataOffset) + ", " +
              std::to_string(blk.dataSize) + " B)";
        return false;
    }
    const bool okDec = gmeshV3MaterializeBlock(range.data(), range.size(),
                                               meta_, blk, out, err);
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - t0)
                          .count();
    elog::info("gmesh: fase=load ms=%.1f (bloco %zu/%zu, %llu B, %u verts)",
               ms, index, blocks_.size(),
               static_cast<unsigned long long>(blk.dataSize),
               static_cast<unsigned>(blk.vertexCount));
    if (!okDec) {
        return false;
    }
    // o nome do grupo/material do bloco (o contrato do leitor inteiro)
    if (blk.materialIndex < materials_.size()) {
        if (!out.groups.empty()) {
            out.groups[0].name = "bloco " + std::to_string(index);
            out.groups[0].material = materials_[static_cast<size_t>(
                blk.materialIndex)];
        }
        out.name = path_ + "#bloco" + std::to_string(index);
    }
    return true;
}

bool BlockMesh::ensureBlock(size_t index, std::string& err) {
    Entry& e = entries_[index];
    const GMeshV3Block& blk = blocks_[index];
    if (e.gpu && e.gpu->ok()) {
        return true;   // residente em VRAM — o caminho quente (a cache
                       // CPU pode ter sido evictada: só pesa no RE-upload)
    }
    if (uploadDead_) {
        return false;   // anti retry-storm: o upload morreu 3× seguidas —
                        // um open novo (close+open) limpa a flag
    }
    // a faixa do disco (se o CPU cache foi evictado, volta ao disco)
    if (!e.cpu) {
        std::shared_ptr<MeshData> data = std::make_shared<MeshData>();
        if (!materializeBlock(index, *data, err)) {
            return false;
        }
        e.cpu = std::move(data);
        e.ramBytes = blockMeshVramBytes(*e.cpu);   // MESMA estimativa (a
                                                   // cache CPU pesa o MeshData)
    }
    // o upload (o par da fase=render do mesh único)
    const auto t0 = std::chrono::steady_clock::now();
    std::unique_ptr<Mesh> gpu = std::make_unique<Mesh>();
    const MeshData& md = *e.cpu;
    bool up;
    if (md.skinned()) {
        up = gpu->createSkinned(md.vertices.data(),
                                static_cast<u32>(md.vertices.size()),
                                md.indices.data(),
                                static_cast<u32>(md.indices.size()),
                                md.skinJoints.data(), md.skinWeights.data());
    } else {
        up = gpu->create(md.vertices.data(),
                         static_cast<u32>(md.vertices.size()),
                         md.indices.data(),
                         static_cast<u32>(md.indices.size()));
    }
    if (!up) {
        err = "upload do bloco #" + std::to_string(index) + " falhou (GL)";
        if (++uploadFails_ >= 3) {
            uploadDead_ = true;
            elog::error("blocos: '%s' — upload GL falhou %u× sem TERM; "
                        "os carregamentos param ate o contexto renascer",
                        path_.c_str(), uploadFails_);
        }
        return false;
    }
    uploadFails_ = 0;
    elog::info("gmesh: fase=render ms=%.1f (bloco %zu/%zu de '%s', %u verts)",
               std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - t0)
                   .count(),
               index, blocks_.size(), path_.c_str(),
               static_cast<unsigned>(blk.vertexCount));
    e.gpu = std::move(gpu);
    e.vramBytes = e.ramBytes;
    return true;
}

// ---- LRU -------------------------------------------------------------------

void BlockMesh::evictOverBudget() {
    // a contabilidade VIVA (o pin afere contra ISTO — nada declarado a
    // mais: o que passa do orçamento sai AGORA, neste frame)
    u64 ram = 0, vram = 0;
    for (const Entry& e : entries_) {
        if (e.cpu) {
            ram += e.ramBytes;
        }
        if (e.gpu) {
            vram += e.vramBytes;
        }
    }
    // ordem LRU: os menos-vistos primeiro (desempate pelo índice — estável)
    std::vector<size_t> order;
    order.reserve(entries_.size());
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].gpu) {
            order.push_back(i);
        }
    }
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return entries_[a].lastSeen != entries_[b].lastSeen
                   ? entries_[a].lastSeen < entries_[b].lastSeen
                   : a < b;
    });

    // VRAM primeiro (o pool caro): destroy do Mesh mantém o CPU cache
    for (size_t i : order) {
        if (vram <= vramBudget_) {
            break;
        }
        Entry& e = entries_[i];
        if (e.gpu) {
            vram -= e.vramBytes;
            e.gpu->destroy();
            e.gpu.reset();
            e.vramBytes = 0;
            ++evictedTotal_;
        }
    }
    // RAM depois — TODAS as cópias CPU contam (com OU sem GPU vivo: um
    // bloco vivo em VRAM sem a cache CPU paga o disco no RE-upload, mas o
    // ORÇAMENTO é o contrato e a contabilidade soma ambas as pool)
    std::vector<size_t> ramOrder;
    ramOrder.reserve(entries_.size());
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].cpu) {
            ramOrder.push_back(i);
        }
    }
    std::sort(ramOrder.begin(), ramOrder.end(), [&](size_t a, size_t b) {
        return entries_[a].lastSeen != entries_[b].lastSeen
                   ? entries_[a].lastSeen < entries_[b].lastSeen
                   : a < b;
    });
    for (size_t i : ramOrder) {
        if (ram <= ramBudget_) {
            break;
        }
        Entry& e = entries_[i];
        ram -= e.ramBytes;
        e.cpu.reset();
        e.ramBytes = 0;
        ++evictedTotal_;   // RAM também conta (as duas pool evictam)
    }

    // o aviso de churn: working set VISÍVEL maior que o orçamento — o
    // mesmo bloco entra/sai por frame (1× por open, com os números)
    if (!churnWarned_) {
        u64 visBytes = 0;
        for (const Entry& e : entries_) {
            if (e.gpu && e.lastSeen == frame_) {
                visBytes += e.vramBytes;
            }
        }
        if (visBytes > vramBudget_) {
            churnWarned_ = true;
            elog::warn(
                "blocos: o conjunto visivel (%.1f MB) excede o orcamento "
                "VRAM (%.1f MB) — havera troca por frame (abaixe a "
                "detalhe ou aumente o orcamento)",
                static_cast<double>(visBytes) / (1024.0 * 1024.0),
                static_cast<double>(vramBudget_) / (1024.0 * 1024.0));
        }
    }
}

void BlockMesh::setBudgets(u64 ramBudgetBytes, u64 vramBudgetBytes) {
    ramBudget_ = ramBudgetBytes;
    vramBudget_ = vramBudgetBytes;
}

// ---- o frame ----------------------------------------------------------------

DrawStats BlockMesh::draw(Renderer& r, const Mat4& model, const Mat4& vp,
                          const Texture* tex, const f32* tint,
                          const Mat4* bones, u32 boneCount, Stats& st) {
    DrawStats total{};
    st = Stats{};
    if (!opened_) {
        return total;
    }
    ++frame_;

    st.totalBlocks = static_cast<u32>(blocks_.size());
    f32 planes[6][4];
    frustumPlanes(vp, planes);

    std::string err;
    for (size_t i = 0; i < blocks_.size(); ++i) {
        if (!blockVisible(model, blocks_[i], planes)) {
            continue;   // CULL: o AABB do bloco está todo fora do frustum
        }
        ++st.visible;
        Entry& e = entries_[i];
        const bool wasResident = e.gpu != nullptr;
        if (!ensureBlock(i, err)) {
            // honesto: o bloco falhou (corrupção/faixa) — loga e segue;
            // os OUTROS blocos não morrem por causa dele
            elog::error("blocos: '%s' bloco %zu FALHOU — %s", path_.c_str(),
                        i, err.c_str());
            err.clear();
            continue;
        }
        e.lastSeen = frame_;
        st.drawn++;
        if (!wasResident) {
            ++st.loadedThisFrame;
        }
        total = total + r.drawMesh(*e.gpu, model, vp, tex, tint, bones,
                                   boneCount);
    }

    evictOverBudget();

    // a contabilidade de saída (o que o HUD/log espelham — pós-eviction)
    for (const Entry& e : entries_) {
        if (e.gpu) {
            ++st.resident;
            st.vramBytes += e.vramBytes;
        }
        if (e.cpu) {
            st.ramBytes += e.ramBytes;
        }
    }
    st.evictedTotal = static_cast<u32>(evictedTotal_);
    last_ = st;
    return total;
}

// (o uploadDead limpa no close() — um open novo merece novas tentativas)

} // namespace vv
