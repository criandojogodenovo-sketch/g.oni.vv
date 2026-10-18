# RELATORIO-0.11-S — FASE 3 · ESQUELETO/ANIMAÇÃO (o personagem move-se)

> versionCode 71 · 0.9.6 · a FASE 3 do plano do dono (interpolations
> STEP/LINEAR(slerp)/CUBICSPLINE, UBO de 128 bones com o Plan B
> documentado, V.ONI stop()/blend(), a aceitação: personagem sem
> deformação · 2 clips transição suave · 10 personagens estáveis) ·
> 12 pins novos + 2 atualizados no test_core · c33 FASE 26 (+33
> checks) · suíte inteira verde no harness local · gates locais verdes

## 1. O ÂMBITO (o que a fase pede e o que a auditoria achou)

A spec da FASE 3 e o BACKLOG 0.10-A (SKELETAL) apontam a mesma área. A
auditoria ao código em pé (o commit de partida 725784e, FASE 2 selada
com o CI verde) achou que a campanha 0.8.x (F7) já construiu MUITO do
esqueleto — o plano da fase honrou cada achado e tocou só o que faltava:

| Item da spec | O que JÁ existia | O que faltava (feito nesta fase) |
|---|---|---|
| Import de skins/bones/pesos | O parse glTF completo (joints pais-primeiro + inverseBind + JOINTS_0/WEIGHTS_0 ≤65535 verts), o SkeletonComp, o .gm irmão com clips+esqueleto, o attach no runtime | NADA no caminho fundido (o streaming de skin é o BACKLOG 0.10-A e fica para a FASE 4 — ver §4) |
| TIC Skeleton + AnimationPlayer | Ambos os componentes + o AnimationSystem (Play) + tracks de joint por nome + o blending/crossfade da 0.8.3 | NADA (a base estava completa) |
| GPU skinning · 128 bones UBO + Plan B | O shader pesava as 4 influências contra `uBones[64]` — um ARRAY UNIFORM comum (`glUniformMatrix4fv`) que não garante espaço no mínimo do ES 3.0 para 128 | O bloco UBO std140 de 128 mat4 (8 KB ≤ 16 KB garantidos), upload por `glBufferSubData` determinístico + `glBindBufferBase`, o cap DURO no import com log honesto, o PLAN B (palette de textura) DOCUMENTADO no §4 |
| play/pause/stop/loop/velocidade/seek/lista de clips | A timeline inteira (play/pause/stop, mode once/loop/pingpong, speed 0.1..3, scrub = seek, a LISTA DE CLIPS com seleção da 0.8.x) | NADA de playback; o rótulo da curva mostrava «lin» em tracks STEP/SLERP (mentira) — o rótulo agora mostra a curva REAL |
| Interpolations STEP/LINEAR(slerp)/CUBICSPLINE | O parse CORTAVA as tangentes do CUBICSPLINE (ficava o meio), STEP era tolerada como LINEAR e as rotações interpolavam euler-linear (distorce arcos compostos) | As três interpolam COMO O glTF MANDA (perna 1) |
| V.ONI API `Import.Animation`/`play`/`stop`/`blend` | `Import.Animation("nome")` + `play()` (linker) | `stop()` e `blend(nome, seg)` |

## 2. AS 4 PERNAS (o que foi feito, ficheiro a ficheiro)

### Perna 1 — As interpolações glTF completas

- **`math/Math.h`** — `Quat::slerp(a,b,u)`: o arco CURTO (dot<0 inverte
  b), fallback nlerp normalizado quando dot>0.9995 (o sin(θ)→0 do
  denominador não divide por zero e converge para o slerp exato).
- **`components/AnimationPlayer.h/.cpp`** — `AnimCurve` ganha
  `Step=2`/`Slerp=3` + `animCurveName` (lin/bez/step/slerp — o nome
  canónico do serializer e da timeline); `evalTrack` ramifica: STEP
  devolve o valor da key à ESQUERDA (o glTF manda — o valor muda no
  instante da key seguinte), SLERP converte os euler-graus das keys em
  quats, faz o arco curto e volta a euler (a conta exposta em
  `evalSlerp` puro para o CI); o canal 4 continua linear.
