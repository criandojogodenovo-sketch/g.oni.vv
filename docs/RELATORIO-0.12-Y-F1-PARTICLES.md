# RELATORIO-0.12-Y-F1-PARTICLES.md — 0.12-Y · Sub-fase F1 — O TIC ParticleEmitter (GPU Particles)

Atualizado: 2026-10-23 · versionCode: **121 → 122** · o céu com o ciclo
dia/noite vive na E (`RELATORIO-0.12-Y-E-SKY.md`); o F1 é o SISTEMA BASE
de partículas — o veículo das próximas (Chuva, Neve, Fogo, Magia).

Cláusula calma: SÓ o que o prompt do F1 pede — o componente, o shader,
o renderer, o Inspector e o preset. ZERO compute shaders (a decisão do
dono: GLES 3.0 — VERTEX SHADER ANIMATION), ZERO física no CPU, os
corpos/compiladores/V.ONI intocados.

## §0.2 · As verificações de partida

- **vc 121** — o STATE.md diz 121 e o `app/build.gradle` declarava
  `versionCode 121` (a 0.12-Y-E fechou no commit emendado 74c19ea).
- **CI VERDE** — o run do 74c19ea fechou SUCCESS (a 1.ª submissão d88f0e1
  morreu no pin `ctx_state_curto`: o STATE.md saiu com 300 linhas EXATAS
  e o teto é <300 — emendado com o índice a 299; a lição: o pin é
  estrito).
- **A suíte local** — test_core 0 falhas (fora os devoradores de disco
  da sandbox documentados: o /tmp enche ~9 GB por corrida de medições;
  o CI é o árbitro) · c33_virtual 1008/0.

## §1 · A ordem do dono e o que mudou

**A ordem**: «Como usamos GLES 3.0, NÃO HÁ COMPUTE SHADERS. Vamos usar
"Vertex Shader Animation". O CPU apenas gera uma vez um buffer com
"seeds" (sementes aleatórias) e a posição inicial das partículas. O
Vertex Shader recebe o uTime (tempo global do motor) e calcula a posição
final da partícula em tempo real. Zero física no CPU.»

### O que mudou (por ficheiro)

- **`components/ParticleEmitter.h` NOVO** — o componente DADOS: `maxParticles`
  (1..2000, default 500 — o teto É o buffer), `lifetime` (0.1..20 s),
  `startColor`/`endColor` (RGBA 0..1 — o âmbar→vermelho do fogo de
  fábrica), `startSize`/`endSize` (0..4), `velocity` (a direção inicial —
  o jorro sobe +3 em Y), `gravity` (cai −4 — a fonte de fábrica),
  `texturePath` ("" = o sprite procedural) + o `active` da casa (o
  MESMO padrão do Environment) + o `clampFields()` de defesa.
- **`render/ParticleShader.h` NOVO** — a FONTE ÚNICA (o padrão
  Pbr.h/SkyRenderer.h; o GLSL replica LINHA A LINHA e os pins aferem a
  MESMA conta): `hashFx`/`randFx` (o hash INTEIRO da família do céu —
  SEM sin, determinístico CPU/GPU), `seedFor`/`generateSeeds` (a esfera
  de raio 0.15 — o jorro nasce volumétrico — + a semente [0,1)),
  `glslMod` (o mod do GLSL — NUNCA negativo; o fmod do C++ mente),
  `localTime` (o M-Yf1 vive aqui), `particlePos` (a balística p =
  p0 + v·t + ½g·t²), `lifeProgress`, `particleSize`, `particleColor` +
  os DOIS shaders: o VS (o billboard pela base da câmara — o MESMO
  viewBasis do céu — + a física) e o FS (a textura ou o disco suave
  procedural + a cor interpolada + o FADE OUT no fim da vida).
