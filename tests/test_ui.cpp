// tests/test_ui.cpp — F5.0-fix: a UI REAL (FontAtlas + UiContext + EditorUi)
// corrida no hospedeiro com uma FONTE REAL a 28 px (igual ao device) e gestos
// injetados (InputState::inject*).
//
// REGRESSÃO DO C33: no device, o texto do Inspector aparecia sobreposto em
// pilhas (nome×Transform3D, tex×input, velx×tc). Causa raiz: linhas de 26 px
// para uma fonte de 28 px — o bloco de glifos invadia a linha de cima. Aqui
// desenhamos o frame completo e aferimos a geometria REAL dos glifos emitidos
// (batches de CPU): ZERO colisões glifo-a-glifo. No código antigo este teste
// apanhava 13 colisões com a mesma fonte.
#include "TestFramework.h"
#include "ui/UiContext.h"
#include "ui/EditorUi.h"
#include "ui/UiEditor.h"   // 0.9.6: anyOverlayOpen/fullscreenOverlayOpen
#include "ui/Toolbar.h"
#include "ui/FontAtlas.h"
#include "ui/SafeArea.h"
#include "ui/EditorLayout.h"
#include "platform/InputState.h"
#include "core/Scene.h"
#include "core/Presets.h"
#include "components/Transform3D.h"
#include "components/MeshRenderer.h"
#include "components/InputMap.h"
#include "components/BodyComp.h"
#include "components/TouchControls.h"

#include <cmath>
#include <cstdio>
#include <vector>

using namespace vv;
using namespace vv::editor;
using ::test::nearEqF;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;
constexpr f32 kFontPx = 28.0f;   // o MESMO heightPx do main.cpp no device

struct Rect { f32 x0, y0, x1, y1; };

void collectRects(const QuadBatch& b, std::vector<Rect>& out) {
    const QuadVertex* v = b.vertices();
    const u32 n = b.vertexCount();
    for (u32 i = 0; i + 5 < n; i += 6) {
        out.push_back({v[i].x, v[i].y, v[i + 2].x, v[i + 2].y});
    }
}

// pior interseção glifo×glifo (px, min(dx,dy) do par que MAIS invade).
// Glifos da MESMA palavra nunca colidem (avanço horizontal); qualquer
// colisão 2D real > ruído de AA indica sobreposição de linhas.
f32 worstGlyphPenetration(const UiContext& ui, f32& ox, f32& oy) {
    std::vector<Rect> g;
    collectRects(ui.glyphsForTest(), g);
    f32 worst = 0.0f;
    ox = oy = 0.0f;
    for (size_t i = 0; i < g.size(); ++i) {
        for (size_t j = i + 1; j < g.size(); ++j) {
            const f32 dx = std::min(g[i].x1, g[j].x1) - std::max(g[i].x0, g[j].x0);
            const f32 dy = std::min(g[i].y1, g[j].y1) - std::max(g[i].y0, g[j].y0);
            if (dx > 0.5f && dy > 0.5f) {   // interseção 2D REAL (para de AA)
                const f32 pen = std::min(dx, dy);
                if (pen > worst) {
                    worst = pen;
                    ox = dx;
                    oy = dy;
                    if (std::getenv("VV_DBG_GLYPH")) {
                        std::printf("GLYPH-OVERLAP: A=(%.1f,%.1f)-(%.1f,%.1f) "
                                    "B=(%.1f,%.1f)-(%.1f,%.1f) pen=%.1f\n",
                                    g[i].x0, g[i].y0, g[i].x1, g[i].y1,
                                    g[j].x0, g[j].y0, g[j].x1, g[j].y1, pen);
                    }
                }
            }
        }
    }
    return worst;
}

// ambiente de um frame: fonte real + UI + cena com o TIC do caso C33
struct Env {
    FontAtlas   font;
    UiContext   ui;
    InputState  input;
    Scene       scene;
    Handle      selected{};
    EditorState st;
    toolbar::GizmoModeState gzMode;   // 0.7.6
    AssetCatalog catalog;
    bool ok = false;

    // F5.2: overlay desenhado DEPOIS dos painéis (como no main) — 1=diálogo
    // de armazenamento, 2=import, 3=viewer de logs, 4=settings
    int overlay = 0;
    std::vector<fileapi::Candidate> importCands;
    std::vector<std::string> logLines, logDumps;
    const char* modeText = "all files";

