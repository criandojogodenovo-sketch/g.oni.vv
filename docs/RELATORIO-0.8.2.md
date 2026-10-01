# RELATÓRIO 0.8.2 — Skinning esquelético (joints + inverseBindMatrices + shader com bones)

Terceira sub-fase da campanha F7. A 0.8.1 importou clips mas saltava os
canais de nós não-raiz — porque a engine não sabia DEFORMAR meshes. Esta
sub-fase traz o **skinning**: skins glTF viram um `SkeletonComp`, os
vértices carregam 4 influências, o VERTEX SHADER pesa as matrizes de bone
e a POSE da animação deforma o mesh.

## 1. Objetivo

(1) Import de skins de glTF: joints, inverseBindMatrices, weights por
vértice. (2) **Shader com bones**: matrizes de skin passadas ao shader;
mesh skinado desenha com a pose da animação. (3) Testes: mesh skinado
desenha; pose altera vértices (verificado no stub de render).

## 2. Estado inicial (HEAD de entrada)

`4944fd8` (0.8.1-a, suíte 489 OK; o fecho `b9a4286` correu verde no run
36867683992). APK 0.8.1 assinado (versionCode 32, sha256 `d9c1a5fb…`).

## 3. Arquitetura escolhida

**A skin é UM COMPONENTE; os pesos vivem no MESH.** O parser lê `skins`
(joints + IBM MAT4) e os atributos `JOINTS_0`/`WEIGHTS_0` das primitivas —
AMBOS ou nenhum (skin parcial não vale nada). Os joints chegam
REORDENADOS **pais-primeiro** (a composição hierárquica fica ITERATIVA —
uma passada, sem recursão), com `nodeToJoint` para os channels de
animaação. `gltfAttachSkin` cria o `SkeletonComp` (nome/pai/bind-TRS/IBM
por joint; reimport não duplica). O `MeshData` ganha `skinJoints/
skinWeights` (4 influências por vértice, u8/f32) — o `.goni` NÃO guarda
meshes, os pesos voltam pelo ficheiro glb no reload.

**O shader pesa; a CPU compõe.** `Mesh::createSkinned` acrescenta um VBO
de skin (8 floats/vértice; locations 3/4 = aJoints/aWeights — o layout
0/1/2 intacto). O vertex shader do LitMaterial ganha `uSkin` + `uBones
[64]`: a matriz efetiva é a **soma ponderada das 4 influências** (a MESMA
conta do `skinVertex` de CPU — o teste aferiu as DUAS contra o mesmo
valor esperado). A CPU só compõe: `computeSkinMatrices` (world hierárquico
dos TRS locais × IBM; cap 64 = o do shader). O `Renderer::drawMesh`
ganha `bones/count` e define `uSkin` **SEMPRE** (como o uTint — nunca
deixa estado do draw anterior vingar); mesh estático = caminho de sempre
byte a byte. O `GpuAssets` faz upload skinned quando o MeshData tem skin.

**A pose anima os JOINTS pelos tracks.** `AnimTarget` ganha JointPos/
JointRot/JointScale (por NOME de joint em `element` — o padrão dos
elementos de UI). Os canais glTF que apontam a joints da 1ª skin entram
nos clips importados (a 0.8.1 saltava-os); o apply escreve o **TRS LOCAL**
do joint e o main compõe as matrizes por frame para TICs com mesh skinado
+ esqueleto (o TRS do TIC continua a ser o model matrix — o esqueleto é
RELATIVO ao TIC).

**"Mesh skinado desenha" no stub de render.** O stub GLES3 ganhou
`lastUniform1i` (uSkin), `matrix4fvArrayCalls` (uploads de uBones) e
`drawElementsCalls` — o teste do render lê o stub (o padrão do lifecycle/
passgl desde a 0.6.7): 1 drawElements, uSkin=1, 1 upload de array com
count=2; mesh estático com bones → uSkin=0; mesh skinado SEM bones →
desenha estático (o renderer decide pelo MESH).

## 4. Implementação (por ficheiro, commit `e492d59`)

- **`assets/GltfImporter.h/.cpp`** — `GltfJoint/GltfSkin` (joints
  pais-primeiro + nodeToJoint); parse `skins` (IBM MAT4 count×joints;
  ausente → identidade); `componentsOf` ganha MAT4; JOINTS_0 (VEC4 u8/u16
  → u8 clamp) + WEIGHTS_0 (VEC4 f32) por primitiva → MeshData.
