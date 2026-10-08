#pragma once
// ui/Theme.h — DESIGN SYSTEM 0.9.6.10 (spec G · GRUPO UI: GRAFITE+ÂMBAR+VIDRO).
//
// FONTE ÚNICA das cores/espaçamentos/tipografia/raios de TODO o editor e da
// tela de projetos. 🔶 MUDANÇA ISOLADA: trocar UM token aqui muda a app toda.
//
// 0.9.6.10 (GRUPO UI · A REESCRITA DA APRESENTAÇÃO, por ordem expressa do
// dono): a PALETA A OFICIAL — grafite + âmbar + vidro (a decisão do dono
// depois de ver o mono no device: os fills brancos do accent mono eram
// BLOCOS BRANCOS CEGANTES — o anti-exemplo da imagem 2). O AZUL morreu do
// chrome TODO (a promessa «zero azul» da spec G; o azul do eixo Z do gizmo
// e da sintaxe V.ONI são CONTEÚDO — exceção documentada de sempre).
//
// TABELA (spec G — grafite+âmbar+vidro):
//   bg         #0E0E10 α1.00 — fundo da app (grafite; == o clear)
//   surface    #161618 α0.80 — VIDRO 80% (a spec do dono): painéis/cards
//   surface2   #202023 α0.86 — vidro denso (premido, toasts, chips ativos)
//   border     #2E2E32 α0.55 — hairline de grafite
//   glassEdge  #FFFFFF α0.12 — o BORDO do vidro (0x1F — luz na aresta)
//   glassTop   #FFFFFF α0.04 — highlight do TOPO do vidro (0x0A)
//   text1      #ECECEE α1.00 — texto primário (15,3:1 sobre surface)
//   text2      #A6A6AD α1.00 — texto secundário (7,5:1 sobre surface)
//   accent     #FFB020 α1.00 — O ÂMBAR (a identidade; era o branco mono)
//   accentPress#E09A00 α1.00 — âmbar premido (feedback de toque)
//   accentInk  #0E0E10 α1.00 — tinta SOBRE âmbar (10,5:1 — escura)
//   accentDim  #4A3714 α1.00 — o âmbar a 25% sobre grafite: o FILL da
//                              SELEÇÃO (linha da hierarquia, chips) + a
//                              BARRA ESQUERDA é o accent puro
//   danger     #E5484D α1.00 — semântica (exceção documentada, como sempre)
//   warn       #FF8A3D α1.00 — LARANJA (nunca == accent: hue 24° vs 39°)
//   ok         #46A758 α1.00
//   scrim      preto 60% — véu MODAL (os cards centrados que têm véu)
//   scrimMenu  preto 40% — véu dos MENUS ANCORADOS (PASSO 4 · 0.9.6.20:
//              «menus ancorados com fundo 40%» — o véu mais leve mantém a
//              cena legível à volta do menu; os modais mantêm os 60%)
//
// CONTRASTE — OS PISOS REAIS (o padrão honesto): sobre surface SÓLIDA e
// sobre o VIDRO REAL (α0.80 sobre o bg grafite — o que está de facto por
// trás dos painéis laterais): text1 15,3/15,5:1 · text2 7,5/7,6:1 ·
// accent 9,9/10,0:1 · danger 4,6:1 · warn 7,7:1 · ok 6,0:1 — TODO o texto
// ≥4,5:1 e TODO o componente ≥3:1.
// O CASO PATOLÓGICO (cena BRANCA PURA por trás do vidro 80%): text1 8,2:1
// ✓ · accent 5,3:1 ✓ · text2 4,0:1 · danger 2,5:1 — A REGRA DA CASA que
// o torna honesto: TEXTO SECUNDÁRIO E SEMÂNTICA NÃO FLUTUAM sobre a cena
// viva — sobre vidro flutuante usa-se text1; severidade vive em painéis
// dokados (surface sólida). blendOver/contrastOnGlass medem TUDO no CI.
//
// ESCALA: espaçamento em múltiplos de 8dp (4 só p/ ícones internos);
// PASSO 1: alvos desenho 32 / toque 40 com ≥8dp entre eles; nada ≥48;
// texto 12sp legendas/status,
// 14sp corpo/linhas, 16sp títulos de secção, 20sp títulos de ecrã;
// raios 8dp cards/botões, 4dp campos/chips. Zero emoji, zero blur/sombras/
// gradientes (immediate-mode C++; o "canto curvo" é escadaria de quads —
// UiContext::panelRounded). O vidro NÃO tem backdrop blur (spec G).
//
// COMPATIBILIDADE: os tokens históricos (BG/PANEL/LINE/TEXT/ACCENT/WARN de
// UiContext.h) continuam a existir e APONTAM para ESTA tabela — a app
// inteira muda de pele num só sítio (desde a 0.9.0; agora grafite+âmbar).
#include "core/Types.h"
#include <cmath>

