#pragma once
// core/PlaySnapshot.h — sandbox do modo Play (F4.2), header-only, GL-free.
//
// O bug do C33 (B3): ao sair do Play os Transform3D ficavam na pose
// pós-simulação (o corpo continuava caído no editor). Comportamento correto
// de editor: Play é uma sandbox TEMPORÁRIA.
//   • ENTRAR no Play  → capture(): guarda pos/rot/scale de TODOS os
//     Transform3D ativos + velocity/grounded de TODOS os BodyComp ativos;
//   • SAIR do Play    → restore(): repõe o snapshot e descarta as mudanças
//     da simulação. A física (TickGroup::Physics, gate `enabled`) continua a
//     correr SÓ durante o Play — inalterado.
//
// Restrições respeitadas: SEM componentes novos e SEM física nova — é um log
// de dados dos componentes existentes, guardado FORA da Scene (no main).
//   • TIC criado durante o Play: não está no snapshot → mantém a pose
//     simulada (aceite pela spec; nada o restaura).
//   • TIC destruído durante o Play: handle obsoleto → get() devolve nullptr
//     → simplesmente saltado no restore (handles generacionais nunca
//     reutilizam slots — ver core/Scene.h), sem crash.
// updateWorld() no restore: o cache do TRS volta à pose de editor no MESMO
// frame (feedback imediato; o TransformSystem reconfirma no passo seguinte).
#include <vector>
#include "components/BodyComp.h"
#include "components/Transform3D.h"
#include "core/Scene.h"

namespace vv {

struct PlaySnapshot {
    struct Tf {
        Handle h{};
        Vec3 pos{};
        Quat rot = Quat::identity();
        Vec3 scale{1.0f, 1.0f, 1.0f};
    };
    struct Body {
        Handle h{};
        Vec3 velocity{};
        bool grounded = false;
    };

    std::vector<Tf>   transforms;
    std::vector<Body> bodies;
    bool captured = false;   // true entre capture e restore (sandbox aberta)
};

// ENTRAR no Play: pose de editor de todos os TICs ativos → snapshot.
inline void playSnapshotCapture(const Scene& scene, PlaySnapshot& out) {
    out.transforms.clear();
    out.bodies.clear();
    using Tf   = PlaySnapshot::Tf;
    using Body = PlaySnapshot::Body;
    // storages independentes: um TIC pode ter Body sem Transform e vice-versa
    scene.forEachActive([&scene, &out](const Tic& t) {
        const Handle h = t.handle;
        if (const Transform3D* tr = scene.components().transforms().find(h)) {
            out.transforms.push_back(Tf{h, tr->pos, tr->rot, tr->scale});
        }
        if (const BodyComp* b = scene.components().bodies().find(h)) {
            out.bodies.push_back(Body{h, b->velocity, b->grounded});
        }
    });
    out.captured = true;
}

// SAIR do Play: repõe a pose de editor e descarta a simulação. TICs mortos
// ou sem o componente são saltados (nunca crasha).
inline void playSnapshotRestore(Scene& scene, const PlaySnapshot& snap) {
    if (!snap.captured) {
        return;   // Play nunca abriu — nada a restaurar
    }
    for (const PlaySnapshot::Tf& rec : snap.transforms) {
        Tic* t = scene.get(rec.h);
        if (!t || !t->active) {
            continue;   // destruído durante o Play → saltado
        }
        if (Transform3D* tr = scene.components().transforms().find(rec.h)) {
            tr->pos = rec.pos;
            tr->rot = rec.rot;
            tr->scale = rec.scale;
            tr->updateWorld();   // cache do TRS de volta à pose de editor
            tr->worldDirty = false;
        }
    }
    for (const PlaySnapshot::Body& rec : snap.bodies) {
        Tic* t = scene.get(rec.h);
        if (!t || !t->active) {
            continue;
        }
        if (BodyComp* b = scene.components().bodies().find(rec.h)) {
            b->velocity = rec.velocity;
            b->grounded = rec.grounded;
        }
    }
}

} // namespace vv
