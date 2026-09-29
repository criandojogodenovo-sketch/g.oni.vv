// assets/AstcCompressor.cpp — ASTC LDR linear via astc-encoder (F5.1-A).
//
// vendor/astc-encoder (Apache-2.0). O SIMD é auto-detectado pelo header
// (NEON no arm64 do NDK; SSE2 no hospedeiro). Perfil LDR linear — o PNG
// decodifica para RGBA8 direto e o sampler não aplica decode sRGB (o
// resultado na tela é idêntico ao caminho RGBA8 não-comprimido).
#include "assets/TextureCompressor.h"
#include "assets/MipGen.h"
#include <cstring>
#include <string>
#include <vector>
#include "astcenc.h"

namespace vv {
namespace {

// qualidade "fast" (10 = preset do codec) — rápida o bastante no C33 para
// um import one-shot (o resultado fica em cache; o custo é pago 1×)
constexpr float kQuality = 10.0f;   // ASTCENC_PRE_FAST

astcenc_swizzle identitySwizzle() {
    astcenc_swizzle sw;
    sw.r = ASTCENC_SWZ_R;
    sw.g = ASTCENC_SWZ_G;
    sw.b = ASTCENC_SWZ_B;
    sw.a = ASTCENC_SWZ_A;
    return sw;
}

} // namespace

AstcCompressor::AstcCompressor(u32 block) : block_(block) {
    if (block_ != 4 && block_ != 6) {
        block_ = 4;
    }
}

bool AstcCompressor::compress(const RawImage& in, CompressedImage& out,
                              std::string& err) {
    err.clear();
    out = CompressedImage{};
    if (!in.ok()) {
        err = "astc: imagem de entrada inválida";
        return false;
    }
    astcenc_config config;
    astcenc_error e = astcenc_config_init(ASTCENC_PRF_LDR, block_,
                                         block_, 1, kQuality, 0, &config);
    if (e != ASTCENC_SUCCESS) {
        err = "astc: config inválida";
        return false;
    }
    astcenc_context* ctx = nullptr;
    e = astcenc_context_alloc(&config, 1, &ctx);
    if (e != ASTCENC_SUCCESS) {
        err = std::string("astc: falha ao alocar contexto: ") +
              astcenc_get_error_string(e);
        return false;
    }

    const char* exitErr = nullptr;
    std::vector<RawImage> chain;
    genMipChainRGBA(in, chain);
    const u32 count = static_cast<u32>(chain.size());
    out.format = block_ == 6 ? CompressedFormat::ASTC_6x6 : CompressedFormat::ASTC_4x4;
    out.width = in.width;
    out.height = in.height;
    out.mips.resize(count);

    std::vector<u8> blob;
    u32 offset = 0;
    const astcenc_swizzle sw = identitySwizzle();
    for (u32 i = 0; i < count && !exitErr; ++i) {
        const RawImage& mip = chain[i];
        const size_t bytes =
            static_cast<size_t>((mip.width + block_ - 1) / block_) *
            ((mip.height + block_ - 1) / block_) * 16u;
        const size_t base = blob.size();
        blob.resize(base + bytes);

        astcenc_image image;
        image.dim_x = mip.width;
        image.dim_y = mip.height;
        image.dim_z = 1;
        image.data_type = ASTCENC_TYPE_U8;
        void* slice = const_cast<u8*>(mip.rgba.data());
        image.data = &slice;

        e = astcenc_compress_image(ctx, &image, &sw, blob.data() + base,
                                   bytes, 0);
        if (e != ASTCENC_SUCCESS) {
            err = std::string("astc: compressão falhou: ") +
                  astcenc_get_error_string(e);
            exitErr = err.c_str();
            break;
        }
        CompressedMip& m = out.mips[i];
        m.width = mip.width;
        m.height = mip.height;
        m.offset = offset;
        m.size = static_cast<u32>(bytes);
        offset += static_cast<u32>(bytes);
    }
    astcenc_context_free(ctx);
    if (exitErr) {
        out = CompressedImage{};
        return false;
    }
    out.data = std::move(blob);
    return true;
}

} // namespace vv
