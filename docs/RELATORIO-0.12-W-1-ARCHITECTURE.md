# RELATÓRIO 0.12-W-1 — Corrigir Bugs Críticos + Arquitetura Correta do AmbientEnvironment

Campanha: **0.12-W-1** · versionCode **137 → 138** · Commit: este ·
CI: monitorizado no push (§8).

A DECISÃO DE ARQUITETURA do dono (OBRIGATÓRIA) foi implementada na
íntegra. Este relatório segue as regras de evidência da casa: cada
afirmação marcada **PROVADO** (com a prova nomeada), **REFUTADO** (com
a prova) ou **NÃO VERIFICADO** (com o porquê).

## §0 As verificações de abertura (§0.2/§9 do STATE)

- **CI no arranque: VERDE no 3c59b98 (vc 137)** — PROVADO pela API do
  GitHub (run do HEAD `main` = `success`, jobs core/c33/APK/verify
  verdes). O repo está AVANÇADO face ao prompt (o prompt diz «vc 124
  ou superior» — 137 satisfaz; ver §9 para as divergências de
  baseline).
- As 4 perguntas do §9: §1 a campanha Z2 FECHADA (a W-1 abre por ordem
  do dono — este prompt É a ordem); §4 nenhuma decisão fechada
  reaberta (o lit por WE continua byte-a-byte sem WE; os presets
  Rain/Snow/Storm da Y-F2 intactos); §7 nenhum bloqueio no caminho;
  o prompt É a tarefa.

## §1 O céu procedural por defeito (decisão 1 do dono) — PROVADO

**O contrato**: sem WorldEnvironment na cena, o SkyRenderer usa os
defaults da casa (timeOfDay 12.0, cloudCoverage 0.3); com
WorldEnvironment, lê as propriedades dele. O WE com céu SÓLIDO mantém
o clear de sempre (byte a byte — o quad nem existe no frame).

**A implementação**: `SkyRenderer::Params::fromEnvironment(env, grade,
thunder, skyTime)` (render/SkyRenderer.h) — a FONTE ÚNICA do
mapeamento WE→params (o main e os pins partilham a MESMA conta):
`env == nullptr` → defaults (`sky::kNoEnvTod = 12.0`,
`sky::kNoEnvCoverage = 0.3`, as cores da identidade Y-E, estrelas/lua
nos defaults do dono); `env != nullptr` → o override (a cobertura, o
relógio, as estrelas e a lua DELE). O lit (a luz direcional das malhas)
CONTINUA byte-a-byte sem WE (a decisão W-1 é do céu, não da luz — o
sync do lit com o sol continua só com WE procedural, `skyCyc`).

**As provas**:
- Pin `world_env_optional` (tests/test_012w1_arch.cpp): os defaults
  TODOS (tod 12.0, cobertura 0.3, limiar 184 = a Y-E, brilho 0.8, lua
  0.5/0.6, a lua do meio-dia SOB o horizonte). **PROVADO**.
- Pin `world_env_override`: com WE (tod 22, cobertura 0.7, density 1.0,
  fase 0.25…) o Params lê TUDO (limiar 112, a lua oposta ao sol do
  relógio DELE). **PROVADO**.
- Píxel (raster `--w1`, Mesa/llvmpipe GLES real): sem WE o topo do
  frame é o AZUL procedural (r=44 b=162 lum=0.313 — o velho era o
  clear escuro) e o abaixo-do-horizonte é o TOM DE CHÃO da Z2-F2.
  **PROVADO**.

## §2 O dia/noite + as estrelas + a lua (decisão 2 do dono) — PROVADO

