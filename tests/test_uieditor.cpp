// tests/test_uieditor.cpp — 0.7.0: EDITOR DE UI DEDICADO + GESTÃO DE TICs.
//
// A UI REAL no hospedeiro (FontAtlas + UiContext + gestos injetados — o
// padrão do test_playui) aferindo o CONTRATO da sub-fase:
//   • separador "3D | UI": o botão UI abre o viewport 2D dedicado (o mesmo
//     centerRect da câmara; nada sobreposto aos painéis);
//   • WYSIWYG: tap seleciona o elemento (o de CIMA ganha), drag MOVE
//     (ox/oy pelo delta em design px), tap no vazio DESSELECIONA;
//   • viewport 2D nunca corta o canvas (scale-to-fit afervido);
//   • Inspector de UI: plano com y cumulativo SEM sobreposição; sliders
//     escrevem pos/size/cor; botões (visível/âncoras/ação/remover) funcionam;
//   • TECLADO IN-APP: geometria partilhada sem sobreposição e DENTRO da
//     safe-area; digitar→APAGA→OK renomeia; Cancelar NÃO aplica;
//   • gestão de TICs: menu contextual (⋮) com Renomear/Remover (com
//     confirmação)/Duplicar (copia componentes)/Visibilidade; olho na
//     Hierarchy; desselecionar no vazio da Hierarchy E do viewport 3D;
//   • cor por TIC: o drawMesh aplica o tint do MeshRenderer (stub GL) e
//     nullptr = branco (o comportamento 0.6.x);
//   • toolbar sem sobreposição (3 botões + 3D|UI + gizmos) em 2 larguras.
#include "TestFramework.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "components/UiCanvas.h"
#include "core/Presets.h"
#include "core/Scene.h"
#include "platform/InputState.h"
#include "render/Cube.h"
#include "render/Mesh.h"
#include "render/Renderer.h"
#include "ui/EditorLayout.h"
#include "ui/Toolbar.h"   // 0.7.6: barra final (G3 segmented 3D|UI)
#include "components/CameraComp.h"   // 0.7.7: TIC de câmara no "+"
#include "core/CameraUtil.h"
#include "ui/EditorUi.h"
#include "ui/FontAtlas.h"
#include "ui/SafeArea.h"
#include "ui/UiContext.h"
#include "ui/UiEditor.h"
#include "ui/UiRuntime.h"
#include <GLES3/gl3.h>   // stub (glstub::stats — 0.7.0: uTint)

using namespace vv;
using namespace vv::editor;
using ::test::nearEqF;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;

// ambiente: o MESMO fluxo do main.frame() em modo editor (toolbar + 3D|UI +
// viewport 2D + painéis + teclado quando aberto)
struct Env {
    FontAtlas  font;
    UiContext  ui;
    InputState input;
    Scene      scene;
    EditorState st;
    toolbar::GizmoModeState gzMode;   // 0.7.6: G4 da barra final
    Handle     hud{};
    bool       ok = false;

    Env() {
        const char* fontPath = FONT_FIXTURE;
        ok = font.loadFromPaths(&fontPath, 1, 28.0f);
        if (!ok) {
            return;
        }
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
        hud = scene.create("HUD");
        if (Tic* t = scene.get(hud)) {
            t->addComponent<UiCanvas>();
        }
        st.selected = hud;
    }

    UiCanvas* canvas() {
        Tic* t = scene.get(st.selected.valid() ? st.selected : hud);
        return t ? t->getComponent<UiCanvas>() : nullptr;
    }

    void frame() {
        ui.beginFrame(nullptr, &input, kSW, kSH);
        // 0.7.5 — o MESMO gate do main: com overlay MODAL aberto o chrome
        // do editor não se desenha; no lugar, o BACKDROP opaco (nada do
        // canvas/painéis à mista com o overlay)
        const bool modalOpen = anyOverlayOpen(st);
        if (modalOpen) {
            drawModalBackdrop(ui, kSW, kSH);
        }
        if (!modalOpen) {
            // 0.7.6 — a BARRA FINAL de 5 grupos (o MESMO chamador do main;
            // com seleção de TIC em 3D o G4 transformação aparece)
            const toolbar::Actions ta =
                toolbar::draw(ui, st, gzMode, scene.get(st.selected) != nullptr);
            if (ta.menuDropdown) {
                st.fileMenu = !st.fileMenu;
            }
            if (st.uiMode) {
                drawUiViewport(ui, scene, st, input, kSW, kSH);
            }
            // o "+" do cabeçalho abre o menu (o MESMO dispatch do main — por
            // modo: elemento de UI em modo UI, preset de TIC no 3D)
            if (drawHierarchy(ui, scene, st)) {
                st.plusMenu = true;
                st.fileMenu = false;
            }
            if (st.uiMode && st.selElement >= 0) {
                drawUiInspector(ui, scene, st, input);
            } else {
                drawInspector(ui, scene, st, nullptr);
            }
        }
        if (st.plusMenu) {
            const int choice = drawPlusMenu(ui, input, kSW, kSH, st);
            if (choice > 0) {
                if (st.uiMode) {
                    // 0.7.4 — o MESMO mapa do main (1..7 elementos, 8
                    // joystick, 9/10 VBox/HBox)
                    const int kind = uiPlusChoiceKind(choice);
                    if (kind >= 0) {
                        uiAddElement(scene, st, static_cast<u32>(kind), kSW, kSH);
                    }
                } else if (choice == 5) {
                    // 0.7.7 — o MESMO dispatch do main: o 5º item do 3D é o
                    // TIC de CÂMARA (Transform3D + CameraComp, nasce A ativa)
                    const Handle hc = scene.create("Camera");
                    if (Tic* ct = scene.get(hc)) {
                        ct->addComponent<Transform3D>();
                        if (ct->addComponent<CameraComp>()) {
                            setOnlyActiveCamera(scene, hc);
                        }
                        st.selected = hc;
                    }
                } else {
                    createTicFromPreset(scene,
                                        static_cast<PresetKind>(choice - 1),
                                        nullptr, nullptr);
                }
            }
        }
        if (st.fileMenu) {
            drawFileMenu(ui, input, kSW, kSH, st);   // 0.7.5: modal com backdrop
        }
        // 0.7.0 — o MESMO dispatch do main: menu contextual + remoção com
        // confirmação + teclado (a sequência real do frame do device)
        if (st.contextMenu) {
            Tic* ct = scene.get(st.contextTic);
            if (!ct) {
                st.contextMenu = false;
            } else {
                const int ch = drawContextMenu(ui, input, kSW, kSH, st,
                                               ct->name.c_str(), ct->visible);
                if (ch == 1) {
                    openTextInput(st, 0, st.contextTic, -1, ct->name.c_str());
                } else if (ch == 2) {
                    st.removeDialog = true;
                } else if (ch == 3) {
                    const Handle dup = duplicateTic(scene, st.contextTic);
                    if (dup.valid()) {
                        st.selected = dup;
                    }
                } else if (ch == 4) {
                    ct->visible = !ct->visible;
                }
            }
        }
        if (st.removeDialog) {
            Tic* ct = scene.get(st.contextTic);
            if (!ct) {
                st.removeDialog = false;
            } else {
                const int ch = drawRemoveDialog(ui, input, kSW, kSH, st,
                                                ct->name.c_str());
                if (ch == 1) {
                    scene.destroy(st.contextTic);
                    if (st.selected == st.contextTic) {
                        st.selected = Handle::invalid();
                        st.selElement = -1;
                    }
                }
            }
        }
        if (st.textInput) {
            drawTextInput(ui, input, kSW, kSH, st, "TECLADO");
        }
        ui.statusLine("status");
        ui.endFrame();
        input.clearEdges();
    }

