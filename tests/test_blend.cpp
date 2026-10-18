// tests/test_blend.cpp — 0.8.3 (F7): BLENDING (peso + crossfade).
//
// Aferição da spec 0.8.3:
//   • blend de duas animações com PESO (walk→run): peso 0.5 produz a pose
//     INTERMÉDIA (TIC pos/rot/scale, UI, JOINT);
//   • CROSSFADE suave: o peso anima 0→1 SEM SALTOS (a pose é contínua em
//     todos os passos e na troca de clip o valor NÃO salta);
//   • joint SEM track correspondente fade para o BIND;
//   • blend é RUNTIME: não serializa.
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "components/AnimationPlayer.h"
#include "components/SkeletonComp.h"
#include "components/Transform3D.h"
#include "components/UiCanvas.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "math/Math.h"

using namespace vv;
using test::nearEqF;
using test::vecNearF;

namespace {

// track pos 0→far em 2 s (linear)
AnimTrack posTrack(f32 far) {
    AnimTrack t;
    t.target = AnimTarget::TicPos;
    AnimKey a; a.t = 0.0f; a.v[0] = 0.0f;
    AnimKey b; b.t = 2.0f; b.v[0] = far;
    t.keys = {a, b};
    return t;
}

// player com 2 clips ("walk" 0→8 e "run" 0→16, mesmos alvos)
struct WalkRun {
    Scene s;
    Handle h{};
    Tic* tic = nullptr;
    Transform3D* tr = nullptr;
    AnimationPlayer* pl = nullptr;

    WalkRun() {
        h = s.create("Boneco");
        tic = s.get(h);
        tr = tic->addComponent<Transform3D>();
        pl = tic->addComponent<AnimationPlayer>();
        AnimClip walk;
        walk.name = "walk";
        walk.tracks.push_back(posTrack(8.0f));
        AnimClip run;
        run.name = "run";
        run.tracks.push_back(posTrack(16.0f));
        pl->clips = {walk, run};
        pl->activeClip = 0;
    }
};

} // namespace

// ---- 1. peso 0.5 = pose INTERMÉDIA ---------------------------------------------

TEST(blend_peso_05_pose_intermedia) {
    WalkRun e;
    e.pl->time = 1.0f;              // walk @1s → x=4; run @1s → x=8
    e.pl->setBlend(1, 0.5f);
    EXPECT(e.pl->blending());
    e.pl->apply(e.s, e.h);
    EXPECT(nearEqF(e.tr->pos.x, 6.0f));   // (4+8)/2 — a INTERMÉDIA
}

TEST(blend_pesos_0_e_1_extremos) {
    WalkRun e;
    e.pl->time = 1.0f;
    e.pl->setBlend(1, 0.0f);
    e.pl->apply(e.s, e.h);
    EXPECT(nearEqF(e.tr->pos.x, 4.0f));   // só walk
    e.pl->setBlend(1, 1.0f);
    e.pl->apply(e.s, e.h);
    EXPECT(nearEqF(e.tr->pos.x, 8.0f));   // só run
    // pesos fora do intervalo: clamp
    e.pl->setBlend(1, 9.0f);
    EXPECT(nearEqF(e.pl->blendWeight, 1.0f));
    e.pl->setBlend(1, -3.0f);
    EXPECT(nearEqF(e.pl->blendWeight, 0.0f));
    // clip inválido/ele próprio: ignora sem crash — o estado ANTERIOR
    // mantém-se (blendClip continua a apontar ao alvo válido de antes)
    e.pl->setBlend(99, 0.5f);
    EXPECT(e.pl->blendClip == 1);
    e.pl->setBlend(0, 0.5f);   // ele próprio
    EXPECT(e.pl->blendClip == 1);
}

TEST(blend_o_clip_de_blend_anda_no_seu_tempo) {
    WalkRun e;
    // clip de blend com duração PRÓPRIA (0.5 s): blendTime faz loop dele
    AnimTrack fast;
    fast.target = AnimTarget::TicPos;
    AnimKey a; a.t = 0.0f; a.v[0] = 0.0f;
    AnimKey b; b.t = 0.5f; b.v[0] = 16.0f;
    fast.keys = {a, b};
    e.pl->clips[1].tracks = {fast};   // run: 0→16 em 0.5 s
    e.pl->setBlend(1, 1.0f);          // só o run
    e.pl->playing = true;
    // 1.2 s de avanço: blendTime 1.2 mod 0.5 = 0.2 → x = 6.4
    e.pl->advance(1.2f);
    e.pl->apply(e.s, e.h);
    EXPECT(nearEqF(e.tr->pos.x, 6.4f, 1e-3f));
}

