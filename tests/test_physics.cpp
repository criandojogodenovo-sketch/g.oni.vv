// tests/test_physics.cpp — F4-B..E: BodyComp no registry/serializer, sistema
// de física (Character slide/repel, Rigid primitivo), input (InputSource/
// TouchControls) e presets. Tudo GL-free (a física é C++ puro).
#include "TestFramework.h"
#include <cstdio>
#include "components/BodyComp.h"
#include "components/InputMap.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/ComponentStore.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "core/TransformSystem.h"
#include "physics/InputSource.h"
#include "physics/PhysicsSystem.h"
#include <cstring>

using namespace vv;
using ::test::nearEqF;
using ::test::vecNearF;

namespace {

// fonte de input falsa para os testes de sistema (GL-free)
struct FakeSource : InputSource {
    Vec2 ax{0.0f, 0.0f};
    bool jump = false;
    Vec2 axis() const override { return ax; }
    bool action(const char* n) const override {
        return jump && std::strcmp(n, "jump") == 0;
    }
};

Handle makeBody(Scene& s, const char* name, BodyType t, const Vec3& pos,
                BodyShape shape) {
    const Handle h = s.create(name);
    Transform3D* tr = s.get(h)->addComponent<Transform3D>();
    tr->pos = pos;
    tr->updateWorld();
    BodyComp* b = s.get(h)->addComponent<BodyComp>();
    b->type = t;
    b->shape = shape;
    return h;
}

const BodyComp* bodyOf(Scene& s, Handle h) {
    return s.get(h)->getComponent<BodyComp>();
}

} // namespace

// ---- registry -----------------------------------------------------------------

TEST(bodycomp_registo_factory_e_ciclo) {
    Scene s;
    ComponentStore& store = s.components();
    const Handle h = s.create("corpo");

    EXPECT(store.registry().find("BodyComp") == 3);   // ordem fixa do ctor
    EXPECT(store.registry().create("BodyComp", store, h));
    BodyComp* b = store.get<BodyComp>(h);
    EXPECT(b != nullptr);
    EXPECT(b->type == BodyType::Static);              // default
    EXPECT(b->shape.index() == 3u);                   // Capsule (default do variant)
    EXPECT(b->owner == s.get(h));

    b->type = BodyType::Character;
    b->velocity = Vec3{1, 2, 3};
    EXPECT(store.remove<BodyComp>(h));
    EXPECT(store.get<BodyComp>(h) == nullptr);

    // removeAll (Scene::destroy) limpa o corpo
    EXPECT(store.registry().create("BodyComp", store, h));
    EXPECT(s.destroy(h));
    EXPECT(store.bodies().size() == 0u);
}

// ---- worldDirty (invalidação do cache world — F4-B.7) ----------------------------

TEST(world_dirty_marcado_pela_fisica_e_limpo_ao_refrescar) {
    Scene s;
    const Handle h = s.create("tic");
    Transform3D* tr = s.get(h)->addComponent<Transform3D>();
    tr->updateWorld();

    // "física move pos": marca dirty sem recomputar o cache
    tr->pos = Vec3{1, 0, 0};
    tr->worldDirty = true;
    EXPECT(tr->worldDirty);
    f32 out[4];
    Mat4::transformPoint4(tr->world, Vec3{0, 0, 0}, out);
    EXPECT(nearEqF(out[0], 0.0f));   // cache ainda obsoleto

    // TransformSystem (Update) refresca e limpa
    TransformSystem sys;
    sys.tick(s, 1.0f / 60.0f);
    EXPECT(!tr->worldDirty);
    Mat4::transformPoint4(tr->world, Vec3{0, 0, 0}, out);
    EXPECT(nearEqF(out[0], 1.0f));

    // updateWorld() direto também limpa
    tr->pos = Vec3{2, 0, 0};
    tr->worldDirty = true;
    tr->updateWorld();
    EXPECT(!tr->worldDirty);
}

