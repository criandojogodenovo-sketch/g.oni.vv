#pragma once
// core/Presets.h — presets de TIC do diálogo "+" (F3).
//
// Receita da spec (F4):
//   PlayerBody3D    = T + Mesh(cubo) + InputMap(vazio) + Body{Character, capsule}
//   CharacterBody3D = T + Mesh(cubo) + Body{Character, capsule}
//   StaticBody3D    = T + Mesh(cubo) + Body{Static, OBB 0.5³}
//   RigidBody3D     = T + Mesh(cubo) + Body{Rigid, sphere r 0.5}
// TouchControls NÃO vem nos presets — adicionável via Inspector a qualquer
// TIC com InputMap (F4-D).
//
// Sem assets: o mesh dos presets é o cubo procedural da F2 (ponteiro não-dono
// partilhado); o material é o lit do Renderer. Transform default: pos
// (0, 0.5, 0) — cubo de 1 unidade assente no grid, igual ao F2 hardcoded.
#include "core/Handle.h"
#include "core/Types.h"
#include "render/Material.h"   // Material = alias de LitMaterial — alias não admite fwd-decl

namespace vv {

class Scene;
class Mesh;

enum class PresetKind : u32 {
    PlayerBody3D    = 0,
    CharacterBody3D = 1,
    StaticBody3D    = 2,
    RigidBody3D     = 3,
    Mesh            = 4,   // 0.8.0 (F7): só Transform+MeshRenderer (a
                           // primitiva vem pelo `mesh` — o main passa a
                           // esfera default do cache; SEM física)
    Audio           = 5,   // 0.8.11: só Transform+AudioPlayer (o clip vem
                           // pelo Inspector; SEM mesh, SEM física)
    Count           = 6,
};

// Nome canônico do preset (usado no menu "+" e como nome base do TIC).
const char* presetName(PresetKind kind);

// Cria um TIC com os componentes do preset. Nome único garantido com sufixo
// Godot-style: "PlayerBody3D", "PlayerBody3D.001", "PlayerBody3D.002"…
// mesh/material podem ser nullptr (o TIC nasce; o draw só acontece quando
// o MeshRenderer aponta para recursos válidos — fluxo do loader).
// Devolve Handle::invalid() se a Scene recusar o create.
Handle createTicFromPreset(Scene& scene, PresetKind kind,
                           Mesh* mesh, Material* material);

} // namespace vv
