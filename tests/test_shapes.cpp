// tests/test_shapes.cpp — F4-A: formas e interseções puras (colide/não colide),
// sweep com TOI+normal, CCD adaptativo e depenetração. Tudo GL-free.
#include "TestFramework.h"
#include "physics/Shapes.h"

using namespace vv;
using namespace vv::phys;
using ::test::nearEqF;
using ::test::vecNearF;

namespace {
constexpr f32 kPi = 3.14159265f;
}

// ---- sphere/sphere ------------------------------------------------------------

TEST(esfera_esfera_colide_e_nao_colide) {
    const Sphere a{Vec3{0, 0, 0}, 0.5f};
    const Sphere b{Vec3{0.9f, 0, 0}, 0.5f};    // soma dos raios = 1.0 > 0.9
    const Sphere c{Vec3{1.1f, 0, 0}, 0.5f};    // 1.0 < 1.1
    EXPECT(intersects(a, b));
    EXPECT(!intersects(a, c));
    EXPECT(intersects(b, a));                  // simetria
}

// ---- sphere/AABB ----------------------------------------------------------------

TEST(esfera_aabb_colide_fora_dentro_e_canto) {
    const AABB box{Vec3{-1, -1, -1}, Vec3{1, 1, 1}};
    const Sphere fora{Vec3{2.5f, 0, 0}, 0.5f};      // longe
    const Sphere tocaFace{Vec3{1.4f, 0, 0}, 0.5f};  // distância 0.4 < r
    const Sphere dentro{Vec3{0, 0, 0}, 0.2f};
    const Sphere canto{Vec3{1.45f, 1.45f, 1.45f}, 0.5f}; // dist euclidiana ~0.779 > r
    const Sphere cantoToca{Vec3{1.28f, 1.28f, 1.28f}, 0.5f}; // 0.28·√3 ≈ 0.485 < r
    EXPECT(!intersects(fora, box));
    EXPECT(intersects(tocaFace, box));
    EXPECT(intersects(dentro, box));
    EXPECT(!intersects(canto, box));      // canto é euclidiano, não por eixo
    EXPECT(intersects(cantoToca, box));
}

// ---- sphere/OBB -------------------------------------------------------------------

TEST(esfera_obb_usa_espaco_local) {
    // caixa 2×2×2 rodada 45° em Y, centrada na origem
    OBB box;
    box.halfExtents = Vec3{1, 1, 1};
    box.rot = Quat::axisAngle(Vec3{0, 1, 0}, kPi * 0.25f);

    // ponto (2.3, 0, 0): no mundo está FORA da caixa alinhada, mas a caixa
    // rodada 45° tem meia-diagonal horizontal √2 ≈ 1.414 no eixo X →
    // distância ao canto rodado... usamos dois casos inequívocos:
    const Sphere pertoDiag{Vec3{2.3f, 0, 0}, 0.5f};   // dist à face rodada < r
    const Sphere longe{Vec3{3.5f, 0, 0}, 0.5f};
    // local de (2.3,0,0) rodado -45°: x' = 2.3·cos45 ≈ 1.626 > 1 → dist 0.626 < 0.5? NÃO
    // recalculamos: face local em x'=1 → dist = 0.626 > r → NÃO colide
    EXPECT(!intersects(pertoDiag, box));
    EXPECT(!intersects(longe, box));

    // encostado à face rodada: local x' = 1.4 → mundo x = 1.4·cos45 ≈ 0.99
    const Sphere face{Vec3{1.35f, 0, 0}, 0.5f};   // local x' ≈ 0.954 → dentro
    EXPECT(intersects(face, box));

    // deslocada: mesmo teste com caixa centrada em (5,0,0)
    OBB desl = box;
    desl.center = Vec3{5, 0, 0};
    EXPECT(intersects(Sphere{Vec3{6.35f, 0, 0}, 0.5f}, desl));
    EXPECT(!intersects(Sphere{Vec3{8.5f, 0, 0}, 0.5f}, desl));
}

// ---- sphere/Capsule -----------------------------------------------------------------

