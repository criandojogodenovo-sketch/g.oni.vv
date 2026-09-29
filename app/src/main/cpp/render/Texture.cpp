// render/Texture.cpp — upload GLES de textura com mipmaps (F5-D, device).
#include "render/Texture.h"
#include "assets/TextureCompressor.h"
#include "platform/Log.h"
#include <GLES3/gl3.h>

namespace vv {

Texture::~Texture() {
    destroy();
}

bool Texture::createFromRGBA(const u8* rgba, u32 w, u32 h) {
    CompressedImage img;
    img.format = CompressedFormat::RGBA8;
    img.width = w;
    img.height = h;
    img.data.assign(rgba, rgba + static_cast<size_t>(w) * h * 4u);
    return createFromCompressed(img);
}

bool Texture::createFromCompressed(const CompressedImage& img) {
    destroy();
    if (!img.ok()) {
        LOGE("Texture: imagem inválida (%ux%u)", img.width, img.height);
        return false;
    }
    if (img.format != CompressedFormat::RGBA8) {
        // F5.1: glCompressedTexImage2D com GL_COMPRESSED_RGBA_ASTC_4x4_KHR /
        // GL_COMPRESSED_RGB8_ETC2 — por agora recusamos com erro claro
        LOGE("Texture: formato comprimido chegando antes da F5.1");
        return false;
    }
    glGenTextures(1, &tex_);
    if (!tex_) {
        LOGE("Texture: falha ao criar objeto GL");
        return false;
    }
    glBindTexture(GL_TEXTURE_2D, tex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 static_cast<GLsizei>(img.width),
                 static_cast<GLsizei>(img.height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, img.data.data());
    // mips: chão/parede vistos de longe no C33 — sem mips cintila
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

    w_ = img.width;
    h_ = img.height;
    LOGI("Texture: %ux%u RGBA8 + mips pronta", img.width, img.height);
    return true;
}

void Texture::destroy() {
    if (tex_) {
        glDeleteTextures(1, &tex_);
        tex_ = 0;
    }
    w_ = h_ = 0;
}

void Texture::bind(u32 unit) const {
    if (!tex_) {
        return;
    }
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, tex_);
}

} // namespace vv
