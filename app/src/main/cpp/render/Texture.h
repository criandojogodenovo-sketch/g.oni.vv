#pragma once
// render/Texture.h — textura GLES do engine (F5-D, device).
//
// Caminho do asset → GPU:
//   PNG → PngLoader (RGBA) → gate 2K → TextureCompressor (passthrough F5)
//       → Texture::createFromCompressed (glTexImage2D + glGenerateMipmap)
//
// Filtros: min LINEAR_MIPMAP_LINEAR (mips evitam cintilação a distância),
// mag LINEAR, wrap REPEAT (tiling de chão). 0.11-M (FASE 2): o sampler do
// glTF PODE SOBREPOR este par por textura (setSampler — os códigos
// kSampler* do GOwnFormats.h; o default 0 = o par de sempre, byte a byte).
#include "core/Types.h"

namespace vv {

class CompressedImage;

// F5.1-A: true se a extensão GL_KHR_texture_compression_astc_ldr existir
// (device — chamar com contexto GL vivo; no hospedeiro devolve false).
bool glAstcSupported();

class Texture {
public:
    Texture() = default;
    ~Texture();
    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;

    // RGBA8 direto (atalho; internamente igual ao caminho comprimido)
    bool createFromRGBA(const u8* rgba, u32 w, u32 h);
    // caminho canônico: aceita o resultado do TextureCompressor
    // (F5: RGBA8; F5.1 liga os formatos GL_COMPRESSED_* aqui)
    bool createFromCompressed(const CompressedImage& img);

    void destroy();
    void bind(u32 unit) const;

    // 0.11-M (FASE 2): o sampler do glTF por CIMA do par de sempre — os
    // códigos kSampler* da casa (0 = default = NÃO mexe; o primeiro
    // setSampler vence por textura, igual à disciplina first-wins do
    // GpuAssets). wrap: 0=REPEAT, 1=CLAMP_TO_EDGE, 2=MIRRORED; filtro min
    // 0=LINEAR_MIPMAP_LINEAR, 1=NEAREST, 2=LINEAR, 3..6 = os 4 mipmap;
    // mag 0=LINEAR, 1=NEAREST, 2=LINEAR.
    void setSampler(u8 wrapS, u8 wrapT, u8 minF, u8 magF);

    u32 handle() const { return tex_; }
    u32 width() const { return w_; }
    u32 height() const { return h_; }
    bool ok() const { return tex_ != 0; }

private:
    u32 tex_ = 0;
    u32 w_ = 0;
    u32 h_ = 0;
    bool samplerSet_ = false;   // o primeiro setSampler vence (a disciplina)
};

} // namespace vv
