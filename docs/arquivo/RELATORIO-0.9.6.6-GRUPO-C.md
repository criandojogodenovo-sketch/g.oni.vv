# RELATORIO-0.9.6.6 — GRUPO C: ESCALA E TIPOGRAFIA (FASE 0.9.6-MASTER)

> O nono mandamento da campanha: «VER, TESTAR, CORRIGIR, NÃO PARTIR». O
> Grupo C é a parte da ESCALA: a linha de base que o Grupo B MEDIU
> (RELATORIO-0.9.6.5 secção 8) morreu toda na mesma classe — o texto não
> tinha escala de densidade e meia dúzia de alvos eram pixels crus. A
> entrega fecha o círculo aberto pela R-018: **dp() para o layout E sp()
> para o texto** — e a prova é a invariância (o ecrã a 2.0 é o ecrã a 1.0
> visto a 2×, FASE 13.6). Sentinelas: R-025 (docs/REGRESSOES.md). Suíte
> local no fecho: **core 838/0 + c33_virtual 386/0** + gates verdes.

## 1 · O QUE FOI PEDIDO

O rastreador (BACKLOG.md, lido no arranque pela P-01) define o Grupo C:
«dp()/sp() únicas; linha→y/col→x única no editor; perf do editor; cantos
suavizados». A motivação com números: a linha de base medida pelo Grupo B
— o editor com 1 ERRO (a label que sangrava 3px o fundo) e 3 avisos, o
browser com 8 avisos de toque < 48dp, o script com 2 truncadas — e o
NÃO VERIFICADO #4 do relatório B («nenhum export de densidade 2.0 foi
corrido no harness — o Grupo C traz a dupla densidade»).

O que NÃO estava no pedido (e não foi feito): nenhuma funcionalidade
nova, nenhum re-arranjo de ecrã além dos alvos/medidas medidos, nada fora
do rastreador.

## 2 · O MÉTODO (o diagnóstico antes de mexer)

Leitura por ordem da P-01 (REGRESSOES → RELATORIO-0.9.6.5 → README →
BACKLOG) e depois o ESTUDO das superfícies com a vara do Grupo B: os
JSONs exportados pela FASE 13 foram RELIDOS entrada a entrada e cada
aviso/erro mapeado à LINHA de código que o desenha (a tabela abaixo). O
mapeamento fechou SEM sobras: TODOS os 18 problemas medidos (incluídos os
2 que o relatório B não contou — a errata da secção 3) têm a mesma classe
raiz.

| problema medido | quem desenha | causa |
|---|---|---|
| editor ERRO label [8,694 166×29] sangra 3px | BottomPanel::drawStatusBar | label de CORPO (bloco 29px do atlas cru) numa banda de 24dp; o comentário dizia «12sp (fontScale 12/14)» e o código chamava `ui.label` — 14sp |
| editor aviso botão(id 28=0x28=40=kIdPlus) 56×40 | EditorUi hierarquia | 56/40 px crus (o comentário dizia «[+] 48dp») |
| editor aviso botão(id 157c=5500=kHierSearchId) 268×40 | EditorUi pesquisa | sfield 40px crus (kSearchRowH=48dp existia) |
| editor aviso label truncada 353px («sem TICs - toca em + para criar») | EditorUi vazio | o texto a 28px crus não cabia em 268px |
| script ERRO título [64,92.1 64×41.4] sangra o TOPO (ERRATA B) | ScriptEditor cabeçalho | o bloco 20sp (41px) não cabia no header de 56dp → fallback de baselines FIXAS (29/51) → 3,9px acima do contentRect |
| script 2 truncadas «ESPACO»(104>92)/«ENTER»(85>60) | drawKeyboard | `ui.button` desenha o rótulo a 14sp; as teclas pedem 12sp (o TAB já desenhava à mão a 12sp) |
| browser fechar 96×36 · 6 raízes 139,7×40 · subir 868×44 | UiEditor::drawFileBrowser | 36/40/44 px crus — o browser inteiro fora do dp() |
| docs ERRO título [64,20.1 57×41.4] sangra o TOPO (ERRATA B) | DocsScreen cabeçalho | a mesma classe do título do script |
| settings actionRows 152×40 («decisão do Grupo C») | SettingsPage actionBtnRect | y+4/kRowH−8 px crus |