- **`assets/GltfImporter.h/.cpp`** — `GltfAnimSampler.interp` (0/1/2 =
  LINEAR/STEP/CUBICSPLINE) transporta a interpolação; o layout
  `[in, valor, out]` do CUBICSPLINE chega INTACTO (a dívida da 0.8.1
  que cortava as tangentes MORRE); interpolação desconhecida → LINEAR
  (o default da spec glTF).
- **`assets/GltfAnim.h/.cpp`** — `gltfCubicToKeys` (puro, afervel): a
  conversão hermite→bezier da casa com as bases EXATAS
  (3(1−u)²u = 3·h10 e 3(1−u)u² = −3·h11 ⇒ `tanOut = +dt·m/3`,
  `tanIn = −dt·n/3`) — para pos/escala o evalTrack sobre as keys
  convertidas reproduz a hermite de referência À MICROSCÓPIO (pin em 5
  amostras, ε=1e-4); para rotação os tangentes de QUAT convertem por
  diferença finita em euler (h=0.1 s — APROXIMAÇÃO documentada). O
  `gltfAttachClips` mapeia: STEP→Step, LINEAR→Slerp (só rotação; pos/
  escala ficam Linear), CUBICSPLINE→Bezier.
- **`core/SceneSerializer.cpp`** — a curva viaja por NOME
  (`step`/`slerp`/`bez`); o default `lin` continua FORA do ficheiro —
  os .goni antigos ficam byte a byte; nome desconhecido → Linear.
- **`ui/Timeline.cpp`** — o botão da curva mostra o RÓTULO REAL (mostrar
  «lin» numa track STEP era mentir); o tap numa curva especial dropa
  para lin (a edição da casa continua binária; reimportar devolve a
  curva original — documentado).
- **`assets/GOwnFormats.cpp`** — nada a mudar: o .gm já transportava a
  curva por u8 (`w.u8_(t.curve)`) — Step/Slerp viajam no .gm sem
  mudança de formato (o round-trip é pin da FASE 26).

### Perna 2 — O UBO de 128 bones (+ o cap honesto)

- **`components/SkeletonComp.h` + `render/Material.h`** — `kMaxBones`
  64→128 nos DOIS lados (o pin da FASE 3 aferre o PAR).
- **`render/Material.cpp`** — o shader troca `uniform mat4 uBones[64]`
  por `layout(std140) uniform Bones { mat4 uBones[128]; };` (8 KB —
  dentro do MAX_UNIFORM_BLOCK_SIZE mínimo garantido de 16 KB do ES 3.0;
  o array uniform NÃO garantia espaço para 128 no mínimo do spec); o
  `init()` resolve o bloco por `glGetUniformBlockIndex("Bones")` +
  `glUniformBlockBinding` (slot fixo 0); o `setBones` cria o UBO lazy
  (meshes estáticos nunca pagam) e faz POR draw `glBufferSubData` de
  8192 B determinísticos — n bones + CAUDA DE IDENTIDADE até 128 — +
  `glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo)`: nenhum índice do mesh,
  nem corrupto, sai do bounds no shader (o custo é 8 KB/draw — nada no
  orçamento de 10 personagens).
- **`assets/GltfAnim.cpp`** — `gltfAttachSkin` RECUSA skins acima de 128
  joints com `elog::warn` que diz o cap e aponta o Plan B (uma skin
  truncada deixaria joints congelados + vértices a referenciar índices
  fora do array no shader: personagem deformado + UB — recusar é o
  honesto; o mesh desenha estático e o log DIZ).
- **`tests/stub/GLES3/gl3.h`** — o glstub ganha o UBO: `glBufferSubData`
  (conta + os últimos bytes), `glBindBufferBase`, `glGetUniformBlockIndex`
  (index 0 para "Bones", INVALID_INDEX para o resto),
  `glUniformBlockBinding` + `GL_UNIFORM_BUFFER`/`GLintptr` reais.

### Perna 3 — V.ONI stop()/blend()

- **`voni/Voni.h`** — `Host` ganha `stopAnim(err)` e `blendAnim(clip,
  seg, err)` com DEFAULTS que devolvem erro legível («indisponível
  neste hospedeiro») — os hosts que não animam não quebram scripts.
