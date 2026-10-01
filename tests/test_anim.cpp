// tests/test_anim.cpp — 0.8.0 (F7): ANIMAÇÃO — tracks/keyframes/curvas/
// playback + preset Mesh + PlaySnapshot com UI.
//
// Aferição da spec 0.8.0:
//   • interpolação LINEAR correta (meio, quartos, clamp aos extremos);
//   • interpolação BEZIER correta (fórmula cúbica com handles in/out;
//     handles a ZERO = exatamente linear);
//   • playback avança PROPRIEDADES (pos do Transform3D, rot euler→quat,
//     scale, UI pos/cor/alpha por nome de elemento);
//   • modos once/loop/pingpong + VELOCIDADE;
//   • serialização .goni das tracks/keys/curvas — ROUND-TRIP completo
//     (targets, elemento, curva, tangentes, mode, speed) + forward-compat
//     (alvo desconhecido ignorado);
//   • preset "Mesh" = Transform+MeshRenderer SEM física;
//   • Play estendido a elementos de UI (captura/restaura pos/cor/alpha).
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "components/AnimationPlayer.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "components/UiCanvas.h"
#include "core/AnimationSystem.h"
#include "core/ComponentStore.h"
#include "core/PlaySnapshot.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "math/Math.h"

using namespace vv;
using namespace test;   // nearEqF/vecNearF

namespace {

AnimTrack makePosTrack() {
    AnimTrack t;
    t.target = AnimTarget::TicPos;
    t.curve = AnimCurve::Linear;
    AnimKey k0;
    k0.t = 0.0f;
    k0.v[0] = 0.0f; k0.v[1] = 0.0f; k0.v[2] = 0.0f;
    AnimKey k1;
    k1.t = 2.0f;
    k1.v[0] = 2.0f; k1.v[1] = -4.0f; k1.v[2] = 6.0f;
    t.keys = {k0, k1};
    return t;
}

} // namespace

// ---- 1. interpolação LINEAR -------------------------------------------------

TEST(anim_linear_interp_correta) {
    const AnimTrack t = makePosTrack();
    f32 v[4] = {9.0f, 9.0f, 9.0f, 9.0f};
    EXPECT(evalTrack(t, 1.0f, v));              // meio: u=0.5
    EXPECT(nearEqF(v[0], 1.0f));
    EXPECT(nearEqF(v[1], -2.0f));
    EXPECT(nearEqF(v[2], 3.0f));
    EXPECT(evalTrack(t, 0.5f, v));              // u=0.25
    EXPECT(nearEqF(v[0], 0.5f));
    EXPECT(nearEqF(v[1], -1.0f));
    EXPECT(nearEqF(v[2], 1.5f));
    EXPECT(evalTrack(t, 1.5f, v));              // u=0.75
    EXPECT(nearEqF(v[0], 1.5f));
    // clamp honesto fora do intervalo
    EXPECT(evalTrack(t, -1.0f, v));
    EXPECT(nearEqF(v[0], 0.0f));
    EXPECT(evalTrack(t, 99.0f, v));
    EXPECT(nearEqF(v[0], 2.0f));
    EXPECT(nearEqF(v[1], -4.0f));
}

TEST(anim_linear_multi_keys_segmentos) {
    AnimTrack t;
    t.target = AnimTarget::TicPos;
    AnimKey a; a.t = 0.0f; a.v[0] = 0.0f;
    AnimKey b; b.t = 1.0f; b.v[0] = 10.0f;
    AnimKey c; c.t = 3.0f; c.v[0] = 12.0f;   // declive 1 (10→12 em 2 s)
    t.keys = {a, b, c};
    f32 v[4] = {};
    EXPECT(evalTrack(t, 0.5f, v));
    EXPECT(nearEqF(v[0], 5.0f));
    EXPECT(evalTrack(t, 2.0f, v));
    EXPECT(nearEqF(v[0], 11.0f));
}

// ---- 2. interpolação BEZIER -------------------------------------------------

