# STATE.md — estado do projeto

Atualizado: 2026-10-24 · vc: 129 · Último commit: este (a 0.12-Z F4 —
a simulação da perda + os 20 cenários; o RELATORIO-0.12-Z-GLCTX.md
§F4);

> Este ficheiro é um ÍNDICE, não é a verdade — a verdade é o código.
> Em caso de divergência: investigar, reportar, corrigir o índice. Nunca inventar.
> Atualiza-se no MESMO commit que fecha a sub-fase (regra §6 abaixo).

## §1 Onde estamos

- **Campanha 0.12-Z (ANTI-PERDA DE CONTEXTO OPENGL + A SOMBRA) — F0..F4
  FECHADAS (vc 125..129)**. **F4 (vc 129)**: o botão «Simular
  perda» (debug-only; 1 frame de sonda morta — a recuperação REAL corre,
  zero atalho) + os 20 CENÁRIOS no c33 (10 SEM WE + 10 COM WE: boot/
  TERM+INIT/RESIZED/tabs/painel/PAUSE+RESUME/foco/perda). O critério
  «recupera em <1 s» provado (1 frame). c33 1113/0. pin
  simulate_loss_recupera + R-069 + M-Z8. **F3 (vc
  128)**: a sonda por frame (2 EGL + glIsProgram/glIsTexture + o
  glGetError em série, limiar 30), a recuperação PELO GESTOR da F1 (o
  reset da interação → o detach → o releaseAll → o invalidateAll → o
  recreateAll → a câmara sã; a falha arma o cooldown de 30), os resets no
  PAUSE/LOST_FOCUS (o ACTION_CANCEL já soltava), o chip «GL: OK/RECRIADO
  n» e o botão «Reiniciar renderer». Picking CPU puro. pin
  context_recovery_ciclo + R-068 + M-Z6/M-Z7. **F2 (vc 127)** **F2 (vc 127)**: a CAUSA RAIZ da sombra (o
  fogDensity NaN PASSAVA pela clamp — comparações com NaN sempre falsas —
  e envenenava a exp2 do lit FS: outColor NaN = o PRETO; §F2.1); a cura
  na FONTE (Environment::sane), a névoa em highp, a grelha EXPLÍCITA,
  o depth func reposto. pin env_sana_nan + R-067 + M-Z5. **F1 (vc 126)**: render/GlResources.h
  NOVO — o GLResourceManager (a FONTE ÚNICA da recriação; o ACHADO: o
  g_particles.destroy() FALTAVA na lista manual do term da 0.6.7); as 8
  camadas no android_main; o term via invalidateAll(); o boot via
  recreateLayer() com o break do renderer (o M-Z4: o for sem break
  deixava o boot continuar SEM RENDER); cameraEnsureSane() +
  editorRecalcHitCaches() no lifecycle. A fila UI→render: sem corrida
  por construção. 0 FBOs. pins ×2 + R-066 + M-Z3/M-Z4.
  **F0 (vc 125)** (a cláusula calma: SÓ o ciclo de vida GL/render +
  testes; UI visual, V.ONI, importadores, física e .gmesh INTACTOS).
  **O DIAGNÓSTICO**: render/GlDebug.h NOVO (os
  Masks com o mestre OFF = render normal byte a byte; os contadores
  onCreated/onDestroyed por kind; o PassState::capture/describe/
  differs; o passBoundary que loga SÓ a MUDANÇA; o checkErrors) · os
  gates POR PASSE no frame real (env/sky/geometria nos 2 caminhos/
  partículas/grelha/gizmos/UI com a toolbar viva; o vidro) · o
  inventário GL no boot · os timestamps no INIT/TERM/LOST_FOCUS/
  GAINED_FOCUS (os 2 últimos NOVOS) · os 11 toggles nas Definições ·
  o §F0.3 fecha as 4 CANDIDATAS
  da sombra com ficheiro:linha (H1 o scrim/sheet preso que come os
  toques — EditorUi.cpp:3947/4086/4258, main.cpp:7424; H2 a textura
  GPU morta fora do boot; H3 o céu/WE com estado vencido; H4 o vidro
  dormente) e o §F0.4 ANALISA O CLIMA «não funciona» (a causa mais
  provável: presets sub-pixel — 0.015 m ≈ 1.3 px a 720p/7u; + o
  contexto perdido + o g_fxTime congelado + a origem no 0,0,0 + o NONE
  que silencia) — a decisão é do dono). 0 FBOs. pins ×4 + R-065.

