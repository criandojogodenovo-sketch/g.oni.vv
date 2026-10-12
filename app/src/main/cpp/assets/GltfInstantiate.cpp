// assets/GltfInstantiate.cpp — GltfModel → TICs (F5-C).
//
// 0.10-M HOTFIX P0 — UMA FONTE DE VERDADE para a configuração das peças:
// o one-shot `gltfExpandInstantiate` de sempre e o lote por frame
// `GltfExpandBatch` (o P0 do dono: a UI não trava) partilham o MESMO
// `gltfExpandConfigureOne` — o resultado final é o MESMO estado (o pin
// compara; a mutação «na thread de UI» morre na LEI DO LOTE do tick).
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

// 0.10-M HOTFIX P0 — configura O REGISTO i (a 2.ª passada de sempre, um
// registo por chamada): parent + TRS + MeshRenderer + bind. O TRS vem
// LOCAL quando parentRec >= 0 — o mundo compõe; MUNDO quando flat, o de
// sempre. O bindMesh injetado recebe a REF DA PEÇA; nullptr NÃO aborta
// (o meshPath fica — o reload re-liga). Chamado pelo lote E pelo one-shot
// (a fonte única — a ordem dos registos é o mesmo contrato dos pins).
static void gltfExpandConfigureOne(
        Scene& scene, const std::vector<convert::Output::NodeOut>& nodes,
        const std::vector<std::string>& refs, const GltfInstantiateCtx& ctx,
        const std::vector<Handle>& created, size_t i) {
    if (!created[i].valid()) {
        return;   // registro sem peça (estrutural) — não vira TIC
    }
    Tic* t = scene.get(created[i]);
    if (!t) {
        return;
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
        tr->updateWorld();   // provisório para pais — composto no fim
    }
    const std::string& ref = refs[static_cast<size_t>(n.mesh)];
    MeshRenderer mr;
    mr.meshPath = ref;   // a ref vive no componente mesmo sem bind
    mr.mesh = ctx.bindMesh ? ctx.bindMesh(ctx.user, ref) : nullptr;
    mr.material = mr.mesh ? ctx.material : nullptr;
    t->addComponent<MeshRenderer>(mr);
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
// 0.10-M HOTFIX P0: o one-shot é o WRAPPER do lote (todas as ticks em
// sequência — o MESMO código, o MESMO estado final; o pin compara).
std::vector<Handle> gltfExpandInstantiate(
        Scene& scene, const std::vector<convert::Output::NodeOut>& nodes,
        const std::vector<std::string>& refs,
        const GltfInstantiateCtx& ctx) {
    GltfExpandBatch job =
        GltfExpandBatch::start(scene, nodes, refs, ctx);
    while (!job.tick()) {
        // o one-shot esgota o lote (o dono do import FUNDIDO não tem
        // overlay; o import EXPANDIDO pelo main usa o job POR FRAME)
    }
    gltfExpandComposeWorlds(nodes, job.handles(), scene);
    return job.handles();
}

// 0.10-M HOTFIX P0 — o LOTE (a cura (a)/(d) do dono: fora da thread de UI
// por lotes com progresso; a explosão de N TICs num frame morre AQUI).
GltfExpandBatch GltfExpandBatch::start(
        Scene& scene, const std::vector<convert::Output::NodeOut>& nodes,
        const std::vector<std::string>& refs,
        const GltfInstantiateCtx& ctx) {
    GltfExpandBatch job;
    job.scene = &scene;
    job.nodes = &nodes;
    job.refs = &refs;
    job.ctx = &ctx;
    job.created.reserve(nodes.size());
    // 1.ª passada: TODOS os TICs (barata — só scene.create; o MESMO nome
    // e a MESMA ordem do one-shot de sempre)
    for (const convert::Output::NodeOut& n : nodes) {
        if (n.mesh < 0 || n.mesh >= static_cast<i32>(refs.size())) {
            job.created.push_back(Handle::invalid());
            continue;   // registro sem peça (estrutural) — não vira TIC
        }
        const std::string name =
            n.name.empty() ? ("node " + std::to_string(n.node)) : n.name;
        job.created.push_back(scene.create(name));
    }
    // o total do progresso (os registos COM peça — os estruturais não
    // contam: o overlay diz peças de verdade)
    for (const convert::Output::NodeOut& n : nodes) {
        if (n.mesh >= 0 && n.mesh < static_cast<i32>(refs.size())) {
            ++job.total_;
        }
    }
    job.finished = nodes.empty();
    return job;
}

bool GltfExpandBatch::tick() {
    if (finished || canceled || scene == nullptr || nodes == nullptr ||
        refs == nullptr || ctx == nullptr) {
        return true;
    }
    // A LEI DO LOTE: no máximo kGltfExpandBatchTics registos configurados
    // por tick — NUNCA tudo num só (a mutação M-P0b «expansão na thread
    // de UI» morre AQUI; o pin do lote aferiu)
    const size_t end = next + kGltfExpandBatchTics;
    const size_t n = nodes->size();
    for (; next < n && next < end; ++next) {
        const convert::Output::NodeOut& rec = (*nodes)[next];
        const bool isPiece =
            rec.mesh >= 0 && rec.mesh < static_cast<i32>(refs->size());
        gltfExpandConfigureOne(*scene, *nodes, *refs, *ctx, created, next);
        if (isPiece && created[next].valid()) {
            ++done_;
            if (perPiece != nullptr) {
                perPiece(*scene, created[next], perPieceUser);
            }
        }
    }
    if (next >= n) {
        finished = true;
    }
    return finished;
}

void GltfExpandBatch::cancel() {
    if (canceled || scene == nullptr) {
        return;
    }
    // o cancelamento LIMPO: os TICs criados MORREM (ordem inversa — os
    // filhos primeiro; o Scene::destroy desanexa os componentes e devolve
    // o slot) — a cena fica COMO ESTAVA antes do expand
    for (size_t i = created.size(); i-- > 0;) {
        if (created[i].valid()) {
            scene->destroy(created[i]);
        }
    }
    created.clear();
    next = 0;
    done_ = 0;
    total_ = 0;
    canceled = true;
}

// 0.10-M HOTFIX (f) — a composição dos mundos (memo — um pai pode ter
// índice maior; a MESMA matemática do TransformSystem: parentWorld ∘ TRS).
// 0.10-M HOTFIX P0: ITERATIVA (o stack explícito) — a cadeia de 5000 era
// 5000 frames de recursão de std::function na thread da UI; o resultado é
// o MESMO (a ordem parent-primeiro e o memo de entrada mantêm-se — o pin
// da sub-árvore compara).
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
    std::vector<size_t> stack;
    for (size_t i = 0; i < created.size() && i < nodes.size(); ++i) {
        if (composed[i] || trs[i] == nullptr) {
            continue;
        }
        composed[i] = 1;   // o memo de ENTRADA (o mesmo do recursivo)
        stack.push_back(i);
        while (!stack.empty()) {
            const size_t top = stack.back();
            const i32 p =
                top < nodes.size() ? nodes[top].parentRec : -1;
            const bool parentLinkable =
                p >= 0 && p < static_cast<i32>(created.size()) &&
                created[static_cast<size_t>(p)].valid() &&
                trs[static_cast<size_t>(p)] != nullptr;
            if (parentLinkable && !composed[static_cast<size_t>(p)]) {
                composed[static_cast<size_t>(p)] = 1;
                stack.push_back(static_cast<size_t>(p));   // o PAI primeiro
                continue;
            }
            if (parentLinkable) {
                trs[top]->world = Mat4::mul(
                    trs[static_cast<size_t>(p)]->world,
                    trs[top]->computeMatrix());
                trs[top]->worldDirty = false;
            } else {
                trs[top]->updateWorld();
            }
            stack.pop_back();
        }
    }
}

} // namespace vv
