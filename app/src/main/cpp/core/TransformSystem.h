#pragma once
// core/TransformSystem.h — system do grupo Update que mantém o cache de
// matrizes dos Transform3D (F3). O pass 3D desenha cada MeshRenderer com
// owner->Transform3D.world — o system garante que o cache reflete pos/rot/
// scale atuais a cada passo fixo.
#include "core/Tick.h"

namespace vv {

class TransformSystem : public System {
public:
    void tick(Scene& scene, f32 dt) override;
};

} // namespace vv