## 3 · A ERRATA do relatório B (honestidade primeiro)

A tabela do RELATORIO-0.9.6.5 secção 8 dizia «docs 54 LIMPO» e «script
(…) 0 erros». Os JSONs que o próprio Grupo B exportou mostram MAIS: o
título 20sp do Docs sangrava 3,9px o TOPO do contentRect
([64,20.1 57.1×41.4] num contentRect que começa em y=24) e o do Script
idem ([64,92.1 64.1×41.4] num contentRect que começa em 96) — ambos
ERROS `texto_sangra` que o validador teria apontado se a auditoria
corresse nesses ecrãs no harness (a 13.2 só auditou o editor; a contagem
do docs/script na secção 8 foi feita à mão e não viu os sangramentos de
TOPO). Este grupo corrige a contagem E os defeitos — ambos morreram com o
sp() (o bloco 20ps passa a caber no cabeçalho e as baselines REAIS
centram-no; o fallback de px fixos deixa de disparar a 1.0).

## 4 · sp(): A ESCALA ÚNICA DO TEXTO (o choke point)

O contrato novo (ui/Theme.h):

- **`sp(v) = v × densidade`** — os px de um texto de v sp (a definição
  Android; 14sp = 14px a 1.0, 28px a 2.0).
- **`textK() = densidade/2`** — o fator do ATLAS da casa (assado a 28px,
  que É o corpo a 2.0 do C33). O UiContext multiplica por ele no CHOKE
  POINT: `fontWidth`/`fontHeight`/`textMetrics`/`labelStyled`/
  `labelFitted` — UM só dono para a escala do texto; os chamadores
  continuam a passar `fontScale(sp)` RELATIVO (sp/14) como sempre.
- A 2.0 o k é 1.0 — o device fica PIXEL-IGUAL ao de sempre (a vara de
  medir é o device: zero mudança onde o dono olha). A 1.0 o corpo são
  14px — o harness deixa de ser um ecrã de texto 2× desproporcional.

Com o textK, os DOIS sangramentos de título curaram-se sozinhos (o bloco
título+subtítulo passa a 34px ≤ 56 e as baselines das MÉTRICAS REAIS
centram-no — o fallback `kHeaderTitleBase` deixa de disparar), a legenda
da status bar a 12sp REAL (4px de folga na banda de 24) e as duas
truncadas do teclado («ESPACO»/«ENTER» agora desenhadas à mão a 12sp como
o TAB — o `ui.button` de corpo truncava-as). E o `button()` deixou de
medir pelo ATLAS CRU (`font_->widthOf` — media uma largura e desenhava
outra no viewport 2D): mede pelo CONTEXTO.

**`labelFittedStyled`** — o FIT com tipografia (nunca excede maxW,
trunca com «…», mas a 12sp quando o texto é legenda): a status bar usa-o.

## 5 · OS ALVOS 48dp E O dp() QUE FALTAVA

O R-018 cobrira as FONTES ÚNICAS de layout (SafeArea/EditorLayout/
componentes); este grupo varreu o que ficou: o [+] e a pesquisa da
hierarquia (48dp reais — os avisos id 0x28/0x157C), o browser INTEIRO
(fechar 96×48dp, 6 raízes 48dp, subir 48dp, painel 900dp, caminho/raízes
em dp), as actionRows do Settings (o botão à altura da LINHA inteira —
48dp; `actionBtnRect` EXPORTADA no header: o draw, o walk do tap e os
testes partilham-na), o cancelar do import, as caixas X/Y/Z e o R do
TransformRow (64×48dp + trilho 84/118dp), os ícones/zonas/recuos da
hierarquia, o kErrH/kHelpStrip* do editor de script, o Settings inteiro
(kRowH/kSectionH eram constexpr px crus — a 2.0 saíam a 24dp) e o
ícone/gap das tabs do painel de baixo.

