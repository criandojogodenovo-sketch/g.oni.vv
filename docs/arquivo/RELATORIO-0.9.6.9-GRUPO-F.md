# RELATORIO 0.9.6.9 — GRUPO F: IDENTIDADE (FASE 0.9.6-MASTER)

> O décimo mandamento da campanha: «VER, TESTAR, CORRIGIR, NÃO PARTIR».
> O Grupo F é a identidade pedida pelo rastreador: «tokens mono+vidro
> (R-020 de tema); ícone G com 4 setas». A entrega traz o REGRESSO ao
> tema MONO original da casa (a rampa neutra com que a app nasceu) com
> o VIDRO de verdade (superfícies translúcidas com a cena por trás), a
> TINTA certa no fill branco (o branco-sobre-branco que o mono traria),
> o CLEAR a ler o token (o fóssil da F1), o ícone G com 4 setas com o
> gerador COMMITADO no repo — e, apanhado ao vivo pelo próprio vidro, O
> WIPE DO FRAMEBUFFER DO STUB (os PNGs exportados desenhavam a UI sobre
> preto desde o Grupo D).
> Sentinela: R-028 (docs/REGRESSOES.md).
> Suíte local no fecho: **core 843/0 + c33_virtual 440/0** + gates verdes.

## 1 · O QUE FOI PEDIDO

O rastreador (BACKLOG.md, lido no arranque pela P-01) define o Grupo F:
«tokens mono+vidro (R-020 de tema); ícone G com 4 setas». São DUAS
entregas e uma vigília:

1. os TOKENS do tema viram MONO+VIDRO — o «R-020 de tema» do
   rastreador é a pendência temática da campanha: a identidade visual
   da app;
2. o ícone da app passa a «G com 4 setas» (o «G com a lâmpada» da
   0.9.0 sai);
3. o que já está vigiado (R-024 o layout exportado, R-018 a escala,
   R-026/R-027 os orçamentos) continua VERDE com a pele nova.

O que NÃO estava no pedido (e não foi feito): nenhuma funcionalidade
nova, nenhuma mudança à linguagem V.ONI, nenhuma mudança de layout (o
tema não mexe em RECTS — a 13.9 prova), nada fora do rastreador.

## 2 · O MÉTODO (o diagnóstico antes de mexer)

Leitura por ordem da P-01 (REGRESSOES → RELATORIO-0.9.6.8-E → README →
BACKLOG) e depois a leitura de código com a pergunta: «o que é o tema
DESTA app e onde ele muda?». A medida:

| item medido/contado | estado ANTES | veredicto |
|---|---|---|
| o tema ORIGINAL (F1, f4.1 «tokens mono») | BG #141414 · PANEL #1E1E1E · LINE #2E2E2E · TEXT #E6E6E6 · ACCENT #F5F5F5 — NEUTRA a sério | «tokens mono» = o REGRESSO à rampa F1 (não só o accent) |
| o tema da spec A (0.9.6 atual) | navy #0B0E13/#151A23/#2A3442 + accent AZUL #2196F3 (31 usos) + accentInk 24 + accentPress 5 — tudo por kTheme | a promessa «flip de 1 token» do header do Theme.h nunca usada |
| o glClearColor | `0.0784314` HARDCODED = **#141414 — o BG DA F1!** | um FÓSSIL: a spec A mudou o bg token para navy e o clear NUNCA acompanhou — invisível enquanto tudo era opaco |
| o blend do pass UI | GL_BLEND SRC_ALPHA/ONE_MINUS no pass INTEIRO (Renderer::endFrame) desde a F1; o scrim 60% e o toast 0.95×alpha os únicos vidros | o vidro é BARATO: os tokens só precisam de ALPHA |
| panelRounded | corpo em CRUZ + escadaria por faixas DISJUNTAS | vidro SEM costuras/duplo-blend por construção |
| o ícone | «G com a lâmpada» (azul #8AB4F8 + amarelo #FFD54F sobre navy #1E2633); ic_launcher==round byte-a-byte; o GERADOR em scripts-local/ NUNCA commitado | PNGs ÓRFÃOS — a fonte do ícone não é reproduzível (dívida de processo) |
| o Button24 (Java) | `setTextColor(outline ? ACCENT : TEXT1)` — o FILL pinta TEXT1 branco por cima | com accent→branco era BRANCO SOBRE BRANCO (o mesmo bug latente no accentInk=text1 da spec A) |
| o voniComment no bg novo | #757575 → 4,0:1 sobre #141414 (<4,5) | o bg neutro é MAIS CLARO que o navy: o comment sobe um degrau |

O ACHADO do diagnóstico é o mesmo do E/D: medidas e artefactos que
ninguém via (o clear fóssil, o gerador órfão, o dst do blend) — a
diferença é que o VIDRO os põe TODOS à vista de uma vez.

## 3 · A TABELA MONO+VIDRO (spec F)

O REGRESSO à rampa neutra F1 com a ESTRUTURA da spec A (text2,
accentInk, scrim, raios, auditoria) e o VIDRO:

| token | valor | o que é |
|---|---|---|
| bg | #141414 α1.00 | a F1 de volta — e o clear LÊ-O (o fóssil morreu) |
| surface | #1E1E1E **α0.88** | **O VIDRO** — painéis/cards (hierarquia, inspector) |
| surface2 | #262626 α0.92 | vidro DENSO (premido, toasts, chips ativos) |
| border | #2E2E2E α0.55 | hairline de vidro (o traço fino) |
| text1 | #E6E6E6 α1.00 | a F1 (13,4:1 sobre surface) |
| text2 | #A6A6A6 α1.00 | NEUTRO (6,9:1 sólido; 4,74:1 no pior caso do vidro) |
| accent | #F5F5F5 α1.00 | **O MONO** — o ACCENT da F1; o azul #2196F3 morreu |
| accentPress | #DADADA α1.00 | o feedback de premir (mais escuro) |
| accentInk | **#141414** α1.00 | **a tinta ESCURA sobre o fill BRANCO** (16,9:1) |
| danger/warn/ok | #EF5350/#FABB45/#66BB6A | semânticas — as exceções documentadas de sempre |
| scrim | preto 60% | o véu modal (o vidro artesanal que já existia) |

A paleta V.ONI fica INTACTA (sintaxe = CONTEÚDO, não chrome — a exceção
documentada ao mono, como as cores dos eixos do gizmo desde a 0.6.9)
com UM degrau: voniComment #757575→#8A8A8A (4,0:1→5,3:1 no bg novo).

**A ARQUITETURA DO VIDRO**: desde o Grupo D o pass 3D é scissored ao
rect da viewport — o vidro materializa-se ONDE HÁ PROFUNDIDADE (os
chips da toolstack, a toolbar inferior, o drawer, os menus ancorados, os
toasts — TODOS sobre a viewport com a cena 3D por trás) e os painéis
laterais ficam sobre o clear (um véu escuro subtil). O scissor do D é
load-bearing (R-026) — NÃO SE MEXE; o vidro é um efeito de TOKEN, não
de arquitetura.

## 4 · A AUDITORIA DO VIDRO (o padrão honesto do F)

O vidro não pode mentir à auditoria: um painel α0.88 sobre uma cena
BRANCA PURA (o pior caso — o dono pode pôr um modelo branco no
viewport) ainda tem de dar aos textos os pisos da casa. As funções
PURAS no Theme.h (testadas no CI):

- `blendOver(fg, behind, out)` — o composto SRC_ALPHA/ONE_MINUS (a
  matemática EXATA do pass UI no device);
- `contrastOnGlass(token)` — o contraste do token contra o vidro no
  pior caso (surface α sobre branco puro).

Os valores medidos (o scripts/f_mono_math.py verificou ANTES de a
tabela ser escrita): text1 9,25:1 · text2 **4,74:1** (≥4,5 ✓) · accent
10,59:1 · danger **3,31:1** (≥3 ✓) · warn 6,74:1 · ok 4,88:1 — e a
tinta accentInk/accent 16,9:1. Os pisos SEGURAM no vidro.

## 5 · O CLEAR LÊ O TOKEN (o fóssil da F1)

O `glClearColor(0.0784314, …)` era o literal **#141414 — o BG DA F1** —
que sobreviveu à spec A por TUDO ser opaco (o bg token passou a navy
#0B0E13 e o clear continuou #141414 para sempre; o dono via o navy das
barras mas o FUNDO por trás de tudo era o cinzento da F1 — e ninguém
via porque as barras cobriam-no). COM VIDRO o fundo por trás dos painéis
fica À VISTA: o clear passa a LER `theme::kTheme.bg`. Com o mono de
volta o VALOR é o mesmo #141414 — mas por CONTRATO, não por fóssil (a
mutação (d) prova: um literal diferente → a 13.9 FALHA no céu).

## 6 · O ÍCONE «G COM 4 SETAS» (e o gerador no repo)

O pedido: o G com as 4 setas. A entrega:

- **O GERADOR COMMITADO** — `scripts/gen_app_icon.py` (o antigo
  scripts-local/gen_app_icon.py NUNCA esteve no repo: os PNGs do «G com
  a lâmpada» eram ÓRFÃOS, irreproduzíveis — a dívida de processo morre
  aqui); vetor puro (scanline + setor polar), ZERO gradientes/blur — as
  regras do manifesto;
- **O DESENHO**: o G BRANCO #F5F5F5 (o accent mono; as proporções do G
  que o dono aprovou — anel com a abertura clássica no quadrante
  superior-direito e a barra a meio) + as 4 SETAS do Move em #B5B5B5
  (N/S/E/W apontando PARA FORA — o verbo primeiro do editor, o MESMO
  motivo do ícone Move do conjunto da casa, ui/Icons.h) sobre o bg mono
  #141414; legível no RMX3624 (o ícone corre a 144px: as setas ≈18px);
- **AS SAÍDAS**: o mestre 512 (gone_logo.png do header/cards +
  docs/icon-gone-512.png de referência) e as 5 mipmaps
  (48/72/96/144/192, ic_launcher==ic_launcher_round byte-a-byte como o
  antigo) por downsample de caixa;
- **O MANIFEST E O GATE**: o comentário do manifest conta o ícone novo
  e ONDE o gerador vive; o projects_ui_check apanha o rótulo «logo G
  com 4 setas».

## 7 · O WIPE DO FRAMEBUFFER DO STUB (o achado ao vivo)

O MAIOR achado do grupo — apanhado AO VIVO pela própria implementação
do vidro. A FASE 13.1 FALHOU ao vivo com a tabela nova: o pixel do
maior painel media (26,26,26) quando a matemática dizia (29,29,29). A
sonda por-pixel (instrumentação temporária no stub) deu a sequência
exata das escritas num pixel do céu:

```
[pix 760,100] clear  out=(0.078,…) dst=(-1,-1,-1)   ← o clear corre (20)
[pix 760,100] ui     out=(0.078,…) dst=(0,0,0)      ← o dst VIROU PRETO?!
```

O clear pintou #141414… e a primeira escrita da UI viu o dst PRETO. A
causa: **o allocFb_ do stub REALOCAVA (color_.assign(0)) a CADA
glViewport de tamanho diferente** — e desde o Grupo D o frame chama
glViewport DUAS VEZES com tamanhos diferentes (o 912×568 do pass 3D
scissored e o 1536×720 do beginUiPass): o primeiro APAGAVA o clear, o
segundo APAGAVA O PASS 3D INTEIRO. **TODOS os PNGs exportados desde a
0.9.6.7 mostravam a UI sobre PRETO** — invisível porque os painéis
eram OPACOS (o dst nunca aparecia) e NENHUMA check aferia conteúdo 3D
(o Grupo B validou o layout, não a cena). O vidro α0.88 pôs o dst à
vista e a mentira apareceu.

O FIX: o framebuffer é da **SUPERFÍCIE EGL** (o
eglCreateWindowSurface do stub empurra `fb::setSurface(eglstub::…)` —
realloc/wipe só quando o ecrã muda a sério, no TERM→INIT do harness) e
o `glViewport` SÓ regista a transformação (como no GL real; o bootstrap
por viewport fica para os testes que não criam superfície — a sentinela
regress_fb_rasteriza continua verde). O glstub_fb.h ficou LIVRE de
tipos GL para poder ser incluído do egl.h.

A PROVA nos píxeis (o PNG da 13.9): o céu da viewport é o clear
#141414 (era preto), o painel da hierarquia é (29,29,29) = o composto
do vidro EXATO, e a grelha/a cena VOLTOU aos exports.

## 8 · O ESPelho JAVA (a tela de projetos)

O Java espelha a tabela (a tela de projetos é PARTE da identidade):

- os tokens mono (ACCENT 0xFFF5F5F5 · ACCENT_PRESS 0xFFDADADA ·
  TEXT1/TEXT2 neutros · a rampa F1);
- **O ACCENT_INK NOVO** — o Button24 do «Novo projeto» pintava TEXT1
  branco sobre o fill ACCENT: com o accent mono era branco sobre
  branco INVISÍVEL (o mesmo bug latente do accentInk=text1 da spec A);
  agora a tinta do fill é ESCURA (16,9:1);
- **O VÉU** — SURFACE_GLA/SURFACE2_GLA (α0.88/0.92) nos cards e campos:
  a tela de projetos não tem cena 3D por trás, o vidro dela é o véu
  subtil sobre o BG — a MESMA matemática do nativo;
