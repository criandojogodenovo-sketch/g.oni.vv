# RELATORIO-0.9.6.5 — GRUPO B: FERRAMENTAS DE VERIFICAÇÃO (FASE 0.9.6-MASTER)

> O nono mandamento da campanha: «VER, TESTAR, CORRIGIR, NÃO PARTIR». O
> Grupo B é a parte do VER feita ferramenta: o framebuffer real, o layout
> exportado, o validador, a auditoria — e O RELATÓRIO DO ESTADO ATUAL COM
> OS PNGs RELIDOS (secção 8) que os Grupos C-I usam como linha de base.
> Sentinelas: R-024 (docs/REGRESSOES.md). Suíte local no fecho:
> **core 0 falhas + c33_virtual 374/0** + 5 gates verdes.

## 1 · O QUE FOI PEDIDO

O rastreador (BACKLOG.md, lido no arranque pela P-01) define o Grupo B:
«Exportar layout (PNG+JSON); validador; Auditoria; relatório do estado
ATUAL com PNGs lidos». A motivação está na arquitetura da campanha: os
Grupos C (escala/typografia), D (orçamento do editor 3D), E (símbolos),
F (identidade), H (ficheiros reais) são todos trabalhos de LAYOUT — sem
uma vara de medir comum, cada grupo mede à sua maneira e os números não
se comparam entre relatórios nem com o device. Este grupo entrega a vara.

O que NÃO estava no pedido (e não foi feito): nenhuma mudança visual, nenhum
re-arranjo de ecrã, nenhuma correção dos problemas que a auditoria listou —
esses pertencem aos Grupos C-I, que agora têm os números para os caçar.

## 2 · O MÉTODO (o diagnóstico antes de mexer)

Leitura por ordem da P-01 (REGRESSOES.md → RELATORIO-0.9.6.md → README →
BACKLOG.md) e depois o estudo das superfícies que o grupo ia tocar:
- **O inventário do que já existia** (a casa não re-inventa): o
  `thumb::encodePngRgb` (PNG encoder com zlib real + CRC32, testado desde a
  0.9.0), o `loadPng` (decoder stb de produção do pipeline de texturas), o
  `core/Json.h` (parser+dumper determinístico), o padrão `captureThumbIfPending`
  (glReadPixels do backbuffer antes do swap), o `glstub` no-op com contadores
  (0.6.7-0.8.12), o `Mat4::ortho` y-down do pass de UI, os DOIS shaders da
  casa (Renderer.cpp/Material.cpp) e o fragment procedural da grelha
  adaptativa (Grid.cpp, com `fwidth`).
- **O rascunho órfão**: a sessão anterior deixou `tests/stub/glstub_fb.h`
  (um rasterizador software completo, não integrado). Foi integrado,
  corrigido (três defeitos — secção 7) e estendido (grid + TRIANGLE_STRIP +
  uniform vec2) em vez de reescrito — o trabalho de diagnose já lá estava.
- **O desenho dos hooks**: o registo do layout tinha de sair do CHOKE POINT
  (os widgets do UiContext — o MESMO código que desenha), nunca de uma
  cópia do layout noutro sítio: a lição R-020 é permanente (a sentinela que
  codificava o bug).

## 3 · O FRAMEBUFFER REAL DO C33 VIRTUAL (glstub_fb)

Com `glstub::fb::enabled = true`, o stub de GL deixa de ser no-op e
RASTERIZA a sério num framebuffer RGBA8 + depth f32 — pelas MESMAS funções
que a engine chama, com as MESMAS regras do Mali:

- **Objetos**: ids ÚNICOS e nunca reutilizados (como um GL real); o modo
  no-op mantém os ids 1..n de sempre — `enabled=false` (default) é o
  comportamento das FASES 1-12, byte a byte (a suíte inteira passou sem
  uma mudança antes de qualquer fb ser ligado).
- **Dados**: `glBufferData` copia os bytes por buffer; `glTexImage2D` guarda
  os píxeis (GL_R8 = cobertura do atlas; GL_RGBA = albedo);
  `glCompressedTexImage2D` marca FLAT (não decodifica ASTC/ETC2 — amostra
  branca; a GEOMETRIA e o TEXTO são o que o layout precisa).
- **Uniforms por NOME** (por programa): o rasterizador conhece `uProj` (UI),
  `uVP`+`uModel` (+`uTint`/`uHasTex`/`uSkin`/`uBones`) do lit, e
  `uVP`+`uCenter`+`uExtent`+`uCamPos`+`uFade`+`uStep` da grelha — o mesmo
  conhecimento que o Mali tem por compilação.