    explicit Env(bool withTc, bool withCatalog) {
        const char* fontPath = FONT_FIXTURE;
        ok = font.loadFromPaths(&fontPath, 1, kFontPx);
        if (!ok) {
            return;
        }
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
        selected = createTicFromPreset(scene, PresetKind::PlayerBody3D, nullptr, nullptr);
        Tic* tic = scene.get(selected);
        if (withTc && tic) {
            tic->addComponent<TouchControls>();
        }
        if (tic) {
            if (MeshRenderer* mr = tic->getComponent<MeshRenderer>()) {
                mr->meshPath = "meshes/quad.obj";
                mr->texPath  = "textures/wood.png";
            }
        }
        st.selected = selected;
        if (withCatalog) {
            catalog.meshes   = {"meshes/quad.obj", "meshes/cube.obj"};
            catalog.textures = {"textures/wood.png"};
        }
    }

    void frame() {
        ui.beginFrame(nullptr, &input, kSW, kSH);
        // 0.9.0 — o MESMO gate do main: com overlay ABERTO os painéis NÃO
        // desenham (não reclamam gestos — o overlay é modal; ver o main:
        // if (!modalOpen) { drawHierarchy… drawInspector… })
        const bool modalOpen = overlay != 0;
        toolbar::draw(ui, st);
        if (!modalOpen) {
            drawHierarchy(ui, scene, st);
            drawInspector(ui, scene, st, withCatalog_());
        }
        switch (overlay) {
            case 1: drawStorageDialog(ui, input, kSW, kSH, st); break;
            case 2: drawImportMenu(ui, input, kSW, kSH, st, importCands); break;
            case 3: drawLogViewer(ui, input, kSW, kSH, st, logLines, logDumps); break;
            case 4: drawSettingsMenu(ui, input, kSW, kSH, st, modeText); break;
            default: break;
        }
        ui.statusLine("status");
        ui.endFrame();
        input.clearEdges();
    }

private:
    const AssetCatalog* withCatalog_() const {
        return catalog.meshes.empty() ? nullptr : &catalog;
    }
};

} // namespace

// métricas REAIS do bake: ascent/descent coerentes com uma sans a 28 px —
// são estes números que alimentam as alturas de linha do plano
TEST(ui_fonte_real_metricas_do_bake) {
    FontAtlas f;
    const char* path = FONT_FIXTURE;
    EXPECT(f.loadFromPaths(&path, 1, kFontPx));
    EXPECT(f.ok());
    EXPECT(nearEqF(f.height(), kFontPx));
    EXPECT(f.ascent() > 14.0f && f.ascent() < 26.0f);    // sans 28 px
    EXPECT(f.descent() > 2.0f && f.descent() < 12.0f);
    const TextMetrics m{f.ascent(), f.descent()};
    EXPECT(m.block() >= 22.0f && m.block() <= 30.0f);
    // o bloco TEM de caber nas linhas novas (o bug: linhas 26 px < bloco 28)
    EXPECT(inspTextRowH(m) >= m.block() + 4.0f);
    EXPECT(inspButtonRowH(m) >= m.block() + 6.0f);
    // baseline centrada: bloco inteiro DENTRO da linha
    const f32 rowH = inspTextRowH(m);
    const f32 b = inspBaseline(10.0f, rowH, m);
    EXPECT(b - m.ascent >= 10.0f);
    EXPECT(b + m.descent <= 10.0f + rowH);
}

// REGRESSÃO DO C33: frame completo do editor com fonte real a 28 px —
// ZERO colisões glifo-a-glifo (o código antigo apanhava 13: tex×input 7 px,
// velx×tc 3 px, etc.). Com o pior caso do dono: Player + tc + assets.
TEST(ui_inspector_sem_sobreposicao_glifos_c33) {
    Env e(/*withTc=*/true, /*withCatalog=*/true);
    EXPECT(e.ok);
    e.frame();

    f32 ox = 0.0f, oy = 0.0f;
    const f32 worst = worstGlyphPenetration(e.ui, ox, oy);
    EXPECT(worst <= 2.0f);   // ≤ ruído de AA (antigo: 7.4 px)
    // e há texto DE VERDADE (a aferição não é vazia)
    EXPECT(e.ui.glyphsForTest().vertexCount() > 6 * 100);

    // o painel do Inspector mostra as linhas do caso do dono em Ys distintos
    const Tic* tic = e.scene.get(e.selected);
    const TextMetrics tm = e.ui.textMetrics();
    const InspProfile prof = inspectorProfile(*tic);
    InspRow plan[32];
    const u32 n = inspectorPlan(prof, tm, true, 0u, plan);
    for (u32 i = 1; i < n; ++i) {
        EXPECT(plan[i].y > plan[i - 1].y);   // sequencial, sem reinício
    }
}

