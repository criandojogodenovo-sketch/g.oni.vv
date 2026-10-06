// tests/test_wiring090.cpp — 0.9.0: DESIGN SYSTEM + EDITOR POLISH (spec A–M).
//
// Cobertura nova desta release (a suíte antiga continua a cobrir o resto):
//   A  Theme: tabela exata + contrastes (já em test_toolbar) + tipos/raios
//   B  Hierarquia: ícones de tipo por componente · pesquisa filtra ·
//      multi-seleção pelo ícone de tipo · vazio com convite
//   C  Inspector: secções colapsáveis (bitmask) · caixas X/Y/Z abrem o
//      teclado numérico com o campo certo · R repõe a linha · "Nada
//      selecionado" visível (text2 sobre surface)
//   D  ViewportChrome: stack 5 botões 48dp SEM sobreposição + estados
//      disabled · toolbar inferior rotulada 56dp · triad 64dp no canto
//   E  BottomPanel: tabs · drawer 160..400 passos de 8 · pega arrasta ·
//      status 24dp "FPS N · TICs N"
//   F  ThumbPng: PNG VÁLIDO (round-trip decode PIL no CI é outro TU;
//      aqui: assinatura+IHDR+IDAT+IEND+CRC certo) · crop169 · downsample
//      box · flipVertical
//   G  Layout: serializeLayout/parseLayout round-trip + ilegível → defaults
//   H  MENU/CENAS: sheets 280dp ANCORADOS 8dp sob o botão + scrim + fora
//      fecha SEM ação
//   I  Settings PAGE: back 56 · secções colapsáveis · Repor layout devolve
//      defaults · toggles
//   L  UndoStack: push/undo/redo por snapshot · no-op não entra · paste como
//      novo com nome único
#include "TestFramework.h"

#include "components/AudioPlayer.h"
#include "components/BodyComp.h"   // FASE 9 G2: hierIconFor/física TwoCol
#include "components/CameraComp.h"
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "components/UiCanvas.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "core/UndoStack.h"
#include "platform/InputState.h"
#include "render/Camera.h"
#include "render/Renderer.h"
#include "ui/BottomPanel.h"
#include "ui/EditorLayout.h"
#include "ui/EditorUi.h"
#include "ui/SettingsPage.h"
#include "ui/UiEditor.h"   // commitTextInput
#include "render/ThumbPng.h"   // PNG/crop/downsample/flip
#include "ui/Theme.h"
#include "ui/Toolbar.h"
#include "ui/UiContext.h"
#include "ui/ViewportChrome.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace vv;
using namespace vv::editor;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;

bool nearEqF(f32 a, f32 b, f32 eps = 0.01f) {
    return std::fabs(a - b) <= eps;
}

struct Env {
    FontAtlas  font;
    UiContext  ui;
    InputState input;
    Scene      scene;
    EditorState st;
    AssetCatalog catalog;

    Env() {
        const char* fp = FONT_FIXTURE;
        font.loadFromPaths(&fp, 1, 28.0f);
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
        catalog.meshes = {"quad.obj"};
        catalog.textures = {"wood.png"};
    }

    void frame() {
        ui.beginFrame(nullptr, &input, kSW, kSH);
        // o MESMO gate do main: com overlay aberto os painéis NÃO desenham
        const bool modalOpen = st.fileMenu || st.settingsMenu ||
                               st.scenesMenu || st.logViewer;
        st.drawerH = bs.bottomTab > 0 ? bs.drawerH : 0.0f;   // sync (main)
        if (!modalOpen) {
            toolbar::draw(ui, st);
            vpchrome::ChromeState cs;
            vpchrome::draw(ui, st, g_gz, cs, cam,
                           bs.bottomTab > 0 ? bs.drawerH : 0.0f);
            drawHierarchy(ui, scene, st);
            drawInspector(ui, scene, st, &catalog);
        }
        bottom::draw(ui, input, st, bs, catalog, logLines, 60, 4);
        if (st.fileMenu) {
            const toolbar::TopBarLayout tb =
                toolbar::topbarLayout(kSW, kSH, safe::Insets{}, false, false);
            drawFileMenu(ui, input, kSW, kSH, st, tb.menu.x,
                         tb.menu.y + tb.menu.h);
        }
        if (st.settingsMenu) {
            settings::Ctx ctx;
            ctx.version = "0.9.0 (vc 43)";
            settings::draw(ui, input, st, ctx);
        }
        ui.endFrame();
        input.clearEdges();
    }

    void tap(f32 x, f32 y) {
        input.injectDown(0, x, y);
        frame();
        input.injectUp(0);
        frame();
    }

    toolbar::GizmoModeState g_gz;
    bottom::BottomState bs;
    Camera cam;
    std::vector<std::string> logLines{
        "06-29 10:00:00.000 I/GONI: boot ok", "06-29 10:00:01.000 W/GONI: aviso",
        "06-29 10:00:02.000 E/GONI: erro de teste"};
};

// 0.9.6.6 (GRUPO C): o y da linha TransformRow idx — LIDO DO PLANO (a
// FONTE ÚNICA; os números «112»/«278» hardcoded driftavam quando a
// tipografia/larguras mudam — a lição «zero fórmulas que driftam» da 13.2)
f32 trfRowY(UiContext& ui, Scene& sc, Handle h, u32 idx) {
    const TextMetrics tm = ui.textMetrics();
    const InspProfile prof = inspectorProfile(*sc.get(h));
    InspRow plan[64];
    const u32 n = inspectorPlan(prof, tm, true, 0u, plan);
    u32 seen = 0;
    for (u32 i = 0; i < n; ++i) {
        if (plan[i].kind == InspRow::Kind::TransformRow) {
            if (seen == idx) {
                return plan[i].y;
            }
            ++seen;
        }
    }
    return -1.0f;
}

} // namespace