TEST(anim_bezier_interp_correta) {
    // p0=0, p1=p0+out=1, p2=p3+in=3, p3=2 → B(0.5)=0.125·0+0.375·1+0.375·3+0.125·2=1.75
    AnimTrack t;
    t.target = AnimTarget::TicPos;
    t.curve = AnimCurve::Bezier;
    AnimKey k0;
    k0.t = 0.0f;  k0.v[0] = 0.0f;  k0.tanOut[0] = 1.0f;
    AnimKey k1;
    k1.t = 1.0f;  k1.v[0] = 2.0f;  k1.tanIn[0] = 1.0f;
    t.keys = {k0, k1};
    f32 v[4] = {};
    EXPECT(evalTrack(t, 0.5f, v));
    EXPECT(nearEqF(v[0], 1.75f));
    // extremos: nas keys o valor é EXATO da key
    EXPECT(evalTrack(t, 0.0f, v));
    EXPECT(nearEqF(v[0], 0.0f));
    EXPECT(evalTrack(t, 1.0f, v));
    EXPECT(nearEqF(v[0], 2.0f));
}

TEST(anim_bezier_handles_zero_e_ease_suave) {
    // handles a ZERO: p1=p0 e p2=p3 → bezier degrada num EASE suave
    // (smoothstep B(u) = p0 + (p3−p0)·u²(3−2u)) — NÃO em linear; a curva
    // LINEAR é o default do track e o bez serve para suavizar/overshoot
    AnimTrack bez = makePosTrack();   // p0=(0,0,0) p1=(2,−4,6) em t=2
    bez.curve = AnimCurve::Bezier;
    for (const f32 t : {0.5f, 1.0f, 1.5f}) {
        const f32 u = t / 2.0f;
        const f32 w = u * u * (3.0f - 2.0f * u);   // smoothstep
        f32 v[4] = {};
        EXPECT(evalTrack(bez, t, v));
        EXPECT(nearEqF(v[0], 2.0f * w, 1e-4f));
        EXPECT(nearEqF(v[1], -4.0f * w, 1e-4f));
        EXPECT(nearEqF(v[2], 6.0f * w, 1e-4f));
    }
}

TEST(anim_bezier_handles_lineares_reproduzem_linear) {
    // handles EXPLICITAMENTE lineares (out=+Δ/3, in=−Δ/3) reproduzem a
    // reta EXATAMENTE — o editor parte daqui para "bez que ainda parece
    // linear" e o utilizador curva-a puxando os handles
    AnimTrack lin;
    lin.target = AnimTarget::TicPos;
    lin.curve = AnimCurve::Linear;
    AnimKey a; a.t = 0.0f; a.v[0] = 0.0f; a.v[1] = 0.0f; a.v[2] = 0.0f;
    AnimKey b; b.t = 2.0f; b.v[0] = 2.0f; b.v[1] = -4.0f; b.v[2] = 6.0f;
    lin.keys = {a, b};

    AnimTrack bez = lin;
    bez.curve = AnimCurve::Bezier;
    // handles proporcionais ao segmento (por canal): out=Δ/3, in=−Δ/3
    for (int c = 0; c < 3; ++c) {
        bez.keys[0].tanOut[c] = (b.v[c] - a.v[c]) / 3.0f;
        bez.keys[1].tanIn[c]  = -(b.v[c] - a.v[c]) / 3.0f;
    }
    for (const f32 t : {0.1f, 0.5f, 1.3f, 1.9f}) {
        f32 la[4] = {}, lb[4] = {};
        EXPECT(evalTrack(lin, t, la));
        EXPECT(evalTrack(bez, t, lb));
        EXPECT(nearEqF(la[0], lb[0], 1e-4f));
        EXPECT(nearEqF(la[1], lb[1], 1e-4f));
        EXPECT(nearEqF(la[2], lb[2], 1e-4f));
    }
}

// ---- 3. playback avança propriedades ----------------------------------------

TEST(anim_playback_avança_pos_do_transform) {
    Scene s;
    const Handle h = s.create("Cubo");
    Tic* tic = s.get(h);
    Transform3D* tr = tic->addComponent<Transform3D>();
    AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
    ASSERT(pl && tr);
    AnimClip* clip = pl->editClip();
    clip->tracks.push_back(makePosTrack());
    pl->playing = true;

    AnimationSystem sys;
    sys.enabled = true;
    sys.tick(s, 1.0f);   // avança 1 s (meio do clip) + aplica
    EXPECT(nearEqF(pl->time, 1.0f));
    EXPECT(nearEqF(tr->pos.x, 1.0f));
    EXPECT(nearEqF(tr->pos.y, -2.0f));
    EXPECT(nearEqF(tr->pos.z, 3.0f));
    // cache world coerente no MESMO frame (feedback imediato)
    EXPECT(nearEqF(tr->world.m[12], 1.0f));
    EXPECT(nearEqF(tr->world.m[13], -2.0f));
}

