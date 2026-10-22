#pragma once
// ui/Theme.h — DESIGN SYSTEM 0.9.6.10 (spec G), RECALIBRADO 0.11-Y (Y-1),
// pela escada 0.12-C, pelo fogo 0.12-E — e AGORA pela FASE UI (A ORDEM DO
// DONO, envio 1/5 · parte A): a paleta BRASA sobre a pedra escura — o
// destaque ÚNICO #FF5A1F (o âmbar #FFB020 MORREU) e a escada de superfícies
// com os hexes EXATOS do dono.
//
// FONTE ÚNICA das cores/espaçamentos/tipografia/raios de TODO o editor e da
// tela de projetos. 🔶 MUDANÇA ISOLADA: trocar UM token aqui muda a app toda.
//
// FASE UI (parte A) — A TABELA DO DONO (os hexes EXATOS da ordem):
//   bg         #0B0B0D α1.00 — fundo viewport (o FUNDO da escada; == o
//              clear, o contrato «o clear lê o token")
//   surface    #121215 α0.85 — OS PAINÉIS (o vidro da casa; o α0.85 da
//              FASE 3 do 0.12-E mantém-se — a ordem nova muda o RGB)
//   surface2   #1A1A1E α0.86 — vidro denso (premido, chips ativos)
//   card       #1B1B20 α1.00 — OS ELEMENTOS (o dono deu o MESMO hex da
//              escada 0.12-C — o token nem mexe)
//   elevated   #26262C α1.00 — os MENUS/SHEETS/TOAST (o nível mais alto;
//              == border, o padrão da casa desde a 0.12-QoL A)
//   border     #26262C α1.00 — AS LINHAS 1px (o dono: «linhas 1px #26262C»)
//   glassEdge  #FFFFFF α0.12 — o BORDO do vidro (o «vidro» da ordem nova:
//              borda fina + leve brilho no selecionado — o glow em baixo)
//   glassTop   #FFFFFF α0.04 — highlight do TOPO do vidro
//   accent     #FF5A1F α1.00 — A BRASA (o destaque ÚNICO; o hex exato da
//              ordem). SÓ em: seleção, aba ativa, Play, botão principal,
//              cursor de tempo. O ÂMBAR #FFB020 morreu (a ordem: «remover
//              o âmbar atual»); SEM azul como destaque (o azul do eixo Z e
//              da sintaxe V.ONI são CONTEÚDO — exceção documentada)
//   accent2    #E0440F α1.00 — a brasa ESCURA (a base do gradiente VERTICAL
//              dos botões ativos: accent no topo → accent2 em baixo; mesma
//              família da brasa — o destaque continua ÚNICO)
//   accentPress#D94A16 α1.00 — brasa premida (feedback de toque)
//   accentInk  #0E0E10 α1.00 — tinta SOBRE a brasa (6,2:1 — escura)
//   accentDim  #2B1810 α1.00 — O FUNDO DE SELEÇÃO do dono (a barra lava +
//              o fill translúcido da linha selecionada)
//   glow       #FF5A1F α0.25 — O BRILHO DA BRASA (o glow de foco composto
//              2 quads + a linha 1dp do topo do vidro a 20%)
//   text1      #E8E6E3 α1.00 — texto primário (o hex exato do dono;
//              15,1:1 sobre o painel composto)
//   text2      #8A8A93 α1.00 — texto secundário (5,5:1 sobre surface)
//   danger     #E5484D α1.00 — semântica (== o axisX novo do dono)
//   warn       #FF8A3D α1.00 — LARANJA semântico (nunca == accent)
//   ok         #46A758 α1.00
//   scrim      preto 60% — véu MODAL (mantém-se)
//   scrimMenu  preto 40% — véu dos MENUS ANCORADOS (mantém-se)
//   axisX/Y/Z  #E5484D · #3FB950 · #3B82F6 — OS EIXOS do dono (exatos;
//              CONTEÚDO 3D, não chrome — o azul do Z é a exceção de sempre)
//
// CONTRASTE — OS PISOS REAIS (validados ANTES do commit, o padrão da casa):
// sobre o painel composto (α0.85 sobre o fundo): text1 15,1:1 · text2
// 5,5:1 · accent 6,1:1 · danger 4,8:1 · warn 8,0:1 · ok 6,2:1 — TODO o
// texto ≥4,5:1 e TODO o componente ≥3:1. O pior caso do vidro (cena BRANCA
// PURA por trás): text1 9,7:1 ✓ · accent 3,9:1 ✓ — a REGRA DA CASA mantém-
// se: TEXTO SECUNDÁRIO E SEMÂNTICA NÃO FLUTUAM sobre a cena viva — sobre
// vidro flutuante usa-se text1; severidade vive em painéis dokados.
// blendOver/contrastOnGlass medem TUDO no CI.
//
// ESCALA: espaçamento 4/8/12/16 (A ESCALA DO DONO na parte A — o 12 entrou
// como kSpaceMid; o 24 do recuo de filhos morre na parte C); alvos desenho
// 32 / toque 40 com ≥8dp entre eles; nada ≥48; texto 12sp legendas/status,
// 14sp corpo/linhas, 16sp títulos de secção, 20sp títulos de ecrã — sans
// para a UI (o atlas único da casa); estado/código mono é o alvo da fase
// (o 2.º atlas é dívida declarada no relatório). Raios 8dp cards/botões,
// 6dp campos/chips (A ORDEM: raio 6-8). Zero emoji, zero blur/sombras
// REAIS (immediate-mode C++; o "canto curvo" é escadaria de quads —
// UiContext::panelRounded). O GLOW do foco é COMPOSTO (2 quads α
// decrescente — a simulação honesta do brilho, sem blur real). O vidro
// NÃO tem backdrop blur.
//
// COMPATIBILIDADE: os tokens históricos (BG/PANEL/LINE/TEXT/ACCENT/WARN de
// UiContext.h) continuam a existir e APONTAM para ESTA tabela — a app
// inteira muda de pele num só sítio (desde a 0.9.0; agora pedra+brasa).
#include "core/Types.h"
#include <cmath>

