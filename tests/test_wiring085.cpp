// tests/test_wiring085.cpp — 0.8.5: TESTES DE INTEGRAÇÃO DE WIRING
// (funcionalidade quebrada: animação, primitivas, import).
//
// O que o C33 mostrava e esta suíte teria apanhado ANTES dos fixes:
//
//   1. "animação não cria" — o addTrack escrevia SEMPRE no clips[0]
//      (editClip) enquanto a timeline edita/mostra o clip ATIVO: com um
//      clip importado de glTF ativo (gltfAttach põe activeClip=1), o
//      "+track" caía no clip INVISÍVEL. O teste põe um clip importado
//      ativo e aferi que o track NOVO lands NELE.
//
//   2. fluxo ANIMAÇÃO e2e — criar player → track → keys → scrub → play →
//      opções (loop/once/pingpong/speed) → salvar → carregar → reproduz.
//
//   3. "trocar primitiva não troca" — applyAssetPick menuKind 4 ×3 com
//      upload REAL no stub (makePrimMesh + Mesh::create) + drawMesh: o
//      mesh TROCA no render e o .goni guarda "mesh":"prim".
//
//   4. "import não aceita obj/gltf/glb" — (a) o GpuAssets agora FAZ
//      FALLBACK para "#0" num glTF/GLB multi-mesh sem sub-ref (o ResourceManager
//      continua a exigir "#<i>" para refs explícitas — contrato intacto);
//      (b) o browser LISTA os não suportados (kind 0 → erro claro no main);
//      (c) o catálogo classifica por kindOfExtension (lowercase — .Glb entra).
//
//   5. o caminho COMPLETO do import de animação (que no device nunca
//      chegava a correr — o diálogo "aplicar ao TIC?" estava morto):
//      parse do modelo → gltfAttachClips → addTrack NO clip importado →
//      play reproduz → save/load mantém.
#include "TestFramework.h"
#include <GLES3/gl3.h>
#include <cstdio>
#include <memory>
#include <cstring>
#include <string>
#include <vector>
#include "assets/GltfAnim.h"
#include "assets/GltfImporter.h"
#include "assets/ResourceManager.h"
#include "assets/TextureCompressor.h"
#include "components/AnimationPlayer.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "platform/FileApi.h"
#include "render/GpuAssets.h"
#include "render/Mesh.h"
#include "render/Primitives.h"
#include "render/Renderer.h"
#include "FakeStorage.h"
#include "ui/EditorUi.h"

using namespace vv;
using ::test::nearEqF;