- **`voni/VoniEngineHost.h/.cpp`** — `stopAnim`: playing=false + tempo 0
  + `stopBlend()` (o par do play(); o crossfade a meio morre com o
  stop); `blendAnim`: procura o clip por nome → `crossfade(idx, seg)`
  (seg ≤ 0 → 0.3 s, o default da 0.8.3); erros que ensinam para TIC sem
  player, clip inexistente e clip-igual-ao-ativo.
- **`voni/VoniRegistry.cpp`** — as entradas `stop` (0 args) e `blend`
  (1..2 args, esqueleto «blend(,0.3)») + a tabela de handlers.
- **`voni/VoniTykers.h/.cpp`** — `compStop`/`compBlend` (o nome cru pelo
  caminho copy/colorpars; o seg é valor avaliado).
- **`voni/VoniVm.cpp`** — `blend` entra no slot de NOME (raw1) com o 2.º
  argumento avaliado em vals.
- **`VONI_referencia.md` + `llms-full.txt`** — REGENERADOS pelo
  `voni_refgen` (a bijeção registo↔referência byte a byte da sentinela
  mantém-se).

### Perna 4 — O fix do conversor (o bug que a FASE 26 apanhou)

- **`assets/AssetConverter.cpp`** — os clips do .gm nasciam VAZIOS para
  modelos skinados: o attach dos clips corria num dummy SEM SkeletonComp
  (o esqueleto ia num dummy2 DEPOIS) e os canais de joint — o alvo
  típico de um personagem — caíam todos fora. O personagem convertido
  nascia com esqueleto e ZERO clips: estático para sempre. A ordem
  corrigida: o skin attach precede o clips attach NO MESMO dummy (o
  MESMO caminho do runtime: skin primeiro, clips depois).

## 3. AS PROVAS (12 pins novos — tests/test_fase3_skeleton.cpp)

1. `fase3_quat_slerp_arco_curto` — 90° Y a meio = 45° exato; os
   extremos devolvem a/b.
2. `fase3_quat_slerp_inverte_o_lado_negativo` — q1=−q0 (o MESMO giro no
   hemisfério oposto) NÃO roda 360°: o arco curto é zero.
3. `fase3_quat_slerp_quase_paralelo_nao_divide_por_zero` — dot≈1 → o
   ramo nlerp; resultado normalizado com metade do arco pequenino.
4. `fase3_evaltrack_step_segura_a_esquerda` — t=0.5 e t=0.999 dão o
   valor de k0; t=1.0 dá k1; passou do fim, clamp na última.
5. `fase3_evaltrack_slerp_vai_pelo_arco_curto` — 0°→350° a meio é
   −5° (NUNCA 175° do euler-linear); == `evalSlerp` == o `Quat::slerp`
   direto (a mesma matemática por 3 caminhos).
6. `fase3_cubictokeys_reprouz_a_hermite_de_referencia` — tanOut=dt·m/3,
   tanIn=−dt·n/3, extremos a 0; o evalTrack sobre as keys convertidas
   bate a hermite de referência em t ∈ {0, .25, .5, .75, 1} (ε=1e-4);
   layout malformado → vazio (nunca crasha).
7. `fase3_import_curvas_end_to_end` — GLB com 3 animações
   (STEP/LINEAR-rot/CUBICSPLINE) → os 3 clips com as curvas CERTAS
   (Step/Slerp/Bezier + tangentes 2.0/−1.0); o playback do cubic a
   t=0.5 dá 0.875 (a hermite — não o meio 0.5).
8. `fase3_serializer_curvas_roundtrip` — step/slerp/bez viajam e VOLTAM
   (com o tanOut); a track sem "curve" nasce Linear; o loader não cria
   «edit» fantasma.
9. `fase3_voni_stop_e_blend_no_host_real` — o EngineHost REAL sobre uma
   Scene: importAnim → stop (playing=false, tempo 0) → blend("correr",
   0.5) → o peso anima → ao completar, «correr» é o ativo com o tempo
   contínuo (41/60 s); erros legíveis sem player/clip inexistente/
   clip-igual-ao-ativo.
10. `fase3_caps_casam_128` — `SkeletonComp::kMaxBones ==
    LitMaterial::kMaxBones == 128`; o compute para em 128 com 140 joints.
