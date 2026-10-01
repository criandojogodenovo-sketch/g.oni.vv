// assets/ResourceManager.cpp — cache de assets em CPU (F5).
//
// Despacho por extensão: .obj → parseObj; .gltf → JSON + parseGltf (buffers
// externos resolvem pelo storage relativo à pasta do ficheiro); .glb →
// bytes + parseGlb. Refs "path#i" (sub-mesh) fazem aliasing do modelo em
// cache — o parse acontece UMA vez por ficheiro.
#include "assets/ResourceManager.h"
#include "assets/GltfTextures.h"
#include "assets/ObjImporter.h"
#include <cstring>

namespace vv {

namespace {

// extensão minúscula do último '.' ("" se não há)
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

// divide "meshes/model.gltf#1" → ("meshes/model.gltf", 1) / sem '#' → (ref, -1)
void splitSubRef(const std::string& ref, std::string& path, i32& subIndex) {
    const size_t hash = ref.rfind('#');
    if (hash == std::string::npos) {
        path = ref;
        subIndex = -1;
        return;
    }
    path = ref.substr(0, hash);
    const std::string num = ref.substr(hash + 1);
    subIndex = 0;
    bool ok = !num.empty();
    for (const char c : num) {
        if (c < '0' || c > '9') {
            ok = false;
            break;
        }
        subIndex = subIndex * 10 + (c - '0');
    }
    if (!ok) {
        path = ref;
        subIndex = -1;
    }
}

// pasta de um caminho ("meshes/a.gltf" → "meshes"; sem '/' → "")
std::string dirOf(const std::string& path) {
    const size_t slash = path.rfind('/');
    return slash == std::string::npos ? "" : path.substr(0, slash);
}

} // namespace

std::shared_ptr<const GltfModel>
ResourceManager::loadModel(const std::string& path, std::string& err) {
    const auto it = models_.find(path);
    if (it != models_.end()) {
        return it->second;   // modelo já parseado
    }
    if (!storage_) {
        err = "sem storage ligado ao ResourceManager";
        return nullptr;
    }
    const std::string ext = lowerExt(path);
    std::shared_ptr<GltfModel> model = std::make_shared<GltfModel>();

    if (ext == "glb") {
        std::vector<u8> bytes;
        if (!storage_->readBytes(path, bytes) || bytes.empty()) {
            err = "ficheiro não encontrado: " + path;
            return nullptr;
        }
        ++meshLoads_;
        // buffers externos não existem em .glb — sem resolver
        if (!parseGlb(bytes.data(), bytes.size(), {}, *model, err)) {
            err = "GLB inválido (" + path + "): " + err;
            return nullptr;
        }
    } else if (ext == "gltf") {
        std::string text;
        if (!storage_->readText(path, text)) {
            err = "ficheiro não encontrado: " + path;
            return nullptr;
        }
        ++meshLoads_;
        // resolver de buffers externos: uri é relativa à PASTA do .gltf
        struct ResolverCtx {
            ProjectStorage* st;
            std::string baseDir;
        };
        ResolverCtx ctx{storage_, dirOf(path)};
        GltfBufferResolver resolver;
        resolver.fn = [](void* user, const char* uri, std::vector<u8>& out) -> bool {
            ResolverCtx* c = static_cast<ResolverCtx*>(user);
            const std::string rel =
                c->baseDir.empty() ? std::string(uri) : c->baseDir + "/" + uri;
            return c->st->readBytes(rel, out) && !out.empty();
        };
        resolver.user = &ctx;
        if (!parseGltf(text.data(), text.size(), {}, resolver, *model, err)) {
            err = "glTF inválido (" + path + "): " + err;
            return nullptr;
        }
    } else {
        err = "formato de mesh não suportado: ." + ext;
        return nullptr;
    }

    // F5.1-B: extrai texturas embutidas (base64/GLB) para textures/ com
    // dedup por hash — falha de extração NÃO invalida o mesh (err2 anotado
    // no log via err de retorno do chamador... aqui apenas segue sem textura)
    {
        std::vector<std::string> meshTex;
        std::string texErr;
        if (extractGltfTextures(*storage_, *model, meshTex, texErr)) {
            model->meshTexture = std::move(meshTex);
        }
    }

    const auto res = models_.emplace(path, std::move(model));
    return res.first->second;
}

std::shared_ptr<const GltfModel>
ResourceManager::model(const std::string& ref, std::string& err) {
    err.clear();
    std::string path;
    i32 sub = -1;
    splitSubRef(ref, path, sub);
    const std::string ext = lowerExt(path);
    if (ext != "gltf" && ext != "glb") {
        err = "não é gltf/glb: " + path;
        return nullptr;
    }
    // loadModel é o MESMO caminho do mesh() — 1 parse por ficheiro (cache)
    return loadModel(path, err);
}

const MeshData* ResourceManager::mesh(const std::string& ref, std::string& err) {
    err.clear();
    const auto it = meshes_.find(ref);
    if (it != meshes_.end()) {
        return it->second.get();   // cache hit — sem I/O, sem re-parse
    }

    std::string path;
    i32 sub = -1;
    splitSubRef(ref, path, sub);
    const std::string ext = lowerExt(path);

    if (ext == "obj") {
        if (!storage_) {
            err = "sem storage ligado ao ResourceManager";
            return nullptr;
        }
        std::string text;
        if (!storage_->readText(path, text)) {
            err = "ficheiro não encontrado: " + path;
            return nullptr;
        }
        auto data = std::make_shared<MeshData>();
        if (!parseObj(text.data(), text.size(), *data, err)) {
            err = "OBJ inválido (" + path + "): " + err;
            return nullptr;
        }
        data->name = path;
        ++meshLoads_;
        const auto res = meshes_.emplace(ref, std::move(data));
        return res.first->second.get();
    }

    if (ext == "gltf" || ext == "glb") {
        const std::shared_ptr<const GltfModel> model = loadModel(path, err);
        if (!model) {
            return nullptr;
        }
        // sem '#': exige mesh único (uso direto do ficheiro como mesh)
        if (sub < 0) {
            if (model->meshes.size() != 1) {
                err = "glTF tem " + std::to_string(model->meshes.size()) +
                      " meshes — use '" + path + "#<i>'";
                return nullptr;
            }
            sub = 0;
            // ref sem '#' aponta para o mesh 0 (o uso mais comum)
            const auto res = meshes_.emplace(
                ref, std::shared_ptr<const MeshData>(model, &model->meshes[0]));
            return res.first->second.get();
        }
        if (sub >= static_cast<i32>(model->meshes.size())) {
            err = "sub-mesh #" + std::to_string(sub) + " fora do range: " + path;
            return nullptr;
        }
        const auto res = meshes_.emplace(
            ref, std::shared_ptr<const MeshData>(model,
                                                 &model->meshes[static_cast<size_t>(sub)]));
        return res.first->second.get();
    }

    err = "formato de mesh não suportado: ." + ext;
    return nullptr;
}

std::string ResourceManager::meshTextureFor(const std::string& ref) const {
    std::string path;
    i32 sub = -1;
    splitSubRef(ref, path, sub);
    const auto it = models_.find(path);
    if (it == models_.end()) {
        return "";
    }
    const GltfModel& m = *it->second;
    if (sub < 0) {
        sub = m.meshes.size() == 1 ? 0 : -1;   // mesma regra do mesh()
    }
    if (sub < 0 || sub >= static_cast<i32>(m.meshTexture.size())) {
        return "";
    }
    return m.meshTexture[static_cast<size_t>(sub)];
}

void ResourceManager::adoptMesh(const std::string& ref, MeshData&& data) {
    auto ptr = std::make_shared<MeshData>(std::move(data));
    const auto it = meshes_.find(ref);
    if (it != meshes_.end()) {
        it->second = std::move(ptr);
        return;
    }
    meshes_.emplace(ref, std::move(ptr));
}

bool ResourceManager::hasMesh(const std::string& ref) const {
    return meshes_.count(ref) != 0;
}

const RawImage* ResourceManager::image(const std::string& relPath,
                                       std::string& err, std::string* warn) {
    err.clear();
    if (warn) {
        warn->clear();
    }
    const auto it = images_.find(relPath);
    if (it != images_.end()) {
        return it->second.get();   // cache hit — sem I/O
    }
    if (!storage_) {
        err = "sem storage ligado ao ResourceManager";
        return nullptr;
    }
    std::vector<u8> bytes;
    if (!storage_->readBytes(relPath, bytes) || bytes.empty()) {
        err = "ficheiro não encontrado: " + relPath;
        return nullptr;
    }
    auto img = std::make_shared<RawImage>();
    if (!loadPng(bytes.data(), bytes.size(), *img, err)) {
        err = "PNG inválido (" + relPath + "): " + err;
        return nullptr;
    }
    ++imageLoads_;
    // gate 2K na CARGA (o aviso sai só na 1ª vez; do cache não repete)
    std::string gateWarn;
    downscaleTo2K(*img, gateWarn);
    if (warn && !gateWarn.empty()) {
        *warn = gateWarn;
    }
    const auto res = images_.emplace(relPath, std::move(img));
    return res.first->second.get();
}

void ResourceManager::releaseMesh(const std::string& ref) {
    meshes_.erase(ref);
    // se o ref é um modelo glTF/GLB, o modelo inteiro sai do cache
    models_.erase(ref);
}

void ResourceManager::releaseImage(const std::string& relPath) {
    images_.erase(relPath);
}

void ResourceManager::releaseAll() {
    meshes_.clear();
    models_.clear();
    images_.clear();
    meshLoads_ = 0;
    imageLoads_ = 0;   // estatística acompanha o ciclo de vida do cache
}

} // namespace vv
