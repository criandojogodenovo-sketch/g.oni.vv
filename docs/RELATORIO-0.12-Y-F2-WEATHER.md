# RELATORIO-0.12-Y-F2-WEATHER.md — o CLIMA E O VENTO

**Campanha**: 0.12-Y-F2 · **versionCode**: 123 → **124** · **Data**: 2026-10-24

## §0 — A ordem do dono e a decisão de arquitetura

O prompt: «Usar o ParticleEmitter (Y-F1) para criar clima (chuva, neve,
trovoada) e vento global», com a DECISÃO DE ARQUITETURA OBRIGATÓRIA
(opção A): o AmbientEnvironment **NÃO renderiza partículas** — ele
**CONFIGURA** o ParticleEmitter no mesmo TIC quando o weatherType muda;
o WindTIC fornece o vento global lido pelo ParticleRenderer; a trovoada
é um **flash de ecrã** via uniform no sky shader (sem geometria de raio);
sem ParticleEmitter no TIC, o applyWeatherPreset não faz nada (aviso no
log); NADA de compute shaders — tudo em GLES 3.0. A decisão foi honrada
à letra: zero partículas desenhadas pelo clima, zero geometria de raio,
zero compute.

**Desvio documentado (vc)**: o prompt pedia «versionCode bump (122→123)»,
mas o vc 123 já foi consumido pelo fix 0.12-V dos bugs do viewport
(commit 39583ee, o §0.2 verificado antes de arrancar). O bump real desta
sub-fase é **123 → 124** — a regra da casa (1 commit de código = 1 bump)
manda; o 123 fica intacto como história.

**Desvio documentado (sentinela)**: o prompt pedia «Sentinela: R-063
regress_weather», mas a R-063 já foi ocupada pelo 0.12-V
(`regress_viewport_gizmo_world` — docs/REGRESSOES.md). A sentinela nova
é **R-064** (`regress_weather`) — a numeração é o contrato do índice.

## §1 — O QUE ENTROU (com ficheiro:linha)

### 1. O WindTIC (components/WindTIC.h NOVO)
- O componente de DADOS do vento global: `windDirection` (normalizada,
  default (1,0,0)) + `windStrength` (0..10, default 1.0) + `active` (o
  1.º ATIVO da cena manda — o invariante da casa). O `clampFields`
  normaliza a direção (o zero cai no default +x) e clampa a força.
- A FONTE PURA `windVector(w, out)` = direção × força (o padrão
  `ambientFromAvg`): o main e os pins partilham a MESMA conta.

### 2. O AmbientEnvironment (components/AmbientEnvironment.h NOVO)
- `weatherType` (u8: 0=NONE, 1=RAIN, 2=SNOW, 3=STORM, default 0),
  `intensity` (0..1, default 0.5), `active`.
- RUNTIME, NUNCA serializados: `thunderFlash` (0..1) + `thunderTimer`
  (a cena carrega SEM raio pendente — o default 4.0 é o meio do
  intervalo).
- `applyWeatherPreset(ParticleEmitter*)` — devolve `bool` (false = sem
  emissor, o chamador decide o aviso). Os valores EXATOS do prompt:
  RAIN → gravidade (0,−12,0) · vel (0,−18,0) · vida 1.2s · tamanho
  0.015→0.015 · cor (0.6,0.7,0.9,0.7)→(0.6,0.7,0.9,0) · máx 1500×int;
  SNOW → gravidade (0,−1.2,0) · vel (0,−2,0) · vida 4.0s · tamanho
  0.04→0.02 · cor (1,1,1,0.9)→(1,1,1,0) · máx 800×int; STORM → igual à
  chuva com máx 2000×int; NONE → maxParticles 0 (o emissor silencia —
  a clamp do PE levantaria o 0 para 1, por isso a clamp só corre nos
  tipos com partículas; linha 131).
