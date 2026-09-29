// render/GpuAssets.cpp — 1 ref → 1 objeto GL (F5-E, device).
#include "render/GpuAssets.h"
#include "assets/TextureCompressor.h"
#include "render/Mesh.h"
#include "render/Texture.h"

namespace vv {

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
    if (!data || !data->ok()) {
        return nullptr;
    }
    auto m = std::make_unique<Mesh>();
    if (!m->create(data->vertices.data(),
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
    std::string err;
    const RawImage* img = rm_->image(relPath, err, warn);
    if (!img || !img->ok()) {
        return nullptr;
    }
    // caminho canônico F5: RawImage → passthrough → GL (F5.1 troca aqui)
    static PassthroughCompressor kCompressor;
    CompressedImage comp;
    std::string cerr;
    if (!kCompressor.compress(*img, comp, cerr)) {
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
