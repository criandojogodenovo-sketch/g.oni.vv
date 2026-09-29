// core/SafStorage.cpp — SAF sobre backend injetável (F5.1-C). GL-free.
//
// Guardas idênticas às do FsStorage: TODO rel-path passa por validRelPath
// antes de tocar o backend (traversal '../../secret' nunca sai da árvore).
#include "core/SafStorage.h"

namespace vv {

SafStorage::SafStorage(SafBackend* backend, std::string treeUri)
    : backend_(backend), treeUri_(std::move(treeUri)) {}

bool SafStorage::makeDirs(const std::string& relDir) {
    if (!backend_ || !validRelPath(relDir)) {
        return false;
    }
    return backend_->makeDirs(treeUri_, relDir);
}

bool SafStorage::exists(const std::string& relPath) const {
    if (!backend_ || !validRelPath(relPath)) {
        return false;
    }
    return backend_->exists(treeUri_, relPath);
}

bool SafStorage::writeText(const std::string& relPath, const std::string& text) {
    if (!backend_ || !validRelPath(relPath)) {
        return false;
    }
    // paridade com FsStorage: cria o diretório-pai do ficheiro
    const size_t slash = relPath.rfind('/');
    if (slash != std::string::npos &&
        !backend_->makeDirs(treeUri_, relPath.substr(0, slash))) {
        return false;
    }
    return backend_->writeBytes(treeUri_, relPath, text.data(), text.size());
}

bool SafStorage::readText(const std::string& relPath, std::string& out) const {
    if (!backend_ || !validRelPath(relPath)) {
        return false;
    }
    std::vector<u8> bytes;
    if (!backend_->readBytes(treeUri_, relPath, bytes)) {
        return false;
    }
    out.assign(bytes.begin(), bytes.end());
    return true;
}

bool SafStorage::writeBytes(const std::string& relPath, const void* data,
                            size_t n) {
    if (!backend_ || !validRelPath(relPath)) {
        return false;
    }
    const size_t slash = relPath.rfind('/');
    if (slash != std::string::npos &&
        !backend_->makeDirs(treeUri_, relPath.substr(0, slash))) {
        return false;
    }
    return backend_->writeBytes(treeUri_, relPath, data, n);
}

bool SafStorage::readBytes(const std::string& relPath,
                           std::vector<u8>& out) const {
    if (!backend_ || !validRelPath(relPath)) {
        return false;
    }
    return backend_->readBytes(treeUri_, relPath, out);
}

bool SafStorage::listDir(const std::string& relDir,
                         std::vector<std::string>& outFiles) const {
    outFiles.clear();
    if (!backend_ || !validRelPath(relDir)) {
        return false;
    }
    return backend_->listFiles(treeUri_, relDir, outFiles);
}

} // namespace vv
