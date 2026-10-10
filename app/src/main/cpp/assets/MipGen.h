// assets/MipGen.h — geração de cadeia de mips em CPU (F5.1-A; PASSO 5A
// acrescenta o ESPAÇO DE COR da filtragem). GL-free.
//
// A média box 2×2 é a MESMA matemática do gate downscaleTo2K (PngLoader):
// dimensões ímpares usam o último pixel disponível (sem wrap). A cadeia
// desce até 1×1 — o upload comprimido via glCompressedTexImage2D aceita
// níveis menores que o bloco (4×4 cobre parcialmente).
//
// PASSO 5A — MipSpace: texturas de COR (conteúdo sRGB) filtram em ESPAÇO
// LINEAR (decode sRGB → média → encode sRGB): a média de luz certa; a média
// em bytes escurece os mips (o clássico 50% — o meio-preto-e-branco devia
// ser ~188, não 128). Normal maps e dados LINEARES filtram em BYTES (a
// matemática de sempre — os seus valores JÁ são lineares).
#pragma once
#include "assets/Assets.h"

namespace vv {

enum class MipSpace : u8 {
    Bytes = 0,      // média em bytes (dados lineares — normal maps; legado)
    SrgbLinear = 1, // decode sRGB → média em luz → encode sRGB (texturas de cor)
};

// reduz `in` por fator 2 (média 2×2 no espaço dado)
void halveImageRGBA(const RawImage& in, RawImage& out,
                    MipSpace space = MipSpace::Bytes);

// chain[0] = cópia de `img`; chain[n] = metade da anterior; último = 1×1
void genMipChainRGBA(const RawImage& img, std::vector<RawImage>& chain,
                     MipSpace space = MipSpace::Bytes);

// ---- transferência sRGB (expostas p/ os testes aferirem os valores) ---------
// decode: o byte sRGB → luz linear [0,1] (a fórmula oficial EGL 1.0)
float srgbToLinear(u8 v);
// encode: luz linear [0,1] → byte sRGB (arredondamento half-up; clamp)
u8 linearToSrgbByte(float v);

} // namespace vv
