# RELATORIO-0.9.6.8 — GRUPO E: EDITOR DE SCRIPT + SÍMBOLOS (FASE 0.9.6-MASTER)

> O nono mandamento da campanha: «VER, TESTAR, CORRIGIR, NÃO PARTIR». O
> Grupo E é a SUBSTITUIÇÃO pedida pelo rastreador: «header flexível; IME;
> barra de símbolos 40dp sobre o IME (teclado da engine REMOVIDO);
> R-018/R-010». A entrega traz a PONTE DO INSET do IME (a engine aprende
> ONDE o teclado do Android está), a BARRA DE SÍMBOLOS de 40dp com os 22
> símbolos da spec em páginas adaptativas, o TECLADO DA ENGINE removido
> (280dp/54 teclas — o IME do sistema é o teclado único), o HEADER
> FLEXÍVEL (dois botões viviam FORA DO ECRÃ no device desde a 0.9.6) e a
> exceção compacta do validador (a spec 40dp vs a casa 48dp — vigiada).
> Sentinela: R-027 (docs/REGRESSOES.md).
> Suíte local no fecho: **core 842/0 + c33_virtual 428/0** + gates verdes.

## 1 · O QUE FOI PEDIDO

O rastreador (BACKLOG.md, lido no arranque pela P-01) define o Grupo E:
«header flexível; IME; barra de símbolos 40dp sobre o IME (teclado da
engine REMOVIDO); R-018/R-010». São TRÊS entregas e uma vigília:

1. o teclado da engine (o QWERTY in-app de 280dp/54 teclas) SAI — o dono
   digita pelo IME do sistema (acentos, gestos, dicionário do GBoard);
2. a barra de símbolos de 40dp desenha SOBRE o IME (a spec E) — os
   símbolos da linguagem a um toque;
3. o header do editor de script fica FLEXÍVEL (a palavra do rastreador);
4. as R-018 (a escala dp) e R-010 (o render nunca mente) continuam
   vigiadas no que muda.

O que NÃO estava no pedido (e não foi feito): nenhuma funcionalidade de
edição nova (o completamento/autossave continuam fora), nenhuma mudança à
linguagem V.ONI, nenhum tema (é o Grupo F), nada fora do rastreador.

## 2 · O MÉTODO (o diagnóstico antes de mexer)

Leitura por ordem da P-01 (REGRESSOES → RELATORIO-0.9.6.7 → README →
BACKLOG) e depois a leitura de código com a pergunta: «o que acontece no
DEVICE quando o dono abre um script?». A medida:

| item medido/contado | estado ANTES | veredicto |
|---|---|---|
| onde está o IME? | a engine NÃO SABIA (o manifest sem adjustResize; o teclado SOBREPÕE a superfície; nenhum caminho media a faixa) | o caret ficava POR BAIXO do teclado |
| o teclado da engine | 280dp/54 teclas + a «política de coexistência» G3 (dois teclados alternativos) | custo duplicado; a spec E manda REMOVER |
| o header no device (360dp) | helpX = docsX−200 = **−56dp**; kbX = docsX−152 = **−8dp** | DOIS botões FORA DO ECRÃ desde a 0.9.6 (invisíveis E intocáveis) |
| o título no device | a zona do título media 80dp (cabe) | ok no harness, o header inteiro não cabia |
| os símbolos da spec | a página «123» do teclado antigo: { } ( ) [ ] = + - * / < > ! , . ; : " _ # @ = **22** símbolos | o set exato que a barra herda (os extras $ % & \| ~ ^ \\ ' vivem no GBoard) |

O ACHADO do header é a mesma família do R-026: medidas pensadas para o
ecrã do harness (720px de largura) que nunca viram os 360dp do device. A
diferença: aqui eram BOTÕES INTEIROS fora do ecrã — o dono perdia o nível
da ajuda e o toggle do teclado há TRÊS versões sem ninguém notar (só a
conta do contentRect os via; o validador do Grupo B nunca os apanhou
porque o que está FORA do ecrã nem precisa de sobrepor — e a regra
ForaDoEcra do validador SÓ olha o contentRect, que no harness largo os
continha a todos).

