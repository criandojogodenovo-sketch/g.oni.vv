// render/Texture.cpp — upload GLES de textura (F5-D; F5.1-A comprimido).
//
// RGBA8: glTexImage2D nível 0 + glGenerateMipmap (mips na GPU — texturas
// pequenas e fallback). Comprimido (ETC2/ASTC): a cadeia de mips chega
// PRONTA da CPU (TextureCompressor) e sobe UM glCompressedTexImage2D por
// nível — a GPU nunca descomprime, a memória de vídeo é a do blob.
#include "render/Texture.h"
#include "assets/TextureCompressor.h"
#include "platform/Log.h"
#include <cstring>
#include <GLES3/gl3.h>

// GLES3.0 não declara as enums ASTC (chegam com a extensão KHR); os valores
// são fixos no registry — guarda contra headers antigos do NDK.
#ifndef GL_COMPRESSED_RGBA_ASTC_4x4_KHR
#define GL_COMPRESSED_RGBA_ASTC_4x4_KHR 0x93B0
#endif
#ifndef GL_COMPRESSED_RGBA_ASTC_6x6_KHR
#define GL_COMPRESSED_RGBA_ASTC_6x6_KHR 0x93B4
#endif

namespace vv {

// F5.1-A: ASTC só se a extensão LDR existir (C33/Mali: sim; Adreno antigo:
// às vezes não). Sem extensão → ETC2 (garantido em GLES 3.0+).
bool glAstcSupported() {
    GLint num = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &num);
    if (num > 0) {
        for (GLint i = 0; i < num; ++i) {
            const GLubyte* e = glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i));
            if (e && std::strstr(reinterpret_cast<const char*>(e),
                                 "texture_compression_astc_ldr")) {
                return true;
            }
        }
        return false;
    }
    // fallback para drivers que ainda expõem a string clássica
    const GLubyte* all = glGetString(GL_EXTENSIONS);
    return all && std::strstr(reinterpret_cast<const char*>(all),
                              "texture_compression_astc_ldr") != nullptr;
}

namespace {

GLenum internalFormatFor(CompressedFormat f) {
    switch (f) {
        case CompressedFormat::ETC2_RGB:  return GL_COMPRESSED_RGB8_ETC2;
        case CompressedFormat::ETC2_RGBA: return GL_COMPRESSED_RGBA8_ETC2_EAC;
        case CompressedFormat::ASTC_4x4:  return GL_COMPRESSED_RGBA_ASTC_4x4_KHR;
        case CompressedFormat::ASTC_6x6:  return GL_COMPRESSED_RGBA_ASTC_6x6_KHR;
        default:                          return 0;
    }
}

} // namespace

Texture::~Texture() {
    destroy();
}

bool Texture::createFromRGBA(const u8* rgba, u32 w, u32 h) {
    CompressedImage img;
    img.format = CompressedFormat::RGBA8;
    img.width = w;
    img.height = h;
    img.data.assign(rgba, rgba + static_cast<size_t>(w) * h * 4u);
    CompressedMip m;
    m.width = w;
    m.height = h;
    m.offset = 0;
    m.size = static_cast<u32>(img.data.size());
    img.mips.push_back(m);
    return createFromCompressed(img);
}

bool Texture::createFromCompressed(const CompressedImage& img) {
    destroy();
    if (!img.ok()) {
        LOGE("Texture: imagem inválida (%ux%u)", img.width, img.height);
        return false;
    }
    glGenTextures(1, &tex_);
    if (!tex_) {
        LOGE("Texture: falha ao criar objeto GL");
        return false;
    }
    glBindTexture(GL_TEXTURE_2D, tex_);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);   // blocos comprimidos: sem padding

    if (img.format == CompressedFormat::RGBA8) {
        // caminho F5: RGBA direto + mips gerados na GPU (fallback/pequenas)
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                     static_cast<GLsizei>(img.width),
                     static_cast<GLsizei>(img.height),
                     0, GL_RGBA, GL_UNSIGNED_BYTE, img.data.data());
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        // caminho F5.1-A: um glCompressedTexImage2D por nível de mip
        const GLenum internal = internalFormatFor(img.format);
        bool okLevel = true;
        for (size_t i = 0; i < img.mips.size() && okLevel; ++i) {
            const CompressedMip& m = img.mips[i];
            glCompressedTexImage2D(GL_TEXTURE_2D, static_cast<GLint>(i),
                                   internal,
                                   static_cast<GLsizei>(m.width),
                                   static_cast<GLsizei>(m.height),
                                   0, static_cast<GLsizei>(m.size),
                                   img.data.data() + m.offset);
            okLevel = glGetError() == 0;
        }
        if (!okLevel) {
            LOGE("Texture: upload comprimido falhou (%s %ux%u)",
                 formatName(img.format), img.width, img.height);
            glBindTexture(GL_TEXTURE_2D, 0);
            destroy();
            return false;
        }
    }

    // mips: chão/parede vistos de longe no C33 — sem mips cintila
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    img.mips.size() > 1 ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);

    w_ = img.width;
    h_ = img.height;
    LOGI("Texture: %ux%u %s (%u mips) pronta", img.width, img.height,
         formatName(img.format), static_cast<unsigned>(img.mips.size()));
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