namespace vv {
namespace theme {

// ---- tabela grafite+âmbar+vidro (spec G) — UM struct, UM só lugar ----
struct Theme {
    // cores de fundo (o ALPHA é o VIDRO: 1.0 = opaco, <1.0 = vê-se o que
    // está por trás — o pass UI desenha TODO com blend desde a F1)
    f32 bg[4];         // #0E0E10 α1.00 (grafite — igual ao clear da app)
    f32 surface[4];    // #161618 α0.80 (VIDRO 80% — a spec do dono)
    f32 surface2[4];   // #202023 α0.86 (vidro denso — premido)
    // traço
    f32 border[4];     // #2E2E32 α0.55 (hairline de grafite)
    f32 glassEdge[4];  // #FFFFFF α0.12 (0x1F — a aresta de luz do vidro)
    f32 glassTop[4];   // #FFFFFF α0.04 (0x0A — o highlight do topo)
    // texto
    f32 text1[4];      // #ECECEE α1.00
    f32 text2[4];      // #A6A6AD α1.00 (7,5:1 sobre surface)
    // ação (O ÂMBAR — a identidade spec G; o mono morreu com os fills
    // brancos cegantes do anti-exemplo)
    f32 accent[4];     // #FFB020 α1.00 (âmbar; 9,9:1 sobre surface)
    f32 accentPress[4];// #E09A00 α1.00 (premido — mais escuro, feedback)
    f32 accentInk[4];  // #0E0E10 α1.00 (tinta SOBRE âmbar — escura, 10,5:1)
    f32 accentDim[4];  // #4A3714 α1.00 (o fill da SELEÇÃO: âmbar 25% sobre
                       //  grafite; text1 9,6:1 e accent 6,2:1 em cima dele)
    f32 danger[4];     // #E5484D α1.00 (semântica — exceção documentada)
    f32 warn[4];       // #FF8A3D α1.00 (LARANJA — hue 24°, nunca o accent 39°)
    f32 ok[4];         // #46A758 α1.00
    f32 scrim[4];      // preto 60% (véu modal)
    f32 scrimMenu[4];  // preto 40% (véu dos menus ANCORADOS — PASSO 4)
    // 0.9.2 — PALETA V.ONI (spec §10 🔶 — coloração do editor de script;
    // o parser classifica tokens, AS CORES VIVEM AQUI — flip de 1 token).
    // CONTEÚDO, não chrome (exceção documentada ao «zero azul» — como os
    // EIXOS do gizmo, sempre foram)
    f32 voniReserved[4];   // #B39DDB reservadas (roxo)
    f32 voniEngine[4];     // #8AB4F8 engine/maiúsculas (azul — conteúdo)
    f32 voniUser[4];       // #ECECEE utilizador (== text1)
    f32 voniString[4];     // #81C784 strings (verde)
    f32 voniNumber[4];     // #FFD54F números (amarelo)
    f32 voniComment[4];    // #8A8A8A comentários (5,6:1 no grafite)
    // EIXOS DO GIZMO (CONTEÚDO 3D, não chrome — a exceção documentada
    // desde 0.6.9; vivem AQUI porque a spec G manda: cores SÓ no Theme)
    f32 axisX[4];      // #E9493B vermelho (X)
    f32 axisY[4];      // #5ACB5F verde (Y)
    f32 axisZ[4];      // #4F92F5 azul (Z — conteúdo, NÃO chrome)
    f32 axisDim[4];    // #9E9E9E cinza (central)
};

// hex→f32 normalizado (compile-time-friendly por field) — com ALPHA:
// VV_RGBA(r,g,b,a) com a em FLOAT 0..1 (o vidro: 0.88f/0.92f/0.55f);
// VV_RGB = opaco (a=1)
#define VV_RGBA(r, g, b, a)                                              \
    {static_cast<f32>(r) / 255.0f, static_cast<f32>(g) / 255.0f,         \
     static_cast<f32>(b) / 255.0f, static_cast<f32>(a)}
#define VV_RGB(r, g, b) VV_RGBA(r, g, b, 1.0f)

// 0E0E10=(14,14,16) · 161618=(22,22,24) · 202023=(32,32,35) · 2E2E32=(46,46,50)
// ECECEE=(236,236,238) · A6A6AD=(166,166,173) · FFB020=(255,176,32)
// E09A00=(224,154,0) · 4A3714=(74,55,20) · E5484D=(229,72,77)
// FF8A3D=(255,138,61) · 46A758=(70,167,88) · E9493B=(233,73,59)
// 5ACB5F=(90,203,95) · 4F92F5=(79,146,245) · 9E9E9E=(158,158,158)
inline constexpr Theme kTheme{
    VV_RGB(14, 14, 16),          // bg         (o grafite)
    VV_RGBA(22, 22, 24, 0.80f),  // surface    α0.80 — O VIDRO (spec G)
    VV_RGBA(32, 32, 35, 0.86f),  // surface2   α0.86 — vidro denso
    VV_RGBA(46, 46, 50, 0.55f),  // border     α0.55 — hairline
    VV_RGBA(255, 255, 255, 31.0f / 255.0f),   // glassEdge (#FFFFFF1F)
    VV_RGBA(255, 255, 255, 10.0f / 255.0f),   // glassTop  (#FFFFFF0A)
    VV_RGB(236, 236, 238),       // text1
    VV_RGB(166, 166, 173),       // text2
    VV_RGB(255, 176, 32),        // accent     (O ÂMBAR)
    VV_RGB(224, 154, 0),         // accentPress
    VV_RGB(14, 14, 16),          // accentInk  (tinta escura no âmbar)
    VV_RGB(74, 55, 20),          // accentDim  (o fill da seleção)
    VV_RGB(229, 72, 77),         // danger
    VV_RGB(255, 138, 61),        // warn       (LARANJA ≠ âmbar)
    VV_RGB(70, 167, 88),         // ok
    {0.0f, 0.0f, 0.0f, 0.60f},   // scrim (60%)
    {0.0f, 0.0f, 0.0f, 0.40f},   // scrimMenu (40% — PASSO 4, menus ancorados)
    // V.ONI (sintaxe = conteúdo): voniUser passa a text1 (#ECECEE); no
    // grafite #0E0E10 TODOS ≥5,6:1 (user 16,4 · engine 9,2 · reserved 8,1
    // · string 9,6 · number 13,7 · comment 5,6)
    VV_RGB(179, 157, 219),  // voniReserved
    VV_RGB(138, 180, 248),  // voniEngine
    VV_RGB(236, 236, 238),  // voniUser
    VV_RGB(129, 199, 132),  // voniString
    VV_RGB(255, 213, 79),   // voniNumber
    VV_RGB(138, 138, 138),  // voniComment
    // EIXOS (conteúdo 3D — os MESMOS valores de sempre do gizmo)
    VV_RGB(233, 73, 59),    // axisX
    VV_RGB(90, 203, 95),    // axisY
    VV_RGB(79, 146, 245),   // axisZ
    VV_RGB(158, 158, 158),  // axisDim
};
#undef VV_RGB
#undef VV_RGBA

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

// ---- ALVOS DE TOQUE (PASSO 1 · 0.9.6.14 — a LEI DE OURO da spec) -----------
// DESENHO 32dp / TOQUE 40dp — nada ≥48 no editor (era o alvo único 48dp);
// os elementos DE LINHA tomam a altura da LINHA da spec (top bar 36 ·
// campo/tabs de baixo 32 · cabeçalho 28 — LayoutDump.h tem os pisos).
constexpr f32 kTarget = 40.0f;      // alvo de toque do botão SOLTO (desenho 32)
constexpr f32 kTargetGap = 8.0f;    // mínimo ENTRE alvos vizinhos
constexpr f32 kIcon = 20.0f;        // ícone outline dentro do alvo (era 24)

// ---- TIPOGRAFIA (sp) — base do atlas = 28px ≈ 14sp no C33 ------------------
// fontScale(sp) = sp/14 (labelStyled multiplica a base 28px). 12=legendas/
// status · 14=corpo/linhas · 16=títulos de secção · 20=títulos de ecrã.
constexpr f32 kFontCaption = 12.0f;
constexpr f32 kFontBody    = 14.0f;
constexpr f32 kFontSection = 16.0f;
constexpr f32 kFontScreen  = 20.0f;
inline constexpr f32 fontScale(f32 sp) { return sp / 14.0f; }

// 0.9.6 (G1-2) — BASelines do CABEÇALHO PADRÃO (a parte útil de 56dp):
// fallback para quando AINDA não há métricas de fonte (o par título(20sp)+
// subtítulo(12sp) CENTRADO e SEM CORTE). 0.9.6.1: as 4 telas (Docs/
// Settings/Script/Texto) derivam as baselines DAS MÉTRICAS REAIS da fonte
// (headerBaselines abaixo — o bloco inteiro centrado na parte útil, em
// qualquer densidade); estas constantes ficam como fallback px.
constexpr f32 kHeaderTitleBase = 29.0f;   // baseline do título no cabeçalho
constexpr f32 kHeaderSubBase   = 51.0f;   // baseline do subtítulo

// ---- DENSIDADE (0.9.6.1 · PASSO 0 — R-018: dp usado como px) ---------------
// A CAUSA RAIZ que o dono mediu no device: as constantes do design system
// (barra 56dp, alvos 48dp, teclas 48dp) eram constexpr EM DP mas consumidas
// COMO PX CRUS — no C33 (densidade 2.0) o cabeçalho media 56px (~29dp
// reais), os botões 48px e as teclas 48×65px: metade do pedido. A CORREÇÃO
// NA ORIGEM: UMA função dp() multiplica pela densidade do device e TODAS as
// fontes únicas de layout (SafeArea/EditorLayout + os componentes) passam
// por ela ao desenhar. A densidade chega do AConfiguration_getDensity
// (== DisplayMetrics.density×160) no arranque (platform/main.cpp) e é
// LOGADA em px e dp (o log de identidade do ecrã). Em testes/harness a
// densidade é 1.0 → dp(v)==v → layout de SEMPRE (o c33_virtual prova o
// mecanismo com 2.0 injetados — FASE 12.10 + a sentinela R-018).
inline f32 g_density = 1.0f;                       // DisplayMetrics.density
inline void setDensity(f32 d) { g_density = d > 0.05f ? d : 1.0f; }
inline f32  dp(f32 v) { return v * g_density; }    // A função dp→px da casa

// 0.9.6.6 (GRUPO C) · A ESCALA ÚNICA DO TEXTO — sp(). O CONTRATO da
// invariância: a densidade multiplica TUDO — dp para o layout E sp para o
// texto; o ecrã a densidade 2.0 é o ecrã a 1.0 visto a 2× (a FASE 13.6
// afere entrada a entrada do registo). ANTES o texto era o ATLAS CRU
// (28px) em qualquer densidade: no C33 (2.0) ficava certo POR ACASO
// (28px == 14sp @2.0), mas no harness (1.0) saía 2× desproporcional — o
// bloco de 29px numa banda de 24dp SANGRAVA o fundo (o ERRO medido do
// Grupo B) e os títulos 20sp (bloco de 41px) não cabiam no cabeçalho de
// 56dp (o fallback de baselines fixas empurrava-os PARA FORA do
// contentRect — o título do Docs/Script, a errata do relatório B).
inline f32 sp(f32 v) { return v * g_density; }   // px de um texto de v sp
// o fator do ATLAS da casa: a base assada (kAtlasPx) é 14sp @ densidade
// 2.0 — desenhar texto CORPO (14sp) é escalar o atlas por densidade/2.
// O UiContext aplica ESTE fator no CHOKE POINT (fontWidth/fontHeight/
// textMetrics/labelStyled/labelFitted) — a escala do texto tem UM só
// dono; os chamadores continuam a passar fontScale(sp) RELATIVO (sp/14).
constexpr f32 kAtlasPx = 28.0f;                  // o bake da casa
inline f32 textK() { return g_density / 2.0f; }  // atlas→px do texto corpo

// baselines do CABEÇALHO PADRÃO a partir das MÉTRICAS REAIS da fonte: o
// bloco título(20sp)+subtítulo(12sp) CENTRADO na parte útil (nada cortado
// no topo, o subtítulo por baixo sem tocar o limite) — a causa do corte
// eram baselines FIXAS px (29/51) com o cabeçalho a meia altura. As
// métricas chegam em px de base (atlas 28px) e escalam por sp aqui.
struct HeaderBaselines {
    f32 title;    // baseline do título (20sp)
    f32 sub;      // baseline do subtítulo (12sp)
};
inline HeaderBaselines headerBaselines(f32 ascentBase, f32 descentBase,
                                       f32 hdrH) {
    const f32 aT = ascentBase * (kFontScreen / 14.0f);
    const f32 dT = descentBase * (kFontScreen / 14.0f);
    const f32 aS = ascentBase * (kFontCaption / 14.0f);
    const f32 dS = descentBase * (kFontCaption / 14.0f);
    const f32 gap = dp(4.0f);
    const f32 block = aT + dT + gap + aS + dS;
    const f32 top = (hdrH - block) * 0.5f;
    if (top < 0.0f || aT <= 0.0f) {
        return {kHeaderTitleBase, kHeaderSubBase};   // fallback do constante
    }
    return {top + aT, top + aT + dT + gap + aS};
}

// baseline de UM texto centrado na vertical de um rect (métricas reais) —
// o mesmo padrão do textBaseline dos componentes, agora FONTES ÚNICA
// (Run/Stop/N cortados no topo = offset fixo +14 px; agora centrado)
inline f32 centeredBaseline(f32 ascentBase, f32 descentBase, f32 rectY,
                            f32 rectH, f32 sp) {
    const f32 a = ascentBase * (sp / 14.0f);
    const f32 d = descentBase * (sp / 14.0f);
    return rectY + (rectH - (a + d)) * 0.5f + a;
}

// ---- RAIOS (cantos curvos por escadaria de quads) ---------------------------
constexpr f32 kRadiusCard  = 8.0f;   // cards/botões primários
constexpr f32 kRadiusField = 4.0f;   // campos/chips

// ---- AUDITORIA DE CONTRASTE (WCAG 2.x, PURA — host-testável no CI) ----------
// razão de luminância relativa entre duas cores RGBA (alpha ignorado —
// contraste afere-se cor sólida sobre cor sólida). Os testes usam isto
// para PROVAR text2≥4,5:1 e componentes≥3:1.
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

// ---- 0.9.6.10 (GRUPO UI) · A AUDITORIA DO VIDRO GRAFITE -------------------
// O vidro não pode mentir à auditoria. O REAL (α0.80 sobre o bg grafite,
// o que está por trás dos painéis dokados): text1 15,5:1 · text2 7,6:1
// · accent 10,0:1. O PATOLÓGICO (cena BRANCA PURA por trás): text1 8,2 ✓
// · accent 5,3 ✓ · text2 4,0 · danger 2,5 — a REGRA DA CASA que fecha o
// caso: texto secundário e cores semânticas NÃO flutuam sobre a cena
// viva (sobre vidro vivo é text1; severidade vive em painéis dokados).
// PURAS, testadas no CI.

// a cor FINAL de um painel de vidro sobre o que está por trás (o MESMO
// SRC_ALPHA/ONE_MINUS_SRC_ALPHA do pass UI — a matemática do device)
inline void blendOver(const f32 fg[4], const f32 behind[4], f32 out[4]) {
    const f32 a = fg[3];
    for (int i = 0; i < 3; ++i) {
        out[i] = fg[i] * a + behind[i] * (1.0f - a);
    }
    out[3] = 1.0f;
}

inline constexpr f32 kWorstBehind[4] = {1.0f, 1.0f, 1.0f, 1.0f};  // branco

// O CONTRASTE DE UM TOKEN contra a superfície de vidro no pior caso dela
// (surface α sobre BRANCO PURO). Com a tabela G: text1 8,16:1 · text2
// 3,98:1 · accent 5,27:1 · danger 2,46:1 · warn 4,11:1 · ok 3,18:1 — os
// pisos REAIS da casa fecham em text1/accent (o que flutua); o resto vive
// em painéis dokados (contrastOnSurface, em cima).
inline f32 contrastOnGlass(const f32 token[4]) {
    f32 w[4];
    blendOver(kTheme.surface, kWorstBehind, w);
    return contrastRatio(token, w);
}

// 0.9.6.10 — o composto REAL do vidro dokado (α0.80 sobre o BG grafite —
// o painel lateral por excelência): a FASE 13.10 afere ESTE pixel
inline f32 contrastOnGlassReal(const f32 token[4]) {
    f32 w[4];
    blendOver(kTheme.surface, kTheme.bg, w);
    return contrastRatio(token, w);
}

// 0.9.6.10 — contraste dos tokens V.ONI sobre o BG grafite (#0E0E10):
// voniUser 16,4:1 · voniEngine 9,2:1 · voniReserved 8,1:1 · voniString
// 9,6:1 · voniNumber 13,7:1 · voniComment 5,6:1 (todos ≥4,5:1)
inline f32 contrastOnBg(const f32 color[4]) {
    return contrastRatio(color, kTheme.bg);
}

} // namespace theme
} // namespace vv
