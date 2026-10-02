#pragma once
// core/SceneBounds.h — AABB da CENA (0.8.9, ESPAÇO SEM TETOS).
//
// Fonte única do FAR DINÂMICO do editor e do Play ("recalculados em
// edição/load" — por frame, que cobre ambos). Pura/GL-free: pos ± extents
// do mesh × escala (primitivas incluídas — o Mesh do cache tem AABB
// calculado no create). ROTAÇÃO ignorada por desenho (AABB do mesh alinhado
// aos eixos do TIC): com a margem (distância-ao-mais-longe + folga) é
// suficiente e mantém a matemática trivial de aferir no CI.
//
// O FAR NÃO é só o "tamanho" da cena: um TIC a py=10 000 numa cena
// "pequena" (AABB de 1 unidade em volta dele) continua a ESTAR longe da
// câmara — o derivado usa a DISTÂNCIA DA ORIGEM ao ponto mais longe
// (sceneFarthest) + a distância do zoom (o olho orbita a origem); no Play,
// a distância do OLHO da câmara de jogo ao canto mais longe do AABB.
//
// TESTES (test_wiring089): cena vazia → 0; TICs em py=10 000 → o far
// derivado CONTÉM o AABB; slider do dono é piso no Play.
#include "core/Types.h"
#include "math/Math.h"
#include "core/Scene.h"
#include "core/ComponentStore.h"
#include "components/Transform3D.h"
#include "components/MeshRenderer.h"
#include "components/CameraComp.h"
#include "render/Camera.h"
#include "render/Mesh.h"   // 0.8.9: boundsMin/Max — dados (GL-free nos acessores)

namespace vv {
namespace camerautil {

// AABB da cena inteira (TICs ativos COM MeshRenderer — o que se VÊ).
// Cena vazia/sem meshes → {0,0,0}/{0,0,0}, raio 0. `radiusOut` = diagonal/2.
inline void sceneAABB(const Scene& scene, Vec3& outMin, Vec3& outMax,
                      f32& radiusOut) {
    f32 mnx = 1e30f, mny = 1e30f, mnz = 1e30f;
    f32 mxx = -1e30f, mxy = -1e30f, mxz = -1e30f;
    bool any = false;
    scene.forEachActive([&](const Tic& t) {
        const MeshRenderer* mr =
            scene.components().meshRenderers().find(t.handle);
        const Transform3D* tr = scene.components().transforms().find(t.handle);
        if (!mr) {
            return;
        }
        any = true;
        const Vec3 pos = tr ? tr->pos : Vec3{0.0f, 0.0f, 0.0f};
        const Vec3 scl = tr ? tr->scale : Vec3{1.0f, 1.0f, 1.0f};
        if (mr->mesh) {
            const Vec3 emn = mr->mesh->boundsMin();
            const Vec3 emx = mr->mesh->boundsMax();
            const f32 lo[3] = {pos.x + emn.x * scl.x,
                               pos.y + emn.y * scl.y,
                               pos.z + emn.z * scl.z};
            const f32 hi[3] = {pos.x + emx.x * scl.x,
                               pos.y + emx.y * scl.y,
                               pos.z + emx.z * scl.z};
            if (lo[0] < mnx) mnx = lo[0];
            if (lo[1] < mny) mny = lo[1];
            if (lo[2] < mnz) mnz = lo[2];
            if (hi[0] > mxx) mxx = hi[0];
            if (hi[1] > mxy) mxy = hi[1];
            if (hi[2] > mxz) mxz = hi[2];
        } else {
            if (pos.x < mnx) mnx = pos.x;
            if (pos.y < mny) mny = pos.y;
            if (pos.z < mnz) mnz = pos.z;
            if (pos.x > mxx) mxx = pos.x;
            if (pos.y > mxy) mxy = pos.y;
            if (pos.z > mxz) mxz = pos.z;
        }
    });
    if (!any) {
        outMin = Vec3{0.0f, 0.0f, 0.0f};
        outMax = Vec3{0.0f, 0.0f, 0.0f};
        radiusOut = 0.0f;
        return;
    }
    outMin = Vec3{mnx, mny, mnz};
    outMax = Vec3{mxx, mxy, mxz};
    const f32 dx = mxx - mnx, dy = mxy - mny, dz = mxz - mnz;
    radiusOut = std::sqrt(dx * dx + dy * dy + dz * dz) * 0.5f;
}

// distância da ORIGEM ao ponto mais longe do AABB (0 se vazio) — o olho do
// editor orbita a origem: o far precisa de chegar AQUI + dist.
inline f32 sceneFarthest(const Vec3& mn, const Vec3& mx) {
    f32 best = 0.0f;
    for (int c = 0; c < 8; ++c) {
        const Vec3 p{c & 1 ? mx.x : mn.x, c & 2 ? mx.y : mn.y,
                     c & 4 ? mx.z : mn.z};
        const f32 d = length(p);
        if (d > best) {
            best = d;
        }
    }
    return best;
}

// distância de `from` (olho) ao canto mais longe do AABB — o derivado do
// Play (a câmara de jogo pode estar em qualquer sítio).
inline f32 aabbFarthestDist(const Vec3& from, const Vec3& mn, const Vec3& mx) {
    f32 best = 0.0f;
    for (int c = 0; c < 8; ++c) {
        const Vec3 p{c & 1 ? mx.x : mn.x, c & 2 ? mx.y : mn.y,
                     c & 4 ? mx.z : mn.z};
        const f32 d = length(p - from);
        if (d > best) {
            best = d;
        }
    }
    return best;
}

// clipes do EDITOR por frame: near = 5% da distância (piso 0.01 — zoom
// mínimo 0.01 → near nunca degenera), far = max(1.5×dist,
// maisLongeDaOrigem + dist + 10, 450). No zoom de trabalho (6): near 0.3 /
// far 450 ≈ o feel F3.1 de sempre; com um TIC a py=10 000: far ≥ 10 016 —
// o objeto CABE no frustum; no zoom máx (100 000): far = 150 000+.
inline void editorClips(f32 dist, f32 sceneFar, f32& nearOut, f32& farOut) {
    nearOut = dist * 0.05f;
    if (nearOut < 0.01f) {
        nearOut = 0.01f;
    }
    farOut = dist * 1.5f;
    const f32 sceneNeed = sceneFar + dist + 10.0f;
    if (sceneNeed > farOut) {
        farOut = sceneNeed;
    }
    if (farOut < Camera::kDefaultFar) {
        farOut = Camera::kDefaultFar;
    }
}

// far EFETIVO do PLAY: o máximo entre o farZ do CameraComp (o slider do
// dono — respeitado como PISO) e a distância do olho ao mais longe + 10.
// "Recalculado em edição/load" = por frame. O near segue o componente com
// defesa de rácio (far/near ≤ 100 000 contra z-fighting a enormidades).
inline f32 playFar(const CameraComp& cam, f32 eyeFarthest) {
    const f32 need = eyeFarthest + 10.0f;
    return cam.farZ >= need ? cam.farZ : need;
}
inline f32 playNear(const CameraComp& cam, f32 farEff) {
    f32 n = cam.nearZ > 0.0f ? cam.nearZ : CameraComp::kDefaultNear;
    if (farEff / n > 100000.0f) {
        n = farEff / 100000.0f;   // rácio máximo — precisão do depth
    }
    return n;
}

} // namespace camerautil
} // namespace vv
