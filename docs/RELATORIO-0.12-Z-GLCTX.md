# RELATORIO-0.12-Z-GLCTX.md — ANTI-PERDA DE CONTEXTO OPENGL + A SOMBRA

Campanha 0.12-Z (cláusula calma: SÓ o ciclo de vida GL/render do editor +
testes — zero features; UI visual, V.ONI, importadores, física e .gmesh
INTACTOS). Uma fase por commit, CI verde antes de avançar. Este relatório
acrescenta um § por fase; o §F0 é o diagnóstico pedido ANTES de corrigir.

---

## §F0 — O DIAGNÓSTICO (FASE 0, sem mudar comportamento)

### F0.1 — A INFRAESTRUTURA (o que entrou)

| Ficheiro | O que faz |
|---|---|
| `render/GlDebug.h` (NOVO) | os `Masks` (mestre + 8 passes; mestre OFF = render normal), os contadores `onCreated/onDestroyed` por kind (program/buffer/vao/texture/fbo), o `PassState::capture/describe/differs`, o `passBoundary` (log SÓ quando o passe altera o estado), `checkErrors`, `checkUniformLoc` |
| `platform/main.cpp` | os gates POR PASSE no frame real; as fronteiras `ceu/geometria/particulas/pass3D/grelha`; o inventário GL no fim do boot (`lifecycle: recursos GL do processo: prog +n/-n …`); timestamps `+%.2fs desde o boot` no INIT/TERM/LOST_FOCUS/GAINED_FOCUS (os 2 últimos NÃO eram logados — main.cpp:5700..5714); o despacho dos 11 toggles |
| `ui/SettingsPage.{h,cpp}` | a secção Diagnóstico ganha: mestre + fundo/céu + WorldEnvironment + geometria + grelha + gizmos + pós + UI + vidro + log de estado + log de erros (ids 5840..5850; de SESSÃO — nunca persistidos) |
| `ui/UiContext.cpp` | o `glassTopLine` respeita o toggle do vidro |
| `render/{Renderer,Material,Grid,SkyRenderer,ParticleRenderer,Mesh,Texture}.cpp` + `ui/FontAtlas.cpp` | os contadores no nascer/morrer de cada recurso (o lit conta-se na camada dele — Material.cpp) |

### F0.2 — O INVENTÁRIO DO CONTEXTO (o que o código já fazia — a linha de partida)

- **TERM_WINDOW (main.cpp:5652..5700)**: desde a 0.6.7, destrói TODOS os
  recursos GL com o contexto ainda corrente (GpuAssets, atlas das fontes,
  cubo, quad do impostor, prims, grid, céu, renderer) e SÓ DEPOIS o EGL.
- **INIT_WINDOW (main.cpp:5381..5577)**: o boot em 6 passos re-cria e
  re-upe tudo (o `contextRecreated` loga a re-criação; o
  `g_windowTerms/g_windowInits` contam os ciclos).
- **NÃO existe** `setPreserveEGLContextOnPause` (NativeActivity pura, sem
  GLSurfaceView): o contexto MORRE no term e nasce no init — o caso
  "preservado" não se aplica, e o F1 documenta a hipótese contrária.
- **NÃO EXISTEM FBOs** no motor (0× `glGenFramebuffers` no produto): o
  candidato "FBO/pós-processamento" do dono está FORA — o toggle "pós" é
  no-op DOCUMENTADO (o pin `gldebug_masks_default` aferra o default).

### F0.3 — A SOMBRA: AS CANDIDATAS (com ficheiro:linha — para FECHAR no device com os toggles)

