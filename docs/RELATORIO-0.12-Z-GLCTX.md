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

## §F1 — A RECRIAÇÃO CORRETA DO CONTEXTO (a preencher no commit da fase)

## §F2 — A ORIGEM DA SOMBRA E A ORDEM DOS PASSES (a preencher)

## §F3 — A DETEÇÃO E A RECUPERAÇÃO (a preencher)

## §F4 — A PREVENÇÃO E OS TESTES 20/20 (a preencher)
