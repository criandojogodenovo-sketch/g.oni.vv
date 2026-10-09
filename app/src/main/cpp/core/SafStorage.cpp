// core/SafStorage.cpp — ProjectStorage sobre SAF (F5.4).
//
// I/O de BYTES: o SafIo entrega um fd POSIX (ParcelFileDescriptor.detachFd
// do lado Java) — a partir daí é read/write normal; fechar o fd é o que
// persiste no provider. Toda falha é logada COM A CAUSA (mensagens
// honestas — nada de engolir erros de permissão como "ficheiro ausente").
#include "core/SafStorage.h"
#include <map>
#include "platform/EngineLog.h"
#include <algorithm>
#include <cerrno>
#include <cctype>
#include <cstring>
#include <unistd.h>

namespace vv {

using storage::SafEntry;
using storage::kSafDirMime;

namespace {

// guard de fd POSIX (o fd veio da Java — fechar persiste no provider)
struct FdGuard {
    int fd;
    explicit FdGuard(int f) : fd(f) {}
    ~FdGuard() {
        if (fd >= 0) {
            ::close(fd);
        }
    }
};

bool writeAllFd(int fd, const void* data, size_t n) {
    const char* p = static_cast<const char*>(data);
    size_t left = n;
    while (left > 0) {
        const ssize_t w = ::write(fd, p, left);
        if (w <= 0) {
            return false;
        }
        p += w;
        left -= static_cast<size_t>(w);
    }
    return true;
}

bool readAllFd(int fd, std::vector<u8>& out) {
    out.clear();
    u8 buf[8192];
    ssize_t r;
    while ((r = ::read(fd, buf, sizeof(buf))) > 0) {
        out.insert(out.end(), buf, buf + r);
    }
    return r == 0;   // 0 = EOF; -1 = erro
}

std::string errnoText() {
    return std::string(::strerror(errno));
}

} // namespace

SafStorage::SafStorage(storage::SafIo* io, std::string treeUri)
    : io_(io), root_(std::move(treeUri)) {}

const char* SafStorage::mimeForName(const std::string& name) {
    // F5.4-hotfix (CAUSA RAIZ da duplicação "main.goni (1).json"): os
    // providers reais passam o displayName por FileUtils.buildUniqueFile(mime,
    // nome) no createDocument — com um mime cuja extensão canónica diverge da
    // do ficheiro, o provider REESCREVE o nome ("x.goni" + application/json →
    // "x.goni.json"). O nome no disco passava a divergir do nome procurado →
    // a verificação de existência falhava em TODO boot → createDocument de
    // novo → sufixo anticolisão " (1)", " (2)"… a crescer para sempre.
    // REGRA: application/octet-stream é o caminho "nome verbatim" (o
    // buildUniqueFile preserva-o SEM acrescentar extensão); application/json
    // só para .json (extensão == canónica do mime — nada muda).
    const size_t dot = name.rfind('.');
    std::string ext = dot == std::string::npos ? "" : name.substr(dot + 1);
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    if (ext == "json") return "application/json";
    return "application/octet-stream";   // .goni/.obj/.gltf/.glb/.png/…
}

bool SafStorage::resolveDir(const std::string& relDir, bool createMissing,
                            std::string& outUri, std::string& err) const {
    const auto cached = dirUris_.find(relDir);
    if (cached != dirUris_.end()) {
        outUri = cached->second;
        return true;
    }
    if (relDir.empty()) {
        // raiz da árvore — o único salto tree→documento (feito na Java)
        std::string uri;
        if (!io_->rootDoc(root_, uri, err)) {
            return false;
        }
        dirUris_[""] = uri;
        outUri = uri;
        return true;
    }
    const size_t slash = relDir.rfind('/');
    const std::string parentRel =
        slash == std::string::npos ? "" : relDir.substr(0, slash);
    const std::string leaf =
        slash == std::string::npos ? relDir : relDir.substr(slash + 1);
    std::string parentUri;
    if (!resolveDir(parentRel, createMissing, parentUri, err)) {
        return false;
    }
    bool found = false;
    std::string uri;
    if (!io_->resolveChild(parentUri, leaf.c_str(), found, uri, err)) {
        return false;
    }
    if (!found) {
        if (!createMissing) {
            err = "pasta ausente no projeto SAF: " + relDir;
            return false;
        }
        if (!io_->create(parentUri, kSafDirMime, leaf.c_str(), uri, err)) {
            err = "criar pasta " + relDir + " falhou: " + err;
            return false;
        }
        elog::info("saf: pasta criada: %s", relDir.c_str());
    }
    dirUris_[relDir] = uri;
    outUri = uri;
    return true;
}

bool SafStorage::resolveFile(const std::string& relPath, bool createMissing,
                             std::string& outUri, std::string& err) const {
    const auto cached = fileUris_.find(relPath);
    if (cached != fileUris_.end()) {
        outUri = cached->second;
        return true;
    }
    const size_t slash = relPath.rfind('/');
    const std::string parentRel =
        slash == std::string::npos ? "" : relPath.substr(0, slash);
    const std::string leaf =
        slash == std::string::npos ? relPath : relPath.substr(slash + 1);
    std::string parentUri;
    if (!resolveDir(parentRel, createMissing, parentUri, err)) {
        return false;
    }
    bool found = false;
    std::string uri;
    if (!io_->resolveChild(parentUri, leaf.c_str(), found, uri, err)) {
        return false;
    }
    if (!found) {
        if (!createMissing) {
            err = "ficheiro ausente no projeto SAF: " + relPath;
            return false;
        }
        if (!io_->create(parentUri, mimeForName(leaf), leaf.c_str(), uri, err)) {
            err = "criar " + relPath + " falhou: " + err;
            return false;
        }
        elog::info("saf: ficheiro criado: %s", relPath.c_str());
    }
    fileUris_[relPath] = uri;
    outUri = uri;
    return true;
}

bool SafStorage::makeDirs(const std::string& relDir) {
    if (!io_ || !validRelPath(relDir)) {
        return false;
    }
    std::string err, uri;
    if (!resolveDir(relDir, true, uri, err)) {
        elog::error("saf: makeDirs %s — %s", relDir.c_str(), err.c_str());
        return false;
    }
    return true;
}

bool SafStorage::exists(const std::string& relPath) const {
    return probe(relPath) == Presence::Present;
}

// F5.4-hotfix — sonda TRI-ESTADO com recursão honesta:
//   cache quente → Present; pai não-Present → herda (Absent confirmado pela
//   MESMA recursão, Unknown herdado); resolveChild ERRO → Unknown;
//   resolveChild ok + achou → Present; ok + não achou → Absent CONFIRMADO.
// É esta distinção que impede o createNew de correr por cima de um
// provider em falha (o bug do boot que duplicava).
Presence SafStorage::probe(const std::string& relPath) const {
    if (!io_ || !validRelPath(relPath)) {
        return Presence::Unknown;
    }
    // cache quente (resolve de dirs já conhecidos não toca no provider)
    if (dirUris_.count(relPath) || fileUris_.count(relPath)) {
        return Presence::Present;
    }
    const size_t slash = relPath.rfind('/');
    const std::string parentRel =
        slash == std::string::npos ? "" : relPath.substr(0, slash);
    const std::string leaf =
        slash == std::string::npos ? relPath : relPath.substr(slash + 1);
    std::string err, parentUri, uri;
    if (!parentRel.empty()) {
        const Presence pp = probe(parentRel);
        if (pp != Presence::Present) {
            // pai ausente CONFIRMADO → filho também ausente (mesma recursão;
            // pai Unknown → filho indecidido)
            return pp == Presence::Absent ? Presence::Absent
                                          : Presence::Unknown;
        }
    }
    if (!resolveDir(parentRel, false, parentUri, err)) {
        return Presence::Unknown;   // provider falhou — NÃO decidir
    }
    bool found = false;
    if (!io_->resolveChild(parentUri, leaf.c_str(), found, uri, err)) {
        return Presence::Unknown;   // query falhou — NÃO decidir
    }
    return found ? Presence::Present : Presence::Absent;
}

bool SafStorage::writeText(const std::string& relPath, const std::string& text) {
    return writeBytes(relPath, text.data(), text.size());
}

bool SafStorage::writeBytes(const std::string& relPath, const void* data,
                            size_t n) {
    if (!io_ || !validRelPath(relPath)) {
        return false;
    }
    std::string err, uri;
    if (!resolveFile(relPath, true, uri, err)) {
        elog::error("saf: write %s — %s", relPath.c_str(), err.c_str());
        return false;
    }
    // "wt" = WRITE + TRUNCATE explícito (o doc já existia — resolvido em
    // cima; reabrir e truncar é o que SOBRESCREVE sem criar "nome (1)")
    int fd = -1;
    if (!io_->openFd(uri, "wt", &fd, err)) {
        elog::error("saf: open(wt) %s — %s", relPath.c_str(), err.c_str());
        return false;
    }
    FdGuard g{fd};
    if (!writeAllFd(fd, data, n)) {
        elog::error("saf: write(fd) %s FALHOU — errno=%d (%s)",
                    relPath.c_str(), errno, errnoText().c_str());
        return false;
    }
    // F5.4-hotfix: linha clara por escrita bem-sucedida — o "Ver logs" do
    // device mostra exatamente o que foi gravado e com que tamanho (o dono
    // confirma assets no sítio sem abrir o gestor de ficheiros)
    elog::info("saf: write %s — %zu bytes", relPath.c_str(), n);
    return true;   // FdGuard fecha → o provider persiste
}

bool SafStorage::readText(const std::string& relPath, std::string& out) const {
    std::vector<u8> bytes;
    if (!readBytes(relPath, bytes)) {
        return false;
    }
    out.assign(bytes.begin(), bytes.end());
    return true;
}

bool SafStorage::readBytes(const std::string& relPath,
                           std::vector<u8>& out) const {
    if (!io_ || !validRelPath(relPath)) {
        return false;
    }
    std::string err, uri;
    if (!resolveFile(relPath, false, uri, err)) {
        elog::error("saf: read %s — %s", relPath.c_str(), err.c_str());
        return false;
    }
    int fd = -1;
    if (!io_->openFd(uri, "r", &fd, err)) {
        elog::error("saf: open(r) %s — %s", relPath.c_str(), err.c_str());
        return false;
    }
    FdGuard g{fd};
    if (!readAllFd(fd, out)) {
        elog::error("saf: read(fd) %s FALHOU — errno=%d (%s)",
                    relPath.c_str(), errno, errnoText().c_str());
        return false;
    }
    return true;
}

bool SafStorage::readBytesAt(const std::string& relPath, u64 offset,
                             size_t len, std::vector<u8>& out) const {
    // 0.10-M (PASSO 3B) — o range EXATO pelo fd do SAF: openFd → lseek →
    // read(len). O guard do load espia o header+meta do .gmesh v3 (192 B)
    // sem ler o ficheiro inteiro pelo content:// (o mesmo contrato do
    // FsStorage::readBytesAt)
    out.clear();
    if (!io_ || !validRelPath(relPath)) {
        return false;
    }
    if (len == 0) {
        return true;
    }
    std::string err, uri;
    if (!resolveFile(relPath, false, uri, err)) {
        elog::error("saf: readAt %s — %s", relPath.c_str(), err.c_str());
        return false;
    }
    int fd = -1;
    if (!io_->openFd(uri, "r", &fd, err)) {
        elog::error("saf: open(r) %s — %s", relPath.c_str(), err.c_str());
        return false;
    }
    FdGuard g{fd};
    if (::lseek(fd, static_cast<off_t>(offset), SEEK_SET) ==
        static_cast<off_t>(-1)) {
        elog::error("saf: lseek(%llu) %s FALHOU — errno=%d (%s)",
                    static_cast<unsigned long long>(offset),
                    relPath.c_str(), errno, errnoText().c_str());
        return false;
    }
    out.resize(len);
    size_t got = 0;
    while (got < len) {
        const ssize_t r = ::read(fd, out.data() + got, len - got);
        if (r < 0) {
            elog::error("saf: read(fd) %s FALHOU — errno=%d (%s)",
                        relPath.c_str(), errno, errnoText().c_str());
            out.clear();
            return false;
        }
        if (r == 0) {
            break;   // EOF antes de len — range além do fim
        }
        got += static_cast<size_t>(r);
    }
    if (got != len) {
        out.clear();
        return false;
    }
    return true;
}

bool SafStorage::openReadFd(const std::string& relPath, int* outFd,
                            std::string& err) {
    // 0.10-M (SAF-STREAM) — O FD DO BRIDGE, p/ o mmap POR FD do conversor
    // streaming: resolveFile → io_->openFd(uri, "r"). No device o
    // JniSafIo chama o VvActivity.bridgeOpenFd (ContentResolver
    // .openFileDescriptor + detachFd) — o fd é PROPRIEDADE do chamador
    // (o mmap segura a própria referência; quem abriu FECHA o fd).
    if (outFd != nullptr) {
        *outFd = -1;
    }
    if (!io_ || !validRelPath(relPath)) {
        err = "caminho inválido: " + relPath;
        return false;
    }
    std::string uri;
    if (!resolveFile(relPath, false, uri, err)) {
        elog::error("saf: openReadFd %s — %s", relPath.c_str(), err.c_str());
        return false;
    }
    int fd = -1;
    if (!io_->openFd(uri, "r", &fd, err)) {
        elog::error("saf: openReadFd open(r) %s — %s", relPath.c_str(),
                    err.c_str());
        return false;
    }
    *outFd = fd;
    return true;
}

bool SafStorage::listDir(const std::string& relDir,
                         std::vector<std::string>& outFiles) const {
    outFiles.clear();
    if (!io_ || !validRelPath(relDir)) {
        return false;
    }
    std::string err, dirUri;
    if (!resolveDir(relDir, false, dirUri, err)) {
        elog::error("saf: listDir %s — %s", relDir.c_str(), err.c_str());
        return false;
    }
    std::vector<SafEntry> kids;
    if (!io_->list(dirUri, kids, err)) {
        elog::error("saf: list %s — %s", relDir.c_str(), err.c_str());
        return false;
    }
    // a interface devolve só FICHEIROS, ordenados (contrato da UI)
    for (const SafEntry& k : kids) {
        if (k.mime != kSafDirMime) {
            outFiles.push_back(k.name);
        }
    }
    std::sort(outFiles.begin(), outFiles.end());
    return true;
}

// ---- 0.8.10: escrita streaming REAL (fd SAF aberto até ao close) -------------
namespace {
std::map<int, int>& safStreams() {
    static std::map<int, int> reg;
    return reg;
}
int safNextHandle() {
    static int next = 2000;
    return next++;
}
} // namespace

int SafStorage::openWriteStream(const std::string& relPath) {
    if (!io_ || !validRelPath(relPath)) {
        return -1;
    }
    std::string err, uri;
    if (!resolveFile(relPath, true, uri, err)) {
        elog::error("saf: writeStream %s — %s", relPath.c_str(), err.c_str());
        return -1;
    }
    int fd = -1;
    if (!io_->openFd(uri, "wt", &fd, err)) {
        elog::error("saf: writeStream open(wt) %s — %s", relPath.c_str(),
                    err.c_str());
        return -1;
    }
    const int h = safNextHandle();
    safStreams()[h] = fd;
    return h;
}

bool SafStorage::writeStreamChunk(int handle, const void* data, size_t n) {
    auto& reg = safStreams();
    const auto it = reg.find(handle);
    if (it == reg.end()) {
        return false;
    }
    return writeAllFd(it->second, data, n);
}

void SafStorage::closeWriteStream(int handle) {
    auto& reg = safStreams();
    const auto it = reg.find(handle);
    if (it == reg.end()) {
        return;
    }
    ::close(it->second);   // o provider persiste no close do fd
    reg.erase(it);
}

bool SafStorage::remove(const std::string& relPath) {
    if (!io_ || !validRelPath(relPath)) {
        return false;
    }
    std::string err, uri;
    if (!resolveFile(relPath, false, uri, err)) {
        elog::error("saf: remove %s — %s", relPath.c_str(), err.c_str());
        return false;
    }
    if (!io_->remove(uri, err)) {
        elog::error("saf: remove(fd) %s — %s", relPath.c_str(), err.c_str());
        return false;
    }
    return true;
}

} // namespace vv