namespace vv {
namespace theme {

// ---- tabela grafite+âmbar+vidro (spec G) — UM struct, UM só lugar ----
struct Theme {
    // cores de fundo (o ALPHA é o VIDRO: 1.0 = opaco, <1.0 = vê-se o que
    // está por trás — o pass UI desenha TODO com blend desde a F1)
    f32 bg[4];         // #0B0B0D α1.00 (O FUNDO VIEWPORT do dono — igual ao
                       //  clear da app)
    f32 surface[4];    // #121215 α0.85 (OS PAINÉIS do dono — o vidro da
                       //  casa; o RGB é novo, o α0.85 mantém-se)
    f32 surface2[4];   // #1A1A1E α0.86 (vidro denso — premido)
    // A ESCADA DE SUPERFÍCIES (0.12-C C-3, recalibrada pela FASE UI com os
    // hexes EXATOS do dono): fundo #0B0B0D (o bg acima) · painel #121215
    // (a surface acima) · ELEMENTOS #1B1B20 (os cards) · ELEVADO #26262C
    // (menus/sheets/toast — o que flutua por cima)
    f32 card[4];       // #1B1B20 α1.00 (OS ELEMENTOS do dono — os cards do
                       //  drawer, o browser, o diálogo aplicar)
    f32 elevated[4];   // #26262C α1.00 (os menus/sheets/toast — o nível
                       //  mais alto da escada)
    // traço
    f32 border[4];     // #26262C α1.00 — AS LINHAS 1px do dono (as bordas
                       //  dos painéis/chips/campos INATIVOS; o destaque
                       //  fica APENAS no ativo/selecionado — a regra da
                       //  casa que se mantém)
    f32 glassEdge[4];  // #FFFFFF α0.12 (0x1F — a aresta de luz do vidro)
    f32 glassTop[4];   // #FFFFFF α0.04 (0x0A — o highlight do topo)
    // texto
    f32 text1[4];      // #E8E6E3 α1.00 (o TEXTO do dono na FASE UI)
    f32 text2[4];      // #8A8A93 α1.00 (o TEXTO SECUNDÁRIO do dono; 5,5:1
                       //  sobre surface)
    // ação (A BRASA — o destaque ÚNICO da FASE UI; o âmbar morreu)
    f32 accent[4];     // #FF5A1F α1.00 (A BRASA; 6,1:1 sobre surface)
    f32 accent2[4];    // #E0440F α1.00 (a brasa ESCURA — a base do gradiente
                       //  dos botões ATIVOS: accent topo → accent2 fundo)
    f32 accentPress[4];// #D94A16 α1.00 (premido — mais escuro, feedback)
    f32 accentInk[4];  // #0E0E10 α1.00 (tinta SOBRE a brasa — escura, 6,2:1)
    f32 accentDim[4];  // #2B1810 α1.00 (O FUNDO DE SELEÇÃO do dono: a linha
                       //  selecionada e os chips ativos; text1 13,6:1 e
                       //  accent 5,4:1 em cima dele)
    f32 glow[4];       // #FF5A1F α0.25 (O BRILHO DA BRASA — o glow de foco
                       //  composto 2 quads α decrescente + a linha 1dp do
                       //  topo do vidro a 20%)
    f32 danger[4];     // #E5484D α1.00 (semântica — exceção documentada)
    f32 warn[4];       // #FF8A3D α1.00 (LARANJA — hue 24°, nunca o accent 39°)
    f32 ok[4];         // #46A758 α1.00
    f32 scrim[4];      // preto 60% (véu modal)
    f32 scrimMenu[4];  // preto 40% (véu dos menus ANCORADOS — PASSO 4)
    // 0.12-E (E-3 · FASE 3.2): A SOMBRA FALSA da profundidade 2.5D — o
    // retângulo preto 15% deslocado 3dp por baixo de CADA botão (a ordem:
    // «Em vez de 3D, desenha um retângulo preto com 15% de opacidade
    // deslocado 3px para baixo e para a direita por baixo de cada botão»;
    // o 3px do dono à densidade de referência 1.0 — a LEI DA CASA R-018
    // manda dp(): o deslocamento escala com a densidade como todo o
    // layout, o efeito é o MESMO em qualquer ecrã)
    f32 dropShadow[4]; // preto α0.15 (a sombra 2.5D)
    // 0.9.2 — PALETA V.ONI (spec §10 🔶 — coloração do editor de script;
    // o parser classifica tokens, AS CORES VIVEM AQUI — flip de 1 token).
    // FASE UI H (a ordem do dono: «brasa só nas palavras reservadas»):
    // as reservadas são A BRASA (o MESMO valor do accent — o destaque
    // único dentro do editor é a sintaxe); string/número/comentário/engine
    // passaram a NEUTRAS DISTINTAS (a família cinza quente da casa — o
    // roxo/azul/amarelo do 0.9.2 morreram). CONTEÚDO, não chrome.
    f32 voniReserved[4];   // #FF5A1F reservadas (A BRASA — o MESMO valor do
                           //  accent; 6,3:1 no bg)
    f32 voniEngine[4];     // #8A8A93 engine/maiúsculas (== text2 — neutra;
                           //  5,8:1 no bg)
    f32 voniUser[4];       // #E8E6E3 utilizador (== text1)
    f32 voniString[4];     // #B5AFA6 strings (neutra média quente — 9,0:1
                           //  no bg; 8,6:1 na banda da linha corrente)
    f32 voniNumber[4];     // #D6D2C8 números (neutra clara — 13,0:1 no bg)
    f32 voniComment[4];    // #7E7E86 comentários (neutra escura — 4,9:1 no
                           //  bg; 4,6:1 na banda da linha corrente)
    // EIXOS DO GIZMO (CONTEÚDO 3D, não chrome — a exceção documentada
    // desde 0.6.9; vivem AQUI porque a spec manda: cores SÓ no Theme;
    // FASE UI: os hexes EXATOS do dono)
    f32 axisX[4];      // #E5484D vermelho (X)
    f32 axisY[4];      // #3FB950 verde (Y)
    f32 axisZ[4];      // #3B82F6 azul (Z — conteúdo, NÃO chrome)
    f32 axisDim[4];    // #9E9E9E cinza (central)
    // 0.12-D (D-2): a TINTA ESCURA do vidro real — o blur da cena × esta
    // tinta é o corpo do painel flutuante (o #6B6B76 α0.94 escurece SEM
    // apagar o que está atrás; o α é a opacidade do corpo)
    f32 glassInk[4];   // #6B6B76 α0.94
    // 0.12-D (D-3): A RAMPA DO FOGO do dono — extraída do ícone REAL
    // (docs/iconegonevv.png, o «G em chama»; o pin fogo_rampa aferra as 4
    // contra a PALETA do PNG): a escura da base → o laranja → o âmbar
    // quente → o amarelo do bico
    f32 fire0[4];      // #7A1F0A (a base escura)
    f32 fire1[4];      // #E8530F (o laranja)
    f32 fire2[4];      // #FFA21F (o âmbar quente)
    f32 fire3[4];      // #FFE08A (o amarelo do bico)
    // 0.12-E (E-2): A PALETA FOGO AVERMELHADO do dono (a ordem EXATA —
    // «Atualiza o ui/Theme.h com esta paleta»): as BRASAS do redesign 2.5D
    // (o gradiente dos ativos da FASE 3: fireRed no topo → fireOrange no
    // fundo; o âmbar/accent fica como o brilho de destaque)
    f32 fireRed[4];    // #FF5A1F (A BRASA — o topo do gradiente dos ativos;
                       //  a FASE UI re-alinha o par à brasa ÚNICA do dono)
    f32 fireOrange[4]; // #E0440F (a brasa escura — o fundo do gradiente)
};

// hex→f32 normalizado (compile-time-friendly por field) — com ALPHA:
// VV_RGBA(r,g,b,a) com a em FLOAT 0..1 (o vidro: 0.88f/0.92f/0.55f);
// VV_RGB = opaco (a=1)
#define VV_RGBA(r, g, b, a)                                              \
    {static_cast<f32>(r) / 255.0f, static_cast<f32>(g) / 255.0f,         \
     static_cast<f32>(b) / 255.0f, static_cast<f32>(a)}
#define VV_RGB(r, g, b) VV_RGBA(r, g, b, 1.0f)

// FASE UI parte A (os hexes EXATOS do dono): 0B0B0D=(11,11,13) ·
// 121215=(18,18,21) · 1A1A1E=(26,26,30) · 26262C=(38,38,44)
// E8E6E3=(232,230,227) · 8A8A93=(138,138,147) · FF5A1F=(255,90,31)
// E0440F=(224,68,15) · D94A16=(217,74,22) · 2B1810=(43,24,16)
// E5484D=(229,72,77) · 3FB950=(63,185,80) · 3B82F6=(59,130,246)
inline constexpr Theme kTheme{
    VV_RGB(11, 11, 13),          // bg         (O FUNDO #0B0B0D do dono — a
                                 //  escada FASE UI; a 0A0A0C foi superada
                                 //  pela ordem expressa do dono, padrão Y-3)
    VV_RGBA(18, 18, 21, 0.85f),  // surface    α0.85 — O VIDRO da casa
                                 //  (o PAINEL #121215 da FASE UI)
    VV_RGBA(26, 26, 30, 0.86f),  // surface2   α0.86 — vidro denso
    VV_RGB(27, 27, 32),          // card       (OS ELEMENTOS #1B1B20 — o
                                 //  dono deu o MESMO hex da escada)
    VV_RGB(38, 38, 44),          // elevated   (O ELEVADO #26262C)
    VV_RGB(38, 38, 44),          // border     (#26262C — AS LINHAS 1px do
                                 //  dono; o destaque só no ativo)
    VV_RGBA(255, 255, 255, 31.0f / 255.0f),   // glassEdge (#FFFFFF1F)
    VV_RGBA(255, 255, 255, 10.0f / 255.0f),   // glassTop  (#FFFFFF0A)
    VV_RGB(232, 230, 227),       // text1      (#E8E6E3 — o texto do dono)
    VV_RGB(138, 138, 147),       // text2      (#8A8A93)
    VV_RGB(255, 90, 31),         // accent     (A BRASA — o destaque ÚNICO)
    VV_RGB(224, 68, 15),         // accent2    (a brasa escura — gradiente)
    VV_RGB(217, 74, 22),         // accentPress
    VV_RGB(14, 14, 16),          // accentInk  (tinta escura na brasa)
    VV_RGB(43, 24, 16),          // accentDim  (O FUNDO DE SELEÇÃO #2B1810)
    VV_RGBA(255, 90, 31, 0.25f), // glow       (O BRILHO DA BRASA)
    VV_RGB(229, 72, 77),         // danger
    VV_RGB(255, 138, 61),        // warn       (LARANJA ≠ brasa)
    VV_RGB(70, 167, 88),         // ok
    {0.0f, 0.0f, 0.0f, 0.60f},   // scrim (60%)
    {0.0f, 0.0f, 0.0f, 0.40f},   // scrimMenu (40% — PASSO 4, menus ancorados)
    {0.0f, 0.0f, 0.0f, 0.15f},   // dropShadow (a sombra 2.5D — FASE 3.2)
    // V.ONI (sintaxe = conteúdo): FASE UI H — a brasa nas RESERVADAS (o
    // MESMO valor do accent) e as restantes NEUTRAS distintas; no fundo
    // #0B0B0D TODOS ≥4,5:1 (user 15,8 · number 13,0 · string 9,0 ·
    // reserved 6,3 · engine 5,8 · comment 4,9) — a mesma família cinza
    // quente da casa, sem roxo/azul/amarelo
    VV_RGB(255, 90, 31),    // voniReserved (A BRASA == accent #FF5A1F)
    VV_RGB(138, 138, 147),  // voniEngine (== text2 #8A8A93)
    VV_RGB(232, 230, 227),  // voniUser (== text1 #E8E6E3 — a FASE UI)
    VV_RGB(181, 175, 166),  // voniString #B5AFA6 (neutra média quente)
    VV_RGB(214, 210, 200),  // voniNumber #D6D2C8 (neutra clara)
    VV_RGB(126, 126, 134),  // voniComment #7E7E86 (neutra escura)
    // EIXOS (conteúdo 3D — OS HEXES EXATOS DO DONO na FASE UI)
    VV_RGB(229, 72, 77),    // axisX  #E5484D
    VV_RGB(63, 185, 80),    // axisY  #3FB950
    VV_RGB(59, 130, 246),   // axisZ  #3B82F6 (conteúdo, NÃO chrome)
    VV_RGB(158, 158, 158),  // axisDim
    // 0.12-D (D-2): a tinta do vidro real (#6B6B76 α0.94)
    VV_RGBA(107, 107, 118, 0.94f),  // glassInk
    // 0.12-D (D-3): a RAMPA DO FOGO (extraída do ícone real — o LOGO é
    // identidade, a FASE UI não o toca)
    VV_RGB(122, 31, 10),    // fire0  #7A1F0A
    VV_RGB(232, 83, 15),    // fire1  #E8530F
    VV_RGB(255, 162, 31),   // fire2  #FFA21F
    VV_RGB(255, 224, 138),  // fire3  #FFE08A
    // FASE UI: o par do gradiente dos ativos RE-ALINHADO à brasa ÚNICA
    VV_RGB(255, 90, 31),    // fireRed    #FF5A1F (A BRASA — o topo)
    VV_RGB(224, 68, 15),    // fireOrange #E0440F (a brasa escura — o fundo)
};
#undef VV_RGB
#undef VV_RGBA

// ---- atalhos estáveis (o chamador escreve theme::SURFACE etc.) -------------
// Os nomes HISTÓRICOS (BG/PANEL/LINE/TEXT/ACCENT/WARN — UiContext.h legado)
// apontam agora para ESTA tabela: BG=bg, PANEL=surface, LINE=border,
// TEXT=text1, ACCENT=text1 (era #F5F5F5 — mesmo valor, zero mudança visual
// nesses usos), WARN=warn. Definidos em UiContext.h (aliases) para não
// duplicar símbolos.

// ---- ESPAÇAMENTO (A ESCALA DO DONO na FASE UI: 4/8/12/16) ----------------
constexpr f32 kSpaceIcon = 4.0f;    // exceção: ícones DENTRO de alvos
constexpr f32 kSpace1 = 8.0f;       // vão mínimo entre alvos de toque
constexpr f32 kSpaceMid = 12.0f;    // FASE UI: o 12 da escala do dono (vãos
                                    //  médios — os vãos X/Y/Z do inspector
                                    //  já o usavam por ordem da QoL A)
constexpr f32 kSpace2 = 16.0f;      // padding de painel/card
constexpr f32 kSpace3 = 16.0f;      // recuo de filhos na hierarquia (a
                                    //  FASE UI parte C alinha o recuo à
                                    //  escala do dono — era 24)
constexpr f32 kSpace4 = 32.0f;      // (2×16 — fora da escala base, mantém-
constexpr f32 kSpace5 = 40.0f;      //  se pelos chamadores existentes)
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

// ---- RAIOS (cantos curvos por escadaria de quads — A ORDEM: raio 6-8) ----
constexpr f32 kRadiusCard  = 8.0f;   // cards/botões primários
constexpr f32 kRadiusField = 6.0f;   // campos/chips (a FASE UI subiu o 4
                                     //  para o 6 da banda do dono)

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

// ---- 0.9.6.10 (GRUPO UI) · A AUDITORIA DO VIDRO — recalibrada 0.11-Y ----
// O vidro não pode mentir à auditoria. O REAL (α0.80 sobre o poço #08080A,
// o que está por trás dos painéis dokados): text1 15,6:1 · text2 7,6:1
// · accent 10,1:1. O PATOLÓGICO (cena BRANCA PURA por trás): text1 8,3 ✓
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
// (surface α sobre BRANCO PURO). Com a tabela 0.11-Y: text1 8,3:1 · text2
// 4,0:1 · accent 5,3:1 · danger 2,5:1 · warn 4,2:1 · ok 3,2:1 — os
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

// 0.9.6.10 · recalibrado FASE UI H — contraste dos tokens V.ONI sobre o BG
// (#0B0B0D): voniUser 15,8:1 · voniNumber 13,0:1 · voniString 9,0:1 ·
// voniReserved (a brasa) 6,3:1 · voniEngine 5,8:1 · voniComment 4,9:1
// (todos ≥4,5:1)
inline f32 contrastOnBg(const f32 color[4]) {
    return contrastRatio(color, kTheme.bg);
}

} // namespace theme
} // namespace vv
