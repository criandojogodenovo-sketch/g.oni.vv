// tests/test_fase3_skeleton.cpp — 0.11-S (FASE 3): ESQUELETO/ANIMAÇÃO —
// as pernas NOVAS da fase (o que já existia da 0.8.x vive no test_skin/
// test_gltfanim/test_anim e continua a correr).
//
//   • Quat::slerp — o arco CURTO (dot<0 inverte) + o fallback nlerp dos
//     quase-paralelos (o sin(θ)→0 não divide por zero);
//   • evalTrack STEP — segura o valor da key à ESQUERDA (o glTF manda);
//   • evalTrack SLERP — a rotação vai pelo arco curto (o euler-linear de
//     350° dava a volta LONGA — o pin aferre o −5°, nunca o 175°);
//   • gltfCubicToKeys — a hermite→bezier EXATA (pos/escala: o evalTrack da
//     casa sobre as keys convertidas reproduz a hermite de referência);
//   • import end-to-end: GLB com STEP+LINEAR(rot)+CUBICSPLINE → as curvas
//     da casa certas por track + o playback do cubic avaliado;
//   • serializer: step/slerp/bez viajam por nome e VOLTAM; ficheiro antigo
//     sem "curve" continua a nascer Linear (byte a byte);
//   • V.ONI stop()/blend() sobre o EngineHost REAL (crossfade completo,
//     erros legíveis sem player/sem clip);
//   • o PAR dos caps (SkeletonComp::kMaxBones == LitMaterial::kMaxBones);
//   • BENCH: 10 personagens × 64 joints × 300 frames (advance+apply+
//     computeSkinMatrices) — o número PRINTADO alimenta a tabela C33 do
//     relatório (host = proxy; o árbitro do device é o sign-off do dono).
#include "TestFramework.h"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "assets/GltfAnim.h"
#include "assets/GltfImporter.h"
#include "components/AnimationPlayer.h"
#include "components/SkeletonComp.h"
#include "components/Transform3D.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "render/Material.h"
#include "voni/VoniEngineHost.h"

using namespace vv;
using test::nearEqF;
using test::vecNearF;

// graus↔radianos (as constantes do player/Math, recriadas aqui p/ os pins)
namespace {
inline f32 deg2rad(f32 d) { return d * 0.01745329252f; }
inline f32 rad2deg(f32 r) { return r * 57.2957795131f; }
} // namespace

