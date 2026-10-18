#pragma once
// assets/PngLoader.h — decode PNG → RawImage RGBA (F5-D).
//
// Usa stb_image (vendor assets/stb/stb_image.h — domínio público, sem
// dependências). Qualquer PNG (bit depth/espacamento de cor) decodifica
// para RGBA 8-bit — o formato que o pipeline do engine consome.
//
// GATE DE TAMANHO (F5; política da F5.1-A vive no TexturePipeline): com
// compressão de hardware (ETC2/ASTC) texturas 4K entram INTEIRAS — ETC2 4K
// ≈ 8 MB de VRAM, aceitável. Sem compressão (fallback raro: textura
// recusada pelo gate <256px ou device sem caminho) reduz por fator 2
// (box 2×2) até caber em 2048, com AVISO ao chamador.
//
// GL-free: testável no CI Linux (fixtures em tests/fixtures/).
#include <cstddef>
#include <string>
#include "assets/Assets.h"

namespace vv {

// false + `err` se os bytes não são um PNG decodificável
bool loadPng(const u8* data, size_t len, RawImage& out, std::string& err);

// PASSO 5A: as DIMS do PNG do header (stbi_info — SEM decodificar pixels);
// a decisão de redução precisa delas antes do trabalho. false + err = PNG
// inválido/truncado (mesma honestidade do loadPng).
bool pngDims(const u8* data, size_t len, u32& w, u32& h, std::string& err);

// reduz `img` por fator 2 (média 2×2) enquanto exceder 2048 em qualquer
// dimensão. Devolve true se a imagem foi reduzida e preenche `warn` com a
// mensagem para o utilizador ("textura 4096x4096 reduzida para 2048x2048 …").
bool downscaleTo2K(RawImage& img, std::string& warn);

} // namespace vv
