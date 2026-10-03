// tests/test_handshake.cpp — F5.3: handshake Java↔native TESTADO NO
// HOSPedeiro. O StorageBridge.cpp (device-only até aqui) agora compila
// DENTRO da suíte com o fake JNI controlável (tests/stub/jni.h) — o "stub
// Java regista activity" da TAREFA 5 é literal: o teste chama
// Java_vv_goni_VvActivity_nativeRegisterActivity diretamente, exatamente o
// que a VvActivity.onCreate/onResume fazem no device.
//
//   handshake simulado → registo, re-registo (onResume), registo parcial
//   attach de thread   → GetEnv EDETACHED → AttachCurrentThread('goni-engine')
//                        + falha com código logado
//   fluxo de permissão → diálogo → intent(4301) → onActivityResult →
//                        fila → pollResult → Granted + ação retomada
//   mensagens honestas → blockReason/blockMessage: "ponte Java indisponível
//                        (handshake)" NUNCA "sistema sem All Files Access"
//   JNI_OnLoad         → 2 nativos registados explicitamente
//   export de logs     → kDownloadsRelPath chega à Java + resultado
#include "TestFramework.h"
#include <dirent.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

// sys/memfd.h não existe em todos os sistemas (a glibc fornece o SÍMBOLO
// desde a 2.27) — protótipo explícito, assinatura estável da glibc
extern "C" int memfd_create(const char* name, unsigned int flags);
#ifndef MFD_CLOEXEC
#define MFD_CLOEXEC 0x0001U
#endif

#include <jni.h>   // FAKE (tests/stub é o primeiro include dir)

#include "platform/StorageBridge.h"
#include "platform/StoragePerm.h"
#include "platform/JniAttach.h"
#include "platform/EngineLog.h"
#include "platform/Saf.h"

using namespace vv;

// métodos nativos definidos em platform/StorageBridge.cpp (compilado na
// suíte) — o papel do "stub Java" é chamá-los diretamente
extern "C" void Java_vv_goni_VvActivity_nativeRegisterActivity(
        JNIEnv*, jclass, jobject activity, jstring origin);
extern "C" void Java_vv_goni_VvActivity_nativeOnActivityResult(
        JNIEnv*, jclass, jint request, jint result, jobject uri, jint flags);
extern "C" void Java_vv_goni_VvActivity_nativeOpenProject(
        JNIEnv*, jclass, jstring treeUri, jstring name);
extern "C" jint JNI_OnLoad(JavaVM* vm, void*);

namespace {

const char* kTestLogs = "test-handshake-logs";

void rmrf(const std::string& dir) {
    DIR* d = ::opendir(dir.c_str());
    if (d) {
        while (dirent* e = ::readdir(d)) {
            const std::string n = e->d_name;
            if (n != "." && n != "..") {
                ::remove((dir + "/" + n).c_str());
            }
        }
        ::closedir(d);
    }
    ::remove(dir.c_str());
}

// linhas do engine.log ativo (a sequência que o log viewer mostra no C33)
std::vector<std::string> logLines(int maxLines = 500) {
    std::vector<std::string> lines;
    vv::elog::readTail(lines, maxLines);
    return lines;
}

int countContaining(const std::vector<std::string>& lines, const char* needle) {
    int n = 0;
    for (const std::string& l : lines) {
        if (l.find(needle) != std::string::npos) ++n;
    }
    return n;
}

int firstIndexOf(const std::vector<std::string>& lines, const char* needle) {
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].find(needle) != std::string::npos) return static_cast<int>(i);
    }
    return -1;
}

// identidade fake da activity (ponteiro opaco — o fake NewGlobalRef devolve-o)
jobject kFakeActivity = reinterpret_cast<jobject>(static_cast<intptr_t>(0xA001));
jclass  kFakeCls      = reinterpret_cast<jclass>(static_cast<intptr_t>(0xA002));

// drena a fila de resultados (estado entre casos)
void drainQueue() {
    while (vv::storage::pollResult()) {
    }
}

// registo simulado (o papel do VvActivity.onCreate/onResume)
void javaRegisters(const char* origin) {
    Java_vv_goni_VvActivity_nativeRegisterActivity(
        g_jni.env, kFakeCls, kFakeActivity, g_jni.newString(origin));
}

} // namespace

// ---------------------------------------------------------------------------
// TAREFA 3 (parte pura) — tabela de decisão do attach
// ---------------------------------------------------------------------------

