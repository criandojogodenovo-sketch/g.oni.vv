// platform/SafBridge.cpp — JNI do SAF (F5.1-C; auditoria+fix F5.1-hotfix,
// device-only).
//
// Java → nativo: VvActivity.onActivityResult chama o método estático nativo
// nativeOnActivityResult — registado EXPLICITAMENTE em JNI_OnLoad via
// RegisterNatives (a auditoria F5.1-hotfix trocou a resolução por nome:
// se falhar, o log diz exatamente qual método, sem UnsatisfiedLinkError
// misterioso no primeiro onActivityResult). O resultado NÃO é processado
// no thread da UI: entra na fila PendingResult e o loop da engine consome
// no thread certo (GL/estado coerentes — fix do crash de picker).
//
// nativo → Java: openTreePicker/openImportPicker/openExportPicker chamam
// métodos da VvActivity; o I/O (SafIo) é resolvido pelo CLASSLOADER da
// activity (FindClass num thread nativo usa o classloader de sistema, que
// não vê classes da app — workaround padrão loadClass).
//
// AUDITORIA (docs/AUDIT_SAF_JNI.md): toda a ponte agora verifica/clear
// exceções JNI pendentes após CADA lookup/chamada (uma exceção pendente
// esquecida em initJava fazia as chamadas JNI seguintes terem comportamento
// indefinido — candidato real ao crash de arranque do C33).
#include "platform/SafBridge.h"
#include "platform/EngineLog.h"
#include <jni.h>
#include <cstring>

// forward declaration FILE-SCOPE do método nativo (definido no fundo deste
// ficheiro) — o JNI_OnLoad (também file-scope) regista-o por ponteiro
extern "C" JNIEXPORT void JNICALL
Java_vv_goni_VvActivity_nativeOnActivityResult(JNIEnv* env, jclass,
                                               jint request, jint result,
                                               jobject uri, jint flags);