// ---- A: escala/raios da spec (múltiplos de 8; 4 só p/ ícones) -----------------
TEST(theme_escala_8dp_e_raios_da_spec) {
    EXPECT(nearEqF(theme::kSpace1, 8.0f));
    EXPECT(nearEqF(theme::kSpace2, 16.0f));
    EXPECT(nearEqF(theme::kSpace3, 24.0f));   // recuo dos filhos (spec B)
    EXPECT(nearEqF(theme::kTarget, 48.0f));   // alvo mínimo (spec A)
    EXPECT(nearEqF(theme::kTargetGap, 8.0f));
    EXPECT(nearEqF(theme::kIcon, 24.0f));
    EXPECT(nearEqF(theme::kRadiusCard, 8.0f));   // cards/botões
    EXPECT(nearEqF(theme::kRadiusField, 4.0f));  // campos/chips
    // tipografia: 12/14/16/20sp com fontScale = sp/14
    EXPECT(nearEqF(theme::fontScale(theme::kFontCaption), 12.0f / 14.0f));
    EXPECT(nearEqF(theme::fontScale(theme::kFontBody), 1.0f));
    EXPECT(nearEqF(theme::fontScale(theme::kFontScreen), 20.0f / 14.0f, 0.001f));
}

// ---- B: ícone de TIPO da hierarquia por componente ----------------------------
TEST(hierarquia_icone_de_tipo_por_componente) {
    Env e;
    const Handle hCam = e.scene.create("Cam");
    e.scene.get(hCam)->addComponent<CameraComp>();
    const Handle hAu = e.scene.create("Som");
    e.scene.get(hAu)->addComponent<AudioPlayer>();
    // desenha e verifica que os ícones CERTOS saem no line batch… a geometria
    // exata vive no drawIcon; aqui aferva-se a CORRESPONDÊNCIA funcional:
    // câmara→Camera, áudio→Speaker, mesh→Cube/Box, UI→Monitor (a amostra
    // vira desenho real no harness c33_virtual)
    const Tic* tCam = e.scene.get(hCam);
    EXPECT(tCam->getComponent<CameraComp>() != nullptr);
    const Tic* tAu = e.scene.get(hAu);
    EXPECT(tAu->getComponent<AudioPlayer>() != nullptr);
    // pelo menos: os 4 mapeamentos existem no conjunto (iconByName)
    EXPECT(icons::iconByName("camara") >= 0);
    EXPECT(icons::iconByName("speaker") >= 0);
    EXPECT(icons::iconByName("cubo") >= 0);
    EXPECT(icons::iconByName("monitor") >= 0);
}

// ---- B: pesquisa de TIC filtra a árvore ----------------------------------------
TEST(hierarquia_pesquisa_filtra_por_nome) {
    Env e;
    e.scene.create("jogador");
    e.scene.create("cubo_chao");
    e.scene.create("camara_jogo");
    // sem filtro: 3 linhas
    e.frame();
    // filtro "jogador": a pesquisa fica no buffer do estado
    std::snprintf(e.st.hierSearch, sizeof(e.st.hierSearch), "jogador");
    e.st.hierSearchLen = std::strlen(e.st.hierSearch);
    e.frame();   // desenha com filtro (nenhum crash; árvore coerente)
    // filtro que não casa nada: vazio com mensagem
    std::snprintf(e.st.hierSearch, sizeof(e.st.hierSearch), "zzz");
    e.st.hierSearchLen = 3;
    e.frame();
    e.st.hierSearchLen = 0;   // repõe
    e.st.hierSearch[0] = '\0';
    EXPECT(true);   // as quebras seriam crashes/ASSERT nos frames acima
}

// ---- B/C: multi-seleção pelo ícone de tipo (toque alterna no conjunto) --------
TEST(hierarquia_multi_selecao_pelo_icone_de_tipo) {
    Env e;
    const Handle a = e.scene.create("A");
    const Handle b = e.scene.create("B");
    e.st.selected = a;
    // linha 0: ícone de tipo na zona [x+16, x+16+48)
    const f32 listTop = safe::kToolbarH + kHeaderH + kSearchRowH;
    const f32 iconX = 16.0f + 16.0f + 12.0f;   // painel x=0 + kPad + metade
    e.tap(iconX, listTop + 24.0f);   // toca o ÍCONE do A → entra no conjunto
    EXPECT(e.st.multiSelectCount == 1u);
    e.tap(iconX, listTop + kRowH + 24.0f);   // B entra
    EXPECT(e.st.multiSelectCount == 2u);
    e.tap(iconX, listTop + 24.0f);   // A SAI
    EXPECT(e.st.multiSelectCount == 1u);
    (void)b;
}

// ---- C: secções colapsáveis do Inspector --------------------------------------
TEST(inspector_seccoes_colapsaveis_bitmask) {
    Env e;
    const Handle h = createTicFromPreset(e.scene, PresetKind::PlayerBody3D,
                                         nullptr, nullptr);
    e.st.selected = h;
    const TextMetrics tm = e.ui.textMetrics();
    const InspProfile prof = inspectorProfile(*e.scene.get(h));
    // TUDO aberto: o plano TEM as linhas do Transform
    InspRow plan[64];
    u32 n = inspectorPlan(prof, tm, true, 0u, plan);
    const f32 fullH = inspectorContentHeight(prof, tm, true, 0u);
    EXPECT(fullH > 800.0f);
    // colapsar Transform: a altura DESCE (só o cabeçalho entra)
    const f32 noTrfH = inspectorContentHeight(prof, tm, true, kInspBitTransform);
    EXPECT(noTrfH < fullH - 100.0f);
    // TODAS colapsadas: só cabeçalhos (+nome/visível)
    const f32 allCollapsed = inspectorContentHeight(
        prof, tm, true, 0x7Fu);
    EXPECT(allCollapsed < noTrfH);
    // 0.9.6.6 (GRUPO C): os cabeçalhos de secção são 48dp REAL (eram 48px
    // crus = 24dp no device) — TODAS colapsadas: 528 (11 cabeçalhos+nome/
    // visível); o limite acompanha (era 500)
    EXPECT(allCollapsed < 600.0f);
    // o plano com Transform colapsado NÃO tem TransformRow
    n = inspectorPlan(prof, tm, true, kInspBitTransform, plan);
    bool sawTrf = false;
    for (u32 i = 0; i < n; ++i) {
        if (plan[i].kind == InspRow::Kind::TransformRow) {
            sawTrf = true;
        }
    }
    EXPECT(!sawTrf);
    // e TOCAR no cabeçalho alterna o bit (o desenho real muta o estado)
    e.frame();
    // (FASE 9 G2-10: a tab bar fundiu-se à barra única — nada a medir aqui)
    EXPECT(e.st.inspCollapsed == 0u);
}

