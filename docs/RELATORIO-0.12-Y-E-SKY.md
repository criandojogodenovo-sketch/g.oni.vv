# RELATORIO-0.12-Y-E-SKY.md — 0.12-Y · Sub-fase E — O Céu Procedural com Nuvens 2.5D e o Ciclo Dia/Noite

Atualizado: 2026-10-23 · versionCode: **120 → 121** · o céu base (o
gradiente, o sol, o FBM das nuvens, o Inspector e o .goni) vive na
sub-fase D (`RELATORIO-0.12-Y-SKY.md`) — a E constrói EM CIMA, sem o
mexer.

Cláusula calma: SÓ o que o prompt da E pede — o ciclo dia/noite, as
estrelas, a altura das nuvens, o Auto Cycle e o Inspector. ZERO
raymarching volumétrico (a ordem do dono mantém-se: FBM 2D no Fragment
Shader, um quad, mobile primeiro — GLES 3.0 / Realme C33).

## §0.2 · As verificações de partida

- **vc 120** — o STATE.md diz 120 e o `app/build.gradle` declarava
  `versionCode 120` (a FASE UI J fechou no commit d914ce4).
- **CI VERDE** — o run do d914ce4 fechou SUCCESS (release.yml: os testes
  do core, o c33 virtual, o APK release arm64 assinado, o
  verify-entry-symbols).
- **Árvore herdada** — a sessão anterior deixou a E escrita na árvore
  SEM commit e SEM a verificação: a auditoria encontrou a MUTAÇÃO M-Ye2
  (a grade morta) AINDA APLICADA no `SkyRenderer.h` — o relógio não
  mudava cor nenhuma e as estrelas valiam 0. O vermelho foi capturado
  AO VIVO antes da cura (a lição do objeto stale do repo, agora com
  testes a provar: `env_daynight_cycle` morreu com «às 20h o céu NÃO
  fica escuro (lum=0.526)»).

## §1 · A ordem do dono e o que mudou

**A ordem**: «1. NUVENS PROCEDURAIS (Mobile-Friendly) — NÃO uses
Raymarching volumétrico 3D … FBM com ruído calculada diretamente no
Fragment Shader … leve parallax dependendo do ângulo da câmara …
cloudCoverage, cloudSpeed, cloudHeight … O UV/coordenada das nuvens deve
deslizar com base no tempo global do motor (uTime). 2. CICLO DIA/NOITE —
timeOfDay (0.0 a 24.0) e dayCycleSpeed … o shader deve calcular a
posição (ângulo) do Sol … o gradiente do céu e a cor da luz ambiente
devem interpolar suavemente entre 4 estados chave: 00:00 Noite (céu
escuro/azul-marinho, estrelas, luar fraco), 06:00 Amanhecer (laranja/
rosa no horizonte), 12:00 Meio-dia (azul claro, sol forte no zénite),
18:00 Pôr do Sol (vermelho/laranja intenso) … A Directional Light deve
rodar em sincronia com o sol e a sua cor/intensidade deve mudar. 3.
INSPECTOR — slider timeOfDay (0 a 24h), checkbox/botão "Auto Cycle",
sliders cloudCoverage, cloudSpeed e cloudHeight.»

### O que mudou (por ficheiro)

- **`components/Environment.h`** — 5 campos novos: `timeOfDay` (0..24,
  default 12 — o MEIO-DIA é a identidade da D: o céu de sempre byte a
  byte), `dayCycleSpeed` (0..1; 1.0 = 1 hora de jogo por segundo real),
  `autoCycle` (bool), `cloudHeight` (0.25..4, default 1 = o plano da D) +
  as constantes `kTodDefault/kTodMax/kCycleMax/kHeightDefault/
  kHeightMin/kHeightMax`. O `clampFields()` trata o relógio como ANEL
  (25.5h é 1.5h; negativo modula) e clampa altura/velocidade.
- **`render/SkyRenderer.h`** (a FONTE ÚNICA pura; o FS replica linha a
  linha) — `sunDirFromTime(tod)` (o ângulo do sol pelo relógio: 06h
  nasce a LESTE +x, 12h no ZÉNITE, 18h põe-se a OESTE −x, 00h sob o
  mundo; 1h = 15° de arco, o tilte −0.25 no z da tarde da D,
  normalizada); `hash3u` + `starField(dx,dy,dz)` (o campo de ESTRELAS:
  células 3D de densidade 60, o hash INTEIRO sem sin decide a presença
  — threshold 184 mata ~72% das células, o offset 0.2..0.8 vem do hash
  e o raio 0.22 em células dá o ponto; morre no horizonte a dir.y<0.08);
  `dayNightGrade(tod, userHor, userZen, userSun, SkyGrade&)` — a GRADE
  dos 4 estados chave com sstep nos segmentos [00→04 plateau noite →06
  amanhecer →12 meio-dia →18 pôr do sol →21 crepúsculo →24 plateau
  noite]; o MEIO-DIA devolve AS CORES DO UTILIZADOR byte a byte (a
  identidade da D — nenhuma segunda mistura dilui o dono); o `SkyGrade`
  transporta hor/zen/sun (os uniforms de sempre) + `light` (a cor da
  DIRECTIONAL — o luar azul fraco kNightLight 0.16/0.22/0.36, o
  alaranjado kDuskLight) + `stars` (o gate 0..1: 1 na noite, 0.10 no
  amanhecer, 0.15 no crepúsculo, 0 no dia); `cloudUv(...,height,...)` —
  a ALTURA escala o plano da projeção (a réplica exata do FS).
