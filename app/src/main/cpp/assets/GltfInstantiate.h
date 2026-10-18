#pragma once
// assets/GltfInstantiate.h — GltfModel → TICs com hierarquia (F5-C).
//
// Um TIC por NÓ do glTF (vazios incluídos — a hierarquia entra fiel; o
// dono pode apagar na mão o que não quiser). Nós com mesh ganham
// MeshRenderer com:
//   meshPath = "<basePath>#<meshOriginal>"   (ex.: "meshes/robot.gltf#1")
//   mesh     = ctx.bindMesh(user, meshPath)  — device liga o Mesh* do
//              ResourceManager/GpuAssets; testes usam sentinelas. nullptr
//              NÃO aborta: o TIC entra sem mesh (rebind depois no reload).
//   material = ctx.material quando o mesh ligou (lit partilhado do engine).
// TRS do nó → Transform3D (translation/rotation/scale) + updateWorld.
//
// 0.10-M (EXT) — gltfExpandInstantiate: o mesmo contrato para o IMPORT
// EXPANDIDO (BACKLOG 0.10-M-ext feito): os registros NodeOut que o
// conversor das PEÇAS (convertGltfToV3Nodes) deixou no Output viram TICs
// — um por NÓ COM MESH, nomes do glTF preservados, a transformação do nó
// no Transform3D e o MeshRenderer com a ref DA PEÇA. FLAT (sem pais):
// o NodeOut JÁ traz a transformação MUNDO decomposta em TRS — o engine
// compõe flat (TransformSystem não resolve pais) e a peça desenha no
// sítio EXATO sem hierarquia; a geometria da peça ficou CRUA no espaço
// local do nó (o bake é a identidade).
//
// GL-free: testável no CI Linux (bindMesh com sentinelas).
#include <string>
#include <vector>
#include "assets/AssetConverter.h"   // 0.10-M (EXT): convert::Output::NodeOut
#include "assets/GltfImporter.h"
#include "core/Handle.h"
#include "render/Material.h"   // Material = alias de LitMaterial (não admite fwd-decl)

