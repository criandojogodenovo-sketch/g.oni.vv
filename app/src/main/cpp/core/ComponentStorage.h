#pragma once
// core/ComponentStorage.h — storage SoA por tipo de componente (F3).
//
// Dois arrays paralelos: os dados do componente e o Handle do TIC dono
// (mesmo padrão do Transform3DStorage da spec: vector<T> + vector<Handle>).
// Remove é swap-remove (O(1), ordem não preservada — F3 não depende de ordem).
//
// attach()/detach() são chamados AQUI (add/remove) — o storage conhece o Tic*
// dono (passado pelo ComponentStore, que conhece a Scene). Componente solto
// (owner nullptr) nunca entra no storage: add exige Tic* não-nulo.
#include <type_traits>
#include <vector>
#include "core/Component.h"
#include "core/Handle.h"

namespace vv {

struct Tic;

template <typename C>
class ComponentStorage {
public:
    static_assert(std::is_base_of<Component, C>::value,
                  "componente tem de derivar de vv::Component");

    // Adiciona e anexa. Tic* vem de Scene::get(handle) — nullptr recusado.
    // Devolve o componente anexado; nullptr se o dono já tem este tipo.
    // attachNow=false adia o attach (add-with-init anexa depois de copiar dados).
    C* add(Handle h, Tic* owner, bool attachNow = true) {
        if (indexOf(h) >= 0) {
            return nullptr;   // um componente por tipo por TIC (F3)
        }
        items.push_back(C{});
        owners.push_back(h);
        C* c = &items.back();
        c->owner = owner;
        if (attachNow) {
            c->attach();
        }
        return c;
    }

    C* find(Handle h) {
        const i32 i = indexOf(h);
        return i >= 0 ? &items[static_cast<size_t>(i)] : nullptr;
    }
    const C* find(Handle h) const {
        const i32 i = indexOf(h);
        return i >= 0 ? &items[static_cast<size_t>(i)] : nullptr;
    }

    // Detacha e remove (swap-remove). false se o dono não tem este tipo.
    bool remove(Handle h) {
        const i32 i = indexOf(h);
        if (i < 0) {
            return false;
        }
        const size_t idx = static_cast<size_t>(i);
        items[idx].detach();
        items[idx].owner = nullptr;
        const size_t last = items.size() - 1;
        if (idx != last) {
            items[idx] = std::move(items[last]);
            owners[idx] = owners[last];
        }
        items.pop_back();
        owners.pop_back();
        return true;
    }

    i32 indexOf(Handle h) const {
        if (!h.valid()) {
            return -1;
        }
        for (size_t i = 0; i < owners.size(); ++i) {
            if (owners[i] == h) {
                return static_cast<i32>(i);
            }
        }
        return -1;
    }

    u32          size() const { return static_cast<u32>(items.size()); }
    C&           at(u32 i)       { return items[i]; }
    const C&     at(u32 i) const { return items[i]; }
    Handle       owner(u32 i) const { return owners[i]; }

    std::vector<C>      items;    // dados (SoA: paralelo a owners)
    std::vector<Handle> owners;   // donos (mesmo índice)
};

} // namespace vv
