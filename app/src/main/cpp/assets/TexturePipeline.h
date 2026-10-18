#pragma once
// assets/TexturePipeline.h — orquestração do caminho da textura (F5.1-A;
// PASSO 5A acrescenta PERFIS, REDUÇÃO POR POLÍTICA e o caminho normal-map).
//
// PNG (bytes) → hash → CACHE (chave = hash + PERFIL + override + normal +
// ASTC; hit = lê blob comprimido do disco) | miss → decode (PngLoader) →
// REDUÇÃO (a política do perfil do projeto; override por asset VENCE; teto
// do device GL_MAX_TEXTURE_SIZE por último — SEMPRE do original, em espaço
// linear para cor) → COMPRESSÃO (HardwareCompressor: ASTC 4x4 se a extensão
// existir, senão ETC2; normal maps e <256px ficam RGBA8 sem perda) → store
// no cache.
//
// AS DUAS OPERAÇÕES SÃO DISTINTAS E LOGADAS DISTINTAS (o dono sabe sempre
// o que aconteceu a cada textura):
//   «textura: reduzida 4096→2048 (perfil Equilibrado)»     — a RESOLUÇÃO
//   «textura: reduzida 4096→512 (override do asset)»       — a RESOLUÇÃO
//   «textura: reduzida 8192→4096 (teto do device 4096)»    — a RESOLUÇÃO
//   «textura: comprimida ASTC 4x4 (2048 KB)»               — o FORMATO
//   «textura: RGBA8 4096 KB (normal map — sem perda de canais)» — o FORMATO
//
// A redução e os mips de texturas de COR filtram em ESPAÇO LINEAR
// (MipSpace::SrgbLinear); normal maps filtram em bytes (dados lineares).
//
// O original NUNCA é substituído: o pipeline só lê os bytes do PNG que o
// chamador lhe dá (a fonte vive em source/ ou textures/ e fica lá) e
// NUNCA recomprime de um blob comprimido (o cache devolve o produto FINAL,
// a entrada é sempre o PNG de origem).
//
// Codificação PARALELA fora da UI: o conversor (AssetConverter) corre o
// passe de texturas num pool de threads; o cache é serializado por MUTEX
// (o disco é rápido, a compressão é quem paraleliza). O pipeline em si é
// thread-safe para process() concorrentes.
//
// GL-free / Android-free: testável no CI com FakeStorage.
#include <map>
#include <mutex>
#include <string>
#include "assets/MipGen.h"      // MipSpace (a filtragem linear dos mips)
#include "assets/TextureCache.h"
#include "assets/TexturePolicy.h"

namespace vv {

struct TextureLoadInfo {
    bool cacheHit = false;
    CompressedFormat format = CompressedFormat::RGBA8;
    const char* via = "";    // "cache" | "compress" | "raw"
    std::string warn;        // aviso do gate 2K (só no fallback sem compressão)
    // ---- PASSO 5A — o que aconteceu (o chamador loga/se usa) --------------
    bool reduced = false;        // houve redução de resolução (qualquer causa)
    u32 reducedFromW = 0;        // dims ANTES da redução (0 = sem redução)
    u32 reducedFromH = 0;
    bool srgb = false;           // flag kTexFlagSrgb (conteúdo de cor)
    bool normalMap = false;      // flag kTexFlagNormal (RGBA8 sem perda)
    u32 overrideDim = 0;         // o teto do override aplicado (0 = sem)
};

class TexturePipeline {
public:
    TexturePipeline(TextureCompressor& comp, TextureCache& cache)
        : comp_(comp), cache_(cache) {}

    // ---- configuração do projeto (o main liga no boot/load) ----------------
    void setPerfil(TexPerfil p) { perfil_ = p; }
    TexPerfil perfil() const { return perfil_; }
    // GL_MAX_TEXTURE_SIZE do device (o main injeta do GL; 0 = desconhecido —
    // sem teto: o leitor .gtext capaa a 16384 como sempre)
    void setMaxTextureSize(u32 px) { maxTexSize_ = px; }
    u32 maxTextureSize() const { return maxTexSize_; }
    // override POR ASSET (textures/overrides.goni): stem → teto da maior
    // dimensão. 0 = NUNCA reduzir este asset. VENCE o perfil.
    void setOverride(const std::string& assetKey, u32 maxDim) {
        if (!assetKey.empty()) {
            overrides_[assetKey] = maxDim;
        }
    }
    void clearOverrides() { overrides_.clear(); }
    u32 overrideCount() const { return overrides_.size(); }

    // `debugName` só aparece em erros. Hit/miss contam no TextureCache.
    // `job` (PASSO 5A) diz o assetKey (override/log) e se é normal map.
    bool process(const u8* png, size_t len, const char* debugName,
                 CompressedImage& out, TextureLoadInfo& info,
                 std::string& err, const TexJob& job = TexJob{});

    // hash do último PNG processado (telemetria)
    u64 lastHash() const { return lastHash_; }

    // o suffix da chave de cache de UM job (exposto p/ os testes provarem
    // que perfis/overrides diferentes = entradas diferentes)
    std::string cacheSuffixFor(const TexJob& job, bool astcAvailable) const;

private:
    // halva `img` até max(w,h) <= cap (fator 2, espaço dado); devolve true
    // se reduziu (e dims originais ficam nos out-params)
    static bool halveUntilCap(RawImage& img, u32 cap, MipSpace space,
                              u32& fromW, u32& fromH);

    TextureCompressor& comp_;
    TextureCache& cache_;
    u64 lastHash_ = 0;

    // ---- estado do projeto (imutável durante um import; o main seta) -------
    TexPerfil perfil_ = TexPerfil::Qualidade;   // default = o pré-5A
    u32 maxTexSize_ = 0;                        // 0 = desconhecido
    std::map<std::string, u32> overrides_;

    // o cache toca no storage: serializado (o resto corre paralelo)
    std::mutex cacheMu_;
};

} // namespace vv