namespace {

void pushF32(std::vector<u8>& b, f32 v) {
    u8 tmp[4];
    std::memcpy(tmp, &v, 4);
    b.insert(b.end(), tmp, tmp + 4);
}

void pushU32(std::vector<u8>& b, u32 v) {
    u8 tmp[4];
    std::memcpy(tmp, &v, 4);
    b.insert(b.end(), tmp, tmp + 4);
}

void pushU16(std::vector<u8>& b, u16 v) {
    u8 tmp[2];
    std::memcpy(tmp, &v, 2);
    b.insert(b.end(), tmp, tmp + 2);
}

// GLB com UM triângulo + TRÊS animações no nó 0 ("raiz"):
//   "passo"  — translation STEP: (0,0,0)@0s → (4,0,0)@2s
//   "girar"  — rotation LINEAR: identidade@0s → 90° Y @2s (→ track Slerp)
//   "cubic"  — translation CUBICSPLINE com tangentes REAIS:
//              key0: in(0,0,0) val(0,0,0) out(6,0,0)   [m0 = 6/s]
//              key1: in(3,0,0) val(1,0,0) out(0,0,0)   [n1 = 3/s]
//              hermite em t=0.5: 0.5·0 + 0.125·6 + 0.5·1 + (−0.125)·3
//                               = 0.75 + 0.5 − 0.375 = 0.875
std::vector<u8> curvasGlb() {
    std::vector<u8> bin;
    const u32 timesOff = 0;
    pushF32(bin, 0.0f);
    pushF32(bin, 1.0f);   // dt=1 — o dt dos asserts (0.875/tanOut=2)
    const u32 posOff = static_cast<u32>(bin.size());
    pushF32(bin, 0); pushF32(bin, 0); pushF32(bin, 0);
    pushF32(bin, 4); pushF32(bin, 0); pushF32(bin, 0);         // 24 B
    const u32 quatOff = static_cast<u32>(bin.size());
    pushF32(bin, 0.0f); pushF32(bin, 0.0f); pushF32(bin, 0.0f);
    pushF32(bin, 1.0f);
    const f32 hs = 0.70710678f;
    pushF32(bin, 0.0f); pushF32(bin, hs); pushF32(bin, 0.0f);
    pushF32(bin, hs);                                          // 32 B
    // CUBICSPLINE: 2 keys × 3 blocos × VEC3 = 18 floats
    const u32 cubOff = static_cast<u32>(bin.size());
    const f32 cub[18] = {
        0, 0, 0,   0, 0, 0,   6, 0, 0,     // key 0: in, valor, out
        3, 0, 0,   1, 0, 0,   0, 0, 0,     // key 1: in, valor, out
    };
    for (const f32 f : cub) {
        pushF32(bin, f);
    }
    // triângulo
    const u32 triPosOff = static_cast<u32>(bin.size());
    pushF32(bin, 0); pushF32(bin, 0); pushF32(bin, 0);
    pushF32(bin, 1); pushF32(bin, 0); pushF32(bin, 0);
    pushF32(bin, 0); pushF32(bin, 1); pushF32(bin, 0);
    const u32 triIdxOff = static_cast<u32>(bin.size());
    pushU16(bin, 0); pushU16(bin, 1); pushU16(bin, 2);
    const u32 binLen = static_cast<u32>(bin.size());

    char j[1800];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"byteLength\":%u}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":8},"    // 0 times
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":24},"   // 1 pos
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":32},"   // 2 quat
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":72},"   // 3 cubic
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":36},"   // 4 tri pos
        "{\"buffer\":0,\"byteOffset\":%u,\"byteLength\":6}],"   // 5 tri idx
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":2,\"type\":\"SCALAR\"},"
        "{\"bufferView\":1,\"componentType\":5126,\"count\":2,\"type\":\"VEC3\"},"
        "{\"bufferView\":2,\"componentType\":5126,\"count\":2,\"type\":\"VEC4\"},"
        "{\"bufferView\":3,\"componentType\":5126,\"count\":6,\"type\":\"VEC3\"},"
        "{\"bufferView\":4,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"},"
        "{\"bufferView\":5,\"componentType\":5123,\"count\":3,\"type\":\"SCALAR\"}],"
        "\"meshes\":[{\"primitives\":[{\"attributes\":{\"POSITION\":4},"
        "\"indices\":5}]}],"
        "\"nodes\":[{\"mesh\":0,\"name\":\"raiz\"}],"
        "\"scenes\":[{\"nodes\":[0]}],"
        "\"animations\":["
          "{\"name\":\"passo\",\"channels\":["
            "{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}}],"
           "\"samplers\":[{\"input\":0,\"output\":1,"
             "\"interpolation\":\"STEP\"}]},"
          "{\"name\":\"girar\",\"channels\":["
            "{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"rotation\"}}],"
           "\"samplers\":[{\"input\":0,\"output\":2}]},"
          "{\"name\":\"cubic\",\"channels\":["
            "{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}}],"
           "\"samplers\":[{\"input\":0,\"output\":3,"
             "\"interpolation\":\"CUBICSPLINE\"}]}"
        "]}",
        binLen, timesOff, posOff, quatOff, cubOff, triPosOff, triIdxOff);
    const std::string json(j);

    std::string jsonP = json;
    while (jsonP.size() % 4 != 0) {
        jsonP += ' ';
    }
    std::vector<u8> binP = bin;
    while (binP.size() % 4 != 0) {
        binP.push_back(0);
    }
    std::vector<u8> glb;
    pushU32(glb, 0x46546C67);   // 'glTF'
    pushU32(glb, 2);
    const u32 total = 12 + 8 + static_cast<u32>(jsonP.size()) + 8 +
                      static_cast<u32>(binP.size());
    pushU32(glb, total);
    pushU32(glb, static_cast<u32>(jsonP.size()));
    pushU32(glb, 0x4E4F534A);   // 'JSON'
    glb.insert(glb.end(), jsonP.begin(), jsonP.end());
    pushU32(glb, static_cast<u32>(binP.size()));
    pushU32(glb, 0x004E4942);   // 'BIN'
    glb.insert(glb.end(), binP.begin(), binP.end());
    return glb;
}

} // namespace

