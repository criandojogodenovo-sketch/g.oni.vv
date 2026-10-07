# Como isto é construído (How this is built)

Este projeto tem um fluxo de trabalho incomum, dito por extenso porque é
parte das decisões de engenharia — não é vergonha a esconder:

- **O código é editado num telemóvel.** Todo o C++/Java/GLSL deste repo é
  escrito e revisto num ecrã de 6,5" (editor de texto Android + acesso ao
  repo por HTTPS). É a restrição de partida do projeto — ela explica o
  estilo: ficheiros pequenos, nomes completos, comentários que documentam
  porquê, e uma suíte de testes que substitui a vigilância que um IDE
  daria de graça.
- **O CI é o único compilador.** Nada é compilado localmente: cada push
  corre o workflow `release.yml` no GitHub Actions (5 jobs: testes do
  core no Linux, o dispositivo virtual C33 com o replay do harness, a
  keystore, o APK arm64 assinado e o gate de símbolos). Um commit só
  conta como fechado quando o CI está verde — e o APK que o dono instala
  no device é SEMPRE o artefacto assinado do CI, nunca um build local.
- **O device real é a vara de medir.** O alvo primário é um Realme C33
  (RMX3624, Android 13, GPU PowerVR GE8320, 60 fps como meta). O que o
  dono vê no device manda; o que os testes afirmam tem de bater certo
  com o device ou é bug. As verificação por versão estão nas checklists
  abaixo (secções "Verificação no Realme C33").
- **As regressões são contratos.** Cada bug de peso ganha uma sentinela
  `R-NNN` em `docs/REGRESSOES.md` com prova de mutação colada (quebrar o
  fix → vermelho → repor → verde). O `c33_virtual` (tests/c33_virtual.cpp)
  repete no Linux o caminho de UI/events do device (FASEs numeradas).
- **O que não está verificado está dito.** Os relatórios por versão
  (`docs/RELATORIO-*.md`) têm uma secção de NÃO VERIFICADO explícita, e
  o gate docs-lint (R-016) proíbe placeholders e linguagem de achismo na
  documentação — a lista vive em `ci/forbidden_docs_patterns.txt`.

O mapa dos módulos está em `ARCHITECTURE.md`. A referência da linguagem
V.ONI (a fonte única, gerada do registo) está em `VONI_referencia.md`.
O rastreador da campanha em curso (FASE 0.9.6-MASTER, grupos A-I) está em
`BACKLOG.md`.

## 0.9.6.19 — HOTFIX: OS ABERTOS DO RE-SIGN-OFF + A CÂMARA GIGANTE

- **R1** o valor do Transform é INTOCÁVEL no orçamento: a letra do eixo
  sai primeiro, depois o padding, nunca o valor (o piso 26dp nos 3
  campos em 180/220/260dp — a 180dp os campos mostram o valor SEM a
  letra).
- **D17** a câmara é um OBJETO PEQUENO: glifo 24dp constante em ecrã +
  frustum fino 1-2px (cinza mudo 35% sem seleção / âmbar com seleção) +
  handles de canto 12dp SÓ com seleção (o do centro/far morreu) + a
  ordem do dono no hit-test (gizmo > handles > frustum INTOCÁVEL) — o
  drag na cena move a câmara pelo gizmo no glifo (provado com gesto
  injetado no device virtual, FASE 16.1).
- **D14** o menu é CONTIDO: abre com offset 0 (mesmo reabrindo com o
  slot sujo), nunca cruza a tab bar, a última linha alcançável inteira;
  CAUSA-RAIZ: os sheets eram px cru (o menu a meia medida com o texto
  2× no device) — os três menus estão em dp real.
- **D15** o menu inteiro é PT («Exportar OBJ», «Exportar Downloads») e
  a tabela do menu entrou no gate ui_vocab (a allowlist técnica
  explícita: «Snapping» fica por decisão do dono).
- **D16** a captura: o pipeline já corria; as causas do card com
  iniciais eram o frame do menu capturado (o guard de frame limpo cura)
  e a leitura Java que não repetia (retry 250/500/1000ms cura); o log
  do dono «thumb: captura ok/falhou (<motivo>)» existe nos 5 motivos.
- **D18** as iniciais: 1 palavra = primeira + ÚLTIMA letra
  (projetoyygf→PF, prooksnsn→PN — todas «PR» morreu); o gate apanha o
  regresso.
- **D19** o toggle «visível» mostra o ESTADO: o switch da casa (pílula
  32×16 + knob 12) desenha on/off nos DOIS inspetores (fonte única
  `drawVisSwitch`).
- Testes: R-035 estendida ((10) R1, (11) D19); test_cameratic
  recalibrado ao D17 + o teste novo dos estados/medidas; FASE 16 NOVA
  no device virtual (D17 E2E com gesto injetado, D14/D15, R1, D16, D19)
  — **572 checks, 0 falhas**; test_core 0 falhas. Mutações
  vermelho→verde por item (8, coladas no relatório §8). Contrato P-08:
  regras §2.16–§2.19 no MESMO commit. Relatório:
  `docs/RELATORIO-0.9.6.19-HOTFIX-ABERTOS-DO-RESIGN.md` · versionCode
  51 · PASSO 4 segue BLOQUEADO (re-sign-off do dono, 7 itens).

## 0.9.6.18b — O CI VERDE (o fix do APK: InitialsThumbView compila)

- O APK release do 0.9.6.18 falhou a compilar (o Java só compila no CI —
  o core e o c33 virtual passavam): `Paint` não tem `setStroke` (o nome
  real é `setStrokeWidth`), a classe chamava `density()` da activity a
  partir de classe `static` e o bordo do tile pintava a laje TODA de
  BORDER em `Style.FILL` (o grafite do contrato D7 nunca aparecia).
- FIX: classe não-static sem membros static no corpo (nível 1.8 do AGP),
  bordo `Style.STROKE` + `setStrokeWidth(dp(1))`, o `dp2` local morre.
- PROVA: ECJ ao nível 8 contra stubs da API real — a classe fixada
  compila; a do a277d0d reproduz os DOIS erros do CI palavra a palavra
  (script de verificação na sandbox do agente, FORA do repo). Causa
  raiz e lição no relatório §11.

## 0.9.6.18 — HOTFIX: OS 12 DEFEITOS DA IMAGEM REAL (D1–D12)

- **D1+D5 · CABEÇALHO DO INSPECTOR NUMA LINHA**: título à esquerda + o
  recolher do pin à direita; a 2.ª tab «Nós» MORREU (duplicado da
  hierarquia — a unidade da engine é TIC). Nenhum botão partilha a faixa
  do título, em 180/220/260dp, com e sem pin.
- **D2 · A LINHA TRANSFORM NO ORÇAMENTO DO RECT**: `transformRowBudget`
  (a fonte única do draw e do tap) — caixas 64→48→40dp com o reset a virar
  ÍCONE inline abaixo do piso; nada desenha fora do rect (o campo Z
  cortado e o «R» a flutuar morreram).
- **D3+D9 · EMPTY-STATES CENTRADOS E CLIPADOS** nas 4 tabs do drawer (o
  helper partilhado + `timeline::canDraw` para a Animação vazia).
- **D4 · SETTINGS LOCALIZADO**: `ui/Strings.{h,cpp}` (PT «Definições» · EN
  «Settings» — o locale entra por AConfiguration_getLanguage); «Repor
  layout» outline compacto (a laje 152dp morreu); «concedido» é texto.
- **D6 · A MARCA É O GLIFO DA FUNÇÃO ÚNICA**: `ui/Brand.{h,cpp}`
  (`drawIcon` com LOD 32dp/×1.2) + o espelho Java `UiIcons.drawBrand` — o
  tile âmbar com a letra lisa morreu (top bar, header Java, empty-state
  Java e launcher partilham o desenho).
- **D7 · A CAPTURA FORA DO HOT PATH**: o save só faz glReadPixels; o
  worker (o padrão ImportJob) codifica 256×144 (≤60KB, orçamento logado)
  no thread de jobs; o card sem thumb.png desenha INICIAIS com matiz de
  hash (paleta fixa de 6) — o ícone da app NUNCA é conteúdo de card.
- **D8 · O GLIFO «U» SAIU DO RAIL**: o ÍMAN é agora uma ferradura com
  capacetes (a função fica — era a spec PASSO 3; a leitura «undo
  duplicado» morre).
- **D10 · ZERO CONTROLOS DE TEXTO CRU**: o toggle de vista é o par de
  ícones Grid/List; o «visível» é Eye/EyeOff + rótulo; o reset é a seta
  circular `Icon::Reset`.
- **D11 · O DIVISOR É A LINHA 1dp** (a coluna de pontos flutuantes morreu;
  a pill de grip só desenha durante o drag).
- **D12 · OS PLANOS DO GIZMO PREENCHEM** a 25% do alfa do eixo + contorno
  (`UiContext::quadCornersFilled` + `kPlaneFillAlpha`).
- **GATES**: `scripts/ui_vocab_check.py` NOVO (o vocabulário do dono —
  M4/M5/M6/M7/M9/M12 vermelhos) + docs-lint com o vocabulário D5 + R-035
  (a sentinela) + FASE 15 do device virtual (40 checks @2.0) + 12
  mutações vermelho→verde coladas.
- **TABELA COMPLETA** (`docs/RELATORIO-0.9.6.18-HOTFIX-12-DEFEITOS.md`) ·
  PNGs antes/depois em docs/hotfix-*.png · **RE-SIGN-OFF P-07**: a tabela
  de 13 itens no relatório §8 (o PASSO 4 segue BLOQUEADO).

## 0.9.6.17 — UI PASSO 3: VIEWPORT (o rail esquerdo, 60% de alfa, nada full-width)

**O que existe agora (o PASSO 3 da spec de layout — o chrome da cena
medido):**
- **O RAIL ESQUERDO**: as ferramentas (Selecionar/Mover/Rodar/Escalar/
  Íman) vivem numa coluna à esquerda da cena (a toolbar horizontal do
  fundo saiu); viewports baixos dividem em colunas — o que não cabe
  ESCONDE, nunca transborda.
