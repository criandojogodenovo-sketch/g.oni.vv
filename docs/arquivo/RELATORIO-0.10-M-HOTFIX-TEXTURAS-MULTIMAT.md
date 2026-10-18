# RELATORIO-0.10-M-HOTFIX-TEXTURAS-MULTIMAT.md

**0.10-M HOTFIX-3** — a spec do dono (o vc 65 no device revelou 2 gaps):
(1) HONESTIDADE DE SLOTS — os 4 slots não-baseColor aparecem preenchidos no
inspector mas o shader lit só amostra o baseColor («aparecem mas não
influenciam»); (2) MULTI-MATERIAL — modelo com 15 texturas = N materiais
(cada primitiva tem o seu) e o inspector mostrava SÓ o material 0. Cura nas
2 pernas pedidas. NADA mais tocado.

## 1. A causa raiz (ficheiro + função)

| # | Bug | Causa | Onde vivia |
|---|---|---|---|
| 1 | slots não-base «aparecem mas não renderizam» | NÃO é bug de vínculo — o lit de hoje tem SÓ o unit 0 (a fronteira documentada nos hotfixes 1-2). O bug é de HONESTIDADE: o Inspector mostrava a ref como se renderizasse, sem dizer que a renderização espera a 0.11-L | os rótulos dos 5 slots construídos inline em `drawInspector` (EditorUi.cpp:1194) — sem nenhuma marca do estado real do shader |
| 2 | inspector mostra só o material 0 | o `MeshRenderer` (MeshRenderer.h) só tinha CINCO campos de textura (o conjunto do material 0); os materiais 1..N-1 viviam SÓ no GTB1 do .gmesh (o render por bloco já os usava) e a UI não tinha nem os dados nem o seletor | `MeshRenderer::texSlot/texSlotRef` (MeshRenderer.h:78-98); o ramo de texturas do `applyAssetPick` escrevia sempre o material 0 (EditorUi.cpp:3755) |

**Complemento**: o auto-bind do apply/da troca de mesh lia SÓ o material 0
do GTB1 (`res.matBinds(rel, 0, binds)`) — nem os dados dos outros
materiais chegavam ao TIC, logo nem o seletor os podia mostrar.

## 2. A cura (as 2 pernas da spec)

1. **HONESTIDADE DE SLOTS** — o sufixo honesto «(vinculado · render na
   0.11-L)» nas linhas dos 4 slots não-baseColor PREENCHIDOS; o baseColor
   mantém-se SEM sufixo (ele renderiza de verdade). A política vive numa
   função PURA afervel aos pins: `texSlotRenders(slot)` + `inspTexSlotLabel`
   (EditorLayout.h — a FONTE ÚNICA usada pelo draw e pelos testes; a
   mutação do sufixo morre AQUI). O `·` é U+00B7 (Latin-1, NO atlas); o
   draw pré-trunca o NOME da textura (textfit, o padrão do button) para o
   SUFIXO ficar visível mesmo no painel estreito do device — a honestidade
   nunca sai do ecrã. Quando a 0.11-L trouxer os samplers 1-4, a mesma
   função muda e os rótulos acompanham.
2. **MULTI-MATERIAL** — a grade INTEIRA dos materiais entra no TIC:
   - `MeshRenderer::matExtraTex` (vector achatado, (N-1)×5 refs na ordem
     kBind*) + `matCount()`/`matTex(mat,slot)`/`matTexRef(mat,slot)`/
     `setMatCount(n)` (MeshRenderer.h) — zero I/O por frame (a contagem
     deriva da grade, a lei R-044);
   - o auto-bind do apply/da troca de mesh SONDA o GTB1 inteiro
     (materiais 0..N-1, teto de sonda 64 — um resolver maluco não gira):
     o material 0 no caminho de sempre, os extras na grade; a troca de
     mesh/none/cube leva a grade atrás (o vestígio morre);
   - o import EXPANDIDO (main.cpp) enche a grade de cada peça (um nó do
     glTF com várias primitivas = peça multi-material);
   - o seletor «material: i/N» (linha NOVA `MatSelRow`, id 5311, SÓ quando
     N>1) com as setas ‹ › VETORIAIS (ícone `ChevronLeft` novo — o atlas
     não tem U+2039/U+203A): o tap no TERÇO esquerdo/direito troca dentro
     de [0, N-1]; o meio da linha não faz nada;
   - as 5 linhas de slot mostram O MATERIAL SELECIONADO
     (`mr->matTex(st.inspMatIdx, s)` com clamp honesto); o `EditorState::
     inspMatIdx` repõe-se a 0 na troca de TIC (o ponto do inspPrevSelected);
   - o pick escreve NO MATERIAL SELECIONADO: `applyAssetPick` ganha
     `materialIndex` (default 0 = o contrato byte a byte dos chamadores
     antigos) — clamp à grade real, nunca material fantasma; material >0
     não toca o ponteiro GL (o baseColor do 0 é o que o lit amostra) e a
     linha de log diz «material: material <i> slot <nome> = …»;
   - persistência: o serializer grava/lê «matExtraTex» (array achatado,
     ausente = 1 material, floor a múltiplo de 5 — ficheiro cortado não
     cria material fantasma) e o undo snapshot/repõe/compara a grade.

