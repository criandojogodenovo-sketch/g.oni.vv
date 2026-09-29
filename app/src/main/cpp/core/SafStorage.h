#pragma once
// core/SafStorage.h — ProjectStorage sobre SAF (F5.1-C). GL-free.
//
// O acesso ao FS real (DocumentsContract via JNI) vive numa interface
// Backend INJETADA — no device é platform/SafIoJni.cpp; nos testes do CI
// é um fake (o mesmo padrão de FsStorage/ProjectStorage). Assim TODA a
// lógica de caminhos/erros fica testável no hospedeiro:
//
//   validRelPath(rel) → backend.makeDirs/readBytes/... (árvore = treeUri)
//
// Falha do backend → false + sem exceção (o RoutingStorage decide fallback).
#include <memory>
#include <string>
#include <vector>
#include "core/ProjectStorage.h"

namespace vv {

// operações de I/O numa árvore SAF (uma instância por treeUri concedida)
class SafBackend {
public:
    virtual ~SafBackend() = default;
    // listDir: só FICHEIROS (contrato ProjectStorage), ordenados
    virtual bool listFiles(const std::string& treeUri, const std::string& relDir,
                           std::vector<std::string>& out) const = 0;
    virtual bool makeDirs(const std::string& treeUri, const std::string& relDir) = 0;
    virtual bool exists(const std::string& treeUri, const std::string& rel) const = 0;
    virtual bool writeBytes(const std::string& treeUri, const std::string& rel,
                            const void* data, size_t n) = 0;
    virtual bool readBytes(const std::string& treeUri, const std::string& rel,
                           std::vector<u8>& out) const = 0;
};

class SafStorage final : public ProjectStorage {
public:
    // backend não-dono (device: um por app; testes: fake por caso)
    SafStorage(SafBackend* backend, std::string treeUri);

    std::string root() const override { return treeUri_; }
    bool makeDirs(const std::string& relDir) override;
    bool exists(const std::string& relPath) const override;
    bool writeText(const std::string& relPath, const std::string& text) override;
    bool readText(const std::string& relPath, std::string& out) const override;
    bool writeBytes(const std::string& relPath, const void* data, size_t n) override;
    bool readBytes(const std::string& relPath, std::vector<u8>& out) const override;
    bool listDir(const std::string& relDir,
                 std::vector<std::string>& outFiles) const override;

    const std::string& treeUri() const { return treeUri_; }

private:
    SafBackend* backend_ = nullptr;
    std::string treeUri_;
};

// Router entre o storage do app (getExternalFilesDir) e o SAF: usa o SAF
// quando há URI concedida; fallback = primário (cancelou/URI inválida).
class RoutingStorage final : public ProjectStorage {
public:
    // ambos não-donos (podem chegar nulos e ligar depois — bind)
    RoutingStorage(ProjectStorage* primary, ProjectStorage* secondary)
        : primary_(primary), secondary_(secondary) {}

    void bind(ProjectStorage* primary, ProjectStorage* secondary) {
        primary_ = primary;
        secondary_ = secondary;
    }

    void useSecondary(bool on) { onSecondary_ = on; }
    bool usingSecondary() const { return onSecondary_; }
    ProjectStorage* active() const { return onSecondary_ ? secondary_ : primary_; }

    std::string root() const override { return active()->root(); }
    bool makeDirs(const std::string& relDir) override {
        return active()->makeDirs(relDir);
    }
    bool exists(const std::string& relPath) const override {
        return active()->exists(relPath);
    }
    bool writeText(const std::string& relPath, const std::string& text) override {
        return active()->writeText(relPath, text);
    }
    bool readText(const std::string& relPath, std::string& out) const override {
        return active()->readText(relPath, out);
    }
    bool writeBytes(const std::string& relPath, const void* data, size_t n) override {
        return active()->writeBytes(relPath, data, n);
    }
    bool readBytes(const std::string& relPath, std::vector<u8>& out) const override {
        return active()->readBytes(relPath, out);
    }
    bool listDir(const std::string& relDir,
                 std::vector<std::string>& outFiles) const override {
        return active()->listDir(relDir, outFiles);
    }

private:
    ProjectStorage* primary_ = nullptr;
    ProjectStorage* secondary_ = nullptr;
    bool onSecondary_ = false;
};

} // namespace vv
