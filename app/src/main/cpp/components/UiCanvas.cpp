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
    }
}

// cor default por tipo (tema mono — tokens do UiContext espelhados aqui para
// manter este TU livre de ui/)
void defaultColor(UiElement::Kind k, f32 out[4]) {
    switch (k) {
        case UiElement::Kind::Panel:
        case UiElement::Kind::Card:
        case UiElement::Kind::Menu:
        case UiElement::Kind::Article:
            out[0] = 0.1176f; out[1] = 0.1176f; out[2] = 0.1176f; out[3] = 1.0f;  // PANEL
            break;
        case UiElement::Kind::Label:
            out[0] = 0.9020f; out[1] = 0.9020f; out[2] = 0.9020f; out[3] = 1.0f;  // TEXT
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
        e.text = "Botao";   // texto default editável (teclado in-app)
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
    if (idx < 0 || static_cast<size_t>(idx) >= elements.size()) {
        return;
    }
    UiElement& e = elements[static_cast<size_t>(idx)];

    // posição absoluta ATUAL no espaço de design (ver uiElementRect — aqui
    // sem insets: o editor desenha em design space puro)
    f32 x = 0.0f, y = 0.0f;
    switch (e.anchorH) {
        case UiElement::AnchorH::Left:   x = e.ox; break;
        case UiElement::AnchorH::Center: x = designW * 0.5f + e.ox; break;
        case UiElement::AnchorH::Right:  x = designW - e.w + e.ox; break;
    }
    switch (e.anchorV) {
        case UiElement::AnchorV::Top:    y = e.oy; break;
        case UiElement::AnchorV::Middle: y = designH * 0.5f + e.oy; break;
        case UiElement::AnchorV::Bottom: y = designH - e.h + e.oy; break;
    }

    // recalcula os offsets pela âncora NOVA (o elemento fica no mesmo sítio)
    e.anchorH = h;
    e.anchorV = v;
    switch (h) {
        case UiElement::AnchorH::Left:   e.ox = x; break;
        case UiElement::AnchorH::Center: e.ox = x - designW * 0.5f; break;
        case UiElement::AnchorH::Right:  e.ox = x - (designW - e.w); break;
    }
    switch (v) {
        case UiElement::AnchorV::Top:    e.oy = y; break;
        case UiElement::AnchorV::Middle: e.oy = y - designH * 0.5f; break;
        case UiElement::AnchorV::Bottom: e.oy = y - (designH - e.h); break;
    }
}

} // namespace vv