**As linhas do Inspector ganharam o piso kRowH (48dp)** com paddings dp:
as alturas vinham do bloco do atlas cru + px (37px no harness ≈ 18,5dp
REAIS no device — a exata classe «teclas 48×65px» que o dono mediu; o
piso garante o alvo da casa em QUALQUER densidade e a linha CRESCE
quando o texto manda, nunca espreme). Os planos ficaram mais altos
(conteúdo do Player completo: 1264→1420 no harness) e os testes que
codificavam as alturas antigas foram recalibrados LENDO O PLANO
(`trfRowY` novo no wiring090 — os «112»/«278» hardcoded morreram; a
lição «zero fórmulas que driftam» aplicada aos próprios testes).

## 6 · A GEOMETRIA ÚNICA + O ÍNDICE + O CULLING (linha→y/col→x e perf)

A fórmula «dp(8) + linha·lh a partir de body.y» vivia em TRÊS sítios à
mão (draw, toque, scroll-segue-caret) — a classe exata do R-019. AGORA:

- **As funções puras no header**: `lineTopOnScreen`/`lineAtScreenY` (UM o
  inverso do outro — afervado pelo roundtrip), `codeX`/`caretInset`
  partilhados por draw/toque/caret, e `lineHeight()` EXPORTADA (a FASE
  12.11 duplicava «34» à mão). O toque desconta o `caretInset` — o tap
  devolve EXATAMENTE a coluna onde o caret desenha.
- **O ÍNDICE de linhas** (`ensureLineIndex`): `lineStarts` + o estado do
  comentário de bloco AO FIM de cada linha, reconstruídos SÓ quando o
  buffer muda (`bufVersion` — bumped em CADA mutação: insert/apagar/Tab/
  SUBSTITUIR/dica/open, com guard de sanidade se o buffer encolher sem
  bump). lineCount O(1), a linha do caret O(log n) por upper_bound, o
  início de linha O(1) no toque.
- **O CULLING**: o draw só tokeniza as linhas da janela visível
  (+folga); o estado bc do primeiro visível vem do cache (pago por
  EDIÇÃO, não por frame). ANTES: o buffer era varrido 3×/frame e o
  tokenizador corria TODAS as linhas — O(buffer) por frame, o custo
  crescia com o script. AGORA: `dbgLinesTokenized` conta o trabalho e a
  FASE 13.3 planta 800 linhas scrolled e espera ~50 (a prova
  determinística do perf — não há cronómetro em CI).

## 7 · OS CANTOS SUAVIZADOS (spec A — «raios 8dp cards/botões»)

O `button()` é o choke point de TODOS os botões da app: agora desenha
`panelRounded`+`frameRounded` a 8dp (dp REAL, raio clampado a
min(w,h)/2 — teclas finas ficam pill sem medo). Os 4 CARDS modais
(browser, menu de contexto, plus, logs) idem — os painéis full-bleed
(hierarquia/inspector/drawer) ficam retos: são superfícies de ecrã, não
cards. A sentinela `regress_cantos_suavizados` afere a GEOMETRIA (a
escadaria emite mais quads que o recto + o arco da moldura) — GL-free,
nos batches.

## 8 · A DUPLA DENSIDADE (FASE 13.6 — o NÃO VERIFICADO #4 fechado)

O export a 2.0: superfície 3072×1440 (a 1.0 duplicada), insets ×2, a
densidade pelo caminho REAL (`vvstub::g_stubDensityDpi = 320` →
AConfiguration → 2.0) e `applyDensity()`. O que a FASE afere:

1. o PNG decodifica a 3072×1440;
2. o VALIDADOR inteiro passa (0 erros, 0 avisos — a regra 48dp multiplica
   pela densidade: os alvos 48dp são 96px lá);
3. **a INVARIÂNCIA entrada a entrada**: as 75 entradas a 2.0 são as de
   1.0 × 2 (mesma ordem, mesmos kinds, rect ±1px) — dp E sp;
4. a largura do TEXTO dobra (o atlas cru media SEMPRE igual — a causa
   raiz deste grupo, provada pela primeira vez).

