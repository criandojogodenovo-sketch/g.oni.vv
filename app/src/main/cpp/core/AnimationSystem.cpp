// core/AnimationSystem.cpp — avanço dos AnimationPlayers no passo fixo.
#include "core/AnimationSystem.h"
#include "components/AnimationPlayer.h"
#include "core/Scene.h"
#include "core/ComponentStore.h"

namespace vv {

void AnimationSystem::tick(Scene& scene, f32 dt) {
    if (!enabled) {
        return;
    }
    auto& players = scene.components().animators();
    for (u32 i = 0; i < players.size(); ++i) {
        AnimationPlayer& pl = players.at(i);
        if (!pl.playing) {
            continue;
        }
        pl.advance(dt);
        pl.apply(scene, players.owner(i));
    }
}

} // namespace vv
