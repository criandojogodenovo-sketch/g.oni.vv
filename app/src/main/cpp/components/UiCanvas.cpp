// components/UiCanvas.cpp — implementação do componente UiCanvas (0.7.0).
//
// GL-free e host-testável: sem includes de Android/GL — só a lógica de
// elementos (criação com nome único, busca por nome, re-ancoragem).
#include "components/UiCanvas.h"
#include <cstdio>
#include <cstring>

namespace vv {
namespace {

// tamanho default por tipo de elemento (px) — o editor pode redimensionar
void defaultSize(UiElement::Kind k, f32& w, f32& h) {
    switch (k) {
        case UiElement::Kind::Panel:   w = 320.0f; h = 180.0f; break;
        case UiElement::Kind::Label:   w = 240.0f; h = 44.0f;  break;
        case UiElement::Kind::Button:  w = 240.0f; h = 72.0f;  break;
        case UiElement::Kind::Image:   w = 256.0f; h = 256.0f; break;
        case UiElement::Kind::Menu:    w = 280.0f; h = 200.0f; break;
        case UiElement::Kind::Card:    w = 300.0f; h = 160.0f; break;
        case UiElement::Kind::Article: w = 360.0f; h = 200.0f; break;
        // 0.7.4 — containers (o tamanho do eixo de CONTEÚDO é auto-ajustado
        // pelo resolver; o transversal é manual)
        case UiElement::Kind::VBox:    w = 280.0f; h = 200.0f; break;
        case UiElement::Kind::HBox:    w = 360.0f; h = 96.0f;  break;
    }
}

// cor default por tipo (tema mono — tokens do UiContext espelhados aqui para
// manter este TU livre de ui/)
// 0.7.4 — Label: SÓ TEXTO por default (fundo TRANSPARENTE, alpha 0 — o fix
// do C33 "Label com fundo branco fixo"); o fundo é OPCIONAL via alpha no
// Inspector (cor com alpha, como os outros). Button mantém fundo escuro.
void defaultColor(UiElement::Kind k, f32 out[4]) {
    switch (k) {
        case UiElement::Kind::Panel:
        case UiElement::Kind::Card:
        case UiElement::Kind::Menu:
        case UiElement::Kind::Article:
        case UiElement::Kind::VBox:
        case UiElement::Kind::HBox:
            out[0] = 0.1176f; out[1] = 0.1176f; out[2] = 0.1176f; out[3] = 1.0f;  // PANEL
            break;
        case UiElement::Kind::Label:
            // fundo TRANSPARENTE (só texto); RGB = PANEL para o caso de o
            // utilizador LIGAR o fundo (alpha > 0) — nasce escuro mono
            out[0] = 0.1176f; out[1] = 0.1176f; out[2] = 0.1176f; out[3] = 0.0f;
            break;
        case UiElement::Kind::Button:
        case UiElement::Kind::Image:
            out[0] = 0.1804f; out[1] = 0.1804f; out[2] = 0.1804f; out[3] = 1.0f;  // LINE
            break;
    }
}

} // namespace

i32 UiCanvas::addElement(UiElement::Kind kind, f32 designW, f32 designH) {
    UiElement e;
    e.kind = kind;
    defaultSize(kind, e.w, e.h);
    defaultColor(kind, e.color);
    // nasce CENTRADO no espaço de design: âncora central (o offset conta
    // da ESQUERDA do elemento — ver elementRect) → ox = −w/2 põe o elemento
    // com o CENTRO em (designW/2, designH/2)
    e.anchorH = UiElement::AnchorH::Center;
    e.anchorV = UiElement::AnchorV::Middle;
    e.ox = -e.w * 0.5f;
    e.oy = -e.h * 0.5f;

    // nome único: base do tipo + sufixo numérico Godot-style
    const std::string base = uiElementKindName(kind);
    e.name = base;
    for (u32 suffix = 1; findElement(e.name) >= 0; ++suffix) {
        char buf[48];
        std::snprintf(buf, sizeof(buf), "%s.%03u", base.c_str(), suffix);
        e.name = buf;
    }

    if (kind == UiElement::Kind::Button) {
        e.text = "Botão";   // texto default editável (teclado in-app)
    } else if (kind == UiElement::Kind::Label || kind == UiElement::Kind::Card) {
        e.text = "Texto";
    } else if (kind == UiElement::Kind::Article) {
        e.text = "Artigo com texto multilinha.";
    } else if (kind == UiElement::Kind::Menu) {
        e.text = "Jogar\nSair";
    }

    elements.push_back(e);
    return static_cast<i32>(elements.size()) - 1;
}

i32 UiCanvas::findElement(const std::string& elemName) const {
    for (size_t i = 0; i < elements.size(); ++i) {
        if (elements[i].name == elemName) {
            return static_cast<i32>(i);
        }
    }
    return -1;
}

void UiCanvas::setAnchor(i32 idx, UiElement::AnchorH h, UiElement::AnchorV v,
                         f32 designW, f32 designH) {
    setAnchor(idx, h, v, designW, designH, 0.0f, 0.0f, 0.0f, 0.0f);
}

// 0.7.4 — com insets (paridade editor↔Play): a posição absoluta é calculada
// pela MESMA fórmula do elementRect (as âncoras esquerda/topo contam o
// inset; direita/fundo contam a borda ÚTIL) e os offsets são re-derivados
// pela âncora nova — o elemento fica exatamente no mesmo sítio.
void UiCanvas::setAnchor(i32 idx, UiElement::AnchorH h, UiElement::AnchorV v,
                         f32 designW, f32 designH,
                         f32 insL, f32 insT, f32 insR, f32 insB) {
    if (idx < 0 || static_cast<size_t>(idx) >= elements.size()) {
        return;
    }
    UiElement& e = elements[static_cast<size_t>(idx)];

    // posição absoluta ATUAL no espaço de design (fórmula do elementRect
    // com insets — ver ui/UiRuntime.h; aqui inline para o TU ficar livre de ui/)
    f32 x = 0.0f, y = 0.0f;
    switch (e.anchorH) {
        case UiElement::AnchorH::Left:   x = insL + e.ox; break;
        case UiElement::AnchorH::Center: x = designW * 0.5f + e.ox; break;
        case UiElement::AnchorH::Right:  x = designW - insR - e.w + e.ox; break;
    }
    switch (e.anchorV) {
        case UiElement::AnchorV::Top:    y = insT + e.oy; break;
        case UiElement::AnchorV::Middle: y = designH * 0.5f + e.oy; break;
        case UiElement::AnchorV::Bottom: y = designH - insB - e.h + e.oy; break;
    }

    // recalcula os offsets pela âncora NOVA (o elemento fica no mesmo sítio)
    e.anchorH = h;
    e.anchorV = v;
    switch (h) {
        case UiElement::AnchorH::Left:   e.ox = x - insL; break;
        case UiElement::AnchorH::Center: e.ox = x - designW * 0.5f; break;
        case UiElement::AnchorH::Right:  e.ox = x - (designW - insR - e.w); break;
    }
    switch (v) {
        case UiElement::AnchorV::Top:    e.oy = y - insT; break;
        case UiElement::AnchorV::Middle: e.oy = y - designH * 0.5f; break;
        case UiElement::AnchorV::Bottom: e.oy = y - (designH - insB - e.h); break;
    }
}

} // namespace vv
