# RELATÓRIO 0.10.5 — 0.10-M EXT · IMPORT DE NÓS COMO SUB-ÁRVORE (o «expandir nós»)

> O fechamento do BACKLOG 0.10-M-ext do dono. A spec, verbatim: «opção no
> import "expandir nós" que cria um TIC por nó com mesh (nomes do glTF
> preservados, transformação do nó como Transform do TIC), em vez de
> fundir num TIC só. Default fundido por performance mobile; expandido
> para peças editáveis. Pin: city scene expandido = 72 TICs com nomes,
> culling por TIC verde.» Junto veio o 0.10-A: «até lá, a mensagem "pele
> ainda não suportada no streaming" mantém-se clara e com apontador para
> este BACKLOG».

## 1. A decisão de arquitetura (o que havia a escolher)

O import de glTF até aqui FUNDIA o modelo inteiro num único `.gmesh` e
aplicava-o a UM TIC. Para «um TIC por nó com mesh» havia dois caminhos:

1. **uma tabela de nós DENTRO do .gmesh fundido** (um ficheiro, ranges de
   blocos por TIC) — mudava o formato v3 (secção nova + leitores), o
   `BlockMesh` (draw por range), o `GpuAssets` (refs `#n<i>`) e o
   serializer/reload;
2. **uma PEÇA por nó** (um `.gmesh` por nó-com-mesh) — reusa o caminho
   INTEIRO vigente: o conversor de sempre (uma chamada por peça), as refs
   simples (`meshPath` é o ficheiro da peça — o serializer/reload de
   hoje abre-a sem UMA linha nova), o `blockHull` do PASSO 4 (a peça abre
   pela tabela) e o frustum por bloco com o model do TIC dono — que É o
   culling por TIC do pin.

Foi o 2.º: o custo é N ficheiros (o overhead por peça é 192 B de meta +
80 B por bloco + os nomes dos materiais — no city de 72 peças, ~30 KB) e
N aberturas por blocos em vez de 1; o ganho é ZERO superfície nova no
runtime de render (o `drawTics` não mudou UMA linha) e o default fundido
intocado. A troca é declarada: o expand é para EDITAR (peças), o fundido
continua a ser o caminho de performance.

A transformação é a parte fina. O engine compõe FLAT —
`TransformSystem::tick` não resolve pais (verificado no código: «sem
hierarquia de pais ainda… trabalho da fase de editor polish»). Logo:

- o registro `NodeOut` traz o TRS **MUNDO** do nó (a cadeia composta,
  decomposta em T/R/S) e a peça sai com a geometria **CRUA no espaço
  local** (bake identidade) — o TIC desenha no sítio exato SEM precisar
  de hierarquia, e «transformação do nó como Transform do TIC» fica
  literal;
- o SHEAR (escala não-uniforme do pai + rotação do filho — sem TRS exato)
  cai no bake do mundo (a peça mantém o nó → o bake de sempre) com TRS
  identidade no registro: o desenho é EXATO na mesma, o log DIZ, e a
  edição por TRS desses nós fica para nós sem shear (raro — o glTF é TRS
  por construção);
- a matriz que o driver decompõe vem da `worldChainOf` — a MESMA rotina
  em que o bake das tarefas se apoia (extraída do corpo do
  `convertGltfToV3` para UM só sítio): a igualdade entre o TRS do TIC e o
  bake da peça shear é garantida por construção, não por esperança.

## 2. A cura (item a item)

1. **A opção** — o toggle «expandir nós» nas Definições (secção Áudio, ao
   lado de «fonte após import»; `editor::settings::kToggleExpandNodes`),
   persistido por projeto em `settings.goni` (`expandNodes=0/1`).
   **Default OFF = o FUNDIDO de sempre.** O `importJobStart` captura o
   valor no LANÇAMENTO (o job corre com o que o dono escolheu) e o
   «reconverter assets» honra o MESMO setting.
