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

using namespace vv;
using ::test::nearEqF;
using ::test::vecNearF;

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