**As propriedades novas do WorldEnvironment** (components/Environment.h,
defaults do prompt): `timeOfDay` (0–24, 12.0) · `dayCycleSpeed` (0–1,
0.0) · `autoCycle` (false) — as três JÁ EXISTIAM da Y-E (o prompt
confirma) — **mais as novas**: `starDensity` (0–1, 0.5) ·
`starBrightness` (0–1, 0.8) · `moonPhase` (0–1, 0.5) · `moonBrightness`
(0–1, 0.6). Todas com sane NaN (o padrão Z2-F1BIS: o NaN volta ao
default do campo) e clamp 0..1. O Inspector ganhou os 4 sliders
(densidade/brilho das estrelas, fase/brilho da lua — EditorLayout.h
5435..5438, os casos no EditorUi.cpp; o slider da hora, o toggle Auto
Cycle e o vel. ciclo já existiam da Y-E).

**A densidade das estrelas**: `sky::starThreshFromDensity(d) = 256−144·d`
— o limiar 184 da Y-E é reproduzido EXATO em d=0.5 (o céu noturno da
Y-E fica byte a byte), 0 mata o campo (limiar 256), 1 dá 112 (denso
sem virar pó). O FS consome `uStarThresh`/`uStarBright`.

**A lua**: `sky::moonDirFromTime(tod)` = a posição OPSTA ao sol (às
22:00 a lua está alta, y=0.84) · o disco `moonDisk` (~1.9°, branco-
amarelado kMoonCol) · **as fases** `moonPhaseLit(u, fase)`: o terminador
LINEAR (a fração iluminada é EXATA nas 4 fases-chave: 0=nova escura,
0.25=quarto com UM lado aceso, 0.5=cheia, 0.75=minguante espelhada,
1=nova) · o glow `moonGlow` (pow 16 × 0.18 — SUBTIL) · tudo atrás das
nuvens (o MESMO contrato do sol: `×(1−a)`) e a morrer no horizonte
(como as estrelas — a lua baixa não assoma por baixo do chão). A sonda
de píxel usa a INVERSA `sky::dirToNdc` (a divisão perspética
dot(dir,fwd)) — a lua às 22:00 ACENTRA no frame pela MATEMÁTICA, sem
constantes mágicas.

**As provas**:
- Pins `stars_visible_at_night` + `moon_visible_at_night`: o gate às
  22:00 ≈ 1 (0 ao meio-dia), a janela 19:00–05:00 do dono CONTIDA na
  rampa suave da Y-E (às 19:00 já acende; às 17:00 só a cauda ténue do
  crepúsculo kDuskStars), as 4 fases-chave exatas, a inversa NDC
  round-trip. **PROVADO**.
- Píxel (raster `--w1`): as estrelas às 22:00 com density 1.0 = **366
  px** na faixa central do viewport; density 0 = **0 px** (o campo
  morre). A lua: cheia (0.5/0.9) = **lum 0.998** no disco; NOVA (fase
  0) = **0.171** (o terminador apagou o centro); brightness 0 =
  **0.018** (disco+glow mortos). **PROVADO POR PÍXEL.**

### §2.1 O ACHADO GPU (P0) — os elementos celestes NUNCA desenharam no rasterizador real

O raster W-1 (o PRIMEIRO a pixel-provar nuvens/estrelas à escala) revelou
que o céu noturno estava **vazio** (0 px de estrelas com density 1.0) e
o golden ANTIGO (vc 137) não tem SOL nem NUVENS (confirmado por VLM nas
capturas de referência). **A causa**: o FS do céu declarava
`precision highp float;` — mas **int/uint em GLSL ES 3.0 nascem MEDIUMP
no fragment shader** (≥16 bits no mínimo do spec): o HASH INTEIRO
(hash21/hash3u — os produtos uint de 32 bits) colapsava na precisão
curta, o hash devolvia sempre o MESMO valor → fbm2 constante →
cloudAlpha 0 (céu SEM nuvens) e starField 0 (SEM estrelas). **Os
elementos celestes (nuvens/estrelas) nunca desenharam no GPU real — os
goldens da Z2 tinham baselined um céu MORTO (determinístico mas vazio)**
— os pins da Y-E aferiam só o stub (a ordem dos uniforms) e as puras
(CPU). **A cura**: `precision highp int;` no FS (o uint segue o int na
casa ES 3.0). Pós-fix: nuvens+estrelas+lua VIVEM no llvmpipe (as provas
de píxel acima) e os goldens foram re-baselined ao céu VIVO (§6).
**PROVADO** (W1DBG: os params chegavam certos ao draw — stars=1.0,
starThresh=112 — e o frame ficava vazio; pós-highp os mesmos params
desenham).

