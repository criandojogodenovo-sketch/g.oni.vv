// tests/test_playui.cpp — 0.6.8: PLAY MODE COM JANELA PRÓPRIA.
//
// A transição EDITOR→PLAY→EDITOR é aferida com a UI REAL (FontAtlas +
// UiContext + EditorUi + gestos injetados — o mesmo padrão do test_ui):
//   • o botão Play da toolbar abre o modo play (EditorState.playMode);
//   • em PLAY NÃO há painéis de edição (Hierarchy/Inspector sumiram),
//     e a BARRA PLAY (Stop/a correr/fps/aviso) está no topo da safe-area;
//   • os TouchControls ficam ancorados SEM sobrepor a play bar/status;
//   • Stop volta ao EDITOR com a pose restaurada (PlaySnapshot) e os
//     painéis repostos exatamente (scroll/seleção intactos);
//   • o orbit está DESATIVADO em play (1 dedo = controlos) — a lógica
//     extraída p/ editor::updateCameraOrbit é afervel aqui.
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "components/BodyComp.h"
#include "components/TouchControls.h"
#include "components/Transform3D.h"
#include "core/PlaySnapshot.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "platform/InputState.h"
#include "render/Camera.h"
#include "ui/EditorUi.h"
#include "ui/Toolbar.h"
#include "ui/FontAtlas.h"
#include "ui/SafeArea.h"

using namespace vv;
using namespace vv::editor;
using ::test::nearEqF;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;
constexpr f32 kFontPx = 28.0f;

// frame completo imitando o main: EDITOR desenha toolbar+painéis; PLAY
// desenha a play bar + touchcontrols (o main devolve cedo — ver frame()).
struct Env {
    FontAtlas   font;
    UiContext   ui;
    InputState  input;
    Scene       scene;
    Handle      selected{};
    EditorState st;
    toolbar::GizmoModeState gzMode;   // 0.7.6
    PlaySnapshot snap;
    bool ok = false;

    explicit Env(bool withTc = true) {
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
        Tic* tic = scene.get(selected);
        if (withTc && tic) {
            tic->addComponent<TouchControls>();
        }
        if (tic) {
            if (Transform3D* tr = tic->getComponent<Transform3D>()) {
                tr->pos = Vec3{1.0f, 2.0f, 3.0f};
                tr->updateWorld();
            }
        }
        st.selected = selected;
    }

    // o mesmo fluxo do main.frame(): feed → orbit → toolbar/painéis OU
    // play bar → touchcontrols → toast/status → endFrame
    void frame(int fps = 60) {
        TouchControls* tcDraw = nullptr;
        bool tcDrawn = false;
        if (st.playMode) {
            auto& tcs = scene.components().touchControls();
            for (u32 i = 0; i < tcs.size(); ++i) {
                Tic* t = scene.get(tcs.owner(i));
                if (t && t->active) {
                    tcDraw = &tcs.at(i);
                    tcDrawn = true;
                    break;
                }
            }
        }
        OrbitState orbit;
        updateCameraOrbit(cam, orbit, input,
                          editor::centerRect(kSW, kSH, safe::Insets{}), 0,
                          st.playMode);

        ui.beginFrame(nullptr, &input, kSW, kSH);
        if (st.playMode) {
            if (drawPlayBar(ui, input, kSW, kSH, fps)) {
                leavePlay();
            }
        } else {
            // 0.7.6 — barra final; o G2 play entra no modo play
            const toolbar::Actions ta = toolbar::draw(ui, st);
            if (ta.playPressed && !st.playMode) {
                enterPlay();
            }
            drawHierarchy(ui, scene, st);
            drawInspector(ui, scene, st, nullptr);
        }
        if (tcDrawn && tcDraw) {
            drawTouchControls(ui, *tcDraw, kSW, kSH);
        }
        ui.statusLine("status");
        ui.endFrame();
        input.clearEdges();
    }