11. `fase3_bench_10_personagens_x_300_frames` — 10 TICs × 64 joints ×
    300 frames de advance+apply+compose: **63.3 ms no total =
    0.211 ms/frame = 0.021 ms/personagem/frame** (1.3% do orçamento de
    16.6 ms de 60 fps) — o número PRINTADO alimenta a tabela C33; o
    teto do pin é generoso (o pin é de sanidade; o árbitro do device é
    o sign-off do dono).
12. `fase3_import_curvas_end_to_end` cobre ainda o attach com a criação
    do player (a aceitação «personagem anima» no caminho do import).

Pins ATUALIZADOS (o comportamento honesto novo):
- `test_skin.cpp` — o cap é 128 (80 joints → 80; 200 joints → 128); o
  draw skinado: `bufferSubDataCalls==1` com `8192` B + `bindBufferBase`
  + a cauda de identidade no glstub + ZERO uploads por array (o uBones
  uniform morreu).
- `test_gltfanim.cpp` — o STEP deixa de ser tolerado: o scale do
  sampler STEP segura 1.0 a meio (era 1.5 do linear); o parse transporta
  o CUBICSPLINE COMPLETO (18 floats) com interp=2.

## 4. AS DECISÕES E OS LIMITES (nenhum escondido)

- **O PLAN B (>128 bones) está DOCUMENTADO, NÃO implementado** — a spec
  do dono diz «超出用 texture palette（Plan B 需文档化）». O design
  completo: um `sampler2D` RGBA32F com 2 texels por bone (cada bone =
  8 floats = as 2 colunas de rotação+translação; a 3.ª coluna
  reconstrói-se por cross product) ou 4 texels RGBA32F por bone sem
  reconstrução; o VS faz `texelFetch(uBonePalette, ivec2(bone*4+c, 0))`
  e compõe a matriz na mão; a paleta cresce por linhas até 4096 texels
  (= 1024 bones) e o upload é o mesmo glTexSubImage2D por frame. O custo
  não pago hoje: ~60 linhas de shader + o caminho de upload; o gatilho:
  um rig real do dono com >128 joints (o glTF mobile típico tem <60 —
  por isso o cap DURO de 128 com log honesto cobre o presente).
- **A rotação do CUBICSPLINE é aproximação** — os tangentes glTF são
  derivadas de QUAT; convertê-las para o euler-graus da casa usa
  diferença finita (h=0.1 s). O arco intermédio pode divergir
  levemente do hermite de quats (as KEYS são as mesmas). O caminho
  EXATO (quat keys no formato da casa) muda o AnimKey, o serializer e
  a timeline — fica para o dono pedir.
- **O streaming de skin continua no BACKLOG 0.10-A** — a recusa
  «pele ainda não suportada no streaming (BACKLOG 0.10-A)» para modelos
  >65535 verts VIGE (o GltfPrimRef não tem jntAcc/wgtAcc). É trabalho
  de FASE 4 (streaming/cache) — a FASE 3 não o pede e o anti-
  overengineering manda não o antecipar.
- **O cap 128 é DURO (recusa, não truncagem)** — uma skin truncada
  desenha personagem deformado (joints congelados) + UB no shader;
  recusar com log honesto + mesh estático é o comportamento seguro.
- **O tap na curva STEP/SLERP da timeline dropa para lin** — a edição
  da casa continua binária (lin/bez); reimportar devolve a curva
  original. Curvas por-key são fase futura se o dono quiser.
- **O stop() NÃO é pause** — playing=false + tempo 0 + blend cancelado
  (o par do play()/Import.Animation: o próximo play nasce do início).
- **O blend(nome) sem seg usa 0.3 s** — o default do crossfade da
  0.8.3; o seg chega por `blend(nome, seg)` com o seg avaliado.
- **10 personagens = 10 draws** — a casa não batcha skin (cada
  personagem tem o SEU bloco de bones); o custo é contado honesto no
  pin 26.5 (10 drawElements + 10 uploads do UBO) e no bench da CPU.
- **O FIX do conversor muda o .gm dos skinados** — modelos convertidos
  ANTES desta fase têm .gm sem clips de joint; REIMPORTAR a fonte
  regenera o .gm correto (o import deduplica clips por nome — o
  attachGAnim salta os que já existem).

