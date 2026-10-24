# RELATÓRIO 0.12-Z2 — F1-BIS: A AUDITORIA ADICIONAL DO CICLO DE VIDA

Complemento do RELATORIO-0.12-Z2-RASTER.md §F1 (a F1 da linha principal)
e do §F2 (o tom de chão, o varrimento, o «Repor câmara»). Este relatório
fecha os achados da auditoria exaustiva do prompt (FASE 1/FASE 4) que as
fases anteriores NÃO cobriam — cada linha com o rótulo do regime de
evidência. Nenhuma palavra proibida entra aqui.

## §A.1 Os 6 achados — a tabela fonte (com ficheiro:linha da árvore atual)

| # | Achado | Classe | Correção |
|---|---|---|---|
| 1 | `eglInitialize(display_, nullptr, nullptr)` (platform/EglContext.cpp) devolve EGL_FALSE 0x3001 no glvnd — o boot da engine MORRIA no hospedeiro («[boot 1/6] contentRect FALHOU (EGL init)» no log do raster harness) | P1 | major/minor locais (&eglVerMajor/&eglVerMinor) — o padrão idiomático no Android; nenhum device muda |
| 2 | o retorno do `eglSwapBuffers` era IGNORADO (EglContext.cpp swap) — o EGL_FALSE (EGL_BAD_NATIVE_WINDOW/EGL_CONTEXT_LOST) não alimentava a sonda | P1 | `lastSwapOk_` no EglContext (o init repõe true) + `contextLostNow()` consome |
| 3 | o `EglContext::init` sem auto-limpeza — a falha a MEIO deixava contexto/surface órfãos (o init seguinte criava um segundo contexto sem dono) | P1 | as falhas de surface/makeCurrent destroem o que já nasceu |
| 4 | o `APP_CMD_TERM_WINDOW` NÃO resetava o estado de interação (o PAUSE e o LOST_FOCUS repunham — o term FALTAVA; o Android pode destruir a janela a meio do gesto sem ACTION_CANCEL garantido) | P1 | `editorRecalcHitCaches()` + `g_input.resetAll()` no TERM (main.cpp) |
| 5 | a camada `font-mono` tornava a recuperação INFINITA — o `recreateAll` abortava na camada 2 numa device sem nenhuma mono da lista (o boot aceitava a falta como "ACEITÁVEL"; a recuperação não) | P1 | o flag `optional` no GLResourceManager (a falha opcional fica logada e o ciclo SEGUE) + o registo do mono com `true` |
| 6 | o `Renderer::init` NÃO começava pelo destroy (a única exceção ao padrão da casa — um 2.º init no mesmo contexto vazava prog/VAO/VBO/texturas) e o `shutdown` não repunha os uniform locations | P1 | `shutdown()` na 1.ª linha do init + TODOS os `loc*` a -1 no shutdown |
| 7 | a recuperação de contexto (`contextRecover`) não chamava `primMeshesDestroyAll()` (o term chamava) — os VAO/VBO/EBO dos prims ficavam órfãos até à cova correr num frame posterior | P2 | a chamada no contextRecover (a paridade com o term) |
| 8 | os testes texbind deixavam ~2,9 GB em `/tmp/goni_tbN_<pid>` por run (7 dirs nunca apagados) — o r039 batia em errno 28 na máquina de discos pequenos | P2 (infra de teste) | o RAII `TmpDirGuard` nos 7 sítios (o ASSERT returna cedo — o guard corre em todos os caminhos) |
| 9 | o default do NaN em `dayCycleSpeed` era 1.0 (Environment::clampFields) — um .goni corrompido LIGAVA o auto-ciclo (comportamento novo por NaN) | P2 | o default 0.0 (o do campo) |

## §A.2 As provas

1. **PROVADO** — o probe isolado com os headers REAIS de Khronos
   (tests/stub-real) + glvnd 1.7.0 + libEGL_mesa 25.0.7:
   `eglInitialize(d, nullptr, nullptr)` → EGL_FALSE, `eglGetError()`
   0x3001; `eglInitialize(d, &maj, &mn)` → EGL_TRUE 1.5. O log do
   harness antes do fix: `[boot 1/6] contentRect FALHOU (EGL init)` +
   o ciclo TERM/INIT da recuperação a martelar. Depois do fix: o
   raster_z2 corre a bateria inteira (27/0).