// o mesmo, SEM TouchControls (variante com o botão "add TouchControls") e
// SEM catálogo (mesh/tex só leitura) — as duas outras formas do painel
TEST(ui_inspector_sem_sobreposicao_variacoes) {
    {
        Env e(false, false);
        EXPECT(e.ok);
        e.frame();
        f32 ox = 0.0f, oy = 0.0f;
        EXPECT(worstGlyphPenetration(e.ui, ox, oy) <= 2.0f);
    }
    {
        Env e(false, true);
        EXPECT(e.ok);
        e.frame();
        f32 ox = 0.0f, oy = 0.0f;
        EXPECT(worstGlyphPenetration(e.ui, ox, oy) <= 2.0f);
    }
}

// scroll por cima: drag vertical dentro da região → offset cresce ATÉ ao
// máximo e o ÚLTIMO campo do plano (tc / addTc) fica inteiro na região
TEST(ui_scroll_revela_ultimo_campo) {
    Env e(true, true);
    EXPECT(e.ok);

    Tic* tic = e.scene.get(e.selected);
    const TextMetrics tm = e.ui.textMetrics();
    const InspProfile prof = inspectorProfile(*tic);
    const f32 contentH = inspectorContentHeight(prof, tm, true, 0u);

    const UiRect panel = safe::inspectorPanelRect(kSW, kSH, safe::Insets{});
    const f32 contentTop = panel.y + kHeaderH + 4.0f;
    const f32 listH = panel.h - kHeaderH - 4.0f;
    EXPECT(contentH > listH);   // o caso C33 tem de transbordar

    // frame 1: press edge dentro da região (margem ESQUERDA do painel —
    // fora do trilho dos sliders, para o scroll reclamar o gesto)
    e.input.injectDown(0, panel.x + 30.0f, panel.y + panel.h * 0.5f);
    e.frame();
    // frame 2: arrasto para CIMA (revela o fundo)
    e.input.injectMove(0, panel.x + 30.0f, panel.y - 999.0f);
    e.frame();
    // frame 3: solta (drag ≥ kTapPx → sem tap re-despachado)
    e.input.injectUp(0);
    e.frame();

    const f32 off = e.ui.scrollOffsetForTest(kInspectorScrollId);
    EXPECT(nearEqF(off, scroll::maxOffset(contentH, listH)));
    EXPECT(off > 0.0f);

    // a ÚLTIMA linha do plano fica INTEIRA dentro da região com o offset
    InspRow plan[32];
    const u32 n = inspectorPlan(prof, tm, true, 0u, plan);
    const InspRow& last = plan[n - 1];
    const f32 lastTop = contentTop + last.y - off;
    EXPECT(lastTop >= contentTop);
    EXPECT(lastTop + last.h <= contentTop + listH + 0.01f);
    // e o indicador de scroll é desenhado (barra fina à direita da região)
    std::vector<Rect> solids;
    collectRects(e.ui.solidsForTest(), solids);
    bool indicator = false;
    for (const Rect& r : solids) {
        if (r.x0 >= panel.x + panel.w - 6.0f && (r.x1 - r.x0) <= 4.0f &&
            r.y1 > r.y0) {
            indicator = true;
        }
    }
    EXPECT(indicator);
}

