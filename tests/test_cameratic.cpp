// tests/test_cameratic.cpp — 0.7.7: TIC DE CÂMARA + FRUSTUM + GIZMOS.
//
// Aferição da spec (uma por bloco):
//   • geometria do frustum (retângulo far derivado de fov/near/far; orto);
//   • seleção por toque no frustum (hit-test 3D por projeção);
//   • gizmo mover/rodar altera o Transform3D da câmara (o caminho de sempre);
//   • gizmo ESCALAR altera fov/orthoSize (NUNCA a escala do transform);
//   • handles do far mudam far/fov (âncoras, snap, clamps) sem conflitar
//     com os eixos do gizmo (hit prioritário);
//   • serialização round-trip + invariante "uma ativa por cena";
//   • Play usa a câmara ATIVA (gameView/gameProj) e o frustum NÃO se
//     desenha em Play (gate visible);
//   • alinhar-à-vista copia a pose da orbit.
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <string>

#include "components/CameraComp.h"
#include "components/Transform3D.h"
#include "core/CameraUtil.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "platform/InputState.h"
#include "render/Camera.h"
#include "render/Renderer.h"
#include "ui/CamGizmo.h"
#include "ui/EditorLayout.h"
#include "ui/EditorUi.h"      // EditorState/drawContextMenu (menu 0.7.7)
#include "ui/UiEditor.h"
#include "ui/Gizmo.h"
#include "ui/UiContext.h"

using namespace vv;
using namespace vv::camgizmo;
using namespace vv::editor;   // InspProfile/InspRow (EditorLayout)
using test::nearEqF;
using test::vecNearF;
using test::matNearF;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;

// uma câmara de cena com pose/parâmetros dados
struct CamTic {
    Scene scene;
    Handle h{};
    Transform3D* tr = nullptr;
    CameraComp* cam = nullptr;

    CamTic() {
        h = scene.create("Camera");
        Tic* t = scene.get(h);
        tr = t->addComponent<Transform3D>();
        cam = t->addComponent<CameraComp>();
    }
};

// vp de uma orbit de edição a olhar para a origem (a do editor por omissão)
Mat4 editorVp(f32 aspect) {
    Camera c;   // yaw 0.6 · pitch 0.5 · dist 6 (defaults)
    return Mat4::mul(c.proj(aspect), c.view());
}

} // namespace

// ---- 1. geometria do frustum ---------------------------------------------------

TEST(cameratic_geometria_do_frustum) {
    CamTic c;   // pos 0, rot identidade → olha para −Z
    c.cam->fovY = 90.0f;
    c.cam->nearZ = 0.5f;
    c.cam->farZ = 100.0f;
    const Frustum f = computeFrustum(*c.tr, *c.cam, 2.0f);   // aspect 2

    // base local
    EXPECT(vecNearF(f.fwd, Vec3{0, 0, -1}));
    EXPECT(vecNearF(f.right, Vec3{1, 0, 0}));
    EXPECT(vecNearF(f.up, Vec3{0, 1, 0}));
    // far: halfH = tan(45°)·100 = 100 · halfW = 200 (aspect 2)
    EXPECT(nearEqF(f.farCenter.z, -100.0f));
    EXPECT(nearEqF(f.farC[0].x, 200.0f) && nearEqF(f.farC[0].y, 100.0f));
    EXPECT(nearEqF(f.farC[2].x, -200.0f) && nearEqF(f.farC[2].y, -100.0f));
    // near: halfH = tan(45°)·0.5 = 0.5 · halfW = 1
    EXPECT(nearEqF(f.nearC[0].x, 1.0f) && nearEqF(f.nearC[0].y, 0.5f));
    EXPECT(nearEqF(f.nearC[0].z, -0.5f));
    // o cone liga near[i]→far[i] (sem torção)
    EXPECT(nearEqF(f.farC[0].x / f.nearC[0].x, 200.0f));

    // ORTO: meia-altura FIXA (orthoSize) nos DOIS planos
    c.cam->projection = CameraComp::Projection::Orthographic;
    c.cam->orthoSize = 5.0f;
    const Frustum o = computeFrustum(*c.tr, *c.cam, 2.0f);
    EXPECT(nearEqF(o.farC[0].y, 5.0f) && nearEqF(o.farC[0].x, 10.0f));
    EXPECT(nearEqF(o.nearC[0].y, 5.0f) && nearEqF(o.nearC[0].x, 10.0f));

    // RODADA: yaw 90° → olha para −X (−Z local no mundo)
    c.cam->projection = CameraComp::Projection::Perspective;
    c.tr->rot = Quat::fromEuler(0.0f, 1.5707963f, 0.0f);
    c.tr->updateWorld();
    const Frustum r = computeFrustum(*c.tr, *c.cam, 1.0f);
    EXPECT(vecNearF(r.fwd, Vec3{-1, 0, 0}, 1e-3f));
    EXPECT(nearEqF(r.farCenter.x, -100.0f));

    // planeHalfExtents é a fonte ÚNICA (a matemática aferida à parte)
    f32 hw = 0.0f, hh = 0.0f;
    planeHalfExtents(*c.cam, 50.0f, 2.0f, hw, hh);
    EXPECT(nearEqF(hh, std::tan(0.78539816f) * 50.0f));   // tan(45°)·50
    EXPECT(nearEqF(hw, hh * 2.0f));
}

