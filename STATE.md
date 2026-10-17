# STATE.md — estado do projeto

Atualizado: 2026-10-18 · vc: 85 · Último commit: este (o 0.12-B B-3, O
CANVAS com a moldura do device — o seletor de proporção, as guias, a
grelha e o toast OPACO, os 3 pins; antes: 813b491 o B-2, O INSPECTOR DE
UI; antes: 032ea2d o B-1, A ÁRVORE DE UI)

> Este ficheiro é um ÍNDICE, não é a verdade — a verdade é o código.
> Em caso de divergência: investigar, reportar, corrigir o índice. Nunca inventar.
> Atualiza-se no MESMO commit que fecha a sub-fase (regra §6 abaixo).

## §1 Onde estamos

- **Campanha 0.12-B (arquitetura da UI) — EM CURSO; B-1..B-3 FECHADAS**
  (o CI do push é o árbitro). **B-3 (o canvas com a moldura do device)**:
  o seletor de proporção ANDROID (as 3 chips do topo — uiCanvasChipsRect
  é a fonte), a MOLDURA centrada com a ÁREA FORA ESCURA (α0.55 — o
  elementRect continua a ser a fonte do layout), as guias de margem
  segura e a grelha de alinhamento (ativáveis), o TOAST OPACO (a fonte
  única vpchrome::toastColors — o contraste WCAG ≥4.5 não depende da
  cena); pins ×3 (test_012b_canvas.cpp) + M-B3a/b vermelho→verde;
  test_uilayout recalibrado (os glifos das chips saem da paridade).
  test_core 0 falhas · c33 1006/0 · vc 85. O C33 do dono está no §24.
  **B-2 (o Inspector de UI — 2D + âncoras)**: **B-2 (o Inspector de UI — 2D + âncoras)**:
  o Inspector de CONTROLO com as 3 secções colapsáveis (o colapso no
  EditorState::inspUiCollapsed; uiInspectorPlanControl é a fonte única),
  os campos da base (margens/pivô/rotação/esticar/opacidade), os campos
  por tipo (texto/letra p/ Label e Button; RAIO/ZONA MORTA/sens p/
  Joystick — a zona morta é o campo NOVO do TouchControls, default 0 =
  byte a byte, o .goni grava `dead` só quando != 0), os botões de RESET
  por campo (o quadrado 30px inteiro; a zona do reset com PRIORIDADE
  sobre o skip dos sliders — o achado §17 do relatório) e SEM «remover
  joystick» (o apagar é do TIC; o uiJoystickPlan/drawJoystickInspector
  saíram; test_joystick recalibrado); pins ×5 (test_012b_inspui.cpp) +
  M-B2a/b vermelho→verde. test_core 0 falhas · c33 1006/0 · vc 84. O C33
  do dono (o PNG do inspector do joystick) está no §19 do relatório.
  **B-1 (a árvore de UI — UI como TICs)**: **B-1 (a árvore de UI — UI como TICs)** — o
  Control NOVO (a base 2D: pos2D/tamanho/âncoras 9+esticar/margens/pivô/
  rotação/visível/opacidade; os elementos são KINDS — sem classes novas,
  a cláusula do prompt), o UIRoot NOVO (o marcador), um TIC filho por
  elemento, o UiCanvas DEPRECATED (o leitor converte —
  migrateUiCanvasToTree no fim do loadText; a animação Ui* resolve na
  árvore), o serializer partilha o gerador/leitor de elemento e grava a
  árvore (os campos da base só quando não-default), o editor tem o modo
  árvore (a vista espelho + sincronização) e o Play desenha/hit-testa a
  árvore; pins ×9 (test_012b_uitree.cpp) + M-B1a/b/c vermelho→verde;
  **R-050 NOVA**; 8 recalibrações legítimas com nota (o CONTRATO DE
  ABERTURA mudou por ordem do prompt); o UIRoot FANTASMA apanhado e
  curado (o null check do dump — §10 do relatório). test_core 0 falhas ·
  c33_virtual 1006/0 · vc 83. O pin C33 do dono (a PNG da hierarquia com
  os 7 TICs) está no §12 do relatório. PRÓXIMA: B-2 (o Inspector de UI
  2D + âncoras) por ordem do fluxo da campanha.
- **Campanha 0.12-A (bugs P0) — FECHADA** (o CI do push é o árbitro):
  A-1 (o diagnóstico provou que as rows LOD/impostor JÁ EXISTEM na UI
  desde o vc 72/76 — PÁRA no fix cego por ordem do prompt; os 5 pins da
  UI novos + M-A1a/b vermelho→verde fecham o gap de cobertura), A-2 (o
  refreshCatalog no arranque + na criação de cena; sceneRowIcon = camadas),
  A-3 (scissor3dRect no modo UI + o tooltip legendTimer 1,5 s/largar),
  A-4 (sampler bench_ram sob ifdef + o bench da curva + o veredito: a
  cache de blocos a ciclizar — RAM, não GPU). test_core 1077/0 ·
  c33_virtual 1006/0 · vc 82. PARAR — a próxima só arranca com CONTINUAR
  do dono.
