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
// 0.10-M (PASSO 2): o ESCRITOR só escreve v3 (blocos, 64-bit, float32 —
// sem quantização); o LEITOR abre v1 e v2 (o payload v1 intacto; a v2 é a
// interpretação tolerante documentada — ver docs/GMESH_formato.md) e v3.
constexpr u16 kGmeshVersionWrite  = 3;   // o escritor grava ISTO
constexpr u16 kGmeshVersionMinRead = 1;  // a versão mais antiga que abre

// escreve MeshData → bytes .gmesh v3 (blocos: 1 por grupo — a pool do
// bloco são os vértices REFERENCIADOS pelo grupo, em ordem de 1.ª
// referência; sem grupos = 1 bloco único). SEM PERDA: pos/normal/uv em
// float32 exato (a quantização 16-bit do v1 só existe nos ficheiros v1).
// Vértices não referenciados por nenhum grupo não sobrevivem ao corte
// (não afetam o conjunto de triângulos — documentado no formato).
bool writeGMesh(const MeshData& m, std::vector<u8>& out, std::string& err);

// lê bytes .gmesh (v1, v2 ou v3) → MeshData. v1/v2: dequantizado como
// sempre. v3: os blocos montados por ordem da tabela (1 grupo por bloco).
// Um v3 com > 65535 vértices NÃO monta (MeshData é u16): erro legível que
// nomeia o PASSO 4 (render por blocos) — o ficheiro está correto.
bool readGMesh(const u8* bytes, size_t len, MeshData& out, std::string& err);

// ---- .gmesh v3 — o formato (a spec viva: docs/GMESH_formato.md §v3) --------
// Layout: [header comum 32][metadados fixos 160][dados dos blocos,
// cada um alinhado a 16][tabela de blocos 80 B/entrada][materiais].
// O checksum do header comum num v3 cobre SÓ os 160 B de metadados (o
// FNV-1a do payload inteiro obrigava a ler o ficheiro inteiro — inimigo
// do mmap/streaming); a integridade do resto vive na tabela: CRC32 por
// bloco + CRC32 da própria tabela.
constexpr size_t kGmeshV3MetaBytes = 160;   // o bloco de metadados (fixo)
constexpr u32    kGmeshV3BlockVertexCap = 65535;  // o teto configurável
constexpr u32    kGmeshV3MaxAttrs = 8;
constexpr u32    kGmeshV3BlockEntryBytes = 80;    // entrada da tabela

// semânticas de atributo (o layout é DESCRITO no header — acrescentar um
// atributo NÃO muda a versão; um leitor que não conheça a semântica usa
// o tamanho do descritor para saltar)
enum : u8 {
    kAttrPosition = 0, kAttrNormal = 1, kAttrUv0 = 2, kAttrTangent = 3,
    kAttrUv1 = 4, kAttrColor = 5, kAttrBones = 6, kAttrWeights = 7,
};
// armazenamento por atributo (o v3 do conversor escreve f32; u16/u8
// existem para o modo compacto opt-in do PASSO 3 e para o futuro)
enum : u8 { kAttrF32 = 0, kAttrU16 = 1, kAttrU8 = 2 };

struct GMeshV3Attr {
    u8 semantic = 0;    // kAttr*
    u8 storage = kAttrF32;
    u8 normalized = 0;  // u16/u8 em [0,1]/[-1,1] (u16/u8 de pele: 0=raw)
    u8 reserved = 0;
    u32 reserved2 = 0;
};

struct GMeshV3Meta {
    u32 flags = 0;   // bit0 = skinned (bones+weights por vértice)
    u32 blockVertexCap = kGmeshV3BlockVertexCap;
    u64 vertexCount = 0;   // TOTAL (pode passar 2^32 — é o ponto do v3)
    u64 indexCount = 0;    // TOTAL (múltiplo de 3)
    u64 blockCount = 0;
    u64 materialCount = 0;
    Vec3 aabbMin{}, aabbMax{};      // AABB global (todas as posições)
    u64 blockTableOffset = 0;       // offset ABSOLUTO no ficheiro
    u64 materialTableOffset = 0;    // offset ABSOLUTO
    GMeshV3Attr attrs[kGmeshV3MaxAttrs]{};
    u32 attrCount = 0;
    u32 tableCrc32 = 0;             // CRC32 das entradas da tabela
    // bytes por vértice (a soma dos componentes dos descritores)
    u32 vertexStride() const;
    bool hasAttr(u8 semantic) const;
    const GMeshV3Attr* attr(u8 semantic) const;
};

struct GMeshV3Block {
    u64 dataOffset = 0;   // ABSOLUTO (o mmap salta direto aqui)
    u64 dataSize = 0;     // verts×stride + índices
    u64 vertexCount = 0;
    u64 indexCount = 0;   // múltiplo de 3
    Vec3 aabbMin{}, aabbMax{};
    u32 materialIndex = 0;
    u32 indexType = 0;    // 0 = u16 local, 1 = u32 local
    u32 crc32 = 0;        // dos dataSize bytes em dataOffset
};

// CRC32 (IEEE, tabela própria — sem dependência de zlib; os formatos
// próprios são GL-free E lib-free)
u32 gcrc32(const u8* data, size_t len);

// lê SÓ header + metadados + tabela (NUNCA aloca os dados dos blocos) —
// o caminho do PASSO 4 e das contagens acima de 2^32. Os materiais vêm
// como nomes (a tabela de materiais é pequena).
bool readGMeshV3Meta(const u8* bytes, size_t len, GMeshV3Meta& meta,
                     std::vector<GMeshV3Block>& blocks,
                     std::vector<std::string>& materials, std::string& err);

// materializa UM bloco → MeshData de 1 grupo (o streaming do PASSO 3 na
// verificação e, no PASSO 4, o carregamento por blocos)
bool readGMeshV3Block(const u8* bytes, size_t len, const GMeshV3Meta& meta,
                      const GMeshV3Block& blk, MeshData& out,
                      std::string& err);

// um bloco de ENTRADA do escritor v3 (a pool já com índices locais; o
// conversor do PASSO 3 produz isto em streaming)
struct GMeshV3BlockIn {
    std::vector<Vertex> vertices;   // a pool do bloco (≤ cap)
    std::vector<u16> indices;       // LOCAIS (usados se !use32)
    std::vector<u32> indices32;     // LOCAIS (usados se use32)
    std::vector<u8>  bones;         // 4/vert — só se skin (soma = 4×verts)
    std::vector<f32> weights;       // 4/vert — só se skin (soma = 4×verts)
    Vec3 aabbMin{}, aabbMax{};
    std::string material;           // nome (dedup na escrita)
    bool use32 = false;
};

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