2. **O driver das peças** — `convertGltfToV3Nodes` (GmeshV3Stream.cpp):
   agrupa as `primRefs` por nó (a ordem do array) e faz de cada
   nó-com-mesh UMA chamada completa ao `convertGltfToV3` — corte,
   assembly, verificação bit a bit POR PEÇA (a MESMA rotina do fundido;
   a verificação de cada peça lê o ficheiro dela e re-avalia o MESMO
   bake). Os nomes: `assets/<stem>_<nomeDoNó>.gmesh` (glTF sanitizado;
   anónimos `n<i>`; dedup determinístico `_<i>` quando o nome repete).
   O progresso entre peças é monótono (o ratchet `PieceProg` — o overlay
   nunca anda para trás). Um glTF SEM nós degenera no fundido com o log
   honesto (não há o que expandir).
3. **A flag** — `kGmeshV3FlagPiece` (bit1 do `flags` do meta v3;
   `convertGltfToV3` ganhou o param `metaFlags`, 0 no fundido). O
   `GpuAssets::mesh` abriu a condição do peek de 192 B: uma peça abre
   POR BLOCOS mesmo pequena — é isso que dá culling por TIC + lazy + LRU
   (o MESMO caminho do PASSO 4) ao apply da peça E ao reload da cena.
4. **O registro e os TICs** — `convert::Output::NodeOut`
   (nome/translation/rotation/scale/mesh/node) em `Output::expandNodes`,
   preenchido só pelo expand; `gltfExpandInstantiate` (GltfInstantiate.
   cpp, GL-free, CI-afervel) cria um TIC por registro: nome do glTF, o
   TRS no `Transform3D`, o `MeshRenderer` com a ref da peça (o binder
   injetado — no device `GpuAssets::blockHull`; nullptr não aborta, o
   meshPath fica para o reload). FLAT: sem pais entre os TICs (o registro
   JÁ traz o mundo; hierarquia entre TICs aplicaria a transformação duas
   vezes no futuro em que o engine passar a compor).
5. **O fecho no main** — o `importJobFinish`: com registros, a sub-árvore
   ENTRA na cena SEM o diálogo «aplicar ao TIC?» (os TICs novos são o
   resultado), com o fit único (a caixa do modelo inteiro em mundo — a
   união dos AABBs das peças transformados pelos TICs novos — define o
   fator do `kImportTargetSize`; `pos*=s` e `scale*=s` em cada TIC novo:
   a translação FORA do TRS encolhe o conjunto NO SÍTIO, o layout
   relativo não parte, e o resto da cena NÃO é tocado), o 1º TIC
   selecionado (o dono vê a sub-árvore no inspector) e o toast
   «expandido: N TIC(s) na cena».
6. **O 0.10-A apontado** — as DUAS cópias da mensagem (GltfImporter.cpp
   do parse integral e AssetConverter.cpp do merge) dizem «pele ainda
   não suportada no streaming (BACKLOG 0.10-A)» e nomeiam o que lá
   vive: ossos+pesos em blocos, skinning por GPU (bones em UBO/SSBO), os
   clips do glTF no import expandido. O BACKLOG.md ganhou a entrada
   0.10-A SKELETAL completa (absorve a antiga «PELE NO STREAMING»).

## 3. As linhas contrato novas (o que o dono vê no engine.log)

```
asset: v3 streaming '<peça>' — 1 tarefa(s), …      (por peça — o corte de sempre)
gmesh: v3 blocos=… verts=… tris=… verificado=1      (por peça — a verificação bit a bit)
gmesh: v3 expandir nós=72 peça(s) blocos=72 verts=72000 tris=43200 verificado=1
asset: expandir nós — o nó 37 ('x') tem SHEAR no mundo … (só quando há — nunca silencioso)
asset: expandir nós — 3 clip(s)/esqueleto ficam em assets/x.gm SEM attach (… o BACKLOG 0.10-A traz os clips)
import: EXPANDIDO — 72 TIC(s) criados (um por nó com mesh; nomes do glTF; culling por TIC pelos blocos)
```

## 4. As provas (verde) e as mutações (vermelho→verde)

**test_gmeshv3stream +5 TESTs `ext_*`:**

