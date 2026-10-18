# RELATORIO-0.9.6.7 — GRUPO D: ORÇAMENTO DO EDITOR 3D (FASE 0.9.6-MASTER)

> O nono mandamento da campanha: «VER, TESTAR, CORRIGIR, NÃO PARTIR». O
> Grupo D é a parte do ORÇAMENTO: a linha de base do Grupo C era 0/0 no
> HARNESS (1536dp de largura) — mas o DEVICE (RMX3624: 1600×720@2.0 =
> 776×336dp de conteúdo) estava PARTIDO por medidas FIXAS em dp. A entrega
> traz a GANGORRA dos três pisos (resolvePanels), os DIVISORES
> arrastáveis (o padrão da pega do drawer), a BARRA DE TOQUE adaptativa e
> o SCISSOR/ASPECT do rect — e a FASE 13.7 como a vara nova (o editor
> medido ao TAMANHO do device). Sentinelas: R-026 (docs/REGRESSOES.md).
> Suíte local no fecho: **core 841/0 + c33_virtual 408/0** + gates verdes.

## 1 · O QUE FOI PEDIDO

O rastreador (BACKLOG.md, lido no arranque pela P-01) define o Grupo D:
«topo/abas/FPS; hierarquia|viewport|inspector com divisores arrastáveis;
barra de toque; scissor». O diagnóstico (secção 2) mostrou que os quatro
itens são a MESMA história: o layout do editor nasceu num ecrã de 1512dp
e o device de 776dp não cabe nas medidas fixas.

O que NÃO estava no pedido (e não foi feito): nenhuma funcionalidade
nova além dos divisores (que são o pedido explícito), nenhuma mudança às
alturas da spec (top bar 56/tabs 48/status 24 — são spec D/E do autor,
medidas e confirmadas, não alteradas), nada fora do rastreador.

## 2 · O MÉTODO (o diagnóstico antes de mexer)

Leitura por ordem da P-01 (REGRESSOES → RELATORIO-0.9.6.6 → README →
BACKLOG) e depois o ESTUDO com a vara do Grupo B — com UMA NOVIDADE: a
medida tinha de ser ao TAMANHO do device. Os JSONs da FASE 13 (1536×720
@1.0) mostravam o editor LIMPO (0/0), mas a conta do device é outra:

| item medido/contado | harness (1512dp) | device (776dp) | veredicto |
|---|---|---|---|
| painéis laterais | 300+300 fixos, sobra 912 | 300+300 fixos, sobra **176** | viewport a 22% do ecrã |
| stack vertical (280dp) | cabe (viewport 568 alto) | viewport ~184 alto → **transborda** por cima da toolbar | sobreposição |
| toolbar+snap (272dp) + [+] (56) | 912 de largura dá tudo | 176 de largura → o [+] cai POR CIMA dos botões | sobreposição |
| drawer default (240dp) | viewport 568 → sobra 328 | viewport ~184 → **painéis a altura 0** | o editor desaparece |
| botões de ícone da top bar | 48dp inteiros | 47dp (a «escala graciosa» comprimia) | <48dp |
| pass 3D | proj(w/h) do ECRÃ + scissor ao centerRect | o MESMO corte | **o FOV horizontal cortado em QUALQUER ecrã** (~22% do FOV visível no device) |

O scissor (0.9.6.1 · G1-3) confinava o 3D ao rect — mas a câmara
continuava a projetar com o ASPECTO DO ECRÃ inteiro: o visível era o
RECORTE CENTRAL de um frustum largo. No device, com uma janela de 176dp
num ecrã de 800dp, o dono via 22% do FOV horizontal. É a mesma classe
dos painéis fixos: uma medida pensada para o ecrã do harness.

## 3 · A GANGORRA (resolvePanels — a fonte única das larguras)

O orçamento é uma GANGORRA de três (hierarquia | viewport | inspector)
com TRÊS pisos (ui/SafeArea.h):

- **kViewportMinW = 288dp** — a toolbar inferior (272dp + margens) é o
  piso: o 3D nunca fecha abaixo do que a barra de toque precisa;
- **kHierMinW = 200dp** — o piso da hierarquia (pesquisa/linhas usáveis);
- **kInspMinW = 272dp** — o piso do inspector (a linha X/Y/Z com caixas
  de 56dp: 184+8+48 = 240dp úteis + paddings).