- **Campanha 0.12-Y-F2 (O CLIMA E O VENTO) — FECHADA no código
  (vc 124; a história no RELATORIO-0.12-Y-F2-WEATHER.md)** (a cláusula
  calma: SÓ o prompt; a DECISÃO DE ARQUITETURA honrada: o clima NÃO
  renderiza — CONFIGURA o ParticleEmitter do MESMO TIC). **F2 (vc
  124)**: o WindTIC (o vento GLOBAL, a fonte pura windVector) · o
  AmbientEnvironment (o applyWeatherPreset com os valores EXATOS corre
  NA CRIAÇÃO E no Inspector; o updateStorm do flash) · o VS ganha
  `+ (uWind * localTime)` · o FS do céu ganha o uThunderFlash · os
  presets Wind/Rain/Snow/Storm · o .goni transporta (o runtime NUNCA
  viaja) · as secções Wind/Ambient no Inspector.
  pins ×5 (wind_tic_cria · ambient_rain_preset · ambient_snow_preset ·
  ambient_serialize · wind_uniform_passed — tests/test_012y_f2.cpp
  NOVO) + R-064 regress_weather + M-Yg1/M-Yg2 VERMELHAS AO VIVO. O
  HOTFIX do CI (o mesmo vc): o repoRoot() do pin devolvia "." e o ctest
  corre com cwd = build-tests — a macro REPO_ROOT do cmake manda (o
  padrão test_012y.cpp).

- **Campanha 0.12-V (OS 3 BUGS DO VIEWPORT 3D) — FECHADA no código
  (vc 123; o CI VERDE)** (a cláusula calma: SÓ viewport/gizmo/render do
  editor + testes; a história no RELATORIO-0.12-V-VIEWPORT.md).
  **V (vc 123)**: o DIAGNÓSTICO com ficheiro:linha antes de mexer e a
  cura: (1) o TELEPORTE do arrasto (o feedback escrevia world = TRS
  LOCAL); (2) o GIZMO ancorado no tr->pos local-as-world; (3) o TAMANHO
  variável (sem tan(fov/2)). A FONTE ÚNICA TransformSystem::composeWorld
  + worldToLocalPos + gizmoAnchor (o MESMO par no hit e no draw) + a
  fórmula do dono dist·tan(fov/2)·kScreenLen (k 0.16; o toque ≥48px).
  O importador/preset PROVADOS inocentes. pins ×4 (o caminho REAL com
  sondas de píxel) + R-063 + M-V1/V2/V3 VERMELHAS AO VIVO.

- **Campanha FASE UI (O REDESIGN VISUAL) — A..J FECHADAS (vc 111..120,
  o CI VERDE em cada parte; um commit por parte; A HISTÓRIA no
  RELATORIO-FASE-UI.md — A os tokens · B a barra 40 · C a hierarquia ·
  D o viewport · E o inspector · F a barra fina · G as folhas · H o
  editor de código · I as abas Áudio/UI · J a tela de projetos).
