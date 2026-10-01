// tests/test_timeline.cpp — 0.8.0 (F7): EDITOR DE TIMELINE.
//
// Aferição da spec 0.8.0 (editor): a timeline abre com o AnimationPlayer,
// NÃO sobrepõe os painéis existentes (strip no fundo do viewport central),
// o SCRUB arrasta o tempo e aplica a pose, add/remove key funciona, a
// curva por track alterna linear↔bezier, o preview Play/Stop restaura a
// pose (sandbox), o modo cicla once→loop→pingpong.
//
// Padrão dos testes de UI da casa: UiContext com stubs GLES3, toques
// injetados por InputState::injectDown/Move/Up, aferição dos batches e do
// ESTADO (player/transform) — sem GL real.
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "components/AnimationPlayer.h"
#include "components/Transform3D.h"
#include "core/Scene.h"
#include "platform/InputState.h"
#include "render/Renderer.h"
#include "ui/EditorUi.h"
#include "ui/Timeline.h"
#include "ui/UiContext.h"

using namespace vv;
using namespace vv::editor;
using namespace test;   // nearEqF/vecNearF

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;

struct Env {
    FontAtlas   font;
    UiContext   ui;
    InputState  input;
    Scene       scene;
    EditorState st;
    timeline::State tl;
    Handle      tic{};

    Env() {
        const char* fontPath = FONT_FIXTURE;
        font.loadFromPaths(&fontPath, 1, 28.0f);
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
        tic = scene.create("Cubo");
        Tic* t = scene.get(tic);
        t->addComponent<Transform3D>();
        t->addComponent<AnimationPlayer>();
        st.selected = tic;
    }

    void frame(f32 dt = 1.0f / 60.0f) {
        ui.beginFrame(nullptr, &input, kSW, kSH);
        timeline::drawTimeline(ui, input, scene, st, tl, dt);
        ui.endFrame();
        input.clearEdges();
    }

    void tap(f32 x, f32 y) {
        input.injectDown(0, x, y);
        frame();
        input.injectUp(0);
        frame();
    }

    void drag(f32 x0, f32 y0, f32 x1, f32 y1, i32 steps = 4) {
        input.injectDown(0, x0, y0);
        frame();
        for (i32 i = 1; i <= steps; ++i) {
            const f32 a = static_cast<f32>(i) / static_cast<f32>(steps);
            input.injectMove(0, x0 + (x1 - x0) * a, y0 + (y1 - y0) * a);
            frame();
        }
        input.injectUp(0);
        frame();
    }

    AnimationPlayer* player() {
        Tic* t = scene.get(tic);
        return t ? t->getComponent<AnimationPlayer>() : nullptr;
    }

    Transform3D* transform() {
        Tic* t = scene.get(tic);
        return t ? t->getComponent<Transform3D>() : nullptr;
    }

    // track 0 (pos) do clip de edição
    AnimTrack* track0() {
        AnimationPlayer* pl = player();
        return pl ? (pl->editClip()->tracks.empty()
                         ? nullptr : &pl->editClip()->tracks[0]) : nullptr;
    }
};

} // namespace

// ---- 1. visibilidade ------------------------------------------------------------

TEST(timeline_visivel_so_com_player) {
    {
        Scene s;
        const Handle h = s.create("sem player");
        EXPECT(!timeline::visible(false, false, s, h));
    }
    {
        Scene s;
        const Handle h = s.create("com player");
        s.get(h)->addComponent<AnimationPlayer>();
        EXPECT(timeline::visible(false, false, s, h));
        // play mode / modo UI → fechada
        EXPECT(!timeline::visible(true, false, s, h));
        EXPECT(!timeline::visible(false, true, s, h));
        // TIC inativo → fechada
        s.get(h)->active = false;
        EXPECT(!timeline::visible(false, false, s, h));
    }
}

// ---- 2. o rect NÃO interfere com os painéis -------------------------------------

