#pragma once
// components/BodyComp.h — corpo de física do TIC (F4).
//
// Três tipos (semântica Godot/Unlight):
//   Static    — não se move; alvo dos sweeps dos outros (chão/parede)
//   Character — movido por input+gravidade, sweep + slide (move_and_slide)
//   Rigid     — gravidade + colisão primitiva contra Static (cai, para,
//               desliza). SEM solver de stacking/resting: dois Rigid
//               empilhados podem intersectar — PLACEHOLDER documentado
//               (ver physics/PhysicsSystem.h e o relatório da fase).
//
// `shape` guarda os parâmetros LOCAIS da forma (relativos ao Transform3D do
// dono — ver physics/Shapes.h); o PhysicsSystem deriva a forma de mundo por
// tick aplicando pos/rot/scale do transform. velocity/grounded são estado de
// runtime (não persistidos no .goni — corpo novo nasce em repouso).
#include "core/Component.h"
#include "physics/Shapes.h"
#include <variant>

namespace vv {

enum class BodyType : u8 {
    Static    = 0,
    Character = 1,
    Rigid     = 2,
};

// Forma do corpo: parâmetros LOCAIS de uma das quatro formas (vv::phys).
using BodyShape = std::variant<phys::Sphere, phys::AABB, phys::OBB, phys::Capsule>;

class BodyComp : public Component {
public:
    BodyType type = BodyType::Static;
    BodyShape shape = phys::Capsule{};
    Vec3 velocity{0.0f, 0.0f, 0.0f};
    bool grounded = false;

    // nome canônico do tipo (serializer/Inspector/testes)
    static const char* typeName(BodyType t) {
        switch (t) {
            case BodyType::Character: return "character";
            case BodyType::Rigid:     return "rigid";
            default:                  return "static";
        }
    }
    static const char* shapeName(const BodyShape& s) {
        switch (s.index()) {
            case 0: return "sphere";
            case 1: return "aabb";
            case 2: return "obb";
            default: return "capsule";
        }
    }
};

} // namespace vv
