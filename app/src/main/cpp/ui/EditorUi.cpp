#include "ui/EditorUi.h"
#include "components/InputMap.h"
#include "render/Camera.h"
#include "components/TouchControls.h"
#include <cmath>
#include "components/MeshRenderer.h"
#include "components/Transform3D.h"
#include "core/Scene.h"
#include <cstdio>

namespace vv {
namespace editor {

namespace {
constexpr u64 kIdPlus      = 40;
constexpr u64 kIdScrollHier = 41;   // F4.1: região de scroll da Hierarchy
constexpr u64 kIdScrollInsp = 42;   // F4.1: região de scroll do Inspector
// F5.2: viewer de logs usa kLogsScrollId (43, EditorLayout.h — compartilhado
// com os testes)
constexpr u64 kIdRowBase   = 1000;
constexpr u64 kIdAssetBase = 6000;   // F5-E: itens do seletor de assets
// ids do Inspector (sliders/botões) vivem em ui/EditorLayout.h — o PLANO é
// a fonte única das posições E dos ids (F5.0-fix).

// Toque (edge de press) fora do rect → fecha overlays.
bool pressedOutside(const InputState& in, f32 x, f32 y, f32 w, f32 h) {
    if (!in.pressed(0)) {
        return false;
    }
    f32 px, py;
    in.pos(0, px, py);
    return !(px >= x && px < x + w && py >= y && py < y + h);
}

f32 rad2deg(f32 r) { return r * 57.29577951f; }
f32 deg2rad(f32 d) { return d * 0.01745329252f; }

// nome curto do asset p/ a linha do Inspector (basename da ref)
const char* assetBasename(const std::string& ref) {
    const size_t slash = ref.rfind('/');
    return ref.c_str() + (slash == std::string::npos ? 0 : slash + 1);
}

// Linha do Inspector: label + slider + valor; aplica em `value` via setter.
// (rowTop, rowH, tm) vêm do PLANO — a baseline é centrada nas métricas REAIS
// da fonte (F5.0-fix: o "+8" antigo deixava o bloco de 28 px invadir a linha
// de cima).
bool sliderRow(UiContext& ui, u64 id, f32 x, f32 rowTop, f32 rowH,
               const TextMetrics& tm, const char* labelText,
               f32 minV, f32 maxV, f32& value, const char* fmt) {
    const f32 baseline = inspBaseline(rowTop, rowH, tm);
    ui.labelFitted(x + kPad, baseline, labelText,
                   theme::TEXT, 84.0f - kPad - 6.0f);   // B2: até ao trilho
    const f32 trackX = x + 84.0f;
    const f32 trackW = 118.0f;
    const bool changed = ui.slider(id, trackX, rowTop, trackW, rowH, minV, maxV, value);

    char val[24];
    std::snprintf(val, sizeof(val), fmt, value);
    if (ui.hasFont()) {
        const f32 tw = ui.fontWidth(val);
        ui.label(x + kPanelW - kPad - tw, baseline, val, theme::TEXT);
    }
    return changed;
}
} // namespace

UiRect centerRect(f32 sw, f32 sh) {
    // QUALIFICADO: chamada não-qualificada era ambígua no NDK clang — o ADL
    // puxava vv::safe::centerRect (o tipo do argumento é safe::Insets) para
    // além de vv::editor::centerRect (mesma assinatura). O CI apanhou.
    return safe::centerRect(sw, sh, safe::Insets{});   // sem safe-area (compat/testes)
}

// F4.2: viewport central DENTRO do contentRect — gestos que nascem atrás da
// nav/status bar não orbitam a câmara (matemática em ui/SafeArea.h).
UiRect centerRect(f32 sw, f32 sh, const safe::Insets& in) {
    return safe::centerRect(sw, sh, in);
}

// ---------------------------------------------------------------------------
// HIERARQUIA — lista COMPLETA de TICs com scroll (F4.1: fim do corte maxRows
// da F3). Drag na lista = scroll; tap numa linha = seleciona (re-despacho).
// 0.7.0 (gestão de TICs): cada linha tem o OLHO (visibilidade — toggle
// imediato) e o "..." (menu contextual Renomear/Remover/Duplicar/
// Visibilidade); tap no VAZIO da lista DESSELECIONA.
// ---------------------------------------------------------------------------
bool drawHierarchy(UiContext& ui, Scene& scene, EditorState& st) {
    // F4.2: painel inteiro dentro do contentRect (insets do sistema)
    const UiRect panel = safe::hierarchyPanelRect(ui.screenWidth(), ui.screenHeight(),
                                                  ui.safeArea());
    const f32 x = panel.x;
    const f32 y = panel.y;
    const f32 w = panel.w;
    const f32 h = panel.h;

    ui.panel(x, y, w, h, theme::PANEL);
    ui.panel(x + w - 1.0f, y, 1.0f, h, theme::LINE);   // separador direito

    // cabeçalho: título + botão "+" (FORA da região de scroll — captura normal)
    const f32 th = ui.fontHeight();
    ui.label(x + kPad, y + kHeaderH * 0.5f + th * 0.30f, "HIERARQUIA", theme::TEXT);
    const f32 plusW = 56.0f, plusH = 40.0f;
    const bool plus = ui.button(kIdPlus, x + w - kPad - plusW, y + (kHeaderH - plusH) * 0.5f,
                                plusW, plusH, "+");

    // lista: região de scroll abaixo do cabeçalho, uma linha por TIC ativo
    const f32 listTop = y + kHeaderH;
    const UiRect listRegion = {x, listTop, w, h - kHeaderH};
    u32 nTics = 0;
    scene.forEachActive([&nTics](const Tic&) { ++nTics; });
    const f32 contentH = hierarchyContentHeight(nTics);

    ui.beginScroll(kIdScrollHier, listRegion, contentH);
    const f32 off = ui.scrollOffset();

    // 0.7.0: geometria da linha — [nome][olho 40][... 40] (com gaps de 6)
    const f32 kEyeW = 40.0f;
    const f32 nameW = w - 2.0f * kPad - 2.0f * kEyeW - 12.0f;

    u32 row = 0;
    scene.forEachActive([&](const Tic& t) {
        const f32 ry = listTop + static_cast<f32>(row) * kRowH - off;
        ++row;
        // o clip da região recusa quads fora do ecrã (rows escondidas custam 0)
        char clipped[40];
        std::snprintf(clipped, sizeof(clipped), "%.30s", t.name.c_str());
        ui.button(kIdRowBase + t.handle.index, x + kPad, ry + 4.0f,
                  nameW, kRowH - 8.0f, clipped);   // só desenha (F4.1)
        if (st.selected == t.handle) {
            ui.frame(x + kPad, ry + 4.0f, nameW, kRowH - 8.0f, 2.0f, theme::ACCENT);
        }
        // 0.7.0 — olho (visibilidade): "O" visível / "X" escondido (o atlas
        // é ASCII — mono brutalist; o estado também está no Inspector)
        char eye[2] = {t.visible ? 'O' : 'X', '\0'};
        ui.button(kHierEyeBase + t.handle.index, x + kPad + nameW + 6.0f,
                  ry + 4.0f, kEyeW, kRowH - 8.0f, eye);
        // 0.7.0 — "..." abre o menu contextual (Renomear/Remover/Duplicar/
        // Visibilidade); o long-press da spec é o atalho alternativo — o
        // botão é determinístico e aferível no CI
        ui.button(kHierDotsBase + t.handle.index, x + kPad + nameW + 6.0f + kEyeW + 6.0f,
                  ry + 4.0f, kEyeW, kRowH - 8.0f, "...");
    });
    ui.endScroll();

    if (nTics == 0) {
        ui.labelFitted(x + kPad, listTop + kRowH, "(vazio - use +)", theme::TEXT,
                       w - 2.0f * kPad);
    }

    // tap re-despachado → seleção/olho/... da linha sob o dedo (mesmo após
    // scroll); F5.0-fix: POR ID — a Hierarchy só consome taps nascidos nela.
    // 0.7.0: tap no VAZIO da lista (abaixo da última linha) DESSELECIONA.
    f32 tx, ty;
    if (ui.scrollTap(kHierarchyScrollId, tx, ty) && scroll::inside(listRegion, tx, ty)) {
        const i32 sel = hierarchyRowAtTap(ty, listTop, off, nTics);
        if (sel < 0) {
            st.selected = Handle::invalid();   // 0.7.0: desselecionar no vazio
        } else {
            u32 i = 0;
            scene.forEachActive([&](const Tic& t) {
                if (i == static_cast<u32>(sel)) {
                    // 0.7.0: hit-test da linha em três zonas (nome/olho/...)
                    const f32 ry = listTop + static_cast<f32>(i) * kRowH - off;
                    const f32 eyeX = x + kPad + nameW + 6.0f;
                    const f32 dotsX = eyeX + kEyeW + 6.0f;
                    if (ty >= ry + 4.0f && ty < ry + kRowH - 4.0f &&
                        tx >= eyeX && tx < eyeX + kEyeW) {
                        // OLHO: toggle de visibilidade IMEDIATO
                        if (Tic* tt = scene.get(t.handle)) {
                            tt->visible = !tt->visible;
                        }
                    } else if (ty >= ry + 4.0f && ty < ry + kRowH - 4.0f &&
                               tx >= dotsX && tx < dotsX + kEyeW) {
                        // "...": menu contextual
                        st.contextMenu = true;
                        st.contextTic = t.handle;
                    } else {
                        st.selected = t.handle;   // nome: seleciona
                        st.selElement = -1;   // elemento de UI: nova seleção
                    }
                }
                ++i;
            });
        }
    }

    return plus;
}

// ---------------------------------------------------------------------------
// INSPECTOR — F5.0-fix: o layout vem do PLANO (ui/EditorLayout.h) — linhas em
// ordem com y cumulativo e alturas derivadas das MÉTRICAS REAIS da fonte.
// O desenho NÃO tem nenhum "+=" próprio: consome o plano e só subtrai o
// offset do scroll. contentHeight = fundo da última linha (soma REAL).
// ---------------------------------------------------------------------------
bool drawInspector(UiContext& ui, Scene& scene, EditorState& st,
                   const AssetCatalog* catalog) {
    // F4.2: painel inteiro dentro do contentRect — a altura REAL alimenta o
    // beginScroll → o overflow do Inspector é detetado e o scroll ativa (B1)
    const UiRect panel = safe::inspectorPanelRect(ui.screenWidth(), ui.screenHeight(),
                                                  ui.safeArea());
    const f32 x = panel.x;
    const f32 y = panel.y;
    const f32 w = panel.w;
    const f32 h = panel.h;

    ui.panel(x, y, w, h, theme::PANEL);
    ui.panel(x, y, 1.0f, h, theme::LINE);   // separador esquerdo

    const f32 th = ui.fontHeight();
    ui.label(x + kPad, y + kHeaderH * 0.5f + th * 0.30f, "INSPECTOR", theme::TEXT);
    ui.panel(x + kPad, y + kHeaderH - 1.0f, w - 2.0f * kPad, 1.0f, theme::LINE);

    // valida seleção (TIC pode ter morrido neste frame)
    Tic* tic = scene.get(st.selected);
    if (!tic) {
        st.selected = Handle::invalid();
        ui.labelFitted(x + kPad, y + kHeaderH + kRowH, "(nada selecionado)",
                       theme::LINE, w - 2.0f * kPad);
        return false;
    }

    // ---- PLANO (fonte única): perfil → linhas sequenciais com y cumulativo
    const TextMetrics tm = ui.textMetrics();
    const InspProfile prof = inspectorProfile(*tic);
    const bool selectable = (catalog != nullptr);
    InspRow plan[32];
    const u32 nRows = inspectorPlan(prof, tm, selectable, plan);
    const f32 contentH = inspectorContentHeight(prof, tm, selectable);

    // região de scroll: abaixo do cabeçalho
    const f32 contentTop = y + kHeaderH + 4.0f;
    const f32 listH = h - kHeaderH - 4.0f;

    ui.beginScroll(kIdScrollInsp, {x, contentTop, w, listH}, contentH);
    const f32 off = ui.scrollOffset();

    // ---- payloads (os VALORES continuam a ser lidos dos componentes; as
    // POSIÇÕES vêm todas do plano)
    Transform3D* tr = tic->getComponent<Transform3D>();
    struct SliderSpec { const char* label; f32 min, max; const char* fmt; f32* value; };
    f32 posArr[3] = {};
    f32 rotDeg[3] = {};
    f32 sclArr[3] = {};
    if (tr) {
        posArr[0] = tr->pos.x; posArr[1] = tr->pos.y; posArr[2] = tr->pos.z;
        f32 ex = 0.0f, ey = 0.0f, ez = 0.0f;
        Quat::toEuler(tr->rot, ex, ey, ez);   // rot em graus (extrai do quat)
        rotDeg[0] = rad2deg(ex); rotDeg[1] = rad2deg(ey); rotDeg[2] = rad2deg(ez);
        sclArr[0] = tr->scale.x; sclArr[1] = tr->scale.y; sclArr[2] = tr->scale.z;
    }
    SliderSpec rows9[9] = {
        {"px", -20.0f, 20.0f, "%.2f", &posArr[0]},
        {"py", -20.0f, 20.0f, "%.2f", &posArr[1]},
        {"pz", -20.0f, 20.0f, "%.2f", &posArr[2]},
        {"rx", -180.0f, 180.0f, "%.0f", &rotDeg[0]},
        {"ry", -180.0f, 180.0f, "%.0f", &rotDeg[1]},
        {"rz", -180.0f, 180.0f, "%.0f", &rotDeg[2]},
        {"sx", 0.1f, 5.0f, "%.2f", &sclArr[0]},
        {"sy", 0.1f, 5.0f, "%.2f", &sclArr[1]},
        {"sz", 0.1f, 5.0f, "%.2f", &sclArr[2]},
    };
    const MeshRenderer* mr = tic->getComponent<MeshRenderer>();
    const InputMap* im = tic->getComponent<InputMap>();
    BodyComp* bc = tic->getComponent<BodyComp>();
    // 0.7.0: o tint é EDITÁVEL (sliders R/G/B) — ponteiro mutável
    MeshRenderer* mrEdit = tic->getComponent<MeshRenderer>();
    char meshLabel[64] = "";
    char texLabel[64] = "";
    char inputLine[48] = "";
    char bodyLine[64] = "";
    if (mr) {
        if (!mr->meshPath.empty()) {
            std::snprintf(meshLabel, sizeof(meshLabel), "mesh: %s",
                          assetBasename(mr->meshPath));
        } else {
            std::snprintf(meshLabel, sizeof(meshLabel), "mesh: %s",
                          mr->mesh ? "cube" : "-");
        }
        if (!mr->texPath.empty()) {
            std::snprintf(texLabel, sizeof(texLabel), "tex: %s",
                          assetBasename(mr->texPath));
        } else {
            std::snprintf(texLabel, sizeof(texLabel), "tex: %s",
                          mr->texture ? "ligada" : "none");
        }
    }
    if (im) {
        std::snprintf(inputLine, sizeof(inputLine), "input: %s",
                      im->source ? "fonte ligada" : "sem fonte");
    }
    if (bc) {
        std::snprintf(bodyLine, sizeof(bodyLine), "body: %s - %s - chao: %s",
                      BodyComp::typeName(bc->type), BodyComp::shapeName(bc->shape),
                      bc->grounded ? "sim" : "nao");
    }
    // linhas de texto (Label) NA MESMA ORDEM do plano: input → body → tc
    const char* labelTexts[3];
    const f32 (*labelColors[3])[4];
    f32 labelInsets[3];
    u32 nLabels = 0;
    if (im) {
        labelTexts[nLabels] = inputLine;
        labelColors[nLabels] = &theme::TEXT;
        labelInsets[nLabels] = 12.0f;
        ++nLabels;
    }
    if (bc) {
        labelTexts[nLabels] = bodyLine;
        labelColors[nLabels] = &theme::ACCENT;
        labelInsets[nLabels] = 0.0f;
        ++nLabels;
    }
    if (im && prof.tc) {
        labelTexts[nLabels] = "tc: stick + jump";
        labelColors[nLabels] = &theme::TEXT;
        labelInsets[nLabels] = 12.0f;
        ++nLabels;
    }
    u32 labelIdx = 0;

    bool edited = false;
    bool trEdited = false;
    u32 sliderIdx = 0;
    u32 colorIdx = 0;   // 0.7.0: payload dos ColorSlider (0=R, 1=G, 2=B)

    // ---- desenho: UMA passagem pelo plano; nenhum cursor local
    for (u32 i = 0; i < nRows; ++i) {
        const InspRow& r = plan[i];
        const f32 ry = contentTop + r.y - off;   // topo da linha em ECRÃ
        switch (r.kind) {
        case InspRow::Kind::Name:
            ui.labelFitted(x + kPad, inspBaseline(ry, r.h, tm), tic->name.c_str(),
                           theme::ACCENT, w - 2.0f * kPad);   // B2: ellipsis
            break;
        case InspRow::Kind::VisToggle: {
            // 0.7.0 — checkbox de visibilidade do TIC (mesmo estado do olho
            // da Hierarchy; TIC invisível não desenha em editor nem Play)
            char vis[32];
            std::snprintf(vis, sizeof(vis), "visivel: %s",
                          tic->visible ? "sim" : "nao");
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      vis);
            break;
        }
        case InspRow::Kind::ColorSlider: {
            // 0.7.0 — cor por TIC: sliders R/G/B do tint do MeshRenderer
            if (mrEdit) {
                static const char* kColLabels[3] = {"cor R", "cor G", "cor B"};
                if (sliderRow(ui, r.id, x, ry, r.h, tm, kColLabels[colorIdx],
                              0.0f, 1.0f, mrEdit->tint[colorIdx], "%.2f")) {
                    edited = true;
                }
            }
            ++colorIdx;
            break;
        }
        case InspRow::Kind::Section:
            ui.label(x + kPad, inspBaseline(ry, r.h, tm), "Transform3D", theme::TEXT);
            ui.panel(x + kPad, ry + r.h - 1.0f, w - 2.0f * kPad, 1.0f, theme::LINE);
            break;
        case InspRow::Kind::Slider:
            if (tr && sliderIdx < 9) {
                const SliderSpec& sp = rows9[sliderIdx];
                if (sliderRow(ui, r.id, x, ry, r.h, tm, sp.label, sp.min, sp.max,
                              *sp.value, sp.fmt)) {
                    trEdited = true;
                    edited = true;
                }
            }
            ++sliderIdx;
            break;
        case InspRow::Kind::Velx:
            if (bc) {
                f32 vx = bc->velocity.x;
                if (sliderRow(ui, r.id, x, ry, r.h, tm, "velx", -60.0f, 60.0f,
                              vx, "%.1f")) {
                    bc->velocity.x = vx;
                    edited = true;
                }
            }
            break;
        case InspRow::Kind::MeshButton:
            // botão da linha INTEIRA, centrado na linha do plano (o botão
            // antigo sangrava 2 px para a linha de baixo)
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      meshLabel);
            break;
        case InspRow::Kind::TexButton:
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      texLabel);
            break;
        case InspRow::Kind::MeshLabel:
            ui.labelFitted(x + kPad + 12.0f, inspBaseline(ry, r.h, tm), meshLabel,
                           theme::TEXT, w - 2.0f * kPad - 12.0f);
            break;
        case InspRow::Kind::TexLabel:
            ui.labelFitted(x + kPad + 12.0f, inspBaseline(ry, r.h, tm), texLabel,
                           theme::TEXT, w - 2.0f * kPad - 12.0f);
            break;
        case InspRow::Kind::Label:
            // input: → body: → tc: — payload NA ORDEM do plano (labelIdx)
            if (labelIdx < nLabels) {
                const f32 inset = labelInsets[labelIdx];
                ui.labelFitted(x + kPad + inset, inspBaseline(ry, r.h, tm),
                               labelTexts[labelIdx], *labelColors[labelIdx],
                               w - 2.0f * kPad - inset);
                ++labelIdx;
            }
            break;
        case InspRow::Kind::AddTc:
            ui.button(r.id, x + kPad, ry + 2.0f, w - 2.0f * kPad, r.h - 4.0f,
                      "add TouchControls");
            break;
        }
    }

    // escreve de volta no componente (rot: graus → quat YXZ)
    if (tr && trEdited) {
        tr->pos = Vec3{posArr[0], posArr[1], posArr[2]};
        tr->rot = Quat::fromEuler(deg2rad(rotDeg[0]), deg2rad(rotDeg[1]), deg2rad(rotDeg[2]));
        tr->scale = Vec3{sclArr[0], sclArr[1], sclArr[2]};
        tr->updateWorld();   // feedback imediato (TransformSystem reconfirma)
    }

    ui.endScroll();

    // tap re-despachado → linhas interativas do PLANO (hit-test do rect em
    // ecrã, igual ao que foi desenhado — nunca diverge)
    f32 tx, ty;
    if (ui.scrollTap(kInspectorScrollId, tx, ty)) {
        for (u32 i = 0; i < nRows; ++i) {
            const InspRow& r = plan[i];
            if (r.kind != InspRow::Kind::AddTc &&
                r.kind != InspRow::Kind::MeshButton &&
                r.kind != InspRow::Kind::TexButton &&
                r.kind != InspRow::Kind::VisToggle) {
                continue;
            }
            const f32 ry = contentTop + r.y - off;
            if (tx < x + kPad || tx >= x + w - kPad) {
                continue;
            }
            if (ty < ry + 2.0f || ty >= ry + r.h - 2.0f) {
                continue;   // mesmo rect do botão desenhado (+2/−2)
            }
            if (r.kind == InspRow::Kind::AddTc) {
                tic->addComponent<TouchControls>();   // F4: cria no TIC
            } else if (r.kind == InspRow::Kind::MeshButton) {
                st.assetMenu = 1;                     // F5-E: seletor de meshes
            } else if (r.kind == InspRow::Kind::TexButton) {
                st.assetMenu = 2;                     // F5-E: seletor de texturas
            } else if (r.kind == InspRow::Kind::VisToggle) {
                tic->visible = !tic->visible;         // 0.7.0: checkbox
            }
        }
    }

    return edited;
}

