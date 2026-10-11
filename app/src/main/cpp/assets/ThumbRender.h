#pragma once
// assets/ThumbRender.h — a MINIATURA 256×256 de um asset (PASSO 5C).
//
// O render é SOFTWARE (GL-free, afervel no CI): câmara ortográfica fixa
// que ENCAIXA o AABB do mesh, z-buffer, lambert flat com a luz da casa
// (a MESMA direção do kLightDir do Material) e fundo escuro. O import
// escreve thumbs/<stem>.png (256×256, PNG via zlib do render/ThumbPng) e
// o browser (os seletores de assets) mostra o ficheiro PRONTO — instantâneo
// (o pin: 100 thumbs carregadas+decodificadas em <500 ms, ThumbIndex).
//
// O ORÇAMENTO (a disciplina da casa): um .gmesh gigante contribui com os
// PRIMEIROS triângulos (kMaxTrisPorThumb) — a pré-visualização é
// representativa e o pico de RAM é o do bloco, nunca o modelo.
#include <string>
#include <vector>
#include "assets/Assets.h"
#include "core/Types.h"

namespace vv {
namespace thumbrender {

constexpr u32 kThumbSize = 256u;
constexpr u32 kMaxTrisPorThumb = 4096u;

// o render do mesh → rgba (size*size*4). false = mesh vazio (o chamador
// usa o placeholder). DETERMINÍSTICO: mesmos vértices → mesmos bytes.
bool renderMesh(const MeshData& mesh, u32 size, std::vector<u8>& rgba);

// o PLACEHOLDER (o asset sem thumb / mesh vazio): fundo + quadro + as
// iniciais do stem (1.ª+última letra — a regra D18 das telas de projeto)
void placeholder(const std::string& stem, u32 size, std::vector<u8>& rgba);

// RGBA → PNG (RGB, zlib real do render/ThumbPng). Vazio = falha.
std::vector<u8> encodePng(const std::vector<u8>& rgba, u32 size);

} // namespace thumbrender
} // namespace vv
