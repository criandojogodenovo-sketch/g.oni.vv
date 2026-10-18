#pragma once
// assets/GltfTextures.h — extração de texturas embutidas do glTF (F5.1-B).
//
// O parser (GltfImporter) decodifica as images (data: URI base64 ou
// bufferView do GLB). ESTE módulo as transforma em ficheiros do projeto:
//
//   bytes do PNG → hash FNV-1a → "textures/gltf_<hash16>.png"
//   (mesmos bytes → MESMO ficheiro — dedup natural entre materiais, meshes
//   e até entre modelos; o ficheiro só é escrito se ainda não existir)
//
// Resultado: 1 caminho por mesh (alinhado com GltfModel::meshes), pronto
// para virar ref "textures/…" no MeshRenderer. Textura EXTERNA (uri não
// data:) passa o uri relativo direto — é o comportamento da F5. MIME
// não-PNG (ex.: jpeg) → caminho vazio (sem falha: o mesh importa, a
// textura fica ausente; KHR extras fora do escopo).
//
// GL-free: testável no CI com FakeStorage.
#include <string>
#include <vector>
#include "assets/GltfImporter.h"
#include "core/ProjectStorage.h"

namespace vv {

// `outMeshTex` fica com model.meshes.size() entradas ("" = sem textura).
bool extractGltfTextures(ProjectStorage& st, const GltfModel& model,
                         std::vector<std::string>& outMeshTex,
                         std::string& err);

// nome canónico do ficheiro extraído (hash dos BYTES do PNG)
std::string gltfTextureRelPath(const std::vector<u8>& pngBytes);

} // namespace vv