## 3. As provas (os pins novos, test_texbind.cpp + test_ui.cpp)

- `multimat_3_materiais_import_seletor_e_slots` — glTF fundido com 3
  materiais × 5 slots (15 imagens): import → `stats.textures == 15`, o
  GTB1 com 3 bindings × 5 slots; o apply com o `matBindsOf` REAL sobre o
  FsStorage enche a grade (`matCount() == 3`, as 15 refs certas); o
  perfil diz 3 e o plano ganha a linha do seletor (e mantém as 5 de slot);
  trocar para um mesh sem GTB1 → `matCount() == 1` e SEM a linha.
- `multimat_pick_no_material_2_nao_toca_no_0` — o pick no material 2
  (slot emissive) muda SÓ o 2; o material 0 INTACTO (o pin do dono); o
  none no material 1 limpa SÓ esse; o clamp honesto (materialIndex 7 → o
  último); mesh de 1 material → clamp a 0 (o caminho de sempre); a linha
  de log diz o material.
- `multimat_sufixo_honesto_no_rotulo` — baseColor preenchido SEM sufixo;
  normal/emissive preenchidos COM «(vinculado · render na 0.11-L)»; slot
  vazio sem sufixo; a fronteira é a função (`texSlotRenders`).
- `ui_seletor_de_material_troca_e_o_pick_escreve_no_material` (test_ui.cpp,
  UI REAL com gestos injetados) — a grade de 3 materiais → o seletor entra
  no plano; o tap no terço › arma 1, de novo arma 2, ‹ volta a 1, o MEIO
  da linha não mexe; o tap na linha do slot 3 arma o slot 3; o pick
  (material 1, slot 3) escreve NO MATERIAL 1 — o material 0 INTACTO (a
  cadeia COMPLETA seletor → inspMatIdx/inspTexSlot → applyAssetPick).
- Os pins dos hotfixes 1-2 mantêm-se verdes (os 5 slots, a auditoria de
  irmãos, o tap nas linhas).

## 4. As mutações (vermelho → verde)

| Mutação | O que morre | Prova |
|---|---|---|
| M-MM1 «seletor hardcoded material 0» | o dispatch do `MatSelRow` não muda o índice (o tap nas setas é morto — o pick fica no material 0) | `ui_seletor_de_material…` VERMELHA: `inspMatIdx` fica 0 (2 asserts) e o pick escreveria no 0 — **reposta: verde** |
| M-MM2 «sufixo removido» | `texSlotRenders` devolve `true` sempre (o sufixo nunca entra) | `multimat_sufixo_honesto_no_rotulo` VERMELHA: 2 asserts (o prefixo e o sufixo em falta) — **reposta: verde** |

## 5. Os gates + suítes

- test_core: **944/945** (940 + 4 pins novos) — o 1 vermelho é a
  `sentinela_r039` de SEMPRE desta sandbox (errno 28 DISCO CHEIO na
  escrita de 1,75 GB + ~3 GB do gmesh de 60M verts; correu verde na
  corrida tc2 com o MESMO binário quando o disco estava limpo — o caminho
  do diff não toca na escrita do convert). O CI é o árbitro.
- c33_virtual: **797/797** HARNESS VERDE.
- Gates: scope VERDE · docs-lint 79 · gmesh-docs 25 (o formato NÃO muda) ·
  ui-vocab · theme · hierarchy · gizmo · projects-ui · reload · glyph ·
  jni-parity · check_main · link-parity 122 TUs · release-identity
  **versionCode 66** (declarado no RELATORIO-0.9.6 §14).

## 5-A. O CI (o selo)

- **CI VERDE À PRIMEIRA — run 38320152995 (b2f63e4)**: core-tests SUCCESS
  · c33 SUCCESS · APK release arm64 assinado SUCCESS · verify-entry-symbols
  SUCCESS — o r039 passa no runner (o flake é da sandbox, confirmado).
  Artefacto: **goni-vv-0.9.6-release-signed** 1,7 MB (versionCode 66, id
  11779463544 pertence ao P0; o 66 correu neste run) — o APK para o
  sign-off no device.

## 6. NÃO VERIFICADO (honesto)

- O sign-off no DEVICE (o dono importa um modelo multi-material e vê o
  «material: 1/N», troca com as setas, vê os 5 slots de cada material com
  o sufixo honesto nos 4 não-base e o pick a escrever no material
  selecionado sem tocar nos outros).
- O GL dos slots 1-4 mantém-se à espera dos samplers da 0.11-L (a mesma
  fronteira dos hotfixes 1-2 — AGORA anunciada no ecrã pelo sufixo).
- O RENDER dos picks em materiais >0: o BlockMesh desenha por blocos com
  os binds do GTB1 — o pick multimat muda a GRADE de edição do TIC; o
  draw refletir o pick por material fica para a 0.11-L junto dos
  samplers (a fronteira está no relatório e no sufixo).
- .gmesh antigos: a grade só nasce do REIMPORT ou do apply de novo (a
  migração é reimportar).
