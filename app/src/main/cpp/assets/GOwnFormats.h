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
// `props` (0.11-M FASE 2): o GTB2 — o MESMO contrato do gmeshV3Skeleton.
struct GMeshMatBind;   // 0.10-M HOTFIX: os binds (definidos mais abaixo)
struct GMeshMatProps;  // 0.11-M FASE 2: as props (definidas mais abaixo)
// `withLod` (0.11-T FASE 3.5a): GERA a cauda GLOD (o decimate ½ por
// bloco + o índice + os blobs). false (o default) = byte a byte com o
// escritor pré-3.5a — os round-trips/exports não mudam; os IMPORTS
// (AssetConverter) passam true (a geração de LOD é do import).
bool writeGMesh(const MeshData& m, std::vector<u8>& out, std::string& err,
                const std::vector<GMeshMatBind>* binds = nullptr,
                const std::vector<GMeshMatProps>* props = nullptr,
                bool withLod = false);

// lê bytes .gmesh (v1, v2 ou v3) → MeshData. v1/v2: dequantizado como
// sempre. v3: os blocos montados por ordem da tabela (1 grupo por bloco).
// Um v3 com > 65535 vértices NÃO monta (MeshData é u16): erro legível que
// nomeia o PASSO 4 (render por blocos) — o ficheiro está correto.
// 0.10-M (PASSO 3B): a mensagem COMEÇA por «memória insuficiente ao
// carregar mesh (cura no PASSO 4: render por blocos)» — o load de hoje
// monta o mesh ÚNICO em RAM (o par file+MeshData) e é ESSA a parede do
// modelo de 203 MB do dono; a causa fica registada com números, NÃO
// contornada (o PASSO 4 é a cura). O orçamento declarado
// (kMeshLoadBudgetBytes) vigia o caso «poucos vértices, índices gigantes»
// com a MESMA mensagem.
bool readGMesh(const u8* bytes, size_t len, MeshData& out, std::string& err);

// ---- .gmesh v3 — o formato (a spec viva: docs/GMESH_formato.md §v3) --------
// Layout: [header comum 32][metadados fixos 160][dados dos blocos,
// cada um alinhado a 16][tabela de blocos 80 B/entrada][materiais].
// O checksum do header comum num v3 cobre SÓ os 160 B de metadados (o
// FNV-1a do payload inteiro obrigava a ler o ficheiro inteiro — inimigo
// do mmap/streaming); a integridade do resto vive na tabela: CRC32 por
// bloco + CRC32 da própria tabela.
constexpr size_t kGHeaderBytes = 32;        // o header comum (CONTRATO)
constexpr size_t kGmeshV3MetaBytes = 160;   // o bloco de metadados (fixo)
constexpr u32    kGmeshV3BlockVertexCap = 65535;  // o teto configurável
constexpr u32    kGmeshV3MaxAttrs = 8;
constexpr u32    kGmeshV3BlockEntryBytes = 80;    // entrada da tabela

// 0.10-M (EXT) — os bits do campo `flags` do meta v3. bit0 = skinned (o
// de sempre). bit1 = PEÇA do import expandido (0.10-M-ext: este .gmesh
// é UM NÓ do modelo — «assets/cidade_c7.gmesh»; quem o ABRE deve usar o
// caminho POR BLOCOS do BlockMesh mesmo sendo pequeno, porque é isso que
// dá o culling por TIC do import expandido — ver GpuAssets::mesh e
// GMESH_formato.md §EXPANDIR-NÓS). Ficheiros merged (o default) ficam a 0
// — NADA muda para eles.
constexpr u32 kGmeshV3FlagSkinned = 0x1u;
constexpr u32 kGmeshV3FlagPiece   = 0x2u;
// 0.11-T (FASE 3.5a): bit2 = o ficheiro traz a cauda GLOD (o LOD por
// distância). O MESMO padrão das peças: um bit no meta que muda o
// comportamento de QUEM ABRE — os leitores de sempre ignoram a cauda
// (o payloadSize do header NÃO conta os bytes GLOD: eles vivem DEPOIS
// do fim da secção de materiais, onde os leitores v3 nunca chegam).
// Ficheiro novo em app antiga perde SÓ o LOD (o mesh fica inteiro);
// ficheiro antigo em app nova abre sem LOD (a cauda não existe).
constexpr u32 kGmeshV3FlagLOD     = 0x4u;

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
    u32 flags = 0;   // bits: kGmeshV3FlagSkinned | kGmeshV3FlagPiece
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

