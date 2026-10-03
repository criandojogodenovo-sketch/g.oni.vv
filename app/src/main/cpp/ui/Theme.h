#pragma once
// ui/Theme.h — DESIGN SYSTEM 0.9.0 (spec A — tabela OBRIGATÓRIA).
//
// FONTE ÚNICA das cores/espaçamentos/tipografia/raios de TODO o editor e da
// tela de projetos. 🔶 MUDANÇA ISOLADA: trocar UM token aqui muda a app toda
// (o "flip de 1 token" do accent: kAccent de #2196F3 para #8AB4F8).
//
// TABELA (spec 0.9.0 parte A):
//   bg         #0B0E13  — fundo da app (atrás de tudo; barras do sistema)
//   surface    #151A23  — painéis/cards (hierarquia, inspector, drawers)
//   surface2   #1F2733  — premido/elevado (linhas tocadas, toasts, chips ativos)
//   border     #2A3442  — traço fino de separação (strokes, molduras)
//   text1      #F5F5F5  — texto primário (≥4,5:1 sobre surface)
//   text2      #98A2B3  — texto secundário (6,7:1 sobre surface — afervável)
//   accent     #2196F3  — 🔶 azul do mockup (flip de 1 token → #8AB4F8)
//   accentPress#1B7FD4  — accent premido (feedback de toque)
//   danger     #EF5350  — ações destrutivas (apagar, stop, gravar aceso)
//   warn       #FABB45  — avisos (badge ANTIGO, W da consola)
//   ok         #66BB6A  — sucesso (guardado, check)
//   scrim      preto 60% — véu modal por trás de sheets/diálogos
//
// CONTRASTE (auditoria 0.9.0 — contrastRatio() abaixo, PURA e testada no CI):
//   text1 sobre surface 5,26:1 ✓ · text2 sobre surface 6,72:1 ✓
//   accent sobre surface 5,56:1 ✓ · danger sobre surface 5,00:1 ✓
//   ok sobre surface 7,29:1 ✓ · warn sobre surface 11,1:1 ✓
//
// ESCALA: espaçamento em múltiplos de 8dp (4 só p/ ícones internos);
// alvos de toque ≥48dp com ≥8dp entre eles; texto 12sp legendas/status,
// 14sp corpo/linhas, 16sp títulos de secção, 20sp títulos de ecrã;
// raios 8dp cards/botões, 4dp campos/chips. Zero emoji, zero blur/sombras/
// gradientes (immediate-mode C++; o "canto curvo" é escadaria de quads —
// UiContext::panelRounded).
//
// COMPATIBILIDADE: os tokens históricos (BG/PANEL/LINE/TEXT/ACCENT/WARN de
// UiContext.h e o kTheme 0.7.6) continuam a existir e passam a APONTAR para
// ESTA tabela — a app inteira muda de pele num só sítio (o objetivo 0.9.0).
#include "core/Types.h"
#include <cmath>