- **o tint do bigLogo REMOVIDO** — o setColorFilter(TEXT2) existia para
  neutralizar o azul+amarelo do ícone velho; o novo já é mono por
  construção;
- o gate projects_ui_check.py recalibrado (TOKENS_F, o fragmento
  ACCENT_INK, o rótulo «logo G com 4 setas»).

## 9 · A FASE 13.9 (a vara nova: o mono+vidro nos píxeis)

A FASE 13.9 (428→440 checks) afere a identidade no CAMINHO DO DEVICE (o
PNG relido pelo loadPng de produção + o registo — zero fórmulas de
layout que driftam):

1. **O MONO no PNG**: a paleta VELHA inteira (o azul #2196F3, o
   accentPress azul, a família navy, o text2 azulado) AUSENTE do ecrã
   exportado — NENHUM píxel (o azul do eixo Z do gizmo #4F92F5 é OUTRO
   azul: a exceção documentada, vive);
2. **O CLEAR LÊ O TOKEN**: o céu da viewport (entre os painéis, acima
   do horizonte da grelha) é o bg #141414 — não o preto do wipe;
3. **O VIDRO no painel**: o painel da hierarquia PELO REGISTO é o
   composto `blend(surface@0.88, bg)` = (29,29,29) EXATO (±1) — e NÃO
   a surface sólida (30): o pixel É a matemática do blend do device;
4. **O VIDRO flutuante**: o chip da toolstack sobre a viewport é o
   MESMO composto — a cena por trás aparece (o vidro REAL, não o véu
   lateral);
5. **o validador 0/0 re-vestido** — a pele nova não mexeu em NENHUM
   rect (a 13.6/13.7/13.8 re-provam o mesmo nas suas densidades);
6. **a dupla densidade**: o painel a 2.0 é o MESMO composto (as CORES
   não escalam com a densidade — o vidro é invariante).

RECALIBRADOS com nota no sítio: o 13.1 (o pixel do maior painel agora É
a matemática do vidro; a banda da toolbar — o piso 30% contava o bg
navy ≠ clear como «povoado» de graça: com o mono a banda pinta o TOKEN
bg que É o clear, e a população é o CONTEÚDO, ≈5,5% medidos → piso
novo 3%, o mesmo contrato «a banda não está vazia»).

## 10 · AS SENTINELAS E AS PROVAS DE MUTAÇÃO

Uma sentinela nova em `tests/test_sentinels.cpp` (842→843):

| sentinela | vigia |
|---|---|
| `regress_identidade_mono_vidro` | (1) a tabela mono a sério (os 9 tokens de chrome NEUTROS, ΔR=G=B≤2/255; o accent É o branco F1; accentPress mais escuro); (2) o vidro (os alphas 0.88/0.92/0.55/1.0 + os pisos NO PIOR CASO — contrastOnGlass); (3) a tinta escura sobre o fill branco; (4) a paleta V.ONI no bg novo (6 tokens ≥4,5:1); (5) o blendOver puro (a matemática do device: 50% cinza sobre branco = 0.75); (6) os aliases ligados bit-a-bit |

**Provas de mutação coladas** (`/mutacao-R028a/b/c/d/e-vermelho.txt`):

- **(a) o AZUL de volta** (accent→#2196F3 na tabela + no alias — a 1ª
  tentativa só na tabela MORREU no static_assert do UiContext.h: a
  ligação bit-a-bit funciona): a sentinela FALHOU no croma (ΔR−B=210)
  + o branco exato; `theme_tabela_spec_f_exata`, o contraste e o toast
  também — **11 testes** no total.
- **(b) o VIDRO morto** (surface α→1.0): a sentinela + a tabela FALHAM
  no alpha e **a 13.9 FALHA no PIXEL** (o painel desenharia 30 sólido —
  o translúcido é vigiado no píxel, não só na tabela).
- **(c) a TINTA CLARA de volta** (accentInk→branco): a sentinela FALHA
  em accentInk<0.5 E no contraste 1:1 — o branco-sobre-branco, o bug
  que o Java também tinha.
- **(d) o CLEAR LITERAL de volta** (0.05 hardcoded): a 13.9 FALHA no
  céu (o clear não segue o token) + o 13.1.
- **(e) o WIPE DO STUB de volta** (o realloc-por-viewport): **CINCO
  checks vermelhos** (o céu, o composto do painel, o chip flutuante, o
  2.0 e o 13.1) — o comportamento REAL do stub desde o Grupo D,
  apanhado pela primeira vez.