TEST(esfera_capsule_segmento_mais_raio) {
    const Capsule cap{Vec3{0, 1, 0}, 0.3f, 1.0f};   // segmento y∈[0,2]
    EXPECT(intersects(Sphere{Vec3{0, 1, 0}, 0.3f}, cap));      // centro no miolo
    EXPECT(intersects(Sphere{Vec3{0, 2.5f, 0}, 0.3f}, cap));   // perto do topo (esférica)
    EXPECT(intersects(Sphere{Vec3{0.5f, 1, 0}, 0.3f}, cap));   // soma 0.6 > 0.5
    EXPECT(!intersects(Sphere{Vec3{0.7f, 1, 0}, 0.3f}, cap));  // 0.6 < 0.7
    EXPECT(!intersects(Sphere{Vec3{0, 3.0f, 0}, 0.3f}, cap));  // acima do domo
}

// ---- AABB/AABB ------------------------------------------------------------------------

TEST(aabb_aabb_colide_e_nao_colide) {
    const AABB a{Vec3{-1, -1, -1}, Vec3{1, 1, 1}};
    const AABB sobreposto{Vec3{0.5f, 0.5f, 0.5f}, Vec3{2, 2, 2}};
    const AABB tocando{Vec3{1, -1, -1}, Vec3{2, 1, 1}};     // face partilhada
    const AABB separado{Vec3{2, -1, -1}, Vec3{3, 1, 1}};
    EXPECT(intersects(a, sobreposto));
    EXPECT(intersects(a, tocando));   // encostado conta como contacto
    EXPECT(!intersects(a, separado));
}

// ---- OBB/OBB (SAT) ----------------------------------------------------------------------

TEST(obb_obb_sat_alinhadas_rotacionadas_e_separadas) {
    const OBB a;   // 1×1×1 na origem (he 0.5)
    const OBB b{Vec3{0.9f, 0, 0}, Vec3{0.5f, 0.5f, 0.5f}};   // sobrepõe em X
    const OBB c{Vec3{1.3f, 0, 0}, Vec3{0.5f, 0.5f, 0.5f}};   // gap 0.3
    EXPECT(intersects(a, b));
    EXPECT(!intersects(a, c));

    // b rodado 45° em Y: o canto alcança mais longe no X (meia-diagonal 0.707)
    OBB d{Vec3{1.1f, 0, 0}, Vec3{0.5f, 0.5f, 0.5f}};
    d.rot = Quat::axisAngle(Vec3{0, 1, 0}, kPi * 0.25f);
    EXPECT(intersects(a, d));    // sem rotação não tocaria (1.1 > 1.0)

    // separado mesmo com rotação
    OBB e{Vec3{2.0f, 0, 0}, Vec3{0.5f, 0.5f, 0.5f}};
    e.rot = Quat::axisAngle(Vec3{0, 1, 0}, kPi * 0.25f);
    EXPECT(!intersects(a, e));   // 2.0 > 1.0 + 0.707
}

// ---- Capsule/Capsule ----------------------------------------------------------------------

TEST(capsule_capsule_paralelas_cruzadas_e_afastadas) {
    const Capsule a{Vec3{0, 0, 0}, 0.3f, 1.0f};    // eixo Y, y∈[-1,1]
    const Capsule b{Vec3{0.5f, 0, 0}, 0.3f, 1.0f}; // paralela, distância 0.5 < 0.6
    const Capsule c{Vec3{0.7f, 0, 0}, 0.3f, 1.0f}; // 0.6 < 0.7
    const Capsule cruz{Vec3{0, 0, 0}, 0.3f, 1.0f};
    // cruzada em X: segmento x∈[-1,1] y=0 — cruza o miolo de a
    Segment sx{Vec3{-1, 0, 0}, Vec3{1, 0, 0}};
    // construí-la exigiria rotação — testamos via segSegClosest diretamente abaixo
    EXPECT(intersects(a, b));
    EXPECT(!intersects(a, c));

    Vec3 p1, p2;
    segSegClosest(Segment{Vec3{-1, 0, 0}, Vec3{1, 0, 0}},   // X horizontal
                  Segment{Vec3{0, -1, 0}, Vec3{0, 1, 0}},   // Y vertical
                  p1, p2);
    EXPECT(vecNearF(p1, Vec3{0, 0, 0}, 1e-5f));
    EXPECT(vecNearF(p2, Vec3{0, 0, 0}, 1e-5f));

    // paralelas deslocadas: pontos mais próximos no meio
    // paralelas: o par mais próximo NÃO é único — valida a distância e o
    // emparelhamento (mesma coordenada X em ambos)
    segSegClosest(Segment{Vec3{-1, 0, 0}, Vec3{1, 0, 0}},
                  Segment{Vec3{-1, 2, 0}, Vec3{1, 2, 0}}, p1, p2);
    EXPECT(nearEqF(length(p1 - p2), 2.0f, 1e-5f));
    EXPECT(nearEqF(p1.x, p2.x, 1e-5f) && nearEqF(p1.z, p2.z, 1e-5f));
}