// ---- 1. Quat::slerp ----------------------------------------------------------

TEST(fase3_quat_slerp_arco_curto) {
    // 90° Y a meio = 45° Y (o arco certo, valores exatos dos quats)
    const Quat q0 = Quat::identity();
    const Quat q1 = Quat::axisAngle(Vec3{0.0f, 1.0f, 0.0f}, 1.5707963f);
    const Quat mid = Quat::slerp(q0, q1, 0.5f);
    f32 ex = 0, ey = 0, ez = 0;
    Quat::toEuler(mid, ex, ey, ez);
    EXPECT(nearEqF(ex, 0.0f, 1e-4f));
    EXPECT(nearEqF(ey, 0.7853981f, 1e-4f));   // 45° em radianos
    // extremos
    const Quat a0 = Quat::slerp(q0, q1, 0.0f);
    const Quat a1 = Quat::slerp(q0, q1, 1.0f);
    EXPECT(nearEqF(a0.w, q0.w, 1e-5f) && nearEqF(a0.y, q0.y, 1e-5f));
    EXPECT(nearEqF(a1.y, q1.y, 1e-5f) && nearEqF(a1.w, q1.w, 1e-5f));
}

TEST(fase3_quat_slerp_inverte_o_lado_negativo) {
    // q1 = −q0 (o MESMO giro representado no hemisfério oposto): dot<0 →
    // slerp NÃO roda 360° — devolve o q0 de sempre (o arco curto é zero)
    const Quat q0 = Quat::axisAngle(Vec3{0.0f, 1.0f, 0.0f}, 0.6f);
    Quat q1 = q0;
    q1 = Quat{-q1.x, -q1.y, -q1.z, -q1.w};
    const Quat mid = Quat::slerp(q0, q1, 0.5f);
    EXPECT(nearEqF(mid.x, q0.x, 1e-4f));
    EXPECT(nearEqF(mid.y, q0.y, 1e-4f));
    EXPECT(nearEqF(mid.z, q0.z, 1e-4f));
    EXPECT(nearEqF(mid.w, q0.w, 1e-4f));
}

TEST(fase3_quat_slerp_quase_paralelo_nao_divide_por_zero) {
    // dot ≈ 1 → o ramo nlerp (o sin(θ) do denominador morria)
    const Quat q0 = Quat::identity();
    const Quat q1 = Quat::axisAngle(Vec3{0.0f, 1.0f, 0.0f}, 0.0005f);
    const Quat mid = Quat::slerp(q0, q1, 0.5f);
    f32 ex = 0, ey = 0, ez = 0;
    Quat::toEuler(mid, ex, ey, ez);
    EXPECT(nearEqF(ey, 0.00025f, 1e-4f));   // metade do arco pequenino
    EXPECT(mid.norm() > 0.999f);
}

// ---- 2. evalTrack STEP --------------------------------------------------------

TEST(fase3_evaltrack_step_segura_a_esquerda) {
    AnimTrack tr;
    tr.curve = AnimCurve::Step;
    AnimKey k0;
    k0.t = 0.0f;
    k0.v[0] = 1.0f;
    k0.v[1] = 10.0f;
    AnimKey k1;
    k1.t = 1.0f;
    k1.v[0] = 3.0f;
    k1.v[1] = 30.0f;
    tr.keys.push_back(k0);
    tr.keys.push_back(k1);
    f32 out[4] = {};
    ASSERT(evalTrack(tr, 0.0f, out));
    EXPECT(nearEqF(out[0], 1.0f));
    ASSERT(evalTrack(tr, 0.5f, out));
    EXPECT(nearEqF(out[0], 1.0f));   // MEIO do segmento: o valor de k0
    EXPECT(nearEqF(out[1], 10.0f));
    ASSERT(evalTrack(tr, 0.999f, out));
    EXPECT(nearEqF(out[0], 1.0f));   // o valor muda NO instante da k1
    ASSERT(evalTrack(tr, 1.0f, out));
    EXPECT(nearEqF(out[0], 3.0f));   // a partir da k1: o valor dela
    ASSERT(evalTrack(tr, 2.0f, out));
    EXPECT(nearEqF(out[0], 3.0f));   // passou do fim: clamp na última
}

