# RELATÓRIO 0.7.10 — Frustum domado (cap visual + hit-test restrito + prioridade de objetos)

Sub-fase final do trio pós-0.7.7 do C33. O dono reportou: **o frustum da
câmara desenha-se grande demais** (escala com `far`) **e o cone rouba
toques** — seleciona a câmara em vez do objeto/orbit, interferindo com o
resto. A 0.7.8 fechou a fronteira GL 3D↔UI; a 0.7.9 fechou o grab-lock
dos gizmos; esta doma o gizmo da câmara.

## 1. Objetivo

(1) **Tamanho visual clampado**: o cone/frustum desenha-se com comprimento
visual limitado (confortável no ecrã), independente do `far` real (que
continua a valer para o render no Play). O valor de far vive no
Inspector, não no tamanho do cone. (2) **Hit-test restrito**: seleção por
toque na câmara só no corpo+lente (a caixa pequena); tocar no cone/
frustum vazio não seleciona nem bloqueia o orbit; handles só hit-testáveis
com a câmara já selecionada. (3) **Prioridade de objetos**: toques que
acertam meshes/TICs selecionáveis têm prioridade; a câmara só é apanhada
se nada mais for acertado (e só via corpo/lente). (4) **Toggle de
visibilidade do frustum** no Inspector (esconder quando polui).
(5) Testes de tudo.

## 2. Estado inicial (HEAD de entrada)

`882e7df` (0.7.9-b, relatório — sub-fase 0.7.9 fechada). Suíte: 433 OK /
0 falhas; CI 100% verde (run 36846579971); APK 0.7.9 assinado
(versionCode 29, sha256 `4ef443cd…`).

## 3. Arquitetura escolhida

**O clamp é só do GIZMO — o render nunca o vê.** `computeFrustum` ganha
`visualFarCap` (default `kVisualFarCap` = 12 u): o retângulo do far, o
cone e a linha de visão desenham-se a `min(farZ, cap)`; o near fica real
(informativo); far curto fica real. A `gameProj` (a projeção do Play) lê
o `CameraComp` — **nunca** a estrutura do frustum — logo o render no Play
fica exatamente como na 0.7.7/0.7.9 com o far REAL. O `Frustum` ganha o
campo `drawFar` (o far efetivamente desenhado) para os testes e para o
discurso honesto do relatório.

