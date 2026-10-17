#pragma once
// components/Control.h — a BASE de todos os elementos de UI (0.12-B, B-1;
// inspirado no Control do Godot). Cada elemento de UI é um TIC FILHO do
// UIRoot com UM Control. CLÁUSULA anti-overengineering da B-1: «sem classes
// novas para além do Control base» — Label/Button/Panel/Image/Menu/Card/
// Article/VBox/HBox/Joystick NÃO são classes C++: são KINDS deste componente
// (o padrão UiElement::Kind da 0.7.0, agora em TICs).
//
// O QUE TODOS PARTILHAM (a base):
//   pos2D      → elem.ox / elem.oy   (design px; relativo ao PONTO DE ÂNCORA)
//   tamanho    → elem.w  / elem.h    (design px)
//   âncoras    → elem.anchorH × elem.anchorV (os 9 CANTOS: esq/centro/dir ×
//                topo/meio/fundo — o MESMO modelo 3×3 da 0.7.0)
//   esticar    → stretch (a âncora 10.ª: preenche o pai — hoje o ECRÃ de
//                design — respeitando as MARGENS l/t/r/b)
//   margens    → marginL/T/R/B (px design; usadas pelo esticar)
//   pivô       → pivotX/pivotY (0..1 dentro do rect; 0.5/0.5 = centro)
//   rotação    → elem.rot (graus, horário, à volta do pivô — o pivô default
//                0.5/0.5 reproduz o comportamento «à volta do centro» 0.8.6)
//   visível    → elem.visible
//   opacidade  → opacity (0..1; multiplica o alfa do elemento no draw —
//                1.0 = o comportamento de sempre)
//
// O PAYLOAD por tipo vive DENTRO do Control (elem: UiElement por valor — o
// mesmo struct da 0.7.0..0.8.6: texto/cor/ação/alvo/parâmetro/tipografia/
// containers). O UiElement NÃO é duplicado nem substituído: é O campo de
// dados do Control — o draw/hit-test/layout existentes (ui/UiRuntime)
// continuam a consumir o MESMO struct, agora alojado no TIC do elemento.
//
// JOYSTICK (Kind::Joystick): o Control marca e POSICIONA; o input/runtime
// vive no componente TouchControls no MESMO TIC (a fonte de input da física
// nunca muda — 0.7.3). O rect do Control num joystick é DERIVADO
// (ui::collectUiTree): ox/oy = topo-esquerda do círculo, w/h = 2×raio.
//
// LAYOUT (fonte única puro/afervel): controlRect() vive em ui/UiRuntime.h —
// AO LADO do elementRect da 0.7.0 (a MESMA camada de layout, zero divergência;
// incluir daqui criaria ciclo com o UiRuntime, que conhece o Control).
// Os 9 cantos delegam no elementRect (MESMA matemática); o esticar devolve o
// pai − margens. O pivot/opacity aplicam-se no DRAW (UiRuntime).
//
// SERIALIZAÇÃO (.goni): componente "Control" — os campos do elem partilham
// os NOMES do UiCanvas (kind/name/text/image/x/y/w/h/color/visible/…, o
// MESMO gerador de JSON extraído para appendUiElementJson) + os campos da
// base SÓ quando não-default: stretch/ml/mt/mr/mb/px/py/op. Builds antigas
// ignoram o tipo (política forward-compat do SceneSerializer).
#include "components/UiCanvas.h"
#include "core/Component.h"

namespace vv {

class Control : public Component {
public:
    // os tipos de elemento (os MESMOS nomes do UiElement::Kind no .goni —
    // a compat de leitura não distingue; +1 kind novo: Joystick)
    enum class Kind : u8 {
        Panel    = 0,
        Label    = 1,
        Button   = 2,
        Image    = 3,
        Menu     = 4,
        Card     = 5,
        Article  = 6,
        VBox     = 7,
        HBox     = 8,
        Joystick = 9,   // 0.12-B: o widget de input (runtime = TouchControls)
    };

    // ---- a base 2D (o que TODOS os elementos partilham) ---------------------
    bool stretch = false;    // âncora "esticar": preenche o pai − margens
    f32  marginL = 0.0f, marginT = 0.0f;   // margens do esticar (design px)
    f32  marginR = 0.0f, marginB = 0.0f;
    f32  pivotX = 0.5f, pivotY = 0.5f;     // pivô 0..1 (0.5/0.5 = centro)
    f32  opacity = 1.0f;                   // 0..1 — multiplica o alfa no draw

    // ---- o payload por tipo (o MESMO struct do UiCanvas 0.7.0..0.8.6) -------
    UiElement elem;

    // O KIND tem a FONTE AQUI (o Joystick=9 não cabe no UiElement::Kind).
    // Nos kinds 0..8 o elem.kind fica ESPELHADO (o draw/hit-test da 0.7.0
    // continuam a ler elem direto); no Joystick o elem.kind fica Panel — o
    // joystick NUNCA passa no drawElement (o draw dele é drawTouchControlsAt
    // via TouchControls do mesmo TIC; a vista da árvore salta-o).
    Kind  kind_ = Kind::Panel;
    bool isContainer() const {
        return kind_ == Kind::VBox || kind_ == Kind::HBox;
    }
    // joystick? (o Control espelha o TouchControls do mesmo TIC)
    bool isJoystick() const { return kind_ == Kind::Joystick; }

    Kind kind() const { return kind_; }
    void setKind(Kind k) {
        kind_ = k;
        if (k != Kind::Joystick) {
            elem.kind = static_cast<UiElement::Kind>(k);   // espelho p/ o draw
        }
    }

    // pos2D/tamanho/âncoras — os ALIASES da base (o editor/Inspector escrevem
    // por aqui; o runtime lê elem direto — zero duplicação de estado)
    f32 posX() const { return elem.ox; }
    f32 posY() const { return elem.oy; }
    f32 width() const { return elem.w; }
    f32 height() const { return elem.h; }
    void setPos(f32 x, f32 y) {
        elem.ox = x;
        elem.oy = y;
    }
    void setSize(f32 w, f32 h) {
        elem.w = w;
        elem.h = h;
    }
};

// nome canônico do kind (o MESMO vocabulário do uiElementKindName + joystick)
inline const char* controlKindName(Control::Kind k) {
    if (k == Control::Kind::Joystick) {
        return "joystick";
    }
    return uiElementKindName(static_cast<UiElement::Kind>(k));
}
inline Control::Kind controlKindFromName(const std::string& n) {
    if (n == "joystick") {
        return Control::Kind::Joystick;
    }
    // os nomes do UiElement (0.7.0..0.7.4) na ordem do enum
    if (n == "panel")   return Control::Kind::Panel;
    if (n == "label")   return Control::Kind::Label;
    if (n == "button")  return Control::Kind::Button;
    if (n == "image")   return Control::Kind::Image;
    if (n == "menu")    return Control::Kind::Menu;
    if (n == "card")    return Control::Kind::Card;
    if (n == "article") return Control::Kind::Article;
    if (n == "vbox")    return Control::Kind::VBox;
    if (n == "hbox")    return Control::Kind::HBox;
    return Control::Kind::Panel;
}

} // namespace vv