// ---- 3. evalTrack SLERP --------------------------------------------------------

TEST(fase3_evaltrack_slerp_vai_pelo_arco_curto) {
    // 0° → 350°: o euler-linear ia 0→350 (a volta LONGA); o slerp vai
    // 0→−10 (o caminho curto). A meio: −5° (== 355°), NUNCA 175°.
    AnimTrack tr;
    tr.curve = AnimCurve::Slerp;
    AnimKey k0;
    k0.t = 0.0f;
    AnimKey k1;
    k1.t = 1.0f;
    k1.v[1] = 350.0f;   // yaw
    tr.keys.push_back(k0);
    tr.keys.push_back(k1);
    f32 out[4] = {};
    ASSERT(evalTrack(tr, 0.5f, out));
    EXPECT(nearEqF(out[1], -5.0f, 1e-3f));
    // e a conta é a MESMA do evalSlerp exposto (o ramo do evalTrack chama-o)
    f32 ref[4] = {};
    evalSlerp(k0.v, k1.v, 0.5f, ref);
    EXPECT(nearEqF(out[1], ref[1], 1e-5f));
    // contra o Quat::slerp DIRETO (a referência da matemática)
    const Quat q0 = Quat::fromEuler(0.0f, 0.0f, 0.0f);
    const Quat q1 = Quat::fromEuler(0.0f, deg2rad(350.0f), 0.0f);
    const Quat qm = Quat::slerp(q0, q1, 0.5f);
    f32 ex = 0, ey = 0, ez = 0;
    Quat::toEuler(qm, ex, ey, ez);
    EXPECT(nearEqF(out[1], rad2deg(ey), 1e-2f));
}

// ---- 4. gltfCubicToKeys: a hermite→bezier EXATA -------------------------------

TEST(fase3_cubictokeys_reprouz_a_hermite_de_referencia) {
    // o MESMO canal do GLB da fixture: m0=6/s, n1=3/s, v0=0, v1=1, dt=1
    std::vector<f32> times = {0.0f, 1.0f};
    std::vector<f32> values = {
        0, 0, 0,   0, 0, 0,   6, 0, 0,   // key 0: in, val, out
        3, 0, 0,   1, 0, 0,   0, 0, 0,   // key 1: in, val, out
    };
    std::vector<AnimKey> keys;
    gltfCubicToKeys(times, values, 3, false, keys);
    ASSERT(keys.size() == 2);
    EXPECT(nearEqF(keys[0].tanOut[0], 2.0f));    // dt·m0/3 = 1·6/3
    EXPECT(nearEqF(keys[1].tanIn[0], -1.0f));    // −dt·n1/3 = −1·3/3
    EXPECT(keys[0].tanIn[0] == 0.0f);            // extremos ficam a 0
    EXPECT(keys[1].tanOut[0] == 0.0f);
    // hermite de REFERÊNCIA canal a canal (a fórmula da spec glTF)
    const auto hermite = [](f32 t) {
        const f32 h00 = 2 * t * t * t - 3 * t * t + 1;
        const f32 h10 = t * t * t - 2 * t * t + t;
        const f32 h01 = -2 * t * t * t + 3 * t * t;
        const f32 h11 = t * t * t - t * t;
        return h00 * 0.0f + h10 * 6.0f + h01 * 1.0f + h11 * 3.0f;
    };
    // o evalTrack da casa sobre as keys convertidas reproduz a hermite
    AnimTrack tr;
    tr.curve = AnimCurve::Bezier;
    tr.keys = keys;
    for (const f32 t : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
        f32 out[4] = {};
        ASSERT(evalTrack(tr, t, out));
        EXPECT(nearEqF(out[0], hermite(t), 1e-4f));
    }
    // malformado (values curto) → vazio (nunca crasha)
    std::vector<AnimKey> bad;
    gltfCubicToKeys(times, {0.0f, 0.0f, 0.0f}, 3, false, bad);
    EXPECT(bad.empty());
    gltfCubicToKeys({}, values, 3, false, bad);
    EXPECT(bad.empty());
}

// ---- 5. import end-to-end: as curvas glTF chegam CERTAS ------------------------

