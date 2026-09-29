#pragma once
// assets/ResourceManager.h — cache de assets em CPU por caminho relativo (F5).
//
// KEYS do cache = a MESMA string que o projeto/serializer usam como ref:
//   "meshes/quad.obj"        — OBJ direto (parse único)
//   "meshes/model.gltf"      — glTF com UM mesh (uso direto)
//   "meshes/model.gltf#1"    — glTF/GLB com vários meshes (sub-ref por índice)
//   "meshes/model.glb#0"     — idem, container binário
//
// CONTRATO (testado no CI):
//   • mesh(ref) carrega UMA vez; chamadas seguintes devolvem o MESMO
//     ponteiro SEM reler (meshLoads() não muda) → nada duplica.
//   • O modelo .gltf/.glb inteiro fica em cache (models_); os refs "#i"
//     apontam para dentro dele (aliasing shared_ptr) — sem cópias.
//   • Buffers externos do .gltf (uri "scene.bin") resolvem RELATIVO à pasta
//     do .gltf, pelo mesmo ProjectStorage.
//   • adoptMesh registra MeshData já em memória (sem storage).
//   • releaseMesh liberta UM ref; releaseAll zera tudo. Ponteiros devolvidos
//     antes do release ficam INVÁLIDOS (cache é dona).
//
// A dedup de memória GPU é a segunda metade do contrato e vive no device
// (render/GpuAssets): 1 ref → 1 MeshData (aqui) → 1 objeto GL (lá).
//
// GL-free / Android-free: CI Linux testa o cache inteiro.
#include <memory>
#include <string>
#include <unordered_map>
#include "assets/Assets.h"
#include "assets/GltfImporter.h"
#include "core/ProjectStorage.h"

namespace vv {

class ResourceManager {
public:
    // storage não-dono (injeção: mesh agora, image na F5-D)
    void setStorage(ProjectStorage* st) { storage_ = st; }
    ProjectStorage* storage() const { return storage_; }

    // .obj/.gltf/.glb por extensão (minúscula). Devolve nullptr e preenche
    // `err` quando falha (ausente, parse inválido, formato, ref #i fora).
    const MeshData* mesh(const std::string& ref, std::string& err);

    // registra MeshData já em memória sob um ref (substitui se existir)
    void adoptMesh(const std::string& ref, MeshData&& data);

    bool hasMesh(const std::string& ref) const;

    void releaseMesh(const std::string& ref);
    void releaseAll();

    // telemetria (testes + status line do editor)
    u32 meshCount() const { return static_cast<u32>(meshes_.size()); }
    u32 modelCount() const { return static_cast<u32>(models_.size()); }
    u32 meshLoads() const { return meshLoads_; }   // parses reais de ficheiro

private:
    // parse de .gltf/.glb com cache do MODELO (path sem '#')
    std::shared_ptr<const GltfModel> loadModel(const std::string& path,
                                               std::string& err);

    ProjectStorage* storage_ = nullptr;
    std::unordered_map<std::string, std::shared_ptr<const MeshData>> meshes_;
    std::unordered_map<std::string, std::shared_ptr<const GltfModel>> models_;
    u32 meshLoads_ = 0;
};

} // namespace vv