- **Campanha 0.11-Y (identidade visual) — FECHADA** (o fecho é o commit
  32a6785: os 13 PNGs por ecrã + o pin y_fecho). Y-0..Y-3 + o fix — 20
  pins novos; a FASE 31 fica no replay para SEMPRE.
- **0.11-Y (o tema o layout os cartões o fecho) e 0.11-CTX — FECHADAS**
  com CI verde (os detalhes vivem no §2 e nos relatórios próprios; o
  run do 3a6d934 ficou VERMELHO no probe 18.2 e o CI apanhou — a cura
  viajou na Y-2; o RELATORIO-0.9.6.md voltou a docs/ e o ctx_refs_ok
  distingue citação de transcrição; 98,8 % de redução no pior caso).
- **0.11-W (Render Systems) com CI verde** — falta o PIN do dono no C33
  real (instalar o APK vigente, importar 203 MB, R-017, PNG do HUD);
  o fecho (BLOCO B) só por ordem do dono.

## §2 O que está FEITO

- 0.6.x–0.9.5 — fundação da engine (V.ONI core, device real, CI 5 jobs).
- 0.12-B B-3 (vc 85) — o canvas com a moldura do device (no
  RELATORIO-0.12-B.md §20..24): o seletor + a moldura + o véu + as guias
  + a grelha + o toast opaco; pins ×3 + M-B3a/b; a paridade recalibrada.
- 0.12-B B-2 (vc 84) — o Inspector de UI (no RELATORIO-0.12-B.md §14..19):
  as secções colapsáveis + os campos da base/por tipo + a zona morta
  (TouchControls::dead) + os resets + o «sem remover joystick»; pins ×5 +
  M-B2a/b; o achado da skip-list (o reset com loop próprio).
- 0.12-B B-1 (vc 83) — a árvore de UI (o relatório próprio:
  docs/RELATORIO-0.12-B.md):
  - **Control/UIRoot** — components/Control.h (a base 2D + os 10 kinds;
    o payload é o UiElement por valor) + components/UIRoot.h (o
    marcador); storages + registry ids 11/12 (o FIM — ids antigos
    intactos).
  - **A compat** — migrateUiCanvasToTree no loadText (UiCanvas → UIRoot
    + filhos; o parent por NOME vira pai TIC; o canvas sai; DE MEMÓRIA)
    + a animação Ui* resolve na árvore por nome (AnimationPlayer).
  - **O serializer** — appendUiElementJson/fillUiElementJson (as fontes
    partilhadas) + o Control com os campos da base opcionais + o UIRoot
    com o null check OBRIGATÓRIO (o fantasma apanhado).
  - **O editor** — ensureUiRoot/uiAddTreeElement/uiAddTreeJoystick; o
    modo árvore no drawUiViewport (espelho + sync-back); o Inspector de
    UI edita o elem ao vivo (reanchor/repante/apaga TIC);
    uniqueNewTicName (a variante criação).
  - **O Play** — drawUiTree/hitTestUiTree/uiElementFromHit; todos os
    TouchControls desenham.
  - **Pins ×9 + M-B1a/b/c** + **R-050**; 8 recalibrações com nota.
- 0.12-A (vc 82) — os 4 bugs P0 fechados (o relatório próprio:
  docs/RELATORIO-0.12-A.md; a investigação: docs/
  RELATORIO-0.12-A-INVEST-RAM.md):
  - **A-1** — o diagnóstico provou que as rows `lod distance`/`impostor`
    JÁ EXISTEM (ficheiro:linha no relatório) — PÁRA no fix cego; os 5
    pins NOVOS da UI (test_012a_lodui.cpp) fecham o gap de cobertura;
    M-A1a/M-A1b vermelho→verde; ZERO produção trocada.
  - **A-2** — o `refreshCatalog()` entra no ARRANQUE e no
    `createSceneNamed` (os contadores a 0 morrem); `sceneRowIcon()` NOVA
    (a fonte única do ícone de cena); pins ×3; M-A2a/M-A2b
    vermelho→verde.
  - **A-3** — `safe::scissor3dRect()` NOVA (o modo UI reserva o painel
    direito no SCISSOR; byte a byte fora do modo UI) + o tooltip do
    gizmo morre (legendTimer/legendArm; nasce no largar, vive 1,5 s);
    pins ×3; M-A3a/M-A3b vermelho→verde.
  - **A-4** — a instrumentação sob `#ifdef GONI_BENCH_RAM` (o sampler
    `bench_ram:` 1×/s no engine.log) + o alvo opcional `bench_ram`
    (option OFF) — A CURVA REPRODUZ O SALTO: linear 50k…4,1M
    (0,01→1,11 ms) e explosão a 8,2M/12,3M (376/900 ms) no cruzamento
    EXATO do teto de VRAM (192 MB) com 9 758/23 470 evictions/frame;
    `evc_par == evc_orb` (o thrash continua com a câmara parada); o RSS
    é LINEAR sem penhasco — VEREDITO: RAM (a cache de blocos a
    ciclizar), NÃO GPU/CPU; o protocolo do dono + as recomendações no
    relatório próprio.
  - **O achado fora da lista (cláusula calma, NÃO fixado)**: o
    `inspectorRowCount` subestima a MALHA em 4 rows para mesh file
    (EditorLayout.h:433 vs o plano 6 rows; sem sintoma — o
    contentHeight deriva do plano; o pior caso teórico 65 > o buffer
    64) — documentado no relatório §6 para a próxima campanha.