TEST(blend_rot_scale_ui_e_joint) {
    Scene s;
    const Handle h = s.create("Tudo");
    Tic* tic = s.get(h);
    Transform3D* tr = tic->addComponent<Transform3D>();
    UiCanvas* ui = tic->addComponent<UiCanvas>();
    SkeletonComp* sk = tic->addComponent<SkeletonComp>();
    AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
    const i32 el = ui->addElement(UiElement::Kind::Panel, 1280.0f, 720.0f);
    SkeletonComp::Joint j;
    j.name = "braco";
    sk->joints = {j};

    AnimClip a;   // pose "A": rot 0°, escala 1, alpha 1, joint pos (0,0,0)
    a.name = "A";
    {
        AnimTrack rot; rot.target = AnimTarget::TicRot;
        AnimTrack scl; scl.target = AnimTarget::TicScale;
        AnimTrack alp; alp.target = AnimTarget::UiAlpha;
        alp.element = ui->elements[(size_t)el].name;
        AnimTrack jp; jp.target = AnimTarget::JointPos;
        jp.element = "braco";
        AnimKey k; k.t = 0.0f;
        k.v[0] = 0.0f; k.v[1] = 0.0f; k.v[2] = 0.0f;
        rot.keys = {k};   // rot 0°
        k.v[0] = 1.0f; k.v[1] = 1.0f; k.v[2] = 1.0f;
        scl.keys = {k};   // escala 1
        k.v[0] = 1.0f;
        alp.keys = {k};   // alpha 1
        k.v[0] = 0.0f; k.v[1] = 0.0f; k.v[2] = 0.0f;
        jp.keys = {k};    // joint (0,0,0)
        a.tracks = {rot, scl, alp, jp};
    }
    AnimClip b;   // pose "B": rot 90°Y, escala 3, alpha 0, joint pos (2,4,6)
    b.name = "B";
    {
        AnimTrack rot; rot.target = AnimTarget::TicRot;
        AnimTrack scl; scl.target = AnimTarget::TicScale;
        AnimTrack alp; alp.target = AnimTarget::UiAlpha;
        alp.element = ui->elements[(size_t)el].name;
        AnimTrack jp; jp.target = AnimTarget::JointPos;
        jp.element = "braco";
        AnimKey k; k.t = 0.0f;
        k.v[0] = 0.0f; k.v[1] = 90.0f; k.v[2] = 0.0f;
        rot.keys = {k};
        k.v[0] = 3.0f; k.v[1] = 3.0f; k.v[2] = 3.0f;
        scl.keys = {k};
        k.v[0] = 0.0f;
        alp.keys = {k};
        k.v[0] = 2.0f; k.v[1] = 4.0f; k.v[2] = 6.0f;
        jp.keys = {k};
        b.tracks = {rot, scl, alp, jp};
    }
    pl->clips = {a, b};
    pl->setBlend(1, 0.5f);
    pl->apply(s, h);
    // TUDO intermédio ao peso 0.5
    EXPECT(nearEqF(tr->rot.y, 0.38268f, 1e-3f));   // 45° yaw
    EXPECT(nearEqF(tr->scale.x, 2.0f));
    EXPECT(nearEqF(ui->elements[(size_t)el].color[3], 0.5f));
    EXPECT(vecNearF(sk->joints[0].pos, Vec3{1.0f, 2.0f, 3.0f}));
}

// ---- 2. crossfade SEM SALTOS ----------------------------------------------------

TEST(blend_crossfade_peso_linear_e_troca_continua) {
    WalkRun e;
    e.pl->playing = true;
    e.pl->crossfade(1, 1.0f);   // run em 1 s
    EXPECT(e.pl->blendClip == 1);
    EXPECT(nearEqF(e.pl->blendWeight, 0.0f));

    // passos de 0.25 s: peso LINEAR; pose CONTÍNUA
    // walk x=4t, run x=8t, blend = 4t(1−w)+8tw com w=t → x=4t+4t²
    f32 prevX = 0.0f;
    for (int step = 1; step <= 4; ++step) {
        const f32 t = 0.25f * static_cast<f32>(step);
        e.pl->advance(0.25f);
        e.pl->apply(e.s, e.h);
        const f32 w = e.pl->blendClip >= 0 ? e.pl->blendWeight : 1.0f;
        EXPECT(nearEqF(w, t, 1e-4f));
        const f32 expectX = 4.0f * t + 4.0f * t * t;
        EXPECT(nearEqF(e.tr->pos.x, expectX, 1e-3f));
        EXPECT(e.tr->pos.x > prevX - 1e-6f);   // monotónico: SEM salto atrás
        prevX = e.tr->pos.x;
        if (step < 4) {
            EXPECT(e.pl->activeClip == 0);     // ainda em walk (a misturar)
        }
    }
    // COMPLETOU: run é o ativo, blend limpo, tempo CONTÍNUO (= blendTime)
    EXPECT(e.pl->activeClip == 1);
    EXPECT(e.pl->blendClip == -1);
    EXPECT(nearEqF(e.pl->blendWeight, 0.0f));
    EXPECT(nearEqF(e.pl->time, 1.0f, 1e-4f));

    // o frame SEGUINTE à troca: pose de run PURA no tempo contínuo — o
    // valor NÃO salta (era 8 no fim do blend; continua 8+dt·8)
    const f32 xNaTroca = e.tr->pos.x;
    e.pl->advance(0.25f);   // t=1.25 → run x=10
    e.pl->apply(e.s, e.h);
    EXPECT(nearEqF(e.tr->pos.x, 10.0f, 1e-3f));
    EXPECT(e.tr->pos.x > xNaTroca);
}