| # | Hipótese | Onde | Como o F0 fecha |
|---|---|---|---|
| H1 | **Sheet/modal/scrim preso** — o véu 40-60% cobre o ecrã inteiro e come os toques (zoom/pan mortos É o mesmo defeito): `theme::kTheme.scrimMenu` desenhado a 0,0..sw,sh (EditorUi.cpp:3947, 4086, 4258); o backdrop modal (main.cpp:7424 `drawModalBackdrop`); o modal de import (main.cpp:7290..7300). Se o flag "aberto" sobreviver ao resume com o input em estado estranho, o dono vê «sombra escura + nada funciona» | ui/EditorUi.cpp:3947 · main.cpp:7424,7290 | toggle UI OFF: com os painéis fora, a sombra DESAPARECE → era a UI; o log de estado na fronteira da UI mostra o que sobrou |
| H2 | **Textura/mesh GPU morta** (sobrevivência de contexto num caminho fora do boot — ex.: o upload LAZY da textura de partícula em `ParticleRenderer::setTexture`) → o GLES amostro handle morto como PRETO; uma malha grande fica «sombra» | render/ParticleRenderer.cpp (setTexture lazy) · TERM main.cpp:5652 | toggle geometria/partículas OFF um de cada vez; os contadores term↔init no engine.log mostram fugas (criado > destruído) |
| H3 | **Clear/céu com estado vencido** (env de cena anterior; depth write do céu a tapar a grelha) | SkyRenderer.cpp:323..332 (repõe depth/cull/mask — verificado limpo) · Grid.cpp:202..211 (idem) | toggles céu/WE OFF: a sombra desaparece → era o céu/WE; as fronteiras logam o estado de saída de cada passe |
| H4 | **Vidro da UI** — a captura D-2 abortada deixou `glassTex_`/`captureGlass` no Renderer (Renderer.cpp:788; DORMENTE — o bloco consumidor morreu em 0.12-E, main.cpp:7259..7263) | Renderer.cpp:788 | toggle vidro OFF; o caminho dormente é confirmado (ninguém chama captureGlass no frame) |

O UTILIZADOR FECHA O CASO: Definições → Diagnóstico → mestre ON → desligar
TUDO menos a grelha (a grelha TEM de aparecer sempre — pin
`gldebug_isolam_grelha`) → ligar um passe de cada vez até a sombra voltar →
o ÚLTIMO passe ligado é o culpado → colar o engine.log.

### F0.4 — O CLIMA/PARTÍCULAS «NÃO FUNCIONA» — A ANÁLISE PEDIDA

A lógica F2 está PROVADA no hospedeiro (5 pins verdes + c33_virtual 1008/0
— o preset aplica os valores exatos, o vento entra no VS, o flash entra no
FS, o .goni transporta o estado). No DEVICE, por ordem de probabilidade:

1. **Escala sub-pixel (a mais provável)**: os presets do prompt usam
   startSize 0.015 m (chuva/storm) e 0.04→0.02 m (neve). A 720p, fov 60°,
   distância de órbita ~7 u: `px = size / (2·dist·tan(30°)) · 720` ≈
   **1.3 px** (chuva) e **3.3→1.7 px** (neve) — com alpha 0.7 azul-claro
   SOBRE o céu. É literalmente invisível ao braço esticado. O jorro da F1
   é visível (0.25 m ≈ 22 px) — por isso «o emissor funciona e o clima não».
   **O QUE FAZER (decisão do dono — é tweak de spec, fora desta campanha)**:
   (i) aprovar tamanhos de preset ~0.05-0.08 (chuva) e ~0.06-0.09 (neve) —
   4 linhas no `AmbientEnvironment.h::applyWeatherPreset`; ou (ii) manter a
   spec e aproximar a câmara do emissor para validar.
2. **Perda de contexto sem recuperação** (o motivo desta campanha): depois
   de um ciclo sair→voltar mal recuperado, QUALQUER passe pode ficar morto
   (handle órfão) — as partículas e o céu são os primeiro a sumir. As F1-F3
   fecham isto (GLResourceManager + sondas por frame).
3. **g_fxTime congelado**: a pausa do play congela o relógio das partículas
   (a honra do g_skyTime). Um `paused` preso após o resume = partículas
   ESTATICAS (parecem «não funcionar»). A F3 (reset do estado de interação
   no resume) cobre.
4. **Origem no (0,0,0)**: os presets de clima nascem na origem; com a
   câmara longe, a chuva cai LONGE. Mover o TIC de clima com o gizmo (que
   desde a 0.12-V está correto).