// sliders DENTRO da região: drag horizontal num slider muda o valor e NÃO
// faz scroll (o slider mantém a prioridade de captura — regra da spec).
// 0.9.0: os 9 sliders do Transform MORRERAM (caixas X/Y/Z — spec C); o
// slider vivo mais alto do painel é o "cor R" do Material.
TEST(ui_slider_captura_dentro_do_scroll) {
    Env e(true, true);
    EXPECT(e.ok);

    Tic* tic = e.scene.get(e.selected);
    MeshRenderer* mr = tic->getComponent<MeshRenderer>();
    ASSERT(mr != nullptr);
    const f32 r0 = mr->tint[0];

    const UiRect panel = safe::inspectorPanelRect(kSW, kSH, safe::Insets{});
    const f32 contentTop = panel.y + kHeaderH + 4.0f;
    // linha "cor R" — derivada do PLANO com as métricas reais (não diverge
    // do desenho)
    const TextMetrics tm = e.ui.textMetrics();
    InspRow plan[32];
    const u32 n = inspectorPlan(inspectorProfile(*tic), tm, true, 0u, plan);
    f32 colY = -1.0f;
    for (u32 i = 0; i < n; ++i) {
        if (plan[i].kind == InspRow::Kind::ColorSlider &&
            plan[i].id == kInspectorColBase) {
            colY = plan[i].y;
            break;
        }
    }
    EXPECT(colY > 0.0f);
    // 0.9.0: o plano é MAIS ALTO (secções/trf) — a linha "cor R" está FORA
    // do primeiro ecrã: ROLA o painel até ela primeiro
    const f32 targetOff = colY - 40.0f;
    e.input.injectDown(0, panel.x + panel.w * 0.5f, contentTop + 200.0f);
    e.frame();
    e.input.injectMove(0, panel.x + panel.w * 0.5f,
                       contentTop + 200.0f - targetOff);
    e.frame();
    e.input.injectUp(0);
    e.frame();
    const f32 off1 = e.ui.scrollOffsetForTest(kInspectorScrollId);
    EXPECT(off1 > targetOff - 60.0f);   // rolou até perto da linha
    const f32 rowScr = contentTop + colY - off1;

    // press NO TRILHO do slider cor R (track x = painel + 84, w = 118). O
    // tint default é BRANCO (1.0) — o arrasto vai para a ESQUERDA (desce)
    e.input.injectDown(0, panel.x + 84.0f + 59.0f, rowScr + 18.0f);
    e.frame();
    // arrasto horizontal para a ESQUERDA (valor desce; vertical quase nulo)
    e.input.injectMove(0, panel.x + 84.0f + 59.0f - 80.0f, rowScr + 18.0f);
    e.frame();
    e.input.injectUp(0);
    e.frame();

    EXPECT(mr->tint[0] < r0 - 0.2f);   // o slider seguiu o dedo
    EXPECT(nearEqF(e.ui.scrollOffsetForTest(kInspectorScrollId), off1,
                   0.5f));   // o scroll NÃO reclamou (offset intacto)
}

// tap re-despachado: tap nos botões do PLANO aciona a ação certa (seletores
// de assets F5-E e "add TouchControls") — dentro da região com scroll
TEST(ui_tap_redespachado_botoes_do_plano) {
    Env e(false, true);   // SEM TouchControls → o botão do fundo existe
    EXPECT(e.ok);

    const Tic* tic = e.scene.get(e.selected);
    const TextMetrics tm = e.ui.textMetrics();
    const InspProfile prof = inspectorProfile(*tic);
    InspRow plan[32];
    const u32 n = inspectorPlan(prof, tm, true, 0u, plan);
    const UiRect panel = safe::inspectorPanelRect(kSW, kSH, safe::Insets{});
    const f32 contentTop = panel.y + kHeaderH + 4.0f;

    auto rowOf = [&](InspRow::Kind k) -> const InspRow* {
        for (u32 i = 0; i < n; ++i) {
            if (plan[i].kind == k) return &plan[i];
        }
        return nullptr;
    };
    const InspRow* mesh = rowOf(InspRow::Kind::MeshButton);
    const InspRow* tex = rowOf(InspRow::Kind::TexButton);
    const InspRow* addTc = rowOf(InspRow::Kind::AddTc);
    EXPECT(mesh && tex && addTc);

    // tap 1-frame (down+up antes da UI) no botão mesh → abre seletor MESH
    e.input.injectDown(0, panel.x + panel.w * 0.5f, contentTop + mesh->y + 4.0f);
    e.input.injectUp(0);
    e.frame();
    EXPECT(e.st.assetMenu == 1);

    // tap no botão tex → seletor TEXTURA
    e.input.injectDown(0, panel.x + panel.w * 0.5f, contentTop + tex->y + 4.0f);
    e.input.injectUp(0);
    e.frame();
    EXPECT(e.st.assetMenu == 2);

    // tap no botão "add TouchControls" → cria o componente no TIC.
    // 0.7.0: o plano ganhou linhas (visivel + R/G/B) e o botão do fundo
    // ficou ALÉM da 1ª página → ROLA até ao fundo e usa a posição COM o
    // offset (o MESMO cálculo do re-despacho: contentTop + y − off)
    e.frame();   // cria o slot de scroll do Inspector
    const f32 contentH = inspectorContentHeight(prof, tm, true, 0u);
    const f32 listH = panel.h - kHeaderH - 4.0f;
    const f32 off = scroll::clampOffset(9999.0f, contentH, listH);
    e.ui.scrollSetOffset(kInspectorScrollId, off);
    e.input.injectDown(0, panel.x + panel.w * 0.5f,
                       contentTop + addTc->y - off + 10.0f);
    e.input.injectUp(0);
    e.frame();
    EXPECT(e.scene.get(e.selected)->getComponent<TouchControls>() != nullptr);

    // tap numa área SEM widget (meio da linha body → label) não faz nada
    e.st.assetMenu = 0;
    const f32 bodyY = [&]() -> f32 {
        for (u32 i = 0; i < n; ++i) {
            if (plan[i].kind == InspRow::Kind::Label) return plan[i].y;
        }
        return 0.0f;
    }();
    e.input.injectDown(0, panel.x + panel.w - 4.0f, contentTop + bodyY + 4.0f);
    e.input.injectUp(0);
    e.frame();
    EXPECT(e.st.assetMenu == 0);
    // (o gesto nasceu DENTRO da região → foi de scroll/tap sem alvo)
}

