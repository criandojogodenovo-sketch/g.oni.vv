#pragma once
// assets/GltfImporter.h — parser glTF 2.0 → GltfModel (F5-C).
//
// ESCOPO (básico, sem extensões):
//   buffers            — GLB BIN chunk, data: URI base64 (embutido) ou
//                        .bin EXTERNO via GltfBufferResolver (F5-C/2)
//   bufferViews        — byteOffset/byteLength/byteStride (respeitado em
//                        atributos de vértice; índices são sempre compactos)
//   accessors          — POSITION (VEC3 f32), NORMAL (VEC3 f32),
//                        TEXCOORD_0 (VEC2 f32), índices (SCALAR u8/u16/u32)
//   meshes/primitives  — só mode 4 (TRIANGLES); primitivas do mesmo mesh
//                        são FUNDIDAS num MeshData (1 grupo por primitiva)
//   materials          — básico: name + pbrMetallicRoughness.baseColorFactor
//                        (o device mapeia para o Material lit partilhado —
//                        uniforms por material são fase de luzes)
//   scene/nodes        — hierarquia simples (name/mesh/pai/TRS)
//   animations         — 0.8.1 (F7): channels/samplers → GltfAnimation
//                        (skin/weights ficam para a 0.8.2)
//
// CONVENÇÕES: glTF é Y-up / right-handed / metros — o engine é igual, não
// há transformação. Sem dedup de vértices (glTF já é indexado). Limite u16
// por mesh fundido (erro explícito), igual ao OBJ.
//
// GL-free: testável no CI Linux (fixtures construídas nos testes).
#include <string>
#include <vector>
#include "assets/Assets.h"
#include "math/Math.h"

namespace vv {

// nó da hierarquia glTF (TRS no próprio nó; pais por índice no array)
struct GltfNode {
    std::string name;
    i32 mesh = -1;          // índice em GltfModel::meshes (-1 = nó vazio)
    i32 parent = -1;        // índice em GltfModel::nodes (-1 = raiz)
    Vec3 translation{0.0f, 0.0f, 0.0f};
    Quat rotation{0.0f, 0.0f, 0.0f, 1.0f};
    Vec3 scale{1.0f, 1.0f, 1.0f};
};

// imagem embutida do glTF (F5.1-B): bytes decodificados + mime. Textura
// EXTERNA (uri não-data) fica com bytes vazios e o uri em `uriPath` — o
// índice continua alinhado com o array `images` do ficheiro.
// 0.9.6.4 (GRUPO A/R-022): `broken` marca a imagem cujo bufferView FALHOU
// no parse (limites/leitura) — o parse NÃO morre por causa dela (a
// geometria segue; a textura entra como falha no passe de texturas com
// warn + toast — nunca silencioso, nunca fatal).
struct GltfImage {
    std::vector<u8> bytes;    // vazio = externa/não suportada
    std::string mime;         // "image/png" (o engine só consome PNG)
    std::string uriPath;      // uri externa (pass-through p/ refs relativas)
    bool broken = false;      // bufferView falhou no parse (warn, não fatal)
};

// material básico (PBR mínimo; interp. p/ lit acontece no device)
// 0.11-M (FASE 2): as PROPRIEDADES COMPLETAS do material glTF — o parse
// lê alphaMode/alphaCutoff/doubleSided, os QUATRO factors
// (baseColorFactor de sempre + metallicFactor/roughnessFactor/
// emissiveFactor) e por CADA slot de textura o KHR_texture_transform
// (offset/rotation/scale) + o índice do sampler do glTF.
struct GltfMaterial {
    std::string name;
    f32 baseColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    i32 baseColorTex = -1;    // índice em GltfModel::images (-1 = sem)
    // PASSO 5A: o índice da IMAGEM do normalTexture (-1 = sem) — o conversor
    // encaminha estas imagens para o caminho RGBA8 SEM PERDA DE CANAIS
    // (comprimidos lossy destruem normais: os canais cruzam-se nos blocos)
    i32 normalTex = -1;
    // 0.10-M HOTFIX-2 (TEXTURAS-VINCULA-2): os OUTROS TRÊS slots do glTF —
    // o bug do device («só o baseColor aparece preenchido»): o parse só lia
    // baseColor+normal, o mapa material→slots ficava com 4 slots vazios e o
    // GTB1 nunca os trazia. metallicRoughness vive DENTRO de
    // pbrMetallicRoughness; emissive e occlusion ao nível do material.
    i32 metalRoughTex = -1;   // pbrMetallicRoughness.metallicRoughnessTexture
    i32 emissiveTex = -1;     // emissiveTexture
    i32 occlusionTex = -1;    // occlusionTexture