5. **NONE silencia**: ciclar o botão do clima para além de Storm volta a
   NONE → maxParticles 0 (desejado, mas surpreende — o Inspector mostra).

A VERIFICAÇÃO no device com o F0: mestre ON + só partículas (a chuva
desenha sozinha?), o inventário do boot mostra o ParticleRenderer a nascer,
o log de erros apanha GL errors do passe.

### F0.5 — O RELATÓRIO DE 16 ITENS (FASE 0)

1. **Objetivo**: diagnosticar sem mudar comportamento (logs, toggles,
   contadores, fronteiras de passe).
2. **Causa raiz da campanha (estática, ficheiro:linha)**: as 4 candidatas
   da sombra em F0.3; o clima invisível em F0.4.1-5. NENHUMA correção nesta
   fase (o dono manda: reportar ANTES de corrigir).
3. **Sintomas cobertos**: sombra escura; grelha que desaparece; clima
   «morto»; input morto — todos agora ISOLÁVEIS por passe e rastreáveis no
   engine.log.
4. **O que mudou**: F0.1 (7 ficheiros de produto + 2 de UI + 1 módulo
   novo, cada um justificado na tabela).
5. **O que NÃO foi tocado**: layout visual da UI (os toggles usam as
   linhas existentes da secção Diagnóstico), V.ONI, importadores, física,
   .gmesh, os formatos, o render de cada passe (os gates só SALTAM passes
   com o mestre ON).
6. **Ordem dos passes (a de sempre, agora LOGADA)**: clear → céu/WE →
   geometria (+impostors) → partículas → grelha → gizmos → UI.
7. **Estado GL**: as fronteiras capturam viewport/scissor/blend/depth
   test-write-func/cull/FBO à entrada e à saída de cada passe; o log só
   dispara na MUDANÇA (o vazamento é o sinal).
8. **Logs novos**: `lifecycle: INIT_WINDOW #n (+t)` / `TERM_WINDOW #n (+t)`
   / `LOST_FOCUS (+t)` / `GAINED_FOCUS (+t)` (NOVOS) /
   `lifecycle: recursos GL do processo: prog +n/-n buf +n/-n …` (no fim do
   boot) / `gldebug: passe X ALTEROU estado: …` / `gldebug: GL error 0x…`
   / `gldebug: uniform X = -1 …`.
9. **Interruptores novos**: Definições → Diagnóstico (11 linhas; mestre +
   8 passes + 2 logs; de sessão).
10. **Testes novos**: `gldebug_masks_default` · `gldebug_counters_ciclo` ·
    `gldebug_isolam_grelha` · `gldebug_pass_state` (tests/test_012z_ctx.cpp
    NOVO) + sentinela **R-065** `regress_glctx_fase0`.
11. **Mutações**: **M-Z1** (gate do caminho direto esquecido → falso
    isolado) VERMELHA → verde; **M-Z2** (mestre nasce ON) VERMELHA → verde.
12. **Verificação local**: test_core 0 falhas (235 TUs; GONI_SKIP dos
    flakes de disco documentados — o CI é o árbitro); gates verdes.
13. **CI**: o commit desta fase (o run no GitHub Actions; só avança com
    verde — a regra do dono).
14. **Capturas**: o dono gera com Definições → Diagnóstico → «Exportar
    layout» (PNG+JSON por passe, com/sem WorldEnvironment) — a sonda de
    píxeis no hospedeiro cobre o isolamento (lit==0 com geometria OFF).
15. **Riscos**: os toggles mudam o que o frame submete — com o mestre ON o
    HUD/statusLine pode mostrar contadores a zero (esperado); o isolation
    UI esconde os painéis (a toolbar fica — a via de escape).
16. **Próximo passo**: FASE 1 — o GLResourceManager (a recriação correta
    do contexto; commit separado).

---

## §F1 — A RECRIAÇÃO CORRETA DO CONTEXTO (FASE 1)

### F1.1 — O QUE ENTROU