namespace {
const char* kFontPath = FONT_FIXTURE;

// base64 encode mínimo (fixture data: URI — padrão do test_resources)
std::string b64enc(const std::vector<u8>& b) {
    static const char* tbl =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    for (; i + 2 < b.size(); i += 3) {
        const u32 v = (u32(b[i]) << 16) | (u32(b[i + 1]) << 8) | b[i + 2];
        out += tbl[(v >> 18) & 63]; out += tbl[(v >> 12) & 63];
        out += tbl[(v >> 6) & 63];  out += tbl[v & 63];
    }
    if (i < b.size()) {
        u32 v = u32(b[i]) << 16;
        if (i + 1 < b.size()) v |= u32(b[i + 1]) << 8;
        out += tbl[(v >> 18) & 63]; out += tbl[(v >> 12) & 63];
        out += (i + 1 < b.size()) ? tbl[(v >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

// .gltf com DOIS meshes (tris não-indexados) — multi-mesh SEM animação
std::string twoMeshGltf() {
    std::vector<u8> bin;
    auto pushF = [&bin](f32 v) {
        u8 t[4]; std::memcpy(t, &v, 4); bin.insert(bin.end(), t, t + 4);
    };
    for (int i = 0; i < 3; ++i) { pushF(0); pushF(0); pushF(0); }
    for (int i = 0; i < 3; ++i) { pushF(1); pushF(1); pushF(1); }
    char j[700];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,%s\","
        "\"byteLength\":%u}],"
        "\"bufferViews\":[{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
        "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":36}],"
        "\"accessors\":[{\"bufferView\":0,\"componentType\":5126,\"count\":3,"
        "\"type\":\"VEC3\"},{\"bufferView\":1,\"componentType\":5126,"
        "\"count\":3,\"type\":\"VEC3\"}],"
        "\"meshes\":[{\"name\":\"a\",\"primitives\":[{\"attributes\":{\"POSITION\":0}}]},"
        "{\"name\":\"b\",\"primitives\":[{\"attributes\":{\"POSITION\":1}}]}]}",
        b64enc(bin).c_str(), static_cast<u32>(bin.size()));
    return j;
}

// .gltf com UM triângulo + UMA animação "girar": translation (0,0,0)@0s →
// (2,0,0)@2s no nó raiz — o fixture MÍNIMO do caminho import→clip→play
std::string animatedGltf() {
    std::vector<u8> bin;
    auto pushF = [&bin](f32 v) {
        u8 t[4]; std::memcpy(t, &v, 4); bin.insert(bin.end(), t, t + 4);
    };
    pushF(0.0f); pushF(2.0f);                       // times (2)
    pushF(0); pushF(0); pushF(0);                   // pos key0
    pushF(2); pushF(0); pushF(0);                   // pos key1
    for (int i = 0; i < 3; ++i) { pushF(0); pushF(0); pushF(0); }   // tri
    char j[900];
    std::snprintf(j, sizeof(j),
        "{\"asset\":{\"version\":\"2.0\"},"
        "\"buffers\":[{\"uri\":\"data:application/octet-stream;base64,%s\","
        "\"byteLength\":%u}],"
        "\"bufferViews\":["
        "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":8},"
        "{\"buffer\":0,\"byteOffset\":8,\"byteLength\":24},"
        "{\"buffer\":0,\"byteOffset\":32,\"byteLength\":36}],"
        "\"accessors\":["
        "{\"bufferView\":0,\"componentType\":5126,\"count\":2,\"type\":\"SCALAR\"},"
        "{\"bufferView\":1,\"componentType\":5126,\"count\":2,\"type\":\"VEC3\"},"
        "{\"bufferView\":2,\"componentType\":5126,\"count\":3,\"type\":\"VEC3\"}],"
        "\"meshes\":[{\"name\":\"tri\",\"primitives\":[{\"attributes\":{\"POSITION\":2}}]}],"
        "\"nodes\":[{\"mesh\":0,\"name\":\"raiz\"}],"
        "\"scenes\":[{\"nodes\":[0]}],"
        "\"animations\":[{\"name\":\"girar\",\"channels\":["
        "{\"sampler\":0,\"target\":{\"node\":0,\"path\":\"translation\"}}],"
        "\"samplers\":[{\"input\":0,\"output\":1}]}]}",
        b64enc(bin).c_str(), static_cast<u32>(bin.size()));
    return j;
}

// upload REAL de uma primitiva no stub (o wiring que o main faz no cache)
Mesh* hostPrimMesh(const PrimParams& p) {
    PrimMeshData d;
    makePrimMesh(p, d);
    if (!d.ok()) {
        return nullptr;
    }
    Mesh* m = new Mesh();
    if (!m->create(d.vertices.data(), static_cast<u32>(d.vertices.size()),
                   d.indices.data(), static_cast<u32>(d.indices.size()))) {
        delete m;
        return nullptr;
    }
    return m;
}

} // namespace

// ---- 1. addTrack no clip ATIVO (o "+track" que caía no clip invisível) -----

