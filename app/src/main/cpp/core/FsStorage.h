#pragma once
// core/FsStorage.h — ProjectStorage sobre ficheiros POSIX (F5-A).
//
// IMPLEMENTAÇÃO DEFAULT da interface: no device o main injeta como raiz
// app->activity->externalDataPath (= getExternalFilesDir(), sem permissões
// desde a API 19; fallback = internalDataPath); nos testes do CI, um
// diretório em /tmp. SAF (F5.2) será OUTRA implementação da mesma interface.
//
// makeDirs tem semântica mkdir -p; listDir devolve só FICHEIROS (nomes,
// ordenados — a UI dos seletores conta com esta ordem); toda operação
// valida o rel-path (validRelPath) e recusa traversal.
#include <string>
#include <vector>
#include "core/ProjectStorage.h"

namespace vv {

class FsStorage final : public ProjectStorage {
public:
    // Raiz REAL do projeto (criada a pedido pelas operações de escrita).
    explicit FsStorage(std::string rootPath);

    std::string root() const override { return root_; }
    bool makeDirs(const std::string& relDir) override;
    bool exists(const std::string& relPath) const override;
    // F5.4-hotfix: tri-estado (ENOENT → Absent; outro errno → Unknown)
    Presence probe(const std::string& relPath) const override;
    bool writeText(const std::string& relPath, const std::string& text) override;
    bool readText(const std::string& relPath, std::string& out) const override;
    bool writeBytes(const std::string& relPath, const void* data, size_t n) override;
    bool readBytes(const std::string& relPath, std::vector<u8>& out) const override;
    // 0.8.10 — escrita STREAMING real (FILE* aberto até ao close)
    int  openWriteStream(const std::string& relPath) override;
    bool writeStreamChunk(int handle, const void* data, size_t n) override;
    void closeWriteStream(int handle) override;
    // 0.8.10 — remove ficheiro (setting "largar a fonte")
    bool remove(const std::string& relPath) override;
    bool listDir(const std::string& relDir,
                 std::vector<std::string>& outFiles) const override;

    // 0.9.6 (G6 · R-017): stat real por ficheiro (o bench do relatório)
    bool statBytes(const std::string& relPath, u64& outBytes) const override;

    // 0.10-M (PASSO 3B): o range real (fopen + fseek + fread exatos) — o
    // espião do guard do load e a semente do load por blocos do PASSO 4
    bool readBytesAt(const std::string& relPath, u64 offset, size_t len,
                     std::vector<u8>& out) const override;

    // 0.10-M (SAF-STREAM): o fd REAL (::open) p/ o mmap POR FD do streaming
    // (o 2.º degrau da cascata — o mmap por caminho é o de sempre)
    bool openReadFd(const std::string& relPath, int* outFd,
                    std::string& err) override;

private:
    std::string root_;
};

} // namespace vv
