# RELATÓRIO 0.8.1 — Import de clips glTF (channels/samplers → clips nomeados)

Segunda sub-fase da campanha F7. A 0.8.0 pôs o sistema de animação a mexer
(timeline/keyframes/AnimationPlayer/primitivas); esta liga o formato da
casa ao formato do MUNDO: um `.glb` com animações importa os clips
NOMEADOS para o AnimationPlayer e o dono escolhe/reproduz na timeline.

## 1. Objetivo

(1) Parser de animações de glTF (channels/samplers) → clips nomeados no
AnimationPlayer. (2) Lista de clips; escolher clip ativo; playback do clip
importado. (3) Testes: importar `.glb` com animação → clip listado e
reproduz.

## 2. Estado inicial (HEAD de entrada)

`cf1abaf` (0.8.0-a, suíte 481 OK; o CI do commit de fecho `0a7853d`
correu 100% verde no run 36863755470). APK 0.8.0 assinado (versionCode
31, sha256 `d6159137…`).

## 3. Arquitetura escolhida

**O parser lê; o converter decide.** `GltfImporter` ganha `animations` —
cada uma com `channels` (nó alvo + path translation/rotation/scale +
sampler) e `samplers` (times do accessor `input` SCALAR f32, values do
`output` VEC3/VEC4). O parser fica GL-free puro (nada de player aqui);
`assets/GltfAnim::gltfAttachClips` é o tradutor: canais do NÓ RAIZ →
tracks TicPos/TicRot (quat→EULER GRAUS — a convenção do player desde a
0.8.0, lossy documentado)/TicScale; canais de OUTROS nós saltados (a
regra "um player por TIC" da 0.8.0; joints chegam na 0.8.2).

**Interpolação honesta e documentada.** LINEAR é a reproduzida. STEP é
tolerada como linear (dívida). CUBICSPLINE tem layout [in, valor, out] por
key — extraímos o VALOR do meio e seguimos linear (dívida documentada; a
curva já serializa como qualquer outra).

**O import usa o fluxo que existia.** O navegador de ficheiros já pergunta
"aplicar ao TIC?" — o Sim aplica o mesh E importa os clips no mesmo gesto
(ResourceManager ganha `model()`: o modelo inteiro pelo MESMO cache do
`mesh()` — 1 parse por ficheiro; o sufixo "#i" ignora-se). Reimportar
DUPLICA os clips (o dono apaga o que não quer — política explícita).

**O seletor de clips vive na timeline.** Botão "clip: <nome>" no header →
overlay CLIPS (nome + nº de tracks + ativo com `*`) + "novo (edit)"
(reutiliza um "edit" existente — não duplica). A timeline EDITA E REPRODUZ
o clip ATIVO (0.8.0 editava clips[0]); trocar de clip recolhe o tempo a
zero e limpa a seleção de key. Round-trip `.goni` pela serialização da
0.8.0 (intocada — os clips importados são dados como os outros).

## 4. Implementação (por ficheiro, commit `4944fd8`)

- **`assets/GltfImporter.h/.cpp`** — `GltfAnimation/GltfAnimChannel/
  GltfAnimSampler` no GltfModel; parse "animations" (input SCALAR f32,
  output VEC3/VEC4 f32; CUBICSPLINE → meio; STEP → linear tolerada);
  animações sem channels/samplers ficam de fora.
- **`assets/GltfAnim.h/.cpp`** (NOVO) — `gltfAttachClips`: clips nomeados
  no player (cria player+Transform3D se faltar); keys ordenadas; o 1º
  importado fica ATIVO quando já havia "edit"; TIC morto → 0 sem crash.
