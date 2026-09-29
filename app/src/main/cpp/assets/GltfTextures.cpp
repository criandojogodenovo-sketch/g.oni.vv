// assets/GltfTextures.cpp — extração/dedup de texturas embutidas (F5.1-B).
#include <string>
#include <unordered_map>
#include <vector>
#include "assets/GltfTextures.h"
#include "assets/PngLoader.h"
#include "assets/TextureCache.h"   // hashBytes (FNV-1a 64 partilhado)

namespace vv {

std::string gltfTextureRelPath(const std::vector<u8>& pngBytes) {
    const u64 hash =
        TextureCache::hashBytes(pngBytes.data(), pngBytes.size());
    char buf[40];
    std::snprintf(buf, sizeof(buf), "gltf_%016llx.png",
                  static_cast<unsigned long long>(hash));
    return std::string("textures/") + buf;
}

bool extractGltfTextures(ProjectStorage& st, const GltfModel& model,
                         std::vector<std::string>& outMeshTex,
                         std::string& err) {
    err.clear();
    outMeshTex.clear();
    outMeshTex.resize(model.meshes.size());

    // dedup: hash dos bytes → caminho já resolvido (1 ficheiro por textura
    // única, mesmo que N materiais/meshes a referenciem)
    std::unordered_map<u64, std::string> dedup;

    for (size_t m = 0; m < model.meshes.size(); ++m) {
        const i32 matIdx =
            m < model.meshMaterial.size() ? model.meshMaterial[m] : -1;
        if (matIdx < 0 ||
            matIdx >= static_cast<i32>(model.materials.size())) {
            continue;   // mesh sem material/textura
        }
        const GltfMaterial& mat = model.materials[static_cast<size_t>(matIdx)];
        if (mat.baseColorTex < 0 ||
            mat.baseColorTex >= static_cast<i32>(model.images.size())) {
            continue;
        }
        const GltfImage& img = model.images[static_cast<size_t>(mat.baseColorTex)];

        if (img.bytes.empty()) {
            // externa (uri relativo) → pass-through se for um rel-path seguro
            if (!img.uriPath.empty() && validRelPath(img.uriPath)) {
                outMeshTex[m] = img.uriPath;
            }
            continue;
        }
        if (img.mime != "image/png") {
            continue;   // jpeg/unknown — fora do escopo (sem textura, sem falha)
        }
        // valida: os bytes têm de ser um PNG decodificável (lixo nunca entra
        // na pasta textures/)
        RawImage probe;
        std::string perr;
        if (!loadPng(img.bytes.data(), img.bytes.size(), probe, perr)) {
            err = "glTF: textura embutida inválida: " + perr;
            continue;
        }
        const u64 hash =
            TextureCache::hashBytes(img.bytes.data(), img.bytes.size());
        const auto it = dedup.find(hash);
        if (it != dedup.end()) {
            outMeshTex[m] = it->second;   // dedup em memória
            continue;
        }
        const std::string rel = gltfTextureRelPath(img.bytes);
        if (!st.exists(rel)) {
            if (!st.makeDirs("textures") ||
                !st.writeBytes(rel, img.bytes.data(), img.bytes.size())) {
                err = "glTF: falha ao escrever " + rel;
                continue;
            }
        }
        dedup.emplace(hash, rel);
        outMeshTex[m] = rel;
    }
    return true;
}

} // namespace vv
