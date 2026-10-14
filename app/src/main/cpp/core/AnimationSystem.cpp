// core/AnimationSystem.cpp — avanço dos AnimationPlayers no passo fixo.
// 0.11-T (FASE 3.5a): o SKINNED CULL — os TICs skinados fora do frustum
// não avançam (o tempo do clip congela; o personagem invisível não gasta
// CPU de advance+apply e o draw salta-o também — o contador é o número
// que o pin C33 aferre).
#include "core/AnimationSystem.h"
#include "components/AnimationPlayer.h"
#include "components/MeshRenderer.h"
#include "components/SkeletonComp.h"
#include "core/Scene.h"
#include "core/ComponentStore.h"
#include "render/BlockMesh.h"
#include "render/Mesh.h"

namespace vv {

// o teste do skinned cull (o MESMO contrato que o drawTics usa no main —
// UMA fonte do cálculo: os bounds do mesh/blocos inchados por
// kSkinCullAabbSwell, transformados pelo world do TIC, contra os planos).
// false = não skinado OU dentro do frustum (nunca culle por engano).
static bool skinOutsideFrustum(const Scene& scene, Handle h,
                               const f32 planes[6][4]) {
    const ComponentStore& comps = scene.components();
    const MeshRenderer* mr = comps.meshRenderers().find(h);
    if (mr == nullptr) {
        return false;
    }
    const bool skinned = (mr->blocks != nullptr && mr->blocks->skinned()) ||
                         (mr->mesh != nullptr && mr->mesh->skinned());
    if (!skinned) {
        return false;   // o cull é SÓ de skinados (o resto segue o de sempre)
    }
    Vec3 mn{}, mx{};
    if (mr->blocks != nullptr) {
        mn = mr->blocks->boundsMin();
        mx = mr->blocks->boundsMax();
    } else if (mr->mesh != nullptr) {
        mn = mr->mesh->boundsMin();
        mx = mr->mesh->boundsMax();
    } else {
        return false;
    }
    // o INCHAMENTO: a pose animada ultrapassa o AABB de bind (swing de
    // braços/pernas) — 25% da extensão em cada lado (conservativo)
    const Vec3 ext{(mx.x - mn.x) * BlockMesh::kSkinCullAabbSwell,
                   (mx.y - mn.y) * BlockMesh::kSkinCullAabbSwell,
                   (mx.z - mn.z) * BlockMesh::kSkinCullAabbSwell};
    mn.x -= ext.x; mn.y -= ext.y; mn.z -= ext.z;
    mx.x += ext.x; mx.y += ext.y; mx.z += ext.z;
    Mat4 model = Mat4::identity();
    if (const Transform3D* tr = comps.transforms().find(h)) {
        model = tr->world;   // o cache do TransformSystem (o passo anterior)
    }
    return !BlockMesh::worldAabbVisible(model, mn, mx, planes);
}

void AnimationSystem::tick(Scene& scene, f32 dt) {
    skinnedCulled = 0;
    culledOwners.clear();
    if (!enabled) {
        return;
    }
    auto& players = scene.components().animators();
    for (u32 i = 0; i < players.size(); ++i) {
        AnimationPlayer& pl = players.at(i);
        if (!pl.playing) {
            continue;
        }
        const Handle owner = players.owner(i);
        // 0.11-T (3.5a): o SKINNED CULL — fora do frustum o TIC não
        // avança (NEM advance NEM apply: o tempo congela e retoma quando
        // o TIC volta a ser visível). Sem frustum injetado o teste nem
        // corre (custo zero — o comportamento de sempre).
        if (frustumValid && frustum != nullptr &&
            skinOutsideFrustum(scene, owner, frustum)) {
            ++skinnedCulled;
            culledOwners.push_back(owner);
            continue;
        }
        pl.advance(dt);
        pl.apply(scene, players.owner(i));
    }
}

} // namespace vv
