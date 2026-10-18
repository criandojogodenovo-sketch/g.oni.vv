// assets/ImpostorIndex.cpp — o índice dos impostores (0.11-W 3.75b).
//
// A geração é PELO BlockMesh (a TABELA — o mesmo open do draw): cada bloco
// materializa-se pela faixa exata (readBytesAt + gmeshV3MaterializeBlock
// SEM cache — o pico de RAM é UM bloco, o contrato R-041), acumula-se no
// MeshData do impostor até kImpostorMaxTris (ou o esgotar da tabela, ou o
// teto de 65 535 verts do índice u16) e sai como PNG RGBA (o fundo
// transparente é o que o MASK do lit corta).
#include "assets/ImpostorIndex.h"
#include "assets/ThumbIndex.h"
#include "assets/ThumbRender.h"
#include "platform/EngineLog.h"
#include "render/BlockMesh.h"
#include "render/Impostor.h"
#include "render/Mesh.h"   // o ~BlockMesh destrói unique_ptr<Mesh> — o tipo
                           // COMPLETO no TU que o constrói (o contrato do
                           // header GL-free do BlockMesh.h)
#include <algorithm>

namespace vv {

const char* ImpostorIndex::kDir = impostor::kDir;   // "impostors"

std::string ImpostorIndex::relFor(const std::string& assetRel) {
    // "assets/city_n3.gmesh" → "impostors/city_n3.png" (o MESMO stem da
    // thumb — o ThumbIndex::stemOf é a fonte; UMA regra de stem na casa)
    return std::string(kDir) + "/" + ThumbIndex::stemOf(assetRel) + ".png";
}

bool ImpostorIndex::hasImpostor(ProjectStorage& st,
                                const std::string& assetRel) {
    return st.exists(relFor(assetRel));
}

bool ImpostorIndex::ensureImpostor(ProjectStorage& st,
                                   const std::string& assetRel, bool force,
                                   std::string& err) {
    // o asset AUSENTE nunca nasce impostor do nada (o MESMO guard do
    // fantasma do thumb — a lição R-041; o impostor órfão mentiria a
    // silhueta de um ficheiro que não existe)
    if (!st.exists(assetRel)) {
        err = "impostor: o asset não existe: " + assetRel;
        return false;
    }
    const std::string impRel = relFor(assetRel);
    if (!force && st.exists(impRel)) {
        return true;   // o hit (a geração corre UMA vez por asset)
    }
    // a TABELA (192 B + tabela + materiais — ZERO dados; o MESMO open do
    // draw — se o draw abre, a geração abre)
    BlockMesh bm;
    if (!bm.open(st, assetRel, err)) {
        err = "impostor: o .gmesh não abriu (" + assetRel + "): " + err;
        return false;
    }
    if (bm.table().empty()) {
        err = "impostor: o .gmesh não tem blocos: " + assetRel;
        return false;
    }
    // o ACUMULADO (bloco a bloco SEM cache — o pico de RAM é UM bloco +
    // o acumulado; o teto de tris é o MESMO 4096 da thumb)
    MeshData acc;
    MeshData blk;
    u32 tris = 0;
    size_t read = 0;   // os blocos TENTADOS (o log é honesto)
    for (size_t i = 0; i < bm.table().size(); ++i) {
        if (tris >= impostor::kImpostorMaxTris) {
            break;   // o orçamento de silhueta chega (a pré-visualização
                     // é representativa — o MESMO critério da thumb)
        }
        ++read;
        std::string rerr;
        if (!bm.materializeBlock(i, blk, rerr)) {
            elog::warn("impostor: o bloco %zu não materializou (%s) — "
                       "segue com os que vieram",
                       i, rerr.c_str());
            continue;   // um bloco partido não mata a silhueta (best-effort)
        }
        const size_t base = acc.vertices.size();
        if (base + blk.vertices.size() > 65535u) {
            break;   // o teto do índice u16 — a silhueta fica com os
                     // blocos que couberam (honesto e limitado)
        }
        acc.vertices.insert(acc.vertices.end(), blk.vertices.begin(),
                            blk.vertices.end());
        acc.indices.reserve(acc.indices.size() + blk.indices.size());
        for (const u16 idx : blk.indices) {
            acc.indices.push_back(static_cast<u16>(base + idx));
        }
        tris = static_cast<u32>(acc.indices.size() / 3);
    }
    if (acc.indices.size() < 3) {
        err = "impostor: nenhum bloco materializou (" + assetRel + ")";
        return false;   // SEM placeholder — o impostor mentido é pior que
                        // os blocos caros (o draw segue o caminho normal)
    }
    std::vector<u8> rgba;
    if (!thumbrender::renderMeshAlpha(acc, impostor::kImpostorSize, rgba)) {
        err = "impostor: o render alfa falhou (" + assetRel + ")";
        return false;
    }
    std::vector<u8> png =
        thumbrender::encodePngRgba(rgba, impostor::kImpostorSize);
    if (png.empty()) {
        err = "impostor: o encode PNG RGBA falhou (" + assetRel + ")";
        return false;
    }
    if (!st.makeDirs(kDir)) {
        err = "impostor: makeDirs falhou em " + std::string(kDir);
        return false;
    }
    if (!st.writeBytes(impRel, png.data(), png.size())) {
        err = "impostor: a escrita falhou: " + impRel;
        return false;
    }
    elog::info("impostor: '%s' gerado (%zu blocos lidos, %u tris, %ux%u RGBA)",
               impRel.c_str(), read, tris, impostor::kImpostorSize,
               impostor::kImpostorSize);
    return true;
}

} // namespace vv
