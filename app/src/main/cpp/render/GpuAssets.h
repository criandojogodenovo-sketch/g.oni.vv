#pragma once
// render/GpuAssets.h — cache de objetos GL por ref (F5-E, device).
//
// Segunda metade do contrato do ResourceManager: 1 ref relativa → 1
// MeshData (CPU, cache lá) → 1 objeto GL (AQUI). Chamadas repetidas com a
// mesma ref devolvem o MESMO ponteiro — TICs diferentes que usam
// "meshes/x.obj" partilham VBO/VAO e a mesma textura: a memória de GPU NÃO
// duplica (critério de aceitação da F5).
//
// O upload acontece no PRIMEIRO uso (lazy) — cenas carregam mesmo com
// assets pesados; o custo de GL paga-se quando o mesh entra em cena.
#include <memory>
#include <string>
#include <unordered_map>
#include "assets/ResourceManager.h"

namespace vv {

class Mesh;
class Texture;

class GpuAssets {
public:
    // rm não-dono (tem de viver enquanto o GpuAssets viver)
    void init(ResourceManager* rm) { rm_ = rm; }

    // nullptr se a ref não resolve (erro no RM ou upload GL falhou)
    Mesh* mesh(const std::string& ref);

    // aviso (gate 2K) sai só na carga do cache CPU — o chamador mostra em toast
    const Texture* texture(const std::string& relPath, std::string* warn = nullptr);

    void releaseAll();

    // telemetria p/ a status line do editor (objetos GL vivos)
    u32 meshCount() const { return static_cast<u32>(gpuMeshes_.size()); }
    u32 textureCount() const { return static_cast<u32>(gpuTextures_.size()); }

private:
    ResourceManager* rm_ = nullptr;
    std::unordered_map<std::string, std::unique_ptr<Mesh>> gpuMeshes_;
    std::unordered_map<std::string, std::unique_ptr<Texture>> gpuTextures_;
};

} // namespace vv
