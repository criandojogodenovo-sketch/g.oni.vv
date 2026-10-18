// tests/test_012a_lodui.cpp — 0.12-A (A-1): as rows "lod distance" e
// "impostor" no Inspector do TIC com mesh file — OS PINS DA UI.
//
// O GAP que motivou a sub-fase: os pins da 3.5a (test_fase35a_lod_cull.cpp)
// e da 3.75b (test_fase375b_impostors.cpp) aferram o CAMPO, o RENDER e a
// SERIALIZAÇÃO — nenhum aferra a UI. O diagnóstico A-1 (§4 do
// RELATORIO-0.12-A) provou que a UI JÁ EXISTE desde a 3.5a/3.75b
// (EditorLayout.h: o push das rows; EditorUi.cpp: o draw + o slider que
// escreve no MeshRenderer) — o prompt manda PÁRA no fix («não faz fix
// cego»); estes pins aferram o contrato EXISTENTE para o gap nunca mais
// reabrir (uma refatoração que mate as rows fica VERMELHA):
//   1. lodui_row_existe — o PLANO do Inspector de um TIC com mesh file tem
//      as 2 rows (kind + id) na secção Malha; TIC sem mesh file NÃO tem
//      (a condição p.fileMesh é o contrato); y cumulativo intacto.
//   2. lodui_aplica — o gesto REAL (down → frame → up) no trilho do slider
//      escreve no MeshRenderer do TIC selecionado (o MESMO caminho do
//      device: drawInspector + InputState injetado).
//   3. lodui_persiste — round-trip .goni: "lodDist"/"impostorDist" viajam
//      quando != 0 e NÃO nascem quando 0 (o contrato byte a byte do dump).
//   4. lodui_compat — .goni SEM os campos abre com os defaults 0 e volta a
//      sair BYTE A BYTE (os ficheiros da era 0.11-S ficam intocados).
//   5. lodui_default_off — TIC novo tem lodDistance=0 e impostorDistance=0
//      (o default jogável manda — o pin manda nele).
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <string>

#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "core/SceneSerializer.h"
#include "platform/InputState.h"
#include "ui/EditorLayout.h"
#include "ui/EditorUi.h"
#include "ui/FontAtlas.h"
#include "ui/SafeArea.h"
#include "ui/Theme.h"
#include "ui/Toolbar.h"
#include "ui/UiContext.h"

using namespace vv;
using namespace vv::editor;
using ::test::nearEqF;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;
constexpr f32 kFontPx = 28.0f;   // o MESMO heightPx do main.cpp no device

// ambiente mínimo do frame do editor (o padrão do test_ui/test_uieditor:
// fonte REAL a 28 px + gestos injetados — a UI de verdade, não um stub)
struct Env {
    FontAtlas  font;
    UiContext  ui;
    InputState input;
    Scene      scene;
    EditorState st;
    toolbar::GizmoModeState gzMode;
    Handle     selected{};
    bool       ok = false;

    explicit Env(const char* meshRef) {
        const char* fontPath = FONT_FIXTURE;
        ok = font.loadFromPaths(&fontPath, 1, kFontPx);
        if (!ok) {
            return;
        }
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
        selected = createTicFromPreset(scene, PresetKind::PlayerBody3D,
                                       nullptr, nullptr);
        if (Tic* tic = scene.get(selected)) {
            if (MeshRenderer* mr = tic->getComponent<MeshRenderer>()) {
                mr->meshPath = meshRef;   // o "mesh file" do caso A-1
            }
        }
        st.selected = selected;
    }

    void frame() {
        ui.beginFrame(nullptr, &input, kSW, kSH);
        toolbar::draw(ui, st);
        drawHierarchy(ui, scene, st);
        drawInspector(ui, scene, st, nullptr);   // sem catálogo (o editor de cena)
        ui.statusLine("status");
        ui.endFrame();
        input.clearEdges();
    }

    // o gesto do device: down → frame (o slider captura e escreve) → up
    void press(f32 x, f32 y) {
        input.injectDown(0, x, y);
        frame();
        input.injectUp(0);
        frame();
    }
};

// o plano do TIC com mesh file + a row pedida (nullptr se ausente)
const InspRow* planRow(const InspProfile& prof, const TextMetrics& tm,
                       InspRow::Kind kind, u64 wantId, InspRow* plan) {
    const u32 n = inspectorPlan(prof, tm, false, 0u, plan);
    for (u32 i = 0; i < n; ++i) {
        if (plan[i].kind == kind && plan[i].id == wantId) {
            return &plan[i];
        }
    }
    return nullptr;
}

} // namespace

