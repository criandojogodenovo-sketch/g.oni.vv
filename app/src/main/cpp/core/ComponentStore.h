#pragma once
// core/ComponentStore.h — bolsa de componentes de uma Scene (F3).
//
// Um storage SoA por tipo conhecido + o ComponentRegistry. É a ponte entre
// TIC (dono) e componentes:
//   store.add<Transform3D>(h)     → cria, atribui owner, chama attach()
//   store.get<Transform3D>(h)     → nullptr se o TIC não tem o componente
//   store.remove<Transform3D>(h)  → detach() + swap-remove
//   store.removeAll(h)            → usado por Scene::destroy
//   registry().create("X", ...)   → caminho da factory (serializer)
//
// A store não é dona da Scene — guarda Scene* para resolver Handle→Tic*
// (o campo Component::owner é Tic*, não Handle, conforme a spec).
//
// ⚠ Inclua core/Scene.h ANTES de instanciar add/get/remove<T>: os templates
// resolvem Scene::get(handle) e precisam do tipo completo.
#include "core/Component.h"
#include "core/ComponentRegistry.h"
#include "core/ComponentStorage.h"
#include "components/InputMap.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"

namespace vv {

class Scene;
struct Tic;

// Resolve Handle→Tic* — definido no .cpp (onde Scene é completa). Os templates
// abaixo chamam isto em vez de scene_->get() direto: o header fica compilável
// com Scene apenas declarada (sem warnings de tipo incompleto).
Tic* resolveTic(Scene* scene, Handle h);

class ComponentStore {
public:
    explicit ComponentStore(Scene* owner);

    // ---- API template (um T = um tipo registrado) --------------------------
    // add-with-init: cria default, copia os dados, atribui owner e SÓ DEPOIS
    // chama attach() (attach nunca vê dados incompletos).
    template <typename T>
    T* add(Handle h, const T& init) {
        if (!scene_) {
            return nullptr;
        }
        T* c = storageOf<T>().add(h, resolveTic(scene_, h), false);
        if (!c) {
            return nullptr;
        }
        Tic* savedOwner = c->owner;
        *c = init;
        c->owner = savedOwner;   // owner vem do storage, não do init
        c->attach();
        return c;
    }

    template <typename T>
    T* add(Handle h) {
        if (!scene_) {
            return nullptr;
        }
        return storageOf<T>().add(h, resolveTic(scene_, h));
    }

    template <typename T>
    T* get(Handle h) {
        return storageOf<T>().find(h);
    }
    template <typename T>
    const T* get(Handle h) const {
        return storageOf<T>().find(h);
    }

    template <typename T>
    bool remove(Handle h) {
        return storageOf<T>().remove(h);
    }

    // Detacha/remove TODOS os componentes de um TIC (Scene::destroy).
    void removeAll(Handle h);
    bool hasAny(Handle h) const;

    // ---- acesso direto aos storages (serializer, pass 3D, systems) ---------
    ComponentStorage<Transform3D>&  transforms()      { return transforms_; }
    ComponentStorage<MeshRenderer>& meshRenderers()   { return meshRenderers_; }
    ComponentStorage<InputMap>&     inputMaps()       { return inputMaps_; }
    const ComponentStorage<Transform3D>&  transforms() const      { return transforms_; }
    const ComponentStorage<MeshRenderer>& meshRenderers() const   { return meshRenderers_; }
    const ComponentStorage<InputMap>&     inputMaps() const       { return inputMaps_; }

    const ComponentRegistry& registry() const { return registry_; }

private:
    template <typename T>
    ComponentStorage<T>& storageOf();
    template <typename T>
    const ComponentStorage<T>& storageOf() const;

    Scene*                          scene_ = nullptr;
    ComponentStorage<Transform3D>   transforms_;
    ComponentStorage<MeshRenderer>  meshRenderers_;
    ComponentStorage<InputMap>      inputMaps_;
    ComponentRegistry               registry_;
};

// ---- especializações do dispatch por tipo (sem RTTI, sem typeid) ----------
template <>
inline ComponentStorage<Transform3D>& ComponentStore::storageOf<Transform3D>() {
    return transforms_;
}
template <>
inline ComponentStorage<MeshRenderer>& ComponentStore::storageOf<MeshRenderer>() {
    return meshRenderers_;
}
template <>
inline ComponentStorage<InputMap>& ComponentStore::storageOf<InputMap>() {
    return inputMaps_;
}
template <>
inline const ComponentStorage<Transform3D>& ComponentStore::storageOf<Transform3D>() const {
    return transforms_;
}
template <>
inline const ComponentStorage<MeshRenderer>& ComponentStore::storageOf<MeshRenderer>() const {
    return meshRenderers_;
}
template <>
inline const ComponentStorage<InputMap>& ComponentStore::storageOf<InputMap>() const {
    return inputMaps_;
}

} // namespace vv