### §2.2 O ACHADO DO ROUND-TRIP (P2) — o «edit fantasma» no sunDir

O pin `w1_goni_compat` apanhou: o `kSunDirDefault {0.5571, 0.7428,
−0.3714}` NÃO era unitário exato — a NORMALIZAÇÃO da `clampFields`
mudava-lhe os bits (0.5571 → 0.557086…) e o round-trip do .goni ganhava
um `sunDir` fantasma (dump ≠ dump(load(dump))). **A cura**: o default
passa a ser o PONTO FIXO bit-a-bit da normalização
{0.557085991, 0.742781341, −0.371390671} (len == 1.0 EXATO em f32 —
verificado por programa de teste; a direção visual é a MESMA, a 4.ª
casa decimal). O round-trip é byte a byte de novo. **PROVADO.**

## §3 O AmbientEnvironment CONTROLADOR com alvo por nome (decisão 3) — PROVADO

**O contrato**: o AE não cria partículas no próprio TIC; é um
CONTROLADOR com `weatherType` (0/1/2/3, default 0), `intensity` (0–1,
0.5) e **`targetEmitter`** (string, default "RainEmitter"). O
`applyWeatherPreset()` procura o ParticleEmitter pelo nome e
configura-o; sem emissor: **log warning e não faz nada**.

**A implementação** (components/AmbientEnvironment.h + core/Presets.cpp
— a FONTE ÚNICA da resolução):
- `findWeatherTarget(scene, aeTic, ae)`: 1.º o PE do MESMO TIC (o
  contrato da Y-F2 byte a byte — os presets Rain/Snow/Storm da casa
  continuam IGUAIS); 2.º sem PE no TIC → o TIC ATIVO com o NOME
  `targetEmitter` que tenha um ParticleEmitter (o fluxo dos DOIS TICs).
- `applyWeatherToTarget(scene, aeTic, ae)`: aplica o preset ao alvo
  resolvido; **sem alvo: o WARN EDGE-TRIGGERED 1×** (o flag runtime
  `targetWarned` desarma o spam; o alvo a aparecer desarma-o — a
  próxima ausência volta a falar) via `elog::warn` (o lineCount é o
  contador da casa).
- O preset **WeatherController** (PresetKind 12, o 14.º do menu "+"):
  Transform + AmbientEnvironment SÓ (o controlador do dono); a criação
  LIGA os dois se o "RainEmitter" já existir. O Inspector ganhou a
  linha «alvo: <nome>» (o teclado in-app, propósito 18); o ciclar do
  clima e o slider de intensidade aplicam pela FONTE ÚNICA; **o RENAME
  de um TIC adota-o** como alvo no momento (o fluxo: cria o
  controlador → cria o PE → renomeia para "RainEmitter" → a chuva
  liga).
- O draw do frame: o PE alvo de um AE ATIVO é CLIMA (segue a câmara
  via fx::followOrigin e paga o orçamento do FPS — a Z2-F3 estendida à
  arquitetura dos DOIS TICs); o jorro do utilizador fica na origem do
  TIC (byte a byte).

**As provas**:
- Pin `ambient_env_finds_emitter`: o DOIS-TICs (o "RainEmitter"
  renomeado + o WeatherController) LIGA (o preset da chuva chega ao
  emissor do OUTRO TIC: grav −12, vel −18, vida 1.2, count 750, raio
  6); o LEGADO prioriza o MESMO TIC; um TIC com o nome mas SEM PE não é
  alvo. **PROVADO**.
