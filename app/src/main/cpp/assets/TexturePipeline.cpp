// assets/TexturePipeline.cpp — orquestração decode/redução/compressão/cache
// (F5.1-A; PASSO 5A: perfis + redução por política + normal maps + teto GL).
#include "assets/TexturePipeline.h"
#include "assets/PngLoader.h"
#include "platform/EngineLog.h"
#include <cstdio>
#include <utility>

namespace vv {

namespace {

// as linhas contrato do PASSO 5A — UMA FONTE (o pipeline loga; o chamador
// vê o eco em TextureLoadInfo). KB = bytes/1024 arredondado.
void logReduced(const char* debugName, u32 fromW, u32 toW, const char* cause) {
    elog::info("textura: reduzida %u→%u (%s) — %s", fromW, toW, cause,
               debugName ? debugName : "?");
}

void logFormat(CompressedFormat f, size_t blobBytes, bool normal,
               const char* debugName) {
    const u32 kb = static_cast<u32>((blobBytes + 1023u) / 1024u);
    if (normal) {
        elog::info("textura: RGBA8 %u KB (normal map — sem perda de "
                   "canais) — %s", kb, debugName ? debugName : "?");
        return;
    }
    switch (f) {
        case CompressedFormat::RGBA8:
            elog::info("textura: RGBA8 %u KB (sem compressão — pequena) — %s",
                       kb, debugName ? debugName : "?");
            break;
        default:
            elog::info("textura: comprimida %s (%u KB) — %s",
                       formatName(f), kb, debugName ? debugName : "?");
            break;
    }
}

} // namespace

std::string TexturePipeline::cacheSuffixFor(const TexJob& job,
                                            bool astcAvailable) const {
    // o override VENCE o perfil — entra na chave com o valor efetivo
    u32 ovr = 0;
    bool hasOvr = false;
    if (job.assetKey && *job.assetKey) {
        const auto it = overrides_.find(job.assetKey);
        if (it != overrides_.end()) {
            ovr = it->second;
            hasOvr = true;
        }
    }
    char buf[48];
    // p=perfil · o=override (>=1 teto, 0=com override "nunca", 255=sem) ·
    // n=normal · a=astc
    std::snprintf(buf, sizeof(buf), "_p%u_o%u_n%u_a%u",
                  static_cast<unsigned>(perfil_),
                  static_cast<unsigned>(hasOvr ? ovr : 255u),
                  job.normalMap ? 1u : 0u, astcAvailable ? 1u : 0u);
    return std::string(buf);
}

bool TexturePipeline::halveUntilCap(RawImage& img, u32 cap, MipSpace space,
                                    u32& fromW, u32& fromH) {
    if (cap == 0) {
        return false;
    }
    const u32 m0 = img.width > img.height ? img.width : img.height;
    if (m0 <= cap) {
        return false;
    }
    fromW = img.width;
    fromH = img.height;
    RawImage reduced;
    while ((img.width > img.height ? img.width : img.height) > cap &&
           (img.width > 1 || img.height > 1)) {
        halveImageRGBA(img, reduced, space);
        img = std::move(reduced);   // halveImageRGBA reseta `reduced` no início
    }
    return true;
}

bool TexturePipeline::process(const u8* png, size_t len, const char* debugName,
                              CompressedImage& out, TextureLoadInfo& info,
                              std::string& err, const TexJob& job) {
    err.clear();
    info = TextureLoadInfo{};
    out = CompressedImage{};
    if (!png || len == 0) {
        err = "pipeline: png vazio";
        return false;
    }
    const u64 hash = TextureCache::hashBytes(png, len);
    lastHash_ = hash;

    // ---- 0) a DECISÃO ANTES do trabalho (a chave do cache precisa dela) ----
    // O override por asset VENCE o perfil. As dims originais vêm do header
    // do PNG (barato — sem decode); o teto do device entra por último.
    u32 origW = 0, origH = 0;
    {
        std::string derr;
        if (!pngDims(png, len, origW, origH, derr)) {
            err = std::string("pipeline(") + (debugName ? debugName : "?") +
                  "): " + derr;
            return false;
        }
    }
    u32 cap = 0;
    bool hasOverride = false;
    if (job.assetKey && *job.assetKey) {
        const auto it = overrides_.find(job.assetKey);
        if (it != overrides_.end()) {
            cap = it->second;      // 0 = "nunca reduzir" (override legítimo)
            hasOverride = true;
        }
    }
    if (!hasOverride) {
        cap = texProfileCapMaxDim(origW, origH, perfil_);
    }
    // o teto do device é o TETO FÍSICO: se sobreviveu ao perfil e ainda
    // excede GL_MAX_TEXTURE_SIZE, halva (o pin: gigante não crasha)
    const u32 m0 = origW > origH ? origW : origH;
    const bool exceedsDevice =
        maxTexSize_ > 0 && m0 > maxTexSize_ &&
        (cap == 0 || cap > maxTexSize_);
    if (exceedsDevice) {
        cap = maxTexSize_;
    }

    // ---- 1) cache em disco (chave por perfil/override/normal/astc) ---------
    const bool astc = comp_.astcAvailable();
    const std::string suffix = cacheSuffixFor(job, astc);
    {
        std::lock_guard<std::mutex> lk(cacheMu_);
        std::string cacheErr;
        if (cache_.loadKeyed(hash, suffix, out, cacheErr)) {
            info.cacheHit = true;
            info.format = out.format;
            info.via = "cache";
            info.srgb = (out.flags & kTexFlagSrgb) != 0;
            info.normalMap = (out.flags & kTexFlagNormal) != 0;
            info.overrideDim = hasOverride ? cap : 0;
            // o hit sabe se a ENTRADA guardada foi reduzida: as dims do
            // PNG de origem (header, barato) contra as dims do blob
            if (origW > out.width || origH > out.height) {
                info.reduced = true;
                info.reducedFromW = origW;
                info.reducedFromH = origH;
            }
            logFormat(out.format, out.data.size(), info.normalMap, debugName);
            elog::info("textura: reutilizada do cache (perfil %s) — %s",
                       texPerfilName(perfil_), debugName ? debugName : "?");
            return true;
        }
    }

    // ---- 2) decode do ORIGINAL (a entrada é SEMPRE o PNG de origem) --------
    RawImage img;
    std::string perr;
    if (!loadPng(png, len, img, perr)) {
        err = std::string("pipeline(") + (debugName ? debugName : "?") +
              "): PNG inválido: " + perr;
        return false;
    }

    // ---- 3) REDUÇÃO (operação 1 — resolução; SEMPRE do original) -----------
    // espaço: cor filtra em LUZ; normal map é dado linear (bytes)
    const MipSpace space = job.normalMap ? MipSpace::Bytes
                                         : MipSpace::SrgbLinear;
    u32 fromW = 0, fromH = 0;
    if (halveUntilCap(img, cap, space, fromW, fromH)) {
        info.reduced = true;
        info.reducedFromW = fromW;
        info.reducedFromH = fromH;
        const char* cause =
            exceedsDevice ? "teto do device"
                          : (hasOverride ? "override do asset"
                                         : texPerfilName(perfil_));
        logReduced(debugName, fromW, img.width, cause);
    }
    info.overrideDim = hasOverride ? cap : 0;

    // ---- 4) COMPRESSÃO (operação 2 — formato) ------------------------------
    // normal map → RGBA8 SEM PERDA (o pin dos canais); o resto segue o
    // gate de sempre (HardwareCompressor: ASTC 4x4 / ETC2; <256px → RGBA8).
    std::string cerr2;
    const bool compressedPath =
        !job.normalMap && comp_.canCompress(img.width, img.height);
    if (!compressedPath) {
        // RGBA8 direto — a identidade (1 nível; mips na GPU como sempre).
        // O fallback sem hardware passa pelo MESMO caminho (com o downscale
        // 2K legado quando a textura é grande e não comprimível — a rede
        // rara de um device sem ETC2 nem ASTC; a política do perfil JÁ
        // agiu acima).
        if (!job.normalMap && !comp_.canCompress(img.width, img.height)) {
            std::string warn;
            downscaleTo2K(img, warn);
            info.warn = warn;
        }
        static PassthroughCompressor kPassthrough;
        if (!kPassthrough.compress(img, out, cerr2)) {
            err = std::string("pipeline(") + (debugName ? debugName : "?") +
                  "): " + cerr2;
            return false;
        }
    } else if (!comp_.compress(img, out, cerr2,
                               /*mipLinear=*/!job.normalMap)) {
        err = std::string("pipeline(") + (debugName ? debugName : "?") +
              "): " + cerr2;
        return false;
    }

    // ---- 5) as FLAGS (viajam no .gtext v2 / .gtc) ---------------------------
    out.flags = 0;
    if (!job.normalMap) {
        out.flags |= kTexFlagSrgb;   // conteúdo de cor (a decisão §3 do doc)
    } else {
        out.flags |= kTexFlagNormal;
    }
    info.srgb = (out.flags & kTexFlagSrgb) != 0;
    info.normalMap = job.normalMap;
    info.format = out.format;
    info.via = formatIsCompressed(out.format) ? "compress" : "raw";

    // ---- 6) store no cache (best effort; serializado) -----------------------
    {
        std::lock_guard<std::mutex> lk(cacheMu_);
        std::string serr;
        cache_.storeKeyed(hash, suffix, out, serr);
    }
    logFormat(out.format, out.data.size(), job.normalMap, debugName);
    return true;
}

} // namespace vv
