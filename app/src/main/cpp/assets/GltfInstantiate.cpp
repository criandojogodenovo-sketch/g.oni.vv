// assets/GltfInstantiate.cpp — GltfModel → TICs (F5-C).
#include "assets/GltfInstantiate.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/Scene.h"

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

// 0.10-M (EXT) — os TICs do import EXPANDIDO: um por nó com mesh, FLAT
// (o NodeOut traz a transformação MUNDO já decomposta — o engine compõe
// flat e a peça desenha no sítio exato sem hierarquia; a geometria da
// peça ficou CRUA no espaço local do nó). O bindMesh injetado recebe a
// REF DA PEÇA; nullptr NÃO aborta (o meshPath fica — o reload re-liga).
// Devolve os handles NA ORDEM dos registros (o pino do dono conta ISTO).
std::vector<Handle> gltfExpandInstantiate(
        Scene& scene, const std::vector<convert::Output::NodeOut>& nodes,
        const std::vector<std::string>& refs,
        const GltfInstantiateCtx& ctx) {
    std::vector<Handle> created;
    for (const convert::Output::NodeOut& n : nodes) {
        if (n.mesh < 0 || n.mesh >= static_cast<i32>(refs.size())) {
            continue;   // registro sem peça (estrutural) — não vira TIC
        }
        const std::string name =
            n.name.empty() ? ("node " + std::to_string(n.node)) : n.name;
        const Handle h = scene.create(name);
        created.push_back(h);
        Tic* t = scene.get(h);
        if (!t) {
            continue;
        }
        if (Transform3D* tr = t->addComponent<Transform3D>()) {
            tr->pos = n.translation;
            tr->rot = n.rotation;
            tr->scale = n.scale;
            tr->updateWorld();
        }
        const std::string& ref = refs[static_cast<size_t>(n.mesh)];
        MeshRenderer mr;
        mr.meshPath = ref;   // a ref vive no componente mesmo sem bind
        mr.mesh = ctx.bindMesh ? ctx.bindMesh(ctx.user, ref) : nullptr;
        mr.material = mr.mesh ? ctx.material : nullptr;
        t->addComponent<MeshRenderer>(mr);
    }
    return created;
}

} // namespace vv