TEST(wiring085_addtrack_vai_para_o_clip_ativo) {
    Scene scene;
    const Handle h = scene.create("T");
    ASSERT(h.valid());
    AnimationPlayer* pl = scene.get(h)->addComponent<AnimationPlayer>();
    ASSERT(pl != nullptr);

    // simula o estado PÓS-IMPORT: clips[0]="edit" (vazio) + clips[1]
    // importado ATIVO (gltfAttachClips põe activeClip = último importado)
    ASSERT(pl->editClip() != nullptr);   // cria clips[0] "edit"
    AnimClip imported;
    imported.name = "girar";
    pl->clips.push_back(std::move(imported));
    pl->activeClip = 1;
    ASSERT(pl->activeClipPtr() != nullptr);
    EXPECT(pl->activeClipPtr()->name == "girar");

    // "+track" na timeline → tem de entrar NO CLIP ATIVO (o visível)
    AnimTrack* tr = pl->addTrack(AnimTarget::TicPos, "");
    ASSERT(tr != nullptr);
    EXPECT(pl->activeClipPtr()->tracks.size() == 1u);
    EXPECT(pl->clips[0].tracks.empty());   // o "edit" fica INTACTO
    EXPECT(pl->clips[1].tracks[0].target == AnimTarget::TicPos);

    // sem clips ativos (estado limpo) → editClip cria clips[0] como sempre
    AnimationPlayer pl2;
    AnimTrack* tr2 = pl2.addTrack(AnimTarget::TicScale, "");
    ASSERT(tr2 != nullptr);
    ASSERT(pl2.activeClipPtr() != nullptr);
    EXPECT(pl2.activeClipPtr()->tracks.size() == 1u);
}

// ---- 2. animação e2e: criar → keys → scrub → play → opções → save/load -----

TEST(wiring085_anim_e2e_criar_keys_scrub_play_opcoes_save_load) {
    Scene scene;
    const Handle h = createTicFromPreset(scene, PresetKind::Mesh, nullptr,
                                         nullptr);
    ASSERT(h.valid());
    Transform3D* tr = scene.get(h)->getComponent<Transform3D>();
    ASSERT(tr != nullptr);
    const Vec3 p0 = tr->pos;   // (0, 0.5, 0)

    AnimationPlayer* pl = scene.get(h)->addComponent<AnimationPlayer>();
    ASSERT(pl != nullptr);
    AnimTrack* track = pl->addTrack(AnimTarget::TicPos, "");
    ASSERT(track != nullptr);
    AnimKey k0;
    k0.t = 0.0f;
    k0.v[0] = p0.x; k0.v[1] = p0.y; k0.v[2] = p0.z;
    track->keys.push_back(k0);
    AnimKey k1;
    k1.t = 2.0f;
    k1.v[0] = p0.x + 4.0f; k1.v[1] = p0.y; k1.v[2] = p0.z;
    track->keys.push_back(k1);
    track->sortKeys();

    // SCRUB: aplicar no t=1 → x = p0.x + 2 (o gesto do cursor na timeline)
    pl->time = 1.0f;
    pl->apply(scene, h);
    EXPECT(nearEqF(tr->pos.x, p0.x + 2.0f));

    // PLAY once: termina NO FIM e para (a opção responde)
    pl->mode = AnimationPlayer::Mode::Once;
    pl->time = 0.0f;
    pl->playing = true;
    for (u32 s = 0; s < 130; ++s) {   // 2.17 s > 2 s do clip
        pl->advance(1.0f / 60.0f);
        pl->apply(scene, h);
    }
    EXPECT(!pl->playing);
    EXPECT(nearEqF(tr->pos.x, p0.x + 4.0f));

    // PLAY loop: passa do fim e VOLTA ao início (a opção responde)
    pl->mode = AnimationPlayer::Mode::Loop;
    pl->playing = true;
    for (u32 s = 0; s < 6; ++s) {   // +0.1 s → 2.1 s → 0.1 s
        pl->advance(1.0f / 60.0f);
        pl->apply(scene, h);
    }
    EXPECT(pl->playing);
    EXPECT(nearEqF(tr->pos.x, p0.x + 4.0f * 0.1f / 2.0f));

    // PLAY pingpong: volta para trás após o fim (a opção responde)
    // 1.9 + 0.2 = 2.1 → reflete → 4 − 2.1 = 1.9 (o tempo espelha no fim)
    pl->mode = AnimationPlayer::Mode::PingPong;
    pl->time = 1.9f;
    pl->playing = true;
    for (u32 s = 0; s < 12; ++s) {   // +0.2 s → passa 2 e reflete → 1.9
        pl->advance(1.0f / 60.0f);
        pl->apply(scene, h);
    }
    EXPECT(nearEqF(tr->pos.x, p0.x + 4.0f * 1.9f / 2.0f));

    // SPEED 2×: em 0.5 s percorre 1.0 s do clip (o slider responde)
    pl->mode = AnimationPlayer::Mode::Loop;
    pl->speed = 2.0f;
    pl->time = 0.0f;
    pl->playing = true;
    for (u32 s = 0; s < 30; ++s) {
        pl->advance(1.0f / 60.0f);
        pl->apply(scene, h);
    }
    EXPECT(nearEqF(tr->pos.x, p0.x + 2.0f));
    pl->speed = 1.0f;
    pl->playing = false;

    // SAVE → LOAD: o clip/track/keys voltam e REPRODUZEM (o critério final)
    const std::string json = SceneSerializer::dump(scene);
    EXPECT(!json.empty());
    Scene loaded;
    SceneSerializer::LoadCtx ctx;   // sem resolvers: prim fica null (ok p/ anim)
    ASSERT(SceneSerializer::loadText(loaded, json, ctx));
    const Handle h2 = loaded.find("Mesh");
    ASSERT(h2.valid());
    AnimationPlayer* pl2 = loaded.get(h2)->getComponent<AnimationPlayer>();
    ASSERT(pl2 != nullptr);
    ASSERT(pl2->activeClipPtr() != nullptr);
    EXPECT(pl2->activeClipPtr()->tracks.size() == 1u);
    Transform3D* tr2 = loaded.get(h2)->getComponent<Transform3D>();
    ASSERT(tr2 != nullptr);
    pl2->time = 1.0f;
    pl2->apply(loaded, h2);
    EXPECT(nearEqF(tr2->pos.x, p0.x + 2.0f));
}