- `ext_uma_peça_por_nó_nomes_flag_e_geometria_igual_ao_fundido` — o city
  (72 nós): 72 peças `assets/city_n<i>.gmesh`, 72 registros com os nomes,
  a flag em TODAS, `verificado=1` ×72, a soma de verts == 72000 == ao
  fundido, e o CONTRASTE: o import sem o setting segue fundido (1
  ficheiro, sem registros, sem flag);
- `ext_transformação_do_nó_no_tic_e_geometria_crua` — pai (10,0,0) com
  filho (0,5,0): o registro do filho traz o TRS MUNDO (10,5,0) e a peça
  o AABB LOCAL [-1,1]³ (a rede 3×3×3 da fixture); a reconstrução T·R·S
  do registro == a matriz-mundo;
- `ext_shear_cai_no_bake_mundo_com_trs_identidade` — pai escala (1,3,1) +
  filho rot 45°Z: o registro do filho fica IDENTIDADE, o AABB da peça é o
  MUNDO [−√2,√2]×[−3√2,3√2]×[−1,1], o log diz «tem SHEAR»; o pai
  (TRS-exato) fica cru com a escala no registro;
- `ext_gltfexpandinstantiate_um_tic_por_nó` — o instantiate puro: nomes,
  TRS→Transform3D, meshPath por peça, o estrutural (mesh=-1) não vira
  TIC, o bind nullptr não aborta, o bind vivo liga mesh/material;
- `ext_a_peça_abre_por_blocos_no_gpuassets` — o GpuAssets com o glstub:
  a peça de 27 verts abre POR BLOCOS (BlockMesh no mapa); o fundido
  pequenino segue o mesh único SEM blocos — o CONTRASTE prova que é a
  FLAG que decide.

**c33 FASE 22 (+19 checks, 724→743) — O PINO:** o city ESPALHADO (72 nós
«c<i>» em faixas de 10 em 10, a do meio no 0; a rede 3×3×3 dá a cada
peça o AABB local exato [-1,1]³): (22.1) o import EXPANDIDO — 72 peças
com nomes, a flag em todas, a verificação POR PEÇA, a faixa de cada nó
no TRS do registro; (22.2) a sub-árvore pelo CAMINHO DO DEVICE (o binder
`blockHull` + `gltfExpandInstantiate`) — 72 TICs com os nomes «c0..c71»,
o hull no slot de cada um, as 72 peças abertas POR BLOCOS, o TIC 36 na
faixa 0 e o TIC 0 na −360; (22.3) o frustum estreito (fovY 0.5, D=20 —
meia-largura ~5.1) **culle 71 TICs e deixa 1 («c36»)** — o culling é POR
TIC, o pin do dono fecha.

**Mutações (cp dos backups, contra o código FINAL):**

- **M-E1** — o `expandNodes` ignorado em `convertGltfCommon` (tudo
  fundido): 5 TESTs `ext_*` VERMELHAS + a FASE 22 com 15 checks
  VERMELHOS (falha LIMPA — os ASSERTs de tamanho vão primeiro: sem UB,
  a lição do SAF-STREAM); reposta → verde.
- **M-E2** — o `-1` nunca posto nas primRefs da peça (o mundo baked E o
  TRS no TIC = transformação DUAS VEZES): `ext_transformação_do_nó…`
  VERMELHA (o AABB local deixa de ser [-1,1]) + `ext_shear…` VERMELHA;
  reposta → verde.
- **M-E3** — a flag nunca escrita (a peça sem culling):
  `ext_uma_peça…` + `ext_a_peça_abre…` VERMELHAS + o check 22.1 da flag
  VERMELHO; reposta → verde.
- **M-E4** — o nome do glTF morto (`n<i>` sempre): `ext_transformação…`
  VERMELHA + os checks de NOMES da FASE 22 VERMELHOS (22.1 os ficheiros
  e registros, 22.2 os TICs, 22.3 o nome do visível); reposta → verde.
- Final: **test_core 0 falhas · c33 743/743 HARNESS VERDE.**

## 5. Gates (no push deste commit)

scope-check (16 ficheiros, tudo em ci/scope.txt) · docs-lint (71
ficheiros) · gmesh-docs (símbolos e âncoras batem — §EXPANDIR-NÓS novo)
· ui-vocab (o toggle «expandir nós» é PT) · theme-hex · check_main
(main.cpp compila contra os stubs) · release-identity versionCode 60
(declarado no RELATORIO-0.9.6 §14).

