#include "Scene.h"

namespace vv {

Handle Scene::create(std::string name, i32 parent) {
    u32 idx;
    if (!freeList_.empty()) {
        idx = freeList_.back();          // reusa slot — generation já está bumped
        freeList_.pop_back();
    } else {
        idx = static_cast<u32>(slots_.size());
        slots_.emplace_back();
        slots_[idx].handle.generation = 1u;   // primeira generation do slot (0 = nulo)
    }
    Tic& t = slots_[idx];
    t.handle.index = idx;
    t.name = std::move(name);
    t.active = true;
    t.parent = parent;
    t.scene = this;                       // F3: liga o dono dos storages
    ++active_;
    return t.handle;
}

bool Scene::destroy(Handle h) {
    if (!alive(h)) {
        return false;   // handle obsoleto, nulo ou já destruído — no-op seguro
    }
    Tic& t = slots_[h.index];
    components_.removeAll(h);   // F3: detach + remove antes de libertar o slot
    t.active = false;
    t.name.clear();
    t.name.shrink_to_fit();
    t.parent = -1;
    t.scene = nullptr;          // F3: Tic solto não aceita composição
    t.handle.generation += 1u;  // bump: handles antigos deste slot ficam obsoletos
    freeList_.push_back(h.index);
    --active_;
    return true;
}

bool Scene::alive(Handle h) const {
    if (!h.valid()) {
        return false;
    }
    if (h.index >= slots_.size()) {
        return false;
    }
    const Tic& t = slots_[h.index];
    return t.active && t.handle.generation == h.generation;
}

Tic* Scene::get(Handle h) {
    return alive(h) ? &slots_[h.index] : nullptr;
}

const Tic* Scene::get(Handle h) const {
    return alive(h) ? &slots_[h.index] : nullptr;
}

Handle Scene::find(const std::string& name) const {
    for (u32 i = 0; i < slots_.size(); ++i) {
        const Tic& t = slots_[i];
        if (t.active && t.name == name) {
            return t.handle;
        }
    }
    return Handle::invalid();
}

void Scene::clear() {
    // Destrói de cima para baixo: free-list termina em ordem ascendente,
    // logo o próximo create() reutiliza os índices baixos primeiro (0,1,2…)
    // — recarregar uma cena recria os slots na ordem esperada.
    for (u32 i = static_cast<u32>(slots_.size()); i > 0; --i) {
        const u32 idx = i - 1;
        if (slots_[idx].active) {
            destroy(slots_[idx].handle);
        }
    }
}

} // namespace vv
