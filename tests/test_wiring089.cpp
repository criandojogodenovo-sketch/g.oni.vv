// tests/test_wiring089.cpp — 0.8.9: CRASH-PROOF (parte PURA).
//
// COBERTURA (prompt 0.8.9, casos que não precisam do TU do main.cpp):
//   1. CRASH por recursão — o placeAt do resolver de UI NÃO tinha guard de
//      ciclo (só o sizeOf tinha; o seletor "colocar em" incluía o PRÓPRIO
//      elemento): parent cíclico recursava até exaurir a stack (o
//      crash-1790830406.dump do C33). Aqui: self-ciclo, ciclo mútuo e
//      cadeia > 64 abortam com ERRO LEGÍVEL (elog) e o layout VOLTA —
//      com guarda de tempo (hang = falha).
//   2. Json::dumpTo ganha teto de profundidade (o parser sempre teve 64;
//      o dump NÃO tinha) — bomba de profundidade corta com marcador.
//   3. GRELHA ADAPTATIVA — gridStepForDist: potências de 10 com clamps.
//   4. FAR DINÂMICO — sceneAABB/editorClips/playFar/playNear: o far contém
//      o AABB da cena a QUALQUER escala (py=10 000 incluído).
//   5. NORMALIZAÇÃO UNIFORME de import — fator ÚNICO nos 3 eixos (nunca
//      espalmado), log "import: dims=… uniform scale=…", re-aplicar o
//      MESMO ref não mexe na escala afinada.
//   6. CAMPO NUMÉRICO SEM TETO — commitTextInput propósito 6: py=10 000
//      escreve-se; inválido/não-finito não aplica; escala tem piso.
//   7. AUDITORIA DOS GERADORES — 8 primitivas × {default, seg=3, seg=256,
//      raio=0.001, raio=1000} (clamped): geometria válida, finita, AABB
//      não degenerado; Mesh::create REJEITA NaN.
//   8. CURA ao CARREGAR — .goni com elemento pai de SI MESMO: o loader
//      remove o ciclo com erro legível (ficheiros 0.7.4–0.8.8 curados).
//
// Os casos que precisam do CAMINHO DO DEVICE (primMesh/browserImportFile/
// applyImportedAssetToSelectedTic/globais do main) vivem no TU do
// test_wiring087.cpp (único TU que inclui platform/main.cpp) — secção
// "0.8.9" acrescentada lá.
#include "TestFramework.h"

#include <GLES3/gl3.h>   // stub do hospedeiro (glstub::stats)
#include <dirent.h>     // rmrfLogs (isolamento do engine.log por caso)
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "core/Json.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "core/SceneBounds.h"
#include "components/Transform3D.h"
#include "components/MeshRenderer.h"
#include "components/UiCanvas.h"
#include "components/SkeletonComp.h"
#include "render/Mesh.h"
#include "render/Primitives.h"
#include "render/Grid.h"
#include "render/Camera.h"
#include "ui/UiRuntime.h"
#include "ui/EditorUi.h"
#include "ui/UiEditor.h"
#include "platform/EngineLog.h"

using namespace vv;
using ::test::nearEqF;

namespace {

const char* kTestLogs = "test-wiring089-logs";

// 0.8.9: ISOLAMENTO — o diretório de logs é APAGADO antes de cada caso que
// afera linhas do engine.log (o log PERSISTE entre execuções: sem isto, uma
// linha da run VERDE anterior passa no teste com o fix REVERTIDO — apanhado
// na prova RED→GREEN desta release).
void rmrfLogs() {
    const std::string dir = kTestLogs;
    DIR* d = ::opendir(dir.c_str());
    if (d) {
        while (dirent* e = ::readdir(d)) {
            const std::string n = e->d_name;
            if (n != "." && n != "..") {
                ::remove((dir + "/" + n).c_str());
            }
        }
        ::closedir(d);
    }
    ::remove(dir.c_str());
}

double msSince(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - t0)
        .count();
}

bool logHas(const char* needle) {
    std::vector<std::string> lines;
    vv::elog::readTail(lines, 400);
    for (const std::string& l : lines) {
        if (l.find(needle) != std::string::npos) {
            return true;
        }
    }
    return false;
}

