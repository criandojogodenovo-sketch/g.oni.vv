// platform/FileApi.cpp — File API POSIX direta (F5.2). Ver FileApi.h.
//
// Toda a falha loga "errno=<n> (<strerror>)" via elog — o dono lê a CAUSA
// no engine.log (o 0.6.1 mostrava "SAF indisponível" sem causa; aqui cada
// fopen/opendir falhado diz exatamente porquê).
#include "platform/FileApi.h"
#include "platform/EngineLog.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>

namespace vv::fileapi {

namespace {

// última mensagem de errno (thread-local para não correr entre threads)
thread_local char g_errBuf[96] = "";

void captureErrno(const char* what, const char* path) {
    const int e = errno;
    std::snprintf(g_errBuf, sizeof(g_errBuf), "errno=%d (%s)", e,
                  std::strerror(e));
    elog::warn("fileapi: %s falhou em '%s' — errno=%d (%s)",
               what, path ? path : "(null)", e, std::strerror(e));
}

std::string lowerExt(const std::string& name) {
    const size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot + 1 >= name.size()) {
        return "";
    }
    std::string ext = name.substr(dot + 1);
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return ext;
}

// nome sem path (p/ ordenação case-insensitive)
std::string baseName(const std::string& path) {
    const size_t slash = path.rfind('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

bool nameLess(const Candidate& a, const Candidate& b) {
    std::string la = a.name, lb = b.name;
    for (char& c : la) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (char& c : lb) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return la < lb;
}

} // namespace

char kindOfExtension(const std::string& name) {
    const std::string e = lowerExt(name);
    if (e == "obj" || e == "gltf" || e == "glb") return 'm';
    if (e == "png") return 't';
    return 0;
}

std::string errnoText() {
    return g_errBuf;
}

bool makeDirs(const std::string& dir) {
    if (dir.empty()) {
        return false;
    }
    std::string path = dir;
    // normaliza barra final (mkdir não gosta de trailing slash nas pontas)
    while (path.size() > 1 && path.back() == '/') {
        path.pop_back();
    }
    for (size_t i = 1; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            const std::string sub = path.substr(0, i);
            if (::mkdir(sub.c_str(), 0775) != 0 && errno != EEXIST) {
                captureErrno("mkdir", sub.c_str());
                return false;
            }
        }
    }
    return true;
}

bool listCandidates(const std::string& dir, std::vector<Candidate>& out) {
    out.clear();
    errno = 0;
    DIR* d = ::opendir(dir.c_str());
    if (!d) {
        captureErrno("opendir", dir.c_str());
        return false;
    }
    while (dirent* e = ::readdir(d)) {
        const std::string name = e->d_name;
        if (name == "." || name == "..") {
            continue;
        }
        if (kindOfExtension(name) == 0) {
            continue;   // só obj/gltf/glb/png
        }
        Candidate c;
        c.name = name;
        c.path = dir + (dir.back() == '/' ? "" : "/") + name;
        c.kind = kindOfExtension(name);
        // exclui DIRETÓRIOS com extensão enganadora (d_type é melhor-effort;
        // fallback stat — DT_UNKNOWN é comum em alguns filesystems)
        if (e->d_type == DT_DIR) {
            continue;
        }
        if (e->d_type == DT_UNKNOWN) {
            struct stat st;
            if (::stat(c.path.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) {
                continue;
            }
        }
        out.push_back(c);
    }
    ::closedir(d);
    std::sort(out.begin(), out.end(), nameLess);
    return true;
}

bool readAll(const std::string& path, std::vector<u8>& out) {
    out.clear();
    errno = 0;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        captureErrno("fopen/read", path.c_str());
        return false;
    }
    std::fseek(f, 0, SEEK_END);
    const long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size < 0) {
        captureErrno("ftell", path.c_str());
        std::fclose(f);
        return false;
    }
    out.resize(static_cast<size_t>(size));
    const size_t got = size > 0 ? std::fread(out.data(), 1, out.size(), f) : 0;
    std::fclose(f);
    if (static_cast<long>(got) != size) {
        std::snprintf(g_errBuf, sizeof(g_errBuf), "errno=%d (leitura curta: %zu/%ld)",
                      EIO, got, size);
        elog::warn("fileapi: leitura curta em '%s' (%zu/%ld)", path.c_str(), got, size);
        out.clear();
        return false;
    }
    return true;
}

bool writeAll(const std::string& path, const void* data, size_t n) {
    // pastas-mãe em falta (mkdir -p) — errno logado dentro
    const size_t slash = path.rfind('/');
    if (slash != std::string::npos) {
        if (!makeDirs(path.substr(0, slash))) {
            return false;
        }
    }
    errno = 0;
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        captureErrno("fopen/write", path.c_str());
        return false;
    }
    bool ok = true;
    if (n > 0) {
        ok = std::fwrite(data, 1, n, f) == n;
    }
    if (std::fflush(f) != 0) {
        ok = false;
    }
    std::fclose(f);
    if (!ok) {
        captureErrno("fwrite", path.c_str());
        return false;
    }
    return true;
}

void logStorageSelfCheck(const char* root, bool isExternal) {
    // 1) o mapeamento do boot (o "null?" pedido no escopo)
    elog::info("self-check: getExternalFilesDir=%s (fonte=%s)",
               (root && root[0]) ? root : "NULL",
               isExternal ? "external" : "internal/fallback");
    if (!root || !root[0]) {
        elog::error("self-check: SEM raiz de armazenamento — editor sem "
                    "persistência (este é o caso 'null' do escopo)");
        return;
    }
    // 2) sonda de LEITURA: opendir na raiz
    errno = 0;
    if (DIR* d = ::opendir(root)) {
        ::closedir(d);
        elog::info("self-check: opendir(%s) OK", root);
    } else {
        const int e = errno;
        elog::error("self-check: opendir(%s) FALHOU — errno=%d (%s)",
                    root, e, std::strerror(e));
    }
    // 3) sonda de ESCRITA: fopen na pasta logs/ (a mesma do engine.log)
    const std::string probe = std::string(root) + "/logs/.probe";
    errno = 0;
    if (FILE* f = std::fopen(probe.c_str(), "a")) {
        std::fclose(f);
        ::remove(probe.c_str());
        elog::info("self-check: fopen(%s) OK — escrita de logs confirmada",
                   probe.c_str());
    } else {
        const int e = errno;
        elog::error("self-check: fopen(%s) FALHOU — errno=%d (%s)",
                    probe.c_str(), e, std::strerror(e));
    }
}

} // namespace vv::fileapi