**O CI do push 3c6a7d5 (run 37970116508): VERDE À PRIMEIRA** — Testes do
core (Linux) ✓ · C33 virtual (dispositivo + sentinelas + gates) ✓ com a
FASE 22 a 743/743 · APK release arm64 assinado ✓ (versionCode 60) ·
verify-entry-symbols ✓. O artefacto é o
`goni-vv-0.9.6-release-signed` — o APK que o dono instala para o
sign-off dos 3 itens do §8.

## 6. O que NÃO foi tocado

O caminho FUNDIDO inteiro (o `convertGltfToV3` só ganhou um param com
default 0 e a `worldChainOf` extraída do corpo — a composição é a MESMA,
as sentinelas R-039/R-040/R-041 e as FASEs 12.8/12.9/19/20/21 correram
verdes sem UMA recalibração); o `drawTics`/`Renderer`/`BlockMesh`
(zero linhas — o culling por TIC é o frustum por bloco de sempre, com o
model do TIC dono); o serializer (as refs das peças são `meshPath`
comuns — o reload abre-as pelo `GpuAssets::mesh` que agora sabe a flag);
a UI de seleção/Inspector (os TICs são TICs normais — o contrato do
picker segue o hull); V.ONI, física, áudio, browser (o import job
continua o mesmo, só o finalize ganhou o ramo expand).

## 7. As decisões honestas (o que o dono deve saber)

- **O expand não hierarquiza os TICs** (flat, TRS mundo por peça): o
  engine ainda não compõe pais; quando o «editor polish» compor, um
  follow-up pode re-linkar (a informação do pai vive no glTF em
  source/). Hierarquizar AGORA aplicaria a transformação duas vezes no
  futuro.
- **N peças = N BlockMesh = N caches LRU** (cada um com os orçamentos
  64+192 MB da casa): o pico TEÓRICO cresce com o número de TICs
  visíveis — no city (1000 verts/peça) é irrelevante; o expand é para
  EDIÇÃO, o fundido é o caminho de performance (o default).
- **Os clips de animação ficam SEM attach no expand** (os alvos de um
  clip são NÓS do glTF; o .gm continua escrito para o merge futuro — o
  log diz; o BACKLOG 0.10-A traz os clips).
- **Texturas embutidas não se auto-aplicam às peças** (o mesmo que o
  apply de um .gmesh fundido: as .gtext ficam no catálogo e aplicam-se
  por TIC; o toast do import continua a contar as texturas).
- **Reconverter com o setting trocado** reconverte no modo NOVO (o log
  do import diz qual; os TICs antigos apontam refs que o modo novo não
  escreveu — re-importar no modo desejado é o caminho limpo).

## 8. NÃO VERIFICADO (honesto — só o device humano afere)

1. **O city REAL de 130 MB** importado com «expandir nós» no C33: o
   count de TICs com nomes no hierarchy (a FASE 22 prova o mecanismo com
   a miniatura de 72 nós) e o log
   `import: EXPANDIDO — <N> TIC(s) criados` no device.
2. **O culling por TIC no orbit do device**: o HUD «FPS · TICs · bl n/m»
   a mudar com o orbit sobre o city expandido (a FASE 22 prova o
   veredicto por TIC com o audit de produção; o HUD do device é o mesmo
   caminho).
3. **O toggle nas Definições**: ligar «expandir nós», importar, ver os
   TICs; desligar, re-importar, ver o fundido de sempre (o toast muda:
   «importado: 1 mesh(es)» ↔ «expandido: N TIC(s) na cena»).

## 9. PÁRA

O PASSO 5 (texturas ASTC) continua BLOQUEADO à espera do OK explícito do
dono (depois o PASSO 6 — a prova de mesa no C33; após o 6, STOP absoluto
do 0.10-M). O BACKLOG 0.10-A (SKELETAL) ficou registado com a mensagem a
apontá-lo pelo nome — é decisão do dono quando o abre.
