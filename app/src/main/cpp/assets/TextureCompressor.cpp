// assets/TextureCompressor.cpp — passthrough da F5 (compressão real = F5.1).
#include "assets/TextureCompressor.h"

namespace vv {

bool PassthroughCompressor::compress(const RawImage& in, CompressedImage& out,
                                     std::string& err) {
    err.clear();
    out = CompressedImage{};
    if (!in.ok()) {
        err = "compressor: imagem de entrada inválida";
        return false;
    }
    out.format = CompressedFormat::RGBA8;
    out.width = in.width;
    out.height = in.height;
    out.data = in.rgba;   // identidade — bytes saem como entraram
    return true;
}

} // namespace vv