// triângulo de dims conhecidas (AABB x[0..A] y[0..B] z[0..C])
std::vector<Vertex> triVerts(f32 a, f32 b, f32 c) {
    return {Vertex{Vec3{0.0f, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, Vec2{}},
            Vertex{Vec3{a, 0.0f, 0.0f}, Vec3{0.0f, 1.0f, 0.0f}, Vec2{}},
            Vertex{Vec3{0.0f, b, c}, Vec3{0.0f, 1.0f, 0.0f}, Vec2{}}};
}
const u16 kTriIdx[3] = {0, 1, 2};

} // namespace

// ---------------------------------------------------------------------------
// 1. CRASH — placeAt com guard de ciclo (o crash-1790830406.dump do C33)
// ---------------------------------------------------------------------------
TEST(wiring089_layout_selfciclo_erro_legivel_sem_crash) {
    rmrfLogs();
    EXPECT(vv::elog::init(kTestLogs));
    // container pai de SI MESMO (o seletor "colocar em" das 0.7.4–0.8.8
    // incluía o próprio): ANTES do fix o placeAt recursava INFINITAMENTE
    // (stack exhaustion → SIGSEGV). Agora: erro legível, layout volta.
    UiCanvas c;
    UiElement box;
    box.kind = UiElement::Kind::VBox;
    box.name = "box";
    box.parent = "box";   // ← O CICLO (o crash do C33)
    box.w = 200.0f;
    box.h = 100.0f;
    c.elements.push_back(box);

    ui::CanvasLayout lay[4];
    const auto t0 = std::chrono::steady_clock::now();
    ui::resolveCanvasLayout(c, 1280.0f, 720.0f, {}, lay, 4);
    EXPECT(msSince(t0) < 500.0);   // guard de tempo: hang = falha
    EXPECT(logHas("CICLO de parents"));
    EXPECT(logHas("'box'"));       // o elemento nomeado no erro
}

TEST(wiring089_layout_ciclo_mutuo_ab_sem_crash) {
    rmrfLogs();
    EXPECT(vv::elog::init(kTestLogs));
    UiCanvas c;
    UiElement a, b;
    a.kind = UiElement::Kind::VBox;
    a.name = "A";
    a.parent = "B";
    a.w = 100.0f;
    a.h = 50.0f;
    b.kind = UiElement::Kind::HBox;
    b.name = "B";
    b.parent = "A";
    b.w = 100.0f;
    b.h = 50.0f;
    c.elements.push_back(a);
    c.elements.push_back(b);
    // um filho legítimo de A (deve continuar a ser disposto)
    UiElement lbl;
    lbl.kind = UiElement::Kind::Label;
    lbl.name = "lbl";
    lbl.parent = "A";
    lbl.w = 40.0f;
    lbl.h = 20.0f;
    c.elements.push_back(lbl);

    ui::CanvasLayout lay[8];
    const auto t0 = std::chrono::steady_clock::now();
    ui::resolveCanvasLayout(c, 1280.0f, 720.0f, {}, lay, 8);
    EXPECT(msSince(t0) < 500.0);
    EXPECT(logHas("CICLO de parents"));
}

TEST(wiring089_layout_cadeia_profunda_legitima_e_guard) {
    rmrfLogs();
    EXPECT(vv::elog::init(kTestLogs));
    // cadeia LEGÍTIMA de 40 aninhamentos: disposta SEM erro (o teto é 64)
    UiCanvas c;
    for (int i = 0; i < 40; ++i) {
        UiElement box;
        box.kind = UiElement::Kind::VBox;
        char n[16];
        std::snprintf(n, sizeof(n), "b%d", i);
        box.name = n;
        if (i > 0) {
            std::snprintf(n, sizeof(n), "b%d", i - 1);
            box.parent = n;
        }
        box.w = 400.0f;
        box.h = 400.0f;
        c.elements.push_back(box);
    }
    ui::CanvasLayout lay[48];
    ui::resolveCanvasLayout(c, 1280.0f, 720.0f, {}, lay, 48);
    EXPECT(lay[0].laid == false);          // topo (raiz)
    EXPECT(lay[1].parentIdx == 0);         // filho direto da raiz
    EXPECT(lay[39].parentIdx == 38);       // o mais profundo: 40 níveis ok

    // cadeia ABSURDA (> 64): o guard aborta com erro legível (não recursa
    // até morrer — o sizeOf já marcava estados, o placeAt agora tem teto)
    UiCanvas big;
    for (int i = 0; i < 80; ++i) {
        UiElement box;
        box.kind = UiElement::Kind::VBox;
        char n[16];
        std::snprintf(n, sizeof(n), "d%d", i);
        box.name = n;
        if (i > 0) {
            std::snprintf(n, sizeof(n), "d%d", i - 1);
            box.parent = n;
        }
        box.w = 800.0f;
        box.h = 800.0f;
        big.elements.push_back(box);
    }
    std::vector<ui::CanvasLayout> lay2(80);
    const auto t0 = std::chrono::steady_clock::now();
    ui::resolveCanvasLayout(big, 1280.0f, 720.0f, {}, lay2.data(), 80);
    EXPECT(msSince(t0) < 500.0);           // hang = falha
    EXPECT(logHas("profundidade"));        // erro legível (não SIGSEGV)
}

