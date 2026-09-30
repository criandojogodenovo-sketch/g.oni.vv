#pragma once
// platform/StoragePerm.h — fluxo de permissão "All Files Access" (F5.2).
//
// MODELO (o mesmo do Godot e de outros editores no Android): a app PERGUNTA
// dentro do editor ("Precisa de acesso a todos os ficheiros para importar/
// exportar projetos"), abre as DEFINIÇÕES DO SISTEMA
// (ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION), o utilizador ativa o
// interruptor manualmente, e ao voltar a app verifica
// Environment.isExternalStorageManager() — concedido → File API POSIX direta
// em qualquer pasta pública (Download, Documents…); recusado/sem suporte →
// modo app-private (getExternalFilesDir, sem permissões).
//
// O SAF tree picker (F5.1-C) foi REMOVIDO: mais complexo e menos familiar —
// no C33 mostrava "SAF indisponível" sem nunca pedir permissão.
//
// Este header é a MÁQUINA DE ESTADO pura (GL-free, sem Android): o mesmo
// código corre no device e nos testes do CI. A ponte JNI vive em
// platform/StorageBridge.{h,cpp} (device-only) e a UI do diálogo no
// EditorUi (overlay mono).
//
// Sequência completa (numeros como nos testes):
//   1. requestAction(Import/Export)  → true = DIÁLOGO deve mostrar;
//   2. dialogAccept()                → PendingSettings + consumeOpenSettings()
//                                      (o device lança o intent aqui);
//      dialogCancel()                → ação abortada (próxima tentativa
//                                      pergunta de novo — fallback ativo);
//   3. onSettingsReturn(granted)     → Granted (File API direta) ou Idle;
//   4. takePendingAction()           → a ação retomada pós-concessão.
//
// systemSupported=false (API < 30: isExternalStorageManager não existe)
// → NUNCA pede: resolveMode devolve AppPrivate e o Settings mostra o modo.
#include <string>
#include "core/Types.h"

namespace vv::storage {

// request code do retorno das definições (espelhado em VvActivity.java —
// DEVE ser igual; o Java reencaminha onActivityResult(req=4301) por JNI)
constexpr i32 kReqAllFiles = 4301;

// action EXATA do intent das definições (constante única afervel no CI —
// a ponte JNI não inventa strings; o Java usa Settings.ACTION_MANAGE_…,
// cujo valor é este)
constexpr const char* kSettingsAction =
    "android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION";

// modo de armazenamento ativo (Settings mostra qual)
enum class Mode {
    Unknown,     // ainda não verificado (boot verifica)
    AllFiles,    // MANAGE_EXTERNAL_STORAGE concedido → File API direta
    AppPrivate   // recusado/sem suporte → getExternalFilesDir (sem permissões)
};

// estado do fluxo
enum class FlowState {
    Idle,             // nada em curso (também = recusou e pode tentar de novo)
    DialogOpen,       // diálogo in-app visível
    PendingSettings,  // janela de permissões do sistema aberta
    Granted,          // isExternalStorageManager()==true
    Unsupported       // sistema sem All Files Access (API < 30)
};

// ação pendente retomada após a concessão
enum class Action { None, Import, Export };

// rótulo curto do modo p/ o Settings (mono, minúsculas)
inline const char* modeLabel(Mode m) {
    switch (m) {
        case Mode::AllFiles:   return "all files";
        case Mode::AppPrivate: return "app-private";
        default:               return "?";
    }
}

// F5.3 — causa REAL do bloqueio de import/export (mensagens honestas).
//
// REGRESSÃO QUE ESTE ENUM FIXA: no 0.6.2 o C33 mostrava "sistema sem All
// Files Access" quando a causa verdadeira era a PONTE JAVA MORTA (handshake
// nunca concluído — GetEnv EDETACHED no thread do glue). systemSupported
// false significa duas coisas diferentes e a mensagem tem de distinguir:
//   BridgeDown   — a VvActivity não se registou no native (handshake);
//   Unsupported  — o SISTEMA não tem a API (Android < 11: sem
//                  isExternalStorageManager).
enum class BlockReason {
    None,          // pode prosseguir (handshake OK + sistema suporta)
    BridgeDown,    // ponte Java indisponível (handshake)
    Unsupported    // sistema sem All Files Access (API < 30)
};

// mapeamento puro afervel no CI: handshake em baixo SOBREPUJA o sistema —
// sem ponte nem dá para perguntar ao sistema, logo a causa reportada é a
// ponte (nunca mentir dizendo que o sistema não suporta)
inline BlockReason blockReason(bool handshakeOk, bool systemSupported) {
    if (!handshakeOk) {
        return BlockReason::BridgeDown;
    }
    if (!systemSupported) {
        return BlockReason::Unsupported;
    }
    return BlockReason::None;
}

// mensagens EXATAS do toast/log (constantes únicas afervéis no CI — o
// teste de regressão trava estas strings)
inline const char* blockMessage(BlockReason r) {
    switch (r) {
        case BlockReason::BridgeDown:
            return "ponte Java indisponível (handshake)";
        case BlockReason::Unsupported:
            return "sistema sem All Files Access — modo app-private";
        default:
            return "";
    }
}

// resolução do modo: sem suporte OU sem concessão → app-private.
inline Mode resolveMode(bool systemSupported, bool isManager) {
    if (!systemSupported) {
        return Mode::AppPrivate;
    }
    return isManager ? Mode::AllFiles : Mode::AppPrivate;
}

class PermFlow {
public:
    FlowState state() const { return state_; }
    Mode      mode() const { return mode_; }
    bool      dialogOpen() const { return state_ == FlowState::DialogOpen; }
    Action    pendingAction() const { return pending_; }
    int       attempts() const { return attempts_; }   // diagnóstico (log)