- **Regras**: viewport/scissor/depth (GL_LESS + mask)/cull back/blend
  SRC_ALPHA/ONE_MINUS_SRC_ALPHA — o confinamento por scissor da 0.9.6.1
  rasteriza exatamente como no device.
- **Os shaders, fiéis aos fontes**: UI `outColor = vColor * texture(uTex,
  vUV).r` (mono: o canal R é a cobertura do glifo); lit `c = 0.16 +
  alb·diff; out.rgb = c·uTint` com o skin por vértice (4 influências).
- **A GRELHA ADAPTATIVA** (o desafio do fwidth): o `vWorld` é interpolado
  perspective-correct no plano y=0 e a DERIVADA sai da avaliação do MESMO
  plano no pixel vizinho — exatamente o que o GPU calcula nos quads 2×2.
  O anti-moiré (`cellPx < 2.5` dissolve), os eixos X/Z com AA de 1px e o
  fade radial até à câmara: o fragment INTEIRO do Grid.cpp, com
  `GL_TRIANGLE_STRIP` decomposto em triângulos com winding alternado.

Custo: um `if (fb::enabled)` por chamada GL; desligado = zero. A FASE 13 é
a única que liga (o reset entre fases limpa os objetos e MANTÉM a flag — o
padrão `astcLdr` de configuração de ambiente).

## 4 · O LAYOUT EXPORTADO (PNG + JSON)

- **O registo** (`ui/LayoutDump.h`): cada widget do UiContext acrescenta a
  SUA entrada quando o audit está ligado — `panel/panelRounded/panelPill`
  (Panel), `frame/frameRounded` (Frame), `label/labelStyled` (Label com a
  largura medida), `labelFitted` (Label com `fullW` do texto ORIGINAL +
  flag `truncado`), `button` (Button + a Label do texto dele), `widgetHit`
  (Button — os hit-rects da toolbar/browser), `slider` (Slider),
  `beginScroll` (Scroll). O GUARD dos compostos: `frame`/`panelRounded`/
  `button` chamam `panel()` por dentro — o widget de TOPO regista UMA
  entrada, não as partes. Os filhos de scroll nascem `clipped=true`.
- **O export** (`layoutDumpIfPending` no main.cpp, no ponto do
  captureThumbIfPending — frame completo, antes do swap): lê o backbuffer
  INTEIRO por glReadPixels, flip vertical, drop alpha,
  `thumb::encodePngRgb` full-res; o JSON sai pelo `core/Json.h`
  (determinístico — a ordem das entradas é a ordem de desenho). Escreve
  `layout/<ecrã>.png` + `layout/<ecrã>.json` na raiz do projeto
  (ProjectStorage — SAF no device).
- **O nome do ecrã** vem do `currentScreenName()` (play/script/docs/
  settings/texto/browser/logs/cenas/seletor/…/editor) — o gating do
  `modalOpen` já garante que só UM ecrã desenha por frame.
- **No DEVICE** (o dono usa): Settings → Diagnóstico → **«Exportar
  layout»**. O Settings FECHA e o próximo frame (o ecrã por baixo) é o
  exportado — um ecrã de cada vez, sem o modal à mista (o validador não
  pode ver camadas sobrepostas de propósito).
- **A janela de corrida correta**: o pedido armado A MEIO do frame (a ação
  do botão dispara durante o draw) NÃO é consumido pelo dump do mesmo frame
  com o registo STALE do ecrã anterior — o `g_layoutFrameAudited` exige que
  o auditBegin tenha corrido NO frame do dump (o bug que o primeiro run da
  FASE 13.2 expôs; secção 7).

## 5 · O VALIDADOR (as regras da casa, PURO)

`layout::validate()` em `ui/LayoutDump.cpp` — GL-free/Android-free, corre
no CI como o `scroll::` e o `textfit::`. As 6 regras:

| regra | severidade | apanha |
|---|---|---|
| `fora_do_ecra` | ERRO | interativo não inteiro no contentRect (meio fora = meio intocável) |
| `sobreposto` | ERRO | dois interativos com interseção > 1px² sem relação de contenção (o scroll CONTÉM os seus filhos — legítimo) |
| `toque_pequeno` | aviso | interativo < 48dp no menor lado (a regra da casa, commit 0.9.6.1-a) |
| `texto_truncado` | aviso | labelFitted que cortou com «…» (informação perdida; os Grupos C-I baixam a contagem) |
| `texto_sangra` | ERRO | label SEM clip fora do contentRect (a classe do bug do C33 0.9.2 — a lista que desenhava fora do sítio) |
| `rect_degenerado` | ERRO | interativo com rect ≤ 0 (a zona morta) |