TEST(timeline_rect_fundo_do_viewport_sem_sobreposicao) {
    const safe::Insets in{};
    const UiRect center = safe::centerRect(kSW, kSH, in, true);
    const UiRect tl = timeline::timelineRect(kSW, kSH, in, true);

    // dentro do viewport central (nada sangra para os painéis)
    EXPECT(tl.x >= center.x - 0.01f);
    EXPECT(tl.x + tl.w <= center.x + center.w + 0.01f);
    // COLA ao fundo do viewport
    EXPECT(nearEqF(tl.y + tl.h, center.y + center.h, 0.01f));
    EXPECT(nearEqF(tl.w, center.w, 0.01f));
    EXPECT(nearEqF(tl.h, timeline::kTimelineH, 0.01f));

    // os painéis laterais ficam FORA da timeline (sem interseção)
    const UiRect hier = safe::hierarchyPanelRect(kSW, kSH, in);
    const UiRect insp = safe::inspectorPanelRect(kSW, kSH, in);
    auto overlap = [](const UiRect& a, const UiRect& b) {
        const f32 ix = (a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w) -
                       (a.x > b.x ? a.x : b.x);
        const f32 iy = (a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h) -
                       (a.y > b.y ? a.y : b.y);
        return ix > 0.5f && iy > 0.5f;
    };
    EXPECT(!overlap(tl, hier));
    EXPECT(!overlap(tl, insp));
    // a toolbar e a status line também ficam fora
    const UiRect tb = safe::toolbarRect(kSW, kSH, in);
    const UiRect stat = safe::statusRect(kSW, kSH, in);
    EXPECT(!overlap(tl, tb));
    EXPECT(!overlap(tl, stat));
}

// ---- 3. SCRUB: arrastar o tempo aplica a pose ------------------------------------

TEST(timeline_scrub_move_o_tempo_e_aplica) {
    Env e;
    AnimationPlayer* pl = e.player();
    ASSERT(pl != nullptr);
    AnimTrack t;
    t.target = AnimTarget::TicPos;
    AnimKey a; a.t = 0.0f; a.v[0] = 0.0f; a.v[1] = 0.0f; a.v[2] = 0.0f;
    AnimKey b; b.t = 2.0f; b.v[0] = 2.0f; b.v[1] = 0.0f; b.v[2] = 0.0f;
    t.keys = {a, b};
    pl->editClip()->tracks.push_back(t);

    const UiRect r = timeline::timelineRect(kSW, kSH, safe::Insets{}, true);
    const f32 keysX = r.x + 300.0f;
    const f32 keysW = r.w - 300.0f - 12.0f;
    // drag na RÉUA: começa no início, acaba no MEIO (t=1s)
    e.drag(keysX + 4.0f, r.y + timeline::kHeaderH + 8.0f,
           keysX + keysW * 0.5f, r.y + timeline::kHeaderH + 8.0f);
    EXPECT(nearEqF(pl->time, 1.0f, 0.05f));
    EXPECT(nearEqF(e.transform()->pos.x, 1.0f, 0.05f));   // pose AO VIVO
    // drag TERMINADO: o estado de scrub fecha no release
    EXPECT(!e.tl.scrubbing);
}

// ---- 4. add/remove key ----------------------------------------------------------

TEST(timeline_add_key_no_cursor_com_valor_atual) {
    Env e;
    AnimationPlayer* pl = e.player();
    ASSERT(pl != nullptr);
    AnimTrack* tr = pl->addTrack(AnimTarget::TicPos);
    ASSERT(tr != nullptr);
    AnimKey a;
    a.t = 0.0f;
    a.v[0] = 0.0f; a.v[1] = 0.0f; a.v[2] = 0.0f;
    tr->keys.push_back(a);

    // posa o objeto em (3, 1, 0) e põe o cursor em t=1
    Transform3D* trns = e.transform();
    trns->pos = Vec3{3.0f, 1.0f, 0.0f};
    trns->updateWorld();
    pl->time = 1.0f;

    const UiRect r = timeline::timelineRect(kSW, kSH, safe::Insets{}, true);
    // botão "+key" da row 0: r.x + 212, y da row 0 + 4
    const f32 rowY = r.y + timeline::kHeaderH + timeline::kRulerH;
    e.tap(r.x + 234.0f, rowY + 20.0f);

    ASSERT(tr->keys.size() == 2);
    EXPECT(nearEqF(tr->keys[1].t, 1.0f, 1e-3f));
    EXPECT(nearEqF(tr->keys[1].v[0], 3.0f));
    EXPECT(nearEqF(tr->keys[1].v[1], 1.0f));

    // carregar "+key" OUTRA VEZ no mesmo t: SUBSTITUI (não duplica)
    trns->pos = Vec3{9.0f, 9.0f, 0.0f};
    trns->updateWorld();
    e.tap(r.x + 234.0f, rowY + 20.0f);
    EXPECT(tr->keys.size() == 2);
    EXPECT(nearEqF(tr->keys[1].v[0], 9.0f));
}

