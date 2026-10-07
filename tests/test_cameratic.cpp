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
#include "render/Mesh.h"       // FASE 9 (G1-6): mesh real no pick pelo corpo
#include "render/Cube.h"        // FASE 9: makeCube (mesh real no pick)
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
    c.cam->farZ = 8.0f;   // ABAIXO do cap visual → geometria REAL
    const Frustum f = computeFrustum(*c.tr, *c.cam, 2.0f);   // aspect 2

    // base local
    EXPECT(vecNearF(f.fwd, Vec3{0, 0, -1}));
    EXPECT(vecNearF(f.right, Vec3{1, 0, 0}));
    EXPECT(vecNearF(f.up, Vec3{0, 1, 0}));
    // far REAL (far 8 < cap): halfH = tan(45°)·8 = 8 · halfW = 16 (aspect 2)
    EXPECT(nearEqF(f.drawFar, 8.0f));
    EXPECT(nearEqF(f.farCenter.z, -8.0f));
    EXPECT(nearEqF(f.farC[0].x, 16.0f) && nearEqF(f.farC[0].y, 8.0f));
    EXPECT(nearEqF(f.farC[2].x, -16.0f) && nearEqF(f.farC[2].y, -8.0f));
    // near: halfH = tan(45°)·0.5 = 0.5 · halfW = 1
    EXPECT(nearEqF(f.nearC[0].x, 1.0f) && nearEqF(f.nearC[0].y, 0.5f));
    EXPECT(nearEqF(f.nearC[0].z, -0.5f));
    // o cone liga near[i]→far[i] (sem torção)
    EXPECT(nearEqF(f.farC[0].x / f.nearC[0].x, 16.0f));

    // 0.7.10 — CAP VISUAL: far ACIMA do cap → o retângulo desenha-se AO
    // CAP (o tamanho no ecrã não cresce com o far; o near fica REAL)
    c.cam->farZ = 100.0f;
    const Frustum g = computeFrustum(*c.tr, *c.cam, 2.0f);
    EXPECT(nearEqF(g.drawFar, kVisualFarCap));            // 12
    EXPECT(nearEqF(g.farCenter.z, -12.0f));
    EXPECT(nearEqF(g.farC[0].x, 24.0f) && nearEqF(g.farC[0].y, 12.0f));
    EXPECT(nearEqF(g.nearC[0].z, -0.5f));                 // near REAL
    // cap explícito MAIOR → a geometria REAL (a fonte de sempre)
    const Frustum h100 = computeFrustum(*c.tr, *c.cam, 2.0f, 200.0f);
    EXPECT(nearEqF(h100.drawFar, 100.0f));
    EXPECT(nearEqF(h100.farCenter.z, -100.0f));

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
    EXPECT(nearEqF(r.farCenter.x, -kVisualFarCap));   // clampado (far 100)

    // planeHalfExtents é a fonte ÚNICA (a matemática aferida à parte)
    f32 hw = 0.0f, hh = 0.0f;
    planeHalfExtents(*c.cam, 50.0f, 2.0f, hw, hh);
    EXPECT(nearEqF(hh, std::tan(0.78539816f) * 50.0f));   // tan(45°)·50
    EXPECT(nearEqF(hw, hh * 2.0f));
}

// ---- 2. seleção por toque NO GLIFO (D17-f: as linhas NUNCA apanham) ----

