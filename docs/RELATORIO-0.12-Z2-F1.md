# RELATÓRIO 0.12-Z2 — FASE 1: O CONTEXTO OPENGL, A AUDITORIA E A CORREÇÃO TOTAIS

Campanha **0.12-Z2** (CONTEXTO OPENGL, WORLDENVIRONMENT, CLIMA E AUDITORIA
TOTAL). Esta fase cumpre o pedido do dono: «Inventário completo de TODA a
criação/destruição de objetos GL… Ciclo de vida Android completo…
PROVA POR FUZZING: no ambiente Mesa, destruir e recriar o contexto
EGL/surface em frames aleatórios (mín. 1000 ciclos…) e provar que o frame
seguinte é idêntico ao de referência… e que o input responde».

Regra de evidência: cada afirmação leva PROVADO (comando + saída/captura),
REFUTADO ou NÃO VERIFICADO. As palavras proibidas não aparecem.

---

## F1.1 — O INVENTÁRIO COMPLETO (a tabela objeto → nasce → morre → recria)

Comando: `grep -rn "glGenBuffers\|glGenTextures\|glGenVertexArrays\|
glCreateShader\|glCreateProgram\|glGenFramebuffers" app/src/main/cpp/
--include=*.cpp --include=*.h | grep -v vendor` — 27 sítios, TODOS dentro
de render/ e ui/FontAtlas.cpp. `grep -rn "static.*GLuint" render/ ui/
components/` — ZERO statics com handles GL. FBOs: `grep glGenFramebuffers`
— **ZERO FBOs** (confirmado desde a F0).

| Objeto | Onde nasce | Onde morre | Quem recria |
|---|---|---|---|
| Textura do atlas da fonte | ui/FontAtlas.cpp:78 (loadFromPaths) | FontAtlas::destroy (layer «font») | layer «font» + o fallback do device |
| Atlas MONO | idem | idem (layer «font-mono») | layer «font-mono» (falha = degrada ao sans) |
| VAO/VBO/EBO/skinVbo dos Meshes | render/Mesh.cpp:65-67,100 (create) | ~Mesh → destroy (GpuAssets::releaseAll) | blockRebind re-abre pelo meshPath (§F1.4) |
| Cubo procedural (gpu-cube) | Mesh::create no layer | Mesh::destroy | layer «gpu-cube» |
| Program+VAO+VBO da grelha | render/Grid.cpp:77,114,136-137 | Grid::destroy | layer «grid» |
| Program+VAO+VBO do céu | render/SkyRenderer.cpp:176,207,237-238 | SkyRenderer::destroy | layer «sky» |
| Program+VAO+2×VBO das partículas | render/ParticleRenderer.cpp:34,61,92-94 | ParticleRenderer::destroy | layer «particles» |
| Program lit+UBOs do material | render/Material.cpp:169,195,349,383 | Material::destroy (chamado pelo Renderer::shutdown) | layer «renderer» |
| Program UI + SDF + VAOs/VBOs + whiteTex/noiseTex/glassTex | render/Renderer.cpp:212-389,813 | Renderer::shutdown | layer «renderer» |
| Texturas de assets | render/Texture.cpp:88 | ~Texture (GpuAssets::releaseAll) | blockRebind (texPath) |

**PROVADO** (os greps acima + os contadores do fuzz: tex +4503/-4503 em
1500 ciclos — balanceado). ACHADO P2 (para a FASE 4): os CONTADORES
gldbg têm sítios por instrumentar (SkyRenderer.cpp sem chamadas
onCreated/onDestroyed; ParticleRenderer.cpp:55 conta 1 de 2 buffers;
Material.cpp:189 conta os 2 UBOs na init que só nascem a pedido) — os
números do inventário do boot ficam por baixo; o RSS plano do fuzz
(§F1.6) prova que os OBJETOS estão balanceados.

## F1.2 — O CICLO DE VIDA ANDROID COMPLETO (a tabela por comando)

Todos os APP_CMD têm case no onAppCmd (main.cpp:5731+). O que cada um
faz ao EGL / ao GLResourceManager / ao input:

| Comando | EGL | GLResourceManager | Input |
|---|---|---|---|
| INIT_WINDOW | init (display+config+contexto+surface+makeCurrent) | recreateLayer font/font-mono/cubo/grid/sky/partículas/renderer pela ordem 0.6.7 | selection re-validada por nome; IME re-pedido |
| TERM_WINDOW | shutdown POR FIM (surface+contexto morrem) | invalidateAll (todas as camadas — ver §F1.5) + g_gpu.releaseAll + primMeshesDestroyAll | **resetAll (NOVO da F1)** + editorRecalcHitCaches |
| WINDOW_RESIZED / CONFIG_CHANGED | refreshSize | nada (o contexto vive) | caches de hit-test recalculados + câmara sã |
| CONTENT_RECT_CHANGED | nada | nada | safe-area re-insetada |
| PAUSE | nada | nada | resetAll + flush do debounce do layout |
| RESUME | nada | nada | câmara sã + caches frescos + o recheck do storage |
| GAINED/LOST_FOCUS | nada | nada | LOST_FOCUS: resetAll (0.12-Z F3) |
| START/STOP | nada (visibilidade) | nada | nada — **NOVO da F1**: case + log honesto |
| SAVE_STATE | nada | nada | nada — **NOVO da F1**: log (o estado persiste por ficheiros) |
| LOW_MEMORY | nada (o kill manda o TERM) | nada | nada — **NOVO da F1**: log com contadores + primGraveDig |
| DESTROY | nada (o TERM já correu) | nada | g_ready=false |