- **Campanha 0.12-Y (O SALTO VISUAL E DE ASSETS) — A..E, o C-FIX, a F1
  e a F2 fechadas (vc 106..124)** (a cláusula calma: SÓ o prompt; a
  V.ONI intocada; A HISTÓRIA COMPLETA nos RELATORIO-0.12-Y-*.md — A o
  PAN (vc 106) · B o ÍCONE IA (107) · C o PBR + WorldEnvironment
  (108) · C-FIX o Inspector do WE (109) · D o CÉU PROCEDURAL (110) ·
  E o DIA/NOITE (121) · F1 o PARTICLEEMITTER (122) · F2 o CLIMA E O
  VENTO (124) — cada uma com os pins, a sentinela e as mutações ao
  vivo no seu relatório).
- **Campanha 0.12-QoL (UI POLISH + NAVEGADOR + AUTO-SAVE) — FECHADA
  (A..D, vc 102–105)** (no RELATORIO-0.12-QoL.md; pins ×13 + R-056/057/058).
- **Campanha 0.12-X (CÂMARA DE TERCEIRA PESSOA) — FECHADA (3.9x, vc
  101)** (camrig/camdrag/limit como Contínuos, o lookDelta a ZERAR, o
  conflito camrig↔follow; pins ×14 + R-055; no RELATORIO-0.12-X-CAMRIG.md).
- **Campanha 0.12-U (V.ONI BLOCO A) — FECHADA (3.9a..3.9c + o FECHO, vc 98–100)** (as 7 features no registo, 64 entradas; pins ×37 + R-052..R-054; o platformer do juiz VERDE; nos RELATORIO-0.12-U-*.md).
- **Campanha 0.12-E (EMERGÊNCIA + REDESIGN 2.5D) — FECHADA (E-1..E-3, vc 95–97)** (o SDF 3D abortado; E-1 o desenho simples, E-2 a paleta no Theme.h — superada pela FASE UI A —, E-3 os efeitos 2.5D; no RELATORIO-0.12-E.md).
- **Campanha 0.12-D (identidade visual) — PARCIALMENTE ABORTADA (a 0.12-E)**: o D-4 (o LOGO) sobrevive; o D-1/D-2/D-3 DORMENTES; D-5..D-8 CANCELADOS (no RELATORIO-0.12-D.md).
- **Campanha 0.12-C (layout) — FECHADA (C-1..C-4, vc 87–90)** (no
  RELATORIO-0.12-C.md: proporções, menus, tipografia, seletor).
- **Campanha 0.12-B (arquitetura da UI) — FECHADA (B-1..B-4, vc 83–86)**
  (a árvore de UI, o Inspector de UI, o canvas, as ações 2-tap; no RELATORIO-0.12-B.md).
- **Campanha 0.12-A (bugs P0) — FECHADA (vc 82)** (as rows LOD, o
  refreshCatalog, o tooltip e o veredito RAM; no RELATORIO-0.12-A.md).- **Campanha 0.11-Y (identidade visual) — FECHADA** (o fecho é o commit32a6785: os 13 PNGs + o pin y_fecho; a FASE 31 fica no replay).- **0.11-CTX — FECHADA** com CI verde (os detalhes no §2 e no relatório).- **0.11-W (Render Systems) com CI verde** — falta o PIN do dono no C33 real; o fecho (BLOCO B) só por ordem do dono.

## §2 O que está FEITO

- 0.12-QoL A..D (vc 102–105) — a limpeza visual, o navegador compacto,
  o viewport limpo e o auto-save (no RELATORIO-0.12-QoL.md §1..§4).
- 0.12-X 3.9x (vc 101) — a câmara de terceira pessoa (no
  RELATORIO-0.12-X-CAMRIG.md): camrig/camdrag/limit como Contínuos no
  REGISTO; o TykerState guarda camYaw/camPitch; o lookDelta (o consumo
  ZERA em Play); o compilador rejeita camrig+follow/look; pins ×14 +
  R-055 + M-3.9x1/x2.
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
- 0.12-D D-1..D-3 (vc 91–93) — o shader SDF de widget, o vidro real e
  falso e o fogo (no RELATORIO-0.12-D.md §1..3): a fonte única ui/Sdf.h
  + o 3.º programa, o Kawase dual + a cache, o ui/Fire.h + a rampa do
  dono; pins ×13 + M-D1a/b..D3a/b; DORMENTES desde a 0.12-E (os
  achados: o rebind do ARRAY_BUFFER, a corrida dos jobs, o flip da
  captura, o continue do blend, a banda sem teto).
