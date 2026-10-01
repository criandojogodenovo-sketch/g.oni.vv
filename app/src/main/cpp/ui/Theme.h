#pragma once
// ui/Theme.h — THEME CENTRAL (0.7.6): fonte ÚNICA das cores lidas pela
// toolbar/ícones.
//
// PRINCÍPIO: o tema MONO dos painéis (Hierarchy/Inspector/overlays/teclado)
// fica INTACTO — os tokens vv::theme:: (UiContext.h) continuam a ser a
// paleta desses painéis. Este struct acrescenta a camada de IDENTIDADE do
// editor 0.7.6:
//
//   • brand  #8AB4F8 — cor de MARCA, usada SÓ em ícones da toolbar e estados
//     ATIVOS (segmented control selecionado, botão ativo). É uma EXCEÇÃO
//     DOCUMENTADA ao tema mono — como as cores de eixo dos gizmos 3D
//     (ui/Gizmo.h), a identidade tem uma cor própria e o resto continua
//     cinza/branco/preto.
//   • bg     #0B0E13 — fundo da BARRA superior (a faixa da toolbar é mais
//     escura que os painéis #1E1E1E — separa o chrome da área de trabalho).
//   • stroke = theme::LINE (o traço fino das separações de grupo).
//
// NENHUM emoji, nenhum gradiente, nenhuma sombra (regras da 0.7.6).
// GL-free: floats puros — host-testável no CI.
#include "core/Types.h"

namespace vv {
namespace theme {

// 0.7.6 — struct Theme: a fonte única da camada de identidade. Os campos
// mono (panel/text/line) re-exportam os tokens históricos para que TUDO o
// que a toolbar lê venha deste struct (um só sítio para mudar).
struct Theme {
    // ---- mono (painéis — INTACTO, espelha UiContext.h) ----
    f32 panel[4];    // #1E1E1E
    f32 text[4];     // #E6E6E6
    f32 line[4];     // #2E2E2E (stroke fino)
    // ---- identidade 0.7.6 ----
    f32 bg[4];       // #0B0E13 (fundo da barra superior)
    f32 brand[4];    // #8AB4F8 (cor de marca — ícones/ativos)
    f32 brandInk[4]; // #0B0E13 (ícone/texto SOBRE fundo de marca)
};

// 8AB4F8 = (138, 180, 248) / 255 · 0B0E13 = (11, 14, 19) / 255
inline constexpr Theme kTheme{
    {0.1176471f, 0.1176471f, 0.1176471f, 1.0f},   // panel  #1E1E1E
    {0.9019608f, 0.9019608f, 0.9019608f, 1.0f},   // text   #E6E6E6
    {0.1803922f, 0.1803922f, 0.1803922f, 1.0f},   // line   #2E2E2E
    {0.0431373f, 0.0549020f, 0.0745098f, 1.0f},   // bg     #0B0E13
    {0.5411765f, 0.7058824f, 0.9725490f, 1.0f},   // brand  #8AB4F8
    {0.0431373f, 0.0549020f, 0.0745098f, 1.0f},   // brandInk #0B0E13
};

} // namespace theme
} // namespace vv