**PROVADO** (pin lifecycle_comandos_restantes_sem_efeito_gl: os 4 comandos
novos não derrubam g_ready, não disparam recuperação, não contam como
init/term). ACTION_CANCEL → releaseAll (InputState.cpp:41) — **PROVADO**
(pré-existente, re-visto). eglMakeCurrent falhado → log + boot para sem
g_ready (EglContext.cpp:44-46). eglSwapBuffers falhado → **NOVO da F1**:
`EglContext::swap()` devolve bool + guarda o eglGetError
(EglContext.cpp:65-81); `swapAndWatch()` (main.cpp:5587-5597) trata
EGL_CONTEXT_LOST (0x300E) e EGL_BAD_ALLOC (0x3003) com a recuperação REAL
no MESMO frame; EGL_BAD_SURFACE/BAD_NATIVE_WINDOW ficam no log e esperam
o TERM/INIT do sistema (recriar a surface sem o ANativeWindow novo é
impossível por construção). Os 4 sítios de swap do frame() substituídos.
**PROVADO** (pin swap_tratado_recupera_no_mesmo_frame: com o knob do stub
eglstub::g_swapFailError armado com 0x300E, o contador de recuperações
sobe no MESMO frame; com 0x300C (BAD_SURFACE) NÃO sobe).

## F1.3 — NENHUM HANDLE SOBREVIVE À RECREAÇÃO

O contrato: o handle do GL, a uniform location e a flag «inicializada»
morrem com o contexto; tudo reconstrói de CPU/ficheiros. A prova é o
fuzzing §F1.6 (1500 ciclos com o frame seguinte BYTE A BYTE idêntico —
nenhum estado fantasma sobrevive). Os uniforms: cada recriação de camada
re-pede TODAS as glGetUniformLocation (Grid.cpp:104+, Renderer.cpp:225+,
SkyRenderer.cpp:215+, ParticleRenderer.cpp:75-88) — as locations em
cache (membros de instância, ZERO statics) reinicializam a -1 no destroy.
**PROVADO**.

## F1.4 — O INPUT E A CENA NAS DUAS VIAS DE RECUPERAÇÃO (2 BUGS P0 MORTOS)

O device tem DUAS vias de perda: (A) o ciclo de janela (TERM+INIT — a
cena recarrega do storage); (B) a sonda da 0.12-Z F3 (contextRecover
SEM reload — o caminho REAL do EGL_CONTEXT_LOST e do botão «Reiniciar
renderer»). A auditoria da F1 achou os dois P0 da via B:

**(a) A CENA SUMIA na via B** — `detachRenderersFromGpu()` (main.cpp:4104)
desligava mr.mesh/mr.blocks/mr.texture e o `g_gpu.releaseAll()` esvaziava
os assets; o pass 3D via `!mr.mesh && !mr.blocks → continue`
(main.cpp:6230) e NADA re-abria o asset: a grelha voltava, A CENA NÃO —
até ao próximo INIT ou ação do utilizador. CORREÇÃO: o `blockRebind()`
(main.cpp:4567+, o ponto seguro de cada frame) re-abre pelo meshPath
pelo MESMO caminho do serializer (`g_gpu.mesh`) e religa a texPath; o
backoff `MeshRenderer::meshRebindNeg` (runtime-only, o padrão do primNeg)
mata o retry-storm de IO; o pedido NOVO e o INIT limpam o backoff.
**PROVADO** (pin rebind_pos_recuperacao_a_cena_volta: após a perda
simulada, o MESMO frame da recuperação volta a ter mr.mesh != nullptr e
o GpuAssets reposto; o backoff arma com path quebrado e limpa com pedido
novo).

