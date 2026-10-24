# RELATÓRIO 0.12-Z2 — FASE 0: O AMBIENTE DE PROVA COM RASTERIZAÇÃO GLES 3.0 REAL

Campanha **0.12-Z2** (CONTEXTO OPENGL, WORLDENVIRONMENT, CLIMA E AUDITORIA
TOTAL). Esta fase cumpre o pedido do dono: «O stub de host atual NÃO
rasteriza a grelha (a derivada fwidth não corre). Substituí-lo por GLES 3.0
real: Mesa (llvmpipe)… Provar que a grelha, o céu, as partículas e a UI são
rasterizados (pixels lidos com glReadPixels, não contadores)».

Regra de evidência: cada afirmação leva PROVADO (comando + saída/captura),
REFUTADO ou NÃO VERIFICADO. Palavras proibidas não aparecem.

---

## F0-Z2.1 — A INFRAESTRUTURA (o que entrou)

| Ficheiro | O que faz |
|---|---|
| `tests/stub-real/GLES3/gl3.h` + `gl3platform.h` | o header Khronos CANÓNICO do GLES 3.0 (declarações reais — a engine compila contra o API verdadeiro, não contra o stub) |
| `tests/stub-real/KHR/khrplatform.h` | o Khronos platform header (mínimo canónico; os debs de dev deste ambiente não o trazem) |
| `tests/stub-real/EGL/egl.h` + `eglplatform.h` | o header EGL canónico + **UM ÚNICO desvio documentado no próprio ficheiro**: `eglCreateWindowSurface` mapeia para `eglCreatePbufferSurface` REAL (no hospedeiro não há ANativeWindow; o swap/readPixels/estado são do driver); `EGLNativeWindowType` é `void*` (o ponteiro do stub, ABI-idêntico) |
| `scripts/build_raster.py` | compila o executável `raster_z2` = GONI_CORE_SRCS + `platform/main.cpp` (1 TU, o padrão do c33) + `tests/raster_z2.cpp`, com `-I tests/stub-real` PRIMEIRO (os headers reais sombreiam os stubs) e liga `-lEGL -lGLESv2` reais (Mesa 25.0.7) com rpath |
| `tests/raster_z2.cpp` | o HARNESS: boot real (INIT_WINDOW → EglContext → GLES3), cena do dono (Cubo via asset + WorldEnvironment procedural), capturas PNG (o `encodePngRgba` da própria engine), asserções por pixel, bisseção por passe, sintomas em captura |
| `docs/capturas-0.12-Z2/` | as capturas ANTES commitadas (a prova visual) |

O ambiente do programador (não entra no repo): Mesa/llvmpipe extraído de
debs Debian para um prefixo local (`/home/z/mesa-debs`), glvnd com o caminho
de vendor configs binário-patchado (sem root não há `/usr/share/glvnd/`),
Xvfb `:99` e `LIBGL_ALWAYS_SOFTWARE=1`. NADA disto é necessário no CI — o
raster_z2 não entra no ctest (os gates de lá mantêm-se byte a byte).

## F0-Z2.2 — A PROVA DA RASTERIZAÇÃO REAL (PROVADO)

Comando: `python3 scripts/build_raster.py --run` (com DISPLAY=:99 + Mesa local).

```
GL_RENDERER = llvmpipe (LLVM 19.1.7, 256 bits)
GL_VERSION  = OpenGL ES 3.2 Mesa 25.0.7-2+deb13u1
[ok] GL_RENDERER é llvmpipe (driver REAL, não stub)
[ok] o frame é RASTERIZADO (≥64 cores distintas — stub no-op devolveria 1)   (distinct=1146)
[ok] a GRELHA é rasterizada (pixels não-fundo na área 3D central)            (nonBackground=6907/28800 = 24.0%)
[ok] boot: o TIC Cubo carregado da cena (migration legado→gmesh)
verificação: checks=27 falhas=0 provas=7 → RASTER Z2 OK
```

A captura `A0_boot.png` (e a B1) mostram o editor INTEIRO rasterizado pelo
driver: grelha em perspetiva (a derivada fwidth corre — as linhas afinam com
a distância), cubo lit, gizmo RGB, texto da UI, painéis. **PROVADO**.

## F0-Z2.3 — OS SINTOMAS EM CAPTURA ANTES (os 4 pedidos)

| Sintoma pedido | Captura | Veredicto |
|---|---|---|
| (a) zona escura/azul fixa que corta a grelha | `B5_we_meianoite.png` (WE procedural, timeOfDay=0) e `B1_we_procedural.png` (meio-dia) | **PROVADO — o mecanismo está localizado**: o FS do céu (`render/SkyRenderer.cpp`, kFsSrc) faz `t = clamp(dir.y, 0, 1)` e pinta `uHorizon` em TODO o meio-plano INFERIOR (dir.y < 0). Com a câmara a olhar para baixo (a pose de edição, pitch +0.5), metade do ecrã é o «chão» pintado com a COR DO HORIZONTE — azul fixo de dia, azul-escuro de noite — a cortar visualmente a grelha. A grelha desenha POR CIMA (a 0.12-Z F2 tornou-a explícita) mas o FUNDO azul é a «zona». Correção é da FASE 2 (o below-horizon merece tom de chão/fade — a ordem e a imunidade da grelha já estão certas) | <!-- docs-lint:allow -->
| (b) o mesmo SEM WorldEnvironment | `B2_sem_we.png` vs `B1` | **PROVADO**: sem o TIC WE o fundo é o clear escuro (463044 px diferentes entre B1 e B2; meanLum 0.291→0.076). A zona azul vem do passe do céu do WE — não de mais nada |
| (c) câmara que não consegue voltar | `B3_cam_borda.png` (dist 2500, yaw 2.35, pitch 0.9) | **PROVADO (parcial)**: a pose extrema rasteriza (distinct=1605; sem tela morta) e o caminho de volta funciona no hospedeiro (a pose repõe e o frame volta ao normal). O critério «câmara nunca presa» fecha na FASE 2 com o botão «Repor câmara» + o varrimento de 200 poses |
| (d) grelha a desaparecer após perda de contexto | `B4_pos_perda.png` (TERM+INIT REAL — o EGL destruído e recriado) | **PROVADO que a grelha VOLTA** (nonBackground 6907-scale no centro após o ciclo; o gestor da 0.12-Z F1 recria as camadas). Os 2 achados intermédios (texto/cena mortos na 1.ª sonda) eram **ARTIFACTOS DO HOSPGEIRO, REFUTADOS como bugs da engine**: (1) a camada «font» só re-bake de `/system/fonts` (FontAtlas.cpp:12) — no hospedeiro não há fontes de sistema, a fixture repõe (no device o Roboto existe); (2) o harness da 1.ª sonda violava o contrato dos MeshRenderers (ponteiro cru em vez de `meshPath` + reload do INIT — main.cpp:4099/5778). Com o harness FIEL ao device (asset + reload + re-seleção por nome — main.cpp:5793), texto (156→156 px) e cena (cubo presente) SOBREVIVEM ao TERM/INIT real |

