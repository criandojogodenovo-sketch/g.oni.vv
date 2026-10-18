// tests/test_serializer.cpp — F3: round-trip save/load, migração v0→v1,
// política forward-compat (tipo desconhecido ignorado) e Json mínimo.
#include "TestFramework.h"
#include <cstdio>
#include <cstring>
#include "components/InputMap.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"

using namespace vv;
using ::test::nearEqF;

namespace {
const char* kTmpPath = "/tmp/goni_test_scene.goni";
} // namespace

TEST(json_parse_dump_sanity) {
    const char* src = R"({"a":1.5,"b":[1,2,3],"c":{"d":"x","e":true},"f":null})";
    Json j;
    EXPECT(Json::parse(src, std::strlen(src), j));
    EXPECT(j.type == Json::Type::Object);
    const Json* a = j.find("a");
    EXPECT(a && a->type == Json::Type::Number && nearEqF(static_cast<f32>(a->number), 1.5f));
    const Json* b = j.find("b");
    EXPECT(b && b->type == Json::Type::Array && b->items.size() == 3u);
    const Json* e = j.find("c")->find("e");
    EXPECT(e && e->type == Json::Type::Bool && e->boolean);

    // dump → reparse é identidade para este documento
    const std::string text = j.dump();
    Json j2;
    EXPECT(Json::parse(text.data(), text.size(), j2));
    EXPECT(j2.find("c")->find("d")->string == "x");

    // inválidos são recusados (truncado, vírgula solta, profundidade)
    Json bad;
    EXPECT(!Json::parse("{\"a\":", 6, bad));
    EXPECT(!Json::parse("[1,,2]", 7, bad));
    EXPECT(!Json::parse("", 0, bad));
}

TEST(json_escapes_e_utf8_curto) {
    const char* src = R"({"s":"a\nb\"c\\d\u0041"})";
    Json j;
    EXPECT(Json::parse(src, std::strlen(src), j));
    EXPECT(j.find("s")->string == std::string("a\nb\"c\\dA"));
}

TEST(serializer_roundtrip_save_load_igual) {
    Scene s;
    const Handle p = createTicFromPreset(s, PresetKind::PlayerBody3D, nullptr, nullptr);
    const Handle st = createTicFromPreset(s, PresetKind::StaticBody3D, nullptr, nullptr);

    // edita o transform do Player (simula o Inspector)
    Transform3D* tr = s.get(p)->getComponent<Transform3D>();
    tr->pos = Vec3{1.25f, -2.5f, 3.0f};
    tr->rot = Quat::fromEuler(0.25f, -1.0f, 0.5f);
    tr->scale = Vec3{1.5f, 2.0f, 0.75f};
    tr->updateWorld();

    EXPECT(SceneSerializer::save(s, kTmpPath));

    // cena destruída e refeita a partir do ficheiro
    Scene s2;
    s2.create("lixo");   // load tem de limpar o que existia
    SceneSerializer::LoadCtx ctx{};   // mesh/material nulos: ponteiros → null
    EXPECT(SceneSerializer::load(s2, kTmpPath, ctx));

    EXPECT(s2.count() == 2u);
    const Handle p2 = s2.find("PlayerBody3D");
    const Handle st2 = s2.find("StaticBody3D");
    EXPECT(p2.valid() && st2.valid());

    Tic* tp = s2.get(p2);
    Transform3D* tr2 = tp->getComponent<Transform3D>();
    EXPECT(tr2 != nullptr);
    EXPECT(nearEqF(tr2->pos.x, 1.25f) && nearEqF(tr2->pos.y, -2.5f) && nearEqF(tr2->pos.z, 3.0f));
    EXPECT(nearEqF(tr2->rot.x, tr->rot.x, 1e-5f) && nearEqF(tr2->rot.y, tr->rot.y, 1e-5f) &&
           nearEqF(tr2->rot.z, tr->rot.z, 1e-5f) && nearEqF(tr2->rot.w, tr->rot.w, 1e-5f));
    EXPECT(nearEqF(tr2->scale.x, 1.5f) && nearEqF(tr2->scale.y, 2.0f) && nearEqF(tr2->scale.z, 0.75f));

    // world recompute no load: matriz coerente com o pos carregado
    f32 out[4];
    Mat4::transformPoint4(tr2->world, Vec3{0.0f, 0.0f, 0.0f}, out);
    EXPECT(nearEqF(out[0], 1.25f) && nearEqF(out[1], -2.5f) && nearEqF(out[2], 3.0f));

    EXPECT(tp->getComponent<InputMap>() != nullptr);       // Player mantém o InputMap
    EXPECT(s2.get(st2)->getComponent<InputMap>() == nullptr);
    EXPECT(s2.get(st2)->getComponent<MeshRenderer>() != nullptr);

    // MeshRenderer com ctx nulo: presença mantida, ponteiros runtime ficam null
    MeshRenderer* mr2 = s2.get(st2)->getComponent<MeshRenderer>();
    EXPECT(mr2->mesh == nullptr && mr2->material == nullptr);

    std::remove(kTmpPath);
}