TEST(cameratic_selecao_por_toque_pelo_glifo) {
    CamTic c;
    c.tr->pos = Vec3{0.0f, 0.5f, 0.0f};
    c.cam->farZ = 500.0f;   // GIGANTE — o caso do C33
    c.cam->fovY = 45.0f;
    c.tr->updateWorld();
    const Mat4 vp = editorVp(kSW / kSH);
    const Frustum f = computeFrustum(*c.tr, *c.cam, kSW / kSH);

    // D17-a/f: tap NO GLIFO (o olho projetado, dentro do raio de toque)
    // → seleciona
    f32 bx = 0.0f, by = 0.0f;
    EXPECT(gizmo::projectPoint(vp, f.pos, kSW, kSH, bx, by));
    EXPECT(pickCameraTic(c.scene, vp, kSW, kSH, bx, by) == c.h);

    // tap no CENTRO do far projetado → NÃO seleciona (o frustum é
    // INTOCÁVEL — D17-f; o cone nunca rouba toques)
    f32 fx = 0.0f, fy = 0.0f;
    EXPECT(gizmo::projectPoint(vp, f.farCenter, kSW, kSH, fx, fy));
    EXPECT(!pickCameraTic(c.scene, vp, kSW, kSH, fx, fy).valid());

    // tap na aresta do CONE (a meio do near→far) → NÃO seleciona
    const Vec3 mid = (f.nearC[0] + f.farC[0]) * 0.5f;
    f32 mx = 0.0f, my = 0.0f;
    EXPECT(gizmo::projectPoint(vp, mid, kSW, kSH, mx, my));
    EXPECT(!pickCameraTic(c.scene, vp, kSW, kSH, mx, my).valid());

    // tap LONGE (canto do ecrã) → nada
    EXPECT(!pickCameraTic(c.scene, vp, kSW, kSH, 60.0f, 660.0f).valid());

    // câmara INVISÍVEL não é selecionável (o olho da Hierarchy desliga-a)
    if (Tic* t = c.scene.get(c.h)) {
        t->visible = false;
    }
    EXPECT(!pickCameraTic(c.scene, vp, kSW, kSH, bx, by).valid());
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
    // 0.7.9 — snap no VALOR FINAL (não no fator cru): 60·1.8=108 → 110
    // (passos de 5° como o handle do fov; antes arredondava o fator a
    // 1.75 e dava 105 — passos relativos à âncora)
    fov = dragScaleToFov(60.0f, 60.0f, false, 100.0f, 180.0f, true);
    EXPECT(nearEqF(fov, 110.0f));
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

// ---- 5. handles de CANTO do far: fov + a ordem do dono (D17-c/e) ---------

TEST(cameratic_handles_somente_cantos_fov) {
    CamTic c;
    const Mat4 vp = editorVp(kSW / kSH);
    const Frustum f = computeFrustum(*c.tr, *c.cam, kSW / kSH);

    // D17-c: SÓ os 4 CANTOS respondem — o handle do CENTRO/far MORREU
    // (o far edita-se no Inspector); o tap no centro é 0
    f32 cx = 0.0f, cy = 0.0f;
    EXPECT(gizmo::projectPoint(vp, f.farCenter, kSW, kSH, cx, cy));
    EXPECT(pickHandle(vp, kSW, kSH, f, cx, cy) == 0);
    f32 kx = 0.0f, ky = 0.0f;
    EXPECT(gizmo::projectPoint(vp, f.farC[1], kSW, kSH, kx, ky));
    EXPECT(pickHandle(vp, kSW, kSH, f, kx, ky) == 2);
    // longe de tudo → 0
    EXPECT(pickHandle(vp, kSW, kSH, f, 80.0f, 640.0f) == 0);

    // D17-e: a ORDEM DO DONO é gizmo > handles — a prioridade vive no
    // feedGizmo do main (o gizmo corre PRIMEIRO no press edge; o aferir
    // E2E no device virtual); o raio do handle (16dp) já não esmaga o
    // raio do eixo: um toque a 20px do CANTO é do handle, um toque a
    // 20px do CENTRO do far é NINGUÉM (o centro deixou de ser handle)
    EXPECT(pickHandle(vp, kSW, kSH, f, kx + 14.0f, ky) == 2);
    EXPECT(pickHandle(vp, kSW, kSH, f, cx + 14.0f, cy) == 0);
    (void)cx; (void)cy;

    // drag do CANTO (fov): fator radial d1/d0 — o ÚNICO drag de handle
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
    const u32 n = inspectorPlan(prof, m, false, 0u, rows);
    EXPECT(n == inspectorRowCount(prof, false, 0u));
    // secção Camera + fov + near + far + projecao + ortho + ativa + frustum
    int sect = 0, fov = 0, near_ = 0, far = 0, proj = 0, ortho = 0, act = 0;
    int frus = 0;   // 0.7.10: toggle do gizmo
    for (u32 i = 0; i < n; ++i) {
        switch (rows[i].kind) {
            case InspRow::Kind::CamSection: ++sect; break;
            case InspRow::Kind::CamFov:     ++fov; break;
            case InspRow::Kind::CamNear:    ++near_; break;
            case InspRow::Kind::CamFar:     ++far; break;
            case InspRow::Kind::CamProj:    ++proj; break;
            case InspRow::Kind::CamOrtho:   ++ortho; break;
            case InspRow::Kind::CamActive:  ++act; break;
            case InspRow::Kind::CamFrustum: ++frus; break;   // 0.7.10
            default: break;
        }
    }
    EXPECT(sect == 1 && fov == 1 && near_ == 1 && far == 1);
    EXPECT(proj == 1 && ortho == 1 && act == 1 && frus == 1);
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
    EXPECT(inspectorRowCount(p2, false, 0u) < inspectorRowCount(prof, false, 0u));
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

// ---- 12. 0.7.10 — FRUSTUM DOMADO: cap visual + prioridade + toggle -------------

TEST(cameratic_frustum_visual_clampado_mas_render_usa_far_real) {
    CamTic c;
    c.cam->farZ = 2000.0f;   // o caso extremo do C33
    c.tr->updateWorld();
    const Frustum f = computeFrustum(*c.tr, *c.cam, kSW / kSH);
    // o GIZMO desenha ao cap — compacto e legível em qualquer far
    EXPECT(nearEqF(f.drawFar, kVisualFarCap));
    EXPECT(nearEqF(length(f.farCenter - f.pos), kVisualFarCap, 1e-3f));
    // ...mas a PROJEÇÃO do jogo (gameProj) usa o far REAL — o clamp é
    // APENAS visual (o render no Play NÃO muda)
    const Mat4 pj = gameProj(*c.cam, 1.7777f);
    const Mat4 want = Mat4::perspective(1.0471975f, 1.7777f, 0.5f, 2000.0f);
    EXPECT(matNearF(pj, want));
    // ...e o Inspector continua a ver o far REAL no componente
    EXPECT(nearEqF(c.cam->farZ, 2000.0f));
}

TEST(cameratic_pick_prioridade_objetos_sobre_a_camara) {
    CamTic c;   // câmara na origem a olhar para −Z (far 500 → cone gigante)
    c.tr->updateWorld();
    // um OBJETO (TIC com MeshRenderer) DENTRO do cone, à frente da câmara
    const Handle hObj = c.scene.create("Cubo");
    Tic* obj = c.scene.get(hObj);
    Transform3D* otr = obj->addComponent<Transform3D>();
    obj->addComponent<MeshRenderer>();
    otr->pos = Vec3{0.0f, 0.0f, -6.0f};   // bem DENTRO do frustum
    otr->updateWorld();
    const Mat4 vp = editorVp(kSW / kSH);

    // tocar no OBJETO → seleciona o OBJETO (a câmara NÃO rouba — o fix do
    // C33: "seleciona a câmara em vez do objeto")
    f32 ox = 0.0f, oy = 0.0f;
    EXPECT(gizmo::projectPoint(vp, otr->pos, kSW, kSH, ox, oy));
    EXPECT(pickSceneTic(c.scene, vp, kSW, kSH, ox, oy) == hObj);

    // o objeto INVISÍVEL já não conta → o toque passa à câmara (corpo/lente)
    obj->visible = false;
    f32 bx = 0.0f, by = 0.0f;
    const Frustum f = computeFrustum(*c.tr, *c.cam, kSW / kSH);
    EXPECT(gizmo::projectPoint(vp, f.pos, kSW, kSH, bx, by));
    EXPECT(pickSceneTic(c.scene, vp, kSW, kSH, bx, by) == c.h);

    // sem objeto e longe do corpo → nada (o orbit fica LIVRE)
    obj->visible = true;
    EXPECT(!pickSceneTic(c.scene, vp, kSW, kSH, 60.0f, 660.0f).valid());

    // dois objetos: o MAIS PRÓXIMO do toque ganha
    const Handle h2 = c.scene.create("Cubo2");
    if (Tic* t2 = c.scene.get(h2)) {
        Transform3D* t2r = t2->addComponent<Transform3D>();
        t2->addComponent<MeshRenderer>();
        t2r->pos = Vec3{2.0f, 0.0f, -6.0f};
        t2r->updateWorld();
    }
    Tic* o2 = c.scene.get(h2);
    f32 tx = 0.0f, ty = 0.0f;
    EXPECT(gizmo::projectPoint(vp, o2->getComponent<Transform3D>()->pos,
                               kSW, kSH, tx, ty));
    EXPECT(pickSceneTic(c.scene, vp, kSW, kSH, tx, ty) == h2);
}

TEST(cameratic_handles_sentam_no_far_visual) {
    CamTic c;
    c.cam->farZ = 2000.0f;   // o real (Inspector/gameProj)
    c.tr->updateWorld();
    const Mat4 vp = editorVp(kSW / kSH);
    const Frustum f = computeFrustum(*c.tr, *c.cam, kSW / kSH);   // clampado

    // D17: os handles PARTILHAM a geometria do desenho — os CANTOS estão
    // no retângulo AO CAP (12 u), não a 2000; o CENTRO já não tem handle
    f32 kx = 0.0f, ky = 0.0f;
    EXPECT(gizmo::projectPoint(vp, f.farC[0], kSW, kSH, kx, ky));
    EXPECT(pickHandle(vp, kSW, kSH, f, kx, ky) == 1);
    // o far REAL projetado está longe/fora — NÃO há handle lá
    const Frustum real = computeFrustum(*c.tr, *c.cam, kSW / kSH, 4000.0f);
    f32 rx = 0.0f, ry = 0.0f;
    if (gizmo::projectPoint(vp, real.farC[0], kSW, kSH, rx, ry)) {
        EXPECT(pickHandle(vp, kSW, kSH, f, rx, ry) == 0);
    }
}

// ---- 0.9.6.19 (D17) — OS PINS VISUAIS: mudo a 35% sem seleção, âmbar
// com seleção, handles ≤12dp SÓ com seleção, glifo sempre presente -------
TEST(cameratic_d17_objeto_pequeno_estados_e_medidas) {
    CamTic c;
    c.tr->updateWorld();
    UiContext ui;
    ui.init();
    InputState in;
    const Mat4 vp = editorVp(kSW / kSH);

    // SEM seleção: o frustum mudo (alfa ~0.35 no batch) + ZERO handles
    ui.beginFrame(nullptr, &in, kSW, kSH);
    drawAll(ui, c.scene, vp, kSW, kSH, Handle{});
    ui.endFrame();
    {
        const auto& vb = ui.solidsForTest();
        u32 muted = 0, amber = 0;
        const f32* accent = vv::theme::kTheme.accent;
        for (u32 i = 0; i + 5 < vb.vertexCount(); i += 6) {
            const auto& v = vb.vertices()[i];
            (void)v;
            const auto& v0 = vb.vertices()[i];
            if (v0.a > 0.30f && v0.a < 0.40f) {
                ++muted;   // o cinza mudo ~35%
            }
            if (v0.a > 0.95f && v0.r == accent[0] && v0.g == accent[1] &&
                v0.b == accent[2]) {
                ++amber;   // âmbar cheio (handle) — não deve existir
            }
        }
        EXPECT(muted > 0u);   // o frustum mudo está presente (pin)
        EXPECT(amber == 0u);  // ZERO handles sem seleção (pin)
    }

    // COM seleção: âmbar + os handles de canto ≤12dp
    ui.beginFrame(nullptr, &in, kSW, kSH);
    drawAll(ui, c.scene, vp, kSW, kSH, c.h);
    ui.endFrame();
    {
        const auto& vb = ui.solidsForTest();
        const f32* accent = vv::theme::kTheme.accent;
        u32 amber = 0;
        f32 maxHandle = 0.0f;
        // um canto do far projetado (os handles vivem à volta dele) — o
        // MESMO frustum que o draw usa (o cap VISUAL de ecrã, não o default)
        f32 hx = 0.0f, hy = 0.0f;
        const Frustum f = computeFrustum(
            *c.tr, *c.cam, kSW / kSH,
            visualCapForScreen(vp, kSW, kSH, c.tr->pos, c.cam->fovY));
        ASSERT(gizmo::projectPoint(vp, f.farC[0], kSW, kSH, hx, hy));
        const f32 tol = theme::dp(13.0f);   // o piso 12dp + a moldura
        for (u32 i = 0; i + 5 < vb.vertexCount(); i += 6) {
            // bbox do quad (6 vértices)
            f32 minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
            for (u32 k = 0; k < 6; ++k) {
                const auto& v = vb.vertices()[i + k];
                if (v.x < minX) minX = v.x;
                if (v.x > maxX) maxX = v.x;
                if (v.y < minY) minY = v.y;
                if (v.y > maxY) maxY = v.y;
            }
            const f32* v0c = nullptr;
            f32 a = vb.vertices()[i].a;
            (void)v0c;
            if (a > 0.95f && vb.vertices()[i].r == accent[0] &&
                vb.vertices()[i].g == accent[1] &&
                vb.vertices()[i].b == accent[2]) {
                ++amber;
            }
            // quads INTEIROS dentro do raio do canto (os handles); linhas
            // longas do frustum não cabem no raio — só os handles medem
            if (maxX - minX <= tol && maxY - minY <= tol &&
                minX >= hx - theme::dp(9.0f) &&
                maxX <= hx + theme::dp(9.0f) &&
                minY >= hy - theme::dp(9.0f) &&
                maxY <= hy + theme::dp(9.0f)) {
                const f32 ext = (maxX - minX) > (maxY - minY) ? (maxX - minX)
                                                              : (maxY - minY);
                if (ext > maxHandle) {
                    maxHandle = ext;
                }
            }
        }
        EXPECT(amber > 0u);   // a seleção é âmbar (pin)
        EXPECT(maxHandle > theme::dp(11.0f));   // o handle de 12dp existe
        EXPECT_MSG(maxHandle <= theme::dp(13.0f),
                   "D17: o handle mediu %.1fpx (limite 12dp+moldura)",
                   (double)maxHandle);   // NUNCA o quadrado gigante de 26dp
    }
}

TEST(cameratic_toggle_frustum_esconde_o_gizmo) {
    CamTic c;
    c.tr->updateWorld();
    UiContext ui;
    ui.init();
    InputState in;
    const Mat4 vp = editorVp(kSW / kSH);

    // visível (default) → linhas no batch
    ui.beginFrame(nullptr, &in, kSW, kSH);
    drawAll(ui, c.scene, vp, kSW, kSH, c.h);
    ui.endFrame();
    const u32 comFrustum = ui.solidsForTest().vertexCount();
    EXPECT(comFrustum > 0u);

    // toggle OFF no Inspector → o gizmo SOME (a câmara continua na cena e
    // a valer para o render — só o desenho editor desaparece)
    c.cam->showFrustum = false;
    ui.beginFrame(nullptr, &in, kSW, kSH);
    drawAll(ui, c.scene, vp, kSW, kSH, c.h);
    ui.endFrame();
    EXPECT(ui.solidsForTest().vertexCount() == 0u);

    // serialização do toggle: false grava "frustum":false e volta; o
    // default (true) NÃO gravado (ficheiros 0.7.9 abrem limpos)
    const SceneSerializer::LoadCtx ctx;
    const std::string text = SceneSerializer::dump(c.scene);
    EXPECT(text.find("\"frustum\"") != std::string::npos);
    Scene b;
    EXPECT(SceneSerializer::loadText(b, text, ctx));
    Tic* lb = nullptr;
    b.forEachActive([&](Tic& t) { lb = &t; });
    if (const CameraComp* cc = lb ? lb->getComponent<CameraComp>() : nullptr) {
        EXPECT(!cc->showFrustum);
        EXPECT(nearEqF(cc->farZ, 500.0f));   // o resto intacto
    }
    // default: sem a chave
    CamTic d;
    const std::string dtext = SceneSerializer::dump(d.scene);
    EXPECT(dtext.find("\"frustum\"") == std::string::npos);
}

// ---------------------------------------------------------------------------
// FASE 9 (G1-6 — a ÚNICA mudança de lógica da fase): tocar o CORPO de um
// TIC grande SELECIONA-o. O hit-test antigo media a distância ao CENTRO
// projetado (teto 44 px) — um cubo escalado a 10 tinha corpo inteiro "morto"
// e o tap LIMPAVA a seleção (viewportTapClearsSelection). AGORA: o AABB do
// mesh projetado (8 cantos pela matriz world) é o alvo; 44 px fica como
// piso; entre dois acertados ganha o MAIS PRÓXIMO DA CÂMARA.
// ---------------------------------------------------------------------------
TEST(cameratic_pick_pelo_corpo_g16) {
    CamTic c;
    c.tr->updateWorld();

    // um cubo GRANDE (mesh real, escala 10) à frente da câmara
    Mesh big;
    const CubeMeshData cube = makeCube(1.0f);
    ASSERT(big.create(cube.vertices.data(),
                      static_cast<u32>(cube.vertices.size()),
                      cube.indices.data(),
                      static_cast<u32>(cube.indices.size())));
    const Handle hBig = c.scene.create("Grande");
    Tic* bigTic = c.scene.get(hBig);
    Transform3D* btr = bigTic->addComponent<Transform3D>();
    MeshRenderer* bmr = bigTic->addComponent<MeshRenderer>();
    bmr->mesh = &big;
    btr->pos = Vec3{0.0f, 0.0f, -6.0f};
    btr->scale = Vec3{10.0f, 10.0f, 10.0f};
    btr->updateWorld();
    const Mat4 vp = editorVp(kSW / kSH);
    // o OLHO da orbit (para o teste de profundidade ser determinístico:
    // cubos ao longo do raio eye→alvo projetam NO MESMO ponto)
    const gizmo::ViewBasis vb = gizmo::viewBasis(
        [] { Camera cc; return cc; }(), kSW / kSH);

    // o CENTRO projeta longe do canto do corpo — o toque no CANTO do corpo
    // (fora dos 44 px do centro) tem de SELECIONAR (o bug do dono)
    f32 cx = 0.0f, cy = 0.0f;
    EXPECT(gizmo::projectPoint(vp, btr->pos, kSW, kSH, cx, cy));
    // um canto do AABB em ecrã (makeCube(1) tem bounds ±0,5 — a ESCALA 10
    // do transform leva-o a ±5 no mundo)
    const Vec3 corner{0.5f, -0.5f, 0.5f};   // canto inferior direito frontal
    const Vec3 worldCorner{
        btr->world.m[0] * corner.x + btr->world.m[4] * corner.y +
            btr->world.m[8] * corner.z + btr->world.m[12],
        btr->world.m[1] * corner.x + btr->world.m[5] * corner.y +
            btr->world.m[9] * corner.z + btr->world.m[13],
        btr->world.m[2] * corner.x + btr->world.m[6] * corner.y +
            btr->world.m[10] * corner.z + btr->world.m[14]};
    f32 bx = 0.0f, by = 0.0f;
    EXPECT(gizmo::projectPoint(vp, worldCorner, kSW, kSH, bx, by));
    // o canto está LONGE do centro (> 60 px — fora do raio antigo de 44)
    const f32 dist = std::sqrt((bx - cx) * (bx - cx) + (by - cy) * (by - cy));
    EXPECT(dist > 60.0f);
    // ...e o toque NELE seleciona o TIC (o corpo é alvo — G1-6)
    EXPECT(pickSceneTic(c.scene, vp, kSW, kSH, bx, by) == hBig);

    // fora do corpo (e do raio do centro): NADA (o orbit fica livre)
    EXPECT(!pickSceneTic(c.scene, vp, kSW, kSH, 60.0f, 660.0f).valid());

    // DOIS cubos SOBREPOSTOS no ecrã: ganha o MAIS PRÓXIMO DA CÂMARA (a
    // regra antiga era "o mais próximo do TOQUE" — trocava na sobreposição).
    // O alvo da orbit é a ORIGEM: um pequeno no raio eye→origem (75% do
    // caminho) projeta NO MESMO PONTO que a origem e está MAIS PERTO.
    Mesh small;
    const CubeMeshData cube2 = makeCube(1.0f);
    ASSERT(small.create(cube2.vertices.data(),
                        static_cast<u32>(cube2.vertices.size()),
                        cube2.indices.data(),
                        static_cast<u32>(cube2.indices.size())));
    const Handle hNear = c.scene.create("Perto");
    Tic* nearTic = c.scene.get(hNear);
    Transform3D* ntr = nearTic->addComponent<Transform3D>();
    MeshRenderer* nmr = nearTic->addComponent<MeshRenderer>();
    nmr->mesh = &small;
    // 75% do caminho eye→origem (a 25% da distância da câmara)
    const Vec3 ray = Vec3{0.0f, 0.0f, 0.0f} - vb.eye;
    ntr->pos = vb.eye + ray * 0.25f;
    ntr->updateWorld();
    f32 ncx = 0.0f, ncy = 0.0f;
    EXPECT(gizmo::projectPoint(vp, ntr->pos, kSW, kSH, ncx, ncy));
    // o toque no centro COMUM (o pequeno projeta no raio; o grande cobre):
    // os DOIS acertam; o PERTO (menor depth) vence
    EXPECT(pickSceneTic(c.scene, vp, kSW, kSH, ncx, ncy) == hNear);
    // e ATRÁS (125% do caminho — dentro do volume do grande, lado de lá):
    // o GRANDE vence (a face dele está mais perto da câmara)
    ntr->pos = vb.eye + ray * 1.25f;
    ntr->updateWorld();
    EXPECT(pickSceneTic(c.scene, vp, kSW, kSH, ncx, ncy) == hBig);

    // TIC com MeshRenderer SEM mesh (o caso dos presets pré-bind): a regra
    // de sempre — centro 44 px — continua a funcionar (cena LIMPA: o cubo
    // gigante cobriria o ecrã inteiro e esconderia o teste)
    {
        Scene bare;
        const Handle hBare = bare.create("SemMesh");
        Tic* bareTic = bare.get(hBare);
        Transform3D* bareTr = bareTic->addComponent<Transform3D>();
        bareTic->addComponent<MeshRenderer>();
        bareTr->pos = Vec3{0.0f, 0.0f, -4.0f};
        bareTr->updateWorld();
        f32 sx = 0.0f, sy = 0.0f;
        EXPECT(gizmo::projectPoint(vp, bareTr->pos, kSW, kSH, sx, sy));
        EXPECT(pickSceneTic(bare, vp, kSW, kSH, sx, sy) == hBare);
    }
}