- **A fila do topo-esquerdo**: desfazer/refazer/guardar/**⋯** — o ⋯ abre o
  menu de ficheiro ancorado a ele; Duplicar/Colar saíram da viewport e
  vivem no menu (itens 9/10).
- **[+] 40dp REDONDO** no fundo-direito (era um chip de 56) e o **GIZMO
  40dp** no topo-direito (o atalho mostrar/esconder o gizmo).
- **NADA full-width**: a strip [Cena][Perspetiva][Global] FOI REMOVIDA
  (era a barra que atravessava a cena; os chips eram SEM FUNÇÃO desde o
  inventário do PASSO 0).
- **60% DE ALFA** em tudo o que o chrome desenha (a cena lê-se através) —
  a constante `kChromeAlpha` é pública e pinada na sentinela R-034.
- Medido (o método do PASSO 0): o desenho do chrome cobre **7,7%** do
  viewport no arranque do device (a cena 92,3%); os estados todos na
  TABELA do RELATORIO-0.9.6.16-17.

## 0.9.6.16 — UI PASSO 2-BIS: AS DECISÕES DO DONO (o pin do trilho + a consola ≥60%)

- **Fixar aberto**: tocar no ícone do TRILHO abre o inspector E fixa
  (`inspPinned` persiste no layout.json); a seta «recolher» no cabeçalho
  desfaz. Fixado, o painel fica aberto MESMO sem seleção. Sem long-press.
- **Consola ≥60%**: a lista de log ocupa ≥60% do conteúdo do drawer
  (chips+extras ≤40%) — o campo de comando SAI quando a lista com ele
  fica <60% (`bottom::conCmdVisible`, a fonte única); no device o campo
  não aparece e a lista passa a existir (antes tinha ALTURA NEGATIVA —
  o campo tapava as chips).

## 0.9.6.15 — UI PASSO 2: PAINÉIS (18%/22%, o trilho 32dp, o drawer no cap 35%)

**O que existe agora (o PASSO 2 da spec de layout — painéis medidos):**
- **Os painéis por PERCENTAGEM**: hierarquia **18%** (piso 140dp),
  inspector **22%** (piso 180dp / teto 260dp — o teto é novo no drag);
  o viewport central fica **≥55%** nos ecrãs de referência (device:
  37,1% → **58,8%** com o inspector aberto, **77,8%** com o trilho).
- **O TRILHO do inspector**: SEM seleção o painel colapsa a 32dp (a
  área junta-se ao viewport; volta ao selecionar) — a pergunta única
  `editor::inspectorCollapsed()` alimenta os rects (`safe::`), o draw,
  o divisor (que desaparece) e o centerRect. SÓ no editor 3D (o modo UI
  mantém a paridade WYSIWYG).
- **O drawer domado**: NUNCA abre sozinho (o auto-abrir da 0.8.0 saiu) e
  a altura efetiva é capada a **35% da altura do content** pelo CAP DUPLO
  em `safe::effectiveDrawerH(raw, vpH, contentH)` — uma fonte para o
  draw e o `currentDrawerH` (o P-08 mantém-se). Pegas de arrasto **24dp**
  (drawer era 12; divisores era 20).
- **A consola honesta**: chips internas **28dp** (Logs/Erros/Avisos — a
  «Consola» duplicada SAIU; a vista completa do log é a chip «Logs») e
  a FASE 13.7h prova o filtro Erros com linhas `E/GONI:` reais.
- **TABELA DE MEDIDAS** (`docs/RELATORIO-0.9.6.15-PASSO2-PAINEIS.md`) +
  PNGs (`docs/passo2-device-*.png`). Contrato P-08 atualizado (o trilho
  na árvore, o cap duplo, as pegas) + sentinela **R-033**
  (`regress_paineis_passo2`).
- **MUTAÇÕES**: M1 (o cap 35% morto → 30 falhas) e M2 (o trilho morto →
  4 falhas) vermelhas; repostas → test_core 0 falhas · c33 467/467 ·
  gates verdes.

## 0.9.6.14 — UI PASSO 1: TAMANHOS (a LEI DE OURO: desenho 32 / toque 40)

**O que existe agora (o PASSO 1 da spec de layout — só dp, sem lógica nova):**
- **A LEI DE OURO**: botão solto desenho 32 / toque 40; os elementos DE
  LINHA tomam a altura da linha da spec — top bar **36dp** (era 56, ícones
  20), cabeçalhos **28dp**, linhas **36dp**, campos **32dp**, tab bar de
  baixo **32dp** (o «FPS · TICs» no canto direito; a status bar de 24dp
  foi REMOVIDA — versão/commit em Settings › Sobre, linha git nova);
  caixas X/Y/Z 32; miniaturas 44; strip da viewport 40; stack/toolbar
  40/32; menus 40/48; NADA chega a 48dp.
- **O VALIDADOR mede a spec**: pisos por classe (40/36/32/28 —
  `ui/LayoutDump.h`) com a flag `auditRowFloorNext` (o padrão da tecla
  compacta); o `auditLabel_` regista agora a largura DESENHADA (o falso
  «SANGRA» das captions morto — o FPS·TICs foi o 1º caso).
- **TABELA DE MEDIDAS** (`docs/RELATORIO-0.9.6.14-PASSO1-TAMANHOS.md`):
  device — chrome 38,1% → **20,2%** (critério c); viewport 37,1% (a/b são
  dos PASSOS 2-3); harness — 60,9% larg / 9,4% alt / cobertura 19,0%.
- **MUTAÇÕES**: M1 (flags desligadas → o validador dispara) e M2 (tab bar
  48 → 17 falhas) vermelhas; repostas → test_core 0 falhas · c33 456/456 ·
  gates verdes. R-025/R-027 reescritas ao novo rodapé/piso.

## 0.9.6.13 — UI PASSO 0: INVENTÁRIO (spec do dono · sem alterações de código)

**O que existe agora (o PASSO 0 da spec de layout: medir antes de mexer):**
- **O INVENTÁRIO** (`docs/RELATORIO-0.9.6.13-PASSO0-INVENTARIO.md` — NOVO):
  todos os controlos visíveis do editor 3D com nome, função real (o
  símbolo chamado, ou «SEM FUNÇÃO») e posição — top bar (T1-T12), viewport
  (V1-V18), hierarquia (H1-H9), inspector (I1-I5), dock de baixo (B1-B13),
  status bar (S1-S2) e overlays. Os suspeitos da spec têm veredito por
  leitura do fonte: os chips «Cena»/«Perspetiva»/«Global» da strip e o
  logo «G» não têm função (info/marca); o «···» da hierarquia e a aba
  «Nós» têm função real; «prooksnsn» não existe no código (é o nome do
  projeto na status bar — dado do utilizador).
- **A TABELA DE MEDIDAS baseline** medida pelo CÓDIGO REAL de layout
  (`ui/SafeArea.h` corrido em anfitrião) nos dois perfis da casa: no
  device 776×336dp o editor falha HOJE os critérios da spec — viewport
  central 37,1% da largura (mín. 55%), 88,5% da área coberta por controlos
  (máx. 10%), topo+abas+estado 38,1% da altura (máx. 20%). No harness
  1536×720: 60,9% / 27,1% / 17,8%.
- **Zero mudanças de código**: o diff deste passo é documentação. A
  auditoria de layout (FASE 13, critério f) mantém a linha de base do
  5edcbde (0 erros/0 avisos; c33_virtual 456/456).

## 0.9.6.12 — GRUPOS J (ARQUITETURA DO EDITOR) + IMPORT A2 (o contrato da hierarquia; os três ficheiros reais do dono)

**O que existe agora (P-08: a hierarquia é CONTRATO; os 5 defeitos de
arquitetura curados nas suas causas raiz reais; o import com o log de
comparação completo e a degradação que não mata o import):**
- **O CONTRATO** (`docs/LAYOUT_HIERARCHY.md` — NOVO): a árvore de regiões
  do editor é a fonte oficial (§0 a tabela nome↔símbolo, §1 a árvore, §2
  as 7 regras, §3 os pisos, §4 as sentinelas); região nova entra PRIMEIRO
  no contrato. O gate `hierarchy-check` (scripts/hierarchy_check.py, NOVO
  no CI) afere que o contrato cita símbolos REAIS e que as sentinelas do
  contrato vivem no fonte.
- **J1/R-022 — a toolbar que subia à top bar**: a causa raiz eram DUAS
  fontes para a altura do drawer (o draw tapava, o `currentDrawerH()` lia
  o cru) — no device (drawer 240 persistido, viewport 208dp) o viewRect
  colapsava a ZERO e a toolbar desenhava-se SOBRE a barra de topo. Agora:
  `safe::effectiveDrawerH` (FONTE ÚNICA, piso `kViewportMinH` = 104dp) +
  clamps no chrome (a toolbar nunca sai do rect POR CONSTRUÇÃO). A
  sentinela apanhou TAMBÉM o `toolPanel` 8dp fora do viewport de 288dp e
  o Ímã 4dp fora no C33 (756dp) — ambos corrigidos.
- **J2/R-023 — o «Glob+»**: os chips da strip tinham larguras FIXAS
  (312dp num viewport de 288dp com o [+] a viver na strip) — o chip
  Global era COBERTO, não ellipsado. Agora: chips MEDIDOS (wrap-content
  com piso 56dp), a reserva do [+] respeitada, a degradação por ordem
  (Perspetiva → Cena → Global por último) e o ellipsis só como último
  recurso. «pesquisar TIC» afervado no piso 200dp da hierarquia.
- **J3/R-024 — o rect do canvas**: o glViewport/glScissor/aspect-do-rect
  já existiam; o que faltava era o DIAGNÓSTICO — a linha
  `vp3d: viewport set to (x, y, w x h) — aspect N.NNN` no engine.log
  quando o rect muda (a arquitetura real não tem o salto JNI que a spec
  imaginava — tudo é C++ sobre UMA superfície).
- **J4/R-025 — o rodapé**: a posição sempre foi a última faixa (vigiada);
  o defeito real era o CORTE: o nome do projeto é agora elipsado A MEIO
  (`textfit::ellipsizeMiddle` NOVO) com o orçamento próprio — o suffixo
  «FPS n · TICs n» sobrevive a nomes de 120 glifos.
- **IMPORT A2 — os três ficheiros reais**: a CAUSA do high_poly era o
  bound da validação que dupla-contava o `accessorByteOffset` (um GLB
  válido com views empacotadas estilo gltfpack era recusado) — corrigido
  (`off + bLen ≤ real`). Em toda a falha de bufferView a linha completa
  «glb: view<i> buffer<b> off=… len=… declared=… real=…» vai ao
  engine.log E ao erro (nunca truncado). `bufferView.buffer` inexistente
  NOMEIA o índice. View genuinamente fora do BIN = DEGRADA a primitiva
  (W + toast) e importa o resto. Guard `copiado == total` no GLB (um
  slice truncado morre com a comparação, não disfarçado). R-014
  reescrita: import → .gmesh em assets/ → o listing do picker; imagem
  podre → mesh sem texturas + W; browser isFile + 1-toque (A4) re-verificado.
- **IMPORT A2-2 — os TETOS de range e o último bloco (0.9.6.12g)**: a
  evidência nova do dono (a vista a acabar EXATAMENTE no fim do buffer de
  212 MB e o dragão de 38 MB a falhar «de forma parecida») NÃO era off-by-one
  nem bloco perdido — eram os TETOS: o `kMaxRangeBytes` de 64 MB recusava o
  view VÁLIDO de 118 MB do scene e o `fileRangeLoad` tinha um teto escondido
  de 16 MB (`kMaxJsonBytes` reusado) que matava o dragão com mentira de I/O.
  Agora: UM teto de 256 MB partilhado (parser + loader) com a mensagem
  honesta «modelo demasiado grande para a memória»; a regra `off+len ≤ real
  E ≤ declared` (`BeyondDeclared` distingue o exporter mentiroso); a linha
  do dono ganha `file=` (o disco) e o EngineLog carrega linhas de 2048 (sem
  «…»); o pool de ranges é um staging único + o accessor compacto ADOTA os
  bytes (o pico de RAM é UM range, nunca o BIN inteiro); o .gltf com irmão
  .bin grande é DEFERIDO (o scene.bin de 212 MB lido POR RANGES). R-032.

**Sentinelas novas:** `regress_hierarquia_contrato` (R-022) ·
`regress_texto_strip_campo` (R-023) · `regress_viewport_rect_segue`
(R-024) · `regress_rodape_intocavel` (R-025) ·
`glb_a2_perfis_do_device` (as fixtures reais da spec) ·
`regress_import_r014_reescrita` (R-014). **FASE 14** do c33_virtual: o
contrato ao device com a FONTE REAL + o PNG do estado J (drawer aberto)
+ o log vp3d A ACONTECER + o rodapé com o projeto longo.

**Verificação no Realme C33 (checklist do dono — por preencher, ver
RELATORIO-0.9.6.12):** os 4 grupos J (a checklist pass/fail do prompt) +
o sign-off A2 (a-d): os três ficheiros importam e aparecem no picker e
nos Ficheiros; 1 toque seleciona.

## 0.9.6.10 — GRUPO UI: A REESCRITA DA APRESENTAÇÃO (spec G · grafite+âmbar+vidro)

**O que existe agora (a estrutura, densidade e inventário da imagem 1 do
dono, ao alcance do telemóvel; o anti-exemplo da imagem 2 — ícones soltos
sobre a grelha, blocos brancos cegantes — morreu):**
- **A PALETA A OFICIAL grafite+âmbar+vidro** (`ui/Theme.h` spec G): bg
  #0E0E10 · surface #161618 α0.80 · surface2 #202023 α0.86 · border
  #2E2E32 · accent **ÂMBAR #FFB020** · accentDim #4A3714 (o fill da
  SELEÇÃO) · text1 #ECECEE · text2 #A6A6AD · danger #E5484D · warn
  **#FF8A3D LARANJA** (nunca o âmbar) · ok #46A758 · o vidro com a
  receita da spec (fill 80% + **bordo #FFFFFF1F** + highlight #FFFFFF0A,
  sem blur). ZERO azul no chrome (o azul do eixo Z e da sintaxe V.ONI são
  conteúdo — exceção documentada). O GATE R-030 no CI: **qualquer hex de
  cor fora do Theme = vermelho** (os eixos do gizmo passam a viver lá).
- **O TOPO da imagem 1**: [G âmbar + G.One][≡ Menu][Cena ▾] · tabs de
  modo ao centro · [▶][⏸][■][Android ▾][⚙] — o STOP explícito (sai do
  play com a pose restaurada) e o chip da plataforma REAL (o toque
  informa «Mobile (Android)» — não há alvos falsos). O menu ≡ tem as 6
  SECÇÕES da referência (Projeto/Cena/Editar/Visualizar/Ferramentas/
  Ajuda) com scroll, as ações de sempre + Desfazer/Refazer/Duplicar/
  Colar/Snap/Docs promovidas a linhas.
- **A HIERARQUIA**: a linha selecionada com **fill accentDim + BARRA
  ESQUERDA âmbar** (o estilo exato da referência; o fill accent cheio —
  o bloco cegante — morreu); o [+] quieto (chip de vidro + ícone âmbar);
  o título do painel 16sp; o menu ⋮ (limpar seleção/nome completo).
- **O DOCK INFERIOR de 4 TABS**: **Ficheiros** (a árvore res:// com as
  PASTAS REAIS + contagens — o toque filtra o browser) · **Assets** (a
  grelha com **MINIATURAS REAIS** — as texturas desenham A TEXTURA em si
  via imageQuad — + o toggle grelha/lista) · **Consola** (as TABS
  Consola/Logs/Erros/Avisos + cores por severidade + **o campo de
  COMANDO** com botão enviar: limpar/ajuda/play/stop/snap — comandos
  REAIS que logam o resultado) · **Animação** (a timeline de sempre).
- **O VIEWPORT com a REGRA DO PAI-PAINEL** (o anti-exemplo morre por
  construção): a STRIP do topo ([Cena][Perspetiva][Global] — informação
  real), o RAIL esquerdo (undo/redo/save/dup/paste) e a TOOLBAR inferior
  — TODOS dentro de PAIS de vidro com a receita completa (a prova VLM do
  lado-a-lado: «visibly enclosed … with distinct visible edges»); o [+
  ] quieto (chip de vidro + âmbar) e, nos ecrãs estreitos, DENTRO da
  strip (nunca mais colide com o stack — a colisão real do validador).
- **O INSPECTOR**: os rótulos **X/Y/Z COLORIDOS** (vermelho/verde/azul —
  as cores dos eixos, tokens do Theme) · as TABS [Inspector][Nós] (a
  vista Nós = a lista de nós da cena com a seleção da casa) · as caixas
  48dp + R de sempre.
- **A STATUS BAR COMPLETA**: «G.One <versão> · <projeto> · FPS n · TICs
  n» à esquerda · o ESTADO (âmbar em play) + «Mobile First» à direita.
- **E5 · O CARET DESALINHADO (confirmado no device)**: o texto usava a
  baseline no TOPO da banda e o caret uma banda própria — «linha 3,5».
  AGORA partilham a MESMA função linha→y (lineBaselineOnScreen) e o
  caret desenha de lineTop a lineTop+lh (sentinela R-029).
- **E1 · O BOTÃO COPIAR SOBRE O TÍTULO (confirmado no device)**: o
  título NUNCA mais esconde nem sangra — tem o piso 48dp; os botões que
  não cabem recolhem ao «⋯» com o menu de overflow (no 360dp: título +
  ⋯ + lupa + Run/Stop).
- **O ÍCONE**: o G passa a ÂMBAR #FFB020 com as setas #ECECEE sobre o
  grafite #0E0E10 (a versão final do dono).

**Checklist C33/RMX3624 (VERIFICAR no device — Grupo UI):**
1. O editor inteiro em GRAFITE+ÂMBAR: as seleções com o fill âmbar-escuro
   (#4A3714) e a BARRA esquerda âmbar; NENHUM bloco branco/amarelo
   cegante (o [+] e as ferramentas ativas são chips quietos).
2. ZERO azul no chrome (o azul só nos EIXOS do gizmo e na sintaxe V.ONI).
3. O topo: o G âmbar + «G.One» · o ■ (stop) sai do play · o chip
   «Android» informa o alvo ao toque.
4. O viewport: a strip [Cena][Perspetiva][Global] no topo, o rail e a
   toolbar de ferramentas DENTRO de painéis de vidro COM BORDO visível —
   nada «solto» sobre a grelha.
5. O dock: Ficheiros (a árvore res:// com contagens) → tocar uma pasta
   abre o Assets FILTRADO; as texturas com MINIATURA REAL; o toggle
   grelha/lista.
6. A consola: as 4 tabs; o aviso LARANJA ≠ âmbar do acento; o comando
   «ajuda» na lista e «limpar» limpa (o teclado abre no campo).
7. O editor de script: tocar na linha 3 → o CARET NA LINHA 3 (alinhado
   aos glifos); o título «Script» sempre visível; os botões que não
   cabem no «⋯».
8. O ícone na gaveta: o G ÂMBAR com as 4 setas sobre o grafite.

## 0.9.6.9 — GRUPO F: IDENTIDADE (FASE 0.9.6-MASTER · R-028)

**O que existe agora (o tema MONO de volta com VIDRO; o ícone G com 4
setas; o stub do harness deixou de mentir nos PNGs):**
- **A TABELA MONO+VIDRO** (`ui/Theme.h` — a spec F): o REGRESSO à rampa
  NEUTRA com que a app nasceu (F1: bg #141414 · surface #1E1E1E · border
  #2E2E2E · text1 #E6E6E6 · accent #F5F5F5 — o AZUL #2196F3 da spec A
  morreu) com o VIDRO: as superfícies ganham ALPHA (surface α0.88 ·
  surface2 α0.92 · border α0.55) e o pass UI já desenhava com blend
  desde a F1 — onde há profundidade (chips/toolbar/drawer/menus/toasts
  sobre a viewport 3D) a cena aparece por trás. A auditoria do vidro é
  honesta: o pior caso (uma cena BRANCA PURA por trás) mantém os pisos
  da casa (text2 4,74:1 ≥4,5 — `contrastOnGlass` no CI).
- **A TINTA CERTA NO FILL BRANCO**: accentInk passa a ESCURO (#141414 —
  16,9:1 sobre o accent). A spec A tinha accentInk=text1: com o accent
  mono seria BRANCO SOBRE BRANCO. O espelho Java ganha o ACCENT_INK (o
  «Novo projeto» tinha o mesmo risco) e os cards/campos o véu
  (SURFACE_GLA α0.88).
- **O CLEAR LÊ O TOKEN** (o fóssil morreu): o `glClearColor` era o
  literal #141414 DA F1 desde sempre — a spec A mudou o bg token para
  navy mas o clear NUNCA acompanhou (invisível com tudo opaco; COM
  VIDRO o fundo por trás dos painéis fica à vista). Agora lê
  `theme::kTheme.bg` — com o mono de volta o valor É o mesmo, mas por
  CONTRATO.
- **O ÍCONE «G COM 4 SETAS»** (`scripts/gen_app_icon.py` — COMMITADO no
  repo; o gerador antigo vivia em scripts-local/ FORA do controlo de
  versões e os PNGs eram órfãos): o G branco (as proporções do G
  aprovado) + as 4 setas do Move em #B5B5B5 sobre o bg mono — vetor
  puro, zero gradientes; o mestre 512 + as 5 mipmaps + o gone_logo.
- **A PALETA V.ONI intacta** (sintaxe = CONTEÚDO, não chrome — a
  exceção documentada, como as cores dos eixos do gizmo): só o
  voniComment sobe um degrau (#757575→#8A8A8A) porque o bg neutro é
  mais claro que o navy — o piso 4,5:1 mantém-se.
- **O WIPE DO STUB** (o achado ao vivo): desde o Grupo D o framebuffer
  do stub era APAGADO a cada glViewport de tamanho diferente — o clear E
  o pass 3D desapareciam de TODOS os PNGs exportados (a UI sobre preto;
  ninguém notava porque os painéis eram opacos e nenhuma check aferia
  conteúdo 3D). O framebuffer é agora da SUPERFÍCIE EGL e o glViewport
  só regista a transformação — o 3D VOLTOU aos PNGs e a FASE 13.9 afere
  o vidro PIXEL a PIXEL (o painel = blend(surface@0.88, bg) = (29,29,29)
  EXATO; o chip flutuante = o MESMO composto com a cena por trás).

**Checklist C33/RMX3624 (VERIFICAR no device — Grupo F):**
1. O ícone na gaveta/lanciador: o G BRANCO com as 4 SETAS cinzentas
   sobre o fundo #141414 (o azul e a lâmpada morreram) — nítido ao
   toque longo e no ecrã principal.
2. A tela de projetos: o header com o logo novo SEM o tint cinzento (o
   ícone já é mono), o «Novo projeto» com FILL BRANCO e TEXTO ESCURO
   legível (o contrário do azul/branco antigo), os cards com o véu
   subtil do vidro.
3. O editor 3D: os painéis (hierarquia/inspector) com o véu escuro
   subtil sobre o fundo — e os CHIPS da toolstack, a toolbar inferior,
   os menus (⋮/MENU/CENAS) e os toasts com a CENA 3D VISÍVEL POR TRÁS
   (o vidro real: mexa a câmara com um menu aberto — o fundo mexe-se).
4. O AZUL morreu: nenhuma seleção/underline/chip azul no editor — as
   seleções são BRANCAS com texto/tinta ESCURA (o «Novo projeto», a
   linha selecionada da hierarquia, a tecla ativa da toolbar).
5. O fundo do editor (o céu da viewport) é o #141414 do tema — o MESMO
   tom das barras (o clear acompanha a identidade; antes o clear era um
   literal que ninguém via).
6. Os avisos semânticos mantêm a cor (o stop VERMELHO, o amarelo dos
   avisos, o verde do guardado) — as exceções documentadas ao mono.
7. No editor de script: os comentários do código ligeiramente MAIS
   CLAROS que antes (o degrau #757575→#8A8A8A — o fundo neutro é mais
   claro e o contraste mantém-se).
8. A «Auditoria do ecrã» no editor e no script: «0 ERRO, 0 aviso» — a
   pele nova não mexeu em NENHUM rect.

## 0.9.6.8 — GRUPO E: EDITOR DE SCRIPT + SÍMBOLOS (FASE 0.9.6-MASTER · R-027)

**O que existe agora (o teclado da engine SAIU; a barra de 40dp desenha
SOBRE o IME do sistema com a medida REAL):**
- **A PONTE DO INSET DO IME**: a VvActivity MEDE a faixa que o teclado do
  Android ocupa (rootHeight − visibleFrame.bottom — funciona do API 24 ao
  34, sem ajustes de manifesto) e empurra o valor por JNI
  (`nativeOnImeInset` → `ime::bottomInset`, o mesmo módulo da fila de
  texto). A engine passa a SABER onde o teclado está — o caret nunca mais
  fica por baixo dele (o corpo, a strip de ajuda, a barra de erro e a
  barra de símbolos param TODOS acima do IME; o scroll segue o caret).
- **A BARRA DE SÍMBOLOS de 40dp** (a spec E): os 22 símbolos da linguagem
  V.ONI — { } ( ) [ ] = + - * / < > ! , . ; : " _ # @ (a página «123» do
  teclado antigo) — em teclas compactas que emitem pelo MESMO caminho do
  IME (o applyEvent: uma fonte de verdade), dokadas SOBRE o teclado real
  em `h − inset − 40dp`. Páginas ADAPTATIVAS (a 1ª tecla é o seletor
  «1/2», o padrão ?123 do GBoard): 9 teclas no device (3 páginas), 14 no
  harness (2 páginas) — sempre ≥40dp de largura. Cada símbolo LOGA
  («editor: símbolo '{' pela barra»).
- **O TECLADO DA ENGINE REMOVIDO** (280dp/54 teclas: QWERTY, shift,
  long-press de acentos, setas, página 123): o IME do sistema é o teclado
  ÚNICO — o GBoard dá acentos, gestos e dicionário; a barra cobre o que
  ele não tem à mão. A «política de coexistência» (dois teclados) morreu.
- **O HEADER FLEXÍVEL**: os botões ancoram À DIREITA (Stop, Run, lupa,
  copiar-referência, nível de ajuda) e o título FLEXIONA — o subtítulo
  some em ecrãs <120dp de zona, o título em <48dp. No device portrait
  (360dp) o header é Back + 5 botões, ZERO sobreposição — ANTES o botão
  do nível media **−56dp** e o do teclado **−8dp**: DOIS botões fora do
  ecrã desde a 0.9.6. Run/Stop estreitam para 48dp nos ecrãs <420dp (os
  alvos ≥48dp mantêm-se).
- **A EXCEÇÃO COMPACTA do validador** (a spec do autor manda 40dp na
  barra; a casa manda 48dp): as teclas da barra registam-se `compactas` —
  o piso do toque é 40dp para ELAS e 48dp para todo o resto. A exceção é
  ESTREITA e vigiada: uma tecla compacta de 39dp FALHA na auditoria, um
  botão regular de 40dp também (a sentinela R-027 prova os dois lados) —
  a POLÍTICA #5 das REGRESSOES cumprida à letra.
- **A FASE 13.8** (a vara nova): o editor de script exportado ao TAMANHO
  do RMX3624 em portrait (720×1600@2.0 = 360×800dp) com o IME aberto
  (880px) — o validador 0/0, o header com tudo ≥48dp DENTRO, a barra de
  9 teclas dokada sobre o teclado, o título flexionado.

**Checklist C33/RMX3624 (VERIFICAR no device — Grupo E):**
1. Abrir um script (Inspector → secção Script → Editar): o editor roda para
   portrait e o TECLADO DO SISTEMA abre (o da engine não existe mais — o
   botão do teclado no cabeçalho SAIU).
2. Com o teclado aberto: a BARRA DE SÍMBOLOS (40dp, faixa com { } ( ) …)
   dokada POR CIMA do teclado — tocar «{» insere o símbolo NO cursor (o
   log «editor: símbolo '{' pela barra» no Diagnóstico → engine.log).
3. O cursor NUNCA fica escondido atrás do teclado: o código e a barra de
   erro sobem com o IME (o scroll segue o cursor enquanto digita).
4. O seletor de página (a 1ª tecla da barra, «1/3»): cicla as páginas —
   as 3 páginas do device cobrem os 22 símbolos ({ } ( ) [ ] = + - * /
   < > ! , . ; : " _ # @ TODOS presentes).
5. Fechar o teclado (o back do GBoard): a barra de símbolos SOME com ele
   e o código volta à altura cheia (o layout de sempre).
6. O cabeçalho no portrait do device: os 5 botões (nível ?, copiar 📋,
   lupa, Run, Stop) TODOS visíveis e tocáveis com o dedo inteiro — ANTES
   o do nível e o do teclado estavam FORA do ecrã. O título «Script» não
   aparece no device (o header é só botões — é o flexível; no C33/harness
   largo o título continua lá).
7. A «Auditoria do ecrã» no editor de script (Diagnóstico): «0 ERRO,
   0 aviso» — com o teclado aberto e a barra visível.
8. Digitar código com o GBoard (acentos por long-press, gestos): tudo
   entra pelo caminho do IME; a barra nunca interfere (símbolo = 1 toque).

## 0.9.6.7 — GRUPO D: ORÇAMENTO DO EDITOR 3D (FASE 0.9.6-MASTER · R-026)

**O que existe agora (o editor ao TAMANHO do device — RMX3624 — passa o
validador 0/0; o ecrã largo ficou PIXEL-IGUAL):**
- **A GANGORRA hierarquia|viewport|inspector** (`safe::resolvePanels` — a
  fonte única): os painéis deixaram de ser 300dp FIXOS. Três pisos —
  hierarquia ≥200dp, inspector ≥272dp (a linha X/Y/Z), viewport ≥288dp (a
  toolbar 272+margens) — e defaults ASSIMÉTRICOS no aperto: o inspector
  mantém os 300dp (a linha X/Y/Z é o conteúdo mais rígido) e a hierarquia
  ABSORVE. No device (776dp de conteúdo): hier 200 | viewport 288 |
  insp 288 — ANTES o viewport era 176dp (22% do ecrã).
- **OS DIVISORES ARRASTÁVEIS** (o padrão da pega do drawer): strip 12dp +
  hit 20dp na borda de cada painel; o press arma, o move redimensiona AO
  VIVO (passos de 8dp, clamp da gangorra contra o OUTRO painel — o
  viewport nunca fecha), o release fixa. O toque na pega é da pega: nunca
  vira scroll, nunca orbita, nunca agarra o gizmo (o input reclama o
  gesto ANTES dos painéis). PERSISTE no layout.json (hierW/inspW; o
  ficheiro antigo continua a ler → defaults).
- **A BARRA DE TOQUE CABE em qualquer orçamento**: o stack
  undo/redo/save/dup/paste vai a COLUNAS (1 coluna nos ecrãs largos —
  zero mudança; 2/3 nos curtos; esconde só no sub-mínimo — degradação
  honesta), o [+] sobe ao canto superior direito quando a toolbar enche a
  largura, a linha X/Y/Z do Inspector adapta (caixas 64→56dp; o R ao lado
  do título em painel estreito), os botões de ÍCONE da top bar NUNCA
  encolhem (mediam 47dp no device), e o DRAWER nunca mais come o editor
  inteiro (deixa a faixa da toolbar viva).
- **O SCISSOR/ASPECT DO RECT**: a câmara do editor projeta com o ASPECTO
  DA JANELA (hierarquia|viewport|inspector), não o do ecrã — ANTES o
  scissor CORTAVA a faixa central de um frustum largo (o dono via ~22% do
  FOV horizontal no device). O viewport GL + o scissor seguem o rect; o
  gizmo, os frustums das câmaras, os glifos de áudio e TODOS os picks
  mapeiam reto-local (coerentes com o draw); em Play nada muda (o jogo
  renderiza o ecrã todo, como sempre).
- **A FASE 13.7** (a vara nova do c33_virtual): o editor exportado ao
  TAMANHO do RMX3624 (1600×720@2.0) — o validador inteiro verde com os
  problemas NA MENSAGEM, a gangorra medida, os divisores arrastados pelo
  caminho real do input e a persistência conferida.

**Checklist C33/RMX3624 (VERIFICAR no device — Grupo D):**
1. Abrir o editor: o viewport 3D VISIVELMENTE maior (288dp — 36% do ecrã
   em vez dos 22% de antes); os painéis laterais mais estreitos
   (hierarquia ~200dp, inspector ~288dp).
2. Arrastar a pega (a risca na borda de cada painel): o painel cresce e
   encolhe AO VIVO em passos de 8dp; soltar fixa; fechar e abrir a app —
   a largura persiste (layout.json).
3. Arrastar uma pega até ao limite: o viewport NUNCA fecha abaixo da
   toolbar inferior; a hierarquia nunca fecha abaixo de ~200dp.
4. O stack à esquerda do viewport (undo/redo/save/dup/paste) em 2/3
   COLUNAS (não transborda por cima da toolbar); o [+] no canto SUPERIOR
   direito do viewport.
5. A câmara do editor: o FOV horizontal PARECE MAIS LARGO (o aspect é o
   da janela — antes era um recorte do meio de um ecrã largo); orbitar e
   tocar num objeto seleciona-o NO SÍTIO onde ele desenha (o pick segue
   o rect).
6. O Inspector com o painel estreito: as caixas X/Y/Z mais estreitas
   (56dp) e o botão R ao lado do título «Posição» — nada sobreposto.
7. A top bar: os 3 botões da direita (pause/play/gear) com o dedo inteiro
   (48dp — mediam 47dp no device).
8. Abrir o painel de baixo (Ficheiros/Consola/Animação) e arrastar a
   pega do drawer até ao máximo: o editor NÃO desaparece (a toolbar do
   viewport continua viva por baixo).
9. Diagnóstico → «Auditoria do ecrã» no editor: «0 ERRO, 0 aviso».

## 0.9.6.6 — GRUPO C: ESCALA E TIPOGRAFIA (FASE 0.9.6-MASTER · R-025)

**O que existe agora (a linha de base do Grupo B CURADA — 0 erros/0 avisos
nos 4 ecrãs medidos):**
- **A ESCALA ÚNICA dp()+sp()**: a densidade multiplica TUDO — o layout pelo
  `dp()` (R-018) e o TEXTO pelo novo `sp()` (`sp(v) = v × densidade`) com o
  fator do atlas aplicado no CHOKE POINT do UiContext (fontWidth/
  textMetrics/labelStyled). A 2.0 o atlas de 28px É o corpo (o device de
  sempre — ZERO mudança onde o dono olha); a 1.0 o corpo são 14px. ANTES o
  texto era o atlas CRU: a legenda «FPS · TICs» (bloco de 29px) SANGRAVA
  3px o fundo do ecrã numa banda de 24dp (o ERRO medido do Grupo B) e os
  títulos 20sp não cabiam no cabeçalho de 56dp (o fallback empurrava-os
  PARA FORA do contentRect — o Docs/Script, incluído na ERRATA do
  relatório B).
- **O ECRÃ A 2.0 É O ECRÃ A 1.0 VISTO A 2×** (a invariância, provada pela
  FASE 13.6 entrada a entrada): o export a densidade 2.0 passa o validador
  INTEIRO e cada rect/texto é exatamente 2× o de 1.0. É a prova de que
  nada fica para trás quando a densidade muda (o «NÃO VERIFICADO #4» do
  relatório B fechado).
- **OS ALVOS 48dp REAIS** (os avisos <48dp medidos): o [+] e a pesquisa da
  hierarquia, o fechar/raízes/subir do browser, as actionRows do Settings,
  o cancelar do import — e as LINHAS DO INSPECTOR com piso 48dp (no device
  saíam a ~18dp: a exata classe «teclas 48×65px» que o dono mediu).
- **A GEOMETRIA ÚNICA do editor de script** (linha→y/col→x): as fórmulas
  vivem no header e o draw, o toque e o scroll-segue-caret partilham-NAS
  (antes: três cópias à mão que driftavam — a classe do R-019).
- **O PERF do editor**: o índice de linhas O(1)/O(log) (reconstruído só
  quando o buffer muda) + o CULLING (só as linhas VISÍVEIS tokenizam por
  frame — 800 linhas scrolled → ~50 tokenizadas; o contador
  `dbgLinesTokenized` afervável no CI).
- **OS CANTOS SUAVIZADOS**: o `button()` no choke point com panelRounded/
  frameRounded 8dp (spec A) — TODOS os botões da app num só sítio — e os 4
  cards modais (browser/contexto/plus/logs) com raios 8dp.

**Checklist C33/RMX3624 (VERIFICAR no device — Grupo C):**
1. Abrir o editor: a barra de estado em baixo mostra «FPS 60 · TICs N»
   INTEIRA e DENTRO da banda (nada cortado no fundo do ecrã — o ERRO
   medido; a auditoria do Diagnóstico diz VERDE).
2. Diagnóstico → «Auditoria do ecrã» em CADA ecrã (editor, script, docs,
   browser): o toast traz «0 ERRO, 0 aviso» (a linha de base do Grupo B
   curada; se o device apontar algo, COLAR no relatório — os números do
   harness são a 1.0, o device é 2.0 e a invariância diz que É O MESMO
   ecrã).
3. O cabeçalho do Docs e do editor de script: o título 20sp INTEIRO
   (nada a sangrar o topo por cima da faixa preta — a errata do relatório
   B).
4. O Inspector: as linhas MAIORES (48dp reais — ~96px no device) — as
   caixas X/Y/Z, o R e os sliders tocáveis sem erro de dedo; conferir que
   o painel faz scroll até ao fundo.
5. A hierarquia: o [+] e o campo de pesquisa tocáveis em toda a altura
   (48dp); o browser: fechar/subir/raízes idem.
6. O teclado da engine: «ESPACO» e «ENTER» INTEIROS nas teclas (sem «…»).
7. Escrever um script LONGO (100+ linhas) e fazer scroll: o editor mantém
   os 60fps mesmo no fim do ficheiro (o culling; o bench do Diagnóstico
   confirma).
8. Todos os BOTÕES com cantos arredondados 8dp (o button() da casa).

## 0.9.6.5 — GRUPO B: FERRAMENTAS DE VERIFICAÇÃO (FASE 0.9.6-MASTER · R-024)

**O que existe agora (a vara de medir dos Grupos C-I):**
- **O FRAMEBUFFER REAL do C33 virtual** (`tests/stub/glstub_fb.h`): com
  `glstub::fb::enabled = true` o stub de GL deixa de ser no-op e RASTERIZA a
  sério (RGBA8 + depth f32) — os DOIS shaders da casa (UI `vColor·tex.r` e lit
  `0.16+alb·diff` com uTint/skin) e a GRELHA adaptativa (o fragment com o
  fwidth analítico do plano y=0, TRIANGLE_STRIP incluído). Desligado (o
  default) = o no-op de sempre, zero mudança nos testes existentes.
- **O LAYOUT EXPORTADO (PNG+JSON)**: cada widget do UiContext regista o rect
  REAL que desenhou (o choke point — o JSON sai do MESMO código que desenha,
  a lição R-020); o fim do frame escreve `layout/<ecrã>.png` (o backbuffer
  full-res) + `layout/<ecrã>.json` (as entradas com tipo/id/rect/truncagem)
  na raiz do projeto. No DEVICE: Settings → Diagnóstico → **«Exportar
  layout»**.
- **A AUDITORIA** (o validador da casa): as 6 regras — `fora_do_ecra`,
  `sobreposto` (interativos que se pisam, sem relação pai-filho),
  `toque_pequeno` (< 48dp), `texto_truncado`, `texto_sangra` (label sem clip
  fora do contentRect — a classe do bug do C33 0.9.2), `rect_degenerado`. No
  DEVICE: Diagnóstico → **«Auditoria do ecrã»** — fecha o Settings, audita o
  ecrã por baixo (um ecrã de cada vez), LOGA cada problema e escreve
  `layout/auditoria-<ecrã>.txt` no projeto.
- **OS PNGs RELIDOS**: a FASE 13 do c33_virtual exporta os ecrãs-chave
  (editor/script portrait/docs/browser), RELÊ cada PNG com o `loadPng` de
  produção e CONFIRMA que o píxel bate com o registo (o maior painel no
  sítio, a toolbar povoada, os glifos do atlas). Os ficheiros sobem como
  artefacto do CI (`layout-screens-c33-virtual`) — o dono VÊ o que o harness
  viu.

**Os bugs que a ferramenta apanhou NO PRIMEIRO RUN** (o loop da campanha a
trabalhar): os dois botões novos do Diagnóstico desenhavam mas o WALK do tap
do SettingsPage não os re-despachava — NATIVOS MORTOS ao toque (a mesma
classe do bug 0.9.1: desenhar ≠ tocar; a FASE 13.2 tocou no rect REAL do
registo e o Settings não fechou); e o validador assinalava as linhas de
scroll scrolled-out como «fora do ecrã» (falso positivo — o clip é desenho).

**O estado atual MEDIDO** (a linha de base dos Grupos C-I, o
`docs/RELATORIO-0.9.6.5-GRUPO-B.md` tem as tabelas): o editor tem 1 ERRO
(label que sangra 3px o fundo) e 3 avisos (2 botões de 40px + 1 label
truncada); o browser tem 8 avisos de toque < 48dp; docs e script (152
entradas, 54 teclas) limpos de erros.

**Checklist de device do Grupo B (RMX3624):**
1. Settings → Diagnóstico → «Exportar layout» — o Settings fecha, o toast
   diz «layout exportado», e o projeto ganha `layout/editor.png` +
   `layout/editor.json` (verificar no gestor de ficheiros: Android/data/
   vv.goni/files/projects/<projeto>/layout/).
2. «Auditoria do ecrã» — o toast traz as contagens (o mesmo 1 ERRO + 3
   avisos do harness se o ecrã estiver igual) e o engine.log tem as linhas
   `layout:` uma a uma.
3. Repetir a auditoria com o editor de script aberto (navegar até ele) — o
   `layout/auditoria-script.txt` sai com as 152 entradas.
4. Abrir o `layout/editor.png` na galeria — tem de ser o ecrã do editor
   EXATAMENTE como se vê (toolbar, painéis, texto — full-res 1536×720).
5. O `layout/editor.json` abre em qualquer visualizador de JSON — as
   entradas com tipo/rect em px são o ecrã por dentro.

## 0.9.6.4 — GRUPO A: IMPORT glTF/GLB REAL (FASE 0.9.6-MASTER · R-021/R-022/R-023)

**Os três defeitos do device e o que os causava (forense por leitura de
código, antes de mexer):**
- **R-021 · o .gltf separado** («buffer externo não resolvido: scene.bin»):
  o resolver lia o URI RELATIVO contra o CWD do processo (no Android «/»)
  — os testes antigos passavam porque escreviam o .bin NO CWD do CI. AGORA
  os irmãos (`buffers[].uri` + `images[].uri`, URI-decode %20 incluído) são
  COPIADOS do diretório original para `source/<subcaminho>` (um log por
  irmão; irmão ausente = erro QUE NOMEIA O FICHEIRO); o projeto fica
  autossuficiente e o RECONVERT funciona sem a pasta original (os irmãos
  são stageados em SAF; a cópia auto-referencial de uma fonte que já vive
  em source/ deixou de se corromper a si mesma).
- **R-022 · o GLB com texturas** («bufferView da imagem fora do buffer»):
  uma imagem má MATAVA o import inteiro com a geometria boa. AGORA uma SÓ
  rotina de validação devolve a CAUSA (limites ≠ I/O ≠ range grande) para
  meshes (fatal) e imagens (warn + skip); o import SEGUE sem as texturas
  falhadas e o TOAST diz «SEM N textura(s)» (nunca silencioso); o LAYOUT
  dos chunks é LOGADO (json/bin/binStart alinhado 4 + bufferView de cada
  imagem) e a CÓPIA em source/ é verificada byte a byte («cópia truncada»
  com a posição exata — a cópia má sai do projeto). O parse em memória
  alinha os chunks a 4 como o de ficheiro (os dois parsers, uma regra).
- **R-023 · o browser morto ao toque** (ACHADO ao vivo pela FASE nova):
  os 8 slots de scroll do UiContext eram definitivos — com 8 regiões
  usadas numa sessão (a casa tem 14), o browser nascia SURDO. AGORA os
  slots são reciclados por frame-stamp (overlay fechado = slot livre).

**A4 · browser**: `isFile` antes do job (o ficheiro que desapareceu desde
a listagem diz O QUÊ aconteceu e a lista re-carrega com o browser ABERTO);
o d_type mentiroso do FUSE é confirmado por stat; 1 toque na linha
importa (provado pela FASE 12.8b pelo caminho REAL: browser aberto → tap
→ job → irmãos → catálogo <1s → seletor aplica).

**Checklist C33/RMX3624 (VERIFICAR no device — Grupo A):**
1. Importar um PAR .gltf+.bin (+ textura com espaço no nome, se tiver) →
   o toast diz «importado: 1 mesh(es), 1 tex»; o seletor de malha lista o
   novo (<1s) e a troca aplica; o engine.log tem «import: irmao 'scene.bin'
   copiado (N B)» por irmão.
2. Importar o mesmo .gltf SEM o .bin ao lado → erro que NOMEIA o ficheiro
   em falta (nunca o genérico antigo).
3. Importar um GLB com texturas → o log traz «glb: layout — … binStart N,
   alinhado 4: sim» e a textura entra; um GLB com imagem PODRE → o mesh
   entra SEM textura e o toast diz «SEM 1 textura(s)».
4. Sessão longa: abrir Settings, Docs, editor de script, Áudio, Consola,
   Ficheiros… e DEPOIS o navegador → o navegador AINDA responde ao toque
   (o bug R-023: 8 overlays e o browser ficava surdo).
5. No navegador, apagar (noutro sítio) um ficheiro já listado e tocá-lo →
   «ficheiro não encontrado: X (a lista atualizou)» e a lista refresca.
6. «Reconverter» uma fonte .gltf guardada → funciona sem a pasta original
   (os irmãos vivem em source/).

## 0.9.6 — IDENTIDADE + SOBREPOSIÇÕES + ECRÃS + TECLADO + MESHES + COMUNICAÇÃO + BENCHMARKS

**Os sete grupos (um commit cada, CI verde em todos):**
- **G0 IDENTIDADE (R-015)**: o artefacto publicado chamava-se 0.9.4 numa release 0.9.5 (o `name:` era um LITERAL no workflow). Nome DINÂMICO + o gate release-identity no CI (build == artefacto == relatório — reverter o bump ou fechar versão sem relatório = vermelho).
- **G1 INSETS/CAMADAS**: os ecrãs cheios (Docs/Settings/Script/Texto) vivem no contentRect (a faixa preta do C33 deixou de cortar títulos); o glifo do áudio/orbit/gizmo MORREM com um modal aberto (a cena já não se mexe por trás do Settings); a barra de baixo esconde-se em ecrãs cheios e com teclado.
- **G2 ECRÃS (R-010)**: Docs com altura VARIÁVEL e quebra por palavras (nada de "…"); Settings com headers sticky; o editor de script NÃO MENTE no render (os espaços/`{` voltaram a desenhar — `centralmain` morreu) e os ERROS ENSINAM com botão SUBSTITUIR (`if` → `exist` na linha do erro; `break` sabe se está num ciclo→resume ou option→stopand).
- **G3 TECLADO PRÓPRIO**: política de ALTERNATIVAS com o GBoard (o botão Keyboard do cabeçalho abre o próprio e esconde o IME); setas/espaço/tab/apagar/enter na linha de baixo; o SHIFT Aa (o teclado só digitava MAIÚSCULAS).
- **G4 IMPORT DE MALHA (R-014)**: a causa do "import não aparece no seletor" era o CAP DE 5 FICHEIROS sem scroll — a lista agora lista TODOS com scroll (o padrão Hierarchy); a FASE 12.8 prova fim-a-fim com um .glb REAL pelo conversor de produção.
- **G5 COMUNICAÇÃO + DOCS-LINT (R-016)**: este capítulo "Como isto é construído" + `ARCHITECTURE.md` (o diagrama de camadas) + dois rascunhos Reddit (docs/reddit/); o gate docs-lint proíbe placeholders/hedging em TODA a documentação do repo (docs/ recursivo + README + llms — 47 ficheiros).
- **G6 BENCHMARKS (R-017)**: Settings→Diagnóstico ganhou **Correr bench** e **Copiar relatório** — um bloco de 9 LINHAS com MEDIÇÕES REAIS (fps média/mín/1% low das duas cenas, verts/draw calls da MESMA fonte da barra de estado, import glTF com cronómetro, compressão ASTC/ETC2, probe de áudio, pico RSS, APK sha256, projeto em MB); o que não pôde ser medido diz **"não medido"** (a honestidade é a sentinela — um valor hardcodado = CI vermelho, provado por mutação).

**Checklist C33/RMX3624 (VERIFICAR no device — 0.9.6):**
1. Instalar o `goni-vv-0.9.6-release-signed` do CI — o NOME do artefacto bate com a versão (o R-015 fecha o ciclo).
2. Abrir Docs/Settings/Editor: NENHUM título cortado pela faixa preta; com o Settings aberto, arrastar no ecrã NÃO orbita a cena e o glifo amarelo do áudio não aparece por cima.
3. Docs: as descrições desenham INTEIRAS (com quebras por palavras, sem "…").
4. Editor: escrever `central main {` — os espaços e a chave DESENHAM; guardar → fechar → abrir: o texto é o MESMO.
5. Escrever `if` num script → a barra de erro ensina "exist" com o botão SUBSTITUIR; um toque substitui a palavra inteira.
6. O botão Keyboard no cabeçalho do editor abre o teclado próprio; a tecla Aa alterna maiúsculas; as setas movem o cursor; a barra de baixo esconde-se.
7. Importar um .glb com 5+ meshes no projeto → abrir o seletor de malha → arrastar a lista → o import NOVO está lá (<1s).
8. **Correr bench** (Settings→Diagnóstico, ~20s+) → **Copiar relatório** → COLAR num bloco de notas: o bloco de 9 linhas com os números REAIS do C33 (colar também no GitHub do relatório).
9. O bloco diz "não medido" APENAS nos sítios sem medição possível (se o aparelho nunca saiu do foreground, o warm start diz "não medido" — sair e voltar à app e re-correr o bench dá o warm).
10. Verificar os 60 fps com a cena bench (64 TICs + mesh importado) — é a pergunta que o bloco responde com o 1% low.

## 0.9.5 — P-01: LINKERS & TYKERS COMPLETOS + O EDITOR QUE ENSINA

**O que mudou (duas metades, um commit cada):**
- METADE 1 · LINKERS & TYKERS (spec fechada): `linker(A)to(B)=RF(nome)` liga duas coisas (objeto/TIC/propriedade/animação) e regista o link num RF; `tyker(nome){ find(RF) componentes… }` é o bloco de comportamento que corre sobre os links do RF (find obrigatório de 1º; vários tykers partilham RF). Os 13 componentes: contínuos `follow()/follow(d)/follow(d,suav)`, `look()`, `orbit(d,vel)`, `copy(prop)`, `map()` (no-op sem mapa); pontuais `Change(origem|destino)to(x)`, `point()/point(x,y,z)`, `colorpars(cor)(nome|#RRGGBB)` (o parâmetro 'cor' TINGE o material), `play()`, `limit(min,max)`, `delay(s)`; reservado `shading()`. SEGURANÇA: ciclos a→b+b→a (e longos) rejeitados com erro legível; profundidade 256 abort legível; RF em falta → "RF 'x' não encontrada" + o tyker não corre (o script segue); nunca crash.
- O REGISTO CENTRAL (voni/VoniRegistry): cada entrada declarada com Docs OBRIGATÓRIA; ADICIONAR COMPONENTE NOVO = 1 handler + 1 linha — o parser NÃO muda (provado pelo teste parser_independente: um componente de teste instala-se e corre sem tocar na gramática).
- METADE 2 · O EDITOR QUE ENSINA (uma só fonte): o registo alimenta as Docs (agora com a equivalência Python/JS por entrada), os erros-que-ensinam (escrever 'if'/'while'/'break'/'print' ENSINA o equivalente V.ONI com linha), a strip fina de ajuda junto à barra de erro (mini-descrição em tempo real DESDE A 1ª LETRA; níveis Iniciante/Normal/Silencioso), o toque numa palavra → explicação com exemplo, os ESQUELETOS por Tab (`exist`+Tab → `exist(){ } notexist{ }`; idem option/repeat/tyker — com o TAB no teclado in-app e no GBoard), e o botão COPIAR REFERÊNCIA (a referência V.ONI completa como texto colável para IAs).
- REFERÊNCIA PÚBLICA: `VONI_referencia.md` + `llms.txt` + `llms-full.txt` na raiz do repo — GERADOS do registo; o CI afere a sincronia byte a byte (R-013).
- SENTINELAS R-011 (ciclo rejeitado) · R-012 (RF em falta) · R-013 (bijeção da ajuda) em docs/REGRESSOES.md; FASE 11 do dispositivo virtual (259 checks).

**Checklist C33/RMX3624 (VERIFICAR no device — 0.9.5):**
1. Escrever `if` num script novo → a barra de erro ENSINA "exist" (com linha).
2. Digitar `exi` → a strip fina junto à barra de erro mostra "exist: …" desde a 1ª letra; o botão I/N/S na toolbar do editor cicla o nível (S apaga a strip).
3. `exist` + TAB (tecla do teclado in-app) → `exist(){ } notexist{ }` com cursor dentro; idem `tyker` (o find(RF) vem no esqueleto).
4. Tocar numa palavra do código → a strip explica com exemplo ("ex.: …").
5. O botão 📋 da toolbar do editor → "Referência V.ONI copiada" → COLAR num bloco de notas (texto completo com tykers e componentes).
6. Um script com `linker…/tyker…/follow(2)` + Run → o TIC SEGUE o alvo (a distância certa por frame); `colorpars(cor)(#FF0000)` → o material fica VERMELHO.
7. Um tyker com `find(fantasma)` → o logcat traz "RF 'fantasma' não encontrada — o tyker … não corre" E o resto do script continua.
8. Dois linkers em ciclo (a→b + b→a) → o Run NÃO arranca e a barra de erro diz "ciclo" com os nomes.
9. Docs (lupa): as categorias Linker/Tyker/Componente povoadas; expandir uma entrada mostra "Python/JS: …".
10. <10 MINUTOS: alguém que sabe Python/JS escreve um script V.ONI funcional só com a ajuda do editor (esqueletos + strip + toque) — cronometrar.

## 0.9.4 — FASE 9: G0 BLOQUEADORES + G1 PONTOS QUEBRADOS + G2 MOCKS

**O que mudou (só apresentação, exceto G1-6 e a lógica mínima dos fixes G0):**
- G0 (BLOQUEADORES): o editor de script ACEITA digitar sem fechar (caret livre + teclado in-app de 2 páginas + IME re-pedido pós-rotação + re-validação do TIC dono por nome); script NOVO abre com o esqueleto `central main { on moment { } allmoments { } }` e cursor no interior; LUPA na toolbar do editor + a linha "Ver docs da V.ONI" do Settings (morta desde a 0.9.2) volta a abrir as Docs com pesquisa viva.
- G1 (QUEBRADOS): toolbar inferior ANCORADA ao rect da viewport (sobe com o painel de baixo, nunca cobre outro painel; só ícones, nome só no ativo; snap = íman; "+" no canto inf-dir); ACENTOS (atlas Latin-1 + Latin Ext-A + "…"; iteração UTF-8 por code point; TODAS as strings da UI acentuadas — "ÁUDIO" desenha inteiro); MATERIAL com legendas inteiras por célula ("Textura/Cor base/Prévia") e o quadrado #FFFFFF ESCURO morto (tint f32[3] lido como f32[4] — alfa lixo); ECRÃ DE PROJETOS sem o quadrado fantasma (emptyBox inteiro desliga), cabeçalho NUMA LINHA (logo+título · pesquisa · ordenar-ícone · Novo FILL + Importar CONTORNO), grelha ADAPTÁVEL (÷180dp, mín. 2, 16:9); layout.json em DEBOUNCE 1,5s (1 write por drag, flush no pause, UMA linha "layout guardado (motivo)"); TOCAR O CORPO de um TIC seleciona-o (AABB projetado; o mais próximo da câmara vence; cada mudança LOGA o motivo) — a única mudança de Lógica.
- G2 (MOCKS): hierarquia com ícone POR TIPO DE CORPO (tic_static/tic_player/tic_rigid/tic_camera) e long-press no nome truncado → nome completo; Inspector SEM labels de debug ("malha: cubo"/"textura: —"/"entrada: …"), FÍSICA em duas colunas (tipo/forma/no chão), "Posição", e VOLTA AO TOPO quando o TIC muda; BARRA ÚNICA de 56dp (menu+tabs+play/pause+gear — o viewport GANHOU 48px; sliders morto e triad "pontinhos fantasma" REMOVIDOS); 4.º ícone da barra vertical = COPY padrão.
- SENTINELAS R-007..R-009 (docs/REGRESSOES.md) + GATE de acentos no CI (`glyph_source_check.py`); harness do dispositivo virtual na FASE 9 (209 checks).

**Checklist C33/RMX3624 (VERIFICAR no device — 0.9.4):**
1. Editor de script: abrir um script NOVO → esqueleto base com cursor dentro; digitar com o GBoard (incl. acentos) SEM fechar; back salva; reabrir → fonte intacta.
2. "ÁUDIO", "Física", "Animação", "seleção" desenham COM acentos em toda a UI (Inspector, Settings, Consola).
3. Toolbar: com o painel de baixo ABERTO a toolbar SOBE e não o cobre; o "+" vive no canto inferior direito da viewport.
4. Ecrã de projetos: SEM quadrado cinzento com projetos; cabeçalho numa linha; rodar o device → grelha adapta as colunas (16:9).
5. Tocar no CORPO de um cubo grande seleciona-o (não só o centro); logcat "seleção: TIC '…' (toque no viewport)".
6. Arrastar o drawer devagar → UMA linha "layout guardado (painel de baixo)" ~1,5s depois (antes: 4 writes em 40s).
7. Barra de cima ÚNICA (menu+3D/UI/ÁUDIO+play+gear) — viewport maior; SEM pontinhos no canto sup-dir.
8. Hierarquia: corpo estático/rígido/personagem com ícones próprios; segurar o dedo num nome cortado → nome completo.
9. Inspector: Física em duas colunas sem truncar; trocar de TIC → volta ao topo (Transform visível).
10. 60fps mantidos (barra de status) com painéis abertos; APK 0.9.4 (versionCode 47) no RMX3624.

## 0.9.3 — HOTFIX: OS DOIS CRASHES DO DEVICE (REG-001/REG-002 = R-005/R-006)

**O que mudou (só os 2 crashes + sentinelas — CLÁUSULA CALMA):**
- REG-001/R-005 — O CRASH AO ABRIR (23×/dia no RMX3624, process vv.goni): `reload()` da tela de projetos fazia `all.clear()+addAll(...)` na MAIN thread (onCreate + onResume) enquanto a lambda do io percorria a MESMA lista em background → a exceção de modificação concorrente matava o processo (crash loop). FIX: portão `ReloadGate` (1 reload de cada vez + coalesce trailing), lista NOVA local na thread de fundo (nunca `all`/`shown`/`missingUris`), publicação única na main thread, try/catch POR ENTRADA (corrompido = saltado com contadores), load FORA da main thread.
- REG-002/R-006 — O CRASH NATIVO DE ÁUDIO (tombstones AAudio/Unisoc da app antiga com.goni.runtime): migração para **OBOE** (google/oboe 1.9.3 PINADA, Apache-2.0, CMake FetchContent — AAudio na API 27+ com fallback OpenSL ES automático nos devices problemáticos); portão atómico `AudioStartGate` em TODOS os backends (start 2× NÃO abre 2º stream — a fuga de stream era a porta do SIGSEGV); AAudio fixado como fallback (mutex errCb×stop, close-no-error-callback, todos os `aaudio_result_t` verificados); AudioTrack também com portão; cadeia Oboe → AAudio → AudioTrack → SEM SOM (nunca crash por áudio); arranque no INIT_WINDOW, nunca no onResume.
- Problema 3 (pressão de memória no arranque): load da lista + queries SAF fora da main thread; memória logada no arranque (Java `Debug.getMemoryInfo` + nativo RSS do `/proc/self/status`); tetos mantidos (THUMBS 24; prim sem cache por decisão 0.8.10).
- SENTINELAS PERMANENTES (docs/REGRESSOES.md R-005/R-006): `regress_concurrent_reload` + `regress_corrupt_projects` (JVM do CI), `regress_audio_lifecycle` (core, com o OboeBackend de produção contra o stub do oboe), check estrutural `scripts/reload_concurrency_check.py`, FASE 8 do c33_virtual (replay do lifecycle agressivo do áudio + contadores de fugas de stream + projetos corrompidos) e o GATE `ci/forbidden_patterns.txt` (as assinaturas exatas dos 2 crashes + o watchdog — qualquer match = CI VERMELHO = sem APK).

**Checklist C33/RMX3624 (VERIFIED do dono — hotfix 0.9.3):**
1. App abre 10× seguidas sem crash (o crash loop do gestor morreu — REG-001).
2. Criar/apagar 5 projetos em sequência rápida: lista atualiza sem exceção (ver logcat "projetos: reload ok — N projeto(s)").
3. Editor abre e o áudio arranca (logcat "audio(oboe): stream ATIVO rate=…") OU falha graciosamente sem crash ("sem som, o editor segue").
4. Sair do editor e voltar 5× (fundo/recentes): zero tombstones novos em `/data/tombstones/`; logcat mostra "audio: PAUSE"/"audio: RESUME".
5. Logcat mostra LOGI informativos no arranque: "gestor: onCreate memoria", "boot: memoria (fim do boot) — RSS=…", "audio(oboe): stream ATIVO".
6. Desligar/ligar headset a meio de uma sessão: o som pode cair ("stream MORREU" no log) mas o editor segue; voltar a entrar no editor recupera o som.
7. Settings → Diagnóstico → probe de áudio: a tabela aparece com "DECISAO" (agora contra o Oboe).
8. APK 0.9.3 (versionCode 46) no RMX3624 sem ANR; CI verde com os gates novos.

## 0.9.2 — V.ONI v0 CORE (LINGUAGEM DE SCRIPTING FECHADA)

**O que mudou (a linguagem da engine):**
- LINGUAGEM V.ONI (spec fechada): `central main { on moment { } allmoments { } }` + top-level 1×; `v++nome=valor` / `v#nome:Tipo=valor` / `@+` exporta para o Inspector; loops `repeat(n)`, `last(cond)`, `last(cond) with n+=1`, `continue`, `resume` (nunca if/else/break); condicionais `exist`, `notexist{ }` com a cadeia `and( cond(ação) stopand )`, `option(sel){ and valor(ação) stopand notoption{ } }`; funções `fn nome(a:Num):Num { return }`; operadores `+ - * / == != < > <= >= and or not`.
- COMANDOS DA ENGINE (lista fechada): `View P "texto"` (log com prefixo `voni:`), `move(x,y,z)`, `Import.Animation("nome")`, `cena.transition.for("destino")`, `Deltatime.Increment(var, valor)`, `Explode.TIC.et/.er`, `Search.alvo.propriedade` (RTTI: pos/rot/escala/name/visible/active + .x/.y/.z).
- COMPONENTE SCRIPT num TIC (Inspector → secção Script → Adicionar/Editar + variáveis @+); o editor abre em PORTRAIT com o IME do sistema, NÚMEROS DE LINHA, COLORAÇÃO (paleta no Theme), RUN/STOP e barra de erro com LINHA; o back salva o fonte no TIC (viaja no .goni).
- SANDBOX: budget de instruções por tick + profundidade 256 — loops infinitos e recursões abortam com erro legível, nunca crash; erros SEMPRE com linha (editor + engine.log).
- SETTINGS → DOCS com pesquisa (lupa): entrada por comando/linker/tyker/componente (nome, 1 linha, sintaxe, exemplo) — 0.9.2 povoa comandos+linguagem.
- Parser PEG (cpp-peglib v1.8.6 pinada) com a GRAMÁTICA em ficheiro único (mudar um literal muda a linguagem sem tocar em C++ — testado).
- FIXES REAIS apanhados pelo loop: a PESQUISA DA HIERARQUIA estava MORTA desde a 0.9.0 (o teclado abria mas o texto nunca chegava ao filtro).

**Checklist C33 (VERIFIED do dono):**
1. TIC → Inspector → Script → "Adicionar script" → "Editar script": o editor abre em PORTRAIT com o TECLADO DO SISTEMA; escrever `central main { on moment { View P "ola" } allmoments { } }`; números de linha visíveis; keywords cor de roxo, `View` azul, texto verde.
2. RUN → o toast/log mostra `voni: ola` (Settings → Diagnóstico → Ver logs procura "voni:"); STOP para; back salva (reabrir mostra o fonte).
3. Erro com LINHA: escrever `v++to=1` (reservada) → Run → a barra vermelha mostra "linha N: 'to' é uma palavra reservada" e a linha N acende no gutter.
4. `allmoments` corre por frame: `allmoments { move(0, 0, 1) }` + Run → o TIC desloca-se; Stop congela.
5. Variável exportada: `v#@+vida:Num=100` no script + Run → o Inspector mostra `vida = 100`.
6. Docs: Settings → Docs → "Ver docs da V.ONI" → pesquisar "move" → a entrada aparece com sintaxe e exemplo; back fecha.
7. Fechar o editor → LANDSCAPE reposto, teclado fechado (o par inseparável).
8. A pesquisa da HIERARQUIA agora filtra ao tocar OK no teclado in-app (o fix 0.9.0).

## 0.9.1 — ORIENTAÇÃO + IME DO SISTEMA

**O que mudou (input de texto real):**
- Janelas de TEXTO PESADO pedem PORTRAIT via JNI (`setRequestedOrientation`) e LANDSCAPE ao fechar — o estado vive na engine (`platform/ImeQueue`) e cada mudança fica LOGADA ("orientacao: portrait pedida (janela de texto aberta)").
- IME DO SISTEMA: EditText invisível (1x1) na VvActivity + `InputMethodManager`; o texto/teclas chegam pela fila JNI (natives `nativeOnImeText`/`nativeOnImeKey`) consumida por frame; show/hide POR CONTA DA ENGINE. O teclado in-app de sempre fica para renomear rápido (landscape).
- JANELA DE TEXTO (semente do editor de script 0.9.2): Settings → Diagnóstico → "editor de texto (IME)" — full-screen portrait, back 56dp, caret piscante, DEL apaga 1 code point UTF-8, ENTER quebra linha.
- FIXES REAIS apanhados pelos testes: os botões de ação/toggles da página de Settings estavam MORTOS desde a 0.9.0 (o scroll comia o toque — Ver logs/Export/Probe/reconverter/All Files/Mic/Repor layout/Imersivo não acionavam); colisão de IDs 5829; `Activity.setImmersive` é final (javac morria); `Entry.uri` final impedia a recuperação de projetos.

**Checklist C33 (VERIFIED do dono):**
1. Settings → Diagnóstico → "editor de texto (IME)" → o ecrã roda para PORTRAIT e o TECLADO DO SISTEMA abre; escrever texto com acentos; ENTER quebra a linha; apagar apaga um acento INTEIRO.
2. Fechar (←) → volta a LANDSCAPE e o teclado fecha; reabrir repete o par.
3. Rodar o aparelho com a janela aberta: sem glifos brancos, o texto permanece, é possível continuar a escrever.
4. Renomear TIC (landscape) continua a usar o teclado in-app; o IME não interfere.
5. Os botões da página de Settings acionam (Ver logs, Export, Probe, reconverter, All Files, Mic, Repor layout, Imersivo) — o fix do scroll.

## 0.9.0 — EDITOR POLISH + DESIGN SYSTEM (spec A–M)

**O que mudou (pele + scope):**
- Theme central (tabela spec A: bg/surface/surface2/border/text1/text2/accent #2196F3/danger/warn/ok/scrim) + 49 ícones outline + CANTOS CURVOS (raios 8/4) + ícone da APP = G com a lâmpada.
- Top bar 56dp + TAB BAR [3D][UI|ÁUDIO] com underline accent (o "UDIO" morreu); viewport com stack undo/redo/save/dup/paste + toolbar rotulada [Selecionar|Mover|Rodar|Escalar] + chip [snap] + [Adicionar TIC] + triad.
- HIERARQUIA com ícones de tipo, PESQUISA de TIC, multi-seleção (toque no ícone de tipo), filhos indentados com conector.
- INSPECTOR com secções COLAPSÁVEIS, Transform em caixas X/Y/Z 48dp + botão R, Material com 3 miniaturas + hex com swatch, "Nada selecionado" LEGÍVEL.
- PAINEL DE BAIXO: [Ficheiros][Consola][Animação] + DRAWER arrastável (160–400) + STATUS 24dp "FPS · TICs".
- MENU/CENAS como sheets ANCORADOS com scrim; PÁGINA de Settings (Geral/Áudio/Permissões/Diagnóstico/Docs/Sobre) com Imersivo e Repor layout; LAYOUT PERSISTENTE (layout.json).
- UNDO/REDO (stack + botões), toasts bottom-center, tela de PROJETOS redesenhada (cards com MINIATURA capturada ao guardar, pesquisa, ordenar, duplicar/renomear/recuperação de projetos em falta).

**Checklist C33 (VERIFIED do dono):**
1. Editor abre com a pele nova (top bar/tabs/drawer/status) e nada sobrepõe; tocar nas 3 tabs (3D/UI/ÁUDIO) não perde seleção.
2. Hierarquia: pesquisar um nome filtra; tocar no ícone de tipo de 2 TICs cria multi-seleção (chip "N ×" no header limpa).
3. Inspector: colapsar/abrir secções (persiste após sair); tocar caixa Y da Pos → teclado numérico → escrever −2.5 → aplica; R repõe a linha.
4. Viewport: undo/redo (mover TIC com gizmo → undo volta); [Adicionar TIC] abre o plus-menu.
5. Drawer: arrastar a pega redimensiona (160..400); Consola filtra [erros]; Animação abre a timeline.
6. MENU sheet ancorado sob o botão (Settings/Guardar/…); Settings: Repor layout funciona; Imersivo esconde as barras.
7. Tela de projetos: card com MINIATURA após guardar a cena (default = logo G); pesquisa/ordenar filtram; ⋮ → duplicar/renomear/apagar com confirmação; SEM swipe.
8. Ícone da app = G com lâmpada no launcher.

# G.One VV 0.8.12 — 5 FIXES CIRÚRGICOS DO C33: seleção que não se perde + none de 1ª classe + staging sem /tmp + dumps com badge ANTIGO + dispositivo virtual em CI com sentinelas permanentes

## Escopo 0.8.12 (implementado — estabilização pura, ZERO features de jogo)

**A CAUSA DE FUNDO** (a evidência dos logs do dono, 0.8.5→0.8.10):
`mesh: troca - → prim esfera ERRO(sem TIC com mesh selecionado)` — a
seleção perdia-se ENTRE selecionar o TIC e tocar no picker; a migração
morria com `fileapi: mkdir falhou em '/tmp' errno=30 (Read-only file
system)` → `staging falhou` (o HOST de testes tem /tmp escrevível, o
Android NÃO); o log viewer mostrava o dump VELHO sem o rotular. CI verde
+ device a falhar = inaceitável — esta release põe o TELEFONE dentro do
CI (o "C33 virtual") e SENTINELAS PERMANENTES que ficam vermelhas para
sempre se qualquer um destes bugs voltar (docs/REGRESSOES.md).

1. **SELEÇÃO QUE NÃO SE PERDE** (T1/T2): o `viewportTapClearsSelection`
   corria SEM guard de overlay — o tap na linha/backdrop do picker caía
   DENTRO do viewRect e limpava a seleção NO MESMO FRAME do dispatch
   (antes do `applyAssetPick` — o "-" e o ERRO dos logs). Agora: guard
   `!anyOverlayOpen` no chamador; `INIT_WINDOW` RE-VALIDA/re-mapeia a
   seleção por NOME após o reload (os handles morrem, o TIC não);
   `pickerGuardBlocked` no Inspector (open) E no dispatch — sem alvo
   válido (câmara/áudio/handle morto): hint **"seleciona um TIC com
   mesh"** + log **"ui: pick bloqueado (sem seleção)"**, NUNCA o caminho
   `ERRO(sem TIC com mesh selecionado)`.

2. **NONE DE PRIMEIRA CLASSE** (T3): o picker de MESH ganhou **none em
   1º lugar** (cube passa a 2º, ficheiros 3+; tex e prim já tinham).
   `mesh: none` LIMPA o slot (o TIC deixa de renderizar mesh) pelo MESMO
   caminho seguro das trocas: posse `primRetire` → **deferred free no
   início do frame seguinte** (a cova; sem perder pendente em
   none→none); `tex: none` limpa a textura (material volta à cor plana).
   Serialização round-trip afervada: o .goni grava `"mesh":"none"` e o
   load recarrega VAZIO; `none→X→none` ×N sem crash, sem leak.

3. **STAGING SEM /tmp** (T4): o `reconvertFile` usava o LITERAL
   `/tmp/goni_reconvert_<pid>.tmp` (read-only no Android, errno=30 — a
   migração morta). Agora `stagingWrite` com FALLBACK em cascata:
   `.staging/` DENTRO do projeto (raiz de ficheiros) → **cache dir da
   app via JNI (`getCacheDir`** — o único sítio garantido escrevível sem
   permissões) → erro LEGÍVEL com os caminhos reais. GATE de CI: grep
   garante que nenhum literal de /tmp resta em FileApi/assets/migração.

4. **DUMPS ROTULADOS** (T5): o `dumpIsFromOtherBuild` existia desde a
   0.8.10 e NUNCA era chamado pelo viewer (wiring morto — o sintoma).
   Agora o log viewer põe **`[ANTIGO (build N)]`** (cor de aviso) em
   dumps de outra build e `[ANTIGO (pre-0.8.10)]` nos anónimos; dumps
   novos ficam limpos. O banner de boot ganhou o formato exigido:
   **`boot: G.One VV <versão> versionCode <N> sha256 <…> git <…>`** (o
   sha256 REAL da .so vem do build_info.txt do CI em 2 passes).

5. **O C33 VIRTUAL EM CI + SENTINELAS** (T6/T7): o executável
   **`c33_virtual`** reproduz o telefone no CI — superfície **1536×720 +
   insets**, **/tmp READ-ONLY (errno=30)**, **cache dir da app via JNI**,
   **content:// SAF**, **lifecycle EGL TERM/INIT com re-upload**, **ASTC
   ativo**, **taps replayáveis pelo frame() REAL** — e corre o **REPLAY
   da sessão real do dono** (a sequência que produzia os sintomas), com
   o output passo-a-passo colado no relatório. As **SENTINELAS**
   (`regress_selection_loss`, `regress_tmp_staging`, `regress_none_slot`,
   `regress_dump_identity` em tests/test_sentinels.cpp) correm em TODAS
   as runs de CI para sempre; o **gate de padrões proibidos**
   (ci/forbidden_log_patterns.txt) grepa o output do replay — qualquer
   match = CI VERMELHO = release bloqueada; o job do APK assinado
   **depende** das sentinelas + gates + harness (sentinela vermelha =
   NÃO HÁ APK). **PROVA DE MUTAÇÃO**: cada fix revertido temporariamente
   → sentinela/harness VERMELHO (o sintoma exato volta ao log);
   reposto → VERDE (docs/RELATORIO-0.8.12.md).

