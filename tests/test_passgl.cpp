// tests/test_passgl.cpp — 0.7.8: SEPARAÇÃO RENDER 3D ↔ UI NO PLAY.
//
// REGRESSÃO DO C33 (a UI de jogo gigante/cortada): no Play com câmara ativa
// e UiCanvas, o pass de UI desenhava com o estado GL que o pass 3D da câmara
// deixou — o viewport vinha do último glViewport (só afirmado no resize),
// o scissor nunca era gerado, o depth/cull só eram desligados DENTRO do
// endFrame (e o early-return com 0 submissões nem isso) e a ortográfica de
// ecrã usava w_/h_ em cache. A fronteira era HERANÇA IMPLÍCITA.
//
// Aqui aferimos o CONTRATO novo com o ESTADO do stub GL (glstub::stats —
// tests/stub/GLES3/gl3.h):
//   • beginUiPass repõe o viewport CHEIO (tamanho ATUAL) e desliga
//     scissor/depth/cull — venha o estado de onde vier (pass 3D sujado a
//     propósito: viewport de câmara, scissor on, depth on, cull on);
//   • endFrame afirma o estado MESMO sem submissões (o early-return antigo
//     deixava o depth do pass 3D ligado para o frame seguinte);
//   • endFrame envia a ORTOGRÁFICA DE ECRÃ (a do editor/modo UI) coerente
//     com o viewport do mesmo frame;
//   • em Play com câmara ATIVA + UiCanvas os rects desenhados são OS RECTS
//     DO RESOLVER (coords de ecrã + safe-area) — NADA gigante/fora — e o
//     layout NÃO depende da câmara (o resolver nem a recebe: o contrato
//     está no tipo);
//   • textScale reposto a 1.0 por frame (o Play nunca herda a escala do
//     viewport 2D do editor).
#include "TestFramework.h"
#include <GLES3/gl3.h>   // stub do hospedeiro (glstub::stats)
#include <cmath>
#include <cstdio>
#include <vector>
#include "components/CameraComp.h"
#include "components/Transform3D.h"
#include "components/UiCanvas.h"
#include "core/Scene.h"
#include "render/Renderer.h"
#include "ui/FontAtlas.h"
#include "ui/SafeArea.h"
#include "ui/UiContext.h"
#include "ui/UiRuntime.h"

using namespace vv;
using namespace vv::ui;
using ::test::nearEqF;

namespace {
constexpr f32 kSW = 1600.0f;   // C33 em landscape
constexpr f32 kSH = 720.0f;
const char* kFontPath = FONT_FIXTURE;

// suja o estado GL como o PASS 3D da câmara de jogo o deixaria (o pior
// caso do C33: viewport estranho da re-criação de superfície + tudo ligado)
void sujarEstadoDoPass3D() {
    glViewport(37, 91, 640, 360);   // viewport PARCIAL/estranho
    glEnable(GL_SCISSOR_TEST);      // scissor esquecido (recorta clear+draw)
    glEnable(GL_DEPTH_TEST);        // LitMaterial deixa ligado
    glEnable(GL_CULL_FACE);
}

// o pass de UI completo do main (fronteira + widgets + submissão), com o
// renderer REAL contra o stub — o mesmo caminho do device
DrawStats frameUi(Renderer& r, UiContext& ui, i32 w, i32 h) {
    r.beginUiPass(w, h);            // 0.7.8: a fronteira explícita
    ui.beginFrame(&r, nullptr, static_cast<f32>(w), static_cast<f32>(h));
    const f32 panel[4] = {0.1f, 0.1f, 0.1f, 1.0f};
    ui.panel(100.0f, 200.0f, 300.0f, 80.0f, panel);   // um widget qualquer
    ui.endFrame();
    return r.endFrame();
}
} // namespace

// ---- 1. a fronteira repõe o estado do pass 3D -------------------------------