// 0.10-M (PASSO 3B) — O ORÇAMENTO DO LOAD INTEIRO: o runtime de hoje
// materializa o MeshData ÚNICO (u16) — verts×sizeof(Vertex) + índices×2 B
// (+ pele 20 B/vértice quando skinned). Acima do orçamento o load RECUSA
// com a mensagem «memória insuficiente ao carregar mesh (cura no PASSO 4:
// render por blocos)» + os números. 256 MB = o número da casa (o teto de
// range R-032): num C33 (3-4 GB com LMK + VRAM partilhada) um MeshData de
// 256 MB pede ~512 MB vivos (RAM + upload GPU) — a beira do device; o
// PASSO 4 substitui este teto pelo orçamento do CACHE de blocos.
constexpr u64 kMeshLoadBudgetBytes = 256ull * 1024 * 1024;

// A ESTIMATURA PURA (bytes de MeshData que o load materializaria a partir
// da META v3 — sem abrir nada): verts×sizeof(Vertex) + índices×2 + pele.
// Exportada porque é o contrato AFERÍVEL (o teste pino sem ficheiro real).
u64 gmeshV3LoadEstimateBytes(const GMeshV3Meta& meta);

// 0.10-M (PASSO 3B) — O ESPIÃO DO GUARDO: header + meta do v3 validados num
// buffer de kGHeaderBytes + kGmeshV3MetaBytes (192 B) lido com
// ProjectStorage::readBytesAt ANTES do ficheiro inteiro. false com err em
// tudo o que não seja um v3 válido (o chamador cai no caminho de sempre).
// A MENSAGEM da recusa (gmeshV3LoadRefusalErr) é a MESMA do readGMesh —
// uma só fonte de verdade entre o guard cedo e a rede do caminho inteiro.
bool gmeshV3PeekMeta(const u8* bytes, size_t len, GMeshV3Meta& meta,
                     std::string& err);

// a mensagem de recusa do load (o guard cedo e o readGMesh dizem o MESMO)
std::string gmeshV3LoadRefusalErr(const GMeshV3Meta& meta);

// lê SÓ header + metadados + tabela (NUNCA aloca os dados dos blocos) —
// o caminho do PASSO 4 e das contagens acima de 2^32. Os materiais vêm
// como nomes (a tabela de materiais é pequena). `binds` (0.10-M HOTFIX
// TEXTURAS-VINCULA): as vinculações do bloco GTB1 da cauda (se houver).
// `props` (0.11-M FASE 2): as propriedades do bloco GTB2 (se houver) —
// o MESMO contrato de cauda do binds. `lod` (0.11-T FASE 3.5a): os
// blocos LOD da cauda GLOD (se o meta tiver kGmeshV3FlagLOD e o
// chamador os pedir) — vazia em ficheiros sem LOD.
bool readGMeshV3Meta(const u8* bytes, size_t len, GMeshV3Meta& meta,
                     std::vector<GMeshV3Block>& blocks,
                     std::vector<std::string>& materials, std::string& err,
                     std::vector<GMeshMatBind>* binds = nullptr,
                     std::vector<GMeshMatProps>* props = nullptr,
                     std::vector<GMeshV3Block>* lod = nullptr);