namespace vv::saf {

namespace {

JavaVM* g_vm = nullptr;
jobject g_activity = nullptr;
jclass g_activityCls = nullptr;
jclass g_safIoCls = nullptr;

jmethodID g_midOpenTree = nullptr;
jmethodID g_midOpenImport = nullptr;
jmethodID g_midOpenExport = nullptr;

ResultHandler g_handler = nullptr;
void* g_handlerUser = nullptr;

// fila UI-thread → engine-thread (ver Saf.h — PendingResult)
PendingResult g_pending;

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

jclass loadAppClass(JNIEnv* env, const char* name) {
    // FindClass no thread nativo = classloader de sistema → não vê classes
    // da app. Usa o classloader DA ACTIVITY (padrão para NativeActivity).
    jclass clsObj = env->GetObjectClass(g_activity);
    jmethodID midLoader = env->GetMethodID(
        clsObj, "getClassLoader", "()Ljava/lang/ClassLoader;");
    if (!midLoader || clearPendingException(env)) {
        elog::error("jni: loadAppClass(%s) — getClassLoader indisponível", name);
        return nullptr;
    }
    jobject loader = env->CallObjectMethod(g_activity, midLoader);
    if (clearPendingException(env)) {
        elog::error("jni: loadAppClass(%s) — getClassLoader lançou exceção", name);
        return nullptr;
    }
    if (!loader) {
        elog::error("jni: loadAppClass(%s) — classloader null", name);
        return nullptr;
    }
    jclass clsLoader = env->GetObjectClass(loader);
    jmethodID midLoad = env->GetMethodID(
        clsLoader, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
    if (!midLoad || clearPendingException(env)) {
        elog::error("jni: loadAppClass(%s) — loadClass indisponível", name);
        return nullptr;
    }
    jstring jname = env->NewStringUTF(name);
    jclass result =
        static_cast<jclass>(env->CallObjectMethod(loader, midLoad, jname));
    const bool threw = clearPendingException(env);
    env->DeleteLocalRef(jname);
    if (threw || !result) {
        elog::error("jni: loadAppClass(%s) FALHOU — classe ausente do dex?", name);
        return nullptr;
    }
    return result;
}

} // namespace

void setHandler(ResultHandler fn, void* user) {
    g_handler = fn;
    g_handlerUser = user;
}

// F5.1-hotfix: consome UM resultado diferido e dispara o handler no thread
// CHAMADOR (o loop da engine chama por frame). false = nada pendente.
bool pollResult() {
    SafResult r;
    if (!g_pending.poll(&r)) {
        return false;
    }
    if (g_handler) {
        g_handler(g_handlerUser, r);
    } else {
        elog::warn("saf: resultado req=%d descartado (sem handler)", r.request);
    }
    return true;
}

void initJava(void* vm, void* activityObject) {
    g_vm = static_cast<JavaVM*>(vm);
    JNIEnv* env = envOrNull();
    if (!env || !activityObject) {
        elog::error("jni: initJava — env/activity indisponíveis (vm=%p)", vm);
        return;
    }
    elog::info("jni: initJava — cache de activity/classes/métodos");
    g_activity = env->NewGlobalRef(static_cast<jobject>(activityObject));
    g_activityCls = static_cast<jclass>(
        env->NewGlobalRef(env->GetObjectClass(g_activity)));

    // AUDITORIA: cada lookup verifica/clear exceção — antes, um
    // NoSuchMethodError pendente aqui contaminava TODAS as chamadas JNI
    // seguintes do boot (UB/crash em ART).
    g_midOpenTree = env->GetMethodID(g_activityCls, "openTreePicker", "(I)V");
    if (!g_midOpenTree || clearPendingException(env)) {
        g_midOpenTree = nullptr;
        elog::error("jni: VvActivity.openTreePicker(I)V NÃO encontrada");
    } else {
        elog::info("jni: VvActivity.openTreePicker OK");
    }
    g_midOpenImport = env->GetMethodID(g_activityCls, "openImportPicker", "(I)V");
    if (!g_midOpenImport || clearPendingException(env)) {
        g_midOpenImport = nullptr;
        elog::error("jni: VvActivity.openImportPicker(I)V NÃO encontrada");
    } else {
        elog::info("jni: VvActivity.openImportPicker OK");
    }
    g_midOpenExport = env->GetMethodID(
        g_activityCls, "openExportPicker", "(ILjava/lang/String;)V");
    if (!g_midOpenExport || clearPendingException(env)) {
        g_midOpenExport = nullptr;
        elog::error("jni: VvActivity.openExportPicker(ILjava/lang/String;)V NÃO encontrada");
    } else {
        elog::info("jni: VvActivity.openExportPicker OK");
    }

    jclass io = loadAppClass(env, "vv.goni.SafIo");
    if (io) {
        g_safIoCls = static_cast<jclass>(env->NewGlobalRef(io));
        elog::info("jni: vv.goni.SafIo carregada (classloader da activity)");
    } else {
        elog::error("jni: vv.goni.SafIo indisponível — I/O SAF desativado");
    }
}

bool openTreePicker(void* activityObject, i32 request) {
    JNIEnv* env = envOrNull();
    if (!env || !g_activity || !g_midOpenTree) {
        return false;
    }
    env->CallVoidMethod(g_activity, g_midOpenTree, static_cast<jint>(request));
    return !clearPendingException(env);
}

bool openImportPicker(void* activityObject, i32 request) {
    JNIEnv* env = envOrNull();
    if (!env || !g_activity || !g_midOpenImport) {
        return false;
    }
    env->CallVoidMethod(g_activity, g_midOpenImport, static_cast<jint>(request));
    return !clearPendingException(env);
}

bool openExportPicker(void* activityObject, i32 request,
                      const char* suggestedName) {
    JNIEnv* env = envOrNull();
    if (!env || !g_activity || !g_midOpenExport) {
        return false;
    }
    jstring jname = suggestedName ? env->NewStringUTF(suggestedName) : nullptr;
    env->CallVoidMethod(g_activity, g_midOpenExport,
                        static_cast<jint>(request), jname);
    if (jname) {
        env->DeleteLocalRef(jname);
    }
    return !clearPendingException(env);
}

// F5.1-hotfix (parte 1.4): export dos logs → Downloads público (MediaStore).
// Chama VvActivity.exportLogsToDownloads("Download/GOneVV/logs") — o path é
// a constante única vv::elog::kDownloadsRelPath (afervel no CI). true = a
// chamada Java correu (código de retorno em *outCount; negativo = falha).
bool jniExportLogsToDownloads(int* outCount) {
    JNIEnv* env = envOrNull();
    if (!env || !g_activity || !g_activityCls) {
        return false;
    }
    static jmethodID midExportLogs = nullptr;   // cache (classe vive o processo)
    if (!midExportLogs) {
        midExportLogs = env->GetMethodID(
            g_activityCls, "exportLogsToDownloads", "(Ljava/lang/String;)I");
        if (!midExportLogs || clearPendingException(env)) {
            elog::error("jni: VvActivity.exportLogsToDownloads NÃO encontrada");
            return false;
        }
    }
    jstring jrel = env->NewStringUTF(vv::elog::kDownloadsRelPath);
    const jint r = env->CallIntMethod(g_activity, midExportLogs, jrel);
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

} // namespace vv::saf

// ---- JNI_OnLoad: registo EXPLÍCITO dos métodos nativos ----------------------
//
// A auditoria F5.1-hotfix trocou a resolução lazy por nome (que só falhava
// no 1º onActivityResult, sem log) por RegisterNatives AQUI: se algo falhar,
// o crash/log acontece NO ARRANQUE com a causa escrita no engine.log
// (elog usa o fallback android porque o android_main ainda não correu).
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    // log em ficheiro desde ANTES do android_main (fallback do device)
    vv::elog::init(vv::elog::androidFallbackDir());
    vv::elog::info("jni: JNI_OnLoad — G.One VV 0.6.1 (registo explícito de nativos)");

