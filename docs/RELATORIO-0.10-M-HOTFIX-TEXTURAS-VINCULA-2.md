# RELATORIO-0.10-M-HOTFIX-TEXTURAS-VINCULA-2.md

**0.10-M HOTFIX-2** — a spec do dono (o device revelou 2 bugs residuais do
vc 64, extensão correta .gtext): (1) o import cria vários .gtext mas só o
slot baseColor aparece preenchido no Inspector — normal, emissive,
metallicRoughness e occlusion ficam vazios; (2) o picker não aceita
preencher os 4 slots vazios — o applyAssetPick rejeita-os ou escreve no
mesmo slot. Cura nas 4 pernas pedidas. NADA mais tocado.

## 1. A causa raiz (ficheiro + função, as 4 probabilidades julgadas)

| # | Causa | Veredicto | Onde vivia |
|---|---|---|---|
| a | o write do bloco GTB1 grava só o primeiro bind | **PARCIAL** — o `gmeshV3AppendBinds` (GOwnFormats.cpp:459) SEMPRE iterou os 5 slots (`kBindSlotCount`); o gap estava ACIMA dele: o parse só lia 2 dos 5 slots, logo o mapa que alimenta o GTB1 nunca tinha mais | `parseGltf` (GltfImporter.cpp:645-684) lia SÓ `baseColorTexture` + `normalTexture`; `GltfMaterial` (GltfImporter.h:57) NÃO TINHA campos para metallicRoughness/emissive/occlusion |
| b | o read no runtime só lê o primeiro bind | **CONFIRMADA** — `gmeshV3ReadBinds` lê os 5 slots por entrada, mas o ÚNICO consumidor devolvia só o baseColor | `GpuAssets::baseColorBindOf` (GpuAssets.cpp:252) devolvia `b.tex[kBindBaseColor]` e nada mais; o auto-bind do apply (EditorUi.cpp `applyAssetPick` ramo do mesh) escrevia só `mr->texPath` (slot 0) |
| c | o applyAssetPick tem hardcode do slot baseColor e rejeita os outros 4 | **NEGATIVA** — o `applyAssetPick` (EditorUi.cpp:3713) já recebia `texSlot` e escrevia `texSlotRef(slot)` (o teste audit_pick provava); a rejeição vivia ANTES dele | ver (d) |
| d | o Inspector mostra os 5 slots mas o id do pick é o mesmo para todos | **CONFIRMADA (PIOR)** — os ids eram DISTINTOS (kInspectorTexSlot0+s), mas o ramo que os lê era **CÓDIGO MORTO** | o dispatch do tap (EditorUi.cpp:2119-2121) listava SÓ MeshButton/TexButton/PrimButton na condição externa — o `else if (TexSlotRow)` (linha 2134) NUNCA corria; o tap nas 5 linhas não fazia NADA e o único caminho para o browser era a linha clássica `tex:` que arma `inspTexSlot = 0` — daí «escreve no mesmo slot» no device |

**Resumo do (1)**: o .gtext nascia (o passe de texturas escreve uma
imagem), mas a CADEIA perdia 3 slots em 3 sítios — parse (só 2 campos) →
mapa (só 2 slots) → auto-bind (só baseColor). O inspector só podia
mostrar o que o auto-bind escreveu: o slot 0.

## 2. A cura (as 4 pernas da spec)

1. **writeGMesh iterar os 5 slots** — `gmeshV3AppendBinds` JÁ iterava
   (verificado, sem mudança); a cura real ficou no upstream: `GltfMaterial`
   ganhou `metalRoughTex`/`emissiveTex`/`occlusionTex` (-1 default) e o
   `parseGltf` lê `pbrMetallicRoughness.metallicRoughnessTexture`,
   `emissiveTexture` e `occlusionTexture` pelo MESMO mapa `textureSources`
   → IMAGEM. O builder do `MatTexMap` (AssetConverter.cpp convertGltfCommon)
   enche os 5 slots com o MESMO gate `imgViable` e a MESMA regra de naming
   `assets/<stem>_<imgIdx>.gtext` — o GTB1 e o log por slot do
   GmeshV3Stream (que já eram de 5) passam a RECEBER os 5.
2. **os leitores lerem os 5 binds** — `gmeshV3ReadBinds` JÁ lia (sem
   mudança); a cura real ficou no runtime: **`GpuAssets::matBindsOf(ref,
   materialIndex, out[5])` NOVO** — o MESMO caminho ligeiro (peek 192 B +
   statBytes + faixa dos materiais, a disciplina R-044) devolvendo os
   CINCO slots; o `baseColorBindOf` vira wrapper (contrato byte a byte).
   O `AssetResolvers::matBinds` (ponteiro de função, null = caminho de
   sempre) liga-o ao main.cpp; o auto-bind do `applyAssetPick` (ramo do
   mesh) limpa os 5 slots, enche o slot 0 (embutida → GTB1 → fallback
   baseColorBind, a ordem de sempre) e escreve CADA slot não-vazio do
   GTB1 na sua linha; o import EXPANDIDO (main.cpp, pós
   `gltfExpandInstantiate`) enche os 5 slots de CADA peça recém-nascida
   pelo matBindsOf do meshPath dela. A TROCA de mesh leva os slots atrás
   (o vestígio do mesh anterior morre — clear+fill honesto).
3. **applyAssetPick aceitar qualquer slot** — JÁ aceitava (o parâmetro
   `texSlot` do hotfix 1); o pin da auditoria mantém-se verde. A
   identidade do alvo fica: TIC selecionado + o slot ARMADO pela linha
   (o materialIdx é o do conjunto do TIC — o render multi-material
   continua no blockHull por bloco, a fronteira documentada no hotfix 1).