// ---- PIN 1 — as rows EXISTEM no plano (o gap aferrado) ----------------------
TEST(lodui_row_existe) {
    Env e("meshes/edificio.gmesh");
    ASSERT(e.ok);
    const Tic* tic = e.scene.get(e.selected);
    ASSERT(tic != nullptr);
    const InspProfile prof = inspectorProfile(*tic);
    ASSERT(prof.mr);
    EXPECT(prof.fileMesh);   // mesh file (o caso do dono)
    const TextMetrics tm = e.ui.textMetrics();
    InspRow plan[64];
    const u32 n = inspectorPlan(prof, tm, false, 0u, plan);

    // as 2 rows, com os IDS da faixa 5311..5314 (a identidade do tap)
    const InspRow* lod = planRow(prof, tm, InspRow::Kind::LodDistance,
                                 kInspectorLodDist, plan);
    const InspRow* imp = planRow(prof, tm, InspRow::Kind::ImpostorDistance,
                                 kInspectorImpostorDist, plan);
    EXPECT(lod != nullptr);
    EXPECT(imp != nullptr);
    if (!lod || !imp) {
        return;
    }
    EXPECT(lod->id == kInspectorLodDist);          // 5311 (0.11-T 3.5a)
    EXPECT(imp->id == kInspectorImpostorDist);     // 5314 (0.11-W 3.75b)
    // a ORDEM do device: dims → escala → lod → instancing → streaming →
    // impostor (as 6 rows do bloco fileMesh, TODAS no plano)
    u32 iDims = n, iScale = n, iLod = n, iImp = n;
    for (u32 i = 0; i < n; ++i) {
        if (plan[i].kind == InspRow::Kind::DimsLabel) iDims = i;
        if (plan[i].kind == InspRow::Kind::ScaleOrig) iScale = i;
        if (plan[i].kind == InspRow::Kind::LodDistance) iLod = i;
        if (plan[i].kind == InspRow::Kind::ImpostorDistance) iImp = i;
    }
    EXPECT(iDims < iScale && iScale < iLod && iLod < iImp);
    // o bloco fileMesh tem EXATAMENTE as 6 rows (nada comeu o plano)
    u32 fileBlock = 0;
    for (u32 i = 0; i < n; ++i) {
        if (plan[i].kind == InspRow::Kind::DimsLabel ||
            plan[i].kind == InspRow::Kind::ScaleOrig ||
            plan[i].kind == InspRow::Kind::LodDistance ||
            plan[i].kind == InspRow::Kind::Instancing ||
            plan[i].kind == InspRow::Kind::Streaming ||
            plan[i].kind == InspRow::Kind::ImpostorDistance) {
            ++fileBlock;
        }
    }
    EXPECT(fileBlock == 6u);
    // y cumulativo SEM sobreposição (o contrato F5.0-fix no bloco novo)
    for (u32 i = 1; i < n; ++i) {
        EXPECT(plan[i].y >= plan[i - 1].y + plan[i - 1].h - 0.01f);
    }

    // o NEGATIVO é o contrato: TIC SEM mesh file (o prim "cube" de sempre)
    // NÃO tem as rows — a condição p.fileMesh é quem as guarda
    Scene s2;
    const Handle h2 = s2.create("Cubo");
    Tic* t2 = s2.get(h2);
    ASSERT(t2 != nullptr);
    t2->addComponent<Transform3D>();
    MeshRenderer* mr2 = t2->addComponent<MeshRenderer>();
    ASSERT(mr2 != nullptr);
    mr2->primOn = true;   // primitiva procedural (meshPath vazio)
    const InspProfile prof2 = inspectorProfile(*t2);
    EXPECT(!prof2.fileMesh);
    const InspRow* lod2 = planRow(prof2, tm, InspRow::Kind::LodDistance, 0,
                                  plan);
    const InspRow* imp2 = planRow(prof2, tm, InspRow::Kind::ImpostorDistance,
                                  0, plan);
    EXPECT(lod2 == nullptr);   // (a procura por id 0 falha sempre — a row
    EXPECT(imp2 == nullptr);   //  nem EXISTE no plano do cubo)
}

