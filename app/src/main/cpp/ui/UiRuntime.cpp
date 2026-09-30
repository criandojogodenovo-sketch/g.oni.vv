// ui/UiRuntime.cpp — runtime da UI criável (0.7.0): desenho, hit-test e ações.
//
// O desenho é 100% tema mono (quads + glifos do atlas; sem cores novas — as
// cores dos elementos são DADOS do utilizador, o default é o token PANEL).
// Tudo é afervel no CI: os testes leem os batches (solids/glyphs) e aferem
// geometria, z-order e ausência de sobreposição.
#include "ui/UiRuntime.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "core/Scene.h"
#include "ui/UiContext.h"

namespace vv {
namespace ui {
namespace {

// espelho mono dos tokens do tema (evita include de UiContext.h aqui —
// drawElement recebe o UiContext por referência e usa os tokens de lá)
constexpr f32 kLine = 0.1803922f;
constexpr f32 kText = 0.9019608f;

bool inRect(const UiRect& r, f32 x, f32 y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

} // namespace

// ---- menu (composto 0.7.3 — mas a geometria já serve o hit-test 0.7.0) -----

u32 menuLineCount(const UiElement& e) {
    if (e.text.empty()) {
        return 0;
    }
    u32 n = 1;
    for (char c : e.text) {
        if (c == '\n') {
            ++n;
        }
    }
    return n;
}

bool menuLineAt(const UiElement& e, u32 row, std::string& outLabel,
                std::string& outTarget) {
    if (row >= menuLineCount(e)) {
        return false;
    }
    u32 cur = 0;
    const char* start = e.text.c_str();
    const char* end = start;
    for (const char* p = start;; ++p) {
        if (*p == '\n' || *p == '\0') {
            if (cur == row) {
                end = p;
                break;
            }
            start = p + 1;
            ++cur;
        }
    }
    std::string line(start, static_cast<size_t>(end - start));
    const size_t gt = line.find('>');
    if (gt == std::string::npos) {
        outLabel = line;          // sem '>' → o alvo é o próprio label
        outTarget = line;
    } else {
        outLabel = line.substr(0, gt);
        outTarget = line.substr(gt + 1);
    }
    return true;
}

UiRect menuItemRect(const UiElement& e, const UiRect& base, u32 row) {
    const u32 n = menuLineCount(e);
    const f32 rowH = n > 0 ? base.h / static_cast<f32>(n) : base.h;
    return {base.x, base.y + static_cast<f32>(row) * rowH, base.w, rowH};
}

// ---- desenho -------------------------------------------------------------------

bool drawElement(UiContext& uictx, const UiElement& e, const UiRect& r,
                 bool sel) {
    if (!e.visible || r.w <= 0.0f || r.h <= 0.0f) {
        return false;
    }
    const f32 line[4] = {kLine, kLine, kLine, 1.0f};
    const f32 text[4] = {kText, kText, kText, 1.0f};

    switch (e.kind) {
        case UiElement::Kind::Panel:
            uictx.panel(r.x, r.y, r.w, r.h, e.color);
            uictx.frame(r.x, r.y, r.w, r.h, 1.0f, line);
            break;

        case UiElement::Kind::Label: {
            uictx.panel(r.x, r.y, r.w, r.h, e.color);
            // texto centrado verticalmente, fitted à largura (nunca sai)
            const TextMetrics tm = uictx.textMetrics();
            const f32 baseline =
                r.y + (r.h - tm.block()) * 0.5f + tm.ascent;
            uictx.labelFitted(r.x + 10.0f, baseline, e.text.c_str(), text,
                              r.w - 20.0f);
            break;
        }

        case UiElement::Kind::Button: {
            uictx.panel(r.x, r.y, r.w, r.h, e.color);
            uictx.frame(r.x, r.y, r.w, r.h, 2.0f, text);
            const TextMetrics tm = uictx.textMetrics();
            const f32 baseline =
                r.y + (r.h - tm.block()) * 0.5f + tm.ascent;
            uictx.labelFitted(r.x + 10.0f, baseline, e.text.c_str(), text,
                              r.w - 20.0f);
            break;
        }

        case UiElement::Kind::Image:
            // 0.7.0: Image = quad de textura do projeto (ref "image"); sem
            // resolver de textura (CI/editor sem asset) → PLACEHOLDER mono:
            // moldura + diagonal (honesto: sabe-se que é uma imagem sem
            // textura). O main liga o resolver ao GpuAssets.
            if (uictx.imageQuad(r.x, r.y, r.w, r.h, e.image, e.color)) {
                break;   // textura emitida (batch de imagens do UiContext)
            }
            uictx.panel(r.x, r.y, r.w, r.h, e.color);
            uictx.frame(r.x, r.y, r.w, r.h, 2.0f, line);
            uictx.drawLine(r.x, r.y, r.x + r.w, r.y + r.h, 1.0f, line);
            uictx.drawLine(r.x + r.w, r.y, r.x, r.y + r.h, 1.0f, line);
            break;

        case UiElement::Kind::Menu: {
            // lista vertical de botões (uma linha por item do texto)
            const u32 n = menuLineCount(e);
            for (u32 i = 0; i < n; ++i) {
                std::string label, target;
                menuLineAt(e, i, label, target);
                const UiRect row = menuItemRect(e, r, i);
                uictx.panel(row.x, row.y, row.w, row.h, e.color);
                uictx.frame(row.x, row.y, row.w, row.h, 1.0f, line);
                const TextMetrics tm = uictx.textMetrics();
                const f32 baseline =
                    row.y + (row.h - tm.block()) * 0.5f + tm.ascent;
                uictx.labelFitted(row.x + 10.0f, baseline, label.c_str(),
                                  text, row.w - 20.0f);
            }
            break;
        }

        case UiElement::Kind::Card: {
            // panel + borda + label (título no topo)
            uictx.panel(r.x, r.y, r.w, r.h, e.color);
            uictx.frame(r.x, r.y, r.w, r.h, 2.0f, text);
            const TextMetrics tm = uictx.textMetrics();
            const f32 titleH = tm.block() + 12.0f;
            uictx.panel(r.x, r.y + titleH, r.w, 1.0f, line);
            const f32 baseline = r.y + (titleH - tm.block()) * 0.5f + tm.ascent;
            uictx.labelFitted(r.x + 10.0f, baseline, e.text.c_str(), text,
                              r.w - 20.0f);
            break;
        }

        case UiElement::Kind::Article: {
            // texto multilinha com wrap pela largura (greedy por palavras)
            uictx.panel(r.x, r.y, r.w, r.h, e.color);
            const TextMetrics tm = uictx.textMetrics();
            const f32 rowH = tm.block() + 4.0f;
            f32 cy = r.y + 8.0f;
            const char* p = e.text.c_str();
            char word[96];
            char lineBuf[256];
            lineBuf[0] = '\0';
            const f32 maxW = r.w - 20.0f;
            while (*p && cy + tm.block() <= r.y + r.h) {
                // palavra seguinte
                int wl = 0;
                while (*p && *p != ' ' && *p != '\n' && wl < 95) {
                    word[wl++] = *p++;
                }
                word[wl] = '\0';
                const bool hardBreak = (*p == '\n');
                while (*p == ' ' || *p == '\n') {
                    ++p;
                }
                char candidate[256];
                if (lineBuf[0]) {
                    std::snprintf(candidate, sizeof(candidate), "%s %s",
                                  lineBuf, word);
                } else {
                    std::snprintf(candidate, sizeof(candidate), "%s", word);
                }
                if (lineBuf[0] && uictx.fontWidth(candidate) > maxW) {
                    // linha cheia → emite e começa nova com a palavra
                    uictx.labelFitted(r.x + 10.0f, cy + tm.ascent, lineBuf,
                                      text, maxW);
                    cy += rowH;
                    std::snprintf(lineBuf, sizeof(lineBuf), "%s", word);
                } else {
                    std::snprintf(lineBuf, sizeof(lineBuf), "%s", candidate);
                }
                if (hardBreak) {
                    uictx.labelFitted(r.x + 10.0f, cy + tm.ascent, lineBuf,
                                      text, maxW);
                    cy += rowH;
                    lineBuf[0] = '\0';
                }
            }
            if (lineBuf[0] && cy + tm.block() <= r.y + r.h) {
                uictx.labelFitted(r.x + 10.0f, cy + tm.ascent, lineBuf, text,
                                  maxW);
            }
            break;
        }
    }

    if (sel) {
        // moldura de seleção do editor (mono: frame ACCENT tracejado — sem
        // tracejado no quad batch: frame contínuo fino)
        const f32 accent[4] = {0.9607843f, 0.9607843f, 0.9607843f, 1.0f};
        uictx.frame(r.x, r.y, r.w, r.h, 2.0f, accent);
    }
    return true;
}

u32 drawCanvas(UiContext& uictx, const UiCanvas& c, f32 sw, f32 sh,
               const safe::Insets& ins) {
    u32 drawn = 0;
    for (const UiElement& e : c.elements) {
        const UiRect r = elementRect(e, sw, sh, ins);
        if (drawElement(uictx, e, r)) {
            ++drawn;   // só os DESENHADOS contam (invisíveis não)
        }
    }
    return drawn;
}

// ---- hit-test --------------------------------------------------------------------

CanvasHit hitTestCanvas(const Scene& scene, f32 x, f32 y, f32 sw, f32 sh,
                        const safe::Insets& ins) {
    CanvasHit hit;   // o ÚLTIMO que contém o ponto ganha (topo do z-order)
    scene.forEachActive([&](const Tic& t) {
        if (!t.visible) {
            return;   // canvas de TIC invisível não hit-testa (não desenha)
        }
        const UiCanvas* c =
            scene.components().uiCanvases().find(t.handle);
        if (!c) {
            return;
        }
        for (size_t i = 0; i < c->elements.size(); ++i) {
            const UiElement& e = c->elements[i];
            if (!e.visible) {
                continue;
            }
            const UiRect r = elementRect(e, sw, sh, ins);
            if (!inRect(r, x, y)) {
                continue;
            }
            if (e.kind == UiElement::Kind::Button) {
                hit.valid = true;
                hit.tic = t.handle;
                hit.element = static_cast<i32>(i);
                hit.menuItem = -1;
            } else if (e.kind == UiElement::Kind::Menu) {
                const u32 n = menuLineCount(e);
                const f32 rowH = n > 0 ? r.h / static_cast<f32>(n) : r.h;
                const i32 row = static_cast<i32>((y - r.y) / (rowH > 0 ? rowH : 1));
                if (row >= 0 && static_cast<u32>(row) < n) {
                    hit.valid = true;
                    hit.tic = t.handle;
                    hit.element = static_cast<i32>(i);
                    hit.menuItem = row;
                }
            }
        }
    });
    return hit;
}

// ---- ações declarativas ------------------------------------------------------------

namespace {

// procura um elemento por NOME em todos os canvases (o alvo das ações
// Show/Hide/Toggle pode estar em QUALQUER canvas da cena)
UiElement* findElementAnywhere(Scene& scene, const std::string& name,
                               Handle& outOwner) {
    UiElement* found = nullptr;
    scene.forEachActive([&](Tic& t) {
        if (found) {
            return;
        }
        if (UiCanvas* c = scene.components().uiCanvases().find(t.handle)) {
            const i32 idx = c->findElement(name);
            if (idx >= 0) {
                found = &c->elements[static_cast<size_t>(idx)];
                outOwner = t.handle;
            }
        }
    });
    return found;
}

} // namespace

UiActionResult applyUiAction(Scene& scene, const UiElement& e,
                             const UiActionCtx& ctx,
                             const char* targetOverride) {
    UiActionResult out;
    const std::string target =
        targetOverride ? std::string(targetOverride) : e.target;

    switch (e.action) {
        case UiElement::Action::None:
            break;

        case UiElement::Action::ShowPanel:
        case UiElement::Action::HidePanel:
        case UiElement::Action::TogglePanel: {
            Handle owner{};
            UiElement* tgt = findElementAnywhere(scene, target, owner);
            if (!tgt) {
                std::snprintf(out.toast, sizeof(out.toast),
                              "elemento '%s' nao existe", target.c_str());
                std::snprintf(out.log, sizeof(out.log),
                              "ui: acao %s — alvo '%s' nao encontrado",
                              uiActionName(e.action), target.c_str());
                out.wantToast = true;
                break;
            }
            switch (e.action) {
                case UiElement::Action::ShowPanel:   tgt->visible = true;  break;
                case UiElement::Action::HidePanel:   tgt->visible = false; break;
                default:                             tgt->visible = !tgt->visible; break;
            }
            out.acted = true;
            std::snprintf(out.toast, sizeof(out.toast), "%s: %s %s",
                          target.c_str(),
                          tgt->visible ? "mostrado" : "escondido",
                          uiActionName(e.action));
            std::snprintf(out.log, sizeof(out.log),
                          "ui: %s %s (%s)", uiActionName(e.action),
                          target.c_str(), tgt->visible ? "visivel" : "escondido");
            break;
        }

        case UiElement::Action::LoadScene:
        case UiElement::Action::TransitionScene: {
            // 0.7.1: o carregamento vive no CHAMADOR (projeto/transição);
            // aqui decide-se se a cena existe (toast honesto se não). Sem
            // callback loadScene a ação NÃO finge sucesso — o toast diz o
            // que falta. Scene.Load = troca INSTANTÂNEA; Scene.Transition =
            // fade/slide conforme o param do elemento.
            const bool trans = e.action == UiElement::Action::TransitionScene;
            if (target.empty()) {
                std::snprintf(out.toast, sizeof(out.toast), "acao sem alvo");
                out.wantToast = true;
                break;
            }
            if (ctx.sceneExists && !ctx.sceneExists(target, ctx.user)) {
                std::snprintf(out.toast, sizeof(out.toast),
                              "cena '%s' nao existe", target.c_str());
                std::snprintf(out.log, sizeof(out.log),
                              "ui: cena '%s' FALHOU — nao existe no projeto",
                              target.c_str());
                out.wantToast = true;
                break;
            }
            if (!ctx.loadScene) {
                std::snprintf(out.toast, sizeof(out.toast),
                              "cena '%s': sem carregador", target.c_str());
                std::snprintf(out.log, sizeof(out.log),
                              "ui: cena '%s' existe — loadScene nao ligado",
                              target.c_str());
                out.wantToast = true;
                break;
            }
            const SceneSwap style =
                !trans ? SceneSwap::Instant
                       : (uiElementTransition(e) == UiElement::Transition::Slide
                              ? SceneSwap::Slide
                              : SceneSwap::Fade);
            ctx.loadScene(target, style, ctx.user);
            out.acted = true;
            std::snprintf(out.toast, sizeof(out.toast),
                          trans ? "cena: %s (%s)" : "cena: %s",
                          target.c_str(),
                          style == SceneSwap::Slide ? "slide" : "fade");
            std::snprintf(out.log, sizeof(out.log),
                          "ui: cena '%s' a carregar (%s)", target.c_str(),
                          style == SceneSwap::Instant ? "instantaneo"
                          : style == SceneSwap::Slide ? "slide" : "fade");
            break;
        }

        case UiElement::Action::Spawn: {
            PresetKind kind = PresetKind::Count;
            if (target == "PlayerBody3D")        kind = PresetKind::PlayerBody3D;
            else if (target == "CharacterBody3D") kind = PresetKind::CharacterBody3D;
            else if (target == "StaticBody3D")    kind = PresetKind::StaticBody3D;
            else if (target == "RigidBody3D")     kind = PresetKind::RigidBody3D;
            if (kind == PresetKind::Count || !ctx.spawnPreset) {
                std::snprintf(out.toast, sizeof(out.toast),
                              "preset '%s' invalido", target.c_str());
                std::snprintf(out.log, sizeof(out.log),
                              "ui: spawn '%s' FALHOU — preset invalido",
                              target.c_str());
                out.wantToast = true;
                break;
            }
            const Handle h = ctx.spawnPreset(kind, ctx.user);
            if (!h.valid()) {
                std::snprintf(out.toast, sizeof(out.toast), "spawn falhou");
                out.wantToast = true;
                break;
            }
            out.acted = true;
            std::snprintf(out.toast, sizeof(out.toast), "spawn: %s",
                          target.c_str());
            std::snprintf(out.log, sizeof(out.log), "ui: spawn %s criado",
                          target.c_str());
            break;
        }
    }
    return out;
}

} // namespace ui
} // namespace vv