| Ficheiro | O que faz |
|---|---|
| `render/GlResources.h` (NOVO) | o **GLResourceManager**: `registerLayer(name, owner, invalidate, recreate)` (a ORDEM da 0.6.7), `invalidateAll()` (o TERM/perda — nunca para numa camada morta), `recreateAll()` (para no 1.º falhado e REPORTA `lastFailed`), `recreateLayer(name)` (o boot em passos), os contadores `invalidations/recreations`. GL-free, testável no CI |
| `platform/main.cpp` | as 8 camadas registadas UMA vez no android_main (font, font-mono, gpu-cube, gpu-imp-quad, grid, sky, particles, renderer — **a partícula FALTAVA na lista manual do term**, o achado do F1); o TERM usa `invalidateAll()` (a lista manual MORREU); o boot [3/6]/[4/6] recria PELO GESTOR com a numeração do dono intacta; **o renderer obrigatório mantém o break** (sem render não há frame — o mesmo comportamento de sempre); **`cameraEnsureSane()`** (NaN/fora dos limites → o PADRÃO, a cena fica; sã → só os clamps) no INIT/RESIZED/RESUME; **`editorRecalcHitCaches()`** (o gizmo/grab/lookTrack recalcam — os caches de ecrã velhos apontavam janelas que já não existem) |

### F1.2 — O RELATÓRIO DE 16 ITENS (FASE 1)

1. **Objetivo**: o contexto EGL pode morrer a qualquer pausa/rotação/janela
   /OOM — a recriação correta com DONO único; o caso "preservado"
   documentado (NativeActivity pura: o contexto morre no term — se algum
   OEM preservar, o invalidate+recreate continua correto, os velhos
   objetos leakam 1× e o log mostra).
2. **Causa raiz (ficheiro:linha)**: a lista de destroys do term era MANUAL
   (main.cpp:5652..5700 da árvore F0) — **o `g_particles.destroy()`
   FALTAVA** (a camada de 0.12-Y-F1 é POSTERIOR à lista 0.6.7): depois de
   um term, o `g_particles.init()` do boot recriava por cima de handles
   órfãos (o init chama destroy() primeiro — os ids velhos eram deletados
   NO CONTEXTO NOVO onde não existem: inofensivo mas frágil; qualquer
   camada futura entraria na mesma lista morta).
3. **Sintomas cobertos**: a recriação correta (o dono: «nenhum handle GL
   nem uniform location em cache ou static sobrevive»); a câmara
   degenerada no resume; os caches de hit-test velhos no resize.
4. **O que mudou**: F1.1 (2 ficheiros).
5. **O que NÃO foi tocado**: o render de cada passe, a cena, os formatos,
   V.ONI, importadores, física, o layout visual da UI.
6. **FBOs (o item 5 do dono)**: NÃO EXISTEM no motor (0×
   `glGenFramebuffers` — o F0.2 provou); o `glCheckFramebufferStatus` não
   tem alvo; a nota fica no §F0.2 — nada a recriar.
7. **A fila UI→render (o item 6 do dono)**: A AUDITORIA provou que NÃO há
   corrida por construção: a UI do editor corre NA THREAD DE RENDER
   (immediate-mode no frame()); os únicos caminhos Java→native são o
   storage (a fila `storage::pollResult()` drenada NO TOPO do frame —
   main.cpp:9500) e o IME (a fila ImeQueue — o MESMO padrão). Nada a
   mudar; o contrato fica documentado.
