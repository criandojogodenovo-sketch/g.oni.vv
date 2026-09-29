#pragma once
// core/SceneSerializer.h — save/load de cena no formato .goni (JSON) (F3).
//
// Formato (versão atual kVersion = 1):
//   {
//     "version": 1,
//     "tics": [
//       {"id":0, "name":"StaticBody3D", "active":true, "parent":-1,
//        "components":[
//          {"type":"Transform3D","pos":[0,0.5,0],"rot":[0,0,0,1],"scale":[1,1,1]},
//          {"type":"MeshRenderer","mesh":"cube"},
//          {"type":"InputMap"}
//        ]}
//     ]
//   }
//
// "id" é a POSIÇÃO no array (não o slot interno) — estável entre saves.
// "parent" guarda a posição do pai no array (-1 = raiz).
// MeshRenderer só serializa a tag do mesh ("cube" = cubo procedural da F2);
// o loader rebinda os ponteiros de runtime (mesh/material) via LoadCtx.
//
// Migrações: load() aplica migrate() antes de reconstruir — ficheiros v0
// (sem "version", tics sem "active") abrem na v1. Tipos de componente
// desconhecidos são IGNORADOS no load (política forward-compat: uma cena
// salva por versão futura abre aqui sem os componentes que não existem).
//
// F5: dump()/loadText() expõem o formato como TEXTO — o Project transporta
// a cena pela interface ProjectStorage (sem passar por caminhos de FS),
// pronto para SAF na F5.2; save()/load(path) continuam como wrappers.
#include "core/Json.h"
#include "core/Types.h"
#include "render/Material.h"   // Material = alias de LitMaterial (não admite fwd-decl)
#include <functional>

namespace vv {

class Scene;
class Mesh;
class Texture;

namespace SceneSerializer {

constexpr u32 kVersion = 1;

// Recursos de runtime ligados aos MeshRenderers no load.
// F5-E: além da tag "cube", MeshRenderers com asset importado rebindam via
// RESOLVERS — o device liga ref → Mesh*/Texture* do ResourceManager/GpuAssets;
// testes usam sentinelas. Resolver ausente/que falha → ponteiro null (a
// hierarquia entra na mesma; rebind é possível recarregar).
struct LoadCtx {
    Mesh*      cubeMesh = nullptr;   // tag "cube" → este mesh
    Material*  material = nullptr;   // material lit partilhado
    std::function<Mesh*(const std::string&)> resolveMesh;       // ref relativa
    std::function<const Texture*(const std::string&)> resolveTex; // textura
};

// F5: texto ↔ cena (storage-agnostic; Project usa estes).
std::string dump(const Scene& scene);
bool loadText(Scene& scene, const std::string& text, const LoadCtx& ctx);

// Ficheiro direto (wrappers de dump/loadText — API da F3 mantida).
bool save(const Scene& scene, const char* path);
bool load(Scene& scene, const char* path, const LoadCtx& ctx);

// Migra o documento para a versão atual (aplicado no load; exposto p/ testes).
Json migrate(Json doc);

} // namespace SceneSerializer

} // namespace vv