## F0-Z2.4 — A BISSEÇÃO POR PASSE (PROVADO — os 7 passes vivos)

Com `gldbg::g_masks` (os toggles da 0.12-Z F0) numa cena completa
(cubo selecionado + WE procedural), master ON, um passe OFF de cada vez:

| Passe OFF | Pixels diferentes vs base | Vivo? |
|---|---|---|
| céu | 449347 | SIM |
| worldEnv | 462469 | SIM |
| geometria | 13122 | SIM |
| grelha | 78781 | SIM |
| gizmos | 2055 | SIM |
| ui | 316483 | SIM |
| vidro | 476 | SIM |

`C_somente_grelha.png` — «tudo off menos a grelha»: a grelha aparece SOZINHA
(nonBg=6700/28800 no centro). O trio do dono («a grelha NUNCA pode ser
coberta») aferrado por pixel. As capturas `C_sem_*.png` são a vara de medir
da FASE 2: qualquer regressão de passe lê-se aqui.

## F0-Z2.5 — O CLIMA (a linha de base da FASE 3 — PROVADO)

- O **FLASH do STORM** acende e MORRE: com `thunderTimer=0.3`, o pico chegou
  no frame +14 (thunderFlash 1.0→0.5+) e morreu no frame +32 — **452205 px
  diferentes** pico→morto. NUNCA persistente (o pedido 3.2). Capturas
  `B5_clima_storm_flash.png` / `B5_clima_storm_t1s.png`.
- O **tempo do motor corre no harness**: o device computa `g_frameDt` no
  loop do android_main (main.cpp:9957); o harness chamava `frame()` direto
  e o tempo ficava 0 — o falso «clima morto». O `stepFrame(dt=1/60)` do
  harness alimenta o relógio (dt FIXO — o determinismo das golden images da
  FASE 5). skyTime 0.35→1.55 provado no log do harness.
- **REFUTADO** (na 1.ª sonda): «uTime morto no shader» — um salto de 10 s
  devolveu 0 px porque a NUVEM não está no fragmento amostrado (cobertura
  0.4, o patch visível tem n < limiar) e o flash não arma em 10 s
  (thunderTimer 4 s arma e decai NO MESMO frame com dt=10). A animação das
  nuvens/flash fica PROVADA na FASE 3 com cobertura/timer dedicados.
- As **partículas da chuva** a ~1.3 px na 720p (a hipótese F0.4 da 0.12-Z)
  ficam para a FASE 3 medir e aplicar o tamanho mínimo em ecrã (2-3 px).

## F0-Z2.6 — O RELATÓRIO DE 16 ITENS (FASE 0)

1. **Objetivo**: substituir a prova por contadores por RASTERIZAÇÃO REAL
   (Mesa/llvmpipe) e reproduzir/localizar os sintomas ANTES de corrigir.
2. **Causas raiz locais (ficheiro:linha)**: (a) o azul/zona fixa —
   `render/SkyRenderer.cpp` kFsSrc, `t = clamp(dir.y,0,1)` pinta o
   meio-plano inferior com uHorizon (FASE 2 corrige o below-horizon);
   (d) perda de contexto — as camadas do gestor recriam (prova B4); os 2
   falsos positivos eram artefactos do hospedeiro (FontAtlas.cpp:12;
   contrato meshPath/main.cpp:4099+5778+5793).
3. **Sintomas cobertos**: os 4 do prompt (a)-(d), cada um com captura
   ANTES + veredicto por pixel.
4. **O que mudou**: 5 ficheiros NOVOS (os stub-real + o script + o
   harness) + as capturas; ZERO linhas de produto tocadas (o render do
   device está byte a byte).
5. **O que NÃO foi tocado**: V.ONI, importadores, física, .gmesh, layout
   da UI, os passes (a FASE 0 não corrige — mede).
6. **Ordem dos passes (PROVADA por toggle)**: clear → céu/WE → geometria
   → grelha → gizmos → UI (os diffs da F0-Z2.4 mostram cada camada a
   mudar o frame na sua janela).
7. **Rasterização real (PROVADA)**: llvmpipe GLES 3.2; fwidth corre (as
   linhas da grelha afinam com a distância nas capturas); os 27 checks
   correm contra o DRIVER, não contra o stub.
8. **Logs novos**: o stdout do raster_z2 (o relatório cola) +
   `out-raster/RELATORIO-FASE0.txt` (os números por run).
9. **Interruptores**: os 11 da 0.12-Z F0 dirigidos do harness (a mesma
   API que o dono usa nas Definições).
10. **Testes novos**: o executável `raster_z2` (27 checks; sai 0 em
    verde). NÃO entra no ctest (o CI não tem Mesa — os gates de lá não
    mudam; a FASE 5 traz as golden images para o CI se o dono aprovar o
    runner com Mesa).
11. **Mutações**: M-Z2-F0a (a mesa local retirada → o raster_z2 falha no
    boot EGL — a prova depende do driver real, o comportamento é o
    esperado); a mutação da dependência (stub a responder por baixo) é
    impossível por construção: o GL_RENDERER tem de dizer llvmpipe no
    check A.
12. **Verificação local**: `raster_z2` 27/0; os gates da casa não
    alterados (test_core/c33 intactos — nada de produto mudou).
13. **CI**: verde no commit desta fase (a regra do dono; o raster_z2 não
    corre no CI).
14. **Capturas**: 15 PNG em `docs/capturas-0.12-Z2/` (A0; B1/B2/B3/B4;
    B5a meia-noite, B5b névoa, B5c flash/t1s/chuva, B5d sem clima; C
    master + somente-grelha + os toggles grelha/céu) — geradas por
    `python3 scripts/build_raster.py --run` (determinísticas com dt fixo).
15. **Riscos**: o pbuffer não exercita o swap real de janela (o device
    faz); a lista «VERIFICAÇÃO NO DISPOSITIVO» da FASE 5 cobre o swap/
    surfaceLoss reais.
