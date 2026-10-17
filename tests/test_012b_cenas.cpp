// tests/test_012b_cenas.cpp — 0.12-B (B-4): o seletor de cenas com AÇÕES.
//
// Os 4 pins da sub-fase B-4 (a ordem do prompt):
//   1. cena_rename          — o renomear com o teclado in-app (o caminho)
//   2. cena_duplicate       — a cópia com nome único Godot-style
//   3. cena_delete_confirm  — o apagar NUNCA sai sem o 2.º toque (2-tap)
//   4. cena_serializa       — o manifesto round-trip com as 3 ações
//
// MUTAÇÃO (colada no RELATORIO-0.12-B.md):
//   M-B4a — o apagar sem confirmação → pin 3 vermelho
#include "TestFramework.h"
#include <string>
#include <vector>
#include "core/Project.h"
#include "core/Scene.h"
#include "platform/InputState.h"
#include "ui/EditorUi.h"
#include "ui/UiEditor.h"   // 0.12-B (B-4): openTextInput (o pré-carregado do 8)
#include "ui/FontAtlas.h"
#include "ui/UiContext.h"
#include "ui/Theme.h"
#include "FakeStorage.h"

using namespace vv;
using namespace vv::editor;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;

// o env do menu CENAS (o MESMO padrão do cenas_menu do test_scenes.cpp —
// a densidade 1 do hospedeiro: dp(x) == x; as zonas [152,192)·[192,232)·
// [232,272) são as MESMAS geometrias que o draw decide)
struct CnEnv {
    FontAtlas  font;
    UiContext  ui;
    InputState input;
    EditorState st;
    bool       ok = false;

    CnEnv() {
        const char* fontPath = FONT_FIXTURE;
        ok = font.loadFromPaths(&fontPath, 1, 28.0f);
        if (!ok) {
            return;
        }
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
    }

    int frame(const std::vector<std::string>& scenes, u32 active) {
        ui.beginFrame(nullptr, &input, kSW, kSH);
        const int pick =
            drawScenesMenu(ui, input, kSW, kSH, st, scenes, active);
        ui.endFrame();
        input.clearEdges();
        return pick;
    }
    int tap(const std::vector<std::string>& scenes, u32 active, f32 x,
            f32 y) {
        input.injectDown(0, x, y);
        frame(scenes, active);
        input.injectUp(0);
        return frame(scenes, active);
    }
};

// a geometria do sheet (o MESMO cálculo do test_scenes.cpp — overlayArea
// sem âncora centra na banda do viewport; lista em listTop)
void sheetGeom(f32& x, f32& y, f32& listTop, u32 nScenes) {
    const f32 rowH = 48.0f;
    const f32 h = 48.0f + 56.0f + 8.0f + static_cast<f32>(nScenes) * rowH +
                  8.0f;
    f32 ox, oy, aw, ah;
    overlayArea(kSW, kSH, safe::Insets{}, ox, oy, aw, ah);
    x = ox + (aw - 280.0f) * 0.5f;
    y = oy + (ah - h) * 0.5f;
    listTop = y + 4.0f + 56.0f + 8.0f;
}

} // namespace

// ---- 1. cena_rename ------------------------------------------------------------

TEST(cena_rename) {
    // a FONTE PURA do caminho: o MESMO filtro do addScene
    std::string nv;
    EXPECT(sceneRenamePath("scenes/main.goni", "nivel", nv));
    EXPECT(nv == "scenes/nivel.goni");
    // os recusados: vazio, caminho, ponto, dois-pontos, barra inversa
    EXPECT(!sceneRenamePath("scenes/main.goni", "", nv));
    EXPECT(!sceneRenamePath("scenes/main.goni", "a/b", nv));
    EXPECT(!sceneRenamePath("scenes/main.goni", "a:b", nv));
    EXPECT(!sceneRenamePath("scenes/main.goni", "a.b", nv));
    EXPECT(!sceneRenamePath("scenes/main.goni", "a\\b", nv));
    // o MESMO nome → false (reescrever o ficheiro por nada nunca)
    EXPECT(!sceneRenamePath("scenes/main.goni", "main", nv));

    // o GESTO real: o lápis da linha 1 devolve a ação (o menu fecha) e o
    // main abre o teclado com o propósito 8 e o nome atual PRÉ-CARREGADO
    CnEnv e;
    ASSERT(e.ok);
    const std::vector<std::string> scenes = {"scenes/main.goni",
                                             "scenes/nivel2.goni",
                                             "scenes/boss.goni"};
    f32 x, y, listTop;
    sheetGeom(x, y, listTop, 3u);
    e.st.scenesMenu = true;
    EXPECT(e.tap(scenes, 1u, x + 172.0f, listTop + 1.0f * 48.0f + 24.0f) ==
           sceneActionPick(1, kSceneActRename));
    EXPECT(!e.st.scenesMenu);   // a ação fecha o menu
    openTextInput(e.st, 8, Handle{}, -1, "nivel2");
    EXPECT(e.st.textInput && e.st.textPurpose == 8);
    EXPECT(std::string(e.st.textBuf) == "nivel2");   // o pré-carregado
}

