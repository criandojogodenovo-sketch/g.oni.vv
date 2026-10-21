# RELATORIO-0.12-Y-SKY.md — 0.12-Y · Sub-fase D — O Céu Procedural

Atualizado: 2026-10-21 · versionCode: **109 → 110** · a campanha vive no
`RELATORIO-0.12-Y.md` (A/B/C/C-FIX) e a abertura no
`RELATORIO-0.12-Y-ABERTURA.md`.

Cláusula calma: SÓ o que está no prompt da sub-fase D — o shader do céu, o
componente, o Inspector e a integração. ZERO refactors, ZERO
raymarching volumétrico (a performance mobile é prioridade do dono: FBM
de DUAS oitavas, um quad, nenhum post-process).

## §0.2 · As verificações de partida

- **vc 109** — o STATE.md diz 109 e o `app/build.gradle` declara
  `versionCode 109` (o C-FIX fechou no commit c642e15).
- **CI VERDE** — o run 39467610506 (release.yml) fechou 4/4 SUCCESS:
  Testes do core (Linux), C33 virtual (dispositivo + sentinelas + gates),
  APK release arm64 assinado, verify-entry-symbols (o init-keystore é
  skipped por design — só corre quando a keystore não existe).
- **Árvore limpa** — o clone sincronizado com origin/main (o filemode do
  sandbox desligado com core.filemode false; o conteúdo idêntico).

## §1 · A ordem do dono e o que mudou

**A ordem**: «1. O SHADER DO CÉU (render/SkyRenderer.h/.cpp) … Gradiente
de Céu: Mistura a cor do Horizonte com a cor do Zénite baseado no
vViewDir.y. Sol: Um disco brilhante e um glow (halo) … Nuvens (2D FBM): …
O UV das nuvens vai deslizar com o tempo (Time) e com a velocidade do
vento. A cobertura das nuvens (0.0 = céu limpo, 1.0 = nublado) controla o
mix entre o céu e a nuvem. 2. O COMPONENTE ENVIRONMENT … 3. O INSPECTOR …
4. A INTEGRAÇÃO (main.cpp) … antes de desenhar os modelos 3D … Passa o
time acumulado do motor para o shader.»

### O que mudou (por ficheiro)

- **`render/SkyRenderer.h` NOVO** — a FONTE ÚNICA pura (o padrão Pbr.h;
  o FS replica LINHA A LINHA e os pins aferem a MESMA conta): as
  constantes do céu (kCloudScale 3, kCloudSoft 0.2, kSunCosDisk 0.9993 ≈
  2.1°, kSunGlowPow 24, kSunGlowGain 0.35, kHorizonFadeY 0.15, kPlaneEps
  0.05, o vento kWindDir (1.0, 0.35)); `hashCellSky/hash21` (o hash
  INTEIRO da família do ui/Noise2D.h — SEM sin: determinístico CPU/GPU,
  o domínio é Z² inteiro — o vento anda para sempre sem emenda nem tile);
  `valueNoise2` (bilinear + smoothstep); `fbm2` (DUAS oitavas — o orçamento
  mobile); `windOffset(time, speed)` (o offset UV do vento — o MESMO t = o
  MESMO offset); `cloudAlpha(n, coverage)` (smoothstep(1−cov, 1−cov+soft,
  n) — 0 = limpo, 1 = nublado); `skyGradient` (horizonte→zénite por dir.y
  clamp 0..1); `sunDisk/sunGlow/horizonFade`; `viewBasis(view)` (a extração
  das LINHAS right/up/−fwd do lookAt column-major — vale para a orbit E
  para a câmara de jogo); `tanFovYFromProj/aspectFromProj` (m[5]=f,
  m[0]=f/aspect); `viewDir` (a FÓRMULA ÚNICA: fwd + ndc.x·aspect·tanF·right
  + ndc.y·tanF·up). A classe `SkyRenderer` (init/destroy/use/draw/ok — o
  irmão do Grid: quad de 4 vértices em NDC, z=1.0, o Params por frame).
- **`render/SkyRenderer.cpp` NOVO** — o VS (o quad fullscreen: vNdc =
  aCorner, gl_Position z/w = 1.0) e o FS que REPLICA as puras linha a
  linha: o gradiente, o halo ANTES das nuvens, a projeção planar
  dir.xz/max(dir.y, eps)·kCloudScale + o VENTO (kWindDir · (uTime ·
  uSpeed) — o TEMPO é a alma; M-Yd1), o fbm2, a cobertura, a nuvem com o
  tint do sol e o ventre escuro (0.75+0.25·n), o disco DEPOIS (morre
  atrás da nuvem densa). O draw: depth test/write OFF, blend OFF, cull
  OFF→ON (o MESMO padrão de janela de estado do Grid) — é o fundo; tudo o
  que vem depois pinta por cima. **uTime é o ÚLTIMO uniform 1f do draw** e
  uSunDir o ÚLTIMO 3f — a ORDEM é o contrato do pin (o stub só guarda o
  último valor por tipo).
