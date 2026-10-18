#pragma once
// core/AssetPersist.h — Salvar materializa os assets da cena nas SUBPASTAS
// certas do projeto (F5.4-hotfix, pedido do dono: "Salvar tem de gravar os
// assets fisicamente, organizados por tipo — não só project.goni/cena").
//
// AUDITORIA de destino por tipo (tudo o que já existia continua certo):
//   project.goni            → raiz (manifesto — é o sítio dele)
//   scenes/*.goni           → Project::saveActiveScene
//   meshes/*.obj/gltf/glb   → import (fileapi) e Export OBJ do menu
//   textures/*.png          → import (fileapi)
//   textures/gltf_<hash>.png→ extractGltfTextures (load do glTF/GLB)
//   textures/cache/…        → TextureCache (derivados comprimidos)
//   CUBO PROCEDURAL         → SÓ existia em runtime — é o que falta:
//                             meshes/cube.obj (formato OBJ JÁ definido no
//                             projeto, o mesmo do Export OBJ do menu)
//
// REGRA: mesh procedural = o MESMO predicado do serializer (meshPath vazio
// + mesh presente → tag "cube"). O ficheiro materializa UMA vez (idempotente
// — já existente não se sobrescreve: o asset pode ter sido editado no
// disco; nada de churn por Salvar). Refs da cena NÃO são reescritos — o
// "cube" continua procedural em runtime; o ficheiro é a CÓPIA FÍSICA
// garantida na pasta certa (base p/ import/export/partilha futuros).
//
// GL-free: geometria do cubo vem de render/Cube (pura), exportObj é puro —
// testável na suíte contra o FakeSafIo (ver test_assetpersist.cpp).
#include <string>
#include <vector>
#include "core/ProjectStorage.h"

namespace vv {

class Scene;

// Materializa os assets em-runtime referenciados pela cena.
//   outWritten → rel-paths gravados NESTA chamada (vazio = nada a fazer)
//   err        → causa real em falha (false)
// Falha só quando UMA escrita necessária falha (honesto — o Salvar decide).
bool persistSceneAssets(ProjectStorage& st, const Scene& scene,
                        std::vector<std::string>& outWritten, std::string& err);

// rel-path canónico do cubo materializado (constante única — testes e
// editor partilham)
inline const char* kCubeAssetRel = "meshes/cube.obj";

} // namespace vv
