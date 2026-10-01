#pragma once
// components/UiCanvas.h — UI criável da engine (0.7.0, F6).
//
// Um UiCanvas é um componente de TIC que guarda ELEMENTOS de UI 2D desenhados
// POR CIMA da cena em modo Play (e editáveis WYSIWYG no viewport 2D do modo
// UI do editor). Elementos base (0.7.0): Panel, Label, Button, Image;
// compostos (0.7.3): Menu, Card, Article + Joystick (TouchControls editável).
//
// COORDENADAS: cada elemento vive no espaço de ECRÃ da superfície (px,
// origem topo-esquerda) + ANCORAGEM 3×3 (horizontal: esquerda/centro/direita;
// vertical: topo/meio/fundo). O rect guardado é o offset (ox, oy) relativo ao
// PONTO DE ÂNCORA + tamanho (w, h). O mapeamento p/ o ecrã real vive em
// ui/UiRuntime.h (elementRect — GL-free, afervel em 2 resoluções no CI):
//   âncora esquerda  → x = safe.left + ox          (fixa à esquerda)
//   âncora centro    → x = sw*0.5 + ox             (segue o centro)
//   âncora direita   → x = sw − safe.right − w + ox (fixa à direita)
// (idem vertical). Em Play a UI fica por cima da cena (pass UI sem depth).
//
// AÇÕES DECLARATIVAS (on-click de Button/Menu): mostrar/esconder/alternar
// panel (por NOME de elemento), carregar cena (0.7.1) e spawn de preset.
// A ação é DADOS (tipo + alvo + param) — a aplicação vive em ui/UiRuntime
// (applyUiAction) com dependências injetadas, afervel no CI.
//
// SERIALIZAÇÃO (.goni): componente "UiCanvas" com os elementos por valor
// (kind/name/text/rect/cor/visível/âncoras/ação). Builds antigas ignoram-no
// (política forward-compat do SceneSerializer).
#include "core/Component.h"
#include "core/Types.h"
#include <string>
#include <vector>

namespace vv {

struct UiElement {
    enum class Kind : u8 {
        Panel  = 0,   // retângulo preenchido (fundo de HUD)
        Label  = 1,   // texto (uma linha, fitted) — FUNDO OPCIONAL (alpha)
        Button = 2,   // retângulo + texto + on-click declarativo
        Image  = 3,   // retângulo com textura do projeto (ref relativa)
        // 0.7.3 — compostos:
        Menu    = 4,  // lista vertical de botões (texto = linhas "label>alvo")
        Card    = 5,  // panel + borda + label (título no topo)
        Article = 6,  // texto multilinha com wrap pela largura
        // 0.7.4 — CONTAINERS de layout (filhos automáticos, aninháveis):
        VBox = 7,     // coluna: filhos empilhados verticalmente
        HBox = 8,     // linha: filhos lado a lado horizontalmente
    };

    // âncoras (0.7.0) — como o elemento se comporta quando a resolução muda
    enum class AnchorH : u8 { Left = 0, Center = 1, Right = 2 };
    enum class AnchorV : u8 { Top = 0, Middle = 1, Bottom = 2 };

    // ação declarativa do on-click (Button; Menu = por linha)
    enum class Action : u8 {
        None = 0,
        ShowPanel = 1,      // alvo = nome do elemento a mostrar
        HidePanel = 2,      // alvo = nome do elemento a esconder
        TogglePanel = 3,    // alvo = nome do elemento a alternar
        LoadScene = 4,      // alvo = nome da cena; troca INSTANTÂNEA
        Spawn = 5,          // alvo = preset (PlayerBody3D/…)
        TransitionScene = 6, // 0.7.1: alvo = cena; param = fade|slide
    };

    // estilo da transição (0.7.1): "fade" (default) ou "slide"
    enum class Transition : u8 { Fade = 0, Slide = 1 };

    // alinhamento no eixo transversal (0.7.4 — Menu: alinhamento do TEXTO
    // nas linhas; VBox/HBox: alinhamento dos FILHOS no eixo transversal)
    enum class Align : u8 { Start = 0, Center = 1, End = 2 };

    Kind kind = Kind::Panel;
    AnchorH anchorH = AnchorH::Left;
    AnchorV anchorV = AnchorV::Top;
    Action action = Action::None;
    Align  align = Align::Start;   // 0.7.4: Menu/VBox/HBox

