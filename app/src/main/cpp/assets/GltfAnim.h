#pragma once
// assets/GltfAnim.h — animações glTF → CLIPS do AnimationPlayer (0.8.1, F7).
//
// gltfAttachClips percorre as GltfAnimation do modelo e converte, para o
// TIC dado, os channels cujo NÓ ALVO é `rootNode` (default 0) em tracks do
// clip: translation → TicPos, rotation → TicRot (quat→EULER GRAUS, a
// convenção do player/Inspector — lossy em rotações compostas, documentado
// no RELATORIO-0.8.0 §5.2), scale → TicScale. Channels de OUTROS nós são
// SALTADOS (o player é por-TIC; joints pedem a 0.8.2 — skinning).
//
// Cada animação vira um CLIP nomeado no AnimationPlayer do TIC (cria-o se
// não existe); o clip "edit" da timeline NÃO é tocado (os importados
// ACRESCENTAM — reimportar duplica; o dono apaga na mão se quiser).
// Animações sem channel útil para o nó ficam DE FORA (clip vazio não entra).
//
// GL-free / host-testável (o CI constrói fixtures .glb em código).
#include "assets/GltfImporter.h"
#include "core/Handle.h"

namespace vv {

class Scene;

// devolve o NÚMERO de clips acrescentados ao player do TIC
u32 gltfAttachClips(Scene& scene, Handle tic, const GltfModel& model,
                    i32 rootNode = 0);

} // namespace vv