// ---- serializer round-trip ---------------------------------------------------------

TEST(bodycomp_serializer_roundtrip) {
    Scene a;
    const Handle h = a.create("PlayerBody3D");
    Transform3D* tr = a.get(h)->addComponent<Transform3D>();
    tr->pos = Vec3{1.5f, 0.55f, -2.0f};
    tr->updateWorld();
    BodyComp* b = a.get(h)->addComponent<BodyComp>();
    b->type = BodyType::Character;
    b->shape = phys::Capsule{Vec3{0, 0.1f, 0}, 0.35f, 0.4f};
    b->velocity = Vec3{9, 8, 7};   // runtime — NÃO deve persistir
    b->grounded = true;

    const char* path = "/tmp/goni_test_body.goni";
    EXPECT(SceneSerializer::save(a, path));

    Scene c;
    SceneSerializer::LoadCtx ctx;
    EXPECT(SceneSerializer::load(c, path, ctx));
    EXPECT(c.count() == 1u);

    Tic* tic = c.get(c.find("PlayerBody3D"));
    EXPECT(tic != nullptr);
    const BodyComp* lb = tic->getComponent<BodyComp>();
    EXPECT(lb != nullptr);
    EXPECT(lb->type == BodyType::Character);
    EXPECT(lb->shape.index() == 3u);
    const phys::Capsule& cp = std::get<phys::Capsule>(lb->shape);
    EXPECT(nearEqF(cp.radius, 0.35f));
    EXPECT(nearEqF(cp.halfHeight, 0.4f));
    EXPECT(vecNearF(cp.center, Vec3{0, 0.1f, 0}));
    EXPECT(nearEqF(lb->velocity.x, 0.0f) && nearEqF(lb->velocity.z, 0.0f)); // repouso
    EXPECT(!lb->grounded);
    const Transform3D* ltr = tic->getComponent<Transform3D>();
    EXPECT(ltr != nullptr && vecNearF(ltr->pos, Vec3{1.5f, 0.55f, -2.0f}));
}

TEST(bodycomp_serializer_todas_as_formas) {
    struct Case { const char* nome; BodyType tipo; BodyShape sh; u32 idx; };
    Scene a;
    const Handle h0 = a.create("b_static_obb");
    {
        BodyComp* b = a.get(h0)->addComponent<BodyComp>();
        b->type = BodyType::Static;
        phys::OBB ob{};
        ob.halfExtents = Vec3{2, 0.25f, 0.1f};
        ob.rot = Quat::axisAngle(Vec3{0, 1, 0}, 0.5f);
        b->shape = ob;
    }
    const Handle h1 = a.create("b_rigid_sphere");
    {
        BodyComp* b = a.get(h1)->addComponent<BodyComp>();
        b->type = BodyType::Rigid;
        b->shape = phys::Sphere{Vec3{0, 0.2f, 0}, 0.45f};
    }
    const Handle h2 = a.create("b_static_aabb");
    {
        BodyComp* b = a.get(h2)->addComponent<BodyComp>();
        b->type = BodyType::Static;
        b->shape = phys::AABB{Vec3{-4, -1, -4}, Vec3{4, 0, 4}};
    }

    const char* path = "/tmp/goni_test_shapes.goni";
    EXPECT(SceneSerializer::save(a, path));
    Scene c;
    EXPECT(SceneSerializer::load(c, path, SceneSerializer::LoadCtx{}));

    const BodyComp* ob = c.get(c.find("b_static_obb"))->getComponent<BodyComp>();
    EXPECT(ob && ob->type == BodyType::Static && ob->shape.index() == 2u);
    const phys::OBB& o = std::get<phys::OBB>(ob->shape);
    EXPECT(nearEqF(o.halfExtents.x, 2.0f) && nearEqF(o.halfExtents.z, 0.1f));
    EXPECT(nearEqF(o.rot.y, std::sin(0.25f), 1e-4f));   // quat de yaw 0.5

    const BodyComp* es = c.get(c.find("b_rigid_sphere"))->getComponent<BodyComp>();
    EXPECT(es && es->type == BodyType::Rigid && es->shape.index() == 0u);
    EXPECT(nearEqF(std::get<phys::Sphere>(es->shape).r, 0.45f));

    const BodyComp* ax = c.get(c.find("b_static_aabb"))->getComponent<BodyComp>();
    EXPECT(ax && ax->shape.index() == 1u);
    EXPECT(nearEqF(std::get<phys::AABB>(ax->shape).max.x, 4.0f));
}

