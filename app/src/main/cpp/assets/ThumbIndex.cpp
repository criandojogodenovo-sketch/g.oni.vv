// assets/ThumbIndex.cpp — o índice das miniaturas (PASSO 5C).
//
// O render vem do PRIMEIRO BLOCO do .gmesh pelas FAIXAS do PASSO 4
// (gmeshV3PeekMeta 192 B + a tabela + o data do bloco 0 por readBytesAt +
// gmeshV3MaterializeBlock) — o mesmo contrato de orçamento do render por
// blocos: o pico de RAM é UM bloco, nunca o modelo. O placeholder é o
// fallback honesto (o browser NUNCA bloqueia a listagem por uma thumb).
#include "assets/ThumbIndex.h"
#include "assets/PngLoader.h"
#include "assets/ThumbRender.h"
#include "assets/GOwnFormats.h"
#include "platform/EngineLog.h"
#include <chrono>

namespace vv {

const char* ThumbIndex::kDir = "thumbs";

std::string ThumbIndex::relFor(const std::string& assetRel) {
    // "assets/city_n3.gmesh" → "thumbs/city_n3.png"
    const std::string stem = stemOf(assetRel);
    return std::string(kDir) + "/" + stem + ".png";
}

std::string ThumbIndex::stemOf(const std::string& assetRel) {
    std::string s = assetRel;
    const size_t slash = s.rfind('/');
    if (slash != std::string::npos) {
        s = s.substr(slash + 1);
    }
    const size_t dot = s.rfind('.');
    if (dot != std::string::npos) {
        s = s.substr(0, dot);
    }
    return s;
}

bool ThumbIndex::hasThumb(ProjectStorage& st, const std::string& assetRel) {
    return st.exists(relFor(assetRel));
}

bool ThumbIndex::firstBlockMesh(ProjectStorage& st, const std::string& rel,
                                MeshData& out, std::string& err) {
    // 1) o peek (header + meta — os 192 B do contrato do PASSO 4) — LEITURA
    //    POR FAIXA (readBytesAt): o readBytes inteiro carregava o .gmesh
    //    COMPLETO para a RAM (a sentinela R-039 mediu 1965 MB num .gmesh de
    //    ~900 MB — o oposto do orçamento «o pico é UM bloco»)
    const size_t peekLen = kGHeaderBytes + kGmeshV3MetaBytes;
    std::vector<u8> peek;
    if (!st.readBytesAt(rel, 0, peekLen, peek) ||
        peek.size() < peekLen) {
        err = "thumb: .gmesh pequeno demais para o peek: " + rel;
        return false;
    }
    GMeshV3Meta meta;
    if (!gmeshV3PeekMeta(peek.data(), kGHeaderBytes + kGmeshV3MetaBytes,
                         meta, err)) {
        err = "thumb: peek falhou em " + rel + ": " + err;
        return false;
    }
    if (meta.blockCount == 0) {
        err = "thumb: o .gmesh não tem blocos: " + rel;
        return false;
    }
    // 2) a tabela (blockCount × 80 B)
    const u64 tableOff = meta.blockTableOffset;
    const size_t tableLen =
        static_cast<size_t>(meta.blockCount) * kGmeshV3BlockEntryBytes;
    std::vector<u8> table;
    if (!st.readBytesAt(rel, tableOff, tableLen, table) ||
        table.size() != tableLen) {
        err = "thumb: a tabela de blocos não leu (" + rel + ")";
        return false;
    }
    std::vector<GMeshV3Block> blocks;
    if (!gmeshV3ReadTableOnly(table.data(), table.size(), meta, blocks,
                              err)) {
        err = "thumb: a tabela não validou (" + rel + "): " + err;
        return false;
    }
    // 3) o data do PRIMEIRO bloco (a faixa exata — o pico de RAM é ele)
    const GMeshV3Block& blk = blocks[0];
    std::vector<u8> data;
    if (!st.readBytesAt(rel, blk.dataOffset,
                        static_cast<size_t>(blk.dataSize), data) ||
        data.size() != blk.dataSize) {
        err = "thumb: o data do bloco 0 não leu (" + rel + ")";
        return false;
    }
    if (!gmeshV3MaterializeBlock(data.data(), data.size(), meta, blk, out,
                                 err)) {
        err = "thumb: o bloco 0 não materializou (" + rel + "): " + err;
        return false;
    }
    return true;
}

bool ThumbIndex::ensureThumb(ProjectStorage& st, const std::string& assetRel,
                             bool force, std::string& err) {
    // o asset AUSENTE nunca nasce thumb do nada (a honestidade da casa —
    // o placeholder é para o asset QUE EXISTE mas não renderiza; o
    // fantasma é erro limpo, sem ficheiro órfão em thumbs/)
    if (!st.exists(assetRel)) {
        err = "thumb: o asset não existe: " + assetRel;
        return false;
    }
    const std::string thumbRel = relFor(assetRel);
    if (!force && st.exists(thumbRel)) {
        return true;   // o hit do pin («instantâneo»)
    }
    MeshData mesh;
    std::string rerr;
    std::vector<u8> rgba;
    if (!firstBlockMesh(st, assetRel, mesh, rerr) ||
        !thumbrender::renderMesh(mesh, thumbrender::kThumbSize, rgba)) {
        // o fallback honesto: o placeholder (o asset existe — a thumb diz
        // QUEM ele é; o log registra a causa do render falhado)
        std::string stem = stemOf(assetRel);
        thumbrender::placeholder(stem, thumbrender::kThumbSize, rgba);
        elog::warn("thumb: %s — o asset fica com o placeholder (%s)",
                   thumbRel.c_str(), rerr.c_str());
    }
    std::vector<u8> png =
        thumbrender::encodePng(rgba, thumbrender::kThumbSize);
    if (png.empty()) {
        err = "thumb: o encode PNG falhou (" + assetRel + ")";
        return false;
    }
    if (!st.makeDirs(kDir)) {
        err = "thumb: makeDirs falhou em " + std::string(kDir);
        return false;
    }
    if (!st.writeBytes(thumbRel, png.data(), png.size())) {
        err = "thumb: a escrita falhou: " + thumbRel;
        return false;
    }
    return true;
}

ThumbIndex::Rows ThumbIndex::loadRows(ProjectStorage& st,
                                      const std::vector<std::string>& assetRels) {
    Rows r;
    const auto t0 = std::chrono::steady_clock::now();
    for (const std::string& rel : assetRels) {
        std::vector<u8> png;
        if (st.readBytes(relFor(rel), png) && !png.empty()) {
            RawImage img;
            std::string perr;
            if (loadPng(png.data(), png.size(), img, perr)) {
                ++r.loaded;
                continue;
            }
        }
        ++r.placeholders;   // o placeholder desenha no sítio (sem I/O a mais)
    }
    const auto t1 = std::chrono::steady_clock::now();
    r.ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    return r;
}

} // namespace vv