- Pin `ambient_env_logs_warning`: sem alvo, o `elog::lineCount` sobe
  UMA vez (+1); a segunda tentativa CALA; o alvo criado desarma; a
  ausência rearmada avisa de novo (+1). **PROVADO**.
- Píxel (raster `--w1`): o "RainEmitter" + o "WeatherController" criados
  por preset → **1856 px de chuva** na faixa central SEM ninguém tocar
  no emissor. **PROVADO POR PÍXEL.**

## §4 O bug da grelha (decisão 4) — a janela IMPLEMENTADA, a ordem REFUTADA

**A janela GL EXATA do dono — IMPLEMENTADA** (render/Grid.cpp):
antes do draw: `glDisable(GL_DEPTH_TEST)` + `glDisable(GL_CULL_FACE)` +
`glEnable(GL_BLEND)` com `glBlendFunc(GL_SRC_ALPHA,
GL_ONE_MINUS_SRC_ALPHA)`; depois: `glEnable(GL_DEPTH_TEST)` +
`glEnable(GL_CULL_FACE)` + `glDisable(GL_BLEND)`. A grelha usa a
PROJEÇÃO da câmara do frame (`g_grid.draw(vp, camEye, camFocus)` — o
vp NUNCA hardcoded) e o ZOOM manda no passo/fade/extent
(`gridStepForDist`/`gridFadeForDist`/extent dinâmico — os degraus
6→1/60→10/600→100 aferidos no pin). **PROVADO** (pin
`grid_no_context_corruption`: a janela completa, NA ORDEM — a abertura
ANTES do draw, a reposição DEPOIS; a mutação M-W3 ao vivo).

**A ordem «grelha ANTES do céu» — REFUTADA AO VIVO**: o passe do céu é
um quad FULLSCREEN com depth OFF (o fundo de sempre, a arquitetura
Z2-F2 provada) — desenhado DEPOIS da grelha, APAGA-A por completo.
**A MUTAÇÃO AO VIVO**: com a grelha movida para antes do céu, **8/9
goldens FICAM VERMELHOS** (default_we/we_midnight/fog_dense/chuva/
neve/trovoada/pos_recuperacao a 8.56% de diff = os 78866 px da grelha
apagados; cam_borda 11.9%; grid_solo EXATO — o céu mascarado nessa
cena, o isolamento por cena provado). A ordem implementada:
**câmara → céu → GRELHA → geometria → partículas → UI** — a grelha
depois do céu (nunca apagada) e antes da geometria (a oclusão dos TICs
vem da ORDEM DE PINTURA: o mesh pinta por cima; o buffer de depth está
limpo, testá-lo não dizia nada) — o corte visual ZERO da Z2-F2 (o tom
de chão) preservado. **REFUTADO o literal, IMPLEMENTADO o intento.**
**PROVADO** (raster `--w1`: com a grelha mascarada o frame muda 78866
px — a grelha desenha POR CIMA do céu).

## §5 Os gizmos nos TICs de partículas (decisão 5) — JÁ CUMPRIDO, PROVADO

O ParticleEmitter **TEM Transform obrigatório**: o preset da Y-F1 cria
Transform+PE (o round-trip .goni mantém — pin
`particle_emitter_has_transform`); o draw EXIGE o Transform (`if (!tr
&& !isWeather) continue` — sem Transform não há origem); o gizmo ancora
no Transform do TIC SELECIONADO (o gizmoTr do main — qualquer TIC com
Transform3D, o PE incluso) e o drag escreve O TRANSFORM (a fonte única
0.12-V: composeWorld/worldToLocalPos); o ParticleRenderer aplica a
transformação às posições (`pp.pos = tr->world.m[12..14]` — a coluna da
translação do world). Tudo JÁ existia da Y-F1+0.12-V — o pin agora
AFERRA o contrato inteiro (o fonte é o contrato + o round-trip).
**PROVADO.**