TEST(timeline_del_key_remove_a_mais_proxima_do_cursor) {
    Env e;
    AnimationPlayer* pl = e.player();
    ASSERT(pl != nullptr);
    AnimTrack* tr = pl->addTrack(AnimTarget::TicPos);
    ASSERT(tr != nullptr);
    AnimKey a; a.t = 0.0f;  a.v[0] = 0.0f;
    AnimKey b; b.t = 1.0f;  b.v[0] = 5.0f;
    AnimKey c; c.t = 2.0f;  c.v[0] = 9.0f;
    tr->keys = {a, b, c};
    pl->time = 1.2f;   // cursor MAIS PERTO da key b (t=1)

    const UiRect r = timeline::timelineRect(kSW, kSH, safe::Insets{}, true);
    const f32 rowY = r.y + timeline::kHeaderH + timeline::kRulerH;
    e.tap(r.x + 278.0f, rowY + 20.0f);   // botão "−" da row 0

    EXPECT(tr->keys.size() == 2);
    EXPECT(nearEqF(tr->keys[0].t, 0.0f));
    EXPECT(nearEqF(tr->keys[1].t, 2.0f));   // a do meio saiu
}

// ---- 5. curva por track: linear ↔ bezier ----------------------------------------

TEST(timeline_curve_toggle_por_track) {
    Env e;
    AnimationPlayer* pl = e.player();
    ASSERT(pl != nullptr);
    AnimTrack* tr = pl->addTrack(AnimTarget::TicPos);
    ASSERT(tr != nullptr);
    EXPECT(tr->curve == AnimCurve::Linear);   // default

    const UiRect r = timeline::timelineRect(kSW, kSH, safe::Insets{}, true);
    const f32 rowY = r.y + timeline::kHeaderH + timeline::kRulerH;
    e.tap(r.x + 188.0f, rowY + 20.0f);   // botão "lin|bez" da row 0
    EXPECT(tr->curve == AnimCurve::Bezier);
    e.tap(r.x + 188.0f, rowY + 20.0f);
    EXPECT(tr->curve == AnimCurve::Linear);
}

// ---- 6. preview Play/Stop: sandbox restaura a pose -------------------------------

TEST(timeline_preview_play_stop_restaura_pose) {
    Env e;
    AnimationPlayer* pl = e.player();
    ASSERT(pl != nullptr);
    AnimTrack t;
    t.target = AnimTarget::TicPos;
    AnimKey a; a.t = 0.0f; a.v[0] = 0.0f; a.v[1] = 0.0f; a.v[2] = 0.0f;
    AnimKey b; b.t = 2.0f; b.v[0] = 8.0f; b.v[1] = 0.0f; b.v[2] = 0.0f;
    t.keys = {a, b};
    pl->editClip()->tracks.push_back(t);
    e.transform()->pos = Vec3{0.5f, 0.5f, 0.5f};   // pose de editor

    const UiRect r = timeline::timelineRect(kSW, kSH, safe::Insets{}, true);
    // PLAY (botão à esquerda do conjunto de controlos)
    e.tap(r.x + r.w - 394.0f, r.y + 24.0f);
    EXPECT(pl->playing);
    EXPECT(e.tl.snap.captured);   // sandbox capturada

    // avança 30 frames de 1/60 (0.5 s) → pos.x = 2
    for (int i = 0; i < 30; ++i) {
        e.frame();
    }
    EXPECT(nearEqF(pl->time, 0.5f, 0.05f));
    EXPECT(nearEqF(e.transform()->pos.x, 2.0f, 0.05f));

    // STOP: pose de editor de VOLTA, tempo a zero, parado
    e.tap(r.x + r.w - 316.0f, r.y + 24.0f);
    EXPECT(!pl->playing);
    EXPECT(nearEqF(pl->time, 0.0f));
    EXPECT(nearEqF(e.transform()->pos.x, 0.5f, 1e-4f));
    EXPECT(nearEqF(e.transform()->pos.y, 0.5f, 1e-4f));
    EXPECT(!e.tl.snap.captured);   // sandbox fechada
}

// ---- 7. modo cicla + velocidade --------------------------------------------------