TEST(jni_plan_attach_table) {
    using vv::jni::AttachAction;
    using vv::jni::planAttach;
    EXPECT(planAttach(vv::jni::kJniOk) == AttachAction::Use);
    EXPECT(planAttach(vv::jni::kJniEdetached) == AttachAction::Attach);
    EXPECT(planAttach(vv::jni::kJniEexist) == AttachAction::Fail);
    EXPECT(planAttach(vv::jni::kJniEinval) == AttachAction::Fail);
    EXPECT(planAttach(1) == AttachAction::Fail);       // desconhecido → falha
    EXPECT(planAttach(-99) == AttachAction::Fail);
    // rótulos não vazios (aparecem nos logs do device)
    EXPECT(vv::jni::attachActionLabel(AttachAction::Use)[0] != '\0');
    EXPECT(vv::jni::attachActionLabel(AttachAction::Attach)[0] != '\0');
    EXPECT(vv::jni::attachActionLabel(AttachAction::Fail)[0] != '\0');
    // nome canónico da thread (afervel no attach real do device)
    EXPECT(std::strcmp(vv::jni::kEngineThreadName, "goni-engine") == 0);
}

// ---------------------------------------------------------------------------
// TAREFA 2 — handshake simulado: registo (onCreate)
// ---------------------------------------------------------------------------

TEST(handshake_register_oncreate_sequence) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    drainQueue();
    g_jni.reset();

    EXPECT(!vv::storage::handshakeOk());   // antes do registo: ponte em baixo

    javaRegisters("onCreate");

    EXPECT(vv::storage::handshakeOk());    // pós-registo: ponte OK

    // sequência EXATA que o log viewer do C33 tem de mostrar
    const auto lines = logLines();
    const int iJava = firstIndexOf(lines, "java: onCreate → nativeRegisterActivity");
    const int iNative = firstIndexOf(lines, "native: activity registada");
    EXPECT(iJava >= 0);
    EXPECT(iNative >= 0);
    EXPECT(iJava < iNative);               // ordem: java → native
    EXPECT(countContaining(lines, "handshake OK") == 1);
}

// ---------------------------------------------------------------------------
// TAREFA 2 — re-registo (onResume reforço, idempotente)
// ---------------------------------------------------------------------------

TEST(handshake_reregister_onresume_idempotent) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    drainQueue();
    g_jni.reset();
    javaRegisters("onCreate");
    // deltas (o binário pode já ter registos de casos anteriores — o estado
    // do bridge é global; o que se aferi é o EFEITO do reforço)
    const int newBefore = g_jni.refs_new;
    const int delBefore = g_jni.refs_del;
    const int reRegBefore = countContaining(logLines(), "re-registada");

    javaRegisters("onResume");             // reforço

    EXPECT(vv::storage::handshakeOk());    // continua OK (não degrada)
    const auto lines = logLines();
    EXPECT(countContaining(lines,
        "native: activity re-registada (onResume — reforço)") ==
        reRegBefore + 1);
    // GlobalRef substituído (delete + new da activity E da class), sem leak
    EXPECT(g_jni.refs_del == delBefore + 2);
    EXPECT(g_jni.refs_new == newBefore + 2);
}

// ---------------------------------------------------------------------------
// TAREFA 3 — attach de thread da engine (EDETACHED → AttachCurrentThread)
// ---------------------------------------------------------------------------

TEST(handshake_attach_engine_thread) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    drainQueue();
    g_jni.reset();
    javaRegisters("onCreate");
    g_jni.vm_attached = false;             // thread da engine desanexado
    g_jni.manager_result = true;           // permissão concedida no fake

    bool mgr = false;
    const bool supported = vv::storage::jniStorageApiSupported(&mgr);

    EXPECT(supported);                     // attach + consulta OK
    EXPECT(mgr);                           // valor propagado
    EXPECT(g_jni.vm_attached);             // thread ficou anexado
    EXPECT(g_jni.attach_calls == 1);
    // attach NOMEADO com o nome canónico da engine
    EXPECT(g_jni.attached_thread_name == vv::jni::kEngineThreadName);
    // consulta à classe de SISTEMA (Environment) gravada
    EXPECT(countContaining(g_jni.find_class_calls, "android/os/Environment") == 1);
    EXPECT(countContaining(g_jni.static_bool_calls, "isExternalStorageManager") == 1);
}

// ---------------------------------------------------------------------------
// TAREFA 3 — falha do attach loga o CÓDIGO (nenhum env assumido)
// ---------------------------------------------------------------------------