TEST(passgl_beginuipass_repoe_viewport_cheio_e_desliga_estado_3d) {
    glstub::reset();
    Renderer r;
    EXPECT(r.init());
    r.resize(1600, 720);
    sujarEstadoDoPass3D();

    r.beginUiPass(1600, 720);
    EXPECT(glstub::stats.viewport[0] == 0 && glstub::stats.viewport[1] == 0);
    EXPECT(glstub::stats.viewport[2] == 1600);
    EXPECT(glstub::stats.viewport[3] == 720);
    EXPECT(!glstub::stats.scissorEnabled);   // a UI nunca é recortada
    EXPECT(!glstub::stats.depthEnabled);     // a UI nunca é ocluída pelo 3D
    EXPECT(!glstub::stats.cullEnabled);      // winding y-down do quad batch
}

TEST(passgl_beginuipass_refresca_tamanho_atual_da_superficie) {
    glstub::reset();
    Renderer r;
    EXPECT(r.init());
    r.resize(1600, 720);
    // a superfície mudou (barras esconderam) e o resize antigo ficou stale:
    // o beginUiPass do frame ATUAL impõe o tamanho que o main lê do EGL —
    // viewport e ortográfica coerentes com o layout do MESMO frame
    sujarEstadoDoPass3D();
    r.beginUiPass(1600, 800);
    EXPECT(glstub::stats.viewport[2] == 1600);
    EXPECT(glstub::stats.viewport[3] == 800);
}

TEST(passgl_endframe_afirma_estado_mesmo_sem_submissoes) {
    glstub::reset();
    Renderer r;
    EXPECT(r.init());
    r.resize(1600, 720);
    sujarEstadoDoPass3D();

    UiContext ui;
    ui.init();
    // pass de UI VAZIO (nenhum widget — o early-return antigo não afirmava
    // NADA e o frame seguinte herdava o depth/cull do pass 3D)
    r.beginUiPass(1600, 720);
    ui.beginFrame(&r, nullptr, 1600.0f, 720.0f);
    ui.endFrame();
    const DrawStats st = r.endFrame();
    EXPECT(st.drawCalls == 0);              // nada desenhado
    EXPECT(glstub::stats.viewport[2] == 1600 && glstub::stats.viewport[3] == 720);
    EXPECT(!glstub::stats.depthEnabled);
    EXPECT(!glstub::stats.cullEnabled);
    EXPECT(!glstub::stats.scissorEnabled);
}

TEST(passgl_endframe_envia_ortografica_de_ecra_coerente_com_viewport) {
    glstub::reset();
    Renderer r;
    EXPECT(r.init());
    r.resize(1600, 720);
    sujarEstadoDoPass3D();

    UiContext ui;
    ui.init();
    const DrawStats st = frameUi(r, ui, 1600, 720);
    EXPECT(st.drawCalls > 0);               // o painel foi submetido
    EXPECT(glstub::stats.drawArraysCalls > 0);
    // a ORTOGRÁFICA DE ECRÃ (a do editor/modo UI): y para baixo, origem no
    // topo-esquerda, 0..w × 0..h — exatamente a que o resolver pressupõe
    const Mat4 esperada = Mat4::ortho(0.0f, 1600.0f, 720.0f, 0.0f, -1.0f, 1.0f);
    EXPECT(glstub::stats.matrix4fvCalls > 0);
    for (int i = 0; i < 16; ++i) {
        EXPECT(nearEqF(glstub::stats.lastMatrix4fv[i], esperada.m[i], 1e-3f));
    }
    // blend LIGADO na submissão (alpha da UI) e desligado no fim (higiene)
    EXPECT(!glstub::stats.blendEnabled);
}

TEST(passgl_play_inteiro_ui_sobre_pass_3d_sujado_estado_correto_no_fim) {
    glstub::reset();
    Renderer r;
    EXPECT(r.init());
    r.resize(1600, 720);

    // pass 3D do Play com câmara ativa (main: beginFrame + drawMesh/grid)
    r.beginFrame();
    sujarEstadoDoPass3D();   // o que o LitMaterial/grid deixam cair (e pior)

    UiContext ui;
    ui.init();
    const DrawStats st = frameUi(r, ui, 1600, 720);
    EXPECT(st.drawCalls > 0);
    EXPECT(glstub::stats.viewport[0] == 0 && glstub::stats.viewport[1] == 0 &&
           glstub::stats.viewport[2] == 1600 && glstub::stats.viewport[3] == 720);
    EXPECT(!glstub::stats.depthEnabled);
    EXPECT(!glstub::stats.cullEnabled);
    EXPECT(!glstub::stats.scissorEnabled);
}