TEST(blend_crossfade_duracao_minima_e_reblend) {
    WalkRun e;
    e.pl->playing = true;
    e.pl->crossfade(1, 0.0f);   // duração degenerada → 0.01 (nunca divide por 0)
    EXPECT(e.pl->blendDuration > 0.0f);
    e.pl->advance(1.0f);        // completa de imediato
    EXPECT(e.pl->activeClip == 1);
    // re-blend para o OUTRO clip troca o alvo
    e.pl->crossfade(0, 0.5f);
    EXPECT(e.pl->blendClip == 0);
    EXPECT(nearEqF(e.pl->blendWeight, 0.0f));
    // clip inválido: ignora
    e.pl->crossfade(42, 0.5f);
    EXPECT(e.pl->blendClip == 0);   // mantém o alvo anterior
}

TEST(blend_stopblend_fica_no_ativo) {
    WalkRun e;
    e.pl->time = 1.0f;
    e.pl->setBlend(1, 0.75f);
    e.pl->apply(e.s, e.h);
    EXPECT(nearEqF(e.tr->pos.x, 7.0f));   // 4·0.25 + 8·0.75
    e.pl->stopBlend();
    EXPECT(!e.pl->blending());
    e.pl->apply(e.s, e.h);
    EXPECT(nearEqF(e.tr->pos.x, 4.0f));   // volta ao clip ATIVO puro
    EXPECT(e.pl->activeClip == 0);
}

// ---- 3. joint SEM correspondência → fade ao BIND -------------------------------

TEST(blend_joint_sem_track_fade_ao_bind) {
    Scene s;
    const Handle h = s.create("J");
    Tic* tic = s.get(h);
    tic->addComponent<Transform3D>();
    SkeletonComp* sk = tic->addComponent<SkeletonComp>();
    AnimationPlayer* pl = tic->addComponent<AnimationPlayer>();
    SkeletonComp::Joint j;
    j.name = "braco";
    j.pos = Vec3{5.0f, 0.0f, 0.0f};        // pose atual = BIND (rest)
    j.bindPos = Vec3{5.0f, 0.0f, 0.0f};    // 0.8.3: alvo do fade
    sk->joints = {j};

    AnimClip a;   // anima o joint para (9,0,0)
    a.name = "A";
    AnimTrack jp; jp.target = AnimTarget::JointPos; jp.element = "braco";
    AnimKey k; k.t = 0.0f; k.v[0] = 9.0f; k.v[1] = 0.0f; k.v[2] = 0.0f;
    jp.keys = {k};
    a.tracks = {jp};
    AnimClip b;   // NÃO tem track do joint (só escala do TIC)
    b.name = "B";
    AnimTrack scl; scl.target = AnimTarget::TicScale;
    AnimKey k2; k2.t = 0.0f; k2.v[0] = 2.0f; k2.v[1] = 2.0f; k2.v[2] = 2.0f;
    scl.keys = {k2};
    b.tracks = {scl};
    pl->clips = {a, b};

    pl->setBlend(1, 0.5f);
    pl->apply(s, h);
    // joint: fade de (9,0,0) para o BIND (5,0,0) ao peso 0.5 → (7,0,0)
    EXPECT(vecNearF(sk->joints[0].pos, Vec3{7.0f, 0.0f, 0.0f}));
    // track do TIC sem correspondência: valor de A passa
    EXPECT(nearEqF(tic->getComponent<Transform3D>()->scale.x, 1.0f));
    // peso 1: joint inteiramente no BIND
    pl->setBlend(1, 1.0f);
    pl->apply(s, h);
    EXPECT(vecNearF(sk->joints[0].pos, Vec3{5.0f, 0.0f, 0.0f}));
}

// ---- 4. blend NÃO serializa (runtime) --------------------------------------------

