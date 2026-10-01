#pragma once
// core/CameraUtil.h — invariante "UMA câmara ativa por cena" (0.7.7) +
// procura da câmara de jogo.
//
// Os .goni podem chegar com várias câmaras marcadas `active` (edição à mão,
// merges): o LOADER chama enforceSingleActiveCamera — a PRIMEIRA (ordem do
// manifesto) fica ativa, as restantes saem. O toggle do Inspector usa
// setOnlyActiveCamera (ativar uma desativa as outras; desativar a ativa
// deixa a cena SEM câmara ativa — o Play cai no fallback da orbit, nunca
// um estado ambíguo).
#include "core/Handle.h"
#include "core/Types.h"

namespace vv {

class Scene;
class Tic;
class CameraComp;
class Transform3D;

// o TIC da câmara ATIVA com Transform3D (null se não houver). Uma só.
Tic* findActiveCameraTic(Scene& scene);
CameraComp* findActiveCamera(Scene& scene);

// marca ESTA como a única ativa (as outras saem). Devolve false se o TIC
// não existir/não tiver CameraComp.
bool setOnlyActiveCamera(Scene& scene, Handle h);

// desativa a câmara de um TIC (se era a ativa, a cena fica sem ativa —
// fallback honesto ao invés de ambiguidade)
bool clearActiveCamera(Scene& scene, Handle h);

// pós-LOAD: a primeira ativa (ordem do manifesto) fica; as restantes saem.
// Sem nenhuma → fica sem (fallback da orbit no Play).
void enforceSingleActiveCamera(Scene& scene);

} // namespace vv