- 0.6.x–0.9.5 — fundação da engine (V.ONI core, device real, CI 5 jobs).- 0.12-C (vc 87–90) — layout, organização e consistência (no
  RELATORIO-0.12-C.md §1..4): C-1 as proporções (hierarquia 20%,
  viewport ≥60%, hierHeaderRects); C-2 os menus e a barra de topo
  (menuSheetLayout, o play A CHAMA, o gizmo duplicado fora); C-3 a
  escada de superfícies + o idioma (os 4 tons, card/elevated); C-4 o
  picker + os vazios com AÇÃO + A REPARAÇÃO da árvore (614 de volta);
  pins ×20 + M-C1a/b..C4a/b.
- 0.12-B (vc 83–86) — a árvore de UI, o Inspector de UI, o canvas com a
  moldura e as ações de cena 2-tap (pins ×21 + R-050/R-051; no
  RELATORIO-0.12-B.md).
- 0.12-A (vc 82) — os 4 bugs P0 fechados (o ficheiro:linha, os pins e
  o veredito vivem no relatório próprio: docs/RELATORIO-0.12-A.md + o
  INVEST-RAM): A-1 as rows LOD JÁ EXISTIAM (PÁRA no fix cego; pins ×5);
  A-2 o refreshCatalog no arranque + sceneRowIcon (pins ×3); A-3 o
  scissor3dRect + o tooltip do gizmo (pins ×3); A-4 a instrumentação
  GONI_BENCH_RAM com a CURVA a reproduzir o salto (veredito: RAM — a
  cache de blocos a ciclizar); pins com M-A1a/b..A3a/b vermelho→verde;
  o achado fora da lista (inspectorRowCount −4 rows mesh, sem sintoma)
  documentado no relatório §6.
- 0.11-Y (vc 77–81) — a identidade visual completa (o ícone, o tema, o layout, os cartões, o fecho com os 13 PNGs).
- 0.9.6 + 0.9.6.4–0.9.6.20 (vc 48–67) — identidade, grupos UI, editor, import glTF, orçamento 3D, leis de desenho/toque, janelas.
- 0.10-M..0.11-W (vc 68–76) — o .gmesh v3, a hierarquia, os materiais glTF, a animação skinada, o skinned cull+LOD, o instancing e o streaming progressivo (os DETALHES vivem nos relatórios da docs/ — o índice não é o arquivo).

## §3 O que está PENDENTE (por ordem)

1. **Fecho 0.11-W (BLOCO B, só com CONTINUAR do dono)** — verificar R-048,
   RELATORIO-0.11-W-FECHO.md, atualizar README/BACKLOG/STATE.md; o pin
   C33 (§1) entra no §3 do fecho.
2. **3 métricas qualitativas da CTX** fecham no relatório da campanha
   SEGUINTE (tempo até à 1.ª ação útil, erros de contexto, cobertura do CODEMAP).

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
- `docs/RELATORIO-0.12-QoL.md` — o relatório da 0.12-QoL (o A no §1 —
  a limpeza visual; o B no §2 — o navegador compacto; as mutações
  coladas, as recalibrações com nota; os C/D entram nos §3/§4).
- `docs/RELATORIO-0.12-X-CAMRIG.md` — o relatório da 0.12-X (a câmara
  de terceira pessoa: as convenções fechadas, o hook lookDelta, o
  TIMING do input, as mutações coladas no §5).
- `docs/RELATORIO-0.12-X-ABERTURA.md` — a abertura da 0.12-X (as 3
  verificações §0.2 + as 5 perguntas com ficheiro:linha).
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
