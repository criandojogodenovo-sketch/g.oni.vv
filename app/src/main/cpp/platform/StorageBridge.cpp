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
#include "platform/JniAttach.h"
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
extern "C" JNIEXPORT void JNICALL
Java_vv_goni_VvActivity_nativeOpenProject(JNIEnv* env, jclass,
                                          jstring treeUri, jstring name);

namespace vv::storage {

namespace {

JavaVM* g_vm = nullptr;
jobject g_activity = nullptr;
jclass g_activityCls = nullptr;

jmethodID g_midOpenAllFiles = nullptr;   // VvActivity.openAllFilesSettings(I)V
jmethodID g_midExportLogs = nullptr;     // VvActivity.exportLogsToDownloads(String)I
// 0.6.7 — "Sair para projetos" (diagnóstico, como bridgeDelete: falhar
// não bloqueia o fluxo principal — o editor mostra um toast honesto)
jmethodID g_midFinish = nullptr;         // VvActivity.bridgeFinish()V

// F5.4 — mids da ponte SAF (métodos de INSTÂNCIA da VvActivity, chamados
// do thread da engine com attachedEnv; a Java trata DocumentsContract,
// ContentResolver e ParcelFileDescriptor — o native só recebe URIs e fds)
jmethodID g_midRootDoc = nullptr;  // bridgeRootDoc(String)String
jmethodID g_midOpenFd  = nullptr;  // bridgeOpenFd(String,String)I
jmethodID g_midList    = nullptr;  // bridgeList(String)[Ljava/lang/String;
jmethodID g_midCreate  = nullptr;  // bridgeCreate(String,String,String)String
jmethodID g_midDelete  = nullptr;  // bridgeDelete(String)Z
// F5.4-hotfix — verificação de existência DEDICADA (query exata por
// displayName; o createDocument só pode correr após ausência CONFIRMADA)
jmethodID g_midFindFile = nullptr; // bridgeFindFile(String,String)String

// F5.4 — fila do projeto escolhido no Gestor (a activity empurra, o boot
// espera com timeout)
ProjectSlot g_projectSlot;

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

// F5.3 (TAREFA 3) — env do thread CHAMADOR, com ATTACH explícito.
// O thread do glue (android_main) e o thread da engine nascem desanexados:
// GetEnv devolve JNI_EDETACHED. O attach é decidido por jni::planAttach
// (tabela pura afervel no CI), é NOMEADO ("goni-engine" — aparece no
// logcat/jstack) e PERMANENTE (sem Detach — os threads vivem até ao fim do
// processo). Cada falha é logada COM O CÓDIGO DE ERRO. Nenhum JNIEnv* é
// assumido não-nulo.
JNIEnv* attachedEnv() {
    if (!g_vm) {
        return nullptr;   // sem registo da activity (handshake) — sem VM
    }
    JNIEnv* env = nullptr;
    const jint rc = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    const jni::AttachAction plan = jni::planAttach(rc);
    if (plan == jni::AttachAction::Use) {
        return env;
    }
    if (plan == jni::AttachAction::Attach) {
        JavaVMAttachArgs args {};
        args.version = JNI_VERSION_1_6;
        args.name    = jni::kEngineThreadName;
        args.group   = nullptr;
        const jint arc = g_vm->AttachCurrentThread(&env, &args);
        if (arc != jni::kJniOk || !env) {
            elog::error("jni: AttachCurrentThread('%s') FALHOU rc=%d — "
                        "chamadas Java indisponíveis neste thread",
                        jni::kEngineThreadName, static_cast<int>(arc));
            return nullptr;
        }
        elog::info("jni: thread '%s' anexado à VM (AttachCurrentThread OK)",
                   jni::kEngineThreadName);
        return env;   // SEM Detach: thread da engine vive até ao fim
    }
    elog::error("jni: GetEnv FALHOU rc=%d (%s) — chamadas Java indisponíveis",
                static_cast<int>(rc), jni::attachActionLabel(plan));
    return nullptr;
}

// limpa exceção pendente e devolve true se havia uma (para log)
bool clearPendingException(JNIEnv* env) {
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
        return true;
    }
    return false;
}

// ---- F5.4: registo IDEMPOTENTE dos nativos (defesa em profundidade) --------
//
// CAUSA RAIZ do UnsatisfiedLinkError no RMX3624 (0.6.3): o
// android.app.NativeActivity carrega a lib com dlopen(RTLD_LOCAL) DIRETO
// (loadNativeCode_native) — um dlopen cru NÃO corre o JNI_OnLoad nem
// coloca a lib no mapa de resolução do JVM, logo o RegisterNatives nunca
// corria no device e a busca por nome não via a lib. O FIX primário é o
// System.loadLibrary no static init da VvActivity (agora o JNI_OnLoad
// corre de verdade). ESTA função é a 2ª camada: re-tenta o registo a
// partir do 1º nativeRegisterActivity via GetObjectClass (funciona mesmo
// que o FindClass do JNI_OnLoad falhe por qualquer razão de classloader).
// A tabela é ÚNICA — o scripts/jni_parity.py afere-a contra os natives
// declarados na VvActivity.java (nome + assinatura) em TODO build.
const JNINativeMethod kNativeMethods[] = {
    { const_cast<char*>("nativeRegisterActivity"),
      const_cast<char*>("(Lvv/goni/VvActivity;Ljava/lang/String;)V"),
      reinterpret_cast<void*>(&Java_vv_goni_VvActivity_nativeRegisterActivity) },
    { const_cast<char*>("nativeOnActivityResult"),
      // F5.4 (gate jni_parity apanhou isto): a Java declara
      // (int, int, Uri, int) → (IILandroid/net/Uri;I)V — a tabela antiga
      // tinha UM 'I' a mais (IIIL...), um bug latente que faria o
      // RegisterNatives falhar em runtime (NoSuchMethodError) logo que o
      // JNI_OnLoad começasse a correr com o fix do loadLibrary
      const_cast<char*>("(IILandroid/net/Uri;I)V"),
      reinterpret_cast<void*>(&Java_vv_goni_VvActivity_nativeOnActivityResult) },
    { const_cast<char*>("nativeOpenProject"),
      // F5.4 — Gestor de Projetos: a VvActivity entrega o projeto escolhido
      // no SAF (extras do Intent) → fila ProjectSlot → boot do android_main
      const_cast<char*>("(Ljava/lang/String;Ljava/lang/String;)V"),
      reinterpret_cast<void*>(&Java_vv_goni_VvActivity_nativeOpenProject) },
};
constexpr int kNativeMethodCount =
    static_cast<int>(sizeof(kNativeMethods) / sizeof(kNativeMethods[0]));

bool g_nativesRegistered = false;

// regista (1×) a tabela acima na classe dada. cls pode vir de FindClass
// (JNI_OnLoad) OU de GetObjectClass(activity) (handshake) — o registo é
// POR CLASSE, não por instância. Falha NÃO é fatal: loga e o próximo
// registo re-tenta (o idempotente é o estado g_nativesRegistered).
void ensureNativesRegistered(JNIEnv* env, jclass cls) {
    if (!env || !cls || g_nativesRegistered) {
        return;
    }
    if (env->RegisterNatives(cls, kNativeMethods, kNativeMethodCount) == JNI_OK) {
        g_nativesRegistered = true;
        elog::info("jni: %d nativo(s) registado(s) — nativeRegisterActivity + "
                   "nativeOnActivityResult (resolução garantida)",
                   kNativeMethodCount);
    } else {
        env->ExceptionClear();
        elog::error("jni: RegisterNatives FALHOU (assinatura divergente?) — "
                    "re-tentativa no próximo registo da activity");
    }
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

    // 0.6.7 — sair para o gestor (bridgeFinish). NÃO crítico: sem ele o
    // botão "Sair para projetos" mostra um toast honesto, o resto do
    // editor funciona.
    g_midFinish = env->GetMethodID(g_activityCls, "bridgeFinish", "()V");
    if (!g_midFinish || clearPendingException(env)) {
        g_midFinish = nullptr;
        elog::error("jni: VvActivity.bridgeFinish NÃO encontrada — 'Sair p/ "
                    "projetos' indisponível (o resto do editor intacto)");
    } else {
        elog::info("jni: VvActivity.bridgeFinish OK (sair p/ o gestor)");
    }

    // F5.4 — ponte SAF do Gestor de Projetos (todas na mesma classe: ou
    // existem todas ou o dex divergiu — o veredito exige as críticas)
    struct BridgeMid {
        jmethodID* mid;
        const char* name;
        const char* sig;
        bool critical;
    };
    const BridgeMid kBridgeMids[] = {
        { &g_midRootDoc, "bridgeRootDoc",
          "(Ljava/lang/String;)Ljava/lang/String;", true },
        { &g_midOpenFd, "bridgeOpenFd",
          "(Ljava/lang/String;Ljava/lang/String;)I", true },
        { &g_midList, "bridgeList",
          "(Ljava/lang/String;)[Ljava/lang/String;", true },
        { &g_midCreate, "bridgeCreate",
          "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", true },
        // F5.4-hotfix — CRÍTICA: sem esta verificação o createDocument
        // duplica ("nome (1)"); o anti-duplicação depende dela
        { &g_midFindFile, "bridgeFindFile",
          "(Ljava/lang/String;Ljava/lang/String;)Ljava/lang/String;", true },
        { &g_midDelete, "bridgeDelete",
          "(Ljava/lang/String;)Z", false },
    };
    for (const BridgeMid& b : kBridgeMids) {
        *b.mid = env->GetMethodID(g_activityCls, b.name, b.sig);
        if (!*b.mid || clearPendingException(env)) {
            *b.mid = nullptr;
            elog::error("jni: VvActivity.%s NÃO encontrada%s", b.name,
                        b.critical ? " — CRÍTICA (I/O do projeto SAF)" : "");
        } else {
            elog::info("jni: VvActivity.%s OK", b.name);
        }
    }
}

} // namespace