Os DEFAULTS são ASSIMÉTRICOS no aperto: o INSPECTOR mantém o kPanelW
(300dp — a linha X/Y/Z é o conteúdo mais rígido) e a HIERARQUIA absorve
(os nomes truncam com o tip do long-press). No device:
**hier 200 | viewport 288 | insp 288** (776 no total). No harness largo:
300 | 912 | 300 — **PIXEL-IGUAL ao de sempre** (a 13.6 invariância
continua verde). O drag de cada divisor clampa contra a largura EFETIVA
do outro painel: o par nunca fecha o viewport (a sentinela varre
hw×iw ∈ [100..900]² e o piso aguenta em todos).

## 4 · OS DIVISORES ARRASTÁVEIS (o padrão da pega do drawer)

- **A pega**: strip VISUAL de 12dp na borda do painel + zona de toque de
  20dp (12 do strip + 8 de folga PARA DENTRO do painel — nunca para o
  lado do viewport: um toque na pega está SEMPRE fora do centerRect, não
  orbita nem agarra o gizmo). Não é alvo de TAP: não se regista no audit
  (a pega do drawer de 12dp é o precedente da spec E; o 48dp da casa é
  para tap).
- **O drag**: press arma (âncoras startX/baseW), o movimento horizontal
  redimensiona AO VIVO (passos de 8dp — o granular do drawer; o estado
  guarda o valor SNAPPED), release fixa. ACHADO AO VIVO da 13.7: o snap
  além do piso gerava um valor NEGATIVO que a resolvePanels lia como
  «default» (−1) — o painel SALTAVA para o default no meio do drag; o
  guard ≥0 fechou-o.
- **A ORDEM (o plano B)**: `dividerInput` corre ANTES dos painéis no UI
  pass — a pega chama `dragHandle` (novo no UiContext: captura como
  widgetHit, sem registo no audit) e RECLAMA `active_` primeiro; o
  `beginScroll` do painel vê `active_ != 0` e NÃO reclama (o toque na
  pega nunca vira scroll, mesmo com a região por baixo). O
  `drawPanelDividers` corre DEPOIS: o strip visível por cima da borda —
  o conteúdo do painel NÃO perde largura (a linha X/Y/Z continua a
  caber; um inset quebrava-a).
- **A persistência** (spec G): hierW/inspW no layout.json (o formato é
  retrocompatível: o ficheiro antigo sem as larguras → defaults; lixo
  fora da gama → defaults; o clamp VIVO por frame é a última defesa). A
  sombra do debounce acompanha a largura EFETIVA (o que o dono VÊ).

## 5 · A BARRA DE TOQUE CABE (o chrome adaptativo)

- **O stack** (undo/redo/save/dup/paste) em COLUNAS: 1 coluna nos ecrãs
  largos (o layout de sempre, zero mudança), 2/3 colunas nos curtos (a
  conta: o menor nº de colunas cujas linhas caibam na altura útil), e
  ESCONDE no sub-mínimo (viewport menor que a toolbar — a degradação
  honesta: toolbar+viewport mandam; os rects ficam degenerados e o draw
  salta). Todos os alvos continuam 48dp inteiros.
- **O [+]** sobe ao canto SUPERIOR direito quando a toolbar (272dp +
  margens) enche a largura da janela (nos largos fica no canto inferior
  direito, como sempre).
- **A linha X/Y/Z do Inspector**: caixas 64→56dp quando a largura útil
  não comporta a linha inteira (272dp; o alvo 48dp mantém-se pela
  ALTURA) e o botão R ao lado do TÍTULO como defesa final (painel
  sub-mínimo — nunca sobre a caixa Z).
- **A top bar**: os botões de ÍCONE (pause/play/gear) NUNCA encolhem —
  a «escala graciosa» comprimia-os a 47dp no device (achado da 13.7);
  o fator k divide só Menu/Cena/tabs pelo espaço que sobra DEPOIS dos
  ícones (a conta fecha exata — nada transborda a barra).
- **O convite do vazio** da hierarquia encurta em painel <240dp («toca
  em + para criar» — o comprido truncava a meio).
- **O DRAWER nunca come o editor**: o clamp do BottomPanel::layout deixa
  a faixa da toolbar do viewport viva (vpH−64dp). No harness o clamp
  histórico (400dp) continua a mandar (568−64 = 504); no device o
  default 240 comia os ~184dp INTEIROS — agora cede.