- **`assets/ResourceManager.h/.cpp`** — `model(ref, err)`: o modelo
  gltf/glb inteiro (sufixo #i ignorado) pelo cache do loadModel.
- **`ui/Timeline.h/.cpp`** — State.clipMenu; ids 7006/7350+i/7360; botão
  "clip:" + overlay CLIPS + "novo (edit)"; a timeline opera sobre o clip
  ATIVO (activeClipPtr com fallback editClip).
- **`platform/main.cpp`** — no "aplicar ao TIC?" de mesh gltf/glb:
  `g_resources.model()` → `gltfAttachClips` no TIC selecionado + toast
  "clips importados (timeline)" + log honesto.
- **`tests/test_gltfanim.cpp`** (NOVO, 8) — ver §6.
- **`app/build.gradle`** — bump 0.8.1 / versionCode 32.
- **`.github/workflows/release.yml`** — artifact
  `goni-vv-0.8.1-release-signed`.
- **`README.md`** — escopo 0.8.1 + roteiro C33 (8 passos).

## 5. Decisões técnicas relevantes

1. **Fixture GLB construída em código** com times/values reais e chunks
   JSON+BIN com padding INCLUÍDO no chunkLength — a convenção da casa
   (glb_com_chunks do test_import_gltf; a 1ª versão da fixture gravava o
   chunkLength sem padding e o parseGlb rejeitava "chunk truncado" —
   corrigido contra o exemplo existente).
2. **O clip importado fica ATIVO** quando já há "edit" (o dono acabou de
   o importar — é o que quer ver); em player novo o activeClip=0 é o 1º
   importado.
3. **`model()` partilha o cache com `mesh()`** — importar clips depois de
   aplicar o mesh NÃO re-lê o ficheiro (loadModel é o mesmo caminho).
4. **Trocar de clip recolhe o tempo a zero** — o playback de um clip
   nunca começa "a meio" por artefacto do anterior.
5. **Canais de nós não-raiz saltados (por agora)**: é a consequência
   honesta do "player por TIC" da 0.8.0 — a 0.8.2 (skinning) traz os
   joints e estes canais voltam a entrar como tracks de JOINT.

## 6. Testes novos (CI Linux)

`tests/test_gltfanim.cpp` (+8): **parser** (channels/samplers/times/
values/VEC4; CUBICSPLINE extraído o MEIO [6 values]; canal do nó 1 lido);
**attach** (clips nomeados no player; cria player+Transform3D; TIC
morto/inválido → 0 sem crash; clips ACRESCENTAM ao "edit" com o importado
ATIVO); **playback** (o TIC move-se: pos lerp 1 s→x=1, scale 1.5, rot
45° yaw no meio; loop 5 s→1); **round-trip .goni** do importado (nome/
tracks/keys euler 90°) E reproduz depois do load; **seletor de clips**
(abre; escolhe o clip 0 → activeClip=0 e tempo a 0; "novo (edit)"
REUTILIZA o existente; +track edita o clip ATIVO com o importado intacto);
**sem animações** (attach devolve 0 e o TIC fica sem player).

## 7. Commits da sub-fase (branch main)

- `4944fd8` — 0.8.1-a: parser de animações glTF + GltfAnim (clips
  nomeados) + ResourceManager::model + seletor de clips na timeline +
  import no "aplicar ao TIC?" + 8 testes + bump 0.8.1/versionCode 32 +
  README.

## 8. HEAD da sub-fase

`4944fd8` + este relatório (commit do relatório fecha a sub-fase).

## 9. Suíte de testes

**489 OK / 0 falhas** (481 → 489: +8). Confirmado no hospedeiro no MESMO
commit do push; o run **36865126581** do CI passou a suíte (ctest 100%) +
check_main + link_parity (84 TUs) + jni_parity + JVM host + projects
check.

## 10. CI

Run **36865126581** 100% verde: Testes do core (489 + checks),
build-release (APK assinado), verify-entry-symbols.

## 11. APK

`goni-vv-0.8.1-release-signed` (artifact 11163319612 do run 36865126581),
versionCode **32**, versionName **0.8.1**.
sha256 do APK assinado:
`d9c1a5fb514d37e4eb84ba51833c7590ae990690cbe936d3145a5e4180695b9d`

## 12. Verificação no device (roteiro C33 — resumo)

Roteiro completo no README (secção 0.8.1, 8 passos): importar .glb com
animação → toast "clips importados"; lista de clips no botão "clip:";
trocar de clip (timeline mostra os tracks DELE, tempo a 0); Play reproduz
o importado (timeline e Play de jogo); "novo (edit)" para editar sem
tocar nos importados; persistência (gravar/abrir → clips voltam e
reproduzem); .glb sem animação → comportamento 0.7.x; regressões
0.8.0/0.7.10/0.7.9.

## 13. Riscos e mitigações

- **Euler nas rotações importadas** (da 0.8.0): rotações compostas por
  key podem distorcer entre keys — aceitável no protótipo; a 0.8.2 usa a
  mesma convenção nos JOINTS (a 0.8.3 blending idem).
- **Reimportar duplica clips** — política explícita (apagar na mão);
  dedup por (nome+conteúdo) ficaria para F8.
- **CUBICSPLINE a linear** — a curva suave do ficheiro fica reta entre
  keys (o MEIO de cada bloco é o valor da key); documentado.

## 14. Dívida técnica conhecida

- Canais de nós não-raiz/não-joint saltados (0.8.2 traz os joints).
- STEP como linear (sem "degrau" entre keys).
- A lista de clips faz cap visual de 6 (sem scroll no overlay — F8).

## 15. Restrições respeitadas (CLÁUSULA CALMA)

Só animação + testes: o parser/converter/seletor são animação pura; zero
física, zero scripting, zero UI de editor nova ALÉM da timeline (o seletor
de clips é um overlay DA timeline; os painéis/toolbar intocados). Tema
mono; nada sobreposto. Bump 0.8.1 / versionCode 32. Nenhum emoji.

## 16. Conclusão

O `.glb` animado entra na engine pelo caminho curto: aplicar o mesh ao TIC
importa os clips com o mesmo gesto, a timeline lista/troca/edita-os e o
playback reproduz (com round-trip `.goni`). Suíte 481→489; CI 100% verde
com APK assinado. **A 0.8.2 traz o SKINNING — os joints destes clips
deixam de ser saltados e o mesh deforma com a pose.**