// o plano e o conteúdo respeitam a área ÚTIL com insets do C33 (nav bar à
// direita): o painel encolhe e o scroll compensa — nada sai do contentRect
TEST(ui_inspector_com_insets_scroll_compensa) {
    Env e(true, true);
    EXPECT(e.ok);
    const safe::Insets in = safe::insetsFromContentRect(kSW, kSH, 0, 24, kSW - 42, 628);
    e.ui.setSafeArea(in);
    e.frame();

    const UiRect panel = safe::inspectorPanelRect(kSW, kSH, in);
    EXPECT(safe::rectInside(panel, safe::contentRect(kSW, kSH, in)));
    f32 ox = 0.0f, oy = 0.0f;
    EXPECT(worstGlyphPenetration(e.ui, ox, oy) <= 2.0f);   // sem sobreposição
}

// ---------------------------------------------------------------------------
// F5.2: overlays de armazenamento — diálogo All Files Access, overlay IMPORT,
// viewer de logs e Settings com modo (taps injetados; a geometria vem das
// fórmulas partilhadas em ui/EditorLayout.h — o MESMO cálculo do desenho).
// Env.overlay faz os overlays serem DESENHADOS nos frames (como no main).
// ---------------------------------------------------------------------------

namespace {
// fecha o gesto (tap completo) num rect
void tapAt(Env& e, f32 x, f32 y) {
    e.input.injectDown(0, x, y);
    e.frame();
    e.input.injectUp(0);
    e.frame();
}
} // namespace

// DIÁLOGO: toque fora fecha; "Permitir" → 1; "Cancelar" → 2
TEST(ui_storage_dialogo_permitir_cancelar_fora) {
    Env e(false, false);
    EXPECT(e.ok);
    e.overlay = 1;

    // 1) toque fora (canto superior esquerdo, na toolbar) → fecha sem ação
    e.st.storageDialog = true;
    e.frame();   // 1º frame: desenha o diálogo (o gesto ainda não nasceu)
    tapAt(e, 40.0f, 40.0f);
    EXPECT(e.st.storageDialog == false);

    // 2) "Permitir" → devolve 1 no frame do release
    const UiRect dlg = centeredMenuRect(0.0f, 0.0f, kSW, kSH, storageDialogHeight());
    UiRect allow{}, cancel{};
    storageDialogButtons(dlg, allow, cancel);

    e.st.storageDialog = true;
    e.frame();
    e.input.injectDown(0, allow.x + allow.w * 0.5f, allow.y + allow.h * 0.5f);
    e.frame();          // press: botão reclama o gesto
    e.input.injectUp(0);
    e.frame();          // release: botão dispara DENTRO do drawStorageDialog
    EXPECT(e.st.storageDialog == false);   // o frame de release fechou o diálogo

    // valor devolvido 1 (Permitir): dois ciclos dentro do MESMO beginFrame
    {
        e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
        EditorState st2;
        st2.storageDialog = true;
        e.input.injectDown(0, allow.x + allow.w * 0.5f, allow.y + allow.h * 0.5f);
        drawStorageDialog(e.ui, e.input, kSW, kSH, st2);   // press
        e.input.injectUp(0);
        const int v = drawStorageDialog(e.ui, e.input, kSW, kSH, st2);   // release
        e.ui.endFrame();
        e.input.clearEdges();
        EXPECT(v == 1);
        EXPECT(st2.storageDialog == false);
    }

    // 3) "Cancelar" → devolve 2 (mesmo padrão)
    {
        e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
        EditorState st3;
        st3.storageDialog = true;
        e.input.injectDown(0, cancel.x + cancel.w * 0.5f, cancel.y + cancel.h * 0.5f);
        drawStorageDialog(e.ui, e.input, kSW, kSH, st3);
        e.input.injectUp(0);
        const int v = drawStorageDialog(e.ui, e.input, kSW, kSH, st3);
        e.ui.endFrame();
        e.input.clearEdges();
        EXPECT(v == 2);
        EXPECT(st3.storageDialog == false);
    }
}