// ---- 2. cena_duplicate ---------------------------------------------------------

TEST(cena_duplicate) {
    // a convenção Godot-style do uniqueTicName (a variante DUPLICAR que
    // SEMPRE sufixa — a cópia nunca herda o nome do original)
    std::vector<std::string> v = {"scenes/main.goni"};
    EXPECT(sceneUniqueName(v, "main") == "main.001");
    v.push_back("scenes/main.001.goni");
    EXPECT(sceneUniqueName(v, "main") == "main.002");
    v.push_back("scenes/main.002.goni");
    EXPECT(sceneUniqueName(v, "main") == "main.003");
    // sem colisão ALGUMA o sufixo entra na mesma (nunca o nome nu)
    std::vector<std::string> b = {"scenes/boss.goni"};
    EXPECT(sceneUniqueName(b, "main") == "main.001");
    // o truncamento a 28 é o contrato do nome
    EXPECT(sceneUniqueName(b, "abcdefghijklmnopqrstuvwxyz0123") ==
           "abcdefghijklmnopqrstuvwxyz01.001");

    // o GESTO real: o duplicar da linha 0 devolve a ação (o menu fecha)
    CnEnv e;
    ASSERT(e.ok);
    f32 x, y, listTop;
    sheetGeom(x, y, listTop, 1u);   // UMA cena — a altura do sheet é dela
    e.st.scenesMenu = true;
    EXPECT(e.tap(b, 0u, x + 212.0f, listTop + 0.0f * 48.0f + 24.0f) ==
           sceneActionPick(0, kSceneActDuplicate));
    EXPECT(!e.st.scenesMenu);
}

// ---- 3. cena_delete_confirm (o pin que a M-B4a mata) ---------------------------

TEST(cena_delete_confirm) {
    // a DECISÃO pura (a fonte do draw e do pin):
    EXPECT(sceneDeleteAction(-1, 0) == 1);   // o 1.º toque SÓ arma
    EXPECT(sceneDeleteAction(0, 0) == 2);    // o 2.º no MESMO ícone confirma
    EXPECT(sceneDeleteAction(0, 1) == 1);    // outra linha RE-ARMA (nunca
                                             // apaga direto)
    EXPECT(sceneDeleteAction(2, 0) == 1);    // o mesmo de outra armação
    EXPECT(sceneDeleteAction(-1, -1) == 1);  // degenerado não espirra

    // o GESTO real no menu: 2-tap de ponta a ponta
    CnEnv e;
    ASSERT(e.ok);
    const std::vector<std::string> scenes = {"scenes/main.goni",
                                             "scenes/nivel2.goni",
                                             "scenes/boss.goni"};
    f32 x, y, listTop;
    sheetGeom(x, y, listTop, 3u);
    const f32 delX = x + 252.0f;   // o centro da zona do apagar [232,272)
    auto delTap = [&](u32 row) {
        return e.tap(scenes, 1u, delX, listTop + static_cast<f32>(row) *
                                                     48.0f + 24.0f);
    };

    // o 1.º toque NUNCA apaga: arma e fica no menu
    e.st.scenesMenu = true;
    EXPECT(delTap(0) == 0);
    EXPECT(e.st.scenesMenu);            // o menu fica aberto
    EXPECT(e.st.sceneDelArm == 0);      // a linha 0 em estado «apagar?»

    // o 2.º toque no MESMO ícone confirma e fecha
    EXPECT(delTap(0) == sceneActionPick(0, kSceneActDelete));
    EXPECT(!e.st.scenesMenu);
    EXPECT(e.st.sceneDelArm == -1);

    // o toque no ícone de OUTRA linha re-arma essa (nunca apaga direto)
    e.st.scenesMenu = true;
    EXPECT(delTap(0) == 0);
    EXPECT(e.st.sceneDelArm == 0);
    EXPECT(delTap(2) == 0);
    EXPECT(e.st.sceneDelArm == 2);      // re-armou a 2, apagou NENHUMA

    // o toque no corpo da linha (trocar) DESARMA
    EXPECT(e.tap(scenes, 1u, x + 140.0f, listTop + 0.0f * 48.0f + 24.0f) ==
           2);
    EXPECT(e.st.sceneDelArm == -1);

    // o toque FORA desarma (o arm morre com o menu)
    e.st.scenesMenu = true;
    EXPECT(delTap(0) == 0);
    EXPECT(e.st.sceneDelArm == 0);
    EXPECT(e.tap(scenes, 1u, 60.0f, 360.0f) == 0);
    EXPECT(e.st.sceneDelArm == -1);
    EXPECT(!e.st.scenesMenu);
}