// ---- 2. seleção por toque no frustum ---------------------------------------------

TEST(cameratic_selecao_por_toque_no_frustum) {
    CamTic c;
    c.tr->pos = Vec3{0.0f, 0.5f, 0.0f};
    c.cam->farZ = 8.0f;    // frustum contido no ecrã (500 u sairia fora)
    c.cam->fovY = 45.0f;
    c.tr->updateWorld();
    const Mat4 vp = editorVp(kSW / kSH);
    const Frustum f = computeFrustum(*c.tr, *c.cam, kSW / kSH);

    // tap no CENTRO do far projetado (fim da linha de visão) → seleciona
    f32 fx = 0.0f, fy = 0.0f;
    EXPECT(gizmo::projectPoint(vp, f.farCenter, kSW, kSH, fx, fy));
    const Handle hit = pickCameraTic(c.scene, vp, kSW, kSH, fx, fy);
    EXPECT(hit == c.h);

    // tap na aresta do CONE (a meio do near→far) → seleciona
    const Vec3 mid = (f.nearC[0] + f.farC[0]) * 0.5f;
    f32 mx = 0.0f, my = 0.0f;
    EXPECT(gizmo::projectPoint(vp, mid, kSW, kSH, mx, my));
    EXPECT(pickCameraTic(c.scene, vp, kSW, kSH, mx, my) == c.h);

    // tap LONGE (canto do ecrã) → nada
    EXPECT(!pickCameraTic(c.scene, vp, kSW, kSH, 60.0f, 660.0f).valid());

    // câmara INVISÍVEL não é selecionável (o olho da Hierarchy desliga-a)
    if (Tic* t = c.scene.get(c.h)) {
        t->visible = false;
    }
    EXPECT(!pickCameraTic(c.scene, vp, kSW, kSH, fx, fy).valid());
}

// ---- 3. gizmo mover/rodar altera o TRANSFORM da câmara ----------------------------