// materializa UM bloco → MeshData de 1 grupo (o streaming do PASSO 3 na
// verificação e, no PASSO 4, o carregamento por blocos)
bool readGMeshV3Block(const u8* bytes, size_t len, const GMeshV3Meta& meta,
                      const GMeshV3Block& blk, MeshData& out,
                      std::string& err);

// ---- 0.10-M (PASSO 4) — abertura por FAIXAS (o render por blocos) ---------
// O layout v3 põe os DADOS dos blocos ANTES da tabela ([header][meta]
// [dados][tabela][materiais]) — abrir pela tabela = ler 192 B de
// header+meta (gmeshV3PeekMeta), a TABELA pela sua faixa e os MATERIAIS
// pela faixa deles: NUNCA um byte dos dados dos blocos. Os validadores
// são OS MESMOS do caminho inteiro (readGMeshV3Meta chama estas funções
// sobre o ficheiro inteiro — zero drift entre os dois leitores).
//
// gmeshV3ReadTableOnly: `tableBytes` = os blockCount×80 B da faixa da
// tabela (valida cada entrada E o CRC32 da própria tabela).
bool gmeshV3ReadTableOnly(const u8* tableBytes, size_t tableLen,
                          const GMeshV3Meta& meta,
                          std::vector<GMeshV3Block>& blocks,
                          std::string& err);

// gmeshV3ReadMaterialsOnly: `matBytes` = a faixa dos materiais (do
// materialTableOffset em diante — o fim pode trazer bytes a mais do que
// os nomes precisam; o reader para em materialCount nomes). `binds`
// (0.10-M HOTFIX TEXTURAS-VINCULA): parse do bloco GTB1 da CAUDA (os
// bytes depois dos nomes) — ausente = vazio; corrompido = false.
// `props` (0.11-M FASE 2): parse do bloco GTB2 DEPOIS do GTB1 — o MESMO
// contrato (ausente = vazio = defaults; corrompido = false).
bool gmeshV3ReadMaterialsOnly(const u8* matBytes, size_t matLen,
                              const GMeshV3Meta& meta,
                              std::vector<std::string>& materials,
                              std::string& err,
                              std::vector<GMeshMatBind>* binds = nullptr,
                              std::vector<GMeshMatProps>* props = nullptr,
                              std::vector<GMeshV3Block>* lod = nullptr);

// gmeshV3MaterializeBlock: materializa UM bloco a partir da FAIXA EXATA
// dos seus dados (dataLen == blk.dataSize, lida por readBytesAt — o
// readGMeshV3Block de sempre espera o ficheiro INTEIRO e valida offsets
// absolutos; este é o par por-faixa do runtime por blocos). O CRC32 do
// bloco é verificado ANTES de qualquer parse (o mesmo contrato).
bool gmeshV3MaterializeBlock(const u8* dataBytes, size_t dataLen,
                             const GMeshV3Meta& meta,
                             const GMeshV3Block& blk, MeshData& out,
                             std::string& err);

// ---- 0.10-M HOTFIX TEXTURAS-VINCULA — os SLOTS de textura por material ----
// O bug do dono (C33): o import gerava os .gtext das imagens mas NADA os
// ligava ao mesh — a tabela de materiais do v3 guardava SÓ o nome
// (v3WriteMaterial) e o índice material→textura→imagem morria no parse
// (textureSources era local a parseGltf). A CURA: o bloco de VINCULAÇÃO
// (magic "GTB1") APPENDADO ao FIM da secção de materiais — o leitor de
// sempre PARA em materialCount nomes (o contrato «o fim pode trazer bytes
// a mais» já documentado), logo os ficheiros antigos LEEM-SE byte a byte
// e os leitores antigos lêm os novos SEM saber (ignoram a cauda).
//
// Os 5 slots do PBR core do glTF (a ordem é o CONTRATO — o inspector e o
// escritor usam os MESMOS índices):
enum : u8 {
    kBindBaseColor = 0, kBindNormal = 1, kBindMetalRough = 2,
    kBindEmissive = 3, kBindOcclusion = 4,
};
constexpr u8 kBindSlotCount = 5;