- `updateStorm(dt, rnd01)` — o pseudocódigo EXATO do prompt: o timer
  desce; ≤0 arma `thunderFlash=1.0` e re-agenda `3.0 + rnd·5.0` s; o
  flash morre a 3.0/s (vive ~0.33s). O `rnd01` vem do chamador (o main
  alimenta com o hash INTEIRO da casa sobre o relógio fx —
  determinístico, zero `rand()`; os testes com valor fixo).

### 3. O vento no shader (render/ParticleShader.h)
- O VS ganha `uniform vec3 uWind` (linha 141) e a posição final ganha o
  termo `+ (uWind * localTime)` (linha 154) — a fórmula do prompt:
  `pos = aPosition + (velocity * localTime) + (0.5 * gravity * t²) +
  (uWind * localTime)`. O resto do shader (cor/tamanho/fade) INTACTO.
- A sobrecarga pura `fx::particlePos(..., wind, lt, out)` replica o VS
  (a fonte única); a assinatura sem vento fica INTACTA — os pins da F1
  e o caminho de sempre byte a byte.

### 4. O uniform no renderer (render/ParticleRenderer.h/.cpp)
- `Params.wind[3]` (default (0,0,0)) + `locWind_` + o `glUniform3f` do
  uWind ANTES dos 1f — o uTime continua o ÚLTIMO 1f do draw (o contrato
  do pin da F1/R-062 intacto; o stub só guarda o último valor por tipo).

### 5. O flash no céu (render/SkyRenderer.h/.cpp)
- `Params.thunder` (default 0.0) + `uniform float uThunderFlash` no FS
  (linha 38) + a linha EXATA do prompt no fim do main do FS:
  `col += vec3(uThunderFlash * 0.6, uThunderFlash * 0.65,
  uThunderFlash * 0.75);` (linha 170) — sem geometria de raio, o ecrã
  aclara azul-frio e morre em ~0.33s. O uniform entra ANTES do uTime no
  draw (o ÚLTIMO 1f do céu continua a ser o relógio das nuvens — o
  contrato do pin da D).

### 6. A cena (core/ComponentStore.{h,cpp}, core/Presets.{h,cpp})
- ComponentStore: os storages `winds_`/`ambients_` + os getters
  `winds()`/`ambients()` + os 6 dispatches `storageOf<T>` + o
  removeAll/hasAny + o registo «WindTIC»/«AmbientEnvironment» SEMPRE NO
  FIM (o 16.º/17.º — os ids intactos).
- Presets: `Wind=8`, `Rain=9`, `Snow=10`, `Storm=11`, `Count=12`. Wind =
  Transform + WindTIC (na origem, como o WorldEnvironment — o «sítio»
  não importa); Rain/Snow/Storm = Transform + ParticleEmitter +
  AmbientEnvironment NO MESMO TIC, com o `applyWeatherPreset` corrido NA
  CRIAÇÃO (intensity 0.5/0.5/0.7) e a origem assente no grid (a chuva
  CAI do sítio).

### 7. O .goni (core/SceneSerializer.cpp)
- WindTIC: `dir` [3] + `strength` — os defaults FORA do dump (a brisa da
  fábrica é silêncio); o `fillWindTIC` normaliza ao carregar.
- AmbientEnvironment: `weather` + `intensity` — os defaults FORA do dump;
  o `thunder` runtime NUNCA viaja (a corrupção de tipo cai no NONE).

### 8. O Inspector (ui/EditorLayout.h, ui/EditorUi.cpp)
- IDs novos: 5450..5453 (vento x/y/z/força) + 5456/5457 (clima/
  intensidade) + os bits `kInspBitWind` (1<<10) e `kInspBitAmbient`
  (1<<11) + os kinds `WindSection/WindDirX/Y/Z/WindStr` e
  `AmbSection/AmbWeather/AmbInt` + o perfil `p.wind`/`p.amb` +
  as secções no plano (kInspSectionBase +10/+11).
- Secção **Wind** (WindTIC): 3 sliders vento x/y/z (−1..1 — o vetor
  normaliza no clamp) + 1 slider força (0..10).
