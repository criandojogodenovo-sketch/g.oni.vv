#pragma once
// core/Presets.h — presets de TIC do diálogo "+" (F3).
//
// Receita da spec:
//   PlayerBody3D    = TIC + Transform3D + MeshRenderer(cubo) + InputMap(vazio)
//   CharacterBody3D = TIC + Transform3D + MeshRenderer(cubo)
//   StaticBody3D    = TIC + Transform3D + MeshRenderer(cubo)
//
// PLACEHOLDER DELIBERADO: os três presets são IGUAIS fora do InputMap — os
// corpos (física/cinemática) chegam na F4 e é lá que passam a diferir. O
// nome do preset é o contrato; os componentes hoje são os da F3.
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
    Count           = 3,
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
