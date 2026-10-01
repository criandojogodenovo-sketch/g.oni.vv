// assets/GltfAnim.cpp — conversão GltfAnimation → AnimClip (0.8.1, F7).
#include "assets/GltfAnim.h"
#include "components/AnimationPlayer.h"
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

    u32 added = 0;
    for (const GltfAnimation& ga : model.animations) {
        AnimClip clip;
        clip.name = ga.name.empty() ? "anim" : ga.name;
        for (const GltfAnimChannel& ch : ga.channels) {
            if (ch.node != rootNode) {
                continue;   // 0.8.2 (skinning): joints; aqui só o nó raiz
            }
            if (ch.sampler < 0 ||
                static_cast<size_t>(ch.sampler) >= ga.samplers.size()) {
                continue;
            }
            const GltfAnimSampler& s = ga.samplers[static_cast<size_t>(ch.sampler)];
            if (s.times.empty() ||
                s.values.size() < s.times.size() * s.components) {
                continue;
            }
            AnimTrack tr;
            tr.curve = AnimCurve::Linear;
            switch (ch.path) {
                case GltfAnimChannel::Path::Translation:
                    tr.target = AnimTarget::TicPos;
                    break;
                case GltfAnimChannel::Path::Rotation:
                    tr.target = AnimTarget::TicRot;
                    break;
                case GltfAnimChannel::Path::Scale:
                    tr.target = AnimTarget::TicScale;
                    break;
            }
            for (size_t k = 0; k < s.times.size(); ++k) {
                AnimKey key;
                key.t = s.times[k] < 0.0f ? 0.0f : s.times[k];
                const f32* src = &s.values[k * s.components];
                if (ch.path == GltfAnimChannel::Path::Rotation) {
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

} // namespace vv
