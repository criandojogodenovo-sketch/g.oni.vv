// ui/UiEditor.cpp — editor de UI dedicado (0.7.0): viewport 2D WYSIWYG,
// Inspector de elemento, teclado in-app, menu contextual e diálogos.
//
// Contratos de layout (o critério transversal "nada sobreposto nem
// desorganizado" é aferido no CI):
//   • o viewport 2D nunca corta o canvas (scale-to-fit) nem sobrepõe os
//     painéis (usa o MESMO centerRect da câmara 3D);
//   • o plano do Inspector de UI tem y CUMULATIVO (o padrão F5.0-fix) e os
//     botões em scroll são re-despachados POR ID (o padrão F4.1);
//   • o teclado tem rects partilhados com os testes (keyboardLayout —
//     FONTE ÚNICA) e vive DENTRO da safe-area.
#include "ui/UiEditor.h"
#include <cstdio>
#include <cstring>
#include <string>
#include "components/BodyComp.h"
#include "components/InputMap.h"
#include "components/MeshRenderer.h"
#include "components/TouchControls.h"
#include "components/Transform3D.h"
#include "core/Scene.h"
#include "platform/InputState.h"
#include "ui/EditorUi.h"
#include "ui/UiContext.h"
#include "ui/UiRuntime.h"

