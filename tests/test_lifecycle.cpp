// tests/test_lifecycle.cpp — 0.6.7: LIFECYCLE GL (term+init sem matar a app).
//
// REGRESSÃO DO C33 (o bug dos "cubinhos"): sair do editor (home/recents) e
// voltar SEM matar a app deixa TODO o texto em quads brancos — o contexto
// EGL é destruído no APP_CMD_TERM_WINDOW (EglContext::shutdown mata surface
// E contexto) mas o FontAtlas guardava tex_ != 0 (guard "já carregado") e o
// 2º INIT_WINDOW nunca re-upa o atlas; o mesmo acontecia aos mapas do
// GpuAssets (Mesh*/Texture* com handles órfãos).
//
// Aqui aferimos o CONTRATO novo, com os contadores do stub GL
// (glstub::stats — tests/stub/GLES3/gl3.h):
//   • destroy() liberta o recurso e regressa ao estado pré-bake;
//   • loadFromPaths() após destroy() = RE-BAKE + RE-UPLOAD (texImage2D 2×);
//   • o guard continua a impedir o upload DUPLICADO no MESMO contexto;
//   • Mesh/GpuAssets/Renderer re-criam os recursos GL após destroy;
//   • sair→reentrar: a cena recarregada do disco re-liga os MeshRenderers
//     (cena preservada) e o texto tem UVs válidos (renderiza).
#include "TestFramework.h"
#include <GLES3/gl3.h>   // stub do hospedeiro (glstub::stats)
#include <cstdio>
#include <string>
#include <vector>
#include "assets/ResourceManager.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/Project.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "core/Presets.h"
#include "render/Cube.h"
#include "render/GpuAssets.h"
#include "render/Mesh.h"
#include "render/Renderer.h"
#include "ui/FontAtlas.h"
#include "FakeStorage.h"

using namespace vv;
using ::test::nearEqF;

namespace {
const char* kFontPath = FONT_FIXTURE;
}

// ---- atlas: destroy + re-upload (o coração do fix) --------------------------

TEST(lifecycle_atlas_re_uploadado_apos_term_e_init) {
    glstub::reset();
    FontAtlas f;
    EXPECT(f.loadFromPaths(&kFontPath, 1, 28.0f));
    EXPECT(f.ok());
    EXPECT(glstub::stats.texImage2D == 1);   // 1º upload (contexto 1)
    EXPECT(glstub::stats.genTextures == 1);

    // TERM_WINDOW: destroy com o contexto ainda corrente
    f.destroy();
    EXPECT(!f.ok());
    EXPECT(glstub::stats.deleteTextures == 1);
    EXPECT(nearEqF(f.ascent(), 0.0f));       // métricas de volta ao pré-bake
    EXPECT(nearEqF(f.height(), 0.0f));

    // INIT_WINDOW (contexto NOVO): re-bake + re-upload — NÃO pode saltar
    EXPECT(f.loadFromPaths(&kFontPath, 1, 28.0f));
    EXPECT(f.ok());
    EXPECT(glstub::stats.texImage2D == 2);   // ← o upload que faltava
    EXPECT(glstub::stats.genTextures == 2);
    EXPECT(f.ascent() > 14.0f && f.ascent() < 26.0f);   // métricas re-medidas
}

TEST(lifecycle_atlas_guard_impede_upload_duplicado_no_mesmo_contexto) {
    glstub::reset();
    FontAtlas f;
    EXPECT(f.loadFromPaths(&kFontPath, 1, 28.0f));
    EXPECT(f.loadFromPaths(&kFontPath, 1, 28.0f));   // 2ª chamada SEM term
    EXPECT(glstub::stats.texImage2D == 1);          // 1 upload só
    EXPECT(glstub::stats.genTextures == 1);
}

