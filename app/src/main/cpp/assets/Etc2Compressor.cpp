// assets/Etc2Compressor.cpp — ETC2 RGB8 / RGBA8 EAC via etcpak (F5.1-A).
//
// etcpak (vendor/etcpak, BSD) consome u32 little-endian com R no byte 0 —
// exatamente o layout de RawImage::rgba. Entrada com padding para múltiplo
// de 4×4 por replicação de borda: os texels extras pertencem a blocos
// parciais que a amostragem GL nunca lê.
#include "assets/TextureCompressor.h"
#include "assets/MipGen.h"
#include <cstring>
#include <string>
#include <vector>
#include "ProcessRGB.hpp"   // vendor/etcpak (include dir do target)

namespace vv {
namespace {

void padTo4x4(const RawImage& img, std::vector<u32>& out, u32& pw, u32& ph) {
    pw = (img.width + 3u) & ~3u;
    ph = (img.height + 3u) & ~3u;
    out.assign(static_cast<size_t>(pw) * ph, 0u);
    for (u32 y = 0; y < ph; ++y) {
        const u32 sy = y < img.height ? y : img.height - 1;
        const u8* row = &img.rgba[static_cast<size_t>(sy) * img.width * 4];
        for (u32 x = 0; x < pw; ++x) {
            const u32 sx = x < img.width ? x : img.width - 1;
            u32 px;
            std::memcpy(&px, row + static_cast<size_t>(sx) * 4, 4);
            out[static_cast<size_t>(y) * pw + x] = px;
        }
    }
}

void compressMip(const RawImage& mip, bool rgba, bool heuristics,
                 std::vector<u8>& dst) {
    std::vector<u32> padded;
    u32 pw = 0, ph = 0;
    padTo4x4(mip, padded, pw, ph);
    const u32 blocks = (pw / 4u) * (ph / 4u);
    // ETC2 RGB8 = 1×u64 por bloco; RGBA8_EAC = 2×u64 (alpha + cor)
    const u32 wordsPerBlock = rgba ? 2u : 1u;
    std::vector<u64> encoded(static_cast<size_t>(blocks) * wordsPerBlock);
    if (rgba) {
        CompressEtc2Rgba(padded.data(), encoded.data(), blocks, pw, heuristics);
    } else {
        CompressEtc2Rgb(padded.data(), encoded.data(), blocks, pw, heuristics);
    }
    const size_t bytes = static_cast<size_t>(blocks) * wordsPerBlock * 8u;
    const size_t base = dst.size();
    dst.resize(base + bytes);
    std::memcpy(dst.data() + base, encoded.data(), bytes);
}

bool hasUsefulAlpha(const RawImage& img) {
    for (size_t i = 3; i < img.rgba.size(); i += 4) {
        if (img.rgba[i] != 255) {
            return true;
        }
    }
    return false;
}

} // namespace

bool Etc2Compressor::compress(const RawImage& in, CompressedImage& out,
                              std::string& err) {
    err.clear();
    out = CompressedImage{};
    if (!in.ok()) {
        err = "etc2: imagem de entrada inválida";
        return false;
    }
    const bool alpha = hasUsefulAlpha(in);
    out.format = alpha ? CompressedFormat::ETC2_RGBA : CompressedFormat::ETC2_RGB;
    out.width = in.width;
    out.height = in.height;

    std::vector<RawImage> chain;
    genMipChainRGBA(in, chain);
    const u32 count = static_cast<u32>(chain.size());
    out.mips.resize(count);

    std::vector<u8> blob;
    u32 offset = 0;
    for (u32 i = 0; i < count; ++i) {
        compressMip(chain[i], alpha, heuristics, blob);
        CompressedMip& m = out.mips[i];
        m.width = chain[i].width;
        m.height = chain[i].height;
        m.offset = offset;
        m.size = static_cast<u32>(blob.size()) - offset;
        offset = static_cast<u32>(blob.size());
    }
    out.data = std::move(blob);
    return true;
}

} // namespace vv