// ---- Capsule vs caixas ----------------------------------------------------------------------

TEST(capsule_aabb_e_obb_colide_e_nao_colide) {
    const AABB chao{Vec3{-10, -1, -10}, Vec3{10, 0, 10}};   // chão até y=0
    const Capsule emPe{Vec3{0, 1.0f, 0}, 0.3f, 0.5f};       // segmento y∈[0.5,1.5]
    // dist do segmento ao chão = 0.5 > r=0.3 → a esfera base chega a y=0.2 > 0
    EXPECT(!intersects(emPe, chao));
    const Capsule baixa{Vec3{0, 0.25f, 0}, 0.3f, 0.5f};     // base do segmento y=−0.25 < 0
    EXPECT(intersects(baixa, chao));
    const Capsule encosta{Vec3{0, 0.29f, 0}, 0.3f, 0.0f};   // base y=0.29 < r=0.3 → toca
    EXPECT(intersects(encosta, chao));

    // parede OBB rodada 30° em Y
    OBB parede;
    parede.center = Vec3{3, 1, 0};
    parede.halfExtents = Vec3{2.0f, 1.0f, 0.1f};
    parede.rot = Quat::axisAngle(Vec3{0, 1, 0}, kPi / 6.0f);
    // distância do ponto (2.5,1,0) ao plano da parede = 0.25 < r=0.3,
    // e cai no miolo da parede (|x'|≈0.43 < 2)
    const Capsule junto{Vec3{2.5f, 1, 0}, 0.3f, 0.5f};
    const Capsule longe{Vec3{-1.5f, 1, 0}, 0.3f, 0.5f};   // |x'|≈3.9 > 2+r
    EXPECT(intersects(junto, parede));
    EXPECT(!intersects(longe, parede));
}

// ---- helpers ----------------------------------------------------------------------------------

TEST(capsule_segment_e_sdf_helpers) {
    const Segment s = capsuleSegment(Capsule{Vec3{1, 2, 3}, 0.3f, 0.5f});
    EXPECT(vecNearF(s.a, Vec3{1, 1.5f, 3}));
    EXPECT(vecNearF(s.b, Vec3{1, 2.5f, 3}));

    const AABB box{Vec3{-1, -1, -1}, Vec3{1, 1, 1}};
    EXPECT(nearEqF(pointAabbSDF(box, Vec3{3, 0, 0}), 2.0f));    // fora, face
    EXPECT(nearEqF(pointAabbSDF(box, Vec3{0, 0, 0}), -1.0f));   // dentro, plano mais próximo
    EXPECT(nearEqF(pointAabbSDF(box, Vec3{2, 2, 2}), std::sqrt(3.0f), 1e-4f)); // canto

    OBB rot;
    rot.halfExtents = Vec3{1, 1, 1};
    rot.rot = Quat::axisAngle(Vec3{0, 1, 0}, kPi * 0.5f);   // 90°
    // ponto no mundo (3,0,0) → local (0,0,-3): distância à face z = 2
    EXPECT(nearEqF(pointObbSDF(rot, Vec3{3, 0, 0}), 2.0f));
}