namespace vv {
namespace editor {

namespace {
// (kPad/kHeaderH/kRowH/kMenuW vêm de ui/EditorLayout.h — a fonte única)

// algum overlay está aberto? (o gesto do viewport 2D NÃO corre por baixo de
// overlays — o +/menu/teclado capturam os toques primeiro)
bool anyOverlayOpen(const EditorState& st) {
    return st.plusMenu || st.fileMenu || st.settingsMenu || st.contextMenu ||
           st.removeDialog || st.textInput || st.assetMenu != 0 ||
           st.importMenu || st.storageDialog || st.logViewer;
}

// linha de slider do Inspector de UI (o mesmo desenho do Inspector de TICs:
// label à esquerda + trilho + valor à direita; baseline pelas métricas REAIS)
bool uiSliderRow(UiContext& ui, u64 id, f32 x, f32 rowTop, f32 rowH,
                 const TextMetrics& tm, const char* labelText, f32 minV,
                 f32 maxV, f32& value, const char* fmt) {
    const f32 baseline = inspBaseline(rowTop, rowH, tm);
    ui.labelFitted(x + kPad, baseline, labelText, theme::TEXT, 84.0f - kPad - 6.0f);
    const bool changed =
        ui.slider(id, x + 84.0f, rowTop, 118.0f, rowH, minV, maxV, value);
    char val[24];
    std::snprintf(val, sizeof(val), fmt, value);
    if (ui.hasFont()) {
        const f32 tw = ui.fontWidth(val);
        ui.label(x + kPanelW - kPad - tw, baseline, val, theme::TEXT);
    }
    return changed;
}
} // namespace

// ---------------------------------------------------------------------------
// VIEWPORT 2D — o modo UI do separador "3D | UI"
// ---------------------------------------------------------------------------

void drawUiViewport(UiContext& ui, Scene& scene, EditorState& st,
                    const InputState& in, f32 sw, f32 sh) {
    const UiRect view = safe::centerRect(sw, sh, ui.safeArea());
    // fundo dedicado (o modo UI NÃO desenha a cena 3D — viewport só da UI)
    ui.panel(view.x, view.y, view.w, view.h, theme::BG);
    ui.frame(view.x, view.y, view.w, view.h, 1.0f, theme::LINE);

    Tic* tic = scene.get(st.selected);
    UiCanvas* canvas = tic ? tic->getComponent<UiCanvas>() : nullptr;
    if (!canvas || canvas->elements.empty()) {
        if (st.selElement >= 0) {
            st.selElement = -1;
        }
        st.elDrag = false;
        const TextMetrics tm = ui.textMetrics();
        ui.labelFitted(view.x + kPad, view.y + view.h * 0.5f + tm.ascent,
                       tic ? "(UI vazia — use + para criar o primeiro elemento)"
                           : "(selecione um TIC na Hierarchy)",
                       theme::LINE, view.w - 2.0f * kPad);
        return;
    }
    if (st.selElement >= static_cast<i32>(canvas->elements.size())) {
        st.selElement = -1;
        st.elDrag = false;
    }

    // transform partilhado: draw + hit-test + drag usam O MESMO
    const ViewportTransform t = uiViewportTransform(view, sw, sh);

    // moldura do espaço de design (o "ecrã" onde a UI vive em Play)
    ui.frame(t.ox, t.oy, sw * t.scale, sh * t.scale, 1.0f, theme::LINE);

    // elementos (ordem do array = z-order; o último fica por cima)
    for (size_t i = 0; i < canvas->elements.size(); ++i) {
        const UiElement& e = canvas->elements[i];
        const UiRect r = ui::elementRect(e, sw, sh, safe::Insets{});
        const UiRect rs{t.ox + r.x * t.scale, t.oy + r.y * t.scale,
                        r.w * t.scale, r.h * t.scale};
        ui::drawElement(ui, e, rs,
                        static_cast<i32>(i) == st.selElement);
    }

    // ---- gesto WYSIWYG (slot 0; NÃO corre por baixo de overlays) ---------
    if (anyOverlayOpen(st)) {
        return;
    }
    if (st.elDrag && st.selElement >= 0) {
        if (in.down(0)) {
            f32 px = 0.0f, py = 0.0f;
            in.pos(0, px, py);
            const f32 dx = (px - st.elDragX) / (t.scale > 0.0f ? t.scale : 1.0f);
            const f32 dy = (py - st.elDragY) / (t.scale > 0.0f ? t.scale : 1.0f);
            UiElement& e = canvas->elements[static_cast<size_t>(st.selElement)];
            e.ox += dx;
            e.oy += dy;
            st.elDragX = px;
            st.elDragY = py;
        } else {
            st.elDrag = false;
        }
        return;
    }
    if (in.pressed(0)) {
        f32 px = 0.0f, py = 0.0f;
        in.pos(0, px, py);
        if (px >= view.x && px < view.x + view.w && py >= view.y &&
            py < view.y + view.h) {
            const f32 dx = uiViewportToDesignX(t, px);
            const f32 dy = uiViewportToDesignY(t, py);
            // o de CIMA ganha (hit-test reverso à ordem de desenho)
            i32 hit = -1;
            for (i32 i = static_cast<i32>(canvas->elements.size()) - 1; i >= 0; --i) {
                const UiElement& e =
                    canvas->elements[static_cast<size_t>(i)];
                const UiRect r = ui::elementRect(e, sw, sh, safe::Insets{});
                if (dx >= r.x && dx < r.x + r.w && dy >= r.y && dy < r.y + r.h) {
                    hit = i;
                    break;
                }
            }
            st.selElement = hit;   // −1 = tap no vazio → desseleciona
            if (hit >= 0) {
                st.elDrag = true;
                st.elDragX = px;
                st.elDragY = py;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// INSPECTOR DE ELEMENTO (painel direito, modo UI)
// ---------------------------------------------------------------------------

bool uiElementHasText(UiElement::Kind k) {
    return k == UiElement::Kind::Label || k == UiElement::Kind::Button ||
           k == UiElement::Kind::Card || k == UiElement::Kind::Article ||
           k == UiElement::Kind::Menu;
}

bool uiElementHasAction(UiElement::Kind k) {
    return k == UiElement::Kind::Button || k == UiElement::Kind::Menu;
}

u32 uiInspectorRowCount(const UiElement& e, const TextMetrics& m) {
    (void)m;
    u32 n = 6;   // nome + X + Y + W + H + visivel
    n += 3;      // R + G + B
    n += 2;      // ancora H + ancora V
    if (uiElementHasText(e.kind)) {
        ++n;     // texto
    }
    if (uiElementHasAction(e.kind)) {
        ++n;     // acao
        if (e.action != UiElement::Action::None) {
            ++n;   // alvo
        }
    }
    ++n;         // remover
    return n;
}

u32 uiInspectorPlan(const UiElement& e, const TextMetrics& m, UiInspRow* rows,
                    u32 cap) {
    const f32 textH = inspTextRowH(m);
    const f32 btnH  = inspButtonRowH(m);
    const f32 sldH  = inspSliderRowH(m);

    u32 n = 0;
    f32 y = 0.0f;
    auto push = [&](UiInspRow::Kind kind, f32 h, u64 id) {
        if (n >= cap) {
            return;
        }
        rows[n].kind = kind;
        rows[n].y    = y;
        rows[n].h    = h;
        rows[n].id   = id;
        ++n;
        y += h;   // o ÚNICO avanço de cursor (o contrato do plano)
    };

    push(UiInspRow::Kind::Name, textH, 0);
    push(UiInspRow::Kind::PosX, sldH, kUiInspX);
    push(UiInspRow::Kind::PosY, sldH, kUiInspY);
    push(UiInspRow::Kind::SizeW, sldH, kUiInspW);
    push(UiInspRow::Kind::SizeH, sldH, kUiInspH);
    push(UiInspRow::Kind::ColR, sldH, kUiInspR);
    push(UiInspRow::Kind::ColG, sldH, kUiInspG);
    push(UiInspRow::Kind::ColB, sldH, kUiInspB);
    push(UiInspRow::Kind::VisToggle, btnH, kUiInspVis);
    push(UiInspRow::Kind::AnchorH, btnH, kUiInspAnchH);
    push(UiInspRow::Kind::AnchorV, btnH, kUiInspAnchV);
    if (uiElementHasText(e.kind)) {
        push(UiInspRow::Kind::TextBtn, btnH, kUiInspText);
    }
    if (uiElementHasAction(e.kind)) {
        push(UiInspRow::Kind::ActType, btnH, kUiInspAct);
        if (e.action != UiElement::Action::None) {
            push(UiInspRow::Kind::ActTarget, btnH, kUiInspTarget);
        }
    }
    push(UiInspRow::Kind::Remove, btnH, kUiInspRemove);
    return n;
}

f32 uiInspectorContentHeight(const UiElement& e, const TextMetrics& m) {
    UiInspRow rows[24];
    const u32 n = uiInspectorPlan(e, m, rows, 24);
    if (n == 0) {
        return 0.0f;
    }
    return rows[n - 1].y + rows[n - 1].h;
}

bool drawUiInspector(UiContext& ui, Scene& scene, EditorState& st,
                     const InputState& in) {
    (void)in;
    const UiRect panel =
        safe::inspectorPanelRect(ui.screenWidth(), ui.screenHeight(),
                                 ui.safeArea());
    const f32 x = panel.x;
    const f32 y = panel.y;
    const f32 w = panel.w;
    const f32 h = panel.h;

    ui.panel(x, y, w, h, theme::PANEL);
    ui.panel(x, y, 1.0f, h, theme::LINE);

    const TextMetrics tm = ui.textMetrics();
    ui.label(x + kPad, y + kHeaderH * 0.5f + tm.block() * 0.30f,
             "INSPECTOR UI", theme::TEXT);
    ui.panel(x + kPad, y + kHeaderH - 1.0f, w - 2.0f * kPad, 1.0f, theme::LINE);

    Tic* tic = scene.get(st.selected);
    UiCanvas* canvas = tic ? tic->getComponent<UiCanvas>() : nullptr;
    if (!canvas || st.selElement < 0 ||
        st.selElement >= static_cast<i32>(canvas->elements.size())) {
        ui.labelFitted(x + kPad, y + kHeaderH + kRowH,
                       "(nenhum elemento selecionado)", theme::LINE,
                       w - 2.0f * kPad);
        return false;
    }
    UiElement& e = canvas->elements[static_cast<size_t>(st.selElement)];

    // ---- PLANO (fonte única — o mesmo contrato do Inspector de TICs)
    const u32 nRows = uiInspectorRowCount(e, tm);
    UiInspRow plan[24];
    const u32 n = uiInspectorPlan(e, tm, plan, 24);
    const f32 contentH = uiInspectorContentHeight(e, tm);
    const f32 contentTop = y + kHeaderH + 4.0f;
    ui.beginScroll(kUiInspScrollId, {x, contentTop, w, h - kHeaderH - 4.0f},
                   contentH);
    const f32 off = ui.scrollOffset();

    bool edited = false;
    u32 colorIdx = 0;   // payload dos ColorSlider (0=R, 1=G, 2=B)

    char nameLine[64];
    std::snprintf(nameLine, sizeof(nameLine), "%s  (%s)", e.name.c_str(),
                  uiElementKindName(e.kind));

    for (u32 i = 0; i < n; ++i) {
        const UiInspRow& r = plan[i];
        const f32 ry = contentTop + r.y - off;
        switch (r.kind) {
        case UiInspRow::Kind::Name:
            ui.labelFitted(x + kPad, inspBaseline(ry, r.h, tm), nameLine,
                           theme::ACCENT, w - 2.0f * kPad);
            break;
        case UiInspRow::Kind::PosX:
            if (uiSliderRow(ui, r.id, x, ry, r.h, tm, "x", -3000.0f, 3000.0f,
                            e.ox, "%.0f")) {
                edited = true;
            }
            break;
        case UiInspRow::Kind::PosY:
            if (uiSliderRow(ui, r.id, x, ry, r.h, tm, "y", -3000.0f, 3000.0f,
                            e.oy, "%.0f")) {
                edited = true;
            }
            break;
        case UiInspRow::Kind::SizeW:
            if (uiSliderRow(ui, r.id, x, ry, r.h, tm, "lar", 8.0f, 3000.0f,
                            e.w, "%.0f")) {
                edited = true;
            }
            break;
        case UiInspRow::Kind::SizeH:
            if (uiSliderRow(ui, r.id, x, ry, r.h, tm, "alt", 8.0f, 3000.0f,
                            e.h, "%.0f")) {
                edited = true;
            }
            break;
        case UiInspRow::Kind::ColR:
        case UiInspRow::Kind::ColG:
        case UiInspRow::Kind::ColB: {
            static const char* kLabels[3] = {"cor R", "cor G", "cor B"};
            if (uiSliderRow(ui, r.id, x, ry, r.h, tm, kLabels[colorIdx], 0.0f,
                            1.0f, e.color[colorIdx], "%.2f")) {
                edited = true;
            }
            ++colorIdx;
            break;
        }
        case UiInspRow::Kind::VisToggle: {
            char label[32];
            std::snprintf(label, sizeof(label), "visivel: %s",
                          e.visible ? "sim" : "nao");
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      label);
            break;
        }
        case UiInspRow::Kind::AnchorH: {
            const char* ah = e.anchorH == UiElement::AnchorH::Left ? "esquerda"
                          : e.anchorH == UiElement::AnchorH::Center ? "centro"
                                                                    : "direita";
            char label[48];
            std::snprintf(label, sizeof(label), "ancora H: %s", ah);
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      label);
            break;
        }
        case UiInspRow::Kind::AnchorV: {
            const char* av = e.anchorV == UiElement::AnchorV::Top ? "topo"
                          : e.anchorV == UiElement::AnchorV::Middle ? "meio"
                                                                    : "fundo";
            char label[48];
            std::snprintf(label, sizeof(label), "ancora V: %s", av);
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      label);
            break;
        }
        case UiInspRow::Kind::TextBtn: {
            char label[64];
            std::snprintf(label, sizeof(label), "texto: %s",
                          e.text.empty() ? "(vazio)" : e.text.c_str());
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      label);
            break;
        }
        case UiInspRow::Kind::ActType: {
            char label[48];
            std::snprintf(label, sizeof(label), "acao: %s",
                          uiActionName(e.action));
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      label);
            break;
        }
        case UiInspRow::Kind::ActTarget: {
            char label[64];
            std::snprintf(label, sizeof(label), "alvo: %s",
                          e.target.empty() ? "(vazio)" : e.target.c_str());
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      label);
            break;
        }
        case UiInspRow::Kind::Remove:
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      "remover elemento");
            break;
        }
    }
    ui.endScroll();

    // tap re-despachado → botões do plano (hit-test no rect DESENHADO)
    f32 tx = 0.0f, ty = 0.0f;
    if (ui.scrollTap(kUiInspScrollId, tx, ty)) {
        for (u32 i = 0; i < n; ++i) {
            const UiInspRow& r = plan[i];
            if (r.kind == UiInspRow::Kind::PosX || r.kind == UiInspRow::Kind::PosY ||
                r.kind == UiInspRow::Kind::SizeW || r.kind == UiInspRow::Kind::SizeH ||
                r.kind == UiInspRow::Kind::ColR || r.kind == UiInspRow::Kind::ColG ||
                r.kind == UiInspRow::Kind::ColB || r.kind == UiInspRow::Kind::Name) {
                continue;   // sliders capturam o gesto diretamente
            }
            const f32 ry = contentTop + r.y - off;
            if (tx < x + kPad || tx >= x + w - kPad) {
                continue;
            }
            if (ty < ry + 2.0f || ty >= ry + r.h - 2.0f) {
                continue;
            }
            switch (r.kind) {
                case UiInspRow::Kind::VisToggle:
                    e.visible = !e.visible;
                    edited = true;
                    break;
                case UiInspRow::Kind::AnchorH:
                    canvas->setAnchor(
                        st.selElement,
                        static_cast<UiElement::AnchorH>(
                            (static_cast<u32>(e.anchorH) + 1u) % 3u),
                        e.anchorV, ui.screenWidth(), ui.screenHeight());
                    edited = true;
                    break;
                case UiInspRow::Kind::AnchorV:
                    canvas->setAnchor(
                        st.selElement, e.anchorH,
                        static_cast<UiElement::AnchorV>(
                            (static_cast<u32>(e.anchorV) + 1u) % 3u),
                        ui.screenWidth(), ui.screenHeight());
                    edited = true;
                    break;
                case UiInspRow::Kind::TextBtn:
                    openTextInput(st, 2, st.selected, st.selElement,
                                  e.text.c_str());
                    break;
                case UiInspRow::Kind::ActType: {
                    // cicla none → show → hide → toggle → scene → spawn
                    const u32 next = e.action == UiElement::Action::None ? 1
                        : (static_cast<u32>(e.action) % 5u) + 1u;
                    e.action = static_cast<UiElement::Action>(next);
                    edited = true;
                    break;
                }
                case UiInspRow::Kind::ActTarget:
                    openTextInput(st, 3, st.selected, st.selElement,
                                  e.target.c_str());
                    break;
                case UiInspRow::Kind::Remove:
                    canvas->elements.erase(
                        canvas->elements.begin() + st.selElement);
                    st.selElement = -1;
                    edited = true;
                    break;
            }
        }
    }
    (void)nRows;
    return edited;
}