TEST(handshake_attach_failure_logs_rc) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    drainQueue();
    g_jni.reset();
    javaRegisters("onCreate");
    g_jni.vm_attached = false;
    g_jni.attach_rc = -8;                  // falha arbitrária com código

    bool mgr = false;
    const bool supported = vv::storage::jniStorageApiSupported(&mgr);

    EXPECT(!supported);                    // sem env → sem suporte apurável
    EXPECT(!mgr);
    EXPECT(g_jni.attach_calls == 1);       // tentou uma vez
    const auto lines = logLines();
    EXPECT(countContaining(lines, "AttachCurrentThread") >= 1);
    EXPECT(countContaining(lines, "rc=-8") == 1);   // CÓDIGO no log
}

// ---------------------------------------------------------------------------
// export de logs — o relPath que a Java recebe é a constante única
// ---------------------------------------------------------------------------

TEST(handshake_export_logs_relpath_and_result) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    drainQueue();
    g_jni.reset();
    javaRegisters("onCreate");

    int copied = 0;
    const bool called = vv::storage::jniExportLogsToDownloads(&copied);

    EXPECT(called);
    EXPECT(copied == 3);                       // resultado do fake propagado
    EXPECT(g_jni.last_new_string ==
           std::string(vv::elog::kDownloadsRelPath));   // constante única
    EXPECT(std::string(vv::elog::kDownloadsRelPath) ==
           "Download/GOneVV/logs");
}

// ---------------------------------------------------------------------------
// TAREFA 4 — fluxo de permissão COMPLETO pós-handshake
// (diálogo → intent → settings → onActivityResult → fila → Granted)
// ---------------------------------------------------------------------------

TEST(handshake_full_permission_flow) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    drainQueue();
    g_jni.reset();
    g_jni.manager_result = false;          // ainda sem permissão
    javaRegisters("onCreate");             // 1) handshake

    vv::storage::PermFlow flow;

    // 2) utilizador tenta importar — diálogo deve mostrar
    EXPECT(flow.requestAction(vv::storage::Action::Import, true));
    EXPECT(flow.dialogOpen());

    // 3) "Permitir" → consume do pedido → ponte lança o intent
    flow.dialogAccept();
    EXPECT(flow.consumeOpenSettings());
    EXPECT(!flow.consumeOpenSettings());   // 1×
    EXPECT(vv::storage::jniOpenAllFilesSettings());
    // a Java recebeu openAllFilesSettings(kReqAllFiles=4301) — exatamente
    // o request code que volta no onActivityResult
    EXPECT(g_jni.void_calls.size() == 1);
    EXPECT(g_jni.void_calls[0].first == "openAllFilesSettings");
    EXPECT(g_jni.void_calls[0].second ==
           static_cast<long>(vv::storage::kReqAllFiles));
    EXPECT(flow.state() == vv::storage::FlowState::PendingSettings);

    // 4) utilizador ativa o interruptor e volta — onActivityResult
    g_jni.manager_result = true;
    Java_vv_goni_VvActivity_nativeOnActivityResult(
        g_jni.env, kFakeCls, vv::storage::kReqAllFiles,
        0 /* RESULT_OK */, nullptr, 0);

    // 5) o loop da engine consome a fila no thread certo
    bool grantedSeen = false;
    vv::saf::SafResult seen{};
    vv::storage::setHandler([](void* user, const vv::saf::SafResult& r) {
        *static_cast<vv::saf::SafResult*>(user) = r;
    }, &seen);
    EXPECT(vv::storage::pollResult());
    EXPECT(seen.request == vv::storage::kReqAllFiles);
    EXPECT(seen.ok);

    // 6) re-verificação (isExternalStorageManager agora true) → Granted
    bool mgr = false;
    const bool supported = vv::storage::jniStorageApiSupported(&mgr);
    EXPECT(supported && mgr);
    flow.onSettingsReturn(supported && mgr);
    EXPECT(flow.state() == vv::storage::FlowState::Granted);
    EXPECT(flow.mode() == vv::storage::Mode::AllFiles);
    EXPECT(flow.takePendingAction() == vv::storage::Action::Import);
    grantedSeen = true;
    EXPECT(grantedSeen);
}

// ---------------------------------------------------------------------------
// TAREFA 4 (regressão) — a mensagem reporta a CAUSA VERDADEIRA
// ---------------------------------------------------------------------------