As DUAS exceções que fazem o validador honesto (ambas apanhadas no
primeiro run): (1) conteúdo de SCROLL (`clipped=true`) não é validado
contra o ecrã — as linhas scrolled-out vivem fora da janela POR DESENHO (o
clip corta-as); a região do scroll é que é validada; (2) painéis sobre
painéis nunca são regra (camadas legítimas — o modal tapa o editor por
desenho).

## 6 · A AUDITORIA (o caminho do device)

Diagnóstico → **«Auditoria do ecrã»**: o Settings fecha, o próximo frame é
exportado (PNG+JSON) E validado; cada problema é LOGADO
(`elog::error` para ERRO, `elog::info` para aviso — o dono cola o
engine.log), o relatório inteiro vai para `layout/auditoria-<ecrã>.txt` no
projeto e o toast traz as contagens. A linha humana-legível
(`layout::describe`) nomeia a regra, o widget e os DOIS rects (o do
problema e o do contentRect) — sem abrir o JSON o dono já sabe o que e
onde.

O WALK do re-despacho do tap no SettingsPage ganhou as duas linhas dos
botões novos (logs/export/probe/bench/copiar/**exportar layout**/**
auditoria do ecrã**/texto) — sem elas os botões desenhavam mas nasciam
MORTOS ao toque (secção 7, o achado ao vivo).

## 7 · OS TRÊS ACHADOS AO VIVO (o loop da campanha a trabalhar)

1. **Os botões nativos mortos** (a classe do bug 0.9.1: desenhar ≠ tocar):
   os dois botões novos do Diagnóstico estavam no DRAW do SettingsPage mas
   faltavam no WALK de re-despacho do scrollTap — no device, tocar neles
   não faria NADA. A FASE 13.2 apanhou no primeiro run: o toque no rect
   REAL (lido do registo) não fechava o Settings. O fix: as duas linhas do
   walk (kExportLayout/kAuditScreen), o MESMO padrão das linhas do bench
   (G6) e da janela de texto (0.9.1) — o walk é a lista DUPLICADA à mão que
   a casa mantém por desenho (immediate mode), e a FASE 13 agora vigia-a
   para sempre.
2. **O falso positivo do clip**: o validador assinalava as linhas scrolled-out
   do settings (y=760/808 num ecrã de 720) como `fora_do_ecra` — eram
   conteúdo de scroll (o clip corta-as por desenho). O fix: a exceção do
   `clipped` na regra (secção 5) — e o teste `regress_validador_apana`
   planta os DOIS casos (o botão meio-fora que TEM de apanhar + o clipped
   scrolled-out que NÃO pode apanhar).

E os TRÊS defeitos do rascunho órfão (achados pela leitura + sonda):
`a.nx` em vez de `c.nx` no lerp das normais do lit (o terceiro vértice
contava o primeiro); os registos `onGenBuffer`/`onGenTexture` INEXISTENTES
(o `bufferData`/`texImage2D` procuravam objetos que nunca existiram —
`bufs_`/`texs_` vazios: NADA rasterizava; a sonda `scripts/fb_probe.cpp`
bissecou ao vivo); e a janela de corrida do pedido armado a meio do frame
(o dump do mesmo frame consumia com o registo STALE — fix
`g_layoutFrameAudited`).

3. **O gate de paridade com ponto cego** (achado ao integrar o LayoutDump
na build): o grep do `scripts/link_parity.sh` exigia a linha do CMake a
TERMINAR em «.cpp» — 10 TUs com comentário à direita estavam FORA da
paridade, entre eles o `core/UndoStack.cpp` cujo PRÓPRIO comentário diz
«faltou no APK: ld.lld undefined symbol no CI» — a exata classe de bug que
o gate existe para apanhar (uma sentinela com ponto cego é decoração — a
regra da casa aplica-se a gates também). O fix: o grep tolera e descarta o
comentário — a paridade passa de 105 para **115 TUs**, TODOS a ligar.

## 8 · O ESTADO ATUAL — OS PNGs RELIDOS (a linha de base dos Grupos C-I)

A FASE 13 exportou CINCO ecrãs no C33 virtual (1536×720, insets status 24
+ pill 24, densidade 1.0 — o ambiente do device) e RELÊ cada PNG com o
`loadPng` DE PRODUÇÃO. O que está nos ficheiros (medido, não descrito):

