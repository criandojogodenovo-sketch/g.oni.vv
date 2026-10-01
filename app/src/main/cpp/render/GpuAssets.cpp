// render/GpuAssets.cpp — 1 ref → 1 objeto GL (F5-E; texturas F5.1-A).
#include "render/GpuAssets.h"
#include "assets/TextureCompressor.h"
#include "assets/TexturePipeline.h"
#include "platform/Log.h"
#include "render/Mesh.h"
#include "render/Texture.h"
#include <cctype>

namespace vv {

namespace {

// 0.8.5: extensão minúscula (a classificação local do fallback — o
// ResourceManager tem a sua; aqui só interessa gltf/glb)
std::string lowerExtOf(const std::string& path) {
    const size_t dot = path.rfind('.');
    if (dot == std::string::npos) {
        return "";
    }
    std::string e = path.substr(dot + 1);
    for (char& c : e) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return e;
}

} // namespace

Mesh* GpuAssets::mesh(const std::string& ref) {
    if (!rm_) {
        return nullptr;
    }
    const auto it = gpuMeshes_.find(ref);
    if (it != gpuMeshes_.end()) {
        return it->second.get();   // MESMO objeto GL — sem duplicar GPU
    }
    std::string err;
    const MeshData* data = rm_->mesh(ref, err);
    if (!data) {
        // 0.8.5 (fix do "import não aplica" no device): glTF/GLB SEM '#'
        // com VÁRIOS meshes falhava no ResourceManager ("use path#<i>") —
        // mas o browser/catálogo/aplicar nunca geram sub-refs. WIRING: o
        // fallback do APPLY é mesh #0 (o contrato do ResourceManager fica
        // intacto p/ refs explícitas; o apply do device deixa de morrer).
        const size_t hash = ref.find('#');
        const std::string ext = lowerExtOf(ref);
        if (hash == std::string::npos && (ext == "gltf" || ext == "glb")) {
            data = rm_->mesh(ref + "#0", err);
            if (data) {
                LOGI("GpuAssets: '%s' tem múltiplos meshes — a aplicar o #0 "
                     "(use '#<i>' no .goni para outro)", ref.c_str());
            }
        }
    }
    if (!data || !data->ok()) {
        return nullptr;
    }
    auto m = std::make_unique<Mesh>();
    // 0.8.2 (F7): MeshData com skin → createSkinned (aJoints/aWeights nas
    // locations 3/4); estático → create de sempre (comportamento idêntico)
    if (data->skinned()) {
        if (!m->createSkinned(data->vertices.data(),
                              static_cast<u32>(data->vertices.size()),
                              data->indices.data(),
                              static_cast<u32>(data->indices.size()),
                              data->skinJoints.data(),
                              data->skinWeights.data())) {
            return nullptr;
        }
    } else if (!m->create(data->vertices.data(),
                          static_cast<u32>(data->vertices.size()),
                          data->indices.data(),
                          static_cast<u32>(data->indices.size()))) {
        return nullptr;   // upload falhou — sem cache de objeto quebrado
    }
    Mesh* raw = m.get();
    gpuMeshes_.emplace(ref, std::move(m));
    return raw;
}

const Texture* GpuAssets::texture(const std::string& relPath, std::string* warn) {
    if (!rm_) {
        return nullptr;
    }
    const auto it = gpuTextures_.find(relPath);
    if (it != gpuTextures_.end()) {
        if (warn) {
            warn->clear();   // do cache não repete o aviso do gate
        }
        return it->second.get();
    }

    // F5.1-A: caminho canônico — PNG bytes → pipeline (cache disco +
    // compressão ETC2/ASTC com cadeia de mips em CPU) → glCompressedTexImage2D
    if (pipeline_ && rm_->storage()) {
        std::vector<u8> bytes;
        if (!rm_->storage()->readBytes(relPath, bytes) || bytes.empty()) {
            LOGE("GpuAssets: textura não encontrada: %s", relPath.c_str());
            return nullptr;
        }
        CompressedImage comp;
        TextureLoadInfo info;
        std::string perr;
        if (!pipeline_->process(bytes.data(), bytes.size(), relPath.c_str(),
                                comp, info, perr)) {
            LOGE("GpuAssets: textura %s falhou: %s", relPath.c_str(),
                 perr.c_str());
            return nullptr;
        }
        if (warn && !info.warn.empty()) {
            *warn = info.warn;
        }
        LOGI("GpuAssets: textura %s %ux%u %s via %s%s", relPath.c_str(),
             comp.width, comp.height, formatName(comp.format), info.via,
             info.warn.empty() ? "" : info.warn.c_str());
        auto t = std::make_unique<Texture>();
        if (!t->createFromCompressed(comp)) {
            return nullptr;
        }
        Texture* raw = t.get();
        gpuTextures_.emplace(relPath, std::move(t));
        return raw;
    }

    // legacy F5 (sem pipeline): RawImage → passthrough → GL
    std::string err;
    const RawImage* img = rm_->image(relPath, err, warn);
    if (!img || !img->ok()) {
        return nullptr;
    }
    static PassthroughCompressor kCompressor;
    CompressedImage comp;
    std::string cerr2;
    if (!kCompressor.compress(*img, comp, cerr2)) {
        return nullptr;
    }
    auto t = std::make_unique<Texture>();
    if (!t->createFromCompressed(comp)) {
        return nullptr;
    }
    Texture* raw = t.get();
    gpuTextures_.emplace(relPath, std::move(t));
    return raw;
}

void GpuAssets::releaseAll() {
    gpuMeshes_.clear();
    gpuTextures_.clear();
}

} // namespace vv