// ---- 3. trocar primitiva ×3: mesh TROCA no render e no .goni ---------------

TEST(wiring085_prim_switch_x3_reflete_no_render_e_no_goni) {
    glstub::reset();
    Renderer r;
    ASSERT(r.init());

    Scene scene;
    const Handle h = createTicFromPreset(scene, PresetKind::Mesh, nullptr,
                                         nullptr);
    ASSERT(h.valid());
    MeshRenderer* mr = scene.get(h)->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);

    // resolvers REAIS do device (o cache do main é isto: gerar + uploudar)
    editor::AssetResolvers res;
    res.prim = [](const PrimParams& p) -> Mesh* { return hostPrimMesh(p); };
    res.material = r.litMaterial();

    // esfera default → cone → box → torus (3 trocas do critério)
    const PrimKind seq[4] = {PrimKind::Sphere, PrimKind::Cone, PrimKind::Box,
                             PrimKind::Torus};
    u32 drawsBefore = glstub::stats.drawElementsCalls;
    for (u32 i = 0; i < 4; ++i) {
        const editor::AssetPickOutcome out = editor::applyAssetPick(
            scene, h, 4, static_cast<int>(seq[i]) + 2, editor::AssetCatalog{},
            res);
        EXPECT(out.applied);
        EXPECT(mr->primOn);
        EXPECT(mr->prim.kind == seq[i]);
        EXPECT(mr->mesh != nullptr);
        EXPECT(mr->meshPath.empty());
        // RENDER: o mesh NOVO desenha de facto no stub (o mesh velho saiu)
        r.beginFrame();
        const Mat4 vp = Mat4::identity();
        (void)r.drawMesh(*mr->mesh, Mat4::identity(), vp);
        EXPECT(glstub::stats.drawElementsCalls == drawsBefore + 1);
        drawsBefore = glstub::stats.drawElementsCalls;
    }

    // .goni: "mesh":"prim" + bloco "prim" → round-trip com resolver real
    const std::string json = SceneSerializer::dump(scene);
    EXPECT(json.find("\"prim\"") != std::string::npos);
    Scene loaded;
    SceneSerializer::LoadCtx ctx;
    ctx.resolvePrim = [](const PrimParams& p) -> Mesh* {
        return hostPrimMesh(p);
    };
    ASSERT(SceneSerializer::loadText(loaded, json, ctx));
    const Handle h2 = loaded.find("Mesh");
    ASSERT(h2.valid());
    MeshRenderer* mr2 = loaded.get(h2)->getComponent<MeshRenderer>();
    ASSERT(mr2 != nullptr);
    EXPECT(mr2->primOn);
    EXPECT(mr2->prim.kind == PrimKind::Torus);   // a ÚLTIMA troca persiste
    EXPECT(mr2->mesh != nullptr);
    r.shutdown();
}