void drawTouchControls(UiContext& ui, const TouchControls& tc, f32 sw, f32 sh) {
    // F4.2: layout fixo recalculado para a ÁREA ÚTIL (superfície menos insets)
    // e deslocado pela origem do contentRect — nada desenhado atrás da
    // nav/status bar. TouchControls.cpp fica intocado (CLÁUSULA CALMA).
    const f32 ox = ui.safeLeft();
    const f32 oy = ui.safeTop();
    const f32 aw = sw - ui.safeLeft() - ui.safeRight();
    const f32 ah = sh - ui.safeTop() - ui.safeBottom();
    const TouchControls::Layout l = TouchControls::layout(aw, ah);
    const f32 jx = ox + l.joyCX;
    const f32 jy = oy + l.joyCY;

    // joystick: base em quadro + knob quadrado (mono brutalist — só retângulos)
    ui.frame(jx - l.joyR, jy - l.joyR, 2.0f * l.joyR, 2.0f * l.joyR,
             2.0f, theme::LINE);
    ui.panel(jx - 1.0f, jy - 1.0f, 2.0f, 2.0f, theme::LINE);
    f32 kx = tc.baseX();
    f32 ky = tc.baseY();
    bool active = false;
    if (tc.joystickActive()) {
        active = true;
        f32 dx = tc.knobX() - tc.baseX();
        f32 dy = tc.knobY() - tc.baseY();
        const f32 len = std::sqrt(dx * dx + dy * dy);
        if (len > l.joyR) {
            dx *= l.joyR / len;
            dy *= l.joyR / len;
        }
        kx = tc.baseX() + dx;
        ky = tc.baseY() + dy;
    }
    const f32 ks = 44.0f;
    kx += ox;   // F4.2: base/knob vivem em coords locais da safe-area
    ky += oy;
    ui.panel(kx - ks * 0.5f, ky - ks * 0.5f, ks, ks,
             active ? theme::ACCENT : theme::PANEL);
    ui.frame(kx - ks * 0.5f, ky - ks * 0.5f, ks, ks, 1.0f, theme::LINE);

    // botão JUMP: premido = invertido (tema mono)
    const bool held = tc.buttonHeld();
    const f32 bx2 = ox + l.btnX;
    const f32 by2 = oy + l.btnY;
    if (held) {
        ui.panel(bx2, by2, l.btnW, l.btnH, theme::TEXT);
    }
    ui.frame(bx2, by2, l.btnW, l.btnH, 2.0f, held ? theme::PANEL : theme::ACCENT);
    if (ui.hasFont()) {
        const f32 tw = ui.fontWidth("JUMP");
        const f32 thh = ui.fontHeight();
        ui.label(bx2 + (l.btnW - tw) * 0.5f, by2 + l.btnH * 0.5f + thh * 0.30f,
                 "JUMP", held ? theme::PANEL : theme::TEXT);
    }
}