TEST(cameratic_gizmo_mover_rodar_altera_transform) {
    CamTic c;
    c.tr->pos = Vec3{1.0f, 0.0f, 0.0f};
    // o caminho do mover/rodar é o do Transform3D de SEMPRE (o gizmo
    // genérico atua no componente) — âncoras + delta:
    const Vec3 h0{0.0f, 0.0f, 0.0f};
    const Vec3 h1{3.0f, 0.0f, 0.0f};
    c.tr->pos = gizmo::dragMoveAxis(c.tr->pos, Vec3{1, 0, 0}, h0, h1, false);
    EXPECT(vecNearF(c.tr->pos, Vec3{4.0f, 0.0f, 0.0f}));
    // snap ao grid de 0.5
    c.tr->pos = gizmo::dragMoveAxis(c.tr->pos, Vec3{0, 0, 1}, h0,
                                    Vec3{0, 0, 0.3f}, true);
    EXPECT(nearEqF(c.tr->pos.z, 0.5f));
    // rodar: âncora + delta em torno do Y
    c.tr->rot = gizmo::dragRotate(c.tr->rot, Vec3{0, 1, 0}, Vec3{0, 0, -1},
                                  0.0f, 0.5235988f, false);   // +30°
    const Vec3 fwd = c.tr->rot.rotate(Vec3{0, 0, -1});
    EXPECT(fwd.x < -0.49f && fwd.x > -0.51f);   // sin(30°) para o −X
    EXPECT(fwd.z < -0.85f && fwd.z > -0.88f);
    c.tr->updateWorld();
}

// ---- 4. gizmo ESCALAR altera fov/orthoSize (nunca o transform) ---------------------

TEST(cameratic_gizmo_escalar_altera_fov_ortho) {
    CamTic c;
    c.cam->fovY = 60.0f;
    // fator 1.5 (dedo afastou 50% do centro) → fov 90 (persp)
    f32 fov = dragScaleToFov(60.0f, 60.0f, false, 100.0f, 150.0f, false);
    EXPECT(nearEqF(fov, 90.0f));
    // snap: fator arredondado ao passo dos gizmos (0.25)
    fov = dragScaleToFov(60.0f, 60.0f, false, 100.0f, 180.0f, true);
    EXPECT(nearEqF(fov, 105.0f));   // fator 1.8 → snap 1.75 → 60·1.75
    // clamp superior
    fov = dragScaleToFov(60.0f, 60.0f, false, 10.0f, 400.0f, false);
    EXPECT(nearEqF(fov, CameraComp::kMaxFov));
    // orto: orthoSize escala (fov irrelevante)
    f32 os = dragScaleToFov(60.0f, 4.0f, true, 100.0f, 200.0f, false);
    EXPECT(nearEqF(os, 8.0f));
    // âncora degenerada (dedo em cima do centro) → fica o valor atual
    fov = dragScaleToFov(60.0f, 60.0f, false, 2.0f, 300.0f, false);
    EXPECT(nearEqF(fov, 60.0f));
    // o CONTRATO: a função NÃO toca no transform (a escala fica 1 — numa
    // câmara não tem significado; o main chama ISTO em vez do dragScale*)
    c.tr->scale = Vec3{1.0f, 1.0f, 1.0f};
    const f32 v = dragScaleToFov(c.cam->fovY, c.cam->orthoSize, false, 100.0f,
                                 150.0f, false);
    c.cam->fovY = v;
    EXPECT(vecNearF(c.tr->scale, Vec3{1.0f, 1.0f, 1.0f}));
    EXPECT(nearEqF(c.cam->fovY, 90.0f));
}

// ---- 5. handles do far: far/fov + prioridade sobre o gizmo -------------------------

