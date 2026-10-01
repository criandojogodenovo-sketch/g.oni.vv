// components/AnimationPlayer.cpp — implementação do player de animação (0.8.0).
//
// GL-free: interpolação/advance/apply são matemática pura sobre os storages
// da Scene — os testes do CI aferem tudo sem GL.
#include "components/AnimationPlayer.h"
#include "components/SkeletonComp.h"   // 0.8.2: tracks de joint
#include "components/Transform3D.h"
#include "components/UiCanvas.h"
#include "core/Scene.h"
#include "core/ComponentStore.h"
#include "math/Math.h"
#include <algorithm>
#include <cmath>

namespace vv {

void AnimTrack::sortKeys() {
    std::stable_sort(keys.begin(), keys.end(),
                     [](const AnimKey& a, const AnimKey& b) { return a.t < b.t; });
}

f32 AnimClip::duration() const {
    f32 d = 0.0f;
    for (const AnimTrack& t : tracks) {
        d = t.duration() > d ? t.duration() : d;
    }
    return d;
}

namespace {

// graus → radianos (mesma constante do EditorUi — Math.h não expõe)
inline f32 deg2rad(f32 d) { return d * 0.01745329252f; }

// bezier cúbica por canal: p0=v0, p1=v0+out, p2=v1+in, p3=v1
inline f32 bezier1d(f32 p0, f32 p1, f32 p2, f32 p3, f32 u) {
    const f32 iu = 1.0f - u;
    const f32 a = iu * iu * iu;
    const f32 b = 3.0f * iu * iu * u;
    const f32 c = 3.0f * iu * u * u;
    const f32 d = u * u * u;
    return a * p0 + b * p1 + c * p2 + d * p3;
}

inline f32 lerp1d(f32 a, f32 b, f32 u) {
    return a + (b - a) * u;
}

} // namespace

bool evalTrack(const AnimTrack& tr, f32 t, f32 out[4]) {
    if (tr.keys.empty()) {
        return false;
    }
    const size_t n = tr.keys.size();
    if (t <= tr.keys.front().t) {
        for (int i = 0; i < 4; ++i) {
            out[i] = tr.keys.front().v[i];
        }
        return true;
    }
    if (t >= tr.keys.back().t) {
        for (int i = 0; i < 4; ++i) {
            out[i] = tr.keys.back().v[i];
        }
        return true;
    }
    // segmento [i, i+1] com keys[i].t <= t < keys[i+1].t
    size_t i = 0;
    for (size_t k = 0; k + 1 < n; ++k) {
        if (t >= tr.keys[k].t && t < tr.keys[k + 1].t) {
            i = k;
            break;
        }
    }
    const AnimKey& k0 = tr.keys[i];
    const AnimKey& k1 = tr.keys[i + 1];
    const f32 span = k1.t - k0.t;
    const f32 u = span > 1e-6f ? (t - k0.t) / span : 1.0f;
    for (int c = 0; c < 4; ++c) {
        if (tr.curve == AnimCurve::Bezier) {
            out[c] = bezier1d(k0.v[c], k0.v[c] + k0.tanOut[c],
                              k1.v[c] + k1.tanIn[c], k1.v[c], u);
        } else {
            out[c] = lerp1d(k0.v[c], k1.v[c], u);
        }
    }
    return true;
}

void blendKeys(const f32 a[4], const f32 b[4], f32 w, f32 out[4]) {
    for (int i = 0; i < 4; ++i) {
        out[i] = a[i] + (b[i] - a[i]) * w;
    }
}

AnimClip* AnimationPlayer::editClip() {
    if (clips.empty()) {
        AnimClip c;
        c.name = "edit";
        clips.push_back(std::move(c));
    }
    if (clips[0].name.empty()) {
        clips[0].name = "edit";
    }
    return &clips[0];
}

AnimClip* AnimationPlayer::activeClipPtr() {
    if (activeClip < 0 || static_cast<size_t>(activeClip) >= clips.size()) {
        return nullptr;
    }
    return &clips[static_cast<size_t>(activeClip)];
}

const AnimClip* AnimationPlayer::activeClipPtr() const {
    if (activeClip < 0 || static_cast<size_t>(activeClip) >= clips.size()) {
        return nullptr;
    }
    return &clips[static_cast<size_t>(activeClip)];
}

AnimTrack* AnimationPlayer::addTrack(AnimTarget target, const char* element) {
    AnimClip* clip = editClip();
    if (!clip) {
        return nullptr;   // defesa (nunca: editClip cria)
    }
    const std::string el = element ? element : "";
    for (AnimTrack& t : clip->tracks) {
        if (t.target == target && t.element == el) {
            return &t;   // idempotente: o MESMO alvo devolve o MESMO track
        }
    }
    AnimTrack t;
    t.target = target;
    t.element = el;
    clip->tracks.push_back(std::move(t));
    return &clip->tracks.back();
}

const char* AnimationPlayer::modeName(Mode m) {
    switch (m) {
        case Mode::Once:     return "once";
        case Mode::Loop:     return "loop";
        case Mode::PingPong: return "pingpong";
    }
    return "loop";
}

f32 AnimationPlayer::duration() const {
    if (activeClip < 0 || static_cast<size_t>(activeClip) >= clips.size()) {
        return 0.0f;
    }
    return clips[static_cast<size_t>(activeClip)].duration();
}

void AnimationPlayer::advance(f32 dt) {
    if (!playing) {
        return;
    }
    const f32 dur = duration();
    if (dur <= 1e-6f) {
        time = 0.0f;   // clip vazio: parado no zero (não acumula)
        return;
    }
    time += dt * speed * static_cast<f32>(dir_);
    switch (mode) {
        case Mode::Once:
            if (time >= dur) {
                time = dur;         // para EXATAMENTE no fim
                playing = false;
            } else if (time < 0.0f) {
                time = 0.0f;        // speed negativa? clamp honesto
            }
            break;
        case Mode::Loop:
            // módulo com sinal correto (time pode ser negativo com speed<0)
            time = std::fmod(time, dur);
            if (time < 0.0f) {
                time += dur;
            }
            break;
        case Mode::PingPong:
            // reflete nos extremos; período 2·dur; dir_ acompanha o sentido
            while (time > dur || time < 0.0f) {
                if (time > dur) {
                    time = 2.0f * dur - time;
                    dir_ = -1;
                } else if (time < 0.0f) {
                    time = -time;
                    dir_ = 1;
                }
            }
            break;
    }
}

void AnimationPlayer::apply(Scene& scene, const Tic& owner) const {
    if (activeClip < 0 || static_cast<size_t>(activeClip) >= clips.size()) {
        return;
    }
    const AnimClip& clip = clips[static_cast<size_t>(activeClip)];
    if (clip.tracks.empty() || !owner.scene) {
        return;
    }
    Tic* tic = scene.get(owner.handle);
    if (!tic) {
        return;
    }
    Transform3D* tr = tic->getComponent<Transform3D>();
    UiCanvas* ui = tic->getComponent<UiCanvas>();
    SkeletonComp* sk = tic->getComponent<SkeletonComp>();   // 0.8.2
    f32 v[4] = {0.0f, 0.0f, 0.0f, 1.0f};
    for (const AnimTrack& track : clip.tracks) {
        if (!evalTrack(track, time, v)) {
            continue;   // track sem keys — nada a escrever
        }
        switch (track.target) {
            case AnimTarget::TicPos:
                if (tr) {
                    tr->pos = Vec3{v[0], v[1], v[2]};
                    tr->updateWorld();
                }
                break;
            case AnimTarget::TicRot:
                if (tr) {
                    tr->rot = Quat::fromEuler(deg2rad(v[0]), deg2rad(v[1]),
                                              deg2rad(v[2]));
                    tr->updateWorld();
                }
                break;
            case AnimTarget::TicScale:
                if (tr) {
                    tr->scale = Vec3{v[0], v[1], v[2]};
                    tr->updateWorld();
                }
                break;
            case AnimTarget::UiPos:
            case AnimTarget::UiColor:
            case AnimTarget::UiAlpha: {
                if (!ui || track.element.empty()) {
                    break;
                }
                const i32 idx = ui->findElement(track.element);
                if (idx < 0) {
                    break;   // elemento sumiu — salta (nunca crasha)
                }
                UiElement& e = ui->elements[static_cast<size_t>(idx)];
                if (track.target == AnimTarget::UiPos) {
                    e.ox = v[0];
                    e.oy = v[1];
                } else if (track.target == AnimTarget::UiColor) {
                    e.color[0] = v[0];
                    e.color[1] = v[1];
                    e.color[2] = v[2];
                } else {
                    e.color[3] = v[0];
                }
                break;
            }
            // 0.8.2 (F7) — TRS LOCAL de um joint do esqueleto (por nome)
            case AnimTarget::JointPos:
            case AnimTarget::JointRot:
            case AnimTarget::JointScale: {
                if (!sk || track.element.empty()) {
                    break;
                }
                const i32 j = sk->findJoint(track.element);
                if (j < 0) {
                    break;   // joint sumiu — salta (nunca crasha)
                }
                SkeletonComp::Joint& jt = sk->joints[static_cast<size_t>(j)];
                if (track.target == AnimTarget::JointPos) {
                    jt.pos = Vec3{v[0], v[1], v[2]};
                } else if (track.target == AnimTarget::JointRot) {
                    jt.rot = Quat::fromEuler(deg2rad(v[0]), deg2rad(v[1]),
                                             deg2rad(v[2]));
                } else {
                    jt.scale = Vec3{v[0], v[1], v[2]};
                }
                break;
            }
        }
    }
}

void AnimationPlayer::apply(Scene& scene, Handle owner) const {
    const Tic* tic = scene.get(owner);
    if (tic) {
        apply(scene, *tic);
    }
}

} // namespace vv
