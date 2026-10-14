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
//
// 0.11-T (FASE 3.5a) — O SKINNED CULL: com `frustumValid` (o main injeta
// os 6 planos do VP — os mesmos do render por blocos, 0.10-M), o system
// SALTA os TICs com SkeletonComp cujo mesh skinado está FORA do frustum
// (o AABB do mesh, inchado 25% pela extensão — a pose animada passa o
// AABB de bind; conservativo: nunca culle um personagem visível). Saltar
// = NEM advance NEM apply — o tempo do clip CONGELA fora do ecrã e
// retoma do mesmo ponto quando o TIC volta (o personagem invisível não
// gasta CPU de skinning: o pin manda «skinned fora do frustum a animar:
// 0»). Sem frustum (os hosts/testes que não o injetam) o custo é ZERO e
// o comportamento é byte a byte o de sempre. O contador `skinnedCulled`
// e a lista `culledOwners` referem-se ao ÚLTIMO tick (o main soma-os no
// frame e espelha no HUD/engine.log).
#include "core/Tick.h"
#include "core/Handle.h"
#include <vector>

namespace vv {

class AnimationSystem : public System {
public:
    bool enabled = false;   // só em Play (como o PhysicsSystem)

    // 0.11-T (3.5a): o frustum do skinned cull — os 6 planos do VP (o
    // MESMO formato de BlockMesh::frustumPlanes). Não-dono: o main é o
    // dono do array e escreve-o POR FRAME (os planos do frame anterior —
    // o vp do frame nasce DEPOIS dos systems; o inchamento conservativo
    // do AABB cobre a diferença de um frame).
    const f32 (*frustum)[4] = nullptr;
    bool frustumValid = false;

    // os resultados DO ÚLTIMO tick (o main soma no frame para o HUD/log)
    u32 skinnedCulled = 0;
    std::vector<Handle> culledOwners;   // os TICs saltados

    void tick(Scene& scene, f32 dt) override;
};

} // namespace vv