- 0.11-Y (vc 77–81) — a identidade visual completa (o ícone, o tema, o
  layout, os cartões, o fecho com os 13 PNGs) — ver §1 e docs/
  RELATORIO-0.11-Y-FECHO.md.
- 0.9.6 + 0.9.6.4–0.9.6.20 (vc 48–67) — identidade, grupos UI, editor,
  import glTF, orçamento 3D, leis de desenho/toque, janelas.
- 0.10-M (vc 68) — .gmesh v3 por blocos (streaming/blocos/texturas/thumbs).
- 0.11-H FASE 1 hierarquia (vc 69) — drag-reparent, apagar/duplicar
  sub-árvore, colapso, ícone por tipo.
- 0.11-M FASE 2 materiais glTF (vc 70) — parse, GTB2, props no lit,
  blend por blocos, samplers no upload.
- 0.11-S FASE 3 esqueleto/animação (vc 71) — slerp/cubic/step, UBO 128
  bones, V.ONI stop/blend, o fix dos clips skinados.
- 0.11-T FASE 3.5a GPU efficiency (vc 72) — skinned cull + LOD;
  0.11-T-FIX Perna 1 (vc 73) — o contorno da corrupção do corte paralelo
  (Perna 2 ADIADA pelo dono — ver §7).
- 0.11-W ABERTURA (docs) — cf6279a.
- 0.11-W 3.5b instancing (vc 74, d0c765d) — blocos de mesma assinatura
  6 TICs → 1 dc; teto 64/draw; R-047.
- 0.11-W 3.75a streaming progressivo (vc 75, 0c11b14) — o freeze do
  import morre por orçamento; pico de RAM = 1 bloco (R-041).
- 0.11-W 3.75b impostores (vc 76, 1524148 + caaef02) — PNG alfa por
  distância; dc do TIC longe = 1; R-049; fix caaef02 (o ImpostorIndex.cpp
  faltava no CMake do app).
- 0.11-Y Y-0 Ícone no launcher ✅ vc 77 — o «G em chama»
  (docs/iconegonevv.png) no launcher: adaptativo (fundo #08080A +
  foreground na zona segura) + legacy composto nas 5 densidades; pins
  y0_* ×7 (test_fase_y0_icone.cpp); o ícone antigo (0.9.6.10) fora do
  launcher, intacto no histórico.
- 0.11-Y Y-0-FIX Escala do G ✅ vc 78 — o pin C33 apanhou o G a ~25 %
  (a bbox do HALO fazia o enquadramento); a bbox de escala passa a ser
  o CONTEÚDO SÓLIDO (alpha==1 ⇔ lum ≥ T1) e o G ocupa 65 % da zona
  segura (39,7 % do canvas; legacy 65 % do lado); 15 PNGs regenerados,
  XMLs/manifest intactos; pin y0_escala NOVO (mede os PNG reais via
  stb_image) — pins y0_* ×8; M-Y0-fix vermelho→reposto→verde.
- 0.11-Y Y-1 Tema ✅ vc 79 — o poço #08080A (o clear lê o token) + o
  vidro #151517/#1E1E21 (-5 %, mesmo alfa) + a hairline ÂMBAR
  #4A3714 α0.60 + accent2 #FF6B00 e glow #FF3D00 α0.25 NOVOS; o botão
  ativo é a chama (glow 2 quads α decrescente + gradiente vertical
  accent→accent2 por vértice, zero mudanças de shader); glassTopLine
  (1 dp, 20 %) pronta para a Y-3; pins y1_* ×5 (TU novo test_fase_y1_tema.cpp)
  + M-Y1a/b/c/d vermelho→reposto→verde; R-028 recalibrada (a border
  sai da rampa neutra — é o âmbar do accentDim).