    // ---- 0.11-M (FASE 2): as propriedades por MATERIAL ------------------
    // alphaMode do glTF (0=OPAQUE default, 1=MASK, 2=BLEND) — os valores
    // são os PRÓPRIOS do glTF (enum inteiro: 0/1/2).
    u8 alphaMode = 0;
    f32 alphaCutoff = 0.5f;   // glTF default 0.5 (só com MASK)
    bool doubleSided = false; // glTF default false (culling de trás)
    f32 metallic = 1.0f;      // metallicFactor (glTF default 1)
    f32 roughness = 1.0f;     // roughnessFactor (glTF default 1)
    f32 emissive[3] = {0.0f, 0.0f, 0.0f};   // emissiveFactor (glTF default 0)

    // ---- 0.11-M (FASE 2): por SLOT (a ordem é o contrato kBind* do
    // GOwnFormats.h — o mesmo índice dos campos de cima): o
    // KHR_texture_transform + o sampler do glTF. Os 5 arrays ficam
    // PARALELOS aos campos i32 de cima (byte a byte para o código antigo —
    // zero ripple; o conversor é quem os lê). Transform default = identidade.
    f32 texOffset[5][2] = {};       // offset.uv (glTF default [0,0])
    f32 texRotation[5] = {};        // radianos (glTF default 0)
    f32 texScale[5][2] = {{1.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 1.0f},
                          {1.0f, 1.0f}, {1.0f, 1.0f}};   // glTF default [1,1]
    i16 texSampler[5] = {-1,-1,-1,-1,-1};   // índice em GltfModel::samplers
};

// 0.11-M (FASE 2): o sampler do glTF — os valores RAW do ficheiro (os
// enums oficiais): wrap 10497=REPEAT (default), 33071=CLAMP_TO_EDGE,
// 33648=MIRRORED_REPEAT; filtro 9728=NEAREST, 9729=LINEAR,
// 9984..9987=NEAREST/LINEAR com mipmaps. O mapeamento para GL vive
// NO DEVICE (render/Texture.cpp) — aqui é só o transporte honesto.
struct GltfSampler {
    i32 wrapS = 10497;
    i32 wrapT = 10497;
    i32 magFilter = -1;    // glTF: SEM default obrigatório (o device usa o
    i32 minFilter = -1;    //   seu par de sempre quando ausente)
};

// ---- 0.8.1 (F7): ANIMAÇÕES ------------------------------------------------
// Um sampler = curva de um canal (times do accessor `input`, values do
// `output`); um channel liga (nó alvo, path) a um sampler. Interpolação:
// 0.11-S (FASE 3) transporta as TRÊS do glTF — `interp` diz qual e o
// GltfAnim (gltfAttachClips) é quem mapeia para as curvas da casa:
//   kInterpLinear (0): values é [key × comps] (o layout de sempre)
//   kInterpStep   (1): idem linear no transporte (a curva é que segura)
//   kInterpCubic  (2): values é [key × 3 × comps] — [in, VALOR, out] por
//       key (as tangentes hermite VÃO no bloco; antes o parse cortava
//       tudo para o meio e seguia linear — a dívida da 0.8.1 morre aqui)
struct GltfAnimSampler {
    static constexpr u8 kInterpLinear = 0;
    static constexpr u8 kInterpStep   = 1;
    static constexpr u8 kInterpCubic  = 2;
    std::vector<f32> times;    // input: SCALAR f32 (segundos)
    std::vector<f32> values;   // output (layout por `interp` — ver acima)
    u32 components = 3;        // 3 ou 4 floats por key
    u8 interp = kInterpLinear; // 0.11-S: a interpolação glTF do sampler
};

struct GltfAnimChannel {
    enum class Path : u8 { Translation = 0, Rotation = 1, Scale = 2 };
    i32  node = -1;            // nó alvo (índice em GltfModel::nodes)
    Path path = Path::Translation;
    i32  sampler = -1;         // índice em GltfAnimation::samplers
};

struct GltfAnimation {
    std::string name;          // "anim N" se o glTF não tiver nome
    std::vector<GltfAnimChannel> channels;
    std::vector<GltfAnimSampler> samplers;
};

// ---- 0.8.2 (F7): SKINS -------------------------------------------------------
// Um glTF "skin" = lista de joints (NÓS com transform hierárquico) + UMA
// inverseBindMatrix por joint. Os vértices carregam JOINTS_0/WEIGHTS_0
// (4 influências) que indexam ESTA lista. Reordenamos os joints PAES
// PRIMEIRO (a composição hierárquica fica iterativa); `nodeToJoint` mapeia
// nó glTF → índice (para os channels de animação).
struct GltfJoint {
    std::string name;          // do nó ("joint N" se anónimo)
    i32  parent = -1;          // pai DENTRO da lista (-1 = raiz da skin)
    Vec3 pos{};
    Quat rot = Quat::identity();
    Vec3 scale{1.0f, 1.0f, 1.0f};
    Mat4 inverseBind = Mat4::identity();
};

struct GltfSkin {
    std::string name;
    std::vector<GltfJoint> joints;       // pais antes de filhos
    std::vector<i32> nodeToJoint;        // alinhado com GltfModel::nodes
};

// 0.10-M (PASSO 3): a REFERÊNCIA de uma primitiva SEM materializar a
// geometria — o conversor streaming (GmeshV3Stream) lê os accessors POR
// BLOCO via mmap do ficheiro copiado; o parse nunca segura o modelo.
struct GltfPrimRef {
    i32 posAcc = -1, nrmAcc = -1, uvAcc = -1;
    i32 idxAcc = -1;        // -1 = primitiva não indexada (0..count-1)
    u32 posCount = 0;       // o count do accessor POSITION
    i32 material = -1;      // índice em GltfModel::materials (-1 = nenhum)
    i32 node = -1;          // o nó dono (a transformação mundo que baked)
    i32 mesh = -1;          // o mesh glTF origem (diag)
};

struct GltfModel {
    std::vector<MeshData> meshes;      // 1 MeshData por glTF mesh
    // 0.10-M (PASSO 3): preenchido SÓ no modo streaming (parseGltf com
    // streamMeshes=true) — as primitivas TRIANGLES na ordem dos nós
    std::vector<GltfPrimRef> primRefs;
    std::vector<GltfMaterial> materials;
    std::vector<GltfNode> nodes;
    std::vector<GltfImage> images;     // F5.1-B: texturas embutidas/externas
    std::vector<GltfAnimation> animations;   // 0.8.1 (F7): channels/samplers
    std::vector<GltfSkin> skins;             // 0.8.2 (F7): joints + ibm
    std::vector<GltfSampler> samplers;       // 0.11-M (FASE 2): wrap/filter
    // material de CADA mesh de saída (primeiro material usado pelas
    // primitivas; -1 = nenhum) — alinhado com meshes
    std::vector<i32> meshMaterial;
    // F5.1-B: caminho relativo da textura extraída por mesh ("textures/
    // gltf_<hash>.png"), PREENCHIDO pelo ResourceManager no load (dedup
    // por hash); vazio = sem textura. Alinhado com meshes.
    std::vector<std::string> meshTexture;

