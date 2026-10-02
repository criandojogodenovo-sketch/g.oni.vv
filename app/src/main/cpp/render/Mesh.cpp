#include "render/Mesh.h"
#include "platform/Log.h"
#include <GLES3/gl3.h>
#include <cstddef>   // offsetof
#include <cmath>     // 0.8.9: isfinite (validação de geometria)
#include <vector>

namespace vv {

Mesh::~Mesh() {
    destroy();
}

bool Mesh::create(const Vertex* vertices, u32 vertexCount,
                  const u16* indices, u32 indexCount) {
    return createSkinned(vertices, vertexCount, indices, indexCount,
                         nullptr, nullptr);
}

// 0.8.2 (F7): create + VBO de skin (aJoints/aWeights, locations 3/4).
// joints (u8) promovem-se a float no upload — o shader lê vec4 direto.
bool Mesh::createSkinned(const Vertex* vertices, u32 vertexCount,
                         const u16* indices, u32 indexCount,
                         const u8* joints, const f32* weights) {
    if (!vertices || vertexCount == 0 || !indices || indexCount == 0) {
        LOGE("Mesh: dados inválidos (v=%u i=%u)", vertexCount, indexCount);
        return false;
    }
    if ((joints == nullptr) != (weights == nullptr)) {
        LOGE("Mesh: skin parcial (joints sem weights ou vice-versa)");
        return false;
    }
    destroy();

    // 0.8.9: AABB da geometria ANTES do upload — dados puros (host-testável).
    // Coordenadas NÃO finitas rejeitam o mesh na origem (o import nunca mais
    // sobe geometria com NaN/Inf — "à prova de falha" do prompt 0.8.9).
    // (depois do destroy(): um upload que falhe depois deixa bounds a zero —
    // mesh falhado = sem AABB, contrato do chamador)
    {
        Vec3 mn{vertices[0].pos.x, vertices[0].pos.y, vertices[0].pos.z};
        Vec3 mx = mn;
        for (u32 v = 1; v < vertexCount; ++v) {
            const Vec3& p = vertices[v].pos;
            if (!std::isfinite(p.x) || !std::isfinite(p.y) ||
                !std::isfinite(p.z)) {
                LOGE("Mesh: vértice %u com coordenada NÃO finita — upload "
                     "recusado", v);
                return false;
            }
            if (p.x < mn.x) mn.x = p.x;
            if (p.y < mn.y) mn.y = p.y;
            if (p.z < mn.z) mn.z = p.z;
            if (p.x > mx.x) mx.x = p.x;
            if (p.y > mx.y) mx.y = p.y;
            if (p.z > mx.z) mx.z = p.z;
        }
        boundsMin_ = mn;
        boundsMax_ = mx;
    }

    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glGenBuffers(1, &ebo_);
    if (!vao_ || !vbo_ || !ebo_) {
        LOGE("Mesh: falha ao criar buffers GL");
        destroy();
        return false;
    }

    glBindVertexArray(vao_);

    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(vertexCount) * sizeof(Vertex),
                 vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(indexCount) * sizeof(u16),
                 indices, GL_STATIC_DRAW);

    // aPos (location 0), aNormal (location 1) e aUV (location 2, F5) —
    // coincide com o Material lit (uv amostro a textura quando presente).
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, uv)));

    // 0.8.2: skin VBO — 8 floats por vértice (joints float + weights)
    if (joints && weights) {
        glGenBuffers(1, &skinVbo_);
        if (!skinVbo_) {
            LOGE("Mesh: falha ao criar o VBO de skin");
            destroy();
            return false;
        }
        glBindBuffer(GL_ARRAY_BUFFER, skinVbo_);
        std::vector<f32> skin;
        skin.reserve(static_cast<size_t>(vertexCount) * 8);
        for (u32 v = 0; v < vertexCount; ++v) {
            for (int c = 0; c < 4; ++c) {
                skin.push_back(static_cast<f32>(joints[v * 4 + c]));
            }
            for (int c = 0; c < 4; ++c) {
                skin.push_back(weights[v * 4 + c]);
            }
        }
        glBufferData(GL_ARRAY_BUFFER,
                     static_cast<GLsizeiptr>(skin.size()) * sizeof(f32),
                     skin.data(), GL_STATIC_DRAW);
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(f32),
                              reinterpret_cast<const void*>(0));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, 8 * sizeof(f32),
                              reinterpret_cast<const void*>(4 * sizeof(f32)));
    }

    glBindVertexArray(0);

    vertexCount_ = vertexCount;
    indexCount_ = indexCount;
    return true;
}

void Mesh::destroy() {
    if (ebo_) { glDeleteBuffers(1, &ebo_); ebo_ = 0; }
    if (skinVbo_) { glDeleteBuffers(1, &skinVbo_); skinVbo_ = 0; }   // 0.8.2
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    vertexCount_ = 0;
    indexCount_ = 0;
    boundsMin_ = Vec3{};   // 0.8.9: sem geometria, sem AABB
    boundsMax_ = Vec3{};
}

void Mesh::bind() const {
    glBindVertexArray(vao_);
}

void Mesh::draw() const {
    if (!vao_ || indexCount_ == 0) {
        return;
    }
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indexCount_),
                   GL_UNSIGNED_SHORT, nullptr);
}

} // namespace vv
