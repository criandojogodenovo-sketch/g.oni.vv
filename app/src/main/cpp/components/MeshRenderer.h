#pragma once
// components/MeshRenderer.h — desenha um mesh com material (F3).
//
// Ponteiros NÃO-DONOS: Mesh e Material são recursos de runtime partilhados
// (o cubo procedural da F2 é o mesh dos presets; meshes importados chegam
// pela F5 via ResourceManager/GpuAssets).
// O serializer guarda a tag "cube" OU a ref relativa "meshes/x.obj[#i]" —
// o loader rebinda os ponteiros via LoadCtx (F5-E).
#include <string>
#include "core/Component.h"
#include "render/Material.h"

namespace vv {

class Mesh;    // render/Mesh.h — recurso GL (fwd: manter header GL-free)

class MeshRenderer : public Component {
public:
    Mesh*      mesh     = nullptr;
    Material*  material = nullptr;
    // F5-C: ref relativa do asset ("meshes/x.obj", "meshes/m.gltf#1") —
    // vazia = mesh procedural ("cube"). Vive no componente para o
    // serializer e para a UI mostrarem o asset de origem.
    std::string meshPath;
};

} // namespace vv