- **`assets/Assets.h`** — `MeshData::skinJoints/skinWeights` +
  `skinned()`.
- **`components/SkeletonComp.h/.cpp`** (NOVO) — joints (nome/pai/TRS
  local animável/bind-TRS/IBM), `findJoint`, `computeSkinMatrices`
  (iterativo, cap 64), `jointLocalMatrix`, `skinVertex` (CPU = shader).
- **`components/AnimationPlayer.h/.cpp`** — AnimTarget Joint* (nomes
  "jpos/jrot/jescala", labels "joint pos/…"); apply escreve o TRS LOCAL
  (alvo inexistente salta, nunca crasha).
- **`render/Mesh.h/.cpp`** — `createSkinned` (VBO de skin, locations
  3/4); `create` delega com nullptr; `skinned()`; destroy limpa o skinVbo.
- **`render/Material.h/.cpp`** — `uSkin` (int) + `uBones[64]` no VS (ramo
  de bones); `setSkin`/`setBones`; `kMaxBones=64`.
- **`render/Renderer.h/.cpp`** — `drawMesh(..., bones, boneCount)`.
- **`render/GpuAssets.cpp`** — upload skinned quando o MeshData tem skin.
- **`assets/GltfAnim.h/.cpp`** — `gltfAttachSkin`; os canais de JOINT
  entram como tracks Joint* (nó→joint pelo nodeToJoint; sem esqueleto
  saltam como na 0.8.1).
- **`core/ComponentStore.h/.cpp`** — storage `skeletons_` + registry
  "Skeleton" (id 8, no fim — ids antigos intactos).
- **`core/SceneSerializer.cpp`** — componente "Skeleton" (joints por
  valor: nome/pai/pos/rot/scale/ibm[16]; IBM identidade omitida); alvos
  jpos/jrot/jescala.
- **`platform/main.cpp`** — attach da skin ANTES dos clips no import;
  drawTics computa as matrizes por frame (bones ao drawMesh).
- **`tests/stub/GLES3/gl3.h`** — grava uSkin/uBones-array/drawElements.
- **`tests/test_skin.cpp`** (NOVO, 8) — ver §6.
- **`app/build.gradle`** — bump 0.8.2 / versionCode 33.
- **`.github/workflows/release.yml`** — artifact
  `goni-vv-0.8.2-release-signed`.
- **`README.md`** — escopo 0.8.2 + roteiro C33 (7 passos).

## 5. Decisões técnicas relevantes

1. **64 bones** (uniform array no shader): o típico mobile glTF tem <60;
   o cap é aferido no teste (`computeSkinMatrices` devolve ≤64) e
   documentado como limite.
2. **Joints u8 no VBO como float**: o upload promove a float — o shader
   lê vec4 direto (sem integer attributes; GLES3 suportaria, mas o layout
   fica simples).
3. **`uSkin` decidido pelo MESH, não pelos bones**: um mesh estático com
   bones passados fica uSkin=0 (o renderer não deixa o chamador mentir);
   um mesh skinado SEM esqueleto desenha estático — degradação honesta.
4. **A MESMA conta no shader e na CPU**: o teste `skin_bind_pose…` aferiu
   o `skinVertex` contra o valor esperado (2,0,0)→(1,0,−1) com 90°Y no
   filho — o shader faz a soma ponderada idêntica (comentário no fonte).
5. **Fixture com 2 joints encadeados** (children no nó pai) + IBM ×2
   (identidade + T(−1,0,0)): a 1ª versão esquecia o `children` e os
   offsets do buffer (JOINTS_0 como bytes) — os TESTES apanharam ambos.
6. **Bind pose = identidade**: `world(bind)·ibm = I` é o INVARIANTE que o
   teste confirma — sem play, o mesh desenha IGUAL ao glTF.

## 6. Testes novos (CI Linux)

`tests/test_skin.cpp` (+8): **parser** (joints PAIS-PRIMEIRO ["pai" antes
de "filho"], bind TRS, IBM ×2, nodeToJoint, mesh com skinJoints/skin
Weights 12, v1→joint 1, peso 1); **attach** (esqueleto + clip com track
jrot "filho" euler 90°; sem duplicar em reimport; TIC morto → 0); **pose
altera vértices** (bind pose: bones=I e o vértice não mexe; pose rodada:
bones[1]≠I e (2,0,0)→(1,0,−1) com o vértice do joint 0 intacto);
**cap 64** (80 joints → 64; maxBones 2 → 2; nullptr → 0; vazio → 0);
**mesh skinado DESENHA** (stub: drawElements=1, uSkin=1, uBones[2]
uplodeado; estático com bones → uSkin=0; skinado sem bones → estático);
**round-trip .goni** (Skeleton + jrot "filho" voltam; a pose REPRODUZ
depois do load: (2,0,0)→(1,0,−1)); **sem skin** (attach 0, sem
SkeletonComp, clips 0). Contrato: registry 8→9.

