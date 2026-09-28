// physics/PhysicsSystem.cpp — física core da F4 (Character slide, Rigid
// primitivo, repel). Construída sobre as primitivas puras de physics/Shapes.
#include "physics/PhysicsSystem.h"
#include "components/BodyComp.h"
#include "physics/InputSource.h"
#include "components/InputMap.h"
#include "components/Transform3D.h"
#include "core/Scene.h"
#include <cmath>

namespace vv {
namespace phys {

namespace {

constexpr f32 kMinDim        = 0.01f;  // dimensão mínima de forma (defesa)
constexpr f32 kGroundNormalY = 0.6f;   // normal com y>0.6 conta como chão
constexpr f32 kMaxFallSpeed  = 60.0f;  // teto de queda (defesa)
// 1 + extra para cantos (spec F4-B): um tick pode tocar CHÃO + PAREDE em
// sequência (diagonal) — cada contacto consome uma iteração a remover a
// componente normal; 4 deixa o tangencial livre no pior caso comum. Bounded:
// cada iteração só remove componentes — sobras descartadas, nunca atravessa.
constexpr u32 kSlideIters    = 4;
constexpr f32 kContactSkin   = 1e-4f;  // pele de contacto: sai 0,1 mm além da
                                       // superfície — impede que o arredondamento
                                       // flutuante re-dispare o contacto a toi=0
                                       // e consuma as iterações do slide

f32 maxComp(const Vec3& v) {
    f32 m = v.x > v.y ? v.x : v.y;
    return m > v.z ? m : v.z;
}

// Deriva a forma de MUNDO a partir do BodyComp (local) + Transform3D.
WorldBody deriveWorld(const BodyComp& bc, const Transform3D& tr, Handle owner) {
    WorldBody w;
    w.owner = owner;
    w.type = bc.type;
    w.ticPos = tr.pos;
    w.velocity = bc.velocity;
    w.grounded = bc.grounded;

    const Quat& q = tr.rot;
    const Vec3& s = tr.scale;
    if (const phys::Sphere* sp = std::get_if<phys::Sphere>(&bc.shape)) {
        w.kind = 0;
        w.center = tr.pos + q.rotate(sp->center);
        w.r = sp->r * (maxComp(s) > kMinDim ? maxComp(s) : kMinDim);
    } else if (const phys::OBB* ob = std::get_if<phys::OBB>(&bc.shape)) {
        w.kind = 1;
        w.center = tr.pos + q.rotate(ob->center);
        w.he = Vec3{ob->halfExtents.x * (s.x > kMinDim ? s.x : kMinDim),
                    ob->halfExtents.y * (s.y > kMinDim ? s.y : kMinDim),
                    ob->halfExtents.z * (s.z > kMinDim ? s.z : kMinDim)};
        w.rot = q * ob->rot;
    } else if (const phys::Capsule* cp = std::get_if<phys::Capsule>(&bc.shape)) {
        w.kind = 2;
        w.center = tr.pos + q.rotate(cp->center);
        const f32 sxz = s.x > s.z ? s.x : s.z;
        w.r = cp->radius * (sxz > kMinDim ? sxz : kMinDim);
        w.hh = cp->halfHeight * (s.y > kMinDim ? s.y : kMinDim);
        const Vec3 axis = q.rotate(Vec3{0.0f, 1.0f, 0.0f});   // eixo Y LOCAL
        w.segA = w.center - axis * w.hh;
        w.segB = w.center + axis * w.hh;
    } else {   // AABB → caixa alinhada ao mundo (rotação do TIC ignorada)
        const phys::AABB* bx = std::get_if<phys::AABB>(&bc.shape);
        w.kind = 1;
        const Vec3 c = bx ? (bx->min + bx->max) * 0.5f : Vec3{0, 0, 0};
        const Vec3 ext = bx ? (bx->max - bx->min) * 0.5f : Vec3{0.5f, 0.5f, 0.5f};
        w.center = tr.pos + Vec3{c.x * s.x, c.y * s.y, c.z * s.z};
        w.he = Vec3{ext.x * s.x, ext.y * s.y, ext.z * s.z};
        w.rot = Quat::identity();
    }
    return w;
}

// MTD capsule↔capsule com eixos ARBITRÁRIOS (Shapes assume Y; aqui o mundo
// pode ter cápsulas rodadas — usa segSegClosest diretamente).
Contact capCapMTD(const WorldBody& a, const WorldBody& b) {
    Contact out;
    Vec3 p1, p2;
    segSegClosest(phys::Segment{a.segA, a.segB}, phys::Segment{b.segA, b.segB},
                  p1, p2);
    const Vec3 d = p1 - p2;
    const f32 dist = length(d);
    const f32 rr = a.r + b.r;
    if (dist > rr) {
        out.separation = dist - rr;
        return out;
    }
    out.hit = true;
    out.depth = rr - dist;
    out.normal = dist > 1e-9f ? d * (1.0f / dist) : Vec3{0, 1, 0};
    return out;
}

// MTD sphere↔capsule com eixo arbitrário da cápsula.
Contact sphCapMTD(const WorldBody& s, const WorldBody& c) {
    Contact out;
    const Vec3 cp = closestPointOnSegment(c.segA, c.segB, s.center);
    const Vec3 d = s.center - cp;
    const f32 dist = length(d);
    const f32 rr = s.r + c.r;
    if (dist > rr) {
        out.separation = dist - rr;
        return out;
    }
    out.hit = true;
    out.depth = rr - dist;
    out.normal = dist > 1e-9f ? d * (1.0f / dist) : Vec3{0, 1, 0};
    return out;
}

} // namespace

void PhysicsSystem::translate(WorldBody& b, const Vec3& v) {
    b.ticPos = b.ticPos + v;
    b.center = b.center + v;
    b.segA = b.segA + v;
    b.segB = b.segB + v;
}

SweepResult PhysicsSystem::sweepVariant(const WorldBody& a, const Vec3& delta,
                                        const WorldBody& b) const {
    // contínuo contra CAIXAS (alvo típico: Static OBB/AABB)
    if (b.kind == 1) {
        if (a.kind == 0) {
            return sweepSegBox(phys::Segment{a.center, a.center}, a.r, delta,
                               b.center, b.he, b.rot);
        }
        if (a.kind == 2) {
            return sweepSegBox(phys::Segment{a.segA, a.segB}, a.r, delta,
                               b.center, b.he, b.rot);
        }
        return {};   // mover-caixa não existe nos presets da F4
    }

    // alvo ESFERA: sweep contínuo (bloqueia sem penetrar — Rigids/bolas)
    if (b.kind == 0) {
        if (a.kind == 0) {
            return sweepSegSphere(phys::Segment{a.center, a.center}, a.r, delta,
                                  b.center, b.r);
        }
        if (a.kind == 2) {
            return sweepSegSphere(phys::Segment{a.segA, a.segB}, a.r, delta,
                                  b.center, b.r);
        }
        return {};
    }

    // alvos CÁPSULA: depenetração discreta (Characters encostados — o repel
    // resolve a sobreposição; sem túnel relevante a velocidades de input)
    Contact ct;
    if (a.kind == 0 && b.kind == 2) {
        ct = sphCapMTD(a, b);
    } else {   // capsule ↔ capsule
        ct = capCapMTD(a, b);
    }
    SweepResult r;
    if (ct.hit) {
        r.hit = true;
        r.toi = 0.0f;
        r.depth = ct.depth;
        r.normal = ct.normal;
    }
    return r;
}

void PhysicsSystem::moveBody(WorldBody& a, const Vec3& delta,
                             std::vector<WorldBody>& all, bool hitStatics,
                             bool hitCharacters, bool hitRigids) {
    Vec3 remaining = delta;
    a.grounded = false;
    for (u32 iter = 0; iter < kSlideIters; ++iter) {
        SweepResult best;
        for (const WorldBody& ob : all) {
            if (ob.owner == a.owner) {
                continue;
            }
            const bool allowed = (ob.type == BodyType::Static && hitStatics) ||
                                 (ob.type == BodyType::Character && hitCharacters) ||
                                 (ob.type == BodyType::Rigid && hitRigids);
            if (!allowed) {
                continue;
            }
            const SweepResult r = sweepVariant(a, remaining, ob);
            if (r.hit && (!best.hit || r.toi < best.toi)) {
                best = r;
            }
        }
        if (!best.hit) {
            translate(a, remaining);
            return;
        }
        contacts_++;
        if (best.toi > 0.0f) {
            translate(a, remaining * best.toi);
            remaining = remaining * (1.0f - best.toi);
            // aterra exatamente na superfície: dá a pele para o próximo
            // passo tangencial não re-disparar o contacto discreto
            translate(a, best.normal * kContactSkin);
        } else if (best.depth > -kContactSkin) {
            // sobreposto (toi=0): depenetra + pele
            translate(a, best.normal * (best.depth + kContactSkin));
        }
        // slide: remove a componente normal, mantém a tangencial
        const f32 into = dot(remaining, best.normal);
        if (into < 0.0f) {
            remaining = remaining - best.normal * into;
        }
        if (best.normal.y > kGroundNormalY) {
            a.grounded = true;
        }
    }
    // sobras descartadas após 2 iterações (canto) — nunca atravessa
}

void PhysicsSystem::tick(Scene& scene, f32 dt) {
    if (!enabled || dt <= 0.0f) {
        return;
    }
    contacts_ = 0;
    ComponentStore& store = scene.components();

    // 1. coleta corpos ativos com transform
    std::vector<WorldBody> bodies;
    std::vector<BodyComp*> comps;
    std::vector<Transform3D*> transforms;
    bodies.reserve(store.bodies().size());
    comps.reserve(store.bodies().size());
    transforms.reserve(store.bodies().size());
    auto& bstore = store.bodies();
    for (u32 i = 0; i < bstore.size(); ++i) {
        const Handle oh = bstore.owner(i);
        Tic* t = scene.get(oh);
        Transform3D* tr = store.transforms().find(oh);
        if (!t || !t->active || !tr) {
            continue;
        }
        bodies.push_back(deriveWorld(bstore.at(i), *tr, oh));
        comps.push_back(&bstore.at(i));
        transforms.push_back(tr);
    }

    // write-back: pos + dirty + world imediato + estado runtime
    auto writeBack = [&](const WorldBody& w) {
        for (size_t i = 0; i < bodies.size(); ++i) {
            if (bodies[i].owner == w.owner) {
                Transform3D* tr = transforms[i];
                BodyComp* bc = comps[i];
                tr->pos = w.ticPos;
                tr->worldDirty = true;      // invalida o cache (spec F4-B.7)
                tr->updateWorld();          // refresca já (sem lag visual)
                bc->velocity = w.velocity;
                bc->grounded = w.grounded;
                return;
            }
        }
    };

    // 2. RIGID: gravidade + sweep/slide contra Static (colisão primitiva;
    //    SEM stacking/resting — PLACEHOLDER documentado)
    for (WorldBody& w : bodies) {
        if (w.type != BodyType::Rigid) {
            continue;
        }
        w.velocity.y -= gravity * dt;
        if (w.velocity.y < -kMaxFallSpeed) {
            w.velocity.y = -kMaxFallSpeed;
        }
        moveBody(w, w.velocity * dt, bodies, true, false, false);
        if (w.grounded) {
            if (w.velocity.y < 0.0f) {
                w.velocity.y = 0.0f;   // assente: sem acumular gravidade
            }
            const f32 k = 1.0f - rigidDamping * dt;   // desliza e abranda
            w.velocity.x *= k > 0.0f ? k : 0.0f;
            w.velocity.z *= k > 0.0f ? k : 0.0f;
        }
        writeBack(w);
    }

    // 3. CHARACTER: input → velocidade; gravidade; sweep+slide vs
    //    Static+Character (Rigid entra como obstáculo no passo 8 da F4-C)
    for (WorldBody& w : bodies) {
        if (w.type != BodyType::Character) {
            continue;
        }
        Vec2 ax{0.0f, 0.0f};
        bool wantsJump = false;
        if (InputMap* im = store.inputMaps().find(w.owner)) {
            const InputSource* src = im->source;   // fonte explícita
            if (src) {
                ax = src->axis();
                wantsJump = src->action("jump");
            }
            // sem fonte explícita: TouchControls irmão entra na F4-D
        }
        const Vec3 want = frame.right * ax.x + frame.fwd * ax.y;
        w.velocity.x = want.x * charSpeed;
        w.velocity.z = want.z * charSpeed;
        if (wantsJump && w.grounded) {
            w.velocity.y = jumpSpeed;
        }
        w.velocity.y -= gravity * dt;
        if (w.velocity.y < -kMaxFallSpeed) {
            w.velocity.y = -kMaxFallSpeed;
        }
        // Rigid entra como OBSTÁCULO (o player não atravessa a bola) — sem
        // transferência de momento: o empurrão na F4 é feito editando a
        // velocity do Rigid no Inspector (colisão primitiva, sem solver)
        moveBody(w, w.velocity * dt, bodies, true, true, true);
        if (w.grounded && w.velocity.y < 0.0f) {
            w.velocity.y = 0.0f;
        }
        writeBack(w);
    }

    // 4. REPEL Character↔Character (mutuo, posicional, metade para cada)
    for (size_t i = 0; i < bodies.size(); ++i) {
        if (bodies[i].type != BodyType::Character) {
            continue;
        }
        for (size_t j = i + 1; j < bodies.size(); ++j) {
            if (bodies[j].type != BodyType::Character) {
                continue;
            }
            const Contact ct = capCapMTD(bodies[i], bodies[j]);
            if (!ct.hit || ct.depth <= 0.0f) {
                continue;
            }
            contacts_++;
            const Vec3 half = ct.normal * (ct.depth * 0.5f);
            translate(bodies[i], half);        // normal aponta de j para i
            translate(bodies[j], half * -1.0f);
            writeBack(bodies[i]);
            writeBack(bodies[j]);
        }
    }
}

} // namespace phys
} // namespace vv