## 3 · A PONTE DO INSET DO IME (a engine aprende onde está o teclado)

- **A MEDIDA (VvActivity.java)**: um listener de layout global no root
  view calcula `rootHeight − visibleDisplayFrame.bottom` — a técnica
  clássica que funciona do API 24 ao 34 (sem WindowInsets.Type, sem
  ajuste de manifesto). O PISO `rootHeight/7` separa a nav bar/gesture
  bar (que também encolhe o frame visível) de um IME REAL: abaixo de
  ~14% da altura não é teclado, é barra de sistema — inset 0.
- **O CAMINHO (JNI novo)**: `nativeOnImeInset(int px)` registado na
  tabela RegisterNatives (a paridade jni_parity verde; os testes de
  handshake recalibrados 6→7 natives COM a expect do novo) →
  `ime::setBottomInset` (o MESMO módulo/mutex da fila de texto — o
  contrato do IME num só sítio; log UMA linha por mudança: «ime: aberto
  (inset 880 px)»; o listener só empurra MUDANÇAS — o layout dispara
  muito).
- **O CONSUMO (ScriptEditor.cpp)**: `bottomLim = imeIns > 0 ? h−imeIns :
  h−ins.bottom` — o inset JÁ inclui a faixa da nav bar quando o teclado
  está aberto (a medida é a MESMA referência), não se soma duas vezes.
  Com o IME FECHADO o limite é o de sempre: o layout é PIXEL-IGUAL ao do
  Grupo D (a invariância preservada).

## 4 · A BARRA DE SÍMBOLOS (40dp — a spec E)

- **A FAIXA**: exatamente 40dp (a spec do autor; a R-018 recalibrada —
  o `keyboardHeight()` morreu com o teclado, a vara afere
  `symbolBarHeight()`), dokada em `h − inset − 40dp`: a barra beija o
  topo do teclado REAL.
- **AS TECLAS**: os 22 símbolos da spec — { } ( ) [ ] = + - * / < > ! ,
  . ; : " _ # @ — o set EXATO da página «123» do teclado antigo (os
  extras $ % & \| ~ ^ \\ ' ficaram fora: o GBoard os tem na própria
  página de símbolos). Largura ≥40dp SEMPRE: 9 teclas no device 360dp,
  14 no harness 720dp (o teto evita «baías» nos largos).
- **AS PÁGINAS**: adaptativas — a 1ª tecla é o SELETOR «1/3»/«1/2» (o
  padrão ?123 do GBoard, à esquerda onde o polegar mora); o device tem 3
  páginas (22/8), o harness 2 (22/13). O toque cicla e LOGA («editor:
  barra de símbolos -> pagina 2/3»).
- **A EMISSÃO**: pelo MESMO `applyEvent` do IME (a fonte única do
  teclado antigo, mantida) — o símbolo entra NO CARET (o contrato da
  R-019) e LOGA por tecla («editor: símbolo '{' pela barra (pagina 1)»).
- **A PRESENÇA**: a barra EXISTE quando o IME está aberto (inset > 0) e
  SOME com ele — é o par abrir/fechar do teclado; o registo do layout
  confirma (a FASE 12.7 afere o desaparecer).

## 5 · O TECLADO DA ENGINE REMOVIDO (a lista do que saiu)

`drawKeyboard` (280dp: 4 filas + a linha de baixo), `kKbLetters` (o
QWERTY), `kKbSymbols` (a página 123), o `longVariant` (os acentos por
long-press), o shift Aa, as setas, o TAB/ESPACO/APAGA/ENTER/FECHAR, o
estado `kbOpen/kbSym/kbLower/kbLongId/kbLongT/kbLongFired`, o botão
`kKbToggleId` do header e o result 7 (o ramo do main fica documentado
como obsoleto — o contrato de resultados é afervável). O teclado do
UIEditor/renomear (kKbBase 6600, outra faixa, outro propósito) NÃO foi
tocado — é o teclado de renomear rápido em landscape, fora do âmbito E.