TEST(cameratic_handles_far_fov_sem_conflito) {
    CamTic c;
    const Mat4 vp = editorVp(kSW / kSH);
    const Frustum f = computeFrustum(*c.tr, *c.cam, kSW / kSH);

    // pickHandle: canto (fov) e centro (far) — ids certos
    f32 cx = 0.0f, cy = 0.0f;
    EXPECT(gizmo::projectPoint(vp, f.farCenter, kSW, kSH, cx, cy));
    EXPECT(pickHandle(vp, kSW, kSH, f, cx, cy) == 5);
    f32 kx = 0.0f, ky = 0.0f;
    EXPECT(gizmo::projectPoint(vp, f.farC[1], kSW, kSH, kx, ky));
    EXPECT(pickHandle(vp, kSW, kSH, f, kx, ky) == 2);
    // longe de tudo → 0
    EXPECT(pickHandle(vp, kSW, kSH, f, 80.0f, 640.0f) == 0);

    // PRIORIDADE sobre o eixo do gizmo: o raio do handle (30px) é MAIOR
    // que o do eixo (22px) — no main o pickHandle corre PRIMEIRO; um toque
    // a <30px do centro do far é do handle MESMO que um eixo esteja perto
    // (aferido: no ponto do handle o eixo pode estar a <22px — o handle
    // ganha porque é consultado antes)
    const f32 hx = cx + 20.0f;   // dentro do raio do handle
    EXPECT(pickHandle(vp, kSW, kSH, f, hx, cy) == 5);
    (void)kx; (void)ky;

    // drag do CENTRO (far): delta projetado no eixo de visão
    const Vec3 h0{0.0f, 0.0f, 0.0f};
    const Vec3 h1{0.0f, 0.0f, -40.0f};   // 40 u para a frente (−Z)
    EXPECT(nearEqF(dragFar(100.0f, h0, h1, f.fwd, false), 140.0f));
    // snap: passos de 1
    EXPECT(nearEqF(dragFar(100.0f, h0, Vec3{0, 0, -40.6f}, f.fwd, true),
                   141.0f));
    // clamp (arrastar PARA A FRENTE = −Z da câmara: 3000 u → clamp 2000)
    EXPECT(nearEqF(dragFar(100.0f, h0, Vec3{0, 0, -3000.0f}, f.fwd, false),
                   CameraComp::kMaxFar));
    EXPECT(nearEqF(dragFar(1.0f, h0, Vec3{0, 0, 5.0f}, f.fwd, false),
                   CameraComp::kMinFar));

    // drag do CANTO (fov): fator radial d1/d0
    EXPECT(nearEqF(dragFov(60.0f, 100.0f, 150.0f, false), 90.0f));
    EXPECT(nearEqF(dragFov(60.0f, 100.0f, 150.0f, true), 90.0f));   // snap 5°
    EXPECT(nearEqF(dragFov(60.0f, 100.0f, 50.0f, false), 30.0f));
    // clamp + âncora degenerada
    EXPECT(nearEqF(dragFov(60.0f, 10.0f, 400.0f, false), CameraComp::kMaxFov));
    EXPECT(nearEqF(dragFov(60.0f, 1.0f, 400.0f, false), 60.0f));
}

// ---- 6. serialização round-trip + uma ativa por cena --------------------------------

