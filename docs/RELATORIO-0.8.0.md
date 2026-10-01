# RELATÓRIO 0.8.0 — Animação: timeline + keyframes + AnimationPlayer + primitivas mesh procedurais

Primeira sub-fase da campanha F7. A engine tinha UI criável, cenas, câmara e
gizmos, mas NADA se movia sozinho (sem timeline/keyframes/clips/skinning/
blending) e só tinha um placeholder de mesh (o cubo). Esta sub-fase dá
MOVIMENTO a TICs e à UI, e primitivas procedurais para prototipagem rápida
— as outras três (0.8.1 clips glTF, 0.8.2 skinning, 0.8.3 blending)
constroem em cima disto.

## 1. Objetivo

(1) **Componente AnimationPlayer** num TIC, com tracks de keyframes.
(2) **Editor de timeline**: scrub (arrastar o tempo), add/remove key,
curvas **linear e bezier** por track. (3) Keyframes animam propriedades de
**TICs** (pos/rot/scale do Transform3D) e de **UI** (pos/cor/alpha de
elementos UiCanvas). (4) **Playback em Play**: modos loop/ping-pong/once,
velocidade ajustável. (5) **Serialização .goni** das tracks/keys/curvas;
round-trip. (6) **Primitivas mesh procedurais**: preset de TIC **"Mesh"**
(só Transform+MeshRenderer, sem física) e seletor de mesh no Inspector com
**esfera, cilindro, cone, box, plano, triângulo/wedge, torus, cápsula** —
cada uma com parâmetros (raio/altura/segmentos) e defaults sensatos; o
tipo+parâmetros serializam no `.goni`. (7) Testes de tudo.

## 2. Estado inicial (HEAD de entrada)

`aecff47` (0.7.10-b, relatório — trio pós-0.7.7 fechado). Suíte: 437 OK /
0 falhas; CI 100% verde (run 36847994992); APK 0.7.10 assinado
(versionCode 30, sha256 `ca11afd0…`).

## 3. Arquitetura escolhida

**O player é DADOS + duas funções puras.** `components/AnimationPlayer`:
`clips` (lista nomeada; o clip 0 "edit" é o da timeline), tracks com
`target` (TicPos/TicRot/TicScale/UiPos/UiColor/UiAlpha) + `elemento` por
nome + `curve` (linear|bezier) + keys `{t, v[4], tanIn[4], tanOut[4]}`.
`evalTrack` (interpolação) e `apply` (escrita nos componentes) são
matemática pura sobre os storages — GL-free, afervel no CI. O runtime
(`playing`/`time`/`dir` do ping-pong) NUNCA serializa: uma cena carregada
nasce parada no t=0.

**Bezier cúbica por canal com tangentes OFFSET.** `p1 = v0 + out`,
`p2 = v1 + in`. Handles a ZERO dão um EASE suave (smoothstep — NÃO
linear; a primeira versão do comentário dizia "linear" e o TESTE
desmentiu: foi corrigido para a verdade); handles proporcionais ao
segmento (`out=Δ/3, in=−Δ/3`) reproduzem a reta — é daí que o editor
parte. Rot em GRAUS Euler XYZ (a convenção do Inspector; quat→euler na
aplicação — conversão lossy documentada, boa para rotações de eixo
simples).

**Playback como a física: gate `enabled`.** `core/AnimationSystem` no
grupo Update, registado ANTES do TransformSystem (escreve pos/rot/scale e
já chama `updateWorld()`; o Transform reconfirma o cache no mesmo passo —
nunca vê dados meio-escritos). Em PLAY o `enterPlayMode` arranca TODOS os
players do zero; `leavePlayMode` para-os e o PlaySnapshot devolve a pose
de editor. **PlaySnapshot estendido a elementos de UI** (ox/oy/cor/alpha
por índice): a UI animada em Play também é descartada limpa.

