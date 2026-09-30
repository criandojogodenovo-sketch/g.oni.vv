// core/AssetPersist.cpp — materialização dos assets em-runtime (F5.4-hotfix).
//
// Hoje: o CUBO PROCEDURAL (presets/addComponent — meshPath vazio + mesh
// presente) não tinha ficheiro nenhum no projeto: "Salvar" gravava cena +
// manifesto e o mesh vivia só na GPU. Agora o Salvar deixa
// meshes/cube.obj na pasta de meshes (formato OBJ já definido — o MESMO
// exportObj do menu File), com o log "saf: write meshes/cube.obj — N bytes"
// (device) / "file: write …" (app-private) a permitir confirmar no
// "Ver logs" sem abrir o gestor de ficheiros.
#include "core/AssetPersist.h"
#include "core/Scene.h"
#include "core/Tic.h"
#include "components/MeshRenderer.h"
#include "render/Cube.h"
#include "assets/Assets.h"
#include "assets/ObjExporter.h"

namespace vv {

bool persistSceneAssets(ProjectStorage& st, const Scene& scene,
                        std::vector<std::string>& outWritten, std::string& err) {
    outWritten.clear();
    err.clear();

    // a cena referenciam o cubo procedural? (MESMO predicado do serializer:
    // meshPath vazio + mesh presente → tag "cube")
    bool needsCube = false;
    scene.forEachActive([&](const Tic& t) {
        if (needsCube) {
            return;
        }
        const MeshRenderer* mr = t.getComponent<MeshRenderer>();
        if (mr && mr->meshPath.empty() && mr->mesh != nullptr) {
            needsCube = true;
        }
    });
    if (!needsCube) {
        return true;   // nada em-runtime a materializar (cena sem cubo)
    }

    // idempotente: já materializado → não se sobrescreve (o asset no disco
    // pode ter sido editado; Salvar nunca faz churn no que já lá está)
    if (st.probe(kCubeAssetRel) == Presence::Present) {
        return true;
    }

    // geometria EXATA do cubo runtime (a mesma do Export OBJ do menu)
    const CubeMeshData c = makeCube(1.0f);
    MeshData m;
    m.name = "cube";
    m.vertices.assign(c.vertices.begin(), c.vertices.end());
    m.indices.assign(c.indices.begin(), c.indices.end());
    MeshData::Group g;
    g.name = "cube";
    g.firstIndex = 0;
    g.indexCount = static_cast<u32>(m.indices.size());
    m.groups.push_back(g);

    const std::string obj = exportObj(m);
    if (obj.empty()) {
        err = "asset: exportObj do cubo devolveu vazio";
        return false;
    }
    if (!st.writeText(kCubeAssetRel, obj)) {
        err = "asset: falha ao gravar " + std::string(kCubeAssetRel)
                    + " (causa no engine.log)";
        return false;
    }
    outWritten.push_back(kCubeAssetRel);
    return true;
}

} // namespace vv