- **`render/SkyRenderer.cpp`** — o FS ganha `uniform float uHeight` +
  `uniform float uStars` e a réplica de `starField`; o UV das nuvens
  passa a `(kCloudScale * uHeight)`; as estrelas somam ATRÁS de um
  branch UNIFORME (`if (uStars > 0.001)`) e MORREM atrás das nuvens (o
  mix da nuvem é depois); o draw passa `uHeight` e `uStars` ANTES do
  `uTime` (o ÚLTIMO 1f continua a ser o contrato do pin da D).
- **`render/Material.{h,cpp}`** — o lit ganha `uniform vec3 uLightDir` +
  `uniform vec3 uLightCol` e o setter `setLight(dir, col)`: a difusa e
  a especular passam a consumir `uLightDir` (a direção PARA a luz) e a
  cor entra MULTIPLICADA na difusa+especular (`uLightCol * (alb*mdiff*
  diff + spec*...)`) — a intensidade vive na magnitude da cor. O main
  manda SEMPRE: sem céu procedural o par é o kLightDir de sempre +
  branco (o lit byte a byte); com o ciclo, o sol graduado da grade.
- **`platform/main.cpp`** — (1) a grade corre UMA VEZ por frame no CPU
  ANTES do draw (`dayNightGrade` + `sunDirFromTime` SÓ com
  skyType==1); o quad do céu consome as cores JÁ graduadas; (2) o lit
  recebe `setLight(sunDirNow, gradeNow.light)` em sincronia com o sol
  (a identidade no resto); (3) o AUTO CICLE avança o relógio no `frame()`
  — o 1.º Environment ATIVO manda (o MESMO invariante do clear),
  `timeOfDay += dayCycleSpeed * g_frameDt` + clampFields (dá a volta em
  24h), 1× por frame e nunca por passo fixo (a honra do g_skyTime: o
  Pause congela o dia também).
- **`core/SceneSerializer.cpp`** — o .goni transporta `tod`, `cycSpd`,
  `cycOn`, `cloudH` com os DEFAULTS OMITIDOS (o meio-dia parado é
  silêncio — o .goni da D carrega byte a byte e o forward-compat
  mantém-se); o load passa pelo clampFields (um relógio corrompido de
  30h entra como 6h).
- **`ui/EditorLayout.h`** — 4 linhas novas na secção procedural:
  `kInspectorEnvTod 5431` (slider hora), `kInspectorEnvCyc 5432` (o
  botão auto ciclo), `kInspectorEnvCycS 5433` (slider vel. ciclo),
  `kInspectorEnvHgt 5434` (slider altura nuvens); o `inspectorRowCount`
  passa de +5 para +9 no procedural e o `inspectorPlan` empurra na
  ORDEM do dono (vento → hora → auto ciclo → vel. → altura, antes do
  céu sólido).
- **`ui/EditorUi.cpp`** — os 4 casos novos: o slider "hora" 0..24
  («%.1fh»), o botão "auto ciclo: ligado|desligado" (o toggle da casa,
  o padrão CamActive — o avanço contínuo vive no main), o slider
  "vel. ciclo" 0..1 («%.2f») e o slider "altura nuvens" 0.25..4; o tap
  do botão troca `autoCycle` (fora da zona do valor — o padrão da D);
  os sliders chamam clampFields (o relógio modula no ato).

## §2 · Como o FBM foi otimizado para o C33 (o resumo técnico pedido)

1. **A grade do dia/noite NUNCA corre por pixel** — os 4 estados chave
   e o ângulo do sol são C++ puro (`dayNightGrade`/`sunDirFromTime`) e
   correm 1× por frame no CPU; o FS só consome os uniforms que a D já
   tinha (uHorizon/uZenith/uSunCol/uSunDir) com os valores JÁ
   graduados. Custo por pixel do ciclo: ZERO ALU novo.
2. **As estrelas atrás de um branch UNIFORME** — `if (uStars > 0.001)`
   é um branch no uniform (o MESMO valor para o frame inteiro): no dia
   o campo inteiro é saltado sem divergência de warp (GLES 3.0 não tem
   dynamically-uniform branching garantido em todos os drivers, mas um
   branch uniforme é o padrão seguro e o C33 aceita-o); à noite o custo
   é 1 hash inteiro (3 mults + 2 shifts, SEM sin) por pixel que passou
   o horizonte.
