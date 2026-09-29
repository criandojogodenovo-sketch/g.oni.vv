#pragma once
// platform/Saf.h — máquina de estado do Storage Access Framework (F5.1-C).
//
// EXCEÇÃO À REGRA "zero Java" (documentada em docs/SAF_EXCEPTION.md): o SAF
// exige uma subclasse de NativeActivity para reencaminhar onActivityResult
// ao nativo — não existe caminho 100% nativo (NativeActivity não
// reencaminha resultados de Activity). A superfície Java é mínima:
// vv/goni/VvActivity.java (picker + forward JNI) e vv/goni/SafIo.java
// (DocumentFile via ContentResolver/DocumentsContract).
//
// ESTADO (GL-free / testável no hospedeiro):
//   Idle    → nenhum pedido em curso
//   Pending → picker aberto, à espera de onActivityResult
//   Granted → URI concedida + persistida (takePersistableUriPermission)
//   Denied  → utilizador cancelou / URI inválida → fallback getExternalFilesDir
//
// O handler é INJETADO (ResultHandler): no device é o bridge JNI; nos
// testes do CI é um fake que alimenta resultados simulados.
#include <mutex>
#include <string>
#include "core/Types.h"

namespace vv::saf {

// request codes do pick (estáveis entre Java e nativo)
constexpr i32 kReqPickTree = 4201;   // ACTION_OPEN_DOCUMENT_TREE (pasta do projeto)
constexpr i32 kReqImport   = 4202;   // ACTION_OPEN_DOCUMENT (importar ficheiro)
constexpr i32 kReqExport   = 4203;   // ACTION_CREATE_DOCUMENT (exportar ficheiro)

enum class State { Idle, Pending, Granted, Denied };

// resultado encaminhado pela Java → nativo
struct SafResult {
    i32 request = 0;
    bool ok = false;          // RESULT_OK
    std::string uri;          // "content://..." (vazio se cancelado)
    i32 flags = 0;            // intent flags (grant/persistable)
};

// permissões persistíveis (Intent.FLAG_GRANT_* standard)
constexpr i32 kFlagRead = 1;
constexpr i32 kFlagWrite = 2;
constexpr i32 kFlagPersistable = 64;

inline bool flagsPersistable(i32 flags) {
    return (flags & kFlagPersistable) != 0;
}
inline bool flagsReadWrite(i32 flags) {
    return (flags & kFlagRead) != 0 && (flags & kFlagWrite) != 0;
}

using ResultHandler = void (*)(void* user, const SafResult& result);

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

// máquina de estado pura (sem Android) — partilhada por device/testes
class SafStateMachine {
public:
    State state() const { return state_; }
    i32 pendingRequest() const { return pending_; }
    const std::string& grantedUri() const { return uri_; }
    bool uriUsable() const { return state_ == State::Granted && !uri_.empty(); }

    void begin(i32 request) {
        state_ = State::Pending;
        pending_ = request;
        uri_.clear();
        flags_ = 0;
    }

    // devolve true se o estado ficou GRANTED (URI não-vazia + OK)
    bool onResult(const SafResult& r) {
        const bool matches = state_ == State::Pending && r.request == pending_;
        if (!matches) {
            state_ = State::Denied;
            return false;
        }
        if (!r.ok || r.uri.empty()) {
            state_ = State::Denied;
            return false;
        }
        uri_ = r.uri;
        flags_ = r.flags;
        state_ = State::Granted;
        return true;
    }

    void reset() {
        state_ = State::Idle;
        pending_ = 0;
        uri_.clear();
        flags_ = 0;
    }

    i32 flags() const { return flags_; }

private:
    State state_ = State::Idle;
    i32 pending_ = 0;
    i32 flags_ = 0;
    std::string uri_;
};

} // namespace vv::saf