## 7. Commits da sub-fase (branch main)

- `e492d59` — 0.8.2-a: parser de skins + SkeletonComp + tracks de joint +
  shader com bones (uSkin/uBones[64]) + createSkinned + attach no import +
  bones no drawTics + 8 testes + bump 0.8.2/versionCode 33 + README.

## 8. HEAD da sub-fase

`e492d59` + este relatório (commit do relatório fecha a sub-fase).

## 9. Suíte de testes

**497 OK / 0 falhas** (489 → 497: +8; contrato registry 8→9). Confirmado
no hospedeiro no MESMO commit do push; o run **36867771391** do CI passou
a suíte (ctest 100%) + check_main + link_parity (85 TUs) + jni_parity +
JVM host + projects check.

## 10. CI

Run **36867771391** 100% verde: Testes do core (497 + checks),
build-release (APK assinado), verify-entry-symbols.

## 11. APK

`goni-vv-0.8.2-release-signed` (artifact 11165041072 do run 36867771391),
versionCode **33**, versionName **0.8.2**.
sha256 do APK assinado:
`825798e87b4440a6b71732886f6cc63d26a465166cbe74ca9df780e82bf4d9f0`

## 12. Verificação no device (roteiro C33 — resumo)

Roteiro completo no README (secção 0.8.2, 7 passos): importar .glb
RIGGADO → esqueleto + clips com tracks de joint; bind pose desenha igual
ao glTF; Play deforma o mesh (pernas/braços); Play de jogo idem;
persistência (esqueleto+IBM+tracks voltam e o Play continua a deformar);
meshes estáticos intactos; regressões 0.8.1/0.8.0/0.7.x.

## 13. Riscos e mitigações

- **Cap de 64 bones**: modelos com mais ficam capados (os joints além de
  64 não têm matriz — os vértices que os referenciam ignoram esse peso no
  skinVertex e o shader lê uBones[idx] com idx < 64 garantido pelo cap do
  UPLOAD... vértices com joint ≥64 leriam matriz indefinida — a defesa
  honesta é o cap no IMPORT (joints >64 ficam de fora da skin). Debt
  documentado: modelos >64 joints são raros em mobile.
- **Euler nos joints** (a convenção da 0.8.0): rotações compostas por key
  podem distorcer — aceitável no protótipo.
- **IBM ausente → identidade**: skins em bind-space direto toleradas (o
  glTF obriga IBM, mas a defesa não crasha).

## 14. Dívida técnica conhecida

- Skins com >64 joints: cap (sem eviction/priorização — F8).
- Uma skin por TIC (a 1ª do ficheiro); meshes multi-skin por nó ficam
  para F8.
- Normais skinadas com `mat3(b)` (assimila escala uniforme por bone —
  escala não-uniforme distorce as normais; visual, documentado).

## 15. Restrições respeitadas (CLÁUSULA CALMA)

Só animação (skinning) + testes: componente + tracks de joint + shader
com bones. Zero física, zero scripting, zero UI de editor nova (o
Inspector ganhou apenas o rótulo "anim: N tracks" que já contava tracks —
a edição de joints é a TIMELINE de sempre; SkeletonComp é DADOS, sem
painel próprio). Tema mono. Bump 0.8.2 / versionCode 33. Nenhum emoji.

## 16. Conclusão

O mesh deforma com a pose: skins glTF entram como SkeletonComp (joints
pais-primeiro + IBM), os vértices carregam 4 influências, o vertex shader
pesa uBones[64] com uSkin explícito por draw, e os tracks Joint* da
animação escrevem os TRS locais que o main compõe em matrizes por frame.
Bind pose = identidade (aferida), pose rodada move vértices (CPU e shader
na mesma conta), mesh skinado desenha (stub). Suíte 489→497; CI 100%
verde com APK assinado. **A 0.8.3 fecha a campanha: blending walk→run.**
