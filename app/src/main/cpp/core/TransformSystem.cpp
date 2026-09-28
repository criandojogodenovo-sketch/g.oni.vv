#include "core/TransformSystem.h"
#include "components/Transform3D.h"
#include "core/Scene.h"

namespace vv {

void TransformSystem::tick(Scene& scene, f32 /*dt*/) {
    // F3: sem hierarquia de pais ainda (Tic.parent existe mas a resolução
    // pai→filho com world composto é trabalho da fase de editor polish);
    // cada transform é independente: world = TRS(pos, rot, scale).
    auto& transforms = scene.components().transforms();
    for (u32 i = 0; i < transforms.size(); ++i) {
        transforms.at(i).updateWorld();
    }
}

} // namespace vv
