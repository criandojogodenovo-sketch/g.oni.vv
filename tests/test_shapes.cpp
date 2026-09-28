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

// ---- sweep + CCD (F4-A.3) -----------------------------------------------------------------

TEST(sweep_esfera_aabb_cara_toi_e_normal_analiticos) {
    const Sphere s{Vec3{-5, 0, 0}, 0.5f};
    const AABB box{Vec3{0, -1, -1}, Vec3{1, 1, 1}};
    const SweepResult r = sweep(s, Vec3{5, 0, 0}, box);
    EXPECT(r.hit);
    EXPECT(nearEqF(r.toi, 0.9f, 1e-3f));       // contacto com centro em x=−0.5
    EXPECT(vecNearF(r.normal, Vec3{-1, 0, 0}, 1e-3f));
}

TEST(sweep_esfera_passa_acima_sem_contacto) {
    const Sphere s{Vec3{-5, 0, 0}, 0.5f};
    const AABB box{Vec3{0, -1, -1}, Vec3{1, 1, 1}};
    const SweepResult r = sweep(s, Vec3{5, 3, 0}, box);   // no ponto mais próximo y=2.7
    EXPECT(!r.hit);
    EXPECT(nearEqF(r.toi, 1.0f));
}

TEST(sweep_esfera_obb_face_rotacionada_45) {
    OBB box;
    box.halfExtents = Vec3{1, 1, 1};
    box.rot = Quat::axisAngle(Vec3{0, 1, 0}, kPi * 0.25f);
    // aproxima-se na direção da normal da face local −X:
    // u = (cos45, 0, −sin45); contacto quando x' local = −(1+0.5) → toi 0.7
    const Vec3 u{0.70710678f, 0.0f, -0.70710678f};
    const Sphere s{u * (-5.0f), 0.5f};
    const SweepResult r = sweep(s, u * 5.0f, box);
    EXPECT(r.hit);
    EXPECT(nearEqF(r.toi, 0.7f, 1e-3f));
    EXPECT(vecNearF(r.normal, u * (-1.0f), 1e-3f));
}

TEST(sweep_capsule_aabb_pe_analitico) {
    const Capsule c{Vec3{-3, 1, 0}, 0.3f, 0.5f};   // segmento y∈[0.5,1.5]
    const AABB box{Vec3{0, -1, -1}, Vec3{1, 2, 1}};
    const SweepResult r = sweep(c, Vec3{3, 0, 0}, box);
    EXPECT(r.hit);
    EXPECT(nearEqF(r.toi, 0.9f, 1e-3f));           // segmento para em x=−0.3
    EXPECT(vecNearF(r.normal, Vec3{-1, 0, 0}, 1e-3f));
}

TEST(sweep_capsule_deitada_eixo_proprio) {
    // capsule rodada 90° em Z: segmento ao longo de X, x∈[−3.5,−2.5], y=1
    Capsule c;
    c.center = Vec3{-3, 1, 0};
    c.radius = 0.3f;
    c.halfHeight = 0.5f;
    // segmento manual (o sistema roda o eixo Y local pelo TIC):
    Segment seg{Vec3{-3.5f, 1, 0}, Vec3{-2.5f, 1, 0}};
    const AABB box{Vec3{0, 0, -1}, Vec3{1, 2, 1}};
    const SweepResult r = sweepSegBox(seg, c.radius, Vec3{3, 0, 0},
                                      aabbCenter(box), aabbExtent(box),
                                      Quat::identity());
    EXPECT(r.hit);
    EXPECT(nearEqF(r.toi, 2.2f / 3.0f, 1e-3f));    // ponta chega a x=−0.3
    EXPECT(vecNearF(r.normal, Vec3{-1, 0, 0}, 1e-3f));
}

TEST(ccd_nao_tunela_parede_fina_a_alta_velocidade) {
    // parede com 0.1 de espessura; |delta| = 20 ≫ r = 0.2 (substeps = 8)
    const Sphere s{Vec3{0, 0, -10}, 0.2f};
    const AABB parede{Vec3{-2, -2, -0.05f}, Vec3{2, 2, 0.05f}};
    const SweepResult r = sweep(s, Vec3{0, 0, 20}, parede);
    EXPECT(r.hit);
    EXPECT(nearEqF(r.toi, 9.75f / 20.0f, 1e-3f));  // centro para em z=−0.25 (face −0.05)
    EXPECT(vecNearF(r.normal, Vec3{0, 0, -1}, 1e-3f));

    // capsule a alta velocidade contra a mesma parede
    const Capsule c{Vec3{0, 1, -10}, 0.3f, 0.5f};
    const SweepResult r2 = sweep(c, Vec3{0, 0, 20}, parede);
    EXPECT(r2.hit);
    EXPECT(nearEqF(r2.toi, (10.0f - 0.3f - 0.05f) / 20.0f, 5e-3f));
    EXPECT(vecNearF(r2.normal, Vec3{0, 0, -1}, 1e-3f));
}

TEST(ccd_move_afasta_sem_contacto_e_parte_de_dentro) {
    const Sphere s{Vec3{5, 0, 0}, 0.5f};
    const AABB box{Vec3{0, -1, -1}, Vec3{1, 1, 1}};
    EXPECT(!sweep(s, Vec3{5, 0, 0}, box).hit);     // afasta-se

    // começa dentro → TOI 0 e normal de saída (+x: face mais próxima)
    const Sphere dentro{Vec3{0.9f, 0, 0}, 0.5f};
    const SweepResult r = sweep(dentro, Vec3{0.1f, 0, 0}, box);
    EXPECT(r.hit && nearEqF(r.toi, 0.0f));
    EXPECT(r.normal.x > 0.9f);
}

TEST(depenetracao_esferas_e_capsules_mtd_exatos) {
    const Contact cs = depenetrate(Sphere{Vec3{0, 0, 0}, 0.5f},
                                   Sphere{Vec3{0.8f, 0, 0}, 0.5f});
    EXPECT(cs.hit);
    EXPECT(nearEqF(cs.depth, 0.2f, 1e-4f));
    EXPECT(vecNearF(cs.normal, Vec3{-1, 0, 0}, 1e-4f));  // afasta A de B (A está à esquerda)

    const Contact cp = depenetrate(Capsule{Vec3{0, 0, 0}, 0.3f, 0.5f},
                                   Capsule{Vec3{0.5f, 0, 0}, 0.3f, 0.5f});
    EXPECT(cp.hit);
    EXPECT(nearEqF(cp.depth, 0.1f, 1e-4f));              // 0.6 − 0.5
    EXPECT(vecNearF(cp.normal, Vec3{-1, 0, 0}, 1e-4f));

    const Contact longe = depenetrate(Sphere{Vec3{0, 0, 0}, 0.5f},
                                      Sphere{Vec3{5, 0, 0}, 0.5f});
    EXPECT(!longe.hit);
}

TEST(depenetracao_capsule_no_chao_normal_para_cima) {
    const AABB chao{Vec3{-10, -1, -10}, Vec3{10, 0, 10}};
    // base da esfera em y = 0.25−(0.5+0.3) = −0.55 → 0.55 de penetração? não:
    // segmento base y = −0.25; distância ao topo do chão (y=0) = 0.25 < r →
    // depth = r − 0.25 = 0.05
    const Contact c = depenetrate(Capsule{Vec3{0, 0.25f, 0}, 0.3f, 0.5f}, chao);
    EXPECT(c.hit);
    EXPECT(nearEqF(c.depth, 0.05f, 1e-3f));
    EXPECT(vecNearF(c.normal, Vec3{0, 1, 0}, 1e-3f));
}