    // tap completo (down → frame → up → frame): o gesto das widgets
    void tap(f32 x, f32 y) {
        input.injectDown(0, x, y);
        frame();
        input.injectUp(0);
        frame();
    }
    void dragFrom(f32 x0, f32 y0, f32 x1, f32 y1) {
        input.injectDown(0, x0, y0);
        frame();
        input.injectMove(0, x1, y1);
        frame();
        input.injectUp(0);
        frame();
    }
};

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
bool rectInside(const Rect& r, const Rect& outer, f32 eps = 0.5f) {
    return r.x0 >= outer.x0 - eps && r.y0 >= outer.y0 - eps &&
           r.x1 <= outer.x1 + eps && r.y1 <= outer.y1 + eps;
}

} // namespace

// ---- 1. separador "3D | UI" ------------------------------------------------------

TEST(uieditor_toggle_abre_viewport_2d_dedicado) {
    Env e;
    EXPECT(e.ok);
    EXPECT(!e.st.uiMode);
    // 0.7.6 — o segmented 3D|UI é o G3 da barra final (toolbar::layout)
    const toolbar::Layout L =
        toolbar::layout(kSW, kSH, safe::Insets{}, false);
    // tap no segmento "UI"
    e.tap(L.modeUi.x + L.modeUi.w * 0.5f, L.modeUi.y + L.modeUi.h * 0.5f);
    EXPECT(e.st.uiMode);
    // e volta ao 3D
    e.tap(L.mode3d.x + L.mode3d.w * 0.5f, L.mode3d.y + L.mode3d.h * 0.5f);
    EXPECT(!e.st.uiMode);
}

TEST(uieditor_viewport_2d_nunca_corta_o_canvas) {
    // scale-to-fit: o espaço de design (1600x720) cabe INTEIRO no viewport
    // central (1000x592 no layout 1600x720) — sem cortes, sem sobreposição
    const UiRect view = safe::centerRect(kSW, kSH, safe::Insets{});
    const ViewportTransform t = uiViewportTransform(view, kSW, kSH);
    const Rect design{t.ox, t.oy, t.ox + kSW * t.scale, t.oy + kSH * t.scale};
    EXPECT(rectInside(design, {view.x, view.y, view.x + view.w, view.y + view.h}));
    // escala = a que CABE (limitada pela largura neste caso)
    EXPECT(nearEqF(t.scale, view.w / kSW, 1e-3f));
    // idem num ecrã mais estreito (1280x720)
    const UiRect view2 = safe::centerRect(1280.0f, kSH, safe::Insets{});
    const ViewportTransform t2 = uiViewportTransform(view2, kSW, kSH);
    const Rect d2{t2.ox, t2.oy, t2.ox + kSW * t2.scale, t2.oy + kSH * t2.scale};
    EXPECT(rectInside(d2, {view2.x, view2.y, view2.x + view2.w, view2.y + view2.h}));
}

TEST(uieditor_viewport_bg_no_centro_sem_sobrepor_paineis) {
    Env e;
    EXPECT(e.ok);
    e.st.uiMode = true;
    e.frame();
    // o bg do viewport 2D ocupa EXATAMENTE o centerRect (o mesmo da câmara)
    const UiRect view = safe::centerRect(kSW, kSH, safe::Insets{});
    std::vector<Rect> rects;
    collectRects(e.ui.solidsForTest(), rects);
    bool found = false;
    for (const Rect& r : rects) {
        if (std::fabs(r.x0 - view.x) < 0.5f && std::fabs(r.y0 - view.y) < 0.5f &&
            std::fabs(r.x1 - (view.x + view.w)) < 0.5f &&
            std::fabs(r.y1 - (view.y + view.h)) < 0.5f) {
            found = true;
        }
    }
    EXPECT(found);
    // e não invade os painéis: hierarchy [0..300], inspector [1300..1600]
    for (const Rect& r : rects) {
        if (std::fabs(r.x0 - view.x) < 0.5f && std::fabs(r.y0 - view.y) < 0.5f &&
            std::fabs(r.x1 - (view.x + view.w)) < 0.5f) {
            EXPECT(!rectsOverlap(r, {0.0f, 88.0f, 299.0f, 680.0f}));
            EXPECT(!rectsOverlap(r, {1301.0f, 88.0f, 1600.0f, 680.0f}));
        }
    }
}

// ---- 2. WYSIWYG: tap seleciona, drag move, vazio desseleciona ---------------------

TEST(uieditor_wysiwyg_tap_seleciona_drag_move_vazio_desseleciona) {
    Env e;
    EXPECT(e.ok);
    UiCanvas* c = e.canvas();
    if (!c) {
        EXPECT(!"canvas não criado no Env");
        return;
    }
    // um botão em (400..640, 200..320) do espaço de design
    UiElement btn;
    btn.kind = UiElement::Kind::Button;
    btn.name = "btn";
    btn.text = "OK";
    btn.anchorH = UiElement::AnchorH::Left;
    btn.anchorV = UiElement::AnchorV::Top;
    btn.ox = 400.0f; btn.oy = 200.0f; btn.w = 240.0f; btn.h = 120.0f;
    c->elements.push_back(btn);

    e.st.uiMode = true;
    e.frame();

    const UiRect view = safe::centerRect(kSW, kSH, safe::Insets{});
    const ViewportTransform t = uiViewportTransform(view, kSW, kSH);
    const Rect onScreen{t.ox + 400.0f * t.scale, t.oy + 200.0f * t.scale,
                        t.ox + 640.0f * t.scale, t.oy + 320.0f * t.scale};

    // tap no CENTRO do elemento (coords de ecrã) → seleciona
    e.tap((onScreen.x0 + onScreen.x1) * 0.5f, (onScreen.y0 + onScreen.y1) * 0.5f);
    EXPECT(e.st.selElement == 0);

    // drag de (+80, +40) px de ecrã → ox/oy movem pelo delta em DESIGN px
    const f32 ox0 = c->elements[0].ox;
    const f32 oy0 = c->elements[0].oy;
    e.dragFrom((onScreen.x0 + onScreen.x1) * 0.5f, (onScreen.y0 + onScreen.y1) * 0.5f,
               (onScreen.x0 + onScreen.x1) * 0.5f + 80.0f,
               (onScreen.y0 + onScreen.y1) * 0.5f + 40.0f);
    EXPECT(nearEqF(c->elements[0].ox - ox0, 80.0f / t.scale, 0.5f));
    EXPECT(nearEqF(c->elements[0].oy - oy0, 40.0f / t.scale, 0.5f));

    // tap no VAZIO do viewport → desseleciona o elemento
    e.tap(view.x + view.w * 0.5f, view.y + 30.0f);   // fora do elemento
    EXPECT(e.st.selElement == -1);
}

