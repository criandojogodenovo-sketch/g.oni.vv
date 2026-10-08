// tests/test_logs_crash.cpp — F5.1-hotfix: log writer (parte 1.1) e crash
// dump (parte 1.2) no hospedeiro. O CI Linux não tem Android — o que se
// aferi é a PARTE DE FICHEIRO (append, rotação 3×N, formatos), que é
// exatamente o que o dono lê no device sem PC.
//
//   parte 1.1 → log writer: append, rotação, inativo seguro
//   parte 1.2 → crash dump: writeDumpFromFrames (formato legível sem
//               ndk-stack) + handler REAL num subprocesso (fork + SIGSEGV)
//   parte 1.4 → export de logs: caminho de Downloads constante/afervel
#include "TestFramework.h"
#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "platform/EngineLog.h"
#include "platform/CrashHandler.h"

#include <sys/wait.h>
#include <unistd.h>

using namespace vv;

namespace {

// diretório de teste RELATIVO (ctest corre no build dir do CI)
const char* kTestDir = "test-logs-tmp";

std::string slurp(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return "";
    std::string data((std::istreambuf_iterator<char>(in)),
                      std::istreambuf_iterator<char>());
    return data;
}

bool fileExists(const std::string& path) {
    struct stat st;
    return ::stat(path.c_str(), &st) == 0 && S_ISREG(st.st_mode);
}

int countLines(const std::string& data) {
    if (data.empty()) return 0;
    int n = 1;
    for (char c : data) if (c == '\n') ++n;
    return n - (data.back() == '\n' ? 1 : 0);
}

void rmrf(const std::string& dir) {
    DIR* d = ::opendir(dir.c_str());
    if (d) {
        while (dirent* e = ::readdir(d)) {
            const std::string n = e->d_name;
            if (n == "." || n == "..") continue;
            ::remove((dir + "/" + n).c_str());
        }
        ::closedir(d);
    }
    ::remove(dir.c_str());
}

} // namespace

// ---------------------------------------------------------------------------
// parte 1.1 — log writer
// ---------------------------------------------------------------------------

TEST(log_write_and_append) {
    rmrf(kTestDir);
    EXPECT(vv::elog::init(kTestDir));
    EXPECT(vv::elog::active());
    EXPECT(std::string(vv::elog::dir()) == kTestDir);

    vv::elog::info("boot msg info %d", 42);
    vv::elog::warn("boot msg warn");
    vv::elog::error("boot msg error");

    const std::string data = slurp(std::string(kTestDir) + "/engine.log");
    EXPECT(countLines(data) == 3);
    EXPECT(data.find("boot msg info 42\n") != std::string::npos);
    EXPECT(data.find(" I/GONI: boot msg info 42") != std::string::npos);
    EXPECT(data.find(" W/GONI: boot msg warn") != std::string::npos);
    EXPECT(data.find(" E/GONI: boot msg error") != std::string::npos);

    // append: uma 2ª chamada NÃO trunca
    vv::elog::info("segunda linha");
    const std::string data2 = slurp(std::string(kTestDir) + "/engine.log");
    EXPECT(countLines(data2) == 4);
    EXPECT(data2.find("boot msg info 42") != std::string::npos);
    EXPECT(data2.find("segunda linha") != std::string::npos);
    vv::elog::shutdown();
    rmrf(kTestDir);
}

TEST(log_inactive_is_safe) {
    rmrf(kTestDir);
    // null e vazio → inativo (só logcat no device); chamadas NÃO crasham
    EXPECT(!vv::elog::init(nullptr));
    EXPECT(!vv::elog::init(""));
    EXPECT(!vv::elog::active());
    vv::elog::info("nada acontece");
    vv::elog::writeLine('E', "nada acontece 2");
    vv::elog::shutdown();
    EXPECT(!fileExists(std::string(kTestDir) + "/engine.log"));
}

TEST(log_rotation_three_files) {
    rmrf(kTestDir);
    // 256 bytes/rotação, 2 backups → no máximo 3 ficheiros
    EXPECT(vv::elog::init(kTestDir, 256, 2));
    const std::string line = "0123456789012345678901234567890123456789"; // 40
    for (int i = 0; i < 40; ++i) {              // ~1.7KB → ≥2 rotações
        vv::elog::writeLine('I', line.c_str());
    }
    vv::elog::info("pos-rotacao-final");        // ficheiro ativo não-vazio
    const std::string base = kTestDir;
    EXPECT(fileExists(base + "/engine.log"));
    EXPECT(fileExists(base + "/engine.log.1"));
    EXPECT(fileExists(base + "/engine.log.2"));
    // o 3º backup NÃO existe (cap de 3 ficheiros; o mais velho é descartado)
    EXPECT(!fileExists(base + "/engine.log.3"));

    // o ficheiro ativo voltou a ficar abaixo do limiar
    const std::string cur = slurp(base + "/engine.log");
    EXPECT(countLines(cur) >= 1);
    EXPECT(cur.size() < 256u);

    // conteúdo antigo preservado nos backups (ordem: .1 mais recente)
    const std::string b1 = slurp(base + "/engine.log.1");
    const std::string b2 = slurp(base + "/engine.log.2");
    EXPECT(!b1.empty());
    EXPECT(!b2.empty());
    for (const std::string* s : {&cur, &b1, &b2}) {
        EXPECT(s->find("I/GONI: ") != std::string::npos);
    }
    vv::elog::shutdown();
    rmrf(kTestDir);
}