3. **O FBM mantém as DUAS oitavas da D** — a E não acrescentou UMA
   oitava; a altura das nuvens é UMA multiplicação no UV (o plano
   sobe/desce = o parallax pedido, sem camada extra de amostragem).
4. **Nenhum raymarching, nenhum pós-processo** — as nuvens continuam a
   projeção planar `dir.xz / max(dir.y, kPlaneEps)` sobre o FBM 2D; a
   cúpula é o MESMO quad fullscreen da D (4 vértices, 1 draw call).
5. **O relógio é CPU e o clamp é a fonte** — o autoCycle soma no
   `frame()` e o clampFields modula em [0,24); o shader nunca vê um
   tod fora do anel e nunca avalia o sol duas vezes.

## §3 · Os testes e os pins (a prova)

- **`env_sky_clouds`** (test_012y.cpp) — a ALTURA escala o plano (2× =
  2× o uv) e o TEMPO continua a empurrar; o horizonte não explode o
  uv (o piso kPlaneEps); a altura 1 é o plano da D; as ESTRELAS
  determinísticas, esparsas (o threshold mata células E deixa outras
  vivas) e o CENTRO da célula com estrela vale 1; os defaults do
  componente + a clamp do anel (25.5→1.5, −2→22, altura [0.25,4]); o
  contrato no FS (uHeight/uStars/starField/branch) SEM comentários
  (o stripCppComments); o draw real no stub GL: os uniforms novos
  existem e o uTime continua a ser o ÚLTIMO 1f (a ordem da D
  sobrevive).
- **`env_daynight_cycle`** — o ângulo do sol (06h leste, 12h zénite,
  18h oeste, 00h sob o mundo, normalizada); a identidade do meio-dia
  byte a byte; **o teste do dono: 20h = céu escuro (lum<0.16) com
  estrelas (>0.5)**; o luar fraco; o amanhecer/pôr do sol com o
  horizonte quente (r>b); a escada das estrelas noite>crepúsculo>dia;
  a continuidade nos vizinhos do meio-dia e o fecho do anel 0≈24; o
  wiring no fonte (o main consome a grade, o sol e a setLight; a
  identidade da luz de sempre preservada) e o lit no fonte (os
  uniforms novos + a cor na expressão da difusa).
- **`env_serialize`** — o roundtrip dos 4 campos (tod/cycSpd/cycOn/
  cloudH + a cobertura), os defaults FORA do dump e o carregamento sem
  campos a cair na identidade, a corrupção do relógio modulada (30h→6h).
- **`inspector_daynight`** (NOVO) — no SÓLIDO nenhuma das 4 linhas
  entra (a regra do M-Yd2); no PROCEDURAL as 4 na ordem do dono
  (depois do vento, antes do céu sólido), a fórmula do rowCount bate
  com o plano e os ids não colidem.
- **R-061 `regress_procedural_sky`** — a secção (5) NOVA: a identidade
  do meio-dia, o teste do dono das 20h, o sol a rodar, a altura a
  escalar, o roundtrip do .goni e os contratos do FS (a altura no UV +
  o branch das estrelas).
- **As mutações AO VIVO** — **M-Ye2** (a grade morta: o tempo devolve
  sempre as cores do utilizador e as estrelas 0) foi capturada VERMELHA
  no estado herdado da sessão anterior: `env_daynight_cycle` morreu com
  «às 20h o céu NÃO fica escuro (lum=0.526)» + «NÃO há estrelas (0.00)»
  + o luar + o amanhecer/pôr do sol (7 EXPECTs). **M-Ye1** (o uTime
  ignorado: o vento sai do cloudUv) foi aplicada e capturada VERMELHA:
  `env_sky_clouds` morreu («O TEMPO NÃO EMPURRA O UV»). As duas curadas
  → suíte completa **0 falhas**.

## §4 · A verificação local

- **test_core** — 0 falhas (GONI_SKIP=sentinela_r039,
  streaming_paralelo — os 2 flakes documentados da SANDBOX: o /tmp
  encheu a 100% durante a sessão e os logs/medições morrem com errno
  28; o CI é o árbitro e corre completo).
- **c33_virtual** — 1008 checks, 0 falhas.
- **versionCode** — 121 (o bump do commit).

## §5 · Os não-feitos honestos

- O PARALLAX das nuvens é o da D (o plano de projeção com a altura
  escalável) — não há camada dupla de nuvens nem oclusão volumétrica.
- As estrelas não cintilam (sem fase temporal por estrela — o custo
  por pixel à noite ficaria acima do orçamento).
- A LUA não tem disco próprio (o luar é a cor da DIRECTIONAL + o
  gradiente noite; o disco é a extensão natural da próxima sub-fase).
- A chuva/tempo não entra aqui (a campanha das partículas Y-F é o
  veículo).
- Nenhuma captura em device físico — o dono valida no telemóvel (o
  c33_virtual é o dispositivo virtual da casa).
