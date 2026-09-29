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
//   scene/nodes        — hierarquia simples (name/mesh/pai/TRS); skinning e
//                        animação = F7 (nós com skin são ignorados)
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

// material básico (PBR mínimo; interp. p/ lit acontece no device)
struct GltfMaterial {
    std::string name;
    f32 baseColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct GltfModel {
    std::vector<MeshData> meshes;      // 1 MeshData por glTF mesh
    std::vector<GltfMaterial> materials;
    std::vector<GltfNode> nodes;

    bool ok() const { return !meshes.empty(); }
};

// resolve buffers EXTERNOS (.gltf referencia xxx.bin por URI relativo).
// false = buffer não encontrado → parse falha com erro claro.
struct GltfBufferResolver {
    bool (*fn)(void* user, const char* uri, std::vector<u8>& out) = nullptr;
    void* user = nullptr;
};

// parse de .gltf (JSON puro) ou do JSON chunk de um .glb.
// `bin`: conteúdo do BIN chunk do GLB (vazio p/ .gltf) — buffer sem URI.
// `resolver`: usado para buffers com URI não-"data:" (pode ser {nullptr,0}
// se o ficheiro só tem buffers embutidos).
bool parseGltf(const char* json, size_t len, const std::vector<u8>& bin,
               const GltfBufferResolver& resolver, GltfModel& out,
               std::string& err);

// container binário .glb (magic 'glTF', JSON chunk + BIN chunk) → parseGltf
bool parseGlb(const u8* data, size_t len, const GltfBufferResolver& resolver,
              GltfModel& out, std::string& err);

// base64 (data: URIs) — exposto para testes
bool decodeBase64(const char* src, size_t len, std::vector<u8>& out);

} // namespace vv