    JNIEnv* env = nullptr;
    if (!vm || vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        vv::elog::error("jni: JNI_OnLoad — GetEnv falhou (VM incompatível?)");
        return JNI_ERR;
    }

    static const JNINativeMethod kMethods[] = {
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
    if (env->RegisterNatives(cls, kMethods, 1) != JNI_OK) {
        env->ExceptionClear();
        vv::elog::error("jni: JNI_OnLoad — RegisterNatives(nativeOnActivityResult "
                        "(IIILandroid/net/Uri;I)V) FALHOU (assinatura divergente?)");
        return JNI_ERR;
    }
    vv::elog::info("jni: JNI_OnLoad — nativeOnActivityResult registado OK (1 método)");
    return JNI_VERSION_1_6;
}

// ---- callback estático chamado pela VvActivity ------------------------------
//
// F5.1-hotfix: corre NO THREAD DA UI — apenas converte e enfileira
// (PendingResult). Todo o processamento (SAF state machine, storage, GL)
// acontece no loop da engine via pollResult().
extern "C" JNIEXPORT void JNICALL
Java_vv_goni_VvActivity_nativeOnActivityResult(JNIEnv* env, jclass,
                                               jint request, jint result,
                                               jobject uri, jint flags) {
    vv::saf::SafResult r;
    r.request = request;
    r.ok = result == 0 /* Activity.RESULT_OK */ && uri != nullptr;
    if (uri) {
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
    vv::elog::info("saf: onActivityResult req=%d ok=%d flags=%d (enfileirado p/ "
                   "thread da engine)", request, r.ok ? 1 : 0, flags);
    vv::saf::g_pending.push(r);   // acesso pelo namespace global do ficheiro
}