    std::string name;    // id do elemento (alvo de Show/Hide/Toggle)
    std::string text;    // Label/Button/Card/Article/Menu ("linhas")
    std::string image;   // Image: ref relativa ("textures/x.png");
                         // 0.7.4: Button/Panel = TEXTURA DE FUNDO (mesmo campo)
    std::string target;  // alvo da ação: nome do elemento/preset/cena (Menu:
                         // o alvo por LINHA sobrepõe-se — "label>alvo")
    std::string param;   // 0.7.1: parâmetro da ação (TransitionScene:
                         // "fade" default | "slide")
    std::string parent;  // 0.7.4: nome do CONTAINER (VBox/HBox) que dispõe
                         // este elemento (vazio = topo, âncoras próprias)

    f32 ox = 0.0f, oy = 0.0f;   // offset do ponto de âncora (px)
    f32 w = 200.0f, h = 80.0f;  // tamanho (px)
    f32 spacing = 0.0f;   // 0.7.4: Menu = vão entre itens; VBox/HBox = vão
                          // entre filhos (px)
    f32 pad = 8.0f;       // 0.7.4: VBox/HBox = resguardo interno (px)
    f32 color[4] = {0.1176f, 0.1176f, 0.1176f, 1.0f};   // RGBA (default = PANEL do tema mono)

    bool visible = true;
};

// nome canônico do tipo de elemento (serializer + labels do editor)
inline const char* uiElementKindName(UiElement::Kind k) {
    switch (k) {
        case UiElement::Kind::Panel:   return "panel";
        case UiElement::Kind::Label:   return "label";
        case UiElement::Kind::Button:  return "button";
        case UiElement::Kind::Image:   return "image";
        case UiElement::Kind::Menu:    return "menu";
        case UiElement::Kind::Card:    return "card";
        case UiElement::Kind::Article: return "article";
        case UiElement::Kind::VBox:    return "vbox";
        case UiElement::Kind::HBox:    return "hbox";
    }
    return "?";
}

// rótulo curto do tipo de ação (serializer + editor)
inline const char* uiActionName(UiElement::Action a) {
    switch (a) {
        case UiElement::Action::None:            return "none";
        case UiElement::Action::ShowPanel:       return "show";
        case UiElement::Action::HidePanel:       return "hide";
        case UiElement::Action::TogglePanel:     return "toggle";
        case UiElement::Action::LoadScene:       return "scene";
        case UiElement::Action::Spawn:           return "spawn";
        case UiElement::Action::TransitionScene: return "trans";
    }
    return "?";
}

// estilo de transição do elemento (param; default fade)
inline UiElement::Transition uiElementTransition(const UiElement& e) {
    return e.param == "slide" ? UiElement::Transition::Slide
                              : UiElement::Transition::Fade;
}

// container? (VBox/HBox — dispõe filhos automaticamente)
inline bool uiElementIsContainer(UiElement::Kind k) {
    return k == UiElement::Kind::VBox || k == UiElement::Kind::HBox;
}

// nome do alinhamento transversal (serializer + Inspector)
inline const char* uiAlignName(UiElement::Align a) {
    switch (a) {
        case UiElement::Align::Start:  return "start";
        case UiElement::Align::Center: return "center";
        case UiElement::Align::End:    return "end";
    }
    return "?";
}

class UiCanvas : public Component {
public:
    std::vector<UiElement> elements;

    // cria um elemento default do tipo (centrado, tamanho por tipo) e devolve
    // o índice; nome único Godot-style ("panel", "panel.001", …)
    i32 addElement(UiElement::Kind kind, f32 designW, f32 designH);

    // índice do PRIMEIRO elemento com o nome (−1 se não existe) — alvo das
    // ações Show/Hide/Toggle e hit-test do editor
    i32 findElement(const std::string& elemName) const;

    // normaliza o rect do elemento `i` para as âncoras dadas SEM o mover na
    // tela (recalcula ox/oy pelo novo ponto de âncora) — usado pelo editor
    // quando o utilizador muda a âncora de um elemento já posicionado.
    // 0.7.4: overload com INSETS (o editor desenha em paridade com o Play —
    // o elementRect conta as bordas da safe-area nas âncoras esq/top/dir/fundo)
    void setAnchor(i32 idx, UiElement::AnchorH h, UiElement::AnchorV v,
                   f32 designW, f32 designH);
    void setAnchor(i32 idx, UiElement::AnchorH h, UiElement::AnchorV v,
                   f32 designW, f32 designH,
                   f32 insL, f32 insT, f32 insR, f32 insB);
};

} // namespace vv
