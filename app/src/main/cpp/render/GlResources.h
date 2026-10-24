#pragma once
// render/GlResources.h — 0.12-Z FASE 1: O GLResourceManager (a recriação
// correta do contexto GL).
//
// O CONTRATO (o dono): o contexto EGL pode MORRER a qualquer pausa,
// rotação, mudança de janela ou falta de memória (e o caso "preservado"
// também tem de funcionar). NENHUM handle GL nem uniform location em
// cache/static sobrevive ao term — TUDO se invalida e se recria a partir
// de dados em CPU ou ficheiros. O gestor é a FONTE ÚNICA dessa lista:
//
//   · registerLayer(name, owner, invalidate, recreate) — UMA vez no
//     arranque do processo, na ORDEM da 0.6.7 (a mesma do term manual que
//     este gestor substituiu);
//   · invalidateAll() — o TERM_WINDOW e a perda de contexto: corre os
//     invalidates pela ordem de registo (tudo com o contexto AINDA
//     corrente — o EGL shutdown é SEMPRE o último a correr, fora daqui);
//   · recreateAll() — o INIT_WINDOW e a recuperação: corre os recreates
//     pela mesma ordem; o 1.º que falha para o ciclo e é reportado
//     (lastFailed + o log "glresources: camada X FALHOU");
//   · recreateLayer(name) — para o boot em passos numerados do dono (a
//     fonte recria no [boot 3/6], o render no [boot 4/6]).
//
// GL-free (puras de ponteiros de função — testável no CI sem GLES real).
// NADA de inicialização "uma só vez" aqui: o recreate pode correr MUITAS
// vezes no mesmo processo (o init de cada camada começa pelo destroy do
// estado velho — o padrão 0.6.7).
#include "core/Types.h"
#include "platform/EngineLog.h"
#include <cstring>

namespace vv {

class GLResourceManager {
public:
    using Fn = bool (*)(void* owner);

    static constexpr u32 kMaxLayers = 16;

    // o registo (o boot do processo chama uma vez por camada, pela ordem
    // da destruição 0.6.7). owner = o objeto global da camada.
    void registerLayer(const char* name, void* owner, Fn invalidate,
                       Fn recreate) {
        if (count_ >= kMaxLayers) {
            elog::error("glresources: registo CHEIO (%d) — %s fora",
                        static_cast<int>(kMaxLayers), name);
            return;
        }
        Layer& l = layers_[count_++];
        l.name = name;
        l.owner = owner;
        l.invalidate = invalidate;
        l.recreate = recreate;
        l.valid = true;   // o estado GL assume-se vivo no registo
    }

    // TERM / perda de contexto: os invalidates pela ORDEM de registo.
    // Nunca lança nem para — uma camada que falhe ao morrer é logada e o
    // ciclo continua (o próximo recreate recomeça pelo destroy de qualquer
    // forma).
    void invalidateAll() {
        for (u32 i = 0; i < count_; ++i) {
            Layer& l = layers_[i];
            if (!l.valid) {
                continue;   // já inválida (term duplo, perda em cadeia)
            }
            if (l.invalidate && !l.invalidate(l.owner)) {
                elog::warn("glresources: camada %s FALHOU ao invalidar "
                           "(o recreate recomeça pelo destroy)", l.name);
            }
            l.valid = false;
            ++invalidations_;
        }
    }

    // INIT / recuperação: os recreates pela ORDEM de registo; para no
    // 1.º falhado (o chamador decide — o render direto ao ecrã, o aviso).
    bool recreateAll() {
        for (u32 i = 0; i < count_; ++i) {
            if (!recreateLayerAt(i)) {
                return false;
            }
        }
        return true;
    }

    // o recreate de UMA camada (o boot em passos do dono usa-a)
    bool recreateLayer(const char* name) {
        for (u32 i = 0; i < count_; ++i) {
            if (std::strcmp(layers_[i].name, name) == 0) {
                return recreateLayerAt(i);
            }
        }
        elog::warn("glresources: camada %s não registada", name);
        return false;
    }

    u32 count() const { return count_; }
    u32 invalidations() const { return invalidations_; }
    u32 recreations() const { return recreations_; }
    const char* lastFailed() const { return lastFailed_; }

    bool isValid(const char* name) const {
        for (u32 i = 0; i < count_; ++i) {
            if (std::strcmp(layers_[i].name, name) == 0) {
                return layers_[i].valid;
            }
        }
        return false;
    }

private:
    struct Layer {
        const char* name = "";
        void* owner = nullptr;
        Fn invalidate = nullptr;
        Fn recreate = nullptr;
        bool valid = true;
    };

    bool recreateLayerAt(u32 i) {
        Layer& l = layers_[i];
        if (l.recreate && !l.recreate(l.owner)) {
            lastFailed_ = l.name;
            elog::error("glresources: camada %s FALHOU a recriar "
                        "(o render degrada e o aviso vai à Consola)",
                        l.name);
            return false;
        }
        l.valid = true;
        ++recreations_;
        return true;
    }

    Layer layers_[kMaxLayers];
    u32 count_ = 0;
    u32 invalidations_ = 0;
    u32 recreations_ = 0;
    const char* lastFailed_ = "";
};

} // namespace vv
