#pragma once
// components/MeshRenderer.h — desenha um mesh com material (F3).
//
// Ponteiros NÃO-DONOS: Mesh/Material/Texture são recursos de runtime
// partilhados. O serializer guarda a tag "cube" OU as refs relativas
// "meshes/x.obj[#i]" e "textures/y.png" — o loader rebinda os ponteiros
// via LoadCtx (F5-E).
//
// 0.8.0 (F7) — PRIMITIVA PROCEDURAL: `primOn` + `prim` formam a assinatura
// serializada no .goni como "prim". A linha "prim:" do Inspector abre o
// seletor; escolher mesh/asset limpa o prim (UMA fonte de mesh de cada
// vez — sem ambiguidade).
//
// 0.8.10 — TROCA DETERMINÍSTICA SEM CACHE: `mesh` == nullptr com primOn
// significa PEDIDO PENDENTE — o main sobe a geometria no PONTO SEGURO do
// frame (início, antes da submissão) pelo caminho ÚNICO
// gera→valida→upload→self-check→bind, com DEFERRED FREE do mesh anterior
// (começa no início do frame seguinte). `primNeg` = backoff: o upload
// desta assinatura FALHOU (GL exausto/contexto morto) e o rebind por frame
// NÃO insiste (anti retry-storm); um pedido NOVO (pick/params/load) ou um
// INIT_WINDOW limpa a flag. Runtime-only — nunca serializado.
//
// 0.10-M (PASSO 4) — RENDER POR BLOCOS: `blocks` aponta para o BlockMesh
// aberto do `meshPath` (o GpuAssets é o dono; o blockRebind() do main
// sincroniza no PONTO SEGURO de cada frame — a fonte é o meshPath, o
// MESMO contrato do resolveMesh do serializer). Quando `blocks` != null o
// drawTics desenha POR BLOCOS (frustum + lazy + LRU) e o `mesh` (o HULL de
// bounds do GpuAssets) fica para os BOUNDS (cena/fit/gizmo — o AABB
// global do meta) — nunca é desenhado. Runtime-only — nunca serializado.
#include "core/Component.h"
#include "render/Material.h"
#include "render/Primitives.h"
#include <string>
#include <vector>

namespace vv {

class Mesh;      // render/Mesh.h — recurso GL (fwd: manter header GL-free)
class Texture;   // render/Texture.h — idem (F5-D)
class BlockMesh; // render/BlockMesh.h — idem (PASSO 4; fwd p/ GL-free)

class MeshRenderer : public Component {
public:
    Mesh*      mesh     = nullptr;
    Material*  material = nullptr;
    // F5-D: textura opcional do material (não-dono; ligada pelo GpuAssets)
    const Texture* texture = nullptr;

    // 0.10-M (PASSO 4): o render por blocos deste meshPath (não-dono — o
    // GpuAssets é dono; ver comentário da classe). null = mesh único/prim.
    BlockMesh* blocks   = nullptr;

    // 0.7.0 — COR POR TIC (gestão de TICs): tint multiplicativo do albedo no
    // shader lit (uniform uTint; default branco = comportamento 0.6.x byte
    // a byte). Sliders R/G/B no Inspector; serializado no .goni
    // ("tint":[r,g,b], default [1,1,1] quando ausente).
    f32 tint[3] = {1.0f, 1.0f, 1.0f};

    // 0.11-T (FASE 3.5a) — LOD POR DISTÂNCIA: a distância (em unidades) a
    // partir da qual ESTE TIC desenha os blocos LOD da cauda GLOD do seu
    // .gmesh (o decimate ½ do import). 0 = OFF (o default jogável manda —
    // um TIC novo NUNCA usa LOD; o pin manda nele). Serializado no .goni
    // como "lodDist" SÓ quando != 0 (os ficheiros da 0.11-S ficam byte a
    // byte). O .gmesh sem cauda GLOD ignora o valor (o draw usa o normal
    // — honesto: o inspector mostra o valor, o render não o tem).
    f32 lodDistance = 0.0f;

