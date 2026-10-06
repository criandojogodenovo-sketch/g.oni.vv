#pragma once
// ui/Theme.h — DESIGN SYSTEM 0.9.6.9 (spec F · GRUPO F: MONO+VIDRO).
//
// FONTE ÚNICA das cores/espaçamentos/tipografia/raios de TODO o editor e da
// tela de projetos. 🔶 MUDANÇA ISOLADA: trocar UM token aqui muda a app toda.
//
// 0.9.6.9 (GRUPO F · IDENTIDADE): o REGRESSO À RAMPA NEUTRA da F1 — o tema
// mono original da casa (BG #141414 · PANEL #1E1E1E · LINE #2E2E2E · TEXT
// #E6E6E6 · ACCENT #F5F5F5 — «tokens mono», f4.1) — com a ESTRUTURA da
// spec A (text2/accentInk/scrim/raios/auditoria) e o VIDRO: as superfícies
// ganham ALPHA e o pass UI já desenha com blend (GL_BLEND SRC_ALPHA em
// TODO o pass desde a F1 — o scrim 60% e o toast 0.95 eram os precedentes).
// Onde há profundidade (chips/toolbar/drawer/menus/toasts sobre a viewport
// 3D) o vidro é REAL — a cena aparece por trás; os painéis laterais ficam
// sobre o clear (véu escuro subtil). O AZUL da spec A (#2196F3) morre.
//
// TABELA (spec F — mono+vidro; ΔR=G=B ≤2 em TODO o chrome):
//   bg         #141414 α1.00 — fundo da app (atrás de tudo; barras sistema)
//   surface    #1E1E1E α0.88 — VIDRO: painéis/cards (hierarquia, inspector)
//   surface2   #262626 α0.92 — vidro DENSO (premido, toasts, chips ativos)
//   border     #2E2E2E α0.55 — hairline de vidro (traço fino)
//   text1      #E6E6E6 α1.00 — texto primário (13,4:1 sobre surface)
//   text2      #A6A6A6 α1.00 — texto secundário (6,9:1 sobre surface)
//   accent     #F5F5F5 α1.00 — o MONO (o branco da F1; era azul #2196F3)
//   accentPress#DADADA α1.00 — accent premido (feedback de toque)
//   accentInk  #141414 α1.00 — tinta SOBRE accent (16,9:1 — o fill é BRANCO,
//                              a tinta passa a ESCURA; era text1: branco
//                              sobre branco, o risco que o Java também tinha)
//   danger     #EF5350 α1.00 — ações destrutivas (exceção documentada: a
//   warn       #FABB45 α1.00 — semântica É cor — como as cores dos EIXOS do
//   ok         #66BB6A α1.00 — gizmo e a paleta V.ONI, sempre foram)
//   scrim      preto 60% — véu modal por trás de sheets/diálogos
//
// CONTRASTE — A AUDITORIA DO VIDRO NÃO MENTE (o padrão honesto do F): o
// pior caso de um painel de vidro é estar sobre uma cena BRANCA PURA (o
// dono pode pôr um modelo branco no viewport). O piso da casa mantém-se
// MESMO AÍ (blendOver/worstCaseGlass abaixo, PURAS e testadas no CI):
//   text1 9,25:1 · text2 4,74:1 (≥4,5 ✓) · accent 10,59:1 · danger 3,31:1
//   warn 6,74:1 · ok 4,88:1 (≥3,0 ✓) — tudo sobre surface@0.88→(57,57,57)
// Sobre a surface SÓLIDA: text1 13,4:1 · text2 6,9:1 · accent 15,3:1 ·
//   danger 4,8:1 · warn 9,7:1 · ok 7,1:1.
// A paleta V.ONI (sintaxe = CONTEÚDO, não chrome; exceção documentada ao
// mono como as cores dos eixos): voniComment sobe um degrau (#757575 →
// #8A8A8A) porque o bg novo #141414 é mais claro que o navy #0B0E13 — o
// piso 4,5:1 mantém-se (4,0:1 → 5,3:1).
//
// ESCALA: espaçamento em múltiplos de 8dp (4 só p/ ícones internos);
// alvos de toque ≥48dp com ≥8dp entre eles; texto 12sp legendas/status,
// 14sp corpo/linhas, 16sp títulos de secção, 20sp títulos de ecrã;
// raios 8dp cards/botões, 4dp campos/chips. Zero emoji, zero blur/sombras/
// gradientes (immediate-mode C++; o "canto curvo" é escadaria de quads —
// UiContext::panelRounded).
//
// COMPATIBILIDADE: os tokens históricos (BG/PANEL/LINE/TEXT/ACCENT/WARN de
// UiContext.h) continuam a existir e APONTAM para ESTA tabela — a app
// inteira muda de pele num só sítio (desde a 0.9.0; agora mono+vidro).
#include "core/Types.h"
#include <cmath>

