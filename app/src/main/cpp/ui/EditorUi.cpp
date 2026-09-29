#include "ui/EditorUi.h"
#include "components/InputMap.h"
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

    u32 row = 0;
    scene.forEachActive([&](const Tic& t) {
        const f32 ry = listTop + static_cast<f32>(row) * kRowH - off;
        ++row;
        // o clip da região recusa quads fora do ecrã (rows escondidas custam 0)
        char clipped[40];
        std::snprintf(clipped, sizeof(clipped), "%.30s", t.name.c_str());
        ui.button(kIdRowBase + t.handle.index, x + kPad, ry + 4.0f,
                  w - 2.0f * kPad, kRowH - 8.0f, clipped);   // só desenha (F4.1)
        if (st.selected == t.handle) {
            ui.frame(x + kPad, ry + 4.0f, w - 2.0f * kPad, kRowH - 8.0f, 2.0f, theme::ACCENT);
        }
    });
    ui.endScroll();

    if (nTics == 0) {
        ui.labelFitted(x + kPad, listTop + kRowH, "(vazio - use +)", theme::TEXT,
                       w - 2.0f * kPad);
    }

    // tap re-despachado → seleção da linha sob o dedo (mesmo após scroll)
    // F5.0-fix: POR ID — a Hierarchy só consome taps nascidos nela
    f32 tx, ty;
    if (ui.scrollTap(kHierarchyScrollId, tx, ty) && scroll::inside(listRegion, tx, ty)) {
        const i32 sel = hierarchyRowAtTap(ty, listTop, off, nTics);
        if (sel >= 0) {
            u32 i = 0;
            scene.forEachActive([&](const Tic& t) {
                if (i == static_cast<u32>(sel)) {
                    st.selected = t.handle;
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
    InspRow plan[20];
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

    // ---- desenho: UMA passagem pelo plano; nenhum cursor local
    for (u32 i = 0; i < nRows; ++i) {
        const InspRow& r = plan[i];
        const f32 ry = contentTop + r.y - off;   // topo da linha em ECRÃ
        switch (r.kind) {
        case InspRow::Kind::Name:
            ui.labelFitted(x + kPad, inspBaseline(ry, r.h, tm), tic->name.c_str(),
                           theme::ACCENT, w - 2.0f * kPad);   // B2: ellipsis
            break;
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
                r.kind != InspRow::Kind::TexButton) {
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
            } else {
                st.assetMenu = 2;                     // F5-E: seletor de texturas
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

int drawPlusMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh, EditorState& st) {
    const f32 w = kMenuW;
    const f32 h = kHeaderH + 4.0f * 64.0f + kPad;
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
    ui.label(x + kPad, y + kHeaderH * 0.5f + th * 0.30f, "CRIAR TIC", theme::TEXT);

    int chosen = 0;
    const char* names[4] = {"PlayerBody3D", "CharacterBody3D", "StaticBody3D",
                            "RigidBody3D"};
    for (int i = 0; i < 4; ++i) {
        if (ui.button(static_cast<u64>(20 + i), x + kPad, y + kHeaderH + i * 64.0f,
                      w - 2.0f * kPad, 56.0f, names[i])) {
            chosen = i + 1;
            st.plusMenu = false;
        }
    }
    return chosen;
}

int drawFileMenu(UiContext& ui, const InputState& in, f32 sw, f32 sh, EditorState& st) {
    const f32 w = kMenuW;
    const f32 h = kHeaderH + 3.0f * 64.0f + kPad;   // F5-E: + Export OBJ
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
    if (ui.button(30, x + kPad, y + kHeaderH, w - 2.0f * kPad, 56.0f, "Save cena")) {
        chosen = 1;
        st.fileMenu = false;
    }
    if (ui.button(31, x + kPad, y + kHeaderH + 64.0f, w - 2.0f * kPad, 56.0f, "Load cena")) {
        chosen = 2;
        st.fileMenu = false;
    }
    if (ui.button(32, x + kPad, y + kHeaderH + 128.0f, w - 2.0f * kPad, 56.0f,
                  "Export OBJ")) {
        chosen = 3;
        st.fileMenu = false;
    }
    return chosen;
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

} // namespace editor
} // namespace vv
