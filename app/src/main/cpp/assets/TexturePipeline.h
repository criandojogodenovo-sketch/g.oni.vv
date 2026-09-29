#pragma once
// assets/TexturePipeline.h — orquestração do caminho da textura (F5.1-A).
//
// PNG (bytes) → hash → CACHE (hit = lê blob comprimido do disco) |
// miss → decode (PngLoader) → GATE 4K (com compressão disponível a textura
// 4K entra INTEIRA — ETC2 4K ≈ 8 MB de VRAM; sem compressão o fallback
// raro reduz para 2K com aviso) → compressão (HardwareCompressor: ASTC se
// a extensão existir, senão ETC2) → store no cache.
//
// O gate usa `TextureCompressor::canCompress(w,h)` — um compressor sem
// capacidade (PassthroughCompressor) empurra TUDO para o fallback 2K,
// que é exatamente o comportamento pré-F5.1.
//
// GL-free / Android-free: testável no CI com FakeStorage.
#include <string>
#include "assets/TextureCache.h"

namespace vv {

struct TextureLoadInfo {
    bool cacheHit = false;
    CompressedFormat format = CompressedFormat::RGBA8;
    const char* via = "";    // "cache" | "compress" | "raw"
    std::string warn;        // aviso do gate 2K (só no fallback sem compressão)
};

class TexturePipeline {
public:
    TexturePipeline(TextureCompressor& comp, TextureCache& cache)
        : comp_(comp), cache_(cache) {}

    // `debugName` só aparece em erros. Hit/miss contam no TextureCache.
    bool process(const u8* png, size_t len, const char* debugName,
                 CompressedImage& out, TextureLoadInfo& info,
                 std::string& err);

    // hash do último PNG processado (telemetria)
    u64 lastHash() const { return lastHash_; }

private:
    TextureCompressor& comp_;
    TextureCache& cache_;
    u64 lastHash_ = 0;
};

} // namespace vv