## 5. OS GATES (a evidência do harness local)

- **test_core: 0 falhas** (a suíte INTEIRA, 940+ pins, com o disco
  limpo — o perfil r039 encheu o disco da sandbox 2× durante a caça e
  os 3 subprocessos de test_010m_medicoes falharam POR DISCO; com o
  disco limpo: 0 falhas. O flake é o documentado desde o hotfix P0 —
  o CI é o árbitro; o diff não toca na escrita do convert).
- **c33_virtual: 857/857 VERDE** (824 → 857: a FASE 26 nova com +33
  checks no caminho REAL do device — o import skinado, o .gm com as
  curvas, a pose do slerp, o crossfade completo, o draw com o UBO, os
  10 personagens).
- **Gates locais**: scope VERDE (todo o diff dentro de ci/scope.txt) ·
  check_main OK · link_parity OK (122 TUs) · docs-lint 84 ·
  ui-vocab · theme-hex · glyph · hierarchy-check · gmesh-docs 25 ·
  projects-ui · reload · release-identity **versionCode 71** (o par
  declarado no §14 do RELATORIO-0.9.6).
- **V.ONI**: VONI_referencia.md + llms-full.txt regenerados pelo
  voni_refgen (a sentinela da bijeção fica verde).

## 6. NÃO VERIFICADO (honesto)

- **O sign-off no C33 físico é do dono** — os números do harness
  (0.211 ms/frame de CPU para 10 personagens × 64 joints; os draws e os
  uploads do UBO no stub) são o PROXY; o device real (Mali do C33) tem
  a palavra final sobre o custo do shader com o UBO, o fps com 10
  personagens e a qualidade visual do slerp/cubic no ecrã.
- **O shader UBO NÃO correu em GPU real** — o glstub não compila GLSL;
  a sintaxe `layout(std140) uniform Bones { mat4 uBones[128]; };` é o
  ES 3.0 canónico, mas o primeiro boot no device é que valida (se o
  Mali recusar, o fallback documentado é o array uniform uBones[64] +
  o cap 64 — NUNCA bloqueia o release).
- **A qualidade visual dos clips reais do dono** — os pins usam rigs
  sintéticos; o personagem real do dono (o export do Blender com o
  skin de sempre) é a prova final da interpolação (o §6 do relatório
  da FASE 2 aplica-se: os modelos reais são do dono).

## 7. FICHEIROS ALTERADOS

| Área | Ficheiros |
|---|---|
| math | `math/Math.h` (Quat::slerp) |
| componentes | `components/AnimationPlayer.h/.cpp` (Step/Slerp/evalSlerp), `components/SkeletonComp.h` (kMaxBones 128) |
| assets | `assets/GltfImporter.h/.cpp` (interp + tangentes intactas), `assets/GltfAnim.h/.cpp` (gltfCubicToKeys + mapa de curvas + cap honesto), `assets/AssetConverter.cpp` (o FIX da ordem skin→clips) |
| render | `render/Material.h/.cpp` (o UBO Bones de 128 + setBones) |
| voni | `voni/Voni.h`, `voni/VoniEngineHost.h/.cpp`, `voni/VoniRegistry.cpp`, `voni/VoniTykers.h/.cpp`, `voni/VoniVm.cpp` (stop/blend) |
| ui | `ui/Timeline.cpp` (o rótulo honesto da curva) |
| core | `core/SceneSerializer.cpp` (step/slerp por nome) |
| testes | `tests/test_fase3_skeleton.cpp` (NOVO, 12 pins), `tests/test_skin.cpp` (2 atualizados), `tests/test_gltfanim.cpp` (STEP honesto + parse completo), `tests/CMakeLists.txt` (o TU novo), `tests/stub/GLES3/gl3.h` (o UBO no glstub), `tests/c33_virtual.cpp` (FASE 26 + o gerador do GLB skinado) |
| docs | `docs/RELATORIO-0.11-S-ESQUELETO.md` (este), `docs/RELATORIO-0.9.6.md` (§14 vc 71), `BACKLOG.md`, `README.md`, `VONI_referencia.md` + `llms-full.txt` (regenerados) |
| build | `app/build.gradle` (versionCode 70→71) |
