#include "core/Presets.h"
#include "components/InputMap.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/Scene.h"
#include <cstdio>

namespace vv {

const char* presetName(PresetKind kind) {
    switch (kind) {
        case PresetKind::PlayerBody3D:    return "PlayerBody3D";
        case PresetKind::CharacterBody3D: return "CharacterBody3D";
        case PresetKind::StaticBody3D:    return "StaticBody3D";
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

    Transform3D* tr = tic->addComponent<Transform3D>();
    if (tr) {
        tr->pos = Vec3{0.0f, 0.5f, 0.0f};   // assente no grid (igual ao F2)
        tr->updateWorld();
    }

    MeshRenderer* mr = tic->addComponent<MeshRenderer>();
    if (mr) {
        mr->mesh = mesh;
        mr->material = material;
    }

    if (kind == PresetKind::PlayerBody3D) {
        tic->addComponent<InputMap>();   // vazio na F3 — bindings são F4
    }
    return h;
}

} // namespace vv