- **`render/ParticleRenderer.{h,cpp}` NOVOS** — a init gera UMA VEZ o
  VBO de seeds com o TETO kSeedCapacity (2000 × 4 floats = 32 KB — o
  slider maxParticles SÓ muda o COUNT do draw, NUNCA regenera) + o quad
  dos cantos (loc 0, divisor 0) e os seeds (loc 1, DIVISOR 1 — o GLES
  3.0 core). O draw é UM `glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4,
  count)` por emissor: blend ON, depth test ON, depth write OFF (as
  partículas NUNCA escrevem o z; o restauro é o MESMO contrato do céu) +
  o uTime ÚLTIMO 1f (o contrato da casa). A textura é LAZY
  (`setTexture`: readBytes → loadPng → createFromRGBA, o cache pelo
  path; a falha LOGA 1× e cai no sprite).
- **`core/ComponentStore.{h,cpp}`** — o storage `particleEmitters_` +
  os acessos + as especializações do dispatch + o registo "ParticleEmitter"
  na factory registry **SEMPRE NO FIM** (o 15.º — id 14; os ids dos
  antigos INTACTOS, a regra da casa) + removeAll/hasAny.
- **`core/Presets.{h,cpp}`** — `PresetKind::ParticleEmitter = 7` (Count
  8): o preset é ESTRUTURA — Transform + ParticleEmitter (SEM mesh, SEM
  física; assenta no grid como os objetos).
- **`core/SceneSerializer.cpp`** — o append (tipo "ParticleEmitter") +
  o fill: maxP/life/colA/colB/szA/szB/vel/grav/tex com os DEFAULTS
  OMITIDOS (a fábrica do jorro é silêncio — o .goni antigo carrega byte
  a byte) + o dispatch do load.
- **`ui/EditorLayout.h`** — o `p.pe` no perfil + o bit 9 do colapso +
  a secção "Particle Emitter" com 8 linhas (ids 5440..5447): máx./vida/
  tam. início/tam. fim/cor início/cor fim/gravidade/textura, na ORDEM
  do dono; a fórmula do rowCount em casa.
- **`ui/EditorUi.cpp`** — os 9 casos (a secção colapsável + 5 sliders +
  2 hex com swatch + o botão textura); o tap fora da zona do valor
  (o padrão da D); os hex 16/17 (`UiEditor.cpp` — o MESMO contrato do
  hex: inválido = estado intacto); o botão textura arma a INTENÇÃO
  `partTexPick` (o MESMO contrato UI→main do hdri).
- **`platform/main.cpp`** — o `g_particles` global + `g_fxTime` (o
  relógio do frame real: a PAUSA CONGELA — a mesma honra do g_skyTime) +
  a init no boot + o PASSE das partículas no drawTics (DEPOIS dos
  opacos — transparentes; a origem sai da coluna da TRANSLAÇÃO do world)
  + o preset choice 9 + a intenção da textura (o flag pendente + o
  intercept ANTES do import de sempre + o browserImportParticleTex — o
  irmão do browserImportHdri: valida o PNG, copia para textures/
  particle_<stem>.png e escreve o texturePath).
- **`tests/stub/GLES3/gl3.h`** — `glDrawArraysInstanced` +
  `glVertexAttribDivisor` (os no-ops que REGISTRAM — os pins aferem os
  stats: 1 call, N instâncias, o divisor loc 1 = 1).

## §2 · Como é que é mobile-friendly (o C33)

1. **Zero física no CPU** — o CPU escreve o buffer de seeds UMA VEZ na
   init e nunca mais toca nele; o frame paga UM uniform update + UM draw
   call por emissor.
2. **O VS faz a matemática** — 1 mod + ~10 mul/add POR VÉRTICE (4
   vértices por partícula; o GLES 3.0 core garante o divisor por
   atributo). Nenhum loop, nenhuma textura no VS, nenhum noise.
3. **O FS é mínimo** — 1 amostra de textura OU o disco procedural (1
   sstep), 1 mix de cor, 1 fade. Sem noise, sem raymarch, sem gradientes
   caros; o discard no alpha < 0.004 mata os invisíveis cedo.
4. **O buffer nunca regenera** — o teto (2000) vive no VBO; o slider
   maxParticles é só o count do draw (mudar o valor não re-aloca nada).
5. **A textura é lazy e cacheada pelo path** — o sprite procedural não
   toca em I/O nenhum; o load falho LOGA 1× (o spam morre no flag).

