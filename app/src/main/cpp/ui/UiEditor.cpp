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
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>   // 0.8.9: strtof (campo numérico do Inspector)
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

namespace {
// 0.8.9 — graus→radianos local (o commit do campo numérico converte rot)
constexpr float deg2radLocal(float d) { return d * 0.01745329252f; }
} // namespace
#include "ui/UiContext.h"
#include "ui/UiRuntime.h"

namespace vv {
namespace editor {

namespace {
// (kPad/kHeaderH/kRowH/kMenuW vêm de ui/EditorLayout.h — a fonte única)

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

// 0.7.5 — algum overlay MODAL aberto? (agora público: o main usa-o para
// decidir se desenha o chrome do editor OU o backdrop modal). 0.7.5
// acrescentou os que faltavam: CENAS (scenesMenu), NAVEGADOR (fileBrowser)
// e APLICAR (applyAsk) — antes não bloqueavam o gesto WYSIWYG por baixo.
bool anyOverlayOpen(const EditorState& st) {
    return st.plusMenu || st.fileMenu || st.settingsMenu || st.contextMenu ||
           st.removeDialog || st.textInput || st.assetMenu != 0 ||
           st.importMenu || st.storageDialog || st.logViewer ||
           st.scenesMenu || st.fileBrowser || st.applyAsk ||
           st.textWin.open ||   // 0.9.1: janela de texto pesado (portrait+IME)
           st.scriptWin.open ||   // 0.9.2: editor de script (portrait+IME)
           st.docsScreen.open;    // 0.9.2: ecrã de Docs
}

// 0.9.6 (G1) — os ECRÃS CHEIOS (a lista fechada da camada modal-maior):
// Settings (página full-screen desde 0.9.0), Docs, editor de script e
// janela de texto. Os DIÁLOGOS (menus/seletores/viewer) NÃO estão aqui —
// continuam a conviver com a barra de baixo (comportamento de sempre).
bool fullscreenOverlayOpen(const EditorState& st) {
    return st.settingsMenu || st.docsScreen.open || st.scriptWin.open ||
           st.textWin.open;
}

// 0.7.5 — BACKDROP MODAL: fundo OPACO que tapa o ecrã TODO (o chrome do
// editor não se desenha com um modal aberto; o backdrop garante que NADA
// — canvas UI incluído — aparece por trás/à mista com o overlay. O fix do
// C33: "os elementos do canvas desenham-se por cima/através do overlay,
// misturando texto"). Só um desenho — nenhum gesto (o overlay dono do
// toque continua a fechar com toque fora, como sempre).
void drawModalBackdrop(UiContext& ui, f32 sw, f32 sh) {
    ui.panel(0.0f, 0.0f, sw, sh, theme::BG);
}

// ---------------------------------------------------------------------------
// VIEWPORT 2D — o modo UI do separador "3D | UI"
// ---------------------------------------------------------------------------

void drawUiViewport(UiContext& ui, Scene& scene, EditorState& st,
                    const InputState& in, f32 sw, f32 sh) {
    // 0.7.6: com o painel do Inspector escondido (G5 da toolbar) o mini-ecrã
    // cresce para a direita — o MESMO centerRect da câmara 3D
    const UiRect view =
        safe::centerRect(sw, sh, ui.safeArea(), st.showInspector);
    // fundo dedicado (o modo UI NÃO desenha a cena 3D — viewport só da UI)
    ui.panel(view.x, view.y, view.w, view.h, theme::BG);
    ui.frame(view.x, view.y, view.w, view.h, 1.0f, theme::LINE);

    Tic* tic = scene.get(st.selected);
    // 0.7.3: o joystick (TouchControls) é editável mesmo SEM canvas — o
    // early-return da dica só corre quando não há canvas NEM joystick
    UiCanvas* canvas = tic ? tic->getComponent<UiCanvas>() : nullptr;
    TouchControls* joy = tic ? tic->getComponent<TouchControls>() : nullptr;
    if (!canvas && !joy) {
        st.selElement = -1;
        st.selJoystick = false;
        st.elDrag = false;
        const TextMetrics tm = ui.textMetrics();
        // 0.7.4: sem TIC selecionado o "+" CRIA o TIC de UI (ensureUiTic no
        // dispatch) — a dica diz o caminho (não obriga a TIC 3D)
        ui.labelFitted(view.x + kPad, view.y + view.h * 0.5f + tm.ascent,
                       tic ? "(UI vazia — use + para criar o primeiro elemento)"
                           : "(use + para criar UI — nasce o TIC 'UI')",
                       theme::LINE, view.w - 2.0f * kPad);
        return;
    }
    static const UiCanvas kEmptyCanvas;   // canvas de LEITURA (só joystick)
    if (!canvas) {
        canvas = const_cast<UiCanvas*>(&kEmptyCanvas);
    }
    if (st.selElement >= static_cast<i32>(canvas->elements.size())) {
        st.selElement = -1;
        st.elDrag = false;
    }

    // transform partilhado: draw + hit-test + drag usam O MESMO
    const ViewportTransform t = uiViewportTransform(view, sw, sh);

    // moldura do espaço de design (o "ecrã" onde a UI vive em Play)
    ui.frame(t.ox, t.oy, sw * t.scale, sh * t.scale, 1.0f, theme::LINE);

    // 0.7.4 — RESOLVER (FONTE ÚNICA do layout; âncoras + safe-area REAL +
    // containers). O editor desenha o MESMO layout do Play, escalado —
    // paridade estrutural (o mesmo resolveCanvasLayout alimenta drawCanvas
    // e hitTestCanvas).
    const safe::Insets ins = ui.safeArea();
    ui::CanvasLayout lay[32];
    const u32 nLay =
        canvas->elements.size() < 32
            ? static_cast<u32>(canvas->elements.size())
            : 32;
    ui::resolveCanvasLayout(*canvas, sw, sh, ins, lay, nLay);

    // 0.7.4 — joystick com a MESMA APARÊNCIA do Play: o núcleo partilhado
    // (drawTouchControlsAt) desenha a base + knob + JUMP no mini-ecrã (com
    // o transform) — o proxy simplificado da 0.7.3 era uma coisa no editor
    // e outra no Play (sem botão JUMP, knob próprio).
    f32 joyX0 = 0.0f, joyY0 = 0.0f, joyX1 = 0.0f, joyY1 = 0.0f;
    if (joy) {
        const TouchControls::Layout jl =
            joy->layoutFor(sw - ins.left - ins.right, sh - ins.top - ins.bottom);
        joyX0 = t.ox + (ins.left + jl.joyCX - jl.joyR) * t.scale;
        joyY0 = t.oy + (ins.top + jl.joyCY - jl.joyR) * t.scale;
        joyX1 = t.ox + (ins.left + jl.joyCX + jl.joyR) * t.scale;
        joyY1 = t.oy + (ins.top + jl.joyCY + jl.joyR) * t.scale;
        {
            // NADA do joystick sangra do mini-ecrã (paridade com o Play)
            const UiContext::ScopedClip clipJ(
                ui, {t.ox, t.oy, sw * t.scale, sh * t.scale});
            const UiContext::ScopedTextScale scopedScale(ui, t.scale);
            drawTouchControlsAt(ui, *joy,
                                t.ox + ins.left * t.scale,
                                t.oy + ins.top * t.scale,
                                sw - ins.left - ins.right,
                                sh - ins.top - ins.bottom, t.scale);
        }
        // overlay de EDIÇÃO (permitido divergir): moldura de seleção +
        // rótulo — o Play não os desenha (só interação)
        ui.frame(joyX0, joyY0, joyX1 - joyX0, joyY1 - joyY0,
                 st.selJoystick ? 3.0f : 1.0f,
                 st.selJoystick ? theme::ACCENT : theme::LINE);
        if (ui.hasFont()) {
            const TextMetrics tm2 = ui.textMetrics();
            ui.labelFitted(joyX0, joyY1 + 4.0f + tm2.ascent, "joystick",
                           theme::LINE, joyX1 - joyX0);
        }
    } else if (st.selJoystick) {
        st.selJoystick = false;   // o componente sumiu — limpa a seleção
        st.elDrag = false;
    }

    // elementos (ordem do array = z-order; o último fica por cima).
    // 0.7.4: clip ao MINI-ECRÃ (nada sangra para os painéis ao lado — o
    // Play recorta na borda física; paridade) + texto ESCALADO (o
    // mini-canvas é o Play reduzido, não texto a 28 px em cima de rects
    // a 0.4× — a causa raiz do "Menu com caixas no editor e texto solto
    // no Play" do C33).
    {
        const UiContext::ScopedClip clip(
            ui, {t.ox, t.oy, sw * t.scale, sh * t.scale});
        const UiContext::ScopedTextScale scopedScale(ui, t.scale);
        for (size_t i = 0; i < canvas->elements.size() && i < 32; ++i) {
            const UiElement& e = canvas->elements[i];
            if (!lay[i].shown) {
                continue;   // invisível (ou em container escondido)
            }
            const UiRect& r = lay[i].rect;
            const UiRect rs{t.ox + r.x * t.scale, t.oy + r.y * t.scale,
                            r.w * t.scale, r.h * t.scale};
            if (lay[i].parentIdx >= 0) {
                // FILHO: clip ao rect do PAI (como no Play)
                const UiRect& pr = lay[static_cast<size_t>(lay[i].parentIdx)].rect;
                const UiContext::ScopedClip clipP(
                    ui, {t.ox + pr.x * t.scale, t.oy + pr.y * t.scale,
                         pr.w * t.scale, pr.h * t.scale});
                ui::drawElement(ui, e, rs,
                                static_cast<i32>(i) == st.selElement);
            } else {
                ui::drawElement(ui, e, rs,
                                static_cast<i32>(i) == st.selElement);
            }
        }
    }

    // ---- GIZMOS de UI (0.8.6): handles de ESCALAR (4 cantos) e RODAR
    // (pega acima do topo-centro) do elemento selecionado — em px DE ECRÃ
    // (tamanho de toque constante, coerentes com os gizmos 3D). Containers:
    // só escala (rotação ignorada — dívida documentada).
    UiRect selRs{};   // rect do selecionado em ECRÃ (para desenho + hit)
    bool hasSel = false;
    if (st.selElement >= 0 && st.selElement < static_cast<i32>(nLay) &&
        lay[static_cast<size_t>(st.selElement)].shown) {
        const UiRect& sr = lay[static_cast<size_t>(st.selElement)].rect;
        selRs = UiRect{t.ox + sr.x * t.scale, t.oy + sr.y * t.scale,
                       sr.w * t.scale, sr.h * t.scale};
        hasSel = true;
    }
    if (hasSel) {
        const UiElement& se =
            canvas->elements[static_cast<size_t>(st.selElement)];
        for (u32 cIdx = 0; cIdx < 4; ++cIdx) {
            const UiRect h = ui::uiGizmoCornerRect(selRs, cIdx, 12.0f);
            ui.panel(h.x, h.y, h.w, h.h, theme::ACCENT);
            ui.frame(h.x, h.y, h.w, h.h, 1.0f, theme::BG);
        }
        if (!uiElementIsContainer(se.kind)) {
            const UiRect rh = ui::uiGizmoRotateHandleRect(selRs, 12.0f);
            // haste do handle ao topo-centro (visual de "ligado" ao elemento)
            ui.drawLine(selRs.x + selRs.w * 0.5f, selRs.y,
                        selRs.x + selRs.w * 0.5f, rh.y + rh.h, 1.0f,
                        theme::ACCENT);
            ui.panel(rh.x, rh.y, rh.w, rh.h, theme::ACCENT);
            ui.frame(rh.x, rh.y, rh.w, rh.h, 1.0f, theme::BG);
        }
    }

    // ---- gesto WYSIWYG (slot 0; NÃO corre por baixo de overlays) ---------
    if (anyOverlayOpen(st)) {
        return;
    }
    if (st.elDrag && (st.selElement >= 0 || st.selJoystick)) {
        if (in.down(0)) {
            f32 px = 0.0f, py = 0.0f;
            in.pos(0, px, py);
            const f32 dx = (px - st.elDragX) / (t.scale > 0.0f ? t.scale : 1.0f);
            const f32 dy = (py - st.elDragY) / (t.scale > 0.0f ? t.scale : 1.0f);
            if (st.selJoystick && joy) {
                // 0.7.3: o drag do joystick move pos (frações da área útil)
                joy->relX += dx / sw;
                joy->relY += dy / sh;
                if (joy->relX < 0.0f) joy->relX = 0.0f;
                if (joy->relX > 1.0f) joy->relX = 1.0f;
                if (joy->relY < 0.0f) joy->relY = 0.0f;
                if (joy->relY > 1.0f) joy->relY = 1.0f;
            } else if (st.elGizmoMode == 1 && st.selElement >= 0) {
                // 0.8.6 — ESCALAR: o canto arrastado segue o dedo, o canto
                // oposto fica FIXO (âncora capturada no press); w/h mín 8 px
                UiElement& e =
                    canvas->elements[static_cast<size_t>(st.selElement)];
                const f32 dpx = uiViewportToDesignX(t, px);
                const f32 dpy = uiViewportToDesignY(t, py);
                const f32 nx = std::min(st.elGizAnchorX, dpx);
                const f32 ny = std::min(st.elGizAnchorY, dpy);
                const f32 nw =
                    std::max(std::fabs(dpx - st.elGizAnchorX), 8.0f);
                const f32 nh =
                    std::max(std::fabs(dpy - st.elGizAnchorY), 8.0f);
                uiGizmoScaleToRect(e, {nx, ny, nw, nh}, sw, sh, ins);
            } else if (st.elGizmoMode == 2 && st.selElement >= 0 &&
                       st.selElement < static_cast<i32>(nLay)) {
                // 0.8.6 — RODAR: delta do ângulo do dedo à volta do centro
                // + snap 15° (o MESMO passo do snap de rotação 3D)
                UiElement& e =
                    canvas->elements[static_cast<size_t>(st.selElement)];
                const UiRect& sr =
                    lay[static_cast<size_t>(st.selElement)].rect;
                const f32 cx = sr.x + sr.w * 0.5f;
                const f32 cy = sr.y + sr.h * 0.5f;
                const f32 dpx = uiViewportToDesignX(t, px);
                const f32 dpy = uiViewportToDesignY(t, py);
                const f32 ang = uiGizmoAngleAt(cx, cy, dpx, dpy);
                e.rot = ui::uiGizmoSnapRot(st.elGizStartRot +
                                       (ang - st.elGizStartAng));
            } else if (st.selElement >= 0) {
                UiElement& e =
                    canvas->elements[static_cast<size_t>(st.selElement)];
                if (!e.parent.empty()) {
                    // 0.7.4 — FILHO de container: o drag DESLIGA-o do pai
                    // (vira elemento de topo NO SÍTIO onde está) e o gesto
                    // passa a mover normalmente — arrastar para fora do
                    // container é a forma natural de o tirar
                    uiDetachElement(*canvas, st.selElement, sw, sh, ins);
                }
                e.ox += dx;
                e.oy += dy;
            }
            st.elDragX = px;
            st.elDragY = py;
        } else {
            st.elDrag = false;
            st.elGizmoMode = 0;   // 0.8.6: o gesto termina em mover
        }
        return;
    }
    if (in.pressed(0)) {
        f32 px = 0.0f, py = 0.0f;
        in.pos(0, px, py);
        if (px >= view.x && px < view.x + view.w && py >= view.y &&
            py < view.y + view.h) {
            // 0.8.6 — os HANDLES do elemento selecionado têm PRIORIDADE
            // (o press num canto/pega NÃO desseleciona nem seleciona outro)
            if (hasSel) {
                bool gizmoHit = false;
                for (u32 cIdx = 0; cIdx < 4 && !gizmoHit; ++cIdx) {
                    const UiRect cr = ui::uiGizmoCornerRect(selRs, cIdx, 18.0f);
                    if (px >= cr.x && px < cr.x + cr.w && py >= cr.y &&
                        py < cr.y + cr.h) {
                        // canto OPOSTO = âncora fixa da escala (em DESIGN)
                        static const u32 kOpp[4] = {3, 2, 1, 0};
                        const UiRect opp =
                            ui::uiGizmoCornerRect(selRs, kOpp[cIdx], 0.0f);
                        st.elGizmoMode = 1;
                        st.elGizAnchorX = uiViewportToDesignX(t, opp.x);
                        st.elGizAnchorY = uiViewportToDesignY(t, opp.y);
                        gizmoHit = true;
                    }
                }
                const UiElement& se =
                    canvas->elements[static_cast<size_t>(st.selElement)];
                if (!gizmoHit && !uiElementIsContainer(se.kind)) {
                    const UiRect rh = ui::uiGizmoRotateHandleRect(selRs, 18.0f);
                    if (px >= rh.x && px < rh.x + rh.w && py >= rh.y &&
                        py < rh.y + rh.h) {
                        st.elGizmoMode = 2;
                        const UiRect& sr =
                            lay[static_cast<size_t>(st.selElement)].rect;
                        const f32 cx = sr.x + sr.w * 0.5f;
                        const f32 cy = sr.y + sr.h * 0.5f;
                        st.elGizStartAng = uiGizmoAngleAt(
                            cx, cy, uiViewportToDesignX(t, px),
                            uiViewportToDesignY(t, py));
                        st.elGizStartRot = se.rot;
                        gizmoHit = true;
                    }
                }
                if (gizmoHit) {
                    st.elDrag = true;
                    st.elDragX = px;
                    st.elDragY = py;
                    return;
                }
            }
            const f32 dx = uiViewportToDesignX(t, px);
            const f32 dy = uiViewportToDesignY(t, py);
            // o de CIMA ganha (hit-test reverso à ordem de desenho) — nos
            // rects RESOLVIDOS (containers incluídos: filhos selecionáveis);
            // 0.8.6: elementos RODADOS hit-testam no espaço do rect
            i32 hit = -1;
            for (i32 i = static_cast<i32>(canvas->elements.size()) - 1;
                 i >= 0; --i) {
                if (i >= static_cast<i32>(nLay)) {
                    continue;
                }
                if (!lay[i].shown) {
                    continue;
                }
                const UiRect& r = lay[i].rect;
                if (ui::uiRotatedRectHit(r, canvas->elements[i].rot, dx, dy)) {
                    hit = i;
                    break;
                }
            }
            st.selElement = hit;   // −1 = tap no vazio → desseleciona
            st.selJoystick = false;
            if (hit >= 0) {
                st.elDrag = true;
                st.elDragX = px;
                st.elDragY = py;
            } else if (joy && px >= joyX0 && px < joyX1 && py >= joyY0 &&
                       py < joyY1) {
                // o tap apanhou o PROXY do joystick (depois dos elementos)
                st.selJoystick = true;
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

// 0.7.4 — textura selecionável (tex: do Inspector; ref vive em e.image)
bool uiElementHasTexture(UiElement::Kind k) {
    return k == UiElement::Kind::Panel || k == UiElement::Kind::Button ||
           k == UiElement::Kind::Image;
}

// 0.7.4 — alinhamento transversal (Menu: texto; containers: filhos)
bool uiElementHasAlign(UiElement::Kind k) {
    return k == UiElement::Kind::Menu || uiElementIsContainer(k);
}

// 0.7.4 — mapa da escolha do "+" (modo UI) → Kind (−1 = joystick)
int uiPlusChoiceKind(int choice) {
    switch (choice) {
        case 1: case 2: case 3: case 4: case 5: case 6: case 7:
            return choice - 1;              // Panel..Article
        case 8:  return -1;                 // joystick (TouchControls)
        case 9:  return static_cast<int>(UiElement::Kind::VBox);
        case 10: return static_cast<int>(UiElement::Kind::HBox);
        default: return 0;
    }
}

u32 uiInspectorRowCount(const UiElement& e, const TextMetrics& m) {
    (void)m;
    const bool child = !e.parent.empty();   // filho: sem pos/âncoras (auto)
    u32 n = 1;                             // nome
    n += child ? 0 : 2;                    // X + Y (filho: posição é do container)
    n += 2;                                // W + H
    n += child ? 0 : 2;                    // ancora H + ancora V
    n += 3;                                // R + G + B
    ++n;                                   // 0.7.4: alpha do fundo
    ++n;                                   // visivel
    if (uiElementHasText(e.kind)) {
        ++n;     // texto
    }
    if (uiElementHasAction(e.kind)) {
        ++n;     // acao
        if (e.action != UiElement::Action::None) {
            ++n;   // alvo
        }
        if (e.action == UiElement::Action::TransitionScene) {
            ++n;   // 0.7.1: estilo fade|slide
        }
    }
    if (uiElementHasTexture(e.kind)) {
        ++n;     // 0.7.4: tex:
    }
    if (e.kind == UiElement::Kind::Menu || uiElementIsContainer(e.kind)) {
        ++n;     // 0.7.4: espaçamento
    }
    if (uiElementIsContainer(e.kind)) {
        ++n;     // 0.7.4: padding
    }
    if (uiElementHasAlign(e.kind)) {
        ++n;     // 0.7.4: alinhamento
    }
    ++n;         // 0.7.4: colocar em (container)
    ++n;         // remover
    return n;
}

u32 uiInspectorPlan(const UiElement& e, const TextMetrics& m, UiInspRow* rows,
                    u32 cap) {
    const f32 textH = inspTextRowH(m);
    const f32 btnH  = inspButtonRowH(m);
    const f32 sldH  = inspSliderRowH(m);
    const bool child = !e.parent.empty();

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
    if (!child) {
        push(UiInspRow::Kind::PosX, sldH, kUiInspX);
        push(UiInspRow::Kind::PosY, sldH, kUiInspY);
    }
    push(UiInspRow::Kind::SizeW, sldH, kUiInspW);
    push(UiInspRow::Kind::SizeH, sldH, kUiInspH);
    push(UiInspRow::Kind::ColR, sldH, kUiInspR);
    push(UiInspRow::Kind::ColG, sldH, kUiInspG);
    push(UiInspRow::Kind::ColB, sldH, kUiInspB);
    push(UiInspRow::Kind::ColA, sldH, kUiInspA);   // 0.7.4: alpha do fundo
    push(UiInspRow::Kind::HexBtn, btnH, kUiInspHex);   // 0.8.6: cor por hex
    push(UiInspRow::Kind::VisToggle, btnH, kUiInspVis);
    if (!child) {
        push(UiInspRow::Kind::AnchorH, btnH, kUiInspAnchH);
        push(UiInspRow::Kind::AnchorV, btnH, kUiInspAnchV);
    }
    if (uiElementHasText(e.kind)) {
        push(UiInspRow::Kind::TextBtn, btnH, kUiInspText);
        // 0.8.6 — TIPOGRAFIA do texto do elemento
        push(UiInspRow::Kind::FontScl, sldH, kUiInspFont);
        push(UiInspRow::Kind::TStyleBtn, btnH, kUiInspTStyle);
    }
    if (uiElementHasAction(e.kind)) {
        push(UiInspRow::Kind::ActType, btnH, kUiInspAct);
        if (e.action != UiElement::Action::None) {
            push(UiInspRow::Kind::ActTarget, btnH, kUiInspTarget);
        }
        if (e.action == UiElement::Action::TransitionScene) {
            push(UiInspRow::Kind::StyleBtn, btnH, kUiInspStyle);   // 0.7.1
        }
    }
    if (uiElementHasTexture(e.kind)) {
        push(UiInspRow::Kind::TexBtn, btnH, kUiInspTex);   // 0.7.4
    }
    if (e.kind == UiElement::Kind::Menu || uiElementIsContainer(e.kind)) {
        push(UiInspRow::Kind::Spacing, sldH, kUiInspSpacing);
    }
    if (uiElementIsContainer(e.kind)) {
        push(UiInspRow::Kind::Pad, sldH, kUiInspPad);
    }
    if (uiElementHasAlign(e.kind)) {
        push(UiInspRow::Kind::AlignBtn, btnH, kUiInspAlign);
    }
    push(UiInspRow::Kind::ParentBtn, btnH, kUiInspParent);   // 0.7.4
    push(UiInspRow::Kind::Remove, btnH, kUiInspRemove);
    return n;
}

f32 uiInspectorContentHeight(const UiElement& e, const TextMetrics& m) {
    UiInspRow rows[32];
    const u32 n = uiInspectorPlan(e, m, rows, 32);
    if (n == 0) {
        return 0.0f;
    }
    return rows[n - 1].y + rows[n - 1].h;
}

// ---- 0.7.3: plano do INSPECTOR DO JOYSTICK ---------------------------------
// pos X/Y (frações 0..1) + tamanho + sensibilidade + cor R/G/B + remover —
// o MESMO contrato de y cumulativo (linhas sequenciais, fonte única)
u32 uiJoystickPlan(const TextMetrics& m, UiInspRow* rows, u32 cap) {
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
        y += h;
    };

    push(UiInspRow::Kind::Name, textH, 0);
    push(UiInspRow::Kind::PosX, sldH, kJoyX);
    push(UiInspRow::Kind::PosY, sldH, kJoyY);
    push(UiInspRow::Kind::SizeW, sldH, kJoySize);   // payload: tamanho
    push(UiInspRow::Kind::Sens, sldH, kJoySens);
    push(UiInspRow::Kind::ColR, sldH, kJoyR);
    push(UiInspRow::Kind::ColG, sldH, kJoyG);
    push(UiInspRow::Kind::ColB, sldH, kJoyB);
    push(UiInspRow::Kind::Remove, btnH, kJoyRemove);
    return n;
}

f32 uiJoystickContentHeight(const TextMetrics& m) {
    UiInspRow rows[16];
    const u32 n = uiJoystickPlan(m, rows, 16);
    if (n == 0) {
        return 0.0f;
    }
    return rows[n - 1].y + rows[n - 1].h;
}

// 0.7.3 — INSPECTOR DO JOYSTICK (TouchControls editável): sliders pos
// X/Y (frações), tamanho, sensibilidade, cor R/G/B + remover. O plano
// (uiJoystickPlan) é a FONTE ÚNICA — o mesmo contrato de y cumulativo.
namespace {
bool drawJoystickInspector(UiContext& ui, EditorState& st, Tic* tic,
                           TouchControls* joy, f32 x, f32 y, f32 w, f32 h) {
    const TextMetrics tm = ui.textMetrics();
    ui.panel(x, y, w, h, theme::PANEL);
    ui.panel(x, y, 1.0f, h, theme::LINE);
    ui.label(x + kPad, y + kHeaderH * 0.5f + tm.block() * 0.30f,
             "INSPECTOR UI", theme::TEXT);
    ui.panel(x + kPad, y + kHeaderH - 1.0f, w - 2.0f * kPad, 1.0f, theme::LINE);

    char nameLine[64];
    std::snprintf(nameLine, sizeof(nameLine), "joystick  (%.24s)",
                  tic->name.c_str());

    UiInspRow plan[16];
    const u32 n = uiJoystickPlan(tm, plan, 16);
    const f32 contentH = uiJoystickContentHeight(tm);
    const f32 contentTop = y + kHeaderH + 4.0f;
    ui.beginScroll(kUiInspScrollId, {x, contentTop, w, h - kHeaderH - 4.0f},
                   contentH);
    const f32 off = ui.scrollOffset();

    bool edited = false;
    for (u32 i = 0; i < n; ++i) {
        const UiInspRow& r = plan[i];
        const f32 ry = contentTop + r.y - off;
        switch (r.kind) {
        case UiInspRow::Kind::Name:
            ui.labelFitted(x + kPad, inspBaseline(ry, r.h, tm), nameLine,
                           theme::ACCENT, w - 2.0f * kPad);
            break;
        case UiInspRow::Kind::PosX:
            if (uiSliderRow(ui, r.id, x, ry, r.h, tm, "pos X", 0.0f, 1.0f,
                            joy->relX, "%.2f")) {
                edited = true;
            }
            break;
        case UiInspRow::Kind::PosY:
            if (uiSliderRow(ui, r.id, x, ry, r.h, tm, "pos Y", 0.0f, 1.0f,
                            joy->relY, "%.2f")) {
                edited = true;
            }
            break;
        case UiInspRow::Kind::SizeW:   // payload: TAMANHO (escala do raio)
            if (uiSliderRow(ui, r.id, x, ry, r.h, tm, "tamanho", 0.4f, 2.0f,
                            joy->size, "%.2f")) {
                edited = true;
            }
            break;
        case UiInspRow::Kind::Sens:    // SENSIBILIDADE do joystick
            if (uiSliderRow(ui, r.id, x, ry, r.h, tm, "sensib.", 0.2f, 3.0f,
                            joy->sens, "%.2f")) {
                edited = true;
            }
            break;
        case UiInspRow::Kind::ColR:
            if (uiSliderRow(ui, r.id, x, ry, r.h, tm, "cor R", 0.0f, 1.0f,
                            joy->colR, "%.2f")) {
                edited = true;
            }
            break;
        case UiInspRow::Kind::ColG:
            if (uiSliderRow(ui, r.id, x, ry, r.h, tm, "cor G", 0.0f, 1.0f,
                            joy->colG, "%.2f")) {
                edited = true;
            }
            break;
        case UiInspRow::Kind::ColB:
            if (uiSliderRow(ui, r.id, x, ry, r.h, tm, "cor B", 0.0f, 1.0f,
                            joy->colB, "%.2f")) {
                edited = true;
            }
            break;
        case UiInspRow::Kind::Remove:
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      "remover joystick");
            break;
        default:
            break;
        }
    }
    ui.endScroll();

    // tap re-despachado → REMOVER (o único botão; sliders capturam sozinhos)
    f32 tx = 0.0f, ty = 0.0f;
    if (ui.scrollTap(kUiInspScrollId, tx, ty)) {
        for (u32 i = 0; i < n; ++i) {
            const UiInspRow& r = plan[i];
            if (r.kind != UiInspRow::Kind::Remove) {
                continue;
            }
            const f32 ry = contentTop + r.y - off;
            if (tx < x + kPad || tx >= x + w - kPad) {
                continue;
            }
            if (ty < ry + 2.0f || ty >= ry + r.h - 2.0f) {
                continue;
            }
            // remove o COMPONENTE do TIC (o InputMap continua; sem fonte)
            tic->removeComponent<TouchControls>();
            st.selJoystick = false;
            edited = true;
        }
    }
    return edited;
}
} // namespace

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
    // 0.7.3 — JOYSTICK selecionado: o painel mostra o INSPECTOR DO
    // JOYSTICK (pos/tamanho/sens/cor/remover do TouchControls do TIC)
    TouchControls* joySel =
        (tic && st.selJoystick) ? tic->getComponent<TouchControls>() : nullptr;
    if (joySel) {
        return drawJoystickInspector(ui, st, tic, joySel, x, y, w, h);
    }
    if (st.selJoystick) {
        st.selJoystick = false;   // o componente sumiu — cai no fluxo normal
    }
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
    UiInspRow plan[32];
    const u32 n = uiInspectorPlan(e, tm, plan, 32);
    const f32 contentH = uiInspectorContentHeight(e, tm);
    const f32 contentTop = y + kHeaderH + 4.0f;
    ui.beginScroll(kUiInspScrollId, {x, contentTop, w, h - kHeaderH - 4.0f},
                   contentH);
    const f32 off = ui.scrollOffset();

    bool edited = false;
    u32 colorIdx = 0;   // payload dos ColorSlider (0=R, 1=G, 2=B)

    // 0.7.4 — nome + container (filho) no cabeçalho
    char nameLine[96];
    if (!e.parent.empty()) {
        std::snprintf(nameLine, sizeof(nameLine), "%s  (%s em %s)",
                      e.name.c_str(), uiElementKindName(e.kind),
                      e.parent.c_str());
    } else {
        std::snprintf(nameLine, sizeof(nameLine), "%s  (%s)", e.name.c_str(),
                      uiElementKindName(e.kind));
    }

    // 0.7.4 — lista de containers do canvas p/ o botão "colocar em".
    // 0.8.9 (CRASH-PROOF): EXCLUI O PRÓPRIO elemento. O comentário antigo
    // dizia "inclui o próprio — o resolver guarda ciclos", mas só o sizeOf
    // tinha guard: o placeAt recursivo NÃO (0.8.8→0.8.9, crash do C33 por
    // stack exhaustion). Colocar um container DENTRO DE SI era um clique e
    // recursava infinitamente; agora a opção nem aparece (e o resolver tem
    // guard duplo por causa dos ficheiros já gravados com ciclo).
    std::vector<std::string> containers;
    for (const UiElement& o : canvas->elements) {
        if (uiElementIsContainer(o.kind) && o.name != e.name) {
            containers.push_back(o.name);
        }
    }

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
        case UiInspRow::Kind::ColA: {
            // 0.7.4 — ALPHA do fundo: 0 = sem fundo (Label default);
            // Menu = caixas ON/OFF; Panel/Button translúcidos se < 1
            if (uiSliderRow(ui, r.id, x, ry, r.h, tm, "fundo A", 0.0f, 1.0f,
                            e.color[3], "%.2f")) {
                edited = true;
            }
            break;
        }
        case UiInspRow::Kind::HexBtn: {
            // 0.8.6 — COR POR CÓDIGO: mostra o hex atual; tocar abre o
            // teclado (em modo hex — ver drawTextInput) para editar
            char hex[12];
            uiHexFormat(e.color, hex, sizeof(hex));
            char label[40];
            std::snprintf(label, sizeof(label), "hex: %s", hex);
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      label);
            break;
        }
        case UiInspRow::Kind::FontScl:
            // 0.8.6 — TAMANHO DA LETRA do texto do elemento (×base 28 px)
            if (uiSliderRow(ui, r.id, x, ry, r.h, tm, "letra", 0.5f, 3.0f,
                            e.fontScale, "%.2fx")) {
                edited = true;
            }
            break;
        case UiInspRow::Kind::TStyleBtn: {
            // 0.8.6 — ESTILO DA LETRA: normal → negrito → itálico → normal
            char label[48];
            std::snprintf(label, sizeof(label), "letra estilo: %s",
                          uiTextStyleName(e.textStyle));
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      label);
            break;
        }
        case UiInspRow::Kind::VisToggle: {
            char label[32];
            std::snprintf(label, sizeof(label), "visível: %s",
                          e.visible ? "sim" : "não");
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
            std::snprintf(label, sizeof(label), "ação: %s",
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
        case UiInspRow::Kind::StyleBtn: {
            // 0.7.1 — estilo da Scene.Transition: fade ↔ slide
            const bool slide =
                uiElementTransition(e) == UiElement::Transition::Slide;
            char label[40];
            std::snprintf(label, sizeof(label), "estilo: %s",
                          slide ? "slide" : "fade");
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      label);
            break;
        }
        case UiInspRow::Kind::TexBtn: {
            // 0.7.4 — "tex: …" (Panel/Button/Image): abre o seletor de
            // TEXTURAS DO ELEMENTO (menuKind 3; com "importar…")
            char label[80];
            std::snprintf(label, sizeof(label), "tex: %s",
                          e.image.empty()
                              ? "none"
                              : e.image.substr(
                                    e.image.find_last_of('/') == std::string::npos
                                        ? 0
                                        : e.image.find_last_of('/') + 1)
                                    .c_str());
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      label);
            break;
        }
        case UiInspRow::Kind::Spacing:
            // 0.7.4 — espaçamento entre itens (Menu) / filhos (containers)
            if (uiSliderRow(ui, r.id, x, ry, r.h, tm, "espaco", 0.0f, 40.0f,
                            e.spacing, "%.0f")) {
                edited = true;
            }
            break;
        case UiInspRow::Kind::Pad:
            // 0.7.4 — resguardo interno do container
            if (uiSliderRow(ui, r.id, x, ry, r.h, tm, "pad", 0.0f, 40.0f,
                            e.pad, "%.0f")) {
                edited = true;
            }
            break;
        case UiInspRow::Kind::AlignBtn: {
            // 0.7.4 — alinhamento (Menu: texto; containers: filhos)
            char label[48];
            std::snprintf(label, sizeof(label), "alinhamento: %s",
                          uiAlignName(e.align));
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      label);
            break;
        }
        case UiInspRow::Kind::ParentBtn: {
            // 0.7.4 — "colocar em": cicla (nenhum) → containers → (nenhum)
            char label[80];
            std::snprintf(label, sizeof(label), "colocar em: %s",
                          e.parent.empty() ? "(nenhum)" : e.parent.c_str());
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
                r.kind == UiInspRow::Kind::ColB || r.kind == UiInspRow::Kind::ColA ||
                r.kind == UiInspRow::Kind::FontScl ||
                r.kind == UiInspRow::Kind::Spacing || r.kind == UiInspRow::Kind::Pad ||
                r.kind == UiInspRow::Kind::Name) {
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
                        e.anchorV, ui.screenWidth(), ui.screenHeight(),
                        ui.safeArea().left, ui.safeArea().top,
                        ui.safeArea().right, ui.safeArea().bottom);
                    edited = true;
                    break;
                case UiInspRow::Kind::AnchorV:
                    canvas->setAnchor(
                        st.selElement, e.anchorH,
                        static_cast<UiElement::AnchorV>(
                            (static_cast<u32>(e.anchorV) + 1u) % 3u),
                        ui.screenWidth(), ui.screenHeight(),
                        ui.safeArea().left, ui.safeArea().top,
                        ui.safeArea().right, ui.safeArea().bottom);
                    edited = true;
                    break;
                case UiInspRow::Kind::TextBtn:
                    openTextInput(st, 2, st.selected, st.selElement,
                                  e.text.c_str());
                    break;
                case UiInspRow::Kind::HexBtn:
                    // 0.8.6 — teclado em MODO HEX (propósito 4) com o hex
                    // atual pré-carregado; inválido = estado fica como estava
                    {
                        char cur[12];
                        uiHexFormat(e.color, cur, sizeof(cur));
                        openTextInput(st, 4, st.selected, st.selElement, cur);
                    }
                    break;
                case UiInspRow::Kind::TStyleBtn:
                    // 0.8.6 — normal → negrito → itálico → normal
                    e.textStyle = static_cast<UiElement::TextStyle>(
                        (static_cast<u32>(e.textStyle) + 1u) % 3u);
                    edited = true;
                    break;
                case UiInspRow::Kind::ActType: {
                    // cicla none → show → hide → toggle → scene → spawn →
                    // trans (0.7.1) → none
                    const u32 next = (static_cast<u32>(e.action) + 1u) % 7u;
                    e.action = static_cast<UiElement::Action>(next);
                    edited = true;
                    break;
                }
                case UiInspRow::Kind::ActTarget:
                    openTextInput(st, 3, st.selected, st.selElement,
                                  e.target.c_str());
                    break;
                case UiInspRow::Kind::StyleBtn:
                    // 0.7.1 — fade ↔ slide (o default é fade)
                    e.param =
                        uiElementTransition(e) == UiElement::Transition::Slide
                            ? "fade"
                            : "slide";
                    edited = true;
                    break;
                case UiInspRow::Kind::TexBtn:
                    // 0.7.4 — abre o seletor de textura DO ELEMENTO
                    // (menuKind 3: none + ficheiros + "importar…")
                    st.assetMenu = 3;
                    break;
                case UiInspRow::Kind::AlignBtn:
                    // 0.7.4 — start → center → end → start
                    e.align = static_cast<UiElement::Align>(
                        (static_cast<u32>(e.align) + 1u) % 3u);
                    edited = true;
                    break;
                case UiInspRow::Kind::ParentBtn: {
                    // 0.7.4 — cicla (nenhum) → containers → (nenhum). Ao
                    // SAIR de um container, o elemento fica NO SÍTIO onde
                    // estava (uiDetachElement conserva a posição visual)
                    if (e.parent.empty()) {
                        if (!containers.empty()) {
                            e.parent = containers[0];
                        }
                    } else {
                        size_t ci = 0;
                        for (size_t k = 0; k < containers.size(); ++k) {
                            if (containers[k] == e.parent) {
                                ci = k;
                                break;
                            }
                        }
                        if (ci + 1 < containers.size()) {
                            e.parent = containers[ci + 1];
                        } else {
                            // sai do último → (nenhum), posição conservada
                            uiDetachElement(*canvas, st.selElement,
                                            ui.screenWidth(),
                                            ui.screenHeight(), ui.safeArea());
                        }
                    }
                    edited = true;
                    break;
                }
                case UiInspRow::Kind::Remove:
                    canvas->elements.erase(
                        canvas->elements.begin() + st.selElement);
                    st.selElement = -1;
                    edited = true;
                    break;
                default:
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
        // 0.7.5 — SEM TIC selecionado (modo UI): assegura/cria o TIC DE UI
        // próprio ("UI", só com UiCanvas — sem mesh/body). Criar UI nunca
        // obrigou a selecionar/criar um TIC 3D (o fix do C33).
        if (!st.uiMode) {
            return false;
        }
        const Handle h = ensureUiTic(scene, st);
        tic = scene.get(h);
        if (!tic) {
            return false;
        }
    }
    UiCanvas* canvas = tic->getComponent<UiCanvas>();
    if (!canvas) {
        canvas = tic->addComponent<UiCanvas>();
    }
    if (!canvas) {
        return false;
    }
    if (kind > 8) {
        return false;   // 0..3 = base (0.7.0); 4..6 = compostos (0.7.3);
                        // 7..8 = containers VBox/HBox (0.7.4)
    }
    const UiElement::Kind k = static_cast<UiElement::Kind>(kind);
    const i32 idx = canvas->addElement(k, sw, sh);
    // 0.7.4 — com um CONTAINER selecionado (ou um FILHO dele), o elemento
    // novo nasce FILHO do mesmo container (irmão do selecionado); o layout
    // passa a dispo-lo automaticamente
    if (st.selElement >= 0 &&
        st.selElement < static_cast<i32>(canvas->elements.size()) &&
        idx >= 0 && idx < static_cast<i32>(canvas->elements.size())) {
        const UiElement& sel = canvas->elements[static_cast<size_t>(st.selElement)];
        UiElement& fresh = canvas->elements[static_cast<size_t>(idx)];
        if (!sel.parent.empty() || uiElementIsContainer(sel.kind)) {
            fresh.parent = sel.parent.empty() ? sel.name : sel.parent;
        }
    }
    st.selElement = idx;
    st.selJoystick = false;   // a seleção passou para o elemento novo
    return true;
}