// ---- PhysicsSystem: Character (F4-B) ----------------------------------------

TEST(character_sem_input_fica_parado_e_cai) {
    Scene s;
    const Handle p = makeBody(s, "player", BodyType::Character,
                              Vec3{0, 0.55f, 0},
                              phys::Capsule{Vec3{0, 0, 0}, 0.3f, 0.25f});
    phys::PhysicsSystem sys;
    sys.enabled = true;
    sys.tick(s, 1.0f / 60.0f);

    const BodyComp* b = bodyOf(s, p);
    EXPECT(nearEqF(b->velocity.x, 0.0f));   // sem fonte → sem input horizontal
    EXPECT(nearEqF(b->velocity.z, 0.0f));
    const Transform3D* tr = s.get(p)->getComponent<Transform3D>();
    EXPECT(nearEqF(tr->pos.x, 0.0f) && nearEqF(tr->pos.z, 0.0f));
    EXPECT(tr->pos.y < 0.55f);              // gravidade aplica-se ao Character
    EXPECT(tr->worldDirty == false);        // writeBack refresca o cache
}

TEST(character_input_move_na_base_da_camara) {
    Scene s;
    const Handle p = makeBody(s, "player", BodyType::Character,
                              Vec3{0, 0.55f, 0},
                              phys::Capsule{Vec3{0, 0, 0}, 0.3f, 0.25f});
    FakeSource src;
    s.get(p)->addComponent<InputMap>()->source = &src;
    src.ax = Vec2{1.0f, 0.0f};   // stick para a direita

    phys::PhysicsSystem sys;
    sys.enabled = true;
    sys.tick(s, 1.0f / 60.0f);

    const BodyComp* b = bodyOf(s, p);
    EXPECT(nearEqF(b->velocity.x, sys.charSpeed, 1e-3f));
    const Transform3D* tr = s.get(p)->getComponent<Transform3D>();
    EXPECT(nearEqF(tr->pos.x, sys.charSpeed / 60.0f, 1e-4f));   // frame.right=(1,0,0)
}

TEST(character_colide_e_desliza_na_parede_sem_atravessar) {
    Scene s;
    // chão (topo em y=0) + parede em x∈[0.7, 2]
    makeBody(s, "chao", BodyType::Static, Vec3{0, -0.5f, 0},
             phys::OBB{Vec3{0, 0, 0}, Vec3{10, 0.5f, 10}, Quat::identity()});
    makeBody(s, "parede", BodyType::Static, Vec3{1.35f, 1.0f, 0},
             phys::OBB{Vec3{0, 0, 0}, Vec3{0.65f, 1.0f, 1.0f}, Quat::identity()});
    const Handle p = makeBody(s, "player", BodyType::Character,
                              Vec3{0, 0.55f, 0},
                              phys::Capsule{Vec3{0, 0, 0}, 0.3f, 0.25f});
    FakeSource src;
    s.get(p)->addComponent<InputMap>()->source = &src;
    src.ax = Vec2{1.0f, 1.0f};   // diagonal: entra na parede E anda em −z

    phys::PhysicsSystem sys;
    sys.enabled = true;
    sys.frame.fwd = Vec3{0, 0, -1};
    // 9 ticks: z ≈ −0.75 — ainda DENTRO do vão da parede (z∈[−1,1])
    for (int i = 0; i < 9; ++i) {
        sys.tick(s, 1.0f / 60.0f);
    }

    const Transform3D* tr = s.get(p)->getComponent<Transform3D>();
    // contacto em x = 0.7 − 0.3 (raio) — nunca passa; −z tangencial avança
    EXPECT(tr->pos.x <= 0.4f + 1e-3f);
    EXPECT(tr->pos.x >= 0.35f);
    EXPECT(tr->pos.z < -0.5f);              // deslizou ao longo da parede
    EXPECT(bodyOf(s, p)->grounded);         // assente no chão durante o slide

    // 8 ticks extra: passa o CANTO da parede (z < −1) e o x volta a avançar
    for (int i = 0; i < 8; ++i) {
        sys.tick(s, 1.0f / 60.0f);
    }
    EXPECT(s.get(p)->getComponent<Transform3D>()->pos.x > 0.41f);   // solto do canto
}