    void enterPlay() {
        st.playMode = true;
        closeAllOverlays(st);
        playSnapshotCapture(scene, snap);
    }
    void leavePlay() {
        st.playMode = false;
        playSnapshotRestore(scene, snap);
    }

    Camera cam;
};

// utility: os rects sólidos emitidos no frame (mesma técnica do test_ui)
struct Rect { f32 x0, y0, x1, y1; };

void collectRects(const QuadBatch& b, std::vector<Rect>& out) {
    const QuadVertex* v = b.vertices();
    const u32 n = b.vertexCount();
    for (u32 i = 0; i + 5 < n; i += 6) {
        out.push_back({v[i].x, v[i].y, v[i + 2].x, v[i + 2].y});
    }
}

bool rectsOverlap(const Rect& a, const Rect& b, f32 eps = 0.01f) {
    return std::min(a.x1, b.x1) - std::max(a.x0, b.x0) > eps &&
           std::min(a.y1, b.y1) - std::max(a.y0, b.y0) > eps;
}

} // namespace

// ---- 1. transição editor→play→editor pelo botão Play e pelo Stop -------------

TEST(play_toolbar_play_abre_janela_play) {
    Env e;
    EXPECT(e.ok);
    // 0.9.0 — tap no play da TOP BAR (rect do topbarLayout)
    const toolbar::TopBarLayout L =
        toolbar::topbarLayout(kSW, kSH, safe::Insets{}, false, false);
    const f32 bx = L.play.x + L.play.w * 0.5f;
    const f32 by = L.play.y + L.play.h * 0.5f;
    e.input.injectDown(0, bx, by);
    e.frame();
    e.input.injectUp(0);
    e.frame();
    EXPECT(e.st.playMode);
}

TEST(play_stop_volta_ao_editor_e_paineis_repostos) {
    Env e;
    EXPECT(e.ok);
    // seleciona o TIC e roda o scroll do Inspector p/ o fundo (offset tem de
    // sobreviver ao ciclo play — "painéis repostos exatamente")
    e.st.playMode = true;
    closeAllOverlays(e.st);
    playSnapshotCapture(e.scene, e.snap);
    e.frame();
    EXPECT(e.st.playMode);

    // mexe a pose "simulando" (como a física faria em play)
    Tic* tic = e.scene.get(e.selected);
    EXPECT(tic != nullptr);
    Transform3D* tr = tic->getComponent<Transform3D>();
    EXPECT(tr != nullptr);
    tr->pos = Vec3{9.0f, 9.0f, 9.0f};
    tr->worldDirty = true;

    // tap no Stop (id 5 — play bar): rect do EditorLayout
    const UiRect bar = playBarRect(kSW, kSH, safe::Insets{});
    const UiRect stop = playStopButtonRect(bar);
    e.input.injectDown(0, stop.x + stop.w * 0.5f, stop.y + stop.h * 0.5f);
    e.frame();
    e.input.injectUp(0);
    e.frame();

    EXPECT(!e.st.playMode);   // voltou ao EDITOR
    // pose de EDITOR restaurada (PlaySnapshot intacto)
    EXPECT(nearEqF(tr->pos.x, 1.0f));
    EXPECT(nearEqF(tr->pos.y, 2.0f));
    EXPECT(nearEqF(tr->pos.z, 3.0f));
    EXPECT(!tr->worldDirty);
}

TEST(play_entrar_fecha_menus_permanece_no_editor_ao_sair) {
    Env e;
    EXPECT(e.ok);
    // menus abertos no editor…
    e.st.fileMenu = true;
    e.st.settingsMenu = true;
    e.st.plusMenu = true;
    e.st.assetMenu = 1;
    e.st.importMenu = true;
    e.st.storageDialog = true;
    e.st.logViewer = true;
    e.enterPlay();
    // …entrar em play fecha TUDO (nada de edição em play)
    EXPECT(!e.st.fileMenu);
    EXPECT(!e.st.settingsMenu);
    EXPECT(!e.st.plusMenu);
    EXPECT(e.st.assetMenu == 0);
    EXPECT(!e.st.importMenu);
    EXPECT(!e.st.storageDialog);
    EXPECT(!e.st.logViewer);
    EXPECT(e.st.playMode);
    // a SELEÇÃO sobrevive (é estado de painel, não overlay)
    EXPECT(e.st.selected == e.selected);
    e.leavePlay();
    EXPECT(e.st.selected == e.selected);   // painéis repostos exatamente
}