TEST(uieditor_wysiwyg_o_de_cima_ganha) {
    Env e;
    EXPECT(e.ok);
    UiCanvas* c = e.canvas();
    if (!c) {
        EXPECT(!"canvas ausente");
        return;
    }
    UiElement a;
    a.kind = UiElement::Kind::Panel; a.name = "baixo";
    a.ox = 100.0f; a.oy = 100.0f; a.w = 400.0f; a.h = 300.0f;
    c->elements.push_back(a);
    UiElement b;
    b.kind = UiElement::Kind::Panel; b.name = "cima";
    b.ox = 100.0f; b.oy = 100.0f; b.w = 400.0f; b.h = 300.0f;
    c->elements.push_back(b);

    e.st.uiMode = true;
    e.frame();
    const UiRect view = safe::centerRect(kSW, kSH, safe::Insets{});
    const ViewportTransform t = uiViewportTransform(view, kSW, kSH);
    e.tap(t.ox + 200.0f * t.scale, t.oy + 200.0f * t.scale);
    EXPECT(e.st.selElement == 1);   // o ÚLTIMO desenhado (por cima)
}

// ---- 3. Inspector de UI -----------------------------------------------------------

TEST(uieditor_inspector_ui_sem_sobreposicao_e_dentro_do_painel) {
    Env e;
    EXPECT(e.ok);
    UiCanvas* c = e.canvas();
    if (!c) {
        EXPECT(!"canvas ausente");
        return;
    }
    UiElement btn;
    btn.kind = UiElement::Kind::Button;
    btn.text = "OK";
    btn.action = UiElement::Action::TogglePanel;   // com alvo → linha extra
    btn.target = "fundo";
    c->elements.push_back(btn);
    e.st.uiMode = true;
    e.st.selElement = 0;
    e.frame();

    // o plano tem as linhas esperadas (nome, x, y, w, h, R, G, B, FUNDO A,
    // visivel, ancoraH, ancoraV, texto, acao, alvo, TEX, COLOCAR EM, remover)
    const TextMetrics tm = e.ui.textMetrics();
    UiInspRow plan[24];
    const u32 n = uiInspectorPlan(c->elements[0], tm, plan, 24);
    EXPECT(n == 18u);   // 0.7.4: + fundo A + tex: + colocar em
    // y CUMULATIVO estrito: nenhuma linha invade a anterior
    for (u32 i = 1; i < n; ++i) {
        EXPECT(plan[i].y >= plan[i - 1].y + plan[i - 1].h - 0.01f);
    }
    // altura do conteúdo = fundo da última
    EXPECT(nearEqF(uiInspectorContentHeight(c->elements[0], tm),
                   plan[n - 1].y + plan[n - 1].h, 0.01f));
}

TEST(uieditor_inspector_ui_sliders_escrevem_pos_size_cor) {
    Env e;
    EXPECT(e.ok);
    UiCanvas* c = e.canvas();
    if (!c) {
        EXPECT(!"canvas ausente");
        return;
    }
    UiElement lbl;
    lbl.kind = UiElement::Kind::Label;
    lbl.name = "lbl";
    c->elements.push_back(lbl);
    e.st.uiMode = true;
    e.st.selElement = 0;
    e.frame();

    // os sliders vivem no painel direito: trilho em x+84, largura 118
    const UiRect panel = safe::inspectorPanelRect(kSW, kSH, safe::Insets{});
    const TextMetrics tm = e.ui.textMetrics();
    UiInspRow plan[24];
    const u32 n = uiInspectorPlan(c->elements[0], tm, plan, 24);
    auto rowRect = [&](u64 id) -> Rect {
        for (u32 i = 0; i < n; ++i) {
            if (plan[i].id == id) {
                const f32 ry = panel.y + editor::kHeaderH + 4.0f + plan[i].y;
                return {panel.x, ry, panel.x + panel.w, ry + plan[i].h};
            }
        }
        return {0, 0, 0, 0};
    };

    // largura (SizeW): slider 8..3000 — tocar a 75% do trilho
    Rect rw = rowRect(kUiInspW);
    const f32 trackX = panel.x + 84.0f;
    const f32 trackW = 118.0f;
    const f32 yw = (rw.y0 + rw.y1) * 0.5f;
    e.dragFrom(trackX + trackW * 0.75f, yw, trackX + trackW * 0.75f, yw);
    EXPECT(c->elements[0].w > 1500.0f);   // ~75% da faixa 8..3000
    EXPECT(c->elements[0].w < 3000.0f);

    // cor R: slider 0..1 a 20%
    Rect rr = rowRect(kUiInspR);
    const f32 yr = (rr.y0 + rr.y1) * 0.5f;
    e.dragFrom(trackX + trackW * 0.2f, yr, trackX + trackW * 0.2f, yr);
    EXPECT(c->elements[0].color[0] > 0.15f);
    EXPECT(c->elements[0].color[0] < 0.30f);
}