// o NOME do slot (os logs e o inspector dizem o MESMO vocabulário — o
// glTF core em português da casa; PURO, afervel)
inline const char* kBindSlotName(u8 slot) {
    switch (slot) {
        case kBindBaseColor: return "baseColor";
        case kBindNormal: return "normal";
        case kBindMetalRough: return "metalRough";
        case kBindEmissive: return "emissive";
        case kBindOcclusion: return "occlusion";
        default: return "?";
    }
}

// uma VINCULAÇÃO por material: o caminho REL do .gtext por slot ("" = o
// slot vazio — o glTF não referenciou textura aí). O materialIndex é o
// ÍNDICE NA TABELA DE MATERIAIS do próprio ficheiro (a ordem dos nomes).
struct GMeshMatBind {
    u32 materialIndex = 0;
    std::string tex[kBindSlotCount];
    bool empty() const {
        for (u8 s = 0; s < kBindSlotCount; ++s) {
            if (!tex[s].empty()) {
                return false;
            }
        }
        return true;
    }
};

// ESCRITOR: appenda o bloco ao FIM da secção de materiais (a mesma vector
// que recebe os nomes — o chamador é o gmeshV3Skeleton). Layout do bloco
// (little-endian, tudo contíguo no fim do ficheiro):
//   magic u32 "GTB1" (0x31504247... os bytes 'G','T','B','1')
//   u32 bindCount (só materiais com PELO MENOS um slot não-vazio)
//   bindCount × { u16 materialIndex · u8 slotCount · slotCount ×
//                 { u8 slot · u16 pathLen · path bytes } · pad a 4 }
//   u32 crc32 (de TODOS os bytes desde o fim do bindCount)
// Sem binds (vazio) NADA é appendado — o ficheiro fica byte a byte com o
// pré-hotfix.
void gmeshV3AppendBinds(std::vector<u8>& materialsSection, u64 cursorAbs,
                        const std::vector<GMeshMatBind>& binds);

// LEITOR: `tail` = os bytes do FIM da secção de materiais DEPOIS dos
// materialCount nomes (o resto da faixa). Sem magic → true com out vazio
// (ficheiro pré-hotfix ou mesh sem texturas). Magic presente mas bytes
// truncados/CRC mau → false com err LEGÍVEL (a quarentena do chamador
// decide — o mesmo contrato da tabela).
// 0.11-M (FASE 2): `consumed` (OPCIONAL, out) recebe o nº de bytes do
// bloco GTB1 (do início do tail até ao fim do CRC) — o leitor do GTB2
// arranca DAÍ (o GTB2 é APPENDADO DEPOIS do GTB1 na mesma secção; os
// chamadores antigos byte a byte com o out-param ausente).
bool gmeshV3ReadBinds(const u8* tail, size_t tailLen,
                      std::vector<GMeshMatBind>& out, std::string& err,
                      size_t* consumed = nullptr);

// ---- 0.11-M (FASE 2) — GTB2: as PROPRIEDADES por material ------------------
// O GTB1 levou os CAMINHOS das texturas; o GTB2 leva as PROPRIEDADES do
// material glTF (alphaMode/alphaCutoff/doubleSided, os QUATRO factors, o
// transform UV por slot do KHR_texture_transform e os samplers normalizados).
// MESMO padrão do GTB1: bloco APPENDADO à secção de materiais (DEPOIS do
// GTB1), o leitor de nomes para nos nomes, os leitores antigos ignoram a
// cauda — ficheiros antigos leem-se byte a byte, ficheiros novos em apps
// antigas perdem SÓ as props (defaults honestos), nunca o mesh.
//
// Layout do bloco (little-endian): magic u32 "GTB2" · u32 propCount (só
// materiais com PELO MENOS UMA propriedade não-default) · propCount ×
// ENTRADA FIXA de 164 B · u32 crc32 (das entradas). A entrada fixa:
//   u16 materialIndex · u8 alphaMode · u8 doubleSided · f32 alphaCutoff
//   f32 baseColor[4] · f32 metallic · f32 roughness · f32 emissive[3]
//   5 × { f32 offset[2] · f32 rotation · f32 scale[2] · u8 wrapS · u8 wrapT
//         · u8 minFilter · u8 magFilter }
// (44 B de props + 5×24 B de slots = 164, já 4-alinhado)
constexpr u32 kGmeshMatPropsEntryBytes = 164;