// IMPORT: pick devolve i+1; vazio mantém aberta e devolve 0
TEST(ui_import_overlay_pick_e_vazio) {
    Env e(false, false);
    EXPECT(e.ok);
    e.overlay = 2;

    std::vector<fileapi::Candidate> cands;
    for (int i = 0; i < 3; ++i) {
        fileapi::Candidate c;
        c.name = "casa" + std::to_string(i) + ".obj";
        c.path = "/Download/" + c.name;
        c.kind = 'm';
        cands.push_back(c);
    }
    cands.push_back({"madeira.png", "/Download/madeira.png", 't'});
    e.importCands = cands;

    const f32 h = importMenuHeight(5);
    const f32 x = (kSW - kMenuW) * 0.5f;
    const f32 y = (kSH - h) * 0.5f;

    // tap na 2ª linha → o drawImportMenu do release devolve 2 (2 ciclos no
    // MESMO beginFrame: press → release)
    const UiRect row1 = importRowRect({x, y, kMenuW, h}, 1);
    {
        e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
        EditorState st2;
        st2.importMenu = true;
        e.input.injectDown(0, row1.x + 20.0f, row1.y + 20.0f);
        drawImportMenu(e.ui, e.input, kSW, kSH, st2, cands);
        e.input.injectUp(0);
        const int v = drawImportMenu(e.ui, e.input, kSW, kSH, st2, cands);
        e.ui.endFrame();
        e.input.clearEdges();
        EXPECT(v == 2);
        EXPECT(st2.importMenu == false);
    }

    // vazio → devolve 0 e o tap DENTRO não fecha
    std::vector<fileapi::Candidate> none;
    e.importCands = none;
    e.st.importMenu = true;
    e.frame();
    tapAt(e, x + kMenuW * 0.5f, y + h * 0.5f);
    EXPECT(e.st.importMenu == true);

    // toque FORA fecha
    tapAt(e, 40.0f, 40.0f);
    EXPECT(e.st.importMenu == false);
}