**A timeline é uma STRIP no viewport central — zero sobreposição.**
`ui/Timeline`: `timelineRect` = fundo do viewport central (entre os
painéis, colada acima da status line); NADA é desenhado sobre a
Hierarchy/Inspector/toolbar. O main calcula `viewRect` (viewport MENOS a
strip) para o orbit e o tap-de-seleção — os gestos 3D nascem só na área
acima da timeline. Scrub com apply AO VIVO; keys (tap seleciona a ≤14 px,
drag move o tempo com clamp entre vizinhas, release ordena); +key por row
com o valor ATUAL da propriedade (posa-se o objeto, clica-se +); preview
Play/Stop com SANDBOX própria (mini PlaySnapshot — trocar de seleção com
preview a correr restaura antes de seguir; entrar em Play de jogo idem).

**Primitivas com winding provado, cache por assinatura.**
`render/Primitives`: 8 geradores (esfera UV com polos EXATOS — `sin(pi)`
flutuante deixava a última linha "quase" no polo e o teste de winding
apanhou 16 triângulos com normal aleatória; cilindro/cone com tampas;
plano em y=0; wedge com a RAMPA como face de trabalho; torus; cápsula
altura total com fans nos polos). O main mantém um **cache por
assinatura** (tipo+parâmetros clampados → 1 `Mesh` GL partilhado;
lifecycle como o cubo: destruído no TERM, rebind lazy por frame). O
**preset "Mesh"** nasce com a esfera default e SEM física
(`createTicFromPreset` retorna antes do BodyComp). O Inspector ganhou a
linha "prim:" (seletor grelha 2×4 mono + none — primitivas são built-in,
não dependem do catálogo) e sliders de parâmetros (raio/tam, altura,
segmentos, tubo) com rebind lazy; escolher cube/file LIMPA o prim (uma
fonte de mesh de cada vez — o mesmo no sentido inverso).

## 4. Implementação (por ficheiro, commit `cf1abaf`)

- **`components/AnimationPlayer.h/.cpp`** (NOVO) — tracks/keys/curvas/
  clips/evalTrack/blendKeys/advance (once/loop/pingpong+speed)/apply
  (TIC + UI por nome; alvos inexistentes saltados, nunca crasha).
- **`core/AnimationSystem.h/.cpp`** (NOVO) — gate `enabled`, avança+aplica
  todos os players no passo fixo.
- **`core/PlaySnapshot.h`** — captura/restaura elementos de UI (por índice;
  canvas que mudou durante o Play → saltado).
- **`render/Primitives.h/.cpp`** (NOVO) — 8 primitivas + `primName/
  primFromName/primDefaults/primClamp/primBounds`; winding CCW.
- **`core/Presets.h/.cpp`** — `PresetKind::Mesh` (Count 5); nasce com
  primOn+esfera default; SEM BodyComp/InputMap.
- **`components/MeshRenderer.h`** — `primOn` + `prim` (assinatura do cache).
- **`core/ComponentStore.h/.cpp`** — storage `animators_` + registry
  "AnimationPlayer" (id 7, sempre no fim — ids antigos intactos).
- **`core/SceneSerializer.h/.cpp`** — LoadCtx.`resolvePrim`;
  AnimationPlayer (clips/tracks/keys/in/out/curve/mode/speed/active;
  runtime não persiste; alvo desconhecido ignorado — forward-compat);
  MeshRenderer `"mesh":"prim"` + bloco `"prim"` (tipo+params; defaults do
  tipo quando ausentes; exclusivo com cube/file).
- **`ui/Timeline.h/.cpp`** (NOVO) — strip + header (play/pause/stop/mode/
  speed/+track) + régua + rows + keys + scrub + overlay de alvos + preview
  sandbox; ids 7000+.
- **`ui/EditorLayout.h`** — InspProfile {prim, primKind, anim, canAnim};
  kinds PrimButton/PrimSlider/AddAnim/AnimLabel; ids 5003/5600..5603/3060;
  `primUsesHeight/Segments/Tube`; buffer do plano 32→48 rows.