## §6 As golden images — o re-baseline do céu VIVO

Os 9 goldens da Z2 baselinavam um céu MORTO (§2.1: sem nuvens, sem
estrelas). Com a cura highp + o céu default sem WE (decisão 1) + a
ordem da grelha, os goldens foram REGENERADOS (`--golden-write`) e
verificados **9/9 EXATO ×2**. O `grid_solo` MANTEVE o hash
(edf7fbcfa947fca2 — o céu mascarado nessa cena: a prova de que a
re-baseline tocou SÓ o céu). A rede continua a apanhar: a mutação da
ORDEM (§4) fica VERMELHA em 8/9, e a mutação canónica kGroundDim da Z2
continua a dever ser apanhada pela mesma maquinaria (R-074 intacta).

## §7 Os testes e as mutações ao vivo (decisão 7)

**Pins NOVOS** (tests/test_012w1_arch.cpp — registado no
tests/CMakeLists.txt): `world_env_optional` · `world_env_override` ·
`ambient_env_finds_emitter` · `ambient_env_logs_warning` ·
`grid_no_context_corruption` · `stars_visible_at_night` ·
`moon_visible_at_night` · `particle_emitter_has_transform` ·
`w1_goni_compat` (o byte-a-byte do .goni — os campos novos só viajam
≠ default; o round-trip sem edit fantasma).

**Sentinela R-076 `regress_architecture`** (docs/REGRESSOES.md) com as
TRÊS mutações AO VIVO (todas VERMELHAS → repostas → VERDES):

- **M-W1** (o fromEnvironment ignora o WE): `world_env_override`
  VERMELHO ×6 («o céu não lê a cobertura/vento/limiar/brilho/lua do
  WE») + `regress_architecture` VERMELHO ×4 («R-076: o céu IGNOROU o
  WorldEnvironment»); `world_env_optional` VERDE (o default intacto —
  o isolamento).
- **M-W2** (o findWeatherTarget sem a procura por nome):
  `ambient_env_finds_emitter` VERMELHO ×5 («não acha o
  ParticleEmitter pelo NOME»; «a aplicação ao alvo falhou») +
  `regress_architecture` VERMELHO («o fluxo dos DOIS TICs morreu»).
- **M-W3** (a grelha sem a reposição GL):
  `grid_no_context_corruption` VERMELHO («o FECHO da janela W-1 está
  incompleto — o M-W3 ao vivo») + `regress_architecture` VERMELHO +
  `regress_glctx_fase2` (R-067 recalibrada) VERMELHO ×3 — defesa em
  camadas.

**A MUTAÇÃO DA ORDEM** (§4): 8/9 goldens VERMELHOS — a refutação
«grelha antes do céu» colada no §4.

**Recalibrações legítimas** (com nota): R-060 (Count 12→13 com o
WeatherController) · R-064 (o Inspector aplica pela FONTE ÚNICA
`applyWeatherToTarget`; o flash viaja pelo `fromEnvironment`) · R-067
(a janela W-1 da grelha — o depth func explícito morreu COM o depth
test desligado durante o draw) · env_sky_clouds (a assinatura
`starField(dir, thresh)`) · inspector_sky/daynight (rowCount +4; a
fórmula GANHA as secções Wind/Ambient que a Y-F2 não contava — lacuna
latente fechada) · wiring011 ×2 (Count 13, menu 14 itens).

**As provas de píxel** (raster_z2 --w1, Mesa/llvmpipe GLES 3.2 real):
13 checks/0 falhas/6 PROVAS: o céu default (azul 0.313 + tom de chão)
· as estrelas 366→0 px · a lua 0.998→0.171→0.018 · a grelha sobre o
céu (78866 px) · a chuva por NOME (1856 px).

## §8 A verificação completa nesta máquina