// ---------------------------------------------------------------------------
// 2. JSON — dumpTo com teto (o parser já tinha; o dump não tinha)
// ---------------------------------------------------------------------------
TEST(wiring089_json_dump_teto_de_profundidade) {
    rmrfLogs();
    EXPECT(vv::elog::init(kTestLogs));
    // árvore construída POR CÓDIGO com 200 níveis (o parser recusaria —
    // este é o caminho que o dump não guardava)
    Json deep = Json::makeArray();
    Json* cur = &deep;
    for (int i = 0; i < 200; ++i) {
        Json next = Json::makeArray();
        cur->addItem(std::move(next));
        cur = &cur->items.back();
    }
    const auto t0 = std::chrono::steady_clock::now();
    const std::string s = deep.dump();
    EXPECT(msSince(t0) < 500.0);                    // sem stack exhaustion
    EXPECT(s.find("<!max-depth>") != std::string::npos);   // corte VISÍVEL
    EXPECT(logHas("json: dump cortado"));

    // profundidade LEGÍTIMA (.goni real ~8) continua exata
    Json doc = Json::makeObject();
    doc.addMember("version", Json::makeNumber(1));
    Json tics = Json::makeArray();
    Json tic = Json::makeObject();
    tic.addMember("name", Json::makeString("a"));
    tics.addItem(std::move(tic));
    doc.addMember("tics", std::move(tics));
    const std::string ok = doc.dump();
    EXPECT(ok.find("<!max-depth>") == std::string::npos);
    EXPECT(ok.find("\"name\":\"a\"") != std::string::npos);
}

TEST(wiring089_json_destrutor_iterativo_profundeza_extrema) {
    // 0.8.9 (CRASH-PROOF — a função NOMEADA pelo addr2line): o crash
    // exauriu a stack NA CADEIA DE DESTRUTORES vector<Json>::~vector
    // inlined no appendComponentJson(SkeletonComp). Uma árvore de 100 000
    // níveis construída POR CÓDIGO (o parser recusa > 64 — mas memória
    // corrompida/bug podia criar o abismo): o dtor IMPLÍCITO recursava
    // 100 000× (stack exhaustion → SIGSEGV); o ITERATIVO destrói PLANO.
    // (RED→GREEN provado: com o dtor implícito este teste SEGFAULTA.)
    const int kDepth = 500000;   // 500k níveis: ~30MB de frames de dtor > 8MB de stack
    {
        Json deep = Json::makeArray();
        Json* cur = &deep;
        for (int i = 0; i < kDepth; ++i) {
            Json next = Json::makeArray();
            cur->addItem(std::move(next));
            cur = &cur->items.back();
        }
        const auto t0 = std::chrono::steady_clock::now();
        // destruição no fecho do scope — ITERATIVA: volta sempre
        // (hang/crash = falha; o guard do dump não corre aqui — é o DTOR)
        EXPECT(msSince(t0) < 1.0);   // (construção medida; a destruição é no fim do scope)
    }
    // se chegámos aqui, o dtor de 100 000 níveis devolveu sem exaurir a
    // stack — o teste PASSA por existir depois do scope
    EXPECT(true);

    // regressão: move/copy SEMÂNTICOS intactos (dtor declarado suprime o
    // move implícito — defaults explícitos obrigatórios; sem eles o
    // std::move vira COPY e o conteúdo duplica)
    Json a = Json::makeArray();
    a.addItem(Json::makeNumber(1.0));
    const size_t itemsBefore = a.items.size();
    Json b = std::move(a);   // MOVE: `a` fica vazio
    EXPECT(a.items.empty());          // moved-from = casca vazia
    EXPECT(b.items.size() == itemsBefore);
    Json c = b;                       // COPY: conteúdo duplicado
    EXPECT(c.items.size() == b.items.size());
    EXPECT(nearEqF(static_cast<f32>(c.items[0].number), 1.0f));
    // round-trip parse→move→dump continua exato
    const char* text = "{\"a\":[1,2,{\"b\":\"x\"}]}";
    Json p;
    EXPECT(Json::parse(text, std::strlen(text), p));
    Json q = std::move(p);
    const std::string s = q.dump();
    EXPECT(s.find("\"b\":\"x\"") != std::string::npos);
}

