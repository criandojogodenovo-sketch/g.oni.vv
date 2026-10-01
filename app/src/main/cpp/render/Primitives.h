#pragma once
// render/Primitives.h — primitivas mesh PROCEDURAIS (0.8.0, F7).
//
// Oito formas geradas EM CÓDIGO (esfera, cilindro, cone, box, plano,
// triângulo/wedge, torus, cápsula), cada uma com parâmetros (raio/altura/
// segmentos) e defaults sensatos. Fim do placeholder "só cubo": uma cena
// bloqueia-se sem importar NADA de fora — o preset de TIC "Mesh" nasce com
// uma primitiva e o Inspector troca o tipo e afina os parâmetros.
//
// GEOMETRIA: centrada na ORIGEM (box = convenção do makeCube da F2; o
// transform do TIC posiciona) — EXCEÇÃO documentada: o PLANO vive em y=0
// (chão por omissão, normal +Y; o utilizador sobe-o com py se precisar).
// Winding CCW visto de fora, normais por vértice, uv por parâmetro
// (esférico/cilíndrico/planar — dentro de [0,1]).
//
// LIMITES (defesa do gerador, testados no CI): radius/size > 0; height >=
// 2*radius na cápsula (clamp interno — a cápsula nunca degenera); segments
// em [3,64]; rings em [2,64]. Saída: u16 (limite 65535 vértices — o formato
// do engine; primitivas densas fazem clamp de segmentos ANTES de estourar).
//
// GL-free: gera arrays de Vertex — host-testável (o upload é do render/Mesh
// como sempre foi; o main faz cache por assinatura, 1 primitiva = 1 objeto
// de GPU partilhado por todos os TICs com os mesmos parâmetros).
#include "render/Vertex.h"
#include <string>
#include <vector>

namespace vv {

enum class PrimKind : u32 {
    Sphere   = 0,   // bola — UV clássica (polos em ±Y)
    Cylinder = 1,   // eixo Y, tampas incluídas
    Cone     = 2,   // base em −h/2, ápice em +h/2 (eixo Y)
    Box      = 3,   // cubo (alias do makeCube — mesma convenção)
    Plane    = 4,   // quad XZ em y=0, normal +Y
    Wedge    = 5,   // prisma triangular (rampa): triângulo em XZ extrudado em Y
    Torus    = 6,   // eixo Y, raio principal + raio do tubo
    Capsule  = 7,   // cilindro + meias-esferas (altura TOTAL, clamp h>=2r)
    Count    = 8,
};

// parâmetros do gerador — defaults sensatos por tipo (primDefaults).
// Um PRIM por vez no MeshRenderer: `kind` + estes números formam a
// ASSINATURA do cache do main (serializada no .goni como "prim").
struct PrimParams {
    PrimKind kind = PrimKind::Sphere;
    f32 radius  = 0.5f;    // esfera/cil/cone/torus(tubo? NÃO — principal)/cápsula
    f32 height  = 1.0f;    // cilindro/cone/cápsula (ALTURA TOTAL)
    f32 radius2 = 0.15f;   // torus: raio do TUBO
    i32 segments = 16;     // segmentos RADIAIS (>=3)
    i32 rings    = 8;      // anéis VERTICAIS (esfera/torus; >=2)
    f32 size    = 1.0f;    // box/plano/wedge (aresta/lado/tamanho)
};

// nome canônico (serializer + seletor do Inspector): "esfera"… — PT como o
// resto da UI do editor; o .goni grava este nome (tipo serializado).
const char* primName(PrimKind k);
// inverso (load): "esfera"→Sphere; desconhecido → Sphere (defesa)
PrimKind primFromName(const std::string& name);
// rótulo curto do seletor ("esfera", "cilindro", …)
const char* primLabel(PrimKind k);
// defaults por tipo (segmentos/rings calibrados por forma)
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

// gera a primitiva (appends em `out` após clear; determinístico)
void makePrimMesh(const PrimParams& p, PrimMeshData& out);

// bounding box (AABB) da geometria gerada — teste de coerência do CI
void primBounds(const PrimMeshData& m, Vec3& mn, Vec3& mx);

} // namespace vv
