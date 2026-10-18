#pragma once
// core/ComponentRegistry.h — registo de tipos de componentes + factory (F3).
//
// Cada tipo conhecido pelo engine fica registado com um nome (usado pelo
// serializer .goni) e uma factory (cria o componente default num ComponentStore
// para um dado Handle). Tipos desconhecidos num ficheiro .goni são ignorados
// no load — política forward-compat das migrações.
//
// O id é o índice de registo (estável durante a execução; a ordem de registo
// é fixa no ctor do ComponentStore: 0=Transform3D, 1=MeshRenderer, 2=InputMap).
#include <vector>
#include "core/Handle.h"
#include "core/Types.h"

namespace vv {

class ComponentStore;

using ComponentFactory = bool (*)(ComponentStore& store, Handle owner);

struct ComponentTypeRecord {
    u32              id = 0;
    const char*      name = nullptr;   // literal estático (dono do registry não copia)
    ComponentFactory factory = nullptr;
};

class ComponentRegistry {
public:
    // Registra um tipo; devolve o id (índice). name tem de viver para sempre.
    u32  add(const char* name, ComponentFactory factory);

    // -1 se o nome não estiver registado.
    i32  find(const char* name) const;

    const ComponentTypeRecord* at(u32 id) const;   // nullptr se fora do intervalo
    u32  count() const { return static_cast<u32>(types_.size()); }

    // Factory do tipo → store.add<T>(owner). false se tipo/factory inválidos
    // ou o dono já tem o componente.
    bool create(u32 id, ComponentStore& store, Handle owner) const;
    bool create(const char* name, ComponentStore& store, Handle owner) const;

private:
    std::vector<ComponentTypeRecord> types_;
};

} // namespace vv