    // 0.11-W (3.5b) — GPU INSTANCING: funde blocos com a MESMA assinatura
    // (mesh+bloco+material+textura, nunca skin) em draws instanciados —
    // visualmente IDÊNTICO (o pin do screenshot manda), por isso o default
    // é ON (ao contrário do LOD, que muda a aparência; o default jogável
    // manda no que se VÊ, não no custo). Serializado no .goni como
    // "instancing":false SÓ quando false (o default omitido — os .goni da
    // 0.11-T ficam byte a byte).
    bool instancing = true;

    // 0.11-W (3.75a) — STREAMING PROGRESSIVO: os blocos deste TIC chegam
    // POR CAMADAS (orçamento de 8 loads/frame no caminho inteiro + perto
    // primeiro) em vez de TODOS no 1.º frame visível (o freeze do import).
    // O default é ON (a cura é o comportamento jogável; o OFF devolve o
    // caminho de sempre — lazy inline de sempre, com o freeze de sempre).
    // Vive no par do instancing: com o OFF o TIC sai do batch e desenha
    // individualmente (o MESMO fallback do instancing OFF). Serializado no
    // .goni como "streaming":false SÓ quando false (o default omitido —
    // os .goni da 0.11-T ficam byte a byte).
    bool streaming = true;

    // 0.11-W (3.75b) — IMPOSTORES: a distância (em unidades) a partir da
    // qual ESTE TIC desenha o PNG alfa da silhueta (o billboard em
    // impostors/<stem>.png, gerado sob demanda com o pico de RAM de UM
    // bloco) em vez dos blocos — 1 dc em vez de ~863. 0 = OFF (o default
    // — o comportamento byte a byte do pré-3.75b; o dono liga por TIC).
    // Skinado NUNCA (a pose é dinâmica — um PNG estático mente; o MESMO
    // guard do instancing). Serializado no .goni como "impostorDist" SÓ
    // quando != 0 (o MESMO contrato do "lodDist").
    f32 impostorDistance = 0.0f;

    // F5-C/E: refs relativas do asset — vazias = procedural ("cube"/sem tex).
    // Vivem no componente para o serializer e para a UI mostrarem a origem.
    std::string meshPath;
    std::string texPath;

    // 0.10-M HOTFIX TEXTURAS-VINCULA — os SLOTS 1-4 do material (o slot 0
    // baseColor É o texPath de cima — UMA fonte de verdade por slot; a
    // ordem é o contrato kBind* do GOwnFormats.h). O import preenche
    // texPath (a baseColor do GTB1 — o auto-bind); o picker do inspector
    // escreve O SLOT SELECIONADO sem tocar nos irmãos (o pin do dono —
    // a causa (d) do bug: «o picker escreve um slot único global»).
    // Os slots 1-4 ficam vinculados (serializer/inspector/log) — o GL
    // carrega-os quando o shader ganhar os samplers (o lit de hoje tem
    // SÓ o unit 0 — a fronteira está no relatório do hotfix).
    static constexpr u32 kMeshTexSlots = 5;   // inclui o texPath (slot 0)
    std::string texNormal;       // kBindNormal
    std::string texMetalRough;   // kBindMetalRough
    std::string texEmissive;     // kBindEmissive
    std::string texOcclusion;    // kBindOcclusion
    // o caminho do slot k (0 = texPath; kBindSlotName dá o rótulo)
    const std::string& texSlot(u32 k) const {
        static const std::string kEmpty;
        switch (k) {
            case 0: return texPath;
            case 1: return texNormal;
            case 2: return texMetalRough;
            case 3: return texEmissive;
            case 4: return texOcclusion;
            default: return kEmpty;
        }
    }
    std::string& texSlotRef(u32 k) {
        switch (k) {
            case 0: return texPath;
            case 1: return texNormal;
            case 2: return texMetalRough;
            case 3: return texEmissive;
            case 4: return texOcclusion;
            default: return texPath;   // nunca acontece (k < kMeshTexSlots)
        }
    }

    // 0.10-M HOTFIX TEXTURAS-MULTIMAT — os SLOTS dos MATERIAIS 1..N-1 do
    // .gmesh (o material 0 são os 5 campos de cima — fonte de verdade de
    // sempre). O auto-bind (import/apply/troca) enche (N-1)×5 refs do GTB1
    // (VAZIAS incluídas — a grade é retangular); o seletor «material: i/N»
    // do Inspector mostra ESTE material e o pick escreve NO MATERIAL
    // SELECIONADO (o pin do dono: pick no material 2 não toca no 0).
    // Vector achatado — 5 refs por material extra na ordem kBind*. Vazio =
    // mesh de 1 material (o comportamento byte a byte de sempre). O Nº de
    // materiais deriva AQUI (zero leitura de ficheiro por frame — a lei
    // R-044: a UI não faz I/O no caminho quente).
    std::vector<std::string> matExtraTex;

