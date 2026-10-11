#include "core/TransformSystem.h"
#include "components/Transform3D.h"
#include "core/Scene.h"

#include <vector>

namespace vv {

namespace {

// 0.10-M HOTFIX (f) — A COMPOSIÇÃO DE PAIS (a dívida «editor polish» do
// F3 paga — a spec do dono do hotfix TEXTURAS-VINCULA manda a sub-árvore
// REAL do import expandido: «nomes do glTF, transform local por TIC»).
// world(i) = world(pai) ∘ TRS(i) quando o TIC tem pai; world = TRS(i) na
// raiz (o comportamento de sempre — TICs sem pai ficam byte a byte).
// MEMO + guard de ciclo: um ciclo (nunca deve — o serializer corta
// auto-parents desde a 0.8.9) resolve como RAIZ em vez de pendurar.
void composeAt(u32 slot,
               std::vector<char>& done, std::vector<char>& visiting,
               std::vector<Transform3D*>& worldBySlot,
               std::vector<i32>& parentBySlot) {
    if (slot >= done.size() || done[slot] || worldBySlot[slot] == nullptr) {
        return;
    }
    if (visiting[slot]) {
        done[slot] = 1;   // ciclo — resolve como raiz (nunca pendurar)
        return;
    }
    visiting[slot] = 1;
    Transform3D& tr = *worldBySlot[slot];
    const i32 p = parentBySlot[slot];
    if (p >= 0 && static_cast<u32>(p) < done.size() &&
        static_cast<u32>(p) != slot &&
        worldBySlot[static_cast<u32>(p)] != nullptr) {
        composeAt(static_cast<u32>(p), done, visiting, worldBySlot,
                  parentBySlot);
        if (!visiting[static_cast<u32>(p)] || done[static_cast<u32>(p)]) {
            // world = parentWorld ∘ local (a MESMA ordem do glTF — o
            // worldChainOf do conversor compõe raiz→folha igual)
            tr.world = Mat4::mul(
                worldBySlot[static_cast<u32>(p)]->world, tr.computeMatrix());
            tr.worldDirty = false;   // o cache é freshly-composto
            visiting[slot] = 0;
            done[slot] = 1;
            return;
        }
    }
    tr.updateWorld();   // raiz (ou pai sem transform): o local de sempre
    visiting[slot] = 0;
    done[slot] = 1;
}

} // namespace

void TransformSystem::tick(Scene& scene, f32 /*dt*/) {
    // F3: cada transform é independente — world = TRS(pos, rot, scale) —
    // e é ISTO que continua a acontecer a TIC SEM PAI (byte a byte).
    // 0.10-M HOTFIX (f): com pai, world = world(pai) ∘ TRS — a sub-árvore
    // do import expandido desenha e edita como ÁRVORE (o filho move-se
    // com o pai; o Transform3D guarda o TRS LOCAL por TIC).
    auto& transforms = scene.components().transforms();
    // F4: refresca TUDO (barato) e limpa worldDirty marcada pela física.
    // O mapa slot→estado resolve os pais onde estiverem (a ordem do
    // storage é densa e não garante pai-antes-de-filho).
    const u32 cap = scene.capacity();
    std::vector<Transform3D*> worldBySlot(cap, nullptr);
    std::vector<i32> parentBySlot(cap, -1);
    for (u32 i = 0; i < transforms.size(); ++i) {
        Transform3D& tr = transforms.at(i);
        const Handle h = transforms.owner(i);
        worldBySlot[h.index] = &tr;
    }
    scene.forEachActive([&](Tic& t) {
        if (t.handle.index < cap) {
            parentBySlot[t.handle.index] = t.parent;
        }
    });
    std::vector<char> done(cap, 0), visiting(cap, 0);
    for (u32 i = 0; i < transforms.size(); ++i) {
        const Handle h = transforms.owner(i);
        composeAt(h.index, done, visiting, worldBySlot, parentBySlot);
    }
}

} // namespace vv