TEST(lifecycle_atlas_glifos_uvs_validos_apos_re_upload) {
    glstub::reset();
    FontAtlas f;
    EXPECT(f.loadFromPaths(&kFontPath, 1, 28.0f));
    f.destroy();
    EXPECT(f.loadFromPaths(&kFontPath, 1, 28.0f));
    // TODO o range ASCII tem UV coerentes no atlas NOVO (o bug: UVs
    // apontavam para uma textura órfã → quads brancos). O espaço (32) não
    // tem tinta — u0==u1 é LEGAL só para ele.
    for (u32 c = FontAtlas::kFirstChar;
         c < FontAtlas::kFirstChar + FontAtlas::kNumChars; ++c) {
        const Glyph& g = f.glyph(static_cast<char>(c));
        if (c == FontAtlas::kFirstChar) {
            EXPECT(g.u1 >= g.u0);
            EXPECT(g.v1 >= g.v0);
            continue;
        }
        EXPECT(g.u1 > g.u0);
        EXPECT(g.v1 > g.v0);
        EXPECT(g.u0 >= 0.0f && g.u1 <= 1.0f);
        EXPECT(g.v0 >= 0.0f && g.v1 <= 1.0f);
    }
    EXPECT(f.widthOf("G.One VV 0.6.7") > 0.0f);
}

// ---- meshes GL: re-criação após destroy --------------------------------------

TEST(lifecycle_mesh_destroy_recreate_no_novo_contexto) {
    glstub::reset();
    const CubeMeshData cube = makeCube(1.0f);
    Mesh m;
    EXPECT(m.create(cube.vertices.data(),
                    static_cast<u32>(cube.vertices.size()),
                    cube.indices.data(),
                    static_cast<u32>(cube.indices.size())));
    EXPECT(m.ok());

    m.destroy();   // TERM_WINDOW (ids zerados — nunca reutilizados)
    EXPECT(!m.ok());
    EXPECT(glstub::stats.deleteVertexArrays >= 1);
    EXPECT(glstub::stats.deleteBuffers >= 2);

    EXPECT(m.create(cube.vertices.data(),
                    static_cast<u32>(cube.vertices.size()),
                    cube.indices.data(),
                    static_cast<u32>(cube.indices.size())));
    EXPECT(m.ok());   // re-upload no contexto novo
}

TEST(lifecycle_gpu_release_all_e_re_upload_dos_assets) {
    glstub::reset();
    FakeStorage st;
    // 1 quad OBJ no storage do projeto
    const char* obj =
        "o quad\n"
        "v -1 0 -1\nv 1 0 -1\nv 1 0 1\nv -1 0 1\n"
        "vn 0 1 0\nvt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
        "f 1/1/1 2/2/1 3/3/1\nf 1/1/1 3/3/1 4/4/1\n";
    st.writeText("meshes/quad.obj", obj);

    ResourceManager rm;
    rm.setStorage(&st);
    GpuAssets gpu;
    gpu.init(&rm);
    Mesh* m1 = gpu.mesh("meshes/quad.obj");
    EXPECT(m1 != nullptr);
    EXPECT(m1->ok());
    EXPECT(gpu.meshCount() == 1);

    // TERM_WINDOW: releaseAll apaga os objetos (glDelete* pelos destrutores)
    gpu.releaseAll();
    EXPECT(gpu.meshCount() == 0);
    EXPECT(gpu.textureCount() == 0);

    // INIT_WINDOW: o resolver re-upa — objeto GL NOVO (meshCount volta a 1).
    // Nota: o endereço de m1 pode ser REUTILIZADO pelo allocator — o que
    // importa é o ciclo release→re-upload, não o ponteiro.
    Mesh* m2 = gpu.mesh("meshes/quad.obj");
    EXPECT(m2 != nullptr);
    EXPECT(m2->ok());
    EXPECT(gpu.meshCount() == 1);
}

// ---- renderer: shutdown + re-init (programas/VBO/whiteTex) -------------------

TEST(lifecycle_renderer_shutdown_e_reinit) {
    glstub::reset();
    Renderer r;
    EXPECT(r.init());
    EXPECT(glstub::stats.createProgram >= 1);   // programa UI (+ lit)
    r.shutdown();
    EXPECT(glstub::stats.deleteProgram >= 1);
    // INIT_WINDOW de novo: re-cria o programa e o whiteTex — endFrame com
    // batches vazios não pode crashar
    EXPECT(r.init());
    QuadBatch vazio;
    r.submit(vazio, r.whiteTexture());
    r.endFrame();
}

