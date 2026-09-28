#pragma once
// core/Tick.h — TickGroups + base System (F3).
//
// Ordem de execução por passo fixo do loop:
//   PreUpdate → Update → PostUpdate → Render   (a UI da F1 é desenhada DEPOIS,
//   por cima — não é um TickGroup).
//
// F3 registra apenas o TransformSystem no grupo Update; F4 (física) registra
// o passo de corpos em PreUpdate/Update/PostUpdate. Os ponteiros não são donos
// — os systems vivem no main (ou no dono do runner) com tempo de vida maior.
#include "core/Types.h"

namespace vv {

class Scene;

enum class TickGroup : u32 {
    PreUpdate  = 0,
    Update     = 1,
    PostUpdate = 2,
    Render     = 3,
    Count      = 4,
};

class System {
public:
    virtual ~System() = default;
    virtual void tick(Scene& scene, f32 dt) = 0;
};

class TickGroups {
public:
    static constexpr u32 kMaxSystemsPerGroup = 8;

    bool add(TickGroup group, System* system) {
        if (!system) {
            return false;
        }
        const u32 g = static_cast<u32>(group);
        if (g >= static_cast<u32>(TickGroup::Count) || counts_[g] >= kMaxSystemsPerGroup) {
            return false;
        }
        systems_[g][counts_[g]++] = system;
        return true;
    }

    // Roda todos os grupos em ordem — chamado UMA VEZ por passo fixo.
    void run(Scene& scene, f32 dt) const {
        for (u32 g = 0; g < static_cast<u32>(TickGroup::Count); ++g) {
            for (u32 i = 0; i < counts_[g]; ++i) {
                systems_[g][i]->tick(scene, dt);
            }
        }
    }

    u32 count(TickGroup group) const { return counts_[static_cast<u32>(group)]; }

private:
    System* systems_[static_cast<u32>(TickGroup::Count)][kMaxSystemsPerGroup] = {};
    u32     counts_[static_cast<u32>(TickGroup::Count)] = {};
};

} // namespace vv
