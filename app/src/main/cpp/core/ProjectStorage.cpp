// core/ProjectStorage.cpp — guarda de caminhos relativos (F5-A).
//
// Implementações de ProjectStorage chamam validRelPath ANTES de tocar o FS:
// o manifesto e os assets só referenciam caminhos dentro da raiz do projeto
// (defesa contra traversal — '../../secret' nunca sai da pasta do jogo).
#include "core/ProjectStorage.h"

#include <map>
#include <vector>

namespace vv {

bool validRelPath(const std::string& rel) {
    if (rel.empty()) {
        return false;
    }
    if (rel.front() == '/' || rel.back() == '/') {
        return false;
    }
    std::string seg;
    const auto checkSeg = [&seg]() {
        return !seg.empty() && seg != "." && seg != "..";
    };
    for (const char c : rel) {
        if (c == '\\') {
            return false;   // só '/' como separador (refs portáveis)
        }
        if (c == '/') {
            if (!checkSeg()) {
                return false;
            }
            seg.clear();
            continue;
        }
        seg.push_back(c);
    }
    return checkSeg();
}

std::string joinRelPath(const std::string& root, const std::string& rel) {
    if (!validRelPath(rel)) {
        return "";
    }
    if (root.empty()) {
        return rel;
    }
    return root + "/" + rel;
}

// ---- 0.8.10: default de escrita streaming (acumula com teto) ----------------
// Implementações com fd/FILE* reais (FsStorage/SafStorage) sobrepõem-no.
// O teto protege a promessa "nunca o ficheiro inteiro em RAM" mesmo no
// caminho default: além de 256 MB devolve false (erro legível no chamador).
namespace {
struct AccumStream {
    std::string rel;
    std::vector<vv::u8> bytes;
    bool failed = false;
};
std::map<int, AccumStream>& accumRegistry() {
    static std::map<int, AccumStream> reg;
    return reg;
}
int nextStreamHandle() {
    static int next = 1;
    return next++;
}
} // namespace

int ProjectStorage::openWriteStream(const std::string& relPath) {
    if (!validRelPath(relPath)) {
        return -1;
    }
    auto& reg = accumRegistry();
    const int h = nextStreamHandle();
    reg[h] = AccumStream{};
    reg[h].rel = relPath;
    return h;
}

bool ProjectStorage::writeStreamChunk(int handle, const void* data, size_t n) {
    auto& reg = accumRegistry();
    const auto it = reg.find(handle);
    if (it == reg.end() || it->second.failed) {
        return false;
    }
    if (it->second.bytes.size() + n > kStreamAccumMax) {
        it->second.failed = true;
        return false;   // teto do acumulador — erro legível no chamador
    }
    const vv::u8* p = static_cast<const vv::u8*>(data);
    it->second.bytes.insert(it->second.bytes.end(), p, p + n);
    return true;
}

void ProjectStorage::closeWriteStream(int handle) {
    auto& reg = accumRegistry();
    const auto it = reg.find(handle);
    if (it == reg.end()) {
        return;
    }
    if (!it->second.failed) {
        writeBytes(it->second.rel, it->second.bytes.data(),
                   it->second.bytes.size());
    }
    reg.erase(it);
}

bool ProjectStorage::remove(const std::string&) {
    return false;   // default honesto: esta implementação não remove
}

bool ProjectStorage::readBytesAt(const std::string& relPath, u64 offset,
                                 size_t len, std::vector<u8>& out) const {
    // 0.10-M (PASSO 3B) — o DEFAULT correto (não poupado): lê o ficheiro
    // INTEIRO e devolve a fatia. As implementações reais (Fs/Saf/Fake)
    // sobrepõem com a leitura exata do range — este default só existe para
    // que uma storage futura nunca minta (false) sobre algo que SABE ler.
    out.clear();
    if (len == 0) {
        return true;   // zero bytes pedidos = zero bytes dados
    }
    std::vector<u8> all;
    if (!readBytes(relPath, all)) {
        return false;
    }
    if (offset >= all.size() || all.size() - offset < len) {
        return false;   // além do fim — range inválido
    }
    out.assign(all.begin() + static_cast<long>(offset),
               all.begin() + static_cast<long>(offset + len));
    return true;
}

bool ProjectStorage::openReadFd(const std::string&, int* outFd,
                                std::string& err) {
    // 0.10-M (SAF-STREAM) — default honesto: este storage não entrega fds
    // de leitura (o readBytesAt é a via). Quem precisa do mmap por fd usa
    // as implementações que o suportam (Fs/Saf).
    if (outFd != nullptr) {
        *outFd = -1;
    }
    err = "este armazenamento não fornece fds de leitura (o caminho/ranges "
          "cobrem)";
    return false;
}

} // namespace vv