TEST(cameratic_serializacao_round_trip) {
    Scene a;
    Handle h1 = a.create("Cam1");
    if (Tic* t = a.get(h1)) {
        Transform3D* tr = t->addComponent<Transform3D>();
        tr->pos = Vec3{1.0f, 2.0f, 3.0f};
        tr->rot = Quat::fromEuler(0.2f, 0.6f, 0.0f);
        tr->updateWorld();
        CameraComp* cc = t->addComponent<CameraComp>();
        cc->fovY = 75.0f;
        cc->nearZ = 0.1f;
        cc->farZ = 800.0f;
        cc->active = true;
    }
    Handle h2 = a.create("Cam2");
    if (Tic* t = a.get(h2)) {
        t->addComponent<Transform3D>();
        CameraComp* cc = t->addComponent<CameraComp>();
        cc->projection = CameraComp::Projection::Orthographic;
        cc->orthoSize = 7.5f;
        cc->active = true;   // DUAS ativas no ficheiro — o load arruma
    }
    const std::string text = SceneSerializer::dump(a);

    // o .goni tem o tipo "Camera" e os parâmetros
    EXPECT(text.find("\"Camera\"") != std::string::npos);
    EXPECT(text.find("\"fov\"") != std::string::npos);
    EXPECT(text.find("\"ortho\"") != std::string::npos);

    Scene b;
    const SceneSerializer::LoadCtx ctx;   // sem resolvers (câmara não precisa)
    EXPECT(SceneSerializer::loadText(b, text, ctx));
    Tic* t1 = nullptr;
    Tic* t2 = nullptr;
    b.forEachActive([&](Tic& t) {
        if (t.name == "Cam1") t1 = &t;
        if (t.name == "Cam2") t2 = &t;
    });
    EXPECT(t1 && t2);
    if (t1 && t2) {
        const CameraComp* c1 = t1->getComponent<CameraComp>();
        const CameraComp* c2 = t2->getComponent<CameraComp>();
        EXPECT(c1 && c2);
        if (c1 && c2) {
            EXPECT(nearEqF(c1->fovY, 75.0f));
            EXPECT(nearEqF(c1->nearZ, 0.1f));
            EXPECT(nearEqF(c1->farZ, 800.0f));
            EXPECT(c1->projection == CameraComp::Projection::Perspective);
            if (const Transform3D* tr = t1->getComponent<Transform3D>()) {
                EXPECT(vecNearF(tr->pos, Vec3{1.0f, 2.0f, 3.0f}));
            }
            EXPECT(c2->projection == CameraComp::Projection::Orthographic);
            EXPECT(nearEqF(c2->orthoSize, 7.5f));
            // UMA ATIVA: a PRIMEIRA do manifesto fica; a segunda sai
            EXPECT(c1->active);
            EXPECT(!c2->active);
        }
    }
    // defaults NÃO gravados (ficheiro 0.7.6 abre limpo): a câmara default
    // (60/0.5/500/persp/ativa) serializa sem fov/near/far e volta default
    Scene d;
    if (Tic* t = d.get(d.create("D"))) {
        t->addComponent<Transform3D>();
        t->addComponent<CameraComp>();
    }
    const std::string dtext = SceneSerializer::dump(d);
    EXPECT(dtext.find("\"fov\"") == std::string::npos);
    Scene e;
    EXPECT(SceneSerializer::loadText(e, dtext, ctx));
    Tic* de = nullptr;
    e.forEachActive([&](Tic& t) { de = &t; });
    if (const CameraComp* cc = de ? de->getComponent<CameraComp>() : nullptr) {
        EXPECT(nearEqF(cc->fovY, CameraComp::kDefaultFov));
        EXPECT(cc->active);
    }
}

// ---- 7. uma ativa por cena (CameraUtil) ----------------------------------------------

TEST(cameratic_uma_ativa_por_cena) {
    CamTic c;
    Handle h2 = c.scene.create("Cam2");
    if (Tic* t = c.scene.get(h2)) {
        t->addComponent<Transform3D>();
        t->addComponent<CameraComp>();
    }
    // a 1ª nasceu ativa; ativar a 2ª desativa a 1ª
    EXPECT(setOnlyActiveCamera(c.scene, h2));
    EXPECT(!c.cam->active);
    EXPECT(c.scene.get(h2)->getComponent<CameraComp>()->active);
    EXPECT(findActiveCameraTic(c.scene) == c.scene.get(h2));
    // desativar a ativa → cena SEM ativa (fallback da orbit no Play)
    EXPECT(clearActiveCamera(c.scene, h2));
    EXPECT(findActiveCameraTic(c.scene) == nullptr);
    // re-ativar a 1ª (setOnly) e o enforce não muda nada
    EXPECT(setOnlyActiveCamera(c.scene, c.h));
    enforceSingleActiveCamera(c.scene);
    EXPECT(c.cam->active);
    EXPECT(!c.scene.get(h2)->getComponent<CameraComp>()->active);
    // sem Transform3D não é câmara de jogo (só o componente não basta)
    Handle h3 = c.scene.create("Cam3");
    if (Tic* t = c.scene.get(h3)) {
        CameraComp* cc = t->addComponent<CameraComp>();
        cc->active = true;   // ativa mas SEM pose — ignorada
    }
    EXPECT(findActiveCameraTic(c.scene) != c.scene.get(h3));
    // setOnly num TIC sem câmara → false (honesto)
    EXPECT(!setOnlyActiveCamera(c.scene, c.scene.create("Cubo")));
}