- test_core: **1282 OK / 0 falhas** (GONI_SKIP=regress_streaming_paralelo
  + r039 — AMBOS pré-existentes da sandbox: o R-046 crash flaky
  PROVADO no binário pristine 3c59b98 (1/3 runs, SIGSEGV no
  pthread_join — o sintoma §4.1 exato, a caça suspensa por ordem do
  dono, STATE §7); o r039 = ENOSPC — o disco da sandbox).
- raster_z2: **36/0** · `--golden` **9/9 EXATO** · `--sky2 200` **0
  falhas** · `--clima` **0 falhas** · `--w1` **13/0**.
- Gates locais: docs-lint VERDE · docs-lint(state) VERDE · ui-vocab
  VERDE · release-identity VERDE (com o par 138 declarado no §14-aa).
  O glslang NÃO VERIFICADO nesta sandbox (o binário não existe — o CI
  instala; o FS foi validado AO VIVO pelo próprio llvmpipe na
  compilação do raster).
- CI do GitHub: monitorizado no push (§8 do fecho).

## §9 As divergências do prompt reportadas (a regra «PÁRA e reporta»)

1. **versionCode**: o prompt diz «bump 124→125» — o repo está no 137
   (a campanha Z2 fechou no meio do tempo entre o baseline do prompt e
   hoje). O bump REAL: **137→138** (a regra da casa é +1 do corrente).
2. **Sentinela R-065**: o prompt pede «R-065 regress_architecture» —
   o R-065 ESTÁ OCUPADO (a 0.12-Z F0, «o diagnóstico GL não morre»,
   tal como o R-066..R-075 da Z2). A sentinela nova é a **R-076** (a
   próxima livre da sequência da casa).
3. **Ficheiros**: o prompt cita `components/WorldEnvironment.h` e
   `render/SkyShader.h` — os ficheiros REAIS são `components/
   Environment.h` (o TIC WorldEnvironment) e `render/SkyRenderer.
   {h,cpp}` (o GLSL vive no .cpp como FS de casa; não há SkyShader.h).
4. **A ordem «grelha antes do céu»**: REFUTADA e reportada (§4) —
   implementada a janela GL exata + a ordem céu→grelha→geometria.
5. **O starBrightness default 0.8**: muda o brilho das estrelas da
   Y-E (que nascia a 1.0 implícito) — é o valor do prompt do dono; a
   mudança está no re-baseline dos goldens (§6) e documentada aqui.

## §10 VERIFICAÇÃO NO DISPOSITIVO (a lista do dono)

- **D1**: criar o TIC WorldEnvironment → skyType procedural → mover o
  slider «hora» para 22:00 → as ESTRELAS e a LUA aparecem (a lua
  cheia alta; o slider «fase da lua» parte o disco ao meio às 0.25).
- **D2**: SEM WorldEnvironment na cena → o céu azul procedural com
  NUVENS (a cobertura 0.3) — o default novo (o velho era o fundo
  escuro).
- **D3**: criar um ParticleEmitter e RENOMEAR o TIC para "RainEmitter"
  → criar o WeatherController → a chuva liga SOZINHA (o alvo por
  nome); apagar o RainEmitter → o engine.log ganha O warn (1× — o
  spam morto); recriar → liga de novo sem warn.
- **D4**: a grelha: dar zoom (pinch) → o passo adapta (0.1/1/10/100);
  a grelha por baixo dos TICs (a oclusão pela pintura); nenhum artefacto
  de estado nos passes seguintes (a janela W-1 fecha).
- **D5**: o Mali (GE8320): as nuvens/estrelas/lua RENDERIZAM (a cura
  highp — se o device respeitar mediump int de 16 bits, sem a cura
  NADA disto aparecia); o fps do clima (o orçamento 50fps da Z2-F3
  agora também no emissor por nome).
- **D6**: o APK assinado do CI (vc 138) — o artefacto do run verde.
