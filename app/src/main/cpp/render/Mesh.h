#pragma once
// render/Mesh.h — geometria estática no GPU (VBO + VAO + index buffer).
// Criada uma vez a partir de arrays de Vertex; draw() desenha com índices.
// Layout dos atributos deve coincidir com o pipeline do Material (location
// 0 = aPos, 1 = aNormal, 2 = aUV).
//
// 0.8.2 (F7): SKINNED — createSkinned acrescenta um VBO de skin (8 floats
// por vértice: aJoints vec4 + aWeights vec4, locations 3/4). O shader lit
// pesa as 4 influências contra uBones[64] quando uSkin != 0; um mesh
// estático (create) mantém o comportamento de sempre (atributos 3/4 não
// ligados → valor constante; uSkin=0 ignora-os).
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

    // 0.8.2 (F7): idem + ATRIBUTOS DE SKIN (joints/weights: 4+4 floats por
    // vértice, arrays com vertexCount*4 elementos). Falha se algum for
    // nulo (skin parcial não vale nada — o MeshData só preenche ambos).
    bool createSkinned(const Vertex* vertices, u32 vertexCount,
                       const u16* indices, u32 indexCount,
                       const u8* joints, const f32* weights);

    void destroy();

    void bind() const;
    void draw() const;                    // glDrawElements(GL_TRIANGLES)

    u32 vertexCount() const { return vertexCount_; }
    u32 indexCount() const { return indexCount_; }
    bool ok() const { return vao_ != 0; }
    bool skinned() const { return skinVbo_ != 0; }   // 0.8.2

private:
    u32 vao_ = 0;
    u32 vbo_ = 0;
    u32 ebo_ = 0;
    u32 skinVbo_ = 0;   // 0.8.2: aJoints/aWeights (locations 3/4)
    u32 vertexCount_ = 0;
    u32 indexCount_ = 0;
};

} // namespace vv