// 0.7.5 — TIC DE UI próprio: o "UI" que hospeda o canvas quando o dono cria
// UI sem TIC 3D selecionado. Procura por NOME (o renomeado deixa de ser
// achado — um novo nasce; comportamento previsível), cria se não existir,
// garante o UiCanvas e seleciona-o. Sem mesh/body — é SÓ UI.
Handle ensureUiTic(Scene& scene, EditorState& st) {
    Handle h = scene.find("UI");
    Tic* t = scene.get(h);
    if (!t) {
        h = scene.create("UI");
        t = scene.get(h);
    }
    if (!t) {
        return Handle::invalid();
    }
    if (!t->getComponent<UiCanvas>()) {
        t->addComponent<UiCanvas>();
    }
    st.selected = h;
    st.selElement = -1;
    return h;
}

// 0.7.4 — desliga o elemento do container CONSERVANDO a posição visual:
// âncoras → esquerda/topo e ox/oy derivados do rect RESOLVIDO atual (o
// elemento fica exatamente onde estava, agora livre)
void uiDetachElement(UiCanvas& c, i32 element, f32 sw, f32 sh,
                     const safe::Insets& ins) {
    if (element < 0 || element >= static_cast<i32>(c.elements.size())) {
        return;
    }
    UiElement& e = c.elements[static_cast<size_t>(element)];
    if (e.parent.empty()) {
        return;   // já é topo — nada a fazer
    }
    ui::CanvasLayout lay[32];
    const u32 cap = c.elements.size() < 32
                        ? static_cast<u32>(c.elements.size())
                        : 32;
    ui::resolveCanvasLayout(c, sw, sh, ins, lay, cap);
    const UiRect r =
        element < static_cast<i32>(cap) ? lay[static_cast<size_t>(element)].rect
                                        : UiRect{};
    e.parent.clear();
    e.anchorH = UiElement::AnchorH::Left;   // Left/Top = posição absoluta
    e.anchorV = UiElement::AnchorV::Top;
    e.ox = r.x - ins.left;   // elementRect(Left/Top) = ins + ox → ox = r − ins
    e.oy = r.y - ins.top;
}