TEST(fase3_import_curvas_end_to_end) {
    const std::vector<u8> glb = curvasGlb();
    GltfModel model;
    std::string err;
    ASSERT(parseGlb(glb.data(), glb.size(), {}, model, err));
    ASSERT(model.animations.size() == 3);

    Scene s;
    const Handle h = s.create("R");
    Tic* tic = s.get(h);
    tic->addComponent<Transform3D>();
    AnimationPlayer* pl = tic->getComponent<AnimationPlayer>();
    ASSERT(pl == nullptr);
    const u32 added = gltfAttachClips(s, h, model);
    ASSERT(added == 3);
    pl = tic->getComponent<AnimationPlayer>();
    ASSERT(pl != nullptr);

    // clip 0 "passo": STEP (translation)
    ASSERT(pl->clips[0].name == "passo");
    ASSERT(pl->clips[0].tracks.size() == 1);
    EXPECT(pl->clips[0].tracks[0].curve == AnimCurve::Step);
    // clip 1 "girar": rotação LINEAR → track SLERP
    ASSERT(pl->clips[1].name == "girar");
    ASSERT(pl->clips[1].tracks.size() == 1);
    EXPECT(pl->clips[1].tracks[0].curve == AnimCurve::Slerp);
    EXPECT(pl->clips[1].tracks[0].target == AnimTarget::TicRot);
    // clip 2 "cubic": CUBICSPLINE → BEZIER com tangentes convertidas
    ASSERT(pl->clips[2].name == "cubic");
    ASSERT(pl->clips[2].tracks.size() == 1);
    EXPECT(pl->clips[2].tracks[0].curve == AnimCurve::Bezier);
    EXPECT(nearEqF(pl->clips[2].tracks[0].keys[0].tanOut[0], 2.0f));
    EXPECT(nearEqF(pl->clips[2].tracks[0].keys[1].tanIn[0], -1.0f));

    // playback do cubic: a meio o x vale 0.875 (a hermite, não o meio 0.5)
    pl->activeClip = 2;
    pl->playing = true;
    pl->mode = AnimationPlayer::Mode::Once;
    pl->advance(1.0f);   // t=1 (o clip dura 1)
    pl->apply(s, h);
    const Transform3D* tf = tic->getComponent<Transform3D>();
    ASSERT(tf != nullptr);
    EXPECT(nearEqF(tf->pos.x, 1.0f, 1e-3f));   // no fim = o valor da key 1

    // a meio (avança de novo do início com time=0.5 manual)
    pl->time = 0.5f;
    pl->apply(s, h);
    EXPECT(nearEqF(tf->pos.x, 0.875f, 1e-3f));
}

// ---- 6. serializer: as curvas viajam e voltam ----------------------------------

TEST(fase3_serializer_curvas_roundtrip) {
    Scene s;
    const Handle h = s.create("A");
    Tic* tic = s.get(h);
    tic->addComponent<Transform3D>();
    AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
    ASSERT(pl != nullptr);
    AnimClip c;
    c.name = "curvas";
    AnimTrack tStep;
    tStep.target = AnimTarget::TicPos;
    tStep.curve = AnimCurve::Step;
    AnimKey k;
    k.t = 0.0f;
    k.v[0] = 1.0f;
    tStep.keys.push_back(k);
    k.t = 1.0f;
    k.v[0] = 2.0f;
    tStep.keys.push_back(k);
    c.tracks.push_back(tStep);
    AnimTrack tSlerp;
    tSlerp.target = AnimTarget::TicRot;
    tSlerp.curve = AnimCurve::Slerp;
    tSlerp.keys.push_back(k);
    c.tracks.push_back(tSlerp);
    AnimTrack tBez;
    tBez.target = AnimTarget::TicScale;
    tBez.curve = AnimCurve::Bezier;
    tBez.keys.push_back(k);
    tBez.keys[0].tanOut[0] = 0.5f;
    c.tracks.push_back(tBez);
    AnimTrack tLin;
    tLin.target = AnimTarget::TicPos;
    tLin.curve = AnimCurve::Linear;   // o default NÃO viaja
    tLin.keys.push_back(k);
    c.tracks.push_back(tLin);
    pl->clips.push_back(c);

    const std::string text = SceneSerializer::dump(s);
    EXPECT(std::strstr(text.c_str(), "\"step\"") != nullptr);
    EXPECT(std::strstr(text.c_str(), "\"slerp\"") != nullptr);
    EXPECT(std::strstr(text.c_str(), "\"bez\"") != nullptr);

    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(s2, text, ctx));
    Tic* back = s2.get(s2.find("A"));
    ASSERT(back != nullptr);
    const AnimationPlayer* pl2 = back->getComponent<AnimationPlayer>();
    ASSERT(pl2 != nullptr);
    // o loader NÃO cria "edit" — volta EXATAMENTE o que foi gravado
    ASSERT(pl2->clips.size() == 1);
    const AnimClip& rc = pl2->clips[0];
    EXPECT(rc.name == "curvas");
    ASSERT(rc.tracks.size() == 4);
    EXPECT(rc.tracks[0].curve == AnimCurve::Step);
    EXPECT(rc.tracks[1].curve == AnimCurve::Slerp);
    EXPECT(rc.tracks[2].curve == AnimCurve::Bezier);
    EXPECT(nearEqF(rc.tracks[2].keys[0].tanOut[0], 0.5f));
    EXPECT(rc.tracks[3].curve == AnimCurve::Linear);   // ausente = default
}