- Secção **Ambient** (AmbientEnvironment): o botão clima (o ciclar da
  casa — o padrão CamProj: nenhum→chuva→neve→trovoada; a LÓGICA no
  despacho do tap, o draw só desenha) + o slider intensidade (0..1 — a
  mudança REAPLICA o preset; o COUNT do emissor acompanha a intensidade
  visível).
- A mudança de clima chama `applyWeatherPreset` NO ParticleEmitter DO
  MESMO TIC; sem emissor: `elog::warn` honesto (a decisão do dono).
- As secções novas entram no `isSection` do tap re-despachado (o
  colapsar funciona — o MESMO bit persistente do PeSection).

### 9. O main (platform/main.cpp)
- O menu «+» 3D: 9 → 13 itens (Wind/Rain/Snow/Storm nas choices 10..13).
- O frame: o update do STORM corre NO TOPO do render (depois do
  autoCycle, ANTES do drawTics — o 1.º AmbientEnvironment ATIVO manda, o
  MESMO invariante do clear/ciclo; o rnd01 = hashFx(g_fxTime·1000)).
- O passe das partículas: o 1.º WindTIC ATIVO → `windVector` →
  `pp.wind` (SEM WindTIC: (0,0,0) — a F1 byte a byte).
- O céu: o 1.º AmbientEnvironment ATIVO em STORM → `thunderNow` →
  `sp.thunder` (o lookup no bloco do Environment, ANTES do draw do céu —
  o valor do MESMO frame).

## §2 — OS TESTES (tests/test_012y_f2.cpp NOVO + R-064)

- `wind_tic_cria` — os defaults ((1,0,0)·1.0), a clamp (a direção (3,4)
  normaliza para (0.6,0.8), a força 99→10, o zero → +x), a fonte pura
  windVector, o preset Wind (Transform+WindTIC, SEM mesh/corpo/emissor,
  nome único), o registo por nome (o factory + o removeAll).
- `ambient_rain_preset` — o preset Rain nasce COM o clima aplicado
  (weatherType 1, intensity 0.5, gravidade (0,−12,0) — a M-Yg2 morre
  AQUI, vel −18, vida 1.2, tamanhos 0.015, cores, máx 750 = 1500×0.5);
  o preset REESCREVE um emissor sujo (int 1.0 → 1500); o NONE silencia
  (maxParticles 0 SEM a clamp levantar); o pe==nullptr é inócuo; a
  trovoada 2000×0.7 = 1400 partilha a balística; o update do flash com
  dt 0.25 (exato em binário — o FP acumulado de 0.1 enganava o gatilho):
  4 passos → timer 5.0, flash 0.25; +1 passo → flash 0 (piso); a chuva
  NÃO flasha.
- `ambient_snow_preset` — o preset Snow (weatherType 2, vida 4.0 s — o
  pin do dono, gravidade −1.2, vel −2, o floco encolhe 0.04→0.02, o
  branco com fade, máx 400 = 800×0.5); a clamp (tipo 99 → NONE,
  intensidade 42 → 1.0).
- `ambient_serialize` — o roundtrip (weather/intensity + dir/strength
  voltam; o runtime thunder NUNCA sai — `countSub(dump, "thunder") ==
  0`; a cena carrega SEM raio pendente); os defaults FORA do dump; a
  corrupção normaliza (dir (3,0) → (1,0), força −5 → 0, weather 7 →
  NONE, intensity 9 → 1.0).
- `wind_uniform_passed` — a fórmula no VS SEM comentários (o stripper da
  lição M-Yd1: `uniform vec3 uWind;` + `(uWind * localTime)`); a
  sobrecarga pura soma o desvio wind·t e a assinatura sem vento fica
  byte a byte; o draw real no stub: o uWind é o ÚLTIMO 3f do draw e o
  uTime continua o ÚLTIMO 1f; sem vento o uWind sai (0,0,0); o caminho
  no main (pp.wind + a fonte única windVector).