// ---------------------------------------------------------------------------
// CRIAÇÃO DE ELEMENTOS (+ do modo UI)
// ---------------------------------------------------------------------------

bool uiAddElement(Scene& scene, EditorState& st, u32 kind, f32 sw, f32 sh) {
    Tic* tic = scene.get(st.selected);
    if (!tic) {
        return false;
    }
    UiCanvas* canvas = tic->getComponent<UiCanvas>();
    if (!canvas) {
        canvas = tic->addComponent<UiCanvas>();
    }
    if (!canvas) {
        return false;
    }
    if (kind > 3) {
        return false;   // 0.7.3 acrescenta os compostos
    }
    const UiElement::Kind k = static_cast<UiElement::Kind>(kind);
    const i32 idx = canvas->addElement(k, sw, sh);
    st.selElement = idx;
    return true;
}

// ---------------------------------------------------------------------------
// GESTÃO DE TICs — menu contextual + diálogo de remoção + duplicar
// ---------------------------------------------------------------------------

int drawContextMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                    EditorState& st, const char* ticName, bool ticVisible) {
    const f32 w = kMenuW;
    constexpr int kItems = 4;
    const f32 h = kHeaderH + static_cast<f32>(kItems) * 64.0f + kPad;
    const f32 ox = ui.safeLeft();
    const f32 oy = ui.safeTop();
    const f32 aw = sw - ox - ui.safeRight();
    const f32 ah = sh - oy - ui.safeBottom();
    const f32 x = ox + (aw - w) * 0.5f;
    const f32 y = oy + (ah - h) * 0.5f;

    // toque fora fecha (o menu é modal sobre o editor)
    f32 px = -1.0f, py = -1.0f;
    if (in.pressed(0)) {
        in.pos(0, px, py);
    }
    if (in.pressed(0) &&
        !(px >= x && px < x + w && py >= y && py < y + h)) {
        st.contextMenu = false;
        return 0;
    }

    ui.panel(x, y, w, h, theme::PANEL);
    ui.frame(x, y, w, h, 2.0f, theme::ACCENT);
    const f32 th = ui.fontHeight();
    char title[64];
    std::snprintf(title, sizeof(title), "%.40s", ticName ? ticName : "?");
    ui.labelFitted(x + kPad, y + kHeaderH * 0.5f + th * 0.30f, title,
                   theme::TEXT, w - 2.0f * kPad);

    char visLabel[48];
    std::snprintf(visLabel, sizeof(visLabel), "Visibilidade: %s",
                  ticVisible ? "esconder" : "mostrar");
    const char* labels[kItems] = {"Renomear", "Remover", "Duplicar",
                                  visLabel};
    int chosen = 0;
    for (int i = 0; i < kItems; ++i) {
        if (ui.button(kCtxRenameId + static_cast<u64>(i), x + kPad,
                      y + kHeaderH + static_cast<f32>(i) * 64.0f,
                      w - 2.0f * kPad, 56.0f, labels[i])) {
            chosen = i + 1;
            st.contextMenu = false;
        }
    }
    return chosen;
}