**Hit-test e visual partilham a geometria — agora MENOS geometria.** O
`pickCameraTic` da 0.7.7 testava TODOS os segmentos do desenho (corpo,
lente, near, far, cone, linha de visão — "o hit-test e o visual partilham
a geometria ou nenhum dos dois presta"). O C33 provou o contrário do
lema: quando o visual é um cone GIGANTE, partilhar a geometria faz o
cone comer o viewport. O 0.7.10 restringe AMBOS ao que interessa: o
visual continua a desenhar o cone (informativo), mas o hit-test de
SELEÇÃO fica só no CORPO+LENTE (a caixa pequena, 22 px — o "macho" da
câmara). Os HANDLES continuam a partilhar a geometria do retângulo do
far — que agora está ao CAP (12 u): os handles acompanham o desenho
compacto e o `pickHandle` no far REAL devolve 0.

**Prioridade como função de consulta única**: `pickSceneTic(scene, vp,
sw, sh, px, py)` — (1) TICs visíveis com MeshRenderer pelo CENTRO
projetado a `kTicPickPx` (44 px — o mesmo alvo generoso do grab-lock; o
mais próximo do toque ganha); (2) só depois `pickCameraTic` (corpo/
lente). O main troca uma linha (o tap do viewport chama `pickSceneTic`);
a regra da prioridade vive num sítio só, pura e afervel.

**Toggle**: `CameraComp.showFrustum` (default true) — o `drawAll` salta a
câmara com o toggle off (a câmara continua na cena, ativa e a valer para
o Play; só o GIZMO desaparece). Inspector: linha `CamFrustum` (id 5412,
botão "frustum: sim|nao") a seguir a "ativa". Serialização:
`"frustum": false` gravado só quando off (default omitido — ficheiros
0.7.9 abrem limpos).

## 4. Implementação (por ficheiro, commit `5eb164a`)

- **`ui/CamGizmo.h/.cpp`** — `kVisualFarCap`/`kTicPickPx`;
  `computeFrustum(…, visualFarCap = kVisualFarCap)` com `drawFar` no
  struct; `pickCameraTic` RESTRITO a corpo+lente; `pickSceneTic` NOVO
  (prioridade: meshes por centro projetado → câmara corpo/lente);
  `drawAll` respeita `showFrustum`.
- **`components/CameraComp.h`** — `bool showFrustum = true` (comentário:
  o render no Play NÃO muda).
- **`core/SceneSerializer.cpp`** — grava `"frustum": false` quando off;
  lê no `fillCameraComp`.
- **`ui/EditorLayout.h`** — `kInspectorCamFrustum` 5412, kind
  `CamFrustum`, linha no plano (a seguir a CamActive), contagem 1+7.
- **`ui/EditorUi.cpp`** — payload do botão ("frustum: sim|nao") + tap
  re-despachado com o toggle (+filtro da linha).
- **`platform/main.cpp`** — o tap do viewport chama `pickSceneTic`
  (prioridade de objetos; log genérico "tic selecionado pelo toque").
- **`tests/test_cameratic.cpp`** — 4 testes novos + 3 contratos
  atualizados (ver §6).
- **`app/build.gradle`** — bump 0.7.10 / versionCode 30.
- **`.github/workflows/release.yml`** — artifact
  `goni-vv-0.7.10-release-signed`.
- **`README.md`** — escopo 0.7.10 + roteiro C33 (8 passos).

## 5. Decisões técnicas relevantes

1. **O clamp vive no `computeFrustum`, não no draw**: desenho, handles e
   (o que resta do) hit-test partilham a MESMA estrutura — nunca divergem.
   Um cap explícito maior devolve a geometria real (afervel).
2. **12 u de cap**: o corpo da câmara tem ~0.6 u e as cenas de trabalho
   vivem na escala 1-10 u — 12 u é generoso mas limitado; o C33 deixa de
   ver um frustum do tamanho do viewport com far 500/2000.
3. **O picker de meshes é MINIMAL de propósito** (centro projetado +
   alvo de 44 px, sem ray-cast): o critério de aceitação do dono era
   "toque num objeto dentro do cone seleciona o objeto" — a projeção do
   centro com alvo generoso resolve o caso real sem um módulo de
   ray-casting (que seria "feature nova" fora da CLÁUSULA CALMA). Dívida
   documentada: meshes muito largos selecionam-se pelo centro (tocar a
   borda longe do centro não apanha).
4. **O toggle esconde o GIZMO, não a câmara**: `showFrustum` NÃO toca em
   `active`/render — a semântica é a de um editor (esconder quando
   polui), não a de desativar o componente.
5. **O lema 0.7.7 ficou mais fino**: "o hit-test e o visual partilham a
   geometria" continua VÁLIDO para os handles (estão ao cap, como o
   desenho); para a SELEÇÃO a regra passa a ser "corpo/lente — o que o
   utilizador percebe como A câmara". O C33 ensinou que partilhar TUDO
   com um visual gigante é o bug.

## 6. Testes novos (CI Linux)

`tests/test_cameratic.cpp` (+4): **cap visual com far 2000** (gizmo a
12 — `drawFar`/`farCenter`; `gameProj` EXATAMENTE `perspective(…, 2000)`
— o render usa o far real; o Inspector vê 2000 no componente); **prioridade
de objetos** (cubo DENTRO do cone a 6 u → tocar nele seleciona o CUBO;
cubo invisível → o toque passa à câmara pelo corpo; nada → invalid [orbit
livre]; dois cubos → o mais próximo do toque); **handles ao cap**
(pickHandle no centro do far CLAMPADO → 5; no far REAL projetado → 0);
**toggle** (drawAll com `showFrustum=false` → 0 quads; serialização
`"frustum":false` gravado/volta com o far intacto; default NÃO gravado).

Contratos atualizados (3): **geometria** (far 8 → REAL com `drawFar`=8 e
cantos a tan(45°)·8; far 100 → CLAMPADO a 12 com near real; cap explícito
200 → geometria real; rodada com far 100 → clampado); **seleção** (far
500 — o caso C33: corpo e lente selecionam; CENTRO do far e ARESTA do
cone NÃO selecionam; longe → nada; invisível → nada); **plano do
Inspector** (linha `CamFrustum` contada; 8 linhas na secção da câmara).