## §3 · Os testes e os pins (a prova)

- **`particle_emitter_cria`** — os defaults da fábrica + a clamp; o
  preset (Transform + ParticleEmitter, SEM mesh/corpo, nome único
  Godot-style); as seeds determinísticas dentro da esfera do raio e a
  semente em [0,1) para o buffer INTEIRO de 2000; o mod do GLSL (−2 mod
  24 = 22), a balística (1+0, 2+3−2, 3+0), o progresso, as interpolações
  e **a prova do M-Yf1** (tempos diferentes movem a partícula); a fórmula
  EXATA do prompt no VS SEM comentários (o stripCppComments); o draw
  real no stub: o divisor (loc 1, 1), 1 call × 2000 vértices, o
  glDrawArraysInstanced com 500 instâncias e o uTime ÚLTIMO 1f.
- **`particle_serializa`** — o roundtrip dos 9 campos (maxP/life/colA/
  colB/szA/szB/vel/grav/tex), os defaults FORA do dump (a fábrica é
  silêncio) e a corrupção clampada (99999→2000, −1→0.1).
- **`particle_inspector`** — SEM o componente NENHUMA linha entra; COM
  o componente a secção + as 8 linhas na ORDEM do dono, a fórmula do
  rowCount bate com o plano e os ids não colidem; o contrato no fonte:
  o slider da vida escreve a LINHA EXATA (`peEdit->lifetime, "%.1fs"` —
  a lição M-UIj: a cópia morta não satisfaz), os clamps, a intenção do
  navegador e os hexes 16/17.
- **R-062 `regress_particles`** — a fábrica, o VS anima (as 3 linhas do
  prompt no fonte), o draw 1×N com o uTime no fim, o .goni transporta o
  emissor inteiro e o Inspector escreve (o M-Yf2 morre aqui).
- **As mutações AO VIVO** — **M-Yf1** (o uTime ignorado no localTime)
  aplicada e capturada VERMELHA: `particle_emitter_cria` morreu com «o
  tempo local (5 mod 2 = 1)» + «o TEMPO NÃO move a partícula (parada)»
  (3 EXPECTs). **M-Yf2** (o Inspector adornado — o slider que não volta
  ao componente) aplicada e capturada VERMELHA: `particle_inspector`
  morreu com «o slider da VIDA deixou de escrever a propriedade» — o 1.º
  pin era AMBÍGUO (a leitura `f32 vidaMorta = peEdit->lifetime`
  satisfazia o countSub) e foi APERTADO para a linha exata do argumento
  (a lição M-UIj repetida). As duas curadas → suíte 0 falhas.
- **Recalibrações com nota** — o 15.º componente na registry (o pin
  14→15, ParticleEmitter==14 SEMPRE NO FIM — os ids intactos); o 9.º
  preset (Count 8; o menu "+" com 9 linhas; o Audio continua o choice 7);
  o R-060 (7→8).

## §4 · A verificação local

- **test_core** — 0 falhas (GONI_SKIP dos 2 flakes documentados da
  sandbox + os 6 devoradores de disco das medições — o /tmp enche ~9 GB
  por corrida; o CI é o árbitro e corre completo).
- **c33_virtual** — 1008 checks, 0 falhas.
- **versionCode** — 122 (o bump do commit).

## §5 · Os não-feitos honestos

- A física é a balística do prompt (sem colisões, sem atração, sem
  curvas de bezier, sem sub-emissores).
- O velocity/gravity de X/Z só editam pelo .goni (o Inspector tem o
  slider de GRAVIDADE Y — o caso real fogo/chuva; os sliders de vetor
  3D são o trabalho do gizmo).
- maxParticles regenerar o buffer em vivo: NÃO — o teto (2000) vive no
  VBO e o slider só muda o count (a decisão §2.4; regenerar por toque
  de slider seria um stutter garantido).
- Sem preview 2D no Inspector (a partícula vê-se no viewport).
- Nenhum preset adicional (fogo/chuva/neve) — o jorro de fábrica é o
  ponto de partida; os presets chegam quando o dono pedir.
- Sem capturas em device físico — o dono valida no telemóvel.