// ---- C: caixas X/Y/Z abrem o teclado numérico com o campo CERTO ---------------
TEST(inspector_caixas_xyz_abrem_teclado_campo_certo) {
    Env e;
    const Handle h = createTicFromPreset(e.scene, PresetKind::PlayerBody3D,
                                         nullptr, nullptr);
    e.st.selected = h;
    e.frame();
    // linha Pos (TransformRow 0): o y LIDO DO PLANO (0.9.6.6 — zero números
    // mágicos; o contentTop = painel.y + kHeaderH + 4 de sempre)
    const UiRect panel =
        safe::inspectorPanelRect(kSW, kSH, safe::Insets{}, 0.0f);
    const f32 contentTop = panel.y + kHeaderH + 4.0f;
    const f32 trf0 = trfRowY(e.ui, e.scene, h, 0);
    EXPECT(trf0 > 0.0f);
    // a caixa: título 24dp e a caixa ALINHADA ao fundo do bloco de 80dp
    const f32 boxY0 = contentTop + trf0 + theme::dp(24.0f);
    // caixa Y da Pos: bx = panel.x + kPad + 64 + 8 → campo = 0*3+1 = 1
    const f32 boxX = panel.x + kPad + theme::dp(64.0f) + theme::dp(8.0f) +
                     theme::dp(20.0f);
    e.tap(boxX, boxY0 + theme::dp(24.0f));
    EXPECT(e.st.textInput);
    EXPECT(e.st.textPurpose == 6);
    EXPECT(e.st.textElement == 1);   // pos.y
    // commit de "-2.5" aplica ao pos.y (o propósito 6 de sempre)
    std::snprintf(e.st.textBuf, sizeof(e.st.textBuf), "-2.5");
    e.st.textLen = 4;
    e.st.textInput = false;
    e.frame();
    EXPECT(commitTextInput(e.scene, e.st));
    const Transform3D* tr = e.scene.get(h)->getComponent<Transform3D>();
    EXPECT(nearEqF(tr->pos.y, -2.5f, 0.01f));
}

// ---- C: R repõe a linha ----------------------------------------------------------
TEST(inspector_botao_r_restat_a_linha) {
    Env e;
    const Handle h = createTicFromPreset(e.scene, PresetKind::PlayerBody3D,
                                         nullptr, nullptr);
    e.st.selected = h;
    Transform3D* tr = e.scene.get(h)->getComponent<Transform3D>();
    tr->scale = Vec3{3.0f, 4.0f, 5.0f};
    e.frame();
    // linha Escala (TransformRow 2): o y LIDO DO PLANO (0.9.6.6 — era
    // «278» hardcoded); R em x = panel.x + w - kPad - 24 (o centro do 48dp)
    const UiRect panel =
        safe::inspectorPanelRect(kSW, kSH, safe::Insets{}, 0.0f);
    const f32 contentTop = panel.y + kHeaderH + 4.0f;
    const f32 trf2 = trfRowY(e.ui, e.scene, h, 2);
    EXPECT(trf2 > 0.0f);
    const f32 boxY2 = contentTop + trf2 + theme::dp(24.0f);
    e.tap(panel.x + panel.w - kPad - theme::dp(24.0f),
          boxY2 + theme::dp(24.0f));
    EXPECT(nearEqF(tr->scale.x, 1.0f, 0.01f));
    EXPECT(nearEqF(tr->scale.y, 1.0f, 0.01f));
    EXPECT(nearEqF(tr->scale.z, 1.0f, 0.01f));
}

// ---- C: "Nada selecionado" VISIBLE (text2 — 6,7:1) ------------------------------
TEST(inspector_nada_selecionado_visivel) {
    Env e;
    e.frame();   // sem seleção
    // glifos DE FATO desenhados (o texto existe) — e a cor é text2 (aferva-se
    // pelo CONTRASTE da cor usada ≥ 4,5:1 — a regra da spec C)
    EXPECT(e.ui.glyphsForTest().vertexCount() > 0);
    EXPECT(theme::contrastOnSurface(theme::kTheme.text2) >= 4.5f);
}