## 7. Commits da sub-fase (branch main)

- `5eb164a` — 0.7.10-a: cap visual + hit-test restrito + pickSceneTic +
  toggle no Inspector + serialização + testes + bump 0.7.10/versionCode
  30 + README.

## 8. HEAD da sub-fase

`5eb164a` + este relatório (commit do relatório fecha a sub-fase).

## 9. Suíte de testes

**437 OK / 0 falhas** (433 → 437: +4 novos, 3 contratos atualizados ao
comportamento 0.7.10). Confirmado no hospedeiro no MESMO commit do push;
o run **36847607201** do CI passou a suíte (ctest 100%) + check_main +
link_parity (79 TUs) + jni_parity + JVM host + projects check.

## 10. CI

Run **36847607201** 100% verde: Testes do core (437 + checks),
build-release (APK assinado), verify-entry-symbols.

## 11. APK

`goni-vv-0.7.10-release-signed` (artifact 11154421798 do run 36847607201),
versionCode **30**, versionName **0.7.10**.
sha256 do APK assinado:
`ca11afd0d540fc60cec2b1d42ba3dbfd862ae12b77990d5d95e64e4580106b26`

## 12. Verificação no device (roteiro C33 — resumo)

Roteiro completo no README (secção 0.7.10, 8 passos): frustum compacto
com far 2000 (e real com far curto), tocar no cone não seleciona nem
bloqueia orbit, corpo seleciona, objeto dentro do cone ganha (prioridade),
handles ao cap (só com a câmara selecionada), toggle esconde o gizmo e
persiste, Play intocado (o clamp é visual), regressões 0.7.9/0.7.8/0.7.7.

## 13. Riscos e mitigações

- **Cap fixo 12 u vs cenas enormes**: uma câmara a 100 u de distância
  desenha um cone de 12 u pequeno no ecrã — correto (frustums de editor
  são para LEGIBILIDADE, não para medir o far; o valor vive no Inspector).
- **Picker por centro em meshes largas**: tocá-lo longe do centro não
  seleciona (o ray-cast ficaria para um ciclo futuro); a Hierarchy
  continua o caminho preciso de sempre.
- **Prioridade pode "roubar" da câmara**: um objeto à frente da câmara
  com o centro a <44 px do toque ganha — intencional (é o pedido do
  dono); a 22 px do CORPO da câmara sem objetos no meio, a câmara
  seleciona como sempre.

## 14. Dívida técnica conhecida

- O picker de meshes é por centro projetado (sem bounds/ray-cast) —
  meshes largas selecionam-se só perto do centro (documentado em §5.3).
- O cap é uma constante global (12 u) — um cap por câmara (ou adaptativo
  à distância da orbit) ficaria para um ciclo futuro se o dono pedir.
- O frustum de câmaras ORTO desenha o retângulo ao cap com meia-altura
  `orthoSize` — consistente, mas orthoSize grande + cap curto continua
  largo (é o orthoSize REAL a desenhar; correto).

## 15. Restrições respeitadas (CLÁUSULA CALMA)

Só frustum/hit-test da câmara + testes. Zero física nova, zero scripting,
zero features novas (o `pickSceneTic` é o hit-test pedido — prioridade de
objetos, sem ray-cast; o toggle é o pedido literal do dono). Toolbar,
layout da UI e projeção real da câmara no Play INTACTOS (`gameProj`
intocado — aferido contra `perspective(…, far REAL)`). Tema mono intacto.
Nenhum emoji. Bump 0.7.10 / versionCode 30.

## 16. Conclusão

O frustum da câmara fica DOMADO: compacto e legível em qualquer `far`
(desenha a 12 u; o far real vive no Inspector e no render do Play),
selecionável só pelo corpo/lente (o cone não rouba toques nem bloqueia o
orbit), com os handles ao cap e os OBJETOS a ganhar a prioridade do tap.
O Inspector ganha o toggle "frustum" para o esconder quando polui. Suíte
433→437; CI 100% verde com APK assinado. **As três sub-fases do trio
pós-0.7.7 fechadas: 0.7.8 (fronteira GL 3D↔UI) + 0.7.9 (grab-lock) +
0.7.10 (frustum domado), 418→437 testes (+19), 3 releases assinadas, 3
relatórios 1-16.** Prontas para o C33.
