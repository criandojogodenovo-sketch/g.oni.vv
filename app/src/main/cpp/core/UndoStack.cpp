// core/UndoStack.cpp — implementação do undo/redo por snapshot (0.9.0).
#include "core/UndoStack.h"
#include "core/Scene.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"

#include <cstdio>

namespace vv {
namespace editor {

TicSnap snapTic(const Scene& scene, Handle h) {
    TicSnap s;
    const Tic* t = scene.get(h);
    if (!t) {
        return s;   // TIC morto: snapshot vazio (o op de DELETE usa-o como after)
    }
    std::snprintf(s.name, sizeof(s.name), "%s", t->name.c_str());
    s.parent = t->parent;
    s.visible = t->visible;
    if (const Transform3D* tr = t->getComponent<Transform3D>()) {
        s.hasTr = true;
        s.pos = tr->pos;
        s.rot = tr->rot;
        s.scale = tr->scale;
    }
    if (const MeshRenderer* mr = t->getComponent<MeshRenderer>()) {
        s.hasMr = true;
        std::snprintf(s.meshPath, sizeof(s.meshPath), "%s", mr->meshPath.c_str());
        std::snprintf(s.texPath, sizeof(s.texPath), "%s", mr->texPath.c_str());
        s.tint[0] = mr->tint[0];
        s.tint[1] = mr->tint[1];
        s.tint[2] = mr->tint[2];
        s.primOn = mr->primOn;
        s.primKind = mr->prim.kind;
        s.primRadius = mr->prim.radius;
        s.primSize = mr->prim.size;
        s.primSegments = mr->prim.segments;
        s.primRings = mr->prim.rings;
    }
    return s;
}

Handle restoreTic(Scene& scene, const TicSnap& s, Handle current) {
    // 1º o HANDLE corrente (o normal: o TIC está vivo — o rename/undo de
    // edição funciona sem procurar por nome)
    Tic* t = scene.get(current);
    if (!t) {
        // 2º por nome (re-criação após DELETE)
        t = scene.get(scene.find(s.name));
    }
    if (!t) {
        // procura por nome é frágil (nomes repetidos): o chamador do undo
        // passa normalmente handles VIVOS; para re-criação, cria raiz nova
        const Handle h = scene.create(s.name, s.parent);
        t = scene.get(h);
        if (!t) {
            return Handle::invalid();
        }
    }
    t->parent = s.parent;
    t->visible = s.visible;
    Transform3D* tr = t->getComponent<Transform3D>();
    if (s.hasTr) {
        if (!tr) {
            tr = t->addComponent<Transform3D>();
        }
        if (tr) {
            tr->pos = s.pos;
            tr->rot = s.rot;
            tr->scale = s.scale;
            tr->updateWorld();
        }
    }
    MeshRenderer* mr = t->getComponent<MeshRenderer>();
    if (s.hasMr) {
        if (!mr) {
            mr = t->addComponent<MeshRenderer>();
        }
        if (mr) {
            mr->meshPath = s.meshPath;
            mr->texPath = s.texPath;
            mr->tint[0] = s.tint[0];
            mr->tint[1] = s.tint[1];
            mr->tint[2] = s.tint[2];
            if (mr->primOn != s.primOn || !s.primOn) {
                // ligar/desligar prim ARMA o pedido (o main sobe no ponto
                // seguro — o mesmo caminho do picker)
                mr->primOn = s.primOn;
                mr->primPending = true;
            }
            mr->prim.kind = s.primKind;
            mr->prim.radius = s.primRadius;
            mr->prim.size = s.primSize;
            mr->prim.segments = s.primSegments;
            mr->prim.rings = s.primRings;
            if (s.primOn) {
                mr->primPending = true;   // re-sobe a geometria se preciso
            }
        }
    }
    return t->handle;
}

Handle pasteAsNew(Scene& scene, const TicSnap& s) {
    // nome único (Godot-style .001/.002…)
    char name[48];
    std::snprintf(name, sizeof(name), "%.28s", s.name);
    u32 suffix = 1;
    while (scene.find(name).valid() && suffix < 1000u) {
        std::snprintf(name, sizeof(name), "%.28s.%03u", s.name, suffix);
        ++suffix;
    }
    const Handle h = scene.create(name, s.parent);
    Tic* t = scene.get(h);
    if (!t) {
        return Handle::invalid();
    }
    t->visible = s.visible;
    if (s.hasTr) {
        if (Transform3D* tr = t->addComponent<Transform3D>()) {
            tr->pos = s.pos;
            tr->rot = s.rot;
            tr->scale = s.scale;
            tr->updateWorld();
        }
    }
    if (s.hasMr) {
        if (MeshRenderer* mr = t->addComponent<MeshRenderer>()) {
            mr->meshPath = s.meshPath;
            mr->texPath = s.texPath;
            mr->tint[0] = s.tint[0];
            mr->tint[1] = s.tint[1];
            mr->tint[2] = s.tint[2];
            mr->primOn = s.primOn;
            mr->prim.kind = s.primKind;
            mr->prim.radius = s.primRadius;
            mr->prim.size = s.primSize;
            mr->prim.segments = s.primSegments;
            mr->prim.rings = s.primRings;
            mr->primPending = s.primOn;   // sobe a geometria no ponto seguro
        }
    }
    return h;
}

bool snapEq(const TicSnap& a, const TicSnap& b) {
    return std::strcmp(a.name, b.name) == 0 && a.parent == b.parent &&
           a.visible == b.visible && a.hasTr == b.hasTr && a.hasMr == b.hasMr &&
           a.pos.x == b.pos.x && a.pos.y == b.pos.y && a.pos.z == b.pos.z &&
           a.scale.x == b.scale.x && a.scale.y == b.scale.y &&
           a.scale.z == b.scale.z &&
           a.rot.x == b.rot.x && a.rot.y == b.rot.y && a.rot.z == b.rot.z &&
           a.rot.w == b.rot.w &&
           std::strcmp(a.meshPath, b.meshPath) == 0 &&
           std::strcmp(a.texPath, b.texPath) == 0 &&
           a.primOn == b.primOn && a.primKind == b.primKind &&
           a.primRadius == b.primRadius && a.primSize == b.primSize &&
           a.primSegments == b.primSegments && a.primRings == b.primRings;
}

bool UndoStack::push(const TicSnap& before, const TicSnap& after,
                     Handle tic) {
    if (snapEq(before, after)) {
        return false;   // no-op (gizmo sem movimento) — não polui a stack
    }
    // a cauda do redo morre (padrão universal: editar após undo corta o redo)
    if (cursor < count) {
        count = cursor;
    }
    if (count >= kCap) {
        // cheia: desloca UMA para a esquerda (a mais antiga sai)
        for (u32 i = 1; i < kCap; ++i) {
            ops[i - 1] = ops[i];
        }
        --count;
    }
    ops[count].before = before;
    ops[count].after = after;
    ops[count].tic = tic;
    ops[count].valid = true;
    ++count;
    cursor = count;
    return true;
}

Handle UndoStack::undo(Scene& scene) {
    if (!canUndo()) {
        return Handle::invalid();
    }
    --cursor;
    return restoreTic(scene, ops[cursor].before, ops[cursor].tic);
}

Handle UndoStack::redo(Scene& scene) {
    if (!canRedo()) {
        return Handle::invalid();
    }
    const Handle h = restoreTic(scene, ops[cursor].after, ops[cursor].tic);
    ++cursor;
    return h;
}

} // namespace editor
} // namespace vv