// ---- 2. Play com câmara ativa + UiCanvas: rects = rects do resolver ----------

TEST(passgl_play_camera_ativa_uicanvas_rects_do_resolver) {
    Scene scene;
    const Handle hTic = scene.create("HUD");
    Tic* tic = scene.get(hTic);
    EXPECT(tic != nullptr);
    UiCanvas* c = tic->addComponent<UiCanvas>();
    EXPECT(c != nullptr);
    UiElement& btn = c->elements.emplace_back();
    btn.kind = UiElement::Kind::Button;
    btn.text = "PLAY";
    btn.anchorH = UiElement::AnchorH::Left;
    btn.anchorV = UiElement::AnchorV::Bottom;
    btn.ox = 60.0f;
    btn.oy = -40.0f;   // 40px ACIMA da borda inferior (a âncora conta p/ baixo)
    btn.w = 220.0f;
    btn.h = 72.0f;

    // uma CÂMARA ATIVA na cena (o cenário do C33: o pass 3D corria por ela)
    Tic* cam = scene.get(scene.create("Cam"));
    EXPECT(cam != nullptr);
    Transform3D* ct = cam->addComponent<Transform3D>();
    CameraComp* cc = cam->addComponent<CameraComp>();
    ct->pos = Vec3{0.0f, 3.0f, 8.0f};
    cc->active = true;

    const safe::Insets ins{10.0f, 24.0f, 10.0f, 20.0f};
    std::vector<CanvasLayout> L(c->elements.size());
    resolveCanvasLayout(*c, kSW, kSH, ins, L.data(),
                        static_cast<u32>(c->elements.size()));
    const UiRect esperado = L[0].rect;   // o resolver é a fonte única

    FontAtlas font;
    EXPECT(font.loadFromPaths(&kFontPath, 1, 28.0f));
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    Renderer r;
    EXPECT(r.init());
    r.resize(1600, 720);

    // o frame de Play: pass 3D pela câmara ativa (sujamos o estado como o
    // LitMaterial faria) → fronteira → drawCanvasPlay por cima
    glstub::reset();
    r.beginFrame();
    sujarEstadoDoPass3D();
    r.beginUiPass(1600, 720);
    ui.beginFrame(&r, nullptr, kSW, kSH);
    ui.setSafeArea(ins);
    // drawCanvasPlay do main: canvases dos TICs ativos/visíveis
    scene.forEachActive([&](const Tic& t) {
        if (!t.visible) {
            return;
        }
        if (const UiCanvas* ccv = scene.components().uiCanvases().find(t.handle)) {
            drawCanvas(ui, *ccv, kSW, kSH, ins);
        }
    });
    ui.endFrame();
    const DrawStats st = r.endFrame();

    // o BUTTON desenhou: um quad do panel do botão começa EXATAMENTE no
    // rect do resolver (nada gigante/fora — o bug do C33)
    EXPECT(st.drawCalls > 0);
    bool achou = false;
    const QuadBatch& solids = ui.solidsForTest();
    for (u32 i = 0; i + 5u < solids.vertexCount(); i += 6u) {
        const QuadVertex& v = solids.vertices()[i];
        if (nearEqF(v.x, esperado.x, 0.5f) && nearEqF(v.y, esperado.y, 0.5f)) {
            achou = true;
            break;
        }
    }
    EXPECT(achou);
    // o rect do resolver está DENTRO do ecrã (coords de ecrã + safe-area)
    EXPECT(esperado.x >= ins.left - 0.5f);
    EXPECT(esperado.y >= ins.top - 0.5f);
    EXPECT(esperado.x + esperado.w <= kSW - ins.right + 0.5f);
    EXPECT(esperado.y + esperado.h <= kSH - ins.bottom + 0.5f);
    // o estado GL no fim da submissão continua o do pass de UI
    EXPECT(glstub::stats.viewport[2] == 1600 && glstub::stats.viewport[3] == 720);
    EXPECT(!glstub::stats.depthEnabled);
    EXPECT(!glstub::stats.scissorEnabled);
}

