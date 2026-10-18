// assets/TextureCompressor.cpp — helpers de formato + passthrough + seleção
// automática (F5/F5.1-A). Os codificadores ETC2/ASTC vivem nos próprios
// TUs (Etc2Compressor.cpp / AstcCompressor.cpp — vendors isolados aqui).
#include "assets/TextureCompressor.h"
#include "assets/MipGen.h"
#include <cmath>

namespace vv {

u32 mipCountFor(u32 w, u32 h) {
    if (w == 0 || h == 0) {
        return 0;
    }
    u32 m = w > h ? w : h;
    u32 count = 1;
    while (m > 1) {
        m >>= 1;
        ++count;
    }
    return count;
}

u32 formatBlockX(CompressedFormat f) {
    return f == CompressedFormat::ASTC_6x6 ? 6u : 4u;
}

u32 formatBlockY(CompressedFormat f) {
    return formatBlockX(f);
}

size_t mipBytesFor(CompressedFormat f, u32 w, u32 h) {
    switch (f) {
        case CompressedFormat::RGBA8:
            return static_cast<size_t>(w) * h * 4u;
        case CompressedFormat::ETC2_RGB:
            return static_cast<size_t>((w + 3u) / 4u) * ((h + 3u) / 4u) * 8u;
        case CompressedFormat::ETC2_RGBA:
            return static_cast<size_t>((w + 3u) / 4u) * ((h + 3u) / 4u) * 16u;
        case CompressedFormat::ASTC_4x4:
            return static_cast<size_t>((w + 3u) / 4u) * ((h + 3u) / 4u) * 16u;
        case CompressedFormat::ASTC_6x6:
            return static_cast<size_t>((w + 5u) / 6u) * ((h + 5u) / 6u) * 16u;
        default:
            return 0;
    }
}

bool formatIsCompressed(CompressedFormat f) {
    return f != CompressedFormat::RGBA8;
}

const char* formatName(CompressedFormat f) {
    switch (f) {
        case CompressedFormat::RGBA8:     return "RGBA8";
        case CompressedFormat::ETC2_RGB:  return "ETC2 RGB";
        case CompressedFormat::ETC2_RGBA: return "ETC2 EAC";
        case CompressedFormat::ASTC_4x4:  return "ASTC 4x4";
        case CompressedFormat::ASTC_6x6:  return "ASTC 6x6";
        default:                          return "?";
    }
}

// ---- PassthroughCompressor (F5) ---------------------------------------------

bool PassthroughCompressor::compress(const RawImage& in, CompressedImage& out,
                                     std::string& err, bool /*mipLinear*/) {
    err.clear();
    out = CompressedImage{};
    if (!in.ok()) {
        err = "compressor: imagem de entrada inválida";
        return false;
    }
    out.format = CompressedFormat::RGBA8;
    out.width = in.width;
    out.height = in.height;
    out.data = in.rgba;   // identidade — bytes saem como entraram (a prova
                          // «sem perda de canais» dos normal maps do 5A)
    CompressedMip m;
    m.width = in.width;
    m.height = in.height;
    m.offset = 0;
    m.size = static_cast<u32>(out.data.size());
    out.mips.push_back(m);
    return true;
}

// ---- HardwareCompressor (F5.1-A: ASTC se extensão, senão ETC2; <256 → RGBA) --

bool HardwareCompressor::compress(const RawImage& in, CompressedImage& out,
                                  std::string& err, bool mipLinear) {
    err.clear();
    out = CompressedImage{};
    if (!in.ok()) {
        err = "compressor: imagem de entrada inválida";
        return false;
    }
    if (!compressible(in.width, in.height)) {
        // overhead de bloco supera o ganho — RGBA direto (mips na GPU)
        lastFormat_ = CompressedFormat::RGBA8;
        return PassthroughCompressor{}.compress(in, out, err);
    }
    if (astcSupported_) {
        std::string astcErr;
        if (astc4_.compress(in, out, astcErr, mipLinear)) {
            lastFormat_ = out.format;
            return true;   // err permanece vazio (contrato: sucesso limpo)
        }
        // ASTC falhou → cai para ETC2 (garantido em GLES3.0+)
    }
    if (etc2_.compress(in, out, err, mipLinear)) {
        lastFormat_ = out.format;
        return true;
    }
    return false;
}

} // namespace vv