// os CÓDIGOS de sampler no GTB2 (normalizados — os enums RAW do glTF não
// cabem num u8): 0 = DEFAULT da casa (wrap REPEAT, min LINEAR_MIPMAP_LINEAR,
// mag LINEAR — o par de sempre); wrap: 1=CLAMP_TO_EDGE, 2=MIRRORED_REPEAT;
// filtro: 1=NEAREST, 2=LINEAR, 3=LINEAR_MIPMAP_LINEAR, 4=NEAREST_MIPMAP_
// LINEAR, 5=LINEAR_MIPMAP_NEAREST, 6=NEAREST_MIPMAP_NEAREST.
enum : u8 {
    kSamplerDefault = 0, kSamplerNearest = 1, kSamplerLinear = 2,
    kSamplerLinearMipmapLinear = 3, kSamplerNearestMipmapLinear = 4,
    kSamplerLinearMipmapNearest = 5, kSamplerNearestMipmapNearest = 6,
    kSamplerWrapClamp = 1, kSamplerWrapMirrored = 2,
};

// o MAPEAMENTO puro glTF → códigos da casa (out[4] = wrapS/wrapT/min/mag).
// GL-free e afervel: o enum desconhecido cai no DEFAULT (out=0) e devolve
// false (o chamador loga o warn honesto UMA vez).
bool gltfSamplerToCodes(i32 wrapS, i32 wrapT, i32 minFilter, i32 magFilter,
                        u8 out[4]);

struct GMeshMatProps {
    u32 materialIndex = 0;
    u8 alphaMode = 0;          // 0=OPAQUE, 1=MASK, 2=BLEND (os valores glTF)
    u8 doubleSided = 0;
    f32 alphaCutoff = 0.5f;
    f32 baseColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    f32 metallic = 1.0f;
    f32 roughness = 1.0f;
    f32 emissive[3] = {0.0f, 0.0f, 0.0f};
    // por slot (kBind*): o transform UV + o sampler (códigos da casa)
    f32 uvOffset[5][2] = {};
    f32 uvRotation[5] = {};
    f32 uvScale[5][2] = {{1,1},{1,1},{1,1},{1,1},{1,1}};
    u8 sampler[5][4] = {};     // {wrapS, wrapT, min, mag}; 0 = default casa

    // o material tem ALGUMA propriedade fora do default glTF? false =
    // NÃO entra no GTB2 (o ficheiro fica byte a byte com o pré-FASE 2)
    bool isDefault() const;
    // o SLOT tem transform/sampler fora do default? (o shader de hoje só
    // aplica o slot 0 — os outros esperam os samplers 1-4 no lit, 0.11-L)
    bool slotDefault(u8 slot) const;
};

// ESCRITOR: appenda o bloco GTB2 ao FIM da secção de materiais (DEPOIS do
// GTB1 — o chamador é o gmeshV3Skeleton; a ordem na cauda é nomes→GTB1→GTB2).
// Sem props não-default NADA é appendado (o ficheiro fica byte a byte).
void gmeshV3AppendProps(std::vector<u8>& materialsSection,
                        const std::vector<GMeshMatProps>& props);