    void setMode(Mode m) {
        mode_ = m;
        if (m == Mode::AllFiles) {
            state_ = FlowState::Granted;
        }
    }

    // passo 1 — o main chama quando o utilizador tenta Importar/Export e o
    // modo ainda não é AllFiles. Devolve true se o DIÁLOGO deve mostrar
    // ESTE frame. systemSupported=false → false SEM diálogo (fallback
    // imediato; o estado fica Unsupported e o Settings mostra "app-private").
    bool requestAction(Action a, bool systemSupported) {
        ++attempts_;
        if (!systemSupported) {
            state_ = FlowState::Unsupported;
            mode_  = Mode::AppPrivate;
            pending_ = Action::None;
            return false;
        }
        if (mode_ == Mode::AllFiles) {
            return false;   // já concedido — o main executa diretamente
        }
        pending_ = a;
        state_   = FlowState::DialogOpen;
        return true;
    }

    // passo 2a — "Permitir" no diálogo
    void dialogAccept() {
        if (state_ != FlowState::DialogOpen) {
            return;
        }
        state_   = FlowState::PendingSettings;
        openReq_ = true;   // o device consome e lança o intent
    }

    // passo 2b — "Cancelar" no diálogo: ação abortada, fluxo volta a Idle
    // e o modo fica app-private (recusa = sem concessão — o Settings mostra
    // o modo ativo). A próxima tentativa ABERTA pelo utilizador pergunta
    // de novo — mesma semântica do Godot.
    void dialogCancel() {
        if (state_ != FlowState::DialogOpen) {
            return;
        }
        state_   = FlowState::Idle;
        mode_    = Mode::AppPrivate;
        pending_ = Action::None;
    }

    // passo 2 — consome o pedido de abrir as definições (1×). O device faz
    // aqui a chamada JNI que lança ACTION_MANAGE_APP_ALL_FILES_ACCESS_…
    bool consumeOpenSettings() {
        const bool r = openReq_;
        openReq_ = false;
        return r;
    }

    // passo 3 — retorno das definições (onActivityResult kReqAllFiles ou
    // re-verificação no boot). granted → Granted + modo AllFiles (o main
    // retoma takePendingAction()); senão volta a Idle — o utilizador pode
    // tentar de novo (o modo continua AppPrivate).
    void onSettingsReturn(bool granted) {
        if (granted) {
            state_ = FlowState::Granted;
            mode_  = Mode::AllFiles;
        } else {
            state_   = FlowState::Idle;
            mode_    = Mode::AppPrivate;
            pending_ = Action::None;
        }
    }

    // passo 4 — consome a ação retomada (Import/Export) pós-concessão
    Action takePendingAction() {
        const Action a = pending_;
        pending_ = Action::None;
        return a;
    }

private:
    FlowState state_  = FlowState::Idle;
    Mode      mode_   = Mode::Unknown;
    Action    pending_ = Action::None;
    bool      openReq_ = false;
    int       attempts_ = 0;
};

// F5.5 — RE-VERIFICAÇÃO NO RESUME (política pura; o main fornece a leitura
// JNI de Environment.isExternalStorageManager e chama isto no APP_CMD_RESUME).
//
// PORQUÊ: o retorno das definições chega normalmente pelo onActivityResult
// (o Java corre-o ANTES do onResume — ver VvActivity), mas NEM TODO ocrã de
// settings OEM termina com setResult: alguns voltam SEM resultado nenhum e o
// fluxo ficaria eternamente em PendingSettings. O resume é o ponto
// GARANTIDO de re-checagem (a activity está visível outra vez).
//
// CONTRATO (o main cumpre esta ordem):
//   1. ANTES de chamar, DRENAR a fila de resultados (pollResult) — se o
//      onActivityResult já correu, o onSettingsReturn tratou do retorno e
//      resumeRecheck devolve false SEM mexer no estado (nada duplica);
//   2. ainda PendingSettings → decide AQUI com a verificação FRESCA:
//        concedido  → Granted + modo AllFiles (o main retoma a ação
//                     pendente via takePendingAction — import prossegue);
//        recusado   → Idle + modo AppPrivate (o main mostra o toast claro
//                     "acesso não ativado" — SEM relançar as definições:
//                     nenhuma repetição automática, nenhum loop);
//   3. sem PendingSettings (Idle/Granted/Unsupported/DialogOpen) → false.
//
// Devolve true se o estado foi consumido NESTE ponto (o main loga
// "storage: all-files granted=1/0" e mostra o toast correspondente).
inline bool resumeRecheck(PermFlow& f, bool systemSupported, bool isManager) {
    if (f.state() != FlowState::PendingSettings) {
        return false;
    }
    f.onSettingsReturn(systemSupported && isManager);
    return true;
}

} // namespace vv::storage
