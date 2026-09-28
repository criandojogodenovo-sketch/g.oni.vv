#pragma once
// components/MeshRenderer.h — desenha um mesh com material (F3).
//
// Ponteiros NÃO-DONOS: Mesh e Material são recursos de runtime partilhados
// (o cubo procedural da F2 é o mesh de todos os presets — sem assets na F3).
// O serializer guarda apenas a tag "cube" e o loader rebinda os ponteiros.
#include "core/Component.h"
#include "render/Material.h"

namespace vv {

class Mesh;   // render/Mesh.h — recurso GL (fwd: manter header GL-free)

class MeshRenderer : public Component {
public:
    Mesh*      mesh     = nullptr;
    Material*  material = nullptr;
};

} // namespace vv