int drawRemoveDialog(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                     EditorState& st, const char* ticName) {
    const f32 h = storageDialogHeight();   // título + 3 linhas + 2 botões
    const f32 ox = ui.safeLeft();
    const f32 oy = ui.safeTop();
    const f32 aw = sw - ox - ui.safeRight();
    const f32 ah = sh - oy - ui.safeBottom();
    const UiRect dlg = centeredMenuRect(ox, oy, aw, ah, h);

    // toque fora = cancelar (o TIC fica)
    f32 px = -1.0f, py = -1.0f;
    if (in.pressed(0)) {
        in.pos(0, px, py);
    }
    if (in.pressed(0) &&
        !(px >= dlg.x && px < dlg.x + dlg.w && py >= dlg.y &&
          py < dlg.y + dlg.h)) {
        st.removeDialog = false;
        return 0;
    }

    ui.panel(dlg.x, dlg.y, dlg.w, dlg.h, theme::PANEL);
    ui.frame(dlg.x, dlg.y, dlg.w, dlg.h, 2.0f, theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.label(dlg.x + kPad, dlg.y + kHeaderH * 0.5f + th * 0.30f,
             "REMOVER TIC", theme::TEXT);

    char line[96];
    std::snprintf(line, sizeof(line), "Remover '%s'?", ticName ? ticName : "?");
    ui.labelFitted(dlg.x + kPad, dlg.y + kHeaderH + 20.0f, line, theme::TEXT,
                   dlg.w - 2.0f * kPad);
    ui.labelFitted(dlg.x + kPad, dlg.y + kHeaderH + 54.0f,
                   "Os componentes sao apagados (nao ha desfazer).",
                   theme::LINE, dlg.w - 2.0f * kPad);

    UiRect del{}, cancel{};
    storageDialogButtons(dlg, del, cancel);
    int chosen = 0;
    if (ui.button(kRemoveConfirmId, del.x, del.y, del.w, del.h, "Remover")) {
        chosen = 1;
        st.removeDialog = false;
    }
    if (ui.button(kRemoveCancelId, cancel.x, cancel.y, cancel.w, cancel.h,
                  "Cancelar")) {
        chosen = 2;
        st.removeDialog = false;
    }
    return chosen;
}

Handle duplicateTic(Scene& scene, Handle h) {
    Tic* src = scene.get(h);
    if (!src) {
        return Handle::invalid();
    }
    // nome único Godot-style: base.001, base.002, …
    std::string base = src->name;
    if (base.size() > 28) {
        base = base.substr(0, 28);
    }
    char name[48];
    u32 suffix = 1;
    do {
        std::snprintf(name, sizeof(name), "%s.%03u", base.c_str(), suffix);
        ++suffix;
    } while (scene.find(name).valid() && suffix < 1000);

    const Handle nh = scene.create(name);
    Tic* dst = scene.get(nh);
    if (!dst) {
        return Handle::invalid();
    }
    dst->visible = src->visible;
    dst->parent = src->parent;

    // componentes por VALOR (os storages são SoA — a cópia é exata; os
    // ponteiros não-donos de Mesh/Texture/Material apontam para objetos
    // partilhados de runtime — a mesma política do load do serializer)
    ComponentStore& store = scene.components();
    if (const Transform3D* c = store.transforms().find(h)) {
        dst->addComponent<Transform3D>(*c);
    }
    if (const MeshRenderer* c = store.meshRenderers().find(h)) {
        dst->addComponent<MeshRenderer>(*c);
    }
    if (const InputMap* c = store.inputMaps().find(h)) {
        dst->addComponent<InputMap>(*c);
    }
    if (const BodyComp* c = store.bodies().find(h)) {
        dst->addComponent<BodyComp>(*c);
    }
    if (const TouchControls* c = store.touchControls().find(h)) {
        dst->addComponent<TouchControls>(*c);
    }
    if (const UiCanvas* c = store.uiCanvases().find(h)) {
        dst->addComponent<UiCanvas>(*c);
    }
    return nh;
}

// ---------------------------------------------------------------------------
// TECLADO IN-APP + input de texto
// ---------------------------------------------------------------------------

const char* KeyboardLayout::keyLabel(u32 row, u32 col) {
    static const char* kRow0[9] = {"A", "B", "C", "D", "E", "F", "G", "H", "I"};
    static const char* kRow1[9] = {"J", "K", "L", "M", "N", "O", "P", "Q", "R"};
    static const char* kRow2[9] = {"S", "T", "U", "V", "W", "X", "Y", "_"};
    static const char* kRow3[10] = {"0", "1", "2", "3", "4",
                                    "5", "6", "7", "8", "9"};
    switch (row) {
        case 0:  return col < 9 ? kRow0[col] : "";
        case 1:  return col < 9 ? kRow1[col] : "";
        case 2:  return col < 9 ? kRow2[col] : "";   // 8 letras + '_'
        default: return col < 10 ? kRow3[col] : "";
    }
}

KeyboardLayout keyboardLayout(f32 sw, f32 sh, const safe::Insets& ins) {
    KeyboardLayout k;
    const f32 aw = sw - ins.left - ins.right;
    const f32 ah = sh - ins.top - ins.bottom;
    const f32 w = aw * 0.92f > 760.0f ? 760.0f : aw * 0.92f;

    constexpr f32 kHeaderH = 48.0f;
    constexpr f32 kBufH = 56.0f;
    constexpr f32 kKeyH = 52.0f;
    constexpr f32 kGap = 6.0f;
    // 5 LINHAS DE TECLAS: 4 de teclas (A..Z_, 0..9) + a linha de baixo
    // (ESPACO/-/APAGA/OK/CANCELAR) — a linha de baixo CONTA na altura (o
    // bug da 1ª versão: sem ela, a linha 3 sobrepunha o CANCELAR)
    const f32 h = kHeaderH + kBufH + 5.0f * kKeyH + 4.0f * kGap + kPad;

    k.dialog = {ins.left + (aw - w) * 0.5f, ins.top + (ah - h) * 0.5f, w, h};
    k.buffer = {k.dialog.x + kPad, k.dialog.y + kHeaderH,
                w - 2.0f * kPad, kBufH};

    const f32 innerW = w - 2.0f * kPad;
    const f32 keyW = (innerW - 9.0f * kGap) / 10.0f;
    for (u32 row = 0; row < 4; ++row) {
        const u32 count = k.keyCount[row];
        const f32 rowW =
            static_cast<f32>(count) * keyW + static_cast<f32>(count - 1) * kGap;
        const f32 x0 = k.dialog.x + kPad + (innerW - rowW) * 0.5f;
        const f32 y0 = k.dialog.y + kHeaderH + kBufH +
                       static_cast<f32>(row) * (kKeyH + kGap);
        for (u32 col = 0; col < count; ++col) {
            k.key[row][col] = {x0 + static_cast<f32>(col) * (keyW + kGap), y0,
                               keyW, kKeyH};
        }
    }

    // linha de baixo (a ÚLTIMA linha de teclas — depois da 4ª, com o MESMO
    // espaçamento): [ESPACO 3u][ '-' 1u ][APAGA 2u][OK 2u][CANCELAR 2u]
    const f32 yB = k.dialog.y + kHeaderH + kBufH + 4.0f * (kKeyH + kGap);
    const f32 unit = (innerW - 4.0f * kGap) / 10.0f;
    f32 x = k.dialog.x + kPad;
    k.space = {x, yB, 3.0f * unit, kKeyH};
    x += 3.0f * unit + kGap;
    k.dash = {x, yB, unit, kKeyH};
    x += unit + kGap;
    k.back = {x, yB, 2.0f * unit, kKeyH};
    x += 2.0f * unit + kGap;
    k.ok = {x, yB, 2.0f * unit, kKeyH};
    x += 2.0f * unit + kGap;
    k.cancel = {x, yB, unit, kKeyH};
    return k;
}

bool uiTextCharAllowed(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-' || c == ' ';
}

int drawTextInput(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                  EditorState& st, const char* title) {
    const KeyboardLayout k = keyboardLayout(sw, sh, ui.safeArea());

    // toque fora = cancelar (o buffer não se aplica)
    f32 px = -1.0f, py = -1.0f;
    if (in.pressed(0)) {
        in.pos(0, px, py);
    }
    if (in.pressed(0) &&
        !(px >= k.dialog.x && px < k.dialog.x + k.dialog.w &&
          py >= k.dialog.y && py < k.dialog.y + k.dialog.h)) {
        st.textInput = false;
        return 2;
    }

    ui.panel(k.dialog.x, k.dialog.y, k.dialog.w, k.dialog.h, theme::PANEL);
    ui.frame(k.dialog.x, k.dialog.y, k.dialog.w, k.dialog.h, 2.0f,
             theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.labelFitted(k.dialog.x + kPad,
                   k.dialog.y + kHeaderH * 0.5f + th * 0.30f, title,
                   theme::TEXT, k.dialog.w - 2.0f * kPad);

    // buffer + cursor '_' (a linha mostra o estado AO VIVO)
    char shown[48];
    std::snprintf(shown, sizeof(shown), "%.*s_", static_cast<int>(st.textLen),
                  st.textBuf);
    ui.panel(k.buffer.x, k.buffer.y, k.buffer.w, k.buffer.h, theme::BG);
    ui.frame(k.buffer.x, k.buffer.y, k.buffer.w, k.buffer.h, 1.0f,
             theme::LINE);
    const TextMetrics tm = ui.textMetrics();
    ui.labelFitted(k.buffer.x + 12.0f,
                   k.buffer.y + (k.buffer.h - tm.block()) * 0.5f + tm.ascent,
                   shown, theme::ACCENT, k.buffer.w - 24.0f);

    int result = 0;
    auto typeChar = [&](char c) {
        if (st.textLen < sizeof(st.textBuf) - 1) {
            st.textBuf[st.textLen++] = c;
            st.textBuf[st.textLen] = '\0';
        }
    };
    for (u32 row = 0; row < 4; ++row) {
        for (u32 col = 0; col < k.keyCount[row]; ++col) {
            const UiRect& r = k.key[row][col];
            if (ui.button(kKbBase + static_cast<u64>(row) * 10u +
                              static_cast<u64>(col),
                          r.x, r.y, r.w, r.h, KeyboardLayout::keyLabel(row, col))) {
                typeChar(KeyboardLayout::keyLabel(row, col)[0]);
            }
        }
    }
    if (ui.button(kKbSpaceId, k.space.x, k.space.y, k.space.w, k.space.h,
                  "ESPACO")) {
        typeChar(' ');
    }
    if (ui.button(kKbDashId, k.dash.x, k.dash.y, k.dash.w, k.dash.h, "-")) {
        typeChar('-');
    }
    if (ui.button(kKbBackId, k.back.x, k.back.y, k.back.w, k.back.h, "APAGA")) {
        if (st.textLen > 0) {
            st.textBuf[--st.textLen] = '\0';
        }
    }
    if (ui.button(kKbOkId, k.ok.x, k.ok.y, k.ok.w, k.ok.h, "OK")) {
        st.textInput = false;
        result = 1;
    }
    if (ui.button(kKbCancelId, k.cancel.x, k.cancel.y, k.cancel.w, k.cancel.h,
                  "X")) {
        st.textInput = false;
        result = 2;
    }
    return result;
}

void openTextInput(EditorState& st, int purpose, Handle tic, i32 element,
                   const char* initial) {
    st.textInput = true;
    st.textPurpose = purpose;
    st.textTic = tic;
    st.textElement = element;
    st.textLen = 0;
    st.textBuf[0] = '\0';
    if (initial) {
        while (*initial && st.textLen < sizeof(st.textBuf) - 1) {
            st.textBuf[st.textLen++] = *initial++;
        }
        st.textBuf[st.textLen] = '\0';
    }
}

bool commitTextInput(Scene& scene, EditorState& st) {
    if (st.textLen == 0) {
        return false;   // nome/texto vazio NÃO se aplica (fica o anterior)
    }
    switch (st.textPurpose) {
        case 0: {   // renomear TIC
            Tic* tic = scene.get(st.textTic);
            if (!tic) {
                return false;
            }
            const Handle found = scene.find(st.textBuf);
            if (!found.valid() || found == st.textTic) {
                tic->name = st.textBuf;   // livre (ou é o próprio) → direto
                return true;
            }
            // nome ocupado por OUTRO TIC ativo → sufixo Godot-style
            char name[48];
            u32 suffix = 1;
            do {
                std::snprintf(name, sizeof(name), "%.28s.%03u", st.textBuf,
                              suffix);
                ++suffix;
            } while (scene.find(name).valid() && suffix < 1000);
            tic->name = name;
            return true;
        }
        case 2: {   // texto do elemento
            Tic* tic = scene.get(st.textTic);
            UiCanvas* canvas = tic ? tic->getComponent<UiCanvas>() : nullptr;
            if (!canvas || st.textElement < 0 ||
                st.textElement >= static_cast<i32>(canvas->elements.size())) {
                return false;
            }
            canvas->elements[static_cast<size_t>(st.textElement)].text =
                st.textBuf;
            return true;
        }
        case 3: {   // alvo da ação
            Tic* tic = scene.get(st.textTic);
            UiCanvas* canvas = tic ? tic->getComponent<UiCanvas>() : nullptr;
            if (!canvas || st.textElement < 0 ||
                st.textElement >= static_cast<i32>(canvas->elements.size())) {
                return false;
            }
            canvas->elements[static_cast<size_t>(st.textElement)].target =
                st.textBuf;
            return true;
        }
        default:
            return false;   // propósito 1 (nome de cena): o chamador (0.7.1)
    }
}

} // namespace editor
} // namespace vv