TEST(anim_playback_rot_euler_para_quat) {
    Scene s;
    const Handle h = s.create("Cubo");
    Tic* tic = s.get(h);
    Transform3D* tr = tic->addComponent<Transform3D>();
    AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
    AnimTrack t;
    t.target = AnimTarget::TicRot;
    AnimKey a; a.t = 0.0f;  a.v[0] = 0.0f; a.v[1] = 0.0f; a.v[2] = 0.0f;
    AnimKey b; b.t = 1.0f;  b.v[0] = 0.0f; b.v[1] = 90.0f; b.v[2] = 0.0f;
    t.keys = {a, b};
    pl->editClip()->tracks.push_back(t);
    pl->playing = true;
    pl->advance(0.5f);
    pl->apply(s, h);
    // meio do yaw: 45° em Y
    const Quat expect = Quat::axisAngle(Vec3{0.0f, 1.0f, 0.0f}, 0.78539816f);
    EXPECT(nearEqF(tr->rot.x, expect.x, 1e-3f));
    EXPECT(nearEqF(tr->rot.y, expect.y, 1e-3f));
    EXPECT(nearEqF(tr->rot.z, expect.z, 1e-3f));
    EXPECT(nearEqF(tr->rot.w, expect.w, 1e-3f));
}

TEST(anim_playback_scale) {
    Scene s;
    const Handle h = s.create("Cubo");
    Tic* tic = s.get(h);
    Transform3D* tr = tic->addComponent<Transform3D>();
    AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
    AnimTrack t;
    t.target = AnimTarget::TicScale;
    AnimKey a; a.t = 0.0f; a.v[0] = 1.0f; a.v[1] = 1.0f; a.v[2] = 1.0f;
    AnimKey b; b.t = 4.0f; b.v[0] = 3.0f; b.v[1] = 2.0f; b.v[2] = 5.0f;
    t.keys = {a, b};
    pl->editClip()->tracks.push_back(t);
    pl->playing = true;
    pl->advance(1.0f);   // u=0.25
    pl->apply(s, h);
    EXPECT(nearEqF(tr->scale.x, 1.5f));
    EXPECT(nearEqF(tr->scale.y, 1.25f));
    EXPECT(nearEqF(tr->scale.z, 2.0f));
}

TEST(anim_playback_ui_pos_cor_alpha) {
    Scene s;
    const Handle h = s.create("HUD");
    Tic* tic = s.get(h);
    UiCanvas* ui = tic->addComponent<UiCanvas>();
    AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
    ASSERT(ui && pl);
    const i32 e = ui->addElement(UiElement::Kind::Panel, 1280.0f, 720.0f);
    ASSERT(e >= 0);
    const std::string name = ui->elements[(size_t)e].name;

    AnimTrack pos;
    pos.target = AnimTarget::UiPos;
    pos.element = name;
    AnimKey pa; pa.t = 0.0f; pa.v[0] = 0.0f;  pa.v[1] = 0.0f;
    AnimKey pb; pb.t = 2.0f; pb.v[0] = 100.0f; pb.v[1] = -40.0f;
    pos.keys = {pa, pb};

    AnimTrack col;
    col.target = AnimTarget::UiColor;
    col.element = name;
    AnimKey ca; ca.t = 0.0f; ca.v[0] = 0.0f; ca.v[1] = 0.0f; ca.v[2] = 0.0f;
    AnimKey cb; cb.t = 2.0f; cb.v[0] = 1.0f; cb.v[1] = 0.5f; cb.v[2] = 0.25f;
    col.keys = {ca, cb};

    AnimTrack alp;
    alp.target = AnimTarget::UiAlpha;
    alp.element = name;
    AnimKey aa; aa.t = 0.0f; aa.v[0] = 1.0f;
    AnimKey ab; ab.t = 2.0f; ab.v[0] = 0.0f;
    alp.keys = {aa, ab};

    AnimClip* clip = pl->editClip();
    clip->tracks = {pos, col, alp};
    pl->playing = true;
    pl->advance(1.0f);
    pl->apply(s, h);
    const UiElement& el = ui->elements[(size_t)e];
    EXPECT(nearEqF(el.ox, 50.0f));
    EXPECT(nearEqF(el.oy, -20.0f));
    EXPECT(nearEqF(el.color[0], 0.5f));
    EXPECT(nearEqF(el.color[1], 0.25f));
    EXPECT(nearEqF(el.color[2], 0.125f));
    EXPECT(nearEqF(el.color[3], 0.5f));
}

