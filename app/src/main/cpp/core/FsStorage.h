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
    bool listDir(const std::string& relDir,
                 std::vector<std::string>& outFiles) const override;

private:
    std::string root_;
};

} // namespace vv
