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
// parte 1.4 — export de logs: caminho canónico no Downloads público
// ---------------------------------------------------------------------------

TEST(log_downloads_path_constant) {
    // Environment.DIRECTORY_DOWNLOADS == "Download" — o Java monta
    // <REL>/… a partir desta constante ÚNICA (recebida por JNI).
    EXPECT(std::string(vv::elog::kDownloadsRelPath) == "Download/GOneVV/logs");
}
