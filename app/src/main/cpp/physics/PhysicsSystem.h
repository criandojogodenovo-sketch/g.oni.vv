#pragma once
// physics/PhysicsSystem.h — sistema de física core da F4 (grupo TickGroup::Physics).
//
// O que faz por passo fixo (dt):
//   1. RIGID   — gravidade + sweep/slide contra Static (cai, para, desliza);
//                amortecimento horizontal no chão. SEM solver de
//                stacking/resting: dois Rigid empilhados PODEM intersectar —
//                PLACEHOLDER de solver, aceite pela spec e documentado.
//   2. CHARACTER — input (InputSource do InputMap) → velocidade horizontal;
//                gravidade; jump (action("jump") com grounded); sweep contra
//                Static + Character (+Rigid como obstáculo) com SLIDE estilo
//                move_and_slide (remove a componente normal, mantém a
//                tangencial; 1 iteração extra para cantos).
//   3. REPEL   — Characters sobrepostos repelem-se mutuamente (posicional).
//
// Após mover cada corpo: Transform3D.pos atualizado + worldDirty marcado +
// updateWorld() imediato (o TransformSystem do Update seguinte reconfirma).
//
// Notas da F4 (documentadas no relatório):
//   • a física só avança em modo Play (enabled — main liga ao botão Play);
//   • movers são sphere/capsule (presets); caixas são alvos (sweep contínuo
//     contra caixas; alvos sphere/capsule usam depenetração discreta por
//     substep — sem túnel porque o CCD limita o avanço por substep);
//   • scale do TIC aplica-se à forma: OBB componente a componente, sphere
//     pelo maior eixo, capsule r=max(sx,sz) e hh=sy (não-uniforme aproximado).
#include "components/BodyComp.h"
#include "core/Handle.h"
#include "core/Tick.h"
#include "core/Types.h"
#include "math/Math.h"
#include "physics/Shapes.h"
#include <vector>

namespace vv {

class Scene;
class InputMap;
class Transform3D;

namespace phys {

// Corpo derivado para o tick: forma em MUNDO (esfera/caixa/cápsula) + estado.
// kind: 0 = sphere, 1 = box (OBB-ready; AABB entra como rot identidade),
//       2 = capsule (eixo Y local do TIC, já rotacionado).
struct WorldBody {
    Handle owner{};
    BodyType type = BodyType::Static;
    Vec3  ticPos;        // pos do TIC (write-back da física)
    u8    kind = 0;
    Vec3  center;        // centro no mundo (sphere/box/capsule)
    f32   r = 0.0f;      // raio (sphere/capsule)
    f32   hh = 0.0f;     // meia-altura do segmento (capsule)
    Vec3  he;            // meia-extensão (box)
    Quat  rot = Quat::identity();   // orientação (box)
    Vec3  segA, segB;    // segmento do mundo (capsule)
    Vec3  velocity{0.0f, 0.0f, 0.0f};
    bool  grounded = false;
};

class PhysicsSystem : public System {
public:
    // Base de movimento do input (mundo). O main liga à câmara: stick para
    // cima = afastar-se da câmara, stick para a direita = direita do ecrã.
    struct MoveFrame {
        Vec3 right{1.0f, 0.0f, 0.0f};
        Vec3 fwd{0.0f, 0.0f, -1.0f};
    };

    MoveFrame frame;
    f32  gravity      = 18.0f;   // unidades/s² (afeta Character e Rigid)
    f32  charSpeed    = 5.0f;    // velocidade horizontal do Character (input)
    f32  jumpSpeed    = 7.0f;    // impulso vertical do jump
    f32  rigidDamping = 6.0f;    // amortecimento horizontal do Rigid no chão
    bool enabled      = false;   // física avança só em modo Play

    void tick(Scene& scene, f32 dt) override;

    // métricas do último tick (testes/diagnóstico)
    u32 contactsLastTick() const { return contacts_; }

private:
    u32 contacts_ = 0;

    void moveBody(WorldBody& a, const Vec3& delta, std::vector<WorldBody>& all,
                  bool hitStatics, bool hitCharacters, bool hitRigids);
    SweepResult sweepVariant(const WorldBody& a, const Vec3& delta,
                             const WorldBody& b) const;
    static void translate(WorldBody& b, const Vec3& v);
};

} // namespace phys
} // namespace vv