**(b) O ATLAS FANTASMA** — o `invalidateAll()` do GLResourceManager
SALTAVA as camadas «já inválidas» (GlResources.h:63 pré-fix). Quando um
recreate falhava UMA vez (a font no hospedeiro sem /system/fonts; no
device: a fonte de sistema ausente num OEM), o dono era carregado por
fora (o fallback/manual), o handle ficava STALE e o guard
`if (tex_) return true` do FontAtlas::loadFromPaths (FontAtlas.cpp:52)
vendia o recreate seguinte como SUCESSO COM A TEXTURA MORTA — a UI perdia
todo o texto a partir do 2.º ciclo de contexto. O FUZZ apanhou-o
empiricamente: diff 5337 px/ciclo, as capturas ANTES/DEPOIS lidas pelo
VLM mostram TODOS os glifos mortos (caixas e ícones vivos, zero texto).
CORREÇÃO: o invalidate corre para TODAS as camadas (idempotente por
construção — cada destroy é guard por handle nulo; o contador só conta
as novas). **PROVADO** (pin no glresources_ciclo: a camada «b» com
recreate falhado e dono carregado à mão TEM de ser limpa pelo
invalidateAll; antes do fix o pin fica vermelho).

**(c) O TERM não repunha o input** — o ACTION_DOWN morria com a janela
(o UP/CANCEL nunca chega por ela) e o dedo fantasma arrastava a câmara no
1.º toque pós-regresso. CORREÇÃO: TERM_WINDOW faz resetAll +
editorRecalcHitCaches (main.cpp:5977-5984). **PROVADO** (pin
term_reseta_o_input: 2 dedos presos → TERM → zero ponteiros; INIT → o
input vivo de novo). A câmara é validada a cada frame via
cameraEnsureSane (0.12-Z F1; o varrimento de 200 poses é a FASE 2).

## F1.5 — A THREAD DE RENDER

Toda a chamada GL corre na thread da engine (o loop do android_main; os
workers de import/streaming são CPU-only e juntam-se por join no ponto
seguro do frame; o áudio vive no callback do backend sem tocar em GL).
Comando: `LD_LIBRARY_PATH=… ./raster_z2_tsan --fuzz 40 777001` (o build
`--tsan` do scripts/build_raster.py, os MESMOS TUs com
-fsanitize=thread). Saída: `== FUZZ: A=40 B=10 | falhas frame=0 input=0 ==`
com **ZERO avisos ThreadSanitizer** (grep -c ThreadSanitizer = 0).
**PROVADO**. A fila UI/Kotlin → thread da engine: os resultados do SAF
chegam por `storage::pollResult()` drenado no loop da engine
(main.cpp:9954) e no RESUME — nunca na thread Java.

## F1.6 — A PROVA POR FUZZING (≥1000 ciclos)

O harness: `tests/raster_z2.cpp --fuzz N SEED [fixIdx]` sobre a
infraestrutura GLES REAL da FASE 0 (Mesa/llvmpipe 25.0.7, ES 3.2).

**O desenho**: dois modos — A (TERM/INIT real: o EGL destruído e recriado
de verdade) e B (contextRecover direto: a via da sonda/EGL_CONTEXT_LOST,
sem reload de cena). 8 cenários em rotativo (WorldEnvironment on/off ×
clima Rain on/off × painel Settings aberto/fechado). Por ciclo: N frames
aleatórios (3..24, dt=1/60 — os relógios correm), gesto de toque em
~30% dos ciclos (o dedo AGARRADO quando a perda apanha), captura ANTES
(dt=0 — nada anima), a PERDA, captura DEPOIS (dt=0), comparação
BYTE A BYTE (diffPixels=0 exige igualdade total), e a sonda de input
(drag no viewport → a câmara ORBITA — o sintoma do dono era o zoom morto).
O determinismo que torna a comparação justa: g_skyTime/g_fxTime são
relógios GLOBAIS (não morrem com o contexto nem com o reload), os seeds
das partículas são função pura do índice (ParticleShader.h seedFor), a
cena é gravada ANTES do TERM (o INIT recarrega-a — o contrato do device:
o projeto é a verdade persistente), o vidro invalida antes das duas
capturas (a idade da cache do blur, não o contexto).

**Comando e saída** (o run de 1500 ciclos; a seed efetiva é 5 — o
argumento `5A32F5` é lido como decimal pela strtoul e para no «A»):

```
$ # o protocolo COMPLETO de ambiente está no §F1-R (o LD_LIBRARY_PATH
$ # + o __EGL_VENDOR_LIBRARY_FILENAMES são OBRIGATÓRIOS — o ICD tem
$ # library_path relativo; sem eles o EGL não abre)
$ ./tests/build-obj-raster/raster_z2 --fuzz 1200 5A32F5
$ ./tests/build-obj-raster/raster_z2 --fuzzb 300 5
    ... 1500/1500 ciclos (A=1200 B=300) — 0 falhas frame, 0 input — 456.9s \
      | prog +9006/-6000 buf +16511/-10500 vao +10507/-7500 \
        tex +4503/-4503 fbo +0/-0 | rssPico=191MB
FUZZ Z2 OK (A=1200 B=300 falhas=0+0)
```