// ---- 7. V.ONI stop()/blend() sobre o EngineHost REAL ---------------------------

TEST(fase3_voni_stop_e_blend_no_host_real) {
    Scene s;
    const Handle h = s.create("ator");
    Tic* tic = s.get(h);
    tic->addComponent<Transform3D>();
    AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
    ASSERT(pl != nullptr);
    AnimClip walk;
    walk.name = "andar";
    AnimTrack tp;
    tp.target = AnimTarget::TicPos;
    AnimKey k;
    k.t = 0.0f;
    AnimKey k1;
    k1.t = 2.0f;
    k1.v[2] = 4.0f;   // anda 4 m em Z
    tp.keys.push_back(k);
    tp.keys.push_back(k1);
    walk.tracks.push_back(tp);
    AnimClip run = walk;
    run.name = "correr";
    run.tracks[0].keys[1].v[2] = 40.0f;
    pl->clips.push_back(walk);
    pl->clips.push_back(run);

    voni::EngineHost host(s, h, nullptr, nullptr);
    std::string err;

    // IMPORT (o caminho de sempre) e o STOP novo
    ASSERT(host.importAnim("andar", err));
    EXPECT(pl->playing && pl->activeClip == 0);
    ASSERT(host.stopAnim(err));
    EXPECT(!pl->playing);
    EXPECT(nearEqF(pl->time, 0.0f));

    // BLEND: crossfade 0.5 s → ao completar, o "correr" é o ativo com o
    // tempo contínuo (a ACEITAÇÃO da spec: 2 clips transição suave)
    ASSERT(host.importAnim("andar", err));
    ASSERT(host.blendAnim("correr", 0.5f, err));
    EXPECT(pl->blendClip == 1);      // o destino ficou marcado
    pl->advance(1.0f / 60.0f);       // o 1.º frame anima o peso 0→…
    EXPECT(pl->blending());          // (o blending() exige peso > 0)
    for (int i = 0; i < 40; ++i) {   // 41 × 1/60 s ≈ 0.683 s > 0.5 s
        pl->advance(1.0f / 60.0f);
    }
    EXPECT(!pl->blending());
    EXPECT(pl->activeClip == 1);     // o "correr" é o ativo
    // o tempo CONTÍNUO do destino (a pose não salta — já era dele)
    EXPECT(nearEqF(pl->time, 41.0f / 60.0f, 0.02f));

    // erros legíveis (nunca crasha)
    EXPECT(!host.blendAnim("inexistente", 0.3f, err));
    EXPECT(err.find("não encontrada") != std::string::npos);
    ASSERT(host.importAnim("andar", err));
    EXPECT(!host.blendAnim("andar", 0.3f, err));   // ele próprio: recusa
    EXPECT(err.find("já é o ativo") != std::string::npos);

    // TIC sem player: erro que ensina, não crash
    const Handle h2 = s.create("vazio");
    s.get(h2)->addComponent<Transform3D>();
    voni::EngineHost host2(s, h2, nullptr, nullptr);
    EXPECT(!host2.stopAnim(err));
    EXPECT(err.find("não tem AnimationPlayer") != std::string::npos);
    EXPECT(!host2.blendAnim("andar", 0.3f, err));
}

