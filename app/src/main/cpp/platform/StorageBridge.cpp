// platform/StorageBridge.cpp — JNI do fluxo All Files Access (F5.2;
// device-only). Sucessora da SafBridge (F5.1-C): a fila PendingResult, o
// JNI_OnLoad com RegisterNatives e a higiene de exceções JNI (auditoria do
// hotfix) são MANTIDAS; os pickers SAF foram REMOVIDOS e a ponte agora só:
//   0. recebe o REGISTO da activity (handshake INVERTIDO — F5.3,
//      docs/HANDSHAKE_AUDIT.md: é a VvActivity — thread da UI, sempre
//      anexado à VM — que se registra; o native nunca mais tenta descobrir
//      a activity do thread do glue, cujo GetEnv devolvia JNI_EDETACHED);
//   1. verifica Environment.isExternalStorageManager();
//   2. lança ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION;
//   3. reencaminha o resultado (onActivityResult → fila → thread da engine);
//   4. exporta logs por MediaStore (sem permissões, API 29+).
//
// Java → nativo: VvActivity.onCreate/onResume chamam nativeRegisterActivity
// e onActivityResult chama o método estático nativo
// nativeOnActivityResult — ambos registados EXPLICITAMENTE em JNI_OnLoad via
// RegisterNatives (auditoria F5.1-hotfix: se falhar, o log diz exatamente
// qual método no arranque). O resultado NÃO é processado no thread da UI:
// entra na fila PendingResult e o loop da engine consome no thread certo
// (GL/estado coerentes — fix do crash de picker mantido).
//
// FindClass num thread nativo usa o classloader de sistema (não vê classes
// da app) — VvActivity é resolvida no nativeRegisterActivity (o jobject vem
// de Java) e no JNI_OnLoad (thread da UI, dentro do loadLibrary);
// Environment é classe de SISTEMA — FindClass direto funciona de qualquer
// thread anexado.
#include "platform/StorageBridge.h"
#include "platform/EngineLog.h"
#include <jni.h>
#include <cstring>

// forward declarations FILE-SCOPE dos métodos nativos (definidos no fundo
// deste ficheiro) — o JNI_OnLoad (também file-scope) regista-os por ponteiro
extern "C" JNIEXPORT void JNICALL
Java_vv_goni_VvActivity_nativeOnActivityResult(JNIEnv* env, jclass,
                                               jint request, jint result,
                                               jobject uri, jint flags);
extern "C" JNIEXPORT void JNICALL
Java_vv_goni_VvActivity_nativeRegisterActivity(JNIEnv* env, jclass,
                                               jobject activity,
                                               jstring origin);

// forward declarations FILE-SCOPE dos métodos nativos (definidos no fundo
// deste ficheiro) — o JNI_OnLoad (também file-scope) regista-os por ponteiro

namespace vv::storage {

namespace {

JavaVM* g_vm = nullptr;
jobject g_activity = nullptr;
jclass g_activityCls = nullptr;

jmethodID g_midOpenAllFiles = nullptr;   // VvActivity.openAllFilesSettings(I)V
jmethodID g_midExportLogs = nullptr;     // VvActivity.exportLogsToDownloads(String)I

// F5.3 — estado do handshake invertido: 1º registo da VvActivity visto
// (mesmo parcial) e veredito (vm + GlobalRef + método crítico OK)
bool g_registrationSeen = false;
bool g_handshake = false;

// isExternalStorageManager (android.os.Environment, estático, API 30+)
jclass   g_envCls = nullptr;
jmethodID g_midIsManager = nullptr;
bool     g_managerLookupDone = false;    // 1 tentativa (falha = API < 30)

saf::ResultHandler g_handler = nullptr;
void* g_handlerUser = nullptr;

// fila UI-thread → engine-thread (ver Saf.h — PendingResult)
saf::PendingResult g_pending;

JNIEnv* envOrNull() {
    if (!g_vm) {
        return nullptr;
    }
    JNIEnv* env = nullptr;
    if (g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        return nullptr;
    }
    return env;
}

// limpa exceção pendente e devolve true se havia uma (para log)
bool clearPendingException(JNIEnv* env) {
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return true;
    }
    return false;
}

// cache dos MÉTODOS da activity (chamado do registo — thread da UI).
// AUDITORIA (mantida): cada lookup verifica/clear exceção — um
// NoSuchMethodError pendente contaminava TODAS as chamadas JNI seguintes.
void cacheActivityMethods(JNIEnv* env) {
    g_midOpenAllFiles = env->GetMethodID(
        g_activityCls, "openAllFilesSettings", "(I)V");
    if (!g_midOpenAllFiles || clearPendingException(env)) {
        g_midOpenAllFiles = nullptr;
        elog::error("jni: VvActivity.openAllFilesSettings(I)V NÃO encontrada");
    } else {
        elog::info("jni: VvActivity.openAllFilesSettings OK");
    }

    g_midExportLogs = env->GetMethodID(
        g_activityCls, "exportLogsToDownloads", "(Ljava/lang/String;)I");
    if (!g_midExportLogs || clearPendingException(env)) {
        g_midExportLogs = nullptr;
        elog::error("jni: VvActivity.exportLogsToDownloads NÃO encontrada");
    } else {
        elog::info("jni: VvActivity.exportLogsToDownloads OK");
    }
}

} // namespace