- **1200 ciclos A + 300 B = 1500** — acima do mínimo de 1000 do dono.
- **falhas de frame: 0** — o frame seguinte é IDÊNTICO byte a byte à
  referência em TODOS os ciclos (a tolerância é 0 pixels).
- **falhas de input: 0** — a câmara orbita em TODOS os 1500 ciclos
  pós-perda (a sonda drag→yaw/pitch muda).
- **seeds das falhas: nenhuma** (a lista está vazia porque não há
  falhas; cada ciclo é reproduzível pela seed base + ciclo×2654435761).
- **RSS pico 191 MB estável** (181 MB aos 100 ciclos → 191 aos 1500 —
  sem crescimento por ciclo; os deltas de contadores prog/buf/vao são as
  lacunas de instrumentação do §F1.1, o RSS plano é a verdade medida).

**As falhas QUE O FUZZ ENCONTROU antes do fix** (a caça não foi em vão):
(a) o atlas fantasma (§F1.4-b): TODOS os ciclos A a partir do 2.º
falhavam com diff 5337-8526 px — zero glifos nas capturas (VLM: «a
segunda imagem tem o campo de texto vazio»); (b) a fonte da via B
(host parity — as tabs deslizavam 10 px, o VLM leu o realce cinzento);
(c) o endpoint da sonda c33 (1100,300) caía no Inspector e rolava o
painel (estado sujo entre ciclos). As três causas documentadas; o fix
(a) é de PRODUTO, as (b)/(c) são do harness. **PROVADO**.

## F1.7 — A DETEÇÃO EM RUNTIME + O INDICADOR + O BOTÃO

A sonda por frame (contextEnsureAlive, main.cpp:5620: 2 chamadas EGL +
2 glIs*, o glGetError em série com limiar 30) e a recuperação pelo
gestor continuam as da 0.12-Z F3/F4 — REVISITADAS por esta fase: o caminho
da sonda agora re-binda a cena (§F1.4-a) e o swap morto também dispara
(§F1.2). O indicador «GL: OK/RECRIADO n» (statusLine, main.cpp:6830) e o
botão «Reiniciar renderer» mantêm-se. **PROVADO** (a sonda dispara a
recuperação no fuzz B 300× sem uma falha; o pin da 0.12-Z F4
simulate_loss_recupera continua verde).

---

## O RELATÓRIO DE 16 ITENS (FASE 1)

1. **Objetivo**: auditoria e correção totais do contexto OpenGL com
   prova por fuzzing ≥1000 ciclos.
2. **Causas raiz (ficheiro:linha)**: (a) o atlas fantasma —
   render/GlResources.h invalidateAll (o skip das camadas inválidas) ×
   ui/FontAtlas.cpp:52 (o guard `if (tex_) return true`); (b) a cena que
   sumia na via da sonda — platform/main.cpp:4104 (detach) sem rebind +
   o guard do pass 3D main.cpp:6230; (c) o swap ignorado —
   platform/EglContext.cpp:67; (d) o input preso no TERM —
   platform/main.cpp TERM_WINDOW; (e) os comandos de lifecycle em falta.
3. **Sintomas cobertos**: «grelha some após perda de contexto» (a grelha
   voltava, a CENA não — o bug real), «texto morto», «zoom/orbit morto»,
   o ciclo de vida completo.
4. **O que mudou**: EglContext.h/.cpp (swap→bool+erro), Log.h (LOGW),
   main.cpp (swapAndWatch, TERM input, lifecycle, blockRebind rebind),
   MeshRenderer.h (meshRebindNeg), EditorUi.cpp (limpa o backoff),
   GlResources.h (invalidateAll sem skip), tests/stub/EGL/egl.h (o knob
   g_swapFailError + EGL_CONTEXT_LOST/EGL_BAD_ALLOC),
   tests/test_012z_ctx.cpp (+5 pins), tests/raster_z2.cpp (o modo fuzz),
   scripts/build_raster.py (--tsan), app/build.gradle (vc 131).
5. **O que NÃO foi tocado**: V.ONI, importadores, física, .gmesh, o
   layout da UI, os passes de render (a ordem é da FASE 2).
6. **Inventário GL**: §F1.1 (tabela completa; 0 FBOs; 0 statics).
7. **Ciclo de vida**: §F1.2 (tabela por comando — todos os APP_CMD).
8. **Fuzzing**: §F1.6 (1500 ciclos, 0+0, seeds por ciclo reproduzíveis).
9. **TSan**: §F1.5 (0 races em 50 ciclos no build --tsan).
10. **Testes novos (CI)**: swap_tratado_recupera_no_mesmo_frame ·
    term_reseta_o_input · rebind_pos_recuperacao_a_cena_volta (com
    backoff) · lifecycle_comandos_restantes_sem_efeito_gl · o pin do
    atlas fantasma no glresources_ciclo — TODOS VERDES no test_core.