TEST(log_rotation_keeps_appending_after_rotate) {
    rmrf(kTestDir);
    EXPECT(vv::elog::init(kTestDir, 200, 1));
    for (int i = 0; i < 30; ++i) {
        vv::elog::writeLine('I', "linha-de-teste-para-rotacao-continua");
    }
    // o log continua a escrever depois das rotações
    vv::elog::info("pos-rotacao");
    const std::string cur = slurp(std::string(kTestDir) + "/engine.log");
    EXPECT(cur.find("pos-rotacao") != std::string::npos);
    vv::elog::shutdown();
    rmrf(kTestDir);
}

// ---------------------------------------------------------------------------
// 0.10-M (PASSO 3B) — O BUG DO «LOG DE ONTEM» no viewer (readTail)
//
// A ordem de leitura antiga (.2 → .1 → ativo, break ao encher maxLines)
// entregava o backup MAIS ANTIGO quando havia rotação: o ficheiro ATIVO
// — o de HOJE, com o banner do boot e os erros da sessão — nunca era
// lido. O dono abria «Ver logs» e via conteúdo de ONTEM. A cura: o ATIVO
// lê-se primeiro; os backups só completam quando FALTAM linhas (pela
// cauda). Estes testes são a tripwire da mutação M-A (repor a ordem
// antiga = VERMELHO).
// ---------------------------------------------------------------------------

TEST(log_readtail_o_ativo_de_hoje_ganha_as_rotacoes) {
    rmrf(kTestDir);
    // A REPRODUÇÃO DO DEVICE: cada ficheiro de rotação guarda MUITAS linhas
    // (no device 1 MB ≈ centenas de linhas de 2 KB do import). Aqui: 20 KB
    // por ficheiro, ~350 linhas de ~55 B cada — o .2 tem MUITO mais que as
    // 300 linhas da janela do viewer. Era isto que fazia o viewer mostrar
    // ONTEM: o .2 enchia a janela sozinho e o ativo nunca era lido.
    EXPECT(vv::elog::init(kTestDir, 20 * 1024, 2));
    const std::string line = "conteudo-velho-de-ontem-0123456789012345"; // ~40
    for (int i = 0; i < 700; ++i) {              // 2+ rotações: .2 cheio
        vv::elog::writeLine('I', line.c_str());
    }
    // AS LINHAS DE HOJE (as últimas escritas — o que o dono PRECISA de ver)
    for (int i = 1; i <= 5; ++i) {
        char m[48];
        std::snprintf(m, sizeof(m), "hoje-marcador-%02d", i);
        vv::elog::info("%s", m);
    }
    EXPECT(fileExists(std::string(kTestDir) + "/engine.log.2"));   // rodou 2×

    // o viewer (300 linhas como no device) TEM as linhas de hoje — com a
    // ordem antiga o .2 enchia as 300 e HOJE NUNCA ENTRAVA (o vermelho da
    // mutação M-A é exatamente aqui)
    std::vector<std::string> out;
    const int n = vv::elog::readTail(out, 300);
    EXPECT(n == 300);
    bool hasToday = false;
    for (const std::string& l : out) {
        if (l.find("hoje-marcador-05") != std::string::npos) {
            hasToday = true;
        }
    }
    EXPECT(hasToday);

    // e o ÚLTIMO elemento é o MAIS RECENTE (ordem cronológica — o fim do
    // ativo, não o fim de um backup velho)
    EXPECT(!out.empty() &&
           out.back().find("hoje-marcador-05") != std::string::npos);

    // janela APERTADA (menos linhas que as disponíveis): as mais RECENTES
    // ganham — as 3 últimas são hoje-03..05, não conteúdo de ontem
    std::vector<std::string> out3;
    EXPECT(vv::elog::readTail(out3, 3) == 3);
    EXPECT(out3[0].find("hoje-marcador-03") != std::string::npos);
    EXPECT(out3[2].find("hoje-marcador-05") != std::string::npos);
    vv::elog::shutdown();
    rmrf(kTestDir);
}

TEST(log_readtail_sem_rotacao_le_o_ativo) {
    rmrf(kTestDir);
    EXPECT(vv::elog::init(kTestDir));
    // sem rotação nenhuma (o cenário comum de uma sessão leve)
    vv::elog::info("linha-unica-1");
    vv::elog::info("linha-unica-2");
    std::vector<std::string> out;
    EXPECT(vv::elog::readTail(out, 300) == 2);
    EXPECT(out.size() == 2);
    EXPECT(out[0].find("linha-unica-1") != std::string::npos);
    EXPECT(out[1].find("linha-unica-2") != std::string::npos);
    vv::elog::shutdown();
    rmrf(kTestDir);
}