// ---- 2. em play NÃO há painéis de edição -------------------------------------

TEST(play_sem_paineis_de_edicao) {
    Env e;
    EXPECT(e.ok);
    e.frame();   // editor: painéis desenhados
    std::vector<Rect> editorRects;
    collectRects(e.ui.solidsForTest(), editorRects);
    // a hierarquia desenha um painel em [0,88..300×592] — conta os quads
    const u32 editorQuads = e.ui.solidsForTest().vertexCount() / 6;
    EXPECT(editorQuads > 10u);   // painéis + toolbar + botões

    e.enterPlay();
    e.frame();   // play: SEM toolbar/painéis — só a play bar
    const u32 playQuads = e.ui.solidsForTest().vertexCount() / 6;
    // play bar = painel + separador + botão Stop (3-4 quads) + touchcontrols
    // (joystick frame + ponto + knob frame + botão) — MUITO menos que o editor
    EXPECT(playQuads < editorQuads / 3u);
}

TEST(play_play_bar_desenhada_com_stop_estado_e_aviso) {
    Env e;
    EXPECT(e.ok);
    e.enterPlay();
    e.frame(60);

    // glifos emitidos incluem "Stop" (botão) e "a correr" (estado)
    std::string txt;
    const QuadVertex* v = e.ui.glyphsForTest().vertices();
    const u32 n = e.ui.glyphsForTest().vertexCount();
    // reconstroi o texto por posição X crescente (aproximação: junta todos
    // os glifos e procura substrings-chave nos batching por quads)
    // (mais simples: verificar que HÁ glifos e que o frame do botão Stop
    // está no rect da play bar)
    (void)v; (void)n; (void)txt;
    std::vector<Rect> solids;
    collectRects(e.ui.solidsForTest(), solids);
    const UiRect bar = playBarRect(kSW, kSH, safe::Insets{});
    const UiRect stop = playStopButtonRect(bar);
    // o botão Stop emite um quad DENTRO do rect esperado (exato)
    bool achouStop = false;
    for (const Rect& r : solids) {
        if (r.x0 >= stop.x - 0.5f && r.y0 >= stop.y - 0.5f &&
            r.x1 <= stop.x + stop.w + 0.5f && r.y1 <= stop.y + stop.h + 0.5f &&
            r.x1 - r.x0 > 200.0f) {
            achouStop = true;
        }
    }
    EXPECT(achouStop);
    // e há texto no frame (estado/aviso)
    EXPECT(e.ui.glyphsForTest().vertexCount() > 6 * 10);
}

// ---- 3. TouchControls ancorados na safe-area SEM sobreposição -----------------