O que o dono GANHA: um teclado só (o GBoard: acentos, gestos, dicionário,
deslizante), os símbolos da linguagem a UM toque na barra, e o ecrã de
código com mais 240dp de altura quando o teclado próprio estaria aberto.

## 6 · O HEADER FLEXÍVEL (os dois botões invisíveis)

- **A ÂNCORA**: os botões medem-se DA DIREITA (Stop, Run, lupa,
  copiar-referência, nível — nesta ordem, da borda para dentro); o
  título FLEXIONA com o que sobra: o subtítulo some quando a zona fica
  <120dp, o título quando fica <48dp.
- **O APERTO**: nos ecrãs <420dp de conteúdo, Run/Stop estreitam de 72dp
  para 48dp de LARGURA (os rótulos cabem; os alvos ≥48dp mantêm-se pela
  altura — o padrão da top bar do Grupo D: o texto divide, os alvos
  nunca encolhem).
- **O DEVICE (360dp)**: Back[0..56] + nível(76) + copiar(132) + lupa(188)
  + Run(248) + Stop(304→352) — ZERO sobreposição, ZERO fora do ecrã, e o
  título SOME (a zona media 4dp). O header é só botões no aperto — a
  FASE 13.8 afere tudo (o validador INTEIRO 0/0 ao tamanho do device).
- **O HARNESS (720dp)**: stop 640, run 560, lupa 500, copiar 444, nível
  388, título [64..380] — o layout de sempre com o título no sítio (as
  posições mudaram 4–8px porque a âncora mudou; os testes recalibrados).

## 7 · A EXCEÇÃO COMPACTA DO VALIDADOR (a spec 40dp vs a casa 48dp)

O rastreador manda 40dp na barra; a casa manda 48dp de toque. A
resolução pela POLÍTICA #5 («mudança legítima de comportamento com a
mudança explicada no relatório e o autor aprova»):

- `Entry.compact` (o JSON diz «compacto») + `buttonCompact()` no
  UiContext — a flag vive SÓ durante a chamada, o label do texto de
  dentro NÃO herda;
- o ToquePequeno aplica o piso 40dp às compactas e 48dp às regulares; a
  MENSAGEM diz o piso que falhou («< 40dp compacto» vs «< 48dp»);
- a exceção é ESTREITA e PROVADA: a sentinela R-027 afere que uma
  compacta de 40dp PASSA, uma de 39dp FALHA, um botão REGULAR de 40dp
  FALHA e um de 48dp passa — o piso compacto é vigiado (a mutação (b)
  desligou-o: a sentinela ficou vermelha NA asserção certa).

O precedente do Grupo D está documentado («a pega do drawer de 12dp é o
precedente da spec E»): a spec do autor cria exceções MEDIDAS, e cada
exceção ganha uma sentinela que a vigia.

## 8 · A FASE 13.8 (a vara nova — o script ao tamanho do device)

O harness corria o script sempre a 720×1536@1.0 (e a 13.6 a duplicar a
densidade no ECRÃ LARGO) — o TAMANHO do device portrait nunca foi medido.
A 13.8 exporta o editor de script a 720×1600@2.0 (= 360×800dp, insets
T48/B48 px = 24dp, o IME a 880px = 440dp — o GBoard real):

1. o PNG decodifica a 720×1600 (os ficheiros sobem como artefactos:
   layout-harness-script-device.png/json);
2. o VALIDADOR INTEIRO verde (0 erros/0 avisos) — com os problemas NA
   MENSAGEM quando falha;
3. o HEADER no aperto: ≥5 alvos de 48dp inteiros (96px @2.0), NADA fora
   do ecrã (o antigo media −56dp), o título SOME (a zona <48dp);
4. a BARRA no device: 9 teclas (360/40), dokada em h−880−80px, TODAS
   compactas no JSON, 3 páginas (22/8);
5. o 13.3 REESCRITO (o harness largo com o IME injetado a 280px): o
   validador 0/0, as 14 teclas compactas, e o TECLADO ANTIGO AUSENTE do
   registo (a faixa de IDs morta verificada);
6. o 12.7 REESCRITO: as 2 páginas cobrem os 22 símbolos (a união é o set
   inteiro), o seletor cicla, os LOGs existem, o IME fechado APAGA a barra.

