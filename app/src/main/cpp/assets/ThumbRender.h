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

// 0.11-W (3.75b) — o render do IMPOSTOR: o MESMO rasterizador software
// (a MESMA câmara isométrica 3/4, o MESMO z-buffer) com DUAS diferenças
// honestas: (1) o fundo é TRANSPARENTE (rgba 0 — o MASK do lit corta o
// quad no alpha); (2) a geometria carrega o lambert CRU d = max(n·L, 0)
// (SEM ambient, sem os offsets de tom da thumb — o draw compensa com o
// baseColor kImpBaseColor e produz a família 0.16+0.62·d da casa; ver o
// render/Impostor.h). O pico de RAM é o MESMO do render de sempre.
bool renderMeshAlpha(const MeshData& mesh, u32 size, std::vector<u8>& rgba);

// o PLACEHOLDER (o asset sem thumb / mesh vazio): fundo + quadro + as
// iniciais do stem (1.ª+última letra — a regra D18 das telas de projeto)
void placeholder(const std::string& stem, u32 size, std::vector<u8>& rgba);

// RGBA → PNG (RGB, zlib real do render/ThumbPng). Vazio = falha.
std::vector<u8> encodePng(const std::vector<u8>& rgba, u32 size);

// 0.11-W (3.75b): RGBA → PNG RGBA (color type 6 — o alpha é o canal do
// MASK). Vazio = falha.
std::vector<u8> encodePngRgba(const std::vector<u8>& rgba, u32 size);

} // namespace thumbrender
} // namespace vv
