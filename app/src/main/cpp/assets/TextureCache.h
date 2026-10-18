#pragma once
// assets/TextureCache.h — cache em disco de texturas comprimidas (F5.1-A).
//
// Chave = FNV-1a 64 dos BYTES do PNG de origem: o mesmo ficheiro → hit;
// PNG alterado → hash diferente → re-comprime (a entrada antiga fica órfã
// até limpeza manual — barato e nunca falso-hit).
//
// Ficheiro: textures/cache/cache_<hash16>[_suffix].gtc (little-endian):
//   [0..3]  magic 'G','V','T','C'
//   [4]     versão = 2 (PASSO 5A; a 1 pré-5A continua a abrir)
//   [5]     CompressedFormat
//   [6]     flags (v2; bits kTexFlagSrgb/kTexFlagNormal) [7] reservado
//   [8..11] width   u32        [12..15] height u32
//   [16..19] mipCount u32
//   [20..27] hash de origem u64 (eco — valida a chave da entrada)
//   [28..]  mipCount × { width u32, height u32, offset u32, size u32 }
//   depois  blob contíguo (mesma disposição de CompressedImage::data)
//
// PASSO 5A — SUFFIX da chave: o RESULTADO do pipeline depende do perfil
// (redução), do override do asset, do caminho normal-map e da presença de
// ASTC — a MESMA textura nos 3 perfis = TRÊS blobs distintos (o pin).
// O suffix («_p1_o0_n0_a1») separa essas variantes; vazio = a chave legada
// de sempre (hash puro) para quem não usa perfis.
//
// GL-free / Android-free (ProjectStorage): testável no CI com FakeStorage.
#include <string>
#include "assets/TextureCompressor.h"
#include "core/ProjectStorage.h"

namespace vv {

class TextureCache {
public:
    explicit TextureCache(ProjectStorage& st) : st_(st) {}

    static const char* kDir;   // "textures/cache"

    // FNV-1a 64 — chave do cache (estático p/ o pipeline usar a mesma)
    static u64 hashBytes(const u8* data, size_t len);
    static std::string fileNameFor(u64 hash, CompressedFormat f);

    // hit: preenche `out` e devolve true. Qualquer ausência/corrupção →
    // false + `err` (o chamador re-comprime e faz store).
    bool load(u64 hash, CompressedImage& out, std::string& err) const;

    // persiste (best effort — false + `err` se o storage falhar)
    bool store(u64 hash, const CompressedImage& img, std::string& err);

    // ---- PASSO 5A: variantes COM SUFFIX de perfil (ver topo) ---------------
    static std::string fileNameForKey(u64 hash, const std::string& suffix);
    bool loadKeyed(u64 hash, const std::string& suffix, CompressedImage& out,
                   std::string& err) const;
    bool storeKeyed(u64 hash, const std::string& suffix,
                    const CompressedImage& img, std::string& err);

    // telemetria p/ a status line (F5.1-D)
    u32 hits() const { return hits_; }
    u32 misses() const { return misses_; }
    void resetStats() { hits_ = 0; misses_ = 0; }

private:
    ProjectStorage& st_;
    mutable u32 hits_ = 0;
    mutable u32 misses_ = 0;   // contados também dentro do load() const
};

} // namespace vv
