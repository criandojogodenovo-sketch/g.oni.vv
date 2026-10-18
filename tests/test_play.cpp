// tests/test_play.cpp — F4.2/B3: sandbox do modo Play (core/PlaySnapshot.h).
// Fluxo do bug do C33: entrar em Play → corpo cai/move → sair do Play → a
// pose de editor TEM de ser reposta (pos/rot/scale + velocity/grounded), com
// a física a correr SÓ durante o Play (gate `enabled` inalterado).
#include "TestFramework.h"
#include "components/BodyComp.h"
#include "components/Transform3D.h"
#include "core/PlaySnapshot.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "physics/PhysicsSystem.h"

using namespace vv;
using ::test::matNearF;
using ::test::nearEqF;
using ::test::vecNearF;

namespace {

bool quatNear(const Quat& a, const Quat& b, f32 eps = 1e-4f) {
    return nearEqF(a.x, b.x, eps) && nearEqF(a.y, b.y, eps) &&
           nearEqF(a.z, b.z, eps) && nearEqF(a.w, b.w, eps);
}

} // namespace

TEST(play_snapshot_restore_pose_exata) {
    Scene s;
    const Handle h = createTicFromPreset(s, PresetKind::RigidBody3D, nullptr, nullptr);
    Transform3D* tr = s.get(h)->getComponent<Transform3D>();
    BodyComp* b = s.get(h)->getComponent<BodyComp>();
    EXPECT(tr && b);

    // pose de editor arbitrária (não é a default do preset)
    const Quat rotEditor = Quat::fromEuler(0.3f, 1.1f, -0.7f);
    tr->pos = Vec3{2.0f, 3.5f, -4.0f};
    tr->rot = rotEditor;
    tr->scale = Vec3{1.5f, 2.0f, 0.5f};
    tr->updateWorld();

    // ENTRAR no Play: captura
    PlaySnapshot snap;
    playSnapshotCapture(s, snap);
    EXPECT(snap.captured);
    EXPECT(snap.transforms.size() == 1);
    EXPECT(snap.bodies.size() == 1);

    // simulação mexe em TUDO (pos/rot/scale, cache world sujo, corpo em movimento)
    tr->pos = Vec3{-9.0f, 0.1f, 7.0f};
    tr->rot = Quat::fromEuler(1.5f, 0.2f, 0.9f);
    tr->scale = Vec3{0.4f, 0.4f, 0.4f};
    tr->updateWorld();
    tr->worldDirty = true;   // a física marca dirty entre passos
    b->velocity = Vec3{5.0f, -2.0f, 1.0f};
    b->grounded = true;

    // SAIR do Play: restore — pose de editor EXATA de volta
    playSnapshotRestore(s, snap);
    EXPECT(vecNearF(tr->pos, Vec3{2.0f, 3.5f, -4.0f}));
    EXPECT(quatNear(tr->rot, rotEditor));
    EXPECT(vecNearF(tr->scale, Vec3{1.5f, 2.0f, 0.5f}));
    EXPECT(!tr->worldDirty);
    EXPECT(matNearF(tr->world, tr->computeMatrix()));   // cache coerente
    EXPECT(vecNearF(b->velocity, Vec3{0.0f, 0.0f, 0.0f}));
    EXPECT(!b->grounded);
}