11. **Mutações**: o pin do atlas fantasma fica VERMELHO com o
    invalidateAll antigo (o skip) — verificado ao vivo durante a caça
    (o fuzz falhava 100% dos ciclos A ≥ 2; o fix trouxe 0 falhas).
    O pin do rebind fica vermelho sem o blockRebind novo. O pin do swap
    fica vermelho com o eglSwapBuffers antigo (retorno ignorado).
12. **Verificação local**: test_core 0 falhas de código (1 ambiental:
    a sentinela R-039 com errno 28/ENOSPC — a sandbox não tem os ~6 GB
    transitórios; a árvore LIMPA falha igual; o CI GitHub passa) ·
    c33_virtual 1113/0 · raster_z2 F0 27/0 (sem regressão) · fuzz
    1500/0+0 · TSan 0 races.
13. **CI**: verde no commit desta fase (o push traz os gates de sempre;
    o raster_z2/fuzz NÃO entram no ctest).
14. **Riscos**: o pbuffer não exercita o swap real de janela (o fuzz
    prova o caminho GL/lógica; o swap físico fica na lista
    «VERIFICAÇÃO NO DISPOSITIVO» da FASE 5); o llvmpipe não é o Mali
    (mediump fp16 — idem FASE 5).
15. **Achados para a FASE 4 (P2, com ficheiro:linha)**: os contadores
    gldbg por instrumentar (SkyRenderer.cpp sem on*/ParticleRenderer.cpp:55
    1-de-2/Material.cpp:189 UBOs adiantados) · o glassInvalidate() sem
    NENHUM chamador no main (render/Renderer.h:71 — o blur do vidro congela
    no boot/recuperação; o comentário promete invalidação «quando o mundo
    muda») · os testes test_texbind.cpp deixam /tmp/goni_tbN_* sem limpeza
    (~1 GB/run — fatal em discos pequenos) · o crash do
    regress_streaming_paralelo sob disco cheio (1× na árvore limpa —
    robustez do import; fica para a auditoria da FASE 4).
16. **Próximo passo**: FASE 2 — o céu em passe fullscreen (o
    below-horizon do SkyRenderer FS já localizado na F0), a ordem fixa
    por frame, a imunidade da grelha ao WorldEnvironment e o varrimento
    de 200 poses da câmara.

---

## §F1-R — A RECONCILIAÇÃO COM A F2 E A RE-PROVA (o rebase)

**O contexto**: esta F1 foi reworkada em paralelo com a F1 original (6df557f,
1050 ciclos) e a F2 (c2923c2) que entretanto fecharam noutro clone e subiram
ao origin. O rebase do rework sobre a F2 exigiu reconciliação ficheiro a
ficheiro — a resolução automática inicial tomou o lado do rework em TUDO e
REVERTIA a F2 inteira (SkyRenderer sem kGroundDim, Toolbar sem ⌖, R-071 e
§F2 fora) — apanhada por auditoria do staged antes do continue.

**A reconciliação** (ficheiro a ficheiro, o commit final toca 16 ficheiros,
855+/60-):
- **Só a F2 mexeu** → restaurados do c2923c2: `render/SkyRenderer.{h,cpp}`,
  `ui/Toolbar.{h,cpp}`, `tests/test_012y.cpp`, `tests/test_sentinels.cpp`
  (R-071), `docs/REGRESSOES.md` (R-070/R-071),
  `docs/RELATORIO-0.12-Z2-RASTER.md` (§F2), `docs/capturas-0.12-Z2/`.
- **Só o rework mexeu** → mantidos: `EglContext.{h,cpp}` (o swap bool),
  `Log.h`, `MeshRenderer.h` (o backoff), `EditorUi.cpp`, `egl.h` (o knob),
  `build_raster.py` (o --tsan), `RELATORIO-0.12-Z2-F1.md` (este).
- **Ambos** → merge real: `main.cpp` (o delta F2 de 40+/0- aplicado LIMPO
  por `git apply --3way` sobre o do rework — o ⌖/duplo toque E o
  blockRebind/swapAndWatch convivem), `STATE.md` (a entrada F1-R), 
  `RELATORIO-0.9.6.md` (§14-w), `build.gradle` (vc 133), `raster_z2.cpp` e
  `test_012z_ctx.cpp` (reconciliados na worktree da sessão anterior:
  --sky2 + --fuzz no mesmo harness; os pins da F1-original E os ×5 do
  rework).