// F5.4 — hook da SUÍTE (nunca chamado em produção): repõe o guard do
// registo idempotente para um caso de teste re-exercitar o RegisterNatives.
void resetNativesRegistrationForTest() {
    g_nativesRegistered = false;
}

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

    // 3.5) F5.4 — reforço do registo dos nativos via GetObjectClass: se o
    //      JNI_OnLoad não correu (dlopen do framework) ou o FindClass dele
    //      falhou, ESTE é o ponto que garante a resolução dos métodos
    //      nativos (idempotente — ver ensureNativesRegistered)
    ensureNativesRegistered(env, g_activityCls);

    // 4) métodos da activity (higiene de exceções — cacheActivityMethods)
    cacheActivityMethods(env);

    // 5) veredito: os métodos CRÍTICOS têm de existir — o fluxo All Files
    //    (openAllFilesSettings) E a ponte SAF do projeto (rootDoc/openFd/
    //    list/create/findFile); exportLogs e bridgeDelete são diagnóstico
    //    (falhar não bloqueia o fluxo principal)
    const bool first = !g_registrationSeen;
    g_registrationSeen = true;
    g_handshake = g_midOpenAllFiles != nullptr &&
                  g_midRootDoc != nullptr &&
                  g_midOpenFd  != nullptr &&
                  g_midList    != nullptr &&
                  g_midCreate  != nullptr &&
                  g_midFindFile != nullptr;
    if (g_handshake) {
        elog::info(first ? "native: activity registada"
                         : "native: activity re-registada (onResume — reforço)");
        elog::info("jni: handshake OK — vm=%p activity=%p openAllFiles=%d "
                   "saf=%d/5 exportLogs=%d",
                   reinterpret_cast<void*>(vm),
                   reinterpret_cast<void*>(g_activity),
                   g_midOpenAllFiles ? 1 : 0,
                   (g_midRootDoc ? 1 : 0) + (g_midOpenFd ? 1 : 0) +
                   (g_midList ? 1 : 0) + (g_midCreate ? 1 : 0) +
                   (g_midFindFile ? 1 : 0),
                   g_midExportLogs ? 1 : 0);
    } else {
        elog::error("native: registo PARCIAL — método crítico ausente "
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
    JNIEnv* env = attachedEnv();
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
    JNIEnv* env = attachedEnv();
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
    JNIEnv* env = attachedEnv();
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

// 0.6.7 — "Sair para projetos": a activity termina-se (finish() no Java,
// postado na UI thread) e o processo CONTINUA VIVO — o gestor
// (ProjectManagerActivity) retoma da back stack. O APP_CMD_TERM_WINDOW
// que se segue liberta os recursos GL (lifecycle 0.6.7-a); reentrar no
// editor arranca um NOVO android_main (reset de estado) com um NOVO
// contexto EGL (re-upload de tudo).
bool jniFinishToLauncher() {
    if (!handshakeOk()) {
        elog::error("jni: bridgeFinish indisponível — ponte Java "
                   "indisponível (handshake)");
        return false;
    }
    JNIEnv* env = attachedEnv();
    if (!env || !g_midFinish) {
        elog::error("jni: bridgeFinish indisponível — env/mid ausentes");
        return false;
    }
    env->CallVoidMethod(g_activity, g_midFinish);
    if (clearPendingException(env)) {
        elog::error("jni: bridgeFinish lançou excepção");
        return false;
    }
    return true;
}

// ---- F5.4: I/O SAF do projeto (SafIo sobre a ponte JNI) --------------------
//
// O native NÃO fala DocumentsContract: cada operação vira uma chamada a um
// método bridge da VvActivity (thread anexado por attachedEnv; a activity
// é o GlobalRef do handshake). Toda falha devolve false + err com a causa
// REAL — sem handshake é SEMPRE "ponte Java indisponível (handshake)".
namespace {

// pré-condições comuns de cada bridge (mensagem honesta em cada saída)
JNIEnv* safBegin(std::string& err) {
    if (!g_handshake) {
        err = "ponte Java indisponível (handshake)";
        return nullptr;
    }
    JNIEnv* env = attachedEnv();
    if (!env) {
        err = "thread da engine sem env (attach falhou — ver engine.log)";
        return nullptr;
    }
    return env;
}

// consome um jstring devolvido pela Java (null → false)
bool pickString(JNIEnv* env, jobject jstr, std::string& out) {
    if (!jstr) {
        return false;
    }
    const char* c = env->GetStringUTFChars(static_cast<jstring>(jstr), nullptr);
    if (c) {
        out = c;
        env->ReleaseStringUTFChars(static_cast<jstring>(jstr), c);
    }
    env->DeleteLocalRef(jstr);
    return !out.empty();
}

// JniSafIo — SafIo via os métodos bridge da activity. O resolveChild usa a
// verificação DEDICADA bridgeFindFile (query exata por displayName — F5.4-
// hotfix: o anti-duplicação NÃO confia no list da primeira tentativa).
class JniSafIo final : public SafIo {
public:
    bool rootDoc(const std::string& treeUri, std::string& outDocUri,
                 std::string& err) override {
        JNIEnv* env = safBegin(err);
        if (!env || !g_midRootDoc) {
            if (env && !g_midRootDoc) err = "bridgeRootDoc ausente (handshake parcial)";
            return false;
        }
        jstring jt = env->NewStringUTF(treeUri.c_str());
        jobject r = env->CallObjectMethod(g_activity, g_midRootDoc, jt);
        env->DeleteLocalRef(jt);
        if (clearPendingException(env)) {
            err = "bridgeRootDoc lançou excepção (tree uri inválida?)";
            return false;
        }
        if (!pickString(env, r, outDocUri)) {
            err = "bridgeRootDoc devolveu null (uri inválida?)";
            return false;
        }
        return true;
    }

    bool openFd(const std::string& docUri, const char* mode, int* outFd,
                std::string& err) override {
        *outFd = -1;
        JNIEnv* env = safBegin(err);
        if (!env || !g_midOpenFd) {
            if (env && !g_midOpenFd) err = "bridgeOpenFd ausente (handshake parcial)";
            return false;
        }
        jstring ju = env->NewStringUTF(docUri.c_str());
        jstring jm = env->NewStringUTF(mode ? mode : "r");
        const jint fd = env->CallIntMethod(g_activity, g_midOpenFd, ju, jm);
        env->DeleteLocalRef(ju);
        env->DeleteLocalRef(jm);
        if (clearPendingException(env)) {
            err = "bridgeOpenFd lançou excepção (documento ausente? provider recusou?)";
            return false;
        }
        if (fd < 0) {
            err = "bridgeOpenFd devolveu -1 (documento ausente ou provider recusou)";
            return false;
        }
        *outFd = static_cast<int>(fd);
        return true;
    }

    bool list(const std::string& dirDocUri, std::vector<SafEntry>& out,
              std::string& err) override {
        out.clear();
        JNIEnv* env = safBegin(err);
        if (!env || !g_midList) {
            if (env && !g_midList) err = "bridgeList ausente (handshake parcial)";
            return false;
        }
        jstring jd = env->NewStringUTF(dirDocUri.c_str());
        jobject r = env->CallObjectMethod(g_activity, g_midList, jd);
        env->DeleteLocalRef(jd);
        if (clearPendingException(env)) {
            err = "bridgeList lançou excepção (pasta ausente? provider recusou?)";
            return false;
        }
        if (!r) {
            err = "bridgeList devolveu null (pasta ausente? provider recusou?)";
            return false;
        }
        const jsize n = env->GetArrayLength(static_cast<jobjectArray>(r));
        out.reserve(static_cast<size_t>(n / 3));
        for (jsize i = 0; i + 2 < n; i += 3) {
            SafEntry e;
            e.uri = stringAt(env, r, i);
            e.name = stringAt(env, r, i + 1);
            e.mime = stringAt(env, r, i + 2);
            out.push_back(std::move(e));
        }
        env->DeleteLocalRef(r);
        return true;
    }

    bool create(const std::string& parentDocUri, const char* mime,
                const char* displayName, std::string& outDocUri,
                std::string& err) override {
        outDocUri.clear();
        JNIEnv* env = safBegin(err);
        if (!env || !g_midCreate) {
            if (env && !g_midCreate) err = "bridgeCreate ausente (handshake parcial)";
            return false;
        }
        jstring jp = env->NewStringUTF(parentDocUri.c_str());
        jstring jm = env->NewStringUTF(mime ? mime : "application/octet-stream");
        jstring jn = env->NewStringUTF(displayName ? displayName : "sem-nome");
        jobject r = env->CallObjectMethod(g_activity, g_midCreate, jp, jm, jn);
        env->DeleteLocalRef(jp);
        env->DeleteLocalRef(jm);
        env->DeleteLocalRef(jn);
        if (clearPendingException(env)) {
            err = "bridgeCreate lançou excepção (pai ausente? nome inválido?)";
            return false;
        }
        if (!pickString(env, r, outDocUri)) {
            err = "bridgeCreate devolveu null (criação recusada pelo provider)";
            return false;
        }
        return true;
    }

    bool remove(const std::string& docUri, std::string& err) override {
        JNIEnv* env = safBegin(err);
        if (!env || !g_midDelete) {
            if (env && !g_midDelete) err = "bridgeDelete ausente (handshake parcial)";
            return false;
        }
        jstring jd = env->NewStringUTF(docUri.c_str());
        const jboolean ok =
            env->CallBooleanMethod(g_activity, g_midDelete, jd);
        env->DeleteLocalRef(jd);
        if (clearPendingException(env)) {
            err = "bridgeDelete lançou excepção";
            return false;
        }
        if (!ok) {
            err = "bridgeDelete devolveu false (provider recusou)";
            return false;
        }
        return true;
    }

    // F5.4-hotfix — verificação de existência DEDICADA (bridgeFindFile):
    //   URI  → found=true (existe — reabrir este URI, escrever "wt")
    //   ""   → found=false SEM erro (ausência CONFIRMADA — só aqui se cria)
    //   null → erro (query falhou) — o chamador NUNCA decide por "não sei"
    // Uma repetição em erro de query (provider ocasionalmente lento) —
    // nunca confiar num único insucesso para decidir criação.
    bool resolveChild(const std::string& dirDocUri, const char* name,
                      bool& found, std::string& outUri,
                      std::string& err) override {
        found = false;
        outUri.clear();
        if (!name || !name[0]) {
            err = "resolveChild — nome vazio";
            return false;
        }
        for (int attempt = 0; attempt < 2; ++attempt) {
            JNIEnv* env = safBegin(err);
            if (!env || !g_midFindFile) {
                if (env && !g_midFindFile) {
                    err = "bridgeFindFile ausente (handshake parcial)";
                }
                return false;
            }
            jstring jd = env->NewStringUTF(dirDocUri.c_str());
            jstring jn = env->NewStringUTF(name);
            jobject r = env->CallObjectMethod(g_activity, g_midFindFile, jd, jn);
            env->DeleteLocalRef(jd);
            env->DeleteLocalRef(jn);
            if (clearPendingException(env)) {
                err = "bridgeFindFile lançou excepção (pasta ausente? provider recusou?)";
                continue;   // 1 repetição — query fresca
            }
            if (!r) {
                err = "bridgeFindFile devolveu null (query falhou — indecidido)";
                continue;   // 1 repetição — query fresca
            }
            // ler a string SEM o pickString ("" aqui é RESPOSTA, não erro)
            const char* c = env->GetStringUTFChars(static_cast<jstring>(r), nullptr);
            const std::string s = c ? c : "";
            if (c) {
                env->ReleaseStringUTFChars(static_cast<jstring>(r), c);
            }
            env->DeleteLocalRef(r);
            if (s.empty()) {
                err.clear();   // resposta válida — sem erro arrastado da 1ª tentativa
                return true;   // ausência CONFIRMADA (found=false, sem erro)
            }
            found = true;
            outUri = s;
            err.clear();
            return true;
        }
        return false;   // as duas tentativas falharam — err já tem a causa
    }

private:
    // elemento i de um String[] fake/real (higiene de local ref)
    static std::string stringAt(JNIEnv* env, jobject arr, jsize i) {
        jobject el = env->GetObjectArrayElement(
            static_cast<jobjectArray>(arr), i);
        if (!el) {
            return "";
        }
        std::string s;
        const char* c = env->GetStringUTFChars(static_cast<jstring>(el), nullptr);
        if (c) {
            s = c;
            env->ReleaseStringUTFChars(static_cast<jstring>(el), c);
        }
        env->DeleteLocalRef(el);
        return s;
    }
};

} // namespace

ProjectSlot& projectSlot() {
    return g_projectSlot;
}

SafIo* jniSafIo() {
    static JniSafIo io;   // sem estado próprio — tudo no bridge/handshake
    return &io;
}

} // namespace vv::storage

// ---- JNI_OnLoad: registo EXPLÍCITO dos métodos nativos ----------------------
//
// F5.4 (causa raiz RMX3624): este JNI_OnLoad SÓ corre se a lib for
// carregada via System.loadLibrary — o que o static init da VvActivity
// agora faz (o dlopen do framework não chama esta função). Com o
// loadLibrary a partir da classe da app, o classloader usado pelo
// FindClass aqui é o da APP — vê as classes da aplicação.
//
// TOLERANTE (novo): FindClass/RegisterNatives a falhar NÃO devolve
// JNI_ERR — isso mataria o System.loadLibrary (excepção no static init
// da activity = app morta no arranque). A falha fica logada e o registo
// é re-tentado no 1º nativeRegisterActivity via GetObjectClass (2ª
// camada — ensureNativesRegistered).
extern "C" JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void*) {
    // log em ficheiro desde ANTES do android_main (fallback do device)
    vv::elog::init(vv::elog::androidFallbackDir());
    vv::elog::info("jni: JNI_OnLoad — G.One VV 0.6.9 (registo explícito de "
                   "nativos; loadLibrary no Java — F5.4)");

    JNIEnv* env = nullptr;
    if (!vm || vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) != JNI_OK) {
        vv::elog::error("jni: JNI_OnLoad — GetEnv falhou (VM incompatível?)");
        return JNI_ERR;   // sem VM compatível a falha é honesta (load morre)
    }

    // FindClass no thread que chamou System.loadLibrary (UI, static init
    // da VvActivity) — classloader da app, vê as classes da aplicação.
    jclass cls = env->FindClass("vv/goni/VvActivity");
    if (!cls) {
        env->ExceptionClear();
        vv::elog::warn("jni: JNI_OnLoad — FindClass(vv/goni/VvActivity) "
                       "FALHOU (classloader inesperado) — registo adiado p/ "
                       "o 1º nativeRegisterActivity (GetObjectClass)");
        return JNI_VERSION_1_6;   // NÃO JNI_ERR — não matar o arranque
    }
    vv::storage::ensureNativesRegistered(env, cls);
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