TEST(play_fisica_cai_no_play_e_restore_repe) {
    Scene s;
    // chão estático (topo em y=0) + bola rígida a 3.0 — o cenário do C33
    const Handle ground = createTicFromPreset(s, PresetKind::StaticBody3D, nullptr, nullptr);
    const Handle ball = createTicFromPreset(s, PresetKind::RigidBody3D, nullptr, nullptr);
    Transform3D* tg = s.get(ground)->getComponent<Transform3D>();
    tg->pos = Vec3{0.0f, -0.5f, 0.0f};
    tg->updateWorld();
    Transform3D* tr = s.get(ball)->getComponent<Transform3D>();
    tr->pos = Vec3{0.0f, 3.0f, 0.0f};
    tr->updateWorld();
    BodyComp* bb = s.get(ball)->getComponent<BodyComp>();

    // fluxo real: capture no ENTRAR → física em Play → restore no SAIR
    PlaySnapshot snap;
    playSnapshotCapture(s, snap);
    EXPECT(snap.transforms.size() == 2);

    phys::PhysicsSystem sys;
    sys.enabled = true;   // modo Play (o main liga isto no botão Play)
    for (int i = 0; i < 120; ++i) {
        sys.tick(s, 1.0f / 60.0f);
    }
    EXPECT(tr->pos.y < 2.9f);          // a bola CAIU durante o Play
    EXPECT(bb->grounded);              // pousou no chão

    sys.enabled = false;               // modo editor: física para (gate)
    playSnapshotRestore(s, snap);
    EXPECT(vecNearF(tr->pos, Vec3{0.0f, 3.0f, 0.0f}));   // pose de editor exata
    EXPECT(vecNearF(bb->velocity, Vec3{0.0f, 0.0f, 0.0f}));
    EXPECT(!bb->grounded);
    // o chão estático também voltou (não mexeu, mas o restore é total)
    EXPECT(vecNearF(tg->pos, Vec3{0.0f, -0.5f, 0.0f}));
}

TEST(play_fisica_somente_durante_play) {
    Scene s;
    const Handle ball = createTicFromPreset(s, PresetKind::RigidBody3D, nullptr, nullptr);
    Transform3D* tr = s.get(ball)->getComponent<Transform3D>();
    tr->pos = Vec3{0.0f, 3.0f, 0.0f};
    tr->updateWorld();

    phys::PhysicsSystem sys;
    sys.enabled = false;   // modo editor — o gate do main (inalterado)
    sys.tick(s, 1.0f / 60.0f);
    sys.tick(s, 1.0f / 60.0f);
    EXPECT(nearEqF(tr->pos.y, 3.0f));   // sem Play, NADA se move

    sys.enabled = true;    // Play
    sys.tick(s, 1.0f / 60.0f);
    EXPECT(tr->pos.y < 3.0f);           // com Play, a gravidade age
}

TEST(play_tic_destruido_durante_o_play) {
    Scene s;
    const Handle a = createTicFromPreset(s, PresetKind::RigidBody3D, nullptr, nullptr);
    const Handle b = createTicFromPreset(s, PresetKind::StaticBody3D, nullptr, nullptr);
    Transform3D* ta = s.get(a)->getComponent<Transform3D>();
    ta->pos = Vec3{1.0f, 1.0f, 1.0f};
    ta->updateWorld();
    Transform3D* tb = s.get(b)->getComponent<Transform3D>();
    tb->pos = Vec3{2.0f, 2.0f, 2.0f};
    tb->updateWorld();

    PlaySnapshot snap;
    playSnapshotCapture(s, snap);
    EXPECT(snap.transforms.size() == 2);

    EXPECT(s.destroy(b));   // destruído durante o Play
    playSnapshotRestore(s, snap);   // NÃO pode crashar nem recriar
    // sobrevivente restaurado; o morto fica morto (handle obsoleto → nullptr)
    EXPECT(vecNearF(ta->pos, Vec3{1.0f, 1.0f, 1.0f}));
    EXPECT(s.get(b) == nullptr);
}

TEST(play_snapshot_vazio_e_sem_capture) {
    Scene s;
    PlaySnapshot snap;
    playSnapshotCapture(s, snap);   // cena vazia
    EXPECT(snap.captured);
    EXPECT(snap.transforms.empty());
    EXPECT(snap.bodies.empty());
    playSnapshotRestore(s, snap);   // no-op sem crash

    PlaySnapshot nunca;             // Play nunca abriu
    EXPECT(!nunca.captured);
    playSnapshotRestore(s, nunca);  // guard: no-op (não toca na cena)
}
