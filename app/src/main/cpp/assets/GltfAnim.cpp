// assets/GltfAnim.cpp — conversão GltfAnimation → AnimClip (0.8.1, F7).
#include "assets/GltfAnim.h"
#include "components/AnimationPlayer.h"
#include "components/SkeletonComp.h"   // 0.8.2
#include "components/Transform3D.h"
#include "core/Scene.h"
#include "core/ComponentStore.h"
#include "math/Math.h"
#include <cmath>

namespace vv {
namespace {

inline f32 rad2deg(f32 r) { return r * 57.2957795131f; }

} // namespace

u32 gltfAttachClips(Scene& scene, Handle ticH, const GltfModel& model,
                    i32 rootNode) {
    if (!ticH.valid() || model.animations.empty()) {
        return 0;
    }
    Tic* tic = scene.get(ticH);
    if (!tic || !tic->active) {
        return 0;
    }
    AnimationPlayer* pl = tic->getComponent<AnimationPlayer>();
    if (!pl) {
        pl = tic->addComponent<AnimationPlayer>();
        if (!pl) {
            return 0;
        }
    }
    if (!tic->getComponent<Transform3D>()) {
        // sem Transform3D os tracks não têm onde escrever — cria (o import
        // de mesh já o faz; defesa para chamadas manuais)
        if (!tic->addComponent<Transform3D>()) {
            return 0;
        }
    }

    // 0.8.2 (F7): esqueleto já anexado → os canais de JOINT entram como
    // tracks Joint* (por nome); sem esqueleto, esses canais saltam
    const SkeletonComp* skeleton = tic->getComponent<SkeletonComp>();
    const std::vector<i32>* skinNodeToJoint = nullptr;
    if (!model.skins.empty() && skeleton) {
        skinNodeToJoint = &model.skins.front().nodeToJoint;
    }

    u32 added = 0;
    for (const GltfAnimation& ga : model.animations) {
        AnimClip clip;
        clip.name = ga.name.empty() ? "anim" : ga.name;
        for (const GltfAnimChannel& ch : ga.channels) {
            if (ch.sampler < 0 ||
                static_cast<size_t>(ch.sampler) >= ga.samplers.size()) {
                continue;
            }
            // 0.8.2 (F7): nó JOINT da 1ª skin → track de JOINT (por nome);
            // nó raiz → track do TIC; resto → saltado
            AnimTarget targetKind;
            std::string jointName;
            if (ch.node == rootNode) {
                switch (ch.path) {
                    case GltfAnimChannel::Path::Translation:
                        targetKind = AnimTarget::TicPos; break;
                    case GltfAnimChannel::Path::Rotation:
                        targetKind = AnimTarget::TicRot; break;
                    default:
                        targetKind = AnimTarget::TicScale; break;
                }
            } else if (skinNodeToJoint && ch.node >= 0 &&
                       ch.node < static_cast<i32>(skinNodeToJoint->size()) &&
                       (*skinNodeToJoint)[static_cast<size_t>(ch.node)] >= 0 &&
                       skeleton) {
                const i32 jointIdx =
                    (*skinNodeToJoint)[static_cast<size_t>(ch.node)];
                jointName = skeleton->joints[static_cast<size_t>(jointIdx)].name;
                switch (ch.path) {
                    case GltfAnimChannel::Path::Translation:
                        targetKind = AnimTarget::JointPos; break;
                    case GltfAnimChannel::Path::Rotation:
                        targetKind = AnimTarget::JointRot; break;
                    default:
                        targetKind = AnimTarget::JointScale; break;
                }
            } else {
                continue;
            }
            const GltfAnimSampler& s = ga.samplers[static_cast<size_t>(ch.sampler)];
            if (s.times.empty() ||
                s.values.size() < s.times.size() * s.components) {
                continue;
            }
            AnimTrack tr;
            tr.curve = AnimCurve::Linear;
            tr.target = targetKind;
            tr.element = jointName;   // vazio nos tracks do TIC
            for (size_t k = 0; k < s.times.size(); ++k) {
                AnimKey key;
                key.t = s.times[k] < 0.0f ? 0.0f : s.times[k];
                const f32* src = &s.values[k * s.components];
                if (targetKind == AnimTarget::TicRot ||
                    targetKind == AnimTarget::JointRot) {
                    // quat (x,y,z,w) → euler GRAUS YXZ (a convenção do player)
                    Quat q{src[0], src[1], src[2],
                           s.components >= 4 ? src[3] : 1.0f};
                    q.normalize();   // glTF quats são unitários — defesa
                    f32 ex = 0.0f, ey = 0.0f, ez = 0.0f;
                    Quat::toEuler(q, ex, ey, ez);
                    key.v[0] = rad2deg(ex);
                    key.v[1] = rad2deg(ey);
                    key.v[2] = rad2deg(ez);
                } else {
                    key.v[0] = src[0];
                    key.v[1] = src[1];
                    key.v[2] = src[2];
                }
                tr.keys.push_back(key);
            }
            tr.sortKeys();
            clip.tracks.push_back(std::move(tr));
        }
        if (clip.tracks.empty()) {
            continue;   // animação sem channel útil p/ o nó → fora
        }
        pl->clips.push_back(std::move(clip));
        ++added;
    }
    if (added > 0 && pl->clips.size() > added) {
        // já havia clips (ex.: "edit"): o 1º IMPORTADO fica ATIVO (o dono
        // acabou de o importar — é o que quer ver a tocar)
        pl->activeClip = 1;
    }
    return added;
}

// ---- 0.8.2 (F7): SKIN → SkeletonComp ----------------------------------------
u32 gltfAttachSkin(Scene& scene, Handle ticH, const GltfModel& model,
                   u32 skinIdx) {
    if (!ticH.valid() || skinIdx >= model.skins.size()) {
        return 0;
    }
    Tic* tic = scene.get(ticH);
    if (!tic || !tic->active) {
        return 0;
    }
    if (tic->getComponent<SkeletonComp>()) {
        return 0;   // já tem esqueleto (reimport não duplica)
    }
    const GltfSkin& src = model.skins[skinIdx];
    SkeletonComp* sk = tic->addComponent<SkeletonComp>();
    if (!sk) {
        return 0;
    }
    for (const GltfJoint& j : src.joints) {
        SkeletonComp::Joint jt;
        jt.name = j.name;
        jt.parent = j.parent;
        jt.pos = j.pos;             // TRS LOCAL nasce no BIND (rest pose)
        jt.rot = j.rot;
        jt.scale = j.scale;
        jt.bindPos = j.pos;          // 0.8.3: alvo do fade do blend
        jt.bindRot = j.rot;
        jt.bindScale = j.scale;
        jt.inverseBind = j.inverseBind;
        sk->joints.push_back(std::move(jt));
    }
    return static_cast<u32>(sk->joints.size());
}

// ---- 0.8.10: .gm (formato próprio) → clips + SkeletonComp --------------------
// O caminho do LOAD de cenas convertidas: o .goni aponta assets/x.gmesh e o
// .gm irmão traz os clips baked + o esqueleto. MESMAS regras do glTF: clips
// ACRESCENTAM ao "edit" (sem duplicar nome); esqueleto só se ainda não há.
u32 attachGAnim(Scene& scene, Handle ticH, const GAnimFile& anim) {
    if (!ticH.valid()) {
        return 0;
    }
    Tic* tic = scene.get(ticH);
    if (!tic || !tic->active) {
        return 0;
    }
    u32 added = 0;
    AnimationPlayer* pl = tic->getComponent<AnimationPlayer>();
    if (!pl) {
        pl = tic->addComponent<AnimationPlayer>();
    }
    if (pl) {
        for (const AnimClip& src : anim.clips) {
            if (src.tracks.empty()) {
                continue;
            }
            bool dup = false;
            for (const AnimClip& c : pl->clips) {
                if (c.name == src.name) {
                    dup = true;
                    break;
                }
            }
            if (!dup) {
                pl->clips.push_back(src);
                ++added;
            }
        }
        if (added > 0 && pl->activeClip < 0) {
            pl->activeClip = 1;
        }
    }
    if (!anim.joints.empty() && !tic->getComponent<SkeletonComp>()) {
        SkeletonComp* sk = tic->addComponent<SkeletonComp>();
        if (sk) {
            sk->joints = anim.joints;
        }
    }
    return added;
}

} // namespace vv
