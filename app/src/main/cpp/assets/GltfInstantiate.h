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
// 0.10-M (EXT) — gltfExpandInstantiate: o mesmo contrato para o IMPORT
// EXPANDIDO (BACKLOG 0.10-M-ext feito): os registros NodeOut que o
// conversor das PEÇAS (convertGltfToV3Nodes) deixou no Output viram TICs
// — um por NÓ COM MESH, nomes do glTF preservados, a transformação do nó
// no Transform3D e o MeshRenderer com a ref DA PEÇA. FLAT (sem pais):
// o NodeOut JÁ traz a transformação MUNDO decomposta em TRS — o engine
// compõe flat (TransformSystem não resolve pais) e a peça desenha no
// sítio EXATO sem hierarquia; a geometria da peça ficou CRUA no espaço
// local do nó (o bake é a identidade).
//
// GL-free: testável no CI Linux (bindMesh com sentinelas).
#include <string>
#include <vector>
#include "assets/AssetConverter.h"   // 0.10-M (EXT): convert::Output::NodeOut
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

// 0.10-M (EXT) — cria os TICs do import EXPANDIDO (um por nó com mesh).
// `nodes` = Output::expandNodes do conversor (a ordem dos nós do glTF);
// `refs` = Output::meshes (nodes[i].mesh indexa ESTE vetor). O bindMesh
// injetado recebe a REF DA PEÇA — no device é o GpuAssets::blockHull (a
// peça abre POR BLOCOS pelo kGmeshV3FlagPiece: culling por TIC + lazy +
// LRU, o MESMO caminho do modelo grande do PASSO 4). Devolve os HANDLES
// criados NA ORDEM dos registros (vazio se `nodes` vazio ou sem peça
// válida — o pino do dono conta ISTO: o city expandido = 72). O TIC SEM
// bind (nullptr) entra na mesma com o meshPath — o reload re-liga.
std::vector<Handle> gltfExpandInstantiate(
        Scene& scene, const std::vector<convert::Output::NodeOut>& nodes,
        const std::vector<std::string>& refs,
        const GltfInstantiateCtx& ctx);

} // namespace vv
