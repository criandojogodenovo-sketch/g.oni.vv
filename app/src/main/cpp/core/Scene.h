#pragma once
// core/Scene.h — storage dos TICs.
// Estratégia F1: vector de slots + free-list LIFO de índices.
// destroy() avança a generation do slot e devolve o índice à free-list:
// o slot pode ser reaproveitado, mas handles antigos continuam inválidos
// (bump de generation = handles nunca reutilizados silenciosamente).
#include <vector>
#include <string>
#include "Handle.h"
#include "Tic.h"

namespace vv {

class Scene {
public:
    Handle create(std::string name, i32 parent = -1);
    bool   destroy(Handle h);   // false se handle obsoleto/inválido
    bool   alive(Handle h) const;
    Tic*   get(Handle h);       // nullptr se handle obsoleto/inválido
    const Tic* get(Handle h) const;
    Handle find(const std::string& name) const;   // primeiro TIC ativo com o nome
    u32    count() const { return active_; }
    u32    capacity() const { return static_cast<u32>(slots_.size()); }

private:
    std::vector<Tic> slots_;
    std::vector<u32> freeList_;   // índices livres (LIFO)
    u32 active_ = 0;
};

} // namespace vv
