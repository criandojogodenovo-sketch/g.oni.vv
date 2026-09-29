#pragma once
// assets/TextureCompressor.h — interface de compressão de textura (F5-D/F5.1-A).
//
// F5: PASSTHROUGH — RGBA entra, RGBA sai. F5.1-A acrescenta os formatos de
// hardware: ETC2 (garantido em GLES 3.0+, via vendor/etcpak) e ASTC 4x4/6x6
// (quando a extensão GL_KHR_texture_compression_astc_ldr existe, via
// vendor/astc-encoder). `compress()` gera a CADEIA DE MIPS COMPLETA em CPU
// num único blob contíguo — o device faz um glCompressedTexImage2D por nível
// (render/Texture). O cache em disco (TextureCache) persiste o mesmo blob.
//
// Contrato: compress() NÃO muta a entrada; falha devolve false + `err`
// (nunca parcialmente preenchido). GL-free — testável no CI.
#include <string>
#include <vector>
#include "assets/Assets.h"

namespace vv {

enum class CompressedFormat : u8 {
    RGBA8,      // sem compressão (upload glTexImage2D + glGenerateMipmap)
    ETC2_RGB,   // GL_COMPRESSED_RGB8_ETC2      — 0.5 B/px (opaco)
    ETC2_RGBA,  // GL_COMPRESSED_RGBA8_ETC2_EAC — 1 B/px (com alpha)
    ASTC_4x4,   // GL_COMPRESSED_RGBA_ASTC_4x4_KHR — 1 B/px
    ASTC_6x6,   // GL_COMPRESSED_RGBA_ASTC_6x6_KHR — ~0.44 B/px
};

// um nível de mip dentro de CompressedImage::data (offset/size em bytes)
struct CompressedMip {
    u32 width = 0;
    u32 height = 0;
    u32 offset = 0;   // início deste nível no blob
    u32 size = 0;     // bytes deste nível
};

struct CompressedImage {
    CompressedFormat format = CompressedFormat::RGBA8;
    u32 width = 0;
    u32 height = 0;
    std::vector<u8> data;    // blob contíguo; nível 0 = full resolution
    std::vector<CompressedMip> mips;

    bool ok() const {
        return width > 0 && height > 0 && !data.empty() && !mips.empty();
    }
};

// ---- helpers GL-free (compartilhados por compressor / cache / upload) ------
u32 mipCountFor(u32 w, u32 h);                        // floor(log2(max))+1
u32 formatBlockX(CompressedFormat f);                 // 4 / 6 / 1 (RGBA8)
u32 formatBlockY(CompressedFormat f);
size_t mipBytesFor(CompressedFormat f, u32 w, u32 h); // bytes de UM nível
bool formatIsCompressed(CompressedFormat f);          // != RGBA8
const char* formatName(CompressedFormat f);           // "RGBA8"/"ETC2 RGB"/…

class TextureCompressor {
public:
    virtual ~TextureCompressor() = default;

    // comprime `in` (RGBA8) gerando TODOS os mips em `out`
    virtual bool compress(const RawImage& in, CompressedImage& out,
                          std::string& err) = 0;

    // nome curto p/ telemetria/log ("passthrough", "etc2", "astc", "auto")
    virtual const char* name() const = 0;
};

// F5: identidade — RGBA in → RGBA out (1 nível; mips geram na GPU).
// Existe para que todo o caminho passe POR AQUI quando não há compressão
// (texturas < 256px e fallback sem suporte de hardware).
class PassthroughCompressor final : public TextureCompressor {
public:
    bool compress(const RawImage& in, CompressedImage& out,
                  std::string& err) override;
    const char* name() const override { return "passthrough"; }
};

// F5.1-A: ETC2 via etcpak (vendor/etcpak, BSD). Escolhe RGB8 (opaco) ou
// RGBA8_EAC (alpha) sozinho, gerando a cadeia de mips completa em CPU.
// `heuristics=false` restringe os blocos a individual/diff (decodificáveis
// pelo decoder dos testes do CI); no device usa true (T/H/planar melhoram).
class Etc2Compressor final : public TextureCompressor {
public:
    bool compress(const RawImage& in, CompressedImage& out,
                  std::string& err) override;
    const char* name() const override { return "etc2"; }
    bool heuristics = true;
};

// F5.1-A: ASTC LDR linear via astc-encoder (vendor/astc-encoder, Apache-2.0).
// Bloco 4x4 (default) ou 6x6;RGBA sempre (alpha no bloco).
class AstcCompressor final : public TextureCompressor {
public:
    explicit AstcCompressor(u32 block = 4);
    bool compress(const RawImage& in, CompressedImage& out,
                  std::string& err) override;
    const char* name() const override { return "astc"; }
    u32 blockX() const { return block_; }

private:
    u32 block_ = 4;
};

// F5.1-A: FACADE de seleção — "ASTC se a extensão existir, senão ETC2;
// não comprimir texturas < 256px (overhead)". O device injeta o resultado
// da detecção de extensão (render/Texture.cpp glAstcSupported()).
class HardwareCompressor final : public TextureCompressor {
public:
    void setAstcSupported(bool s) { astcSupported_ = s; }
    bool astcSupported() const { return astcSupported_; }

    // regra do gate: ambas as dims >= 256 → comprime; menor → RGBA (custo
    // de bloco > ganho em sprites pequenos)
    static bool compressible(u32 w, u32 h);

    bool compress(const RawImage& in, CompressedImage& out,
                  std::string& err) override;
    const char* name() const override { return "auto"; }

    // última decisão (telemetria/status line do editor)
    CompressedFormat lastFormat() const { return lastFormat_; }

private:
    bool astcSupported_ = false;
    CompressedFormat lastFormat_ = CompressedFormat::RGBA8;
    Etc2Compressor etc2_;
    AstcCompressor astc4_{4};
};

} // namespace vv
