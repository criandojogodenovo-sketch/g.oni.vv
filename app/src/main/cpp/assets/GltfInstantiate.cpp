// assets/GltfInstantiate.cpp — GltfModel → TICs (F5-C).
#include "assets/GltfInstantiate.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/Scene.h"

#include <functional>

namespace vv {

Handle gltfInstantiate(Scene& scene, const GltfModel& model,
                       const std::string& basePath,
                       const GltfInstantiateCtx& ctx) {
    if (model.nodes.empty()) {
        return Handle::invalid();
    }

    // 1ª passada: cria TODOS os TICs (ordem do array — filhos podem vir
    // antes dos pais; os links ficam p/ a 2ª passada)
    std::vector<Handle> created(model.nodes.size(), Handle::invalid());
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        const GltfNode& n = model.nodes[i];
        const std::string name =
            n.name.empty() ? ("node " + std::to_string(i)) : n.name;
        created[i] = scene.create(name);
    }

    // 2ª passada: parent + TRS + MeshRenderer
    for (size_t i = 0; i < model.nodes.size(); ++i) {
        const GltfNode& n = model.nodes[i];
        Tic* t = scene.get(created[i]);
        if (!t) {
            continue;
        }
        if (n.parent >= 0 &&
            n.parent < static_cast<i32>(created.size()) &&
            created[static_cast<size_t>(n.parent)].valid()) {
            t->parent = created[static_cast<size_t>(n.parent)].index;
        }
        if (Transform3D* tr = t->addComponent<Transform3D>()) {
            tr->pos = n.translation;
            tr->rot = n.rotation;
            tr->scale = n.scale;
            tr->updateWorld();
        }
        if (n.mesh >= 0 && n.mesh < static_cast<i32>(model.meshes.size())) {
            const std::string ref = basePath + "#" + std::to_string(n.mesh);
            MeshRenderer mr;
            mr.meshPath = ref;   // ref vive no componente mesmo sem bind
            mr.mesh = ctx.bindMesh ? ctx.bindMesh(ctx.user, ref) : nullptr;
            mr.material = mr.mesh ? ctx.material : nullptr;
            t->addComponent<MeshRenderer>(mr);
        }
    }
    return created[0];
}

// 0.10-M (EXT) — os TICs do import EXPANDIDO: um por nó com mesh. O TRS
// vinha MUNDO (flat — o NodeOut trazia a transformação decomposta e o
// engine compunha flat). 0.10-M HOTFIX (e/f — a spec do dono): a SUB-
// ÁRVORE é REAL — `parentRec` >= 0 pendura o TIC no ancestral registado
// (os nós vazios não viram TICs — o pin do 5B mantém-se) com o TRS LOCAL
// da cadeia; o TransformSystem compõe parent∘local a cada tick E AQUI em
// baixo (o fit do import lê tr->world LOGO — não pode esperar o tick).
// parentRec = -1 mantém o flat de sempre (TRS MUNDO nos campos) — os nós
// com shear continuam exatos sem hierarquia (o log do conversor diz).
// O bindMesh injetado recebe a REF DA PEÇA; nullptr NÃO aborta (o meshPath
// fica — o reload re-liga). Devolve os handles NA ORDEM dos registos (o
// pino do dono conta ISTO).
std::vector<Handle> gltfExpandInstantiate(
        Scene& scene, const std::vector<convert::Output::NodeOut>& nodes,
        const std::vector<std::string>& refs,
        const GltfInstantiateCtx& ctx) {
    std::vector<Handle> created;
    created.reserve(nodes.size());
    // 1.ª passada: TODOS os TICs (a ordem dos registos — um pai pode ter
    // índice MAIOR que o filho; os links ficam para a 2.ª)
    for (const convert::Output::NodeOut& n : nodes) {
        if (n.mesh < 0 || n.mesh >= static_cast<i32>(refs.size())) {
            created.push_back(Handle::invalid());
            continue;   // registro sem peça (estrutural) — não vira TIC
        }
        const std::string name =
            n.name.empty() ? ("node " + std::to_string(n.node)) : n.name;
        created.push_back(scene.create(name));
    }
    // 2.ª passada: parent + TRS + MeshRenderer (o TRS vem LOCAL quando
    // parentRec >= 0 — o mundo compõe; MUNDO quando flat, o de sempre)
    std::vector<Transform3D*> trs(created.size(), nullptr);
    for (size_t i = 0; i < nodes.size(); ++i) {
        if (!created[i].valid()) {
            continue;
        }
        Tic* t = scene.get(created[i]);
        if (!t) {
            continue;
        }
        const convert::Output::NodeOut& n = nodes[i];
        if (n.parentRec >= 0 &&
            n.parentRec < static_cast<i32>(created.size()) &&
            created[static_cast<size_t>(n.parentRec)].valid()) {
            t->parent = created[static_cast<size_t>(n.parentRec)].index;
        }
        if (Transform3D* tr = t->addComponent<Transform3D>()) {
            tr->pos = n.translation;
            tr->rot = n.rotation;
            tr->scale = n.scale;
            tr->updateWorld();   // provisório para pais — composto em baixo
            trs[i] = tr;
        }
        const std::string& ref = refs[static_cast<size_t>(n.mesh)];
        MeshRenderer mr;
        mr.meshPath = ref;   // a ref vive no componente mesmo sem bind
        mr.mesh = ctx.bindMesh ? ctx.bindMesh(ctx.user, ref) : nullptr;
        mr.material = mr.mesh ? ctx.material : nullptr;
        t->addComponent<MeshRenderer>(mr);
    }
    // 3.ª passada: a COMPOSIÇÃO dos mundos (o helper público — o fit do
    // main recomps depois de escalar as raízes)
    gltfExpandComposeWorlds(nodes, created, scene);
    return created;
}

// 0.10-M HOTFIX (f) — a composição dos mundos (memo — um pai pode ter
// índice maior; a MESMA matemática do TransformSystem: parentWorld ∘ TRS)
void gltfExpandComposeWorlds(
        const std::vector<convert::Output::NodeOut>& nodes,
        const std::vector<Handle>& created, Scene& scene) {
    std::vector<Transform3D*> trs(created.size(), nullptr);
    for (size_t i = 0; i < created.size() && i < nodes.size(); ++i) {
        if (!created[i].valid()) {
            continue;
        }
        if (Tic* t = scene.get(created[i])) {
            trs[i] = t->getComponent<Transform3D>();
        }
    }
    std::vector<char> composed(created.size(), 0);
    std::function<void(size_t)> compose = [&](size_t i) {
        if (composed[i] || trs[i] == nullptr) {
            return;
        }
        composed[i] = 1;
        const i32 p = nodes[i].parentRec;
        if (p >= 0 && p < static_cast<i32>(created.size()) &&
            created[static_cast<size_t>(p)].valid()) {
            compose(static_cast<size_t>(p));
            if (trs[static_cast<size_t>(p)] != nullptr) {
                trs[i]->world = Mat4::mul(
                    trs[static_cast<size_t>(p)]->world,
                    trs[i]->computeMatrix());
                trs[i]->worldDirty = false;
                return;
            }
        }
        trs[i]->updateWorld();
    };
    for (size_t i = 0; i < created.size() && i < nodes.size(); ++i) {
        compose(i);
    }
}

} // namespace vv