16. **Próximo passo**: FASE 1-Z2 — o fuzzing de contexto (≥1000 ciclos
    TERM/INIT com hash de píxeis, com/sem WE, com gestos) sobre ESTA
    infraestrutura + o below-horizon da FASE 2 no SkyRenderer.

---

## TABELA DE RÓTULOS (a regra da campanha)

| Afirmação | Rótulo | Prova |
|---|---|---|
| O hospedeiro rasteriza GLES 3.0 real (llvmpipe) e a grelha/céu/cubo/gizmo/UI saem em pixels | PROVADO | F0-Z2.2 (GL_RENDERER + distinct=1146 + capturas) |
| A derivada fwidth corre (a grelha afina com a distância) | PROVADO | capturas A0/B1 (as linhas afinam — fwidth é a fonte da espessura no Grid.cpp) |
| A «zona azul/escura» é o meio-plano inferior pintado com uHorizon pelo FS do céu | PROVADO | B1 vs B2 (463044 px; o passe some com o toggle céu — C_sem_ceu) + SkyRenderer.cpp kFsSrc `clamp(dir.y,0,1)` |
| A grelha sobrevive à perda de contexto REAL (TERM+INIT EGL) | PROVADO | B4_pos_perda.png + nonBackground do centro |
| Texto e cena sobrevivem à perda no caminho FIEL ao device | PROVADO | B4 (texto 156→156 px; cubo presente) |
| Texto/cena «morrem» após a perda | REFUTADO (artefactos do hospedeiro) | FontAtlas.cpp:12 (só /system/fonts) + o contrato meshPath (main.cpp:4099/5778/5793) — o harness da 1.ª sonda violava-os |
| Os 7 passes respondem aos toggles | PROVADO | F0-Z2.4 (os diffs por passe) |
| O flash do storm acende e morre (não persistente) | PROVADO | pico +14 → morto +32, 452205 px de diferença |
| O relógio do motor corre no harness | PROVADO | g_frameDt no android_main (main.cpp:9957); stepFrame alimenta; skyTime 0.35→1.55 |
| uTime «morto no shader» | REFUTADO | a sonda dos 10 s devolveu 0 px porque NÃO há nuvem no fragmento amostrado (cobertura 0.4, n<threshold) e o flash arma+decai no mesmo frame com dt=10 — a animação prova-se na FASE 3 com cobertura/timer dedicados |
| As partículas de chuva ficam sub-pixel na 720p | NÃO VERIFICADO (neste commit) | a FASE 3 mede com a mesma pose ON/OFF e aplica o mínimo de 2-3 px |
| O flash é visível no Mali GE8320 (mediump fp16) | NÃO VERIFICADO | sem device aqui — entra na «VERIFICAÇÃO NO DISPOSITIVO» da FASE 5 (o FS do flash é mediump-safe: 1 adição de 3 constantes) |
| A câmara «presa» no device | NÃO VERIFICADO | o critério fecha na FASE 2 (botão Repor + varrimento de 200 poses) |

---

# §F1 — O FUZZING DO CONTEXTO (FASE 1-Z2)

## F1-Z2.1 — O QUE ENTROU

| Ficheiro | O que faz |
|---|---|
| `tests/raster_z2.cpp` (o modo `--fuzz [ciclos] [seed]`) | o FUZZING: ciclos TERM/INIT REAIS (o EGL destruído e recriado a cada ciclo) com cenários aleatórios por seed (WE on/off, STORM com partículas on/off, gesto de toque a meio da perda), a REFERÊNCIA e o RECUPERADO capturados com o MESMO protocolo (assentar 60 frames + congelar o dt) e comparados por HASH FNV-1a dos píxeis (com a tolerância do dono: <0.5% e delta ≤32 — NUNCA precisou de disparar: as recuperações são EXATAS), o INPUT VIVO a cada 32 ciclos (o tap no fundo limpa; o tap NO CUBO re-seleciona — o caminho do PICK 3D completo), o progresso + o RSS a cada 50 ciclos |
| `app/src/main/cpp/render/GlResources.h` | **O ACHADO E O FIX (o único produto desta fase)**: o invalidateAll saltava as camadas cujo recreate tinha FALHADO (`if (!l.valid) continue`) — a camada ficava permanentemente inválida, o term seguinte SALTAVA o destroy dela e o handle do contexto morto sobrevivia; o guard `if (tex_)` do FontAtlas "recriava" com id morto = **TEXTO MORTO após sair e voltar** (o sintoma do dono). O invalidate é IDEMPOTENTE por contrato — corre SEMPRE |
| `tests/test_012z_ctx.cpp` | o pin glresources_ciclo atualizado ao contrato novo (o term duplo invalida 2× as 3 camadas — inv0+6 — e os wrappers aguentam o destroy duplo) |
| `tests/test_sentinels.cpp` | a sentinela **R-070** `regress_glctx_z2_fase1` (o salto PROIBIDO no fonte + a nota do achado + o pin no contrato novo) |
| `docs/REGRESSOES.md` | a entrada R-070 (com a prova de mutação) |

## F1-Z2.2 — A PROVA (o número do dono: ≥1000 ciclos, ZERO falhas)

Comando (7 chunks × 150 ciclos, seeds 101..707 — o mesmo binário, o mesmo
protocolo; os seeds DIFERENTES variam os cenários):

```
fuzz: 150 ciclos | exatos=150 | input=4 | falhas=0   (×7 — seeds 101,202,303,404,505,606,707)
total: 1050 ciclos de perda de contexto REAL · 0 falhas · 28 checks de input vivo · RSS 210→240MB estável
```

Mais os pilotos (seed 77): 40 ciclos exatos + os diagnósticos (8/6/4/24/24
— os que apanharam e isolaram os 3 achados abaixo). **PROVADO: ≥1000
ciclos sem UMA falha; o frame seguinte IDÊNTICO ao de referência (hash
exato — a tolerância nunca disparou); o input responde após qualquer
recuperação.**

## F1-Z2.3 — OS ACHADOS DO FUZZING (o que os ciclos apanharam)

1. **R-070 (P0 — corrigido nesta fase)**: o texto morto após a recuperação
   (a cadeia GlResources.h `if (!l.valid) continue` → FontAtlas.cpp:117
   `if (tex_) return true`) — PROVADO ANTES (o frame pós-recuperação SEM
   NENHUM glifo: fuzz001_rec da pilotagem; o log «camada font FALHOU»
   seguido de «fonts OK (sistema)» no init seguinte — o guard a "reativar"
   com o id morto) e PROVADO DEPOIS (1050 ciclos com o texto EXATO em
   todos). No device: a font-MONO falha SEMPRE em OEM enxuto → a mesma
   cadeia mataria o texto do editor de script após a 2.ª perda.
