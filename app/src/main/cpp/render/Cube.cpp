#include "render/Cube.h"

namespace vv {

CubeMeshData makeCube(f32 size) {
    CubeMeshData out{};

    // Para cada face: normal n e tangentes u, v ortonormais com cross(u, v) = n.
    // Winding (-1,-1) → (+1,-1) → (+1,+1) → (-1,+1) no plano (u, v) é CCW visto
    // de fora (do lado para onde n aponta).
    struct Face { Vec3 n, u, v; };
    static constexpr Face kFaces[6] = {
        {{ 0.0f, 0.0f,  1.0f}, { 1.0f, 0.0f, 0.0f}, {0.0f, 1.0f,  0.0f}},  // +Z
        {{ 0.0f, 0.0f, -1.0f}, {-1.0f, 0.0f, 0.0f}, {0.0f, 1.0f,  0.0f}},  // -Z
        {{ 1.0f, 0.0f,  0.0f}, { 0.0f, 0.0f,-1.0f}, {0.0f, 1.0f,  0.0f}},  // +X
        {{-1.0f, 0.0f,  0.0f}, { 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f,  0.0f}},  // -X
        {{ 0.0f, 1.0f,  0.0f}, { 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f, -1.0f}},  // +Y
        {{ 0.0f,-1.0f,  0.0f}, { 1.0f, 0.0f, 0.0f}, {0.0f, 0.0f,  1.0f}},  // -Y
    };

    const f32 h = size * 0.5f;
    u16 vi = 0;
    u32 ii = 0;
    for (const Face& f : kFaces) {
        const u16 base = vi;
        static constexpr f32 kSigns[4][2] = {
            {-1.0f, -1.0f}, {1.0f, -1.0f}, {1.0f, 1.0f}, {-1.0f, 1.0f},
        };
        for (const auto& sg : kSigns) {
            Vertex& vtx = out.vertices[vi++];
            vtx.pos    = f.n * h + f.u * (sg[0] * h) + f.v * (sg[1] * h);
            vtx.normal = f.n;
            // F5: UV por face — canto (-1,-1) = (0,0), (+1,+1) = (1,1)
            vtx.uv = Vec2{(sg[0] + 1.0f) * 0.5f, (sg[1] + 1.0f) * 0.5f};
        }
        // dois triângulos por face: (0,1,2) e (0,2,3) — CCW visto de fora
        out.indices[ii++] = base;
        out.indices[ii++] = static_cast<u16>(base + 1);
        out.indices[ii++] = static_cast<u16>(base + 2);
        out.indices[ii++] = base;
        out.indices[ii++] = static_cast<u16>(base + 2);
        out.indices[ii++] = static_cast<u16>(base + 3);
    }
    return out;
}

} // namespace vv