A invariância é a NOVA VARA: qualquer constante px crua que volte a
entrar no layout QUEBRA-A (foi assim que a 13.6 apanhou o ícone/gap das
tabs do painel de baixo a 24/8 px crus — o rótulo deslocava 16px a 2.0;
estava FORA da lista medida e a invariância encontrou-o sozinha). O
toast da auditoria da 13.2 ainda vivia no 13.6 (1,8s de vida e os frames
do harness correm em milissegundos) — morto antes do export para o estado
ser o MESMO da 13.1.

## 9 · OS ACHADOS AO VIVO (o loop da campanha)

1. **O button() media pelo atlas cru** (`font_->widthOf`) e desenhava
   pela escala do contexto — truncava/centrava errado no viewport 2D e em
   qualquer densidade ≠2. A classe «medir ≠ desenhar» que este grupo
   fecha: corrigido para `fontWidth`/`textMetrics` (o choke point).
2. **O gap das tabs** (24/8 px crus no BottomPanel) — INVISÍVEL a 1.0 e
   para todas as ferramentas de medição da linha de base; a invariância
   da 13.6 apontou-o (16px de desvio do rótulo a 2.0). A lição: a dupla
   densidade não é um teste a mais — é uma vara que mede o que a 1.0 não
   vê.
3. **O FLACKE do /tmp** voltou DUAS vezes durante as mutações (o
   subprocesso dos 500MB do wiring010 falha quando o disco enche — a
   lição da FASE 9 documentada nas REGRESSOES): com /tmp limpo o verde
   volta por inteiro. As provas de mutação foram feitas com o /tmp
   limpo e a nota está colada no mutacao-R025a.

## 10 · SENTINELAS E PROVAS DE MUTAÇÃO (R-025)

Quatro sentinelas novas em `tests/test_sentinels.cpp` (834→838):

| sentinela | vigia |
|---|---|
| `regress_sp_escala_unica` | o contrato sp/textK (1.0/2.0/3.0) + o choke point (fontWidth/textMetrics dobram com a densidade) + o bloco da legenda CABE na banda em qualquer densidade (o ERRO do Grupo B nunca volta) |
| `regress_toque_48dp_validador` | o validador R-024 como VARA: hierarquia+browser desenhados com audit a 1.0 E 2.0 — zero `toque_pequeno`; o [+] (id 0x28) e a pesquisa (0x157C) ≥48; `actionBtnRect` ≥48 nas duas densidades |
| `regress_script_geom_unica` | o roundtrip linha↔y (com e sem scroll), codeX/caretInset partilhados, o índice == a varredura manual, a invalidação por `bufVersion` e o guard de sanidade |
| `regress_cantos_suavizados` | o button() emite a escadaria + o arco (mais geometria que o panel reto) — nos batches, GL-free |

**Provas de mutação coladas** (`/mutacao-R025a/b/c-vermelho.txt`):
- **(a) o `textK()` com a densidade cega** (`return 1.0f` — o fix
  revertido): `regress_sp_escala_unica` FALHOU em CINCO asserções e a
  FASE 13 perdeu o VERDE do editor (o ERRO da status bar VOLTOU) e a
  invariância/sp da 13.6.
- **(b) o culling desligado** (o loop volta a TODAS as linhas): a 13.3
  FALHOU — 800 tokenizadas, a prova do perf pela razão certa.
- **(c) o [+] de volta a 56×40 px crus**: `regress_toque_48dp_validador`
  FALHOU nas DUAS densidades e a 13.2/13.6 perderam o VERDE e a
  invariância (o 40px não dobra).
- Reposições → **838/0 + 386/0**.

Recalibrações documentadas (os testes que codificavam os números antigos):
`passgl` (fontHeight 28→28×textK — o «28px CRUS» era o atlas, não a
escala), `wiring086` (a comparação tipográfica passou a RELATIVA — o
claim é o crescimento 2×, não um número mágico de px), os planos do
Inspector (safearea/scroll/wiring090: as alturas piso-48dp; os y das
linhas agora LÊM-SE do plano — `trfRowY` — em vez de «112»/«278»), o
`inspThumbsH` (64dp + sp(12)), a FASE 12.11 (consome as funções
exportadas da geometria única) e a `regress_layout_dump_nao_mente`
(mede pela fonte do CONTEXTO — medir pelo atlas cru diverge do draw).