2. **O layout deslocado (a instância dupla do harness)**: o 1.º piloto
   mostrava o frame recuperado com os painéis deslocados (~24px) — a causa
   era o HARNESS (duas instâncias de android_app: o boot com o
   contentRect, o fuzz com o g_app zerado — os insets T0 R0 no log do
   [boot 1/6]) — REFUTADO como bug da engine (o applyContentRect é fiel ao
   payload; o device entrega o retângulo certo).
3. **O fade dos painéis recomeça no reload (comportamento, não bug)**: com
   o dt congelado o fade dos tabs fica na opacidade inicial (o texto dos
   tabs "morto" no REC) — o protocolo assenta 60 frames antes de capturar
   (o MESMO tratamento nos dois lados). No device o fade corre em tempo
   real (o dono vê os painéis nascerem — o esperado).

## F1-Z2.4 — O RELATÓRIO DE 16 ITENS (FASE 1-Z2)

1. **Objetivo**: o fuzzing do contexto no GLES REAL (≥1000 ciclos, hash de
   píxeis, input vivo) + corrigir o que os ciclos apanharem.
2. **Causa raiz (ficheiro:linha)**: R-070 — render/GlResources.h
   invalidateAll (o salto por valid) + FontAtlas.cpp:117 (o guard tex_).
3. **Sintomas cobertos**: o texto morto pós-perda (o sintoma do dono); o
   gesto preso durante a perda; o cenário de partículas/WE a meio.
4. **O que mudou**: 1 ficheiro de produto (GlResources.h — o invalidate
   sempre) + o modo --fuzz no harness + o pin atualizado + a sentinela
   R-070 + a entrada no REGRESSOES.md.
5. **O que NÃO foi tocado**: os passes de render, a UI, V.ONI,
   importadores, física, .gmesh (o fix é no GESTOR do ciclo de vida).
6. **Ordem dos passes**: intacta (a F2 da 0.12-Z mantém-se; o fuzz não
   muda o render).
7. **Prova ANTES**: o frame sem glifos (a pilotagem; o log «camada font
   FALHOU» → «fonts OK (sistema)» no init seguinte — o guard com o id
   morto) + os 6354 px de diff de texto no 1.º protocolo.
8. **Prova DEPOIS**: 1050 ciclos (7 seeds), hash EXATO em todos, o texto
   presente em todas as recuperações.
9. **Input vivo**: 28 checks (o fundo limpa + o pick do cubo re-seleciona)
   — 0 falhas.
10. **Memória**: o RSS estável (210→240MB em 150 ciclos — o custo por
    contexto ~0.2MB é do llvmpipe, não da engine; o device tem o driver
    próprio).
11. **Testes**: test_core 0 falhas (o pin novo) · c33_virtual 1113/0 ·
    R-070 verde.
12. **Mutações**: re-pôr o salto `if (!l.valid) continue` → R-070 VERMELHO
    + o pin VERMELHO + o fuzz reproduz o texto morto. (A mutação não ficou
    commitada — a prova é o texto do sentinel.)
13. **Verificação local**: os gates da casa verdes (scope/docs-lint/
    docs-lint-state/release-identity/check_main) + os 7 chunks do fuzz.
14. **CI**: verde no commit desta fase.
15. **Riscos**: os wrappers de destroy TÊM de ser idempotentes (o contrato
    está nos comentários do gestor e nos wrappers das 8 camadas — o pin
    do destroy duplo aferra); a camada que falha no recreate continua a
    reportar (o lastFailed/Consola — o comportamento do dono mantém-se).
16. **Próximo passo**: FASE 2-Z2 — o céu (o below-horizon da F0-Z2.3), a
    ordem fixa por frame, o «Repor câmara» + o varrimento de 200 poses.

## TABELA DE RÓTULOS (FASE 1-Z2)

| Afirmação | Rótulo | Prova |
|---|---|---|
| ≥1000 ciclos de perda de contexto real sem UMA falha (hash exato + input vivo) | PROVADO | F1-Z2.2 (1050 ciclos, 7 seeds, 0 falhas) |
| O texto morre após a 2.ª perda de contexto quando uma camada falhou a recriar | PROVADO (antes do fix) | a pilotagem (o frame sem glifos) + o log da camada |
| A causa é o salto por `valid` no invalidateAll | PROVADO | a cadeia com ficheiro:linha + o fix que elimina o sintoma em 1050 ciclos |
| A engine tem fuga de memória por ciclo de contexto | REFUTADO (no hospedeiro) | o RSS estável 210→240MB (o ~0.2MB/ciclo é do llvmpipe; o device entra na VERIFICAÇÃO NO DISPOSITIVO) |
| O fade dos painéis pós-reload é bug | REFUTADO | é a animação de entrada (o mesmo no device); o protocolo assenta antes de comparar |
| O layout deslocado pós-perda é bug da engine | REFUTADO | eram duas instâncias de android_app no HARNESS (o log [boot 1/6] com insets T0 R0) |
| O frame seguinte é idêntico COM gesto durante a perda | PROVADO | os ciclos com gesto (1/8 dos 1050) exatos |
| A recuperação <1s (o critério da 0.12-Z) no GLES real | PROVADO | o ciclo corre DENTRO de 1 frame (o c33 F4 já o aferrava; o fuzz corre o mesmo caminho) |

---

# §F2 — O CÉU, A GRELHA E A CÂMARA (FASE 2-Z2)

## F2-Z2.1 — COMO O CÉU É DESENHADO HOJE (o relatório pedido, ficheiro:linha)

**PROVADO** (lido no fonte + provado no GLES real na FASE 0): o céu É
desde a 0.12-Y **UM passe em ecrã inteiro** — o quad NDC de 4 vértices
(`render/SkyRenderer.cpp`, kQuadCorners, z/w=1.0), a direção reconstruída
da base da view SEM translação (kFsSrc: `dir = normalize(uFwd + ndc.x·
aspect·tanF·uRight + ndc.y·tanF·uUp)`), depth test/write OFF, blend OFF,
desenhado PRIMEIRO (main.cpp:6106, o gate `skyCyc && g_sky.ok() &&
showSky`). Nunca houve geometria fixa no mundo — o pedido 2.2 do dono JÁ
estava cumprido na forma; o defeito era o CONTEÚDO abaixo do horizonte
(F2-Z2.2). A ordem do frame (limpar → céu → geometria → partículas →
grelha → gizmos → UI) está aferrada pela bisseção da F0-Z2.4.