void setHandler(saf::ResultHandler fn, void* user) {
    g_handler = fn;
    g_handlerUser = user;
}

// F5.3 — true pós-handshake invertido (registo da VvActivity + método
// crítico do fluxo All Files cacheado). false = ponte Java indisponível.
bool handshakeOk() {
    return g_handshake;
}

// consome UM resultado diferido e dispara o handler no thread CHAMADOR (o
// loop da engine chama por frame). false = nada pendente.
bool pollResult() {
    saf::SafResult r;
    if (!g_pending.poll(&r)) {
        return false;
    }
    if (g_handler) {
        g_handler(g_handlerUser, r);
    } else {
        elog::warn("storage: resultado req=%d descartado (sem handler)", r.request);
    }
    return true;
}

// ---- REGISTO da activity (handshake INVERTIDO — F5.3) -----------------------
//
// Chamado da VvActivity.onCreate e onResume (thread da UI, SEMPRE anexado
// à VM). Guarda a JavaVM + GlobalRef da activity + métodos. Idempotente:
// o re-registo (onResume) substitui o GlobalRef e re-cacha os métodos —
// sobrevive a recriação da activity.
extern "C" JNIEXPORT void JNICALL
Java_vv_goni_VvActivity_nativeRegisterActivity(JNIEnv* env, jclass,
                                               jobject activity,
                                               jstring origin) {
    if (!env || !activity) {
        elog::error("jni: nativeRegisterActivity — env/activity nulos");
        return;
    }
    // 1) eco do lado Java no engine.log — o log viewer do C33 mostra a
    //    sequência completa: "java: onCreate → nativeRegisterActivity"
    const char* o = origin ? env->GetStringUTFChars(origin, nullptr) : nullptr;
    elog::info("java: %s → nativeRegisterActivity", o ? o : "?");
    if (o) {
        env->ReleaseStringUTFChars(origin, o);
    }
    if (clearPendingException(env)) {
        elog::warn("jni: nativeRegisterActivity — exceção ao ler a origem");
    }

    // 2) JavaVM — GetJavaVM no thread anexado (UI) nunca falha; se falhar,
    //    loga e sai (handshake parcial, mensagens dirão "handshake")
    JavaVM* vm = nullptr;
    if (env->GetJavaVM(&vm) != JNI_OK || !vm) {
        elog::error("jni: GetJavaVM FALHOU no registo (thread da UI não "
                    "anexado?!) — handshake incompleto");
        g_registrationSeen = true;
        g_handshake = false;
        return;
    }
    g_vm = vm;

    // 3) GlobalRef da activity + classe — substitui os anteriores (o
    //    onResume re-regista; sem isto, recriação da activity deixa ref morta)
    if (g_activity) {
        env->DeleteGlobalRef(g_activity);
        g_activity = nullptr;
    }
    if (g_activityCls) {
        env->DeleteGlobalRef(g_activityCls);
        g_activityCls = nullptr;
    }
    g_activity = env->NewGlobalRef(activity);
    if (!g_activity) {
        elog::error("jni: NewGlobalRef(activity) FALHOU — handshake incompleto");
        g_registrationSeen = true;
        g_handshake = false;
        return;
    }
    g_activityCls = static_cast<jclass>(
        env->NewGlobalRef(env->GetObjectClass(g_activity)));
    if (!g_activityCls || clearPendingException(env)) {
        g_activityCls = nullptr;
        elog::error("jni: GetObjectClass/NewGlobalRef FALHOU — handshake incompleto");
        g_registrationSeen = true;
        g_handshake = false;
        return;
    }

    // 4) métodos da activity (higiene de exceções — cacheActivityMethods)
    cacheActivityMethods(env);

    // 5) veredito: o método crítico do fluxo All Files tem de existir;
    //    exportLogs é diagnóstico (falhar não bloqueia o fluxo de permissão)
    const bool first = !g_registrationSeen;
    g_registrationSeen = true;
    g_handshake = g_midOpenAllFiles != nullptr;
    if (g_handshake) {
        elog::info(first ? "native: activity registada"
                         : "native: activity re-registada (onResume — reforço)");
        elog::info("jni: handshake OK — vm=%p activity=%p openAllFiles=%d "
                   "exportLogs=%d",
                   reinterpret_cast<void*>(vm),
                   reinterpret_cast<void*>(g_activity),
                   g_midOpenAllFiles ? 1 : 0, g_midExportLogs ? 1 : 0);
    } else {
        elog::error("native: registo PARCIAL — openAllFilesSettings ausente "
                    "(dex/assinatura divergente?) — ponte Java indisponível");
    }
}