// ---- fluxo completo sair→reentrar (cena preservada + texto renderiza) --------

TEST(lifecycle_sair_reentrar_cena_preservada_e_meshes_religados) {
    glstub::reset();
    FakeStorage st;
    Project p;
    EXPECT(Project::openOrCreate(st, "lifecycle", p));

    // editor: 2 TICs com poses distintas → save (com o cubo procedural
    // ligado, como o main faz: createTicFromPreset(scene, kind, &cubeMesh))
    Mesh cubeMesh;
    {
        const CubeMeshData cube = makeCube(1.0f);
        EXPECT(cubeMesh.create(cube.vertices.data(),
                               static_cast<u32>(cube.vertices.size()),
                               cube.indices.data(),
                               static_cast<u32>(cube.indices.size())));
    }
    Scene s;
    const Handle h1 = createTicFromPreset(s, PresetKind::PlayerBody3D,
                                          &cubeMesh, nullptr);
    const Handle h2 = createTicFromPreset(s, PresetKind::StaticBody3D,
                                          &cubeMesh, nullptr);
    Tic* t1 = s.get(h1);
    Tic* t2 = s.get(h2);
    EXPECT(t1 && t2);
    t1->name = "player";
    t2->name = "chao";
    if (Transform3D* tr = t1->getComponent<Transform3D>()) {
        tr->pos = Vec3{2.0f, 3.0f, -4.0f};
        tr->updateWorld();
    }
    if (Transform3D* tr = t2->getComponent<Transform3D>()) {
        tr->pos = Vec3{0.0f, -1.0f, 0.0f};
        tr->updateWorld();
    }
    EXPECT(p.saveActiveScene(st, s));
    EXPECT(p.saveManifest(st));

    // ---- "sair" (TERM_WINDOW): recursos GL mortos; cena em memória perde
    // os ponteiros de GPU (política do main: detach + releaseAll) ----
    {
        // o cubo procedural é um objeto ESTÁTICO re-criado NO SITIO no
        // próximo init (Mesh::destroy só zera os ids GL)
        cubeMesh.destroy();
        const CubeMeshData cube = makeCube(1.0f);
        EXPECT(cubeMesh.create(cube.vertices.data(),
                               static_cast<u32>(cube.vertices.size()),
                               cube.indices.data(),
                               static_cast<u32>(cube.indices.size())));
    }

    // ---- "reentrar" (INIT_WINDOW): storage vivo → cena recarregada ----
    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ctx.cubeMesh = &cubeMesh;
    EXPECT(p.loadActiveScene(st, s2, ctx));
    EXPECT(s2.count() == 2);

    // TICs preservados com nomes e poses do save
    bool achouPlayer = false, achouChao = false;
    s2.forEachActive([&](const Tic& t) {
        if (std::string(t.name) == "player") {
            achouPlayer = true;
            const Transform3D* tr =
                s2.components().transforms().find(t.handle);
            EXPECT(tr != nullptr);
            EXPECT(nearEqF(tr->pos.x, 2.0f));
            EXPECT(nearEqF(tr->pos.y, 3.0f));
            EXPECT(nearEqF(tr->pos.z, -4.0f));
        }
        if (std::string(t.name) == "chao") {
            achouChao = true;
            const Transform3D* tr =
                s2.components().transforms().find(t.handle);
            EXPECT(tr != nullptr);
            EXPECT(nearEqF(tr->pos.y, -1.0f));
        }
    });
    EXPECT(achouPlayer);
    EXPECT(achouChao);

    // MeshRenderers re-ligados ao cubo re-criado (tag "cube" → LoadCtx)
    const auto& mrs = s2.components().meshRenderers();
    EXPECT(mrs.size() == 2);
    for (u32 i = 0; i < mrs.size(); ++i) {
        EXPECT(mrs.at(i).mesh == &cubeMesh);
        EXPECT(mrs.at(i).mesh != nullptr && mrs.at(i).mesh->ok());
    }
}
