// tests/test_presets.cpp — F3: presets do diálogo "+".
// Verifica receita de componentes, nomes únicos e transform default.
// (A parte visual — criar via "+", ver o cubo mover — é teste no device,
// não no CI, conforme a spec.)
#include "TestFramework.h"
#include <cstring>
#include "components/BodyComp.h"
#include "components/InputMap.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/Presets.h"
#include "core/Scene.h"

using namespace vv;
using ::test::nearEqF;

TEST(preset_nomes_canonicos) {
    EXPECT(std::strcmp(presetName(PresetKind::PlayerBody3D), "PlayerBody3D") == 0);
    EXPECT(std::strcmp(presetName(PresetKind::CharacterBody3D), "CharacterBody3D") == 0);
    EXPECT(std::strcmp(presetName(PresetKind::StaticBody3D), "StaticBody3D") == 0);
    EXPECT(std::strcmp(presetName(PresetKind::RigidBody3D), "RigidBody3D") == 0);
}

TEST(preset_player_tem_inputmap_outros_nao) {
    Scene s;
    const Handle p = createTicFromPreset(s, PresetKind::PlayerBody3D, nullptr, nullptr);
    const Handle c = createTicFromPreset(s, PresetKind::CharacterBody3D, nullptr, nullptr);
    const Handle st = createTicFromPreset(s, PresetKind::StaticBody3D, nullptr, nullptr);

    EXPECT(s.alive(p) && s.alive(c) && s.alive(st));

    Tic* tp = s.get(p);
    EXPECT(tp->getComponent<Transform3D>() != nullptr);
    EXPECT(tp->getComponent<MeshRenderer>() != nullptr);
    EXPECT(tp->getComponent<InputMap>() != nullptr);       // exclusivo do Player

    Tic* tc = s.get(c);
    EXPECT(tc->getComponent<Transform3D>() != nullptr);
    EXPECT(tc->getComponent<MeshRenderer>() != nullptr);
    EXPECT(tc->getComponent<InputMap>() == nullptr);

    Tic* ts = s.get(st);
    EXPECT(ts->getComponent<Transform3D>() != nullptr);
    EXPECT(ts->getComponent<MeshRenderer>() != nullptr);
    EXPECT(ts->getComponent<InputMap>() == nullptr);

    EXPECT(s.count() == 3u);
}

TEST(preset_materiais_e_mesh_ligados) {
    Scene s;
    MeshRenderer captura{};   // ponteiros não-donos de teste
    static Mesh* fakeMesh = reinterpret_cast<Mesh*>(0x1);      // sentinelas
    static Material* fakeMat = reinterpret_cast<Material*>(0x2);

    const Handle h = createTicFromPreset(s, PresetKind::CharacterBody3D, fakeMesh, fakeMat);
    MeshRenderer* mr = s.get(h)->getComponent<MeshRenderer>();
    EXPECT(mr != nullptr);
    EXPECT(mr->mesh == fakeMesh);
    EXPECT(mr->material == fakeMat);
    (void)captura;
}

TEST(preset_nomes_unicos_com_sufixo) {
    Scene s;
    const Handle a = createTicFromPreset(s, PresetKind::StaticBody3D, nullptr, nullptr);
    const Handle b = createTicFromPreset(s, PresetKind::StaticBody3D, nullptr, nullptr);
    const Handle c = createTicFromPreset(s, PresetKind::StaticBody3D, nullptr, nullptr);
    EXPECT(s.get(a)->name == "StaticBody3D");
    EXPECT(s.get(b)->name == "StaticBody3D.001");
    EXPECT(s.get(c)->name == "StaticBody3D.002");

    // destruir o do meio → o próximo reusa o sufixo liberado (find não o vê)
    EXPECT(s.destroy(b));
    const Handle d = createTicFromPreset(s, PresetKind::StaticBody3D, nullptr, nullptr);
    EXPECT(s.get(d)->name == "StaticBody3D.001");
}

TEST(preset_transform_default_assente_no_grid) {
    Scene s;
    const Handle h = createTicFromPreset(s, PresetKind::PlayerBody3D, nullptr, nullptr);
    Transform3D* tr = s.get(h)->getComponent<Transform3D>();
    EXPECT(tr != nullptr);
    EXPECT(nearEqF(tr->pos.x, 0.0f));
    EXPECT(nearEqF(tr->pos.y, 0.55f));   // cápsula (0.3+0.25) assente no chão
    EXPECT(nearEqF(tr->pos.z, 0.0f));
    EXPECT(tr->rot.x == 0.0f && tr->rot.y == 0.0f && tr->rot.z == 0.0f && tr->rot.w == 1.0f);
    EXPECT(nearEqF(tr->scale.x, 1.0f) && nearEqF(tr->scale.y, 1.0f) && nearEqF(tr->scale.z, 1.0f));

    // world cache já atualizada → o primeiro draw já nasce posicionado
    f32 out[4];
    Mat4::transformPoint4(tr->world, Vec3{0.0f, -0.5f, 0.0f}, out);
    EXPECT(nearEqF(out[1], 0.05f, 1e-5f));   // base do cubo a 5 cm do chão
}

// ---- F4: corpos dos presets --------------------------------------------------

TEST(preset_corpos_f4_por_receita) {
    Scene s;
    const Handle p = createTicFromPreset(s, PresetKind::PlayerBody3D, nullptr, nullptr);
    const Handle c = createTicFromPreset(s, PresetKind::CharacterBody3D, nullptr, nullptr);
    const Handle st = createTicFromPreset(s, PresetKind::StaticBody3D, nullptr, nullptr);
    const Handle rb = createTicFromPreset(s, PresetKind::RigidBody3D, nullptr, nullptr);

    // Player: Character + cápsula 0.3/0.25
    const BodyComp* bp = s.get(p)->getComponent<BodyComp>();
    EXPECT(bp != nullptr && bp->type == BodyType::Character);
    EXPECT(bp->shape.index() == 3u);
    EXPECT(nearEqF(std::get<phys::Capsule>(bp->shape).radius, 0.3f));
    EXPECT(nearEqF(std::get<phys::Capsule>(bp->shape).halfHeight, 0.25f));

    // CharacterBody: igual sem InputMap
    const BodyComp* bc = s.get(c)->getComponent<BodyComp>();
    EXPECT(bc != nullptr && bc->type == BodyType::Character);
    EXPECT(s.get(c)->getComponent<InputMap>() == nullptr);

    // Static: OBB 0.5³
    const BodyComp* bs = s.get(st)->getComponent<BodyComp>();
    EXPECT(bs != nullptr && bs->type == BodyType::Static);
    EXPECT(bs->shape.index() == 2u);
    EXPECT(nearEqF(std::get<phys::OBB>(bs->shape).halfExtents.x, 0.5f));

    // Rigid: esfera r 0.5
    const BodyComp* br = s.get(rb)->getComponent<BodyComp>();
    EXPECT(br != nullptr && br->type == BodyType::Rigid);
    EXPECT(br->shape.index() == 0u);
    EXPECT(nearEqF(std::get<phys::Sphere>(br->shape).r, 0.5f));
    EXPECT(s.get(rb)->getComponent<InputMap>() == nullptr);

    // Nenhum preset nasce com TouchControls (adicionável via Inspector)
    EXPECT(s.get(p)->getComponent<TouchControls>() == nullptr);
    EXPECT(s.get(c)->getComponent<TouchControls>() == nullptr);
}