// 1) Environment.isExternalStorageManager() — classe de SISTEMA (boot
// classpath): FindClass direto. Em API < 30 o método não existe →
// *supported=false (o fluxo cai no app-private SEM pedir nada).
bool jniStorageApiSupported(bool* outManager) {
    if (outManager) {
        *outManager = false;
    }
    JNIEnv* env = envOrNull();
    if (!env) {
        return false;   // sem VM → sem suporte apurável (device só)
    }
    if (!g_managerLookupDone) {
        g_managerLookupDone = true;
        g_envCls = env->FindClass("android/os/Environment");
        if (!g_envCls || clearPendingException(env)) {
            g_envCls = nullptr;
            elog::error("jni: FindClass(android/os/Environment) FALHOU");
        } else {
            g_envCls = static_cast<jclass>(env->NewGlobalRef(g_envCls));
            g_midIsManager = env->GetStaticMethodID(
                g_envCls, "isExternalStorageManager", "()Z");
            if (!g_midIsManager || clearPendingException(env)) {
                g_midIsManager = nullptr;
                elog::info("jni: Environment.isExternalStorageManager AUSENTE "
                           "(API < 30) — All Files Access sem suporte");
            } else {
                elog::info("jni: Environment.isExternalStorageManager OK (API 30+)");
            }
        }
    }
    if (!g_envCls || !g_midIsManager) {
        return false;
    }
    const jboolean r = env->CallStaticBooleanMethod(g_envCls, g_midIsManager);
    if (clearPendingException(env)) {
        return false;
    }
    if (outManager) {
        *outManager = r == JNI_TRUE;
    }
    return true;
}

// 2) abre a janela de permissões do sistema (o resultado volta por
// onActivityResult(kReqAllFiles) → nativeOnActivityResult → fila).
// Diagnóstico honesto: handshake por fazer ≠ env indisponível no thread.
bool jniOpenAllFilesSettings() {
    if (!handshakeOk()) {
        elog::warn("jni: openAllFilesSettings indisponível — ponte Java "
                   "indisponível (handshake)");
        return false;
    }
    JNIEnv* env = envOrNull();
    if (!env) {
        elog::warn("jni: openAllFilesSettings indisponível — env do thread "
                   "chamador indisponível");
        return false;
    }
    env->CallVoidMethod(g_activity, g_midOpenAllFiles,
                        static_cast<jint>(kReqAllFiles));
    return !clearPendingException(env);
}