// ---- 0.11-T (FASE 3.5a) — GLOD: o LOD por distância no .gmesh -------------
// O MESMO padrão de cauda do GTB1/GTB2: um bloco APPENDADO DEPOIS do GTB2
// (a ordem da cauda é nomes→GTB1→GTB2→GLOD), o leitor de nomes para nos
// nomes, os leitores antigos nunca chegam aqui (o payloadSize NÃO conta
// o GLOD — os bytes vivem além do fim do payload, onde os leitores v3
// de sempre não leem). O bloco GLOD é um ÍNDICE de entradas de bloco
// (o MESMO layout de 80 B da tabela) 1:1 POR ORDEM com a tabela normal
// (a entrada k é o LOD do bloco k), seguido dos DADOS dos blocos LOD
// (escritos PELO CHAMADOR depois do índice, nos offsets ABSOLUTOS que as
// entradas declaram). Layout:
//   magic u32 "GLOD" · u32 version=1 · u32 lodCount · lodCount × 80 B
//   · u32 crc32 (de magic..fim das entradas)
// Uma entrada com indexCount==0 = o bloco k NÃO tem LOD (pequenino — o
// draw usa o normal). O chamador valida lodCount == blockCount (o
// leitor recusa outro valor — a corrupção honesta).
//
// A DECIMAÇÃO (o «a ½ dos vértices» do pin): PARITY-WELD determinístico
// — os vértices ÍMPARES soldam ao vizinho PAR mais próximo (i-1 ou i+1)
// se a distância ≤ 25% da diagonal do AABB do bloco (fora disso o
// ímpar é MANTIDO — nunca um sliver alongado); triângulos degenerados
// após o remapeamento CAEM; os atributos do REPRESENTANTE (o par) vencem
// (a pele também — a emenda de pesos é a fronteira documentada). Pura,
// O(n), sem alocações além da saída. false = não há LOD para esta pool
// (< 8 verts, AABB degenerado, ou NENHUM soldável — o LOD igual ao
// original não é LOD).
bool gmeshDecimateHalf(const std::vector<Vertex>& pool,
                       const std::vector<u32>& indices32,
                       const std::vector<u8>* bones,
                       const std::vector<f32>* weights,
                       const Vec3& aabbMin, const Vec3& aabbMax,
                       std::vector<Vertex>& outPool,
                       std::vector<u32>& outIndices,
                       std::vector<u8>* outBones,
                       std::vector<f32>* outWeights,
                       Vec3& outMn, Vec3& outMx);

// ESCRITOR: serializa o ÍNDICE GLOD para `out` (o chamador appenda-o ao
// fim da secção de materiais, DEPOIS do GTB2). As entradas chegam COM
// dataOffset/dataSize/crc32 JÁ preenchidos (os offsets absolutos dos
// blobs que o chamador escreve DEPOIS do índice). `cursorAbs` = o offset
// ABSOLUTO onde o índice vai nascer (só para o log/contract — os CRCs
// não dependem dele). lodCount == 0 NÃO escreve nada.
void gmeshV3AppendLod(std::vector<u8>& out,
                      const std::vector<GMeshV3Block>& lodEntries);

// o tamanho do índice GLOD para lodCount entradas (o chamador precisa
// dele ANTES de escrever para calcular os offsets dos blobs):
// 16 (magic+ver+count+crc) + lodCount × 80.
constexpr u64 gmeshV3LodIndexBytes(u64 lodCount) {
    return 16ull + lodCount * kGmeshV3BlockEntryBytes;
}

// LEITOR: `tail` = os bytes DEPOIS do fim do GTB2 (o `consumed` do
// gmeshV3ReadProps). Sem magic → true com out vazio (ficheiro sem LOD).
// Magic presente: version!=1, lodCount != meta.blockCount, CRC mau ou
// entrada inválida → false com err LEGÍVEL (o mesmo contrato dos irmãos).
// Os dataOffset das entradas são ABSOLUTOS (apontam PARA A FRENTE, para
// os blobs que vivem depois do índice — validados ao materializar).
bool gmeshV3ReadLod(const u8* tail, size_t tailLen,
                    const GMeshV3Meta& meta,
                    std::vector<GMeshV3Block>& out, std::string& err);

