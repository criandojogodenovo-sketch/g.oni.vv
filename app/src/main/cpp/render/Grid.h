#pragma once
// render/Grid.h — grid de chão do viewport 3D (F2; reescrito em shader F3.1).
//
// Era uma malha de linhas (centenas de vértices, custo crescendo com a cena);
// F3.1 troca por UM quad de 4 vértices que segue a câmara, com as linhas
// desenhadas no fragment shader: grid procedural (fract + derivadas fwidth
// para AA), fade radial pela distância à câmara e anti-moiré por minificação
// (células sub-pixel dissolvem em vez de cintilar).
//
// "Espaço infinito" mantido: o fade termina muito antes da borda do quad
// (extent ≥ 2000, a borda nunca fica visível) e o chão perde-se no fundo
// como um horizonte. Interface preservada (init/destroy/draw/vertexCount/ok)
// — o F8 pode evoluir isto para gizmo de chão sem tocar nos chamadores.
//
// Tema mono intacto: linhas cinza escuro (0.30), eixos X/Z mais claros
// (0.55). O quad testa depth (respeita os TICs) e não escreve depth (nunca
// os oculta) — igual ao grid de linhas que substitui.
#include "core/Types.h"
#include "math/Math.h"
#include "render/DrawStats.h"

namespace vv {

class Grid {
public:
    // defaults F3.1 — meia-extensão do quad (mundo) e passo das linhas
    static constexpr f32 kExtentDefault = 3000.0f;   // ≥ 2000 exigidos pela spec
    static constexpr f32 kStepDefault   = 1.0f;
    static_assert(kExtentDefault >= 2000.0f,
                  "grid: extent efetivo >= 2000 para a borda nunca aparecer");

    bool init(f32 extent = kExtentDefault, f32 step = kStepDefault);
    void destroy();

    // vp = projeção×vista; camPos para o fade radial; focusDist = distância
    // câmara→target — o fade escala com o zoom (F3.1; o default 0 mantém a
    // assinatura antiga válida e cai no zoom de trabalho 6). Devolve métricas.
    DrawStats draw(const Mat4& vp, const Vec3& camPos, f32 focusDist = 0.0f) const;

    u32 vertexCount() const { return vertexCount_; }   // 4 (quad único) após init
    bool ok() const { return vao_ != 0; }

private:
    u32 prog_ = 0;
    u32 vao_ = 0;
    u32 vbo_ = 0;
    i32 locVP_ = -1;
    i32 locCenter_ = -1;
    i32 locCam_ = -1;
    i32 locFade_ = -1;
    i32 locStep_ = -1;
    u32 vertexCount_ = 0;
    f32 step_ = kStepDefault;
};

} // namespace vv
