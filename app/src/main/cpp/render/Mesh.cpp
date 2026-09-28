#include "render/Mesh.h"
#include "platform/Log.h"
#include <GLES3/gl3.h>
#include <cstddef>   // offsetof

namespace vv {

Mesh::~Mesh() {
    destroy();
}

bool Mesh::create(const Vertex* vertices, u32 vertexCount,
                  const u16* indices, u32 indexCount) {
    if (!vertices || vertexCount == 0 || !indices || indexCount == 0) {
        LOGE("Mesh: dados inválidos (v=%u i=%u)", vertexCount, indexCount);
        return false;
    }
    destroy();

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

    // aPos (location 0) e aNormal (location 1) — coincide com o Material lit.
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, pos)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
                          reinterpret_cast<const void*>(offsetof(Vertex, normal)));

    glBindVertexArray(0);

    vertexCount_ = vertexCount;
    indexCount_ = indexCount;
    return true;
}

void Mesh::destroy() {
    if (ebo_) { glDeleteBuffers(1, &ebo_); ebo_ = 0; }
    if (vbo_) { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_) { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    vertexCount_ = 0;
    indexCount_ = 0;
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