- **`ui/EditorUi.h/.cpp`** — AssetResolvers.`prim`; drawAssetMenu kind 4
  (grelha de primitivas); applyAssetPick menuKind 4 (primOn+defaults+mesh
  do resolver; none desliga; cube/file limpam o prim); Inspector: linha
  "prim:" + sliders de params (rebind pendente = mesh null) + add
  Animacao/anim label; drawPlusMenu 6 itens ("Mesh"); plan[48].
- **`platform/main.cpp`** — g_animSystem + g_timeline + cache de
  primitivas (`primMesh`/`primMeshDestroy`/`rebindPrimMeshes` lazy);
  resolvePrim no LoadCtx e resolvers; registro ANTES do TransformSystem;
  viewRect reduzido para orbit/tap com timeline visível; timeline wired;
  preset Mesh no "+" e no spawn; enter/leavePlayMode com players
  (arrancam/param do zero); TERM destrói o cache; reentrada reseta.
- **`tests/test_anim.cpp`** (NOVO, 15) · **`tests/test_prims.cpp`** (NOVO,
  10) · **`tests/test_timeline.cpp`** (NOVO, 11) — ver §6.
- **`tests/TestFramework.h`** — `ASSERT` (pré-condições que abortam o caso
  em vez de crashar o runner).
- **`app/build.gradle`** — bump 0.8.0 / versionCode 31.
- **`.github/workflows/release.yml`** — artifact
  `goni-vv-0.8.0-release-signed`.
- **`README.md`** — escopo 0.8.0 + roteiro C33 (11 passos).

## 5. Decisões técnicas relevantes

1. **Beziers com offsets de valor** (não de tempo): a curva avalia `u`
   normalizado do segmento; os handles mexem o VALOR dos pontos de
   controlo. Handles zero = ease suave; o default do track é LINEAR e o
   botão bez alterna — sem handles editáveis por arrasto nesta sub-fase
   (dívida documentada; a curva já interpola e serializa).
2. **Rot por euler-graus** (convenção do Inspector): round-trip humano e
   serialização limpa; a conversão quat→euler→quat é lossy em rotações
   compostas — aceitável para o protótipo, documentada para 0.8.2/0.8.3
   (skin/blend trabalham sobre os mesmos keys).
3. **Um player por TIC, alvo = o dono** (ou elemento do canvas do dono):
   mantém o apply O(1) sem mapa de handles; os clips glTF da 0.8.1
   herdam esta regra (channels do nó raiz → o TIC; o resto fica para a
   0.8.2 quando há joints).
4. **Cache de primitivas por assinatura CLAMPADA**: `raio 100` e `raio 64`
   fazem o MESMO mesh (o clamp acontece antes da chave) — chaves estáveis,
   sem fragmentação do cache.
5. **Rebind lazy das primitivas** (mesh null → main religa no frame
   seguinte): o padrão immediate-mode da casa (o seletor de texturas faz
   igual) — os sliders de params nunca bloqueiam o frame.
6. **A timeline não é modal e não é painel**: é strip do viewport. O
   viewport 3D perde 192 px de altura quando visível (orbit incluído) —
   nada overlap, nada de z-order novo, a status line fica onde estava.
7. **Winding provado, não assumido**: a primeira versão dos geradores
   tinha a esfera e o torus INVERTIDOS (análise de Jacobiano num
   ficheiro de debug revelou 16 triângulos maus na esfera; os polos não
   eram exatos). O teste `prim_winding_concorda_com_as_normais` agora
   confere triângulo a triângulo que a normal da face concorda com as
   normais dos vértices — nenhum gerador volta a sair invertido.

## 6. Testes novos (CI Linux)

