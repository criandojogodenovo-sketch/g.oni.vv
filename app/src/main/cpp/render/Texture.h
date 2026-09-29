#pragma once
// render/Texture.h — textura GLES do engine (F5-D, device).
//
// Caminho do asset → GPU:
//   PNG → PngLoader (RGBA) → gate 2K → TextureCompressor (passthrough F5)
//       → Texture::createFromCompressed (glTexImage2D + glGenerateMipmap)
//
// Filtros: min LINEAR_MIPMAP_LINEAR (mips evitam cintilação a distância),
// mag LINEAR, wrap REPEAT (tiling de chão). clamp/UV custom = fase de
// materiais (F8).
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

    u32 handle() const { return tex_; }
    u32 width() const { return w_; }
    u32 height() const { return h_; }
    bool ok() const { return tex_ != 0; }

private:
    u32 tex_ = 0;
    u32 w_ = 0;
    u32 h_ = 0;
};

} // namespace vv