| ecrã | px | entradas | composição | erros | avisos |
|---|---|---|---|---|---|
| editor | 1536×720 | 75 | 27 painéis · 24 botões · 15 labels · 8 frames · 1 scroll | **1** | 3 |
| script (portrait, teclado aberto) | 720×1536 | 152 | 20 painéis · 54 botões (teclas) · 69 labels | 0 | 2 |
| docs | 1536×720 | 54 | 14 painéis · 10 botões · 28 labels | 0 | 0 |
| browser (estado opendir falhou) | 1536×720 | 38 | 9 painéis · 11 botões · 16 labels | 0 | 8 |
| settings (o export do próprio 13.2) | 1536×720 | 62 | — | 0 | 1+ |

**O ERRO do editor** (o único de todo o levantamento):
`ERRO label SANGRA o contentRect: [8,694 166x29] vs ecrã 1512x696+0,24` —
uma label na faixa do fundo cujo bloco desce até y=723 num contentRect que
acaba em 720 (3px fora; a linha de estado/inferior). É trabalho do Grupo C
(a tipografia/linha) ou D (o orçamento vertical do editor).

**Os avisos (a lista de trabalho):**
- editor: `botao(id 28) 56x40 < 48dp` e `botao(id 157c) 268x40 < 48dp`
  (dois botões de 40px de altura — a regra da casa pede 48dp); `label
  truncada "…" largura inteira 353px` (uma linha que perde informação).
- browser: 8 avisos de toque < 48dp — as 6 teclas de raiz (139,7×40), a
  linha de raiz ativa (96×36) e a barra de caminho (868×44).
- script: 2 labels truncadas (nomes/linhas longas no editor de código).
- settings: os botões de ação de 152×40 (a linha inteira do actionRow é o
  alvo visual, mas o BOTÃO direito tem 40px — decisão do Grupo C).

**O que os píxeis confirmam** (o «PNG lido» ao pé da letra — a FASE 13
afere por ecrã): o PNG do editor decodifica a 1536×720 EXATOS; o maior
painel do registo tem píxel de painel (não o clear) no canto do rect que o
JSON diz; a banda da toolbar (y 24..80) está POVOADA (>30% dos píxeis ≠
fundo — o chrome rasterizou); os GLIFOS do atlas desenham (píxeis >200 na
banda da toolbar — o texto é COBERTURA R8 pelo caminho do Mali). O PNG do
script é 720×1536 (o ciclo portrait REAL da janela, insets T96/B48) e traz
as 54 teclas do teclado da engine — o INSUMO do Grupo E.

Os ficheiros sobem como artefacto do CI em CADA run (`layout-screens-
c33-virtual`: os PNGs + JSONs + auditoria) — o dono descarrega e VÊ o que
o harness viu, sem abrir o device.

## 9 · SENTINELAS E PROVAS DE MUTAÇÃO (R-024)

Quatro sentinelas novas em `tests/test_sentinels.cpp` (a suíte passa de
830 para 834 casos):

| sentinela | vigia |
|---|---|
| `regress_layout_dump_nao_mente` | o registo é o draw: contagem exata (guard dos compostos — o button NÃO explode em widgetHit+panel+frame), a truncagem com fullW, os filhos do scroll clipped, o JSON determinístico e re-parsável |
| `regress_validador_apana` | UM caso plantado POR REGRA + o registo limpo fica limpo + as duas exceções (contenção pai-filho, clip do scroll) |
| `regress_fb_rasteriza` | o quad rasteriza no sítio certo (orto+viewport = identidade em x), borda DURA, o scissor corta — pelo caminho GL exato do Renderer |
| `regress_png_layout_ida_e_volta` | encodePngRgb → loadPng byte a byte a 1536×720 (o PNG exportado é VÁLIDO) |

**Provas de mutação coladas** (`/mutacao-R024a/b/c-vermelho.txt`):
- **(a) o `Record::add` no-op**: `regress_layout_dump_nao_mente` +
  `regress_validador_apana` FALHARAM e a FASE 13 ficou com 8 vermelhos — o
  JSON exportado ficaria VAZIO e a auditoria diria «VERDE» por não ver
  NADA (o perigo exato da ferramenta que mente).
- **(b) a regra Sobreposto removida**: `regress_validador_apana` FALHOU em
  `hasRule(Sobreposto)` — e MAIS NADA quebrou (nota honesta: um validador
  que «meio funciona» continua verde nos ecrãs limpos; a sentinela é que
  obriga cada regra a provar que apanha o seu caso).