namespace vv {
namespace theme {

// ---- tabela mono+vidro (spec F) — UM struct, UM só lugar para mudar -------
struct Theme {
    // cores de fundo (o ALPHA é o VIDRO: 1.0 = opaco, <1.0 = vê-se o que
    // está por trás — o pass UI desenha TODO com blend desde a F1)
    f32 bg[4];         // #141414 α1.00 (a F1 — igual ao clear da app)
    f32 surface[4];    // #1E1E1E α0.88 (VIDRO)
    f32 surface2[4];   // #262626 α0.92 (vidro denso — premido)
    // traço
    f32 border[4];     // #2E2E2E α0.55 (hairline)
    // texto
    f32 text1[4];      // #E6E6E6 α1.00 (a F1)
    f32 text2[4];      // #A6A6A6 α1.00 (neutro — 6,9:1 sobre surface)
    // ação (MONO: branco da casa; o azul da spec A morreu)
    f32 accent[4];     // #F5F5F5 α1.00 (o ACCENT da F1)
    f32 accentPress[4];// #DADADA α1.00 (premido — mais escuro, feedback)
    f32 accentInk[4];  // #141414 α1.00 (tinta SOBRE accent — o fill é
                       // BRANCO, a tinta é ESCURA: 16,9:1. A spec A tinha
                       // accentInk=text1: com accent mono seria branco
                       // sobre branco — INVISÍVEL)
    f32 danger[4];     // #EF5350 α1.00 (semântica — exceção documentada)
    f32 warn[4];       // #FABB45 α1.00
    f32 ok[4];         // #66BB6A α1.00
    f32 scrim[4];      // preto 60% (véu modal)
    // 0.9.2 — PALETA V.ONI (spec §10 🔶 — coloração do editor de script;
    // o parser classifica tokens, AS CORES VIVEM AQUI — flip de 1 token)
    f32 voniReserved[4];   // #B39DDB reservadas (roxo)
    f32 voniEngine[4];     // #8AB4F8 engine/maiúsculas (azul)
    f32 voniUser[4];       // #F5F5F5 utilizador (branco)
    f32 voniString[4];     // #81C784 strings (verde)
    f32 voniNumber[4];     // #FFD54F números (amarelo)
    f32 voniComment[4];    // #8A8A8A comentários (o degrau do mono — 5,3:1)
};

// hex→f32 normalizado (compile-time-friendly por field) — com ALPHA:
// VV_RGBA(r,g,b,a) com a em FLOAT 0..1 (o vidro: 0.88f/0.92f/0.55f);
// VV_RGB = opaco (a=1)
#define VV_RGBA(r, g, b, a)                                              \
    {static_cast<f32>(r) / 255.0f, static_cast<f32>(g) / 255.0f,         \
     static_cast<f32>(b) / 255.0f, static_cast<f32>(a)}
#define VV_RGB(r, g, b) VV_RGBA(r, g, b, 1.0f)

// 141414=(20,20,20) · 1E1E1E=(30,30,30) · 262626=(38,38,38) · 2E2E2E=(46,46,46)
// E6E6E6=(230,230,230) · A6A6A6=(166,166,166) · F5F5F5=(245,245,245)
// DADADA=(218,218,218) · EF5350=(239,83,80) · FABB45=(250,187,69)
// 66BB6A=(102,187,106)
inline constexpr Theme kTheme{
    VV_RGB(20, 20, 20),        // bg         (a rampa F1 de volta)
    VV_RGBA(30, 30, 30, 0.88f),  // surface    α0.88 — O VIDRO
    VV_RGBA(38, 38, 38, 0.92f),  // surface2   α0.92 — vidro denso
    VV_RGBA(46, 46, 46, 0.55f),  // border     α0.55 — hairline
    VV_RGB(230, 230, 230),     // text1      (a F1)
    VV_RGB(166, 166, 166),     // text2      (neutro)
    VV_RGB(245, 245, 245),     // accent     (o MONO — o ACCENT da F1)
    VV_RGB(218, 218, 218),     // accentPress
    VV_RGB(20, 20, 20),        // accentInk  (tinta ESCURA no fill branco)
    VV_RGB(239, 83, 80),       // danger     (semântica — exceção)
    VV_RGB(250, 187, 69),      // warn
    VV_RGB(102, 187, 106),     // ok
    {0.0f, 0.0f, 0.0f, 0.60f},   // scrim (60%)
    // V.ONI (sintaxe = conteúdo): voniComment sobe um degrau — o bg novo
    // #141414 é mais claro que o navy #0B0E13 e o piso 4,5:1 mantém-se
    // (4,0:1 → 5,3:1); os outros tokens caem <8% e ficam todos ≥7,7:1
    VV_RGB(179, 157, 219),  // voniReserved
    VV_RGB(138, 180, 248),  // voniEngine
    VV_RGB(245, 245, 245),  // voniUser
    VV_RGB(129, 199, 132),  // voniString
    VV_RGB(255, 213, 79),   // voniNumber
    VV_RGB(138, 138, 138),  // voniComment (#757575 → #8A8A8A: o degrau)
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

// ---- 0.9.6.9 (GRUPO F) · A AUDITORIA DO VIDRO --------------------------------
// O vidro não pode mentir à auditoria: um painel α0.88 sobre uma cena
// BRANCA PURA (o pior caso — o dono pode pôr um modelo branco no viewport)
// ainda tem de dar aos textos os pisos da casa. PURAS, testadas no CI.

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
// (surface α sobre BRANCO PURO). Com a tabela F: text1 9,25:1 · text2
// 4,74:1 · accent 10,59:1 · danger 3,31:1 · warn 6,74:1 · ok 4,88:1 —
// os pisos da casa (4,5 texto / 3,0 componente) SEGURAM no vidro.
inline f32 contrastOnGlass(const f32 token[4]) {
    f32 w[4];
    blendOver(kTheme.surface, kWorstBehind, w);
    return contrastRatio(token, w);
}

// 0.9.6.9 — contraste dos tokens V.ONI sobre o BG NOVO do editor (#141414,
// o mono): voniUser 16,9:1 · voniEngine 8,7:1 · voniReserved 7,7:1 ·
// voniString 9,2:1 · voniNumber 13,1:1 · voniComment 5,3:1 (todos ≥4,5:1 —
// o comment subiu um degrau porque o bg neutro é mais claro que o navy)
inline f32 contrastOnBg(const f32 color[4]) {
    return contrastRatio(color, kTheme.bg);
}

} // namespace theme
} // namespace vv