TEST(uieditor_inspector_ui_botoes_visivel_ancoras_acao_remover) {
    Env e;
    EXPECT(e.ok);
    UiCanvas* c = e.canvas();
    if (!c) {
        EXPECT(!"canvas ausente");
        return;
    }
    UiElement btn;
    btn.kind = UiElement::Kind::Button;
    btn.name = "btn";
    btn.text = "OK";
    btn.ox = 100.0f; btn.oy = 100.0f; btn.w = 240.0f; btn.h = 80.0f;
    c->elements.push_back(btn);
    e.st.uiMode = true;
    e.st.selElement = 0;
    e.frame();

    const UiRect panel = safe::inspectorPanelRect(kSW, kSH, safe::Insets{});
    // tapRow recalcula o PLANO a cada chamada (linhas mudam quando a ação
    // ganha/perde o ALVO — um plano obsoleto apontaria a linha errada) e
    // FAZ SCROLL até à linha (0.7.4: o plano cresceu — fundo A/tex:/colocar
    // em — e as últimas linhas podem estar abaixo da dobra)
    auto tapRow = [&](u64 id) {
        UiInspRow fresh[24];
        const u32 fn = uiInspectorPlan(c->elements[0], e.ui.textMetrics(),
                                       fresh, 24);
        for (u32 i = 0; i < fn; ++i) {
            if (fresh[i].id == id) {
                // offset CLAMPADO (o beginScroll do próximo frame faz o
                // mesmo clamp — sem isto a linha desenhada fica mais abaixo
                // do que a matemática prevê)
                const f32 contentH =
                    uiInspectorContentHeight(c->elements[0], e.ui.textMetrics());
                const f32 visibleH = panel.h - editor::kHeaderH - 4.0f;
                f32 off = fresh[i].y;
                if (off > contentH - visibleH) {
                    off = contentH - visibleH;
                }
                if (off < 0.0f) {
                    off = 0.0f;
                }
                e.ui.scrollSetOffset(kUiInspScrollId, off);
                const f32 ry = panel.y + editor::kHeaderH + 4.0f +
                               fresh[i].y + fresh[i].h * 0.5f - off;
                e.tap(panel.x + panel.w * 0.5f, ry);
                return;
            }
        }
    };

    // visível: toggle
    EXPECT(c->elements[0].visible);
    tapRow(kUiInspVis);
    EXPECT(!c->elements[0].visible);

    // âncora H cicla (a POSIÇÃO absoluta é preservada pelo setAnchor)
    const f32 x0 = ui::elementRect(c->elements[0], kSW, kSH, safe::Insets{}).x;
    EXPECT(c->elements[0].anchorH == UiElement::AnchorH::Left);
    tapRow(kUiInspAnchH);
    EXPECT(c->elements[0].anchorH == UiElement::AnchorH::Center);
    EXPECT(nearEqF(ui::elementRect(c->elements[0], kSW, kSH, safe::Insets{}).x,
                   x0, 0.5f));

    // ação cicla none → show
    EXPECT(c->elements[0].action == UiElement::Action::None);
    tapRow(kUiInspAct);
    EXPECT(c->elements[0].action == UiElement::Action::ShowPanel);

    // texto abre o teclado in-app
    EXPECT(!e.st.textInput);
    tapRow(kUiInspText);
    EXPECT(e.st.textInput);
    EXPECT(e.st.textPurpose == 2);
    // cancelar fecha sem aplicar
    const KeyboardLayout kb = keyboardLayout(kSW, kSH, safe::Insets{});
    e.tap((kb.cancel.x + kb.cancel.w * 0.5f), (kb.cancel.y + kb.cancel.h * 0.5f));
    EXPECT(!e.st.textInput);
    EXPECT(c->elements[0].text == "OK");   // intacto

    // remover apaga o elemento e desseleciona
    tapRow(kUiInspRemove);
    EXPECT(c->elements.empty());
    EXPECT(e.st.selElement == -1);
}

// ---- 4. teclado in-app ---------------------------------------------------------------

TEST(uieditor_teclado_geometria_sem_sobreposicao_na_safe_area) {
    const safe::Insets ins{24.0f, 40.0f, 8.0f, 32.0f};
    const KeyboardLayout kb = keyboardLayout(kSW, kSH, ins);
    const Rect area{ins.left, ins.top, kSW - ins.right, kSH - ins.bottom};

    // o diálogo INTEIRO vive dentro da safe-area
    EXPECT(rectInside({kb.dialog.x, kb.dialog.y,
                       kb.dialog.x + kb.dialog.w, kb.dialog.y + kb.dialog.h},
                      area));

    // linha de baixo (declaração ANTES dos cruzamentos com as teclas).
    // 0.7.5: + a tecla de CASO abc/ABC (o espaço encolheu de 3u para 2u)
    const Rect bottom[6] = {
        {kb.space.x, kb.space.y, kb.space.x + kb.space.w, kb.space.y + kb.space.h},
        {kb.caseKey.x, kb.caseKey.y, kb.caseKey.x + kb.caseKey.w,
         kb.caseKey.y + kb.caseKey.h},
        {kb.dash.x, kb.dash.y, kb.dash.x + kb.dash.w, kb.dash.y + kb.dash.h},
        {kb.back.x, kb.back.y, kb.back.x + kb.back.w, kb.back.y + kb.back.h},
        {kb.ok.x, kb.ok.y, kb.ok.x + kb.ok.w, kb.ok.y + kb.ok.h},
        {kb.cancel.x, kb.cancel.y, kb.cancel.x + kb.cancel.w,
         kb.cancel.y + kb.cancel.h},
    };

    // as teclas NÃO se sobrepõem entre si (linhas completas) — nem ENTRE
    // linhas (regressão do bug da 1ª versão: a linha 3 sobrepunha o
    // CANCELAR e a tecla "8" roubava o toque do botão)
    for (u32 row = 0; row < 4; ++row) {
        for (u32 a = 0; a < kb.keyCount[row]; ++a) {
            for (u32 b = a + 1; b < kb.keyCount[row]; ++b) {
                const Rect ra{kb.key[row][a].x, kb.key[row][a].y,
                              kb.key[row][a].x + kb.key[row][a].w,
                              kb.key[row][a].y + kb.key[row][a].h};
                const Rect rb{kb.key[row][b].x, kb.key[row][b].y,
                              kb.key[row][b].x + kb.key[row][b].w,
                              kb.key[row][b].y + kb.key[row][b].h};
                EXPECT(!rectsOverlap(ra, rb));
            }
            // tecla vs TODAS as teclas das linhas SEGUINTES
            for (u32 row2 = row + 1; row2 < 4; ++row2) {
                for (u32 b = 0; b < kb.keyCount[row2]; ++b) {
                    const Rect ra{kb.key[row][a].x, kb.key[row][a].y,
                                  kb.key[row][a].x + kb.key[row][a].w,
                                  kb.key[row][a].y + kb.key[row][a].h};
                    const Rect rb{kb.key[row2][b].x, kb.key[row2][b].y,
                                  kb.key[row2][b].x + kb.key[row2][b].w,
                                  kb.key[row2][b].y + kb.key[row2][b].h};
                    EXPECT(!rectsOverlap(ra, rb));
                }
            }
            // tecla vs linha de BAIXO (o bug da 1ª versão)
            const Rect ra{kb.key[row][a].x, kb.key[row][a].y,
                          kb.key[row][a].x + kb.key[row][a].w,
                          kb.key[row][a].y + kb.key[row][a].h};
            for (const Rect& b : bottom) {
                EXPECT(!rectsOverlap(ra, b));
            }
        }
    }
    // linha de baixo também exclusiva entre si (6 teclas — 0.7.5: + abc/ABC)
    for (u32 a = 0; a < 6; ++a) {
        for (u32 b = a + 1; b < 6; ++b) {
            EXPECT(!rectsOverlap(bottom[a], bottom[b]));
        }
    }
    // rótulos: A..I na linha 0, 0..9 na linha 3; 0.7.5: MINÚSCULAS com o
    // toggle (abc/ABC) — dígitos/'_' não mudam de caso
    EXPECT(std::string(KeyboardLayout::keyLabel(0, 0)) == "A");
    EXPECT(std::string(KeyboardLayout::keyLabel(3, 9)) == "9");
    EXPECT(std::string(KeyboardLayout::keyLabel(0, 0, true)) == "a");
    EXPECT(std::string(KeyboardLayout::keyLabel(1, 4, true)) == "n");
    EXPECT(std::string(KeyboardLayout::keyLabel(2, 8)) == "_");   // sem caso
    EXPECT(std::string(KeyboardLayout::keyLabel(3, 0, true)) == "0");
    // filtro de caracteres válidos
    EXPECT(uiTextCharAllowed('A') && uiTextCharAllowed('z'));
    EXPECT(uiTextCharAllowed('5') && uiTextCharAllowed('_'));
    EXPECT(uiTextCharAllowed('-') && uiTextCharAllowed(' '));
    EXPECT(!uiTextCharAllowed('!') && !uiTextCharAllowed('.'));
    EXPECT(!uiTextCharAllowed('\n') && !uiTextCharAllowed('/'));
}