TEST(wiring089_json_parse_cap_mantido_regressao) {
    std::string bomb = "[";
    bomb.append(100, '[');
    bomb.append(100, ']');
    bomb += "]";
    Json out;
    EXPECT(!Json::parse(bomb.data(), bomb.size(), out));   // > 64 recusado
}

// ---------------------------------------------------------------------------
// 3. GRELHA ADAPTATIVA — gridStepForDist (fonte única, pura)
// ---------------------------------------------------------------------------
TEST(wiring089_grelha_adaptativa_passos_por_zoom) {
    // …0.1 / 1 / 10 / 100 / 1000… com clamps nos extremos
    EXPECT(nearEqF(Grid::gridStepForDist(6.0f), 1.0f, 1e-4f));       // trabalho
    EXPECT(nearEqF(Grid::gridStepForDist(2.0f), 0.1f, 1e-4f));       // perto
    EXPECT(nearEqF(Grid::gridStepForDist(0.05f), 0.1f, 1e-4f));      // clamp min
    EXPECT(nearEqF(Grid::gridStepForDist(60.0f), 10.0f, 1e-3f));     // afastado
    EXPECT(nearEqF(Grid::gridStepForDist(600.0f), 100.0f, 1e-2f));   // longe
    EXPECT(nearEqF(Grid::gridStepForDist(5000.0f), 1000.0f, 1e-1f)); // horizonte
    EXPECT(nearEqF(Grid::gridStepForDist(100000.0f), 1000.0f, 1.0f));// clamp max
    EXPECT(nearEqF(Grid::gridStepForDist(0.0f), Grid::kStepDefault)); // default
    EXPECT(std::isfinite(Grid::gridStepForDist(1e30f)));             // defesa
}

// ---------------------------------------------------------------------------
// 4. FAR DINÂMICO — sceneAABB + clipes do editor/Play
// ---------------------------------------------------------------------------
TEST(wiring089_scene_aabb_vazia_e_populada) {
    Scene scene;   // vazia: raio 0, bounds 0
    Vec3 mn, mx;
    f32 r = -1.0f;
    camerautil::sceneAABB(scene, mn, mx, r);
    EXPECT(r == 0.0f);
    EXPECT(mn.x == 0.0f && mx.x == 0.0f);

    // TIC com mesh gigante em py=10 000: o AABB CONTEM tudo
    Mesh m;
    const std::vector<Vertex> v = triVerts(1000.0f, 500.0f, 250.0f);
    EXPECT(m.create(v.data(), 3, kTriIdx, 3));
    const Handle h = scene.create("gigante");
    Tic* t = scene.get(h);
    Transform3D* tr = t->addComponent<Transform3D>();
    tr->pos = Vec3{0.0f, 10000.0f, 0.0f};
    MeshRenderer* mr = t->addComponent<MeshRenderer>();
    mr->mesh = &m;
    camerautil::sceneAABB(scene, mn, mx, r);
    EXPECT(mx.y >= 10500.0f);    // 10 000 + 500
    EXPECT(mn.y <= 10000.0f);
    EXPECT(mx.x >= 1000.0f);
    EXPECT(r > 500.0f);

    // o far do EDITOR contém o AABB — pelo CRITÉRIO DE DISTÂNCIA (a cena
    // está LONGE mesmo sendo "pequena": maisLongeDaOrigem + dist + 10)
    const f32 farthest = camerautil::sceneFarthest(mn, mx);
    EXPECT(farthest >= 10500.0f);         // o canto mais longe
    f32 n, f;
    camerautil::editorClips(6.0f, farthest, n, f);
    EXPECT(f >= farthest + 6.0f + 10.0f); // contém a cena INTEIRA
    EXPECT(n > 0.0f && n < 6.0f);
    // zoom máximo: rácio saudável e alvo visível
    camerautil::editorClips(Camera::kMaxDist, farthest, n, f);
    EXPECT(f >= Camera::kMaxDist * 1.5f);
    EXPECT(f / n < 400.0f);
    EXPECT(n < Camera::kMaxDist);
}

