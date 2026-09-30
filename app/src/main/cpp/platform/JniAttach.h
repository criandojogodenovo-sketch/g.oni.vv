#pragma once
// platform/JniAttach.h — DECISÃO PURA do attach de threads JNI (F5.3).
//
// REGRA (TAREFA 3): qualquer thread da engine que chame Java tem de estar
// ANEXADO à VM. O thread do glue (android_main) nasce desanexado — GetEnv
// devolve JNI_EDETACHED (a causa única das pontes mortas 0.6.0→0.6.2, ver
// docs/HANDSHAKE_AUDIT.md). Nenhum JNIEnv* é assumido não-nulo: cada
// falha é logada COM O CÓDIGO DE ERRO.
//
// Este header é a tabela de decisão PURA (sem Android, sem JNI real — só
// os códigos numéricos): o StorageBridge (device) usa-a para escolher a
// ação e o CI afere a tabela diretamente (test_handshake.cpp).
//
//   rc de GetEnv        → ação
//   0  (JNI_OK)         → Use      (env válido; usar logo)
//   -2 (JNI_EDETACHED)  → Attach   (AttachCurrentThread com nome de thread)
//   resto (EINVAL/EEXIST/…) → Fail (logar rc e não chamar Java)
//
// DetachCurrentThread NÃO é chamado: os threads da engine vivem até ao fim
// do processo e o detach/re-attach por frame seria desperdício (e risco de
// refs locais órfãs). O attach é permanente e nomeado — o nome aparece no
// logcat/jstack e nos logs de diagnóstico.
namespace vv::jni {

// códigos de retorno da JNI (iguais aos do jni.h real; duplicados como
// inteiros para este header compilar no CI sem <jni.h>)
constexpr int kJniOk        = 0;
constexpr int kJniEdetached = -2;   // thread não anexado à VM
constexpr int kJniEexist    = -3;
constexpr int kJniEinval    = -4;

enum class AttachAction { Use, Attach, Fail };

inline AttachAction planAttach(int getEnvResult) {
    if (getEnvResult == kJniOk) {
        return AttachAction::Use;
    }
    if (getEnvResult == kJniEdetached) {
        return AttachAction::Attach;
    }
    return AttachAction::Fail;
}

inline const char* attachActionLabel(AttachAction a) {
    switch (a) {
        case AttachAction::Use:    return "env pronto";
        case AttachAction::Attach: return "attach necessário";
        default:                   return "falha (VM incompatível?)";
    }
}

// nome canónico da thread da engine no AttachCurrentThread — aparece nos
// logs ("jni: thread 'goni-engine' anexado") e é aferível no CI
constexpr const char* kEngineThreadName = "goni-engine";

} // namespace vv::jni
