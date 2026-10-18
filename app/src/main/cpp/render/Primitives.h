#pragma once
// render/Primitives.h — primitivas mesh PROCEDURAIS (0.8.0, F7; 0.8.10).
//
// 0.8.10 — DECISÃO DO DONO: SÓ CUBO E ESFERA. O cilindro e as outras seis
// formas saem do gerador, do seletor, do serializer e dos testes — a
// intermitência do C33 ("esfera e cilindro só às vezes") vivia no caminho
// partilhado (cache de meshes GPU), não nos geradores individuais, e a
// resposta foi encolher o caminho ao mínimo determinístico:
//   • DUAS formas geradas EM CÓDIGO (esfera, box), centradas na origem;
//   • MIGRAÇÃO: um .goni antigo com "cilindro"/"cone"/"plano"/"triangulo"/
//     "torus"/"capsula" carrega como CUBE + log `mesh: prim <x> removido ->
//     cube` + toast UMA vez por load — nunca crash, nunca silêncio;
//   • PUREZA: mesmos parâmetros → BYTES IDÊNTICOS (primMeshHash no CI).
//
// GEOMETRIA: centrada na ORIGEM (box = convenção do makeCube da F2; o
// transform do TIC posiciona). Winding CCW visto de fora, normais por
// vértice, uv por parâmetro (esférico/planar — dentro de [0,1]).
//
// LIMITES (defesa do gerador, testados no CI): radius/size > 0; segments
// em [3,64]; rings em [2,64]. Saída: u16 (limite 65535 vértices — o formato
// do engine; primitivas densas fazem clamp de segmentos ANTES de estourar).
//
// GL-free: gera arrays de Vertex — host-testável. 0.8.10: SEM CACHE no
// main — cada MeshRenderer com primOn tem o SEU mesh (troca determinística
// com deferred free; ver platform/main.cpp).
#include "render/Vertex.h"
#include <string>
#include <vector>

namespace vv {

enum class PrimKind : u32 {
    Sphere = 0,   // bola — UV clássica (polos em ±Y)
    Box    = 1,   // cubo (alias do makeCube — mesma convenção)
    Count  = 2,
};

// parâmetros do gerador — defaults sensatos por tipo (primDefaults).
// Um PRIM por vez no MeshRenderer: `kind` + estes números formam a
// ASSINATURA serializada no .goni como "prim". 0.8.10: height/radius2
// deixaram de ser usados pelos 2 geradores vivos — mantêm-se no struct
// (compatibilidade do .goni antigo: os campos são LIDOS e ignorados).
struct PrimParams {
    PrimKind kind = PrimKind::Sphere;
    f32 radius  = 0.5f;    // esfera
    f32 height  = 1.0f;    // (removido 0.8.10 — lido do .goni antigo, ignorado)
    f32 radius2 = 0.15f;   // (removido 0.8.10 — idem)
    i32 segments = 16;     // segmentos RADIAIS (>=3)
    i32 rings    = 8;      // anéis VERTICAIS (esfera; >=2)
    f32 size    = 1.0f;    // box (aresta)
};

// nome canónico (serializer + seletor do Inspector): PT como o resto da UI.
const char* primName(PrimKind k);
// inverso (load): "esfera"→Sphere, "box"→Box; desconhecido → Sphere (defesa)
PrimKind primFromName(const std::string& name);
// 0.8.10 — MIGRAÇÃO: true se o nome é uma primitiva REMOVIDA (carrega como
// cube com aviso). "cilindro" "cone" "plano" "triangulo" "torus" "capsula".
bool primRemoved(const std::string& name);
// rótulo curto do seletor ("esfera", "box")
const char* primLabel(PrimKind k);
// defaults por tipo
PrimParams primDefaults(PrimKind k);
// clamp defensivo dos parâmetros (usa o mesmo em todo o lado)
void primClamp(PrimParams& p);

// geometria gerada (vertices/indices prontos para o Mesh::create)
struct PrimMeshData {
    std::vector<Vertex> vertices;
    std::vector<u16>    indices;

    void clear() { vertices.clear(); indices.clear(); }
    bool ok() const { return !vertices.empty() && indices.size() >= 3; }
};

// gera a primitiva (appends em `out` após clear; DETERMINÍSTICO: mesmos
// parâmetros clampados → mesmos bytes, sempre)
void makePrimMesh(const PrimParams& p, PrimMeshData& out);

// 0.8.10 — PUREZA: hash FNV-1a dos bytes da geometria (posições/normais/uvs
// bit-a-bit + índices). O teste do CI gera DUAS vezes e compara — provar
// que o gerador é função pura dos parâmetros (sem estado escondido).
u64 primMeshHash(const PrimMeshData& m);

// bounding box (AABB) da geometria gerada — teste de coerência do CI
void primBounds(const PrimMeshData& m, Vec3& mn, Vec3& mx);

} // namespace vv
