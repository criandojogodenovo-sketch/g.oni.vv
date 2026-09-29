// assets/MipGen.h — geração de cadeia de mips em CPU (F5.1-A). GL-free.
//
// A média box 2×2 é a MESMA matemática do gate downscaleTo2K (PngLoader):
// dimensões ímpares usam o último pixel disponível (sem wrap). A cadeia
// desce até 1×1 — o upload comprimido via glCompressedTexImage2D aceita
// níveis menores que o bloco (4×4 cobre parcialmente).
#pragma once
#include "assets/Assets.h"

namespace vv {

// reduz `in` por fator 2 (média 2×2)
void halveImageRGBA(const RawImage& in, RawImage& out);

// chain[0] = cópia de `img`; chain[n] = metade da anterior; último = 1×1
void genMipChainRGBA(const RawImage& img, std::vector<RawImage>& chain);

} // namespace vv