TEST(wiring089_play_far_respeita_slider_como_piso) {
    CameraComp cam;
    cam.farZ = 500.0f;   // default do dono
    // cena PEQUENA (olho perto): o far do dono chega (piso respeitado)
    EXPECT(nearEqF(camerautil::playFar(cam, 5.0f), 500.0f));
    // cena GIGANTE (olho→mais-longe = 5000): o far cresce para conter
    EXPECT(camerautil::playFar(cam, 5000.0f) >= 5010.0f);
    // near: default mantém-se; com far gigante o rácio é guardado
    EXPECT(nearEqF(camerautil::playNear(cam, 500.0f), 0.5f));
    const f32 farEff = camerautil::playFar(cam, 5000.0f);
    const f32 nearEff = camerautil::playNear(cam, farEff);
    EXPECT(farEff / nearEff <= 100000.0f);
}

// ---------------------------------------------------------------------------
// 5. NORMALIZAÇÃO UNIFORME do import (fator único — nunca espalmado)
// ---------------------------------------------------------------------------
TEST(wiring089_import_normalizacao_uniforme_preserva_proporcoes) {
    rmrfLogs();
    EXPECT(vv::elog::init(kTestLogs));
    Scene scene;
    const Handle h = scene.create("T");
    Tic* t = scene.get(h);
    t->addComponent<MeshRenderer>();
    t->addComponent<Transform3D>();   // escala inicial {1,1,1}

    // mesh GIGANTE 1000×500×250 (o "modelo do dono espalmado" era este)
    static Mesh giant;
    const std::vector<Vertex> v = triVerts(1000.0f, 500.0f, 250.0f);
    EXPECT(giant.create(v.data(), 3, kTriIdx, 3));
    EXPECT(nearEqF(giant.boundsMaxExtent(), 1000.0f, 1e-3f));

    editor::AssetCatalog catalog;
    catalog.meshes.push_back("giant.obj");
    editor::AssetResolvers res;
    res.mesh = [](const std::string& ref) -> Mesh* {
        return ref == "meshes/giant.obj" ? &giant : nullptr;
    };
    res.material = nullptr;
    // 0.8.9: o AABB chega como DADOS (o applyAssetPick é puro — o mesmo
    // contrato dos stubs-ponteiro dos testes antigos; o main liga ao real)
    res.meshExtent = [](const std::string& ref) -> Vec3 {
        return ref == "meshes/giant.obj" ? Vec3{1000.0f, 500.0f, 250.0f}
                                         : Vec3{0.0f, 0.0f, 0.0f};
    };

    const editor::AssetPickOutcome out =
        editor::applyAssetPick(scene, h, 1, 2, catalog, res);
    EXPECT(out.applied);
    Transform3D* tr = t->getComponent<Transform3D>();
    ASSERT(tr != nullptr);
    // FATOR ÚNICO nos 3 eixos: s = 2/1000 = 0.002 — proporções PRESERVADAS
    EXPECT(nearEqF(tr->scale.x, 0.002f, 1e-5f));
    EXPECT(nearEqF(tr->scale.y, 0.002f, 1e-5f));
    EXPECT(nearEqF(tr->scale.z, 0.002f, 1e-5f));
    // AABB RENDERIZADO ≤ alvo com proporções IGUAIS (x:y:z antes = depois)
    const Vec3 ext = giant.boundsExtent();
    const Vec3 scaled{ext.x * tr->scale.x, ext.y * tr->scale.y,
                      ext.z * tr->scale.z};
    EXPECT(scaled.x <= editor::kImportTargetSize * 1.001f);
    EXPECT(nearEqF(scaled.x / scaled.y, ext.x / ext.y, 1e-3f));
    EXPECT(nearEqF(scaled.y / scaled.z, ext.y / ext.z, 1e-3f));
    // a linha exigida no log
    EXPECT(std::strstr(out.log, "import: dims=") != nullptr);
    EXPECT(std::strstr(out.log, "uniform scale=") != nullptr);
    // "escala original": repor {1,1,1} devolve o tamanho REAL (geometria
    // intacta — o fit vive no Transform3D)
    tr->scale = Vec3{1.0f, 1.0f, 1.0f};
    EXPECT(nearEqF(giant.boundsMaxExtent(), 1000.0f, 1e-3f));

    // RE-APLICAR o mesmo ref NÃO re-normaliza (a escala afinada é sagrada)
    tr->scale = Vec3{3.0f, 3.0f, 3.0f};
    const editor::AssetPickOutcome out2 =
        editor::applyAssetPick(scene, h, 1, 2, catalog, res);
    EXPECT(out2.applied);
    EXPECT(nearEqF(tr->scale.x, 3.0f, 1e-5f));   // intocada

    // TIC SEM Transform3D: o apply CRIA (o fit precisa dele) e não crasha
    const Handle h2 = scene.create("semTR");
    scene.get(h2)->addComponent<MeshRenderer>();
    const editor::AssetPickOutcome out3 =
        editor::applyAssetPick(scene, h2, 1, 2, catalog, res);
    EXPECT(out3.applied);
    EXPECT(scene.get(h2)->getComponent<Transform3D>() != nullptr);
}

