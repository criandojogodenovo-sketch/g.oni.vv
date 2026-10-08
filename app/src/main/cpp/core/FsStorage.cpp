// core/FsStorage.cpp — ProjectStorage sobre POSIX (F5-A).
//
// POSIX puro (dirent/stat/fopen): compila no NDK E no CI Linux sem Android.
#include "core/FsStorage.h"
#include "platform/EngineLog.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <map>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>

namespace vv {

namespace {

// mkdir -p: cria cada segmento do caminho real; true se ficou existente.
bool ensureRealDir(const std::string& realPath) {
    if (realPath.empty()) {
        return false;
    }
    std::string partial;
    size_t i = 0;
    if (realPath[0] == '/') {
        partial = "/";
        i = 1;
    }
    while (i <= realPath.size()) {
        const size_t next = realPath.find('/', i);
        const std::string seg = realPath.substr(
            i, next == std::string::npos ? std::string::npos : next - i);
        if (!seg.empty()) {
            if (!partial.empty() && partial.back() != '/') {
                partial += '/';
            }
            partial += seg;
            if (::mkdir(partial.c_str(), 0775) != 0 && errno != EEXIST) {
                return false;
            }
        }
        if (next == std::string::npos) {
            break;
        }
        i = next + 1;
    }
    struct stat st{};
    return ::stat(realPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool realExists(const std::string& realPath) {
    struct stat st{};
    return ::stat(realPath.c_str(), &st) == 0;
}

// F5.4-hotfix — sonda tri-estado real (mesma semântica do SafStorage::probe):
// ENOENT é AUSÊNCIA CONFIRMADA; qualquer outro errno é INDECIDIDO (nunca
// minta "não existe" — criar por cima de um "não sei" duplica ficheiros).
Presence realProbe(const std::string& realPath) {
    struct stat st{};
    if (::stat(realPath.c_str(), &st) == 0) {
        return Presence::Present;
    }
    return errno == ENOENT ? Presence::Absent : Presence::Unknown;
}

bool readWholeFile(const std::string& realPath, std::string& out) {
    FILE* f = std::fopen(realPath.c_str(), "rb");
    if (!f) {
        return false;
    }
    out.clear();
    char buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        out.append(buf, n);
    }
    const bool ok = std::ferror(f) == 0;
    std::fclose(f);
    return ok;
}

bool writeWholeFile(const std::string& realPath, const void* data, size_t n) {
    FILE* f = std::fopen(realPath.c_str(), "wb");
    if (!f) {
        return false;
    }
    const bool ok = n == 0 || std::fwrite(data, 1, n, f) == n;
    return ok && std::fclose(f) == 0;
}

} // namespace

FsStorage::FsStorage(std::string rootPath) : root_(std::move(rootPath)) {}

bool FsStorage::makeDirs(const std::string& relDir) {
    const std::string real = joinRelPath(root_, relDir);
    if (real.empty()) {
        return false;
    }
    return ensureRealDir(real);
}

bool FsStorage::exists(const std::string& relPath) const {
    const std::string real = joinRelPath(root_, relPath);
    return !real.empty() && realExists(real);
}

Presence FsStorage::probe(const std::string& relPath) const {
    const std::string real = joinRelPath(root_, relPath);
    if (real.empty()) {
        return Presence::Unknown;   // path inválido — indecidido (honesto)
    }
    return realProbe(real);
}

bool FsStorage::writeText(const std::string& relPath, const std::string& text) {
    const std::string real = joinRelPath(root_, relPath);
    if (real.empty()) {
        return false;
    }
    // garante o diretório do ficheiro (writeText("scenes/x.goni") sem makeDirs)
    const size_t slash = real.rfind('/');
    if (slash != std::string::npos && !ensureRealDir(real.substr(0, slash))) {
        return false;
    }
    if (!writeWholeFile(real, text.data(), text.size())) {
        return false;
    }
    // F5.4-hotfix: linha clara por escrita (o "Ver logs" do device mostra o
    // que foi gravado e com que tamanho — sem abrir gestor de ficheiros)
    elog::info("file: write %s — %zu bytes", relPath.c_str(), text.size());
    return true;
}

bool FsStorage::readText(const std::string& relPath, std::string& out) const {
    const std::string real = joinRelPath(root_, relPath);
    return !real.empty() && readWholeFile(real, out);
}

bool FsStorage::writeBytes(const std::string& relPath, const void* data, size_t n) {
    const std::string real = joinRelPath(root_, relPath);
    if (real.empty()) {
        return false;
    }
    const size_t slash = real.rfind('/');
    if (slash != std::string::npos && !ensureRealDir(real.substr(0, slash))) {
        return false;
    }
    if (!writeWholeFile(real, data, n)) {
        return false;
    }
    elog::info("file: write %s — %zu bytes", relPath.c_str(), n);
    return true;
}

bool FsStorage::readBytes(const std::string& relPath, std::vector<u8>& out) const {
    const std::string real = joinRelPath(root_, relPath);
    if (real.empty()) {
        return false;
    }
    // 0.10-M (PASSO 3B) — a DUPLICAÇÃO morta: o caminho antigo lia para uma
    // std::string (crescendo por realocação) e COPIAVA para o vector — o
    // pico de RAM era ~2x o ficheiro (medido: 2007 MB para ler um .gmesh de
    // 972 MB — a app do C33 morria só de LER para recusar). Agora: o
    // tamanho vem do stat (resize UMA vez) e o fread corre direto no buffer
    u64 want = 0;
    if (!statBytes(relPath, want)) {
        return false;
    }
    // (size_t == u64 em arm64/x86_64 — os alvos da engine e do CI)
    FILE* f = std::fopen(real.c_str(), "rb");
    if (!f) {
        return false;
    }
    out.resize(static_cast<size_t>(want));
    size_t got = 0;
    while (got < out.size()) {
        const size_t n = std::fread(out.data() + got, 1, out.size() - got, f);
        if (n == 0) {
            break;
        }
        got += n;
    }
    const bool ok = got == out.size() && std::ferror(f) == 0;
    if (!ok) {
        out.clear();
    }
    std::fclose(f);
    return ok;
}

// 0.9.6 (G6 · R-017): stat REAL (o tamanho do projeto no relatório do
// bench — sem ler os ficheiros; o SAF não implementa e diz "não medido")
bool FsStorage::statBytes(const std::string& relPath, u64& outBytes) const {
    const std::string real = joinRelPath(root_, relPath);
    if (real.empty()) {
        return false;
    }
    struct stat st{};
    if (::stat(real.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
        return false;
    }
    outBytes = static_cast<u64>(st.st_size);
    return true;
}

bool FsStorage::readBytesAt(const std::string& relPath, u64 offset,
                            size_t len, std::vector<u8>& out) const {
    // 0.10-M (PASSO 3B) — o range EXATO: fopen → fseek(offset) → fread(len).
    // O guard do load espia os 192 B do header+meta do .gmesh v3 sem tocar
    // no resto; o PASSO 4 usará o MESMO caminho para materializar blocos
    // pela tabela (offsets absolutos)
    const std::string real = joinRelPath(root_, relPath);
    if (real.empty() || len == 0) {
        out.clear();
        return len == 0;
    }
    FILE* f = std::fopen(real.c_str(), "rb");
    if (!f) {
        return false;
    }
    if (std::fseek(f, static_cast<long>(offset), SEEK_SET) != 0) {
        std::fclose(f);
        return false;
    }
    out.resize(len);
    const size_t got = std::fread(out.data(), 1, len, f);
    const bool ok = got == len && std::ferror(f) == 0;
    if (!ok) {
        out.clear();
    }
    std::fclose(f);
    return ok;
}

bool FsStorage::listDir(const std::string& relDir,
                        std::vector<std::string>& outFiles) const {
    const std::string real = joinRelPath(root_, relDir);
    if (real.empty()) {
        return false;
    }
    DIR* d = ::opendir(real.c_str());
    if (!d) {
        return false;
    }
    outFiles.clear();
    while (const dirent* e = ::readdir(d)) {
        const std::string name = e->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        struct stat st{};
        if (::stat((real + "/" + name).c_str(), &st) != 0 || !S_ISREG(st.st_mode)) {
            continue;   // só ficheiros regulares (subdirs ficam fora da lista)
        }
        outFiles.push_back(name);
    }
    ::closedir(d);
    std::sort(outFiles.begin(), outFiles.end());
    return true;
}

// ---- 0.8.10: escrita streaming REAL (FILE* aberto até ao close) -------------
namespace {
std::map<int, FILE*>& fsStreams() {
    static std::map<int, FILE*> reg;
    return reg;
}
int fsNextHandle() {
    static int next = 1000;
    return next++;
}
} // namespace

int FsStorage::openWriteStream(const std::string& relPath) {
    if (!validRelPath(relPath)) {
        return -1;
    }
    const std::string abs = joinRelPath(root(), relPath);
    if (abs.empty()) {
        return -1;
    }
    // "wb" cria/trunca (o makeDirs do caller garantiu o pai; fopen falha
    // com errno claro se faltar)
    FILE* f = std::fopen(abs.c_str(), "wb");
    if (!f) {
        elog::error("fs: writeStream open '%s' FALHOU (errno=%d)", abs.c_str(), errno);
        return -1;
    }
    const int h = fsNextHandle();
    fsStreams()[h] = f;
    return h;
}

bool FsStorage::writeStreamChunk(int handle, const void* data, size_t n) {
    auto& reg = fsStreams();
    const auto it = reg.find(handle);
    if (it == reg.end()) {
        return false;
    }
    if (n > 0 && std::fwrite(data, 1, n, it->second) != n) {
        return false;
    }
    return true;
}

void FsStorage::closeWriteStream(int handle) {
    auto& reg = fsStreams();
    const auto it = reg.find(handle);
    if (it == reg.end()) {
        return;
    }
    std::fflush(it->second);
    std::fclose(it->second);
    reg.erase(it);
}

bool FsStorage::remove(const std::string& relPath) {
    if (!validRelPath(relPath)) {
        return false;
    }
    const std::string abs = joinRelPath(root(), relPath);
    if (abs.empty() || ::remove(abs.c_str()) != 0) {
        elog::error("fs: remove '%s' FALHOU (errno=%d)", relPath.c_str(), errno);
        return false;
    }
    return true;
}

} // namespace vv
