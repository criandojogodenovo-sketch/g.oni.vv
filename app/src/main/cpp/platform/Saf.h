#pragma once
// platform/Saf.h — fila de resultados UI-thread → engine-thread (F5.2).
//
// HISTÓRIA: este header era a máquina de estado do SAF (F5.1-C). O SAF tree
// picker foi REMOVIDO (F5.2 — o fluxo de permissões é All Files Access, ver
// platform/StoragePerm.h); o que sobrevive é a infraestrutura de FILA que o
// hotfix F5.1 introduziu e que o StorageBridge reutiliza para o retorno das
// definições do sistema:
//
//   Java (UI thread): onActivityResult → nativeOnActivityResult (JNI)
//     → PendingResult::push (1 slot, só enfileira — NUNCA toca GL/estado)
//   Engine thread: pollResult() → handler injetado (main.cpp processa)
//
// O nome do ficheiro mantém-se (história do git + comentários do contrato);
// o conteúdo é o subconjunto vivo.
#include <mutex>
#include <string>
#include "core/Types.h"

namespace vv::saf {

// resultado encaminhado pela Java → nativo (retorno das definições; ok =
// RESULT_OK — a VERDADE da permissão é re-verificada com
// Environment.isExternalStorageManager() no processamento)
struct SafResult {
    i32 request = 0;
    bool ok = false;          // RESULT_OK
    std::string uri;          // "" (as definições não devolvem URI)
    i32 flags = 0;            // 0 (mantido por compat do formato da fila)
};

// F5.1-hotfix (auditoria JNI, fix 3): fila de 1 slot que DESACOPLA o thread
// da UI (onActivityResult) do thread da engine. Antes o handler corria no
// thread Java — sem contexto EGL — e chamava releaseAll/loadActiveScene com
// GL = crash. Agora o JNI só faz push; o loop nativo faz poll e processa no
// thread certo. GL-free / testável no hospedeiro.
class PendingResult {
public:
    void push(const SafResult& r) {
        std::lock_guard<std::mutex> lk(mu_);
        if (valid_) {
            dropped_ = true;   // resultado anterior não consumido — sobrepõe
        }
        r_ = r;
        valid_ = true;
    }

    // consome o resultado pendente; false se não há nada
    bool poll(SafResult* out) {
        std::lock_guard<std::mutex> lk(mu_);
        if (!valid_) {
            return false;
        }
        if (out) {
            *out = r_;
        }
        valid_ = false;
        return true;
    }

    // true se algum resultado foi descartado por sobreposição (diagnóstico)
    bool droppedAny() const { return dropped_; }

private:
    std::mutex mu_;
    bool valid_ = false;
    bool dropped_ = false;
    SafResult r_;
};

// handler dos resultados (retorno das definições) — assinatura estável da
// ponte (main.cpp injeta; o CI alimenta com fakes)
using ResultHandler = void (*)(void* user, const SafResult& result);

} // namespace vv::saf
