// tests/test_assetpersist.cpp — F5.4-hotfix: o botão "Salvar" materializa
// os assets que só existem em runtime, na SUBPASTA certa do projeto.
//
// Pedido do dono: criar um TIC com mesh "cube" no editor → Salvar →
// meshes/ tem de conter um ficheiro correspondente com conteúdo real
// (não vazio, formato correto). No device a escrita loga
// "saf: write meshes/cube.obj — N bytes" (linha do SafStorage — visível
// no "Ver logs"); aqui aferimos o ficheiro, o formato e a idempotência.
//
// O cubo procedural é o único asset que não tinha ficheiro: refs de mesh/
// textura importadas JÁ são ficheiros nas suas pastas (importCandidate;
// extractGltfTextures grava textures/gltf_<hash>.png) — o teste cobre
// também a regra "refs que já são ficheiros não tocam".
#include "TestFramework.h"
#include <string>
#include <vector>

#include "FakeStorage.h"
#include "core/AssetPersist.h"
#include "core/Project.h"
#include "core/Scene.h"
#include "components/MeshRenderer.h"
#include "assets/ObjImporter.h"
#include "render/Cube.h"
#include "render/Mesh.h"

using namespace vv;

namespace {

// nomes com sufixo anticolisão no storage (o Salvar NUNCA pode duplicar)
int collisions(const FakeStorage& st) {
    int n = 0;
    for (const auto& kv : st.files) {
        if (kv.first.find(" (1)") != std::string::npos) {
            ++n;
        }
    }
    return n;
}

} // namespace

TEST(asset_persist_cube_materializes_into_meshes) {
    FakeStorage st;
    Project proj;
    EXPECT(Project::openOrCreate(st, "projeto", proj));

    // "editor": TIC com mesh CUBO procedural (mesmo predicado do serializer:
    // meshPath vazio + mesh presente)
    Scene scene;
    const Handle h = scene.create("Cube");
    EXPECT(h.valid());
    Mesh runtimeMesh;                    // recurso GL (stubs no hospedeiro)
    MeshRenderer* mr = scene.get(h)->addComponent<MeshRenderer>();
    EXPECT(mr != nullptr);
    mr->mesh = &runtimeMesh;             // procedural — meshPath fica vazio

    // "Salvar" → materializa
    std::vector<std::string> written;
    std::string err;
    EXPECT(persistSceneAssets(st, scene, written, err));
    EXPECT(err.empty());
    EXPECT(written.size() == 1);
    EXPECT(written[0] == "meshes/cube.obj");

    // meshes/ contém EXATAMENTE o ficheiro certo (nada solto na raiz)
    std::vector<std::string> files;
    EXPECT(st.listDir(Project::kDirMeshes, files));
    EXPECT(files.size() == 1);
    EXPECT(files[0] == "cube.obj");

    // conteúdo REAL: não vazio, formato OBJ já definido no projeto
    // (cabeçalho do exportObj + round-trip com a geometria do cubo)
    std::string obj;
    EXPECT(st.readText("meshes/cube.obj", obj));
    EXPECT(!obj.empty());
    EXPECT(obj.find("# exportado pela G.One VV") == 0);
    const CubeMeshData c = makeCube(1.0f);
    MeshData cube;
    cube.vertices.assign(c.vertices.begin(), c.vertices.end());
    cube.indices.assign(c.indices.begin(), c.indices.end());
    MeshData back;
    EXPECT(parseObj(obj.data(), obj.size(), back, err));
    EXPECT(back.vertices.size() == cube.vertices.size());   // 24
    EXPECT(back.indices.size() == cube.indices.size());     // 36

    // re-Salvar: idempotente — nada reescrito, nenhum " (1)" no projeto
    EXPECT(persistSceneAssets(st, scene, written, err));
    EXPECT(written.empty());
    EXPECT(collisions(st) == 0);
    files.clear();
    EXPECT(st.listDir(Project::kDirMeshes, files));
    EXPECT(files.size() == 1);

    // e o Salvar do PROJETO (cena + manifesto) continua coeso com o asset:
    EXPECT(proj.saveActiveScene(st, scene));
    EXPECT(proj.saveManifest(st));
    files.clear();
    EXPECT(st.listDir(Project::kDirMeshes, files));
    EXPECT(files.size() == 1);
    EXPECT(files[0] == "cube.obj");
}

TEST(asset_persist_skips_when_nothing_is_runtime) {
    FakeStorage st;
    Project proj;
    EXPECT(Project::openOrCreate(st, "projeto", proj));

    // cena 1: TIC SEM MeshRenderer → nada a materializar
    Scene scene;
    EXPECT(scene.create("Vazio").valid());
    std::vector<std::string> written;
    std::string err;
    EXPECT(persistSceneAssets(st, scene, written, err));
    EXPECT(written.empty());

    // cena 2: TIC com mesh IMPORTADO (ref = ficheiro que já existe na
    // pasta certa) + textura importada → nada em-runtime → nada gravado
    EXPECT(st.writeText("meshes/robot.obj", "v 0 0 0\n"));
    Scene scene2;
    const Handle h2 = scene2.create("Robot");
    MeshRenderer* mr = scene2.get(h2)->addComponent<MeshRenderer>();
    mr->meshPath = "meshes/robot.obj";
    mr->texPath = "textures/base.png";
    EXPECT(persistSceneAssets(st, scene2, written, err));
    EXPECT(written.empty());

    // o ficheiro do cubo NÃO aparece (a cena não usa o cubo procedural)
    std::vector<std::string> files;
    EXPECT(st.listDir(Project::kDirMeshes, files));
    EXPECT(files.size() == 1);
    EXPECT(files[0] == "robot.obj");
    EXPECT(collisions(st) == 0);
}

TEST(asset_persist_reports_write_failure_honestly) {
    FakeStorage st;
    st.failProbe = true;    // verificação indecidida (provider em falha)
    st.failWrites = true;   // e a escrita em si também falha

    Scene scene;
    const Handle h = scene.create("Cube");
    Mesh runtimeMesh;
    MeshRenderer* mr = scene.get(h)->addComponent<MeshRenderer>();
    mr->mesh = &runtimeMesh;

    std::vector<std::string> written;
    std::string err;
    // falha HONESTA com a causa — sem criar ficheiro nenhum nem fingir
    // sucesso (o Salvar loga o erro; a cena continua salva)
    EXPECT(!persistSceneAssets(st, scene, written, err));
    EXPECT(written.empty());
    EXPECT(!err.empty());
    EXPECT(collisions(st) == 0);
}
