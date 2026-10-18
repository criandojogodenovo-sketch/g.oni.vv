#include "core/ComponentRegistry.h"
#include "core/ComponentStore.h"
#include <cstring>

namespace vv {

u32 ComponentRegistry::add(const char* name, ComponentFactory factory) {
    ComponentTypeRecord rec;
    rec.id = static_cast<u32>(types_.size());
    rec.name = name;
    rec.factory = factory;
    types_.push_back(rec);
    return rec.id;
}

i32 ComponentRegistry::find(const char* name) const {
    if (!name) {
        return -1;
    }
    for (size_t i = 0; i < types_.size(); ++i) {
        if (std::strcmp(types_[i].name, name) == 0) {
            return static_cast<i32>(i);
        }
    }
    return -1;
}

const ComponentTypeRecord* ComponentRegistry::at(u32 id) const {
    return id < types_.size() ? &types_[id] : nullptr;
}

bool ComponentRegistry::create(u32 id, ComponentStore& store, Handle owner) const {
    const ComponentTypeRecord* rec = at(id);
    if (!rec || !rec->factory) {
        return false;
    }
    return rec->factory(store, owner);
}

bool ComponentRegistry::create(const char* name, ComponentStore& store, Handle owner) const {
    const i32 id = find(name);
    return id >= 0 && create(static_cast<u32>(id), store, owner);
}

} // namespace vv
