// tests/test_project.cpp — F5-A: formato do projeto .goni (manifesto +
// estrutura scenes/meshes/textures), refs relativos, persistência do projeto
// aberto e guarda de caminhos. Storage injetado: FakeStorage em memória
// (a implementação POSIX real é testada em test_storage.cpp).
#include "TestFramework.h"
#include <cstdio>
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/Presets.h"
#include "core/Project.h"
#include "core/Scene.h"
#include "FakeStorage.h"

using namespace vv;
using ::test::nearEqF;

namespace {

} // namespace

TEST(valid_relpath_guarda) {
    EXPECT(validRelPath("scenes/main.goni"));
    EXPECT(validRelPath("a.png"));
    EXPECT(validRelPath("meshes/sphere.obj"));
    EXPECT(!validRelPath(""));                    // vazio
    EXPECT(!validRelPath("/absoluto.png"));       // absoluto
    EXPECT(!validRelPath("../fuga.goni"));        // traversal
    EXPECT(!validRelPath("scenes/../../fuga"));   // traversal escondido
    EXPECT(!validRelPath("scenes//main.goni"));   // segmento vazio
    EXPECT(!validRelPath("scenes/"));             // termina em '/'
    EXPECT(!validRelPath("a\\b.png"));            // separador errado
    // joinRelPath recusa o que validRelPath recusa
    EXPECT(joinRelPath("/root", "../x") == "");
    EXPECT(joinRelPath("/root", "scenes/m.goni") == "/root/scenes/m.goni");
}

TEST(projeto_create_new_estrutura_completa) {
    FakeStorage st;
    Project p;
    EXPECT(Project::createNew(st, "meu jogo", p));
    EXPECT(p.name == "meu jogo");
    EXPECT(p.scenes.size() == 1u);
    EXPECT(p.scenes[0] == "scenes/main.goni");
    EXPECT(p.activeScene == 0u);
    // estrutura no storage
    EXPECT(st.exists("project.goni"));
    EXPECT(st.exists("scenes/main.goni"));   // cena inicial já escrita
    EXPECT(st.dirs.count("scenes") == 1u);
    EXPECT(st.dirs.count("meshes") == 1u);
    EXPECT(st.dirs.count("textures") == 1u);
    // manifesto parseável com os campos do formato
    Json doc;
    const std::string& m = st.files["project.goni"];
    EXPECT(Json::parse(m.data(), m.size(), doc));
    EXPECT(doc.find("version") && static_cast<u32>(doc.find("version")->number) == Project::kVersion);
    EXPECT(doc.find("name") && doc.find("name")->string == "meu jogo");
    EXPECT(doc.find("scenes") && doc.find("scenes")->items.size() == 1u);
    EXPECT(doc.find("settings") && doc.find("settings")->type == Json::Type::Object);
}

TEST(projeto_create_new_recusa_sobrescrever) {
    FakeStorage st;
    Project p;
    EXPECT(Project::createNew(st, "a", p));
    const u32 filesBefore = static_cast<u32>(st.files.size());
    EXPECT(!Project::createNew(st, "b", p));   // já existe manifesto
    EXPECT(static_cast<u32>(st.files.size()) == filesBefore);   // nada tocado
    Project again;
    EXPECT(Project::open(st, again));
    EXPECT(again.name == "a");                 // projeto original intacto
}

TEST(projeto_manifesto_roundtrip_e_settings_verbatim) {
    FakeStorage st;
    Project p;
    EXPECT(Project::createNew(st, "rt", p));
    p.name = "renomeado";
    p.activeScene = 0;
    // settings aberto — o engine não interpreta, só transporta verbatim
    Json set = Json::makeObject();
    set.addMember("chuva", Json::makeBool(true));
    set.addMember("zoom", Json::makeNumber(2.5));
    Json sub = Json::makeObject();
    sub.addMember("tag", Json::makeString("fase5"));
    set.addMember("extra", std::move(sub));
    p.settings = set;
    EXPECT(p.saveManifest(st));

    Project q;
    EXPECT(Project::open(st, q));
    EXPECT(q.name == "renomeado");
    EXPECT(q.scenes == p.scenes);
    EXPECT(q.activeScene == p.activeScene);
    const Json* chuva = q.settings.find("chuva");
    EXPECT(chuva && chuva->type == Json::Type::Bool && chuva->boolean);
    const Json* zoom = q.settings.find("zoom");
    EXPECT(zoom && nearEqF(static_cast<f32>(zoom->number), 2.5f));
    const Json* tag = q.settings.find("extra");
    EXPECT(tag && tag->find("tag") && tag->find("tag")->string == "fase5");
}

TEST(projeto_open_ausente_ou_corrompido_falha) {
    FakeStorage st;
    Project p;
    EXPECT(!Project::open(st, p));                    // sem manifesto
    st.files["project.goni"] = "{isto nao e json";
    EXPECT(!Project::open(st, p));                    // corrompido
    st.files["project.goni"] = "{\"scenes\":[\"../fuga.goni\"]}";
    EXPECT(!Project::open(st, p));                    // ref inválido
    st.files["project.goni"] = "{\"version\":99,\"scenes\":[]}";
    EXPECT(!Project::open(st, p));                    // versão futura
}