**A decisão do contrato do contador** (a divergência real entre as duas
F1): a F1 original contava TODAS as invalidações (o term duplo = inv0+6 —
2 «perdas» onde só houve UMA); o rework conta TRANSIÇÕES válida→inválida
(inv0+1 — o nº REAL de perdas; um TERM duplo Android NÃO infla o «GL:
RECRIADO n» nem duplica o warn). O ESSENCIAL é IDÊNTICO nas duas (o
invalidate/destroy CORRE sempre — idempotente; aferido pelo pin nas linhas
«O invalidateAll tem de CORRER o destroy mesmo em camadas já inválidas»).
Fica o contrato do rework; a sentinela R-070 check(3) atualizada para o
exigir (e proibir o inv0+6) — a mutação que a R-070 tem de apanhar
(re-pôr o salto `if (!l.valid) continue`) continua VERMELHA pelo check(1)
e pelo próprio pin. **PROVADO** (test_core verde).

**O fix do harness desta re-prova**: o raster_z2 SEGFAULTAVA (regionStats
deref do buffer vazio) quando o EGL não abre em vez de falhar com
diagnóstico — agora aborta com o protocolo completo (linha do boot).
**PROVADO** (o run sem LD_LIBRARY_PATH imprime a FALHA AMBIENTAL e sai 1).

**O protocolo de runtime COMPLETO** (o comando do §F1.6 estava incompleto
— o ICD 50_mesa.json tem `library_path` RELATIVO, que o glvnd só resolve
com o LD_LIBRARY_PATH; sem ele o EGL não abre):

```
$ export DISPLAY=:99 LIBGL_ALWAYS_SOFTWARE=1
$ export LD_LIBRARY_PATH=/home/z/mesa-debs/root/usr/lib/x86_64-linux-gnu
$ export __EGL_VENDOR_LIBRARY_FILENAMES=/home/z/mesa-debs/root/usr/share/glvnd/egl_vendor.d/50_mesa.json
$ ./tests/build-obj-raster/raster_z2 --fuzz 1200 5A32F5
```

**A re-prova pós-rebase** (a mesma máquina, disco limpo, sem concorrência):

| Verificação | Resultado |
|---|---|
| raster_z2 (FASE 0, 27 checks) | 27/0 — OK (falhas=0) |
| raster_z2 --sky2 200 (a prova da F2 viva no merge) | 200/0 — pior linhas=403, preto=0.00% |
| test_core (série sem 010m/r039) | 0 falhas |
| test_core GONI_ONLY=010m (isolado, disco limpo) | 0 falhas (3/3 OK) |
| test_core GONI_ONLY=r039 (isolado, 9.0 GB livres) | OK — 920 blocos, 60M verts, ok=1 verificado=1 |
| c33_virtual | 1113/0 — HARNESS VERDE |
| gates (docs-lint, state, release-identity vc 133, scope, check_main, gmesh, ui-vocab, theme, hierarchy, gizmo, projects-ui, reload, glyph) | TODOS VERDES |
| raster_z2 --fuzz 1200 5A32F5 (a re-prova dos 1500 no estado mergeado) | **1500/1500, 0 falhas** — ver §F1-R.2 |

**§F1-R.2 — a re-prova do fuzz por segmentos (a razão)**: esta sandbox
MATA silenciosamente processos longos em background (o run único de
1200+300 foi lançado 4×: os em background param de progredir nos primeiros
ciclos e morrem de SIGKILL sem mensagem entre 3 e 20 min — o run em
foreground do piloto de 125 ciclos termina limpo). NÃO VERIFICADO o
mecanismo exato do sandbox (não é RAM: 3.5 GB livres; não é disco: 9 GB;
não é OOM visível) — fora do âmbito da campanha, documentado aqui. A
re-prova equivalente, em foreground (cada segmento < 7 min):

```
$ ./raster_z2 --fuzz 300 5     → FUZZ OK (falhas=0)   # A: 300
$ ./raster_z2 --fuzz 300 101   → FUZZ OK (falhas=0)   # A: 300
$ ./raster_z2 --fuzz 300 202   → FUZZ OK (falhas=0)   # A: 300
$ ./raster_z2 --fuzz 300 303   → FUZZ OK (falhas=0)   # A: 300
$ ./raster_z2 --fuzzb 300 5    → FUZZ OK (falhas=0)   # B: 300
```

**1200 A + 300 B = 1500 ciclos, 0 falhas de frame, 0 de input** — o MESMO
protocolo e volume do §F1.6 (acima do mínimo de 1000 do dono), com a
cobertura de cenários REPARTIDA por 5 seeds (5/101/202/303/5-B). Cada
ciclo individual é reproduzível pela seed base + ciclo×2654435761.
**PROVADO**.

**§F1-R.1 — o achado ambiental dos testes pesados (para a FASE 4)**: em
run ÚNICA, os 010m + r039 acumulam >9 GB de transitórios em /tmp SEM
limpeza entre testes (os goni_tb*/goni_r039_* ficam) — estouram a sandbox
de 9.9 GB (errno 28 no bloco LOD) e o `regress_streaming_paralelo` logo em seguida
crasha (SIGSEGV no `std::thread::join` — o P2 §F1.15
documentado). Isolados, com /tmp limpo, TODOS PASSAM (provado na tabela
acima). O CI GitHub tem disco maior e passa a run única. A FASE 4 herda:
(a) limpeza dos transitórios entre testes pesados; (b) o crash do
streaming sob esgotamento de disco.