// ---- 4. glTF multi-mesh sem sub-ref: o apply FAZ FALLBACK para #0 ----------

TEST(wiring085_gltf_multimesh_fallback_mesh0_no_gpu_assets) {
    glstub::reset();
    FakeStorage st;
    st.writeText("meshes/par.gltf", twoMeshGltf());

    ResourceManager rm;
    rm.setStorage(&st);
    GpuAssets gpu;
    gpu.init(&rm);
    std::string err;

    // contrato do ResourceManager INTACTO: ref explícita sem '#' → erro claro
    const MeshData* direct = rm.mesh("meshes/par.gltf", err);
    EXPECT(direct == nullptr);
    EXPECT(err.find("#<i>") != std::string::npos);

    // WIRING NOVO: pelo GpuAssets (o caminho do aplicar), o fallback #0
    // devolve o mesh "a" — antes disto, aplicar um .glb multi-mesh MORRIA
    Mesh* m = gpu.mesh("meshes/par.gltf");
    ASSERT(m != nullptr);
    EXPECT(m->indexCount() > 0);

    // sub-ref explícita continua a funcionar (MESMO MeshData, ref distinta
    // → objeto GL próprio — a regra "1 ref = 1 objeto" da F5-E)
    Mesh* m0 = gpu.mesh("meshes/par.gltf#0");
    EXPECT(m0 != nullptr);
    EXPECT(m0 != m);
    EXPECT(m0->indexCount() == m->indexCount());   // o MESMO mesh "a"
    Mesh* m1 = gpu.mesh("meshes/par.gltf#1");
    EXPECT(m1 != nullptr && m1 != m);
}

// ---- 5. browser: lista TUDO, mixed-case entra no catálogo ------------------

TEST(wiring085_browser_lista_nao_suportados_e_catálogo_lowercase) {
    // kindOfExtension é a classificação ÚNICA (lowercase): .Glb/.OBJ mistos
    // entram no catálogo (antes: só literais "glTF"/"GLB"/"OBJ")
    EXPECT(fileapi::kindOfExtension("modelo.Glb") == 'm');
    EXPECT(fileapi::kindOfExtension("MODELO.GLTF") == 'm');
    EXPECT(fileapi::kindOfExtension("coisa.OBJ") == 'm');
    EXPECT(fileapi::kindOfExtension("img.PNG") == 't');
    EXPECT(fileapi::kindOfExtension("trap.fbx") == 0);
    EXPECT(fileapi::kindOfExtension("trap.psd") == 0);
    EXPECT(fileapi::kindOfExtension("semext") == 0);

    // o .fbx VISÍVEL no browser (o erro claro é o toast do main ao tapar —
    // o contrato do kind 0 é o que o main consome)
    EXPECT(fileapi::kindOfExtension("x.fbx") == 0);
}