// ---- 8. Play usa a câmara ATIVA; o frustum NÃO se desenha em Play --------------------

TEST(cameratic_play_usa_camara_ativa_e_nao_desenha_frustum) {
    CamTic c;
    c.tr->pos = Vec3{0.0f, 0.0f, 5.0f};   // olha para −Z (identidade)
    c.tr->updateWorld();
    // o gate: o frustum é gizmo de EDITOR — nunca em play nem no modo UI
    EXPECT(!camgizmo::visible(true, false));    // play
    EXPECT(!camgizmo::visible(false, true));    // modo UI
    EXPECT(camgizmo::visible(false, false));    // editor 3D

    // gameView: a pose vira lookAt (eye, eye+fwd, up local)
    const Mat4 view = gameView(*c.tr);
    const Mat4 want = Mat4::lookAt(Vec3{0, 0, 5}, Vec3{0, 0, 4}, Vec3{0, 1, 0});
    EXPECT(matNearF(view, want));
    // gameProj: persp 60° == perspective(rad60)
    const Mat4 pj = gameProj(*c.cam, 1.7777f);
    const Mat4 pjw = Mat4::perspective(1.0471975f, 1.7777f, 0.5f, 500.0f);
    EXPECT(matNearF(pj, pjw));
    // orto: ±orthoSize·aspect
    c.cam->projection = CameraComp::Projection::Orthographic;
    const Mat4 oj = gameProj(*c.cam, 2.0f);
    const Mat4 ojw = Mat4::ortho(-10.0f, 10.0f, -5.0f, 5.0f, 0.5f, 500.0f);
    EXPECT(matNearF(oj, ojw));

    // o frustum SÓ aparece quando o caller o chama em editor — drawAll
    // emite as linhas (o batch prova o desenho; o gate acima prova o nunca
    // em play)
    UiContext ui;
    ui.init();
    InputState in;
    ui.beginFrame(nullptr, &in, kSW, kSH);
    drawAll(ui, c.scene, editorVp(kSW / kSH), kSW, kSH, c.h);
    ui.endFrame();
    EXPECT(ui.solidsForTest().vertexCount() > 0);
    // câmara selecionada: os handles saem também (mais quads que sem seleção)
    const u32 withSel = ui.solidsForTest().vertexCount();
    ui.beginFrame(nullptr, &in, kSW, kSH);
    drawAll(ui, c.scene, editorVp(kSW / kSH), kSW, kSH, Handle{});
    ui.endFrame();
    EXPECT(ui.solidsForTest().vertexCount() < withSel);
}

// ---- 9. alinhar à vista ----------------------------------------------------------------

TEST(cameratic_alinhar_a_vista) {
    CamTic c;
    Camera orbit;
    orbit.yaw = 0.6f;
    orbit.pitch = 0.5f;
    orbit.dist = 4.0f;
    orbit.target = Vec3{1.0f, 2.0f, 3.0f};
    alignToView(*c.tr, orbit);
    // posição = olho da orbit
    EXPECT(vecNearF(c.tr->pos, orbit.eye()));
    // direção de visão = eye→target (o −Z local aponta para o alvo)
    const Vec3 fwd = gameForward(*c.tr);
    const Vec3 want =
        normalized(orbit.target - orbit.eye());
    EXPECT(vecNearF(fwd, want, 1e-3f));
    // o up local fica ~ +Y do mundo (sem rolagem)
    const Vec3 up = c.tr->rot.rotate(Vec3{0, 1, 0});
    EXPECT(up.y > 0.86f);
    // alinhar de novo é idempotente
    Transform3D before = *c.tr;
    alignToView(*c.tr, orbit);
    EXPECT(vecNearF(c.tr->pos, before.pos));
    EXPECT(nearEqF(c.tr->rot.x, before.rot.x, 1e-3f));
    EXPECT(nearEqF(c.tr->rot.w, before.rot.w, 1e-3f));
}

