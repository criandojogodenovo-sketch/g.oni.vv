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
class Tic;
class ParticleEmitter;
class AmbientEnvironment;

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
    WorldEnvironment = 6,  // 0.12-Y (3.Y-C): só Transform+Environment — o
                           // céu/névoa/HDRI da cena (o 1.º ATIVO manda;
                           // SEM mesh, SEM física)
    ParticleEmitter  = 7,  // 0.12-Y-F1: só Transform+ParticleEmitter — o
                           // emissor de partículas GPU (a física vive NO
                           // VERTEX SHADER; SEM mesh, SEM física de corpos)
    Wind             = 8,  // 0.12-Y-F2: só Transform+WindTIC — o vento
                           // GLOBAL da cena (o 1.º ATIVO manda; SEM mesh,
                           // SEM física, SEM partículas)
    Rain             = 9,  // 0.12-Y-F2: Transform+ParticleEmitter+
                           // AmbientEnvironment (weatherType 1, 0.5) — o
                           // clima CONFIGURA o emissor do MESMO TIC
    Snow             = 10, // 0.12-Y-F2: idem (weatherType 2, 0.5)
    Storm            = 11, // 0.12-Y-F2: idem (weatherType 3, 0.7) — a
                           // trovoada (o flash no céu via uThunderFlash)
    WeatherController = 12, // 0.12-W-1: Transform+AmbientEnvironment SÓ — o
                           // CONTROLADOR do clima do dono (o emissor é UM
                           // TIC À PARTE chamado targetEmitter — o fluxo dos
                           // DOIS TICs; SEM PE no próprio TIC)
    Count           = 13,
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

// ---- 0.12-W-1: O ALVO DO CLIMA (a FONTE ÚNICA da resolução) ----------------
// Resolve o ParticleEmitter que o AmbientEnvironment controla:
//   1.º o PE do MESMO TIC (o contrato da Y-F2 — os presets Rain/Snow/Storm
//       da casa continuam byte a byte);
//   2.º sem PE no TIC → o TIC ATIVO com o NOME ae->targetEmitter que tenha
//       um ParticleEmitter (o fluxo W-1: "RainEmitter" + "WeatherController"
//       em DOIS TICs separados).
// nullptr quando NENHUM dos dois existe.
ParticleEmitter* findWeatherTarget(Scene& scene, Tic& aeTic,
                                   const AmbientEnvironment& ae);

// APLICA o preset ao alvo resolvido (a criação do WeatherController e os
// edits do Inspector chamam AQUI — nunca por frame). Sem alvo: o LOG WARN
// EDGE-TRIGGERED 1× (ae->targetWarned; o pin ambient_env_logs_warning
// aferre o elog::lineCount) e devolve false — NADA acontece.
bool applyWeatherToTarget(Scene& scene, Tic& aeTic, AmbientEnvironment& ae);

} // namespace vv