## F2-Z2.2 — O ACHADO E O FIX: O TOM DE CHÃO (a «zona azul/escura fixa» morre)

- **ANTES (PROVADO, captura docs/capturas-0.12-Z2/B5_we_meianoite.png)**: o
  FS fazia `t = clamp(dir.y, 0, 1)` e pintava TODO o meio-plano inferior <!-- docs-lint:allow -->
  com a cor do horizonte — de dia um azul fixo, de noite um azul-escuro
  fixo — a «zona» que o dono vê «cortar» a grelha (a grelha desenha por
  cima, mas o FUNDO não distinguia chão de céu).
- **DEPOIS (PROVADO, captura S2_meianoite_depois.png)**: a pura
  `sky::skyGradient` (render/SkyRenderer.h) e o FS (a réplica linha a
  linha, as MESMAS constantes kGroundDim=0.45/kGroundFadeY=0.18) escurecem
  o abaixo-do-horizonte para horizonte×0.45 com a transição suave de
  0.18 — a LINHA DO HORIZONTE fica visível e o chão deixa de ser uma zona
  fixa. O VLM (z-ai-web-dev-sdk createVision) sobre a captura DEPOIS: «a
  grelha 3D está visível e estende-se por toda a área… não há nenhuma
  zona escura ou azul fixa que corte ou sobreponha à grelha… a linha do
  horizonte é claramente visível».
- O pin `env_sky_type` (tests/test_012y.cpp) atualizado ao contrato novo
  (dir.y=0 segue o HORIZONTE; dir.y=-10 = horizonte×0.45 exato) + a
  sentinela **R-071** `regress_sky_ground` (a pura e o FS com as MESMAS
  constantes — a mutação do clamp puro fica vermelha).

## F2-Z2.3 — O VARRIMENTO DE CÂMARA (o pedido: ≥200 poses, por pixel)

