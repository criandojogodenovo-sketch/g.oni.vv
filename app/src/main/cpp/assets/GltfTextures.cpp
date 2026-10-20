// assets/GltfTextures.cpp — extração/dedup de texturas embutidas (F5.1-B).
// 0.12-Y (3.Y-C): os 5 SLOTS do material glTF (baseColor/normal/
// metalRough/emissive/occlusion — o parse já lia os índices, a extração
// só levava o baseColor) + a PASTA POR MODELO: os ficheiros vão para
// assets/<stem>/textures/ (a ordem do dono: «cria a pasta
// assets/NomeDoModelo/ e coloca lá o .gmesh e uma subpasta textures/»;
// os imports antigos na raiz textures/ continuam a carregar — os
// rel-paths guardados nos .goni ficam intactos).
#include <string>
#include <unordered_map>
#include <vector>
#include "assets/GltfTextures.h"
#include "assets/PngLoader.h"
#include "assets/TextureCache.h"   // hashBytes (FNV-1a 64 partilhado)
#include "platform/EngineLog.h"   // 0.12-Y-fix: o LOG do material que o dono lê

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
            // 0.12-Y-fix: o SKIP nunca mais é silencioso (o modelo branco do
            // dono tinha de dizer O QUE aconteceu — o jpeg é a causa comum)
            elog::warn("[material] textura IGNORADA (mesh %zu): mime '%s' "
                       "fora do escopo — o slot fica vazio",
                       m, img.mime.c_str());
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
        // 0.12-Y-fix (a ordem do dono): o par NOME -> CAMINHO no engine.log
        elog::info("[material] textura extraida: %s -> %s",
                   rel.substr(rel.find_last_of('/') + 1).c_str(), rel.c_str());
    }
    return true;
}

// ---- 0.12-Y (3.Y-C): OS 5 SLOTS + A PASTA DO MODELO ------------------------
// A mesma dedup FNV-1a, mas (a) TODOS os slots do material (o parse já
// lia os 5 índices — GltfMaterial::baseColorTex/normalTex/metalRoughTex/
// emissiveTex/occlusionTex) e (b) os ficheiros em
// assets/<stem>/textures/ (a ordem do dono). `outSlots` fica
// model.meshes.size() × 5 ("" = o slot vazio — o mesmo contrato do
// outMeshTex). O modelo SEM nome usa "modelo" (a pasta nunca é a raiz).
bool extractGltfTexSlots(ProjectStorage& st, const GltfModel& model,
                         const std::string& modelStem,
                         GltfTexSlots& outSlots, std::string& err) {
    err.clear();
    outSlots.meshes = model.meshes.size();
    outSlots.tex.assign(outSlots.meshes * kBindSlotCount,
                        std::string());
    const std::string dir =
        std::string("assets/") +
        (modelStem.empty() ? std::string("modelo") : modelStem) +
        "/textures";
    // dedup GLOBAL ao modelo: 1 ficheiro por textura única (o hash é do
    // BYTES — o mesmo contrato do extractGltfTextures)
    std::unordered_map<u64, std::string> dedup;
    for (size_t m = 0; m < model.meshes.size(); ++m) {
        const i32 matIdx =
            m < model.meshMaterial.size() ? model.meshMaterial[m] : -1;
        if (matIdx < 0 ||
            matIdx >= static_cast<i32>(model.materials.size())) {
            continue;
        }
        const GltfMaterial& mat = model.materials[static_cast<size_t>(matIdx)];
        const i32 slots[5] = {mat.baseColorTex, mat.normalTex,
                              mat.metalRoughTex, mat.emissiveTex,
                              mat.occlusionTex};
        for (int s = 0; s < 5; ++s) {
            const i32 imgIdx = slots[s];
            if (imgIdx < 0 ||
                imgIdx >= static_cast<i32>(model.images.size())) {
                continue;
            }
            const GltfImage& img = model.images[static_cast<size_t>(imgIdx)];
            if (img.bytes.empty()) {
                // externa (uri relativo seguro) → pass-through APONTANDO a
                // pasta do modelo (o uri fica RELATIVO ao .gltf original;
                // copiar o ficheiro é do chamador — o contrato da F5)
                if (!img.uriPath.empty() && validRelPath(img.uriPath)) {
                    outSlots.tex[m * 5 + static_cast<size_t>(s)] = img.uriPath;
                }
                continue;
            }
            if (img.mime != "image/png") {
                // 0.12-Y-fix: o skip do slot NUNCA silencioso (o dono lê o
                // engine.log e sabe PORQUE é que o modelo ficou branco)
                elog::warn("[material] slot %s IGNORADO (mesh %zu): mime "
                           "'%s' fora do escopo",
                           kBindSlotName(static_cast<u8>(s)), m,
                           img.mime.c_str());
                continue;   // jpeg/fora do escopo — sem textura, sem falha
            }
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
                outSlots.tex[m * 5 + static_cast<size_t>(s)] = it->second;
                continue;
            }
            char nm[64];
            std::snprintf(nm, sizeof(nm), "gltf_%016llx.png",
                          static_cast<unsigned long long>(hash));
            const std::string rel = dir + "/" + nm;
            if (!st.exists(rel)) {
                if (!st.makeDirs(dir) ||
                    !st.writeBytes(rel, img.bytes.data(), img.bytes.size())) {
                    err = "glTF: falha ao escrever " + rel;
                    continue;
                }
            }
            dedup.emplace(hash, rel);
            outSlots.tex[m * 5 + static_cast<size_t>(s)] = rel;
            // 0.12-Y-fix (a ordem do dono, LITERAL): «[material] textura
            // extraida: <nome> -> <caminho>» — o slot vai no prefixo
            elog::info("[material] textura extraida: %s -> %s (slot %s)",
                       nm, rel.c_str(),
                       kBindSlotName(static_cast<u8>(s)));
        }
    }
    return true;
}

} // namespace vv