// ---- 4. cena_serializa ---------------------------------------------------------

TEST(cena_serializa) {
    // o MANIFESTO round-trip com as 3 ações aplicadas pelo MESMO caminho
    // do main (a fonte pura decide os caminhos; o storage é o fake da casa)
    FakeStorage fs;
    Project p;
    ASSERT(Project::createNew(fs, "teste", p));
    Scene vazia;
    EXPECT(p.saveActiveScene(fs, vazia));   // scenes/main.goni (ativa 0)
    EXPECT(p.addScene("nivel"));            // ativa → 1
    EXPECT(p.saveActiveScene(fs, vazia));   // scenes/nivel.goni
    EXPECT(p.addScene("boss"));             // ativa → 2
    EXPECT(p.saveActiveScene(fs, vazia));   // scenes/boss.goni
    // um setting verbatim (o objeto aberto segue o dono no round-trip)
    p.settings = Json::makeObject();
    Json mark = Json::makeObject();
    mark.addMember("marcador", Json::makeString("B4"));
    p.settings.addMember("b4", mark);
    EXPECT(p.saveManifest(fs));

    // ROUND-TRIP 1: o manifesto abre IGUAL (a lista, a ativa, o setting)
    Project q;
    ASSERT(Project::open(fs, q));
    EXPECT(q.scenes.size() == 3u);
    EXPECT(q.scenes[0] == "scenes/main.goni");
    EXPECT(q.scenes[1] == "scenes/nivel.goni");
    EXPECT(q.scenes[2] == "scenes/boss.goni");
    EXPECT(q.activeScene == 2u);
    const Json* b4 = q.settings.find("b4");
    ASSERT(b4 != nullptr);
    const Json* mk = b4->find("marcador");
    ASSERT(mk != nullptr);
    EXPECT(std::string(mk->string) == "B4");

    // AÇÃO 1 (renomear — o MESMO caminho do renameSceneInProject)
    std::string nv;
    ASSERT(sceneRenamePath(q.scenes[0], "mundo", nv));
    EXPECT(nv == "scenes/mundo.goni");
    std::string txt;
    ASSERT(fs.readText("scenes/main.goni", txt));
    EXPECT(fs.writeText(nv, txt));
    EXPECT(fs.remove("scenes/main.goni"));
    q.scenes[0] = nv;
    EXPECT(q.saveManifest(fs));
    Project r;
    ASSERT(Project::open(fs, r));
    EXPECT(r.scenes[0] == "scenes/mundo.goni");
    EXPECT(!fs.exists("scenes/main.goni"));
    EXPECT(fs.exists("scenes/nivel.goni"));   // as irmãs intactas
    EXPECT(r.activeScene == 2u);              // a ativa é ÍNDICE — não mexe

    // AÇÃO 2 (duplicar — a cópia NÃO fica ativa)
    const std::string dup = sceneUniqueName(r.scenes, "mundo");
    EXPECT(dup == "mundo.001");
    const std::string dupRel =
        std::string(Project::kDirScenes) + "/" + dup + ".goni";
    EXPECT(fs.writeText(dupRel, txt));   // o MESMO conteúdo da origem
    r.scenes.push_back(dupRel);
    EXPECT(r.saveManifest(fs));
    Project s;
    ASSERT(Project::open(fs, s));
    EXPECT(s.scenes.size() == 4u);
    EXPECT(s.scenes[3] == "scenes/mundo.001.goni");
    EXPECT(s.activeScene == 2u);   // a cópia não roubou o foco
    std::string dupTxt;
    ASSERT(fs.readText("scenes/mundo.001.goni", dupTxt));
    EXPECT(dupTxt == txt);   // byte a byte o conteúdo da origem

    // AÇÃO 3 (apagar a NÃO-ativa que vinha ANTES da ativa — o índice
    // re-assenta: a ativa 2 («boss») vira 1)
    const u32 alvo = 0;
    EXPECT(alvo != s.activeScene);
    EXPECT(fs.remove(s.scenes[alvo]));
    s.scenes.erase(s.scenes.begin() + static_cast<ptrdiff_t>(alvo));
    if (s.activeScene > alvo) {
        --s.activeScene;
    }
    EXPECT(s.saveManifest(fs));
    Project t;
    ASSERT(Project::open(fs, t));
    EXPECT(t.scenes.size() == 3u);
    EXPECT(t.scenes[0] == "scenes/nivel.goni");
    EXPECT(t.scenes[1] == "scenes/boss.goni");
    EXPECT(t.activeScene == 1u);   // re-assentou (o boss continua ativo)
    EXPECT(!fs.exists("scenes/mundo.goni"));
    EXPECT(fs.exists("scenes/mundo.001.goni"));   // a cópia sobreviveu
}