- 0.11-Y Y-2 Layout ✅ vc 80 — as regras CruzaTabBar/PainelFora no
  validador + a flag overlay + a FASE 31 (13 ecrãs × 3 checks); 3 bugs
  reais curados (o toast dp(), o back 36×36, os rótulos da régua); pins
  y2_* ×3 + M-Y2a/b vermelho→verde.

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

## §5 Regras de convivência

- Um commit só conta como fechado com o CI VERDE em todos os jobs.
- Toda a regressão estrutural ganha sentinela R-NNN com a mutação COLADA
  no relatório (quebrar → vermelho → repor → verde).
-Compat byte a byte nos formatos .goni/.gmesh; defaults novos explícitos.
- Cláusula calma de campanha: só o que o prompt da campanha manda.
- PARAR entre campanhas — a próxima só arranca com CONTINUAR do dono.
- Se o estado real divergir do esperado: PÁRA e reporta.
- Commit que fecha sub-fase com código TEM de incluir STATE.md (o gate
  docs-lint valida; ver §6).

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
- `docs/RELATORIO-0.11-CTX.md` — o relatório da campanha em curso (a
  medição da redução, as mutações, as divergências corrigidas).
- `docs/RELATORIO-0.11-W-*.md` — a 0.11-W (o fecho é o BLOCO B).
- `docs/RELATORIO-0.11-Y-Y0.md` — o relatório da Y-0 (o ícone no
  launcher; as mutações e os limites da extração).
- `docs/RELATORIO-0.11-Y-Y0-FIX.md` — o relatório do Y-0-FIX (a escala
  do G: a medição do C33, a regra do sólido, o pin y0_escala, a
  M-Y0-fix).
- `docs/RELATORIO-0.11-Y-Y1.md` — o relatório da Y-1 (o tema: o poço,
  a chama, o gradiente por vértice, as primitivas do cristal, as
  mutações M-Y1a..d).
- `docs/RELATORIO-0.11-Y-Y2.md` — o relatório da Y-2 (o layout: as
  regras CruzaTabBar/PainelFora, a varredura FASE 31, os 3 bugs reais,
  as mutações M-Y2a/b).
- `docs/RELATORIO-0.11-Y-Y3.md` — o relatório da Y-3 (o cristal: a
  linha do topo + os divisores gradientes, o D11 superado, as mutações
  M-Y3a/b).
- `docs/RELATORIO-0.11-Y-FECHO.md` — o fecho da campanha (as 5
  sub-fases + o fix, os critérios C33 mapeados às provas, os limites
  declarados).
- `docs/RELATORIO-0.12-B.md` — o relatório da 0.12-B (a B-1: a
  arquitetura, os pins ×9, as mutações coladas, as recalibrações, o
  UIRoot fantasma, os limites, o protocolo C33 do dono).
- `docs/RELATORIO-0.12-A.md` — o relatório da 0.12-A (os 4 bugs P0: os
  diagnósticos com ficheiro:linha, as decisões, os pins ×11, as 6
  mutações coladas, o achado fora da lista, os limites).
- `docs/RELATORIO-0.12-A-INVEST-RAM.md` — a investigação RAM vs GPU do
  A-4 (a curva do hospedeiro, a análise (a)-(d), o veredito CACHE
  THRASH, o protocolo do device do dono, as recomendações).
- `tests/bench_ram_curve.cpp` + o alvo `bench_ram` (tests/CMakeLists,
  option GONI_BENCH_RAM OFF) — a curva 50k…12,3M do A-4 no hospedeiro.
- `scripts/y0_gen_icons.py` — o gerador dos PNGs do launcher a partir
  da fonte do dono (o threshold de luminância documentado lá dentro).
- `docs/arquivo/` — os relatórios das campanhas FECHADAS (71, git mv;
  o RELATORIO-0.9.6.md NÃO está lá — é a âncora viva do gate
  release-identity R-015; a 0.11-T-FIX também não — Perna 2 adiada, a
  campanha não fechou).
- `docs/TEMPLATE_PROMPT.md` — a pirâmide de leitura dos prompts.
- `VONI_referencia.md` — a referência V.ONI gerada (fonte: VoniRegistry).
- `ci/scope.txt` — os caminhos que uma campanha pode tocar.

## §9 As 4 perguntas antes de qualquer trabalho

1. §1 confirma que posso avançar? (campanha/sub-fase em curso = a minha?)
2. §4 cobre o que vou tocar? (nenhuma decisão fechada reabrindo?)
3. §7 diz que estou desbloqueado? (nenhum bloqueio no meu caminho?)
4. O prompt alinha com §3? (a tarefa é mesmo a pendente da ordem?)

Se qualquer resposta for não: PÁRA e reporta ao dono.