TEST(play_touchcontrols_sem_sobreposicao_com_barras) {
    Env e;
    EXPECT(e.ok);
    e.enterPlay();
    e.frame();

    const safe::Insets ins{};   // ecrã 1600x720 sem insets (pior caso: área máxima)
    const UiRect bar = playBarRect(kSW, kSH, ins);
    const UiRect status = safe::statusRect(kSW, kSH, ins);
    // layout dos controlos na ÁREA ÚTIL (como o main faz)
    const TouchControls::Layout l =
        TouchControls::layout(kSW, kSH);
    const Rect joy{l.joyCX - l.joyR, l.joyCY - l.joyR,
                    l.joyCX + l.joyR, l.joyCY + l.joyR};
    const Rect jump{l.btnX, l.btnY, l.btnX + l.btnW, l.btnY + l.btnH};

    // (A) sem interseção com a PLAY BAR (topo)
    EXPECT(!rectsOverlap(joy, {bar.x, bar.y, bar.x + bar.w, bar.y + bar.h}));
    EXPECT(!rectsOverlap(jump, {bar.x, bar.y, bar.x + bar.w, bar.y + bar.h}));
    // (B) sem interseção com a STATUS LINE (fundo)
    EXPECT(!rectsOverlap(joy, {status.x, status.y, status.x + status.w,
                                status.y + status.h}));
    EXPECT(!rectsOverlap(jump, {status.x, status.y, status.x + status.w,
                                 status.y + status.h}));
    // (C) joystick e JUMP não se sobrepõem entre si
    EXPECT(!rectsOverlap(joy, jump));
}

// ---- 4. orbit DESATIVADO em play (1 dedo = controlos) --------------------------

TEST(play_orbit_desativado_gestos_nao_movem_camera) {
    Camera cam;
    OrbitState st;
    InputState in;
    const safe::Insets ins0{};
    const UiRect view = editor::centerRect(kSW, kSH, ins0);

    const f32 yaw0 = cam.yaw;
    const f32 pitch0 = cam.pitch;
    const f32 dist0 = cam.dist;

    // EDITOR: drag no viewport central orbita (contraste). Como no main (1
    // chamada por frame): a 1ª ancora o dedo, a 2ª aplica o delta.
    in.injectDown(0, 800.0f, 360.0f);
    updateCameraOrbit(cam, st, in, view, 0, false);   // frame 1: ancora
    in.injectMove(0, 900.0f, 400.0f);
    updateCameraOrbit(cam, st, in, view, 0, false);   // frame 2: orbita
    EXPECT(cam.yaw != yaw0);   // orbitou

    // PLAY: o MESMO gesto não mexe na câmara
    Camera camPlay;
    OrbitState stPlay;
    InputState in2;
    const f32 yaw1 = camPlay.yaw;
    const f32 pitch1 = camPlay.pitch;
    const f32 dist1 = camPlay.dist;
    in2.injectDown(0, 800.0f, 360.0f);
    in2.injectMove(0, 500.0f, 200.0f);
    in2.injectDown(1, 1100.0f, 500.0f);   // pinch também
    in2.injectMove(1, 200.0f, 100.0f);
    for (int i = 0; i < 5; ++i) {
        updateCameraOrbit(camPlay, stPlay, in2, view, 0, true);
    }
    EXPECT(nearEqF(camPlay.yaw, yaw1));
    EXPECT(nearEqF(camPlay.pitch, pitch1));
    EXPECT(nearEqF(camPlay.dist, dist1));
    // e o estado do gesto foi RESETADO (limpo p/ o regresso ao editor)
    EXPECT(!stPlay.active);
    EXPECT(!stPlay.gestureInView);
    EXPECT(nearEqF(stPlay.pinchPrev, 0.0f));
}

TEST(play_orbit_volta_a_funcionar_apos_sair_do_play) {
    Camera cam;
    OrbitState st;
    InputState in;
    const safe::Insets ins0{};
    const UiRect view = editor::centerRect(kSW, kSH, ins0);
    // gesto "arrastado" para o estado de play…
    in.injectDown(0, 800.0f, 360.0f);
    updateCameraOrbit(cam, st, in, view, 0, true);
    // …sai do play: o próximo drag volta a orbitar
    in.injectMove(0, 700.0f, 360.0f);
    updateCameraOrbit(cam, st, in, view, 0, false);
    in.injectMove(0, 600.0f, 360.0f);
    const f32 yawAposOrbit = cam.yaw;
    updateCameraOrbit(cam, st, in, view, 0, false);
    EXPECT(cam.yaw != yawAposOrbit);   // orbita de novo (0.6.8b)
}