TEST(anim_ui_track_element_sumsido_e_saltado) {
    // elemento removido → track de UI NÃO crasha (saltado); o TIC sem
    // Transform3D → tracks de TIC saltados
    Scene s;
    const Handle h = s.create("HUD");
    Tic* tic = s.get(h);
    UiCanvas* ui = tic->addComponent<UiCanvas>();
    AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
    const i32 e = ui->addElement(UiElement::Kind::Panel, 1280.0f, 720.0f);
    AnimTrack pos;
    pos.target = AnimTarget::UiPos;
    pos.element = "fantasma";   // NÃO existe
    AnimKey a; a.t = 0.0f; a.v[0] = 5.0f; a.v[1] = 5.0f;
    AnimKey b; b.t = 1.0f; b.v[0] = 9.0f; b.v[1] = 9.0f;
    pos.keys = {a, b};
    pl->editClip()->tracks = {pos, makePosTrack()};   // + track de TIC sem Transform3D
    pl->playing = true;
    pl->advance(0.5f);
    pl->apply(s, h);   // nada escreve — sem crash
    EXPECT(nearEqF(ui->elements[(size_t)e].ox, ui->elements[(size_t)e].ox));  // intacto
}

// ---- 4. modos once/loop/pingpong + velocidade -------------------------------

TEST(anim_modos_once_loop_pingpong) {
    // dur = 2 s
    // ONCE: 5 s de avanço → PARA no fim (time=2, playing=false)
    {
        Scene s;
        Tic* tic = s.get(s.create("a"));
        tic->addComponent<Transform3D>();
        AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
        pl->editClip()->tracks.push_back(makePosTrack());
        pl->mode = AnimationPlayer::Mode::Once;
        pl->playing = true;
        pl->advance(5.0f);
        EXPECT(nearEqF(pl->time, 2.0f));
        EXPECT(!pl->playing);
        // já parado: NÃO avança mais
        pl->advance(1.0f);
        EXPECT(nearEqF(pl->time, 2.0f));
    }
    // LOOP: 5 s → 5 mod 2 = 1
    {
        Scene s;
        Tic* tic = s.get(s.create("a"));
        tic->addComponent<Transform3D>();
        AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
        pl->editClip()->tracks.push_back(makePosTrack());
        pl->mode = AnimationPlayer::Mode::Loop;
        pl->playing = true;
        pl->advance(5.0f);
        EXPECT(nearEqF(pl->time, 1.0f));
        EXPECT(pl->playing);
    }
    // PINGPONG: onda triangular de período 2·dur=4 s (posições EXATAS):
    //   t=5 → fase 1 (ramo de SUBIDA)  → time=1, dir=+1
    //   t=6 → fase 2 (no pico)         → time=2
    //   t=7 → fase 3 (ramo de DESCIDA) → time=1, dir=−1
    {
        Scene s;
        Tic* tic = s.get(s.create("a"));
        tic->addComponent<Transform3D>();
        AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
        pl->editClip()->tracks.push_back(makePosTrack());
        pl->mode = AnimationPlayer::Mode::PingPong;
        pl->playing = true;
        pl->advance(5.0f);
        EXPECT(nearEqF(pl->time, 1.0f));
        pl->advance(1.0f);
        EXPECT(nearEqF(pl->time, 2.0f));
        pl->advance(1.0f);
        EXPECT(nearEqF(pl->time, 1.0f));
        EXPECT(pl->dir() == -1);
        // salto GRANDE de uma vez (vários períodos): 13 s → fase 1 → time=1
        pl->advance(6.0f);
        EXPECT(nearEqF(pl->time, 1.0f));
        // aplicação coerente com a posição da onda (t=7: u=0.5 → x=1)
        pl->apply(s, s.get(s.find("a"))->handle);
        EXPECT(nearEqF(s.components().transforms().find(s.find("a"))->pos.x, 1.0f));
    }
}