TEST(projeto_migracao_manifesto_sem_version) {
    FakeStorage st;
    st.files["project.goni"] =
        "{\"name\":\"antigo\",\"scenes\":[\"scenes/main.goni\"],\"activeScene\":0}";
    Project p;
    EXPECT(Project::open(st, p));                     // rascunho → v1
    EXPECT(p.version == Project::kVersion);
    EXPECT(p.name == "antigo");
    EXPECT(p.settings.type == Json::Type::Object);    // default ao reabrir
    // re-salva já com version explícito
    EXPECT(p.saveManifest(st));
    Json doc;
    const std::string& m = st.files["project.goni"];
    EXPECT(Json::parse(m.data(), m.size(), doc));
    EXPECT(static_cast<u32>(doc.find("version")->number) == Project::kVersion);
}

TEST(projeto_addScene_ativa_e_sem_duplicar) {
    FakeStorage st;
    Project p;
    EXPECT(Project::createNew(st, "x", p));
    EXPECT(p.addScene("nivel2"));
    EXPECT(p.scenes.size() == 2u);
    EXPECT(p.scenes[1] == "scenes/nivel2.goni");
    EXPECT(p.activeScene == 1u);
    EXPECT(p.addScene("nivel2"));                     // já existe → só ativa
    EXPECT(p.scenes.size() == 2u);
    EXPECT(p.activeScene == 1u);
    EXPECT(!p.addScene("../injecao"));                // nome tem de ser stem
    EXPECT(!p.addScene(""));
    EXPECT(p.activeScenePath() && *p.activeScenePath() == "scenes/nivel2.goni");
}

TEST(projeto_reabrir_carrega_cena_e_refs_relativos) {
    FakeStorage st;
    Project p;
    EXPECT(Project::createNew(st, "refs", p));

    // cena com conteúdo (preset) → saveActiveScene
    Scene s;
    const Handle h = createTicFromPreset(s, PresetKind::PlayerBody3D, nullptr, nullptr);
    Transform3D* tr = s.get(h)->getComponent<Transform3D>();
    tr->pos = Vec3{3.0f, 1.25f, -0.5f};
    tr->updateWorld();
    EXPECT(p.saveActiveScene(st, s));

    // reabrir do ZERO (novo objeto Project + nova Scene — simula reboot)
    Project q;
    EXPECT(Project::open(st, q));
    const std::string rel = *q.activeScenePath();
    EXPECT(rel == "scenes/main.goni");                // ref relativo intacto

    Scene s2;
    s2.create("lixo");
    SceneSerializer::LoadCtx ctx{};
    EXPECT(q.loadActiveScene(st, s2, ctx));
    EXPECT(s2.count() == 1u);
    const Handle h2 = s2.find("PlayerBody3D");
    EXPECT(h2.valid());
    const Transform3D* tr2 = s2.get(h2)->getComponent<Transform3D>();
    EXPECT(tr2 && nearEqF(tr2->pos.x, 3.0f) && nearEqF(tr2->pos.y, 1.25f) &&
           nearEqF(tr2->pos.z, -0.5f));

    // activeScene persistida no manifesto — trocar para a 2ª cena e salvar
    EXPECT(q.addScene("caverna"));
    EXPECT(q.saveManifest(st));
    Scene vazio;
    EXPECT(q.saveActiveScene(st, vazio));
    Project r;
    EXPECT(Project::open(st, r));
    EXPECT(r.activeScene == 1u);
    EXPECT(*r.activeScenePath() == "scenes/caverna.goni");
    Scene s3;
    EXPECT(r.loadActiveScene(st, s3, ctx));
    EXPECT(s3.count() == 0u);                         // caverna está vazia
}

TEST(projeto_sem_cena_ativa_falha_sem_crash) {
    FakeStorage st;
    Project p;
    EXPECT(Project::createNew(st, "y", p));
    p.scenes.clear();
    p.activeScene = 0;
    EXPECT(p.activeScenePath() == nullptr);
    Scene s;
    EXPECT(!p.saveActiveScene(st, s));
    EXPECT(!p.loadActiveScene(st, s, SceneSerializer::LoadCtx{}));
}

TEST(projeto_openOrCreate_abre_ou_cria_no_boot) {
    FakeStorage st;
    Project p;
    // 1º boot: não há manifesto → cria com o nome dado
    EXPECT(Project::openOrCreate(st, "boot1", p));
    EXPECT(p.name == "boot1");
    EXPECT(st.exists("scenes/main.goni"));
    // 2º boot: abre o MESMO projeto (não cria, não renomeia)
    Project q;
    EXPECT(Project::openOrCreate(st, "outro_nome", q));
    EXPECT(q.name == "boot1");
    EXPECT(q.scenes == p.scenes);
    // storage quebrado (rel-dir raiz imutável no FakeStorage — simula via
    // guarda: manifesto existe E open falha E createNew falha não é possível
    // no FakeStorage; o contrato aberto/criado já está coberto acima).
}