namespace vv {

class Scene;
class Mesh;

struct GltfInstantiateCtx {
    Mesh* (*bindMesh)(void* user, const std::string& ref) = nullptr;
    void* user = nullptr;
    Material* material = nullptr;   // lit partilhado dos meshes importados
};

// Cria os TICs; devolve o handle do 1º nó (Handle::invalid() se o modelo
// não tem nós). Referências cruzadas (filho antes do pai no array) são
// tratadas — a criação é em 2 passadas.
Handle gltfInstantiate(Scene& scene, const GltfModel& model,
                       const std::string& basePath,
                       const GltfInstantiateCtx& ctx);

// 0.10-M (EXT) — cria os TICs do import EXPANDIDO (um por nó com mesh).
// `nodes` = Output::expandNodes do conversor (a ordem dos nós do glTF);
// `refs` = Output::meshes (nodes[i].mesh indexa ESTE vetor). O bindMesh
// injetado recebe a REF DA PEÇA — no device é o GpuAssets::blockHull (a
// peça abre POR BLOCOS pelo kGmeshV3FlagPiece: culling por TIC + lazy +
// LRU, o MESMO caminho do modelo grande do PASSO 4). Devolve os HANDLES
// criados NA ORDEM dos registros (vazio se `nodes` vazio ou sem peça
// válida — o pino do dono conta ISTO: o city expandido = 72). O TIC SEM
// bind (nullptr) entra na mesma com o meshPath — o reload re-liga.
// 0.10-M HOTFIX (e/f): a SUB-ÁRVORE é REAL — parentRec >= 0 pendura o TIC
// no ancestral (TRS LOCAL nos campos) e os mundos compõem-se parent∘local
// AQUI e no TransformSystem (a dívida «editor polish» paga). parentRec =
// -1 mantém o flat (TRS MUNDO — o comportamento byte a byte de sempre).
std::vector<Handle> gltfExpandInstantiate(
        Scene& scene, const std::vector<convert::Output::NodeOut>& nodes,
        const std::vector<std::string>& refs,
        const GltfInstantiateCtx& ctx);

// 0.10-M HOTFIX (f) — recomps os mundos dos TICs criados pelo expand
// (parentWorld ∘ TRS local pela cadeia dos parentRec; raiz = TRS). O fit
// do import chama DEPOIS de escalar as raízes (os filhos seguem o pai).
void gltfExpandComposeWorlds(
        const std::vector<convert::Output::NodeOut>& nodes,
        const std::vector<Handle>& created, Scene& scene);

// ---------------------------------------------------------------------------
// 0.10-M HOTFIX P0 — A EXPANSÃO EM LOTES (o dono: «expansão assíncrona com
// progresso+cancelamento · lotes fora da thread de UI»; o device travava
// com o expand INTEIRO no frame do finalize — importJobFinish corre na
// thread da UI e fazia criar+configurar+ligar N TICs de UMA vez).
//
// O contrato: `gltfExpandBatchStart` cria TODOS os TICs (a 1.ª passada de
// sempre — barata, sem GL nem I/O) e devolve o job; o chamador corre
// `gltfExpandBatchTick` UMA VEZ POR FRAME no ponto seguro (o MESMO do
// importJobFinish — nunca dentro da submissão GL): cada tick configura no
// MÁXIMO kGltfExpandBatchTics registos (parent+TRS+MeshRenderer+bind).
// O progresso (done/total) alimenta o overlay; o cancelamento limpo
// (`gltfExpandBatchCancel`) destrói os TICs criados — a cena fica COMO
// ESTAVA. O resultado final (todas as ticks + gltfExpandComposeWorlds) é
// o MESMO estado do `gltfExpandInstantiate` de sempre (o one-shot fica
// como wrapper por cima do lote — UMA fonte de verdade; o pin compara).
// O gancho `perPiece` corre DEPOIS de configurar cada peça (o main injeta
// o auto-bind dos slots; os testes contam as chamadas).
// ---------------------------------------------------------------------------
constexpr u32 kGltfExpandBatchTics = 32;   // o lote por frame (a lei)

struct GltfExpandBatch {
    Scene* scene = nullptr;
    const std::vector<convert::Output::NodeOut>* nodes = nullptr;
    const std::vector<std::string>* refs = nullptr;
    const GltfInstantiateCtx* ctx = nullptr;
    std::vector<Handle> created;   // NA ORDEM dos registos (o contrato)
    size_t next = 0;               // o próximo registo a configurar
    u32 done_ = 0;                 // peças configuradas (o progresso)
    u32 total_ = 0;                // peças no total (o progresso)
    bool finished = false;
    bool canceled = false;
    void (*perPiece)(Scene&, Handle, void* user) = nullptr;
    void* perPieceUser = nullptr;

    // 1.ª passada: TODOS os TICs (nomes; ordem dos registos — um pai pode
    // ter índice MAIOR que o filho; os links ficam para as ticks)
    static GltfExpandBatch start(
            Scene& scene,
            const std::vector<convert::Output::NodeOut>& nodes,
            const std::vector<std::string>& refs,
            const GltfInstantiateCtx& ctx);
    // configura O PRÓXIMO LOTE (≤ kGltfExpandBatchTics registos) — o lote
    // É A LEI: nunca tudo num só tick (a mutação M-P0b morre AQUI).
    // Devolve true quando acabou.
    bool tick();
    // o cancelamento LIMPO: destrói os TICs criados (ordem inversa) e
    // fecha o job — zero TIC metade-nascido na cena
    void cancel();
    bool active() const { return !finished && !canceled; }
    u32 done() const { return done_; }
    u32 total() const { return total_; }
    const std::vector<Handle>& handles() const { return created; }
};

} // namespace vv