TEST(uieditor_teclado_digita_apaga_e_ok_renomeia) {
    Env e;
    EXPECT(e.ok);
    // abre o input de RENOMEAR para o TIC HUD
    openTextInput(e.st, 0, e.hud, -1, "HUD");
    EXPECT(e.st.textInput);
    EXPECT(std::string(e.st.textBuf) == "HUD");

    // APAGA 3× (limpa "HUD")… melhor: apaga 1 → "HU", digita "D2"
    const KeyboardLayout kb = keyboardLayout(kSW, kSH, safe::Insets{});
    auto keyTap = [&](const UiRect& r) {
        e.tap(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
    };
    keyTap(kb.back);   // "HU"
    EXPECT(std::string(e.st.textBuf) == "HU");
    // "D" = linha 0? D é a 4ª letra → row0 col3
    keyTap(kb.key[0][3]);   // "HUD"
    // "2" = linha 3 col 2
    keyTap(kb.key[3][2]);   // "HUD2"
    EXPECT(std::string(e.st.textBuf) == "HUD2");

    // OK → aplica o renomear (o MESMO fluxo do main)
    keyTap(kb.ok);
    EXPECT(!e.st.textInput);
    EXPECT(commitTextInput(e.scene, e.st));
    EXPECT(e.scene.get(e.hud)->name == "HUD2");
}

TEST(uieditor_teclado_cancel_nao_aplica_e_vazio_nao_aplica) {
    Env e;
    EXPECT(e.ok);
    openTextInput(e.st, 0, e.hud, -1, "HUD");
    const KeyboardLayout kb = keyboardLayout(kSW, kSH, safe::Insets{});
    // Cancelar → o diálogo fecha e NADA se aplica (o main SÓ chama o
    // commit no OK — o caminho do cancelar nunca chega lá)
    e.tap(kb.cancel.x + kb.cancel.w * 0.5f, kb.cancel.y + kb.cancel.h * 0.5f);
    EXPECT(!e.st.textInput);
    EXPECT(e.scene.get(e.hud)->name == "HUD");

    // buffer VAZIO no OK → NÃO aplica (nome vazio é recusado)
    openTextInput(e.st, 0, e.hud, -1, "");
    EXPECT(e.st.textLen == 0u);
    e.tap(kb.ok.x + kb.ok.w * 0.5f, kb.ok.y + kb.ok.h * 0.5f);
    EXPECT(!e.st.textInput);
    EXPECT(!commitTextInput(e.scene, e.st));
    EXPECT(e.scene.get(e.hud)->name == "HUD");

    // renomear OUTRO TIC para um nome JÁ EXISTENTE ganha sufixo único (o
    // find devolve o PRIMEIRO com o nome — o dono original — e o novo
    // recebe o sufixo Godot-style)
    Handle other = e.scene.create("OUTRO");
    openTextInput(e.st, 0, other, -1, "HUD");
    EXPECT(e.st.textTic == other);
    e.tap(kb.ok.x + kb.ok.w * 0.5f, kb.ok.y + kb.ok.h * 0.5f);
    EXPECT(commitTextInput(e.scene, e.st));
    const std::string renamed = e.scene.get(other)->name;
    EXPECT(renamed != "HUD");   // "HUD.001" (o TIC original ficou "HUD")
    EXPECT(renamed.find("HUD") == 0);
    EXPECT(e.scene.get(e.hud)->name == "HUD");
}

// ---- 5. gestão de TICs: menu contextual, remover, duplicar, visibilidade --------

TEST(uieditor_hierarquia_olho_e_dots_e_vazio) {
    Env e;
    EXPECT(e.ok);
    e.frame();

    // geometria da linha 0 (1 TIC): [nome 12..196][olho 202..242][... 248..288]
    const f32 rowCY = 88.0f + 48.0f + 26.0f;   // listTop + meia linha

    // OLHO: toggle de visibilidade imediato
    EXPECT(e.scene.get(e.hud)->visible);
    e.tap(202.0f + 20.0f, rowCY);
    EXPECT(!e.scene.get(e.hud)->visible);
    e.tap(202.0f + 20.0f, rowCY);
    EXPECT(e.scene.get(e.hud)->visible);

    // "..." abre o menu contextual
    EXPECT(!e.st.contextMenu);
    e.tap(248.0f + 20.0f, rowCY);
    EXPECT(e.st.contextMenu);
    EXPECT(e.st.contextTic == e.hud);

    // toque FORA fecha sem ação
    e.frame();
    e.tap(1200.0f, 360.0f);
    EXPECT(!e.st.contextMenu);

    // VAZIO da lista (abaixo da última linha, dentro do painel) DESSELECIONA
    EXPECT(e.st.selected.valid());
    e.tap(150.0f, 88.0f + 48.0f + 3.0f * 52.0f);   // sob a linha do único TIC
    EXPECT(!e.st.selected.valid());
}

TEST(uieditor_menu_contextual_quatro_acoes) {
    Env e;
    EXPECT(e.ok);
    // um TIC Player com componentes para o duplicar copiar
    const Handle player =
        createTicFromPreset(e.scene, PresetKind::PlayerBody3D, nullptr, nullptr);
    EXPECT(player.valid());
    e.st.contextTic = player;
    e.st.selected = player;
    e.frame();

    // choice por clique direto nos itens (o main despacha; aqui aferimos
    // cada retorno + o efeito de DUPLICAR/VISIBILIDADE que são imediatos)
    auto clickItem = [&](int item) {
        e.st.contextMenu = true;
        e.frame();
        // itens: y = menuY + kHeaderH + i*64 + 28 — menu centrado
        const f32 h = kHeaderH + 4.0f * 64.0f + kPad;
        const f32 y = (kSH - h) * 0.5f + kHeaderH + static_cast<f32>(item) * 64.0f + 28.0f;
        e.tap(800.0f, y);
    };

    // VISIBILIDADE (item 4): toggle
    clickItem(3);
    EXPECT(!e.scene.get(player)->visible);
    clickItem(3);
    EXPECT(e.scene.get(player)->visible);

    // DUPLICAR (item 3): o main chama duplicateTic — aferimos o puro:
    const u32 before = e.scene.count();
    const Handle dup = duplicateTic(e.scene, player);
    EXPECT(dup.valid());
    EXPECT(e.scene.count() == before + 1);
    Tic* d = e.scene.get(dup);
    EXPECT(d->name != e.scene.get(player)->name);   // nome único
    EXPECT(d->getComponent<Transform3D>() != nullptr);
    EXPECT(d->getComponent<MeshRenderer>() != nullptr);
    EXPECT(d->getComponent<BodyComp>() != nullptr);
    EXPECT(d->getComponent<InputMap>() != nullptr);
    // duplicar o duplicado: sufixo avança (.001 → .002)
    const Handle dup2 = duplicateTic(e.scene, dup);
    EXPECT(e.scene.get(dup2)->name != d->name);

    // RENOMEAR (item 1): abre o teclado in-app
    clickItem(0);
    EXPECT(e.st.textInput);
    EXPECT(e.st.textPurpose == 0);
    EXPECT(e.st.textTic == player);

    // REMOVER (item 2): o diálogo de CONFIRMAÇÃO abre
    clickItem(1);
    EXPECT(e.st.removeDialog);

    // Cancelar mantém o TIC
    e.frame();
    e.tap(200.0f, 360.0f);   // fora do diálogo = cancelar
    EXPECT(!e.st.removeDialog);
    EXPECT(e.scene.get(player) != nullptr);
}

TEST(uieditor_dialogo_remover_confirmar_apaga) {
    Env e;
    EXPECT(e.ok);
    Handle victim = e.scene.create("Vitima");
    e.st.contextTic = victim;
    e.st.removeDialog = true;
    e.st.selected = victim;
    e.frame();

    // botões lado a lado (mesma geometria do diálogo de armazenamento)
    const f32 h = storageDialogHeight();
    const UiRect dlg = centeredMenuRect(0.0f, 0.0f, kSW, kSH, h);
    UiRect del{}, cancel{};
    storageDialogButtons(dlg, del, cancel);

    // CONFIRMAR: o TIC morre e a seleção limpa
    e.tap(del.x + del.w * 0.5f, del.y + del.h * 0.5f);
    EXPECT(!e.st.removeDialog);
    EXPECT(e.scene.get(victim) == nullptr);
    EXPECT(!e.st.selected.valid());
    EXPECT(e.scene.count() >= 1u);   // o HUD do Env continua
}

// ---- 6. desselecionar no viewport 3D ----------------------------------------------

TEST(uieditor_desselecionar_viewport_3d_tap_nao_drag) {
    EditorState st;
    InputState in;
    st.selected = Handle{5, 1};
    const UiRect view = safe::centerRect(kSW, kSH, safe::Insets{});

    // TAP parado no viewport: press → release no mesmo sítio → desseleciona
    // (as edges valem 1 FRAME — clearEdges marca a fronteira, como no main)
    in.injectDown(0, 800.0f, 300.0f);
    EXPECT(!viewportTapClearsSelection(st, in, view, 0));   // arma, não dispara
    EXPECT(st.selected.valid());
    in.clearEdges();
    in.injectUp(0);
    EXPECT(viewportTapClearsSelection(st, in, view, 0));   // release = tap
    EXPECT(!st.selected.valid());

    // DRAG (orbit): press → move 60px → release NÃO desseleciona
    st.selected = Handle{5, 1};
    in.resetAll();
    in.injectDown(0, 800.0f, 300.0f);
    viewportTapClearsSelection(st, in, view, 0);           // frame 1: arma
    in.clearEdges();
    in.injectMove(0, 860.0f, 300.0f);
    EXPECT(!viewportTapClearsSelection(st, in, view, 0));   // frame 2: drag cancela
    EXPECT(st.selected.valid());   // o drag NÃO desseleciona
    in.clearEdges();
    in.injectUp(0);
    EXPECT(!viewportTapClearsSelection(st, in, view, 0));   // frame 3: sem arm
    EXPECT(st.selected.valid());

    // toque RECLAMADO (gizmo/controlo): não arma
    in.resetAll();
    in.injectDown(0, 800.0f, 300.0f);
    EXPECT(!viewportTapClearsSelection(st, in, view, 1u));   // slot 0 claimed
    in.clearEdges();
    in.injectUp(0);
    EXPECT(!viewportTapClearsSelection(st, in, view, 1u));
    EXPECT(st.selected.valid());

    // TAP DE 1 FRAME (press+release entre frames): arma e dispara na MESMA
    // chamada — o mesmo caso que o scroll trata no endScroll
    in.resetAll();
    st.selected = Handle{5, 1};
    in.injectDown(0, 800.0f, 300.0f);
    in.injectUp(0);
    EXPECT(viewportTapClearsSelection(st, in, view, 0));
    EXPECT(!st.selected.valid());
}

// ---- 7. cor por TIC: tint no render (stub GL) ---------------------------------------

TEST(uieditor_cor_por_tic_render_binda_o_tint) {
    glstub::reset();
    Renderer r;
    if (!r.init()) {
        EXPECT(!"Renderer::init falhou (stub)");
        return;
    }
    Mesh mesh;
    {
        const CubeMeshData cube = makeCube(1.0f);
        if (!mesh.create(cube.vertices.data(),
                         static_cast<u32>(cube.vertices.size()),
                         cube.indices.data(),
                         static_cast<u32>(cube.indices.size()))) {
            EXPECT(!"upload do mesh falhou (stub)");
            return;
        }
    }

    // o tint do MeshRenderer (sliders R/G/B do Inspector) chega ao uTint
    const f32 tint[3] = {1.0f, 0.5f, 0.25f};
    glstub::reset();
    r.drawMesh(mesh, Mat4::identity(), Mat4::identity(), nullptr, tint);
    EXPECT(glstub::stats.uniform3fCalls >= 1);
    EXPECT(nearEqF(glstub::stats.lastUniform3f[0], 1.0f));
    EXPECT(nearEqF(glstub::stats.lastUniform3f[1], 0.5f));
    EXPECT(nearEqF(glstub::stats.lastUniform3f[2], 0.25f));

    // SEM tint (TICs antigos / sliders no branco) = branco — o 0.6.x
    glstub::reset();
    r.drawMesh(mesh, Mat4::identity(), Mat4::identity(), nullptr, nullptr);
    EXPECT(nearEqF(glstub::stats.lastUniform3f[0], 1.0f));
    EXPECT(nearEqF(glstub::stats.lastUniform3f[1], 1.0f));
    EXPECT(nearEqF(glstub::stats.lastUniform3f[2], 1.0f));
}

TEST(uieditor_inspector_tic_mostra_visivel_e_cor) {
    // o plano do Inspector de TICs ganhou as linhas NOVAS (visivel + R/G/B)
    Env e;
    EXPECT(e.ok);
    const Handle player =
        createTicFromPreset(e.scene, PresetKind::PlayerBody3D, nullptr, nullptr);
    e.st.selected = player;
    e.frame();   // desenha o Inspector de TICs (modo 3D)

    const TextMetrics tm = e.ui.textMetrics();
    const Tic* t = e.scene.get(player);
    const InspProfile prof = inspectorProfile(*t);
    EXPECT(prof.mr);   // preset Player tem MeshRenderer → linhas de cor
    InspRow plan[32];
    const u32 n = inspectorPlan(prof, tm, false, plan);
    u32 visRows = 0, colorRows = 0;
    for (u32 i = 0; i < n; ++i) {
        if (plan[i].kind == InspRow::Kind::VisToggle) {
            ++visRows;
            EXPECT(plan[i].id == kInspectorVis);
        }
        if (plan[i].kind == InspRow::Kind::ColorSlider) {
            ++colorRows;
        }
    }
    EXPECT(visRows == 1u);
    EXPECT(colorRows == 3u);
    // y cumulativo intacto (o contrato F5.0-fix continua)
    for (u32 i = 1; i < n; ++i) {
        EXPECT(plan[i].y >= plan[i - 1].y + plan[i - 1].h - 0.01f);
    }
}

// ---- 8. criação de elementos (+ no modo UI) ------------------------------------------

TEST(uieditor_plus_modo_ui_cria_elementos_no_canvas) {
    Env e;
    EXPECT(e.ok);
    e.st.uiMode = true;
    e.frame();
    // "+" da Hierarchy abre o menu (no modo UI: CRIAR ELEMENTO UI)
    e.tap(300.0f - 12.0f - 28.0f, 88.0f + 24.0f);   // botão + do cabeçalho
    EXPECT(e.st.plusMenu);
    // 0.7.4: 10 itens (Panel/Label/Button/Image/Menu/Card/Article/Joystick/
    // VBox/HBox)
    const f32 h = kHeaderH + 10.0f * 64.0f + kPad;
    const f32 y = (kSH - h) * 0.5f + kHeaderH + 2.0f * 64.0f + 28.0f;   // Button
    e.tap(800.0f, y);
    EXPECT(!e.st.plusMenu);
    UiCanvas* c = e.canvas();
    EXPECT(c != nullptr && c->elements.size() == 1);
    EXPECT(c->elements[0].kind == UiElement::Kind::Button);
    EXPECT(e.st.selElement == 0);   // selecionado p/ WYSIWYG imediato

    // 0.7.5 — SEM TIC selecionado (em modo UI): o uiAddElement ASSEGURA/
    // CRIA o TIC DE UI próprio ("UI", só com UiCanvas) e cria o elemento
    // — criar UI nunca obrigou a um TIC 3D (o contrato novo do C33)
    e.st.selected = Handle::invalid();
    e.st.selElement = -1;
    e.st.uiMode = true;
    EXPECT(uiAddElement(e.scene, e.st, 0, kSW, kSH));
    const Handle hui = e.scene.find("UI");
    EXPECT(hui.valid());
    Tic* tu = e.scene.get(hui);
    EXPECT(tu != nullptr);
    EXPECT(tu->getComponent<UiCanvas>() != nullptr);   // SÓ com canvas
    EXPECT(tu->getComponent<MeshRenderer>() == nullptr);   // sem mesh
    EXPECT(tu->getComponent<BodyComp>() == nullptr);        // sem body
    EXPECT(e.st.selected == hui);   // ficou selecionado (Hierarchy vê-o)
    const u32 nTics = e.scene.count();
    EXPECT(uiAddElement(e.scene, e.st, 2, kSW, kSH));   // reutiliza o mesmo
    EXPECT(e.scene.count() == nTics);   // nenhum "UI.001"
    EXPECT(e.scene.get(hui)->getComponent<UiCanvas>()->elements.size() == 2u);
}

TEST(uieditor_plus_cria_canvas_a_primeira_vez) {
    Env e;
    EXPECT(e.ok);
    // TIC SEM canvas: o + do modo UI cria o canvas e o 1º elemento
    const Handle t = createTicFromPreset(e.scene, PresetKind::StaticBody3D,
                                         nullptr, nullptr);
    e.st.selected = t;
    e.st.uiMode = true;
    EXPECT(e.scene.get(t)->getComponent<UiCanvas>() == nullptr);
    EXPECT(uiAddElement(e.scene, e.st, 0, kSW, kSH));   // Panel
    UiCanvas* c = e.scene.get(t)->getComponent<UiCanvas>();
    EXPECT(c != nullptr && c->elements.size() == 1);
    EXPECT(c->elements[0].kind == UiElement::Kind::Panel);
}

// ---- 9. (0.7.6) a toolbar sem sobreposição passou ao test_toolbar.cpp
// (barra final de 5 grupos com layout dinâmico — o caso 3 botões morreu)

// ---- 10. 0.7.5 — overlays modais TAPAM o canvas (z-order) ---------------------------

TEST(uieditor_overlay_modal_tapa_o_canvas) {
    Env e;
    EXPECT(e.ok);
    UiCanvas* c = e.canvas();
    if (!c) {
        EXPECT(!"canvas ausente");
        return;
    }
    // um elemento BEM VISÍVEL no centro do ecrã de design (o caso do C33:
    // "TESTE"/"Botao" desenhados ATRAVÉS do MENU)
    UiElement lbl;
    lbl.kind = UiElement::Kind::Label;
    lbl.name = "lbl";
    lbl.text = "TESTE";
    lbl.color[3] = 1.0f;   // com fundo (quad sólido identificável)
    lbl.ox = 600.0f; lbl.oy = 300.0f; lbl.w = 300.0f; lbl.h = 80.0f;
    lbl.anchorH = UiElement::AnchorH::Left;
    lbl.anchorV = UiElement::AnchorV::Top;
    c->elements.push_back(lbl);
    e.st.uiMode = true;
    e.st.selElement = -1;

    // sem modal: o quad do label ESTÁ lá (sanidade)
    e.frame();
    {
        std::vector<Rect> rects;
        collectRects(e.ui.solidsForTest(), rects);
        const UiRect view = safe::centerRect(kSW, kSH, safe::Insets{});
        const ViewportTransform t = uiViewportTransform(view, kSW, kSH);
        const Rect want{t.ox + 600.0f * t.scale, t.oy + 300.0f * t.scale,
                        t.ox + 900.0f * t.scale, t.oy + 380.0f * t.scale};
        bool found = false;
        for (const Rect& r : rects) {
            if (std::fabs(r.x0 - want.x0) < 1.0f &&
                std::fabs(r.y0 - want.y0) < 1.0f &&
                std::fabs(r.x1 - want.x1) < 1.0f &&
                std::fabs(r.y1 - want.y1) < 1.0f) {
                found = true;
            }
        }
        EXPECT(found);
    }

    // com o MENU DE FICHEIROS aberto: o chrome NÃO se desenha (o canvas não
    // desenha) e há um BACKDROP OPACO que tapa o ecrã TODO — nenhum quad
    // do elemento é visível por cima do overlay
    e.st.fileMenu = true;
    e.frame();
    {
        std::vector<Rect> rects;
        collectRects(e.ui.solidsForTest(), rects);
        // o backdrop opaco cobre o ECRÃ INTEIRO (0,0,SW,SH)
        bool backdrop = false;
        for (const Rect& r : rects) {
            if (r.x0 <= 0.5f && r.y0 <= 0.5f && r.x1 >= kSW - 0.5f &&
                r.y1 >= kSH - 0.5f) {
                backdrop = true;
            }
        }
        EXPECT(backdrop);
        // NENHUM quad do elemento do canvas (transformado) existe
        const UiRect view = safe::centerRect(kSW, kSH, safe::Insets{});
        const ViewportTransform t = uiViewportTransform(view, kSW, kSH);
        const Rect want{t.ox + 600.0f * t.scale, t.oy + 300.0f * t.scale,
                        t.ox + 900.0f * t.scale, t.oy + 380.0f * t.scale};
        for (const Rect& r : rects) {
            EXPECT(!(std::fabs(r.x0 - want.x0) < 1.0f &&
                     std::fabs(r.y0 - want.y0) < 1.0f));
        }
    }
    // o mesmo com o MENU CONTEXTUAL aberto (o segundo caso do C33)
    e.st.fileMenu = false;
    e.st.contextMenu = true;
    e.st.contextTic = e.hud;
    e.frame();
    {
        std::vector<Rect> rects;
        collectRects(e.ui.solidsForTest(), rects);
        bool backdrop = false;
        for (const Rect& r : rects) {
            if (r.x0 <= 0.5f && r.y0 <= 0.5f && r.x1 >= kSW - 0.5f &&
                r.y1 >= kSH - 0.5f) {
                backdrop = true;
            }
        }
        EXPECT(backdrop);
    }
    // fechar o menu devolve o editor inteiro (chrome + canvas de volta)
    e.st.contextMenu = false;
    e.frame();
    EXPECT(!anyOverlayOpen(e.st));
    {
        std::vector<Rect> rects;
        collectRects(e.ui.solidsForTest(), rects);
        const UiRect view = safe::centerRect(kSW, kSH, safe::Insets{});
        const ViewportTransform t = uiViewportTransform(view, kSW, kSH);
        const Rect want{t.ox + 600.0f * t.scale, t.oy + 300.0f * t.scale,
                        t.ox + 900.0f * t.scale, t.oy + 380.0f * t.scale};
        bool found = false;
        for (const Rect& r : rects) {
            if (std::fabs(r.x0 - want.x0) < 1.0f &&
                std::fabs(r.y0 - want.y0) < 1.0f &&
                std::fabs(r.x1 - want.x1) < 1.0f &&
                std::fabs(r.y1 - want.y1) < 1.0f) {
                found = true;
            }
        }
        EXPECT(found);
    }
}

// ---- 11. 0.7.5 — teclado escreve MINÚSCULAS após o toggle abc/ABC -------------------

TEST(uieditor_teclado_minusculas_apos_toggle) {
    Env e;
    EXPECT(e.ok);
    // abre o input de RENOMEAR (buffer vazio)
    openTextInput(e.st, 0, e.hud, -1, "");
    EXPECT(e.st.textInput);
    EXPECT(!e.st.kbLower);   // nasce em MAIÚSCULAS (0.7.3-compat)

    const KeyboardLayout kb = keyboardLayout(kSW, kSH, safe::Insets{});
    auto keyTap = [&](const UiRect& r) {
        e.tap(r.x + r.w * 0.5f, r.y + r.h * 0.5f);
    };

    // maiúsculas por default: 'A' (linha 0, col 0)
    keyTap(kb.key[0][0]);
    EXPECT(std::string(e.st.textBuf) == "A");

    // toggle abc/ABC → minúsculas: 'k' (linha 1 = J..R, col 1)
    keyTap(kb.caseKey);
    EXPECT(e.st.kbLower);
    keyTap(kb.key[1][1]);
    EXPECT(std::string(e.st.textBuf) == "Ak");

    // mantém minúsculas até voltar a alternar; dígitos não mudam de caso
    keyTap(kb.key[3][5]);
    EXPECT(std::string(e.st.textBuf) == "Ak5");

    // volta a MAIÚSCULAS e continua a escrever ('B' = linha 0, col 1)
    keyTap(kb.caseKey);
    EXPECT(!e.st.kbLower);
    keyTap(kb.key[0][1]);
    EXPECT(std::string(e.st.textBuf) == "Ak5B");

    // o commit aplica minúsculas sem filtro (uiTextCharAllowed já aceitava)
    EXPECT(uiTextCharAllowed('a') && uiTextCharAllowed('Z'));
}