// LOG VIEWER: auto-scroll para o fundo no 1º frame + drag revela/clampa no fim
TEST(ui_log_viewer_scroll_e_autoscroll_fundo) {
    Env e(false, false);
    EXPECT(e.ok);
    e.overlay = 3;

    std::vector<std::string> lines;
    for (int i = 0; i < 60; ++i) {
        char b[48];
        std::snprintf(b, sizeof(b), "%02d-29 10:00:00.000 I/GONI: linha-%02d", 9, i);
        lines.emplace_back(b);
    }
    e.logLines = lines;
    e.logDumps = {"crash-300.dump - signal: SIGSEGV (11)"};

    // geometria do painel (86% × 80% da BANDA DO VIEWPORT 0.9.0: o overlay
    // nunca fica por baixo do chrome — o "fechar" sempre clicável)
    f32 ox, oy, aw, ah;
    overlayArea(kSW, kSH, safe::Insets{}, ox, oy, aw, ah);
    const f32 w = aw * 0.86f, h = ah * 0.80f;
    const f32 x = ox + (aw - w) * 0.5f, y = oy + (ah - h) * 0.5f;
    const f32 listTop = y + kHeaderH;
    const f32 regionH = h - kHeaderH;
    const TextMetrics tm = e.ui.textMetrics();
    const f32 rowH = tm.block() + 6.0f;
    const f32 contentH = 34.0f + 60.0f * rowH + (34.0f + 1.0f * rowH);
    EXPECT(contentH > regionH);   // tem de transbordar

    // 1º frame com justOpened → offset = contentH cru (set após endScroll);
    // o 2º frame clampa para o MÁXIMO da região — fundo visível
    e.st.logViewer = true;
    e.st.logViewerJustOpened = true;
    e.frame();
    e.frame();
    EXPECT(nearEqF(e.ui.scrollOffsetForTest(kLogsScrollId),
                   scroll::maxOffset(contentH, regionH), 0.5f));

    // drag para cima a partir do fundo: fica no máximo (clamp)
    e.input.injectDown(0, x + w * 0.5f, y + h * 0.8f);
    e.frame();
    e.input.injectMove(0, x + w * 0.5f, y - 500.0f);
    e.frame();
    e.input.injectUp(0);
    e.frame();
    EXPECT(nearEqF(e.ui.scrollOffsetForTest(kLogsScrollId),
                   scroll::maxOffset(contentH, regionH), 0.5f));

    // a ÚLTIMA linha fica inteira dentro da região com esse offset
    const f32 lastTop = listTop + contentH -
                        e.ui.scrollOffsetForTest(kLogsScrollId);
    EXPECT(lastTop <= listTop + regionH + 0.01f);

    // botão "fechar" (topo direito do painel) fecha o viewer
    tapAt(e, x + w - kPad - 48.0f, y + 22.0f);
    EXPECT(e.st.logViewer == false);
}

// SETTINGS: modo visível, 3 itens com devolução 1/2/3 — faixa de ids 4400+
// (sem colisão com o "+" da Hierarchy id 40 no MESMO frame)
// 0.9.6 (G1) — OS ECRÃS CHEIOS: a lista fechada da camada modal-maior
// (capturam TODO o toque, escondem glifos/gizmos e a barra de baixo). Os
// DIÁLOGOS flutuantes NÃO contam — continuam a conviver com o chrome.
TEST(ui_fullscreen_overlay_e_a_lista_fechada) {
    {
        EditorState st;
        st.settingsMenu = true;
        EXPECT(fullscreenOverlayOpen(st));
        EXPECT(anyOverlayOpen(st));
    }
    {
        EditorState st;
        st.docsScreen.open = true;
        EXPECT(fullscreenOverlayOpen(st));
    }
    {
        EditorState st;
        st.scriptWin.open = true;
        EXPECT(fullscreenOverlayOpen(st));
    }
    {
        EditorState st;
        st.textWin.open = true;
        EXPECT(fullscreenOverlayOpen(st));
    }
    {   // os DIÁLOGOS: modais mas NÃO ecrãs cheios
        EditorState st;
        st.fileMenu = true;
        st.assetMenu = 1;
        st.logViewer = true;
        st.contextMenu = true;
        st.textInput = true;
        EXPECT(anyOverlayOpen(st));
        EXPECT(!fullscreenOverlayOpen(st));
    }
    {   // nada aberto
        EditorState st;
        EXPECT(!anyOverlayOpen(st));
        EXPECT(!fullscreenOverlayOpen(st));
    }
}