TEST(character_ground_jump_e_queda) {
    Scene s;
    makeBody(s, "chao", BodyType::Static, Vec3{0, -0.5f, 0},
             phys::OBB{Vec3{0, 0, 0}, Vec3{10, 0.5f, 10}, Quat::identity()});
    const Handle p = makeBody(s, "player", BodyType::Character,
                              Vec3{0, 0.55f, 0},
                              phys::Capsule{Vec3{0, 0, 0}, 0.3f, 0.25f});
    FakeSource src;
    s.get(p)->addComponent<InputMap>()->source = &src;

    phys::PhysicsSystem sys;
    sys.enabled = true;

    sys.tick(s, 1.0f / 60.0f);              // assenta no chão
    EXPECT(bodyOf(s, p)->grounded);
    EXPECT(nearEqF(bodyOf(s, p)->velocity.y, 0.0f));
    EXPECT(nearEqF(s.get(p)->getComponent<Transform3D>()->pos.y, 0.55f, 1e-3f));

    src.jump = true;                        // salta (grounded)
    sys.tick(s, 1.0f / 60.0f);
    EXPECT(!bodyOf(s, p)->grounded);
    EXPECT(s.get(p)->getComponent<Transform3D>()->pos.y > 0.6f);   // subiu
    src.jump = false;
    for (int i = 0; i < 90; ++i) {          // volta ao chão
        sys.tick(s, 1.0f / 60.0f);
    }
    EXPECT(bodyOf(s, p)->grounded);
    EXPECT(s.get(p)->getComponent<Transform3D>()->pos.y <= 0.551f);
}

TEST(characters_sobrepostos_repelem_se_mutuamente) {
    Scene s;
    const phys::Capsule cap{Vec3{0, 0, 0}, 0.3f, 0.25f};
    const Handle a = makeBody(s, "a", BodyType::Character, Vec3{0, 0.55f, 0}, cap);
    const Handle b = makeBody(s, "b", BodyType::Character, Vec3{0.4f, 0.55f, 0}, cap);

    phys::PhysicsSystem sys;
    sys.enabled = true;
    sys.gravity = 0.0f;   // isola o repel horizontal
    sys.tick(s, 1.0f / 60.0f);

    const f32 dxa = s.get(a)->getComponent<Transform3D>()->pos.x;
    const f32 dxb = s.get(b)->getComponent<Transform3D>()->pos.x;
    EXPECT(dxb - dxa >= 0.6f - 1e-3f);   // soma dos raios — afastados
    // A (à esquerda, processado primeiro) absorve a separação para −x
    EXPECT(dxa < -0.05f);
    EXPECT(dxb > 0.39f);   // B não é invadido (fica onde estava ou recua)
}

// ---- PhysicsSystem: Rigid (F4-C) -----------------------------------------------