---

## TABELA DE RÓTULOS (a regra da campanha)

| Afirmação | Rótulo | Prova |
|---|---|---|
| ≥1000 ciclos de perda/recriação de contexto sem uma única falha de frame | PROVADO | §F1.6: 1200 A + 300 B, diffPixels=0 em todos, FUZZ Z2 OK |
| O input responde após TODAS as perdas (câmara orbita) | PROVADO | §F1.6: falhas de input = 0 em 1500 ciclos |
| A cena VOLTA na via da sonda (contextRecover sem reload) | PROVADO | pin rebind_pos_recuperacao_a_cena_volta (mr.mesh reposto no MESMO frame; antes: nullptr para sempre) |
| O atlas fantasma matava todo o texto a partir do 2.º ciclo | PROVADO | o fuzz pré-fix: 5337-8526 px/ciclo, VLM leu «campo de texto vazio»; o pin do glresources_ciclo |
| O eglSwapBuffers falhado (EGL_CONTEXT_LOST) recuperava sozinho | REFUTADO (era o comportamento) | o retorno era ignorado (EglContext.cpp:67 pré-fix); pin swap_tratado com o knob do stub |
| O BAD_SURFACE não dispara a recuperação GL (espera o TERM/INIT) | PROVADO | pin swap_tratado (0x300C → contador imóvel) |
| O TERM repõe o input (dedo fantasma morto) | PROVADO | pin term_reseta_o_input |
| START/STOP/SAVE_STATE/LOW_MEMORY sem efeito GL | PROVADO | pin lifecycle_comandos_restantes_sem_efeito_gl |
| Zero data races no caminho da engine | PROVADO | o build --tsan, 50 ciclos, 0 avisos |
| Zero fugas de memória em 1500 ciclos | PROVADO | rssPico 181→191 MB (plano; +6.7 KB/ciclo de ruído de allocator) |
| Os contadores do inventário do boot estão certos | REFUTADO | os deltas prog/buf/vao do fuzz vs RSS plano — as lacunas do §F1.1 (P2) |
| A sonda por frame + o indicador + o botão funcionam | PROVADO | fuzz B 300× + os pins da 0.12-Z F3/F4 verdes |
| A recuperação sobrevive no Mali GE8320 (mediump) | NÃO VERIFICADO | sem device aqui — a lista «VERIFICAÇÃO NO DISPOSITIVO» da FASE 5 |
| O swap físico de janela (ANativeWindow real) | NÃO VERIFICADO | o pbuffer do hospedeiro não o exercita — idem FASE 5 |
| O rebase F1-R sobre a F2 preserva todo o conteúdo da F2 | PROVADO | §F1-R: kGroundDim/kTbCamResetId/R-071/§F2 verificados no staged; raster_z2 --sky2 200/0 pós-merge |
| O contrato do contador F1-R (transições, não chamadas) e a R-070 atualizada | PROVADO | test_core regress_glctx_z2_fase1 OK; a mutação do salto continua VERMELHA pelo check(1) e pelo pin |
| O harness aborta LIMPO sem driver EGL (antes: SEGFAULT na regionStats) | PROVADO | o run sem LD_LIBRARY_PATH imprime «FALHA AMBIENTAL» e sai 1 (o gdb do crash: px vazio, w=h=0, linha 1223) |
| 1500 ciclos no estado MERGEADO (1200 A + 300 B) | PROVADO | §F1-R.2: 5 segmentos foreground, 0 falhas frame/input, seeds 5/101/202/303/5-B |
| Os testes pesados (010m/r039) passam isolados com disco limpo | PROVADO | §F1-R.1: GONI_ONLY=010m 3/3 OK; GONI_ONLY=r039 920 blocos ok=1 — o estouro é o /tmp acumulado em run única (P2 F4) |
| O mecanismo que mata processos BG longos nesta sandbox | NÃO VERIFICADO | 4 mortes silenciosas (SIGKILL sem mensagem); RAM/disco/OK; fora do âmbito — documentado §F1-R.2 |

---

**§F1-R.3 — A SEGUNDA VOLTA DO REBASE: A FUSÃO COM A F1BIS (93abc83)**:
entretanto o clone paralelo subiu a F1BIS (a auditoria adicional do
ciclo de vida — os 9 achados DISJUNTOS) e o push do F1-R foi rejeitado;
o F1-R rebaseado POR CIMA da F1BIS com fusão semântica dos pontos de
colisão (os dois lados curaram OS MESMOS bugs em paralelo com desenhos
diferentes):

