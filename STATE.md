# STATE.md — estado do projeto

Atualizado: 2026-10-19 · vc: 100 · Último commit: este (o 0.12-U 3.9c —
os RELÓGIOS: every/after/when no AST+parser, o estado por nó AST que
morre no stop; R-054 + M-3.9c1/2/3; o relatório próprio diz o resto)

> Este ficheiro é um ÍNDICE, não é a verdade — a verdade é o código.
> Em caso de divergência: investigar, reportar, corrigir o índice. Nunca inventar.
> Atualiza-se no MESMO commit que fecha a sub-fase (regra §6 abaixo).

## §1 Onde estamos

- **Campanha 0.12-U (V.ONI BLOCO A — a REDUÇÃO DE TRABALHO) — FECHADA
  (3.9a..3.9c + o FECHO, vc 98–100)** (as 7 features no registo — o
  View P universal, as 14 Funções, o spawn pela porta do Host, os
  relógios every/after/when com o estado por nó AST; 64 entradas; pins
  ×37 + R-052/R-053/R-054 + M-3.9a1..c3; o CRITÉRIO DO PLATFORMER corre
  VERDE — o juiz final; a história vive no RELATORIO-0.12-U-ABERTURA.md
  e nos relatórios das sub-fases + o FECHO).
  **3.9a (vc 98)**: o View P aceita EXPRESSÃO dos 7 tipos via
  valueText() (a string de sempre byte a byte); as 14 FUNÇÕES no
  registo (Kind::Funcao NOVO — abs/min/max/clamp/lerp/floor/ceil/round/
  sqrt/sin/cos/random/random_int/distance; sin/cos em GRAUS; o
  despacho no evalCall consulta a TABELA e o argc vem da entrada; a fn
  do utilizador vence); pins ×15 + R-052 + M-3.9a1/a2/a3
  vermelho→verde. test_core 0 falhas · c33 1010/0 · vc 98.
  **3.9b (vc 99)**: o spawn(tipo, x, y, z) → TIC — a entrada no registo,
  o Host::spawnTic NOVO (default honesto) e a EngineHost real (a lista
  viva esfera/box; ACTIVE/VISIBLE + Transform3D + prim + o NOME ÚNICO
  base.001); pins ×9 + R-053 + M-3.9b1/b2. c33 1010/0 · vc 99.
  **3.9c (vc 100)**: every/after/when — a sintaxe nova no AST+parser
  (as 3 palavras reservadas), o estado POR NÓ AST no Impl (morre no
  stop; zero static); pins ×12 + R-054 + M-3.9c1/2/3. test_core 0
  falhas · c33 1010/0 · vc 100. O FECHO correu (o platformer julgou
  VERDE — a campanha fecha).