TEST(rigid_cai_para_no_chao_e_nunca_atravessa) {
    Scene s;
    makeBody(s, "chao", BodyType::Static, Vec3{0, -0.5f, 0},
             phys::OBB{Vec3{0, 0, 0}, Vec3{10, 0.5f, 10}, Quat::identity()});
    const Handle b = makeBody(s, "bola", BodyType::Rigid, Vec3{0, 3.0f, 0},
                              phys::Sphere{Vec3{0, 0, 0}, 0.5f});

    phys::PhysicsSystem sys;
    sys.enabled = true;
    f32 minY = 3.0f;
    for (int i = 0; i < 120; ++i) {           // 2 s: cai de 3 e assenta
        sys.tick(s, 1.0f / 60.0f);
        const f32 y = s.get(b)->getComponent<Transform3D>()->pos.y;
        if (y < minY) minY = y;
        EXPECT(y >= 0.499f);                  // fundo da esfera nunca passa y=0
    }
    const BodyComp* bc = bodyOf(s, b);
    EXPECT(bc->grounded);
    EXPECT(nearEqF(bc->velocity.y, 0.0f));
    EXPECT(nearEqF(minY, 0.5f, 5e-3f));       // repousa com o fundo no chão
}

TEST(rigid_lancado_nao_atravessa_parede_fina) {
    Scene s;
    makeBody(s, "chao", BodyType::Static, Vec3{0, -0.5f, 0},
             phys::OBB{Vec3{0, 0, 0}, Vec3{10, 0.5f, 10}, Quat::identity()});
    makeBody(s, "parede", BodyType::Static, Vec3{1.35f, 1.0f, 0},
             phys::OBB{Vec3{0, 0, 0}, Vec3{0.65f, 1.0f, 1.0f}, Quat::identity()});
    const Handle b = makeBody(s, "bala", BodyType::Rigid, Vec3{-5, 0.5f, 0},
                              phys::Sphere{Vec3{0, 0, 0}, 0.5f});
    s.get(b)->getComponent<BodyComp>()->velocity = Vec3{60.0f, 0.0f, 0.0f};   // 1 unidade/tick

    phys::PhysicsSystem sys;
    sys.enabled = true;
    for (int i = 0; i < 30; ++i) {
        sys.tick(s, 1.0f / 60.0f);
        const f32 x = s.get(b)->getComponent<Transform3D>()->pos.x;
        EXPECT(x <= 0.2f + 1e-2f);            // face da parede (0.7) − raio
    }
    EXPECT(bodyOf(s, b)->grounded);
}

TEST(rigid_empurrado_desliza_e_abranda_no_chao) {
    Scene s;
    makeBody(s, "chao", BodyType::Static, Vec3{0, -0.5f, 0},
             phys::OBB{Vec3{0, 0, 0}, Vec3{10, 0.5f, 10}, Quat::identity()});
    const Handle b = makeBody(s, "bola", BodyType::Rigid, Vec3{-2, 0.5f, 0},
                              phys::Sphere{Vec3{0, 0, 0}, 0.5f});
    s.get(b)->getComponent<BodyComp>()->velocity = Vec3{8.0f, 0.0f, 0.0f};    // empurrão

    phys::PhysicsSystem sys;
    sys.enabled = true;
    f32 x0 = -2.0f, maxX = -2.0f, prevVel = 8.0f;
    for (int i = 0; i < 60; ++i) {
        sys.tick(s, 1.0f / 60.0f);
        const f32 x = s.get(b)->getComponent<Transform3D>()->pos.x;
        const f32 vx = bodyOf(s, b)->velocity.x;
        EXPECT(x > x0 - 1e-4f);               // nunca recua
        maxX = x > maxX ? x : maxX;
        EXPECT(vx <= prevVel + 1e-4f);        // amortecimento só abranda
        prevVel = vx;
        x0 = x;
    }
    EXPECT(maxX > -0.8f);                     // deslizou ~1.2+ (8 u/s com damping 6/s)
    EXPECT(prevVel < 0.5f);                   // e abrandou quase a parar
}