TEST(handshake_honest_messages_regression) {
    using vv::storage::BlockReason;

    // CASO C33 (0.6.2): handshake em baixo + consulta indisponível —
    // a causa é a PONTE, nunca o "sistema sem All Files Access"
    EXPECT(vv::storage::blockReason(false, false) == BlockReason::BridgeDown);
    EXPECT(std::string(vv::storage::blockMessage(BlockReason::BridgeDown)) ==
           "ponte Java indisponível (handshake)");
    // a mensagem honesta NÃO contém a mentira antiga
    EXPECT(std::string(vv::storage::blockMessage(BlockReason::BridgeDown))
               .find("sistema sem") == std::string::npos);

    // handshake OK + sistema sem API (Android < 11) — aí SIM é o sistema
    EXPECT(vv::storage::blockReason(true, false) == BlockReason::Unsupported);
    EXPECT(std::string(vv::storage::blockMessage(BlockReason::Unsupported)) ==
           "sistema sem All Files Access — modo app-private");

    // tudo OK → sem bloqueio
    EXPECT(vv::storage::blockReason(true, true) == BlockReason::None);
    EXPECT(std::string(vv::storage::blockMessage(BlockReason::None)).empty());
}

// ---------------------------------------------------------------------------
// registo PARCIAL (método crítico ausente) → handshakeOk=false
// ---------------------------------------------------------------------------

TEST(handshake_partial_registration_fails) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    drainQueue();
    g_jni.reset();
    g_jni.fail_methods["openAllFilesSettings"] = true;

    javaRegisters("onCreate");

    EXPECT(!vv::storage::handshakeOk());   // ponte Java indisponível
    const auto lines = logLines();
    EXPECT(countContaining(lines, "registo PARCIAL") == 1);
    EXPECT(countContaining(lines,
        "VvActivity.openAllFilesSettings(I)V NÃO encontrada") == 1);

    // e o bloqueio de import/export diria a causa certa:
    EXPECT(vv::storage::blockReason(vv::storage::handshakeOk(), false) ==
           vv::storage::BlockReason::BridgeDown);

    // recuperação: registo seguinte (sem falha) repõe a ponte
    g_jni.reset();
    javaRegisters("onResume");
    EXPECT(vv::storage::handshakeOk());
}

// ---------------------------------------------------------------------------
// JNI_OnLoad — registo EXPLÍCITO dos 3 nativos (fallback: VM anexada)
// ---------------------------------------------------------------------------

TEST(jni_onload_registers_two_natives) {
    rmrf(kTestLogs);
    g_jni.reset();
    vv::storage::resetNativesRegistrationForTest();   // F5.4: re-exercitar registo
    g_jni.vm_attached = true;              // loadLibrary corre no thread da UI

    const jint rc = JNI_OnLoad(fakeVm(), nullptr);

    EXPECT(rc == JNI_VERSION_1_6);
    EXPECT(g_jni.register_natives_calls == 1);
    EXPECT(g_jni.register_natives_names.size() == 6);   // 0.9.1: +buildInfo +IME (text/key)
    EXPECT(g_jni.register_natives_sigs.size() == 6);
    bool hasRegister = false, hasResult = false, hasOpenProject = false;
    for (const std::string& n : g_jni.register_natives_names) {
        if (n == "nativeRegisterActivity") hasRegister = true;
        if (n == "nativeOnActivityResult") hasResult = true;
        if (n == "nativeOpenProject") hasOpenProject = true;
    }
    EXPECT(hasRegister);   // handshake invertido registado
    EXPECT(hasResult);     // retorno das definições registado
    EXPECT(hasOpenProject);   // F5.4: projeto do gestor registado

    // F5.4 — assinaturas EXATAS: uma divergência Java↔tabela é outra forma
    // de UnsatisfiedLinkError (RegisterNatives casa a string inteira)
    for (size_t i = 0; i < g_jni.register_natives_names.size(); ++i) {
        if (g_jni.register_natives_names[i] == "nativeRegisterActivity") {
            EXPECT(g_jni.register_natives_sigs[i] ==
                   "(Lvv/goni/VvActivity;Ljava/lang/String;)V");
        }
        if (g_jni.register_natives_names[i] == "nativeOnActivityResult") {
            EXPECT(g_jni.register_natives_sigs[i] ==
                   "(IILandroid/net/Uri;I)V");   // F5.4: gate apanhou o 'I' a mais da tabela antiga
        }
        if (g_jni.register_natives_names[i] == "nativeOpenProject") {
            EXPECT(g_jni.register_natives_sigs[i] ==
                   "(Ljava/lang/String;Ljava/lang/String;)V");
        }
    }

    // FindClass da VvActivity a partir do JNI_OnLoad (thread da UI)
    EXPECT(countContaining(g_jni.find_class_calls, "vv/goni/VvActivity") == 1);

    // JNI_OnLoad re-inicializou o elog com o fallback do device ("" no
    // hospedeiro) — repor um diretório de teste para os casos seguintes
    vv::elog::init(kTestLogs);
}