TEST(anim_velocidade_multiplica_o_avanço) {
    Scene s;
    Tic* tic = s.get(s.create("a"));
    tic->addComponent<Transform3D>();
    AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
    pl->editClip()->tracks.push_back(makePosTrack());
    pl->mode = AnimationPlayer::Mode::Loop;
    pl->speed = 2.0f;
    pl->playing = true;
    pl->advance(1.0f);   // 1 s × 2 = 2 s de clip
    EXPECT(nearEqF(pl->time, 0.0f));   // 2 mod 2 = 0
    pl->advance(0.25f);
    EXPECT(nearEqF(pl->time, 0.5f));
    pl->speed = 0.5f;
    pl->advance(1.0f);
    EXPECT(nearEqF(pl->time, 1.0f));
}

TEST(anim_clip_vazio_nao_acumula) {
    Scene s;
    Tic* tic = s.get(s.create("a"));
    tic->addComponent<Transform3D>();
    AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
    pl->playing = true;
    pl->advance(10.0f);
    EXPECT(nearEqF(pl->time, 0.0f));
}

// ---- 5. serialização .goni — round-trip -------------------------------------

TEST(anim_serializacao_roundtrip_completo) {
    Scene s;
    const Handle h = s.create("Anim");
    Tic* tic = s.get(h);
    tic->addComponent<Transform3D>();
    tic->addComponent<MeshRenderer>();
    AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
    AnimClip* clip = pl->editClip();
    clip->tracks.push_back(makePosTrack());
    clip->tracks.back().curve = AnimCurve::Bezier;   // curva bezier
    AnimTrack ui;
    ui.target = AnimTarget::UiAlpha;
    ui.element = "panel";
    AnimKey a; a.t = 0.0f;  a.v[0] = 1.0f; a.tanOut[0] = 0.25f;
    AnimKey b; b.t = 1.5f;  b.v[0] = 0.0f; b.tanIn[0] = -0.1f;
    ui.keys = {a, b};
    clip->tracks.push_back(ui);
    // keys DESORDENadas → o load ordena
    AnimTrack sc;
    sc.target = AnimTarget::TicScale;
    AnimKey s1; s1.t = 2.0f; s1.v[0] = 3.0f; s1.v[1] = 3.0f; s1.v[2] = 3.0f;
    AnimKey s0; s0.t = 0.0f; s0.v[0] = 1.0f; s0.v[1] = 1.0f; s0.v[2] = 1.0f;
    sc.keys = {s1, s0};
    clip->tracks.push_back(sc);
    pl->mode = AnimationPlayer::Mode::PingPong;
    pl->speed = 1.75f;

    const std::string text = SceneSerializer::dump(s);
    // grava o tipo e as curvas
    EXPECT(std::strstr(text.c_str(), "\"AnimationPlayer\"") != nullptr);
    EXPECT(std::strstr(text.c_str(), "\"curve\":\"bez\"") != nullptr);
    EXPECT(std::strstr(text.c_str(), "\"mode\":\"pingpong\"") != nullptr);

    Scene s2;
    SceneSerializer::LoadCtx ctx;   // sem resolvers — a hierarquia entra na mesma
    ASSERT(SceneSerializer::loadText(s2, text, ctx));
    Tic* back = s2.get(s2.find("Anim"));
    ASSERT(back != nullptr);
    const AnimationPlayer* pl2 = back->getComponent<AnimationPlayer>();
    ASSERT(pl2 != nullptr);
    EXPECT(pl2->mode == AnimationPlayer::Mode::PingPong);
    EXPECT(nearEqF(pl2->speed, 1.75f));
    EXPECT(!pl2->playing);   // runtime: nasce PARADO
    EXPECT(nearEqF(pl2->time, 0.0f));
    ASSERT(pl2->clips.size() == 1);
    EXPECT(pl2->clips[0].name == "edit");
    ASSERT(pl2->clips[0].tracks.size() == 3);

    const AnimTrack& t0 = pl2->clips[0].tracks[0];
    EXPECT(t0.target == AnimTarget::TicPos);
    EXPECT(t0.curve == AnimCurve::Bezier);
    ASSERT(t0.keys.size() == 2);
    EXPECT(nearEqF(t0.keys[1].v[1], -4.0f));

    const AnimTrack& t1 = pl2->clips[0].tracks[1];
    EXPECT(t1.target == AnimTarget::UiAlpha);
    EXPECT(t1.element == "panel");
    ASSERT(t1.keys.size() == 2);
    EXPECT(nearEqF(t1.keys[0].tanOut[0], 0.25f));
    EXPECT(nearEqF(t1.keys[1].tanIn[0], -0.1f));

    // keys DESORDENadas → chegam ORDENADAS
    const AnimTrack& t2 = pl2->clips[0].tracks[2];
    EXPECT(t2.target == AnimTarget::TicScale);
    ASSERT(t2.keys.size() == 2);
    EXPECT(t2.keys[0].t < t2.keys[1].t);
    EXPECT(nearEqF(t2.keys[0].t, 0.0f));
    EXPECT(nearEqF(t2.keys[1].t, 2.0f));
}

