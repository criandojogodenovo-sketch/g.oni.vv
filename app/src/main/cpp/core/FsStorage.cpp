// core/FsStorage.cpp — ProjectStorage sobre POSIX (F5-A).
//
// POSIX puro (dirent/stat/fopen): compila no NDK E no CI Linux sem Android.
#include "core/FsStorage.h"
#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
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
    return writeWholeFile(real, text.data(), text.size());
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
    return writeWholeFile(real, data, n);
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

} // namespace vv