TEST(blend_nao_serializa) {
    WalkRun e;
    e.pl->setBlend(1, 0.5f);
    e.pl->playing = true;
    const std::string text = SceneSerializer::dump(e.s);
    EXPECT(std::strstr(text.c_str(), "blend") == nullptr);   // nada de blend
    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(s2, text, ctx));
    Tic* back = s2.get(s2.find("Boneco"));
    ASSERT(back != nullptr);
    AnimationPlayer* pl2 = back->getComponent<AnimationPlayer>();
    ASSERT(pl2 != nullptr);
    EXPECT(pl2->blendClip == -1);   // nasce SEM blend
    EXPECT(pl2->blendWeight == 0.0f);
    EXPECT(!pl2->blending());
}

// ---- 5. UI: botão "fade" do overlay de clips arranca o crossfade ------------------

#include "platform/InputState.h"
#include "render/Renderer.h"
#include "ui/Timeline.h"
#include "ui/UiContext.h"

namespace {

struct BlendEnv {
    FontAtlas   font;
    UiContext   ui;
    InputState  input;
    Scene       scene;
    editor::EditorState st;
    timeline::State tl;
    Handle      tic{};

    BlendEnv() {
        const char* fontPath = FONT_FIXTURE;
        font.loadFromPaths(&fontPath, 1, 28.0f);
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
        tic = scene.create("B");
        Tic* t = scene.get(tic);
        t->addComponent<Transform3D>();
        AnimationPlayer* pl = t->addComponent<AnimationPlayer>();
        AnimClip walk;
        walk.name = "walk";
        walk.tracks.push_back(posTrack(8.0f));
        AnimClip run;
        run.name = "run";
        run.tracks.push_back(posTrack(16.0f));
        pl->clips = {walk, run};
        st.selected = tic;
    }

    AnimationPlayer* player() {
        return scene.get(tic)->getComponent<AnimationPlayer>();
    }

    void frame() {
        ui.beginFrame(nullptr, &input, 1600.0f, 720.0f);
        timeline::drawTimeline(ui, input, scene, st, tl, 0.25f);
        ui.endFrame();
        input.clearEdges();
    }

    void tap(f32 x, f32 y) {
        input.injectDown(0, x, y);
        frame();
        input.injectUp(0);
        frame();
    }
};

} // namespace

TEST(blend_ui_fade_do_overlay_arranca_crossfade) {
    BlendEnv e;
    AnimationPlayer* pl = e.player();
    ASSERT(pl != nullptr);
    ASSERT(pl->clips.size() == 2);

    // abre o seletor de clips e carrega "fade" do clip 1 (run)
    const UiRect r = timeline::timelineRect(1600.0f, 720.0f, safe::Insets{}, true);
    e.tap(r.x + 464.0f, r.y + 24.0f);   // botão "clip:"
    EXPECT(e.tl.clipMenu);
    const f32 w = 300.0f;
    const f32 x = (1600.0f - w) * 0.5f;
    const f32 y = (720.0f - (timeline::kHeaderH + 3.0f * 44.0f + 12.0f)) * 0.5f;
    e.tap(x + w - 52.0f, y + timeline::kHeaderH + 44.0f + 18.0f);   // "fade" da row do run
    EXPECT(pl->blendClip == 1);
    EXPECT(nearEqF(pl->blendWeight, 0.0f));
    EXPECT(pl->blendDuration > 0.0f);

    // frames de 0.25 s COM O PLAYBACK A CORRER (o crossfade avança no
    // advance — como a física, gate playing): 3 × 0.25 = 0.75 s > 0.4 s
    // → o crossfade COMPLETOU e o run ficou ATIVO
    pl->playing = true;
    for (int i = 0; i < 3; ++i) {
        e.frame();
    }
    EXPECT(pl->activeClip == 1);
    EXPECT(pl->blendClip == -1);
    EXPECT(pl->clips[1].name == "run");
}

TEST(blend_ui_slider_manual_assume_o_peso) {
    BlendEnv e;
    AnimationPlayer* pl = e.player();
    ASSERT(pl != nullptr);
    pl->setBlend(1, 0.2f);   // blend manual aberto

    // o slider de blend aparece NO LUGAR do speed (slot contextual)
    const UiRect r = timeline::timelineRect(1600.0f, 720.0f, safe::Insets{}, true);
    const f32 sx = r.x + r.w - 164.0f + 2.0f;    // extremo esquerdo = peso ~0
    e.input.injectDown(0, sx, r.y + 24.0f);
    e.frame();
    e.input.injectUp(0);
    e.frame();
    EXPECT(nearEqF(pl->blendWeight, 0.0f, 0.05f));   // arrastado ao mínimo
    EXPECT(pl->blendDuration == 0.0f);               // manual: crossfade parado
    EXPECT(pl->blendClip == 1);                      // o alvo mantém-se
}