// 4) export dos logs → Downloads público (MediaStore). Mantido do hotfix.
bool jniExportLogsToDownloads(int* outCount) {
    JNIEnv* env = envOrNull();
    if (!env || !g_activity || !g_midExportLogs) {
        return false;
    }
    jstring jrel = env->NewStringUTF(vv::elog::kDownloadsRelPath);
    const jint r = env->CallIntMethod(g_activity, g_midExportLogs, jrel);
    if (jrel) {
        env->DeleteLocalRef(jrel);
    }
    if (clearPendingException(env)) {
        return false;
    }
    if (outCount) {
        *outCount = static_cast<int>(r);
    }
    return true;
}

} // namespace vv::storage

// ---- JNI_OnLoad: registo EXPLÍCITO dos métodos nativos ----------------------
//
// Mantido da auditoria F5.1-hotfix: se algo falhar, o crash/log acontece NO
// ARRANQUE com a causa escrita no engine.log (elog usa o fallback android
// porque o android_main ainda não correu).
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    // log em ficheiro desde ANTES do android_main (fallback do device)
    vv::elog::init(vv::elog::androidFallbackDir());
    vv::elog::info("jni: JNI_OnLoad — G.One VV 0.6.2 (registo explícito de "
                   "nativos; handshake invertido — F5.3)");

    JNIEnv* env = nullptr;
    if (!vm || vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        vv::elog::error("jni: JNI_OnLoad — GetEnv falhou (VM incompatível?)");
        return JNI_ERR;
    }

    static const JNINativeMethod kMethods[] = {
        { const_cast<char*>("nativeRegisterActivity"),
          const_cast<char*>("(Lvv/goni/VvActivity;Ljava/lang/String;)V"),
          reinterpret_cast<void*>(&Java_vv_goni_VvActivity_nativeRegisterActivity) },
        { const_cast<char*>("nativeOnActivityResult"),
          const_cast<char*>("(IIILandroid/net/Uri;I)V"),
          reinterpret_cast<void*>(&Java_vv_goni_VvActivity_nativeOnActivityResult) },
    };

    // JNI_OnLoad corre no thread que chamou System.loadLibrary (UI thread,
    // dentro de NativeActivity.onCreate) — FindClass VÊ as classes da app.
    jclass cls = env->FindClass("vv/goni/VvActivity");
    if (!cls) {
        env->ExceptionClear();
        vv::elog::error("jni: JNI_OnLoad — FindClass(vv/goni/VvActivity) FALHOU "
                        "(dex ausente? hasCode=false?)");
        return JNI_ERR;
    }
    if (env->RegisterNatives(cls, kMethods, 2) != JNI_OK) {
        env->ExceptionClear();
        vv::elog::error("jni: JNI_OnLoad — RegisterNatives FALHOU "
                        "(nativeRegisterActivity/nativeOnActivityResult — "
                        "assinatura divergente?)");
        return JNI_ERR;
    }
    vv::elog::info("jni: JNI_OnLoad — nativeRegisterActivity + "
                   "nativeOnActivityResult registados OK (2 métodos)");
    return JNI_VERSION_1_6;
}

// ---- callback estático chamado pela VvActivity ------------------------------
//
// Corre NO THREAD DA UI — apenas converte e enfileira (PendingResult). Todo
// o processamento acontece no loop da engine via pollResult().
extern "C" JNIEXPORT void JNICALL
Java_vv_goni_VvActivity_nativeOnActivityResult(JNIEnv* env, jclass,
                                               jint request, jint result,
                                               jobject uri, jint flags) {
    vv::saf::SafResult r;
    r.request = request;
    r.ok = result == 0 /* Activity.RESULT_OK */;
    if (uri) {
        // as definições devolvem intent SEM data — guard defensivo (o mesmo
        // código servia as URIs do SAF)
        jclass uriCls = env->GetObjectClass(uri);
        jmethodID midToString =
            env->GetMethodID(uriCls, "toString", "()Ljava/lang/String;");
        jstring jstr = static_cast<jstring>(
            env->CallObjectMethod(uri, midToString));
        if (jstr) {
            const char* c = env->GetStringUTFChars(jstr, nullptr);
            if (c) {
                r.uri = c;
                env->ReleaseStringUTFChars(jstr, c);
            }
            env->DeleteLocalRef(jstr);
        }
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
        }
    }
    r.flags = flags;
    vv::elog::info("storage: onActivityResult req=%d ok=%d (enfileirado p/ "
                   "thread da engine)", request, r.ok ? 1 : 0);
    vv::storage::g_pending.push(r);
}
