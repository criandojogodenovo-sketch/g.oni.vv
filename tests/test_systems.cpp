// tests/test_systems.cpp — F3/F4: TickGroups (ordem Pre→Up→Physics→Post→Render)
// e TransformSystem (cache world atualizado a partir de pos/rot/scale).
#include "TestFramework.h"
#include <vector>
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/Scene.h"
#include "core/Tick.h"
#include "core/TransformSystem.h"

using namespace vv;
using ::test::nearEqF;

namespace {

struct RecordingSystem : System {
    u32                              id;
    std::vector<u32>*                log;
    explicit RecordingSystem(u32 i, std::vector<u32>* l) : id(i), log(l) {}
    void tick(Scene& /*scene*/, f32 /*dt*/) override { log->push_back(id); }
};

} // namespace

TEST(tickgroups_rodam_na_ordem_pre_update_post_render) {
    Scene s;
    std::vector<u32> log;

    RecordingSystem pre(0, &log), up(1, &log), fis(2, &log), post(3, &log), render(4, &log);
    // registra FORA da ordem — a ordem vem do grupo, não do registo
    TickGroups tg;
    EXPECT(tg.add(TickGroup::Render, &render));
    EXPECT(tg.add(TickGroup::PostUpdate, &post));
    EXPECT(tg.add(TickGroup::PreUpdate, &pre));
    EXPECT(tg.add(TickGroup::Update, &up));
    EXPECT(tg.add(TickGroup::Physics, &fis));

    tg.run(s, 1.0f / 60.0f);
    EXPECT(log.size() == 5u);
    EXPECT(log[0] == 0u && log[1] == 1u && log[2] == 2u && log[3] == 3u && log[4] == 4u);

    // dois no mesmo grupo: ordem de registo dentro do grupo
    log.clear();
    RecordingSystem up2(5, &log);
    EXPECT(tg.add(TickGroup::Update, &up2));
    tg.run(s, 1.0f / 60.0f);
    EXPECT(log.size() == 6u);
    EXPECT(log[1] == 1u && log[2] == 5u);   // up (1) antes de up2 (5)
}

TEST(tickgroups_recusa_excesso_e_nulos) {
    Scene s;
    std::vector<u32> log;
    TickGroups tg;
    EXPECT(!tg.add(TickGroup::Update, nullptr));
    RecordingSystem rec(0, &log);
    for (u32 i = 0; i < TickGroups::kMaxSystemsPerGroup; ++i) {
        EXPECT(tg.add(TickGroup::Update, &rec));
    }
    EXPECT(!tg.add(TickGroup::Update, &rec));   // grupo cheio
    EXPECT(tg.count(TickGroup::Update) == TickGroups::kMaxSystemsPerGroup);
}

TEST(transform_system_atualiza_cache_world) {
    Scene s;
    const Handle h = s.create("cubo");
    Transform3D* tr = s.get(h)->addComponent<Transform3D>();
    tr->pos = Vec3{1.0f, 2.0f, 3.0f};
    tr->rot = Quat::axisAngle(Vec3{0.0f, 1.0f, 0.0f}, 1.5707963f);   // +90° yaw
    tr->scale = Vec3{1.0f, 1.0f, 1.0f};

    // cache obsoleto (identidade) — como se ninguém tivesse chamado updateWorld
    tr->world = Mat4::identity();

    TransformSystem sys;
    TickGroups tg;
    EXPECT(tg.add(TickGroup::Update, &sys));
    tg.run(s, 1.0f / 60.0f);

    // local (1,0,0) → rotY90 → (0,0,-1) → +pos = (1,2,2)
    f32 out[4];
    Mat4::transformPoint4(tr->world, Vec3{1.0f, 0.0f, 0.0f}, out);
    EXPECT(nearEqF(out[0], 1.0f, 1e-4f));
    EXPECT(nearEqF(out[1], 2.0f, 1e-4f));
    EXPECT(nearEqF(out[2], 2.0f, 1e-4f));

    // alteração DEPOIS do passo só aparece no mundo após o próximo tick
    tr->pos = Vec3{0.0f, 0.5f, 0.0f};
    Mat4::transformPoint4(tr->world, Vec3{0.0f, 0.0f, 0.0f}, out);
    EXPECT(nearEqF(out[0], 1.0f, 1e-4f));   // ainda com o cache antigo
    tg.run(s, 1.0f / 60.0f);
    Mat4::transformPoint4(tr->world, Vec3{0.0f, 0.0f, 0.0f}, out);
    EXPECT(nearEqF(out[0], 0.0f) && nearEqF(out[1], 0.5f) && nearEqF(out[2], 0.0f));
}

TEST(transform_system_cobre_todos_os_transforms) {
    Scene s;
    for (int i = 0; i < 5; ++i) {
        const char* names[5] = {"a", "b", "c", "d", "e"};
        const Handle h = s.create(names[i]);
        Transform3D* tr = s.get(h)->addComponent<Transform3D>();
        tr->pos = Vec3{static_cast<f32>(i), 0.0f, 0.0f};
        tr->world = Mat4::identity();   // força estado obsoleto
    }

    TransformSystem sys;
    TickGroups tg;
    tg.add(TickGroup::Update, &sys);
    tg.run(s, 1.0f / 60.0f);

    auto& transforms = s.components().transforms();
    EXPECT(transforms.size() == 5u);
    for (u32 i = 0; i < transforms.size(); ++i) {
        const Transform3D& tr = transforms.at(i);
        f32 out[4];
        Mat4::transformPoint4(tr.world, Vec3{0.0f, 0.0f, 0.0f}, out);
        EXPECT(nearEqF(out[0], static_cast<f32>(i)) && nearEqF(out[1], 0.0f) &&
               nearEqF(out[2], 0.0f));
    }
}
