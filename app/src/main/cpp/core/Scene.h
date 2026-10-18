#pragma once
// core/Scene.h — storage dos TICs + bolsa de componentes (F3).
//
// Estratégia F1: slots + free-list LIFO de índices.
// destroy() avança a generation do slot e devolve o índice à free-list:
// o slot pode ser reaproveitado, mas handles antigos continuam inválidos
// (bump de generation = handles nunca reutilizados silenciosamente).
//
// F3: slots_ é std::deque (não vector) — create/destroy NUNCA movem elementos,
// logo Tic* (back-pointer scene→tic, Component::owner) fica válido enquanto o
// TIC viver. Um vector realocaria no push_back e deixaria todos os owners
// pendentes (apanhado pelo teste tic_api_add_get_remove_componente).
//
// F3: a Scene é dona do ComponentStore (um storage SoA por tipo). create()
// liga o back-pointer do Tic; destroy() desanexa e remove TODOS os
// componentes do TIC antes de libertar o slot (detach vê o Tic vivo).
#include <deque>
#include <vector>
#include "ComponentStore.h"
#include "Handle.h"
#include "Tic.h"

namespace vv {

class Scene {
public:
    Scene() : components_(this) {}

    Handle create(std::string name, i32 parent = -1);
    bool   destroy(Handle h);   // false se handle obsoleto/inválido
    bool   alive(Handle h) const;
    Tic*   get(Handle h);       // nullptr se handle obsoleto/inválido
    const Tic* get(Handle h) const;
    Handle find(const std::string& name) const;   // primeiro TIC ativo com o nome
    u32    count() const { return active_; }
    u32    capacity() const { return static_cast<u32>(slots_.size()); }

    // F3: destrói todos os TICs ativos (usado pelo load do serializer).
    void clear();

    // F3: visita os TICs ativos (serializer/Hierarchy; ordem = índice do slot).
    template <typename F>
    void forEachActive(F&& fn) {
        for (Tic& t : slots_) {
            if (t.active) {
                fn(t);
            }
        }
    }
    template <typename F>
    void forEachActive(F&& fn) const {
        for (const Tic& t : slots_) {
            if (t.active) {
                fn(t);
            }
        }
    }

    // Bolsa de componentes desta cena (Tic::addComponent<T> passa por aqui).
    ComponentStore&       components()       { return components_; }
    const ComponentStore& components() const { return components_; }

private:
    std::deque<Tic> slots_;   // endereços estáveis (ver nota F3 no topo)
    std::vector<u32> freeList_;   // índices livres (LIFO)
    u32 active_ = 0;

    ComponentStore components_;   // F3: SoA por tipo de componente
};

} // namespace vv