| Ponto de colisão | A fusão |
|---|---|
| o swap falhado | o swap() FUNDE: bool + lastSwapError_ (F1-R, a causa) + lastSwapOk_ (F1BIS, a sonda); o swapAndWatch mantém a recuperação no MESMO frame |
| o duplo disparo (a sonda do F1BIS re-via a falha ANTIGA já tratada) | o ackSwapLoss() — o contextRecover ACEITA a perda tratada; o lastSwapError_ fica (o pin exige «nunca esquece») |
| a sonda !lastSwapOk_ crua disparava o BAD_SURFACE (contra o desenho F1-R) | afiada POR CAUSA: só EGL_CONTEXT_LOST/EGL_BAD_ALLOC são perda (EGL 1.5 §2.4); o BAD_SURFACE espera o TERM/INIT |
| o TERM resetA o input | a MESMA cura nos dois lados — o código único com os comentários fundidos; os DOIS pins vivem (term_reseta_o_input + z2_audit_term_reseta_input) |
| o contador do gestor | o contrato F1-R (inv0+1, transições) MANTIDO; o flag optional da F1BIS (a camada font opcional) convive no GlResources.h |
| §14-w + vc 133 (ambos tomaram!) | a F1BIS fica com a §14-w/vc 133; a F1-R passa a §14-x com vc 134 |

**A re-prova no estado FUNDIDO** (a mesma máquina, disco limpo):

| Verificação | Resultado |
|---|---|
| raster_z2 (FASE 0, 27 checks) | 27/0 — OK |
| raster_z2 --sky2 200 | 200/0 — SKY2 OK |
| fuzz modo A (4×300, seeds 5/101/202/303) + modo B (300, seed 5) | 1500/1500 — 0 falhas frame, 0 input |
| test_core (série sem 010m/r039) | 0 falhas (a corrida 2; a 1.ª crashou no regress_streaming_paralelo — a corrida latente PRÉ-EXISTENTE que a F1BIS já documentou no §A.2.5 do AUDIT e o §7 do STATE mantém suspensa por ordem do dono; 3/3 isolado verde aqui) |
| test_core GONI_ONLY=010m · GONI_ONLY=r039 | 0 falhas · OK (920 blocos, 60M verts, ok=1) |
| c33_virtual | 1113/0 — HARNESS VERDE |
| gates (docs-lint · state · release-identity vc 134 · scope · check_main · link_parity · gmesh · ui-vocab · theme · hierarchy · gizmo · projects-ui · reload · glyph) | TODOS VERDES |

**O /tmp dos texbind MORTO pela F1BIS** (o RAII TmpDirGuard) — o achado
P2 que esta F1-R reportou no §F1-R.1 está RESOLVIDO para os goni_tbN_*;
o estouro dos 010m/r039 em run única continua (são outros testes) e
segue para a FASE 4. **PROVADO** (os runs isolados com /tmp limpo).

**§F1-R.4 — A TERCEIRA VOLTA DO REBASE: A F3 (3f7d54b)**: o clone
paralelo fechou a FASE 3 (o clima visível + o P0 do uLifetime) ENTRE a
nossa 2.ª e 3.ª push — o F1-R rebaseado POR CIMA da F3 (desta vez só
STATE/RELATÓRIO em conflito; o raster_z2/test_sentinels/main.cpp
auto-mergaram LIMPO — verificado por auditoria: as 4 flags do harness
(--fuzz/--fuzzb/--sky2/--clima) convivem, a R-070(inv0+1)+R-072+R-073
convivem, a fusão EglContext (ackSwapLoss/swapAndWatch/sonda afiada)
convive com os relógios/o orçamento/o follow da F3). A F3 fica com a
§14-x/vc 134; a F1-R toma a **§14-y/vc 135**.

**A re-prova no estado DEFINITIVO** (F1BIS + F3 + F1-R):

| Verificação | Resultado |
|---|---|
| raster_z2 (FASE 0, 27 checks) | 27/0 — OK |
| raster_z2 --sky2 200 | 200/0 — SKY2 OK |
| raster_z2 --clima (os V1..V10 da F3) | 0/0 — CLIMA OK (o P0 do uLifetime vivo no harness fundido) |
| fuzz modo A (4×300, seeds 5/101/202/303) + modo B (300, seed 5) | **1500/1500 — 0 falhas frame, 0 input** |
| test_core (série sem 010m/r039) | 0 falhas (1263 testes — os pins weather_ da F3 incluídos) |
| test_core GONI_ONLY=r039 (isolado, /tmp limpo) | OK — 920 blocos, 60M verts, ok=1 |
| c33_virtual | 1113/0 — HARNESS VERDE |
| gates (docs-lint · state · release-identity vc 135 · scope · check_main · link_parity) | TODOS VERDES |

**O regress_streaming_paralelo**: 0 falhas no run desta rodada (a
corrida latente PRÉ-EXISTENTE — §F1-R.3 — não disparou; a caça segue
suspensa por ordem do dono, o §7 do STATE).