// ---------------------------------------------------------------------------
// parte 1.4 — export de logs: caminho canónico no Downloads público
// ---------------------------------------------------------------------------

TEST(log_downloads_path_constant) {
    // Environment.DIRECTORY_DOWNLOADS == "Download" — o Java monta
    // <REL>/… a partir desta constante ÚNICA (recebida por JNI).
    EXPECT(std::string(vv::elog::kDownloadsRelPath) == "Download/GOneVV/logs");
}

// ---------------------------------------------------------------------------
// parte 1.2 — crash dump
// ---------------------------------------------------------------------------

TEST(crash_writeDumpFromFrames_formato_legivel) {
    rmrf(kTestDir);
    ::mkdir(kTestDir, 0755);
    void* pcs[8];
    const int n = vv::crash::captureFrames(pcs, 8);
    EXPECT(n >= 2);   // este teste + o runner de casos já dão ≥2 frames

    const std::string path = std::string(kTestDir) + "/crash-test.dump";
    EXPECT(vv::crash::writeDumpFromFrames(path.c_str(), "SIGSEGV", 11,
                                          nullptr, pcs, n) == 0);
    const std::string data = slurp(path);
    EXPECT(data.find("crash dump") != std::string::npos);
    EXPECT(data.find("signal: SIGSEGV (11)") != std::string::npos);
    EXPECT(data.find("frames: ") != std::string::npos);
    // frames com pc + offset relativo à base do módulo (legível sem ndk-stack)
    EXPECT(data.find("#00 pc 0x") != std::string::npos);
    EXPECT(data.find("libgoni_vv") != std::string::npos ||
           data.find("test_core") != std::string::npos);
    // a 2ª frame referência o símbolo do TESTE atual (dladdr resolve)
    EXPECT(data.find("test_") != std::string::npos);
    rmrf(kTestDir);
}

// handler REAL num subprocesso: instala, levanta SIGSEGV, verifica que o
// dump aparece com o nome do sinal e frames (o CI Linux executa o MESMO
// código que o device corre no boot).
void child_raises_sigsegv(const char* dir) {
    vv::crash::install(dir);
    ::raise(SIGSEGV);   // handler → dump → re-raise default → morte por sinal
    ::_exit(0);         // não deve chegar aqui
}

TEST(crash_handler_captura_sigsegv_num_subprocesso) {
    rmrf(kTestDir);
    ::mkdir(kTestDir, 0755);
    const pid_t pid = ::fork();
    EXPECT(pid >= 0);
    if (pid == 0) {
        child_raises_sigsegv(kTestDir);   // nunca retorna
    }
    int status = 0;
    EXPECT(::waitpid(pid, &status, 0) == pid);
    EXPECT(WIFSIGNALED(status));        // morreu POR SIGSEGV (re-raise)
    EXPECT(WTERMSIG(status) == SIGSEGV);

    // o dump existe e tem o formato esperado
    const std::string path = std::string(kTestDir) + "/crash-test.dump";
    // o handler usa crash-<unixtime>.dump — procurar o ficheiro real
    std::string found;
    DIR* d = ::opendir(kTestDir);
    if (d) {
        while (dirent* e = ::readdir(d)) {
            const std::string n = e->d_name;
            if (n.rfind("crash-", 0) == 0 &&
                n.rfind(".dump") == n.size() - 5) {
                found = std::string(kTestDir) + "/" + n;
            }
        }
        ::closedir(d);
    }
    EXPECT(!found.empty());
    const std::string data = found.empty() ? "" : slurp(found);
    EXPECT(data.find("signal: SIGSEGV (11)") != std::string::npos);
    EXPECT(data.find("#00 pc ") != std::string::npos ||
           data.find("#-1 pc ") != std::string::npos ||
           data.find("frames: ") != std::string::npos);

    // e o engine.log (se existir no dir) recebe a marca "CRASH"
    vv::elog::init(kTestDir);
    vv::elog::info("linha-normal-do-boot");
    vv::elog::shutdown();
    rmrf(kTestDir);
}

TEST(crash_signalName_cobre_os_4_sinais) {
    EXPECT(std::string(vv::crash::signalName(SIGSEGV)) == "SIGSEGV");
    EXPECT(std::string(vv::crash::signalName(SIGABRT)) == "SIGABRT");
    EXPECT(std::string(vv::crash::signalName(SIGBUS)) == "SIGBUS");
    EXPECT(std::string(vv::crash::signalName(SIGFPE)) == "SIGFPE");
    EXPECT(vv::crash::writeDumpFromFrames(nullptr, "SIGSEGV", 11, nullptr,
                                          nullptr, 0) != 0);   // path null → erro, sem crash
}