// ---- PIN 2 — o tap no trilho ESCREVE no MeshRenderer ------------------------
TEST(lodui_aplica) {
    Env e("meshes/edificio.gmesh");
    ASSERT(e.ok);
    e.frame();   // o 1.º frame cria o slot de scroll do Inspector

    const Tic* tic = e.scene.get(e.selected);
    ASSERT(tic != nullptr);
    const InspProfile prof = inspectorProfile(*tic);
    const TextMetrics tm = e.ui.textMetrics();
    InspRow plan[64];
    (void)inspectorPlan(prof, tm, false, 0u, plan);
    const InspRow* lod = planRow(prof, tm, InspRow::Kind::LodDistance,
                                 kInspectorLodDist, plan);
    const InspRow* imp = planRow(prof, tm, InspRow::Kind::ImpostorDistance,
                                 kInspectorImpostorDist, plan);
    ASSERT(lod != nullptr);
    ASSERT(imp != nullptr);

    const UiRect panel = safe::inspectorPanelRect(kSW, kSH, safe::Insets{});
    const f32 contentTop = panel.y + kHeaderH + 4.0f;
    const f32 listH = panel.h - kHeaderH - 4.0f;
    const f32 contentH = inspectorContentHeight(prof, tm, false, 0u);

    // o trilho do sliderRow: x + dp(84), largura dp(118) — a fonte ÚNICA
    // da geometria (o MESMO cálculo do draw e do dispatch)
    const f32 trackX = panel.x + theme::dp(84.0f);
    const f32 trackW = theme::dp(118.0f);

    // --- LOD: tap a 50 % do trilho → 250 u (faixa 0..500) ---
    {
        const f32 off = scroll::clampOffset(lod->y - 24.0f, contentH, listH);
        e.ui.scrollSetOffset(kInspectorScrollId, off);
        const f32 ty = contentTop + lod->y - off + lod->h * 0.5f;
        EXPECT(nearEqF(tic->getComponent<MeshRenderer>()->lodDistance, 0.0f));
        e.press(trackX + trackW * 0.5f, ty);
        MeshRenderer* mr = e.scene.get(e.selected)->getComponent<MeshRenderer>();
        EXPECT(mr != nullptr);
        if (mr) {
            // 250 ± 1 (o slider é contínuo — o tap dá o valor do ponto)
            EXPECT(mr->lodDistance > 240.0f && mr->lodDistance < 260.0f);
        }
    }
    // --- IMPOSTOR: tap a 20 % do trilho → 100 u (faixa 0..500) ---
    {
        const f32 off = scroll::clampOffset(imp->y - 24.0f, contentH, listH);
        e.ui.scrollSetOffset(kInspectorScrollId, off);
        const f32 ty = contentTop + imp->y - off + imp->h * 0.5f;
        e.press(trackX + trackW * 0.2f, ty);
        MeshRenderer* mr = e.scene.get(e.selected)->getComponent<MeshRenderer>();
        EXPECT(mr != nullptr);
        if (mr) {
            EXPECT(mr->impostorDistance > 90.0f && mr->impostorDistance < 110.0f);
        }
    }
    // o tap FORA do trilho (zona da label) NÃO escreve (o rect é o contrato)
    {
        const f32 off = scroll::clampOffset(lod->y - 24.0f, contentH, listH);
        e.ui.scrollSetOffset(kInspectorScrollId, off);
        const f32 ty = contentTop + lod->y - off + lod->h * 0.5f;
        const f32 antes = e.scene.get(e.selected)
                              ->getComponent<MeshRenderer>()
                              ->lodDistance;
        e.press(panel.x + theme::dp(20.0f), ty);   // na label, longe do trilho
        EXPECT(nearEqF(
            e.scene.get(e.selected)->getComponent<MeshRenderer>()->lodDistance,
            antes, 0.01f));
    }
}