- **R-064 `regress_weather`** (tests/test_sentinels.cpp) — (1) o vento no
  VS sem comentários + a pura + o draw (o uWind no shader, o uTime
  intacto); (2) o clima configura o emissor na criação (Rain nasce com a
  gravidade −12) + o Inspector chama applyWeatherPreset + o ciclar + os
  valores exatos no componente; (3) a linha do flash no FS do céu + o
  uniform + o sp.thunder + o updateStorm no frame + a condição do STORM;
  (4) o .goni transporta e nunca o runtime.
- **Recalibrações com nota**: `test_components.cpp` (o registry 15→17 —
  os 2 componentes novos SEMPRE NO FIM), `test_wiring011.cpp` (o Count
  8→12 e o menu + com 13 itens — a geometria do tap do Audio segue a
  altura nova), `test_sentinels.cpp` R-060 (o Count 12).

## §3 — AS MUTAÇÕES (vermelho → repor → verde)

- **M-Yg1** (o uWind ignorado no VS — `+ (uWind * localTime)` removido da
  posição): `wind_uniform_passed` VERMELHO («o TERMO DO VENTO saiu da
  posição do VS — M-Yg1») + `regress_weather` VERMELHA («R-064: o TERMO
  DO VENTO saiu da posição do VS»). Reposto — VERDES. As partículas não
  desviariam com vento nenhum: o WindTIC seria um TIC decorativo.
- **M-Yg2** (o applyWeatherPreset não chamado na criação — o `if (false
  && ae)` no Presets.cpp): `ambient_rain_preset` VERMELHO («a gravidade
  NÃO é (0,−12,0) — o applyWeatherPreset não correu na criação —
  M-Yg2») + `ambient_snow_preset` VERMELHO + `regress_weather` VERMELHA
  («R-064: o preset Rain não nasce com o clima aplicado (grav −4.0 vida
  2.00)» — o emissor ficava com o jorro da fábrica debaixo da chuva).
  Reposto — VERDES.

## §4 — O QUE NÃO FOI MEXIDO (a cláusula calma)

- A FÍSICA (BodyComp/phys) — intocada.
- A V.ONI (registry/vm/tykers) — intocada.
- O importador glTF e o .gmesh — intocados.
- O TEMA da UI (Theme.h) e as leis de desenho/toque — intocados.
- O ParticleEmitter da F1 — só ganhou o uWind no shader (o termo zero
  byte a byte) e o uniform no renderer; os defaults, a clamp, o preset,
  o Inspector e o .goni da F1 intactos (os pins dela passam sem recalibração).
- O caminho de sempre SEM clima: sem WindTIC o uWind é (0,0,0); sem
  AmbientEnvironment o uThunderFlash é 0.0; os dois shaders degradam
  byte a byte para a F1/D.

## §5 — Verificação

- `test_core` **0 falhas** localmente (GONI_SKIP=r039,streaming_paralelo
  + os 2 devoradores de disco das medições — os flakes DOCUMENTADOS da
  sandbox: o /tmp enche ~9 GB por corrida; o CI é o árbitro).
- Os gates: check_main · link_parity (124 TUs) · theme_hex · ui_vocab ·
  projects_ui · hierarchy · gizmo_camera_medidas · glyph_source ·
  reload_concurrency · gmesh_docs · jni_parity · scope_check — VERDES.
- Mutações M-Yg1/M-Yg2 provadas AO VIVO (vermelho → repor → verde).
- c33_virtual: a build local não tem cmake (a sandbox) — o CI corre os
  1008 asserts (o árbitro de sempre).
- O achado do dia: o build_tests.py só compara o mtime do .cpp (os
  headers mudados NÃO invalidam os .o) — o ABI misto crashou o
  saf_storage com um segfault que NÃO era meu. A cura: `rm -rf
  tests/build-obj` antes de testar com headers mudados (a regra da casa
  «rebuild de .o quando headers mudam» agora com a prova do porquê).
