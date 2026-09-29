// assets/ResourceManager.cpp — cache de assets em CPU (F5).
#include "assets/ResourceManager.h"
#include "assets/ObjImporter.h"
#include <cstring>

namespace vv {

namespace {

// extensão minúscula do último '.' do caminho ("" se não há)
std::string lowerExt(const std::string& path) {
    const size_t dot = path.rfind('.');
    if (dot == std::string::npos) {
        return "";
    }
    std::string ext = path.substr(dot + 1);
    for (char& c : ext) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return ext;
}

} // namespace

const MeshData* ResourceManager::mesh(const std::string& relPath, std::string& err) {
    err.clear();
    const auto it = meshes_.find(relPath);
    if (it != meshes_.end()) {
        return &it->second;   // cache hit — sem I/O, sem re-parse
    }
    if (!storage_) {
        err = "sem storage ligado ao ResourceManager";
        return nullptr;
    }
    const std::string ext = lowerExt(relPath);
    if (ext != "obj") {
        // F5-C adiciona gltf/glb aqui (mesmo ponto de despacho)
        err = "formato de mesh não suportado: ." + ext;
        return nullptr;
    }
    std::string text;
    if (!storage_->readText(relPath, text)) {
        err = "ficheiro não encontrado: " + relPath;
        return nullptr;
    }
    MeshData data;
    if (!parseObj(text.data(), text.size(), data, err)) {
        err = "OBJ inválido (" + relPath + "): " + err;
        return nullptr;
    }
    data.name = relPath;
    ++meshLoads_;
    const auto res = meshes_.emplace(relPath, std::move(data));
    return &res.first->second;
}

void ResourceManager::adoptMesh(const std::string& relPath, MeshData&& data) {
    const auto it = meshes_.find(relPath);
    if (it != meshes_.end()) {
        it->second = std::move(data);
        return;
    }
    meshes_.emplace(relPath, std::move(data));
}

bool ResourceManager::hasMesh(const std::string& relPath) const {
    return meshes_.count(relPath) != 0;
}

void ResourceManager::releaseMesh(const std::string& relPath) {
    meshes_.erase(relPath);
}

void ResourceManager::releaseAll() {
    meshes_.clear();
    meshLoads_ = 0;   // estatística acompanha o ciclo de vida do cache
}

} // namespace vv