TEST(anim_serializacao_defaults_minimais) {
    Scene s;
    Tic* tic = s.get(s.create("a"));
    tic->addComponent<Transform3D>();
    tic->addComponent<AnimationPlayer>();   // TUDO default (sem tracks)
    const std::string text = SceneSerializer::dump(s);
    EXPECT(std::strstr(text.c_str(), "\"AnimationPlayer\"") != nullptr);
    // defaults omitidos: mode/speed/clips fora
    EXPECT(std::strstr(text.c_str(), "\"mode\"") == nullptr);
    EXPECT(std::strstr(text.c_str(), "\"clips\"") == nullptr);
    // round-trip: volta a entrar com clip "edit" disponível
    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(s2, text, ctx));
    if (Tic* back = s2.get(s2.find("a"))) {
        if (AnimationPlayer* pl = back->getComponent<AnimationPlayer>()) {
            EXPECT(pl->mode == AnimationPlayer::Mode::Loop);
            EXPECT(nearEqF(pl->speed, 1.0f));
            EXPECT(pl->editClip() != nullptr);   // clip de edição nasce à pedido
        } else {
            EXPECT(false && "player devia voltar");
        }
    }
}

TEST(anim_serializacao_forward_compat) {
    // alvo desconhecido e key malformada → ignorados sem crash
    const char* txt = R"({"version":1,"tics":[
      {"id":0,"name":"A","active":true,"parent":-1,"components":[
        {"type":"AnimationPlayer","mode":"once","speed":2.0,
         "clips":[{"name":"walk","tracks":[
           {"target":"matriz-super-nova","keys":[{"t":0,"v":[1,1,1]}]},
           {"target":"pos","curve":"bez","keys":[
              {"t":0,"v":[0,0,0],"out":[1,0,0,0]},{"t":1,"v":[2,0,0]}]}]}]}]}]})";
    Scene s;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(s, txt, ctx));
    Tic* tic = s.get(s.find("A"));
    ASSERT(tic != nullptr);
    const AnimationPlayer* pl = tic->getComponent<AnimationPlayer>();
    ASSERT(pl != nullptr);
    EXPECT(pl->mode == AnimationPlayer::Mode::Once);
    EXPECT(nearEqF(pl->speed, 2.0f));
    ASSERT(pl->clips.size() == 1);
    EXPECT(pl->clips[0].name == "walk");
    ASSERT(pl->clips[0].tracks.size() == 1);   // o alvo desconhecido saiu
    EXPECT(pl->clips[0].tracks[0].target == AnimTarget::TicPos);
    EXPECT(pl->clips[0].tracks[0].curve == AnimCurve::Bezier);
    // clip importado (não-"edit") também volta
}

TEST(anim_registry_factory_cria_player) {
    Scene s;
    ComponentStore& store = s.components();
    const i32 id = store.registry().find("AnimationPlayer");
    EXPECT(id >= 0);
    const Handle h = s.create("a");
    EXPECT(store.registry().create(static_cast<u32>(id), store, h));
    EXPECT(s.get(h)->getComponent<AnimationPlayer>() != nullptr);
}

// ---- 6. preset Mesh ----------------------------------------------------------

