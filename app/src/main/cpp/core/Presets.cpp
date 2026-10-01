#include "core/Presets.h"
#include "components/BodyComp.h"
#include "components/InputMap.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/Scene.h"
#include "render/Primitives.h"
#include <cstdio>

namespace vv {

const char* presetName(PresetKind kind) {
    switch (kind) {
        case PresetKind::PlayerBody3D:    return "PlayerBody3D";
        case PresetKind::CharacterBody3D: return "CharacterBody3D";
        case PresetKind::StaticBody3D:    return "StaticBody3D";
        case PresetKind::RigidBody3D:     return "RigidBody3D";
        case PresetKind::Mesh:            return "Mesh";   // 0.8.0 (F7)
        default:                          return "Tic";
    }
}

namespace {

// Primeiro nome livre: base, base.001, base.002… (limite prático: 999).
std::string uniqueTicName(Scene& scene, const char* base) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s", base);
    if (!scene.find(buf).valid()) {
        return std::string(buf);
    }
    for (u32 i = 1; i < 1000u; ++i) {
        std::snprintf(buf, sizeof(buf), "%s.%03u", base, i);
        if (!scene.find(buf).valid()) {
            return std::string(buf);
        }
    }
    std::snprintf(buf, sizeof(buf), "%s.999", base);
    return std::string(buf);
}

} // namespace

Handle createTicFromPreset(Scene& scene, PresetKind kind,
                           Mesh* mesh, Material* material) {
    const Handle h = scene.create(uniqueTicName(scene, presetName(kind)));
    Tic* tic = scene.get(h);
    if (!tic) {
        return Handle::invalid();
    }

    // corpos com cápsula assentam a base no y=0 (raio 0.3 + meia-altura 0.25);
    // 0.8.0: o TIC "Mesh" (esfera default r=0.5) assenta igual — r 0.5
    const f32 baseY = (kind == PresetKind::PlayerBody3D ||
                       kind == PresetKind::CharacterBody3D ||
                       kind == PresetKind::Mesh) ? 0.55f : 0.5f;
    Transform3D* tr = tic->addComponent<Transform3D>();
    if (tr) {
        tr->pos = Vec3{0.0f, baseY, 0.0f};
        tr->updateWorld();
    }

    MeshRenderer* mr = tic->addComponent<MeshRenderer>();
    if (mr) {
        mr->mesh = mesh;
        mr->material = material;
        // 0.8.0 (F7): o preset Mesh NASCE com a PRIMITIVA esfera default
        // (a assinatura serializa; o main pode rebindar outro tipo depois)
        if (kind == PresetKind::Mesh) {
            mr->primOn = true;
            mr->prim = primDefaults(PrimKind::Sphere);
        }
    }

    if (kind == PresetKind::PlayerBody3D) {
        tic->addComponent<InputMap>();   // SEM TouchControls — via Inspector (F4-D)
    }

    // F4: corpo de física do preset (0.8.0: o preset Mesh NÃO leva física —
    // prototipagem pura de cena/animação)
    if (kind == PresetKind::Mesh) {
        return h;
    }
    if (BodyComp* b = tic->addComponent<BodyComp>()) {
        switch (kind) {
            case PresetKind::PlayerBody3D:
            case PresetKind::CharacterBody3D:
                b->type = BodyType::Character;
                b->shape = phys::Capsule{Vec3{0.0f, 0.0f, 0.0f}, 0.3f, 0.25f};
                break;
            case PresetKind::StaticBody3D:
                b->type = BodyType::Static;
                b->shape = phys::OBB{Vec3{0.0f, 0.0f, 0.0f},
                                     Vec3{0.5f, 0.5f, 0.5f}, Quat::identity()};
                break;
            case PresetKind::RigidBody3D:
                b->type = BodyType::Rigid;
                b->shape = phys::Sphere{Vec3{0.0f, 0.0f, 0.0f}, 0.5f};
                break;
            default:
                break;
        }
    }
    return h;
}

} // namespace vv
