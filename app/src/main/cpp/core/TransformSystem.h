#pragma once
// core/TransformSystem.h — system do grupo Update que mantém o cache de
// matrizes dos Transform3D (F3). O pass 3D desenha cada MeshRenderer com
// owner->Transform3D.world — o system garante que o cache reflete pos/rot/
// scale atuais a cada passo fixo.
//
// 0.12-V — O FEEDBACK IMEDIATO NO CONTRATO DO TICK: o drag do gizmo e os
// campos do Inspector escrevem pos/rot/scale ENTRE ticks e refrescavam o
// cache com updateWorld() (world = TRS local — a raiz de sempre). Com o TIC
// PAIOUTRO, esse cache provisório IGNORAVA o pai: o mesh desenhava UM FRAME
// no sítio errado (o mundo local-as-world — o "teleporte" visível durante o
// arrasto e nos edits do Inspector). composeWorld() é a FONTE ÚNICA da
// composição (o MESMO contrato do tick: world = parentWorld ∘ TRS; TRS na
// raiz) — o tick de baixo e os sites de feedback consomem a MESMA matemática.
#include "core/Tick.h"
#include "components/Transform3D.h"   // 0.12-V: composeWorld/worldToLocalPos
#include "math/Math.h"                // 0.12-V: Mat4/Vec3 na assinatura

namespace vv {

class TransformSystem : public System {
public:
    void tick(Scene& scene, f32 dt) override;

    // 0.12-V — A COMPOSIÇÃO ÚNICA: world = parentWorld ∘ TRS quando há pai;
    // world = TRS(pos, rot, scale) na raiz (o comportamento de sempre — os
    // TICs sem pai ficam byte a byte). Limpa worldDirty (cache fresco).
    static void composeWorld(Transform3D& tr, const Mat4* parentWorld);

    // 0.12-V — MUNDO→LOCAL do PAI (TRS puro, sem shear — o mesmo contrato
    // da composição): p_local = S⁻¹·R⁻¹·(p_mundo − pos). O gizmo arrasta NO
    // MUNDO (o que o dedo vê); o Transform3D guarda o TRS LOCAL — a conversa
    // mundo↔local vive AQUI (uma fonte; o main e os pins partilham).
    static Vec3 worldToLocalPos(const Transform3D& parent,
                                const Vec3& worldPos);

    // 0.12-V — o Transform3D do PAI do TIC (parentSlot é o ÍNDICE do slot —
    // Tic::parent; nullptr se o slot não existe/está inativo ou se o pai não
    // tem Transform3D). Resolução por evento (drag/edit — nunca por frame).
    static const Transform3D* parentTransformOf(Scene& scene, i32 parentSlot);
};

} // namespace vv