// 0.7.0 — separador "3D | UI" na toolbar: MUDA o modo do viewport
// central (3D = cena com orbit/gizmos; UI = viewport 2D dedicado à UI
// criável). O botão do modo ATIVO fica com frame ACCENT (a linguagem do
// seletor do gizmo). Não corre em PLAY (a toolbar não existe em play).
void drawModeToggle(UiContext& ui, EditorState& st) {
    const UiRect r = toolbarModeRect(ui.screenWidth(), ui.screenHeight(),
                                     ui.safeArea());
    if (ui.button(kMode3dId, r.x, r.y, 96.0f, r.h, "3D")) {
        st.uiMode = false;
        st.selElement = -1;
        st.elDrag = false;
    }
    if (ui.button(kModeUiId, r.x + 96.0f + 8.0f, r.y, 96.0f, r.h, "UI")) {
        st.uiMode = true;
    }
    // frame do modo ATIVO
    if (!st.uiMode) {
        ui.frame(r.x, r.y, 96.0f, r.h, 2.0f, theme::ACCENT);
    } else {
        ui.frame(r.x + 96.0f + 8.0f, r.y, 96.0f, r.h, 2.0f, theme::ACCENT);
    }
}

// 0.7.0 — DESSELECCIONAR no viewport 3D: arm no press edge dentro do
// viewport central (não reclamado); limpa no release se o dedo NÃO se
// mexeu além do limiar (tap ≠ drag de orbit/gizmo). Puro e afervel.
bool viewportTapClearsSelection(EditorState& st, const InputState& in,
                                 const UiRect& view, u32 claimedMask) {
    // arm: press edge do slot 0 dentro do viewport, não reclamado (só se
    // ainda NÃO armado — um press edge persistente não re-arma com a pos
    // nova; o drag de orbit continua a ser drag)
    if (in.pressed(0) && !st.deselectArm && !(claimedMask & 1u)) {
        f32 px = 0.0f, py = 0.0f;
        in.pos(0, px, py);
        if (px >= view.x && px < view.x + view.w && py >= view.y &&
            py < view.y + view.h) {
            st.deselectArm = true;
            st.deselectX = px;
            st.deselectY = py;
        }
    }
    if (!st.deselectArm) {
        return false;
    }
    // release do slot 0: foi um tap parado dentro do viewport?
    if (!in.released(0)) {
        // dedo deslizou além do limiar → drag (orbit/gizmo), cancela o arm
        if (in.down(0)) {
            f32 px = 0.0f, py = 0.0f;
            in.pos(0, px, py);
            const f32 dx = px - st.deselectX;
            const f32 dy = py - st.deselectY;
            if (dx * dx + dy * dy > 14.0f * 14.0f) {
                st.deselectArm = false;
            }
        }
        return false;
    }
    st.deselectArm = false;
    f32 px = 0.0f, py = 0.0f;
    in.pos(0, px, py);
    const f32 dx = px - st.deselectX;
    const f32 dy = py - st.deselectY;
    if (dx * dx + dy * dy > 14.0f * 14.0f) {
        return false;   // arrastou — foi orbit/gizmo, não um tap
    }
    if (px < view.x || px >= view.x + view.w || py < view.y ||
        py >= view.y + view.h) {
        return false;
    }
    st.selected = Handle::invalid();
    st.selElement = -1;
    return true;
}

int drawPlusMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh, EditorState& st) {
    // 0.7.0: no modo UI o "+" cria ELEMENTOS (Panel/Label/Button/Image);
    // no 3D cria TICs de preset (como sempre). O main despacha pelo modo.
    const bool uiMode = st.uiMode;
    const int kItems = uiMode ? 4 : 4;
    const f32 w = kMenuW;
    const f32 h = kHeaderH + static_cast<f32>(kItems) * 64.0f + kPad;
    // F4.2: centrado no viewport ÚTIL (dentro do contentRect)
    const f32 ox = ui.safeLeft();
    const f32 oy = ui.safeTop();
    const f32 aw = sw - ox - ui.safeRight();
    const f32 ah = sh - oy - ui.safeBottom();
    const f32 x = ox + (aw - w) * 0.5f;
    const f32 y = oy + (ah - h) * 0.5f;

    if (pressedOutside(in, x, y, w, h)) {
        st.plusMenu = false;
        return 0;
    }

    ui.panel(x, y, w, h, theme::PANEL);
    ui.frame(x, y, w, h, 2.0f, theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.label(x + kPad, y + kHeaderH * 0.5f + th * 0.30f,
             uiMode ? "CRIAR ELEMENTO UI" : "CRIAR TIC", theme::TEXT);

    int chosen = 0;
    const char* names[4] = {"PlayerBody3D", "CharacterBody3D", "StaticBody3D",
                            "RigidBody3D"};
    const char* elems[4] = {"Panel", "Label", "Button", "Image"};
    const char* const* labels = uiMode ? elems : names;
    for (int i = 0; i < kItems; ++i) {
        if (ui.button(static_cast<u64>(20 + i), x + kPad, y + kHeaderH + i * 64.0f,
                      w - 2.0f * kPad, 56.0f, labels[i])) {
            chosen = i + 1;
            st.plusMenu = false;
        }
    }
    return chosen;
}

int drawFileMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh, EditorState& st) {
    const f32 w = kMenuW;
    // F5-E: + Export OBJ; F5.2: + Importar…/Export Downloads (All Files
    // Access) — o "Pasta (SAF)" foi REMOVIDO com o fluxo SAF.
    // 0.6.7: + "Sair para projetos" (auto-save no main + volta ao gestor
    // SEM matar a app — VvActivity.finish() pela ponte Java)
    constexpr int kItems = 6;
    const f32 h = kHeaderH + static_cast<f32>(kItems) * 64.0f + kPad;
    // F4.2: centrado no viewport ÚTIL (dentro do contentRect)
    const f32 ox = ui.safeLeft();
    const f32 oy = ui.safeTop();
    const f32 aw = sw - ox - ui.safeRight();
    const f32 ah = sh - oy - ui.safeBottom();
    const f32 x = ox + (aw - w) * 0.5f;
    const f32 y = oy + (ah - h) * 0.5f;

    if (pressedOutside(in, x, y, w, h)) {
        st.fileMenu = false;
        return 0;
    }

    ui.panel(x, y, w, h, theme::PANEL);
    ui.frame(x, y, w, h, 2.0f, theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.label(x + kPad, y + kHeaderH * 0.5f + th * 0.30f, "MENU", theme::TEXT);

    int chosen = 0;
    const char* labels[kItems] = {"Save cena", "Load cena", "Export OBJ",
                                  "Importar…", "Export Downloads",
                                  "Sair para projetos"};
    for (int i = 0; i < kItems; ++i) {
        if (ui.button(static_cast<u64>(30 + i), x + kPad,
                      y + kHeaderH + static_cast<f32>(i) * 64.0f,
                      w - 2.0f * kPad, 56.0f, labels[i])) {
            chosen = i + 1;
            st.fileMenu = false;
        }
    }
    return chosen;
}

int drawSettingsMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                     EditorState& st, const char* storageMode) {
    // F5.1-hotfix: menu do botão Settings — mono, mesmo padrão dos overlays.
    // F5.2: 3 itens + linha do modo de armazenamento ativo.
    constexpr int kItems = 3;
    constexpr f32 kModeLineH = 30.0f;
    const bool showMode = storageMode && storageMode[0];
    const f32 h = kHeaderH + (showMode ? kModeLineH : 0.0f) +
                  static_cast<f32>(kItems) * 64.0f + kPad;
    const f32 ox = ui.safeLeft();
    const f32 oy = ui.safeTop();
    const f32 aw = sw - ox - ui.safeRight();
    const f32 ah = sh - oy - ui.safeBottom();
    const f32 x = ox + (aw - kMenuW) * 0.5f;
    const f32 y = oy + (ah - h) * 0.5f;

    if (pressedOutside(in, x, y, kMenuW, h)) {
        st.settingsMenu = false;
        return 0;
    }

    ui.panel(x, y, kMenuW, h, theme::PANEL);
    ui.frame(x, y, kMenuW, h, 2.0f, theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.label(x + kPad, y + kHeaderH * 0.5f + th * 0.30f, "SETTINGS", theme::TEXT);

    // F5.2: modo de armazenamento ativo (o item 6 do escopo — o dono vê
    // sempre QUAL modo está em uso)
    f32 itemsTop = y + kHeaderH;
    if (showMode) {
        char line[48];
        std::snprintf(line, sizeof(line), "armazenamento: %s", storageMode);
        ui.labelFitted(x + kPad, y + kHeaderH + kModeLineH * 0.5f + th * 0.30f,
                       line, theme::LINE, kMenuW - 2.0f * kPad);
        itemsTop += kModeLineH;
    }

    int chosen = 0;
    const char* labels[kItems] = {"Exportar logs", "Ver logs",
                                  "Acesso a ficheiros…"};
    for (int i = 0; i < kItems; ++i) {
        if (ui.button(static_cast<u64>(4400 + i), x + kPad,
                      itemsTop + static_cast<f32>(i) * 64.0f,
                      kMenuW - 2.0f * kPad, 56.0f, labels[i])) {
            chosen = i + 1;
            st.settingsMenu = false;
        }
    }
    return chosen;
}

// ---------------------------------------------------------------------------
// F5.2: DIÁLOGO All Files Access — "Precisa de acesso a todos os ficheiros
// para importar/exportar projetos" + Permitir/Cancelar (tema mono).
// A mensagem é quebrada em ATÉ 3 linhas que caibam no painel (medidas com
// a fonte real — nunca sai do rect).
// ---------------------------------------------------------------------------

namespace {

// quebra por palavras (greedy) em até maxLines linhas de até cap-1 chars;
// devolve o nº de linhas usadas (texto que não couber fica na última)
int wrapText3(UiContext& ui, const char* text, f32 maxW, int maxLines,
              char out[][96]) {
    for (int i = 0; i < maxLines; ++i) {
        out[i][0] = '\0';
    }
    if (!text || !ui.hasFont() || maxLines <= 0) {
        return 0;
    }
    int line = 0;
    const char* p = text;
    while (*p && line < maxLines) {
        const char* word = p;
        while (*p && *p != ' ') ++p;          // fim da palavra
        const size_t wlen = static_cast<size_t>(p - word);
        while (*p == ' ') ++p;                // espaços entre palavras

        char candidate[96];
        if (out[line][0]) {
            std::snprintf(candidate, sizeof(candidate), "%s %.*s", out[line],
                          static_cast<int>(wlen), word);
        } else {
            std::snprintf(candidate, sizeof(candidate), "%.*s",
                          static_cast<int>(wlen), word);
        }
        if (ui.fontWidth(candidate) <= maxW || !out[line][0]) {
            std::snprintf(out[line], 96, "%s", candidate);
        } else {
            ++line;                            // a palavra não cabe → nova linha
            if (line < maxLines) {
                std::snprintf(out[line], 96, "%.*s", static_cast<int>(wlen), word);
            }
        }
        // palavra MAIOR que a linha inteira: trunca (não há em texto fixo)
    }
    int used = 0;
    for (int i = 0; i < maxLines; ++i) {
        if (out[i][0]) used = i + 1;
    }
    return used;
}

} // namespace

int drawStorageDialog(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                      EditorState& st) {
    const f32 h = storageDialogHeight();
    const f32 ox = ui.safeLeft();
    const f32 oy = ui.safeTop();
    const f32 aw = sw - ox - ui.safeRight();
    const f32 ah = sh - oy - ui.safeBottom();
    const UiRect dlg = centeredMenuRect(ox, oy, aw, ah, h);

    if (pressedOutside(in, dlg.x, dlg.y, dlg.w, dlg.h)) {
        st.storageDialog = false;   // toque fora = cancelar (sem ação)
        return 0;
    }

    ui.panel(dlg.x, dlg.y, dlg.w, dlg.h, theme::PANEL);
    ui.frame(dlg.x, dlg.y, dlg.w, dlg.h, 2.0f, theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.label(dlg.x + kPad, dlg.y + kHeaderH * 0.5f + th * 0.30f,
             "ARMAZENAMENTO", theme::TEXT);

    // mensagem EXATA do escopo, quebrada para caber
    char rows[3][96];
    wrapText3(ui,
              "Precisa de acesso a todos os ficheiros para importar/exportar "
              "projetos",
              kMenuW - 2.0f * kPad, 3, rows);
    for (int i = 0; i < 3; ++i) {
        if (rows[i][0]) {
            ui.labelFitted(dlg.x + kPad,
                           dlg.y + kHeaderH + static_cast<f32>(i) * 34.0f + 20.0f,
                           rows[i], theme::TEXT, dlg.w - 2.0f * kPad);
        }
    }

    // botões lado a lado (faixa de ids exclusiva 6300+)
    UiRect allow{}, cancel{};
    storageDialogButtons(dlg, allow, cancel);
    int chosen = 0;
    if (ui.button(6301, allow.x, allow.y, allow.w, allow.h, "Permitir")) {
        chosen = 1;
        st.storageDialog = false;
    }
    if (ui.button(6302, cancel.x, cancel.y, cancel.w, cancel.h, "Cancelar")) {
        chosen = 2;
        st.storageDialog = false;
    }
    return chosen;
}

// ---------------------------------------------------------------------------
// F5.2: overlay IMPORT — ficheiros suportados de Download/Documents (File
// API direta; o main copia o escolhido para o projeto). Cap 8 (mono).
// ---------------------------------------------------------------------------

int drawImportMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                   EditorState& st,
                   const std::vector<fileapi::Candidate>& cands) {
    // cap de linhas no overlay (sem scroll — F8)
    constexpr size_t kMaxRows = 8;
    const size_t shown = cands.size() < kMaxRows ? cands.size() : kMaxRows;

    const f32 h = importMenuHeight(static_cast<u32>(shown) + 1u);
    const f32 ox = ui.safeLeft();
    const f32 oy = ui.safeTop();
    const f32 aw = sw - ox - ui.safeRight();
    const f32 ah = sh - oy - ui.safeBottom();
    const f32 x = ox + (aw - kMenuW) * 0.5f;
    const f32 y = oy + (ah - h) * 0.5f;

    if (pressedOutside(in, x, y, kMenuW, h)) {
        st.importMenu = false;
        return 0;
    }

    ui.panel(x, y, kMenuW, h, theme::PANEL);
    ui.frame(x, y, kMenuW, h, 2.0f, theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.label(x + kPad, y + kHeaderH * 0.5f + th * 0.30f, "IMPORTAR",
             theme::TEXT);

    int chosen = 0;
    if (cands.empty()) {
        ui.labelFitted(x + kPad, y + kHeaderH + 30.0f,
                       "(nenhum obj/gltf/glb/png em Download/Documents)",
                       theme::LINE, kMenuW - 2.0f * kPad);
        return 0;
    }
    for (size_t i = 0; i < shown; ++i) {
        // rótulo: nome + tipo (m/t → mesh/textura)
        char label[80];
        std::snprintf(label, sizeof(label), "%s  [%s]", cands[i].name.c_str(),
                      cands[i].kind == 'm' ? "mesh" : "tex");
        const UiRect row = importRowRect({x, y, kMenuW, h}, static_cast<u32>(i));
        if (ui.button(6100 + static_cast<u64>(i), row.x, row.y, row.w, row.h,
                      label)) {
            chosen = static_cast<int>(i) + 1;
            st.importMenu = false;
        }
    }
    if (cands.size() > kMaxRows) {
        char more[48];
        std::snprintf(more, sizeof(more), "+%u ficheiros (cap do overlay)",
                      static_cast<unsigned>(cands.size() - kMaxRows));
        ui.labelFitted(x + kPad,
                       importRowRect({x, y, kMenuW, h},
                                     static_cast<u32>(shown)).y + 20.0f,
                       more, theme::LINE, kMenuW - 2.0f * kPad);
    }
    return chosen;
}

// ---------------------------------------------------------------------------
// F5.2: VIEWER de logs — engine.log (tail) + crash dumps com scroll (id 43),
// tema mono. Funcional SEM export: é a mesma leitura POSIX que o export faz.
// ---------------------------------------------------------------------------

void drawLogViewer(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                   EditorState& st, const std::vector<std::string>& lines,
                   const std::vector<std::string>& dumps) {
    const f32 ox = ui.safeLeft();
    const f32 oy = ui.safeTop();
    const f32 aw = sw - ox - ui.safeRight();
    const f32 ah = sh - oy - ui.safeBottom();
    // painel GRANDE central (86% × 80% da área útil — o log precisa de espaço)
    const f32 w = aw * 0.86f;
    const f32 h = ah * 0.80f;
    const f32 x = ox + (aw - w) * 0.5f;
    const f32 y = oy + (ah - h) * 0.5f;

    if (pressedOutside(in, x, y, w, h)) {
        st.logViewer = false;
        return;
    }

    ui.panel(x, y, w, h, theme::PANEL);
    ui.frame(x, y, w, h, 2.0f, theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.label(x + kPad, y + kHeaderH * 0.5f + th * 0.30f,
             "LOGS (engine.log + crashes)", theme::TEXT);
    if (ui.button(6401, x + w - kPad - 96.0f, y + 4.0f, 96.0f, 36.0f, "fechar")) {
        st.logViewer = false;
        return;
    }
    ui.panel(x + kPad, y + kHeaderH - 1.0f, w - 2.0f * kPad, 1.0f, theme::LINE);

    // conteúdo: altura REAL = linhas (block da fonte) + secção de dumps
    const TextMetrics tm = ui.textMetrics();
    const f32 rowH = tm.block() + 6.0f;
    const f32 dumpsH = dumps.empty() ? 0.0f : (34.0f + static_cast<f32>(dumps.size()) * rowH);
    const f32 contentH = 34.0f + static_cast<f32>(lines.size()) * rowH + dumpsH;

    const f32 listTop = y + kHeaderH;
    const UiRect region{x, listTop, w, h - kHeaderH};
    ui.beginScroll(kLogsScrollId, region, contentH);
    const f32 off = ui.scrollOffset();

    auto baselineOf = [&](f32 rowTop) {
        return rowTop + rowH * 0.5f + tm.ascent - tm.block() * 0.5f;
    };

    f32 cy = listTop - off;
    ui.labelFitted(x + kPad, baselineOf(cy + 4.0f),
                   lines.empty() ? "(log vazio)" : "engine.log:",
                   theme::LINE, w - 2.0f * kPad);
    cy += 34.0f;
    for (const std::string& l : lines) {
        // logs são longos — labelFitted corta na largura do painel
        ui.labelFitted(x + kPad, baselineOf(cy), l.c_str(), theme::TEXT,
                       w - 2.0f * kPad);
        cy += rowH;
    }
    if (!dumps.empty()) {
        ui.labelFitted(x + kPad, baselineOf(cy + 4.0f), "crash dumps:",
                       theme::ACCENT, w - 2.0f * kPad);
        cy += 34.0f;
        for (const std::string& d : dumps) {
            ui.labelFitted(x + kPad, baselineOf(cy), d.c_str(), theme::TEXT,
                           w - 2.0f * kPad);
            cy += rowH;
        }
    }
    ui.endScroll();

    // F5.2: 1º frame após abrir → salta para o FIM (o recente é o que importa)
    if (st.logViewerJustOpened) {
        ui.scrollSetOffset(kLogsScrollId, contentH);   // beginScroll clampa
        st.logViewerJustOpened = false;
    }
}

// ---------------------------------------------------------------------------
// F5-E: SELETOR DE ASSETS — overlay mono com "cube/none" + ficheiros de
// meshes/ ou textures/ (cap 5 ficheiros; sem scroll no overlay — F8).
// Devolve 1-based (1 = cube/none, 2.. = ficheiros), 0 = nada este frame.
// ---------------------------------------------------------------------------
int drawAssetMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh,
                  EditorState& st, const AssetCatalog& catalog) {
    const bool pickMesh = (st.assetMenu == 1);
    const std::vector<std::string>& files =
        pickMesh ? catalog.meshes : catalog.textures;

    // cap de ficheiros no overlay (mono, sem scroll — F8 traz scroll)
    constexpr size_t kMaxFiles = 5;
    const size_t shown = files.size() < kMaxFiles ? files.size() : kMaxFiles;

    const f32 w = kMenuW;
    const f32 h = kHeaderH + (1.0f + static_cast<f32>(shown)) * 48.0f + kPad;
    const f32 ox = ui.safeLeft();
    const f32 oy = ui.safeTop();
    const f32 aw = sw - ox - ui.safeRight();
    const f32 ah = sh - oy - ui.safeBottom();
    const f32 x = ox + (aw - w) * 0.5f;
    const f32 y = oy + (ah - h) * 0.5f;

    if (pressedOutside(in, x, y, w, h)) {
        st.assetMenu = 0;
        return 0;
    }

    ui.panel(x, y, w, h, theme::PANEL);
    ui.frame(x, y, w, h, 2.0f, theme::ACCENT);
    const f32 th = ui.fontHeight();
    ui.label(x + kPad, y + kHeaderH * 0.5f + th * 0.30f,
             pickMesh ? "MESH" : "TEXTURA", theme::TEXT);

    int chosen = 0;
    // item 0: cube (mesh) / none (textura)
    const char* first = pickMesh ? "cube (procedural)" : "none";
    if (ui.button(kIdAssetBase, x + kPad, y + kHeaderH, w - 2.0f * kPad, 40.0f,
                  first)) {
        chosen = 1;
        st.assetMenu = 0;
    }
    for (size_t i = 0; i < shown; ++i) {
        if (ui.button(kIdAssetBase + 1 + static_cast<u64>(i), x + kPad,
                      y + kHeaderH + static_cast<f32>(i + 1) * 48.0f,
                      w - 2.0f * kPad, 40.0f, files[i].c_str())) {
            chosen = static_cast<int>(i) + 2;
            st.assetMenu = 0;
        }
    }
    if (files.size() > kMaxFiles) {
        // aviso mono de cap (sem scroll no overlay)
        char more[48];
        std::snprintf(more, sizeof(more), "+%u ficheiros (cap do overlay)",
                      static_cast<unsigned>(files.size() - kMaxFiles));
        ui.labelFitted(x + kPad,
                       y + kHeaderH + static_cast<f32>(shown + 1) * 48.0f + 12.0f,
                       more, theme::LINE, w - 2.0f * kPad);
    }
    return chosen;
}

// ---------------------------------------------------------------------------
// F6: DISPATCH da escolha do seletor — applyAssetPick (o wiring que faltava).
// Era o bloco do main.cpp que nunca corria: drawAssetMenu fecha o seletor no
// clique (st.assetMenu = 0) e o dispatch lia g_editor.assetMenu DEPOIS →
// código morto desde a F5-E. A mesma lógica, agora PURA e afervel no CI.
// ---------------------------------------------------------------------------
AssetPickOutcome applyAssetPick(Scene& scene, Handle selected, int menuKind, int pick,
                                const AssetCatalog& catalog, const AssetResolvers& res) {
    AssetPickOutcome out;
    if (pick <= 0) {
        return out;   // nada escolhido neste frame
    }
    Tic* tic = scene.get(selected);
    MeshRenderer* mr = tic ? tic->getComponent<MeshRenderer>() : nullptr;
    if (!mr) {
        return out;   // TIC morto ou sem MeshRenderer — sem crash, sem ação
    }

    if (menuKind == 1) {
        // ---- seletor de MESHES -------------------------------------------
        if (pick == 1) {   // cube procedural
            mr->mesh = res.cubeMesh;
            mr->material = res.material;
            mr->meshPath.clear();
            out.applied = true;
            std::snprintf(out.toast, sizeof(out.toast), "mesh: cube");
            std::snprintf(out.log, sizeof(out.log), "editor: mesh cube aplicado");
        } else {
            const size_t idx = static_cast<size_t>(pick - 2);
            if (idx >= catalog.meshes.size()) {
                return out;   // fora do catálogo — sem crash
            }
            const std::string rel = std::string("meshes/") + catalog.meshes[idx];
            if (Mesh* m = res.mesh ? res.mesh(rel) : nullptr) {
                mr->mesh = m;
                mr->material = res.material;
                mr->meshPath = rel;
                // F5.1-B: textura embutida do glTF/GLB aplica-se logo
                // (import sem PC — o material fica referenciado)
                bool withTex = false;
                if (res.meshTextureFor) {
                    const std::string texRel = res.meshTextureFor(rel);
                    if (!texRel.empty()) {
                        std::string warn;
                        if (const Texture* tex = res.texture ? res.texture(texRel, &warn)
                                                             : nullptr) {
                            mr->texture = tex;
                            mr->texPath = texRel;
                            withTex = true;
                        }
                    }
                }
                out.applied = true;
                std::snprintf(out.toast, sizeof(out.toast), "%s",
                              withTex ? "mesh aplicado (+textura)" : "mesh aplicado");
                if (withTex) {
                    std::snprintf(out.log, sizeof(out.log),
                                  "editor: mesh %s aplicado com textura %s",
                                  rel.c_str(), mr->texPath.c_str());
                } else {
                    std::snprintf(out.log, sizeof(out.log),
                                  "editor: mesh %s aplicado", rel.c_str());
                }
            } else {
                // carga falhou — o estado ANTERIOR fica intacto
                std::snprintf(out.toast, sizeof(out.toast), "falha ao carregar mesh");
                std::snprintf(out.log, sizeof(out.log),
                              "editor: mesh %s FALHOU ao carregar", rel.c_str());
            }
        }
    } else if (menuKind == 2) {
        // ---- seletor de TEXTURAS -----------------------------------------
        if (pick == 1) {   // none → liberta a referência
            mr->texture = nullptr;
            mr->texPath.clear();
            out.applied = true;
            std::snprintf(out.toast, sizeof(out.toast), "tex: none");
            std::snprintf(out.log, sizeof(out.log), "material: textura removida");
        } else {
            const size_t idx = static_cast<size_t>(pick - 2);
            if (idx >= catalog.textures.size()) {
                return out;   // fora do catálogo — sem crash
            }
            const std::string rel = std::string("textures/") + catalog.textures[idx];
            std::string warn;
            if (const Texture* tex = res.texture ? res.texture(rel, &warn) : nullptr) {
                mr->texture = tex;
                mr->texPath = rel;
                out.applied = true;
                std::snprintf(out.toast, sizeof(out.toast), "%s",
                              warn.empty() ? "textura aplicada" : warn.c_str());
                std::snprintf(out.log, sizeof(out.log),
                              "material: textura aplicada %s", rel.c_str());
            } else {
                // carga falhou — o estado ANTERIOR fica intacto
                std::snprintf(out.toast, sizeof(out.toast), "falha ao carregar textura");
                std::snprintf(out.log, sizeof(out.log),
                              "material: textura %s FALHOU ao carregar", rel.c_str());
            }
        }
    }
    return out;
}


// ---------------------------------------------------------------------------
// 0.6.8 — PLAY MODE com janela própria
// ---------------------------------------------------------------------------

void closeAllOverlays(EditorState& st) {
    st.plusMenu = false;
    st.fileMenu = false;
    st.settingsMenu = false;
    st.assetMenu = 0;
    st.storageDialog = false;
    st.importMenu = false;
    st.logViewer = false;
    st.logViewerJustOpened = false;
    // 0.7.0 — os overlays da gestão de TICs/UI também fecham (a SELEÇÃO e
    // o offset dos scrolls são estado de painel e ficam intactos)
    st.contextMenu = false;
    st.removeDialog = false;
    st.textInput = false;
    st.elDrag = false;
}

// Orbit da câmara — extraído do main.cpp (era globais + função estática).
// A lógica é INTACTA (regra F3 do dono-do-gesto, pinch, clamps da Camera);
// o que muda: o estado vive num OrbitState puro e o PLAY desliga o orbit.
void updateCameraOrbit(Camera& cam, OrbitState& st, const InputState& in,
                       const UiRect& view, u32 claimedMask, bool playMode) {
    // 0.6.8: em PLAY o orbit está DESATIVADO — 1 dedo = controlos de toque.
    // O gesto pendente é RESETADO (um drag que começou no editor e o Play
    // entretanto não pode continuar a orbitar) e o estado fica limpo para
    // o regresso ao editor.
    if (playMode) {
        st.active = false;
        st.gestureInView = false;
        st.pinchPrev = 0.0f;
        return;
    }

    u32 active = 0;
    for (u32 s = 0; s < kMaxPointerSlots; ++s) {
        if (in.down(s) && !(claimedMask & (1u << s))) {
            ++active;
        }
    }
    if (active == 0) {
        st.gestureInView = false;
        st.active = false;
        st.pinchPrev = 0.0f;
        return;
    }

    // F3: o PRIMEIRO toque decide o dono do gesto. Se nasce num painel
    // (Hierarchy/Inspector/toolbar) ou num controlo de toque (F4), a câmara
    // não orbita — mesmo que o dedo depois atravessasse o viewport.
    if (!st.gestureInView) {
        for (u32 s = 0; s < kMaxPointerSlots; ++s) {
            if (!in.pressed(s)) continue;
            if (claimedMask & (1u << s)) break;   // nasceu num controlo
            f32 x, y;
            in.pos(s, x, y);
            if (x >= view.x && x < view.x + view.w && y >= view.y && y < view.y + view.h) {
                st.gestureInView = true;
            }
            break;   // só o primeiro pointer com edge interessa
        }
    }
    if (!st.gestureInView) {
        return;
    }

    constexpr f32 kSens = 0.0075f;   // rad/px (~0,43° por pixel) — como no main
    if (active >= 2) {
        // pinch: distância entre os dois primeiros dedos ativos (não reclamados)
        f32 x0 = 0, y0 = 0, x1 = 0, y1 = 0;
        bool got0 = false, got1 = false;
        for (u32 s = 0; s < kMaxPointerSlots && !(got0 && got1); ++s) {
            if (!in.down(s)) continue;
            if (claimedMask & (1u << s)) continue;
            f32 x, y;
            in.pos(s, x, y);
            if (!got0) { x0 = x; y0 = y; got0 = true; }
            else       { x1 = x; y1 = y; got1 = true; }
        }
        if (got0 && got1) {
            const f32 d = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
            if (st.pinchPrev > 0.0f && d > 1.0f) {
                cam.zoomBy(st.pinchPrev / d);   // dedos afastam → aproxima
            }
            st.pinchPrev = d;
        }
        st.active = false;
        return;
    }
    st.pinchPrev = 0.0f;
    if (active == 1) {
        for (u32 s = 0; s < kMaxPointerSlots; ++s) {
            if (!in.down(s)) continue;
            if (claimedMask & (1u << s)) continue;
            f32 x, y;
            in.pos(s, x, y);
            if (st.active) {
                cam.orbit((x - st.x) * kSens, (y - st.y) * kSens);
            }
            st.x = x;
            st.y = y;
            st.active = true;
            break;
        }
    } else {
        st.active = false;
    }
}

bool drawPlayBar(UiContext& ui, const InputState& in, f32 sw, f32 sh, int fps) {
    // F4.2: barra na faixa da toolbar, DENTRO da safe-area (nada atrás da
    // nav/status bar). Tema mono — mesma linguagem da toolbar.
    const UiRect r = playBarRect(sw, sh, ui.safeArea());
    ui.panel(r.x, r.y, r.w, r.h, theme::PANEL);
    ui.panel(r.x, r.y + r.h - 1.0f, r.w, 1.0f, theme::LINE);

    // Stop (id 5 — faixa exclusiva; ver EditorLayout.h)
    const UiRect stop = playStopButtonRect(r);
    const bool stopClicked = ui.button(kPlayStopId, stop.x, stop.y, stop.w,
                                       stop.h, "Stop");

    if (ui.hasFont()) {
        const f32 th = ui.fontHeight();
        const f32 cy = r.y + r.h * 0.5f + th * 0.30f;
        // estado "a correr" + fps ao lado do Stop
        char run[64];
        std::snprintf(run, sizeof(run), "a correr · fps %d", fps);
        ui.label(stop.x + stop.w + 24.0f, cy, run, theme::TEXT);
        // aviso à direita: nunca sai da barra (labelFitted)
        const f32 warnW = r.w * 0.5f;
        ui.labelFitted(r.x + r.w - warnW - kPad, cy,
                       "simulação — alterações descartadas ao parar",
                       theme::LINE, warnW);
    }
    return stopClicked;
}


// ---------------------------------------------------------------------------
// 0.6.9 — seletor de modo do gizmo na toolbar (grupo à direita)
// ---------------------------------------------------------------------------

void drawGizmoToolbar(UiContext& ui, const InputState& in, GizmoModeState& st) {
    // o grupo vive na FAIXA DA TOOLBAR (mesma altura dos 3 botões), à
    // direita; os 3 botões Menu/Play/Settings ficam intactos à esquerda.
    // 0.7.0: o limite esquerdo passou a ser o FIM do separador "3D | UI"
    // (toolbarModeEndX) — nunca por cima dele; em ecrãs estreitos os botões
    // ENCOLHEM em vez de sobreporem (nada sobreposto, o critério da fase).
    const UiRect bar = ui.toolbarRect();
    f32 btnW = 150.0f;
    const f32 btnH = 56.0f;
    const f32 snapW = 120.0f;
    const f32 gap = 10.0f;
    const f32 by = bar.y + (safe::kToolbarH - btnH) * 0.5f;

    const f32 leftBound =
        toolbarModeEndX(ui.screenWidth(), ui.screenHeight(), ui.safeArea());
    const f32 avail = bar.x + bar.w - kPad - leftBound;
    // 4 controlos: [Mover][Rodar][Escalar][Snap] — encolhe os 3 de modo se
    // não couber (min 80px; o Snap é fixo)
    const f32 wanted = 3.0f * btnW + snapW + 3.0f * gap;
    if (wanted > avail && avail > snapW + 3.0f * 80.0f + 3.0f * gap) {
        btnW = (avail - snapW - 3.0f * gap) / 3.0f;
    }
    f32 x = bar.x + bar.w - kPad - (3.0f * btnW + snapW + 3.0f * gap);
    if (x < leftBound) {
        x = leftBound;   // defesa: nunca por cima do separador
    }

    static const char* kModeNames[3] = {"Mover", "Rodar", "Escalar"};
    for (int i = 0; i < 3; ++i) {
        const u64 id = kGizmoModeMoveId + static_cast<u64>(i);
        if (ui.button(id, x, by, btnW, btnH, kModeNames[i])) {
            st.mode = i;
        }
        if (st.mode == i) {
            // modo ATIVO: frame ACCENT por cima do botão (inversível demais
            // faria os rótulos ilegíveis a 28px — o frame salienta)
            ui.frame(x, by, btnW, btnH, 2.0f, theme::ACCENT);
        }
        x += btnW + gap;
    }
    // Snap (toggle — o rótulo mostra o estado)
    if (ui.button(kGizmoSnapId, x, by, snapW, btnH,
                  st.snap ? "Snap on" : "Snap")) {
        st.snap = !st.snap;
    }
    if (st.snap) {
        ui.frame(x, by, snapW, btnH, 2.0f, theme::ACCENT);
    }
    (void)in;
}
} // namespace editor
} // namespace vv