// ---- 10. inspector da câmara (plano puro) ------------------------------------------------

TEST(cameratic_inspector_plano) {
    CamTic c;
    Tic* t = c.scene.get(c.h);
    EXPECT(t != nullptr);
    if (!t) {
        return;
    }
    const InspProfile prof = inspectorProfile(*t);
    EXPECT(prof.tr && prof.cam);
    const TextMetrics m{22.0f, 6.0f};   // fallback de métricas (28px)
    InspRow rows[40];
    const u32 n = inspectorPlan(prof, m, false, rows);
    EXPECT(n == inspectorRowCount(prof, false));
    // secção Camera + fov + near + far + projecao + ortho + ativa
    int sect = 0, fov = 0, near_ = 0, far = 0, proj = 0, ortho = 0, act = 0;
    for (u32 i = 0; i < n; ++i) {
        switch (rows[i].kind) {
            case InspRow::Kind::CamSection: ++sect; break;
            case InspRow::Kind::CamFov:     ++fov; break;
            case InspRow::Kind::CamNear:    ++near_; break;
            case InspRow::Kind::CamFar:     ++far; break;
            case InspRow::Kind::CamProj:    ++proj; break;
            case InspRow::Kind::CamOrtho:   ++ortho; break;
            case InspRow::Kind::CamActive:  ++act; break;
            default: break;
        }
    }
    EXPECT(sect == 1 && fov == 1 && near_ == 1 && far == 1);
    EXPECT(proj == 1 && ortho == 1 && act == 1);
    // y cumulativo sem sobreposição (o contrato do plano de sempre)
    for (u32 i = 1; i < n; ++i) {
        EXPECT(rows[i].y >= rows[i - 1].y + rows[i - 1].h - 0.01f);
    }
    // sem câmara o perfil não tem as linhas (TIC comum não as vê)
    Scene s2;
    const Handle h2 = s2.create("Cubo");
    if (Tic* t2 = s2.get(h2)) {
        t2->addComponent<Transform3D>();
    }
    const InspProfile p2 = inspectorProfile(*s2.get(h2));
    EXPECT(!p2.cam);
    EXPECT(inspectorRowCount(p2, false) < inspectorRowCount(prof, false));
}

// ---- 11. criação pelo "+" do 3D (5º item) + contexto "Alinhar a vista" -------------------

TEST(cameratic_criacao_e_menu_contextual) {
    CamTic c;
    // o TIC de câmara tem Transform3D (pose) + CameraComp (perspetiva) —
    // o perfil que o "+" do main cria (choice 5)
    EXPECT(c.tr && c.cam);
    EXPECT(c.cam->active);
    // o menu contextual da câmara tem o 5º item (Alinhar a vista): a
    // geometria do menu cresce uma linha (hasCamera)
    UiContext ui;
    ui.init();
    InputState in;
    EditorState st;
    st.contextMenu = true;
    ui.beginFrame(nullptr, &in, kSW, kSH);
    drawContextMenu(ui, in, kSW, kSH, st, "Camera", true, true);
    ui.endFrame();
    // 5 botões + título: o batch tem pelo menos os quads dos 5
    // (cada botão = painel + moldura 4 = 5 quads + painel do menu)
    const u32 nCam = ui.solidsForTest().vertexCount() / 6;
    ui.beginFrame(nullptr, &in, kSW, kSH);
    EditorState st2;
    st2.contextMenu = true;
    drawContextMenu(ui, in, kSW, kSH, st2, "Cubo", true, false);
    ui.endFrame();
    const u32 nNoCam = ui.solidsForTest().vertexCount() / 6;
    EXPECT(nCam > nNoCam);   // com câmara há UMA linha a mais
}