TEST(timeline_mode_cicla_once_loop_pingpong) {
    Env e;
    AnimationPlayer* pl = e.player();
    ASSERT(pl != nullptr);
    EXPECT(pl->mode == AnimationPlayer::Mode::Loop);   // default
    const UiRect r = timeline::timelineRect(kSW, kSH, safe::Insets{}, true);
    const f32 modeX = r.x + r.w - 280.0f + 54.0f;   // centro do botão mode
    e.tap(modeX, r.y + 24.0f);
    EXPECT(pl->mode == AnimationPlayer::Mode::PingPong);
    e.tap(modeX, r.y + 24.0f);
    EXPECT(pl->mode == AnimationPlayer::Mode::Once);
    e.tap(modeX, r.y + 24.0f);
    EXPECT(pl->mode == AnimationPlayer::Mode::Loop);   // volta ao início
}

TEST(timeline_speed_slider_do_header) {
    Env e;
    AnimationPlayer* pl = e.player();
    ASSERT(pl != nullptr);
    const UiRect r = timeline::timelineRect(kSW, kSH, safe::Insets{}, true);
    // slider de speed: [r.x + r.w − 164, +96] × 6..(bh−6)
    const f32 sx = r.x + r.w - 164.0f + 90.0f;   // ~0.9 do percurso
    const f32 sy = r.y + 24.0f;
    e.input.injectDown(0, sx, sy);
    e.frame();
    e.input.injectUp(0);
    e.frame();
    EXPECT(pl->speed > 2.0f);   // arrastado para a direita = mais rápido
}

// ---- 8. overlay de novo track ----------------------------------------------------

TEST(timeline_overlay_novo_track_adiciona) {
    Env e;
    AnimationPlayer* pl = e.player();
    ASSERT(pl != nullptr);
    const UiRect r = timeline::timelineRect(kSW, kSH, safe::Insets{}, true);
    // abre o overlay "+"
    e.tap(r.x + r.w - 36.0f, r.y + 24.0f);
    EXPECT(e.tl.addTrackMenu);
    // item 1 = "posicao" (botão a y do 1º item do overlay, centrado no ecrã)
    const f32 w = 300.0f;
    const f32 h = timeline::kHeaderH + 6.0f * 44.0f + 12.0f;
    const f32 x = (kSW - w) * 0.5f;
    const f32 y = (kSH - h) * 0.5f;
    e.tap(x + 150.0f, y + timeline::kHeaderH + 18.0f);
    EXPECT(!e.tl.addTrackMenu);   // escolheu → fechou
    ASSERT(pl->editClip()->tracks.size() == 1);
    EXPECT(pl->editClip()->tracks[0].target == AnimTarget::TicPos);
    // idempotente: escolher o MESMO alvo de novo devolve o MESMO track
    e.tap(r.x + r.w - 36.0f, r.y + 24.0f);
    e.tap(x + 150.0f, y + timeline::kHeaderH + 18.0f);
    EXPECT(pl->editClip()->tracks.size() == 1);
}

// ---- 9. troca de seleção com preview a correr → restaura ------------------------

TEST(timeline_troca_selecao_para_o_preview) {
    Env e;
    AnimationPlayer* pl = e.player();
    ASSERT(pl != nullptr);
    AnimTrack t;
    t.target = AnimTarget::TicPos;
    AnimKey a; a.t = 0.0f; a.v[0] = 0.0f; a.v[1] = 0.0f; a.v[2] = 0.0f;
    AnimKey b; b.t = 2.0f; b.v[0] = 8.0f; b.v[1] = 0.0f; b.v[2] = 0.0f;
    t.keys = {a, b};
    pl->editClip()->tracks.push_back(t);
    e.transform()->pos = Vec3{0.5f, 0.5f, 0.5f};

    const UiRect r = timeline::timelineRect(kSW, kSH, safe::Insets{}, true);
    e.tap(r.x + r.w - 394.0f, r.y + 24.0f);   // play
    for (int i = 0; i < 20; ++i) {
        e.frame();
    }
    EXPECT(e.transform()->pos.x > 1.3f);   // animou (20 frames = ~0.33 s → x≈1.3)

    // seleciona OUTRO TIC → o preview do primeiro RESTAURA
    const Handle other = e.scene.create("Outro");
    e.scene.get(other)->addComponent<AnimationPlayer>();
    e.st.selected = other;
    e.frame();
    EXPECT(!e.tl.snap.captured);
    EXPECT(!pl->playing);
    EXPECT(nearEqF(e.transform()->pos.x, 0.5f, 1e-4f));   // pose de editor
}
