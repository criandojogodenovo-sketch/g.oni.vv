#pragma once
// core/Component.h — base de todos os componentes (F3).
//
// Semântica Unreal/Godot: o TIC (ator) é DONO dos componentes; um componente
// existe anexado a um TIC. O ciclo de vida é:
//   add   → owner aponta para o Tic → attach()
//   remove→ detach() → owner = nullptr → storage devolve o slot
//
// Os componentes vivem por VALOR em storage SoA (ComponentStorage<T>), um
// storage por tipo — nunca como objetos polimórficos soltos. A base existe
// para dar owner + hooks uniformes (F4: BodyComp sobrescreve attach/detach).
//
// NOTA GL-free: nenhum include de Android/GL aqui — tudo isto roda nos testes
// do CI Linux.
#include "core/Handle.h"

namespace vv {

struct Tic;

class Component {
public:
    Tic* owner = nullptr;   // TIC dono; nullptr = componente solto/detachado

    virtual ~Component() = default;
    virtual void attach() {}   // chamado logo após o owner ser atribuído
    virtual void detach() {}   // chamado antes de o componente sair do storage
};

} // namespace vv
