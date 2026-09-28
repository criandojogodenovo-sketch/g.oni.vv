// tests/test_cube.cpp — gerador do cubo: 24 vértices / 36 índices, normais
// unitárias, posições no cubo, winding CCW visto de fora.
#include "TestFramework.h"
#include "render/Cube.h"

using namespace vv;

namespace {

bool nearEq(f32 a, f32 b, f32 eps = 1e-4f) {
    const f32 d = a - b;
    return d < eps && d > -eps;
}

} // namespace

TEST(cubo_tem_24_vertices_e_36_indices) {
    const CubeMeshData c = makeCube(2.0f);
    EXPECT(c.vertices.size() == 24u);
    EXPECT(c.indices.size() == 36u);
}

TEST(cubo_indices_validos) {
    const CubeMeshData c = makeCube(2.0f);
    for (const u16 idx : c.indices) {
        EXPECT(idx < 24u);
    }
}

TEST(cubo_normais_unitarias) {
    const CubeMeshData c = makeCube(1.3f);
    for (const Vertex& v : c.vertices) {
        EXPECT(nearEq(length(v.normal), 1.0f, 1e-3f));
    }
}

TEST(cubo_seis_normais_orto_e_quatro_vertices_cada) {
    const CubeMeshData c = makeCube(2.0f);
    const Vec3 axes[6] = {
        {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
    };
    for (const Vec3& axis : axes) {
        int count = 0;
        for (const Vertex& v : c.vertices) {
            if (nearEq(v.normal.x, axis.x) && nearEq(v.normal.y, axis.y) &&
                nearEq(v.normal.z, axis.z)) {
                ++count;
            }
        }
        EXPECT(count == 4);
    }
}

TEST(cubo_posicoes_dentro_do_volume) {
    const f32 size = 2.0f;
    const CubeMeshData c = makeCube(size);
    const f32 h = size * 0.5f;
    for (const Vertex& v : c.vertices) {
        EXPECT(v.pos.x <= h + 1e-4f && v.pos.x >= -h - 1e-4f);
        EXPECT(v.pos.y <= h + 1e-4f && v.pos.y >= -h - 1e-4f);
        EXPECT(v.pos.z <= h + 1e-4f && v.pos.z >= -h - 1e-4f);
    }
}

TEST(cubo_winding_ccw_visto_de_fora) {
    // para cada triângulo: dot(cross(B-A, C-A), normal) > 0 → front face CCW
    const CubeMeshData c = makeCube(2.0f);
    for (u32 i = 0; i + 2 < c.indices.size(); i += 3) {
        const Vertex& a = c.vertices[c.indices[i]];
        const Vertex& b = c.vertices[c.indices[i + 1]];
        const Vertex& d = c.vertices[c.indices[i + 2]];
        const Vec3 n = cross(b.pos - a.pos, d.pos - a.pos);
        EXPECT(dot(n, a.normal) > 0.0f);
    }
}