8. **A proibição de one-time init (o item 4)**: a auditoria
   `static bool` em render/* devolveu ZERO guardes de init único (o F0
   aferiu); o gestor reforça: o recreate pode correr MUITAS vezes.
9. **Logs novos**: `glresources: 8 camadas registadas…` (no boot do
   processo), `glresources: camada X FALHOU a recriar` (a degradação com
   dono), `camera: estado inválido … repõe o PADRÃO (a cena fica)`.
10. **Testes novos**: `glresources_ciclo` (a ordem + a falha sob controlo
    com camadas de TESTE — o ambiente tem fontes de sistema e não é
    determinístico; as camadas GL do produto recriam individualmente) ·
    `camera_sana` (NaN → o padrão; 1e9 → clamp; sã → byte a byte).
11. **Sentinela**: **R-066** `regress_glctx_fase1` — o registo no
    android_main; o term SEM a lista manual (a região do CASE é que
    manda); o boot pelo gestor com o break do renderer; a câmara sã nos
    TRÊS eventos; o contrato do gestor no fonte.
12. **Verificação local**: test_core 0 falhas; gates verdes.
13. **CI**: o run do commit desta fase (o F0 fechou verde no e09c2e5).
14. **Capturas**: as do dono no device (o ciclo sair→voltar com o
    «Exportar layout»); o engine.log mostra o inventário term↔init.
15. **Riscos**: o recreateLayer("font") no hospedeiro de CI depende das
    fontes do sistema (o teste usa camadas de TESTE para a ordem/falha —
    determinístico por construção).
16. **Próximo passo**: FASE 2 — a origem da sombra e a ordem dos passes
    (o reset de estado à entrada de cada passe; os clamps do WE).


## §F2 — A ORIGEM DA SOMBRA E A ORDEM DOS PASSES (FASE 2)

### F2.1 — A CAUSA RAIZ DA SOMBRA (com ficheiro:linha — o pedido do dono)

**O NaN no `fogDensity` do WorldEnvironment envenena a exp2 do lit FS.**
A cadeia:

1. `Environment::clampFields` (components/Environment.h:82..91 da árvore
   F1) aferia os limites com comparações (`if (v < 0)`/`if (v > max)`) —
   **comparações com NaN são SEMPRE falsas**: um NaN entrava e SAÍA.
2. O lit FS consumia `uFogDensity` direto: `fog = 1.0 - exp(-pow(len *
   uFogDensity, 2.0))` (render/Material.cpp:157) — **NaN propaga**: fog =
   NaN → `mix(c, uFogColor, NaN)` → `outColor = NaN` → **o fragmento sai
   INDEFINIDO (preto no Mali do C33)** — o dono vê «uma sombra escura por
   cima da cena».
3. O mediump (fp16 no Mali-GE8320) amplificava: `pow(x, 2.0)` em fp16 a
   distância perdia precisão e criava bandas/escuridão SEM NaN (a H3 sem
   corrupção).
4. ONDE NASCE o NaN: corrupção do .goni (o serializer aceita qualquer
   f32), um HDRI com média NaN (o ambient do C), ou um float degenerado
   de um edit por slider. INTERMITENTE por construção — o sintoma «às
   vezes» do dono.

**Porque explica o «MESMO SEM WorldEnvironment»**: o resto das
candidatas do F0.3 (H1 o scrim preso, H2 textura morta) continua de pé —
mas o H3 tinha ESTE buraco real, agora fechado na FONTE. O dono fecha o
caso no device com os toggles do F0 (o §F0.3 mantém-se).

### F2.2 — O QUE ENTROU

| Ficheiro | O que faz |
|---|---|
| `components/Environment.h` | `clampFields()` ganha **`sane(v, def, lo, hi)`** — a REJEIÇÃO de NaN/inf ANTES da clamp (o NaN/inf volta ao DEFAULT; os finitos ficam byte a byte). Cobre fogDensity/ambient/skyColor/fogColor/sunDirection/cloudCoverage/cloudSpeed/timeOfDay/dayCycleSpeed/cloudHeight |
| `render/Material.cpp` | o lit FS: a névoa em **`highp float`** + **sem pow** (`fogD*fogD` — exato e barato) + `max(uFogDensity, 0.0)` local (a 2.ª linha de defesa) — a fórmula exp2 byte a byte para densidades finitas |
| `render/Grid.cpp` | o depth test da grelha **EXPLÍCITO**: `glEnable(GL_DEPTH_TEST)` + `glDepthFunc(GL_LESS)` (o inheritado era o vazamento — o dono: «a grelha NUNCA pode ser coberta por nada do mundo») |
| `platform/main.cpp` | a ENTRADA do pass 3D repõe `glDepthFunc(GL_LESS)` (o único estado que ninguém repunha — o test/write/blend/cull já eram repostos pelos draws) |

### F2.3 — A ORDEM DOS PASSES (antes = depois — agora AFERRADA)

limpar → céu/fundo (depth OFF, mask OFF, blend OFF — SkyRenderer.cpp:323) →
geometria (depth ON, cull ON — Renderer.cpp:465) → partículas (blend ON,
mask OFF) → **grelha (depth test ON EXPLÍCITO + func LESS + mask OFF +
blend ON)** → gizmos (UI-space, sem depth — beginUiPass) → UI (depth OFF,
scissor OFF, viewport cheio — beginUiPass:401..410; o fim repõe blend —
endFrame:600). **A grelha e os gizmos NUNCA são cobertos por nada do
mundo.** As fronteiras do F0 logam a ordem real no engine.log.

### F2.4 — O RELATÓRIO DE 16 ITENS (FASE 2)

1. **Objetivo**: a origem da sombra + a ordem fixa dos passes + o reset
   de estado à entrada + os clamps do WE.
2. **Causa raiz**: F2.1 (o NaN na névoa — ficheiro:linha).
3. **Sintomas cobertos**: a sombra escura (a H3 fechada na fonte); a
   grelha que «às vezes» desaparece (o depth test inheritado); as bandas
   de névoa a distância (o mediump).
4. **O que mudou**: F2.2 (4 ficheiros, cada um justificado).
5. **O que NÃO foi tocado**: os shaders do céu/partículas (já highp com
   precision highp float), o tema, V.ONI, importadores, física, .gmesh, o
   layout visual.
6. **Ordem dos passes**: F2.3 (antes = depois; agora explícita e aferrada
   pela sentinela).
7. **Estado GL**: depth func reposto à entrada do 3D; a grelha com o trio
   do dono explícito; o fim da UI repõe blend (endFrame — verificado).
8. **Fullscreen quads**: SÓ o céu (antes da geometria, blend OFF, mask
   OFF — o contrato do dono cumpre-se; 0 FBOs, 0 outros quads).
9. **Clamps WE**: o sane NaN/inf + as clamps existentes — TODOS os campos
   f32 do Environment.
10. **Testes novos**: `env_sana_nan` (o NaN/inf morre na fonte; os finitos
    byte a byte; o meio-dia da D intacto).
11. **Sentinela**: **R-067** `regress_glctx_fase2` (o sane no fonte; o
    highp/x*x; o pow PROIBIDO; a grelha explícita; o depth func na
    entrada).
12. **Verificação local**: test_core 0 falhas; c33_virtual 1008/0; gates
    verdes.
13. **CI**: o run do commit desta fase.
14. **Capturas**: as do dono (com/sem WE; a névoa a 0 e a 1 — o byte a
    byte das densidades finitas é o pin).
15. **Riscos**: o highp na névoa custa ALU mínima (1 exp + 1 mul) — o C33
    mantém o orçamento (o c33 1008/0 prova o replay).
16. **Próximo passo**: FASE 3 — as sondas por frame + a recuperação +
    o reset do estado de interação.


## §F3 — A DETEÇÃO E A RECUPERAÇÃO (FASE 3)

### F3.1 — O QUE ENTROU

| Peça | Onde | O que faz |
|---|---|---|
| A SONDA por frame | main.cpp `contextEnsureAlive()` no TOPO do frame() | 2 chamadas EGL (contexto/surface nulos = perdido) + `glIsProgram(uiProgram)` + `glIsTexture(whiteTexture)`; o `glGetError` em SÉRIE com limiar 30 (~0.5 s a 60 fps — um erro isolado NÃO é perda); cooldown de 30 frames entre tentativas (nunca martelar) |
| A RECUPERAÇÃO | main.cpp `contextRecover(why)` | o ciclo do dono: reset da interação (`editorRecalcHitCaches` + `g_input.resetAll`) → `detachRenderersFromGpu` → `g_gpu.releaseAll` (os assets recarregam LAZY da CPU) → `invalidateAll` → `recreateAll` (o gestor da F1 — UMA fonte) → `cameraEnsureSane` → o log «recuperação #n COMPLETA» |
| O reset da interação | APP_CMD_PAUSE + APP_CMD_LOST_FOCUS | o gesto atual, os ponteiros ativos, o gizmo agarrado, o eixo, o drag, a hit-test cache (o F1 já cobria INIT/RESIZED/RESUME; a F3 completa o dono) — o ACTION_CANCEL/pointer-up JÁ soltavam tudo (InputState.cpp:41..43, verificado) |
| O indicador de saúde | statusLine (só com o mestre do debug ON) | «GL: OK» / «GL: RECRIADO n» + o log na Consola a cada recuperação |
| O botão do dono | Definições → Diagnóstico → «Reiniciar renderer» | a recriação TOTAL sem fechar o app — o MESMO caminho do contextRecover (zero atalhos) |

### F3.2 — O PICKING (o item 4 do dono — verificado, nada a mudar)

O picking e o gizmo são **CPU puro**: `gizmo::projectPoint` (a matemática
da vp) + a hit-test dos rects — **0 leituras GL** (o único glReadPixels do
produto é o export de layout — main.cpp:1128). NÃO há FBO/textura de
picking → nada para recriar; os únicos handles que os pins usam são os
probes da própria sonda.

### F3.3 — O RELATÓRIO DE 16 ITENS (FASE 3)

1. **Objetivo**: detetar a perda por frame (barata), recuperar sozinho
   (<1 s), avisar, e dar o botão manual.
2. **Causa raiz da campanha**: sem sondas, o app desenhava contra
   contexto morto SEM saber (o §F0.2 mapeou o ciclo; a F1 deu o dono da
   recriação; a F3 fecha o circuito).
3. **Sintomas cobertos**: «o utilizador só percebe depois» — agora a
   deteção é por frame, o log avisa, o indicador mostra.
4. **O que mudou**: main.cpp (a sonda+recuperação+resets+chip+botão),
   Renderer.h (o accessor uiProgram), SettingsPage.{h,cpp} (a linha),
   stubs (as sondas glIs*/EGL do hospedeiro).
