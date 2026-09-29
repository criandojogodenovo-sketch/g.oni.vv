// assets/TexturePipeline.cpp — orquestração decode/gate/compress/cache (F5.1-A).
#include "assets/TexturePipeline.h"
#include "assets/PngLoader.h"

namespace vv {

bool TexturePipeline::process(const u8* png, size_t len, const char* debugName,
                              CompressedImage& out, TextureLoadInfo& info,
                              std::string& err) {
    err.clear();
    info = TextureLoadInfo{};
    out = CompressedImage{};
    if (!png || len == 0) {
        err = "pipeline: png vazio";
        return false;
    }
    const u64 hash = TextureCache::hashBytes(png, len);
    lastHash_ = hash;

    // 1) cache em disco
    std::string cacheErr;
    if (cache_.load(hash, out, cacheErr)) {
        info.cacheHit = true;
        info.format = out.format;
        info.via = "cache";
        return true;
    }

    // 2) decode
    RawImage img;
    std::string perr;
    if (!loadPng(png, len, img, perr)) {
        err = std::string("pipeline(") + (debugName ? debugName : "?") +
              "): PNG inválido: " + perr;
        return false;
    }

    // 3) gate 4K — só o FALLBACK sem compressão reduz (com aviso)
    std::string warn;
    if (!comp_.canCompress(img.width, img.height)) {
        downscaleTo2K(img, warn);
    }

    // 4) compressão
    std::string cerr2;
    if (!comp_.compress(img, out, cerr2)) {
        err = std::string("pipeline(") + (debugName ? debugName : "?") +
              "): " + cerr2;
        return false;
    }

    // 5) store (best-effort — falha de disco não invalida a carga)
    std::string serr;
    cache_.store(hash, out, serr);

    info.cacheHit = false;
    info.format = out.format;
    info.via = formatIsCompressed(out.format) ? "compress" : "raw";
    info.warn = warn;
    return true;
}

} // namespace vv