## 11 · RASTREABILIDADE (pedido → causa → fix → teste → harness)

| pedido do Grupo C | implementação | sentinela | FASE 13 |
|---|---|---|---|
| dp()/sp() únicas | sp()/textK() no Theme.h + o fator no CHOKE POINT do UiContext + labelFittedStyled | regress_sp_escala_unica | 13.6 (a invariância + o texto a dobrar) |
| linha→y/col→x única | lineTopOnScreen/lineAtScreenY/codeX/caretInset no header; draw/toque/scroll consomem | regress_script_geom_unica | 13.3 (o roundtrip) |
| perf do editor | ensureLineIndex (O(1)/O(log)) + culling + dbgLinesTokenized | regress_script_geom_unica | 13.3 (800 linhas → ~50) |
| cantos suavizados | button() com panelRounded/frameRounded 8dp + os 4 cards modais | regress_cantos_suavizados | (a geometria nos batches; o PNG mostra o canto) |
| a linha de base B | os 18 problemas medidos (tabela da secção 2) curados | regress_toque_48dp_validador | 13.2 (o editor VERDE — 0/0 nos 4 ecrãs) |
| dupla densidade (NÃO VERIF. #4 do B) | o export a 2.0 com a invariância entrada a entrada | (a invariância É a sentinela) | 13.6 |

## 12 · NÃO VERIFICADO (honestidade do fecho)

1. **O DEVICE a 2.0 continua sendo a vara final**: a invariância prova
   que o ecrã a 2.0 é o de 1.0 ×2 NO HARNESS (rasterizador stub); o
   Mali do C33 desenha o MESMO registro — o checklist do README pede a
   «Auditoria do ecrã» no aparelho com as contagens COLADAS no próximo
   relatório (se o device apontar algo, os números divergem da
   invariância e isso é bug NOVO — não linha de base).
2. **A legibilidade a 1.0**: o corpo a 14px no harness é PEQUENO (é o
   tamanho real de 14sp a 1.0 — correto por definição; o device a 2.0
   desenha 28px como sempre). Se o dono prefere corpo maior no aparelho,
   é uma decisão de DESIGN (kFontBody 14→16sp) — um flip de token, não
   uma correção; não foi feito por não estar no pedido.
3. **O bench 60s** (fps do editor com script longo no DEVICE): o culling
   é provado determinísticamente no CI (o contador); o número de fps
   real do C33 com o script carregado é o Grupo I (BENCHMARKS) que mede.
4. **Os PNGs a 2.0 no artefacto**: o layout-harness-editor-2x.png sobe
   como artefacto do run; ninguém o RELIU píxel a píxel além da
   decodificação (a 3072×1440 o check é a resolução + o registo; o
   píxel-vs-registo da 13.1 continua sendo a prova do mecanismo).

## 13 · O ESTADO DA SUÍTE NO FECHO

- **test_core**: 838 casos, 0 falhas (834 do Grupo B + 4 sentinelas
  R-025; 10 testes recalibrados com nota no sítio — a lição R-020
  aplicada: teste que codifica o número antigo mede o bug).
- **c33_virtual**: 386 checks, 0 falhas (374 do Grupo B + 12 novos:
  13.2 VERDE, 13.3 culling/índice/roundtrip, 13.6 ×5, 13.5/13.4
  reorganizados).
- **Gates locais**: check_main, link_parity (115 TUs), jni_parity,
  glyph_source, docs_lint (49 ficheiros) — verdes.
- **A LINHA DE BASE NOVA** (para o Grupo D): editor 75 entradas
  0/0 · script 156 (54 teclas) 0/0 · browser 38 0/0 · docs 64 0/0 —
  ZERO erros e ZERO avisos em TODOS os ecrãs medidos, a 1.0 E a 2.0
  (o Grupo D hertera o orçamento vertical do editor com a casa limpa).