2. **PROVADO** — o 2.º desvio do shim (opt-in `RASTER_SURFACELESS=1`,
   documentado no próprio tests/stub-real/EGL/egl.h): sem X11, o
   `eglGetDisplay(EGL_DEFAULT_DISPLAY)` ligava-se à plataforma X11 da
   build do Mesa e o `xcb_connect` falhava (EGL_LOG_LEVEL=debug →
   «EGL user error 0x3003 in eglInitialize: xcb_connect failed»). O
   remapeia para `EGL_MESA_platform_surfaceless` REAL do mesmo driver +
   o bit WINDOW→PBUFFER no eglChooseConfig. O raster_z2 e o raster_z2
   --fuzz correm VERDES sem X (1205 checks no --fuzz 150 ciclos).
3. **PROVADO** — o pin `z2_audit_term_reseta_input`
   (tests/test_012z_ctx.cpp): o gesto injetado + o TERM real → 0
   ponteiros vivos. A mutação (o reset removido do term) deixa a
   sentinela R-072 VERMELHA («R-072: o TERM deixou de resetar o input»)
   — o vermelho→verde PROVADO na sessão de desenvolvimento.
4. **PROVADO** — a suíte: test_core 0 falhas (o pin e a sentinela novos
   entram) · c33 1113/0 · raster_z2 27/0 · raster_z2 --fuzz 150/0.
5. **PROVADO** (pré-existente, NÃO é desta campanha) — o
   `regress_streaming_paralelo` crasha INTERMITENTEMENTE na suíte
   completa (sinal 11 no std::thread::join durante o convertGltfToV3) e
   passa 3/3 isolado: a corrida latente do corte paralelo que o §7 do
   STATE.md mantém suspensa POR ORDEM do dono. Prova da pré-existência:
   a árvore SEM qualquer alteração desta sessão crasha da mesma forma.
6. **NÃO VERIFICADO no dispositivo** — o comportamento do Mali GE8320
   com os 6 fixes (o hospedeiro prova a LÓGICA, não o driver do OEM).
   Os passos para o dono vivem no §F5.3 (o relatório final da campanha).

## §A.3 O inventário do censo GL (a confirmação desta sessão)

PROVADO por censo de `\bgl[A-Z]\w+\(` (fora vendor): 0 FBOs, 0
renderbuffers, 0 queries, 0 DSA; os uniform locations vivem TODOS como
membros `loc*` (0 caches globais); as threads (áudio, import, thumbs,
streaming) não tocam GL (o glReadPixels do thumb corre na thread GL
antes do spawn). A tabela objeto→nasce→morre→recria completa vive no
RELATORIO-0.12-Z2-GLCTX.md §F1.1 (o irmão desta linha de trabalho; os
achados são DISJUNTOS — esta fase não repete nenhum).

## §A.4 Rótulos

| Afirmação | Rótulo | Prova |
|---|---|---|
| O eglInitialize(nullptr) falha no glvnd | PROVADO | probe isolado (§A.2.1) |
| O term deixava o gesto sobreviver | PROVADO | o pin z2_audit_term_reseta_input + a mutação da R-072 |
| A device sem mono não recuperava nunca | PROVADO | a leitura do recreateAll (o stop na camada 2) + o log «a recuperação #1 FALHOU (font)» do hospedeiro sem fontes de sistema |
| O init parcial deixava contexto órfão | PROVADO | a leitura dos returns do EglContext::init |
| Os locs do Renderer sobreviviam ao shutdown | PROVADO | a leitura (o shutdown não tocava nos membros) |
| A corrida do streaming_paralelo é pré-existente | PROVADO | a árvore limpa crasha igual; 3/3 isolado verde |
| O Mali GE8320 comporta-se igual | NÃO VERIFICADO | os passos no dispositivo (o §F5.3) |