## 9 · OS ACHADOS AO VIVO (o loop da campanha)

1. **A contagem dos símbolos**: o rascunho da barra declarava
   `kSymbols[24]` com 22 inicializadores — os 2 NULLs crashavam a
   `std::string` na sentinela ANTES de qualquer teste de layout. A
   contagem da spec ({ } ( ) [ ] = + - * / < > ! , . ; : " _ # @) é 22,
   e a correção atravessou o .cpp, o header, a sentinela, o 12.7 e o
   13.8 (a lição: o array externo aferva-se pelo PRIMEIRO e ÚLTIMO
   elemento, e o crash foi a prova).
2. **O export escreve pelo currentScreenName()**: a 13.8 nova pedia
   «script-device» e lia um ficheiro que ninguém escreveu — o PNG que
   decodificava era o STALE da 13.3 (720×1536) e o registo dizia
   «editor». O padrão da 13.7 («as cópias do device vão para artefactos
   próprios») aplicado; e o TIC do script tem de nascer DEPOIS do
   INIT_WINDOW (o reload do lifecycle re-cria a cena do último save — o
   «Ator» da 13.3 vivia só em memória).
3. **O /tmp voltou a encher** (98% — 9,1G) a meio das mutações: o flake
   do wiring010 apareceu de novo (a lição da FASE 9, terceira vez na
   campanha); limpo → verde por inteiro nas reposições.
4. **Os labels do código na janela do header**: o primeiro check do
   «título flexiona» apanhava os labels do CÓDIGO (y≈153px) e os rótulos
   do Run/Stop (x=521/630) — a janela da asserção teve de ser a zona
   REAL do título (y<150 E x<152): a lição do «a check tem de provar o
   que DIZ que prova» (a R-014 aplicada às próprias checks).

## 10 · SENTINELAS E PROVAS DE MUTAÇÃO (R-027)

Uma sentinela nova em `tests/test_sentinels.cpp` (841→842):

| sentinela | vigia |
|---|---|
| `regress_barra_simbolos_ime` | (1) a ponte do inset (set/get/insetVisible/clearForTest); (2) a geometria (40dp exatos, 9/14 teclas, 3/2 páginas, os 22 símbolos, a densidade 2.0 dobra); (3) o DRAW REAL auditado (a barra dokada sobre o IME, as teclas compactas, o validador 0, a tecla insere NO CARET, o seletor cicla, IME fechado = sem barra); (4) a exceção ESTREITA (40dp compacta passa / 39dp FALHA / regular 40dp FALHA / regular 48dp passa); (5) o header ao tamanho do device (≥5 alvos 48dp DENTRO, nada fora, a barra 9 teclas) |

**Provas de mutação coladas** (`/mutacao-R027a/b/c/d-vermelho.txt`):
- **(a) a tecla MORTA** (o applyEvent desligado): a R-027 FALHOU em 4
  asserções + o wiring092 — o par desenhar≠tocar vigiado.
- **(b) o piso compacto morto** (40→30): a R-027 FALHOU na tecla de 39dp
  — a exceção da spec é VIGIADA, não é desculpa.
- **(c) o corpo cego ao inset**: a R-027 FALHOU na posição + 8 checks do
  c33 — a tecla desenhada DEBAIXO do teclado é INTOCÁVEL (o tap cai no
  corpo): a lição do R-014 aplicada ao IME.
- **(d) o header flexível morto** (narrow=false): a R-027 FALHOU no
  validador do device (ForaDoEcra) + a 13.8 — o defeito exato dos dois
  botões invisíveis, apanhado ao vivo.
- Reposições → **842/0 + 428/428**.

## 11 · RASTREABILIDADE (pedido → causa → fix → teste → harness)

| pedido do Grupo E | implementação | sentinela | FASE |
|---|---|---|---|
| IME | a ponte do inset (VvActivity mede → JNI → ime::bottomInset; o corpo reserva; o caret nunca escondido) | regress_barra_simbolos_ime (1)(3) | 12.3 (a barra dokada no sítio), 13.8 (o orçamento do device) |
| barra de símbolos 40dp | drawSymbolBar: 22 símbolos da spec, páginas adaptativas, o seletor ?123, applyEvent o MESMO do IME, LOG por tecla | regress_barra_simbolos_ime (2)(3) | 9.x (a tecla no CARET), 12.7 (as páginas/LOGs/sumir), 13.3 (as compactas no JSON) |
| teclado da engine REMOVIDO | drawKeyboard + tabelas + estado + botão + result 7 fora (o ramo 7 documentado como obsoleto) | (a compilação é a prova: keyboardHeight/kbOpen não existem) | 13.3 (a faixa de IDs morta AUSENTE do registo) |
| header flexível | os botões ancoram à direita; o título flexiona; Run/Stop 48dp no aperto | regress_barra_simbolos_ime (5) | 13.8 (≥5 alvos 48dp DENTRO, o título SOME, validador 0/0) |
| R-018/R-010 | a barra em dp REAL (40dp→80px @2.0); os símbolos pelo applyEvent (o render das peças intacto — a R-010 não mudou) | a R-018 recalibrada (symbolBarHeight); a R-010 recalibrada (o draw com o IME injetado) | 12.10 (a vara da densidade na barra) |

## 12 · NÃO VERIFICADO (honestidade do fecho)

1. **A MEDIDA REAL do GBoard no RMX3624**: a 13.8 injeta 880px (440dp —
   o valor típico de um GBoard em portrait); o inset que o device MEDIR
   pode ser outro (o floating keyboard, o teclado compacto, o One-Hand
   Mode mudam a faixa) — a ponte mede o que for; o checklist item 2/3
   pede o olhar do dono com o SEU teclado.
2. **O feel da barra ao dedo**: as teclas de 40dp são compactas POR SPEC
   (a casa manda 48 para os alvos primários; a barra é uma strip de
   símbolos — o dedo do dono é a vara final): se ao usar o dono achar as
   teclas pequenas DEMAIS, a spec pode subir para 44/48dp com UMA
   constante (a sentinela acompanha — o piso compacto é a MESMA
   constante). O checklist item 2.
3. **A barra sobre OUTROS IMEs**: a técnica do visible-frame cobre o
   GBoard e os teclados da Samsung/SwiftKey (todos encolhem o frame
   visível); um teclado FLUTUANTE (que não ancora no fundo) mede inset
   MENOR que a posição real das teclas dele — a barra dokaria acima do
   espaço vazio. O comportamento é o degradado honesto (a barra nunca
   fica POR BAIXO do teclado); o checklist item 2 com o teclado por
   omissão do dono.
4. **A audiência das 3 páginas no device**: a matemática cobre os 22
   símbolos em 3 páginas (8+8+6) e a FASE afere a união; nenhum humano
   ainda opinou se ciclar 2 vezes para chegar ao «@» é natural — se o
   dono preferir, a ordem dos símbolos por página é UMA tabela (a
   primeira página pode levar os mais usados: { } ( ) = < > ; o ajuste
   é de GOSTO, não de mecanismo).

## 13 · O ESTADO DA SUÍTE NO FECHO

- **test_core**: 842 casos, 0 falhas (841 do Grupo D + 1 sentinela
  R-027; recalibrados COM NOTA: a R-018/R-010 da contagem do teclado, os
  wiring092 das posições do header flexível, os handshake 6→7 natives).
- **c33_virtual**: 428 checks, 0 falhas (408 do Grupo D + 20 novos:
  12.3/12.7 reescritos, 13.3 reescrito, 13.8 inteiro).
- **Gates locais**: check_main, link_parity (115 TUs), jni_parity (com o
  nativeOnImeInset), glyph_source, docs_lint — verdes.
- **A LINHA DE BASE NOVA** (para o Grupo F · IDENTIDADE): o editor de
  script passa o validador 0/0 ao tamanho do device (360×800dp) E no
  harness largo (pixel-igual ao de sempre com o IME fechado); o teclado
  da engine não existe mais; o Grupo F herda o tema mono+vidro com a
  casa limpa nos dois orçamentos e a barra/símbolos como widgets novos
  para vestir.
