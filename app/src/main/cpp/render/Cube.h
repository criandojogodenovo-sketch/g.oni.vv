#pragma once
// render/Cube.h — cubo procedural do pass 3D (F2).
//
// PLACEHOLDER — NÃO É IMPLEMENTAÇÃO FINAL.
// Motivo: F2 precisa de um objeto 3D determinístico para validar o pipeline
// (depth, câmara, materiais) sem dependê-lo de assets. Substituição na F5:
// import de mesh de asset → este gerador sai.
//
// 24 vértices (4 por face × 6 faces, normais por face) + 36 índices.
// Winding CCW visto de fora (front face default do GLES). GL-free: os testes
// do CI Linux geram e validam os arrays sem contexto OpenGL.
#include <array>
#include "render/Vertex.h"

namespace vv {

struct CubeMeshData {
    std::array<Vertex, 24> vertices;
    std::array<u16, 36>    indices;
};

// Cubo centrado na origem com aresta `size`, alinhado aos eixos.
CubeMeshData makeCube(f32 size);

} // namespace vv