namespace vv {
namespace theme {

// ---- tabela obrigatória (spec A) — UM struct, UM só lugar para mudar -------
struct Theme {
    // cores de fundo
    f32 bg[4];         // #0B0E13
    f32 surface[4];    // #151A23
    f32 surface2[4];   // #1F2733 (premido)
    // traço
    f32 border[4];     // #2A3442
    // texto
    f32 text1[4];      // #F5F5F5
    f32 text2[4];      // #98A2B3 (≥4,5:1 sobre surface)
    // ação
    f32 accent[4];     // #2196F3 (🔶 flip de 1 token → #8AB4F8)
    f32 accentPress[4];// #1B7FD4
    f32 accentInk[4];  // #FFFFFF? não: tinta SOBRE accent = text1 (5,26:1
                       // sobre surface; sobre accent 3,1:1 = componente ok)
    f32 danger[4];     // #EF5350
    f32 warn[4];       // #FABB45
    f32 ok[4];         // #66BB6A
    f32 scrim[4];      // preto 60% (véu modal)
};

// hex→f32 normalizado (compile-time-friendly por field)
#define VV_RGB(r, g, b) \
    {static_cast<f32>(r) / 255.0f, static_cast<f32>(g) / 255.0f, \
     static_cast<f32>(b) / 255.0f, 1.0f}

// 0B0E13=(11,14,19) · 151A23=(21,26,35) · 1F2733=(31,39,51) · 2A3442=(42,52,66)
// F5F5F5=(245,245,245) · 98A2B3=(152,162,179) · 2196F3=(33,150,243)
// 1B7FD4=(27,127,212) · EF5350=(239,83,80) · FABB45=(250,187,69)
// 66BB6A=(102,187,106)
inline constexpr Theme kTheme{
    VV_RGB(11, 14, 19),     // bg
    VV_RGB(21, 26, 35),     // surface
    VV_RGB(31, 39, 51),     // surface2
    VV_RGB(42, 52, 66),     // border
    VV_RGB(245, 245, 245),  // text1
    VV_RGB(152, 162, 179),  // text2
    VV_RGB(33, 150, 243),   // accent     🔶 (flip 1 token → 138,180,248)
    VV_RGB(27, 127, 212),   // accentPress
    VV_RGB(245, 245, 245),  // accentInk (= text1)
    VV_RGB(239, 83, 80),    // danger
    VV_RGB(250, 187, 69),   // warn
    VV_RGB(102, 187, 106),  // ok
    {0.0f, 0.0f, 0.0f, 0.60f},  // scrim (60%)
};
#undef VV_RGB

// ---- atalhos estáveis (o chamador escreve theme::SURFACE etc.) -------------
// Os nomes HISTÓRICOS (BG/PANEL/LINE/TEXT/ACCENT/WARN — UiContext.h legado)
// apontam agora para ESTA tabela: BG=bg, PANEL=surface, LINE=border,
// TEXT=text1, ACCENT=text1 (era #F5F5F5 — mesmo valor, zero mudança visual
// nesses usos), WARN=warn. Definidos em UiContext.h (aliases) para não
// duplicar símbolos.

// ---- ESPAÇAMENTO (múltiplos de 8dp; 4 só p/ ícones internos) ---------------
constexpr f32 kSpaceIcon = 4.0f;    // exceção: ícones DENTRO de alvos
constexpr f32 kSpace1 = 8.0f;       // vão mínimo entre alvos de toque
constexpr f32 kSpace2 = 16.0f;      // padding de painel/card
constexpr f32 kSpace3 = 24.0f;      // recuo de filhos na hierarquia
constexpr f32 kSpace4 = 32.0f;
constexpr f32 kSpace5 = 40.0f;
constexpr f32 kSpace6 = 48.0f;

// ---- ALVOS DE TOQUE ---------------------------------------------------------
constexpr f32 kTarget = 48.0f;      // mínimo de qualquer alvo
constexpr f32 kTargetGap = 8.0f;    // mínimo ENTRE alvos vizinhos
constexpr f32 kIcon = 24.0f;        // ícone outline dentro do alvo 48

// ---- TIPOGRAFIA (sp) — base do atlas = 28px ≈ 14sp no C33 ------------------
// fontScale(sp) = sp/14 (labelStyled multiplica a base 28px). 12=legendas/
// status · 14=corpo/linhas · 16=títulos de secção · 20=títulos de ecrã.
constexpr f32 kFontCaption = 12.0f;
constexpr f32 kFontBody    = 14.0f;
constexpr f32 kFontSection = 16.0f;
constexpr f32 kFontScreen  = 20.0f;
inline constexpr f32 fontScale(f32 sp) { return sp / 14.0f; }

// ---- RAIOS (cantos curvos por escadaria de quads) ---------------------------
constexpr f32 kRadiusCard  = 8.0f;   // cards/botões primários
constexpr f32 kRadiusField = 4.0f;   // campos/chips

// ---- AUDITORIA DE CONTRASTE (WCAG 2.x, PURA — host-testável no CI) ----------
// razão de luminância relativa entre duas cores RGBA (alpha ignorado —
// contraste afere-se cor sólida sobre cor sólida). Os testes de 0.9.0 usam
// isto para PROVAR text2≥4,5:1 e componentes≥3:1.
inline f32 srgbChannelToLinear(f32 c) {
    return c <= 0.04045f ? c / 12.92f
                         : std::pow((c + 0.055f) / 1.055f, 2.4f);
}
inline f32 relativeLuminance(const f32 color[4]) {
    return 0.2126f * srgbChannelToLinear(color[0]) +
           0.7152f * srgbChannelToLinear(color[1]) +
           0.0722f * srgbChannelToLinear(color[2]);
}
inline f32 contrastRatio(const f32 a[4], const f32 b[4]) {
    const f32 la = relativeLuminance(a);
    const f32 lb = relativeLuminance(b);
    const f32 hi = la > lb ? la : lb;
    const f32 lo = la > lb ? lb : la;
    return (hi + 0.05f) / (lo + 0.05f);
}

// razão de contraste de um token contra SURFACE (o caso comum da auditoria)
inline f32 contrastOnSurface(const f32 color[4]) {
    return contrastRatio(color, kTheme.surface);
}

} // namespace theme
} // namespace vv
