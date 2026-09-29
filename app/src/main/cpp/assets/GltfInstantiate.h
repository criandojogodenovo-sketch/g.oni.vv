#pragma once
// assets/GltfInstantiate.h — GltfModel → TICs com hierarquia (F5-C).
//
// Um TIC por NÓ do glTF (vazios incluídos — a hierarquia entra fiel; o
// dono pode apagar na mão o que não quiser). Nós com mesh ganham
// MeshRenderer com:
//   meshPath = "<basePath>#<meshOriginal>"   (ex.: "meshes/robot.gltf#1")
//   mesh     = ctx.bindMesh(user, meshPath)  — device liga o Mesh* do
//              ResourceManager/GpuAssets; testes usam sentinelas. nullptr
//              NÃO aborta: o TIC entra sem mesh (rebind depois no reload).
//   material = ctx.material quando o mesh ligou (lit partilhado do engine).
// TRS do nó → Transform3D (translation/rotation/scale) + updateWorld.
//
// GL-free: testável no CI Linux (bindMesh com sentinelas).
#include <string>
#include <vector>
#include "assets/GltfImporter.h"
#include "core/Handle.h"
#include "render/Material.h"   // Material = alias de LitMaterial (não admite fwd-decl)

namespace vv {

class Scene;
class Mesh;

struct GltfInstantiateCtx {
    Mesh* (*bindMesh)(void* user, const std::string& ref) = nullptr;
    void* user = nullptr;
    Material* material = nullptr;   // lit partilhado dos meshes importados
};

// Cria os TICs; devolve o handle do 1º nó (Handle::invalid() se o modelo
// não tem nós). Referências cruzadas (filho antes do pai no array) são
// tratadas — a criação é em 2 passadas.
Handle gltfInstantiate(Scene& scene, const GltfModel& model,
                       const std::string& basePath,
                       const GltfInstantiateCtx& ctx);

} // namespace vv
