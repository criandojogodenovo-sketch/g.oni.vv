# RELATÓRIO 0.8.3 — Blending (peso + crossfade walk→run)

Quarta e ÚLTIMA sub-fase da campanha F7. A 0.8.0 pôs a cena a mexer, a
0.8.1 importou clips glTF, a 0.8.2 deformou meshes com skins — esta MISTURA
duas animações com peso e faz a transição walk→run SEM salto.

## 1. Objetivo

(1) Blending de duas animações com peso (ex.: walk→run). (2) Crossfade
suave. (3) Testes: blend com peso 0.5 produz pose intermédia; crossfade
sem saltos.

## 2. Estado inicial (HEAD de entrada)

`e492d59` (0.8.2-a, suíte 497 OK; o fecho `8ba507b` correu verde no run
36870189932). APK 0.8.2 assinado (versionCode 33, sha256 `825798e8…`).

## 3. Arquitetura escolhida

**O blend é RUNTIME do player — dois clips, um peso.** `blendClip` +
`blendWeight` (0 = só o ativo, 1 = só o blend) + `blendTime` (o clip de
blend avança em paralelo, no SEU duration — crossfade walk→run com os
DOIS a andar). `setBlend` (manual, blendTime sincronizado ao tempo
corrente), `crossfade` (peso ANIMADO 0→1; destino arranca do 0; re-blend
troca o alvo), `stopBlend` (fica no ativo). NADA disto serializa — cena
carregada nasce sem blend (como `time`/`playing` desde a 0.8.0).

**O apply mistura por track correspondente.** Cada track do clip ATIVO
procura o par (mesmo alvo + elemento) no clip de blend e `blendKeys` ao
peso — TIC (pos/rot/escala), UI (pos/cor/alpha) e JOINTS misturam.
Joints SEM correspondência fade para o **TRS de BIND** (agora campos
próprios do joint — sem isto o "bind" era o valor ANIMADO pelo apply
anterior e o fade CONGELAVA: o teste re-setBlend apanhou o bug); TIC/UI
sem correspondência passam o valor do ativo (crossfade pressupõe os
mesmos alvos — rigs iguais; documentado).

**A troca NÃO salta — por construção.** Ao completar (peso 1), o clip de
destino passa a ser o ATIVO com o tempo QUE ELE JÁ TEM — a pose era dele
a peso 1, o frame seguinte é dele a peso 1 (a matemática é contínua).
O `advance` faz a troca com **early-return**: sem isto o `time += dt`
corria DEPOIS da troca e o tempo avançava DUAS VEZES no frame da troca —
o teste de continuidade (pose = 4t+4t², monotónica em todos os passos)
apanhou o salto.

**UI mínima, na timeline (CLÁUSULA CALMA).** Botão "fade" por clip no
seletor (crossfade 0.4 s; o tap continua a troca instantânea) + slider
contextual "blend N%" no header NO LUGAR do speed enquanto há blend —
arrastar assume o peso MANUAL (para o automático). O título mostra
"walk → run" durante a transição.

## 4. Implementação (por ficheiro, commit `1cc6a4d`)

- **`components/AnimationPlayer.h/.cpp`** — campos de blend + setBlend/
  crossfade/stopBlend/blending(); advance com o blend em paralelo e a
  troca por early-return; apply mistura por track correspondente com o
  fade-ao-bind dos joints.
- **`components/SkeletonComp.h`** — `bindPos/bindRot/bindScale` (o alvo
  do fade; o TRS animável continua a ser o que o apply escreve).
- **`assets/GltfAnim.cpp`** — o attach copia o TRS do nó para os campos
  de BIND também.
- **`core/SceneSerializer.cpp`** — Skeleton grava `bpos/brot/bscale`
  quando o bind difere do TRS atual (bind pose de 0.8.2 abre limpa);
  ausentes = o TRS carregado (mig. amigável).
- **`ui/Timeline.h/.cpp`** — ids 7007/7370+i; botão "fade" por clip;
  slider contextual de blend; título "A → B".
- **`tests/test_blend.cpp`** (NOVO, 11) — ver §6.
- **`app/build.gradle`** — bump 0.8.3 / versionCode 34.
- **`.github/workflows/release.yml`** — artifact
  `goni-vv-0.8.3-release-signed`.
- **`README.md`** — escopo 0.8.3 + roteiro C33 (6 passos).

## 5. Decisões técnicas relevantes

1. **blendKeys linear por canal** (a primitiva exposta desde a 0.8.0):
   eulers misturam linearmente — walk→run de rigs iguais fica correto;
   rotações compostas distorcem (a dívida euler da 0.8.0, conhecida).
2. **setBlend sincroniza blendTime = time**: os DOIS clips amostrados no
   MESMO instante no blend manual (o crossfade é que arranca do 0 — o
   destino "entra" do início, como um Godot AnimationNode).
3. **Fade-ao-bind dos joints sem par**: o membro "descansa" em vez de
   congelar no último valor do clip A — o que exigiu os TRS de BIND
   separados (§3; o bug era invisível no PRIMEIRO apply e só aparecia no
   re-blend — o teste cobre exatamente essa sequência).
4. **Slider contextual em vez de novo widget**: o header tem UM slot de
   modulação — speed quando não há blend, peso do blend quando há. Zero
   largura nova, zero sobreposição (CLÁUSULA CALMA).
5. **O crossfade só avança com playback** (gate `playing`, como tudo):
   fade sem Play fica a peso 0 à espera — coerente com o resto da engine.

## 6. Testes novos (CI Linux)