TEST(serializer_rebinda_mesh_e_material) {
    Scene s;
    // cria com ponteiros "antigos" → tag "cube" no JSON
    createTicFromPreset(s, PresetKind::StaticBody3D,
                        reinterpret_cast<Mesh*>(0x1), reinterpret_cast<Material*>(0x2));
    EXPECT(SceneSerializer::save(s, kTmpPath));

    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ctx.cubeMesh = reinterpret_cast<Mesh*>(0x10);      // sentinela (não desreferenciado)
    ctx.material = reinterpret_cast<Material*>(0x20);
    EXPECT(SceneSerializer::load(s2, kTmpPath, ctx));

    EXPECT(s2.components().meshRenderers().size() == 1u);
    const Handle owner = s2.components().meshRenderers().owner(0);
    MeshRenderer* mr = s2.get(owner) ? s2.get(owner)->getComponent<MeshRenderer>() : nullptr;
    EXPECT(mr != nullptr);
    EXPECT(mr->mesh == ctx.cubeMesh);       // tag "cube" rebinda para o mesh atual
    EXPECT(mr->material == ctx.material);
    std::remove(kTmpPath);
}

TEST(serializer_migracao_v0_abre_na_v1) {
    // v0: sem "version" no root e tics sem "active" (rascunho pré-F3-final)
    const char* v0 =
        "{\"tics\":["
        "  {\"id\":0,\"name\":\"PlayerBody3D\",\"parent\":-1,\"components\":["
        "     {\"type\":\"Transform3D\",\"pos\":[0,0.5,0],\"rot\":[0,0,0,1],\"scale\":[1,1,1]},"
        "     {\"type\":\"MeshRenderer\",\"mesh\":\"cube\"},"
        "     {\"type\":\"InputMap\"}]},"
        "  {\"id\":1,\"name\":\"StaticBody3D\",\"parent\":-1,\"components\":["
        "     {\"type\":\"Transform3D\",\"pos\":[2,0.5,0],\"rot\":[0,0,0,1],\"scale\":[1,1,1]}]}"
        "]}";

    Scene s;
    SceneSerializer::LoadCtx ctx{};
    EXPECT(SceneSerializer::load(s, kTmpPath, ctx) == false);   // ficheiro ainda não existe

    FILE* f = std::fopen(kTmpPath, "wb");
    EXPECT(f != nullptr);
    std::fwrite(v0, 1, std::strlen(v0), f);
    std::fclose(f);

    EXPECT(SceneSerializer::load(s, kTmpPath, ctx));
    EXPECT(s.count() == 2u);
    const Handle h = s.find("PlayerBody3D");
    EXPECT(h.valid());
    EXPECT(s.get(h)->active == true);                          // v0 → default true
    EXPECT(s.get(h)->getComponent<InputMap>() != nullptr);

    // migração adiciona version=1 ao documento
    Json doc;
    f = std::fopen(kTmpPath, "rb");
    char buf[2048];
    const size_t n = std::fread(buf, 1, sizeof(buf), f);
    std::fclose(f);
    EXPECT(Json::parse(buf, n, doc));
    Json migrated = SceneSerializer::migrate(std::move(doc));
    const Json* v = migrated.find("version");
    EXPECT(v && v->type == Json::Type::Number && static_cast<u32>(v->number) == SceneSerializer::kVersion);
    std::remove(kTmpPath);
}