// ---- 8. o PAR dos caps (shader ↔ componente) ------------------------------------

TEST(fase3_caps_casam_128) {
    EXPECT(SkeletonComp::kMaxBones == 128);
    EXPECT(LitMaterial::kMaxBones == 128);
    // o cap da casa manda no compute (o resto da semântica vive no test_skin)
    SkeletonComp sk;
    sk.joints.resize(140);
    Mat4 bones[SkeletonComp::kMaxBones];
    EXPECT(computeSkinMatrices(sk, bones, SkeletonComp::kMaxBones) == 128);
}

// ---- 9. BENCH: 10 personagens × 64 joints × 300 frames ---------------------------

TEST(fase3_bench_10_personagens_x_300_frames) {
    Scene s;
    struct Personagem {
        Handle h;
        SkeletonComp* sk;
        AnimationPlayer* pl;
    };
    std::vector<Personagem> ps;
    for (int i = 0; i < 10; ++i) {
        char nm[16];
        std::snprintf(nm, sizeof(nm), "p%d", i);
        const Handle h = s.create(nm);
        Tic* tic = s.get(h);
        tic->addComponent<Transform3D>();
        SkeletonComp* sk = tic->addComponent<SkeletonComp>();
        ASSERT(sk != nullptr);
        sk->joints.resize(64);   // o rig típico mobile (<60 + folga)
        for (u32 j = 0; j < sk->joints.size(); ++j) {
            SkeletonComp::Joint& jt = sk->joints[j];
            jt.parent = j > 0 ? static_cast<i32>(j) - 1 : -1;
            jt.name = "j" + std::to_string(j);
            jt.pos = Vec3{0.0f, 0.0f, 0.1f};
            jt.rot = Quat::axisAngle(Vec3{0.0f, 1.0f, 0.0f}, 0.02f * (f32)j);
        }
        AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
        ASSERT(pl != nullptr);
        // UM track de rotação por joint (64 tracks × 30 keys) — o pior caso
        // honesto de um personagem animado por clip
        AnimClip clip;
        clip.name = "correr";
        for (u32 j = 0; j < 64; ++j) {
            AnimTrack tr;
            tr.target = AnimTarget::JointRot;
            tr.element = "j" + std::to_string(j);
            tr.curve = AnimCurve::Slerp;
            for (int kk = 0; kk < 30; ++kk) {
                AnimKey key;
                key.t = 0.1f * (f32)kk;
                key.v[0] = 10.0f + (f32)kk;
                key.v[1] = (f32)(kk % 20);
                tr.keys.push_back(key);
            }
            clip.tracks.push_back(tr);
        }
        pl->clips.push_back(clip);
        pl->activeClip = 0;
        pl->playing = true;
        ps.push_back({h, sk, pl});
    }

    const auto t0 = std::chrono::steady_clock::now();
    f32 acc = 0.0f;
    for (int f = 0; f < 300; ++f) {   // 5 s @60 fps
        for (Personagem& p : ps) {
            p.pl->advance(1.0f / 60.0f);
            p.pl->apply(s, p.h);
            Mat4 bones[SkeletonComp::kMaxBones];
            acc += computeSkinMatrices(*p.sk, bones,
                                       SkeletonComp::kMaxBones) > 0
                       ? 1.0f
                       : 0.0f;
        }
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double ms =
        std::chrono::duration<double, std::milli>(t1 - t0).count();
    // o número PRINTADO alimenta a tabela C33 do relatório (o host é
    // proxy — o árbitro do device é o sign-off do dono)
    std::printf("  bench 10 personagens × 64 joints × 300 frames: %.1f ms "
                "(%.3f ms/frame · %.3f ms/personagem/frame · acc=%.0f)\n",
                ms, ms / 300.0, ms / 3000.0, acc);
    // teto GENEROSO (o pin é de sanidade, não de race — o CI partilha CPU):
    // 10 personagens COMPLETOS em menos de 4 s ⇒ sobra orçamento de sobra
    EXPECT(ms < 4000.0);
}