    // 0.12-Y (3.Y-C): os 5 SLOTS por mesh (a ordem kBindBaseColor..
    // kBindOcclusion; meshes × 5 achatado; "" = slot vazio) — os
    // ficheiros em assets/<stem>/textures/ (a ordem do dono). Preenchido
    // pelo ResourceManager ao lado do meshTexture (que fica para os
    // consumidores antigos byte a byte).
    std::vector<std::string> meshTexSlots;

    // 0.9.6.12 (A2/R-014 · a spec 2d): primitivas LARGADAS por bufferView
    // que genuinamente excede o buffer (exporter malformado) — DEGRADA em
    // vez de matar o import: cada queda LOGA W com a linha completa
    // «glb: view<i> buffer<b> off=… len=… declared=… real=…» e o resto do
    // modelo entra. A CAUSA da última queda fica aqui (o erro final quando
    // TODAS caem nomeia a comparação completa — nunca truncado)
    u32 primsDropped = 0;
    std::string primDropCause;

    bool ok() const { return !meshes.empty(); }
};

// resolve buffers EXTERNOS (.gltf referencia xxx.bin por URI relativo).
// false = buffer não encontrado → parse falha com erro claro.
struct GltfBufferResolver {
    bool (*fn)(void* user, const char* uri, std::vector<u8>& out) = nullptr;
    void* user = nullptr;
};

// 0.9.6.12g (A2-2 · FAZ 5 do dono): o TETO ÚNICO por range materializado —
// PARTILHADO pelo parser (GltfBufferStore::span) e pelo range loader de
// ficheiro (fileRangeLoad). O teto ANTIGO de 64 MB recusava views VÁLIDAS
// (o scene do dono: UM view de 123738528 B = 118 MB num BIN de 212 MB —
// glTF perfeitamente legal) e o fileRangeLoad tinha um SEGUNDO teto
// escondido de 16 MB (kMaxJsonBytes reusado) que matava o dragão com uma
// mentira de I/O. 256 MB = o mesmo kStreamAccumMax da casa (ProjectStorage).
// Acima disto a falha é HONESTA: «modelo demasiado grande para a memória».
constexpr u64 kMaxRangeBytes = 256ull * 1024 * 1024;

// 0.8.10 — STREAMING: carrega RANGES de um buffer por demanda (o BIN chunk
// de um .glb EM FICHEIRO: o import nunca o carrega inteiro — accessors e
// imagens materializam SÓ os seus ranges, cada um ≤ kMaxRangeBytes).
// `binLen`: tamanho total do buffer 0 (validação de bounds).
// 0.9.6.12g (A2-2): `fileBytes` = o tamanho do FICHEIRO em disco (vai ao
// log de diagnóstico do dono — «file=», FAZ 1) e `deferUri` = o URI do
// buffer EXTERNO de um .gltf que passa a ser DEFERIDO também (o scene.bin
// de 212 MB lido POR RANGES — nunca mais inteiro em RAM; vazio em GLB).
struct GltfRangeLoader {
    bool (*fn)(void* user, u32 bufferIndex, u64 offset, u64 len,
               std::vector<u8>& out) = nullptr;
    void* user = nullptr;
    u64  binLen = 0;        // tamanho do buffer 0 (GLB: o BIN chunk em ficheiro)
    u64  fileBytes = 0;     // o ficheiro EM DISCO (diag file= do dono, FAZ 1)
    std::string deferUri;   // .gltf: o URI do buffer externo DEFERIDO (A2-2)
};

// parse de .gltf (JSON puro) ou do JSON chunk de um .glb.
// `bin`: conteúdo do BIN chunk do GLB (vazio p/ .gltf) — buffer sem URI.
// `resolver`: usado para buffers com URI não-"data:" (pode ser {nullptr,0}
// se o ficheiro só tem buffers embutidos).
// `rangeLoader` (0.8.10, opcional): com `bin` VAZIO, o buffer 0 é DEFERIDO
// — ranges carregados por demanda (import de ficheiros enormes).
// `streamOwnedBin` (0.10-M SAF-STREAM, opcional): no modo streaming, o
// buffer 0 EMBUTIDO (data: URI) sai INTEIRO daqui — o conversor streaming
// corre sobre os bytes (a geometria data-uri também fica SEM teto).
bool parseGltf(const char* json, size_t len, const std::vector<u8>& bin,
               const GltfBufferResolver& resolver, GltfModel& out,
               std::string& err, const GltfRangeLoader* rangeLoader = nullptr,
               bool streamMeshes = false,
               std::vector<u8>* streamOwnedBin = nullptr);

// container binário .glb (magic 'glTF', JSON chunk + BIN chunk) → parseGltf
bool parseGlb(const u8* data, size_t len, const GltfBufferResolver& resolver,
              GltfModel& out, std::string& err);

// base64 (data: URIs) — exposto para testes
bool decodeBase64(const char* src, size_t len, std::vector<u8>& out);

} // namespace vv