    // nº de materiais que ESTE TIC conhece (1 + os extras carregados do
    // GTB1; o render POR BLOCO usa o GTB1 na mesma — aqui é a EDIÇÃO)
    u32 matCount() const {
        return static_cast<u32>(matExtraTex.size()) / kMeshTexSlots + 1u;
    }
    // reposição da grade extras (o auto-bind; n = 1 limpa — honesto)
    void setMatCount(u32 n) {
        const size_t want =
            n > 0 ? static_cast<size_t>(n - 1) * kMeshTexSlots : 0u;
        matExtraTex.resize(want);
    }
    // a ref do slot (mat, slot) — LEITURA; fora da grade = vazia (nunca
    // cresce por aqui — a leitura é const)
    const std::string& matTex(u32 mat, u32 slot) const {
        static const std::string kEmpty;
        if (mat == 0) {
            return texSlot(slot);
        }
        if (slot >= kMeshTexSlots) {
            return kEmpty;
        }
        const size_t i =
            (static_cast<size_t>(mat) - 1) * kMeshTexSlots + slot;
        return i < matExtraTex.size() ? matExtraTex[i] : kEmpty;
    }
    // a ref do slot (mat, slot) — ESCRITA (o pick); nullptr = fora da grade
    // (o chamador NÃO cria material fantasma — o guard do clamp manda)
    std::string* matTexRef(u32 mat, u32 slot) {
        if (mat == 0) {
            return &texSlotRef(slot);
        }
        if (slot >= kMeshTexSlots) {
            return nullptr;
        }
        const size_t i =
            (static_cast<size_t>(mat) - 1) * kMeshTexSlots + slot;
        return i < matExtraTex.size() ? &matExtraTex[i] : nullptr;
    }

    // 0.8.10 — primitiva procedural ativa? kind+parâmetros em `prim`; o
    // ponteiro `mesh` aponta para o mesh PRÓPRIO deste TIC (sem cache —
    // posse no main; ver plataforma/main.cpp).
    //
    // TROCA DETERMINÍSTICA (runtime-only, nunca serializado):
    //  • primPending: PEDIDO armado (pick/params/load) — o mesh ANTIGO
    //    continua a renderizar; o main sobe o novo no PONTO SEGURO do frame
    //    e faz o bind atómico (o antigo vai p/ cova = deferred free);
    //  • primRetire: mesh cuja posse vai para a COVA no ponto seguro
    //    (usado pelos picks none/cube/asset — a flush só retira o que é
    //    dela; ponteiros não-nossos são ignorados);
    //  • primNeg: backoff pós-falha de upload — o flush NÃO insiste por
    //    frame (anti retry-storm); pedido NOVO ou INIT_WINDOW limpam.
    //    Em falha o mesh ANTERIOR fica intacto e a renderizar.
    bool       primOn = false;
    PrimParams prim{};
    PrimParams primPrev{};      // 0.8.10: assinatura ANTERIOR (label do log)
    bool       primPending = false;
    Mesh*      primRetire = nullptr;
    bool       primNeg = false;

    // 0.12-Z2 F1 — O REBIND PÓS-RECUPERAÇÃO (runtime-only, nunca
    // serializado): a recuperação da sonda (contextRecover SEM reload de
    // cena — o caminho REAL do EGL_CONTEXT_LOST) desliga mr.mesh/mr.blocks
    // (detachRenderersFromGpu) e esvazia o GpuAssets; o blockRebind() do
    // frame RE-ABRE o asset pelo meshPath (o MESMO g_gpu.mesh do
    // serializer). Este flag é o BACKOFF (o padrão do primNeg): a falha
    // de re-open NÃO insiste por frame (anti retry-storm de IO); um
    // pedido NOVO (pick/params/load) ou o INIT_WINDOW limpam a flag.
    bool       meshRebindNeg = false;
};

} // namespace vv