TEST(ui_settings_menu_modo_e_tres_itens) {
    Env e(false, false);
    EXPECT(e.ok);
    e.overlay = 4;
    e.modeText = "all files";

    e.st.settingsMenu = true;
    e.frame();   // settings sobre o editor completo
    f32 ox = 0.0f, oy = 0.0f;
    EXPECT(worstGlyphPenetration(e.ui, ox, oy) <= 2.0f);   // sem sobreposição

    // rects dos 7 itens (formula do drawSettingsMenu; 0.8.10: +fonte/+recon;
    // 0.8.11: +probe de áudio +volume geral)
    const f32 modeH = 30.0f;
    const f32 h = kHeaderH + modeH + 7.0f * 64.0f + kPad;
    const f32 x = (kSW - kMenuW) * 0.5f;
    const f32 y = (kSH - h) * 0.5f;
    const f32 itemsTop = y + kHeaderH + modeH;

    for (int i = 0; i < 7; ++i) {
        e.st.settingsMenu = true;
        e.frame();
        e.input.injectDown(0, x + kMenuW * 0.5f,
                           itemsTop + static_cast<f32>(i) * 64.0f + 28.0f);
        e.frame();
        e.input.injectUp(0);
        e.frame();
        // devolução ANTES de fechar (st mutado no frame de release): reabre
        // e valida o valor com o gesto consumido — o mesmo padrão dos outros
        e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
        EditorState st2;
        st2.settingsMenu = true;
        const int v = drawSettingsMenu(e.ui, e.input, kSW, kSH, st2, "all files", true);
        e.ui.endFrame();
        e.input.clearEdges();
        e.input.injectUp(0);
        if (i == 0) {
            EXPECT(v == 0);   // gesto já consumido pelo frame anterior
            EXPECT(e.st.settingsMenu == false);   // o frame anterior fechou
        }
    }
    // devoluções DIRETAS dos 7 itens (gestos limpos, um por vez; 0.8.10
    // acrescentou "fonte: manter/largar" e "reconverter assets"; 0.8.11
    // acrescentou "diagnostico audio (probe)" e "volume geral")
    for (int i = 0; i < 7; ++i) {
        e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
        EditorState st2;
        st2.settingsMenu = true;
        e.input.injectDown(0, x + kMenuW * 0.5f,
                           itemsTop + static_cast<f32>(i) * 64.0f + 28.0f);
        // press processado na 1ª chamada; release na 2ª
        drawSettingsMenu(e.ui, e.input, kSW, kSH, st2, "all files");
        e.input.injectUp(0);
        const int v = drawSettingsMenu(e.ui, e.input, kSW, kSH, st2, "all files", true, 1.0f);
        e.ui.endFrame();
        e.input.clearEdges();
        EXPECT(v == i + 1);
        EXPECT(st2.settingsMenu == false);
    }
}

// 0.9.6 (G2-5) — A QUEBRA DE LINHA das Docs: por PALAVRAS (nunca corta a
// meio), cobre o texto TODO (a descrição inteira desenha — nada de "…"),
// fronteiras de code point respeitadas (acentos nunca partidos).
TEST(textwrap_quebra_por_palavras_e_cobre_tudo) {
    // medidor fake mono (10px/char) — o algoritmo é GL-free
    auto mono10 = [](const char* s) { return (f32)(10.0f * std::strlen(s)); };
    std::vector<textwrap::Line> ls;
    {
        textwrap::wrap("aaa bb ccc ddd", 70.0f, mono10, ls);
        EXPECT(ls.size() == 2);                       // "aaa bb" + "ccc ddd"
        EXPECT(std::string("aaa bb ccc ddd", ls[0].begin, ls[0].len) ==
               "aaa bb");
        EXPECT(std::string("aaa bb ccc ddd", ls[1].begin, ls[1].len) ==
               "ccc ddd");
    }
    {
        // cobertura: as linhas JUNTAS contêm TODAS as palavras
        textwrap::wrap("Define um parâmetro de cor: o parâmetro tinge o "
                       "material do TIC de origem", 120.0f, mono10, ls);
        std::string all;
        for (const auto& l : ls) {
            EXPECT(l.len > 0);
            all.append(std::string(
                "Define um parâmetro de cor: o parâmetro tinge o material "
                "do TIC de origem", l.begin, l.len));
            all += ' ';
        }
        EXPECT(all.find("Define") != std::string::npos);
        EXPECT(all.find("parâmetro") != std::string::npos);
        EXPECT(all.find("origem") != std::string::npos);
        EXPECT(ls.size() >= 2);                       // realmente quebrou
    }
    {
        // palavra MAIOR que a largura: parte por LARGURA (cada pedaço cabe)
        textwrap::wrap("abcdefghijklmnop", 50.0f, mono10, ls);
        EXPECT(ls.size() >= 2);
        for (const auto& l : ls) {
            EXPECT(static_cast<f32>(l.len) * 10.0f <= 50.0f + 0.01f);
        }
    }
    {
        // acentos: o corte NUNCA parte um code point (o 'ç' é 2 bytes)
        textwrap::wrap("ação é show mostrar", 50.0f, mono10, ls);
        // se partir, os bytes de continuação não iniciam linha
        for (const auto& l : ls) {
            const char* base = "ação é show mostrar";
            const unsigned char c = (unsigned char)base[l.begin];
            EXPECT((c & 0xC0) != 0x80);
        }
    }
    {
        // texto curto: 1 linha inteira
        textwrap::wrap("curto", 500.0f, mono10, ls);
        EXPECT(ls.size() == 1 && ls[0].len == 5);
    }
}
