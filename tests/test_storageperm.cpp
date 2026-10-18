// tests/test_storageperm.cpp — F5.2: fluxo All Files Access + File API direta
// no hospedeiro. O CI Linux não tem Android — aferi-se a MÁQUINA DE ESTADO
// (StoragePerm.h), a File API POSIX (FileApi.cpp sobre /tmp) e as CONSTANTES
// que a ponte JNI/Java consome (action do intent, request code, pastas).
//
//   diálogo      → requestAction devolve true na 1ª tentativa
//   intent       → dialogAccept + consumeOpenSettings (ponto exato do JNI)
//   permissão    → onSettingsReturn(true) → Granted + ação retomada
//   fallback     → recusa/sem suporte → AppPrivate (sem diálogo em loop)
//   File API     → listCandidates/readAll/writeAll/errno em /tmp
//   fila de reto → PendingResult com kReqAllFiles (retorno das settings)
//   F5.5 resume  → resumeRecheck: conceder→resume→import prossegue;
//                  recusar→toast claro SEM loop; drain-primeiro e resultado
//                  tardio NUNCA duplicam a ação retomada (1×)
#include "TestFramework.h"
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "platform/StoragePerm.h"
#include "platform/FileApi.h"
#include "platform/Saf.h"
#include "platform/EngineLog.h"

using namespace vv;
using storage::Action;
using storage::FlowState;
using storage::Mode;
using storage::PermFlow;

