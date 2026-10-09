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
struct GltfMaterial {
    std::string name;
    f32 baseColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    i32 baseColorTex = -1;    // índice em GltfModel::images (-1 = sem)
};

// ---- 0.8.1 (F7): ANIMAÇÕES ------------------------------------------------
// Um sampler = curva de um canal (times do accessor `input`, values do
// `output`); um channel liga (nó alvo, path) a um sampler. Interpolação:
// LINEAR (default) é a que o AnimationPlayer reproduz; STEP é tolerada
// como linear (dívida); CUBICSPLINE tem layout [in, valor, out] por key —
// extraímos o VALOR (meio) e seguimos linear (dívida documentada).
struct GltfAnimSampler {
    std::vector<f32> times;    // input: SCALAR f32 (segundos)
    std::vector<f32> values;   // output: VEC3 (translation/scale) ou VEC4 (rotation)
    u32 components = 3;        // 3 ou 4 floats por key
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
    // material de CADA mesh de saída (primeiro material usado pelas
    // primitivas; -1 = nenhum) — alinhado com meshes
    std::vector<i32> meshMaterial;
    // F5.1-B: caminho relativo da textura extraída por mesh ("textures/
    // gltf_<hash>.png"), PREENCHIDO pelo ResourceManager no load (dedup
    // por hash); vazio = sem textura. Alinhado com meshes.
    std::vector<std::string> meshTexture;

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