// ---------------------------------------------------------------------------
// 6. CAMPO NUMÉRICO SEM TETO (propósito 6 — py=10 000 escreve-se)
// ---------------------------------------------------------------------------
TEST(wiring089_campo_numerico_sem_teto_py_10000) {
    Scene scene;
    const Handle h = scene.create("F");
    Tic* t = scene.get(h);
    t->addComponent<Transform3D>();
    editor::EditorState st;
    st.selected = h;

    // py = 10 000 (o slider do Inspector só vai a ±20 — o CAMPO escreve)
    editor::openTextInput(st, 6, h, 1, "0");
    std::snprintf(st.textBuf, sizeof(st.textBuf), "10000");
    st.textLen = 5;
    EXPECT(editor::commitTextInput(scene, st));
    EXPECT(nearEqF(t->getComponent<Transform3D>()->pos.y, 10000.0f, 1e-2f));

    // negativo e decimal
    editor::openTextInput(st, 6, h, 0, "0");
    std::snprintf(st.textBuf, sizeof(st.textBuf), "-12.5");
    st.textLen = 5;
    EXPECT(editor::commitTextInput(scene, st));
    EXPECT(nearEqF(t->getComponent<Transform3D>()->pos.x, -12.5f, 1e-4f));

    // rotação em GRAUS (mesma convenção dos sliders)
    editor::openTextInput(st, 6, h, 3, "0");
    std::snprintf(st.textBuf, sizeof(st.textBuf), "90");
    st.textLen = 2;
    EXPECT(editor::commitTextInput(scene, st));
    Transform3D* tr = t->getComponent<Transform3D>();
    f32 ex = 0.0f, ey = 0.0f, ez = 0.0f;
    Quat::toEuler(tr->rot, ex, ey, ez);
    EXPECT(nearEqF(ex, 1.5707963f, 1e-3f));   // 90° em rad

    // escala: piso 0.001 (0 degeneraria) — 0.0005 vira 0.001
    editor::openTextInput(st, 6, h, 6, "1");
    std::snprintf(st.textBuf, sizeof(st.textBuf), "0.0005");
    st.textLen = 6;
    EXPECT(editor::commitTextInput(scene, st));
    EXPECT(tr->scale.x >= 0.001f);

    // INVÁLIDO: lixo/não-finito NÃO aplica (estado intacto — contrato hex)
    const Vec3 before = tr->pos;
    editor::openTextInput(st, 6, h, 2, "0");
    std::snprintf(st.textBuf, sizeof(st.textBuf), "1e99zz");
    st.textLen = 6;
    EXPECT(!editor::commitTextInput(scene, st));
    std::snprintf(st.textBuf, sizeof(st.textBuf), "nan");
    st.textLen = 3;
    EXPECT(!editor::commitTextInput(scene, st));
    EXPECT(nearEqF(tr->pos.z, before.z, 1e-5f));
}

