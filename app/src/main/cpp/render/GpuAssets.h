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
//
// 0.10-M (PASSO 4) — O CAMINHO DOS BLOCOS: um .gmesh v3 que o mesh único
// RECUSARIA (>65 535 verts ou estimativa > kMeshLoadBudgetBytes) abre por
// aqui como BlockMesh (a TABELA, nunca os dados) e mesh(ref) devolve o
// HULL de bounds — um Mesh* VÁLIDO com o AABB global do meta, para o
// contrato do picker/serializer (fit/cena/bounds SEM carregar nada). O
// render é do BlockMesh (drawTics consulta MeshRenderer::blocks
// PRIMEIRO — o hull nunca desenha). 1 ref = 1 BlockMesh (como 1 ref =
// 1 Mesh): a memória não duplica. TERM_WINDOW: releaseAll apaga tudo —
// o re-open do INIT custa 192 B + tabela (nunca dados; o mesmo contrato
// do re-upload dos Mesh comuns).
#include <memory>
#include <string>
#include <unordered_map>
#include "assets/ResourceManager.h"
#include "render/BlockMesh.h"   // PASSO 4: unique_ptr<BlockMesh> pede o tipo
                                    // COMPLETO nos TUs que instanciam GpuAssets
                                    // (o header é GL-free — o GL vive nos .cpp)
#include "render/Mesh.h"
#include "render/Texture.h"
// (os headers acima são GL-free — o GL vive nos .cpp; o unique_ptr abaixo
// precisa do tipo COMPLETO quando main.cpp instancia o destrutor do cache)

namespace vv {

class TexturePipeline;

class GpuAssets {
public:
    // rm não-dono (tem de viver enquanto o GpuAssets viver)
    void init(ResourceManager* rm) { rm_ = rm; }
    // F5.1-A: pipeline de texturas (cache disco + compressão) — opcional;
    // sem pipeline, o caminho legacy F5 (passthrough) é usado.
    void setPipeline(TexturePipeline* p) { pipeline_ = p; }

    // nullptr se a ref não resolve (erro no RM ou upload GL falhou).
    // Para um .gmesh v3 DE BLOCOS devolve o HULL de bounds (ver acima).
    Mesh* mesh(const std::string& ref);

    // 0.10-M (PASSO 4): o BlockMesh ABERTO desta ref (nullptr se não está
    // aberto — mesh(ref) abre-o; o lookup puro para o drawTics/export).
    BlockMesh* blockMeshIfOpen(const std::string& ref) const;

    // aviso (gate 2K no fallback) sai da carga — o chamador mostra em toast
    const Texture* texture(const std::string& relPath, std::string* warn = nullptr);

    void releaseAll();   // TERM_WINDOW: apaga TUDO (blocks incluídos — o
                         // re-open custa 192 B + tabela, nada de dados)

    // telemetria p/ a status line do editor (objetos GL vivos)
    u32 meshCount() const { return static_cast<u32>(gpuMeshes_.size()); }
    u32 textureCount() const { return static_cast<u32>(gpuTextures_.size()); }
    u32 blockMeshCount() const { return static_cast<u32>(blockMeshes_.size()); }

private:
    // abre (ou encontra) o BlockMesh da ref + cria o hull — o caminho do
    // mesh() quando o peek diz «blocos»
    Mesh* blockHull(const std::string& ref);

    ResourceManager* rm_ = nullptr;
    TexturePipeline* pipeline_ = nullptr;
    std::unordered_map<std::string, std::unique_ptr<Mesh>> gpuMeshes_;
    std::unordered_map<std::string, std::unique_ptr<Texture>> gpuTextures_;
    std::unordered_map<std::string, std::unique_ptr<BlockMesh>> blockMeshes_;
};

} // namespace vv