**`test_anim.cpp` (+15)**: linear correta (meio/quartos/clamp aos
extremos/multi-keys por segmentos); bezier correta (fórmula cúbica
1.75 no caso canónico; extremos exatos); bezier handles-zero = ease
suave (smoothstep) E handles lineares reproduzem a reta; playback avança
pos (com world coerente NO MESMO frame), rot euler→quat (45° no meio),
scale, UI pos/cor/alpha (por nome de elemento); alvo inexistente salta
sem crash; once para no fim (playing=false e não avança mais), loop dá
a volta, pingpong = onda triangular exata (5 s→1, 6 s→2, 7 s→1 com dir
−1, salto de 6 s de uma vez); speed 2×/0.5×; clip vazio não acumula;
round-trip .goni COMPLETO (targets/elemento/curva/tangentes/mode/speed,
keys desordenadas chegam ordenadas, runtime nasce parado); defaults
minimais (mode/speed/clips omitidos); forward-compat (alvo desconhecido
ignorado, clip importado volta); registry factory; preset Mesh SEM física
(nome único Mesh.001); PlaySnapshot UI (captura/restaura ox/oy/cor/alpha;
índice fora de intervalo salta); addTrack idempotente + duration max.

**`test_prims.cpp` (+10)**: as 8 geram geometria válida (índices %3 e no
range, normais unitárias, posições finitas, uv em [0,1]); winding CCW
concorda com as normais triângulo a triângulo (degenerados contados à
parte); bbox coerente (esfera ±r, cilindro ±r/±h/2, cone base/ápice, box
24/36 do makeCube, plano y=0, wedge com topo só em z=−0.5, torus
±(R+r) com rings=8 a amostrar th=90°, cápsula altura TOTAL); params
respeitados (raio 2→±2; segments/rings contam vértices exatos;
determinismo); clamps (segments<3, cápsula h=2r, torus tubo=R/2, caps
u16); nomes round-trip (PT: "esfera"…"capsula"; desconhecido→esfera);
defaults sensatos (escala 1-10 u); serialização "mesh":"prim"+tipo+params
no dump; round-trip com resolver sentinela (rebind + params vistos);
sem resolver entra na mesma (primOn+params, mesh null); params parciais
→ defaults do tipo; prim e cube/file EXCLUSIVOS (dump decide por primOn).

**`test_timeline.cpp` (+11)**: visível só com player (e fechada em
play/modo UI/TIC inativo); rect colado ao fundo do viewport central SEM
interseção com hierarquia/inspector/toolbar/status; scrub move o tempo e
aplica a pose AO VIVO (drag régua até meio → t=1 → pos.x=1); +key no
cursor com o valor ATUAL (e substitui a <1/30 s em vez de duplicar);
−key remove a mais próxima do cursor; curva lin↔bez por track; preview
Play/Stop restaura a pose de editor exata (sandbox fecha, tempo a 0);
mode cicla once→loop→pingpong→once; speed slider (>2× à direita); overlay
+track adiciona e é idempotente; troca de seleção com preview a correr
restaura o primeiro TIC.

Contratos atualizados: `registry().count()` 7→8; plano do Inspector
(linha "prim:" entre mesh e tex; add Animacao no fundo; alturas
750→828 / 742→820; última linha do plano = AddAnim).

## 7. Commits da sub-fase (branch main)

- `cf1abaf` — 0.8.0-a: AnimationPlayer + AnimationSystem + PlaySnapshot
  UI + Primitives (8) + preset Mesh + seletor "prim:" + timeline editor +
  serialização prim/anim + 44 testes + bump 0.8.0/versionCode 31 + README.

## 8. HEAD da sub-fase

`cf1abaf` + este relatório (commit do relatório fecha a sub-fase).

## 9. Suíte de testes

**481 OK / 0 falhas** (437 → 481: +44 novos, 4 contratos atualizados ao
comportamento 0.8.0). Confirmado no hospedeiro no MESMO commit do push; o
run **36863098500** do CI passou a suíte (ctest 100%) + check_main +
link_parity (83 TUs) + jni_parity + JVM host + projects check.

