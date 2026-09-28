#pragma once
// render/Grid.h — grid de chão do viewport 3D (F2).
//
// PLACEHOLDER — NÃO É IMPLEMENTAÇÃO FINAL.
// Motivo: dar referência de escala/orientação durante o orbit da câmara.
// Destino na F8: sai ou vira gizmo de chão dedicado.
//
// "Espaço infinito": as linhas desvanecem com a distância À CÂMARA (fade
// radial no shader) e a malha estende-se para lá do alcance do fade — a
// borda do grid nunca fica visível, o chão perde-se no fundo como um
// horizonte. Extensão física: plano XZ centrado na origem, step de 1 unidade,
// eixos X/Z destacados. Linhas não escrevem depth (não ocultam o cubo).
#include "core/Types.h"
#include "math/Math.h"
#include "render/DrawStats.h"

namespace vv {

class Grid {
public:
    bool init(f32 extent = 96.0f, f32 step = 1.0f);
    void destroy();

    // vp = projeção×vista; camPos para o fade radial. Devolve métricas do draw.
    DrawStats draw(const Mat4& vp, const Vec3& camPos) const;

    u32 vertexCount() const { return vertexCount_; }
    bool ok() const { return vao_ != 0; }

private:
    u32 prog_ = 0;
    u32 vao_ = 0;
    u32 vbo_ = 0;
    i32 locVP_ = -1;
    i32 locCam_ = -1;
    u32 vertexCount_ = 0;
};

} // namespace vv
