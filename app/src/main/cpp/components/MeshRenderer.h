#pragma once
// components/MeshRenderer.h — desenha um mesh com material (F3).
//
// Ponteiros NÃO-DONOS: Mesh/Material/Texture são recursos de runtime
// partilhados (o cubo procedural da F2 é o mesh dos presets; assets
// importados chegam pela F5 via ResourceManager/GpuAssets — 1 ref = 1
// objeto GL, cache não duplica memória de GPU).
// O serializer guarda a tag "cube" OU as refs relativas "meshes/x.obj[#i]"
// e "textures/y.png" — o loader rebinda os ponteiros via LoadCtx (F5-E).
#include "core/Component.h"
#include "render/Material.h"
#include <string>

namespace vv {

class Mesh;      // render/Mesh.h — recurso GL (fwd: manter header GL-free)
class Texture;   // render/Texture.h — idem (F5-D)

class MeshRenderer : public Component {
public:
    Mesh*      mesh     = nullptr;
    Material*  material = nullptr;
    // F5-D: textura opcional do material (não-dono; ligada pelo GpuAssets)
    const Texture* texture = nullptr;

    // 0.7.0 — COR POR TIC (gestão de TICs): tint multiplicativo do albedo no
    // shader lit (uniform uTint; default branco = comportamento 0.6.x byte
    // a byte). Sliders R/G/B no Inspector; serializado no .goni
    // ("tint":[r,g,b], default [1,1,1] quando ausente).
    f32 tint[3] = {1.0f, 1.0f, 1.0f};

    // F5-C/E: refs relativas do asset — vazias = procedural ("cube"/sem tex).
    // Vivem no componente para o serializer e para a UI mostrarem a origem.
    std::string meshPath;
    std::string texPath;
};

} // namespace vv