namespace {

const char* kTmp = "test-fileapi-tmp";
const char* kTestLogsDir = "test-perm-logs";

void rmrf(const std::string& dir) {
    DIR* d = ::opendir(dir.c_str());
    if (d) {
        while (dirent* e = ::readdir(d)) {
            const std::string n = e->d_name;
            if (n == "." || n == "..") continue;
            const std::string full = dir + "/" + n;
            struct stat st;
            if (::stat(full.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                rmrf(full);
            } else {
                ::remove(full.c_str());
            }
        }
        ::closedir(d);
    }
    ::remove(dir.c_str());
}

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    return std::string((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
}

} // namespace

// ---------------------------------------------------------------------------
// CONSTANTES que o Java/JNI consome (afervel no CI — nada de strings
// divergentes entre nativo e Java)
// ---------------------------------------------------------------------------

TEST(storage_constantes_do_fluxo) {
    // request code espelhado em VvActivity.java
    EXPECT(storage::kReqAllFiles == 4301);
    // action EXATA do intent (Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION)
    EXPECT(std::string(storage::kSettingsAction) ==
           "android.settings.MANAGE_APP_ALL_FILES_ACCESS_PERMISSION");
    // pastas de import + pasta de export (Downloads público)
    EXPECT(fileapi::kImportDirCount == 2);
    EXPECT(std::string(fileapi::kImportDirs[0]) == "Download");
    EXPECT(std::string(fileapi::kImportDirs[1]) == "Documents");
    EXPECT(std::string(fileapi::kExportRelDir) == "Download/GOneVV/export");
    EXPECT(std::string(fileapi::kExternalRoot) == "/storage/emulated/0");
}

// ---------------------------------------------------------------------------
// resolveMode — a regra do modo ativo (Settings mostra)
// ---------------------------------------------------------------------------

TEST(storage_resolve_mode) {
    // suportado + concedido → All Files (File API direta)
    EXPECT(storage::resolveMode(true, true) == Mode::AllFiles);
    // suportado + recusado → app-private
    EXPECT(storage::resolveMode(true, false) == Mode::AppPrivate);
    // SEM suporte (API < 30) → app-private mesmo com flag estranha
    EXPECT(storage::resolveMode(false, true) == Mode::AppPrivate);
    EXPECT(storage::resolveMode(false, false) == Mode::AppPrivate);
    // rótulos p/ o Settings
    EXPECT(std::string(storage::modeLabel(Mode::AllFiles)) == "all files");
    EXPECT(std::string(storage::modeLabel(Mode::AppPrivate)) == "app-private");
    EXPECT(std::string(storage::modeLabel(Mode::Unknown)) == "?");
}

// ---------------------------------------------------------------------------
// FLUXO: diálogo na 1ª tentativa → Permitir → settings → concedido →
// ação retomada (o caminho feliz do C33)
// ---------------------------------------------------------------------------

TEST(storage_fluxo_dialogo_permitir_concede_retoma) {
    PermFlow f;
    EXPECT(f.state() == FlowState::Idle);
    EXPECT(f.mode() == Mode::Unknown);

    // 1ª tentativa de import → DIÁLOGO mostra (true)
    EXPECT(f.requestAction(Action::Import, /*systemSupported=*/true));
    EXPECT(f.dialogOpen());
    EXPECT(f.pendingAction() == Action::Import);

    // "Permitir" → estado vai p/ settings e o pedido de INTENT fica 1× pronto
    f.dialogAccept();
    EXPECT(f.state() == FlowState::PendingSettings);
    EXPECT(f.dialogOpen() == false);
    EXPECT(f.consumeOpenSettings());   // o device lança o intent AQUI
    EXPECT(!f.consumeOpenSettings());  // consumido (1×, nunca duplica)

    // voltou das settings CONCEDIDO → Granted + modo All Files
    f.onSettingsReturn(true);
    EXPECT(f.state() == FlowState::Granted);
    EXPECT(f.mode() == Mode::AllFiles);

    // a ação de import é RETOMADA pós-concessão
    EXPECT(f.takePendingAction() == Action::Import);
    EXPECT(f.takePendingAction() == Action::None);   // consumida

    // tentativas seguintes NEM pedem diálogo (o main executa direto)
    EXPECT(!f.requestAction(Action::Export, true));
}

// ---------------------------------------------------------------------------
// FLUXO: "Cancelar" → ação abortada, modo fica app-private, próxima
// tentativa pergunta de novo (sem nag em loop)
// ---------------------------------------------------------------------------

TEST(storage_fluxo_cancelar_aborta_sem_loop) {
    PermFlow f;
    EXPECT(f.requestAction(Action::Export, true));
    f.dialogCancel();
    EXPECT(f.state() == FlowState::Idle);
    EXPECT(f.mode() == Mode::AppPrivate);
    EXPECT(f.takePendingAction() == Action::None);   // ação abortada

    // a tentativa SEGUINTA (decisão do utilizador) pergunta de novo
    EXPECT(f.requestAction(Action::Export, true));
    EXPECT(f.dialogOpen());
    // e o pedido de intent NUNCA ficou pendente do cancelamento
    f.dialogCancel();
    EXPECT(!f.consumeOpenSettings());
}

// ---------------------------------------------------------------------------
// FLUXO: utilizador NÃO ativou nas settings → volta a Idle (pode tentar de
// novo), modo app-private
// ---------------------------------------------------------------------------

TEST(storage_fluxo_settings_sem_concessao) {
    PermFlow f;
    EXPECT(f.requestAction(Action::Import, true));
    f.dialogAccept();
    EXPECT(f.consumeOpenSettings());
    f.onSettingsReturn(false);
    EXPECT(f.state() == FlowState::Idle);
    EXPECT(f.mode() == Mode::AppPrivate);
    EXPECT(f.takePendingAction() == Action::None);

    // e pode tentar de novo (diálogo volta)
    EXPECT(f.requestAction(Action::Import, true));
}

// ---------------------------------------------------------------------------
// F5.5 — RE-VERIFICAÇÃO NO RESUME (resumeRecheck): o caminho em que o
// onActivityResult NUNCA chega (ecrã de settings OEM sem setResult / volta
// pelos recents) — o APP_CMD_RESUME do main verifica isExternalStorageManager
// FRESCO e decide ali. Contrato do main: drenar a fila ANTES de chamar.
// ---------------------------------------------------------------------------

// CONCEDER → RESUME → IMPORT PROSSEGUE (o caminho feliz da F5.5): o
// utilizador ativa o toggle, volta SEM resultado nenhum, e o resume decide
TEST(storage_resume_concede_import_prossegue) {
    PermFlow f;
    EXPECT(f.requestAction(Action::Import, true));
    f.dialogAccept();
    EXPECT(f.consumeOpenSettings());   // intent lançado; resultado NUNCA veio

    // RESUME: verificação fresca diz CONCEDIDO → transição feita AQUI
    EXPECT(storage::resumeRecheck(f, /*systemSupported=*/true,
                                  /*isManager=*/true));
    EXPECT(f.state() == FlowState::Granted);
    EXPECT(f.mode() == Mode::AllFiles);

    // o IMPORT prossegue SEM re-pedir: a ação pendente sai 1×
    EXPECT(f.takePendingAction() == Action::Import);
    EXPECT(f.takePendingAction() == Action::None);

    // tentativa seguinte NEM pergunta (o main executa direto)
    EXPECT(!f.requestAction(Action::Export, true));
}

// RECUSAR → RESUME → TOAST CLARO SEM LOOP: voltou sem ativar o toggle —
// Idle + app-private + ação abortada; NADA se auto-relança (só uma NOVA
// tentativa do utilizador volta a abrir o diálogo)
TEST(storage_resume_recusa_toast_claro_sem_loop) {
    PermFlow f;
    EXPECT(f.requestAction(Action::Import, true));
    f.dialogAccept();
    EXPECT(f.consumeOpenSettings());

    // RESUME com o toggle desligado → recusa decidida AQUI (toast claro do
    // main: "acesso não ativado — modo app-private")
    EXPECT(storage::resumeRecheck(f, true, false));
    EXPECT(f.state() == FlowState::Idle);
    EXPECT(f.mode() == Mode::AppPrivate);
    EXPECT(f.takePendingAction() == Action::None);   // SEM retoma

    // SEM LOOP: o resume NÃO reabre definições nem diálogo — a próxima
    // tentativa é SEMPRE do utilizador (e pergunta de novo, sem nag)
    EXPECT(f.requestAction(Action::Import, true));
    EXPECT(f.dialogOpen());
    // e o pedido de intent NUNCA ficou pendente da recusa do resume
    EXPECT(!f.consumeOpenSettings());
}

// GUARD do resume: sem PendingSettings o recheck NÃO mexe em NADA (o
// resume de arranque frio e as voltas do fundo ficam de fora do fluxo)
TEST(storage_resume_sem_pendente_nao_mexe) {
    PermFlow f;
    // arranque frio: Idle (antes mesmo do boot verificar) — nada corre
    EXPECT(!storage::resumeRecheck(f, true, true));
    EXPECT(f.state() == FlowState::Idle);
    EXPECT(f.mode() == Mode::Unknown);   // nem o modo foi tocado

    // já concedido (boot/boot anterior): Granted mantém-se
    f.setMode(Mode::AllFiles);
    EXPECT(!storage::resumeRecheck(f, true, true));
    EXPECT(f.state() == FlowState::Granted);

    // diálogo ABERTO (ainda não foi às definições): o resume não decide
    PermFlow g;
    EXPECT(g.requestAction(Action::Import, true));
    EXPECT(!storage::resumeRecheck(g, true, true));
    EXPECT(g.dialogOpen());

    // API < 30 no resume: sem suporte → recusa (nunca concede o impossível)
    PermFlow h;
    EXPECT(h.requestAction(Action::Import, true));
    h.dialogAccept();
    EXPECT(storage::resumeRecheck(h, false, true));
    EXPECT(h.state() == FlowState::Idle);
    EXPECT(h.mode() == Mode::AppPrivate);
}

// CONTRATO drain-primeiro: o onActivityResult corre ANTES do onResume
// (lifecycle Java) — tratado pelo onSettingsReturn, o recheck do resume
// que se segue devolve false SEM mexer (o import NUNCA corre 2×)
TEST(storage_resume_drain_primeiro_resultado_tratado_nao_duplica) {
    PermFlow f;
    EXPECT(f.requestAction(Action::Import, true));
    f.dialogAccept();
    EXPECT(f.consumeOpenSettings());

    // 1) o retorno CHEGOU pela fila (caminho normal): ação retomada AQUI
    f.onSettingsReturn(true);
    EXPECT(f.takePendingAction() == Action::Import);

    // 2) o resume que se segue: estado JÁ não é PendingSettings → false
    //    e nenhuma SEGUNDA retoma acontece
    EXPECT(!storage::resumeRecheck(f, true, true));
    EXPECT(f.state() == FlowState::Granted);
    EXPECT(f.takePendingAction() == Action::None);
}

// CORRIDA INVERSA (resultado TARDO): o resume decide sem o resultado; o
// onActivityResult chega DEPOIS ao loop da engine — onSettingsReturn(true)
// é idempotente e a ação JÁ foi consumida (o export NUNCA corre 2×)
TEST(storage_resume_resultado_tardio_nao_duplica) {
    PermFlow f;
    EXPECT(f.requestAction(Action::Export, true));
    f.dialogAccept();
    EXPECT(f.consumeOpenSettings());

    // o resume decide PRIMEIRO (ecrã OEM sem setResult): concedido
    EXPECT(storage::resumeRecheck(f, true, true));
    EXPECT(f.takePendingAction() == Action::Export);   // retomado 1×

    // o resultado tardio chega: Granted mantém-se e NÃO há 2ª ação
    f.onSettingsReturn(true);
    EXPECT(f.state() == FlowState::Granted);
    EXPECT(f.mode() == Mode::AllFiles);
    EXPECT(f.takePendingAction() == Action::None);
}


// ---------------------------------------------------------------------------
// FALLBACK: sistema sem All Files Access (API < 30) → NUNCA pede, modo
// app-private imediato + estado Unsupported
// ---------------------------------------------------------------------------

TEST(storage_fallback_sem_suporte_do_sistema) {
    PermFlow f;
    // import E export NÃO abrem diálogo
    EXPECT(!f.requestAction(Action::Import, /*systemSupported=*/false));
    EXPECT(!f.requestAction(Action::Export, false));
    EXPECT(f.state() == FlowState::Unsupported);
    EXPECT(f.mode() == Mode::AppPrivate);
    EXPECT(!f.dialogOpen());
    EXPECT(!f.consumeOpenSettings());
    EXPECT(f.takePendingAction() == Action::None);
    // counter de tentativas p/ diagnóstico no log
    EXPECT(f.attempts() == 2);
}

// ---------------------------------------------------------------------------
// setMode do boot (o boot verifica isExternalStorageManager antes da UI)
// ---------------------------------------------------------------------------

TEST(storage_set_mode_do_boot) {
    PermFlow f;
    f.setMode(Mode::AllFiles);
    EXPECT(f.mode() == Mode::AllFiles);
    EXPECT(f.state() == FlowState::Granted);
    // com modo já concedido, tentativas são executadas direto (sem diálogo)
    EXPECT(!f.requestAction(Action::Import, true));

    PermFlow g;
    g.setMode(Mode::AppPrivate);
    EXPECT(g.mode() == Mode::AppPrivate);
    EXPECT(g.state() != FlowState::Granted);   // continua a pedir no 1º uso
}

// ---------------------------------------------------------------------------
// fila do retorno das settings (PendingResult reutilizado com kReqAllFiles)
// ---------------------------------------------------------------------------

TEST(storage_pending_result_do_retorno_das_settings) {
    saf::PendingResult q;
    saf::SafResult r;
    r.request = storage::kReqAllFiles;
    r.ok = true;   // RESULT_OK da janela de permissões (uri vazia — irrelevante)
    r.uri = "";
    q.push(r);

    saf::SafResult out;
    EXPECT(q.poll(&out));
    EXPECT(out.request == storage::kReqAllFiles);
    EXPECT(out.ok);
    EXPECT(out.uri.empty());
}

// ---------------------------------------------------------------------------
// File API — kindOfExtension (import decide meshes/ vs textures/ por ext)
// ---------------------------------------------------------------------------

TEST(fileapi_kind_por_extensao) {
    EXPECT(fileapi::kindOfExtension("casa.obj") == 'm');
    EXPECT(fileapi::kindOfExtension("boneco.glTF") == 'm');   // case-insensitive
    EXPECT(fileapi::kindOfExtension("cena.GLB") == 'm');
    EXPECT(fileapi::kindOfExtension("madeira.png") == 't');
    EXPECT(fileapi::kindOfExtension("MADEIRA.PNG") == 't');
    EXPECT(fileapi::kindOfExtension("leia-me.txt") == 0);
    EXPECT(fileapi::kindOfExtension("semext") == 0);
    EXPECT(fileapi::kindOfExtension(".png") == 't');          // dotfile tratado
    EXPECT(fileapi::kindOfExtension("") == 0);
}

// ---------------------------------------------------------------------------
// File API — listCandidates: só obj/gltf/glb/png, ordenado, dirs excluídos
// ---------------------------------------------------------------------------

TEST(fileapi_lista_candidatos_ordenada_e_filtrada) {
    rmrf(kTmp);
    EXPECT(fileapi::makeDirs(std::string(kTmp) + "/sub"));
    const char* names[] = {"z.png", "a.OBJ", "mid.glb", "note.txt",
                           "b.gltf", "sem_ext", "sub"};
    for (const char* n : names) {
        const std::string p = std::string(kTmp) + "/" + n;
        if (n == std::string("sub")) {
            continue;   // diretório
        }
        FILE* f = std::fopen(p.c_str(), "wb");
        EXPECT(f != nullptr);
        if (f) std::fclose(f);
    }
    // diretório COM extensão enganadora — não é candidato
    EXPECT(fileapi::makeDirs(std::string(kTmp) + "/trap.png"));

    std::vector<fileapi::Candidate> cands;
    EXPECT(fileapi::listCandidates(kTmp, cands));
    EXPECT(cands.size() == 4u);   // a.OBJ, b.gltf, mid.glb, z.png
    if (cands.size() == 4u) {
        EXPECT(cands[0].name == "a.OBJ" && cands[0].kind == 'm');
        EXPECT(cands[1].name == "b.gltf" && cands[1].kind == 'm');
        EXPECT(cands[2].name == "mid.glb" && cands[2].kind == 'm');
        EXPECT(cands[3].name == "z.png" && cands[3].kind == 't');
        // paths ABSOLUTOS relativos à pasta pedida
        EXPECT(cands[0].path == std::string(kTmp) + "/a.OBJ");
    }

    // pasta inexistente → false (errno logado)
    std::vector<fileapi::Candidate> none;
    EXPECT(!fileapi::listCandidates("/definitivamente/ausente-xyz", none));
    rmrf(kTmp);
}

// ---------------------------------------------------------------------------
// File API — writeAll/readAll roundtrip + errno capturado em falha
// ---------------------------------------------------------------------------

TEST(fileapi_write_read_roundtrip_e_errno) {
    rmrf(kTmp);
    // escrita em pasta PROFUNDA inexistente → cria as mães
    const std::string deep = std::string(kTmp) + "/a/b/c/exp.obj";
    const char* payload = "# cube de teste\nv 0 0 0\n";
    EXPECT(fileapi::writeAll(deep, payload, std::strlen(payload)));
    std::vector<u8> out;
    EXPECT(fileapi::readAll(deep, out));
    EXPECT(out.size() == std::strlen(payload));
    EXPECT(std::memcmp(out.data(), payload, out.size()) == 0);

    // escrita repetida SUBSTITUI (export 2×)
    EXPECT(fileapi::writeAll(deep, "curto", 5));
    EXPECT(fileapi::readAll(deep, out));
    EXPECT(out.size() == 5u);

    // vazio é válido (0 bytes)
    EXPECT(fileapi::writeAll(std::string(kTmp) + "/vazio.bin", "", 0));
    EXPECT(fileapi::readAll(std::string(kTmp) + "/vazio.bin", out));
    EXPECT(out.empty());

    // falha: ficheiro ausente → false + errnoText estável "errno=N (…)"
    out.clear();
    EXPECT(!fileapi::readAll(std::string(kTmp) + "/nao-existe.obj", out));
    const std::string e1 = fileapi::errnoText();
    EXPECT(e1.rfind("errno=", 0) == 0);
    EXPECT(e1.find('(') != std::string::npos && e1.find(')') != std::string::npos);

    // falha de ESCRITA: raiz que não pode ser criada (/proc é read-only no
    // CI e mkdir dentro falha com errno capturado)
    EXPECT(!fileapi::writeAll("/proc/goni-vv/impossivel/x.obj", "x", 1));
    EXPECT(fileapi::errnoText().rfind("errno=", 0) == 0);
    rmrf(kTmp);
}

// ---------------------------------------------------------------------------
// BOOT SELF-CHECK: raiz OK → linhas OK; raiz null/ausente → causa + errno
// (o que o dono lê no engine.log quando o storage falha)
// ---------------------------------------------------------------------------

TEST(fileapi_self_check_loga_causa_e_errno) {
    rmrf(kTestLogsDir);
    rmrf(kTmp);
    // a sonda de escrita do self-check mira <root>/logs/ — no device esse dir
    // já existe (o elog::init corre antes); aqui recria-se o mesmo cenário
    EXPECT(fileapi::makeDirs(std::string(kTmp) + "/logs"));
    EXPECT(vv::elog::init(kTestLogsDir));

    // raiz BOA: mapping + opendir OK + fopen OK
    fileapi::logStorageSelfCheck(kTmp, true);

    // raiz NULA: linha de erro explícita (caso "getExternalFilesDir=null")
    fileapi::logStorageSelfCheck(nullptr, false);

    vv::elog::shutdown();
    const std::string log = slurp(std::string(kTestLogsDir) + "/engine.log");
    EXPECT(log.find("self-check: getExternalFilesDir=NULL") != std::string::npos);
    EXPECT(log.find("self-check: SEM raiz de armazenamento") != std::string::npos);
    EXPECT(log.find("self-check: opendir(") != std::string::npos);
    EXPECT(log.find("self-check: fopen(") != std::string::npos);
    EXPECT(log.find("escrita de logs confirmada") != std::string::npos);

    // raiz AUSENTE: opendir falha COM errno no log
    rmrf(kTestLogsDir);
    EXPECT(vv::elog::init(kTestLogsDir));
    fileapi::logStorageSelfCheck("/goni-vv-teste-inexistente-xyz", true);
    vv::elog::shutdown();
    const std::string log2 = slurp(std::string(kTestLogsDir) + "/engine.log");
    EXPECT(log2.find("opendir(/goni-vv-teste-inexistente-xyz) FALHOU") !=
           std::string::npos);
    EXPECT(log2.find("errno=") != std::string::npos);
    rmrf(kTestLogsDir);
    rmrf(kTmp);
}

// ---------------------------------------------------------------------------
// EngineLog::readTail — o viewer lê o FIM do log, ordem antiga→recente
// ---------------------------------------------------------------------------

TEST(elog_readtail_ultima_linhas_em_ordem) {
    rmrf(kTestLogsDir);
    EXPECT(vv::elog::init(kTestLogsDir));
    for (int i = 0; i < 120; ++i) {
        char line[64];
        std::snprintf(line, sizeof(line), "linha-%03d-do-log", i);
        vv::elog::writeLine('I', line);
    }
    // NÃO shutdown antes de ler: no device o viewer lê com o log ATIVO
    // (o shutdown é fim-de-processo; no hospedeiro limparia o dir)

    // tail de 50: contém 070..119 em ORDEM (antiga primeiro)
    std::vector<std::string> lines;
    const int n50 = vv::elog::readTail(lines, 50);
    EXPECT(n50 == 50);
    if (n50 == 50) {
        EXPECT(lines[0].find("linha-070-do-log") != std::string::npos);
        EXPECT(lines[49].find("linha-119-do-log") != std::string::npos);
    }

    // tail maior que o ficheiro → todas as linhas
    lines.clear();
    const int nAll = vv::elog::readTail(lines, 500);
    EXPECT(nAll == 120);
    if (nAll == 120) {
        EXPECT(lines[0].find("linha-000-do-log") != std::string::npos);
        EXPECT(lines[119].find("linha-119-do-log") != std::string::npos);
    }

    // maxLines<=0 → 0 (guard, sem crash)
    lines.clear();
    EXPECT(vv::elog::readTail(lines, 0) == 0);
    EXPECT(vv::elog::readTail(lines, -5) == 0);

    // log INATIVO → 0 seguro
    EXPECT(vv::elog::init(nullptr) == false);
    lines.clear();
    EXPECT(vv::elog::readTail(lines, 10) == 0);
    vv::elog::shutdown();
    rmrf(kTestLogsDir);
}

TEST(elog_readtail_continua_nos_backups_de_rotacao) {
    rmrf(kTestLogsDir);
    // ~55 bytes/linha; 220 → ~4 linhas/ficheiro, 60 escritas → várias rotações
    EXPECT(vv::elog::init(kTestLogsDir, 220, 2));
    for (int i = 0; i < 60; ++i) {
        char line[48];
        std::snprintf(line, sizeof(line), "rot-%03d-xxxx-xxxx-xxxx", i);
        vv::elog::writeLine('I', line);
    }
    // o tail do viewer vem dos 3 FICHEIROS (.2 → .1 → ativo) em ORDEM de
    // continuidade (fim do mais antigo liga ao seguinte) — mesmo com rotação
    std::vector<std::string> lines;
    const int n = vv::elog::readTail(lines, 10);
    EXPECT(n > 0 && n <= 10);
    // continuidade: cada linha aparece 1× (sem repetição entre ficheiros)
    for (size_t i = 1; i < lines.size(); ++i) {
        EXPECT(lines[i] != lines[i - 1]);
    }
    // a ÚLTIMA linha é a mais recente sobrevivente do log
    if (!lines.empty()) {
        EXPECT(lines.back().find("rot-059") != std::string::npos);
    }
    vv::elog::shutdown();
    rmrf(kTestLogsDir);
}

// ---------------------------------------------------------------------------
// EngineLog::listDumps — o viewer lista crash-*.dump (recente primeiro)
// ---------------------------------------------------------------------------

TEST(elog_listdumps_recentes_primeiro) {
    rmrf(kTestLogsDir);
    ::mkdir(kTestLogsDir, 0755);
    // dumps "crash-<unixtime>.dump": 200, 100, 300 → ordem pedida 300,200,100
    const char* dumps[] = {"crash-200.dump", "crash-100.dump", "crash-300.dump"};
    for (const char* d : dumps) {
        FILE* f = std::fopen((std::string(kTestLogsDir) + "/" + d).c_str(), "wb");
        EXPECT(f != nullptr);
        if (f) std::fclose(f);
    }
    // distratores que NÃO são dumps
    FILE* other = std::fopen((std::string(kTestLogsDir) + "/engine.log").c_str(), "wb");
    if (other) std::fclose(other);
    FILE* bad = std::fopen((std::string(kTestLogsDir) + "/crash-xx.txt").c_str(), "wb");
    if (bad) std::fclose(bad);

    EXPECT(vv::elog::init(kTestLogsDir));
    std::vector<std::string> out;
    EXPECT(vv::elog::listDumps(out));
    EXPECT(out.size() == 3u);
    if (out.size() == 3u) {
        EXPECT(out[0] == "crash-300.dump");
        EXPECT(out[1] == "crash-200.dump");
        EXPECT(out[2] == "crash-100.dump");
    }
    vv::elog::shutdown();

    // sem log ativo → false
    out.clear();
    EXPECT(!vv::elog::listDumps(out));
    EXPECT(out.empty());
    rmrf(kTestLogsDir);
}

// ---------------------------------------------------------------------------
// F5.2 (item 5): o fopen do PRÓPRIO engine.log falho é seguro e DIAGNOSTICADO
// (logcat no device/stderr no hospedeiro) — o elog fica inativo sem crashar
// ---------------------------------------------------------------------------

TEST(elog_init_em_pasta_nao_criavel_e_seguro) {
    // mkdir -p não consegue criar DENTRO de /proc (raiz read-only) → open
    // falha → elog inativo; as chamadas seguintes NÃO crasham
    EXPECT(!vv::elog::init("/proc/goni-vv-teste-impossivel/logs"));
    EXPECT(!vv::elog::active());
    vv::elog::info("nada acontece");
    vv::elog::writeLine('E', "nada acontece 2");
    std::vector<std::string> lines;
    EXPECT(vv::elog::readTail(lines, 10) == 0);
    std::vector<std::string> dumps;
    EXPECT(!vv::elog::listDumps(dumps));
    vv::elog::shutdown();
}