// 0.8.6 — reancora o elemento para o rect de design dado (o inverso de
// elementRect, as 6 combinações de âncora; usado pelo gizmo de ESCALAR:
// w/h novos + ox/oy derivados de modo que o rect FIQUE onde o gizmo pôs)
void uiGizmoScaleToRect(UiElement& e, const UiRect& nr,
                        f32 sw, f32 sh, const safe::Insets& ins) {
    e.w = nr.w;
    e.h = nr.h;
    switch (e.anchorH) {
        case UiElement::AnchorH::Left:
            e.ox = nr.x - ins.left;              // x = ins.left + ox
            break;
        case UiElement::AnchorH::Center:
            e.ox = nr.x - sw * 0.5f;             // x = sw*0.5 + ox
            break;
        case UiElement::AnchorH::Right:
            e.ox = nr.x + nr.w - (sw - ins.right);   // x = sw−ins.right−w+ox
            break;
    }
    switch (e.anchorV) {
        case UiElement::AnchorV::Top:
            e.oy = nr.y - ins.top;               // y = ins.top + oy
            break;
        case UiElement::AnchorV::Middle:
            e.oy = nr.y - sh * 0.5f;             // y = sh*0.5 + oy
            break;
        case UiElement::AnchorV::Bottom:
            e.oy = nr.y + nr.h - (sh - ins.bottom);  // y = sh−ins.bottom−h+oy
            break;
    }
}