- Reposições → **843/0 + 440/440**.

O flake do /tmp (a lição da FASE 9) apareceu na mutação (a) — os
fixtures de 500MB do wiring010 acumularam até 98%; limpo → o wiring010
verde e a mutação vermelha nos SÍTIOS certos (documentado no próprio
ficheiro da mutação).

## 11 · RASTREABILIDADE (pedido → causa → fix → teste → harness)

| pedido do Grupo F | implementação | sentinela | FASE |
|---|---|---|---|
| tokens mono | a rampa F1 de volta na tabela única + os aliases ligados + o Java espelhado | regress_identidade_mono_vidro (1) | 13.9 (1) a paleta velha AUSENTE do PNG |
| tokens +vidro | os alphas 0.88/0.92/0.55 nas superfícies + a auditoria contrastOnGlass | (2)(5) | 13.9 (3)(4)(6) o composto EXATO no painel/chip/2.0 |
| a tinta certa | accentInk #141414 + o ACCENT_INK do Java | (3) | (o contraste 16,9:1 afervado na sentinela) |
| o clear do tema | o Renderer lê theme::kTheme.bg (o fóssil morreu) | (mutação d) | 13.9 (2) o céu = o token |
| ícone G com 4 setas | scripts/gen_app_icon.py NO REPO + o mestre + as mipmaps + o gone_logo | (o gate projects_ui apanha o rótulo; o gerador é a fonte) | (o ícone é ASSET da tela Java — o checklist item 1) |
| o wipe do stub (o achado) | fb::setSurface pela SUPERFÍCIE EGL; o glViewport nunca realoca | (mutação e) | 13.9 (2)(3)(4)(6) + o 13.1 |
| não partir | o validador 0/0 re-vestido; a 13.6/13.7/13.8 verdes; os gates verdes | (a suíte inteira) | 13.9 (5) |

## 12 · NÃO VERIFICADO (honestidade do fecho)

1. **O FEEL do vidro ao dedo/olho no device**: os alphas 0.88/0.92 e a
   hairline 0.55 são MATEMÁTICA afervada (os pisos seguram no pior
   caso), mas nenhum humano ainda viu o vidro no RMX3624 — se ao dono
   parecer SUBTIL demais (os painéis laterais ficam sobre o clear, um
   véu escuro) ou FORTE demais, os alphas são TRÊS literais na tabela
   (a sentinela acompanha — os pisos recalculam sozinhos).
2. **As juntas do frameRounded com vidro**: os arcos dos cantos vivem
   no line batch e os encontros aresta↔arco podem mostrar um ponto de
   duplo-blend a α0.55 — invisível no harness (o PNG da 13.9 não afere
   micro-juntas); o checklist item 3 pede o olhar do dono nos cantos
   dos chips.
3. **O ícone na gaveta de CADA launcher**: o PNG tem cantos
   arredondados r≈20% como o antigo, mas o RECORTE final é do launcher
   (alguns máscaras círculo/squircle podem comer as pontas das setas
   N/S — as setas vivem a ≥26px da borda no mestre 512; o checklist
   item 1).
4. **A tela de projetos no device**: o véu dos cards (SURFACE_GLA) e o
   «Novo projeto» branco-com-tinta-escura são verificados ESTRUTURAL-
   mente pelo gate (o CI não tem instrumentação Android) — o checklist
   item 2 é o olhar final do dono.

## 13 · O ESTADO DA SUÍTE NO FECHO

- **test_core**: 843 casos, 0 falhas (842 do Grupo E + 1 sentinela
  R-028; recalibrados COM NOTA: o theme_tabela_spec_a→f_exata, o
  theme_contraste_auditoria_spec com contrastOnGlass, o projects_ui
  TOKENS_F).
- **c33_virtual**: 440 checks, 0 falhas (428 do Grupo E + 12 novos:
  a 13.9 inteira; recalibrado o 13.1 com nota).
- **Gates locais**: check_main, link_parity (115 TUs), jni_parity (7
  natives), glyph_source, docs_lint, projects_ui_check — verdes.
- **A LINHA DE BASE NOVA** (para o Grupo G · ANIMATION): a identidade
  mono+vidro vive na tabela única (flip de 1 token cumprido à letra —
  a mutação (a) prova que mexer num token sem o alias MATA o build); o
  framebuffer do harness já não mente (o 3D está nos PNGs — o Grupo G
  herda PNGs HONESTOS para o drawer de animação); os alphas do vidro
  são literais vigiadas e o pior caso do vidro tem auditoria PRÓPRIA.
