#pragma once
// core/SafStorage.h — ProjectStorage sobre SAF (F5.4, Gestor de Projetos).
//
// A MESMA interface que o FsStorage (F5-A) implementou — nada acima dela
// muda: Project::openOrCreate cria manifesto+cenas, o editor grava/lê,
// o ResourceManager resolve assets, o import escreve meshes/textures.
// A diferença é só ONDE os bytes vivem: dentro da pasta que o utilizador
// escolheu no Gestor de Projetos (URI de árvore SAF com permissão
// persistente), em vez do app-private.
//
// RESOLUÇÃO DE CAMINHOS: o native não constrói URIs — caminha pela árvore
// com SafIo::resolveChild/list e GUARDA o URI de cada dir/ficheiro
// resolvido (caches). Escrever num ficheiro existente REUTILIZA o URI
// (create com nome duplicado criaria "nome (1)" — nunca duplicamos).
// makeDirs/escrita criam os diretórios em falta (semântica mkdir -p).
//
// Threading: as operações correm no thread da engine (boot/loop); o SafIo
// por baixo (JniSafIo) anexa o thread à VM por conta própria.
#include <map>
#include <string>
#include <vector>
#include "core/ProjectStorage.h"
#include "platform/SafIo.h"

namespace vv {

class SafStorage final : public ProjectStorage {
public:
    // io_ não é dono (o JniSafIo é um singleton do StorageBridge; nos
    // testes, o FakeSafIo vive no caso)
    SafStorage(storage::SafIo* io, std::string treeUri);

    std::string root() const override { return root_; }   // URI da árvore

    bool makeDirs(const std::string& relDir) override;
    bool exists(const std::string& relPath) const override;
    // F5.4-hotfix: tri-estado — resolveChild sobre o pai; exceção/recusa do
    // provider → Unknown (NUNCA "não existe" por erro alheio)
    Presence probe(const std::string& relPath) const override;
    bool writeText(const std::string& relPath, const std::string& text) override;
    bool readText(const std::string& relPath, std::string& out) const override;
    bool writeBytes(const std::string& relPath, const void* data, size_t n) override;
    bool readBytes(const std::string& relPath, std::vector<u8>& out) const override;

    // 0.10-M (PASSO 3B): o range real (openFd + lseek + read exatos)
    bool readBytesAt(const std::string& relPath, u64 offset, size_t len,
                     std::vector<u8>& out) const override;

    // 0.10-M (SAF-STREAM) — O FD DO BRIDGE: resolve o URI e abre por
    // io_->openFd(uri, "r") — no device é o VvActivity.bridgeOpenFd
    // (ContentResolver.openFileDescriptor + detachFd). O conversor
    // streaming mapeia POR FD (mmap sem caminho); o chamador FECHA.
    bool openReadFd(const std::string& relPath, int* outFd,
                    std::string& err) override;

    // 0.8.10 — escrita STREAMING real (fd SAF aberto até ao close)
    int  openWriteStream(const std::string& relPath) override;
    bool writeStreamChunk(int handle, const void* data, size_t n) override;
    void closeWriteStream(int handle) override;
    // 0.8.10 — remove ficheiro (setting "largar a fonte")
    bool remove(const std::string& relPath) override;
    bool listDir(const std::string& relDir,
                 std::vector<std::string>& outFiles) const override;

private:
    // resolve o URI de um diretório ("" = raiz); createMissing=true cria os
    // segmentos em falta (mkdir -p via createDocument DIR)
    bool resolveDir(const std::string& relDir, bool createMissing,
                    std::string& outUri, std::string& err) const;
    // resolve o URI de um ficheiro; createMissing=true cria no pai
    bool resolveFile(const std::string& relPath, bool createMissing,
                     std::string& outUri, std::string& err) const;
    static const char* mimeForName(const std::string& name);

    storage::SafIo* io_ = nullptr;
    std::string root_;
    mutable std::map<std::string, std::string> dirUris_;   // relDir → docUri
    mutable std::map<std::string, std::string> fileUris_;  // relPath → docUri
};

} // namespace vv
