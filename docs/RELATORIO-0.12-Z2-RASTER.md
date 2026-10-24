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
