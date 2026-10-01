#pragma once
// assets/GltfAnim.h — animações/skins glTF → AnimationPlayer (0.8.1/0.8.2, F7).
//
// gltfAttachClips percorre as GltfAnimation do modelo e converte, para o
// TIC dado, os channels cujo NÓ ALVO é `rootNode` (default 0) em tracks do
// clip: translation → TicPos, rotation → TicRot (quat→EULER GRAUS, a
// convenção do player/Inspector — lossy em rotações compostas, documentado
// no RELATORIO-0.8.0 §5.2), scale → TicScale. Channels de OUTROS nós são
// SALTADOS (o player é por-TIC; joints pedem a 0.8.2 — skinning).
//
// 0.8.2: gltfAttachSkin cria o SkeletonComp (joints pais-primeiro + bind
// TRS + inverseBind) e gltfAttachClips ganha os tracks de JOINT (por nome
// de joint): canais cujo nó é joint da 1ª skin viram JointPos/JointRot/
// JointScale — o apply escreve o TRS LOCAL do joint e o render compõe.
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

// 0.8.2: cria o SkeletonComp do TIC com a 1ª skin do modelo (joints +
// inverseBind). Devolve o número de joints (0 se sem skin/já tem esqueleto)
u32 gltfAttachSkin(Scene& scene, Handle tic, const GltfModel& model,
                   u32 skinIdx = 0);

} // namespace vv
