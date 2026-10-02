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
    std::string tmp;
    if (!readWholeFile(real, tmp)) {
        return false;
    }
    out.assign(tmp.begin(), tmp.end());
    return true;
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

} // namespace vv