TEST(passgl_resolver_da_ui_nao_depende_da_camara) {
    // O CONTRATO ESTÁ NO TIPO: resolveCanvasLayout/drawCanvas/hitTestCanvas
    // recebem (sw, sh, insets) — NÃO existe parâmetro de câmara. Duas cenas
    // com poses/fovs de câmara DIFERENTES resolvem os MESMOS rects (a UI de
    // jogo nunca é afetada pela câmara: coords de ecrã, safe-area, por cima
    // do 3D). Teste de guarda: se alguém acoplar o layout à câmara, falha.
    UiCanvas c;
    UiElement& btn = c.elements.emplace_back();
    btn.kind = UiElement::Kind::Button;
    btn.w = 220.0f;
    btn.h = 72.0f;

    Scene a;
    Tic* ca = a.get(a.create("CamA"));
    EXPECT(ca != nullptr);
    Transform3D* ta = ca->addComponent<Transform3D>();
    CameraComp* cca = ca->addComponent<CameraComp>();
    ta->pos = Vec3{0.0f, 1.0f, 4.0f};
    cca->fovY = 100.0f;
    Tic* ha = a.get(a.create("HUD"));
    ha->addComponent<UiCanvas>()->elements = c.elements;

    Scene b;
    Tic* cb = b.get(b.create("CamB"));
    EXPECT(cb != nullptr);
    Transform3D* tb = cb->addComponent<Transform3D>();
    CameraComp* ccb = cb->addComponent<CameraComp>();
    tb->pos = Vec3{50.0f, -20.0f, 400.0f};
    ccb->fovY = 20.0f;
    ccb->farZ = 2000.0f;
    Tic* hb = b.get(b.create("HUD"));
    hb->addComponent<UiCanvas>()->elements = c.elements;

    const safe::Insets ins{8.0f, 30.0f, 8.0f, 16.0f};
    std::vector<CanvasLayout> la(1), lb(1);
    const UiCanvas* cua = a.components().uiCanvases().find(ha->handle);
    const UiCanvas* cub = b.components().uiCanvases().find(hb->handle);
    EXPECT(cua != nullptr && cub != nullptr);
    resolveCanvasLayout(*cua, kSW, kSH, ins, la.data(), 1u);
    resolveCanvasLayout(*cub, kSW, kSH, ins, lb.data(), 1u);
    EXPECT(nearEqF(la[0].rect.x, lb[0].rect.x));
    EXPECT(nearEqF(la[0].rect.y, lb[0].rect.y));
    EXPECT(nearEqF(la[0].rect.w, lb[0].rect.w));
    EXPECT(nearEqF(la[0].rect.h, lb[0].rect.h));
}

// ---- 3. textScale: o Play nunca herda a escala do viewport 2D -----------------

TEST(passgl_textscale_reset_por_frame_o_play_fica_a_1) {
    FontAtlas font;
    EXPECT(font.loadFromPaths(&kFontPath, 1, 28.0f));
    UiContext ui;
    ui.init();
    ui.setFont(&font);
    Renderer r;
    EXPECT(r.init());

    // frame de EDITOR no modo UI: o viewport 2D escala o texto (ScopedTextScale)
    r.beginUiPass(1600, 720);
    ui.beginFrame(&r, nullptr, kSW, kSH);
    {
        const UiContext::ScopedTextScale scale(ui, 0.4f);   // mini-canvas
        EXPECT(nearEqF(ui.textScale(), 0.4f));
        EXPECT(ui.fontHeight() < 16.0f);   // 28 * 0.4 ≈ 11.2 px
    }
    ui.endFrame();
    r.endFrame();

    // frame de PLAY no MESMO UiContext (o que o main faz): o beginFrame
    // repõe textScale a 1.0 — o texto da UI de jogo fica na escala CORPO
    // (0.9.6.6 · GRUPO C: o atlas de 28px × textK — a 1.0 o corpo são 14px,
    // a densidade de sempre dos testes; o «28px CRUS» morreu com o sp():
    // medir pelo atlas cru DIVERGE do que desenha — a lição do choke point)
    r.beginUiPass(1600, 720);
    ui.beginFrame(&r, nullptr, kSW, kSH);
    EXPECT(nearEqF(ui.textScale(), 1.0f));
    EXPECT(nearEqF(ui.fontHeight(), 28.0f * vv::theme::textK()));
    ui.endFrame();
    r.endFrame();
}
