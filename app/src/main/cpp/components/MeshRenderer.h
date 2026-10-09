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

    // F5-C/E: refs relativas do asset — vazias = procedural ("cube"/sem tex).
    // Vivem no componente para o serializer e para a UI mostrarem a origem.
    std::string meshPath;
    std::string texPath;

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
};

} // namespace vv
