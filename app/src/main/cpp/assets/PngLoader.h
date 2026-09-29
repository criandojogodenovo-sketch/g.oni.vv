#pragma once
// assets/PngLoader.h — decode PNG → RawImage RGBA (F5-D).
//
// Usa stb_image (vendor assets/stb/stb_image.h — domínio público, sem
// dependências). Qualquer PNG (bit depth/espacamento de cor) decodifica
// para RGBA 8-bit — o formato que o pipeline do engine consome.
//
// GATE DE TAMANHO (F5): 1K/2K passam como estão; texturas 4K (ou maiores)
// são reduzidas por fator 2 (box 2×2) até caber em 2048 — com AVISO ao
// chamador. O gate REAL (compressão ASTC/ETC2) é a F5.1; aqui o objetivo é
// não subir uma textura gigante para a GPU do C33 sem precisar.
//
// GL-free: testável no CI Linux (fixtures em tests/fixtures/).
#include <cstddef>
#include <string>
#include "assets/Assets.h"

namespace vv {

// false + `err` se os bytes não são um PNG decodificável
bool loadPng(const u8* data, size_t len, RawImage& out, std::string& err);

// reduz `img` por fator 2 (média 2×2) enquanto exceder 2048 em qualquer
// dimensão. Devolve true se a imagem foi reduzida e preenche `warn` com a
// mensagem para o utilizador ("textura 4096x4096 reduzida para 2048x2048 …").
bool downscaleTo2K(RawImage& img, std::string& warn);

} // namespace vv