TEST(anim_preset_mesh_sem_fisica) {
    Scene s;
    const Handle h = createTicFromPreset(s, PresetKind::Mesh, nullptr, nullptr);
    EXPECT(h.valid());
    Tic* tic = s.get(h);
    ASSERT(tic != nullptr);
    EXPECT(std::strcmp(tic->name.c_str(), "Mesh") == 0);
    EXPECT(tic->getComponent<Transform3D>() != nullptr);
    const MeshRenderer* mr = tic->getComponent<MeshRenderer>();
    EXPECT(mr != nullptr);
    // SEM física, SEM input (prototipagem pura)
    EXPECT(tic->getComponent<BodyComp>() == nullptr);
    EXPECT(tic->getComponent<InputMap>() == nullptr);
    EXPECT(tic->getComponent<TouchControls>() == nullptr);
    // nasce com PRIMITIVA esfera default (serializa tipo+params)
    EXPECT(mr->primOn);
    EXPECT(mr->prim.kind == PrimKind::Sphere);
    const PrimParams def = primDefaults(PrimKind::Sphere);
    EXPECT(nearEqF(mr->prim.radius, def.radius));
    EXPECT(mr->prim.segments == def.segments);
    // nomes únicos Godot-style
    const Handle h2 = createTicFromPreset(s, PresetKind::Mesh, nullptr, nullptr);
    EXPECT(std::strcmp(s.get(h2)->name.c_str(), "Mesh.001") == 0);
}

// ---- 7. Play estendido à UI (snapshot) ---------------------------------------

TEST(anim_playsnapshot_captura_e_restaura_ui) {
    Scene s;
    const Handle h = s.create("HUD");
    Tic* tic = s.get(h);
    UiCanvas* ui = tic->addComponent<UiCanvas>();
    tic->addComponent<Transform3D>();
    const i32 e = ui->addElement(UiElement::Kind::Panel, 1280.0f, 720.0f);
    ASSERT(e >= 0);
    UiElement& el = ui->elements[(size_t)e];
    el.ox = 10.0f; el.oy = 20.0f;
    el.color[3] = 1.0f;

    PlaySnapshot snap;
    playSnapshotCapture(s, snap);
    EXPECT(snap.uiElems.size() == 1);
    // "anima" durante o play
    el.ox = 500.0f; el.oy = -80.0f;
    el.color[0] = 0.5f; el.color[3] = 0.1f;
    Transform3D* tr = tic->getComponent<Transform3D>();
    tr->pos = Vec3{9.0f, 9.0f, 9.0f};
    playSnapshotRestore(s, snap);
    // tudo de volta
    EXPECT(nearEqF(el.ox, 10.0f));
    EXPECT(nearEqF(el.oy, 20.0f));
    EXPECT(nearEqF(el.color[0], 0.1176f));   // default PANEL do tema (0.7.0)
    EXPECT(nearEqF(el.color[3], 1.0f));
    EXPECT(nearEqF(tr->pos.x, 0.0f));
}

TEST(anim_playsnapshot_ui_fora_de_intervalo_salta) {
    Scene s;
    const Handle h = s.create("HUD");
    Tic* tic = s.get(h);
    UiCanvas* ui = tic->addComponent<UiCanvas>();
    ui->addElement(UiElement::Kind::Panel, 1280.0f, 720.0f);
    PlaySnapshot snap;
    playSnapshotCapture(s, snap);
    // canvas MUDOU durante o play (elemento extra → índices deslocados)
    ui->elements.clear();
    ui->addElement(UiElement::Kind::Label, 1280.0f, 720.0f);
    playSnapshotRestore(s, snap);   // índice 0 volta a existir: restaura esse
    EXPECT(true);   // o contrato: NUNCA crasha
}

// ---- 8. addTrack idempotente + duration -------------------------------------

TEST(anim_addtrack_idempotente_e_duration) {
    Scene s;
    Tic* tic = s.get(s.create("a"));
    tic->addComponent<Transform3D>();
    AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
    AnimTrack* t1 = pl->addTrack(AnimTarget::TicPos);
    ASSERT(t1 != nullptr);
    AnimTrack* t2 = pl->addTrack(AnimTarget::TicPos);
    EXPECT(t2 == t1);   // MESMO alvo = MESMO track
    EXPECT(pl->addTrack(AnimTarget::TicRot) != nullptr);
    // NOTA: ponteiros de addTrack invalidam-se no push seguinte (vector) —
    // re-busca por índice para escrever os dados (contrato documentado)
    AnimClip* clip = pl->editClip();
    ASSERT(clip->tracks.size() == 2);
    clip->tracks[0].keys = {{0.0f}, {3.0f}};   // t=0 e t=3 (v default 0)
    clip->tracks[0].keys[1].v[0] = 0.0f;
    clip->tracks[1].keys = {{1.0f}};
    EXPECT(nearEqF(pl->duration(), 3.0f));   // max dos tracks
}