// ---- PIN 3 — o round-trip .goni transporta os campos ------------------------
TEST(lodui_persiste) {
    Scene s1;
    const Handle h1 = s1.create("Torre");
    Tic* t1 = s1.get(h1);
    ASSERT(t1 != nullptr);
    t1->addComponent<Transform3D>();
    MeshRenderer* mr1 = t1->addComponent<MeshRenderer>();
    ASSERT(mr1 != nullptr);
    mr1->meshPath = "meshes/edificio.gmesh";
    mr1->lodDistance = 30.0f;
    mr1->impostorDistance = 50.0f;

    const std::string text = SceneSerializer::dump(s1);
    // as chaves nasceram (só quando != 0 — o contrato do dump)
    EXPECT(text.find("lodDist") != std::string::npos);
    EXPECT(text.find("impostorDist") != std::string::npos);

    SceneSerializer::LoadCtx ctx;   // sem resolvers (o host de testes)
    Scene s2;
    EXPECT(SceneSerializer::loadText(s2, text, ctx));
    const Handle h2 = s2.find("Torre");
    const Tic* t2 = h2.valid() ? s2.get(h2) : nullptr;
    ASSERT(t2 != nullptr);
    const MeshRenderer* mr2 = t2->getComponent<MeshRenderer>();
    ASSERT(mr2 != nullptr);
    EXPECT(nearEqF(mr2->lodDistance, 30.0f));
    EXPECT(nearEqF(mr2->impostorDistance, 50.0f));

    // e o NEGATIVO: 0/0 NÃO nasce no ficheiro (os .goni antigos intocados)
    Scene s3;
    const Handle h3 = s3.create("Limpo");
    Tic* t3 = s3.get(h3);
    ASSERT(t3 != nullptr);
    t3->addComponent<Transform3D>();
    MeshRenderer* mr3 = t3->addComponent<MeshRenderer>();
    ASSERT(mr3 != nullptr);
    mr3->meshPath = "meshes/edificio.gmesh";   // lod/impostor ficam 0
    const std::string clean = SceneSerializer::dump(s3);
    EXPECT(clean.find("lodDist") == std::string::npos);
    EXPECT(clean.find("impostorDist") == std::string::npos);
}

// ---- PIN 4 — .goni SEM os campos abre byte a byte ---------------------------
TEST(lodui_compat) {
    // o ficheiro "da era 0.11-S": um dump SEM lodDist/impostorDist (o dump
    // de um TIC com os defaults — o MESMO bytes que a era produzia)
    Scene s1;
    const Handle h1 = s1.create("Antigo");
    Tic* t1 = s1.get(h1);
    ASSERT(t1 != nullptr);
    t1->addComponent<Transform3D>();
    MeshRenderer* mr1 = t1->addComponent<MeshRenderer>();
    ASSERT(mr1 != nullptr);
    mr1->meshPath = "meshes/edificio.gmesh";
    const std::string antigo = SceneSerializer::dump(s1);
    EXPECT(antigo.find("lodDist") == std::string::npos);
    EXPECT(antigo.find("impostorDist") == std::string::npos);

    // abre com os DEFAULTS 0 (o loader declara-os explicitamente — o
    // contrato de compat: ausente = 0 = OFF)
    SceneSerializer::LoadCtx ctx;
    Scene s2;
    EXPECT(SceneSerializer::loadText(s2, antigo, ctx));
    const Handle h2 = s2.find("Antigo");
    const Tic* t2 = h2.valid() ? s2.get(h2) : nullptr;
    ASSERT(t2 != nullptr);
    const MeshRenderer* mr2 = t2->getComponent<MeshRenderer>();
    ASSERT(mr2 != nullptr);
    EXPECT(nearEqF(mr2->lodDistance, 0.0f));
    EXPECT(nearEqF(mr2->impostorDistance, 0.0f));

    // e re-dump é BYTE A BYTE o ficheiro antigo (nenhuma chave fantasma)
    const std::string reDump = SceneSerializer::dump(s2);
    EXPECT(reDump == antigo);
}

// ---- PIN 5 — TIC novo nasce com LOD/impostor OFF ----------------------------
TEST(lodui_default_off) {
    // (a) o preset do "+"
    {
        Scene s;
        const Handle h = createTicFromPreset(s, PresetKind::PlayerBody3D,
                                             nullptr, nullptr);
        Tic* t = s.get(h);
        ASSERT(t != nullptr);
        const MeshRenderer* mr = t->getComponent<MeshRenderer>();
        ASSERT(mr != nullptr);
        EXPECT(nearEqF(mr->lodDistance, 0.0f));
        EXPECT(nearEqF(mr->impostorDistance, 0.0f));
    }
    // (b) o componente nu (o caminho do serializer/picker)
    {
        Scene s;
        const Handle h = s.create("Novo");
        Tic* t = s.get(h);
        ASSERT(t != nullptr);
        const MeshRenderer* mr = t->addComponent<MeshRenderer>();
        ASSERT(mr != nullptr);
        EXPECT(nearEqF(mr->lodDistance, 0.0f));
        EXPECT(nearEqF(mr->impostorDistance, 0.0f));
    }
}