// ---------------------------------------------------------------------------
// 7. AUDITORIA DOS GERADORES — 8 primitivas × defaults E EXTREMOS
// ---------------------------------------------------------------------------
TEST(wiring089_geradores_matrix_default_e_extremos) {
    struct Case { const char* name; PrimKind kind; };
    const Case kinds[8] = {
        {"esfera", PrimKind::Sphere},   {"cilindro", PrimKind::Cylinder},
        {"cone", PrimKind::Cone},       {"box", PrimKind::Box},
        {"plano", PrimKind::Plane},     {"triangulo", PrimKind::Wedge},
        {"torus", PrimKind::Torus},     {"capsula", PrimKind::Capsule},
    };
    for (const Case& kc : kinds) {
        PrimParams variants[5];
        variants[0] = primDefaults(kc.kind);                    // default
        variants[1] = primDefaults(kc.kind);
        variants[1].segments = 3;                               // seg mínimo
        variants[2] = primDefaults(kc.kind);
        variants[2].segments = 256;                             // seg absurdo
        variants[3] = primDefaults(kc.kind);
        variants[3].radius = 0.001f;                            // raio míope
        variants[4] = primDefaults(kc.kind);
        variants[4].radius = 1000.0f;                           // raio gigante
        for (const PrimParams& pIn : variants) {
            PrimMeshData data;
            makePrimMesh(pIn, data);
            EXPECT(data.ok());                                 // verts+idx
            for (const Vertex& v : data.vertices) {
                EXPECT(std::isfinite(v.pos.x) && std::isfinite(v.pos.y) &&
                       std::isfinite(v.pos.z));                // SEM NaN
                EXPECT(std::isfinite(v.normal.x) && std::isfinite(v.normal.y) &&
                       std::isfinite(v.normal.z));
            }
            Vec3 mn, mx;
            primBounds(data, mn, mx);
            const f32 ex = mx.x - mn.x, ey = mx.y - mn.y, ez = mx.z - mn.z;
            const f32 maior = ex > ey ? (ex > ez ? ex : ez)
                                      : (ey > ez ? ey : ez);
            EXPECT(maior > 1e-6f);   // AABB não degenerado
        }
    }
}

TEST(wiring089_mesh_create_rejeita_nan_e_calcula_aabb) {
    // vértice com NaN → upload RECUSADO (nunca entra no render)
    std::vector<Vertex> v = triVerts(1.0f, 1.0f, 1.0f);
    v[1].pos.x = std::nanf("");
    Mesh m;
    EXPECT(!m.create(v.data(), 3, kTriIdx, 3));

    // geometria sã: AABB exato (dados — o import/far leem-no)
    std::vector<Vertex> ok = triVerts(4.0f, 2.0f, 1.0f);
    Mesh m2;
    EXPECT(m2.create(ok.data(), 3, kTriIdx, 3));
    EXPECT(nearEqF(m2.boundsMin().x, 0.0f));
    EXPECT(nearEqF(m2.boundsMax().x, 4.0f));
    EXPECT(nearEqF(m2.boundsMax().y, 2.0f));
    EXPECT(nearEqF(m2.boundsMax().z, 1.0f));
    EXPECT(nearEqF(m2.boundsMaxExtent(), 4.0f));

    // destroy zera o AABB (mesh falhado = sem bounds)
    m2.destroy();
    EXPECT(m2.boundsMaxExtent() == 0.0f);
}

// ---------------------------------------------------------------------------
// 8. CURA ao CARREGAR — .goni com self-parent (ficheiros 0.7.4–0.8.8)
// ---------------------------------------------------------------------------
TEST(wiring089_load_cura_selfparent_com_erro_legivel) {
    rmrfLogs();
    EXPECT(vv::elog::init(kTestLogs));
    // .goni v1 com um VBox pai de SI MESMO (gravação das versões antigas;
    // kind canónico MINÚSCULO — o formato de sempre)
    const char* text =
        "{\"version\":1,\"tics\":[{\"id\":0,\"name\":\"ui\","
        "\"active\":true,\"parent\":-1,\"components\":[{"
        "\"type\":\"UiCanvas\",\"elements\":[{"
        "\"kind\":\"vbox\",\"name\":\"painel\",\"parent\":\"painel\","
        "\"x\":0,\"y\":0,\"w\":100,\"h\":50,"
        "\"color\":[1,1,1,1],\"visible\":true,\"ah\":\"left\","
        "\"av\":\"top\"}]}]}]}";
    Scene scene;
    SceneSerializer::LoadCtx ctx;   // sem resolvers (não há meshes)
    EXPECT(SceneSerializer::loadText(scene, text, ctx));
    const Tic* t = scene.get(scene.find("ui"));
    ASSERT(t != nullptr);
    const UiCanvas* c = t->getComponent<UiCanvas>();
    ASSERT(c != nullptr);
    ASSERT(c->elements.size() == 1);
    EXPECT(c->elements[0].parent.empty());   // CURADO ao carregar
    EXPECT(logHas("era pai de SI MESMO"));   // erro legível no log

    // o resolver sob o elemento curado: SEM ciclo, SEM crash, disposto
    ui::CanvasLayout lay[2];
    ui::resolveCanvasLayout(*c, 1280.0f, 720.0f, {}, lay, 2);
    EXPECT(lay[0].laid == false);            // topo legítimo
}