TEST(serializer_tipo_desconhecido_ignorado_forward_compat) {
    // documento "v2 simulado": componente que esta build não conhece
    const char* futurista =
        "{\"version\":1,\"tics\":["
        "  {\"id\":0,\"name\":\"StaticBody3D\",\"active\":true,\"parent\":-1,\"components\":["
        "     {\"type\":\"BodyComp\",\"massa\":10.0},"
        "     {\"type\":\"Transform3D\",\"pos\":[1,0.5,0],\"rot\":[0,0,0,1],\"scale\":[1,1,1]}]}"
        "]}";
    FILE* f = std::fopen(kTmpPath, "wb");
    EXPECT(f != nullptr);
    std::fwrite(futurista, 1, std::strlen(futurista), f);
    std::fclose(f);

    Scene s;
    SceneSerializer::LoadCtx ctx{};
    EXPECT(SceneSerializer::load(s, kTmpPath, ctx));
    EXPECT(s.count() == 1u);
    const Handle h = s.find("StaticBody3D");
    EXPECT(h.valid());
    EXPECT(s.get(h)->getComponent<Transform3D>() != nullptr);   // conhecido: carregado
    Transform3D* tr = s.get(h)->getComponent<Transform3D>();
    EXPECT(nearEqF(tr->pos.x, 1.0f));
    EXPECT(s.get(h)->getComponent<InputMap>() == nullptr);      // desconhecido: ignorado
    std::remove(kTmpPath);
}

TEST(serializer_ficheiro_ausente_ou_corrompido_falha_sem_destruir) {
    Scene s;
    SceneSerializer::LoadCtx ctx{};
    EXPECT(!SceneSerializer::load(s, "/tmp/goni_nao_existe.goni", ctx));
    EXPECT(s.count() == 0u);   // load falhou ANTES do clear — cena intacta

    createTicFromPreset(s, PresetKind::StaticBody3D, nullptr, nullptr);
    EXPECT(s.count() == 1u);

    FILE* f = std::fopen(kTmpPath, "wb");
    std::fwrite("{\"tics\":[{\"id\":", 1, 15, f);   // truncado
    std::fclose(f);
    EXPECT(!SceneSerializer::load(s, kTmpPath, ctx));
    EXPECT(s.count() == 1u);   // cena NÃO foi limpa por um ficheiro corrompido
    std::remove(kTmpPath);
}

// ---- F5-E: refs relativas + resolvers no LoadCtx -----------------------------