`tests/test_blend.cpp` (+11): **peso 0.5 = pose INTERMÉDIA** (walk x=4 +
run x=8 → 6); **extremos 0/1** + clamps de peso + clips inválidos/ele
próprio (estado mantém); **o clip de blend anda no SEU tempo** (duração
0.5 s: 1.2 s → blendTime 0.2 → x=6.4); **TUDO mistura** (rot 45° yaw,
scale 2, alpha 0.5, joint (1,2,3) ao peso 0.5); **crossfade peso LINEAR e
troca contínua** (passos 0.25 s: w=t exato, pose 4t+4t² monotónica, ao
completar activeClip=run com time contínuo, o frame SEGUINTE continua
sem salto); **duração mínima** (0 → 0.01, nunca divide por 0) + re-blend
troca o alvo; **stopBlend** fica no ativo (a pose volta ao clip puro);
**joint sem track fade ao BIND** (w=0.5 → (9,0,0)→(7,0,0) contra o bind
(5,0,0); RE-setBlend w=1 → (5,0,0) — o bug do bind animado apanhado);
**blend não serializa** (dump sem "blend"; round-trip nasce limpo);
**UI**: "fade" do seletor arranca o crossfade e COMPLETA com o playback
a correr (3 frames de 0.25 s > 0.4 s → run ativo); slider manual assume
o peso (blendDuration=0, alvo mantém-se).

## 7. Commits da sub-fase (branch main)

- `1cc6a4d` — 0.8.3-a: blending (campos+API no player, apply mistura por
  track, fade-ao-bind com TRS de BIND separados, crossfade com troca por
  early-return) + botão fade/slider contextual na timeline + 11 testes +
  bump 0.8.3/versionCode 34 + README.

## 8. HEAD da sub-fase

`1cc6a4d` + este relatório (commit do relatório fecha a sub-fase E a
campanha F7).

## 9. Suíte de testes

**508 OK / 0 falhas** (497 → 508: +11). Confirmado no hospedeiro no MESMO
commit do push; o run **36870275608** do CI passou a suíte (ctest 100%) +
check_main + link_parity (85 TUs) + jni_parity + JVM host + projects
check.

## 10. CI

Run **36870275608** 100% verde: Testes do core (508 + checks),
build-release (APK assinado), verify-entry-symbols.

## 11. APK

`goni-vv-0.8.3-release-signed` (artifact 11166543233 do run 36870275608),
versionCode **34**, versionName **0.8.3**.
sha256 do APK assinado:
`967bcce375c972279553147879179ca9a3ee61c0a65e75472d62433f2ea9425b`

## 12. Verificação no device (roteiro C33 — resumo)

Roteiro completo no README (secção 0.8.3, 6 passos): crossfade walk→run
pelo "fade" do seletor (título "walk → run", peso a subir no slider, sem
SALTO na troca); peso manual pelo slider (50% = pose intermédia); joints
misturam (mesh deforma intermédio; membros só-de-um-clip descansam no
bind); persistência limpa (blend é runtime); regressões 0.8.2/0.8.1/
0.8.0/0.7.x.

## 13. Riscos e mitigações

- **Euler linear no blend** (rot): misturas de rotações compostas podem
  tomar o caminho "curto" errado — a dívida euler conhecida; slerp por
  quat pede tracks quat (F8).
- **Crossfade pressupõe os mesmos alvos**: TIC/UI sem par passam o valor
  do ativo (documentado; rigs iguais — o caso walk→run — estão cobertos).
- **Blend de DOIS clips só** (A↔B): cadeias de blend (A→B→C) fazem
  crossfade encadeado (o novo crossfade troca o alvo) — sem árvore de
  estados (F8, se o dono pedir).

## 14. Dívida técnica conhecida

- Tracks quat (slerp) para rotações compostas — F8.
- Árvore de blend/estados de animação (além de A↔B) — F8.
- O slider contextual partilha o slot do speed (não há os dois ao mesmo
  tempo — decisão de espaço, documentada no §5.4).

## 15. Restrições respeitadas (CLÁUSULA CALMA)

Só animação (blending) + testes. Zero física, zero scripting, zero UI de
editor nova ALÉM da timeline (o botão "fade" e o slider contextual vivem
NA timeline/seletor de sempre; painéis/toolbar intocados). Tema mono;
nada sobreposto. Bump 0.8.3 / versionCode 34. Nenhum emoji.

## 16. Conclusão

Walk→run sem salto: o blend mistura os dois clips ao peso (TIC, UI e
joints; membros sem par descansam no BIND — que agora é dado próprio),
o crossfade anima o peso com os dois clips a andar e a troca é contínua
por construção (o destino herda o tempo dele; o early-return do advance
garante que o frame da troca não avança duas vezes — o teste apanhou-o).
Suíte 497→508; CI 100% verde com APK assinado.

**CAMPANHA F7 FECHADA — as quatro sub-fases com release próprio cada:**
- **0.8.0** — timeline + keyframes + AnimationPlayer (curvas linear/
  bezier, once/loop/pingpong, speed, UI anima) + 8 primitivas mesh
  procedurais (preset Mesh, seletor/params no Inspector);
- **0.8.1** — import de clips glTF (channels/samplers → clips nomeados,
  seletor na timeline, round-trip .goni);
- **0.8.2** — skinning esquelético (SkeletonComp, joints pais-primeiro,
  shader com uBones[64], tracks de joint, pose deforma o mesh);
- **0.8.3** — blending com peso + crossfade suave (fade-ao-bind, troca
  contínua).
437→**508 testes** (+71), 4 releases assinadas (versionCode 31-34), 4
relatórios 1-16 (`docs/RELATORIO-0.8.{0,1,2,3}.md`), roteiros C33
cumulativos no README. A engine ANIMA: TICs, UI e meshes skinados, do
editor (timeline com scrub/keys/preview sandbox) ao Play.