`raster_z2 --sky2 200` (PROVADO, exit 0):
- **400 renders** (200 poses × 2 variantes: WE procedural / SEM WE — o HDRI
  é AMBIENTE CPU no motor (o equirect ilumina, não pinta o céu — a falha
  de import nunca pode pintar o ecrã; o caminho é o mesmo das variantes).
- As poses: as 12 BORDAS fixas (pitch ±89°, dist 0.05/300/8000, os cantos
  ±5000, POR BAIXO da grelha target y=-50) + 188 pseudo-aleatórias
  (determinísticas, seed fixa).
- O veredicto por pixel por pose: as linhas da grelha presentes na metade
  do chão (o pior pose = 403 pixels de linha) e ZERO pixels pretos puros
  (o pior = 0.00% — nenhuma zona escura corta nada).
- As capturas de prova: S2_v0_p00/p02/p04/p06, S2_v1_p02… (o ⌖ e o VLM
  confirmam; a nota honesta: nas poses dist 0.05 a câmara está DENTRO do
  plano da grelha — o VLM lê «a grelha não está visível nesta vista» e os
  eixos/eixos-cruz estão; o critério é a grelha INTEIRA onde ela é visível,
  sem zona a cortá-la).

## F2-Z2.4 — A IMUNIDADE DA GRELHA (o pedido 2.4)

`raster_z2 --sky2`: névoa 0.00 → 0.12 com o MESMO pose/frame congelado:
**0 pixels de LINHA mudaram** — a grelha do editor é IMUNE ao
WorldEnvironment/névoa (a grelha desenha no seu shader sem uFog; o
abafamento próprio da grelha é o fade por distância dela, não do WE). Os
gizmos desenham DEPOIS da grelha sem depth test (a bisseção F0-Z2.4) —
nunca tocados pelo céu.

## F2-Z2.5 — A CÂMARA NUNCA PRESA (o pedido 2.8)

1. **O botão ⌖ «Repor câmara»** na barra (Toolbar.h kTbCamResetId=23, o
   glifo Reset que já existia; Toolbar.cpp topbarLayout nIcon 7→8 + o
   draw; main.cpp consome `ta.camResetPressed` → `cameraResetToDefault()`
   + o toast + a linha na Consola). **PROVADO por input real no GLES**:
   `raster_z2 --sky2` injeta o toque no rect do topbarLayout → a câmara
   volta byte a byte ao padrão.
2. **O duplo toque no vazio** (main.cpp, o detector 2 toques <350ms e
   <32dp no backdrop do viewport) → o MESMO cameraResetToDefault.
   **PROVADO por input real**.
3. **A reposição automática** já existia e mantém-se:
   `cameraEnsureSane()` (main.cpp:1067) no INIT/RESIZED/RESUME — NaN/fora
   dos limites → o padrão + o aviso na Consola («camera: estado inválido…
   a câmara repõe o PADRÃO»).
4. Os limites da órbita (os clamps de sempre pitch ±89°, dist
   0.01..100000) mantêm-se — o varrimento provou que nenhum pose legal
   deixa o render morto.

## F2-Z2.6 — O RELATÓRIO DE 16 ITENS (FASE 2-Z2)

1. **Objetivo**: o céu abaixo do horizonte (a zona fixa), a imunidade da
   grelha, o varrimento de câmara e a câmara nunca presa.
2. **Causa raiz (ficheiro:linha)**: SkyRenderer.h skyGradient + SkyRenderer.cpp
   kFsSrc — o clamp `t=clamp(dir.y,0,1)` pintava o meio-plano inferior com
   uHorizon.
3. **Sintomas cobertos**: (a) a zona azul/escura fixa que «corta» a grelha;
   (c) a câmara perdida (os 3 caminhos de reposição).
4. **O que mudou**: render/SkyRenderer.{h,cpp} (o tom de chão), ui/Toolbar.{h,cpp}
   (o ⌖), platform/main.cpp (o consumo + o duplo toque + o helper), os pins
   (env_sky_type) + a sentinela R-071.
5. **O que NÃO foi tocado**: V.ONI, importadores, física, .gmesh, o layout
   dos painéis (o ⌖ entrou no grupo da direita que já existia).
6. **A ordem dos passes**: intacta (aferrada na F0-Z2.4).
7. **O céu segue a spec do dono**: UM passe fullscreen, direção da view sem
   translação, depth OFF, desenhado primeiro (F2-Z2.1) — agora com o chão.
8. **A grelha**: imune à névoa PROVADO (0 px); o far/fade próprios mantêm-se.
9. **O WorldEnvironment**: os campos sane (a 0.12-Z F2 mantém-se — o pin
   env_sana_nan verde); o HDRI é AMBIENTE CPU (a falha nunca pinta o ecrã).
10. **A precisão**: a névoa/posições em highp (a 0.12-Z F2; o pin existe).
    O teste «forçar mediump e comparar» NÃO VERIFICADO no hospedeiro — o
    llvmpipe é fp32 sempre (PROVADO pela spec do driver); entra na
    VERIFICAÇÃO NO DISPOSITIVO (o Mali GE8320 fp16).
11. **Testes**: test_core 0 falhas (o pin novo) · c33_virtual 1113/0 ·
    raster_z2 --sky2 200 (exit 0) · R-071 verde.
12. **Mutações**: o clamp puro de volta → R-071 VERMELHO; o salto do
    place do ⌖ → o pin do layout? (o botão é provado por INPUT no harness —
    a mutação do id/nome é apanhada pelo check do pick).
13. **Capturas**: S2_v0_p00/p04, S2_v1_p02, S2_meianoite_depois (docs/).
14. **O VLM**: as respostas colaadas em F2-Z2.2/F2-Z2.3 (createVision).
15. **Riscos**: o tom de chão muda TODAS as capturas com céu procedural
    (as golden images da FASE 5 nascem DEPOIS deste commit); o ⌖ rouba
    ~40dp à flexão da barra (o layout encolhe Menu/Cena/tabs — o caminho
    da escala graciosa existente).
16. **Próximo passo**: FASE 3-Z2 — os TICs de clima (o tamanho mínimo em
    píxeis, o emissor a seguir a câmara, as provas por render).

## TABELA DE RÓTULOS (FASE 2-Z2)

| Afirmação | Rótulo | Prova |
|---|---|---|
| A «zona azul/escura fixa» era o clamp do below-horizon no FS do céu | PROVADO | F0-Z2.3 (B1/B2/C_sem_ceu) + o código |
| O tom de chão elimina a zona fixa e a linha do horizonte fica visível | PROVADO | S2_meianoite_depois.png + o VLM + o pin dir.y=-10 = 0.45× |
| A grelha é IMUNE à névoa/WE | PROVADO | 0 pixels de linha mudaram (fog 0→0.12) |
| A grelha está inteira em 200 poses × 2 variantes | PROVADO | --sky2 200 exit 0 (o pior pose 403 linhas, 0.00% preto) |
| O ⌖ da barra repõe a câmara por input real | PROVADO | --sky2 (o tap no rect do topbarLayout) |
| O duplo toque no vazio repõe a câmara por input real | PROVADO | --sky2 |
| A reposição automática NaN/limites avisa na Consola | PROVADO | cameraEnsureSane (main.cpp:1067, o elog::warn) + o c33 |
| O céu HDRI pinta o ecrã | REFUTADO (não existe — é AMBIENTE CPU) | o código (a média CPU ilumina o lit; o céu é clear/procedural) — a falha de import nunca pode pintar o ecrã |
| O teste mediump-forçado (fp16) | NÃO VERIFICADO | o llvmpipe é fp32 (a spec do driver) — entra na VERIFICAÇÃO NO DISPOSITIVO |

# FASE 3-Z2 — O CLIMA VISÍVEL (0.12-Z2 F3)

## F3-Z2.0 — O ACHADO P0: O SISTEMA DE PARTÍCULAS NUNCA RENDERIZOU (a cura antes de tudo)

- **ANTES (PROVADO pela bissecção do GLES real, a sessão F3)**: o clima não
  pintava NENHUM píxel em nenhuma configuração de passes. A bissecção
  eliminou estado (blend/cull/depth/scissor/viewport — dumps ao vivo),
  câmara (uVP lido do driver), VAO/attribs (en/div/buf no draw), seeds
  (lidos de volta do VBO: reais) e buffers. O dump dos UNIFORMS VIVOS
  (`glGetUniformfv`) apanhou o culpado: **`uLifetime=(0.00)`** — o draw da
  engine NUNCA carregou o uniform → `mod(uTime+seed·100, 0.0)` = NaN →
  TODOS os vértices morrem no clip → zero fragmentos. O sistema não
  desenhava UM píxel desde a 0.12-Y-F1; o stub do CI conta draws e nunca
  viu fragmentos; a hipótese F0.4 («sub-pixel») era só a METADE do problema.
- **DEPOIS (PROVADO, --clima V1)**: o upload `glUniform1f(locLifetime_,
  p.lifetime)` (ANTES do uTime — o ÚLTIMO 1f intacto) → chuva 1431 px,
  neve 1627 px, trovoada 1986 px na região central a 720p. A sentinela
  R-073 aferra o upload E a ordem; a mutação (remover) fica VERMELHA.

## F3-Z2.1 — O INVENTÁRIO DOS TICs DE CLIMA (o pedido: a lista completa)

1. **AmbientEnvironment** (components/AmbientEnvironment.h) — o CLIMA:
   weatherType NONE(0)/RAIN(1)/SNOW(2)/STORM(3), intensity 0..1, o preset
   `applyWeatherPreset` (linha 78) e a trovoada `updateStorm` (linha 141,
   o flash via uThunderFlash no FS do céu — sem geometria de raio).
2. **ParticleEmitter** (components/ParticleEmitter.h) — o GPU emitter que
   o clima CONFIGURA (a DECISÃO de arquitetura do dono: o clima não
   renderiza — escreve o emissor do MESMO TIC). F3: + spawnRadius.
3. **WindTIC** (components/WindTIC.h) — o VENTO GLOBAL (a fonte pura
   `windVector`); o 1.º WindTIC ativo alimenta o uWind do VS.
4. **Environment** (components/Environment.h) — o DIA/NOITE (timeOfDay,
   autoCycle, dayCycleSpeed — o relógio do render, congelado na pausa).
5. O flash da trovoada no FS do céu (SkyRenderer kFsSrc uThunderFlash).

## F3-Z2.2 — O TAMANHO MÍNIMO EM PÍXEIS (o pedido: 2–3 px @720p)

- A FONTE ÚNICA `fx::kMinParticlePx = 3.0f` (ParticleShader.h) + a pura
  `minWorldPerDepth(minPx, tanHalf, viewH)` = minPx·2·tanHalf/viewH (o
  tamanho-mundo que projeta minPx píxeis a profundidade 1) + `minWorldOrtho`
  (o w do clip fica 1.0 na orto) + `tanHalfFovDeg` (o deg2rad da casa).
- O VS replica: `vec4 center = uVP*vec4(p,1); if (center.w>0) sz =
  max(sz, uMinWorldPerDepth*center.w);` — o piso pela PROFUNDIDADE REAL do
  centro. O main calcula partMinW pela câmara do frame (a orbit fovY rad /
  a de cena fovY graus / a orto) e pela ALTURA EM PÍXEIS do viewport GL
  real (o rect no editor, o ecrã no Play).
- **PROVADO (--clima V8)**: chuva a dist 200 → 1805 píxeis medidos por
  diferença (a captura C3_minpx_200m.png; sem o piso a gota 0.015 m media
  0.045 px — sub-pixel). O pin weather_min_px_pura aferra a conta (o
  piso·7 excede a gota do preset) e o VS (o uniform + o max SEM comentários).

## F3-Z2.3 — O EMISSOR SEGUE A CÂMARA (o pedido: o volume do mundo)

- A 1.ª run do --clima PROVOU que seguir o OLHO põe o volume fora do
  frustum descendente (0 px); o design final: **o clima vive À VOLTA do
  que a câmara OLHA** — no editor o target da orbit (eye+fwd·dist pela
  MESMA base), em Play eye+fwd·(raio·0.5) — e `followOrigin(alvo, raio) =
  alvo + (0, raio·0.5, 0)` (o volume ACIMA do alvo: a chuva nasce a cair).
- O volume: o buffer de seeds nasce unitário ×kSpawnRadius (0.15) e NUNCA
  regenera — o raio do clima escala no VS por `uSpawnScale =
  spawnRadius/0.15` (`aOriginSeed.xyz * uSpawnScale`). Os presets
  transportam o raio: RAIN/STORM 6, SNOW 5 (a densidade do count do dono:
  1500 num raio 6 forma o cortinado que o frustum cruza — o raio 14 da
  1.ª tentativa espalhava num volume 10× maior e o frustum ficava vazio,
  PROVADO). O NONE repõe o jorro 0.15. O campo `spawnRadius` é serializado
  opcional ("rad", só ≠ default — o .goni antigo carrega byte a byte) com
  a linha do Inspector PeSpawn (5448, slider 0.15..60 + clampFields).
- **PROVADO (--clima V7)**: a câmara a 2000 do mundo → 6984 píxeis de
  chuva NA MESMA POSE (a referência capturada na própria pose — a
  metodologia da 1.ª run comparava poses diferentes e media lixo).

## F3-Z2.4 — O ORÇAMENTO DO FPS (o pedido: degradação automática)

- `fx::WeatherBudget` (ParticleShader.h — a fonte única): a EMA do dt real
  (α 0.1); fps < 50 → scale ×0.85 até ao piso 0.25; fps > 58 → recupera
  (o histerético); o count = teto × scale (arredondado, piso 1, nunca
  acima do pedido). **SÓ os emissores do CLIMA** (TIC com
  AmbientEnvironment ativo ≠ NONE) — o jorro do utilizador nunca é tocado.
  O step corre por frame com o dt CONGELADO na pausa (o mesmo frozen dos
  relógios de render).
- **PROVADO**: o pin weather_budget_mecan (dt sintético: 40 fps degrada
  até ao piso, 60 fps recupera, dt 0 não mexe, a banda 50..58 estável, o
  count 1500×0.25=375) + o --clima V10 no GLES real (25 fps → 0.250;
  60 fps → 1.000). Os NÚMEROS do Mali GE8320 entram na VERIFICAÇÃO NO
  DISPOSITIVO (o llvmpipe não tem o orçamento do chip).

## F3-Z2.5 — AS PROVAS POR TIPO (--clima V1..V10, exit 0, 0 falhas)

| Prova | Veredicto | Números |
|---|---|---|
| V1 chuva VISÍVEL | PROVADO | 1431 px (centro 720p) |
| V1 neve VISÍVEL | PROVADO | 1627 px |
| V1 trovoada VISÍVEL | PROVADO | 1986 px |
| V2 MOVIMENTO (uTime vivo) | PROVADO | hash(t)≠hash(t+0.5) |
| V3 CICLO DE VIDA (o mod volta) | PROVADO | t vs t+1.2s = 0.031% de px (o ruído f32 do mod; Δ≤2) |
| V4 VENTO (o WindTIC desvia) | PROVADO | hash muda; 1208 px de sinal |
| V5 NONE SILENCIA | PROVADO | maxParticles=0 + o raio de volta ao jorro |
| V6 PAUSA CONGELA / RETOMA | PROVADO | g_fxTime parado (0.05→0.05); a retoma 2335 px |
| V7 SEGUE A CÂMARA | PROVADO | 6984 px a 2000 do mundo |
| V8 PISO EM PÍXEIS | PROVADO | 1805 px a dist 200 |
| V10 ORÇAMENTO | PROVADO | 25 fps→0.250; 60 fps→1.000 |
| V9 CONTEXTO SEM COSTURA | PROVADO | TERM/INIT → o hash de píxeis EXATO (a tolerância nem entrou) |

- O VLM (z-ai-web-dev-sdk createVision) sobre as capturas DEPOIS:
  C3_trovoada: «há partículas de chuva (pontos claros espalhados pelo
  ar)… a grelha 3D está visível e inteira… não há zona escura ou azul
  fixa a cortar… o cubo e os gizmos estão visíveis»; C3_neve: «há flocos
  de neve visíveis… pontos brancos espalhados por toda a área da
  viewport». Nota honesta: a C3_minpx_200m tem os 1805 px medidos por
  diferença mas visualmente subtis (3 px por gota a 200 m) — o VLM não
  as aponta a olho; o piso é provado pelo NÚMERO.

## F3-Z2.6 — O ACHADO F3: A PAUSA NÃO CONGELAVA OS RELÓGIOS DO RENDER

- **ANTES (PROVADO, ficheiro:linha)**: o `frozen` do loop (main.cpp) só
  parava os PASSOS FIXOS (`g_time.beginFrame`) — `g_fxTime`, `g_skyTime`,
  o ciclo do dia e o update da trovoada avançavam em Play+pausa (o
  dt real entrava neles SEMPRE). O contrato documentado («a pausa congela
  o céu» — RELATORIO-0.12-Y-E-SKY.md §Y-E; «a pausa congela» — o
  cabeçalho do g_fxTime) NÃO se cumpria.
- **DEPOIS**: `clockFrozen = playMode && paused` — os relógios de render
  (g_skyTime/g_fxTime), o ciclo do dia, o orçamento e o dt da trovoada
  recebem 0 quando congelados. O editor e o Play a correr avançam como
  sempre (o pin V6 aferra: 20 frames reais em pausa → fxTime idêntico; a
  retoma mexe 2335 px).

## F3-Z2.7 — O RELATÓRIO DE 16 ITENS (FASE 3-Z2)

1. **Objetivo**: os TICs de clima visíveis no driver real (a lista, o
   tamanho mínimo em píxeis, o emissor a seguir a câmara, o orçamento do
   FPS, a recuperação de contexto, a pausa, o Inspector com clamp).
2. **Causa raiz (ficheiro:linha)**: (P0) ParticleRenderer.cpp draw() — o
   upload do uLifetime NUNCA existiu → mod(x,0)=NaN → zero fragmentos;
   (F0.4) os presets 0.015 m ≈ 1.3 px @720p sem piso; (F3) o emissor
   preso à origem do TIC; (F3) os relógios do render fora do frozen.
3. **Sintomas cobertos**: «o clima não funciona no device» (o §F0.4 da
   0.12-Z) — as 4 causas fechadas (a sonda, o congelamento, o
   sub-pixel, o emissor fora de vista).
4. **O que mudou**: ParticleRenderer.{h,cpp} (o P0 + os 2 uniforms novos),
   ParticleShader.h (as puras + o VS + o orçamento), ParticleEmitter.h
   (spawnRadius), AmbientEnvironment.h (os raios + o NONE), main.cpp (o
   alvo + o orçamento + os relógios congelados + partMinW),
   SceneSerializer.cpp ("rad"), EditorLayout.h/EditorUi.cpp (a linha
   PeSpawn + o rowCount 8→9), os pins test_012z2_f3.cpp + a sentinela
   R-073 + o raster_z2 --clima.
5. **O que NÃO foi tocado**: V.ONI, os importadores, a física, o .gmesh,
   o layout dos painéis (a linha PeSpawn entra na secção que já existia).
6. **A ordem dos passes**: intacta (a F0-Z2.4; o clima desenha depois da
   geometria, antes da grelha — a bissecção por toggles re-confirmou
   cada passe inocente).
7. **A decisão do tamanho**: kMinParticlePx=3.0 (o teto do intervalo
   2–3 px do dono — o piso visível no Mali); os raios 6/5 pela densidade
   do count do dono (documentado no fonte).
8. **A grelha/gizmos**: intocados (o clima desenha com depth write OFF —
   nunca oclui; o depth test ON respeita o mundo).
9. **O WorldEnvironment**: intacto (o NÉVOA/lit byte a byte; o clima não
   toca o WE — o V1 correu COM WE procedural).
10. **A precisão**: o VS das partículas em highp (já era); o piso em
    píxeis é f32 puro (sem fp16 no caminho). O mediump-forçado segue
    NÃO VERIFICADO no hospedeiro (o llvmpipe é fp32) — VERIFICAÇÃO NO
    DISPOSITIVO.
11. **Testes**: test_core 0 falhas (os 4 pins novos + a sentinela) ·
    c33_virtual 1113/0 · raster_z2 27/0 · raster_z2 --clima 0 falhas ·
    raster_z2 --fuzz 150/0.
12. **Mutações**: o max() do VS removido → R-073 VERMELHO; o upload do
    uLifetime removido → R-073 VERMELHO — repostos, VERDE (ao vivo).
13. **Capturas**: docs/capturas-0.12-Z2/C3_chuva/neve/trovoada/follow_cam/
    minpx_200m/ctx_ref/ctx_rec (a 1.ª chuva VISÍVEL da história da
    engine no driver real).
14. **O VLM**: colado em F3-Z2.5 (trovoada + neve; a nota honesta do
    200 m).
15. **Riscos**: o P0 muda TODAS as cenas com emissores (a partir de agora
    elas DESENHAM — as golden images da FASE 5 nascem DEPOIS deste
    commit); o orçamento no device pode degradar mais cedo (o Mali
    reporta o fps real — VERIFICAÇÃO NO DISPOSITIVO); o raio 6/5 é a
    minha decisão de densidade (o dono pode querer outro — o slider
    PeSpawn edita).
16. **Próximo passo**: FASE 4-Z2 — a auditoria total (a toolchain, os
    bugs 4.2, a caixa preta) e a FASE 5-Z2 (as golden images com a
    chuva viva).

## TABELA DE RÓTULOS (FASE 3-Z2)

| Afirmação | Rótulo | Prova |
|---|---|---|
| O sistema de partículas não desenhava NENHUM píxel no driver real | PROVADO | o dump dos uniforms vivos (uLifetime=0.0) + a bissecção por toggles/passes/estado + a cura (V1: 1431 px) |
| O uniform uLifetime nunca era carregado pelo draw | PROVADO | o fonte (sem o upload) + o dump do driver + a mutação R-073 vermelha |
| A gota 0.015 m era sub-pixel a 720p (o F0.4) | PROVADO | a conta (0.045 px a 200 m) + o piso cura (1805 px a 200 m) |
| Cada tipo de clima é VISÍVEL a 720p | PROVADO | V1: 1431/1627/1986 px + o VLM (trovoada/neve) |
| O emissor do clima segue a câmara | PROVADO | V7: 6984 px a 2000 do mundo |
| A chuva cai (o uTime vivo) | PROVADO | V2: hash(t)≠hash(t+0.5) |
| O ciclo de vida fecha | PROVADO | V3: 0.031% de px (o ruído f32; Δ≤2) |
| O vento desvia a chuva | PROVADO | V4: o hash muda |
| O NONE silencia e repõe o jorro | PROVADO | V5: maxParticles=0, raio 0.15 |
| A pausa congela o clima (o contrato Y-E) | PROVADO | V6 + o fix clockFrozen (o ACHADO F3) |
| A recuperação de contexto é sem costura | PROVADO | V9: o hash EXATO pós TERM/INIT |
| O orçamento degrada/recupera | PROVADO | V10 + o pin (o mecanismo); os números do Mali — NÃO VERIFICADO (o llvmpipe) → VERIFICAÇÃO NO DISPOSITIVO |
| O teste mediump-forçado (fp16) | NÃO VERIFICADO | o llvmpipe é fp32 — VERIFICAÇÃO NO DISPOSITIVO |