TEST(serializer_refs_relativas_roundtrip_com_resolvers) {
    Scene s;
    const Handle h = createTicFromPreset(s, PresetKind::StaticBody3D,
                                         reinterpret_cast<Mesh*>(0x1),
                                         reinterpret_cast<Material*>(0x2));
    MeshRenderer* mr = s.get(h)->getComponent<MeshRenderer>();
    mr->mesh = reinterpret_cast<Mesh*>(0x1);
    mr->meshPath = "meshes/sphere.obj#0";
    mr->texPath = "textures/wood.png";

    EXPECT(SceneSerializer::save(s, kTmpPath));

    // no JSON: tag "file" + meshPath + texPath
    Json doc;
    {
        FILE* f = std::fopen(kTmpPath, "rb");
        char buf[4096];
        const size_t n = std::fread(buf, 1, sizeof(buf), f);
        std::fclose(f);
        EXPECT(Json::parse(buf, n, doc));
    }
    const Json* jt = doc.find("tics")->items[0].find("components")->items[1].find("meshPath");
    EXPECT(jt && jt->string == "meshes/sphere.obj#0");
    const Json* tp = doc.find("tics")->items[0].find("components")->items[1].find("texPath");
    EXPECT(tp && tp->string == "textures/wood.png");

    // load com resolvers sentinelas — refs rebindam para os objetos atuais
    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ctx.material = reinterpret_cast<Material*>(0x20);
    ctx.resolveMesh = [](const std::string& ref) -> Mesh* {
        return ref == "meshes/sphere.obj#0" ? reinterpret_cast<Mesh*>(0x10) : nullptr;
    };
    ctx.resolveTex = [](const std::string& ref) -> const Texture* {
        return ref == "textures/wood.png" ? reinterpret_cast<const Texture*>(0x30) : nullptr;
    };
    EXPECT(SceneSerializer::load(s2, kTmpPath, ctx));

    const Handle h2 = s2.find("StaticBody3D");
    EXPECT(h2.valid());
    MeshRenderer* mr2 = s2.get(h2)->getComponent<MeshRenderer>();
    EXPECT(mr2 != nullptr);
    EXPECT(mr2->meshPath == "meshes/sphere.obj#0");
    EXPECT(mr2->mesh == reinterpret_cast<Mesh*>(0x10));        // resolver ligou
    EXPECT(mr2->material == ctx.material);                     // material volta
    EXPECT(mr2->texPath == "textures/wood.png");
    EXPECT(mr2->texture == reinterpret_cast<const Texture*>(0x30));
    std::remove(kTmpPath);
}

TEST(serializer_ref_sem_resolver_fica_null_mas_mantem_ref) {
    Scene s;
    const Handle h = createTicFromPreset(s, PresetKind::StaticBody3D, nullptr, nullptr);
    MeshRenderer* mr = s.get(h)->getComponent<MeshRenderer>();
    mr->mesh = reinterpret_cast<Mesh*>(0x1);
    mr->meshPath = "meshes/faltando.obj";
    EXPECT(SceneSerializer::save(s, kTmpPath));

    Scene s2;
    SceneSerializer::LoadCtx ctx{};   // SEM resolvers (ex.: asset removido)
    EXPECT(SceneSerializer::load(s2, kTmpPath, ctx));
    MeshRenderer* mr2 = s2.get(s2.find("StaticBody3D"))->getComponent<MeshRenderer>();
    EXPECT(mr2->mesh == nullptr && mr2->material == nullptr);
    EXPECT(mr2->meshPath == "meshes/faltando.obj");   // ref sobrevive p/ rebind
    std::remove(kTmpPath);
}

TEST(serializer_resolver_que_falha_nao_crasha) {
    Scene s;
    const Handle h = createTicFromPreset(s, PresetKind::StaticBody3D, nullptr, nullptr);
    s.get(h)->getComponent<MeshRenderer>()->meshPath = "meshes/x.gltf#1";
    s.get(h)->getComponent<MeshRenderer>()->texPath = "textures/nao_existe.png";
    EXPECT(SceneSerializer::save(s, kTmpPath));

    Scene s2;
    SceneSerializer::LoadCtx ctx;
    ctx.resolveMesh = [](const std::string&) -> Mesh* { return nullptr; };
    ctx.resolveTex = [](const std::string&) -> const Texture* { return nullptr; };
    EXPECT(SceneSerializer::load(s2, kTmpPath, ctx));
    MeshRenderer* mr2 = s2.get(s2.find("StaticBody3D"))->getComponent<MeshRenderer>();
    EXPECT(mr2->mesh == nullptr && mr2->texture == nullptr);
    EXPECT(mr2->material == nullptr);   // sem mesh → sem material
    EXPECT(mr2->meshPath == "meshes/x.gltf#1" && mr2->texPath == "textures/nao_existe.png");
    std::remove(kTmpPath);
}