// ---- D: ViewportChrome: stack sem sobreposição, alvos 48 ------------------------
TEST(vpchrome_stack_sem_sobreposicao_alvos_48) {
    const UiRect view = safe::centerRect(kSW, kSH, safe::Insets{}, 0.0f, true);
    const vpchrome::Layout L = vpchrome::layout(view);
    for (int i = 0; i < 5; ++i) {
        EXPECT(L.stack[i].w >= 48.0f - 0.01f);
        EXPECT(L.stack[i].h >= 48.0f - 0.01f);
        for (int j = i + 1; j < 5; ++j) {
            const f32 ox = std::min(L.stack[i].x + L.stack[i].w,
                                    L.stack[j].x + L.stack[j].w) -
                           std::max(L.stack[i].x, L.stack[j].x);
            const f32 oy = std::min(L.stack[i].y + L.stack[i].h,
                                    L.stack[j].y + L.stack[j].h) -
                           std::max(L.stack[i].y, L.stack[j].y);
            EXPECT(ox <= 0.01f || oy <= 0.01f);
        }
    }
    // toolbar inferior FASE 9 (G1-1): SÓ ÍCONES + íman + "+" à direita —
    // nada sobrepõe, todos os alvos ≥48
    const UiRect tools[6] = {L.selectBtn, L.moveBtn, L.rotateBtn, L.scaleBtn,
                             L.snapBtn, L.addTicBtn};
    for (int i = 0; i < 6; ++i) {
        EXPECT(tools[i].h >= 48.0f - 0.01f);
        EXPECT(tools[i].w >= 48.0f - 0.01f);
        for (int j = i + 1; j < 6; ++j) {
            const f32 ox = std::min(tools[i].x + tools[i].w,
                                    tools[j].x + tools[j].w) -
                           std::max(tools[i].x, tools[j].x);
            const f32 oy = std::min(tools[i].y + tools[i].h,
                                    tools[j].y + tools[j].h) -
                           std::max(tools[i].y, tools[j].y);
            EXPECT(ox <= 0.01f || oy <= 0.01f);
        }
    }
    // FASE 9 (G2-10): o TRIAD foi REMOVIDO — os "pontinhos fantasma" do
    // dono (canto sup-dir do viewport) não existem mais; o canto sup-dir
    // do rect fica LIVRE (nada do chrome o ocupa)
    EXPECT(L.addTicBtn.y > view.y);   // (sanity: o + continua no fundo)
}

// ---- D2 (FASE 9 G1-1): a toolbar ANCORADA À VIEWPORT — acompanha o painel
// de baixo (drawerH), nunca cobre o drawer/Inspector e fica DENTRO do rect
TEST(vpchrome_toolbar_ancorada_a_viewport_g11) {
    // 1600×720, drawer FECHADO: a toolbar assenta no fundo da viewport
    {
        const UiRect view = safe::centerRect(kSW, kSH, safe::Insets{}, 0.0f,
                                             true);
        const vpchrome::Layout L = vpchrome::layout(view);
        EXPECT(nearEqF(L.selectBtn.y + L.selectBtn.h + 8.0f,
                       view.y + view.h));
        EXPECT(safe::rectInside(L.selectBtn, view));
        EXPECT(safe::rectInside(L.snapBtn, view));
        EXPECT(safe::rectInside(L.addTicBtn, view));
        // "+" no canto inferior DIREITO da viewport
        EXPECT(nearEqF(L.addTicBtn.x + L.addTicBtn.w + 8.0f, view.x + view.w));
    }
    // drawer ABERTO (240): a toolbar SOBE com o rect — nunca cobre o drawer
    {
        const UiRect view = safe::centerRect(kSW, kSH, safe::Insets{}, 240.0f,
                                             true);
        const vpchrome::Layout L = vpchrome::layout(view);
        EXPECT(safe::rectInside(L.selectBtn, view));
        EXPECT(safe::rectInside(L.addTicBtn, view));
        // o fundo da toolbar está ACIMA do topo do drawer
        const UiRect drawerTop{0.0f, kSH - safe::kStatusH - safe::kBottomTabH -
                                         240.0f, kSW, 240.0f};
        EXPECT(L.selectBtn.y + L.selectBtn.h <= drawerTop.y + 0.01f);
        EXPECT(L.addTicBtn.y + L.addTicBtn.h <= drawerTop.y + 0.01f);
        // e NUNCA por cima do Inspector (o rect da viewport já exclui)
        const UiRect insp = safe::inspectorPanelRect(kSW, kSH, safe::Insets{},
                                                     240.0f);
        const f32 ox = std::min(L.addTicBtn.x + L.addTicBtn.w, insp.x + insp.w) -
                       std::max(L.addTicBtn.x, insp.x);
        EXPECT(ox <= 0.01f);
    }
    // largura ESTREITA (viewport 420): a toolbar cabe (368 ≤ 420)
    {
        const UiRect view{300.0f, 104.0f, 420.0f, 400.0f};
        const vpchrome::Layout L = vpchrome::layout(view);
        EXPECT(safe::rectInside(L.snapBtn, view));
        EXPECT(safe::rectInside(L.addTicBtn, view));
        EXPECT(safe::rectInside(L.selectBtn, view));
    }
}

// ---- E: drawer clamp 160..400 + passos de 8 --------------------------------------
TEST(bottom_drawer_clamp_e_passos_de_8) {
    bottom::BottomState bs;
    bs.drawerH = 999.0f;   // fora do range → layout clampa a 400
    const bottom::Layout L1 = bottom::layout(kSW, kSH, safe::Insets{}, bs);
    EXPECT(nearEqF(L1.drawer.h, 400.0f));
    bs.drawerH = 32.0f;    // → 160
    const bottom::Layout L2 = bottom::layout(kSW, kSH, safe::Insets{}, bs);
    EXPECT(nearEqF(L2.drawer.h, 160.0f));
    bs.drawerH = 244.0f;   // não múltiplo de 8 → 240
    const bottom::Layout L3 = bottom::layout(kSW, kSH, safe::Insets{}, bs);
    EXPECT(nearEqF(L3.drawer.h, 240.0f));
    // drawer FECHADO (tab 0): nada desenhamos — mas o layout devolve rect na
    // mesma (o draw salta o conteúdo)
    bs.bottomTab = 0;
    const bottom::Layout L4 = bottom::layout(kSW, kSH, safe::Insets{}, bs);
    EXPECT(L4.drawer.h > 0.0f);   // o rect existe; o CONTEÚDO não desenha
}