// ---- F5.4: projeto escolhido no Gestor de Projetos --------------------------
//
// Corre NO THREAD DA UI (VvActivity.onCreate, após o handshake): apenas
// converte e enfileira no ProjectSlot — o android_main espera (com
// timeout) e monta o SafStorage no thread da engine. Sobrepõe (1 slot).
extern "C" JNIEXPORT void JNICALL
Java_vv_goni_VvActivity_nativeOpenProject(JNIEnv* env, jclass,
                                          jstring treeUri, jstring name) {
    if (!env || !treeUri) {
        vv::elog::error("jni: nativeOpenProject — uri ausente");
        return;
    }
    const char* u = env->GetStringUTFChars(treeUri, nullptr);
    const char* n = name ? env->GetStringUTFChars(name, nullptr) : nullptr;
    vv::elog::info("java: openProject → fila (nome=%s) — boot monta o "
                   "storage sobre a pasta SAF", (n && n[0]) ? n : "projeto");
    vv::storage::ProjectRequest r;
    r.treeUri = u ? u : "";
    r.name = (n && n[0]) ? n : "projeto";
    if (u) {
        env->ReleaseStringUTFChars(treeUri, u);
    }
    if (n) {
        env->ReleaseStringUTFChars(name, n);
    }
    if (env->ExceptionCheck()) {
        env->ExceptionClear();
    }
    if (r.treeUri.empty()) {
        vv::elog::error("jni: nativeOpenProject — treeUri VAZIA (extras do "
                        "intent perdidos?) — projeto não enfileirado");
        return;
    }
    vv::storage::g_projectSlot.push(r);
}