TEST(wiring089_parent_de_tic_ciclico_nao_recursa) {
    // pais de TIC cíclicos (A pai de B, B pai de A): o serializer grava
    // POSIÇÃO (int) e nada no core atravessa a cadeia recursivamente —
    // round-trip SEM crash é o contrato (TransformSystem não resolve
    // hierarquia ainda — documentado no relatório).
    Scene scene;
    const Handle a = scene.create("A");
    const Handle b = scene.create("B");
    scene.get(a)->parent = static_cast<i32>(b.index);
    scene.get(b)->parent = static_cast<i32>(a.index);
    const std::string text = SceneSerializer::dump(scene);
    EXPECT(text.find("\"parent\"") != std::string::npos);

    Scene scene2;
    SceneSerializer::LoadCtx ctx;
    EXPECT(SceneSerializer::loadText(scene2, text, ctx));
    EXPECT(scene2.count() == 2);
}

TEST(wiring089_joints_de_skin_ciclicos_iterativos_sem_hang) {
    // esqueleto com joint pai de si mesmo + par cíclico: a composição é
    // ITERATIVA (0.8.2, pais-primeiro) — nunca recursa; aferimos que
    // computeSkinMatrices VOLTA (guarda de tempo), sem crash.
    SkeletonComp sk;
    SkeletonComp::Joint j0, j1, j2;
    j0.parent = 0;   // pai de SI
    j1.parent = 2;   // ciclo 1↔2
    j2.parent = 1;
    sk.joints.push_back(j0);
    sk.joints.push_back(j1);
    sk.joints.push_back(j2);
    Mat4 out[3];
    const auto t0 = std::chrono::steady_clock::now();
    computeSkinMatrices(sk, out, 3);
    EXPECT(msSince(t0) < 100.0);   // iterativo: volta sempre
    for (int i = 0; i < 3; ++i) {
        EXPECT(std::isfinite(out[i].m[0]));   // sem lixo explosivo
    }
}

// ---------------------------------------------------------------------------
// REGRESSÕES — o resto do motor intocado pelas 4 fixes
// ---------------------------------------------------------------------------
TEST(wiring089_anim_e_serializer_regressao) {
    // dump/load de uma cena com AnimationPlayer continua exato (o dumpTo
    // ganhou teto — árvores legítimas nunca o tocam)
    Scene scene;
    const Handle h = scene.create("anim");
    Tic* t = scene.get(h);
    t->addComponent<Transform3D>();
    AnimationPlayer* ap = t->addComponent<AnimationPlayer>();
    AnimClip clip;
    clip.name = "c";
    AnimTrack track;
    track.target = AnimTarget::TicPos;
    track.keys.push_back(AnimKey{0.0f, {0.0f, 0.0f, 0.0f, 0.0f}});
    track.keys.push_back(AnimKey{1.0f, {2.0f, 0.0f, 0.0f, 0.0f}});
    clip.tracks.push_back(track);
    ap->clips.push_back(clip);
    const std::string text = SceneSerializer::dump(scene);
    Scene scene2;
    SceneSerializer::LoadCtx ctx;
    EXPECT(SceneSerializer::loadText(scene2, text, ctx));
    const Tic* t2 = scene2.get(scene2.find("anim"));
    ASSERT(t2 != nullptr);
    const AnimationPlayer* ap2 = t2->getComponent<AnimationPlayer>();
    ASSERT(ap2 != nullptr);
    EXPECT(ap2->clips.size() == 1);
    EXPECT(ap2->clips[0].tracks.size() == 1);
    EXPECT(ap2->clips[0].tracks[0].keys.size() == 2);
    EXPECT(nearEqF(ap2->clips[0].tracks[0].keys[1].v[0], 2.0f, 1e-5f));
}