// ---------------------------------------------------------------------------
// F5.4 — CAUSA RAIZ RMX3624: JNI_OnLoad TOLERANTE + recuperação no handshake.
// O NativeActivity carrega a lib com dlopen(RTLD_LOCAL) — o JNI_OnLoad não
// corria e o RegisterNatives nunca acontecia no device. Com o
// System.loadLibrary no static init da VvActivity o JNI_OnLoad volta a
// correr; e SE o FindClass falhar (classloader inesperado), o JNI_OnLoad
// NÃO devolve JNI_ERR (mataria o load e o arranque) — o registo é
// recuperado no 1º nativeRegisterActivity via GetObjectClass.
// ---------------------------------------------------------------------------

TEST(jni_onload_findclass_failure_soft_recovery) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    g_jni.reset();
    vv::storage::resetNativesRegistrationForTest();
    g_jni.vm_attached = true;
    g_jni.vvactivity_class_ok = false;     // FindClass não encontra a classe

    const jint rc = JNI_OnLoad(fakeVm(), nullptr);

    // TOLERANTE: devolve a versão (NUNCA JNI_ERR — isso mataria o
    // System.loadLibrary e com ele o arranque da app)
    EXPECT(rc == JNI_VERSION_1_6);
    EXPECT(g_jni.register_natives_calls == 0);   // não registou (sem classe)

    // RECUPERAÇÃO: o 1º registo da activity re-tenta via GetObjectClass —
    // o mesmo caminho que o device agora percorre com o static loadLibrary
    // (o JNI_OnLoad re-iniciou o elog com o fallback do device — repor o
    // log em ficheiro antes do registo)
    EXPECT(vv::elog::init(kTestLogs));
    javaRegisters("onCreate");

    EXPECT(vv::storage::handshakeOk());
    EXPECT(g_jni.register_natives_calls == 1);   // registo recuperado AQUI
    EXPECT(g_jni.register_natives_names.size() == 6);   // 0.9.1: +buildInfo +IME (text/key)
    bool hasRegister = false, hasResult = false, hasOpenProject = false;
    bool hasBuildInfo = false;
    for (const std::string& n : g_jni.register_natives_names) {
        if (n == "nativeRegisterActivity") hasRegister = true;
        if (n == "nativeOnActivityResult") hasResult = true;
        if (n == "nativeOpenProject") hasOpenProject = true;
        if (n == "nativeSetBuildInfo") hasBuildInfo = true;
    }
    EXPECT(hasRegister);
    EXPECT(hasResult);
    EXPECT(hasBuildInfo);   // 0.8.10: a identidade da build está na tabela
    EXPECT(hasOpenProject);
    // a classe veio do GetObjectClass (a FindClass falhada ficou gravada 1×)
    EXPECT(countContaining(g_jni.find_class_calls, "vv/goni/VvActivity") == 1);
    const auto lines = logLines();
    EXPECT(countContaining(lines, "nativo(s) registado(s)") == 1);
    vv::elog::init(kTestLogs);
}

// ---------------------------------------------------------------------------
// F5.4 — nativeOpenProject: o projeto escolhido no gestor chega à fila que
// o android_main consome (o "stub Java" é o VvProjects.launchEditor →
// VvActivity.onCreate → nativeOpenProject do device)
// ---------------------------------------------------------------------------

TEST(saf_native_open_project_enqueues) {
    rmrf(kTestLogs);
    EXPECT(vv::elog::init(kTestLogs));
    drainQueue();
    g_jni.reset();

    Java_vv_goni_VvActivity_nativeOpenProject(
        g_jni.env, kFakeCls, g_jni.newString("content://tree/pasta-x"),
        g_jni.newString("meu jogo"));

    vv::storage::ProjectRequest got;
    EXPECT(vv::storage::projectSlot().tryPoll(&got));
    EXPECT(got.treeUri == "content://tree/pasta-x");
    EXPECT(got.name == "meu jogo");
    EXPECT(countContaining(logLines(), "java: openProject → fila") == 1);

    // uri vazia = extras perdidos — NÃO enfileira (log diz a causa)
    Java_vv_goni_VvActivity_nativeOpenProject(
        g_jni.env, kFakeCls, g_jni.newString(""), g_jni.newString("x"));
    EXPECT(!vv::storage::projectSlot().tryPoll(&got));
    EXPECT(countContaining(logLines(), "treeUri VAZIA") == 1);
    vv::elog::init(kTestLogs);
}