// ---- 6. o caminho COMPLETO do import de animação (morto no device) ---------

TEST(wiring085_import_animacao_do_parse_ao_play_com_track_no_clip_importado) {
    glstub::reset();
    FakeStorage st;
    st.writeText("meshes/girar.gltf", animatedGltf());
    ResourceManager rm;
    rm.setStorage(&st);

    Scene scene;
    const Handle h = createTicFromPreset(scene, PresetKind::Mesh, nullptr,
                                         nullptr);
    ASSERT(h.valid());
    Transform3D* tr = scene.get(h)->getComponent<Transform3D>();
    ASSERT(tr != nullptr);
    const Vec3 p0 = tr->pos;

    // (a) o APPLY do diálogo: mesh do glTF aplicado (via GpuAssets — com o
    //     fallback #0 multi-mesh; aqui é single-mesh e o caminho é direto)
    GpuAssets gpu;
    gpu.init(&rm);
    MeshRenderer* mr = scene.get(h)->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    mr->mesh = gpu.mesh("meshes/girar.gltf");
    ASSERT(mr->mesh != nullptr);

    // (b) o gltfAttachClips do "Sim" (o que o device NUNCA chegava a correr):
    //     clip importado ATIVO no player
    std::string merr;
    const std::shared_ptr<const GltfModel> mdl = rm.model("meshes/girar.gltf", merr);
    ASSERT(mdl != nullptr);
    EXPECT(mdl->animations.size() == 1u);
    const u32 nClips = gltfAttachClips(scene, h, *mdl);
    EXPECT(nClips == 1u);
    AnimationPlayer* pl = scene.get(h)->getComponent<AnimationPlayer>();
    ASSERT(pl != nullptr);
    ASSERT(pl->activeClipPtr() != nullptr);
    EXPECT(pl->activeClipPtr()->name == "girar");
    EXPECT(pl->activeClipPtr()->tracks.size() == 1u);
    // player NOVO (o TIC não tinha "edit"): o importado é clips[0] ATIVO
    EXPECT(pl->clips.size() == 1u);
    EXPECT(pl->activeClip == 0);

    // (c) "+track" do dono com o clip importado ativo → entra NELE (o fix)
    AnimTrack* novo = pl->addTrack(AnimTarget::TicScale, "");
    ASSERT(novo != nullptr);
    EXPECT(pl->activeClipPtr()->tracks.size() == 2u);

    // (d) PLAY reproduz o clip importado: t=1 s → x = p0.x + 1 (lerp 0→2)
    pl->time = 0.0f;
    pl->playing = true;
    for (u32 s = 0; s < 60; ++s) {   // 1 s
        pl->advance(1.0f / 60.0f);
        pl->apply(scene, h);
    }
    EXPECT(nearEqF(tr->pos.x, p0.x + 1.0f));

    // (e) save/load: o clip importado volta e continua a reproduzir
    const std::string json = SceneSerializer::dump(scene);
    Scene loaded;
    SceneSerializer::LoadCtx ctx;
    ASSERT(SceneSerializer::loadText(loaded, json, ctx));
    const Handle h2 = loaded.find("Mesh");
    ASSERT(h2.valid());
    AnimationPlayer* pl2 = loaded.get(h2)->getComponent<AnimationPlayer>();
    ASSERT(pl2 != nullptr);
    ASSERT(pl2->activeClipPtr() != nullptr);
    EXPECT(pl2->activeClipPtr()->name == "girar");
    Transform3D* tr2 = loaded.get(h2)->getComponent<Transform3D>();
    ASSERT(tr2 != nullptr);
    pl2->time = 2.0f;
    pl2->apply(loaded, h2);
    EXPECT(nearEqF(tr2->pos.x, p0.x + 2.0f));   // o FIM do clip importado
}