- **`components/Environment.h`** — os 7 campos do dono: `skyType`
  (0 = cor sólida — o C de sempre byte a byte; 1 = procedural), 
  `skyColorHorizon` (#4A90E2), `skyColorZenith` (#1E3A8A), `sunColor`
  (#FFDDAA), `sunDirection` (normalize({0.45, 0.60, −0.30}) — a tarde),
  `cloudCoverage` (0.4), `cloudSpeed` (0.02; o teto kSpeedMax 0.2 = 10× a
  brisa). Os defaults vivem em constantes estáticas (kHorizonDefault etc.)
  — o serializer compara contra ELAS (uma fonte só). O `clampFields`
  cresce: o tipo SÓ tem 0/1 (corrupção cai no sólido), as 3 cores em
  [0,1], a cobertura 0..1, o vento 0..0.2 e a DIREÇÃO DO SOL NORMALIZA
  (o zero cai no default da tarde).
- **`core/SceneSerializer.cpp`** — o append ganha `skyType` (omitido no
  0), `skyHor/skyZen/sunCol/sunDir` (arrays 3 omitidos quando iguais aos
  defaults — o putCol3) e `cloudCov/cloudSpd` (omitidos nos defaults); o
  fill lê tudo com os ausentes = os defaults e o clampFields no fim (a
  lei da casa: defaults omitidos = forward-compat, o .goni antigo carrega
  igual).
- **`ui/EditorLayout.h`** — os 6 kinds novos (EnvSkyType/EnvHor/EnvZen/
  EnvSunC/EnvCov/EnvSpd), os ids 5425..5430 (a faixa 5420..5439 — zero
  colisões com a câmara 5400..5412 e os fields 5530+), o perfil ganha
  `envSkyType` (o plano CONDICIONA a isto — M-Yd2), a fórmula do rowCount
  e o plano: o dropdown SEMPRE visível + os 5 campos SÓ com skyType == 1,
  a seguir os 5 controlos do C-FIX intocados (a ORDEM do dono: tipo →
  horizonte → zénite → sol → nuvens → vento → céu → hdri → ambiente →
  névoa → cor da névoa).
- **`ui/EditorUi.cpp`** — o rótulo do dropdown ("céu: cor sólida"/"céu:
  procedural") + os 3 hexes formatados uma vez; os casos de desenho: o
  dropdown (o ciclar do CamProj), os 3 hex + swatch vivo 48dp (o MESMO
  padrão do céu/névoa do C-FIX), os sliders nuvens 0..1 e vento 0..0.2; o
  despacho de toque: o ciclar escreve o componente, os hex abrem o
  teclado (propósitos 13/14/15).
- **`ui/UiEditor.cpp`** — os casos 13/14/15 do commitTextInput (horizonte/
  zénite/sol por hex; inválido = estado intacto — o contrato do hex).
- **`platform/main.cpp`** — o global `g_sky` + o relógio `g_skyTime`
  (avança com o frame REAL 1× por frame — a pausa congela o céu, a mesma
  honra do sdf::g_time); o boot inicializa o g_sky junto ao g_grid e o
  teardown destroi; o `drawTics` recebe view+proj (o céu precisa da BASE
  da câmara e do fov — os dois extraídos pelas puras) e NO bloco do
  Environment, com skyType == 1 e o g_sky pronto, o quad desenha POR CIMA
  do clear (o 0 = o quad NEM EXISTE no frame — byte a byte); os títulos
  do teclado 11..15 ("COR DO CÉU"/"COR DA NÉVOA"/"COR DO HORIZONTE"/"COR
  DO ZÉNITE"/"COR DO SOL" — os 11/12 ficaram sem título no C-FIX; a cura
  entra aqui).
- **`CMakeLists` (cpp + tests)** — o SkyRenderer.cpp entra nos DOIS alvos
  (o test_wiring087 #include o main.cpp — o drawTics novo precisa dele no
  hospedeiro).

## §2 · Os pins (tests/test_012y.cpp)

- **`env_sky_type`** — (1) as puras: o gradiente nos extremos e no meio,
  o disco/halo nos limites, o fade do horizonte, a cobertura 0 = limpo e
  1 = nublado e a monotonia, o FBM em [0,1] + o determinismo, o VENTO COM
  TEMPO (10 s > 0 s; speed 0 = parado — M-Yd1); (2) a base da câmara (a
  identidade, o lookAt da orbit, o tan/aspect da projeção, a direção nos
  ndc 0 e 1); (3) o componente: os defaults do dono (0.4/0.02/#4A90E2),
  a clamp (corrupção do tipo cai no sólido, os tetos, a direção que
  normaliza, o sol nulo → o default); (4) o roundtrip .goni (skyType +
  as 4 cores + as nuvens byte a byte; os defaults FORA do dump); (5) o
  CONTRATO NO FONTE SEM COMENTÁRIOS (o stripper test::stripCppComments —
  a M-Yd1 provou o falso verde por comentário); (6) o DRAW REAL no stub
  (o padrão pbr_texture_bind): os uniforms existem PELO NOME (uTime/
  uSunDir/uCoverage), o draw devolve {4,1} e manda O TEMPO (o ÚLTIMO 1f =
  3.5 → 3.5; o 0 → 0 — nunca confundir com uniform ausente) e a direção
  do sol (o ÚLTIMO 3f).
- **`inspector_sky`** — (1) o modo SÓLIDO: o dropdown entra (1×), AS
  NUVENS NÃO (hor/zen/sol/cov/spd = 0 — M-Yd2), a fórmula do rowCount
  BATE com o plano; (2) o modo PROCEDURAL: as 6 linhas na ORDEM do dono
  (tipo → horizonte → zénite → sol → nuvens → vento → céu), a fórmula
  bate, os ids não colidem; (3) colapsada: o bit8 esconde TUDO; (4) o
  commit dos hexes 13/14/15 escreve o componente (e o inválido deixa o
  estado intacto).

## §3 · A sentinela R-061 + as mutações

- **`regress_procedural_sky`** (tests/test_sentinels.cpp + a entrada no
  docs/REGRESSOES.md): (1) a fonte única (o vento usa o tempo, o vento 0
  é o parado de sempre, o gradiente, a cobertura, a base da identidade);
  (2) o roundtrip completo (os 7 campos + os defaults fora do dump);
  (3) o inspector condicional (o SÓLIDO com ZERO nuvens, o PROCEDURAL com
  AS 6 na ordem); (4) o contrato no fonte sem comentários.
- **M-Yd1** (o TEMPO morto — o FS vira `vec2(kWindDirX, kWindDirY) *
  uSpeed`): env_sky_type VERMELHO («O TEMPO EMPURRA O UV DAS NUVENS no
  FS (a expressão do contrato — M-Yd1 morre AQUI)»), regress_procedural_
  sky VERMELHA («R-061: o FS DEIXOU DE USAR O TEMPO no UV das nuvens (a
  nuvem parada voltou — M-Yd1)») — reposto, VERDES (1208 OK, 0 falhas).
  **O ACHADO fora da lista**: a 1.ª prova da M-Yd1 ficou VERDE POR UM
  COMENTÁRIO (a linha da réplica citava a expressão dentro do GLSL) — o
  falso verde morreu com o `test::stripCppComments` no TestFramework.h (o
  padrão do ui_vocab_check: o gate caça o fonte SEM comentários) e a
  linha do contrato passou a ser SÓ a expressão de código
  `vec2(kWindDirX, kWindDirY) * (uTime * uSpeed)`.
- **M-Yd2** (a condição morta — o plano vira `if (true || skyType==1)`):
  inspector_sky VERMELHO («NO MODO SÓLIDO as nuvens NÃO entram (hor=1
  zen=1 sol=1 cov=1 spd=1 — M-Yd2 vermelho)» + «a fórmula do rowCount
  divergiu do plano (17 vs 22)»), regress_procedural_sky VERMELHA
  («R-061: O MODO SÓLIDO MOSTRA 5 CAMPOS DE NUVENS (M-Yd2 vermelho — a
  condição morreu)») — reposto, VERDES (1208 OK, 0 falhas). O
  inspector_worldenv ficou VERDE na mutação (a ordem relativa dos 5
  controlos antigos aguenta) — é exatamente por isso que a divergência
  fórmula↔plano vive no inspector_sky.

## §4 · Verificação

- **A LACUNA QUE O CI A PANO** (a honestidade da corrida vermelha): o
  build local (scripts/build_tests.py) só compila o ALVO test_core — o
  CI compila TAMBÉM o c33_virtual, e os 17 chamadores do drawTics lá
  dentro (FASE 25..30) ficaram com a assinatura velha de 2 argumentos →
  o passo «Compilar» do job C33 ficou VERMELHO no 1.º push. A cura: os
  17 chamadores passam view+proj (FASE 25/26 têm as variáveis; os grupos
  ORTHO 27c/28/29/30 passam Mat4::identity() + o próprio vp — o céu
  NUNCA desenha nestes FASEs: o Environment dos harnesses é o sólido de
  sempre, byte a byte). O replay local do c33 (g++-14 + zlib, as flags do
  CMakeLists): **1009 checks, 0 falhas**.
- **test_core** — 1208 OK, 0 falhas com os flakes da sandbox excluídos
  (GONI_SKIP: regress_streaming_paralelo — o crash preexistente provado;
  sentinela_r039/medicoes_010m/medicao_010m — o fork flaky documentado; o
  CI é o árbitro). Corrida completa sem skips: os MESMOS 2 flakes
  documentados, nada mais. Os 3 testes novos verdes (env_sky_type,
  inspector_sky, regress_procedural_sky) e TODOS os antigos intactos
  (env_roundtrip, inspector_worldenv, pbr_texture_bind, wiring087 — o TU
  que compila o main.cpp — incluído).
- **versionCode: 109 → 110** (o §14 do RELATORIO-0.9.6.md declara o par
  no MESMO commit para o gate R-015).
- **STATE.md** — atualizado no MESMO commit (a regra §6; 297 < 300).
- **CODEMAP.md** — o SkyRenderer entra no bullet do render/ (399 < 400).