4. **Inspector: 5 linhas com ids distintos** — os ids JÁ eram distintos;
   a cura é o UMA-LINHA do (d): `TexSlotRow` entra na condição externa do
   dispatch (EditorUi.cpp:2119-2122) — o ramo existente deixa de estar
   morto; o dispatch lê `r.id - kInspectorTexSlot0` e passa o slot ao
   pick (o caminho main → applyAssetPick já passava `g_editor.inspTexSlot`).

## 3. As provas (os pins novos, test_texbind.cpp + test_ui.cpp)

- `bind2_import_5_slots_matriz_log_e_matbindsof` — glTF com 1 material e
  5 imagens (baseColor+normal+metallicRoughness+emissive+occlusion):
  import → `stats.textures == 5`, OS 5 slots no GTB1 com os caminhos
  certos, OS 5 ficheiros existem, **5 linhas de log** «textura: vinculada
  material 'mat0' slot <nome> -> assets/modelo_<i>.gtext», e o
  `matBindsOf` REAL sobre o FsStorage devolve os 5 (+ material fora →
  false; o wrapper baseColorBindOf mantém o contrato).
- `bind2_autobind_5_slots_no_apply_e_troca_de_mesh` — o apply do mesh
  enche OS 5 slots do TIC (texPath+texture no 0; normal/metalRough/
  emissive/occlusion nos 1-4); a troca para um mesh com menos binds
  LIMPA os que sobram (nenhum vestígio); mesh sem GTB1 → slots limpos.
- `ui_tap_na_linha_de_slot_arma_o_slot` (test_ui.cpp, UI REAL com gestos
  injetados) — tap na linha do slot 1 arma `inspTexSlot == 1` + abre o
  seletor; slot 3 arma 3 (o id É a identidade); o pick com o slot armado
  escreve o emissive e o baseColor/normal ficam INTACTOS (a cadeia
  COMPLETA linha → inspTexSlot → applyAssetPick).
- Os pins do hotfix 1 mantêm-se verdes (retro-compat, 4 materiais = 4
  bindings, 50 sintéticas, auditoria de irmãos).

## 4. As mutações (vermelho → verde)

| Mutação | O que morre | Prova |
|---|---|---|
| M-TB2a «write só 1 slot» | o builder do mapa enche SÓ o baseColor (os 3 slots forçados a "") | `bind2_import_5_slots…` VERMELHA: 3× slot errado no GTB1 + contaSlots≠5 + 3× log em falta (8 asserts) — **reposta: verde** |
| M-TB2b «pick rejeita não-base» | `applyAssetPick` força `slot = 0` (o comportamento do device) | `ui_tap_na_linha…` VERMELHA (o emissive não escreve; o baseColor é tocado) + a família `audit_pick…` com 8 testes vermelhos — **reposta: verde** |

(A mutação da causa (d) em si — tirar o `TexSlotRow` da condição externa —
é o estado ANTERIOR documentado na §1; o pin do tap nasce VERMELHO nela
por construção: `assetMenu` fica 0.)

## 5. Os gates + suítes

- test_core: **935/0** (932 + 3 pins novos; full rebuild limpo — a lição
  do incremental SEM rastreio de includes morde de novo: os TUs que
  incluem main.cpp/EditorUi.h têm de ser tocados à mão). Duas flakies de
  AMBIENTE desta sandbox (blockmesh_pin RAM + handshake por ordem)
  exoneradas por controlo: saltando os 3 pins novos elas continuam (e o
  mesmo binary passou 0/0 na corrida mais limpa) — o CI é o árbitro.
- c33_virtual: **797/797** HARNESS VERDE.
- Gates: scope VERDE (o diff todo dentro de ci/scope.txt) · docs-lint 77
  · gmesh-docs 25 âncoras (o formato NÃO muda — o GTB1 já era de 5) ·
  ui-vocab · theme · hierarchy · gizmo · projects-ui · reload · glyph ·
  jni-parity · check_main · link-parity 122 TUs · release-identity
  **versionCode 65** (declarado no RELATORIO-0.9.6 §14).

## 5-A. O CI (o selo)

- **CI VERDE À PRIMEIRA — run 38250308996 (02c8614)**: core-tests SUCCESS
  (12:38→12:44) · c33 SUCCESS (12:38→12:40) · APK release arm64 assinado
  SUCCESS (12:44→12:47) · verify-entry-symbols SUCCESS — o fix -j2 do
  paralelismo continua a segurar (4 runs verdes seguidos desde o 218).
  Artefacto: **goni-vv-0.9.6-release-signed** 1,7 MB (versionCode 65,
  id 11747799810) — o APK que o dono instala para o sign-off no device.

## 6. NÃO VERIFICADO (honesto)

- O GL dos slots 1-4 continua à espera dos samplers (o lit de hoje tem só
  o unit 0) — os slots 1-4 ficam VINCULADOS (refs + inspector + log); o
  carregamento para GL é o mesmo salto do hotfix 1.
- O sign-off no DEVICE (o dono importa o modelo de 4-5 texturas e vê as
  5 linhas com slot+nome no engine.log + os 5 slots cheios no Inspector +
  o picker a escrever cada slot sem tocar nos irmãos).
- O modelo com 4 MATERIAIS × 5 slots: o inspector edita o conjunto do
  TIC (o material 0 do auto-bind); a edição por material no Inspector é
  a próxima perna se o dono a quiser (o render por material já é do
  blockHull).
- .gmesh antigos (pré-GTB1 ou GTB1 a 2 slots): continuam a abrir; os
  slots novos só nascem do REIMPORT (a migração é reimportar).
