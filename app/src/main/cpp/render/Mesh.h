#pragma once
// render/Mesh.h — geometria estática no GPU (VBO + VAO + index buffer).
// Criada uma vez a partir de arrays de Vertex; draw() desenha com índices.
// Layout dos atributos deve coincidir com o pipeline do Material (location
// 0 = aPos, 1 = aNormal).
#include "core/Types.h"
#include "render/Vertex.h"

namespace vv {

class Mesh {
public:
    Mesh() = default;
    ~Mesh();                              // liberta GL se ainda dono
    Mesh(const Mesh&) = delete;           // recurso de GL: não copiável
    Mesh& operator=(const Mesh&) = delete;

    // Faz upload da geometria; não mantém ponteiros para os arrays.
    bool create(const Vertex* vertices, u32 vertexCount,
                const u16* indices, u32 indexCount);
    void destroy();

    void bind() const;
    void draw() const;                    // glDrawElements(GL_TRIANGLES)

    u32 vertexCount() const { return vertexCount_; }
    u32 indexCount() const { return indexCount_; }
    bool ok() const { return vao_ != 0; }

private:
    u32 vao_ = 0;
    u32 vbo_ = 0;
    u32 ebo_ = 0;
    u32 vertexCount_ = 0;
    u32 indexCount_ = 0;
};

} // namespace vv