## 10. CI

Run **36863098500** 100% verde: Testes do core (481 + checks),
build-release (APK assinado), verify-entry-symbols.

## 11. APK

`goni-vv-0.8.0-release-signed` (artifact 11162341964 do run 36863098500),
versionCode **31**, versionName **0.8.0**.
sha256 do APK assinado:
`d6159137bf5c4b7c0793f9e26deecbc644b7019592fefdfb50b8619b2e36d98f`

## 12. Verificação no device (roteiro C33 — resumo)

Roteiro completo no README (secção 0.8.0, 11 passos): primitivas do "+"
Mesh e do seletor "prim:" com sliders ao vivo; Mesh sem física; timeline
abre com "add Animacao" sem tocar nos painéis; track+keys (+key posa e
clica); scrub ao vivo; preview Play/Stop com pose de editor de volta;
modos/velocidade; UI anima (preview e Play de jogo); Play de jogo anima
todos os players e o Stop devolve TUDO (objetos e UI); persistência
(primitiva+params, tracks/keys/curvas, mode/speed); regressões 0.7.10/
0.7.9/0.7.8.

## 13. Riscos e mitigações

- **Euler nas rotações**: composições extremas (gimbal) distorcem entre
  keys — aceitável no protótipo; a 0.8.2/0.8.3 revisitam (skin/blend).
- **Timeline sem scroll na lista de rows** (cap 3 visíveis + "+N tracks"):
  cenas com muitos tracks ficam apertadas — scroll na strip fica para um
  ciclo futuro (CLÁUSULA CALMA: zero UI nova além da timeline).
- **Handles bezier não arrastáveis**: a curva interpola/serializa mas o
  editor não tem handles visuais — os valores default (ease) cobrem o caso
  "suavizar"; edição fina fica para F8.
- **Cache de primitivas cresce sem limite**: cada assinatura distinta é um
  objeto GL; o slider de segmentos gera até ~28 assinaturas por tipo —
eviction policy fica para F8 (memória por primitiva é pequena).

## 14. Dívida técnica conhecida

- Timeline: cap de 3 rows visíveis (sem scroll — §13).
- Bezier sem handles arrastáveis (§13); CUBICSPLINE do glTF será tolerado
  como linear na 0.8.1 (debt já mapeado).
- Keys de UI por NOME de elemento: elemento renomeado órfã o track
  (salta no apply — sem crash, sem efeito; o dono re-aponta na timeline).
- Player por TIC: animações glTF multi-nó chegarão só pelo nó raiz na
  0.8.1 (o resto pede a 0.8.2 — joints).

## 15. Restrições respeitadas (CLÁUSULA CALMA)

Só animação + primitivas mesh + testes. Zero física nova (o preset Mesh
NEM leva BodyComp; a física intocada), zero scripting (V.ONI=F9.1
intocado), zero UI de editor nova além da TIMELINE (a strip vive no
viewport central; painéis/toolbar/status intocados — aferido no teste de
interseção). Tema mono; nada sobreposto (o plano do Inspector continua
sequencial sem sobreposição — alturas atualizadas nos contratos). Bump
0.8.0 / versionCode 31. Nenhum emoji.

## 16. Conclusão

A cena mexe-se sozinha: AnimationPlayer com tracks/keys/curvas
linear+bezier, timeline com scrub/keys/preview sandbox, playback
once/loop/pingpong com velocidade em Play (e a UI anima junto, com
snapshot estendido), e a prototipagem deixou de importar nada — 8
primitivas procedurais com parâmetros no Inspector, preset "Mesh" sem
física, tudo serializado no `.goni` com round-trip. Suíte 437→481; CI
100% verde com APK assinado. **Base da campanha F7 posta — a 0.8.1
importa clips glTF para este player.**
