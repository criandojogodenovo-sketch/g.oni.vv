// assets/GltfAnim.cpp — conversão GltfAnimation → AnimClip (0.8.1, F7).
#include "assets/GltfAnim.h"
#include "components/AnimationPlayer.h"
#include "components/SkeletonComp.h"   // 0.8.2
#include "components/Transform3D.h"
#include "core/Scene.h"
#include "core/ComponentStore.h"
#include "math/Math.h"
#include "platform/EngineLog.h"        // 0.11-S: o cap honesto do UBO
#include <cmath>

namespace vv {
namespace {

inline f32 rad2deg(f32 r) { return r * 57.2957795131f; }

// 0.11-S (FASE 3): euler graus de um quat glTF (normalizado — a spec manda
// quats unitários, o parser tolera desvio).
inline void quatToEulerDeg(const f32* src, f32 out[3]) {
    Quat q{src[0], src[1], src[2], src[3]};
    q.normalize();
    f32 ex = 0.0f, ey = 0.0f, ez = 0.0f;
    Quat::toEuler(q, ex, ey, ez);
    out[0] = rad2deg(ex);
    out[1] = rad2deg(ey);
    out[2] = rad2deg(ez);
}

} // namespace

// ---- 0.11-S (FASE 3): CUBICSPLINE glTF → keys BEZIER da casa --------------
// O glTF CUBICSPLINE é uma hermite por canal: v(t) = h00·p0 + h10·(dt·m0)
// + h01·p1 + h11·(dt·n1), com m0 = tangente OUT da key k e n1 = tangente IN
// da key k+1 (VALORES ABSOLUTOS por segundo). A bezier da casa avalia
// B(u) com as bases de Bernstein — e 3·(1−u)²u = 3·h10 e 3·(1−u)u² = −3·h11,
// logo a MESMA curva sai com:
//   tanOut[k]   = +dt(k,k+1)·m0[k]   / 3   (o offset de valor da casa)
//   tanIn[k+1]  = −dt(k,k+1)·n1[k+1] / 3
// EXATO para translation/scale (mesmo espaço, mesma unidade). Para rotação
// os tangentes glTF são derivadas de QUAT — convertidas por diferença
// finita em euler (q+m·h → euler, h=0.1 s) — APROXIMAÇÃO documentada (as
// keys continuam euler graus, o formato da casa; o arco intermédio pode
// divergir levemente do hermite de quats — o pin da FASE 25 aferre o
// resultado no device).
void gltfCubicToKeys(const std::vector<f32>& times,
                     const std::vector<f32>& values, u32 comps, bool rot,
                     std::vector<AnimKey>& out) {
    out.clear();
    const size_t n = times.size();
    if (n == 0 || values.size() < n * 3 * comps) {
        return;
    }
    out.resize(n);
    // tangentes em "euler graus/segundo" (rot) ou valor/segundo (pos/escala)
    std::vector<f32> mIn(n * 4), mOut(n * 4);
    for (size_t k = 0; k < n; ++k) {
        const f32* vin = &values[(k * 3 + 0) * comps];
        const f32* val = &values[(k * 3 + 1) * comps];
        const f32* vout = &values[(k * 3 + 2) * comps];
        AnimKey& key = out[k];
        key.t = times[k] < 0.0f ? 0.0f : times[k];
        if (rot) {
            // valores: quat → euler graus (a MESMA conversão das keys lineares)
            f32 e[3];
            quatToEulerDeg(val, e);
            key.v[0] = e[0]; key.v[1] = e[1]; key.v[2] = e[2];
            // tangentes: diferença finita em euler — o quat perturbado pela
            // tangente (q + m·h, h=0.1 s) volta a euler; a diferença / h É a
            // derivada euler do canal (graus/segundo)
            constexpr f32 h = 0.1f;
            for (int pass = 0; pass < 2; ++pass) {
                const f32* vm = pass == 0 ? vin : vout;
                Quat q{val[0] + vm[0] * h, val[1] + vm[1] * h,
                       val[2] + vm[2] * h, val[3] + vm[3] * h};
                q.normalize();
                f32 em[3];
                const f32 qd[4] = {q.x, q.y, q.z, q.w};
                quatToEulerDeg(qd, em);
                f32 e0[3];
                quatToEulerDeg(val, e0);
                f32* dst = pass == 0 ? &mIn[k * 4] : &mOut[k * 4];
                for (int c = 0; c < 3; ++c) {
                    dst[c] = (em[c] - e0[c]) / h;
                }
            }
        } else {
            for (u32 c = 0; c < comps && c < 4; ++c) {
                key.v[c] = val[c];
                mIn[k * 4 + c] = vin[c];
                mOut[k * 4 + c] = vout[c];
            }
        }
    }
    // offsets hermite→bezier por SEGMENTO (a casa guarda o handle NA key —
    // out segue o segmento SEGUINTE, in o ANTERIOR; extremos ficam a 0)
    for (size_t k = 0; k < n; ++k) {
        AnimKey& key = out[k];
        if (k + 1 < n) {
            const f32 dt = times[k + 1] - times[k];
            for (int c = 0; c < 4; ++c) {
                key.tanOut[c] = dt * mOut[k * 4 + c] / 3.0f;
            }
        }
        if (k > 0) {
            const f32 dt = times[k] - times[k - 1];
            for (int c = 0; c < 4; ++c) {
                key.tanIn[c] = -dt * mIn[k * 4 + c] / 3.0f;
            }
        }
    }
}

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
            // 0.11-S: o layout dos values depende da interpolação (o cubic
            // carrega [in, valor, out] ×3 por key)
            const size_t keysz =
                s.interp == GltfAnimSampler::kInterpCubic
                    ? 3u
                    : 1u;
            if (s.times.empty() ||
                s.values.size() < s.times.size() * keysz * s.components) {
                continue;
            }
            // a curva da casa sai da interpolação glTF (o mapa da FASE 3):
            //   STEP → Step (o valor seguro); LINEAR → Slerp na ROTAÇÃO (o
            //   arco curto) e Linear no resto; CUBICSPLINE → Bezier com as
            //   tangentes convertidas (gltfCubicToKeys — hermite→bezier)
            const bool isRot = targetKind == AnimTarget::TicRot ||
                               targetKind == AnimTarget::JointRot;
            std::vector<AnimKey> cubicKeys;
            if (s.interp == GltfAnimSampler::kInterpCubic) {
                gltfCubicToKeys(s.times, s.values, s.components, isRot,
                                cubicKeys);
                if (cubicKeys.empty()) {
                    continue;   // layout cubico malformado — canal fora
                }
            }
            AnimTrack tr;
            tr.curve =
                s.interp == GltfAnimSampler::kInterpStep
                    ? AnimCurve::Step
                    : (s.interp == GltfAnimSampler::kInterpCubic
                           ? AnimCurve::Bezier
                           : (isRot ? AnimCurve::Slerp : AnimCurve::Linear));
            tr.target = targetKind;
            tr.element = jointName;   // vazio nos tracks do TIC
            if (!cubicKeys.empty()) {
                tr.keys = std::move(cubicKeys);
            } else {
                for (size_t k = 0; k < s.times.size(); ++k) {
                    AnimKey key;
                    key.t = s.times[k] < 0.0f ? 0.0f : s.times[k];
                    const f32* src = &s.values[k * s.components];
                    if (isRot) {
                        // quat (x,y,z,w) → euler GRAUS YXZ (a convenção do player)
                        f32 e[3];
                        quatToEulerDeg(src, e);
                        key.v[0] = e[0];
                        key.v[1] = e[1];
                        key.v[2] = e[2];
                    } else {
                        key.v[0] = src[0];
                        key.v[1] = src[1];
                        key.v[2] = src[2];
                    }
                    tr.keys.push_back(key);
                }
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
    // 0.11-S (FASE 3): o cap do UBO é DURO — skin acima de 128 joints NÃO
    // entra (uma skin truncada deixaria joints congelados e vértices a
    // referenciar índices fora do array no shader: personagem deformado +
    // UB no device). O mesh desenha ESTÁTICO (uSkin=0) e o log diz o porquê
    // + o Plan B (palette de textura) documentado no RELATORIO-0.11-S §4.
    if (src.joints.size() > SkeletonComp::kMaxBones) {
        elog::warn(
            "gltf: skin com %zu joints passa o cap de %u do UBO de skinning "
            "— o mesh desenha estático (sem pose); o Plan B (palette de "
            "textura) está documentado no RELATORIO-0.11-S §4",
            src.joints.size(), SkeletonComp::kMaxBones);
        return 0;
    }
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
