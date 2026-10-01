#pragma once
// core/AnimationSystem.h — system do grupo Update que avança os
// AnimationPlayers (0.8.0, F7).
//
// Como a física: gate `enabled` — a animação só AVANÇA em modo Play (a
// sandbox do PlaySnapshot devolve a pose de editor ao parar). No editor o
// preview vive na timeline (ui/Timeline), que avança o player com dt real
// e captura/restaura a pose por conta própria.
//
// Ordem no grupo Update: ANTES do TransformSystem — o player escreve
// pos/rot/scale (e já chama updateWorld() para feedback imediato); o
// TransformSystem reconfirma o cache no mesmo passo (ordem conservadora:
// nunca vê dados meio-escritos).
#include "core/Tick.h"

namespace vv {

class AnimationSystem : public System {
public:
    bool enabled = false;   // só em Play (como o PhysicsSystem)

    void tick(Scene& scene, f32 dt) override;
};

} // namespace vv