// LEITOR: `tail` = os bytes DEPOIS do fim do GTB1 (o `consumed` do
// gmeshV3ReadBinds). Sem magic → true com out vazio (pré-FASE 2). Truncado
// /CRC mau → false com err LEGÍVEL (o mesmo contrato do GTB1).
// 0.11-T (FASE 3.5a): `consumed` (OPCIONAL, out) recebe o nº de bytes do
// bloco GTB2 — o leitor do GLOD arranca DAÍ (a ordem é nomes→GTB1→GTB2→GLOD).
bool gmeshV3ReadProps(const u8* tail, size_t tailLen,
                      std::vector<GMeshMatProps>& out, std::string& err,
                      size_t* consumed = nullptr);

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

// ---- o ESCRITOR v3 PARTILHADO (0.10-M PASSO 3) ------------------------------
// A "esqueleto" do ficheiro v3 — header (32 B, com o checksum dos
// metadados) + metadados (160 B, com o CRC da tabela) + tabela de blocos
// (80 B/entrada) + secção de materiais. O writeGMesh (em memória) e o
// conversor streaming (GmeshV3Stream, em disco) partilham EXATAMENTE esta
// serialização — ZERO drift de formato entre os dois escritores (a mesma
// ordem de campos, a mesma aritmética de offsets, os mesmos CRCs). Os
// DADOS dos blocos NÃO passam por aqui (o writeGMesh emite dos bins; o
// streaming emite do temporário em disco — nunca o modelo inteiro em RAM).
// ENTRADA: `meta` com flags/attrs/contagens/AABB/blockVertexCap; `entries`
// com dataSize/vertexCount/indexCount/aabbMin/aabbMax/materialIndex/
// indexType/crc32, NA ORDEM de emissão dos dados. SAÍDA: os offsets
// preenchidos (meta.blockTableOffset/materialTableOffset,
// entry.dataOffset) + os 4 vetores prontos a escrever NESTA ordem.
struct GMeshV3Skeleton {
    std::vector<u8> header;      // 32 B
    std::vector<u8> meta;        // 160 B
    std::vector<u8> table;       // entries.size() × 80 B
    std::vector<u8> materials;   // variável (entrada 4-alinhada)
};
// `binds` (0.10-M HOTFIX TEXTURAS-VINCULA): as vinculações material→slot
// →.gtext appendadas ao FIM da secção de materiais (o bloco GTB1). nullptr
// OU vazio = o ficheiro fica byte a byte com o pré-hotfix. O payloadSize
// do header CONTA os bytes do bloco (o leitor valida pelo tamanho).
// `props` (0.11-M FASE 2): as propriedades por material appendadas DEPOIS
// do GTB1 (o bloco GTB2 — o MESMO padrão de cauda). nullptr OU vazio/total-
// default = byte a byte com o pré-FASE 2.
bool gmeshV3Skeleton(GMeshV3Meta& meta,
                     std::vector<GMeshV3Block>& entries,
                     const std::vector<std::string>& materials,
                     GMeshV3Skeleton& sk, std::string& err,
                     const std::vector<GMeshMatBind>* binds = nullptr,
                     const std::vector<GMeshMatProps>* props = nullptr);

// ---- .gtext -----------------------------------------------------------------
// PASSO 5A: o ESCRITOR grava v2 (payload ganha um u8 `flags` — bits
// kTexFlagSrgb/kTexFlagNormal, TexturePolicy.h); o LEITOR abre v1 (flags=0,
// os ficheiros antigos continuam a carregar byte a byte) e v2.
constexpr u16 kGtextVersionWrite = 2;
constexpr u16 kGtextVersionMinRead = 1;
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
