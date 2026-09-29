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
constexpr u64 kIdSliderBase = 2000;
constexpr u64 kIdVelX      = 2100;
constexpr u64 kIdAddTc     = 3001;
constexpr f32 kAddTcH      = 34.0f;   // altura do botão "add TouchControls"

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

// Linha do Inspector: label + slider + valor; aplica em `value` via setter.
// y já vem em coords de ECRÃ (cy − offset feito pelo chamador).
bool sliderRow(UiContext& ui, u64 id, f32 x, f32 y, const char* labelText,
               f32 minV, f32 maxV, f32& value, const char* fmt) {
    ui.label(x + kPad, y + kSliderRow * 0.5f + 9.0f, labelText, theme::TEXT);
    const f32 trackX = x + 84.0f;
    const f32 trackW = 118.0f;
    const bool changed = ui.slider(id, trackX, y, trackW, kSliderRow, minV, maxV, value);

    char val[24];
    std::snprintf(val, sizeof(val), fmt, value);
    if (ui.hasFont()) {
        const f32 tw = ui.fontWidth(val);
        ui.label(x + kPanelW - kPad - tw, y + kSliderRow * 0.5f + 9.0f, val, theme::TEXT);
    }
    return changed;
}
} // namespace

UiRect centerRect(f32 sw, f32 sh) {
    return centerRect(sw, sh, safe::Insets{});   // sem safe-area (compat/testes)
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
        ui.label(x + kPad, listTop + kRowH, "(vazio - use +)", theme::TEXT);
    }

    // tap re-despachado → seleção da linha sob o dedo (mesmo após scroll)
    f32 tx, ty;
    if (ui.scrollTap(tx, ty) && scroll::inside(listRegion, tx, ty)) {
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
// INSPECTOR — componentes do TIC com scroll (F4.1): BodyComp, velx e o botão
// "add TouchControls" no fundo ficam sempre alcançáveis. Slider mantém
// prioridade de captura; tap re-despachado aciona o botão do fundo.
// ---------------------------------------------------------------------------
bool drawInspector(UiContext& ui, Scene& scene, EditorState& st) {
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
        ui.label(x + kPad, y + kHeaderH + kRowH, "(nada selecionado)", theme::LINE);
        return false;
    }

    // região de scroll: abaixo do cabeçalho; conteúdo medido por
    // inspectorContentHeight (fonte única — ui/EditorLayout.h)
    const f32 contentTop = y + kHeaderH + 4.0f;
    const f32 listH = h - kHeaderH - 4.0f;
    const f32 contentH = inspectorContentHeight(*tic);

    ui.beginScroll(kIdScrollInsp, {x, contentTop, w, listH}, contentH);
    const f32 off = ui.scrollOffset();

    f32 cy = contentTop;   // coords de CONTEÚDO; widgets em (cy − off)
    char clipped[40];
    std::snprintf(clipped, sizeof(clipped), "%s", tic->name.c_str());
    ui.label(x + kPad, (cy - off) + 8.0f, clipped, theme::ACCENT);
    cy += 30.0f;

    bool edited = false;

    if (Transform3D* tr = tic->getComponent<Transform3D>()) {
        ui.label(x + kPad, (cy - off) + 8.0f, "Transform3D", theme::TEXT);
        cy += 26.0f;
        ui.panel(x + kPad, (cy - off) - 3.0f, w - 2.0f * kPad, 1.0f, theme::LINE);

        struct Row { const char* label; f32 min, max; const char* fmt; f32* value; };
        f32 posArr[3] = {tr->pos.x, tr->pos.y, tr->pos.z};
        // rot em graus (extrai do quat a cada frame)
        f32 ex = 0.0f, ey = 0.0f, ez = 0.0f;
        Quat::toEuler(tr->rot, ex, ey, ez);
        f32 rotDeg[3] = {rad2deg(ex), rad2deg(ey), rad2deg(ez)};
        f32 sclArr[3] = {tr->scale.x, tr->scale.y, tr->scale.z};

        Row rows[9] = {
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

        u64 id = kIdSliderBase;
        for (Row& rw : rows) {
            if (sliderRow(ui, id++, x, cy - off, rw.label, rw.min, rw.max, *rw.value, rw.fmt)) {
                edited = true;
            }
            cy += kSliderRow;
        }

        // escreve de volta no componente (rot: graus → quat YXZ)
        tr->pos = Vec3{posArr[0], posArr[1], posArr[2]};
        tr->rot = Quat::fromEuler(deg2rad(rotDeg[0]), deg2rad(rotDeg[1]), deg2rad(rotDeg[2]));
        tr->scale = Vec3{sclArr[0], sclArr[1], sclArr[2]};
        if (edited) {
            tr->updateWorld();   // feedback imediato (TransformSystem reconfirma no passo)
        }
    }

    if (const MeshRenderer* mr = tic->getComponent<MeshRenderer>()) {
        ui.label(x + kPad + 12.0f, (cy - off) + 8.0f,
                 mr->mesh ? "mesh: cube" : "mesh: -", theme::TEXT);
        cy += 26.0f;
    }

    if (const InputMap* im = tic->getComponent<InputMap>()) {
        char line[48];
        std::snprintf(line, sizeof(line), "input: %s",
                      im->source ? "fonte ligada" : "sem fonte");
        ui.label(x + kPad + 12.0f, (cy - off) + 8.0f, line, theme::TEXT);
        cy += 26.0f;
    }

    // F4: BodyComp — tipo/forma/estado + slider de velocidade (lança corpos
    // para testar CCD e empurrões no device)
    if (BodyComp* b = tic->getComponent<BodyComp>()) {
        char line[64];
        std::snprintf(line, sizeof(line), "body: %s - %s - chao: %s",
                      BodyComp::typeName(b->type), BodyComp::shapeName(b->shape),
                      b->grounded ? "sim" : "nao");
        ui.label(x + kPad, (cy - off) + 8.0f, line, theme::ACCENT);
        cy += 26.0f;

        f32 vx = b->velocity.x;
        if (sliderRow(ui, kIdVelX, x, cy - off, "velx", -60.0f, 60.0f, vx, "%.1f")) {
            b->velocity.x = vx;
        }
        cy += kSliderRow;
    }

    // F4: TouchControls adicionável a qualquer TIC com InputMap (não-criável
    // no resto: layout fixo; UI criável é F6). O botão vive no FUNDO da lista
    // e é acionado por tap re-despachado (clicável mesmo após scroll).
    if (tic->getComponent<InputMap>()) {
        if (!tic->getComponent<TouchControls>()) {
            ui.button(kIdAddTc, x + kPad, (cy - off) + 2.0f, w - 2.0f * kPad, kAddTcH,
                      "add TouchControls");   // só desenha (F4.1)
            cy += 42.0f;
        } else {
            ui.label(x + kPad + 12.0f, (cy - off) + 8.0f, "tc: stick + jump", theme::TEXT);
            cy += 26.0f;
        }
    }

    ui.endScroll();

    // tap re-despachado → botão "add TouchControls" (hit-test do rect em ecrã)
    f32 tx, ty;
    if (ui.scrollTap(tx, ty) && tic->getComponent<InputMap>() &&
        !tic->getComponent<TouchControls>()) {
        const f32 btnTop = contentTop + inspectorAddTcTop(contentH) - off;
        if (tx >= x + kPad && tx < x + w - kPad &&
            ty >= btnTop && ty < btnTop + kAddTcH) {
            tic->addComponent<TouchControls>();
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
    const f32 h = kHeaderH + 2.0f * 64.0f + kPad;
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
    return chosen;
}

} // namespace editor
} // namespace vv