## 6 · O SCISSOR/ASPECT DO RECT (a câmara da janela)

- A câmara do EDITOR projeta com o **ASPECTO DO RECT** (o centerRect
  com as larguras dos divisores): a janela é o ecrã da câmara — o FOV
  inteiro dela cabe DENTRO (antes: o aspect do ecrã + o scissor = o
  recorte central de um frustum largo).
- O **viewport GL + o scissor** seguem o rect (o NDC do 3D mapeia dentro
  da janela); o `beginUiPass` repõe o viewport cheio para a UI (a
  fronteira 0.7.8 intocada — defesa em camadas).
- **Todo o mundo↔ecrã mapeia reto-local**: `projectPoint` ganhou (ox,oy)
  (o NDC mapeia para (vw×vh) local e soma a origem; defaults 0,0 = o
  ecrã todo — os testes e o Play ficam IGUAIS); o gizmo, os frustums
  das câmaras (drawAll/drawFrustum), os glifos de áudio e o
  `pickSceneTic`/tap-de-seleção/feedGizmo todos coerentes com o draw.
  O FRUSTUM da câmara de jogo mantém o ASPECTO DO JOGO (em Play a
  câmara renderiza o ecrã todo — o shape do frustum não muda).
- **A PROVA DO CORTE** (sentinela): um ponto na borda do frustum DA
  JANELA projeta DENTRO dela com o aspect do rect e FORA dela com o
  aspect do ecrã — o bug medido e o fix, na mesma asserção.

## 7 · A FASE 13.7 (a vara nova — o editor ao TAMANHO do device)

O harness corria sempre a 1536×720@1.0 (e a 13.6 a 3072×1440@2.0 = os
MESMOS dp) — o tamanho do device nunca foi medido. A 13.7 exporta o
editor a 1600×720@2.0 (insets ×2 = 776×336dp de conteúdo):

1. o PNG decodifica a 1600×720 (os ficheiros sobem como artefactos:
   layout-harness-editor-device.png/json);
2. o VALIDADOR inteiro VERDE (0 erros/0 avisos) — com os problemas NA
   MENSAGEM da check quando falha (o dono vê O QUE apontou);
3. a GANGORRA medida: hier 200dp (o piso), insp ~288dp, viewport ≥288dp
   — eram 176dp de viewport;
4. o CHROME adaptativo: o stack em colunas, o [+] no canto sup-dir,
   Todos os alvos ≥48dp reais e DENTRO do rect;
5. os DIVISORES ao VIVO: press na pega arma, o drag encolhe o inspector
   (o viewport CRESCE), os pisos seguram, o teto da gangorra devolve, o
   release fixa — pelo caminho real do input (injectDown/Move/Up);
6. a PERSISTÊNCIA: o layout.json leva hierW/inspW, o round-trip volta, o
   formato antigo → defaults.

## 8 · OS ACHADOS AO VIVO (o loop da campanha)

1. **O snap negativo virava default** (secção 4) — a 13.7 apanhou no 1º
   run: o drag além do piso fazia o painel SALTAR para a largura default.
2. **Os botões de ícone a 47dp** — a «escala graciosa» da top bar
   comprimia TUDO proporcionalmente; no device os três mediam 47dp. O
   piso 48dp agora é absoluto para eles.
3. **O convite do vazio truncado** — 353px de texto num painel de
   400px@2.0; o convite curto em painel estreito.
4. **O /tmp encheu** (9.1G/98% — os 500MB do wiring010 acumularam 5
   cópias entre runs): a lição da FASE 9 de novo; limpo → verde por
   inteiro.

## 9 · SENTINELAS E PROVAS DE MUTAÇÃO (R-026)

Três sentinelas novas em `tests/test_sentinels.cpp` (838→841):

| sentinela | vigia |
|---|---|
| `regress_orcamento_gangorra` | os defaults assimétricos (harness IGUAL, device absorve), a varredura do par de drags (o viewport NUNCA fecha; os pisos aguentam em [100..900]²), o negativo ≠ default, o clamp do drawer (o device cede, o harness não) |
| `regress_divisores_arrastaveis` | as pegas armam/arrastam/clampam/release (edges por frame — o clearEdges é a fronteira), o snap 8dp, a persistência (round-trip + formato antigo + lixo), o chrome adaptativo (colunas, [+] topo, ≥48dp dentro, a degradação honesta) |
| `regress_scissor_aspecto_rect` | o NDC mapeia ao rect+origem (o centro da vista cai no CENTRO DO RECT), a prova do corte (a borda do frustum da janela: dentro com o aspect do rect, fora com o do ecrã), o pick reto-local coerente com o draw |