// ---------------------------------------------------------------------------
// GESTÃO DE TICs — menu contextual + diálogo de remoção + duplicar
// ---------------------------------------------------------------------------

int drawContextMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                    EditorState& st, const char* ticName, bool ticVisible,
                    bool hasCamera) {
    const f32 w = kMenuW;
    // 0.7.7: câmaras ganham "Alinhar a vista" (copia a pose da orbit de
    // edição para o transform da câmara)
    const int kItems = hasCamera ? 5 : 4;
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

    // 0.9.6.6 (GRUPO C · CANTOS SUAVIZADOS): o CARD modal com raios 8dp
    // (spec A) — moldura idem (os full-bleed hierarchy/drawer ficam retos:
    // são superfícies de ecrã, não cards)
    ui.panelRounded(x, y, w, h, theme::dp(theme::kRadiusCard), theme::PANEL);
    ui.frameRounded(x, y, w, h, 2.0f, theme::dp(theme::kRadiusCard),
                    theme::ACCENT);
    const f32 th = ui.fontHeight();
    char title[64];
    std::snprintf(title, sizeof(title), "%.40s", ticName ? ticName : "?");
    ui.labelFitted(x + kPad, y + kHeaderH * 0.5f + th * 0.30f, title,
                   theme::TEXT, w - 2.0f * kPad);

    char visLabel[48];
    std::snprintf(visLabel, sizeof(visLabel), "Visibilidade: %s",
                  ticVisible ? "esconder" : "mostrar");
    const char* labels[5] = {"Renomear", "Remover", "Duplicar",
                             visLabel, "Alinhar a vista"};
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
                   "Os componentes são apagados (não há desfazer).",
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

const char* KeyboardLayout::keyLabel(u32 row, u32 col, bool lower) {
    // 0.7.5: `lower` devolve MINÚSCULAS (o toggle abc/ABC do teclado —
    // o atlas tem ambos os casos; dígitos/'_' não mudam).
    // BUG LATENTE 0.7.0 apanhado pelo teste novo: a linha 2 tinha SÓ 8
    // inicializadores (faltava o 'Z') — a 9ª tecla era um ponteiro NULL
    // (tecla fantasma que crashava o typeChar ao tocar). Alfabeto completo:
    // A..R (2×9) + S..Z (8) + '_' = 9 na linha 2.
    static const char* kRow0[9] = {"A", "B", "C", "D", "E", "F", "G", "H", "I"};
    static const char* kRow1[9] = {"J", "K", "L", "M", "N", "O", "P", "Q", "R"};
    static const char* kRow2[9] = {"S", "T", "U", "V", "W", "X", "Y", "Z", "_"};
    static const char* kRow3[10] = {"0", "1", "2", "3", "4",
                                    "5", "6", "7", "8", "9"};
    static const char* kRow0l[9] = {"a", "b", "c", "d", "e", "f", "g", "h", "i"};
    static const char* kRow1l[9] = {"j", "k", "l", "m", "n", "o", "p", "q", "r"};
    static const char* kRow2l[9] = {"s", "t", "u", "v", "w", "x", "y", "z", "_"};
    switch (row) {
        case 0:  return col < 9 ? (lower ? kRow0l[col] : kRow0[col]) : "";
        case 1:  return col < 9 ? (lower ? kRow1l[col] : kRow1[col]) : "";
        case 2:  return col < 9 ? (lower ? kRow2l[col] : kRow2[col]) : "";   // 8 letras + '_'
        default: return col < 10 ? kRow3[col] : "";   // dígitos (sem caso)
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
    // espaçamento). 0.7.5: + a tecla de CASO abc/ABC (o espaço encolhe de
    // 3u para 2u): [ESPACO 2u][abc 1u][ '-' 1u][APAGA 2u][OK 2u][CANCELAR 1u]
    const f32 yB = k.dialog.y + kHeaderH + kBufH + 4.0f * (kKeyH + kGap);
    const f32 unit = (innerW - 4.0f * kGap) / 10.0f;
    f32 x = k.dialog.x + kPad;
    k.space = {x, yB, 2.0f * unit, kKeyH};
    x += 2.0f * unit + kGap;
    k.caseKey = {x, yB, unit, kKeyH};   // 0.7.5: toggle abc/ABC
    x += unit + kGap;
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

// ---- 0.8.6 — COR POR CÓDIGO HEX (#RRGGBB) ----------------------------------

// formata a cor (RGB 0..1) como "#RRGGBB" (maiúsculas — o atlas é upper)
void uiHexFormat(const f32 rgb[3], char* out, u32 cap) {
    if (!out || cap == 0) {
        return;
    }
    const int r = std::clamp(int(rgb[0] * 255.0f + 0.5f), 0, 255);
    const int g = std::clamp(int(rgb[1] * 255.0f + 0.5f), 0, 255);
    const int b = std::clamp(int(rgb[2] * 255.0f + 0.5f), 0, 255);
    std::snprintf(out, cap, "#%02X%02X%02X", r, g, b);
}

// parse de "#RRGGBB" (ou "RRGGBB" sem cardinal; case-insensitive).
// true = ok (out[0..2] = RGB 0..1, out[3] intocado); false = inválido.
bool uiHexParse(const char* s, f32 out[4]) {
    if (!s || !out) {
        return false;
    }
    if (s[0] == '#') {
        ++s;
    }
    size_t n = std::strlen(s);
    if (n != 6) {
        return false;
    }
    u32 v = 0;
    for (size_t i = 0; i < n; ++i) {
        const char c = s[i];
        u32 d;
        if (c >= '0' && c <= '9') {
            d = static_cast<u32>(c - '0');
        } else if (c >= 'A' && c <= 'F') {
            d = static_cast<u32>(c - 'A' + 10);
        } else if (c >= 'a' && c <= 'f') {
            d = static_cast<u32>(c - 'a' + 10);
        } else {
            return false;
        }
        v = (v << 4) | d;
    }
    out[0] = static_cast<f32>((v >> 16) & 0xFF) / 255.0f;
    out[1] = static_cast<f32>((v >> 8) & 0xFF) / 255.0f;
    out[2] = static_cast<f32>(v & 0xFF) / 255.0f;
    return true;
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
    // 0.7.5 — as teclas de LETRAS seguem o CASO ativo (st.kbLower)
    for (u32 row = 0; row < 4; ++row) {
        for (u32 col = 0; col < k.keyCount[row]; ++col) {
            const UiRect& r = k.key[row][col];
            if (ui.button(kKbBase + static_cast<u64>(row) * 10u +
                              static_cast<u64>(col),
                          r.x, r.y, r.w, r.h,
                          KeyboardLayout::keyLabel(row, col, st.kbLower))) {
                typeChar(KeyboardLayout::keyLabel(row, col, st.kbLower)[0]);
            }
        }
    }
    if (ui.button(kKbSpaceId, k.space.x, k.space.y, k.space.w, k.space.h,
                  "ESPACO")) {
        typeChar(' ');
    }
    // 0.7.5 — toggle de CASO abc/ABC: o rótulo mostra o ESTADO SEGUINTE
    // (maiúsculas ativas → "abc" disponível); NÃO escreve no buffer.
    // 0.8.6: em modo HEX (propósitos 4/5) a tecla vira "#" (o cardinal do
    // código) — a geometria é a mesma, o CARÁTER é do modo.
    // 0.8.9: em modo NUMÉRICO (propósito 6) a tecla vira "-" (sinal) e a
    // tecla do traço vira "." (decimal) — dígitos estão na linha de baixo.
    const bool hexMode = st.textPurpose == 4 || st.textPurpose == 5;
    const bool numMode = st.textPurpose == 6;
    if (ui.button(kKbCaseId, k.caseKey.x, k.caseKey.y, k.caseKey.w,
                  k.caseKey.h,
                  numMode ? "-" : (hexMode ? "#" : (st.kbLower ? "ABC" : "abc")))) {
        if (numMode) {
            typeChar('-');
        } else if (hexMode) {
            typeChar('#');
        } else {
            st.kbLower = !st.kbLower;
        }
    }
    if (ui.button(kKbDashId, k.dash.x, k.dash.y, k.dash.w, k.dash.h,
                  numMode ? "." : (hexMode ? "." : "-"))) {
        typeChar(numMode ? '.' : (hexMode ? '.' : '-'));   // 0.8.9: num/hex têm PONTO; texto tem traço
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
    // 0.9.2: nas PESQUISAS (8 hierarquia, 9 Docs) o texto vazio é VÁLIDO —
    // limpa o filtro (nos restantes propósitos o vazio não se aplica)
    if (st.textLen == 0 && st.textPurpose != 8 && st.textPurpose != 9) {
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
        case 4: {   // 0.8.6: cor do ELEMENTO por hex (#RRGGBB)
            Tic* tic = scene.get(st.textTic);
            UiCanvas* canvas = tic ? tic->getComponent<UiCanvas>() : nullptr;
            if (!canvas || st.textElement < 0 ||
                st.textElement >= static_cast<i32>(canvas->elements.size())) {
                return false;
            }
            f32 rgba[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            if (!uiHexParse(st.textBuf, rgba)) {
                return false;   // inválido: o estado fica como estava
            }
            UiElement& e = canvas->elements[static_cast<size_t>(st.textElement)];
            e.color[0] = rgba[0];
            e.color[1] = rgba[1];
            e.color[2] = rgba[2];
            return true;
        }
        case 5: {   // 0.8.6: cor do TIC (tint do MeshRenderer) por hex
            Tic* tic = scene.get(st.textTic);
            MeshRenderer* mr = tic ? tic->getComponent<MeshRenderer>() : nullptr;
            if (!mr) {
                return false;
            }
            f32 rgba[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            if (!uiHexParse(st.textBuf, rgba)) {
                return false;
            }
            mr->tint[0] = rgba[0];
            mr->tint[1] = rgba[1];
            mr->tint[2] = rgba[2];
            return true;
        }
        case 6: {   // 0.8.9 — CAMPO NUMÉRICO SEM TETO do Inspector (Transform3D)
            // st.textElement = índice do campo: 0..2 pos, 3..5 rot (graus),
            // 6..8 escala. strtof EXIGE consumo total; não-finito/inválido =
            // estado intacto (o mesmo contrato do hex). Escala tem piso
            // 0.001 (0/negativo degeneraria o mesh); pos/rot são livres.
            Tic* tic = scene.get(st.textTic);
            Transform3D* tr =
                tic ? tic->getComponent<Transform3D>() : nullptr;
            if (!tr || st.textElement < 0 || st.textElement > 8) {
                return false;
            }
            char* end = nullptr;
            const f32 v = std::strtof(st.textBuf, &end);
            if (end == st.textBuf || *end != '\0' || !std::isfinite(v)) {
                return false;   // inválido: fica como estava
            }
            switch (st.textElement) {
                case 0: tr->pos.x = v; break;
                case 1: tr->pos.y = v; break;
                case 2: tr->pos.z = v; break;
                case 3:   // rot: graus → quat (mesma conversão dos sliders)
                case 4:
                case 5: {
                    f32 ex = 0.0f, ey = 0.0f, ez = 0.0f;
                    Quat::toEuler(tr->rot, ex, ey, ez);
                    if (st.textElement == 3) ex = deg2radLocal(v);
                    else if (st.textElement == 4) ey = deg2radLocal(v);
                    else ez = deg2radLocal(v);
                    tr->rot = Quat::fromEuler(ex, ey, ez);
                    break;
                }
                default: {   // 6..8: escala (piso 0.001)
                    const f32 sv = v < 0.001f ? 0.001f : v;
                    if (st.textElement == 6) tr->scale.x = sv;
                    else if (st.textElement == 7) tr->scale.y = sv;
                    else tr->scale.z = sv;
                    break;
                }
            }
            tr->updateWorld();
            return true;
        }
        case 8: {   // 0.9.0 — pesquisa da hierarquia. BUG 0.9.2: o COMMIT
            // FALTAVA (o campo abria o teclado mas hierSearch nunca recebia
            // o texto — a pesquisa só funcionava por estado direto nos
            // testes). O commit é AQUI: textBuf → hierSearch.
            std::snprintf(st.hierSearch, sizeof(st.hierSearch), "%s",
                          st.textBuf);
            st.hierSearchLen = st.textLen;
            return true;
        }
        case 9: {   // 0.9.2 — pesquisa das Docs V.ONI (§11)
            std::snprintf(st.docsScreen.query, sizeof(st.docsScreen.query),
                          "%s", st.textBuf);
            st.docsScreen.queryLen = st.textLen;
            st.docsScreen.expanded = -1;
            return true;
        }
        default:
            return false;   // propósito 1 (nome de cena): o chamador (0.7.1)
    }
}


// ---------------------------------------------------------------------------
// 0.7.2 — NAVEGADOR DE FICHEIROS + APLICAR-APÓS-IMPORT
// ---------------------------------------------------------------------------

std::string browserPathLabel(const std::string& cwd, f32 maxW,
                             f32 (*measure)(const std::string&, void*),
                             void* user) {
    if (!measure || cwd.empty()) {
        return cwd;
    }
    if (measure(cwd, user) <= maxW) {
        return cwd;   // cabe inteiro — o caminho TODO visível
    }
    // não cabe: corta o INÍCIO e guarda o FIM (o dono quer ver ONDE está:
    // "...DCIM/Camera" diz mais que "storage/emulated/0/D...")
    const std::string dots = "...";
    if (measure(dots, user) > maxW) {
        return "";   // nem as reticências cabem — nada a mostrar
    }
    size_t lo = 0;
    size_t hi = cwd.size();
    while (lo + 1 < hi) {
        const size_t mid = (lo + hi) / 2;
        const std::string cand = dots + cwd.substr(mid);
        if (measure(cand, user) <= maxW) {
            hi = mid;   // ainda cabe — pode cortar mais do início
        } else {
            lo = mid;   // não cabe — corta mais ainda
        }
    }
    const std::string out =
        hi < cwd.size() ? dots + cwd.substr(hi) : cwd;
    return measure(out, user) <= maxW ? out : dots;   // defesa numérica
}

std::string browserEmptyMessage(const std::string& cwd, bool opendirFailed) {
    // COM O CAMINHO — nunca um toast cego (o dono vê onde procurou)
    char buf[160];
    std::snprintf(buf, sizeof(buf), "%s: %s",
                  opendirFailed ? "(sem acesso)" : "(vazio)", cwd.c_str());
    return buf;
}

int drawFileBrowser(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                    EditorState& st, const std::string& cwd,
                    const std::vector<fileapi::DirEntry>& entries,
                    bool opendirFailed) {
    // painel GRANDE (o browser precisa de espaço): 92% da área útil
    // 0.9.6.6 (GRUPO C): dp REAL em TUDO (eram px crus — no device 2.0 o
    // browser saía a metade; a auditoria do Grupo B media fechar 96×36,
    // as 6 raízes 139,7×40 e o subir 868×44 — TODOS <48dp, avisos)
    const f32 ox = ui.safeLeft();
    const f32 oy = ui.safeTop();
    const f32 aw = sw - ox - ui.safeRight();
    const f32 ah = sh - oy - ui.safeBottom();
    const f32 w = aw * 0.92f > theme::dp(900.0f) ? theme::dp(900.0f)
                                                 : aw * 0.92f;
    const f32 rowH = theme::dp(48.0f);
    constexpr u32 kMaxRows = 8;   // linhas visíveis (o scroll revela o resto)
    const u32 shown = entries.size() < kMaxRows
                           ? static_cast<u32>(entries.size())
                           : kMaxRows;
    const f32 listH = static_cast<f32>(shown) * rowH;
    const f32 h = kHeaderH + theme::dp(34.0f) /*caminho*/ +
                  theme::dp(52.0f) /*raízes*/ + theme::dp(8.0f) +
                  theme::dp(52.0f) /*subir*/ + theme::dp(8.0f) + listH + kPad;
    const f32 x = ox + (aw - w) * 0.5f;
    const f32 y = oy + (ah - h) * 0.5f;

    // toque fora fecha (cancela o import)
    f32 px = -1.0f, py = -1.0f;
    if (in.pressed(0)) {
        in.pos(0, px, py);
    }
    if (in.pressed(0) &&
        !(px >= x && px < x + w && py >= y && py < y + h)) {
        st.fileBrowser = false;
        return 0;
    }

    // 0.9.6.6 (GRUPO C · CANTOS SUAVIZADOS): o CARD modal com raios 8dp
    // (spec A) — moldura idem (os full-bleed hierarchy/drawer ficam retos:
    // são superfícies de ecrã, não cards)
    ui.panelRounded(x, y, w, h, theme::dp(theme::kRadiusCard), theme::PANEL);
    ui.frameRounded(x, y, w, h, 2.0f, theme::dp(theme::kRadiusCard),
                    theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.label(x + kPad, y + kHeaderH * 0.5f + th * 0.30f, "NAVEGADOR",
             theme::TEXT);
    // o FECHAR: alvo 48dp REAL (era 96×36 px crus — aviso <48dp do Grupo B)
    if (ui.button(kBrowserCloseId, x + w - kPad - theme::dp(96.0f),
                  y + (kHeaderH - theme::dp(48.0f)) * 0.5f, theme::dp(96.0f),
                  theme::dp(48.0f), "fechar")) {
        st.fileBrowser = false;
        return 0;
    }

    // CAMINHO NO TOPO — mostra ONDE procura (corta o início quando longo)
    const f32 pathY = y + kHeaderH + theme::dp(6.0f);
    ui.panel(x + kPad, pathY - theme::dp(18.0f), w - 2.0f * kPad,
             theme::dp(30.0f), theme::BG);
    const TextMetrics tm = ui.textMetrics();
    const std::string pathText = browserPathLabel(
        cwd, w - 2.0f * kPad - theme::dp(16.0f),
        [](const std::string& s, void* user) -> f32 {
            return static_cast<UiContext*>(user)->fontWidth(s.c_str());
        },
        &ui);
    ui.labelFitted(x + kPad + theme::dp(8.0f), pathY + tm.ascent,
                   pathText.c_str(), theme::ACCENT,
                   w - 2.0f * kPad - theme::dp(16.0f));

    // raízes: [Raiz][Download][Docs][Camera][Pictures][Music] (galeria + a
    // raiz de ÁUDIO do dono 0.8.11; a largura reparte por TODAS) — alvo
    // 48dp REAL (eram 40px crus; a largura reparte-se na mesma)
    const f32 rootsY = pathY + theme::dp(34.0f) - theme::dp(18.0f) +
                       theme::dp(12.0f);
    const f32 rootW = (w - 2.0f * kPad -
                       static_cast<f32>(fileapi::kBrowserRootCount - 1) *
                           theme::dp(6.0f)) /
                      static_cast<f32>(fileapi::kBrowserRootCount);
    int chosen = 0;
    for (int i = 0; i < fileapi::kBrowserRootCount; ++i) {
        if (ui.button(kBrowserRootBase + static_cast<u64>(i),
                      x + kPad + static_cast<f32>(i) * (rootW + theme::dp(6.0f)),
                      rootsY, rootW, theme::dp(48.0f),
                      fileapi::kBrowserRoots[i].label)) {
            chosen = i + 1;
        }
    }

    // subir (o pai; na raiz não faz nada — o main trata) — alvo 48dp REAL
    // 0.8.11: o nº vem DEPOIS das raízes (count+1) — com a 6ª raiz (Music)
    // o fixo "6" colidia: tocar Subir saltava para o Music
    const f32 upY = rootsY + theme::dp(52.0f);
    if (ui.button(kBrowserUpId, x + kPad, upY, w - 2.0f * kPad,
                  theme::dp(48.0f), "^ Subir")) {
        chosen = fileapi::kBrowserRootCount + 1;
    }

    // lista: diretorias primeiro (ordem do listDirEntries); scroll id 45
    const f32 listTop = upY + theme::dp(52.0f);
    const UiRect region{x, listTop, w, listH};
    const f32 contentH = static_cast<f32>(entries.size()) * rowH;
    ui.beginScroll(kBrowserScrollId, region, contentH);
    const f32 off = ui.scrollOffset();
    for (size_t i = 0; i < entries.size(); ++i) {
        const f32 ry = listTop + static_cast<f32>(i) * rowH - off;
        char label[72];
        if (entries[i].isDir) {
            std::snprintf(label, sizeof(label), "/ %s",
                          entries[i].name.c_str());
        } else if (entries[i].kind == 'm') {
            std::snprintf(label, sizeof(label), "mesh: %s",
                          entries[i].name.c_str());
        } else if (entries[i].kind == 't') {
            std::snprintf(label, sizeof(label), "tex: %s",
                          entries[i].name.c_str());
        } else if (entries[i].kind == 's') {
            // 0.8.11 — ÁUDIO (.wav/.ogg/.mp3 → importa p/ .gi)
            std::snprintf(label, sizeof(label), "audio: %s",
                          entries[i].name.c_str());
        } else {
            // 0.8.5: formato fora de obj/gltf/glb/png — VISÍVEL (o tap dá
            // o erro claro no main; o "?" marca o que a engine não lê)
            std::snprintf(label, sizeof(label), "? %s",
                          entries[i].name.c_str());
        }
        ui.button(kBrowserRowBase + static_cast<u64>(i), x + kPad,
                  ry + theme::dp(2.0f), w - 2.0f * kPad, rowH - theme::dp(4.0f),
                  label);   // só desenha (scroll)
    }
    ui.endScroll();
    if (entries.empty()) {
        // mensagem COM O CAMINHO (nunca toast cego)
        const std::string msg = browserEmptyMessage(cwd, opendirFailed);
        ui.labelFitted(x + kPad, listTop + rowH * 0.5f + tm.ascent,
                       msg.c_str(), theme::LINE, w - 2.0f * kPad);
    }

    // tap re-despachado → linha da lista (a MESMA geometria desenhada)
    // 0.8.11: as linhas começam DEPOIS de raízes+subir (count+2)
    f32 tx = 0.0f, ty = 0.0f;
    if (chosen == 0 && ui.scrollTap(kBrowserScrollId, tx, ty)) {
        const i32 row = static_cast<i32>((ty - listTop + off) / rowH);
        if (row >= 0 && static_cast<u32>(row) < entries.size()) {
            chosen = row + fileapi::kBrowserRootCount + 2;
        }
    }
    return chosen;
}

int drawApplyDialog(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                    EditorState& st, const char* fileName, const char* ticName) {
    const f32 h = storageDialogHeight();   // título + linhas + 2 botões
    const f32 ox = ui.safeLeft();
    const f32 oy = ui.safeTop();
    const f32 aw = sw - ox - ui.safeRight();
    const f32 ah = sh - oy - ui.safeBottom();
    const UiRect dlg = centeredMenuRect(ox, oy, aw, ah, h);

    // toque fora = Nao (o import já ficou feito; só não se aplica)
    f32 px = -1.0f, py = -1.0f;
    if (in.pressed(0)) {
        in.pos(0, px, py);
    }
    if (in.pressed(0) &&
        !(px >= dlg.x && px < dlg.x + dlg.w && py >= dlg.y &&
          py < dlg.y + dlg.h)) {
        st.applyAsk = false;
        return 2;
    }

    ui.panel(dlg.x, dlg.y, dlg.w, dlg.h, theme::PANEL);
    ui.frame(dlg.x, dlg.y, dlg.w, dlg.h, 2.0f, theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.label(dlg.x + kPad, dlg.y + kHeaderH * 0.5f + th * 0.30f,
             "APLICAR AO TIC?", theme::TEXT);

    char line[96];
    std::snprintf(line, sizeof(line), "Aplicar '%s' ao TIC '%s'?",
                  fileName ? fileName : "?", ticName ? ticName : "?");
    ui.labelFitted(dlg.x + kPad, dlg.y + kHeaderH + 20.0f, line, theme::TEXT,
                   dlg.w - 2.0f * kPad);
    ui.labelFitted(dlg.x + kPad, dlg.y + kHeaderH + 54.0f,
                   "Pode aplicar depois nos seletores do Inspector.",
                   theme::LINE, dlg.w - 2.0f * kPad);

    UiRect yes{}, no{};
    storageDialogButtons(dlg, yes, no);
    int chosen = 0;
    if (ui.button(kApplyYesId, yes.x, yes.y, yes.w, yes.h, "Sim")) {
        chosen = 1;
        st.applyAsk = false;
    }
    if (ui.button(kApplyNoId, no.x, no.y, no.w, no.h, "Não")) {
        chosen = 2;
        st.applyAsk = false;
    }
    return chosen;
}

} // namespace editor
} // namespace vv