- **Campanha 0.12-E (EMERGÊNCIA + REDESIGN 2.5D, FOGO AVERMELHADO) —
  FECHADA (E-1..E-3, vc 95–97)** (a ordem do dono abortou o SDF 3D;
  cláusulas: ZERO matemática 3D, ZERO mexidas no FontAtlas, TICs
  intocados; os DETALHES vivem no RELATORIO-0.12-E.md).
  **E-3 (vc 97)**: o vidro α0.85 + a linha âmbar 1dp a 20%, a sombra
  falsa preta α0.15 deslocada 3dp, os ativos com o gradiente
  fireRed→fireOrange e a borda de ruído 2D em scroll (o ui/Noise2D.h
  NOVO); pins ×4 + M-E3a/b + a FASE 35. c33 1010/0.
  **E-2 (vc 96)**: a paleta EXATA do dono no Theme.h (o surface #141416,
  o text1 #F2F2F5, os tokens fireRed/fireOrange novos); os
  static_asserts aferraram a fonte única no 1.º build. c33 1007/0.
  **E-1 (vc 95)**: a restauração da UI básica — o desenho volta ao
  SIMPLES 2D no UiContext, a maquinaria SDF/vidro/fogo fica DORMENTE
  (zero chamadores), 13 pins reformados + 6 recalibrações. c33 1007/0.
- **Campanha 0.12-D (identidade visual) — PARCIALMENTE ABORTADA PELO
  DONO (a 0.12-E)**: o D-4 (o LOGO, vc 94) SOBREVIVE; o D-1 (SDF), o
  D-2 (vidro) e o D-3 (fogo) estão DORMENTES (zero chamadores — vc
  91-93); D-5..D-8 CANCELADOS. A história vive no
  RELATORIO-0.12-D.md §1..4.
- **Campanha 0.12-C (layout, organização e consistência) — FECHADA
  (C-1..C-4, vc 87–90)** (os DETALHES no RELATORIO-0.12-C.md): C-1 as
  proporções (a hierarquia 20%, o viewport ≥60%), C-2 os menus e a
  barra de topo, C-3 a tipografia/idioma/superfícies, C-4 o seletor +
  os vazios com AÇÃO + a REPARAÇÃO da árvore (614 de volta).
- **Campanha 0.12-B (arquitetura da UI) — FECHADA (B-1..B-4, vc 83–86)**
  (os DETALHES vivem no RELATORIO-0.12-B.md): B-1 a árvore de UI (pins
  ×9, R-050), B-2 o Inspector de UI (pins ×5), B-3 o canvas (pins ×3),
  B-4 as ações de cena 2-tap (pins ×4, R-051).
- **Campanha 0.12-A (bugs P0) — FECHADA (vc 82)** (os DETALHES vivem no
  RELATORIO-0.12-A.md + no INVEST-RAM): A-1 as rows LOD JÁ EXISTIAM,
  A-2 o refreshCatalog no arranque, A-3 o scissor3dRect + o tooltip,
  A-4 o veredito RAM (a cache de blocos a ciclizar).
- **Campanha 0.11-Y (identidade visual) — FECHADA** (o fecho é o commit
  32a6785: os 13 PNGs + o pin y_fecho; a FASE 31 fica no replay).
- **0.11-CTX — FECHADA** com CI verde (os detalhes no §2 e no relatório).
- **0.11-W (Render Systems) com CI verde** — falta o PIN do dono no C33
  real; o fecho (BLOCO B) só por ordem do dono.

## §2 O que está FEITO

- 0.12-U FECHO (vc 100) — o CRITÉRIO DO PLATFORMER corre VERDE (o pin
  platformer_bloco_a_juiz_final: 7 features em cadeia, os contadores do
  juiz batem) + README/ARCHITECTURE §5/BACKLOG atualizados (no
  RELATORIO-0.12-U-FECHO.md) — a CAMPANHA 0.12-U FECHA.
- 0.12-U 3.9c (vc 100) — os relógios every/after/when (no
  RELATORIO-0.12-U-3.9c.md): o estado por nó AST; pins ×12 + R-054 +
  M-3.9c1/2/3.
- 0.12-U 3.9b (vc 99) — o spawn (no RELATORIO-0.12-U-3.9b.md): o Host
  ganha a porta de criação; pins ×9 + R-053 + M-3.9b1/b2.
- 0.12-U 3.9a (vc 98) — o View P universal + as 14 funções do registo +
  a distance (no RELATORIO-0.12-U-3.9a.md): pins ×15 + R-052 +
  M-3.9a1/a2/a3; a campanha 0.12-U abre com o critério do platformer
  como juiz final.
- 0.12-E E-3 (vc 97) — os efeitos 2.5D + o FECHO da 0.12-E (no
  RELATORIO-0.12-E.md §3): sombra falsa, vidro 0.85+âmbar, gradiente
  fogo, ruído em scroll; pins ×4 + M-E3a/b; a FASE 35.
- 0.12-E E-2 (vc 96) — o tema FOGO AVERMELHADO (§2 do relatório): a
  paleta exata do dono, os 2 tokens novos, os gates a aferrar a fonte
  única.
- 0.12-E E-1 (vc 95) — a restauração da UI básica (§1): o SDF 3D
  abortado pelo dono, o desenho simples 2D de volta, os 13 pins
  reformados + as FASE 32/33 do c33, as 6 recalibrações de volta.
- 0.12-D D-4 (vc 94) — o logo «G em chama» (no RELATORIO-0.12-D.md §4):
  as setas mortas, a rampa por traço, o ignite, as 3 versões, o espelho
  Java, o overdraw da barra; pins ×4 + M-D4a/b; a FASE 34.
- 0.12-D D-3 (vc 93) — o fogo (no RELATORIO-0.12-D.md §3): a fonte única
  ui/Fire.h, a rampa do dono + a proveniência (d3_palette.py), a borda
  animada aditiva, o glow por baixo, as brasas com portão, a composição
  simultânea, o toggle Baixo nos três; pins ×5 + M-D3a/b; os achados (o
  continue do blend, o 0x0304, a banda sem teto, a duplicação do stub, o
  play em destaque da C-2).
- 0.12-D D-2 (vc 92) — o vidro real e falso (no RELATORIO-0.12-D.md §2):
  a captura + o Kawase dual + a cache, o ramo uSdfTexOn, o vidro falso
  (gradiente/reflexo/ruído/Fresnel); pins ×4 + M-D2a/b; os achados (o
  flip da captura, o grão nas sondas, o teto da cache).
- 0.12-D D-1 (vc 91) — o shader SDF único de widget (no
  RELATORIO-0.12-D.md §1): o 3.º programa (progSdf_), a fonte única
  ui/Sdf.h, o toggle Alto/Médio/Baixo, os botões pelo widget SDF; pins ×4
  + M-D1a/b; a FASE 32; os achados (o rebind do ARRAY_BUFFER, a corrida
  fina dos jobs).
- 0.6.x–0.9.5 — fundação da engine (V.ONI core, device real, CI 5 jobs).
- 0.12-C C-4 (vc 90) — o seletor + os vazios (no RELATORIO-0.12-C.md
  §4): o picker pela fonte única (o breadcrumb que salta, a pesquisa
  propósito 10, as pastas ponto fora, o Subir fixo, as linhas
  ícone+nome+seta); os 4 vazios com AÇÃO; os glifos únicos (o Image
  novo); pins ×8 + M-C4a/b; OS ACHADOS (o rolar importava, o claim
  comia o IMPORTAR, o botão fora do drawer 240); A REPARAÇÃO da árvore
  (614 ficheiros de volta + as 6 recalibrações); a CAMPANHA 0.12-C
  FECHA.
- 0.12-C C-3 (vc 89) — a escada de superfícies + o idioma (no
  RELATORIO-0.12-C.md §3): os 4 tons + os tokens card/elevated; «Áudio»
  /«Exportar para Downloads»/«N TICs»; pins ×4 + M-C3a/b.
- 0.12-C C-2 (vc 88) — os menus e a barra de topo (no
  RELATORIO-0.12-C.md §2): menuSheetLayout; o play A CHAMA; o GIZMO
  fora (o duplicado do Mover); pins ×4 + M-C2a/b.
- 0.12-C C-1 (vc 87) — as proporções e os painéis (no
  RELATORIO-0.12-C.md §1): hierarquia 20%; viewport ≥60% de fábrica;
  hierHeaderRects; pins ×4 + M-C1a/b.
- 0.12-B B-4 (vc 86) — as ações de cena (no RELATORIO-0.12-B.md
  §25..29): o renomear/duplicar/apagar 2-tap + o manifesto round-trip;
  pins ×4 + M-B4a; R-051; a CAMPANHA 0.12-B FECHA.
- 0.12-B B-3 (vc 85) — o canvas com a moldura do device (no
  RELATORIO-0.12-B.md §20..24): o seletor + a moldura + o véu + as guias
  + a grelha + o toast opaco; pins ×3 + M-B3a/b.
- 0.12-B B-2 (vc 84) — o Inspector de UI (no RELATORIO-0.12-B.md §14..19):
  as secções colapsáveis + os campos da base/por tipo + a zona morta
  (TouchControls::dead) + os resets + o «sem remover joystick»; pins ×5 +
  M-B2a/b.
- 0.12-B B-1 (vc 83) — a árvore de UI (o relatório próprio:
  docs/RELATORIO-0.12-B.md): Control/UIRoot (os storages ids 11/12); a
  compat migrateUiCanvasToTree (o UiCanvas sai; DE MEMÓRIA); o
  serializer (as fontes partilhadas appendUiElementJson/
  fillUiElementJson; o UIRoot FANTASMA curado no §10); o editor (o modo
  árvore com espelho + sync-back; o Inspector de UI; uniqueNewTicName);
  o Play (drawUiTree/hitTestUiTree/uiElementFromHit); **Pins ×9 +
  M-B1a/b/c** + **R-050**; 8 recalibrações com nota.
- 0.12-A (vc 82) — os 4 bugs P0 fechados (o relatório próprio:
  docs/RELATORIO-0.12-A.md; a investigação:
  docs/RELATORIO-0.12-A-INVEST-RAM.md):
  - **A-1** — as rows `lod distance`/`impostor` JÁ EXISTEM (ficheiro:
    linha no relatório) — PÁRA no fix cego; os 5 pins NOVOS da UI
    (test_012a_lodui.cpp) fecham o gap de cobertura; M-A1a/M-A1b
    vermelho→verde; ZERO produção trocada.
  - **A-2** — o `refreshCatalog()` entra no ARRANQUE e no
    `createSceneNamed` (os contadores a 0 morrem); `sceneRowIcon()` NOVA
    (a fonte única do ícone de cena); pins ×3; M-A2a/M-A2b vermelho→verde.
  - **A-3** — `safe::scissor3dRect()` NOVA (o modo UI reserva o painel
    direito no SCISSOR; byte a byte fora do modo UI) + o tooltip do
    gizmo morre (legendTimer/legendArm; nasce no largar, vive 1,5 s);
    pins ×3; M-A3a/M-A3b vermelho→verde.
  - **A-4** — a instrumentação sob `#ifdef GONI_BENCH_RAM` (o sampler
    `bench_ram:` 1×/s no engine.log) + o alvo opcional `bench_ram`
    (option OFF) — A CURVA REPRODUZ O SALTO: linear 50k…4,1M e explosão
    a 8,2M/12,3M no cruzamento EXATO do teto de VRAM (192 MB) com
    9 758/23 470 evictions/frame; `evc_par == evc_orb`; o RSS é LINEAR —
    VEREDITO: RAM (a cache de blocos a ciclizar), NÃO GPU/CPU; o
    protocolo do dono + as recomendações no relatório próprio.
  - **O achado fora da lista (cláusula calma, NÃO fixado)**: o
    `inspectorRowCount` subestima a MALHA em 4 rows para mesh file
    (EditorLayout.h:433; sem sintoma — documentado no relatório §6).
- 0.11-Y (vc 77–81) — a identidade visual completa (o ícone, o tema, o
  layout, os cartões, o fecho com os 13 PNGs) — ver §1 e docs/
  RELATORIO-0.11-Y-FECHO.md.
- 0.9.6 + 0.9.6.4–0.9.6.20 (vc 48–67) — identidade, grupos UI, editor,
  import glTF, orçamento 3D, leis de desenho/toque, janelas.
- 0.10-M..0.11-W (vc 68–76) — o .gmesh v3 (streaming/blocos/impostores),
  a hierarquia (drag-reparent), os materiais glTF, a animação skinada
  (UBO 128), o skinned cull+LOD, o instancing e o streaming progressivo
  (os DETALHES vivem nos relatórios próprios da docs/ e no §2 do
  CODEMAP — o índice não é o arquivo).

## §3 O que está PENDENTE (por ordem)

1. **Fecho 0.11-W (BLOCO B, só com CONTINUAR do dono)** — verificar R-048,
   RELATORIO-0.11-W-FECHO.md, atualizar README/BACKLOG/STATE.md; o pin
   C33 (§1) entra no §3 do fecho.
2. **3 métricas qualitativas da CTX** fecham no relatório da campanha
   SEGUINTE (tempo até à 1.ª ação útil, erros de contexto, cobertura do
   CODEMAP).

## §4 Decisões fechadas (NÃO REABRIR)

- O culling estava CERTO: far 500 e ~31% dos blocos fora do frustum; bl
  5164/5178 explicado pela margem AABB ±4. Não é bug (§1.8 da 0.11-W).
- Instancing só para blocos de MESMA assinatura (mesh+material), teto
  64 instâncias por draw, toggle disponível, skin/LOD mismatch respeitado.
- Impostor: limiar exclusivo (0=OFF), billboard com a normal PARA a
  câmara, PNG RGBA com MASK discard, TIC skinado NUNCA tem impostor,
  pico de RAM = UM bloco (contrato R-041), impostor falhado não existe.
- Streaming: orçamento por frame no import; open do .gmesh = tabela
  (192 B + índices), dados só por bloco a materializar.
- Compat: campos novos são opcionais com default; .goni/.gmesh antigos
  reproduzem byte a byte o comportamento anterior.
- Stub de testes: rasterizador honesto, SEM GLES real; pins rodam com
  injeção de dependências.
- V.ONI: fonte única no registo (VoniRegistry); docs gerados pelo
  voni_refgen (bijeção aferrada por sentinela).
- UI: unidade é TIC (nunca Node/GameObject/Entity); cores só em
  ui/Theme.h; vocabulário e acentos vigiados por gates.
- Documentação sem placeholders nem achismo (docs-lint R-016).
- STATE.md e CODEMAP.md são ÍNDICES; o código é a verdade.
- O APK instalado no device é SEMPRE o artefacto assinado do CI.
- 0.12-C: as PASTAS ponto ficam fora do picker; os FICHEIROS ponto
  ficam (a decisão 0.8.5 mantém-se); a lupa é SÓ «pesquisar»; o vazio
  usa o glifo da PRÓPRIA função (emptyStateIcon); os vazios têm SEMPRE
  uma ação real (nunca um convite que manda procurar o botão).

## §5 Regras de convivência

- Um commit só conta como fechado com o CI VERDE em todos os jobs.
- Toda a regressão estrutural ganha sentinela R-NNN com a mutação COLADA
  no relatório (quebrar → vermelho → repor → verde).
- Compat byte a byte nos formatos .goni/.gmesh; defaults novos explícitos.
- Cláusula calma de campanha: só o que o prompt da campanha manda.
- PARAR entre campanhas — a próxima só arranca com CONTINUAR do dono.
- Se o estado real divergir do esperado: PÁRA e reporta.
- Commit que fecha sub-fase com código TEM de incluir STATE.md (o gate
  docs-lint valida; ver §6).
- A árvore vai INTEIRA no commit (a lição da reparação: um git add -A
  numa árvore parcial EMPURRA as deleções — conferir o diffstat antes
  do push; 614 deleções = um repo partido).

## §6 Regra de atualização

O STATE.md é atualizado no MESMO commit que fecha a sub-fase: se o diff
toca código (cpp/, app/, tests/) e a mensagem declara sub-fase/feat/fix,
o STATE.md viaja no mesmo commit — o gate docs-lint fica vermelho com
"sub-fase declarada sem atualização do STATE.md" caso contrário.
Commits puramente documentais não são afetados.

## §7 Bloqueios conhecidos

- **Perna 2 do 0.11-T-FIX** — adiada pelo dono (o dono sabe; retoma só
  por ordem dele).
- **Pin C33 da 0.11-W** — depende do dono (instalar APK vc 76, importar
  203 MB, R-017, PNG do HUD). Nada na fila depende disto para arrancar.
- **R-048 (streaming progressivo)** — a verificar no fecho da 0.11-W;
  se não existir entrada no REGRESSOES.md, é dívida declarada, não
  inventada.
- Corrida latente do corte paralelo sob TSAN (§4.1 do 0.11-T) — passo de
  diagnóstico continue-on-error no CI; a caça continua suspensa.

## §8 Onde ir buscar detalhe

- `CODEMAP.md` — onde vive o quê + receitas de extensão (o ler código).
- `ARCHITECTURE.md` — o mapa dos módulos (o porquê das camadas).
- `docs/REGRESSOES.md` — sentinelas R-NNN (consulta, não leitura inteira).
- `BACKLOG.md` — rastreador de campanhas (na RAIZ do repo).
- `docs/RELATORIO-0.12-E.md` — o relatório da 0.12-E (a EMERGÊNCIA: o
  E-1 no §1 — o aborto do SDF 3D, os pins reformados, as recalibrações;
  as FASE 2/3 do redesign 2.5D fecham nos §2/§3).
- `docs/RELATORIO-0.12-D.md` — o relatório da 0.12-D (a D-1 no §1 — o
  shader SDF, o toggle, os pins, as mutações, os achados do rebind e da
  corrida dos jobs; D-1..D-3 DORMENTES desde a 0.12-E — a história).
- `docs/RELATORIO-0.12-C.md` — o relatório da 0.12-C (a C-1 no §1, a C-2
  no §2, a C-3 no §3, a C-4 no §4 — o picker, os vazios, os achados do
  gesto/claim/drawer, a REPARAÇÃO da árvore, as mutações coladas).
- `docs/RELATORIO-0.12-B.md` — o relatório da 0.12-B (a B-1 no §1..13,
  a B-2 no §14+, a B-3 no §20+, a B-4 no §25+ — as ações de cena, o
  R-051 e o fecho da campanha).
- `docs/RELATORIO-0.12-A.md` + `docs/RELATORIO-0.12-A-INVEST-RAM.md` —
  a 0.12-A (os 4 bugs P0 com ficheiro:linha, os pins ×11, as 6 mutações
  coladas; a investigação RAM vs GPU com o veredito CACHE THRASH).
- `tests/bench_ram_curve.cpp` + o alvo `bench_ram` (tests/CMakeLists,
  option GONI_BENCH_RAM OFF) — a curva 50k…12,3M do A-4 no hospedeiro.
- `scripts/y0_gen_icons.py` — o gerador dos PNGs do launcher a partir
  da fonte do dono (o threshold de luminância documentado lá dentro).
- `docs/arquivo/` — os relatórios das campanhas FECHADAS (71, git mv;
  o RELATORIO-0.9.6.md NÃO está lá — é a âncora do gate release-identity
  R-015; a 0.11-T-FIX também não — Perna 2 adiada, a campanha não fechou).
- `docs/TEMPLATE_PROMPT.md` — a pirâmide de leitura dos prompts.
- `VONI_referencia.md` — a referência V.ONI gerada (fonte: VoniRegistry).
- `ci/scope.txt` — os caminhos que uma campanha pode tocar.

## §9 As 4 perguntas antes de qualquer trabalho

1. §1 confirma que posso avançar? (campanha/sub-fase em curso = a minha?)
2. §4 cobre o que vou tocar? (nenhuma decisão fechada reabrindo?)
3. §7 diz que estou desbloqueado? (nenhum bloqueio no meu caminho?)
4. O prompt alinha com §3? (a tarefa é mesmo a pendente da ordem?)

Se qualquer resposta for não: PÁRA e reporta ao dono.