**Provas de mutação coladas** (`/mutacao-R026a/b/c-vermelho.txt`):
- **(a) o piso do viewport desligado** (vpMin=0): `regress_orcamento_gangorra`
  FALHOU (a hierarquia default não absorve; o viewport do device fecha; a
  varredura aponta) e **656 testes** no total — o piso é LOAD-BEARING em
  todo o layout do editor (a prova mais forte possível da gangorra).
- **(b) o drag dos divisores morto**: `regress_divisores_arrastaveis`
  FALHOU no resize vivo (a pega arma mas o painel não mexe).
- **(c) o projectPoint cego a (ox,oy)**: `regress_scissor_aspecto_rect`
  FALHOU (o ponto do centro da vista deixa de cair no centro do rect).
- Reposições → **841/0 + 408/0**.

## 10 · RASTREABILIDADE (pedido → causa → fix → teste → harness)

| pedido do Grupo D | implementação | sentinela | FASE 13.7 |
|---|---|---|---|
| topo/abas/FPS | os botões de ícone nunca <48dp (o k divide os outros); as alturas da spec medidas e confirmadas (não alteradas — spec do autor) | regress_orcamento_gangorra (o drawer) + a 13.7 (o validador) | o validador 0/0 ao tamanho do device |
| divisores arrastáveis | resolvePanels + pegas 12/20dp + dragHandle + snap 8dp + persistência | regress_divisores_arrastaveis | o drag AO VIVO (clamps + release) e o round-trip |
| barra de toque | o stack em colunas, o [+] relocável, a linha X/Y/Z adaptativa, o drawer com teto vivo | regress_divisores_arrastaveis (o chrome) | todos os alvos ≥48dp DENTRO do rect |
| scissor | o aspect do rect + glViewport/scissor do rect + o mapeamento reto-local em todo o mundo↔ecrã | regress_scissor_aspecto_rect | (a sentinela é a prova; o PNG mostra a janela) |

## 11 · NÃO VERIFICADO (honestidade do fecho)

1. **O DEVICE continua sendo a vara final**: a 13.7 prova o orçamento
   no HARNESS ao tamanho e à densidade do RMX3624; o Mali real pode
   divergir em pormenor — o checklist do README (9 itens) pede a
   «Auditoria do ecrã» no aparelho com as contagens COLADAS no próximo
   relatório.
2. **O scissor no GPU real**: o glstub rasteriza o viewport GL (a
   `regress_fb_rasteriza` afere o scissor no stub); o comportamento do
   Mali com viewport+scissor do rect é o padrão GL ES 3.0 (a fronteira
   beginUiPass repõe o estado) — o checklist pede o olhar do dono na
   janela 3D (item 5).
3. **Os divisores ao dedo**: a 13.7 arrasta pelo caminho REAL do input
   do harness (injectDown/Move/Up nos slots) — mas o «feel» do dedo
   (a pega de 20dp agarrada com o dedo real) só o dono afirma; o
   checklist item 2.
4. **A degradação sub-mínima** (o stack ESCONDE em viewport menor que a
   toolbar) é afervada pela sentinela mas NINGUÉM a viu desenhada (o
   device com o drawer aberto ao máximo é o caso real — checklist item
   8).

## 12 · O ESTADO DA SUÍTE NO FECHO

- **test_core**: 841 casos, 0 falhas (838 do Grupo C + 3 sentinelas
  R-026).
- **c33_virtual**: 408 checks, 0 falhas (386 do Grupo C + 22 novos da
  FASE 13.7).
- **Gates locais**: check_main, link_parity (115 TUs), jni_parity,
  glyph_source, docs_lint — verdes.
- **A LINHA DE BASE NOVA** (para o Grupo E): o editor passa o validador
  0/0 ao TAMANHO do device E no harness largo (pixel-igual ao de
  sempre); o Grupo E herda o editor de script com o teclado da engine
  (o insumo medido na FASE 13 do Grupo B) e a casa limpa nos dois
  orçamentos.