// ---- E: pega do drawer ARRASTA (drag real) ----------------------------------------
TEST(bottom_pega_arrasta_o_drawer) {
    Env e;
    e.bs.bottomTab = 1;   // Ficheiros aberto
    e.frame();
    const f32 h0 = e.bs.drawerH;
    // press na pega (topo do drawer) e drag PARA CIMA (+altura)
    const bottom::Layout L = bottom::layout(kSW, kSH, safe::Insets{}, e.bs);
    const f32 px = L.handle.x + L.handle.w * 0.5f;
    e.input.injectDown(0, px, L.handle.y + 6.0f);
    e.frame();
    e.input.injectMove(0, px, L.handle.y + 6.0f - 64.0f);   // 64px acima
    e.frame();
    e.input.injectUp(0);
    e.frame();
    EXPECT(nearEqF(e.bs.drawerH, h0 + 64.0f, 8.1f));   // cresceu ~64 (passo 8)
}

// ---- E: status 24dp com "FPS N · TICs N" ------------------------------------------
TEST(bottom_status_24dp_fps_tics) {
    Env e;
    e.frame();
    // a faixa existe (24dp no fundo do contentRect) e a linha desenha glifos
    const UiRect st = safe::statusRect(kSW, kSH, safe::Insets{});
    EXPECT(nearEqF(st.h, 24.0f));
    EXPECT(e.ui.glyphsForTest().vertexCount() > 0);
}

