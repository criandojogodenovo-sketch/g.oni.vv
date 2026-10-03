#pragma once
// core/UndoStack.h — UNDO/REDO 0.9.0 (scope funcional acordado: "stack de
// operações ligada à stack da viewport").
//
// DESIGN: snapshot POR OPERAÇÃO (antes/depois do TIC inteiro — transform,
// mesh/tex/prim, nome, pai, visível). ~200 B/op × 64 ops. Cada ação
// editável empurra UMA operação com os DOIS estados; undo repõe `before`,
// redo repõe `after`. A "stack da viewport" (gizmos) empurra no FIM do
// drag (press = captura before, release = captura after).
//
// PURO (sem GL, sem storage) — afervel no CI como o resto do core.
// TICs APAGADOS: o undo RE-CRIA (o snapshot tem tudo o que o serializer
// gravaria do estado do TIC: nome/pai/visível/transform/mesh/prim/tint).
#include "core/Types.h"
#include "core/Handle.h"
#include "components/Transform3D.h"
#include "components/MeshRenderer.h"
#include "render/Primitives.h"

#include <cstring>
#include <string>

namespace vv {

class Scene;

namespace editor {

// ---- snapshot do estado editável de UM TIC ----------------------------------
struct TicSnap {
    char     name[40] = "";
    i32      parent = -1;
    bool     visible = true;
    // Transform3D (se tinha)
    bool     hasTr = false;
    Vec3     pos{}, scale{1.0f, 1.0f, 1.0f};
    Quat     rot{};
    // MeshRenderer (se tinha)
    bool     hasMr = false;
    char     meshPath[96] = "";
    char     texPath[96] = "";
    f32      tint[3] = {1.0f, 1.0f, 1.0f};
    bool     primOn = false;
    PrimKind primKind = PrimKind::Box;
    f32      primRadius = 0.5f, primSize = 1.0f;
    i32      primSegments = 16, primRings = 8;
};

// captura o snapshot do TIC (vive AQUI — o main e os testes partilham)
TicSnap snapTic(const class Scene& scene, Handle h);

// repõe o snapshot (o handle corrente primeiro — renames/undo de edição;
// re-cria por nome se o TIC morreu — undo de delete). Devolve o handle vivo.
Handle restoreTic(class Scene& scene, const TicSnap& s,
                  Handle current = Handle::invalid());

// cola o snapshot como TIC NOVO (cópia com nome único Godot-style — o PASTE
// da área de transferência; nunca toca no original)
Handle pasteAsNew(class Scene& scene, const TicSnap& s);

// ---- operação -----------------------------------------------------------------
struct UndoOp {
    TicSnap before{};
    TicSnap after{};
    Handle  tic{};           // handle CORRENTE do TIC (o do estado `after`)
    bool    valid = false;   // false = slot livre
};

// ---- a stack (cap fixo; push come a cauda do redo) -----------------------------
struct UndoStack {
    static constexpr u32 kCap = 64;

    UndoOp ops[kCap] = {};
    u32    count = 0;    // ops ocupadas
    u32    cursor = 0;   // posição do redo (count = topo; cursor < count ⇒ há redo)

    bool canUndo() const { return cursor > 0; }
    bool canRedo() const { return cursor < count; }
    void clear() {
        count = 0;
        cursor = 0;
    }
    // empurra (a cauda além do cursor MORRE — padrão undo universal);
    // devolve false se TUDO inválido (operação no-op: before == after)
    bool push(const TicSnap& before, const TicSnap& after, Handle tic);
    // aplica undo/redo à cena; devolve o handle tocado (invalid = nada feito)
    Handle undo(Scene& scene);
    Handle redo(Scene& scene);
};

// comparação utilitária (para o push ignorar no-ops: gizmo sem movimento)
bool snapEq(const TicSnap& a, const TicSnap& b);

} // namespace editor
} // namespace vv