- **(c) o rasterizador morto**: `regress_fb_rasteriza` FALHOU (o quad não
  está no píxel) + FASE 13 com 3 vermelhos (o PNG sai e decodifica mas é um
  retângulo do clear — o «PNG lido» confirmaria o nada).
- NOTA honesta (a lição da FASE 9 aplicada): um 2.º run da mutação (c)
  mostrou 5 falhas extra na FASE 12.8b — o FLACKE do /tmp com fixtures
  stale de runs anteriores («o flake do disco não é regressão»); com /tmp
  limpo o verde volta por inteiro ao repor.

Recalibração documentada: `wiring091` (a linha «editor de texto» do
Diagnóstico) contava 5 linhas antes da dele — as duas linhas novas do
Grupo B empurraram a do texto para a 8.ª posição; o teste recalibrou para
7 linhas × 48 (o MESMO precedente do G6/R-017, documentado no sítio).

## 10 · RASTREABILIDADE (pedido → causa → fix → teste → harness)

| pedido do Grupo B | implementação | sentinela | FASE 13 |
|---|---|---|---|
| PNG do ecrã | glstub_fb rasteriza + glReadPixels + thumb::encodePngRgb full-res | regress_fb_rasteriza | 13.1/13.3/13.4/13.5 (decodifica, resolução exata) |
| JSON do layout | audit nos widgets do UiContext (choke point) + core/Json.h | regress_layout_dump_nao_mente | 13.1 (as entradas, os botões da toolbar) |
| validador | layout::validate — 6 regras + 2 exceções | regress_validador_apana | 13.2 (o veredito sobre o ecrã real) |
| Auditoria | Diagnóstico → «Auditoria do ecrã» → log + auditoria-<ecrã>.txt | (o walk: FASE 13.2) | 13.2 (o tap pelo rect REAL do registo) |
| PNGs relidos | loadPng de produção + píxel-vs-registo | regress_png_layout_ida_e_volta | 13.1 (painel no sítio, banda povoada, glifos) |
| relatório do estado atual | ESTE ficheiro (secção 8, com os números medidos) | — | o artefacto layout-screens-c33-virtual do CI |

## 11 · NÃO VERIFICADO (honestidade do fecho)

1. **O PNG no DEVICE**: a FASE 13 prova o mecanismo no C33 virtual (a
   rasterização é o STUB; o device tem Mali). O checklist do README
   (Grupo B, 5 itens) pede ao dono: abrir o `layout/editor.png` na
   galeria e confirmar que é o ecrã EXATAMENTE como se vê — a prova do
   glReadPixels no hardware real só existe no hardware.
2. **As contagens da auditoria no DEVICE**: os avisos medidos
   (botões de 40px, labels truncadas) são do ambiente do harness
   (densidade 1.0, insets 24+24, a cena default); o device do dono
   (RMX3624, densidade 2.0 real) pode apresentar números DIFERENTES —
   o checklist pede «Auditoria do ecrã» no aparelho e a comparação
   colada no próximo relatório.
3. **O teclado GBoard por cima do IME**: o export do script correu com o
   TECLADO DA ENGINE aberto (kbOpen=true — deliberado: é o insumo do
   Grupo E); o IME do sistema sobre o editor de script não foi exportado
   (o IME não desenha no backbuffer da app — é outra janela).
4. **A densidade 2.0 no export**: o harness corre a 1.0 (o layout de
   sempre dos testes); o `theme::g_density` viaja no JSON e o validador
   multiplica os 48dp por ele — mas nenhum export de densidade 2.0 foi
   corrido no harness (o Grupo C traz a dupla densidade com o
   `regress_density_escala_dp`).

## 12 · O ESTADO DA SUÍTE NO FECHO

- **test_core**: 834 casos, 0 falhas (830 do Grupo A + 4 sentinelas R-024).
- **c33_virtual**: 374 checks, 0 falhas (340 do Grupo A + 34 da FASE 13).
- **Gates locais**: check_main, link_parity (**115 TUs** — o grep ganhou a
tolerância ao comentário à direita e os 10 TUs cegos entraram, LayoutDump
incluído), jni_parity, glyph_source, docs_lint — verdes (corridos no
passo final antes do commit).
- **CI**: 5/5 jobs esperados; o job do C33 virtual sobe o artefacto
  `layout-screens-c33-virtual` (os PNGs+JSONs+auditoria) em cada run.
