// tests/test_grid.cpp — invariantes GL-free do grid F3.1 (quad de shader).
// O init/draw exigem contexto GLES3 (só no device/NDK build — o build NDK
// valida o GLSL embutido); no CI Linux validamos os limites constexpr da
// spec e o estado pré-init da interface preservada.
#include "TestFramework.h"
#include "render/Grid.h"

using namespace vv;

TEST(grid_f31_extent_efetivo_cobre_a_spec) {
    // spec F3.1: extent efetivo ≥ 2000 — a borda do quad nunca aparece,
    // nem no zoom máx (300); passo positivo e menor que a extensão
    EXPECT(Grid::kExtentDefault >= 2000.0f);
    EXPECT(Grid::kStepDefault > 0.0f);
    EXPECT(Grid::kExtentDefault > Grid::kStepDefault);
}

TEST(grid_pre_init_estado_limpo) {
    // contrato da interface (mantida da F2): sem init não há VAO nem vértices
    Grid g;
    EXPECT(!g.ok());
    EXPECT(g.vertexCount() == 0);
}
