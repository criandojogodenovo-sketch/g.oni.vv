#pragma once
// core/Tic.h — entidade/nó da VV (inspiração Godot/itsmagic).
//
// F1: identidade + estado mínimo (handle generacional, nome, active, parent).
// F3: o TIC é DONO de componentes — container. Os dados vivem nos storages
// SoA da Scene (ComponentStore); o Tic expõe a API de composição:
//   addComponent<T>()        → cria e anexa (attach); nullptr se já tem
//   addComponent<T>(init)    → idem, com dados iniciais
//   getComponent<T>()        → nullptr se não tem
//   removeComponent<T>()     → detach + remove; false se não tem
//
// `scene` é back-pointer (não-dono) atribuído por Scene::create. Um Tic sem
// Scene (fora da cena) recusa composição com nullptr — nunca crasha.
#include <string>
#include "ComponentStore.h"   // storeOf() + storages (sem ciclo: não inclui Tic.h)
#include "Handle.h"

namespace vv {

class Scene;

struct Tic {
    Handle      handle{};      // handle generacional deste TIC
    std::string name;          // nome de busca (F1: primeiro ativo com o nome)
    bool        active = false;
    i32         parent = -1;   // índice do slot do TIC pai; -1 = sem pai
    Scene*      scene  = nullptr;   // F3: dono dos storages de componentes

    // ---- composição (F3) ----------------------------------------------------
    // storeOf() (decl. em ComponentStore.h, def. em ComponentStore.cpp) evita
    // acesso a membro de tipo incompleto na fase 1 — sem warnings e sem ciclo.
    template <typename T>
    T* addComponent() {
        return scene ? storeOf(scene).template add<T>(handle) : nullptr;
    }

    template <typename T>
    T* addComponent(const T& init) {
        return scene ? storeOf(scene).template add<T>(handle, init) : nullptr;
    }

    template <typename T>
    T* getComponent() {
        return scene ? storeOf(scene).template get<T>(handle) : nullptr;
    }

    template <typename T>
    const T* getComponent() const {
        return scene ? storeOf(scene).template get<T>(handle) : nullptr;
    }

    template <typename T>
    bool removeComponent() {
        return scene ? storeOf(scene).template remove<T>(handle) : false;
    }
};

} // namespace vv