5. **O que NÃO foi tocado**: o render de cada passe, o picking (CPU puro),
   V.ONI, importadores, física, .gmesh, o tema, o layout visual.
6. **Ordem dos passes**: intocada (a da F2).
7. **Estado GL**: a recuperação repõe TUDO pelo gestor (as 8 camadas) +
   os uniforms de cada camada renascem nos seus init (a F1).
8. **Logs novos**: «CONTEXTO PERDIDO (…) — a recuperação #n começa» /
   «recuperação #n COMPLETA» / «a recuperação #n FALHOU (…) — nova
   tentativa em 30 frames».
9. **Interruptores novos**: nenhum (o botão usa a secção existente).
10. **Testes novos**: `context_recovery_ciclo` (o ciclo completo no host —
    determinístico: a fonte falha no host e é REPORTADA com cooldown; a
    cena fica; a interação zera; a sonda é ESTÁVEL sem perda).
11. **Sentinela**: **R-068** `regress_glctx_fase3` (a sonda no frame; os
    glIs*; o limiar 30; a recuperação pelo gestor; os resets no
    PAUSE/LOST_FOCUS; o chip; o botão; o ACTION_CANCEL).
12. **Verificação local**: test_core 0 falhas; gates verdes.
13. **CI**: o run do commit desta fase.
14. **Capturas**: as do dono — o chip «GL: RECRIADO n» na status bar
    (debug ON) + o engine.log antes/depois do botão.
15. **Riscos**: a sonda EGL por frame custa ~µs (2 chamadas); o limiar do
    glGetError evita o loop de recuperação por erro benigno.
16. **Próximo passo**: FASE 4 — o botão «Simular perda de contexto» + os
    testes 20/20.


## §F4 — A PREVENÇÃO E OS TESTES 20/20 (a preencher)
