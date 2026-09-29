#pragma once
// assets/TextureCompressor.h — interface de compressão de textura (F5-D).
//
// F5: PASSTHROUGH — RGBA entra, RGBA sai (sem recompressão). A F5.1 troca
// por implementações ASTC/ETC2 da MESMA interface; os chamadores (ResourceManager,
// render/Texture) já falam em CompressedImage, então nada acima muda.
//
// Contrato: compress() NÃO muta a entrada; falha devolve false + `err`
// (nunca parcialmente preenchido). GL-free — testável no CI.
#include <string>
#include <vector>
#include "assets/Assets.h"

namespace vv {

enum class CompressedFormat : u8 {
    RGBA8,   // F5: identidade (upload via glTexImage2D + mips)
    ASTC,    // F5.1
    ETC2,    // F5.1
};

struct CompressedImage {
    CompressedFormat format = CompressedFormat::RGBA8;
    u32 width = 0;
    u32 height = 0;
    std::vector<u8> data;

    bool ok() const {
        return width > 0 && height > 0 && !data.empty();
    }
};

class TextureCompressor {
public:
    virtual ~TextureCompressor() = default;

    // comprime `in` para `out` (ou transporta sem alteração, no passthrough)
    virtual bool compress(const RawImage& in, CompressedImage& out,
                          std::string& err) = 0;

    // nome curto p/ telemetria/log ("passthrough", "astc", "etc2"…)
    virtual const char* name() const = 0;
};

// Implementação F5: identidade — RGBA in → RGBA out. Existe para que todo
// o caminho (loader → gate 2K → compressor → GPU) já passe POR AQUI na F5;
// a F5.1 adiciona ASTC/ETC2 sem tocar nos chamadores.
class PassthroughCompressor final : public TextureCompressor {
public:
    bool compress(const RawImage& in, CompressedImage& out,
                  std::string& err) override;
    const char* name() const override { return "passthrough"; }
};

} // namespace vv
