#pragma once
// assets/ResourceManager.h — cache de assets em CPU por caminho relativo (F5).
//
// Regras do cache (contrato testado no CI):
//   • mesh("meshes/a.obj") carrega UMA vez; chamadas seguintes devolvem o
//     MESMO ponteiro SEM reler (meshLoads() não muda) → nada duplica.
//   • adoptMesh registra um MeshData já em memória (sem storage — usado
//     por testes e por quem já tem os bytes).
//   • releaseMesh/releaseImage libertam UM caminho; releaseAll zera tudo.
//     Ponteiros devolvidos antes do release ficam INVÁLIDOS (cache é dona).
//
// A dedup de memória GPU é a segunda metade do contrato e vive no device
// (render/GpuAssets): 1 caminho → 1 MeshData (aqui) → 1 objeto GL (lá).
//
// GL-free / Android-free: CI Linux testa o cache inteiro.
#include <string>
#include <unordered_map>
#include "assets/Assets.h"
#include "core/ProjectStorage.h"

namespace vv {

class ResourceManager {
public:
    // storage não-dono (injeção por sub-fase: mesh agora, image na F5-D)
    void setStorage(ProjectStorage* st) { storage_ = st; }
    ProjectStorage* storage() const { return storage_; }

    // .obj → parse; glTF/GLB entram na F5-C; extensão desconhecida → err.
    // Devolve nullptr e preenche `err` quando falha (ficheiro ausente, parse
    // inválido, formato não suportado).
    const MeshData* mesh(const std::string& relPath, std::string& err);

    // registra MeshData já em memória sob um caminho relativo (substitui se
    // o caminho já existir no cache)
    void adoptMesh(const std::string& relPath, MeshData&& data);

    bool hasMesh(const std::string& relPath) const;

    void releaseMesh(const std::string& relPath);
    void releaseAll();

    // telemetria (testes + status line do editor)
    u32 meshCount() const { return static_cast<u32>(meshes_.size()); }
    u32 meshLoads() const { return meshLoads_; }   // cargas reais do storage

private:
    ProjectStorage* storage_ = nullptr;
    std::unordered_map<std::string, MeshData> meshes_;
    u32 meshLoads_ = 0;
};

} // namespace vv