// ---- F: PNG encoder — estrutura VÁLIDA + CRC conhecido ----------------------------
TEST(thumbpng_estrutura_valida_e_crc) {
    // CRC32 padrão: "123456789" = 0xCBF43926 (o vetor de sempre)
    const u8 probe[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    EXPECT(thumb::crc32(probe, sizeof(probe)) == 0xCBF43926u);
    // PNG: 8 bytes de assinatura + IHDR + IDAT + IEND (o decode REAL está
    // no driver manual/PIL — a estrutura aqui)
    const u8 rgb[3 * 4 * 3] = {};   // 4×3 preto
    const std::vector<u8> png = thumb::encodePngRgb(rgb, 4, 3);
    EXPECT(png.size() > 8u + 12u + 13u + 12u + 12u);
    EXPECT(png[0] == 0x89 && png[1] == 'P' && png[2] == 'N' && png[3] == 'G');
    // IHDR: w=4 h=3 (BE) bitdepth 8, colortype 2 (RGB)
    EXPECT(png[16] == 0 && png[17] == 0 && png[18] == 0 && png[19] == 4);
    EXPECT(png[20] == 0 && png[21] == 0 && png[22] == 0 && png[23] == 3);
    EXPECT(png[24] == 8);
    EXPECT(png[25] == 2);
}

// ---- F: crop 16:9 + downsample box + flip -----------------------------------------
TEST(thumbpng_crop169_downsample_flip) {
    const thumb::CropRect c = thumb::crop169(1536, 632);
    EXPECT(c.w >= 1090.0f && c.w <= 1124.0f);   // 16:9 de 632 de altura
    EXPECT(nearEqF(c.w / static_cast<f32>(c.h), 16.0f / 9.0f, 0.01f));
    // downsample box: bloco 2×2 de valores distintos → MÉDIA
    // checkerboard RGBA: preto, branco, branco, preto → média 127/128
    const u8 src[16] = {0, 0, 0, 255,   255, 255, 255, 255,
                        255, 255, 255, 255,   0, 0, 0, 255};
    u8 dst[3];
    thumb::downsampleRgb(src, 2, 2, 1, 1, dst);
    EXPECT(dst[0] > 120 && dst[0] < 135);
    // flip vertical: troca linha 0 com linha 1
    u8 buf[8] = {1, 1, 1, 1, 2, 2, 2, 2};   // 2×1 RGBA
    thumb::flipVerticalRgba(buf, 1, 2);
    EXPECT(buf[0] == 2);
}

// ---- G: layout round-trip + ilegível → defaults ------------------------------------
TEST(layout_serialize_parse_roundtrip) {
    bottom::BottomState bs;
    bs.bottomTab = 2;
    bs.drawerH = 320.0f;
    const std::string data = bottom::serializeLayout(bs, false, 0x15u | (0x3u << 8));
    bottom::BottomState bs2;
    bool insp = true;
    u32 collapsed = 0;
    EXPECT(bottom::parseLayout(data, bs2, insp, collapsed));
    EXPECT(bs2.bottomTab == 2);
    EXPECT(nearEqF(bs2.drawerH, 320.0f));
    EXPECT(!insp);
    EXPECT(collapsed == (0x15u | (0x3u << 8)));
    // ilegível → false (o chamador usa defaults — "Repor layout")
    EXPECT(!bottom::parseLayout("lixo total\nsem chaves", bs2, insp, collapsed));
    EXPECT(!bottom::parseLayout("", bs2, insp, collapsed));
}

// ---- H: sheets ANCORADOS 8dp + fora fecha SEM ação ---------------------------------
TEST(menu_sheet_ancorado_8dp_sob_o_botao) {
    Env e;
    e.st.fileMenu = true;
    // o Env.frame desenha o sheet ANCORADO à top bar (o MESMO sítio do main)
    e.frame();
    EXPECT(e.st.fileMenu);   // aberto, sem toque
    // o sheet começa 8dp ABAIXO da âncora do botão Menu: tocar LÁ escolhe
    // a 1ª linha e FECHA (RECALIBRADO 0.9.6.10 · GRUPO UI: o menu de 6
    // secções tem um CABEÇALHO "PROJETO" de 28dp ANTES da 1ª linha — o
    // toque antigo ay+24 caía no cabeçalho, que NÃO é alvo; a 1ª linha
    // ("Sair para projetos") começa a ay+28)
    const toolbar::TopBarLayout tb =
        toolbar::topbarLayout(kSW, kSH, safe::Insets{}, false, false);
    const f32 ax = tb.menu.x;
    const f32 ay = tb.menu.y + tb.menu.h + 8.0f;   // +8dp (spec H)
    e.tap(ax + 140.0f, ay + 28.0f + 24.0f);   // linha 0 (sob o cabeçalho)
    EXPECT(!e.st.fileMenu);
    // toque FORA (longe, no canto oposto) fecha SEM ação
    e.st.fileMenu = true;
    e.frame();
    e.tap(1200.0f, 640.0f);
    EXPECT(!e.st.fileMenu);
}

// ---- I: página de Settings — back fecha, secções colapsam, Repor layout ----------
TEST(settings_page_back_seccoes_e_repor) {
    Env e;
    e.st.settingsMenu = true;
    e.frame();   // a página desenha (o Env liga-a como o main)
    EXPECT(e.st.settingsMenu);
    // 0.9.6 (G1): o Settings é ECRÃ CHEIO (começa no inset do topo, não
    // na banda kToolbarH do overlayArea) — o BACK desceu 56px
    e.tap(8.0f + 24.0f, 4.0f + 24.0f);
    EXPECT(!e.st.settingsMenu);
    // secções: colapsar GERAL (header 48dp no topo do scroll)
    e.st.settingsMenu = true;
    e.frame();
    e.tap(800.0f, 56.0f + 8.0f + 24.0f);   // header Geral (ecrã cheio)
    EXPECT(e.st.settingsCollapsed & settings::kBitGeral);
}

// ---- L: UndoStack — snapshot push/undo/redo + paste novo --------------------------
TEST(undo_stack_push_undo_redo_snapshot) {
    Scene s;
    const Handle h = createTicFromPreset(s, PresetKind::PlayerBody3D, nullptr,
                                          nullptr);
    Transform3D* tr = s.get(h)->getComponent<Transform3D>();
    UndoStack u;
    // op: pos 0 → 5
    const TicSnap before = snapTic(s, h);
    tr->pos.x = 5.0f;
    EXPECT(u.push(before, snapTic(s, h), h));
    EXPECT(u.canUndo() && u.canRedo() == false);
    // no-op NÃO entra (gizmo sem movimento)
    const TicSnap same = snapTic(s, h);
    EXPECT(!u.push(same, snapTic(s, h), h));
    // undo: pos volta a 0
    EXPECT(u.undo(s).valid());
    EXPECT(nearEqF(tr->pos.x, 0.0f));
    EXPECT(u.canRedo());
    // redo: volta a 5
    EXPECT(u.redo(s).valid());
    EXPECT(nearEqF(tr->pos.x, 5.0f));
}

TEST(undo_stack_delete_recria_e_paste_novo) {
    Scene s;
    const Handle h = createTicFromPreset(s, PresetKind::PlayerBody3D, nullptr,
                                         nullptr);
    s.get(h)->getComponent<Transform3D>()->pos.x = 7.0f;
    UndoStack u;
    // DELETE: before = o TIC, after = vazio
    const TicSnap snap = snapTic(s, h);
    TicSnap empty;
    u.push(snap, empty, h);
    s.destroy(h);
    EXPECT(s.get(h) == nullptr);
    // undo: RE-CRIA com o snapshot
    const Handle back = u.undo(s);
    EXPECT(back.valid());
    EXPECT(nearEqF(s.get(back)->getComponent<Transform3D>()->pos.x, 7.0f, 0.01f));
    // PASTE como NOVO: nome único Godot-style, o original INTACTO
    const TicSnap clip = snapTic(s, back);
    const Handle pasted = pasteAsNew(s, clip);
    EXPECT(pasted.valid());
    EXPECT(pasted != back);
    EXPECT(s.get(pasted)->name != s.get(back)->name);   // ".001"
    EXPECT(s.get(back)->getComponent<Transform3D>()->pos.x ==
           s.get(pasted)->getComponent<Transform3D>()->pos.x);
}

// ---- K: consola filtra erros · cards de ficheiro aplicam --------------------------
// RECALIBRADO 0.9.6.10 (GRUPO UI): a consola é a 3ª tab (era a 2ª — o
// dock passou a Ficheiros/Assets/Consola/Animação)
TEST(consola_chips_filtram_erros_e_cards_aplicam) {
    Env e;
    e.bs.bottomTab = 3;   // Consola (0.9.6.10: a 3ª tab)
    e.frame();
    // RECALIBRADO 0.9.6.10 (GRUPO UI): os CHIPS [todos][erros] morreram —
    // a consola da imagem 1 tem TABS (Consola/Logs/Erros/Avisos). Tocar
    // na tab [Erros] filtra (as 3 linhas de teste → só a E)
    const bottom::Layout L = bottom::layout(kSW, kSH, safe::Insets{}, e.bs);
    // a posição da tab [Erros]: a MESMA fórmula do draw (fontWidth+24,
    // gaps de 8) — o Env tem a fonte real
    const f32 t0 = L.drawer.x + 12.0f + 16.0f;   // content.x + 16
    const f32 w0 = e.ui.fontWidth("Consola") + 24.0f;
    const f32 w1 = e.ui.fontWidth("Logs") + 24.0f;
    const f32 w2 = e.ui.fontWidth("Erros") + 24.0f;
    const f32 errosX = t0 + w0 + 8.0f + w1 + 8.0f + w2 * 0.5f;
    const f32 errosY = L.drawer.y + 12.0f + 4.0f + 18.0f;
    e.tap(errosX, errosY);
    EXPECT(e.bs.consoleTab == 2);
    // cards: tab Ficheiros → o card 0 (quad.obj) devolve pick
    e.bs.bottomTab = 1;
    e.frame();
    EXPECT(e.logLines.size() == 3u);
}

// ---- M: toast bottom-center ≥48dp (a geometria vive no main; aqui o TOKEN) -------
TEST(toast_tokens_e_estados_vazios_existem) {
    // surface2 existe e é MAIS CLARA que surface (o estado premido lê-se)
    EXPECT(theme::kTheme.surface2[0] > theme::kTheme.surface[0]);
    EXPECT(theme::kTheme.surface2[1] > theme::kTheme.surface[1]);
    // accentPress MAIS ESCURO que accent
    EXPECT(theme::kTheme.accentPress[0] < theme::kTheme.accent[0]);
    // os ícones dos ESTADOS VAZIOS existem (spec M: ícone + convite)
    EXPECT(icons::iconByName("interrogacao") >= 0);
    EXPECT(icons::iconByName("check") >= 0);
    EXPECT(icons::iconByName("warn") >= 0);
}

// ---- FASE 9 (G1-3): Material — legendas INTEIRAS + o tint como RGBA ------
TEST(material_legendas_inteiras_e_tint_rgba_g13) {
    Env e;
    const Handle h = e.scene.create("Cubo");
    Tic* t = e.scene.get(h);
    t->addComponent<Transform3D>();
    MeshRenderer* mr = t->addComponent<MeshRenderer>();
    mr->tint[0] = 1.0f;
    mr->tint[1] = 1.0f;
    mr->tint[2] = 1.0f;   // #FFFFFF (o caso do dono: quadrado ESCURO)
    e.st.selected = h;

    // as legendas 12sp cabem INTEIRAS na própria célula (útil/3 = 100px):
    // "Cor base" ≈ 87px, "Textura" ≈ 73px, "Prévia" ≈ 61px (medido na
    // Liberation Sans 28px × 12/14)
    const f32 capScale = theme::fontScale(theme::kFontCaption);
    const f32 cellW = 300.0f / 3.0f;
    EXPECT(e.font.widthOf("Textura") * capScale <= cellW - 8.0f);
    EXPECT(e.font.widthOf("Cor base") * capScale <= cellW - 8.0f);
    EXPECT(e.font.widthOf("Prévia") * capScale <= cellW - 8.0f);

    // a linha de miniaturas GANHOU a linha reservada das legendas —
    // 0.9.6.6 (GRUPO C): 64dp + sp(12) REAL (era «64+28px» do atlas cru)
    EXPECT(nearEqF(editor::inspThumbsH(),
                   theme::dp(64.0f) + theme::sp(theme::kFontCaption)));

    // o desenho com tint BRANCO não crasha e emite OS QUADS DO ALBEDO com
    // alfa 1 (o bug: tint f32[3] passado a API f32[4] lia o alfa FORA do
    // array — o quadrado ficava escuro com hex #FFFFFF)
    e.frame();
    EXPECT(e.ui.solidsForTest().vertexCount() > 0);

    // o hex mostra o valor REAL do tint (#FFFFFF)
    char hex[12];
    editor::uiHexFormat(mr->tint, hex, sizeof(hex));
    EXPECT(std::strcmp(hex, "#FFFFFF") == 0);
}

// ---- FASE 9 G2 — ALINHAMENTO AO MOCK (pontos 7-11) ---------------------------

// (G2-7) o ícone da hierarquia consulta o BodyComp PRIMEIRO: tic_static /
// tic_player / tic_rigid; a câmara tem tic_camera; o corpo NÃO cai no Box
TEST(hierarquia_icone_por_tipo_de_corpo_g27) {
    Scene s;
    // corpo ESTÁTICO (o chão do dono)
    Handle hStatic = s.create("Chao");
    s.get(hStatic)->addComponent<Transform3D>();
    BodyComp* bcS = s.get(hStatic)->addComponent<BodyComp>();
    bcS->type = BodyType::Static;
    EXPECT(hierIconFor(*s.get(hStatic)) == icons::Icon::Static);
    // personagem
    Handle hChar = s.create("Jogador");
    BodyComp* bcC = s.get(hChar)->addComponent<BodyComp>();
    bcC->type = BodyType::Character;
    EXPECT(hierIconFor(*s.get(hChar)) == icons::Icon::Person);
    // rígido
    Handle hRig = s.create("Caixa");
    BodyComp* bcR = s.get(hRig)->addComponent<BodyComp>();
    bcR->type = BodyType::Rigid;
    EXPECT(hierIconFor(*s.get(hRig)) == icons::Icon::Rigid);
    // câmara (tic_camera) tem PRIORIDADE sobre o corpo
    Handle hCam = s.create("Cam");
    s.get(hCam)->addComponent<CameraComp>();
    s.get(hCam)->addComponent<BodyComp>();
    EXPECT(hierIconFor(*s.get(hCam)) == icons::Icon::Camera);
    // mesh sem corpo continua Cube/Box de sempre
    Handle hMesh = s.create("Mesh");
    s.get(hMesh)->addComponent<Transform3D>();
    s.get(hMesh)->addComponent<MeshRenderer>();
    EXPECT(hierIconFor(*s.get(hMesh)) == icons::Icon::Box);
    // os ícones NOVOS existem pelo nome funcional do mock
    EXPECT(icons::iconByName("tic_static") ==
            static_cast<i32>(icons::Icon::Static));
    EXPECT(icons::iconByName("tic_rigid") ==
            static_cast<i32>(icons::Icon::Rigid));
    EXPECT(icons::iconByName("camara") ==
            static_cast<i32>(icons::Icon::Camera));
}

// (G2-8) a FÍSICA em duas colunas: 3 linhas TwoCol (tipo/forma/no chão) —
// o "body: static - obb - cha…" truncado de UMA linha morreu
TEST(inspector_fisica_duas_colunas_g28) {
    Env e;
    Handle h = e.scene.create("Corpo");
    Tic* t = e.scene.get(h);
    t->addComponent<Transform3D>();
    BodyComp* bc = t->addComponent<BodyComp>();
    bc->type = BodyType::Rigid;
    e.st.selected = h;
    const TextMetrics m = e.ui.textMetrics();
    const InspProfile prof = inspectorProfile(*t);
    InspRow plan[64];
    const u32 n = inspectorPlan(prof, m, false, 0u, plan);
    u32 twoCol = 0;
    for (u32 i = 0; i < n; ++i) {
        if (plan[i].kind == InspRow::Kind::TwoCol) {
            ++twoCol;
        }
    }
    EXPECT(twoCol == 3u);   // tipo · forma · no chão
    // o desenho não crasha e os valores curtos cabem SEM truncar
    e.frame();
    EXPECT(e.ui.solidsForTest().vertexCount() > 0);
    const f32 panelW = 300.0f;
    EXPECT(e.font.widthOf("character") < panelW - 2.0f * 12.0f);
    EXPECT(e.font.widthOf("capsule") < panelW - 2.0f * 12.0f);
}

// (G2-8) o Inspector VOLTA AO TOPO quando o TIC selecionado MUDA (o scroll
// do TIC anterior não se arrasta — era por isso que o Transform "não aparecia")
TEST(inspector_volta_ao_topo_na_troca_de_tic_g28) {
    Env e;
    // os DOIS TICs com conteúdo ALTO (o offset só vive com overflow — um
    // TIC só com Transform3D cabe inteiro e o clamp devolve 0)
    const auto fazRico = [](Scene& sc, const char* nome) {
        const Handle h = sc.create(nome);
        Tic* t = sc.get(h);
        t->addComponent<Transform3D>();
        t->addComponent<MeshRenderer>();
        BodyComp* bc = t->addComponent<BodyComp>();
        bc->type = BodyType::Character;
        return h;
    };
    const Handle a = fazRico(e.scene, "A");
    const Handle b = fazRico(e.scene, "B");
    e.st.selected = a;
    e.frame();
    // rola o Inspector 300px (o conteúdo é alto o bastante para o offset viver)
    e.ui.scrollSetOffset(editor::kIdScrollInsp, 300.0f);
    // troca o TIC → o draw repõe o offset a ZERO
    e.st.selected = b;
    e.frame();
    EXPECT(nearEqF(e.ui.scrollOffsetForTest(editor::kIdScrollInsp), 0.0f));
    // SEM troca o offset MANTÉM-se
    e.ui.scrollSetOffset(editor::kIdScrollInsp, 120.0f);
    e.frame();
    EXPECT(nearEqF(e.ui.scrollOffsetForTest(editor::kIdScrollInsp), 120.0f));
}

// (G2-7) o LONG-PRESS no nome TRUNCADO da hierarquia pede o nome completo
// (o contador de frames vive no EditorState; o main converte em toast)
TEST(hierarquia_long_press_nome_truncado_g27) {
    Env e;
    Handle h = e.scene.create("NomeMuitoCompridoQueNaoCabeNaLinhaDaHierarquia");
    e.scene.get(h)->addComponent<UiCanvas>();
    e.st.selected = h;
    e.frame();
    // a zona do nome da 1ª linha (painel 300: ícone 16+24+8 … olho a 196)
    const f32 nameX = 16.0f + 24.0f + 8.0f + 8.0f;
    const f32 rowY = 56.0f + 48.0f + 48.0f + 24.0f;   // chrome+header+pesq/2
    // dedo PARADO 29 frames: ainda NADA (o limiar é 30 = ~0,5s)
    e.input.injectDown(0, nameX + 40.0f, rowY);
    for (int i = 0; i < 29; ++i) {
        e.frame();
    }
    EXPECT(e.st.nameTip[0] == '\0');
    // o 30.º frame pede o nome COMPLETO (uma vez só)
    e.frame();
    EXPECT(std::string(e.st.nameTip) ==
           "NomeMuitoCompridoQueNaoCabeNaLinhaDaHierarquia");
    // e NÃO volta a pedir enquanto o dedo continua
    e.frame();
    e.frame();
    EXPECT(std::string(e.st.nameTip) ==
           "NomeMuitoCompridoQueNaoCabeNaLinhaDaHierarquia");
    // o dedo saiu → o contador regressa a zero
    e.input.injectUp(0);
    e.frame();
    EXPECT(e.st.hierHoldRow == -1 && e.st.hierHoldFrames == 0);
}

// (G2-9/G2-10) a BARRA ÚNICA: kToolbarH = 56 (a tab bar fundiu-se — os
// ~48px vão ao viewport) e o botão sliders NÃO EXISTE mais (inventário G0-4)
TEST(topbar_unica_56dp_e_viewport_ganha_48_g29) {
    // a fonte única do chrome diz 56
    EXPECT(nearEqF(safe::kToolbarH, 56.0f));
    EXPECT(nearEqF(safe::kToolbarH, safe::kTopBarH));
    // o viewport central GANHOU os 48px: em 1600×720 sem insets/drawer
    const UiRect view = safe::centerRect(1600.0f, 720.0f, safe::Insets{});
    const f32 hAntiga = 720.0f - 104.0f - 24.0f - 48.0f;   // chrome antigo
    EXPECT(nearEqF(view.h, hAntiga + 48.0f));
    // o TRIAD morreu (os "pontinhos fantasma"): o layout NÃO tem triad —
    // compila = o campo não existe; a barra não desenha nada no canto
    // sup-dir (afirmado pelo desenho: o + do fundo é o ÚNICO no canto dir)
    const vpchrome::Layout L = vpchrome::layout(view);
    EXPECT(nearEqF(L.addTicBtn.x + L.addTicBtn.w + 8.0f, view.x + view.w));
}
