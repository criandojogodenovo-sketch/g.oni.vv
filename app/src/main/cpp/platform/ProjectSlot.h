#pragma once
// platform/ProjectSlot.h — fila 1-slot Activity→android_main do projeto
// escolhido no Gestor de Projetos (F5.4).
//
// TIMING: o android_main corre no thread do glue e começa ANTES do corpo
// do VvActivity.onCreate terminar (a activity lança o native em
// super.onCreate e SÓ DEPOIS entrega o projeto). O boot então ESPERE
// (waitFor com timeout) pelo pedido que a activity enfileira em
// nativeOpenProject — o mesmo padrão da fila PendingResult do retorno
// das permissões (F5.1-hotfix): a Java só empurra, o thread da engine
// consome no momento certo.
//
// Sem pedido no timeout (lançamento direto, arranque sem gesto, handshake
// morto) o boot segue no modo app-private — comportamento 0.6.x intacto.
//
// GL-free / Android-free: testável no hospedeiro (test_saf_project).
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>

namespace vv::storage {

// pedido do Gestor de Projetos (URI da pasta SAF + nome do projeto)
struct ProjectRequest {
    std::string treeUri;   // content://…/tree/… (takePersistableUriPermission já feito na Java)
    std::string name;      // nome escolhido no gestor
};

class ProjectSlot {
public:
    // a Activity (thread da UI) empurra o projeto escolhido; se já havia um
    // não consumido, sobrepõe (onResume não re-empurra — só onCreate)
    void push(const ProjectRequest& r) {
        {
            std::lock_guard<std::mutex> lk(mu_);
            if (valid_) {
                ++dropped_;
            }
            req_ = r;
            valid_ = true;
            if (waitDone_) {
                ++late_;   // chegou DEPOIS do boot desistir (diagnóstico)
            }
        }
        cv_.notify_all();
    }

    // o boot espera até timeoutMs; true = consumiu UM pedido
    bool waitFor(ProjectRequest* out, int timeoutMs) {
        std::unique_lock<std::mutex> lk(mu_);
        const bool got = cv_.wait_for(lk, std::chrono::milliseconds(timeoutMs),
                                      [this] { return valid_; });
        waitDone_ = true;
        if (!got) {
            return false;
        }
        if (out) {
            *out = req_;
        }
        valid_ = false;
        return true;
    }

    // não-bloqueante (testes/poll)
    bool tryPoll(ProjectRequest* out) {
        std::lock_guard<std::mutex> lk(mu_);
        waitDone_ = true;
        if (!valid_) {
            return false;
        }
        if (out) {
            *out = req_;
        }
        valid_ = false;
        return true;
    }

    int droppedAny() const {
        std::lock_guard<std::mutex> lk(mu_);
        return dropped_;
    }

    // nº de pushes chegados DEPOIS de o boot ter desistido (diagnóstico —
    // se > 0, o gesto do utilizador foi mais lento que o timeout)
    int latePushes() const {
        std::lock_guard<std::mutex> lk(mu_);
        return late_;
    }

private:
    mutable std::mutex mu_;
    std::condition_variable cv_;
    bool valid_ = false;
    bool waitDone_ = false;
    int dropped_ = 0;
    int late_ = 0;
    ProjectRequest req_;
};

} // namespace vv::storage
