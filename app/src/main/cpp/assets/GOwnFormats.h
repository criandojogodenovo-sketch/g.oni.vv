#pragma once
// assets/GOwnFormats.h — FORMATOS PRÓPRIOS da engine (0.8.10).
//
// .gmesh / .gtext / .gm: os convertidos do import. O RUNTIME carrega SÓ
// estes (as fontes — obj/gltf/glb/png — vivem em source/ e são tocadas
// apenas pelo CONVERSOR: re-parse por arranque = zero).
//
// HEADER COMUM (32 bytes, little-endian — ver GOwnFormats.cpp):
//   magic[4]    "GMES" | "GVTX" | "GANM"
//   version     u16 (1)
//   endianMark  u16 (0x1A2B — lido de volta: se vier trocado, o ficheiro
//               veio de outra endianess → erro legível, NUNCA lixo)
//   align       u32 (8 — alinhamento das secções; eco informativo)
//   payloadSize u64
//   checksum    u64 (FNV-1a do PAYLOAD — corrupção → erro legível)
//
// PAYLOADS:
//   .gmesh — indexado+deduplicado (MeshData já é), QUANTIZADO 16-bit:
//     pos u16×3 no AABB (dequant: mn + q/65535·ext), normal u16×3 em
//     [-1,1] (dequant + renormaliza), uv u16×2 em [0,1]; índices u16;
//     grupos (nome/material/first/count). 8 B/vértice vs 32 B do Vertex
//     float — o RÁCIO do relatório (vs OBJ texto ~30-60 B/v).
//     Skin (joints u8×4 + weights f32... NÃO: weights u16 em [0,1] e a
//     soma renormaliza) — vazio = estático.
//   .gtext — CompressedImage (formato + dims + tabela de mips + blob):
//     o produto do TexturePipeline (ASTC/ETC2) persistido SEM re-comprimir
//     por arranque; upload direto glCompressedTexImage2D.
//   .gm — clips ANIMADOS (tracks/keys com tangentes) binários.
//
// TUDO GL-free/Android-free → testável no CI com bytes em memória.
#include <string>
#include <vector>

#include "assets/Assets.h"
#include "assets/TextureCompressor.h"
#include "components/AnimationPlayer.h"
#include "components/SkeletonComp.h"
#include "core/Types.h"

namespace vv {

// ---- header comum -----------------------------------------------------------
struct GFileHeader {
    char magic[4] = {0, 0, 0, 0};
    u16   version = 1;
    u16   endianMark = 0x1A2B;
    u32   align = 8;
    u64   payloadSize = 0;
    u64   checksum = 0;   // FNV-1a do payload
};

// FNV-1a 64 do payload (a MESMA do primMeshHash — uma só convenção)
u64 gfnv1a(const u8* data, size_t len);

// lê+valida o header comum (magic esperado, endian, versão, tamanho,
// checksum do payload). Erro → false + `err` LEGÍVEL (nunca crash).
bool gReadHeader(const u8* bytes, size_t len, const char* wantMagic,
                 GFileHeader& h, std::string& err);

// ---- .gmesh -----------------------------------------------------------------
// escreve MeshData → bytes .gmesh (false + err se > 65535 vértices ou
// geometria vazia — o limite u16 do engine, dito com número)
bool writeGMesh(const MeshData& m, std::vector<u8>& out, std::string& err);

// lê bytes .gmesh → MeshData (dequantizado). Checksum errado / magic
// trocado / contagens inconsistentes → false + err legível.
bool readGMesh(const u8* bytes, size_t len, MeshData& out, std::string& err);

// ---- .gtext -----------------------------------------------------------------
bool writeGText(const CompressedImage& img, std::vector<u8>& out,
                std::string& err);
bool readGText(const u8* bytes, size_t len, CompressedImage& out,
               std::string& err);

// ---- .gm (clips de animação + esqueleto opcional) ---------------------------
struct GAnimFile {
    std::vector<AnimClip> clips;
    // esqueleto (skins glTF convertidas; vazio = sem): joints com TRS +
    // bind TRS + inverseBind — o loader cria o SkeletonComp do TIC
    std::vector<SkeletonComp::Joint> joints;
};
bool writeGAnim(const GAnimFile& anim, std::vector<u8>& out,
                std::string& err);
bool readGAnim(const u8* bytes, size_t len, GAnimFile& out,
               std::string& err);

} // namespace vv