**TESTES**: 608 → **613** (sentinelas ×4; assetpick none/none→X→none;
banner/badge novos; picks 3+ do mesh picker; migração/reconverter com
cache dir + /tmp RO + SAF content://) + **c33_virtual: 97 checks** no
replay do dispositivo (migração no boot, prim/mesh/tex picks, backdrop,
none×N, sem seleção→hint, 2× TERM/INIT com revalidação, SAF content://,
dump ANTIGO, gate de proibidos). **RED→GREEN PROVADO ×4 (mutação)**:
guard de seleção revertido → 20 FALHAS + `mesh: troca - → prim esfera
ERRO(sem TIC com mesh selecionado)` ×6 no replay; /tmp de volta → 11
FALHAS + `staging falhou` ×7; none→cube → 7 FALHAS; badge morto → 1
FALHA. CLÁUSULA CALMA: só os 5 fixes + harness + sentinelas + testes.

## Checklist C33 (o dono preenche pass/fail por item — 0.8.12)

1. **Seleção**: com um TIC de mesh selecionado, abrir pickers
   prim/mesh/tex e trocar 3× cada — a seleção CONTINUA (o Inspector não
   fica "(nada selecionado)"); tap no backdrop do picker fecha SEM
   desselecionar; **fundo/recents do Android e voltar** → o TIC continua
   selecionado (log: `lifecycle: selecao re-validada pos-INIT WINDOW`);
   no motor 3D|UI|ÁUDIO a seleção segue.
2. **Hint em vez de ERRO**: desselecionar (tap no vazio do viewport) e
   tocar em QUALQUER linha de picker incl. none → toast
   "seleciona um TIC com mesh" + engine.log `ui: pick bloqueado (sem
   seleção)` — ZERO linhas `ERRO(sem TIC com mesh selecionado)` no log
   viewer (as trocas todas dão `fim ok`).
3. **none**: picker de mesh → **none** (1º lugar) → o TIC deixa de
   renderizar; tex → none → material volta à cor plana; none→cube→none
   repetido ×5 sem crash; **save/load** → o TIC sem mesh continua sem
   mesh (`"mesh":"none"` no .goni).
4. **Migração sem /tmp**: abrir um projeto ANTIGO (com meshes/*.obj) →
   converte em silêncio (log `asset: staging em '…/.staging/…'` ou cache
   dir — JAMAIS /tmp); ZERO `mkdir falhou em '/tmp'` /
   `staging falhou` no engine.log.
5. **Dumps**: viewer de logs → dump de build antiga com
   `[ANTIGO (build N)]`; nenhum dump novo aparece por uso normal; a 1ª
   linha do boot log é `boot: G.One VV 0.8.12 versionCode 42 sha256 …`.
6. **Sentinelas no CI**: o job "C33 virtual (dispositivo + sentinelas +
   gates)" VERDE com o output do replay; o APK só sai se ele estiver
   verde (needs).

---

# (histórico) G.One VV 0.8.11 — ÁUDIO: AAudio com probe + fallback AudioTrack + formato próprio .gi (ADPCM/OGG/MP3) + TIC AudioPlayer + workspace ÁUDIO + gravação de mic

## Escopo 0.8.11 (implementado — o som da cena, ZERO features de jogo)

**A ARQUITETURA (a decisão do prompt)**: o áudio da engine é UM
MISTURADOR DE SOFTWARE PURO (`core/AudioEngine` — vozes com cursor/loop/
volume/pitch/posicional somadas por frame) que NUNCA fala com o hardware.
O BACKEND puxa o misturador no callback: **AAudio** no device (carregado
por **dlopen** — o minSdk 24 não liga a API 26; o mesmo binário corre na
24/25 caindo no fallback) e, se o **PROBE DE ESTABILIDADE** (Settings →
"diagnostico audio": 50 ciclos start/stop + 10 pause/resume, tabela no
engine.log) acusar QUALQUER falha/disconnect, o **fallback AudioTrack**
(JNI) assume DEBAIXO DA MESMA INTERFACE — o misturador nem sabe qual dos
dois corre. O lifecycle segue a activity (pause/resume; TERM fecha).

1. **FORMATO PRÓPRIO `.gi`** (header comum de 32 B das 0.8.10: magic/
   versão/endianness/checksum FNV-1a): `ADPCM IMA 4:1` para WAV (o codec
   próprio, round-trip aferido: contagem EXATA, energia dentro de 0.5%),
   **passthrough** para OGG/MP3 (já comprimidos; decode no load pelos
   vendors minimp3/stb_vorbis — CC0/public domain, um TU só). Corrupção
   → erro legível, nunca crash. Import `.wav/.ogg/.mp3` → `audio/<nome>.gi`
   com a linha de rácio no log (`audio: import … ratio=X`); guarda de
   256 MB (áudio gigante = lixo, não é geometria).

2. **TIC AudioPlayer (estrutura primeiro)**: componente puro com
   clipPath/autoplay/loop/volume/pitch/posicional+raios; o preset "+ Audio"
   cria Transform+AudioPlayer SEM mesh; Inspector com a secção ÁUDIO
   completa (clip pelo seletor novo, **ouvir** = preview no mesmo
   misturador do Play, autoplay/loop/posicional, sliders volume/pitch/
   raios); serializer round-trip com defaults omitidos; **glifo de
   ALTIFALANTE + esfera wireframe dos raios** no editor (em Play nada
   desenha — só soa); posicional atenua pela distância ao listener
   (câmara ativa) com os raios a seguir a POS VIVA do TIC.

3. **WORKSPACE ÁUDIO** (toolbar G3: **3D | UI | ÁUDIO** — exclusivos): a
   aba ÁUDIO substitui o viewport (o MESMO rect do editor de UI — nada
   sobrepõe painéis): lista de clips (nome/duração/codec), **Importar**
   (navegador na raiz Music), **Gravar/Parar** (mic → `.gi` ADPCM com
   medidor de nível e temporizador), preview play/stop, **waveform** de
   picos PCM com linha de progresso, renomear (teclado in-app),
   apagar com confirmação, atribuir a TIC. Trim = dívida documentada.

4. **GRAVAÇÃO (mic → .gi)**: AudioRecord (JNI) numa thread própria —
   PCM16 mono 44100 lido por chunks; STOP → `audio/rec-<unix>.gi` +
   catálogo + rácio no log. A permissão RECORD_AUDIO é pedida NO 1º GRAVAR
   (declarada no manifest; diálogo do sistema; o dono re-toca e segue —
   o mesmo contrato humano do All Files, nada de loops). No CI o mic é
   SINTÉTICO (senoide) pela MESMA máquina de estados — o wiring inteiro
   aferido sem hardware.

5. **MISTURADOR (o coração)**: vozes (slots reutilizáveis), resample
   linear por pitch E pela taxa do device (clip 22050 no device 44100 =
   tempo real), mono→stereo, clamp [-1,1], master volume (Settings →
   "volume geral", persistido por projeto), fim exato detetado no fim do
   bloco (o bug apanhado pelo CI: a voz ficava "playing" entre callbacks),
   posicional com cursor a andar mesmo mudo (o loop conta o tempo).

6. **NAGEVADOR +5ª raiz**: Music ao lado de Raiz/Download/Docs/Camera/
   Pictures (o áudio do dono vive aí); ficheiros .wav/.ogg/.mp3 listados
   como `audio: <nome>` e o toque IMPORTA → `.gi`. Os números de
   raiz/subir/lista são PARAMÉTRICOS (o "Subir" fixo em 6 colidia com a
   6ª raiz Music — apanhado na revisão). O catálogo de áudio alimenta o
   Inspector E o workspace pelo mesmo refresh.

**TESTES**: 581 → **608** (ADPCM contagem exata/energia/defesas, .gi
round-trip + 6 corrupções legíveis, import WAV mono/stereo com rácio +
erros de formato, misturador cursor/loop/fim/pitch/resample/slots/
posicional/master, PROBE com fakes: falha→FALLBACK/disconnect→FALLBACK/
ok→AAudio OK, workspace com fake host + confirmação de apagar + guards,
serializer/preset/registro, FileApi kinds + raiz Music, seletor de clips
(menu 5: none limpa/pick aplica/fora não crash), plano do Inspector,
"+" com Audio; DEVICE: import wav e2e → diálogo "aplicar ao TIC?" → "Sim"
atribui, sem AudioPlayer não pergunta, gravação sintética e2e com rácio +
ponte do mic (fake JNI: concedida/negada), boot/probe/troca de backend +
pause/resume do lifecycle, frame no modo ÁUDIO + preview do Inspector pelo
caminho real). **RED→GREEN provado**: encoder da 1ª versão (amostras
dobradas) e fim-de-bloco do misturador revertidos → **12 testes FALHAM**;
restaurados → 608 OK. CLÁUSULA CALMA: só áudio + testes.

---

# (histórico) G.One VV 0.8.10 — MESH DETERMINÍSTICA (só cubo e esfera) + import 500 MB STREAMING + formatos próprios (.gmesh/.gtext/.gm) + archives + diagnóstico com identidade

## Escopo 0.8.10 (implementado — a decisão do dono, ZERO features de jogo)

**O DIAGNÓSTICO**: no C33 "o cubo funciona quase sempre, a esfera e o
cilindro só às vezes" — intermitência em VÁRIAS primitivas = o problema
não estava nos geradores, estava no CAMINHO PARTILHADO: o cache de meshes
GPU por assinatura (0.8.0), onde TODAS as trocas de TODOS os TICs se
cruzavam (evicção/release/rebind podiam mexer no mesh de outro TIC).
DECISÃO DO DONO: **ficar só com CUBO e ESFERA e MATAR o cache**.

1. **SÓ 2 PRIMITIVAS + MIGRAÇÃO**: cilindro/cone/plano/triângulo/torus/
   cápsula saem do gerador, seletor, serializer, Docs e testes. Um `.goni`
   antigo com prim removida carrega como **cube** + log
   `mesh: prim <x> removido -> cube` + toast 1× por load (nunca crash).
   Pureza: mesmos parâmetros → BYTES IDÊNTICOS (hash FNV-1a no CI).

2. **TROCA DETERMINÍSTICA (SEM CACHE, deferred free)**: cada MeshRenderer
   com prim tem o SEU mesh; UM SÓ caminho no **ponto seguro do frame**
   (início, antes de qualquer submissão GL): `passo=gerador →
   passo=validacao (finitas+AABB) → passo=upload + SELF-CHECK de contagens
   → passo=bind` com **deferred free** (o mesh antigo só morre no início do
   frame SEGUINTE — buffers em voo nunca morrem; no frame da troca o
   antigo AINDA desenha). Falha em qualquer passo: mesh anterior mantém +
   seleção intacta + backoff (zero retry-storm) + log passo-a-passo
   `mesh: troca <de>→<para> passo=<p> ok/ERRO(<razão>)` + toast no ecrã.
   O pick ARMA o pedido (o antigo renderiza até ao bind) — upload NUNCA
   a meio do frame.

3. **IMPORT STREAMING 500 MB (nunca o ficheiro inteiro em RAM)**: o import
   corre numa THREAD própria (o frame desenha o OVERLAY de progresso com
   barra e CANCELAR); cópia por chunks de 6 MB pelo WRITE STREAM do storage
   (ProjectStorage::openWriteStream — FsStorage FILE* real, SafStorage fd
   real); conversão OBJ linha-a-linha; GLB com JSON ≤ 16 MB + BIN chunk
   DEFERIDO (accessors/imagens materializam só os seus ranges); PNG com
   guarda 64 MB. **PROVA no CI (output real)**: fixture de 500 MB importada
   com **RSS de pico 11 MB em 1,7 s**; o caminho ANTIGO (readAll) medido
   no mesmo ficheiro: **510 MB de RSS** (o crash do dono, morto).

4. **FORMATOS PRÓPRIOS com header comum** (magic/versão/endianness/
   alinhamento/checksum FNV-1a): `.gmesh` (indexado+dedup+QUANTIZAÇÃO
   16-bit — 8 B/vértice), `.gtext` (mips ASTC/ETC2 persistidos — upload
   direto), `.gm` (clips + esqueleto). Corrupção → **erro legível**
   ("CHECKSUM CORROMPIDO…"), nunca crash. Storage: fonte em `source/` +
   convertidos em `assets/`; o RUNTIME carrega SÓ formatos próprios
   (`asset: load <nome>.gmesh verts=N em Xms`); migração de projetos
   antigos EM SILÊNCIO no primeiro load; Settings: **fonte: manter/largar**
   + botão **reconverter assets**.

5. **ARCHIVES (2 passos separados — EXTRAIR ≠ IMPORTAR)**: o navegador
   lista `.zip/.rar`; tocar num .zip extrai STREAMING para
   `extracted/<nome>/` (ficheiros CRUS — ZERO conversão; o browser ABRE a
   pasta extraída) com **zip-slip rejeitado** (../ ou absoluto → log,
   nunca escreve fora), **bomb-guard** (teto por entrada/total → erro
   legível), CRC por entrada, archives aninhados ignorados, cancelamento
   sem estado parcial. RAR: sem decoder (licença unrar) → erro legível
   "usa .zip" (tabela de decisão no RELATORIO). ZIP = parser próprio de
   central-directory + inflate do zlib do NDK (sem vendoring).

6. **DIAGNÓSTICO COM IDENTIDADE**: todo crash dump nasce com
   `build: <versão> (versionCode N)` + `git:` + `so: <sha256>` + `epoch:`
   no header E no NOME (`crash-<unix>-vc<N>.dump` — o CI escreve
   `assets/build_info.txt` com o sha256 REAL da .so em 2 passes; a
   VvActivity entrega pela JNI no onCreate); **banner de versão** no boot
   log; log viewer marca dumps de outra build com **[ANTIGO]**.

**TESTES**: 568 → **581** (formatos round-trip+rácio, checksum/magic/
versão/endian rejeitados, 500 MB com RSS REAL medido + prova RED do
readAll, zip cru/zip-slip/bomb-guard/cancelamento, migração e2e com ref
reescrita, identidade nos dumps + badge, setting fonte + reconverter,
stress 600 trocas SEM cache 100% em 3 ms, deferred free aferido no stub,
backoff sem retry-storm, pureza por hash). CLÁUSULA CALMA: só isto + testes.

---

# (histórico) G.One VV 0.8.9 — CRASH-PROOF: recursão + primitivas à prova de falha + fit uniforme + espaço sem tetos

## Escopo 0.8.9 (implementado — 4 fixes cirúrgicos, ZERO features)

**O DIAGNÓSTICO (dump-driven)**: o crash-1790830406.dump do C33 (SIGSEGV,
`si_addr ≈ sp` = stack exhaustion, 60 frames com o MESMO pc) foi resolvido
NO CI contra o build assinado exato (BuildID `79363917…` casado byte-a-byte;
workflow `resolve-crash.yml`): `addr2line` nomeia `0xe2de8`/`0xe2f24` =
`vv::SceneSerializer::(anonymous namespace)::appendComponentJson(Json&,
SkeletonComp const*)` com a cadeia INLINE `std::vector<vv::Json>::~vector`
(vector:445) — **o stack exauriu na CADEIA DE DESTRUTORES recursiva do
Json**; `0xf2938` = `stbtt_GetGlyphKernAdvance+0x110` (função que NADA chama
no binário — frame de unwind andando stack corrompida). O "TIC desseleciona
e continua cubo" = a activity a reiniciar após o crash e a recarregar a cena
gravada. O `ERRO(gerador/upload falhou)` com origem `-` era SELEÇÃO PERDIDA
(o applyAssetPick sem alvo), não gerador — o gerador estava (e está) são.

1. **CRASH (dump-driven)**: destrutor do Json REESCRITO ITERATIVO (cova
   deque + fila — QUALQUER profundidade destrói plana; folhas pagam zero);
   `placeAt` do resolver de UI ganhou GUARD DE CICLO + PROFUNDIDADE (o
   `sizeOf` já guardava ciclos — o `placeAt` NÃO: um container pai de si
   próprio — o seletor "colocar em" INCLUÍA o próprio — recursava
   infinitamente; RED→GREEN: sem os guards o teste SEGFAULTA); o seletor
   deixa de se auto-listar; `Json::dumpTo` ganhou teto 64 (o parser já
   tinha); o loader CURA self-parent ao carregar (ficheiros 0.7.4–0.8.8);
   **qualquer traversal recursivo do core tem agora teto/iteração — erro
   legível em vez de SIGSEGV**.

2. **PRIMITIVAS À PROVA DE FALHA**: o primMesh VALIDA a geometria ANTES do
   upload (verts/idx, coordenadas finitas — o Mesh::create REJEITA NaN —,
   AABB não degenerado); falha em qualquer passo → mantém o mesh anterior,
   ERRO com a razão exata, seleção intacta, sem crash. O ERRO da troca é
   HONESTO: "sem TIC com mesh selecionado" (seleção perdida) em vez do
   "gerador/upload falhou" enganador. Auditoria: as 8 primitivas com
   defaults E EXTREMOS (seg=3/256, raio=0.001/1000) geram válido, finito,
   não-degenerado (tabela no RELATORIO-0.8.9).

3. **NORMALIZAÇÃO UNIFORME NO IMPORT**: fator ÚNICO `s = 2 / maiorEixo`
   aplicado aos 3 EIXOS (proporções preservadas — NUNCA espalmado); a
   escala vive no Transform3D (geometria intacta); Inspector ganhou
   "dims: X×Y×Z" + botão **escala original** (repõe {1,1,1}); log
   `import: dims=… uniform scale=…` no engine.log (log viewer).

4. **ESPAÇO SEM TETOS**: zoom 0.01 → 100 000 (era 1..300); near/far
   DINÂMICOS por frame (editor E Play) derivados do zoom + AABB da cena
   (distância ao ponto mais longe + margem — o far SEMPRE contém a cena;
   o slider far do dono é PISO no Play); campos NUMÉRICOS sem teto no
   Inspector (tocar o VALOR à direita do trilho → teclado numérico —
   py=10 000 escreve-se; sliders mantêm o range suave); grelha ADAPTATIVA
   (passo 0.1/1/10/100/1000 pelo zoom) — e o BUG LATENTE da F3.1 corrigido:
   o uniform uExtent NUNCA era enviado (o quad estava degenerado — o grid
   invisível desde a 0.3.1). Nota: jitter de float32 > ~100 000 unidades é
   limite conhecido; origin rebasing = FUTURO (não implementado).

**TESTES**: 545 → **568** (+23: ciclos de layout com guard/RED→GREEN
SEGFAULT provado, dtor Json iterativo 500k níveis/RED→GREEN SEGFAULT
provado, dumpTo teto, grelha adaptativa, far dinâmico contém a cena,
normalização uniforme proporções+re-aplicar, campo numérico py=10 000,
geradores ×extremos, Mesh NaN/AABB, cura de self-parent, sequência exata do
device com origem "-", falha GL mantém mesh+seleção, 8 prims ×3 ordens,
import gigante e2e, zoom extremos+frames). CLÁUSULA CALMA: só os 4 fixes +
testes.

---

# G.One VV 0.8.7 — HOTFIX CIRÚRGICO: import abre o navegador + troca de mesh sem travar

## Escopo 0.8.7 (implementado — hotfix: import + troca de mesh, ZERO resto)

**O DIAGNÓSTICO**: no C33 o botão Import não abria NADA e a troca de mesh
travava a engine intermitentemente ou dava "falha ao gerar primitiva". A
AUDITORIA DE EXISTÊNCIA provou que o gerador de primitivas ESTÁ no APK
(render/Primitives.cpp no CMake da app desde a 0.8.0 — não é o precedente
FileApi) e que o handler chama o gerador no caminho real; os bugs eram de
WIRING e de RECURSO, todos invisíveis à suíte porque o main.cpp (onde vivem
primMesh/attemptImport/rebindPrimMeshes) era DEVICE-ONLY, fora dos testes:

1. **IMPORT MORTO (wiring)**: `attemptImport()` chamava `browserOpen()`
   (põe `g_browser.open`) mas NUNCA setava `g_editor.fileBrowser` — o gate
   do overlay no frame() exige as DUAS flags. O navegador "abria"
   INVISÍVEL: o toque chegava, o handler corria, nada aparecia ("o botão
   Import não abre nada"). Fix: as duas juntas, sempre (toque e retoma
   pós-concessão); `openImportScan` (scan Download/Documents) foi removido
   — o import É o navegador 0.7.2.

2. **TROCA INTERMITENTE (recurso)**: o cache de primitivas era SEM LIMITE
   — cada posição de slider era uma assinatura nova = mesh GL vivo para
   sempre → exaustão de memória de GPU → uploads começavam a FALHAR (o
   "dá erro") e o driver engasgava (o "trava"). E quando um upload falhava,
   o `rebindPrimMeshes` re-gerava+re-uplodava A CADA FRAME (retry-storm =
   freeze). Fix: CAP (48) + EVICÇÃO de não-referenciados (o mesh em uso
   NUNCA sai) + NEGATIVE-cache (a assinatura que falhou devolve a MESMA
   resposta sem regerar — nova tentativa só em contexto novo).

3. **GESTO ÓRFÃO (input)**: um widget que desaparece a meio do gesto
   (overlay fechado antes do release) deixava o `active_` do UiContext
   preso PARA SEMPRE — todo o botão/slider exige `active_ == 0` para
   capturar, a UI inteira morria ("a engine trava": o 3D continua, nada
   responde). Fix: no FIM do frame, sem dedo em cima, o active_ órfão morre
   (depois de todos os widgets terem tido a sua frame de release).

4. **LOGGING EMBUTIDO (o petitorio)**: `import: <passo>` em cada passo do
   import; `mesh: troca <de>→<para> inicio` / `fim ok verts=N idx=M` /
   `ERRO(<razão>)`; `mesh: prim <tipo> verts=N idx=M` a cada upload novo —
   a PROVA no log viewer do C33 de que a geometria existe e é chamada.

5. **GATE DE SÍMBOLOS no CI**: o verify-entry-symbols agora AFIRMA que
   makePrimMesh/primDefaults/primName/primClamp (gerador) e applyAssetPick
   (dispatch da troca) estão no .dynsym do APK — a auditoria de existência
   institucionalizada (nunca mais "nos testes mas não no device").

6. **O CAMINHO REAL DO DEVICE NA SUÍTE**: test_wiring087 `#include
   platform/main.cpp` — primMesh, attemptImport, browserImportFile,
   rebindPrimMeshes, o BOOT do INIT_WINDOW e o frame() inteiro correm no
   hospedeiro contra os stubs GLES3/EGL/JNI (o stub EGL ganhou init feliz
   + 1280×720; o stub GL ganhou injetor de falha). 13 casos novos, todos
   VERMELHOS antes dos fixes (afirmado ao correr a suíte com os fixes
   revertidos): 532→545. versionCode 38. CLAUSULA CALMA: import + troca +
   auditoria + logging + testes — zero features, zero layout, zero física.

<!-- (0.8.6 abaixo — histórico) -->

# G.One VV 0.8.6 — UX/layout/Inspector: hex, tipografia, gizmos de UI, Theme uniforme

## Escopo 0.8.6 (implementado — campanha F8: UX/layout/Inspector)

**O DIAGNÓSTICO**: a UI do editor estava funcional mas desorganizada no
device — cores fora do Theme (launcher Java com 11 hex inline + diálogos
claros), Inspector sem edição de cor por código nem tipografia, elementos
de UI sem gizmos (só mover) e testes de sobreposição que não cobriam a
paisagem completa. Tudo fixado com CLAUSULA CALMA (polimento, ZERO features
novas — tipografia/gizmos são o polimento pedido pelo dono):

1. **COR POR CÓDIGO HEX (TIC + elemento de UI)**: linha "hex: #RRGGBB" no
   Inspector de TICs (aplica ao tint do MeshRenderer) e no Inspector de UI
   (aplica ao color[] do elemento). Tocar abre o teclado in-app em MODO HEX
   (a tecla de caso vira "#"); parse/format são funções PURAS testadas
   (redondo exato, recusas honestas — hex inválido não altera o estado).

2. **TAMANHO E ESTILO DE LETRA no Label (texto do elemento)**: os elementos
   com texto (Label/Button/Menu/Card/Article) ganham "letra: Nx" (slider
   0.5..3.0 sobre a base 28 px) e "letra estilo: normal/negrito/italico"
   (cicla). O render passa pelo labelStyled: escala de glifos, negrito por
   duplo-draw embutido (+1 px), itálico por cisalhamento dos VERTICES TOP
   (sem segundo atlas — zero memória nova). Testes aferem o BATCH real.

3. **GIZMOS de UI — escalar e rodar (além de mover)**: elemento selecionado
   no modo UI ganha 4 handles de canto (12 px, toque constante) + pega de
   rotação acima do topo-centro com haste. Arrastar canto = escala com o
   canto OPOSTO fixo (reancoragem por elementRect nas 6 âncoras — inverso
   exato testado); arrastar a pega = rotação com SNAP 15° (o MESMO passo do
   snap 3D). Hit-test (editor e Play) respeita rotação (inversa exata).

4. **ZERO SOBREPOSIÇÃO AFERVÉVEL EM PAISAGEM**: novo teste de composição
   COMPLETA (toolbar/painéis/centro/timeline/status/header da timeline)
   par-a-par e dentro do contentRect a 1600×720 (C33) e 1280×720.

5. **THEME UNIFORME**: os espelhos de tokens do UiRuntime (kLine/kText
   duplicados) MORRERAM — o desenho usa os tokens de UiContext.h. O
   LAUNCHER Java centraliza os tokens (BRAND/BG/TEXT/TEXT_DIM/SURFACE/LINE
   — espelho do Theme.h; o check estrutural REJEITA hex inline fora deles)
   e os 3 AlertDialogs correm agora num ContextThemeWrapper ESCURO (o
   manifest é claro — a identidade não quebra mais).

6. **PÁGINA INICIAL LIMPA**: título de marca + subtítulo discreto ("editor
   de jogos no telemóvel") + ações criar/importar no topo + "Meus projetos"
   com nome/data + empty-state com CTA — hierarquia clara, mesmo idioma
   visual do editor.

Suíte 525→532 (+7; contratos de alturas dos Inspectores atualizados: 828→864,
820→856, 18→21 linhas). versionCode 37. CLAUSULA CALMA: só fixes e polish
listado — nenhuma feature fora do escopo, nenhuma física, nenhum scripting.

<!-- (0.8.5 abaixo — histórico) -->

# G.One VV 0.8.5 — funcionalidade desbloqueada no device: animação e2e, primitivas, import

## Escopo 0.8.5 (implementado — campanha F8: funcionalidade quebrada)

**O DIAGNÓSTICO**: três funcionalidades implementadas na F7 morriam no
wiring do device — os testes passavam porque replicavam o fluxo "peça a
peça" (ou setavam à mão a flag que o main nunca punha a true). Seis fixes,
todos com teste de integração:

1. **IMPORT MORTO DESDE A 0.7.2 (a causa-mãe)**: o diálogo "aplicar ao
   TIC?" é despachado por `g_editor.applyAsk && g_applyAsk.open` — o
   `browserImportFile` setava SÓ `g_applyAsk.open`; o único lugar onde
   `st.applyAsk` ficava true eram os TESTES. No C33: importar obj/gltf/glb
   com um TIC selecionado = sem diálogo, sem toast, sem aplicar — e
   `gltfAttachSkin/gltfAttachClips` (skins/clips glTF!) ficavam
   INATINGÍVEIS. FIX: as DUAS flags nos DOIS caminhos de import (browser E
   menu Importar — o 2.º nunca perguntava). O e2e de animação importada
   (test 6 abaixo) corre agora o caminho que o device nunca alcançou.

2. **ANIMAÇÃO "NÃO CRIA"**: `AnimationPlayer::addTrack` ia SEMPRE para
   clips[0] (editClip) enquanto a timeline edita/mostra o clip ATIVO —
   com um clip importado ativo, o "+track" caía no clip INVISÍVEL. FIX:
   o track vai para o `activeClipPtr()` (fallback editClip). Teste: clip
   importado ativo + addTrack → entra NELE, o "edit" fica intacto.

3. **ERRO CLARO PARA FORMATOS NÃO SUPORTADOS** (nunca silêncio): o
   navegador LISTA todos os ficheiros (kind 0 = fora de obj/gltf/glb/png,
   marcados "?"); tocar num .fbx/.psd → toast "formato nao suportado
   ainda: .fbx" + linha no engine.log, sem importar nem ler o ficheiro.

4. **GLTF/GLB MULTI-MESH APLICA**: refs sem `#` com vários meshes
   falhavam ("use path#<i>") mas o browser/catálogo nunca geram sub-refs
   → todo .glb multi-mesh era impossível de aplicar. FIX no WIRING
   (GpuAssets::mesh): fallback para `#0` com log honesto; o contrato do
   ResourceManager fica intacto para refs explícitas (testado).

5. **CATÁLOGO case-insensitive**: o filtro comparava literais
   ("glTF"/"GLB"/"OBJ") e escondia casings mistos (.Glb/.OBJ) que o
   browser aceitava e importava. FIX: classificação CENTRALIZADA no
   `fileapi::kindOfExtension` (lowercase) — o MESMO predicado do browser
   e do teste e2e que já o modelava.

6. **PRIMITIVAS**: o wiring seletor→apply→cache→render estava íntegro —
   os sintomas no device vinham da corrupção de memória da 0.8.4 (fix
   anterior) e do mesmo input. A prova agora é de integração: 3 trocas
   (esfera→cone→box→torus) com upload REAL no stub + drawMesh + round-trip
   `.goni` (a ÚLTIMA troca persiste).

Suíte 519→525 (+6; contrato do browser atualizado: não suportados
VISÍVEIS com kind 0). versionCode 36. CLÁUSULA CALMA respeitada: só fixes
e wiring — nenhuma feature nova, nenhuma física, nenhum scripting.

<!-- (0.8.4 abaixo — histórico) -->

# G.One VV 0.8.4 — estabilização no C33: crashes e freezes domados (wiring)

## Escopo 0.8.4 (implementado — campanha F8: estabilização, ZERO features novas)

**O DIAGNÓSTICO**: os testes unitários passavam (508) e o device crashava —
o gap era WIRING/INTEGRAÇÃO. Três causas raiz, cada uma com o seu teste de
integração que FALHA antes do fix (o do resolver dá SEGV sem o fix — provado
no CI local) e passa depois:

1. **ECRÃ PRETO AO ADICIONAR UI/TIC (corrupção de memória)**: o
   `resolveCanvasLayout` iterava TODOS os elementos do canvas mas escrevia
   `out[i]` SEM respeitar o cap do chamador — o editor usa
   `CanvasLayout lay[32]` NA STACK (`ui/UiEditor.cpp` ×2). Passar de 32
   elementos = escrita fora dos limites = crash/freeze/ecrã preto. FIX: o
   resolver opera SEMPRE dentro do cap (buffers do chamador são autoridade;
   os vetores internos têm o tamanho do cap). Teste: SENTINELA após o
   buffer (40 elementos num lay[32] → guard intacto) + VBox com 33 filhos
   + storm 50 mesh + 50 UI + 10 câmaras ×10 frames.

2. **TEXTO/QUADS QUE DESAPARECIAM ("funções que param")**: os caps FIXOS
   de submissão (kMaxRuns=32 no UiContext, kMaxSubs=32 no Renderer)
   descartavam runs em silêncio e, pior, os GLIFOS são sempre a ÚLTIMA
   submissão — com 32 runs todo o texto saía do ecrã (browser com muitas
   thumbnails, timeline + painéis). FIX: armazenamento DINÂMICO
   (reserve(64) — zero realloc em steady state), NADA se descarta mais; o
   frame anómalo (>66 submissões) LOGA 1× por frame no engine.log. Testes:
   128 runs submetidos na íntegra; 64 runs + glifos = 65 submissões OK.

3. **HEADER DA TIMELINE SOBREPOSTO (botões a lutar pelo toque)**: offsets
   fixos da direita vs "clip:" a x+376 — sobrepunham com strip < 988 px
   (o C33 dá ~952-1000 úteis). FIX: `timeline::headerLayout(r)` — função
   PURA com layout fluido (cluster direito fixo como sempre; "clip:"
   encosta ao play com folga 8 e encolhe 176→120→96 se preciso; o título
   usa o resto). Testes: sem sobreposição a 1600/1000/988/952/800/720/680
   px; a 952 os dois botões que lutavam ficam separados com folga ≥ 8.

**MAIS WIRING AFERVÉVEL (mesma suíte `test_wiring084`, +11 testes)**:
play/stop ×20 com snapshot (pose e UI de editor INTACTAS, players param no
zero); save/load round-trip de cena grande (50 mesh + 50 UI + anim);
steady-state GL — 30 frames SEM um único objeto GL novo (realloc em loop,
o gatilho de freeze, é agora impossível sem o teste falhar); frame-time do
resolver de 50 elementos × 600 frames < 50 ms.

Suíte 508→519 (+11). versionCode 35. CLÁUSULA CALMA: só fixes e wiring —
nenhuma feature, nenhuma física, nenhum scripting.

<!-- (0.8.3 abaixo — histórico) -->

# G.One VV 0.8.3 — animação: blending (peso + crossfade walk→run)

<!-- (0.8.2 abaixo — histórico) -->

# G.One VV 0.8.2 — animação: skinning esquelético (joints + shader com bones)

<!-- (0.8.1 abaixo — histórico) -->

# G.One VV 0.8.1 — animação: import de clips glTF (channels/samplers → clips nomeados)

<!-- (0.8.0 abaixo — histórico) -->

# G.One VV 0.8.0 — animação: timeline + keyframes + AnimationPlayer + primitivas mesh procedurais

<!-- (0.7.10 abaixo — histórico) -->

# G.One VV 0.7.10 — frustum domado (cap visual + hit-test restrito + prioridade de objetos)

<!-- (0.7.9 abaixo — histórico) -->

# G.One VV 0.7.9 — grab-lock dos gizmos (fim da oscilação/"fuga" do drag)

<!-- (0.7.8 abaixo — histórico) -->

# G.One VV 0.7.8 — separação render 3D↔UI no Play (fronteira GL explícita)

<!-- (0.7.7 abaixo — histórico) -->

# G.One VV 0.7.7 — TIC de câmara: frustum wireframe no editor + gizmos + handles + câmara de jogo em Play

<!-- (0.7.6 abaixo — histórico) -->

# G.One VV 0.7.6 — reestruturação UI/UX do editor: toolbar final de 5 grupos + ícones vetoriais + tela de projetos + Theme central

Engine com editor, projeto `.goni` e maturação de assets (compressão ETC2/ASTC
com cache, extração de texturas glTF/GLB, import OBJ/glTF/GLB/PNG, export
OBJ). Mobile-first: arm64-v8a, minSdk 24, landscape travado
(`sensorLandscape`). Devices de teste: Realme C33 (720x1600) e Realme
RMX3624 (Android 13).

Relatórios 1-16 das sub-fases: `docs/RELATORIO-0.7.{4,5,6,7,8,9,10}.md` e
`docs/RELATORIO-0.8.{0,1,2,3}.md` (com os sha256 dos APKs assinados).

## Escopo 0.8.3 (implementado — campanha F7: blending)

**BLENDING COM PESO**: o AnimationPlayer ganha `blendClip` + `blendWeight`
(0 = só o ativo, 1 = só o blend) — o apply mistura os tracks
CORRESPONDENTES (mesmo alvo+elemento) dos dois clips ao peso; TIC
(pos/rot/escala), UI (pos/cor/alpha) e JOINTS misturam. Joints SEM
correspondência no clip de blend fade para o TRS de BIND (agora guardado
nos joints — sem isto o fade congelava no valor animado); o blend é
RUNTIME, nunca serializado.

**CROSSFADE SUAVE**: `crossfade(clip, duração)` anima o peso 0→1 com os
DOIS clips a avançar em paralelo (cada um no seu tempo, mesmo speed); ao
chegar a 1 o clip de destino passa a ser o ATIVO com o tempo CONTÍNUO — a
pose NÃO salta (aferido passo a passo: monotónica em todo o crossfade e o
frame seguinte à troca continua de onde estava). Na timeline: botão
"fade" por clip no seletor (crossfade 0.4 s) e um slider contextual
"blend N%" no header (arrastar assume o peso MANUAL e para o automático).

Suíte 497→508 (+11: peso 0.5 = pose intermédia em pos/rot/scale/UI/joint;
extremos 0/1 + clamps + clips inválidos; o clip de blend anda no SEU
tempo (loop próprio); crossfade peso linear + pose contínua 4t+4t² +
troca sem salto; duração mínima + re-blend; stopBlend fica no ativo;
joint sem track fade ao BIND (re-setBlend mostra o bug do bind animado —
TRS de BIND separados nos joints); blend não serializa; UI fade do
seletor arranca crossfade e completa; slider manual assume o peso).

## Escopo 0.8.2 (implementado — campanha F7: skinning esquelético)

**IMPORT DE SKINS glTF**: o parser lê `skins` (joints + inverseBind
Matrices) e os atributos `JOINTS_0`/`WEIGHTS_0` (4 influências por
vértice) dos primitivas. Os joints chegam REORDENADOS PAIS-PRIMEIRO (a
composição fica iterativa); `assets/GltfAnim::gltfAttachSkin` cria o
`SkeletonComp` do TIC (nome/pai/bind-TRS/IBM por joint — serializado no
`.goni` como componente "Skeleton").

**SHADER COM BONES**: `Mesh::createSkinned` acrescenta um VBO de skin
(aJoints/aWeights, locations 3/4); o vertex shader do LitMaterial ganha
`uSkin` + `uBones[64]` — a matriz efetiva é a soma ponderada das 4
influências (a MESMA conta do `skinVertex` de CPU, aferida no teste). O
Renderer recebe as matrizes por draw (`drawMesh(..., bones, count)`); o
main compõe `computeSkinMatrices` por frame para TICs com mesh skinado +
esqueleto (world hierárquico dos TRS locais × IBM).

**TRACKS DE JOINT**: `AnimTarget` ganha JointPos/JointRot/JointScale
(por NOME de joint) — os canais glTF que apontam a joints da skin entram
nos clips importados (a 0.8.1 saltava-os); o apply escreve o TRS LOCAL
do joint e o render deforma o mesh com a POSE (verificado no stub: bind
pose = identidade; pose rodada move os vértices; mesh skinado DESENHA com
uSkin=1 e uBones uplodeado).

Suíte 489→497 (+8: parser [joints pais-primeiro, IBM, JOINTS_0/WEIGHTS_0],
attach [esqueleto + clips com track de joint, sem duplicar, TIC morto],
pose [bind=I, rodada move vértices — (2,0,0)→(1,0,−1) com 90°Y no filho],
cap 64 bones, mesh skinado desenha no stub [uSkin/uBones/drawElements],
round-trip .goni do Skeleton + joint tracks E a pose reproduz, sem skin
não mexe; registry 8→9).

## Escopo 0.8.1 (implementado — campanha F7: clips glTF)

**PARSER DE ANIMAÇÕES glTF** (`assets/GltfImporter`): o `GltfModel` ganha
`animations` — cada uma com `channels` (nó alvo + path
translation/rotation/scale + sampler) e `samplers` (times do accessor
`input`, values do `output` VEC3/VEC4). Interpolação LINEAR é a
reproduzida; STEP é tolerada como linear; CUBICSPLINE extrai o VALOR do
meio do layout [in, valor, out] (dívidas documentadas).

**CONVERSOR** (`assets/GltfAnim` — `gltfAttachClips`): as animações de um
.glb/.gltf aplicado a um TIC viram CLIPS NOMEADOS no AnimationPlayer
(cria-o se não existe): os canais do NÓ RAIZ mapeiam para tracks
TicPos/TicRot (quat→Euler graus)/TicScale; canais de OUTROS nós são
saltados (joints pedem a 0.8.2 — skinning). O import acontece no fluxo
"aplicar ao TIC?" do navegador (mesh gltf/glb → mesh + clips no mesmo
gesto, com toast "clips importados"); `ResourceManager::model()` expõe o
modelo em cache (1 parse por ficheiro).

**SELETOR DE CLIPS NA TIMELINE**: botão "clip: <nome>" no header → lista
com os clips (nome + nº de tracks + ativo marcado) + "novo (edit)"; o clip
escolhido passa a ser o ATIVO (playback E edição — a timeline toda opera
sobre o clip ativo); trocar de clip recolhe o tempo a zero. Round-trip
.goni dos clips importados (mesma serialização da 0.8.0).

Suíte 481→489 (+8: parser [channels/samplers/times/values/VEC4/CUBICSPLINE
meio], attach [clips nomeados, cria player+transform, acrescenta ao edit
com o importado ativo, sem animações não mexe], playback [o TIC move-se,
loop], round-trip .goni do importado, seletor de clips [troca de ativo,
novo edit reutiliza, edição no clip ativo]).

## Escopo 0.8.0 (implementado — campanha F7: a cena ganha MOVIMENTO)

**ANIMAÇÃO (AnimationPlayer)**: componente novo num TIC com tracks de
keyframes que animam propriedades do TIC dono (pos/rot/escala do
Transform3D — rot em graus Euler XYZ, a convenção do Inspector) e de
elementos de UI do UiCanvas (pos/cor/alpha, por NOME). Curvas por track:
LINEAR (default) ou BEZIER (cúbica por canal com tangentes in/out por
key — handles a ZERO dão um ease suave entre keys; handles proporcionais
ao segmento reproduzem a reta). Playback com modos once/loop/ping-pong e
VELOCIDADE 0.1×–3×; em PLAY o AnimationSystem (grupo Update, ANTES do
TransformSystem, gate `enabled` como a física) avança e aplica todos os
players — ao parar, o PlaySnapshot (agora com elementos de UI) devolve a
pose de editor.

**EDITOR DE TIMELINE** (`ui/Timeline`): strip no FUNDO do viewport
central, só quando o TIC selecionado tem player (o Inspector ganhou
"add Animacao") — NADA sobrepõe os painéis existentes (Hierarchy/
Inspector intactos; o orbit nasce só na área acima da strip). SCRUB
(arrastar o cursor do tempo aplica a pose AO VIVO), keys (diamantes:
tap seleciona, drag move o tempo com clamp entre vizinhas), +key por
row (key NO CURSOR com o valor ATUAL da propriedade — posa-se o objeto,
clica-se +), −key (a mais próxima do cursor), curva por track (lin↔bez),
+track (overlay com os 6 alvos), play/pause/stop com PREVIEW EM SANDBOX
(captura/restaura a pose — o preview nunca suja o editor) e mode/speed.

**PRIMITIVAS MESH PROCEDURAIS** (`render/Primitives`): 8 formas geradas
em código com parâmetros e defaults sensatos — ESFERA, CILINDRO, CONE,
BOX, PLANO (em y=0), TRIÂNGULO/WEDGE (rampa), TORUS, CÁPSULA (altura
total). Novo preset de TIC **"Mesh"** ("+" → Mesh: só Transform+
MeshRenderer com a esfera default, SEM física — prototipagem pura) e
seletor "prim:" no Inspector (grelha mono com as 8 formas + none) com
sliders de parâmetros (raio/tam, altura, segmentos, tubo). O mesh vive
num CACHE por assinatura no main (1 assinatura = 1 objeto GL
partilhado; lifecycle como o cubo — destruído no TERM, rebind lazy).
Serialização: `"mesh":"prim" + "prim":{tipo+parâmetros}` no `.goni`
(round-trip com re-resolução pelo cache).

Suíte 437→481 (+44: animação [interp linear/bezier com handles lineares,
playback pos/rot/scale/UI, once/loop/pingpong, speed, round-trip .goni,
forward-compat, preset Mesh, PlaySnapshot com UI, addTrack idempotente],
primitivas [geometria válida nas 8, winding CCW concordante com as
normais, bbox coerente, params respeitados, clamps, nomes, serialização
tipo+params+exclusividade com cube/file], timeline [visibilidade, rect
sem sobreposição com painéis, scrub aplica pose, add/del key, curva,
preview play/stop restaura, modo cicla, speed, overlay de track, troca
de seleção para o preview]; contratos atualizados: registry 7→8,
plano do Inspector com prim/add Animacao, alturas 750→828/742→820).

## Escopo 0.7.10 (implementado — frustum domado)

**O PROBLEMA (C33)**: o frustum da câmara desenhava-se GRANDE DEMAIS (o
cone escala com `far` — far 500 atravessava o viewport) e o cone ROUBAVA
TOQUES: selecionava a câmara em vez do objeto/orbit, interferindo com o
resto.

**TAMANHO VISUAL CLAMPADO** (`kVisualFarCap` = 12 u): o cone/retângulo do
far desenha-se a `min(far, 12)` — confortável no ecrã e INDEPENDENTE do
far real. O far REAL continua a valer para o RENDER no Play (`gameProj`
lê o CameraComp, nunca a estrutura do gizmo) e vive no Inspector. Far
curto fica real (informativo). Os handles sentam-se no retângulo AO CAP
(partilham a geometria do desenho — o hit-test e o visual nunca divergem).

**HIT-TEST RESTRITO**: a seleção por toque na câmara é SÓ no CORPO+LENTE
(a caixa pequena). Tocar no cone/frustum vazio NÃO seleciona a câmara nem
bloqueia o orbit. Handles (cantos/centro do far) só hit-testáveis com a
câmara JÁ selecionada (como na 0.7.7).

**PRIORIDADE DE OBJETOS** (`pickSceneTic`): o tap do viewport testa
PRIMEIRO os TICs selecionáveis (meshes — centro projetado a 44 px, o
mesmo alvo generoso do grab-lock; o mais próximo ganha) e SÓ DEPOIS a
câmara (corpo/lente). Tocar num objeto DENTRO do cone seleciona o OBJETO
(o fix do C33); a câmara nunca rouba o toque de um objeto.

**TOGGLE "frustum" no Inspector** da câmara (sim/não): esconde o GIZMO
quando polui — a câmara continua na cena e a valer para o render; só o
desenho do editor desaparece. Serializado como `"frustum": false`
(default true omitido — ficheiros 0.7.9 abrem limpos).

Suíte 433→437 (+4: cap visual com far 2000 [gizmo a 12, gameProj com o
far REAL, Inspector vê o real], prioridade [objeto dentro do cone ganha,
invisível passa à câmara, nada → orbit livre, dois objetos → o mais
próximo], handles ao cap [pick no far real → nada], toggle [esconde o
gizmo + serializa/volta]) + contratos atualizados (geometria: far 8 real
+ far 100 clampado + cap explícito; seleção: SÓ corpo/lente com far 500
— cone/far NÃO selecionam; plano do Inspector com a linha frustum).

## Escopo 0.7.9 (implementado — grab-lock dos gizmos)

**O PROBLEMA (C33)**: ao arrastar um eixo do gizmo, o gizmo OSCILAVA e
FOGIA do dedo. Causa raiz tripla no wiring do main: o press edge NUNCA
capturava as âncoras geométricas (`anchorHit` ficava (0,0,0) — o objeto
saltava no primeiro frame para distâncias do hit contra a ORIGEM DO MUNDO;
`anchorAngle`/`anchorDist` ficavam 0 — o rodar saltava o ângulo absoluto
do dedo e o escalar-uniforme estava MORTO pelo guard degenerado) e o hit
de cada frame era medido contra um plano RE-ANCORADO na posição ATUAL do
gizmo (que mexe com o drag → realimentação → oscilação/fuga).

**GRAB-LOCK** (`gizmo::Grab` + `beginGrab`/`grabHit`, em `ui/Gizmo`): no
touch down captura TUDO — o ALVO (eixo/anel/plano/handle), o RAIO (base da
câmara no grab), o PLANO FIXO (⟂ à câmara no grab, passa pela pos do TIC
NO ARRANQUE — nunca pela pos atual), o hit no plano fixo, o ângulo e a
distância do dedo ao centro projetado do arranque. Durante o move NÃO há
hit-test: o delta do dedo é projetado no plano FIXO — o gizmo move-se com
o objeto mas o drag NÃO depende do dedo estar sobre ele. Touch up liberta
o lock. Os HANDLES do frustum da câmara (far/fov) ganham o mesmo lock
(raio/plano/centro do grab — a orbit pode mexer com outro dedo que o
delta não salta).

**ALVO DE TOQUE GENEROSO NO GRAB** (`kGrabPx` = 44 px — o mínimo de
toque do Android): o press edge agarra com 44 px; o HOVER continua fino
(22 px) para o destaque não "acender" meio viewport.

**SNAP NO VALOR FINAL** (não no delta cru): mover aterra em degraus
ABSOLUTOS do grid (âncora fora do grid incluída); escalar aterra nas
componentes FINAIS em passos de 0.25; o escalar-da-câmara (fov/ortho)
aterra em passos de 5°/0.25 no VALOR (o fov do handle).

Suíte 426→433 (+7 grab: âncoras capturadas no arranque [plano fixo,
ângulo, distância, centro], raio generoso agarra a 40 px onde o hover não
destaca, primeiro frame NÃO salta + trava o eixo, drag do eixo Y segue o
dedo com desvio lateral a meio SEM oscilação + idempotente, snap no valor
final com âncora fora do grid, âncora do ângulo (rodar sem salto),
âncora da distância (escalar-uniforme VIVE — estava morto); ajuste do
snap do dragScaleToFov 105→110 [valor final]).

## Escopo 0.7.8 (implementado — fronteira 3D↔UI explícita no render)

**O PROBLEMA (C33)**: no Play com câmara ATIVA e UiCanvas, a UI de jogo
desenhava-se GIGANTE/CORTADA — o pass de UI HERDAVA o estado GL do pass 3D
da câmara de jogo: o `glViewport` só era afirmado no resize (nunca entre
passes), o scissor nunca era gerido, o depth/cull só eram desligados DENTRO
do `endFrame` (e o early-return com 0 submissões nem isso), e a matriz
ortográfica de ecrã usava `w_/h_` em cache — descolada do viewport real.

**FRONTEIRA EXPLÍCITA** (`Renderer::beginUiPass(w,h)`, chamada no main
DEPOIS do render 3D, ANTES de qualquer widget): repõe o viewport CHEIO com
o tamanho ATUAL do frame (o mesmo que o layout lê do EGL), desliga
scissor/depth/cull (a UI nunca é recortada nem ocluída) e refresca `w_/h_`
(a ortográfica de ecrã fica COERENTE com o viewport e com o resolver).
`endFrame` passa a AFIRMAR o estado sempre (mesmo sem submissões — o
early-return antigo deixava o depth do 3D ligado para o frame seguinte).

**RE-SYNC DEFENSIVO** no `CONTENT_RECT_CHANGED`: barras do sistema a
esconder/mostrar podem mudar a superfície SEM `WINDOW_RESIZED` em alguns
OEMs — o contentRect agora re-sincroniza o tamanho do EGL e o viewport do
renderer quando a superfície mudou.

**LAYOUT INTACTO POR CONSTRUÇÃO**: o resolver da UI
(`resolveCanvasLayout`/`elementRect`) usa COORDENADAS DE ECRÃ + safe-area —
não recebe câmara; a UI desenha DEPOIS do 3D, por cima. O `textScale` é
reposto a 1.0 por frame (o Play nunca herda a escala do viewport 2D do
editor).

**REGRESSÕES**: TouchControls corretos no Play; editor/modo UI
inalterados (a fronteira é invisível quando o estado já está certo —
idempotente).

Suíte 418→426 (+8 `test_passgl`: fronteira repõe viewport cheio/scissor/
depth/cull do estado sujado do pass 3D, tamanho ATUAL da superfície,
endFrame afirma estado sem submissões, ortográfica de ecrã coerente, Play
inteiro com câmara ativa + UiCanvas → rects do resolver dentro do ecrã,
resolver NÃO depende da câmara [guarda de contrato], textScale 1.0 no
Play).

## Escopo 0.7.7 (implementado — TIC de câmara + frustum + gizmos)

**COMPONENTE `Camera`** (`components/CameraComp.h`, registado no fim do
ComponentStore — id 6; serializado no .goni como "Camera"): fovY (graus),
near, far, projeção (perspetiva|ortográfica), orthoSize (meia-altura) e
`active` — **UMA câmara ativa por cena** (`core/CameraUtil` assegura o
invariante no load e nos toggles do Inspector). A POSE vem do Transform3D
do mesmo TIC (−Z local = direção de visão, +Y = up).

**FRUSTUM WIREFRAME no editor** (`ui/CamGizmo`): corpo (caixa + lente),
cone de 4 arestas, retângulo do plano far, linha de visão central e
handles nos 4 cantos + centro do far — na COR DE MARCA #8AB4F8, desenhado
pelo line batch dos gizmos. **Só no editor, nunca em Play** (como os
gizmos); a câmara selecionada ganha os handles.

**SELEÇÃO POR TOQUE**: tocar no corpo/frustum de qualquer câmara no
viewport seleciona o TIC dela (hit-test 3D por projeção — a mesma técnica
dos gizmos), além da Hierarchy.

**GIZMOS NA CÂMARA**: mover/rodar atuam no Transform3D (o caminho de
sempre); **escalar ajusta fovY/orthoSize** (o frustum escala — a escala
do transform fica intacta: não tem significado numa câmara). Snap ativo:
o fator salta nos passos dos gizmos; fov arredonda a 5° nos handles.

**HANDLES DO FAR**: arrastar o CENTRO muda `far` (delta projetado no eixo
de visão; snap 1 u); arrastar um CANTO muda `fovY` (fator radial). Com a
câmara selecionada, o hit-test dos handles tem PRIORIDADE sobre os eixos
do gizmo — sem conflitos de drag.

**CÂMARA DE JOGO**: em Play a cena renderiza pela câmara ATIVA (view da
pose + proj dos parâmetros, persp ou orto); sem câmara ativa o fallback é
a orbit de edição. O editor mantém a orbit SEMPRE. A base de movimento
dos controlos segue a câmara de jogo em Play.

**ALINHAR À VISTA**: item novo no menu contextual da câmara (⋮) copia a
pose da orbit de edição para o transform dela.

**INSPECTOR da câmara**: secção Camera com fov/near/far (sliders),
projeção (cicla persp/orto), orthoSize e ativa (uma ativa por cena).
Criação: o "+" do 3D ganha "Camera" (nasce A ativa).

Suíte 407→418 (+11: geometria do frustum [far/near/orto/rodada +
planeHalfExtents], seleção por toque no frustum [centro/cone/vazio/
invisível], gizmo mover/rodar no transform, escalar=fov/ortho com clamps
e snap, handles far/fov [ids, raio prioritário, âncoras, snap, clamps],
serialização round-trip + uma ativa, CameraUtil, play usa a ativa
[gameView/gameProj exatos] + frustum nunca em play + batch, alinhar-à-
vista [pose idempotente], plano do Inspector, criação/menu contextual).

## Escopo 0.7.6 (implementado — reestruturação UI/UX definitiva)

**BARRA SUPERIOR FINAL DE 5 GRUPOS** (a toolbar cresceu orgânica — tudo
texto, tudo ao mesmo nível — e já não escalava). Princípios: texto só
para identidade/ações raras; uso frequente = ÍCONES; grupos com
separador visual; estados exclusivos = segmented control; esconder o que
não se aplica:
- **G1 sistema** `[Menu ▾][Cena ▾]` — Menu abre o dropdown (Settings,
  Guardar, Carregar, Export OBJ, Importar…, Export Downloads, Sair — o
  Settings deixou de ser botão próprio); Cena abre a lista de cenas do
  projeto (o item "Cenas…" saiu do menu de ficheiros);
- **G2 playback** `[pause][play]` — ícones;
- **G3 modo** `[3D][UI]` — segmented, ativo com fundo de marca;
- **G4 transformação** `[mover][rodar][escalar][snap]` — segmented de
  ícones SÓ com seleção ativa em 3D (some da barra, não fica cinzento);
  mover/rodar/escalar exclusivos, snap é toggle;
- **G5 painéis** `[inspector]` — mostra/esconde o painel direito (a área
  junta-se ao viewport central).

**8 ÍCONES VETORIAIS PRÓPRIOS** (Mover/Rodar/Escalar/Snap/Inspector/
Cena/Play/Pause) como POLILINHAS (viewBox 0..24, stroke uniforme)
desenhadas pelo line batch dos gizmos — sem parser de SVG, sem raster.
Cor de marca `#8AB4F8` sobre o fundo da barra; ativo inverte (fundo de
marca + ícone escuro). Nenhum emoji.

**THEME CENTRAL** (`ui/Theme.h`): struct Theme lido pela toolbar/ícones —
cor de marca `#8AB4F8` (exceção DOCUMENTADA ao tema mono dos painéis,
que fica intacto), fundo da barra `#0B0E13`, stroke.

**TELA DE PROJETOS** (lado Java): UMA entrada de criação ([Novo projeto]
no topo — o botão de fundo morreu), + [Importar projeto] (pasta que já é
um projeto .goni) e o cabeçalho "Meus projetos"; a lista mostra SÓ nome
+ data da última edição (o URI — com o prefixo interno "primary:" —
nunca mais aparece; `ProjectsFormat.folderLabel` limpa o prefixo e é
testado na JVM do CI); botões de CONTORNO `#8AB4F8` sobre fundo escuro
(sem o gradiente cinza do tema do sistema).

Layout dinâmico (larguras proporcionais quando não cabe — nada
sobreposto em nenhuma largura). Suíte 401→407 (+7: 5 grupos sem
sobreposição em 2 larguras ±G4, transformação condicional, segmented
exclusivos + snap toggle, ícones dentro do rect/24/32px + espessura
uniforme, Theme central com cores exatas, ações G1/G2/G5, dropdown do
Menu 0.7.6) + testes Java host + check estrutural no CI.

## Escopo 0.7.5 (histórico — fix das falhas de UX do C33 0.7.4)

**Z-ORDER DOS OVERLAYS MODAIS** (o fix do "texto do canvas ATRAVÉS do
MENU"): com um modal aberto (+, MENU, Settings, seletores, diálogo de
armazenamento, import, logs, menu contextual, remoção, teclado, CENAS,
navegador, aplicar) o CHROME DO EDITOR NÃO SE DESENHA — no lugar, um
BACKDROP OPACO tapa o ecrã todo (nada do canvas UI/painéis/toolbar à
mista com o overlay). Os widgets são immediate-mode: não desenhados =
não interativos (os toques só pertencem ao modal, que fecha com toque
fora como sempre). `anyOverlayOpen` ficou público e ganhou os 3 que
faltavam (CENAS/navegador/aplicar não bloqueavam o gesto WYSIWYG).

**TIC DE UI PRÓPRIO + criação direta** (o fix do "criar UI obriga a um
TIC 3D"): no modo UI, criar um elemento SEM TIC selecionado assegura/cria
o TIC **"UI"** (só com UiCanvas — sem mesh/body) que hospeda o canvas e
aparece na Hierarchy como qualquer outro (renomeável/duplicável/
removível). Segundo elemento reutiliza o mesmo TIC. O toast diz
"TIC 'UI' criado + elemento".

**TECLADO COM MINÚSCULAS**: toggle **abc/ABC** visível na linha de baixo
(o espaço encolhe 3u→2u; o rótulo mostra o estado SEGUINTE), minúsculas
para renomear/texto/alvo/nome de cena; dígitos/'_' sem caso; `_`, `-`,
espaço, APAGA, OK, X mantêm-se. **Fix de um bug latente da 0.7.0 apanhado
pelo teste novo**: a linha S..Z tinha o **'Z' em falta** (a 9ª tecla era
um ponteiro NULL — tecla fantasma que crashava ao tocar).

Suíte: 399→401 testes (+ backdrop modal tapa o canvas [com MENU e menu
contextal; reabre o chrome ao fechar], TIC de UI sem seleção [só canvas,
sem mesh/body, reutiliza], teclado minúsculas após toggle [geometria da
linha de baixo com 6 teclas sem sobreposição]). Gates verdes.

## Escopo 0.7.4 (histórico — fix dos gaps de qualidade do C33 0.7.3)

**PARIDADE EDITOR↔PLAY (princípio transversal)** — o que o dono viu no C33
("o Menu mostra caixas no editor e texto solto no Play") tinha causa raiz
TRIPLA no viewport 2D: o texto era desenhado a 28 px CRUS sobre rects
escalados a ~0.4x, os insets/molduras constantes não escalavam e o layout
usava insets ZERO (o Play usa a safe-area real). Tudo corrigido:

- **`textScale` no UiContext**: métricas + EMISSÃO de glifos escalam com o
  viewport — o mini-canvas é o Play REDUZIDO ao pixel (texto, insets,
  molduras, joias de desenho — tudo × escala; no Play k=1);
- **Resolver de layout único** (`resolveCanvasLayout`): âncoras + safe-area
  REAL + containers — o MESMO código alimenta drawCanvas (Play),
  drawUiViewport (editor: draw/hit-test/drag) e hitTestCanvas. Paridade
  ESTRUTURAL, afervida por tipo de elemento no CI (rect-a-rect, editor =
  Play × escala + offset);
- **Joystick com a aparência do Play** no viewport 2D: o núcleo
  `drawTouchControlsAt` (parametrizável por origem/escala) desenha base +
  knob + botão JUMP nos dois modos (o proxy 0.7.3 era outra coisa);
- **Clip ao mini-ecrã**: nada do canvas sangra para os painéis ao lado (o
  Play recorta na borda física do ecrã; o editor recorta igual).

**LABEL = SÓ TEXTO por default** (o fix do "fundo branco fixo"): alpha 0;
fundo OPCIONAL configurável no Inspector (cor com alpha — slider "fundo A"
em todos os elementos). Button mantém o fundo escuro.

**TEXTURAS DE FUNDO em Button/Panel + Image escolhe a imagem**:

- Linha **`tex:`** no Inspector de UI (Panel/Button/Image) → seletor de
  `textures/` + **"importar…"** (abre o NAVEGADOR 0.7.2 — galeria
  incluída; o ficheiro cai em `textures/` e volta ao seletor);
- A ref vive no elemento (`image`); a renderização resolve POR FRAME pelo
  resolver do UiContext (o caminho do Image 0.7.0 — agora partilhado);
  **tint branco×alpha** (antes o Image tingia a textura com o LINE escuro
  e a imagem ficava quase preta);
- Sem imagem: **placeholder claro "(sem imagem)"** centrado (Image sem
  ref) ou com o NOME da ref quando a carga falha (honesto);
- **z-order sólidos↔texturas**: submissão em RUNS na ordem real de
  emissão (antes: grupos fixos — um botão sólido desenhado depois de um
  painel texturizado ficava POR BAIXO dele na tela).

**MENU configurável**: espaçamento entre itens (slider "espaco"), fundo
das caixas ON/OFF (alpha 0 = só texto) e alinhamento do texto
(start/center/end — default start = 0.7.3 exato).

**CONTAINERS VBox/HBox** (o fix do "organizar é posicionar tudo à mão"):

- Filhos (campo `parent` por nome) dispostos automaticamente em
  coluna/linha com **espaçamento + padding + alinhamento transversal**
  (start/center/end) — o filho não precisa de posição; aninháveis;
- **Auto-fit no eixo do conteúdo** (VBox: h; HBox: w) — os filhos nunca
  transbordam; filhos invisíveis COLAPSAM; container invisível esconde os
  descendentes; guard de ciclos (órfãos de topo, sem crash);
- Criação: "+" ganha **10 itens** (+ VBox/HBox); com um container
  selecionado, o elemento novo nasce FILHO; Inspector: "colocar em:"
  cicla os containers e ao SAIR o elemento fica NO SÍTIO onde estava
  (detach conserva a posição); arrastar um filho tira-o do container;
  filhos de container hit-testam no rect DISPOSTO (não no ox/oy);
- **Serialização**: spacing/pad/align/parent (ausentes = defaults
  0.7.3-compat; `.goni` antigos abrem); alpha já vinha no color[4].

Suíte: 386→399 testes (+13 em `tests/test_uilayout.cpp`: label default,
texturas + fallback + placeholder, paridade por tipo (rect-a-rect com
transform exato), containers (layout/auto-fit/visibilidade/ciclos/hit-test
/detach/add-filho), menu espaçamento/fundo/alinhamento, round-trip,
z-order dos runs, applyUiTexPick). Gates verdes.

## Escopo 0.7.3 (histórico — F6: joystick editável + compostos)

**JOYSTICK/TOUCHCONTROLS EDITÁVEL** — o widget de input passa a ser uma
instância EDITÁVEL ("a UI do Player passa a ser esta instância"):

- **Campos do componente** (pos/tamanho/sensibilidade/cor), editáveis no
  Inspector de UI e serializados no `.goni` (quando não-default; os
  `.goni` da 0.6.x abrem com o layout fixo de sempre);
- **Pos em frações da área útil** (resolução-independente; o default
  reproduz o layout fixo 0.6.x no ecrã de referência 1600×720),
  **tamanho** escala o raio (base 75px), **sensibilidade** multiplica o
  eixo (clamp no círculo unitário — sens 1 = o comportamento 0.6.x
  exato), **cor** tinge a base/o knob (default = LINE do tema mono);
- **PROXY no viewport 2D**: o joystick do TIC selecionado desenha-se com
  a MESMA geometria do Play (layoutFor), arrastável (pos, com clamp
  0..1) e selecionável — mesmo SEM canvas (o early-return da dica só
  corre quando não há canvas NEM joystick);
- **"+" do modo UI ganha 8 itens**: Panel/Label/Button/Image +
  Menu/Card/Article + **Joystick** (adiciona o componente ao TIC e
  seleciona-o para edição);
- **O INPUT em Play segue o EDIT**: `touchBegin` claima na posição NOVA
  e o eixo vem escalado pela sensibilidade (aferido no CI);
- **Inspector do joystick**: pos X/Y, tamanho, sensib., cor R/G/B,
  "remover joystick" (o InputMap fica — sem fonte até nova adição).

**COMPOSTOS** (elementos de UI compostos, criáveis pelo "+"):

- **Menu**: lista vertical de botões — o texto é uma linha por item no
  formato "label>alvo" (alvo opcional; sem '>' o alvo é o próprio
  label); cada item dispara a AÇÃO do elemento com o alvo da LINHA;
  hit-test por linha (já aferido na 0.7.0);
- **Card**: panel + moldura + título no topo com separador;
- **Article**: texto multilinha com WRAP pela largura (greedy por
  palavras; nunca sai do rect).

Testes 380→386 (+6 em `test_joystick.cpp`): layout derivado dos campos
(default reproduz o fixo; pos/tamanho mandam; o layout fixo continua),
o edit afeta o INPUT (claim na posição nova; eixo × sens com clamp; sens
1 = 0.6.x), serialização (round-trip dos campos; `.goni` 0.6.x abre com
defaults), proxy no viewport 2D (sem canvas; tap seleciona; drag move
relX/relY com clamp; plano do inspector com y cumulativo; remover tira o
componente), compostos (Menu 3 itens desenha; Article faz wrap — dezenas
de glifos dentro da largura; round-trip completo; uiAddElement aceita
4..6 e recusa 7), "+" com 8 itens (Joystick adiciona e seleciona).

## Escopo 0.7.2 (histórico — F6: import robusto — navegador + galeria + aplicar)

**NAVEGADOR DE FICHEIROS IN-APP** (Importar… abre o NAVEGADOR — substitui a
lista fixa Download/Documents da 0.6.x):

- **Navegar QUALQUER pasta** do armazenamento (com o All Files Access
  concedido): diretorias primeiro (ordenadas), ficheiros só os suportados
  (.obj/.gltf/.glb/.png — case-insensitive); subir com "^ Subir" (a raiz
  fica na raiz); a lista faz scroll (id 45) quando excede 8 linhas;
- **A GALERIA nas raízes navegáveis**: [Raiz][Download][Docs][Camera]
  [Pictures] — DCIM/Camera (fotos da câmara) e Pictures entram como raízes
  próprias (um toque chega);
- **O CAMINHO é VISÍVEL no topo** do overlay: o caminho inteiro quando
  cabe; quando não cabe corta o INÍCIO e guarda o FIM ("…DCIM/Camera" — é
  onde o dono está);
- **Pasta vazia/sem acesso → mensagem COM O CAMINHO** dentro do overlay
  (nunca um toast cego); o opendir falho também loga o errno no engine.log;
- **Aplicar-após-import**: ao importar com um TIC selecionado (com
  MeshRenderer), a engine PERGUNTA "Aplicar ao TIC?" — Sim aplica já a
  textura/mesh pelo MESMO caminho do seletor do Inspector
  (`applyAssetPick`: ref do projeto + resolvers de GPU + toast/log
  honestos); Nao deixa só importado (aplicável depois nos seletores).

Testes 372→380 (+8 em `test_browser.cpp`): `listDirEntries` (diretorias
primeiro ordenadas, ficheiros suportados com .PNG case-insensitive,
subpastas, pasta inexistente → false), `parentPath`, raízes com a galeria,
rótulo do caminho (inteiro quando cabe; corta o início guardando o fim;
nunca excede a largura), mensagem vazia COM O CAMINHO, overlay (raízes
1..5 / subir 6 / entradas 7+; pasta vazia desenha a mensagem; fora
fecha), diálogo aplicar (Nao não mexe; Sim liga texture+texPath pelo
caminho do seletor) e e2e do import (listar → ler → gravar no projeto →
catálogo → aplicar no TIC).

## Escopo 0.7.1 (histórico — F6: cenas múltiplas + transições)

**CENAS MÚLTIPLAS** — cada cena é um `.goni` próprio no manifesto do
projeto (`Project::scenes`; a estrutura já existia desde a F5 — a 0.7.1
traz a UI e o fluxo completo):

- **Overlay CENAS** (Menu → "Cenas…"): lista as cenas do projeto com a
  ATIVA marcada, botão "+ Nova cena" (teclado in-app — zero IME) e troca
  com um toque; a lista faz scroll quando excede 6 linhas;
- **Criar cena**: guarda a cena ATUAL no ficheiro dela, regista a nova no
  manifesto (ativa), escreve o `.goni` VAZIO dela e persiste o manifesto;
  duplicados são recusados com toast claro (o guard impede que a cena
  existente seja substituída pela vazia);
- **Trocar de cena**: guarda a atual → muda a ativa → persiste o manifesto
  → carrega a nova (refs relativos re-ligam pelo LoadCtx canônico); a
  seleção não sobrevive à troca; falha de save aborta SEM trocar (a cena
  atual fica intacta);
- **Persistência**: fechar/reabrir mantém a cena ativa (manifesto).

**TRANSIÇÕES (fade / slide)** — `ui/SceneFx.h` puro e afervel:

- **`Scene.Transition` como ação declarativa** (novo tipo de ação do
  Button/Menu, com alvo = nome da cena e estilo `fade`|`slide` no
  Inspector de UI — botão "estilo" cicla; serializa como `act: "trans"` +
  `param`);
- **`Scene.Load` = troca instantânea** (ação `scene`, como na 0.7.0 —
  agora ligada ao carregador real);
- **Em Play** a transição tapa o ecrã e faz o SWAP no PONTO MÉDIO (nunca
  se vê a troca a seco): fade = quad preto fullscreen com alpha em rampa
  0→1→0; slide = quad que cobre vindo da esquerda e sai pela direita;
  duração 0.6 s; o overlay desenha-se por cima de TUDO (também no editor,
  para transições futuras);
- **Honestidade**: cena inexistente → toast claro; sem carregador →
  "sem carregador" (nunca sucesso falso).

Testes 367→372 (+5 em `test_scenes.cpp`): criar/trocar/persistir cenas
e2e no FakeStorage (A→B→A com round-trip de TICs e reopen do manifesto),
transição pura (swap UMA vez no ponto médio, cover em rampa, inativa =
custo zero), draw fade/slide nos batches (fullscreen/alpha; ida pela
esquerda, volta pela direita), overlay CENAS (lista/marca ativa/troca/
nova/fora fecha) e ações Scene.Load (instant) / Scene.Transition
(fade default, slide por param) + round-trip `.goni` com `act: trans`.

## Escopo 0.7.0 (histórico — F6: UI criável core + editor de UI + gestão de TICs)

**UI CRIÁVEL (componente `UiCanvas`)** — qualquer TIC pode ter um canvas
de elementos 2D desenhados POR CIMA da cena em Play (pass UI sem depth —
a UI nunca é ocluída):

- **Elementos**: Panel, Label, Button, Image (textura do projeto via ref
  relativa — placeholder mono com moldura+diagonal quando a textura não
  existe/ainda não carregou);
- **Ancoragens 3×3** (esquerda/centro/direita × topo/meio/fundo): o rect de
cada elemento vive relativo ao PONTO DE ÂNCORA da safe-area — resolução-
independente (aferido em 2 resoluções no CI); mudar a âncora PRESERVA a
posição absoluta (`setAnchor`);
- **Ações declarativas on-click**: mostrar/esconder/alternar panel (por
  NOME, em qualquer canvas), carregar cena (alvo validado contra o
  manifesto; o carregamento em Play chega na 0.7.1 com as transições — o
  toast é HONESTO, nunca finge sucesso) e spawn de preset
  (PlayerBody3D/CharacterBody3D/StaticBody3D/RigidBody3D);
- **Hit-test em Play**: o TOPO ganha (z-order), só Button/Menu são
  interativos; o toque num botão da UI é RECLAMADO (não vai aos
  TouchControls nem à câmara); o press ARMA e o release DENTRO do mesmo
  elemento dispara (o gesto clássico de botão);
- **Serialização**: `UiCanvas` completo no `.goni` (elementos com kind/
  nome/texto/rect/cor/visível/âncoras/ação+alvo) — builds antigas IGNORAM
  o componente (forward-compat do serializer).

**EDITOR DE UI DEDICADO (separador "3D | UI" na toolbar)**:

- O botão "UI" troca o viewport central: deixa de mostrar a cena 3D e
  passa a mostrar o canvas do TIC selecionado em ESCALA-CABER (nunca
  corta; o espaço de design é o ecrã inteiro) com moldura do "ecrã";
- **WYSIWYG**: tap seleciona o elemento sob o dedo (o de CIMA ganha),
  drag MOVE (ox/oy pelo delta em design px), tap no vazio DESSELECCIONA;
  os gizmos e o orbit ficam DESLIGADOS no modo UI;
- **Inspector de UI** (painel direito, quando há elemento selecionado):
  pos x/y, largura/altura, cor R/G/B, visível, âncoras (ciclam), texto
  (teclado in-app), ação + alvo, remover elemento — plano com y cumulativo
  (o contrato F5.0-fix) e scroll;
- **"+" do modo UI cria elementos** (Panel/Label/Button/Image; o canvas é
  criado à primeira no TIC selecionado) e SELECIONA o novo — WYSIWYG
  imediato.

**GESTÃO COMPLETA DE TICs**:

- **Desselecionar**: tocar no VAZIO do viewport (tap parado — drag continua
  a orbitar) ou da Hierarchy limpa a seleção;
- **Menu contextual** (botão "..." na linha da Hierarchy — o long-press da
  spec é o atalho alternativo; o botão é determinístico e aferível):
  **Renomear** (teclado in-app) / **Remover** (diálogo de CONFIRMAÇÃO —
  substitui o apagar sem aviso) / **Duplicar** (todos os componentes por
  valor + nome único Godot-style ".001") / **Visibilidade**;
- **Visibilidade**: toggle "O/X" na Hierarchy + checkbox "visivel" no
  Inspector; TIC invisível NÃO desenha em editor nem em Play — a FÍSICA e
  a LÓGICA continuam (invisível ≠ desligado);
- **Cor por TIC**: sliders R/G/B no Inspector (MeshRenderer) — tint
  multiplicativo no shader lit (`uTint`; branco default = comportamento
  0.6.x byte a byte);
- **Teclado in-app**: A-Z, 0-9, '_', '-', espaço, APAGA, OK/Cancelar —
  ZERO IME de sistema (frágil em NativeActivity); geometria partilhada
  com os testes (sem sobreposição, dentro da safe-area).

**Serialização nova**: `visible` (TIC; ausente = visível), `tint` (R/G/B;
ausente = branco), componente `UiCanvas`. Testes 337→367 (+30):
âncoras em 2 resoluções, hit-test (topo/interativos/invisíveis), ações
declarativas (toggle por nome, spawn, cena honesta), menu por linha,
desenho sem sobreposição, round-trip `.goni` completo, cenas 0.6.x a
abrir com defaults, toggle 3D|UI, viewport 2D (scale-to-fit, WYSIWYG
seleciona/move/desseleciona), plano do Inspector de UI, teclado
(geometria + digitar/apagar/OK/cancelar/vazio/sufixo único), menu
contextual (4 ações), remoção com confirmação, desselecionar (viewport
tap≠drag, vazio da Hierarchy), cor por TIC (sliders + bind no stub GL),
toolbar sem sobreposição em 2 larguras.

## Escopo 0.6.10 (histórico — fix do seletor de textura do C33)

**WIRING DO SELETOR DE TEXTURA DO INSPECTOR**: no C33 (0.6.9), após
importar um PNG (import OK, `textures/screenshot-….png` no projeto) e tocar
`tex:` escolhendo a imagem, NADA acontecia — o estado ficava `tex: none`,
o cubo continuava cinzento e o engine.log não registava linha de aplicação
nem de erro. CAUSA RAIZ: `drawAssetMenu` fecha o seletor NO CLIQUE
(`st.assetMenu = 0` antes do return) e o dispatch do main lia
`g_editor.assetMenu` DEPOIS da chamada → sempre 0 → o bloco `if (pick >
0)` era **código morto desde a F5-E (0.5.0)** — a escolha nunca chegava ao
MeshRenderer (o mesmo no seletor de mesh):

- **Dispatch puro e afervel** — `editor::applyAssetPick` (ui/EditorUi):
  escolher textura → atualiza `texture` + `texPath` do MeshRenderer do TIC
  selecionado (o render é immediate-mode: `drawTics` lê `mr->texture` a
  cada frame → o bind acontece no frame seguinte, sem flags dirty);
  escolher mesh → `mesh`/`material`/`meshPath` + textura embutida do
glTF/GLB quando existe;
- **Fix da ordem** — o `menuKind` é capturado ANTES do `drawAssetMenu`
  (o seletor fecha-se a si próprio no clique; o protocolo da sequência está
  travado por teste com um clique REAL injetado);
- **Remover textura** — `none` no seletor liberta a referência (Inspector
  volta a `tex: none`, cubo cinzento);
- **Falha honesta** — carga que falha mantém o estado ANTERIOR intacto +
  toast + linha `FALHOU` no log;
- **Logging** — `material: textura aplicada <ref>` / `material: textura
  removida` no engine.log (visível no log viewer do C33);
- **Persistência intacta** — a ref já era gravada no `.goni`
  (`SceneSerializer`): Save → Load preserva a textura aplicada (o
  re-resolve via `LoadCtx` já existia);
- **Testes CI** — `test_assetpick.cpp` (11 casos): escolher muda o estado,
  sequência do main (o bug da ordem), bind confirmado no stub GL
  (`glBindTexture` + `uHasTex=1/0`), round-trip `.goni`, remover volta a
  none, aviso do gate no toast, alvos inválidos sem crash.

## Escopo F5.5 (histórico — fix do All Files Access no C33)

**MANAGE_EXTERNAL_STORAGE DECLARADO** (fix do C33 0.6.9): sem a declaração,
o sistema não tinha o que conceder — a app não aparecia na lista "Acesso a
todos os ficheiros" (o Godot aparecia "Permitida") e o toggle na página da
app não concedia nada (`isExternalStorageManager()` sempre `false`):

- **Manifest**: `<uses-permission android:name="android.permission.
  MANAGE_EXTERNAL_STORAGE" />` — sem física/render/componentes (CLÁUSULA
  CALMA; decisão e implicação Play Policy documentadas em
  `docs/SAF_EXCEPTION.md`);
- **Fluxo mantido**: diálogo in-app → "Permitir" →
  `ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION` → voltar → re-verificar
  `isExternalStorageManager()` → prosseguir import/export;
- **Re-verificação no resume** (`APP_CMD_RESUME`): drena a fila do
  `onActivityResult` primeiro; se o retorno nunca chegou (ecrã OEM sem
  `setResult`), verifica fresco e decide — `storage::resumeRecheck`
  (política pura, afervel no CI). Recusa → toast claro SEM loop;
- **Log de transição**: `storage: all-files granted=1/0` no engine.log a
cada transição (boot / retorno / resume / variação por tentativa) — visível
no log viewer;
- **Gate CI**: o manifest BINÁRIO do APK passou a ser aferido (aapt2) — a
  permissão tem de estar presente em todo build.

## Escopo 0.6.9 (histórico)

**GIZMOS DE TRANSFORMAÇÃO — Mover / Rodar / Escalar** (só no TIC
selecionado, SÓ em EDITOR — nunca em PLAY):

- **Mover:** 3 setas por eixo + 3 quads de plano (XY/XZ/YZ) — drag ao longo
  do eixo (projeção no eixo; ruído fora do eixo é ignorado) ou no plano
  (decomposição nos 2 eixos do plano);
- **Rodar:** 3 anéis por eixo — drag angular no plano do anel (ângulo do
  dedo em torno do centro projetado; sinal corrigido pela orientação do
  eixo face à câmara; rotação GLOBAL — pré-multiplicação);
- **Escalar:** 3 handles por eixo + handle central uniforme (arrasto
  radial por distâncias em px ao centro); clamp em 0.05 (nunca
  zero/negativo);
- **Hit-test 3D:** raio do toque contra eixos/anéis/handles — distâncias
  em PX de ecrã sobre a geometria PROJETADA (o MESMO critério do desenho);
  eixo sob press DESTACADO (branco ACCENT + traço mais grosso);
- **Seletor de modo na toolbar** (grupo à direita — os 3 botões
  Menu/Play/Settings ficam INTACTOS): [Mover][Rodar][Escalar] + [Snap]
  (toggle);
- **Snapping opcional:** mover ao grid de 0.5 u, rodar a 15°, escalar em
  passos de 0.25;
- **Gestos:** drag em gizmo NÃO orbita a câmara (o slot que apanha o
  gizmo é reclamado — a máscara vai ao `editor::updateCameraOrbit`); drag
  fora do gizmo = orbit normal;
- **Escrita no Transform3D** (pos/rot/scale) com âncoras (pose final =
  âncora + delta — o jitter nunca se acumula) + `updateWorld()` imediato;
- **CORES DE EIXO (X vermelho / Y verde / Z azul): EXCEÇÃO DOCUMENTADA ao
  tema mono, SÓ nos gizmos 3D** — a UI 2D mantém o tema mono intacto;
- **Desenho:** projeção da geometria 3D para segmentos de ecrã
  (`QuadBatch::line` novo — retângulo rotacionado), SEM depth (sempre
  visível) e por baixo dos painéis (z-order de editor); tamanho de ecrã
  CONSTANTE (~16% da distância da câmara).

O núcleo (projeção/raio/hit-test/drag/snapping) é GL-free e testado no CI
Linux (26 testes novos em `tests/test_gizmo.cpp`).

## Escopo 0.6.8 (histórico)

**PLAY MODE COM JANELA PRÓPRIA.** Dois modos de UI com transição Play/Stop:
o **EDITOR** (toolbar de 3 botões + Hierarchy + Inspector + menus — intactos)
e o **PLAY** (viewport fullscreen + TouchControls ancorados na safe-area +
barra superior mínima). Em PLAY: painéis de edição e toolbar ESCONDIDOS
(early return no frame — nunca desenhados); o botão Stop (id 5, faixa
exclusiva) devolve ao editor com a pose restaurada (PlaySnapshot intacto) e
os painéis repostos exatamente (scroll/seleção vivem fora das flags de
overlay). A barra PLAY mostra o estado "a correr · fps N", o botão Stop e o
aviso "simulação — alterações descartadas ao parar" (labelFitted — nunca
sai da barra). O ORBIT fica DESATIVADO em play (1 dedo = controlos): a
lógica de orbit foi extraída do main para `editor::updateCameraOrbit`
(`OrbitState` puro, regra F3 do dono-do-gesto e pinch intactos) com guard
`playMode` que reseta o gesto pendente — o orbit volta a funcionar ao sair.
O estado do modo vive em `EditorState.playMode` (não numa global do main) —
a transição completa editor→play→editor é testável na suíte.

## Escopo 0.6.7 (histórico)

**1 — LIFECYCLE GL (fix dos "cubinhos" do C33).** Sair do editor
(home/recents) e reentrar SEM matar a app deixava todo o texto em quads
brancos: o `APP_CMD_TERM_WINDOW` destrói o contexto EGL (o
`EglContext::shutdown` mata surface E contexto — todos os ids GL ficam
órfãos), mas o `FontAtlas` guardava `tex_ != 0` (guard "já carregado") e o
2º `APP_CMD_INIT_WINDOW` NUNCA re-upava o atlas → o pass UI amostrava uma
textura órfã = quads brancos. O mesmo acontecia aos mapas do `GpuAssets`
(`releaseAll()` existia mas ninguém chamava). FIX: `FontAtlas::destroy()`
novo (`glDeleteTextures` + reset do id E das métricas — chamado no
TERM_WINDOW com o contexto ainda corrente; o guard passa a impedir só o
upload duplicado no MESMO contexto); TERM_WINDOW com ordem obrigatória
detach dos MeshRenderers → `g_gpu.releaseAll()` → `g_font.destroy()` →
cubo/grid → renderer → `g_egl.shutdown()` POR FIM; INIT_WINDOW loga a
RE-CRIAÇÃO do contexto e confirma o RE-BAKE + RE-UPLOAD. NENHUM recurso GL
é assumido vivo entre term/init (a cena é recarregada do disco e os
resolvers re-upam os assets).

**2 — APAGAR PROJETO no gestor.** Long-press numa entrada → diálogo com o
nome e a pasta e DUAS ações: "Remover da lista" (a pasta fica — como
antes) e "Apagar projeto" → confirmação EXPLÍCITA separada ("Apagar
projeto X? Não pode ser desfeito — a PASTA e TODOS os ficheiros são
apagados") → `VvProjects.deleteProject` novo remove a pasta via File API
(`DocumentsContract.deleteDocument` no URI de documento da raiz) + sai da
lista; a permissão persistente é libertada e a lista refresca.

**3 — "Sair para projetos" no editor.** Menu → 6º item: auto-save da cena
(cena + manifesto + assets de runtime materializados) → a activity
termina-se (`VvActivity.bridgeFinish` novo, chamado por
`jniFinishToLauncher` na ponte JNI) e o GESTOR retoma da back stack SEM
matar a app. Reentrar arranca um NOVO `android_main` — que agora faz RESET
de estado na entrada (TickGroups.clear() novo — sem isto a física seria
registada 2× e daria dois passos por frame; InputState.resetAll() novo;
cena/editor/play/projeto/catalog resetados) com um contexto EGL novo
(re-upload de tudo — o fix 1 cobre a reentrada).

**4 — fix do nome do projeto invisível.** O diálogo de novo projeto herda
o tema CLARO do manifest (`Theme.NoTitleBar.Fullscreen`) → painel branco;
o EditText tinha só texto quase-branco = texto branco sobre fundo branco
durante a digitação. Fix determinístico sem `res/`: fundo escuro
explícito (0xFF1E222A) + texto claro + hint + `requestFocus()` pós-show.

## Escopo F5.4 (histórico)

**PARTE 1 — fix do `UnsatisfiedLinkError` (RMX3624, Android 13).** O
logcat mostrava `No implementation found for void
vv.goni.VvActivity.nativeRegisterActivity(...)` no onCreate E no onResume —
o handshake nunca ficava OK e todo import/export falhava com "ponte Java
indisponível (handshake)". Causa raiz (docs/HANDSHAKE_AUDIT.md §6): o
`android.app.NativeActivity` carrega a lib com `dlopen(RTLD_LOCAL)` CRUO
(`loadNativeCode_native`) — um dlopen cru NÃO corre o `JNI_OnLoad` nem
registra a lib no mapa de resolução do JVM, logo o `RegisterNatives` da
0.6.3 nunca correu no device e a busca por nome não via a lib (a premissa
"super.onCreate faz System.loadLibrary" era FALSA — o hospedeiro testava o
C++ direto, a RESOLUÇÃO não era modelada). FIX em 2 camadas: (1)
`static { System.loadLibrary("goni_vv"); }` na VvActivity — o JNI_OnLoad
corre de verdade, o RegisterNatives executa e a lib entra no mapa do JVM;
(2) `ensureNativesRegistered()` idempotente no 1º nativeRegisterActivity
via GetObjectClass + `JNI_OnLoad` TOLERANTE (FindClass/RegisterNatives a
falhar NUNCA devolvem JNI_ERR — logam e adiam para a 2ª camada). GATE NOVO
pedido pelo dono: `scripts/jni_parity.py` no CI afere todo native da
VvActivity.java contra a tabela RegisterNatives (nome+assinatura), o static
loadLibrary obrigatório e, no release, os símbolos `Java_vv_goni_VvActivity_*`
no `.dynsym` do .so real — na 1ª execução apanhou um bug latente real
(assinatura de nativeOnActivityResult com um 'I' a mais; corrigida).

**PARTE 2 — Gestor de Projetos (estilo Godot, múltiplas pastas).** A app
abre agora no ecrã **"Projetos"** (ProjectManagerActivity, launcher novo):
lista dos projetos já criados (vazia na 1ª instalação), toque abre direto
no editor, long-press remove da lista (a pasta NÃO é apagada). "+" → nome →
seletor de pastas do sistema (SAF, `ACTION_OPEN_DOCUMENT_TREE`) → a pasta
de CADA projeto é escolhida por ele: cada escolha gera um URI próprio,
guardado com `takePersistableUriPermission` — projetos diferentes em
pastas diferentes, NUNCA se misturam, MESMO SEM All Files Access. A
estrutura (project.goni, scenes/, meshes/, textures/) é criada na pasta
colhida e o editor abre esse projeto (SafStorage: ProjectStorage sobre a
árvore SAF — o editor não sabe a diferença). A lista vive em
`projects.json` no app-private (não depende do handshake nem de
permissões). O All Files Access mantém-se EXATAMENTE como estava, só para
import/export de assets soltos DENTRO de um projeto aberto
(Download/Documents) — as duas coisas COEXISTEM.

## Escopo F5.3 (histórico — handshake invertido)
Objetivo: a ponte Java↔native LIGAR DE VERDADE em runtime (handshake
INVERTIDO: a activity Java registra-se no native), para o fluxo All Files
Access funcionar no C33: diálogo → settings do app → permissão ativada →
File API direta. Mensagens de erro honestas (causa real).

No C33 (0.6.2), o engine.log mostrava `jni: initJava env/activity
indisponíveis (vm=0x…)` → `supported=0 manager=0 modo app-private` →
`export falhou (copied=0)`. Causa única: o `android_main` corre no thread do
glue (pthread) que NÃO está anexado à VM — `GetEnv` devolvia `JNI_EDETACHED`
e a ponte morria no arranque. As auditorias estáticas (manifest/dex/símbolos)
estavam corretas — o bug era o handshake runtime
(docs/HANDSHAKE_AUDIT.md tem o despejo do manifest do APK 0.6.2 REAL).

1. **APK real verificado (item 1)** — o 0.6.2 do CI despejado:
   `hasCode=true`, launcher=`vv.goni.VvActivity`, `lib_name=goni_vv` —
   o sistema instancia a VvActivity; o dex não tinha onCreate/onResume
   (nunca se registrava). Gate NOVO no CI (`aapt2 dump xmltree`) afere o
   manifest binário em todo build.
2. **Handshake invertido (item 2)** — `VvActivity.onCreate()` chama
   `nativeRegisterActivity(this,"onCreate")` (após `super.onCreate`, quando
   o loadLibrary/JNI_OnLoad já correu); `onResume()` reforça (idempotente —
   GlobalRefs substituídos). O native guarda `JavaVM` + `GlobalRef` + métodos
   e loga: `java: onCreate → nativeRegisterActivity` →
   `native: activity registada` → no boot `handshake=1`. A chamada
   `initJava` do android_main foi REMOVIDA (o native não descobre a
   activity sozinho — era a causa).
3. **Attach de threads (item 3)** — qualquer thread da engine que chame
   Java faz `AttachCurrentThread` NOMEADO (`goni-engine`, via
   `JavaVMAttachArgs`) e PERMANENTE (sem Detach); decisões na tabela pura
   `platform/JniAttach.h` (JNI_OK→usa; EDETACHED→attach; resto→falha com o
   CÓDIGO no log). Nenhum `JNIEnv*` assumido não-nulo.
4. **Fluxo All Files pós-handshake + mensagens honestas (item 4)** —
   diálogo → `ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION` →
   `isExternalStorageManager()` no retorno → File API direta. Se o
   handshake falhar, toast/log diz **"ponte Java indisponível (handshake)"**
   — NUNCA "sistema sem All Files Access" (0.6.2 mentia sobre a causa;
   `blockReason`/`blockMessage` em `StoragePerm.h` distinguem ponte vs
   sistema, testados no CI).
5. **Testes CI (item 5)** — `tests/test_handshake.cpp` + a ponte JNI
   INTEIRA compilando na suíte com um fake JNI controlável
   (`tests/stub/jni.h`): handshake simulado (o teste chama
   `nativeRegisterActivity` — o papel do "stub Java"), re-registro
   onResume, registo parcial, attach com/sem falha (rc logado), fluxo de
   permissão completo (diálogo → intent 4301 → onActivityResult → fila →
   Granted + ação retomada), regressão das mensagens honestas e
   `JNI_OnLoad` com 2 nativos. 254→264 testes.

## Escopo F5.2 (implementado)
Objetivo: o fluxo correto de permissões de armazenamento (o mesmo do Godot) +
diagnóstico in-app — no C33, o 0.6.1 mostrava "SAF indisponível" sem nunca
pedir permissão; o SAF tree picker foi REMOVIDO.

1. **Diálogo in-app (item 1)** — ao tentar Importar/Export pela primeira vez:
   overlay mono "ARMAZENAMENTO — Precisa de acesso a todos os ficheiros para
   importar/exportar projetos" com **Permitir/Cancelar** (mensagem quebrada
   pela fonte real; toque fora fecha). Máquina de estado GL-free em
   `platform/StoragePerm.h` (testada no CI: diálogo só na 1ª tentativa,
   cancelar volta a Idle, sem nag em loop).
2. **Abrir settings (item 2)** — "Permitir" → `VvActivity.openAllFilesSettings(4301)`
   lança **`ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION`** com `package:vv.goni`
   (a janela de permissões DO app). Constante `kSettingsAction` afervel no CI.
3. **Verificar + File API direta (item 3)** — o retorno chega por
   `onActivityResult` → fila JNI → thread da engine → `Environment.isExternalStorageManager()`
   re-verificada; concedido → **File API POSIX direta**: import varre
   `Download/` + `Documents/` (obj/gltf/glb/png, ordenado, overlay de escolha)
   e copia para `meshes/`/`textures/`; export grava
   `Download/GOneVV/export/export_<nome>.obj` (visível no gestor).
4. **Log viewer in-app (item 4)** — Settings → "Ver logs": viewer com SCROLL
   (id 43, auto-scroll para o fim) desenha o tail do `engine.log` + a lista de
   `crash-*.dump` — **funcional sem export**.
5. **Boot self-check (item 5)** — `fileapi::logStorageSelfCheck` no arranque:
   `getExternalFilesDir` (null?) + errno de cada opendir/fopen falhado no
   `engine.log` — a CAUSA de qualquer falha de storage fica registrada.
6. **Fallbacks (item 6)** — recusa ou API < 30 → modo **app-private**
   (`getExternalFilesDir`, sem permissões); o Settings mostra sempre
   "armazenamento: all files / app-private" + botão "Acesso a ficheiros…".
7. **Testes CI (item 7)** — 26 novas verificações no hospedeiro: constantes
   do fluxo, resolveMode, diálogo (mostrado 1×/permitir/cancelar/negado),
   intent (consumeOpenSettings), permissão verificada (onSettingsReturn +
   ação retomada), fallback sem suporte, File API (lista/roundtrip/errno),
   self-check, readTail/listDumps, e 4 testes de UI (diálogo, import,
   viewer, settings) com fonte real e taps injetados.

SAF removido: `SafIo.java`, `SafIoJni.cpp`, `core/SafStorage` e o item
"Pasta (SAF)" saíram; `docs/SAF_EXCEPTION.md` documenta o All Files Access
como caminho primário (com nota de Play Policy). A ponte Java mínima
(VvActivity) continua — `NativeActivity` não lança a janela de permissões nem
reencaminha `onActivityResult`.

## Escopo F5.1-hotfix (implementado)
Objetivo: o 0.6.0 crasha no arranque no C33 e o dono não tem PC/logcat — a
engine passa a **autodiagnosticar-se em ficheiros legíveis** e a ponte JNI/SAF
foi auditada e corrigida (docs/AUDIT_SAF_JNI.md).

1. **Log writer (parte 1.1)** — `vv::elog::info/warn/error` faz DUAS coisas:
   `__android_log_write` (logcat) + append a `Android/data/vv.goni/files/logs/`
   (= `getExternalFilesDir("logs")`) com rotação por tamanho: 3 ficheiros de
   1MB (`engine.log` + `.1` + `.2`, o mais velho descartado). Thread-safe;
   ativo desde a 1ª linha (o `JNI_OnLoad` usa o caminho fallback antes do
   `android_main`).
2. **Boot progress (parte 1.2)** — cada passo crítico do arranque escreve um
   marcador: `[boot 2/6] storage`, `[boot 5/6] physics` (android_main),
   `[boot 1/6] contentRect`, `[boot 3/6] fonts`, `[boot 4/6] renderer`,
   `[boot 6/6] scene OK → editor` (INIT_WINDOW). O dono vê EXATAMENTE onde o
   boot parou.
3. **Crash dump (parte 1.3)** — handlers C++ para SIGSEGV/SIGABRT/SIGBUS/
   SIGFPE (SA_ONSTACK + sigaltstack): recolhem o stacktrace via
   `_Unwind_Backtrace` (bionic `backtrace()` só existe API 33+; C33 é 31/32)
   e resolvem módulo+função+offset com `dladdr()` → `crash-<timestamp>.dump`
   no MESMO diretório, formato legível sem ndk-stack
   (`#03 pc 0x1a2b3c  libgoni_vv.so (função+0x88)`) + pc/lr/sp do fault
   (arm64). Ativo em TODAS as builds (release incluída); re-raise mantém o
   tombstone do sistema.
4. **Export de logs (parte 1.4)** — botão **Settings → "Exportar logs"**:
   copia TODOS os ficheiros de `getExternalFilesDir("logs")` para
   `Downloads/GOneVV/logs/` via MediaStore (API 29+, SEM
   WRITE_EXTERNAL_STORAGE; export repetido substitui). Caminho definido pela
   constante única `vv::elog::kDownloadsRelPath` (afervel no CI).
5. **Auditoria JNI/SAF (parte 2)** — evidência no APK 0.6.0 real: manifest,
   dex e símbolos OK (crash é runtime). Fixes: `JNI_OnLoad` com
   `RegisterNatives` explícito (falha morre no arranque COM log), higiene de
   exceções JNI em toda a ponte, resultado SAF DIFERIDO para o thread da
   engine (o handler antigo corria no thread da UI e chamava GL sem contexto
   EGL), guardas nulos + logging por método. Detalhe em
   **docs/AUDIT_SAF_JNI.md**.

## Escopo F5 (implementado)
- **Projeto e storage (F5-A)**: `core/ProjectStorage` (interface única —
  F5.2/SAF entra como outra implementação) + `core/FsStorage` (POSIX; no
  device a raiz é `getExternalFilesDir()`, sem permissões) + `core/Project`
  (manifesto `project.goni`: nome/versão/cenas/settings verbatim; refs
  RELATIVOS; `scenes/` `meshes/` `textures/`). Boot abre o projeto existente
  ou cria "projeto"; a cena ativa volta do disco a cada arranque; Menu
  Save/Load opera no projeto. Guarda de caminhos: `../`, absoluto e `//`
  recusados em toda a interface.
- **Import OBJ (F5-B)**: `assets/ObjImporter` (v/vn/vt, todas as formas de
  canto, fan de polígonos, dedup de cantos, grupos g/o + usemtl com ranges,
  índices negativos, CRLF, limite u16 sobre vértices ÚNICOS) +
  `assets/ObjExporter` (round-trip testado) + `assets/ResourceManager`
  (cache CPU por ref: 1 carga, ponteiro estável).
- **Import glTF/GLB (F5-C)**: `assets/GltfImporter` — accessors
  bounds-checked (POSITION/NORMAL/TEXCOORD_0, índices u8/u16/u32),
  bufferViews com byteStride, data URIs base64, buffers externos relativos à
  pasta do .gltf, container GLB validado, materiais básicos (name +
  baseColorFactor), nós → hierarquia (TRS). `assets/GltfInstantiate` cria
  TICs com parent/Transform3D/MeshRenderer com ref `path#i`.
- **Texturas (F5-D)**: `assets/PngLoader` (stb_image vendor, RGBA 8-bit) com
  gate 1K/2K OK, 4K+ reduzido para 2K com aviso; `assets/TextureCompressor`
  (interface; passthrough na F5 — ASTC/ETC2 é F5.1); `render/Texture`
  (glTexImage2D + glGenerateMipmap, LINEAR_MIPMAP_LINEAR, REPEAT).
- **Export + editor (F5-E)**: Menu ganha "Export OBJ" (mesh do TIC →
  `meshes/export_<tic>.obj`); Inspector mostra `mesh:`/`tex:` com a origem do
  asset e abre SELETORES de `meshes/` e `textures/`; status line mostra
  objetos de GPU (`am`/`at`); `render/GpuAssets` garante 1 ref = 1 objeto GL
  (memória de GPU não duplica); `MeshRenderer` persiste `meshPath`/`texPath`
  no `.goni` com resolvers no load.
- CI: parsers OBJ/glTF/GLB, round-trip, cache, caminhos relativos e gate 4K
  cobertos por testes (196 no total).

Sem skinning/animação (F7), sem editor de materiais (F8), sem streaming (F9),
sem compressão ASTC/ETC2 (F5.1), sem SAF (F5.2) — CLÁUSULA CALMA.

## Escopo F4.2 (implementado)
Três bugs vistos no C33, corrigidos com CLÁUSULA CALMA (só layout/UiContext/
EditorUi + testes; zero física nova, zero componentes novos, zero render 3D;
mono/3 botões/landscape intactos):
- **B1 — safe-area (fix raiz do scroll morto)**: a UI assumia a superfície
  EGL inteira, mas a nav bar tapava o fundo dos painéis → o Inspector media
  contentHeight 536 contra um visibleHeight inflado (540) → maxOffset 0 →
  scroll nunca ativava. O main lê `android_app->contentRect`
  (`APP_CMD_CONTENT_RECT_CHANGED`) e injeta `safe::Insets` (novo
  `ui/SafeArea.h`, GL-free, fonte única das constantes de layout) — toolbar,
  painéis, viewport, status line, toast, TouchControls e overlays vivem
  DENTRO do contentRect; com a altura real do painel o overflow é detetado e
  o scroll ativa. Diagnóstico no logcat: `safearea: surface … content …
  insets …`
- **B2 — labels cortadas**: novo `ui/TextFit.h` (GL-free) +
  `UiContext::labelFitted` — toda label mede antes de desenhar e trunca com
  reticência ASCII `...` (o atlas não tem U+2026) mantendo o maior prefixo
  que caiba: nome do TIC, `body: rigid - sphere - chao: sim` (o caso
  reportado), mesh/input/tc, texto dos botões (linhas da Hierarchy), status
  line e toast
- **B3 — sandbox do Play**: novo `core/PlaySnapshot.h` — ENTRAR no Play
  captura pos/rot/scale de todos os Transform3D + velocity/grounded de todos
  os BodyComp; SAIR repõe a pose de editor EXATA e descarta a simulação
  (TIC destruído no Play é saltado; handle generacional nunca recria). A
  física (TickGroup::Physics) continua a correr SÓ durante o Play (gate
  `enabled` inalterado)
- CI: fix do workflow — o output `artifact_name` era descartado pelo
  mascaramento do secret `VV_KEY_ALIAS` ("vv"); o gate verify-entry-symbols
  passa a baixar por `pattern` (robusto a segredos e a mudanças de versão)

## Escopo F4.1 (implementado)
Scroll como primitivo de UI, aplicado ao Inspector e à Hierarchy — conteúdo
mais alto que o painel fica alcançável (desbloqueia a verificação da F4 no
device). Só scroll (CLÁUSULA CALMA): sem safe-area, sem colapsáveis, sem
física, sem render 3D.
- `ui/ScrollMath.h` (GL-free, testada no CI): clamp `[0, content − visible]`,
  drag-to-scroll com clamp nos dois extremos, limiar tap (12 px) vs drag,
  protocolo de claim (slider > scroll > botão dentro de região), clip de
  quads por interseção com UV proporcional, geometria do indicador
- `UiContext::beginScroll(id, region, contentHeight)` / `endScroll()` —
  slots por id (offset persiste), botões dentro da região só desenham (o tap
  é re-despachado pelo painel — "add TouchControls" clicável mesmo após
  scroll), sliders mantêm prioridade, indicador fino ACCENT só com overflow;
  recorte por interseção (sem glScissor no quad batch único)
- Hierarchy: lista TODOS os TICs (fim do corte em ~10 linhas da F3); tap na
  linha seleciona via `hierarchyRowAtTap`
- Inspector: BodyComp, `velx` e o botão "add TouchControls" no fundo ficam
  sempre alcançáveis; `ui/EditorLayout.h` é a fonte ÚNICA das alturas de
  conteúdo (desenho e testes partilham os números)
- Gestos sem conflito: drag na região = scroll; drag no slider = slider;
  tap = seleção/ação; viewport central NÃO é região de scroll (orbit/pinch
  intactos)
- CI: suíte do core (122 testes) + APK assinado

## Escopo F4 (implementado)
Física core num novo `TickGroup::Physics` (entre Update e PostUpdate) —
`physics/` é C++ puro, host-testável, sem GL:
- Formas: `Sphere`, `AABB`, `OBB` (rotQuat), `Capsule` (eixo Y local do TIC);
  interseções puras (sphere×{sphere,AABB,OBB,Capsule}, AABB×AABB, OBB×OBB SAT
  15 eixos, Capsule×Capsule) e sweep contínuo com TOI+normal para sphere/
  capsule contra AABB/OBB com CCD adaptativo (substeps máx 8 + avanço
  conservador exato via gradiente da SDF — nunca tunela paredes finas)
- `BodyComp` (Static/Character/Rigid + shape variant + velocity + grounded);
  `PhysicsSystem`: Character com input e slide estilo `move_and_slide`
  (remove a componente normal; iterações extra para cantos), repel entre
  Characters, Rigid com gravidade + colisão primitiva contra Static +
  amortecimento no chão. PLACEHOLDER DE SOLVER: dois Rigid empilhados
  intersectam (sem stacking/resting, documentado)
- `TouchControls` mínimo e FIXO (joystick esquerda + botão JUMP direita, tema
  mono, desenhado só em modo Play) como fonte de `InputSource`; `InputMap`
  liga zero ou uma fonte (sem fonte → sem input); UI criável = F6
- Presets: Player/Character/Static ganham Body, NOVO RigidBody3D; TouchControls
  adicionável via Inspector; Inspector mostra BodyComp (tipo·forma·chão) com
  slider `velx` para lançar corpos no device (testes de CCD/empurrão)
- CI: suíte do core (113 testes) + APK assinado

## Escopo F3.1 (implementado)
A pedido do dono no C33: remover as barreiras artificiais de câmara e baratear
o grid — só câmara + grid + testes (CLÁUSULA CALMA; nada de componentes,
presets, serialização, física ou UI de editor).
- Câmara: zoom 1..300 (era 1.5..40), pitch ±89° (era ±83° — top-down quase
  total sem degenerar o lookAt), yaw livre; near 0.5 / far 450 (zoom máx ×
  1.5, rácio 900:1 — sem z-fighting no depth de 24 bits)
- Grid: a malha de linhas virou UM quad de 4 vértices que segue a câmara, com
  linhas procedurais no fragment shader (`fract` + `fwidth`), anti-moiré por
  minificação e fade radial adaptativo ao zoom; extent ≥ 2000, borda nunca
  visível; interface (`init/draw/vertexCount/ok`) e tema mono mantidos
- CI: suíte do core (74 testes) + APK assinado; o build NDK valida o GLSL
  embutido

## Escopo F3 (implementado)
- `components/`: `Transform3D` (pos/Quat/scale + cache world TRS), `MeshRenderer`
  (Mesh* + Material* não-donos), `InputMap` (vazio — F4 preenche)
- `core/`: `Component` (owner + hooks attach/detach), `ComponentStorage<T>` SoA
  (deque — endereços estáveis), `ComponentRegistry` (nome→id→factory),
  `ComponentStore` (bolsa da Scene), `Tic` container
  (`addComponent<T>/getComponent<T>/removeComponent<T>`), `Presets`
  (PlayerBody3D/CharacterBody3D/StaticBody3D, nomes únicos .001),
  `Tick` (PreUpdate→Update→PostUpdate→Render), `TransformSystem`,
  `Json` mínimo + `SceneSerializer` (.goni v1, migração v0→v1, tipos
  desconhecidos ignorados no load)
- `math/`: `Quat` (unitário, euler YXZ, `toMat4` reproduz `rotX/Y/Z` da F2)
- `ui/`: `UiContext.slider` + `EditorUi` — Hierarchy esquerda (seleção + botão
  "+"), Inspector direita (9 sliders pos/rot graus/scale), overlays
  ("+" → 3 presets; Menu → Save/Load), toasts; tema mono; 3 botões F1 mantidos
- `platform/main.cpp`: pass 3D desenha todos os TICs com MeshRenderer
  (fim do cubo hardcoded); TickGroups por passo fixo; gate da câmara
  (gestos só nascem no viewport central)
- CI: `verify-entry-symbols` + suíte do core (113 testes) + APK assinado

Sem física (BodyComp é F4), sem luzes, sem assets, sem animação, sem linguagem
(CLÁUSULA CALMA).

## Histórico
- **F5.4-hotfix (0.6.5)**: SAF SEM duplicação ("main.goni (1).json", "project.goni (2)" em todo boot): (1) causa raiz = mime `application/json` para `.goni` → o provider renomeia no createDocument (FileUtils.buildUniqueFile acrescenta extensão canónica) e o nome no disco divergia do nome procurado → verificação falhava → createDocument de novo; (2) `bridgeFindFile` NOVA — pesquisa EXATA por displayName com query fresca ao provider ANTES de qualquer createDocument (método CRÍTICO do handshake; repetição em falha de query); (3) contrato TRI-ESTADO — "não sei" (provider em falha) NUNCA decide criação: `Presence::probe` em FsStorage/SafStorage + createNew só cria com ausência CONFIRMADA; (4) mime octet-stream para tudo menos .json (nome verbatim); (5) abertura "wt" (truncate) no doc EXISTENTE — nunca create por cima; (6) CURA dos projetos 0.6.4: ficheiros já renomeados ("x.goni.json") reabrem e continuam a ser usados — as cópias " (1)"/" (2)" são lixo a apagar manualmente; Salvar materializa assets em-runtime: cubo procedural → meshes/cube.obj (formato OBJ já definido, idempotente) e todo write loga `saf: write <rel> — N bytes` (visível no Ver logs); fake do SAF agora MODELA rename+colisão do provider — 279 testes.
- **F5.4 (0.6.4)**: Gestor de Projetos (SAF multi-pasta, takePersistableUriPermission por projeto, SafStorage = ProjectStorage sobre SAF, fallback app-private intacto) + fix do UnsatisfiedLinkError (JNI_OnLoad nunca corria no device — dlopen do NativeActivity não chama; static loadLibrary na VvActivity + registo idempotente pós-handshake + gate jni_parity.py que apanhou um bug latente de assinatura) — 272 testes (docs/HANDSHAKE_AUDIT.md §6).
- **F5.2 (0.6.2)**: All Files Access (diálogo → settings → File API direta) + remoção do SAF tree picker + log viewer in-app + boot self-check com errno — 254 testes.
- **F5.3 (0.6.3)**: handshake Java↔native INVERTIDO (VvActivity regista-se no native — onCreate + onResume; causa única das pontes mortas 0.6.0→0.6.2: GetEnv EDETACHED no thread do glue) + attach de threads nomeado + mensagens honestas ("ponte Java indisponível (handshake)") + gate do manifest binário no CI — 264 testes (docs/HANDSHAKE_AUDIT.md).
- **F5.1 (0.6.0)**: maturação de assets em 4 sub-blocos.
- **F5.1-hotfix (0.6.1)**: crash dump permanente + engine.log com rotação e boot progress por passos + export p/ Downloads/GOneVV/logs (MediaStore) + auditoria/fix da ponte JNI SAF (JNI_OnLoad/RegisterNatives, higiene de exceções, resultado SAF no thread da engine) — 243 testes. **A** — vendors
  etcpak 2.1 (BSD) e astc-encoder 5.3.0 (Apache-2.0), CompressedImage com
  cadeia de mips completa em CPU (blob contíguo), HardwareCompressor (ASTC
  se GL_KHR_texture_compression_astc_ldr, senão ETC2; <256px fica RGBA),
  cache em disco `textures/cache/` (header LE + hash FNV-1a do PNG; hit/miss
  contados; corrupção → miss), gate 4K REAL (com compressão 4K entra inteira
  — ETC2 4K ≈ 8 MB; fallback sem compressão reduz para 2K com aviso),
  `glCompressedTexImage2D` por nível. **B** — parser glTF lê `images`
  (data:image/png;base64 e bufferView do GLB) + `textures` + materiais com
  `baseColorTexture`; extração para `textures/gltf_<hash>.png` com DEDUP por
  hash (N materiais → 1 ficheiro); aplicar um mesh glb no Inspector aplica
  logo a textura. **C** — SAF com exceção documentada à regra zero-Java
  (docs/SAF_EXCEPTION.md): VvActivity (pickers + takePersistableUriPermission
  + JNI), SafIo sobre DocumentsContract (sem androidx), SafStorage com
  backend injetável + RoutingStorage (fallback getExternalFilesDir).
  **D** — menu: Pasta (SAF)/Importar…/Export SAF; status line mostra o
  formato da textura (etc2/eac/astc4/astc6/rgba) e hits/misses do cache;
  round-trip completo testado no CI (glb → extração → ETC2+cache → export
  OBJ → reimport → cena recarregada). 232 testes.
- **F5.0-fix (0.5.1)**: bug do C33 no Inspector (texto sobreposto em pilhas:
  nome×Transform3D, mesh/tex/input/body×sz, tc×velx) — o cursor Y SEMPRE foi
  partilhado e sequencial; a causa raiz eram as linhas de 26/30 px (baseline
  '+8') para uma fonte de 28 px: o bloco de glifos invadia a linha de cima.
  O Inspector agora vem de um PLANO único (EditorLayout.h) com cursor Y
  cumulativo, alturas derivadas das MÉTRICAS REAIS da fonte (ascent/descent
  medidos no bake), baseline centrado, contentHeight = fundo da última linha
  e tap re-despachado POR REGIÃO (a Hierarchy comia o tap do Inspector).
  UI real testada no CI com fonte embutida: 0 colisões glifo-a-glifo (o
  código antigo apanhava 13 com a mesma fonte); 203 testes.
- **F5 (0.5.0)**: projeto `.goni` com pasta (manifesto + scenes/meshes/
  textures, refs relativos, reopen persistente), import OBJ e glTF/GLB
  (hierarquia + materiais básicos), PNG com mipmaps e gate 4K→2K, export
  cena/.goni + OBJ round-trip, ResourceManager com cache (CPU e GPU sem
  duplicação), seletores de assets no Inspector; 196 testes.
- **F4.2 (0.4.2)**: correções do device — safe-area do sistema (scroll do
  Inspector ativa com a nav bar contabilizada), truncagem de labels com
  ellipsis e sandbox do Play (pose de editor restaurada ao sair); 135 testes.
- **F4.1 (0.4.1)**: scroll como primitivo de UI — Inspector e Hierarchy com
  ScrollRegion (clamp, drag-vs-tap, indicador, tap re-despachado); fundo dos
  painéis alcançável no device.
- **F4 (0.4.0)**: física core — 4 formas + sweep/CCD, BodyComp, slide do
  Character, Rigid primitivo, TouchControls (modo Play) e preset RigidBody3D.
- **F3.1 (0.3.1)**: porto do editor — clamps generosos de câmara (zoom 300,
  pitch 89°) e grid de linhas → quad de shader (4 vértices, fade adaptativo,
  anti-moiré); near/far recalibrados.
- **F1 (0.1.0)**: fundação — NativeActivity, EGL/GLES3, core (Handle/Tic/Scene/
  Time), UI immediate-mode, crash log, CI assinado.
- **F2 (0.2.0)**: primeiro 3D — Vertex/Mesh/LitMaterial, cubo procedural,
  grid com fade ("espaço infinito"), câmara de orbit (1 dedo + pinch, clamp),
  depth test, status line com vértices/draw calls.

## CI — fluxo da assinatura (uma vez)
1. Actions → **release** → *Run workflow* → preencha `store_pw` / `key_pw`
   → o job **init-keystore** gera o artifact `vv-release-keystore` (baixe e guarde OFFLINE).
2. `base64 -w0 vv-release.jks` → configure os secrets:
   `VV_KEYSTORE` (base64), `VV_STORE_PW`, `VV_KEY_ALIAS`, `VV_KEY_PW`.
3. Todo push publica o artifact `goni-vv-0.5.1-release-signed` (APK arm64).
   Sem secrets: sai `-UNSIGNED` (nunca debug).

Trocar a keystore muda a assinatura — exige desinstalar/reinstalar no device.

## Build local (opcional)
Android SDK + NDK 26.3 + CMake 3.22.1 + JDK 17 → `./gradlew assembleRelease`.

## Testes do core (Linux)
`cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests`

## Verificação no Realme C33 (dono) — F5.1-hotfix (diagnóstico sem PC)
1. Instalar o APK **0.6.1** → abrir. **Se abrir**: ir a
   `Android/data/vv.goni/files/logs/` (gestor de ficheiros) → `engine.log`
   deve mostrar a sequência completa:
   `[boot 2/6] storage OK` … `[boot 1/6] contentRect OK` …
   `[boot 3/6] fonts OK` … `[boot 4/6] renderer OK` …
   `[boot 6/6] scene OK → editor`.
2. **Export**: toolbar **Settings → "Exportar logs"** → toast "logs
   exportados: N" → abrir **Downloads/GOneVV/logs/** no gestor de ficheiros →
   `engine.log` visível e abrível no telefone.
3. **Se ainda assim crashar**: o mesmo gestor de ficheiros mostra
   `Android/data/vv.goni/files/logs/crash-<data>.dump` (e a marca `CRASH` no
   fim do `engine.log`) — enviar o dump; ele diz o SINAL e a função C++
   exata com offset. Exportar também pelo botão Settings (o dump vai junto).
4. **SAF depois do fix**: Menu → Pasta (SAF) → escolher pasta → a app
   continua viva (o resultado agora é processado no thread certo) → Importar
   um .obj/.glb/.png → Export SAF. Reiniciar → pasta SAF reaberta.
5. **Regressões**: F5.1 (status line `etc2/astc4`, cache `c1/1`, glb com
   textura), F5 (Save/Load), F4.2 (Play/scroll).

## Verificação no Realme C33 (dono) — 0.8.11 (ÁUDIO: AAudio + .gi + TIC AudioPlayer + workspace + gravação; APK CUMULATIVO)

Instalar o APK 0.8.11 (artifact `goni-vv-0.8.11-release-signed` do run do
job `build-release`; versionCode 41). A alvo são os SETE blocos — cada
passo diz onde confirmar (ouvido, UI ou log viewer). **Zero crash dumps
novos continua a ser o critério global.**

1. **O SOM OUVE-SE (o backend)**: arranque → Settings → Ver logs → a
   linha `audio: backend aaudio ATIVO (44100 Hz stereo…)` (se aparecer
   `AAudio recusou — FALLBACK AudioTrack`, é o fallback DOCUMENTADO a
   trabalhar — o som segue igual). Play de uma cena com som = áudio no
   altifalante. Botão home → o som PAUSA (`audio: PAUSE`); voltar →
   `audio: RESUME` e o som continua de onde estava.

2. **PROBE (a exigência do prompt)**: Settings → "diagnostico audio
   (probe)" → ~2 s → toast "audio: AAudio estavel (probe ok)" e no log a
   TABELA completa (`audio: probe aaudio — ciclos 50/50 ok … DECISAO:
   AAudio OK`). DESLIGUE um headset bluetooth a meio de um playback e
   re-corra o probe: se a tabela acusar disconnects, a linha final diz
   `DECISAO: FALLBACK AudioTrack` e o backend TROCA sozinho (log
   `audio: TROCA de backend aaudio -> audiotrack`) — o som NÃO morre.

3. **IMPORT de áudio**: toolbar → aba **ÁUDIO** → Importar → navegador
   abre em **Music** → escolher um .wav/.ogg/.mp3 → toast "clip: <nome>
   (Xs)" e o clip aparece na lista com codec+duração, a WAVEFORM desenha.
   No log: `audio: import audio/<nome>.gi codec=<adpcm|ogg|mp3>
   ratio=X`. Com um TIC de Audio selecionado, o diálogo "aplicar ao TIC?"
   → **Sim** → o Inspector do TIC mostra `clip: <nome>`.

4. **GRAVAR**: aba ÁUDIO → Gravar → (1ª vez: diálogo do sistema do
   MICROFONE → Permitir) → tocar Gravar de novo → "A GRAVAR... Ns" com o
   MEDIDOR a mexer → PARAR → toast "gravado: Ns" e o clip `rec-…` entra
   na lista (no log `audio: gravado … ratio=4.0x`). Sem permissão = a
   gravação não arranca mas NADA crasha (toast honesto).

5. **TIC de ÁUDIO na cena**: "+" → **Audio** → o TIC nasce (glifo de
   ALTIFALANTE no viewport; SEM mesh) → Inspector: "clip:" → seletor com
   os clips do projeto (ou importar…) → **ouvir** = preview imediato
   (botão vira "parar"; o fim do clip volta a "ouvir") → ligar
   "posicional" → a ESFERA wireframe dos raios desenha no editor e
   afastar a câmara ABAFA o som ao ouvir em Play (raios no Inspector).

6. **Play com autoplay**: TIC de Audio com autoplay=1 (e loop) → Play →
   o som começa sozinho e enrola; Stop → silêncio total (o Play é
   sandbox: o leavePlay mata TODAS as vozes). Guardar/reabrir o projeto:
   o clip/autoplay/loop/volume/pitch/posicional voltam exatos (Inspector).

7. **Volume geral + renomear/apagar**: Settings → "volume geral: 100%" →
   cicla 0/25/50/75/100 (0 = mudo total; persiste por projeto). Na aba
   ÁUDIO: renomear (teclado in-app) move o clip E os TICs que o usavam
   seguem a nova ref; apagar pede CONFIRMAÇÃO (2 toques) e o clip sai da
   lista e do storage. Zero crash dumps novos = release saudável.

## Verificação no Realme C33 (dono) — 0.8.10 (MESH DETERMINÍSTICA + 500 MB + FORMATOS + ARCHIVES; APK CUMULATIVO)

Instalar o APK 0.8.10 (artifact `goni-vv-0.8.10-release-signed` do run do
job `build-release`; versionCode 40). A alvo são os CINCO blocos — cada
passo diz onde confirmar (UI ou log viewer). **Zero crash dumps novos
continua a ser o critério global** — e agora, se algum houver, o NOME e o
header do dump dizem EXATAMENTE que build o produziu.

1. **Trocar cubo↔esfera 10× em ordens variadas** (a intermitência): TIC
   Mesh → Inspector → "prim:" → esfera→box→esfera… e box→esfera→box…,
   várias voltas com params de slider no meio (raio/segmentos/aneis) —
   **0 falhas, 0 desseleções, 0 dumps novos**. No log viewer, a cada
   troca as QUATRO linhas do passo-a-passo:
   `mesh: troca <de>→<para> passo=gerador ok verts=… idx=…` →
   `passo=validacao ok` → `passo=upload ok (self-check ok)` →
   `passo=bind ok (cova=… vivos=…)` — e no frame seguinte
   `mesh: deferred free … mesh(es) de prim (inicio do frame)`.
   (Se alguma vez falhar: `passo=<p> ERRO(<razão exata>)` + toast no ecrã
   — e o mesh ANTERIOR fica a renderizar, a seleção intacta.)

2. **Cena antiga com cilindro/cone/etc. carrega com aviso**: abrir um
   projeto 0.8.x com TIC de prim removida → o TIC aparece como **CUBE** +
   toast "prim cilindro foi removida -> cube" (1×) + no log
   `mesh: prim cilindro removido -> cube`. Re-salvar grava "box" (a cena
   fica limpa para a frente). NUNCA crash.

3. **Importar o FICHEIRO MAIOR DISPONÍVEL no device** (o crash de 500 MB):
   Import → navegador → escolher o .obj/.glb/.png mais GORDO do
   armazenamento — o overlay "IMPORT…" aparece com barra de progresso e
   botão **cancelar** (testar cancelar a meio uma vez: toast "import
   cancelado", SEM ficheiro parcial); deixar correr — **sem crash, sem
   freeze** (o frame continua vivo — a cópia corre noutra thread). No log:
   `import: job iniciado` → `import: fonte copiada … em chunks de 6 MB` →
   `asset: convert '…' -> N mesh(es) … (ratio X)` →
   `asset: load … verts=… em Xms`. O convertido vive em `assets/` e o
   projeto fica **MENOR** (o rácio está no log).

4. **Projeto antigo converte em silêncio**: abrir um projeto 0.8.9 com
   meshes/x.obj importado → no primeiro load o log mostra
   `asset: migracao 'meshes/x.obj'` + `asset: ref migrada 'meshes/x.obj'
   -> 'assets/x.gmesh'` e o TIC continua a desenhar (o loader próprio
   `asset: load assets/x.gmesh verts=… em Xms`). Nada perguntado ao dono.

5. **Extrair um .zip (2 passos separados)**: navegador → tocar num .zip
   com modelo+textura+som → progresso → toast "extraido: N ficheiro(s)
   crus" e o **browser ABRE a pasta `extracted/<nome>/`** com os ficheiros
   crus (nada convertido). Importar DEPOIS um deles a partir da pasta
   extraída → converte normalmente pelo pipeline (assets/). Um .rar dá
   erro legível "usa .zip". Nada é extraído fora da pasta do projeto.

6. **Settings novos**: Settings → "fonte apos import: manter/largar"
   (largar = `source/` sai depois de converter; o log diz
   `fonte 'source/…' largada`) e "reconverter assets" (reconverte tudo de
   `source/`; toast com a contagem).

7. **Identidade**: abrir Settings → Ver logs → a 1ª linha do arranque é o
   banner `boot: goni-vv 0.8.10 (versionCode 40, git …, so ok)`; qualquer
   dump antigo na lista aparece com **[ANTIGO]**. (O `app-release.apk`
   assinado tem o sha256 no relatório; o dump de qualquer crash novo
   carrega o mesmo versionCode no nome.)

## Verificação no Realme C33 (dono) — 0.8.9 (CRASH-PROOF; APK CUMULATIVO)

Instalar o APK 0.8.9 (artifact `goni-vv-0.8.9-release-signed` do run do
job `build-release`). A alvo são os QUATRO fixes — cada passo diz onde
confirmar (UI ou log viewer). **Zero crash dumps novos é o critério
global: a pasta de dumps (Settings → Ver logs) fica VAZIA depois da
sessão.**

1. **Ciclar as 8 primitivas em ordens variadas** (o crash): TIC Mesh →
   Inspector → "prim:" → esfera→box→cápsula→cone→torus→cilindro→plano→
   triângulo e depois noutra ordem (box→cápsula→esfera→cone→…) várias
   voltas — **nenhuma desseleciona, nenhum crash, todas visíveis (cone e
   cápsula incluídos)**. No log viewer, a cada troca:
   `mesh: troca … fim ok verts=… idx=…`. (Se alguma vez falhar, a linha
   `mesh: prim … ERRO(<razão exata>)` aparece — coords não finitas, AABB
   degenerado, upload GL — e o mesh ANTERIOR fica no viewport.)

2. **A cena com UI não crasha mais**: criar um container (VBox) → no
   Inspector de UI, "colocar em:" NUNCA lista o próprio container (o
   seletor excluiu a si mesmo). Se um PROJETO ANTIGO tinha um elemento
   pai de si próprio (gravado pelas 0.7.4–0.8.8), ao CARREGAR o log
   mostra `load: elemento '…' era pai de SI MESMO … parent removido` e
   a app segue viva (antes: crash-loop no Play/edição de UI).

3. **Importar modelo gigante — não espalmado**: navegador → um .obj/.glb
   grande → "Sim" → o modelo entra UTILIZÁVEL (maior eixo ≈ 2 unidades,
   proporções iguais às do ficheiro). No log viewer:
   `import: dims=<x,y,z> uniform scale=<s>`. No Inspector: linha
   "dims: X×Y×Z" + botão **escala original** → repõe o tamanho real
   (e um Save grava o estado reposto). Re-escolher o MESMO mesh NÃO mexe
   na escala que o dono afinou.

4. **Espaço sem tetos**: pinçar para AFASTAR até ver o modelo inteiro —
   o zoom agora vai até 100 000 (a grelha alarga: passo 0.1/1/10/100/
   1000 conforme o zoom; no zoom de trabalho continua 1). Aproximar até
   0.01 também funciona. Posicionar um TIC em **py=10 000**: tocar o
   VALOR do slider py (à direita do trilho, aceso a azul com sublinhado)
   → teclado numérico ("-"/"." + dígitos) → "10000" → OK → o TIC ESTÁ lá
   (ver no zoom afastado — o far dinâmico contém a cena). Tudo o que
   estava no viewport antes continua renderizando (far/near por frame).

5. **Play também sem tetos**: dar Play numa cena com câmara — o far
   acompanha a cena (o slider "far" do Inspector é PISO: valores
   pequenos continuam a valer; a cena nunca clipa pelo far default).

6. **Zero crash dumps**: depois de TODA a sessão de teste (trocas,
   imports gigantes, zooms extremos, py=10 000, Play), a lista de dumps
   no log viewer continua VAZIA — o critério de aceitação do prompt.

**SE ALGO FALHAR no C33**: abrir o log viewer e procurar a ÚLTIMA linha
`mesh:` / `import:` / `ui: layout` / `load:`:
- `ui: layout — CICLO de parents no elemento '…'` → ciclo gravado no
  .goni (abrir noutro editor de texto e tirar o "parent" do elemento
  nomeado — a próxima versão cura sozinha ao carregar);
- `mesh: prim … ERRO(…)` → a razão exata do gerador/upload;
- `mesh: troca … ERRO(sem TIC com mesh selecionado)` → a seleção estava
  perdida (selecionar um TIC de mesh primeiro — não é bug do gerador).

## Verificação no Realme C33 (dono) — 0.8.7 (HOTFIX: import + troca; APK CUMULATIVO)

Instalar o APK 0.8.7 (artifact `goni-vv-0.8.7-release-signed` do run do
job `build-release`). A alvo são os DOIS pontos do hotfix — cada passo diz
onde confirmar (UI ou log viewer):

1. **Import ABRE o navegador (o morto)**: com um projeto aberto → Menu →
   "Importar…" → o NAVEGADOR DE FICHEIROS ABRE em cheio (raízes
   [Raiz][Download][Docs][Camera][Pictures], caminho visível no topo,
   "^ Subir", lista) — antes: NADA acontecia (o toque corria mas o overlay
   nunca desenhava). No log viewer: `import: toque no botao` e
   `import: navegador ABERTO (Download) — flag fileBrowser=1`;

2. **Importar .obj/.gltf/.glb aplica ao TIC**: TIC Mesh selecionado →
   navegador → tocar num .obj (ou .glb) → diálogo "APLICAR AO TIC?" →
   **Sim** → o mesh aplica no viewport (toast + `import: aplicado verts=N
   idx=N` no log). Um .fbx/.psd dá o erro claro "formato nao suportado
   ainda: .fbx";

3. **Trocar primitiva SEM travar (a intermitente)**: TIC Mesh →
   Inspector → "prim:" → trocar entre as 8 formas em ORDENS DIFERENTES
   (esfera→torus→cápsula→cone→box→…) várias vezes — nenhuma trava, nenhum
   erro, a forma troca no viewport. No log viewer, a CADA troca:
   `mesh: troca prim esfera → prim torus inicio` / `fim ok verts=… idx=…`
   e a cada upload novo `mesh: prim torus verts=441 idx=2400` — a PROVA de
   que o gerador existe no build e é chamado;

4. **Arrastar sliders do prim (o esquecido)**: com "prim: esfera" →
   arrastar raio/segmentos devagar — o mesh regenera suave, sem freeze
   (o cache agora tem cap 48 + evicção; o log mostra os uploads novos);

5. **A UI nunca "morre"**: abrir o seletor de primitivas → tocar num botão
   e ARRASTAR PARA FORA antes de soltar → soltar fora → o seletor fecha e
   TUDO continua respondendo (toolbar, painéis, timeline) — antes era
   possível a UI inteira morrer com um gesto órfão;

6. **Regressões**: animação (play/stop/loop com o TIC Mesh), 0.8.6 (hex/
   tipografia/gizmos), 0.8.5 (clips do glb importado), 0.8.4 (storm/play
   ×20).

**SE ALGO FALHAR no C33**: abrir o log viewer (Settings → "Ver logs") e
procurar a ÚLTIMA linha `import:` ou `mesh:` — com o logging embutido a
correção seguinte é UMA LINHA, não uma campanha:
- parado depois de `mesh: troca … inicio` sem `fim` → a falha está no
  gerador/upload (a linha `mesh: prim … ERRO(…)` diz a razão);
- `import: toque no botao` sem `navegador ABERTO` → o problema é a
  permissão (ver `storage: all-files granted=`);
- browser aberto mas vazinho → ver a linha `browser: … opendir FALHOU`.

## Verificação no Realme C33 (dono) — 0.8.6 (UX/layout/Inspector; APK CUMULATIVO)

Instalar o APK 0.8.6 (artifact `goni-vv-0.8.6-release-signed` do run do
job `build-release`). A alvo é o POLIMENTO — cada passo tem o "antes":

1. **Cor por hex**: TIC Mesh selecionado → Inspector → linha "hex:" →
   tocar → teclado (a tecla de caso é agora "#") → escrever "#FF8800" →
   OK → o tint do mesh muda NA HORA (sliders R/G/B acompanham);
2. **Hex no elemento de UI**: modo UI → elemento → Inspector → "hex:" →
   "#33CC66" → OK → o fundo do elemento muda;
3. **Tamanho e estilo da letra**: elemento Label → "letra: 2.00x" → o
   texto dobra NO editor e no Play; "letra estilo" → negrito (traço mais
   grosso) → itálico (inclinado);
4. **Gizmos de UI**: selecionar um elemento → 4 quadradinhos nos CANTOS +
   pega acima → arrastar um canto = ESCALA (o canto oposto fica fixo);
   arrastar a pega de cima = RODA com passos de 15°; o Play respeita o
   rodado (o toque acerta o elemento onde ele ESTÁ);
5. **Página inicial**: o launcher abre com título + subtítulo, as duas
   ações no topo e a lista limpa — sem lixo visual;
6. **Diálogos escuros**: "Novo projeto" → o diálogo do nome é ESCURO
   (antes: claro, do sistema); long-press num projeto → idem;
7. **Theme uniforme**: toolbar/painéis/timeline/launcher numa única
   identidade (nada de azul-claro do sistema em diálogos);
8. **Zero sobreposição**: percorrer TODOS os ecrãs (editor 3D, modo UI
   com timeline, Play, navegador, teclado, seletor de clips) — nenhum
   texto/rect sobreposto;
9. **Regressões**: 0.8.5 (import/clips/primitivas), 0.8.4 (storm/
   play-stop/texto), 0.8.3 (blend), 0.8.0 (timeline).

## Verificação no Realme C33 (dono) — 0.8.5 (funcionalidade; APK CUMULATIVO)

Instalar o APK 0.8.5 (artifact `goni-vv-0.8.5-release-signed` do run do
job `build-release`). A alvo é a FUNCIONALIDADE que estava morta — cada
passo tem um "antes" que falhava:

1. **Import com aplicar (o morto da 0.7.2)**: selecionar um TIC Mesh →
   Menu → Importar… → navegar até um .obj → tocar → o diálogo "APLICAR AO
   TIC?" APARECE (antes: nada, silêncio total) → **Sim** → o mesh aplica
   (toast + linha no log) e o viewport mostra o objeto;
2. **Import .glb com animação**: importar um .glb animado → "Sim" → toast
   "clips importados (timeline)" (antes: inatingível) → a timeline abre
   com o clip IMPORTADO ativo;
3. **"+track" com clip importado ativo**: com o clip importado na
   timeline → "+" → posição → "+key" → o track APARECE na strip e o play
   mexe o objeto (antes: caía no clip invisível — "animação não cria");
4. **Opções da animação**: play/pausa/stop · botão once→loop→pingpong ·
   slider "vel" — TODOS respondem na hora (o header deixou de ter botões
   sobrepostos no ecrã estreito);
5. **Trocar primitiva ×3**: TIC Mesh → Inspector → "prim:" → cone → box →
   torus → a forma TROCA no viewport a cada escolha e sobrevive a
   gravar/reabrir;
6. **Multi-mesh**: importar um .glb com VÁRIOS meshes → "Sim" → aplica o
   mesh #0 com log "a aplicar o #0" (antes: "falha ao carregar mesh");
7. **Formato não suportado tem VOZ**: no navegador, um .fbx/.psd APARECE
   marcado "?" → tocar → toast "formato nao suportado ainda: .fbx"
   (antes: invisível, sem explicação);
8. **Casings mistos**: importar um .Glb/.OBJ (maiúsculas no meio) → fica
   NO seletor de meshes do Inspector (antes: importava e desaparecia);
9. **Regressões**: 0.8.4 (storm/play-stop/texto/timeline), 0.8.3 (blend),
   0.8.2 (skin), 0.8.0 (primitivas/params).

## Verificação no Realme C33 (dono) — 0.8.4 (estabilização; APK CUMULATIVO)

Instalar o APK 0.8.4 (artifact `goni-vv-0.8.4-release-signed` do run do
job `build-release`). A alvo desta release são os CRASHES e FREEZES — cada
passo abaixo é um toque concreto que antes partia:

1. **Storm de UI (o ecrã preto)**: "+" → modo UI → criar 35+ elementos
   (Panel/Label/Botão, passando BEM os 32) → o editor NUNCA crasha, o
   mini-ecrã continua a desenhar e a selecionar todos os elementos;
2. **Storm 3D**: "+" → 10× Mesh + 2× Camera + presets (Player/Character/
   Static/Rigid) em sequência → sem crash, sem ecrã preto, fps estável na
   status line;
3. **Add/remove repetido**: criar e apagar 20 elementos UI alternados com
   20 TICs mesh (Hierarchy → remover) ×3 voltas → a app sobrevive toda a
   volta (antes: corrupção a partir do 33.º elemento);
4. **Play/stop repetido**: dar Play de jogo e Stop 20× seguidas (com anim
   e física na cena) → a pose de editor volta SEMPRE ao sítio, fps
   estável, sem leak visível (status line: `verts/dc` coerentes);
5. **Texto nunca some**: abrir o navegador (Menu → Importar…) com uma
   pasta com muitas imagens + timeline aberta → TODOS os labels continuam
   visíveis (antes: com ~32 alternâncias de runs, o texto INTEIRO saía);
6. **Timeline utilizável no C33**: selecionar um TIC com animação → no
   header da strip, play/stop/mode/vel/clip ficam todos CLICÁVEIS (antes:
   "clip:" e play sobrepunham no ecrã estreito);
7. **Regressões**: 0.8.3 (blend/crossfade), 0.8.2 (skin), 0.8.1 (clips),
   0.8.0 (timeline/primitivas), 0.7.x (gizmos/frustum/import).

## Verificação no Realme C33 (dono) — 0.8.3 (blending; APK CUMULATIVO)

Instalar o APK 0.8.3 (artifact `goni-vv-0.8.3-release-signed` do run do
job `build-release`). Esperado em cada passo (precisa de um .glb com DUAS
animações, ex.: walk e run):

1. **Crossfade**: com o clip walk ativo (a tocar em Play), abrir o
   seletor de clips → tocar "fade" na linha do run → a transição é SUAVE
   (o título mostra "walk → run"; o slider de blend aparece no header com
   o peso a subir) e ao fim o run fica ATIVO sem NENHUM salto de pose;
2. **Peso manual**: durante (ou depois de abrir) um blend, arrastar o
   slider "blend N%" → a pose mistura ao peso escolhido (50% = pose
   intermédia exata entre walk e run); soltar em 0 volta ao walk;
3. **Joints**: num .glb riggado, o crossfade walk→run mistura a POSE DOS
   JOINTS (o mesh deforma intermédio); joints que só UM clip anima
   descansam no BIND em vez de congelar;
4. **Sem salto na troca**: observar o fim do crossfade — o valor
   (posição/rotação) CONTINUA de onde estava (nada de snap);
5. **Persistência limpa**: gravar/reabrir com um blend a meio → a cena
   carrega SEM blend (runtime), o clip ATIVO e a pose intactos;
6. **Regressões**: 0.8.2 (skin/bind pose), 0.8.1 (clips/seletor), 0.8.0
   (timeline/primitivas), 0.7.x.

## Verificação no Realme C33 (dono) — 0.8.2 (skinning; APK CUMULATIVO)

Instalar o APK 0.8.2 (artifact `goni-vv-0.8.2-release-signed` do run do
job `build-release`). Esperado em cada passo (precisa de um .glb
RIGGADO, ex.: personagem do Sketchfab com skin):

1. **Import com skin**: importar o .glb riggado → "aplicar ao TIC?" →
   Sim → o mesh aplica E o esqueleto entra (log "esqueleto importado (N
   joints)" no Ver logs); os clips importados agora têm os tracks de
   JOINT (o Inspector mostra mais tracks no "anim:");
2. **Bind pose**: sem dar Play, o mesh desenha IGUAL ao glTF (a bind
   pose é identidade nas matrizes de skin);
3. **A pose deforma**: Play na timeline com um clip importado → o mesh
   DEFORMA com a animação (pernas/braços mexem — os vértices seguem os
   joints); scrub também;
4. **Play de jogo**: idem em Play (o AnimationSystem aplica os tracks de
   joint; o render compõe as matrizes por frame);
5. **Persistência**: gravar/reabrir → o esqueleto (joints + IBM) e os
   tracks de joint voltam; o Play continua a deformar;
6. **Meshes estáticos intactos**: cubos/primitivas/imports sem skin
   desenham como sempre (uSkin=0 — o caminho estático byte a byte);
7. **Regressões**: 0.8.1 (clips/seletor), 0.8.0 (timeline/primitivas),
   0.7.x (frustum/gizmos/import de texturas).

## Verificação no Realme C33 (dono) — 0.8.1 (clips glTF; APK CUMULATIVO)

Instalar o APK 0.8.1 (artifact `goni-vv-0.8.1-release-signed` do run do
job `build-release`). Esperado em cada passo:

1. **Import com animação**: importar um `.glb` COM animação (ex.: um
   personagem do Sketchfab com idle/walk) pelo navegador → "aplicar ao
   TIC?" → **Sim** → o mesh aplica E o toast "clips importados (timeline)"
   aparece; a timeline abre com o clip IMPORTADO ativo;
2. **Lista de clips**: botão "clip: <nome>" no header da timeline → a
   lista mostra os clips do ficheiro (nome + nº de tracks, o ativo com
   `*`) + "novo (edit)";
3. **Trocar de clip**: escolher outro clip → a timeline mostra os tracks
   DELE e o playback REPRODUZ esse clip (o tempo recomeça a 0);
4. **Playback do importado**: Play na timeline → o TIC mexe-se conforme o
   clip (posição/rotação/escala do nó raiz); em Play de jogo idem;
5. **Edição misturada**: "novo (edit)" → +track/keys no clip próprio — os
   clips importados ficam intactos;
6. **Persistência**: gravar/reabrir o projeto → os clips importados
   (nomes, tracks, keys) voltam exatos e reproduzem;
7. **Sem animações**: importar um .glb SEM animação → comportamento da
   0.7.x (mesh aplica, nada de clips, sem toast de clips);
8. **Regressões**: 0.8.0 (timeline/primitivas/preview), 0.7.10/0.7.9
   (frustum/gizmos), import OBJ/glTF de texturas intacto.

## Verificação no Realme C33 (dono) — 0.8.0 (animação + primitivas; APK CUMULATIVO)

Instalar o APK 0.8.0 (artifact `goni-vv-0.8.0-release-signed` do run do
job `build-release`). Esperado em cada passo:

1. **Primitivas**: "+" → **Mesh** → nasce uma ESFERA assente no grid;
   Inspector → linha "prim:" → abrir o seletor → escolher
   cilindro/cone/box/plano/triângulo/torus/cápsula → a forma TROCA no
   viewport; os sliders (raio/tam, altura, segmentos, tubo no torus)
   mudam a geometria AO VIVO;
2. **Sem física no Mesh**: o TIC Mesh NÃO cai no Play (sem BodyComp);
   os presets antigos (Player/Character/Static/Rigid) continuam iguais;
3. **Timeline abre**: selecionar o TIC Mesh → Inspector → "add Animacao"
   → a STRIP da timeline aparece no fundo do viewport (os painéis
   laterais e a status line ficam INTACTOS — nada sobreposto);
4. **Track + keys**: "+track" → posicao → posar o objeto noutro sítio
   (gizmo/Inspector) → "+key" no t=0; puxar o cursor para 2 s → posar
   noutro sítio → "+key" → diamantes na row;
5. **Scrub**: arrastar o cursor do tempo → o objeto SEGUE a curva AO
   VIVO; antes da 1ª key fica na 1ª pose, depois da última fica na última;
6. **Preview**: Play na timeline → o objeto ANIMA (loop por default);
   Stop → a pose de EDITOR volta exata (sandbox) e o tempo recolhe a 0;
   pausa deixa o tempo onde está;
7. **Modos e velocidade**: botão mode cicla once/loop/pingpong
   (pingpong vai e vem); slider "vel" 2× anima ao dobro;
8. **UI anima**: TIC com UiCanvas (botão) → add Animacao → "+track" →
   "ui pos"/"ui cor"/"ui alpha" → keys com o elemento posicionado em
   sítios diferentes → Play na timeline → o ELEMENTO mexe/muda de cor/
   alpha no preview; em Play de jogo IDEM (por cima da cena);
9. **Play de jogo**: Play da toolbar → TODOS os TICs com player animam
   do zero; Stop → TUDO volta à pose de editor (objetos E elementos de
   UI — o snapshot estendido);
10. **Persistência**: gravar o projeto e reabrir → primitiva (tipo +
    parâmetros), tracks/keys/curvas, modo e velocidade VOLTAM exatos;
11. **Regressões**: 0.7.10 (frustum/seleção), 0.7.9 (grab-lock),
    0.7.8 (UI de jogo), import/export de OBJ/glTF intactos.

## Verificação no Realme C33 (dono) — 0.7.10 (frustum domado; APK CUMULATIVO)

Instalar o APK 0.7.10 (artifact `goni-vv-0.7.10-release-signed` do run do
job `build-release`). Esperado em cada passo:

1. **Frustum compacto**: criar a câmara ("+" → Camera) e puxar o `far`
   no Inspector até 2000 → o cone CONTINUA compacto e legível (desenha a
   12 u — não atravessa o ecrã como antes); com far curto (ex.: 8) o
   cone fica real (encolhe);
2. **Cone não rouba toques**: tocar NO MEIO do cone (vazio) → NADA é
   selecionado e um ARRASTE ali ORBITA a câmara normalmente (antes
   selecionava a câmara/roubava o gesto);
3. **Corpo seleciona**: tocar na CAIXA da câmara (corpo/lente) → o TIC
   dela é selecionado (Inspector com a secção Camera);
4. **Objetos têm prioridade**: pôr um cubo DENTRO do cone e tocar nele →
   seleciona o CUBO (não a câmara); tocar no vazio → desseleciona;
5. **Handles**: com a câmara selecionada, os quadrados do far aparecem
   no retângulo COMPACTO (não no far real): o centro arrasta `far` (o
   valor no Inspector sobe — o desenho para no cap) e um canto muda
   `fovY`; SEM a câmara selecionada os handles NÃO apanham toques;
6. **Toggle**: Inspector da câmara → "frustum: nao" → o gizmo SOME (a
   câmara continua a valer para o Play); "sim" volta; gravar/abrir o
   projeto preserva a escolha;
7. **Play intocado**: com a câmara ativa e far 2000, o Play renderiza
   normal (o clamp é só visual — nada mudou na projeção);
8. **Regressões**: 0.7.9 (grab-lock dos gizmos), 0.7.8 (UI de jogo no
   Play), 0.7.7 (Inspector/alinhar-à-vista/uma ativa).

## Verificação no Realme C33 (dono) — 0.7.9 (grab-lock dos gizmos; APK CUMULATIVO)

Instalar o APK 0.7.9 (artifact `goni-vv-0.7.9-release-signed` do run do
job `build-release`). Esperado em cada passo:

1. **Agarrar e arrastar um eixo**: selecionar um TIC → modo Mover →
   tocar a seta Y e arrastar PARA CIMA com o dedo a DESVIAR-SE
   lateralmente a meio → o objeto move-se SUAVE só em Y, SEM oscilar e
   SEM fugir do dedo (o drag continua mesmo com o dedo longe do gizmo);
2. **Sem salto no arranque**: tocar um eixo e NÃO mexer → o objeto fica
   EXATAMENTE onde estava (antes saltava no primeiro frame);
3. **Rodar**: anel Z → rodar em volta do centro → rotação SUAVE desde o
   primeiro frame (sem o salto do ângulo absoluto do dedo);
4. **Escalar**: handle central → afastar o dedo → a escala MEXE (o drag
   uniforme estava morto no 0.7.7); com Snap, passos de 0.25;
5. **Snap estável**: com Snap ligado, arrastar devagar → o objeto salta
   de DEGRAU EM DEGRAU do grid (0.5 u) sem tremer entre degraus — mesmo
   com o objeto inicial FORA do grid;
6. **Grab generoso**: tocar 3-4 mm AO LADO da ponta de um eixo → AINDA
   agarra (alvo de 44 px); o highlight fino só acende em cima do eixo;
7. **Câmara**: handles do far (centro=far, canto=fov) com o mesmo
   comportamento suave; escalar numa câmara continua a ajustar fov/
   orthoSize com snap nos VALORES (60→110, não 105);
8. **Regressões**: 0.7.8 (UI de jogo no Play com câmara ativa), 0.7.7
   (frustum/seleção/Inspector), orbit normal fora dos gizmos.

## Verificação no Realme C33 (dono) — 0.7.8 (render 3D↔UI; APK CUMULATIVO)

Instalar o APK 0.7.8 (artifact `goni-vv-0.7.8-release-signed` do run do
job `build-release`). Esperado em cada passo:

1. **Cenário do bug**: cena com uma CÂMARA ATIVA (0.7.7, "+ → Camera") e
   um TIC com UiCanvas (ex.: botão ancorado ao fundo) → **Play**;
2. **UI de jogo a tamanho/posição corretos**: o botão do canvas aparece
   EXATAMENTE onde o viewport 2D do editor o mostrava (coords de ecrã +
   safe-area), texto a 28 px NORMAL — nada gigante, nada cortado, por
   cima da cena renderizada pela câmara ativa;
3. **TouchControls normais**: joystick/jump responsivos no Play; botões
   do canvas clicáveis (press→release dentro dispara a ação);
4. **Editor/modo UI inalterados**: voltar ao editor (Stop) — toolbar,
   painéis, gizmos, frustum e o viewport 2D exatamente como na 0.7.7;
5. **Home → voltar** (lifecycle): com o Play ativo e câmara ativa, sair
   para o fundo e voltar → a UI de jogo volta correta (a fronteira
   afirma o estado em CADA frame — nada herda do re-arranque);
6. **Regressões**: roteiro 0.7.7 completo (câmara/frustum/handles/Play)
   + 0.7.6 (toolbar/projetos) intactos.

## Verificação no Realme C33 (dono) — 0.7.7 (TIC de câmara; APK CUMULATIVO)

Instalar o APK 0.7.7 (artifact `goni-vv-0.7.7-release-signed` do run do
job `build-release`). Esperado em cada passo:

1. **Criar a câmara**: editor 3D → "+" → "Camera" → o TIC "Camera" entra
   na Hierarchy e o FRUSTUM WIREFRAME azul aparece na cena (corpo +
   lente + cone + retângulo do far + linha central). Numa cena com outra
   câmara ativa, a nova toma o lugar (uma ativa).
2. **Seleção por toque**: tocar no corpo/frustum no viewport seleciona o
   TIC da câmara (Inspector mostra a secção Camera); tocar no vazio
   desseleciona como sempre.
3. **Gizmos**: com a câmara selecionada, mover/rodar atuam na pose (o
   frustum segue); **escalar abre/estreita o frustum (fovY)** — a escala
   do transform NÃO muda; snap salta nos passos.
4. **Handles do far**: arrastar o QUADRADO DO CENTRO do far muda `far`
   (o retângulo afasta-se); arrastar um CANTO muda `fovY` (o cone
   abre/fecha). Perto de um handle o drag é DELE (não rouba o eixo do
   gizmo).
5. **Inspector**: secção Camera com fov/near/far/orthoSizo, "projecao"
   cicla perspetiva↔ortográfica (o far vira retângulo fixo), "ativa"
   sim/nao (uma ativa por cena).
6. **Alinhar à vista**: ⋮ da câmara → "Alinhar a vista" → o frustum fica
   EXATAMENTE na pose da orbit (o que se vê é o que a câmara vê).
7. **Play**: com a câmara ativa, a cena renderiza PELA CÂMARA (o
   joystick move o player em relação a ela) e NENHUM frustum se desenha;
   Stop volta ao editor com a orbit intacta. Sem câmara ativa: o
   fallback é a orbit de sempre.
8. **Regressões**: toolbar 0.7.6 (5 grupos/ícones/G4 condicional),
   paridade editor↔Play, overlays, teclado — intactos.

## Verificação no Realme C33 (dono) — 0.7.6 (histórico; toolbar final + ícones + tela de projetos)

Instalar o APK 0.7.6 (artifact `goni-vv-0.7.6-release-signed` do run do
job `build-release`). Esperado em cada passo:

1. **Tela de projetos (a app abre nela)**: topo com [Novo projeto] e
   [Importar projeto] (botões de CONTORNO azul #8AB4F8, sem gradiente
   cinza) + cabeçalho "Meus projetos"; a lista mostra SÓ nome + data da
   última edição — NENHUM "primary:" em lado nenhum; sem botão de criação
   no fundo. [Novo projeto] pede o nome → seletor de pasta → entra no
   editor (a linha da lista fica com a data de agora ao voltar).
   [Importar projeto] abre o seletor direto (o nome vem da pasta).
2. **Toolbar do editor**: 5 grupos separados por linhas finas —
   [Menu ▾][Cena ▾] texto · [pause][play] ÍCONES azuis · [3D|UI]
   segmented (o ativo com fundo azul) · [inspector] ícone à direita. SEM
   botão "Settings" (está no dropdown do Menu) e SEM "Mover/Rodar/
   Escalar" em texto.
3. **G4 condicional**: sem TIC selecionado o grupo de transformação NÃO
   existe na barra; selecionar um TIC (Hierarchy ou tap) → os 4 íCONES
   (mover/rodar/escalar/snap) aparecem entre o 3D|UI e o inspector;
   mover/rodar/escalar exclusivos (um ativo), snap liga/desliga sem
   trocar o modo.
4. **Dropdowns do G1**: Menu → Settings/Guardar/Carregar/Export OBJ/
   Importar…/Export Downloads/Sair (Settings abre o menu de logs/
   armazenamento de sempre); Cena → lista de cenas do projeto (a ativa
   marcada) + "+ Nova cena".
5. **G5 inspector**: tocar o ícone → o painel direito desaparece e o
   viewport/gesto cresce para a direita; tocar de novo → volta.
6. **Regressões**: separador 3D|UI continua a trocar o viewport;
   gizmos/teclado/overlays (0.7.4/0.7.5) intactos; em Play a barra
   desaparece (só a play bar com Stop).

## Verificação no Realme C33 (dono) — 0.7.5 (histórico; overlays + TIC de UI + minúsculas)

Instalar o APK 0.7.5 (artifact `goni-vv-0.7.5-release-signed` do run do
fecho). Roteiro cumulativo — os anteriores continuam a aplicar-se:

1. **Overlays modais**: no modo UI com elementos no canvas ("TESTE",
   "Botao"), abrir o **MENU** → o ecrã fica todo tapado pelo fundo
   escuro e SÓ o menu aparece (nenhum texto do canvas à mista); o mesmo
   com o menu contextual (⋮), o teclado, CENAS e o navegador; toque fora
   fecha e o editor volta INTEIRO (toolbar/painéis/canvas);
2. **TIC de UI próprio**: SEM nada selecionado, modo UI → "+" → Label →
   nasce o TIC **"UI"** na Hierarchy (só com UiCanvas — sem mesh/body) e
   o elemento dentro dele (toast "TIC 'UI' criado + elemento"); criar um
   segundo elemento REUTILIZA o mesmo TIC; selecionar o Player e criar →
   continua a anexar ao Player (o comportamento de sempre);
3. **Minúsculas**: renomear (⋮ → Renomear) → teclado → tocar **abc** →
   as letras escrevem em minúsculas ("cena2", "ola mundo"); **ABC** volta
   às maiúsculas; a tecla **Z** existe e escreve (o fix da tecla fantasma);
   dígitos/'-'/'_'/' '/APAGA/OK/X como sempre;
4. **Regressões**: paridade editor↔Play (0.7.4), texturas, containers,
   joystick, cenas, navegador, gestão de TICs — tudo como antes.

## Verificação no Realme C33 (dono) — 0.7.4 (paridade + texturas + containers; APK CUMULATIVO)

Instalar o APK 0.7.4 (artifact `goni-vv-0.7.4-release-signed` do run do
fecho). Roteiro cumulativo — os anteriores continuam a aplicar-se:

1. **Paridade editor↔Play** (o caso do Menu): criar um Menu com 2-3 itens
   no modo UI → **Play** → o Menu é EXATAMENTE o que o viewport 2D
   mostrava (caixas, texto à mesma proporção, mesmos espaçamentos);
   alternar editor↔Play em Panel/Label/Button/Image/Menu/Card/Article e
   conferir que NADA muda de aparência (só a interação difere);
2. **Label só texto**: criar um Label → SEM fundo (só o texto); Inspector
   → "fundo A" a 1 → aparece o fundo (cor R/G/B editável); o Button
   continua a nascer com o fundo escuro;
3. **Texturas de fundo**: selecionar um Panel → "tex:" → escolher uma
   textura de `textures/` (ou "importar..." → navegador → escolher um PNG
   da galeria → volta ao seletor) → o painel fica com a IMAGEM de fundo;
   o mesmo num Button (com o texto por cima) e num Image; "tex: none"
   limpa;
4. **Z-order**: painel COM textura + botão (sem textura) POR CIMA → o
   botão continua VISÍVEL sobre a textura (o caso que desenhava mal);
5. **Menu configurável**: selecionar um Menu → "espaco" → os itens afastam-
   se; "fundo A" a 0 → as caixas somem (só texto); "alinhamento" cicla
   start/center/end do texto das linhas;
6. **Containers**: "+" → VBox → com ele selecionado, "+" → Button → nasce
   DENTRO (empilhado; sem posição manual); + Label → em baixo; "espaco"/
   "pad"/"alinhamento" do VBox mexem nos filhos; VBox cresce sozinho
   (auto-fit); HBox põe lado a lado; VBox dentro de HBox aninha;
   arrastar um filho PARA FORA tira-o do container (fica onde largou);
   "colocar em:" no Inspector também move;
7. **Persistência**: Save → Load preserva texturas/alpha/spacing/pad/
   alinhamento/filhos; um `.goni` 0.7.x abre como sempre;
8. **Regressões**: joystick editável/compostos/navegador/cenas/UI criável/
   gestão de TICs/gizmos — tudo como antes.

## Verificação no Realme C33 (dono) — 0.7.3 (joystick editável + compostos; APK CUMULATIVO — FECHO DA CAMPANHA 0.7)

Instalar o APK 0.7.3 (artifact `goni-vv-0.7.3-release-signed` do run do
fecho). Roteiro cumulativo — os anteriores continuam a aplicar-se:

1. **Joystick no modo UI**: selecionar o Player (com TouchControls) →
   modo UI → o joystick aparece como um QUADRO com knob no viewport 2D
   (mesmo sem elementos de UI);
2. **Editar**: arrastar o joystick move-o; selecioná-lo abre o
   INSPECTOR DO JOYSTICK (pos X/Y, tamanho, sensib., cor R/G/B, remover);
   mexer no tamanho cresce/encolhe o quadro; a cor tinge a base/knob;
3. **O edit afeta o Play**: Play → o joystick está ONDE se pôs, do
   TAMANHO escolhido; mover o dedo 1/4 do raio com sensib. 2 anda o
   DOBRO (o eixo escala); a física responde;
4. **Adicionar**: "+" no modo UI → Joystick → o TIC ganha o componente
   (e fica selecionado); "remover joystick" tira-o (add TouchControls do
   Inspector de 3D continua a funcionar);
5. **Persistência**: Save → Load preserva pos/tamanho/sens/cor; um
   projeto 0.6.x abre com o joystick no sítio de sempre;
6. **Compostos**: "+" → Menu (lista vertical; "Jogar>cena2" por linha
   dispara a ação com o alvo da linha) / Card (moldura + título) /
   Article (texto comprido faz wrap dentro da largura);
7. **Regressões**: navegador/cenas/transições/UI criável/gestão de
   TICs/gizmos — tudo como antes (o APK é cumulativo e fecha a campanha).

## Verificação no Realme C33 (dono) — 0.7.2 (import robusto; APK CUMULATIVO)

Instalar o APK 0.7.2 (artifact `goni-vv-0.7.2-release-signed` do run do
fecho). Roteiro cumulativo — o da 0.7.1 continua a aplicar-se:

1. **Navegador**: Menu → Importar… → abre o NAVEGADOR na Download (com o
   all-files concedido); o CAMINHO atual aparece no topo (barra escura);
2. **Galeria**: tocar "Camera" → as fotos de DCIM/Camera aparecem
   ("tex: IMG_….jpg" não — só .png/.obj/.gltf/.glb; fotos da câmara em
   .png aparecem); "Pictures" idem; "Raiz" despeja tudo o que há;
3. **Subir/descer**: tocar uma pasta entra ("^ Subir" volta); na raiz o
   Subir fica na raiz;
4. **Pasta vazia**: entrar numa pasta sem ficheiros suportados → a
   mensagem "(vazio) <caminho>" aparece DENTRO do overlay (nunca um toast
   que não diz onde);
5. **Importar + aplicar**: selecionar um TIC com mesh → Importar… →
   navegar até um .png → tocar → pergunta "Aplicar 'x.png' ao TIC?" →
   Sim → a textura liga (Inspector "tex: x.png"; o cubo mostra a imagem);
   Nao → fica só importado (aplicável depois no seletor tex:);
6. **Sem seleção**: importar sem TIC selecionado → só o toast
   "importado: textures/x.png" (sem pergunta);
7. **Permissão**: sem all-files → o diálogo de armazenamento (o fluxo da
   0.6.10 continua); "Raiz" numa pasta sem acesso → "(sem acesso)
   <caminho>" + errno no engine.log;
8. **Regressões**: cenas/transições (0.7.1), UI criável/gestão de TICs
   (0.7.0), seletor de textura do Inspector, export, logs.

## Verificação no Realme C33 (dono) — 0.7.1 (cenas múltiplas + transições; APK CUMULATIVO)

Instalar o APK 0.7.1 (artifact `goni-vv-0.7.1-release-signed` do run do
fecho). Roteiro cumulativo — o da 0.7.0 continua a aplicar-se:

1. **Lista de cenas**: Menu → "Cenas…" → o overlay lista "main" (ou a cena
   do projeto) com ">" na ativa;
2. **Nova cena**: "+ Nova cena" → digitar "nivel2" (teclado in-app) → OK →
   cena vazia ativa (toast "cena criada"); criar um cubo e voltar a
   "Cenas…" → tocar na cena antiga → o cubo ANTERIOR volta (nada se perdeu
   na troca); repetir o nome → toast "cena ja existe";
3. **Persistir**: Sair para projetos → reabrir → a cena ativa é a mesma
   (manifesto);
4. **Transição fade em Play**: numa cena com UI, criar um Button com
   acao "trans", alvo "nivel2", estilo "fade" → Play → tocar o botão → o
   ecrã escurece até preto, a cena TROCA ao escuro e volta a clarear
   (nunca se vê a troca a seco);
5. **Transição slide**: mudar o estilo para "slide" (Inspector UI) →
   repetir → o quadro preto cobre pela esquerda e sai pela direita;
6. **Scene.Load**: acao "scene" → Play → tocar → troca INSTANTÂNEA (sem
   transição) — os dois comportamentos convivem;
7. **Cena inexistente**: alvo "xyz" → Play → tocar → toast
   "cena 'xyz' nao existe";
8. **Regressões**: UI criável/gestão de TICs (0.7.0), gizmos, seletor de
   textura, import, logs — tudo como antes (o APK é cumulativo).

## Verificação no Realme C33 (dono) — 0.7.0 (UI criável + gestão de TICs; APK CUMULATIVO)

Instalar o APK 0.7.0 (artifact `goni-vv-0.7.0-release-signed` do run do
fecho). Roteiro cumulativo — os anteriores continuam a aplicar-se:

1. **Separador 3D | UI**: com um TIC selecionado, tocar "UI" na toolbar →
   o viewport central fica um ecrã 2D escuro com moldura (sem cena 3D);
   voltar a "3D" devolve a cena com os gizmos;
2. **Criar UI**: no modo UI, "+" → Button → o botão aparece CENTRADO e
   selecionado no viewport 2D e o painel direito mostra o INSPECTOR UI
   (x/y/lar/alt/cor R-G-B/visivel/ancoras/texto/acao/remover);
3. **WYSIWYG**: arrastar o botão no viewport 2D move-o; tocar num sítio
   vazio desseleciona; tocar de novo seleciona;
4. **Ancoragem**: no Inspector UI, "ancora H" até "direita" → o elemento
   NÃO salta de posição; girar o ecrã/reabrir continua colado à direita;
5. **Ação declarativa**: criar um Panel + um Button; no Button: acao =
   "toggle", alvo = nome do Panel (via teclado in-app — tocar "texto"/
   "alvo" abre o TECLADO; nada de IME do sistema); Play → tocar o botão
   alterna o Panel (a UI fica POR CIMA da cena);
6. **Gestão de TICs**: "..." numa linha da Hierarchy → Renomear (teclado
   in-app; OK aplica, Cancelar não mexe) / Remover (pede confirmação) /
   Duplicar (cópia com ".001") / Visibilidade; o OLHO "O/X" na linha
   alterna na hora; TIC invisível desaparece do editor E do Play (a
   física continua — um chão invisível ainda segura);
7. **Desselecionar**: tocar no vazio do viewport 3D ou abaixo da última
   linha da Hierarchy limpa a seleção (arrastar continua a orbitar);
8. **Cor por TIC**: TIC com mesh → sliders "cor R/G/B" no Inspector → o
   cubo tinge-se no frame seguinte; Save → Load preserva (e `none`/branco
   = o cinzento de sempre);
9. **Teclado in-app**: renomear um TIC → digitar → APAGA → OK; as teclas
   nunca se sobrepõem e o diálogo vive na safe-area;
10. **Regressões**: gizmos/play mode/seletores de textura/import/logs —
    tudo como na 0.6.10 (o APK é cumulativo).

## Verificação no Realme C33 (dono) — 0.6.10 (seletor de textura; APK CUMULATIVO)

Instalar o APK 0.6.10 (artifact `goni-vv-0.6.10-release-signed` do run
36748264165; sha256 e detalhes no `docs/RELATORIO-F6.md`) — cobre TAMBÉM a
0.6.9 (gizmos), 0.6.8 (play mode) e 0.6.7 (lifecycle/apagar/sair).

1. **Importar** um PNG (Menu → Importar… → escolher o screenshot de
   Download) → toast de import OK → `textures/screenshot-….png` no projeto
   (status line `at` sobe);
2. **Aplicar**: selecionar o TIC (cubo) → tocar na linha **`tex:`** do
   Inspector → o seletor TEXTURA abre com o screenshot na lista → tocar a
   imagem → **o cubo mostra a textura** e o Inspector passa a
   **`tex: screenshot-…`** (o estado agora muda de verdade);
3. **Log viewer**: Settings → Ver logs → procurar a linha
   `material: textura aplicada textures/screenshot-….png`;
4. **Persistência**: Menu → **Save** → **Load** (ou sair para projetos e
   reentrar) → o cubo continua com a textura aplicada (a ref está no
   `.goni` e é re-resolvida no load);
5. **Remover**: tocar `tex:` de novo → **none** → o Inspector volta a
   `tex: none` e o cubo ao cinzento → log
   `material: textura removida`;
6. **Mesh (mesmo fix)**: tocar `mesh:` → escolher um ficheiro de `meshes/`
   → o mesh muda de verdade (toast + linha `editor: mesh … aplicado`);
7. **Regressões**: 0.6.9 (gizmos), 0.6.8 (play mode), 0.6.7 (lifecycle GL,
   apagar/sair), F5.5 (All Files Access — import/export continuam OK).

## Verificação no Realme C33 (dono) — F5.5 (All Files Access de verdade)

APK: **0.6.9 + F5.5** (versão mantida — mesmo versionCode 18; distinguível
pelo sha256 no `docs/RELATORIO-F5.5.md` e pela linha do log viewer abaixo).

1. **Lista do sistema**: Definições do Android → Privacidade →
   **"Acesso a todos os ficheiros"** (ou "Permissão de todos os ficheiros")
   → a **G.One VV AGORA APARECE** na lista (antes só o Godot aparecia).
2. **Fluxo pelo editor**: abrir um projeto → Menu → **Importar…** → diálogo
   "Precisa de acesso a todos os ficheiros…" → **Permitir** → abre a página
   DA APP nas definições → **ativar o interruptor** → voltar → toast "acesso
   concedido — File API direta" e a lista de ficheiros de **Download/Documents
   abre SEM re-pedir** (import retomado).
3. **Export**: com um TIC com mesh → Menu → **Export Downloads** → toast
   "exportado: Download/GOneVV/export/…" → confirmar no gestor de ficheiros
   em `Download/GOneVV/export/`.
4. **Log viewer**: Settings → **"Ver logs"** → procurar
   `storage: all-files granted=1` (depois de conceder; `granted=0` antes).
   A linha aparece no boot, no retorno das definições e na re-verificação
   do resume — cada transição logada.
5. **Recusar (fallback)**: (não conceder / desativar o interruptor) →
   tentar importar → recusa no diálogo ou ao voltar → toast claro
   "acesso não ativado — modo app-private" — SEM repetições/loop; o
   projeto (SAF) continua a funcionar completo.
6. **Regressões**: 0.6.9 (gizmos), 0.6.8 (play mode), 0.6.7 (lifecycle GL,
   apagar projeto, sair para projetos), F5.4 (gestor multi-pasta).

## Verificação no Realme C33 (dono) — 0.6.9 (gizmos; APK CUMULATIVO)

Instalar o APK 0.6.9 (artifact `goni-vv-0.6.9-release-signed`) — cobre
TAMBÉM a 0.6.7 (lifecycle/apagar/sair) e a 0.6.8 (play mode).

1. Abrir um projeto → criar/selecionar um TIC → o GIZMO aparece no TIC
   (modo Mover por omissão): 3 setas (X vermelho, Y verde, Z azul) + 3
   quadradinhos de plano;
2. **Mover:** tocar na seta X (destaca-se a branco) e arrastar → o TIC
   mexe SÓ em X; arrastar um quadradinho de plano → move nos 2 eixos do
   plano; a câmara NÃO orbita durante o drag (arrastar fora do gizmo
   orbita normalmente);
3. **Rodar** (botão Rodar na toolbar): 3 anéis; arrastar o anel → roda no
   eixo do anel; com Snap, saltos de 15°;
4. **Escalar** (botão Escalar): 3 handles + quadrado central; o central
   escala uniforme; com Snap, passos de 0.25; nunca desaparece (clamp);
5. **Snap:** ligar/desligar na toolbar — mover salta ao grid de 0.5;
6. **Inspector coerente:** depois de um drag, os sliders do Transform3D
   mostram a pose nova (pos/rot/scale);
7. **PLAY:** tocar Play → o gizmo DESAPARECE (nada de edição em play);
   Stop → o gizmo volta na pose restaurada;
8. Regressões 0.6.8 (play bar/touchcontrols/orbit) e 0.6.7 (lifecycle
   home/voltar; sair para projetos; apagar projeto; nome visível).

## Verificação no Realme C33 (dono) — 0.6.8 (play mode)

1. Instalar o APK 0.6.8 (artifact `goni-vv-0.6.8-release-signed`).
2. Entrar num projeto com um TIC PlayerBody3D + TouchControls (add
   TouchControls no Inspector) e mover a câmara para um ângulo reconhecível.
3. **Play**: tocar no botão Play da toolbar → esperado: a toolbar e os
   painéis SUMIR; fica o viewport fullscreen + a barra PLAY no topo com
   "Stop", "a correr · fps N" e o aviso "simulação — alterações
   descartadas ao parar"; o joystick à esquerda-baixo e o JUMP à
   direita-baixo, SEM sobrepor nada (nem a barra, nem a status line).
4. **Orbit desativado**: em play, arrastar 1 dedo no viewport (fora dos
   controlos) NÃO mexe a câmara; pinch também não.
5. Simular (andar com o stick/saltar) → tocar **Stop** → esperado: volta ao
   EDITOR com a pose ANTERIOR ao play (não a pós-simulação), painéis e
   seleção exatamente como antes, orbit a funcionar de novo.
6. Regressões: Save/Load, gizmos ainda não existem (0.6.9), lifecycle
   0.6.7 (home → voltar → texto normal).

## Verificação no Realme C33 (dono) — 0.6.7 (lifecycle GL + gestão de projetos)

Instalar o APK 0.6.7 (artifact `goni-vv-0.6.7-release-signed` do CI). O
roteiro cobre os três fixes; o log viewer in-app (Settings → Ver logs)
mostra as linhas `lifecycle:` se algo falhar.

**A — lifecycle GL (cubinhos):**
1. Abrir um projeto no gestor → criar/mover um TIC → **botão home** (ou
   recents) → voltar à app SEM a matar;
2. Esperado: o texto (toolbar, Hierarchy, Inspector, status line) renderiza
   NORMAL — sem quads brancos;
3. Settings → Ver logs: procurar `lifecycle: INIT_WINDOW #2 — contexto
   EGL RE-CRIADO` e `[boot 3/6] fonts OK … RE-UPLOAD no contexto novo`;
   e no sair: `lifecycle: TERM_WINDOW #1 — … NENHUM recurso GL assumido
   vivo`.

**B — apagar projeto (com confirmação):**
1. No gestor, criar um projeto de teste (ex.: "lixo") com uma pasta à
   escolha → entrar nele → criar um TIC → Menu → Sair para projetos;
2. Long-press na entrada "lixo" → "Apagar projeto" → confirmação
   ("Não pode ser desfeito") → "Apagar";
3. Esperado: Toast "projeto apagado", a entrada SAI da lista e a pasta
   escolhida desaparece do gestor de ficheiros do sistema;
4. Repetir com "Remover da lista" noutro projeto: a pasta PERMANECE
   (comportamento antigo intacto).

**C — Sair para projetos (sem matar a app):**
1. No editor: mover/rodar um TIC → Menu → "Sair para projetos";
2. Esperado: volta ao GESTOR (a app não fecha/reinicia — sem splash), a
   cena foi auto-salva (toast "cena salva — a sair…");
3. Reentrar no MESMO projeto: cena carregada com o TIC na pose deixada,
   texto normal, física a 1× velocidade (play não acelerado);
4. Ver logs: `editor: sair p/ projetos — auto-save OK` e `lifecycle:
   REENTRADA do android_main`.

**D — nome do projeto visível:**
1. Gestor → "+ Novo projeto" → digitar: o texto digitado TEM de estar
   visível (campo escuro, texto claro, cursor a piscar).

## Verificação no Realme C33 E RMX3624 (dono) — F5.4-hotfix (0.6.5)
> A regressão do handshake APARECEU no RMX3624 (Android 13) — a fase só
> fecha VERIFIED depois de passar nos DOIS devices.

1. Instalar o APK **0.6.5** (por cima da 0.6.4 SEM apagar os dados — os
   projetos da lista continuam lá).
2. **ANTI-DUPLICAÇÃO (o bug desta fase)**: abrir um projeto criado pela
   0.6.4 (ou criar um novo) → adicionar/editar algo → Salvar → fechar a
   app (swipe) → abrir de novo → Salvar outra vez → repetir 2–3×. NO
   gestor de ficheiros, a pasta do projeto tem de continuar com EXATAMENTE
   um `project.goni`, um `scenes/main.goni` — NENHUM ` (1)`/` (2)`. O
   projecto 0.6.4 que tinha `project.goni.json`/`main.goni.json` continua
   a abrir (cura) SEM criar ficheiros novos; as cópias ` (1)`/` (2)` antigas
   são lixo inofensivo — podem ser apagadas à mão.
3. **Salvar materializa assets**: criar um TIC (preset Cube) → Menu →
   Salvar → Settings → Ver logs: tem de aparecer
   `saf: write meshes/cube.obj — N bytes` (e `file: write …` no modo
   app-private). No gestor de ficheiros: `meshes/cube.obj` existe com
   conteúdo (OBJ de texto — abre em qualquer editor). Salvar de novo NÃO
   reescreve (idempotente).
4. **Ecrã inicial = "Projetos"** (NÃO o editor). **Criar projeto**: "+
   Novo projeto" → nome → seletor de pastas (SAF) → Documents/JogoA →
   editor. No log viewer: `java: onCreate → nativeRegisterActivity` →
   `native: activity registada` → `jni: handshake OK — … saf=5/5` →
   `java: openProject → fila` → `projeto: '<nome>' pronto (SAF)`.
5. **2º projeto em pasta DIFERENTE** (Documents/JogoB) → ambos abrem
   independentemente (nada se mistura).
6. **Persistência**: fechar a app (swipe) → abrir de novo → a lista
   continua; tocar num projeto abre-o com as cenas gravadas — e ao Salvar
   NÃO crescem ficheiros na pasta (ponto 2).
7. **Sem All Files Access**: os projetos funcionam COMPLETOS (save/load/
   scene) SEM conceder All Files Access.
8. **All Files Access coexiste**: num projeto aberto, Menu → Importar… →
   import de Download/Documents para meshes//textures/ (subpasta certa);
   export para Download/GOneVV/export (igual 0.6.3).
9. **Mensagens honestas**: se algo falhar por ponte, o toast/log diz
   **"ponte Java indisponível (handshake)"** — nunca "sistema sem suporte".
10. **Sem UnsatisfiedLinkError**: no engine.log a linha
    `jni: JNI_OnLoad — G.One VV 0.6.5 … registado(s)` aparece ANTES do
    `java: onCreate → nativeRegisterActivity`.
11. **Regressões**: F5.4 (gestor, multi-pasta), F5.3 (import/export All
    Files), F5.2 (log viewer, modo no Settings), F5.1 (status line,
    cache), F5 (Save/Load), F4.2 (Play/scroll).

## Verificação no Realme C33 (dono) — F5.3 (histórico)
1. Instalar o APK **0.6.3** → confirmar "0.6.3" nas infos.
2. **Handshake no log viewer** (SEM PC): abrir a app → Settings → **"Ver
   logs"** → a sequência completa tem de aparecer:
   `java: onCreate → nativeRegisterActivity` →
   `native: activity registada` →
   `jni: handshake OK — vm=… openAllFiles=1 exportLogs=1` →
   `storage: All Files Access — handshake=1 supported=1 manager=? …`
   (`manager=1` se a permissão já estava concedida; `0` na 1ª instalação).
3. **Import**: Menu → **Importar…** → o diálogo "Precisa de acesso a todos
   os ficheiros…" aparece → **Permitir** → abre a janela de permissões DO
   app ("All files access") → ativar o interruptor → voltar → toast "acesso
   concedido — File API direta" e a lista de Download/Documents abre (import
   retomado). 4. **Export**: com um TIC com mesh → Menu → **Export
   Downloads** → toast "exportado: Download/GOneVV/export/…" e o OBJ no
   gestor de ficheiros.
5. **Mensagem honesta (se algo falhar)**: se a ponte Java estiver em baixo,
   o toast/log diz **"ponte Java indisponível (handshake)"** — NUNCA
   "sistema sem All Files Access" (a causa real). O C33 (Android 12) SUPORTA
   All Files Access — se viu essa mensagem no 0.6.2, era a mentira antiga.
6. **Regressões**: F5.2 (diálogo/perm/fallback/log viewer), F5.1 (status
   line `etc2/astc4`, cache `c1/1`), F5 (Save/Load), F4.2 (Play/scroll).

## Verificação no Realme C33 (dono) — F5.2 (All Files Access + log viewer)
1. Instalar o APK **0.6.2** → confirmar "0.6.2" nas infos.
2. **Diálogo**: Menu → **Importar…** → aparece o overlay "ARMAZENAMENTO —
   Precisa de acesso a todos os ficheiros para importar/exportar projetos"
   com **Permitir / Cancelar** (na 1ª tentativa de import/export).
3. **Settings do sistema**: tocar **Permitir** → abre a janela de permissões
   DO G.One VV ("All files access"); ativar o interruptor → voltar à app →
   toast "acesso concedido — File API direta" e a lista de ficheiros de
   Download/Documents abre em "IMPORTAR" (o import da ação pendente é
   RETOMADO automaticamente).
4. **Import**: tocar num .obj/.glb/.png da lista → toast "importado:
   meshes/…" (ou textures/) e o ficheiro aparece nos seletores do Inspector.
5. **Export**: selecionar um TIC com mesh → Menu → **Export Downloads** →
   toast "exportado: Download/GOneVV/export/export_…" → abrir
   **Download/GOneVV/export/** no gestor de ficheiros → o OBJ está lá.
6. **Log viewer**: Settings → **"Ver logs"** → o viewer mostra o tail do
   engine.log com scroll (abre no FIM) + os crash dumps; **sem export**.
   A linha "self-check: … getExternalFilesDir=… fopen(…) OK" aparece no
   arranque do log; se algo falhar, a linha diz `errno=N (causa)`.
7. **Modo no Settings**: Settings mostra "armazenamento: all files" (após
   conceder) ou "app-private" (se recusar); **"Acesso a ficheiros…"** abre a
   janela de permissões a qualquer momento.
8. **Fallback**: recusar no diálogo → toast "sem acesso — modo app-private";
   os fluxos Save/Load/Export OBJ do projeto continuam a funcionar
   (app-private não precisa de permissões).
9. **Regressões**: F5.1 (status line `etc2/astc4`, cache `c1/1`, glb com
   textura embutida), F5 (Save/Load), F4.2 (Play/scroll).

## Verificação no Realme C33 (dono) — F5.1 (assets maduros)
1. Instalar o APK 0.6.1 → confirmar "0.6.1" nas infos.
2. **Compressão**: importar um PNG 2K/4K (Menu → Importar…) → aplicar como
   textura → a status line mostra `etc2` (ou `astc4`/`astc6` no Mali do C33)
   no fim da linha; logcat `GpuAssets: textura … 4096x4096 ETC2 RGB via
   compress` (sem "reduzida").
3. **Cache**: aplicar a MESMA textura outra vez (ou reiniciar a app e
   aplicar) → status line mostra `c1/1` (1 hit, 1 miss) e o logcat diz
   `via cache`. Alterar o PNG e importar de novo → nova entrada (hash novo).
4. **glTF/GLB com textura embutida**: importar um .glb com textura interna →
   aplicar no TIC → toast "mesh aplicado (+textura)" e o modelo aparece
   texturizado; a pasta do projeto passa a ter `textures/gltf_<hash>.png`.
5. **SAF**: Menu → **Pasta (SAF)** → escolher/criar uma pasta no armazenamento
   → toast "pasta do projeto ativa"; Menu → **Importar…** → escolher um
   .obj/.glb/.png → toast "importado: …" e aparece nos seletores; Menu →
   **Export SAF** → gravar o OBJ noutro sítio (Downloads, p.ex.) → toast
   "exportado: …". Fechar a app e reabrir → a pasta SAF volta a ser a raiz
   do projeto (URI persistida).
6. **Regressões F5/F4.2**: cena Save/Load, gate 2K sem compressão (toast de
   aviso — agora só no fallback), Play snapshot, scroll do Inspector.

## Verificação no Realme C33 (dono) — F5.0-fix (Inspector sem sobreposição)
1. Instalar o APK 0.5.1 → confirmar "0.5.1" nas infos da app.
2. **Sem sobreposição**: selecionar o PlayerBody3D (com TouchControls e um
   asset importado) → no Inspector o nome, `Transform3D`, os 9 sliders,
   `mesh:`, `tex:`, `input:`, `body:`, `velx` e `tc:` estão cada um na SUA
   linha — nada desenhado em cima de outra linha.
3. **Scroll por cima**: arrastar para cima no painel → o conteúdo desce
   inteiro (indicador à direita); o ÚLTIMO campo (tc) chega ao fundo sem
   corte e sem saltar.
4. **Sliders/taps com scroll**: arrastar um slider horizontalmente → muda o
   valor e NÃO faz scroll; tap em `mesh:` / `tex:` abre o seletor; tap em
   `add TouchControls` cria o componente.
5. Regressão F5: repetir a verificação da F5 abaixo (projeto, import, gate
   4K, export, cache) — nada mudou nesses fluxos.

## Verificação no Realme C33 (dono) — F5 (projeto + assets)
1. Instalar o APK 0.5.1 → confirmar "0.5.1" nas infos da app.
2. **Projeto**: primeiro arranque cria a estrutura
   (`adb shell ls /sdcard/Android/data/<pkg>/files/` → `project.goni`,
   `scenes/`, `meshes/`, `textures/`); criar TICs → Menu → Save cena →
   fechar a app → reabrir → os TICs VOLVEM (cena ativa do projeto).
3. **Import**: `adb push esfera.obj /sdcard/Android/data/<pkg>/files/meshes/`
   e `adb push madeira.png .../textures/` → abrir a app → selecionar um TIC →
   no Inspector tocar em `mesh: cube` → escolher `esfera.obj` no seletor →
   o mesh importa e renderiza num TIC; tocar em `tex: none` → escolher
   `madeira.png` → a textura aplica no material.
4. **Gate 4K**: empurrar uma textura 4096×4096 → ao aplicar aparece o toast
   "textura 4096x4096 reduzida para 2048x2048 (gate 2K; compressao real =
   F5.1)" (1× por carga).
5. **Export**: selecionar o TIC com asset → Menu → Export OBJ → toast
   `export: meshes/export_<nome>.obj`; `adb shell ls .../meshes/` confirma.
6. **Cache de GPU**: dois TICs com o MESMO asset → status line mostra
   `am 1` (um objeto de GL, não dois).
7. **Regressões**: safe-area/scroll/labels/sandbox da F4.2 continuam
   funcionando; física continua só no Play; tema mono, 3 botões, landscape.

## Verificação no Realme C33 (dono) — F4.2 (safe-area + labels + sandbox)
1. Instalar o APK 0.4.2 → confirmar "0.4.2" nas infos da app.
2. **B1/scroll**: selecionar o PlayerBody3D → com a nav bar visível, o
   Inspector agora DETETA o overflow (indicador fino à direita) → arrastar
   para cima revela `body: …`, `velx` e o botão **add TouchControls** no
   fundo, SEM nada tapado pela nav bar; toolbar, status line e painéis todos
   dentro da área visível. (Opcional: `adb logcat | grep safearea` mostra a
   superfície, o contentRect e os insets detetados.)
3. **B2/labels**: com um Rigid no chão, a linha `body: rigid - sphere - chao:
   sim` aparece inteira OU termina em `...` — nunca cortada a meio; nomes
   longos de TIC na Hierarchy terminam em `...` dentro do botão.
4. **B3/sandbox**: dar **Play** → deixar o corpo cair/mover (stick + JUMP) →
   sair do Play → os TICs voltam EXATAMENTE à pose de editor (o corpo
   "desce" de volta ao sítio original); entrar/sair repetidas vezes mantém
   a pose estável.
5. **Regressões**: orbit/pinch no viewport central intactos; gestos atrás da
   nav bar não orbitam a câmara; scroll dos painéis continua com limites no
   topo/fundo; slider continua com prioridade sobre o scroll.

## Verificação no Realme C33 (dono) — F4.1 (scroll)
1. Instalar o APK 0.4.1 → confirmar "0.4.1" nas infos da app.
2. **Inspector**: selecionar o PlayerBody3D → arrastar PARA CIMA na lista →
   o conteúdo desce e revela `body: …`, `velx` e o botão **add TouchControls**
   no fundo → tocar no botão (clicável após o scroll) → aparece "tc: stick +
   jump"; o indicador fino aparece à direita só quando há overflow.
3. **Hierarchy**: criar 12+ TICs (repetir "+") → arrastar a lista → TODAS as
   linhas alcançáveis; tap numa linha que estava cortada seleciona (frame
   branco + Inspector mostra o TIC).
4. **Gestos sem conflito**: arrastar na pista de um slider (px/py/velx) muda
   o valor e NÃO faz scroll; arrastar fora da pista faz scroll; o viewport
   central continua a orbitar/pinçar como sempre.
5. **Limites**: no topo e no fundo o scroll para (não passa do fim); soltar
   sem mover = tap (não scrolla).

## Verificação no Realme C33 (dono) — F4
1. Instalar o APK 0.4.0 → confirmar "0.4.0" nas infos da app.
2. **Montar a arena** (modo editor):
   - Chão: **+** → StaticBody3D → Inspector: `py −0.5`, `sx 5`, `sy 1`, `sz 5`
     (OBB 5×1×5 com topo em y=0).
   - Parede fina rotacionada: **+** → StaticBody3D → `px 1.85`, `py 0.5`,
     `sx 4`, `sy 1`, `sz 0.2`, `ry 30`.
   - Player: **+** → PlayerBody3D (nasce em (0, 0.55, 0)).
3. **TouchControls**: com o player selecionado → Inspector → botão
   **add TouchControls** → tocar **Play** (toast "modo play") → joystick à
   esquerda empurra o player; botão **JUMP** salta quando `grounded`;
   empurrar contra a parede → **colide e desliza**, nunca atravessa.
4. **CCD**: em modo editor, selecionar o player → `velx 40` → **Play** →
   o player é lançado contra a parede fina e **não tunela** (para/encosta).
5. **Rigid**: **+** → RigidBody3D → `py 3` → **Play** → cai e **para no chão**;
   `velx 8` → desliza e abranda; empurrar o player contra a bola → bloqueia
   (empurrão = `velx` no Inspector). Dois Rigid empilhados intersectam
   (PLACEHOLDER de solver aceite).
6. **Modo editor**: sem Play não há painel de controlos nem física; orbit
   (1 dedo) e pinch continuam; gestos nos controlos não giram a câmara.
7. Menu → **Save/Load cena** persiste os corpos (BodyComp round-trip).
8. Status line: fps/tics/verts/dc; crash log em
   `/data/user/0/vv.goni/files/goni_crash.log`.

## Verificação F3.1 (câmara + grid)
- Pinch afasta até 300 sem a borda do grid aparecer; aproxima até ~1; orbit
  até ~89° sem inversão; sem z-fighting; grid custa 4 vértices (status line).
