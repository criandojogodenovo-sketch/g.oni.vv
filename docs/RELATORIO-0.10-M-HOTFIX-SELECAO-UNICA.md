# RELATORIO-0.10-M-HOTFIX-SELECAO-UNICA.md

**0.10-M HOTFIX SELEÇÃO-ÚNICA** — a spec do dono (dono, C33, vc 67): o
estado CONTRADITÓRIO da seleção reproduzido no device — linha da hierarquia
ÂMBAR com nome em branco + inspector «Nada selecionado» + sem gizmo, ao
MESMO tempo. O id selecionado ficava PENDURADO (troca de prim/mesh,
guarda-restauro de layout) e o highlight da lista SOBREVIVIA ao modelo.
Cura nas 5 pernas da spec. NADA mais tocado.

## 1. A causa raiz (ficheiro + função)

| # | Causa | Veredicto | Onde vivia |
|---|---|---|---|
| a | VÁRIOS escritores de `EditorState::selected`, cada um com semântica própria (uns levavam `selElement`, outros não) | **CONFIRMADA** — 26 sítios escreviam `g_editor.selected`/`st.selected` DIRETO (main.cpp, EditorUi.cpp, UiEditor.cpp); a divergência entre escritores era a matéria-prima do estado contraditório | main.cpp (21 sítios: `doSwitchScene`, `addScene`, undo/redo, presets, duplicar/colar, loads), EditorUi.cpp (`drawHierarchy` tap, `drawInspector`, `viewportTapClearsSelection`), UiEditor.cpp (`uiAddElement`) |
| b | o id pendurado NÃO era auditado antes dos leitores | **CONFIRMADA** — nenhum ponto validava que o handle marcado estava VIVO antes do frame consumir o estado; o inspector validava (e limpava) MAS era o 3.º leitor — o gizmo (o 1.º) e a hierarquia (o 2.º) já tinham corrido com o morto | main.cpp `frame()` (o gizmo alimentava em `feedGizmo(g_scene.get(g_editor.selected))` sem audit), EditorUi.cpp `drawHierarchy` (linhas desenhadas sem audit) |
| c | o inspector escrevia `selected` DIRETO (2.º escritor a meio do frame) | **CONFIRMADA** — o ramo «Nada selecionado» fazia `st.selected = Handle::invalid()` direto (EditorUi.cpp, `drawInspector`) — fora de qualquer contrato | EditorUi.cpp `drawInspector` (o ramo do `if (!tic)`) |
| d | o tap do viewport armava o deselect ANTES das reclamações do frame | **CONFIRMADA (risco estrutural)** — `viewportTapClearsSelection` corre ANTES do `dividerInput` no frame; qualquer press «dentro do view» armava — a cura da ORIGEM (o clear só com toque NASCIDO no viewport) precisava de ser EXPLÍCITA e afervel | main.cpp `frame()` (a ordem deselect → dividerInput) |
| e | a pasta `audio/` não nascia com o projeto; o `listDir` SAF registava E fantasma | **CONFIRMADA** — `Project::create` criava scenes/meshes/textures e NUNCA audio/; o `refreshFilesTree` (o catálogo) lista «audio» → `SafStorage::listDir` → `resolveDir(createMissing=false)` falha → `elog::error("saf: listDir audio — pasta ausente no projeto SAF: audio")` — o E que sujava o diagnóstico ao abrir o catálogo num projeto novo | core/Project.cpp `create`; core/SafStorage.cpp `listDir`/`resolveDir` |

## 2. A cura (as 5 pernas da spec)

1. **UMA FONTE DE VERDADE** — `selectTic(EditorState&, Handle)` /
   `clearSelection(EditorState&)` (EditorUi.h/.cpp, ao lado do
   `revalidateSelection`) são os ÚNICOS escritores de
   `EditorState::selected` na produção: os 26 sítios passam pelos dois
   (ambos repõem `selElement = -1` — o elemento de UI do TIC antigo não
   sobrevive). O highlight da linha é DERIVADO no draw
   (`st.selected == t->handle` — já era; agora é LEI, com pin e mutação) e
   o inspector e o gizmo leem O MESMO estado (as funções são puras e
   afervéis no CI; a mutação M1 prova a derivação).
2. **ID PENDURADO NÃO É SELEÇÃO** — `selectionAudit(const Scene&,
   EditorState&)`: handle marcado que não existe na cena →
   `clearSelection` COMPLETO (a linha incluída — ela deriva, morre junto)
   + os órfãos do multiSelect saem por COMPACÇÃO (a ordem dos vivos
   preservada — o chip «N x» nunca conta fantasmas). O audit corre em
   DUAS linhas de defesa: no frame do main (main.cpp `frame()`, depois dos
   jobs de import/expand/transição, ANTES do primeiro leitor — o gizmo) e
   à ENTRADA do `drawHierarchy` (os painéis podem correr sem o frame
   completo — o padrão da suíte). O inspector passa a usar o MESMO
   `clearSelection` (3.ª linha).
3. **TROCA DE PRIM/MESH MANTÉM A SELEÇÃO** — o rebuild da primitiva e a
   troca do MeshRenderer acontecem DENTRO do TIC (o `applyAssetPick` edita
   componentes, nunca destrói a TIC — verificado por leitura); o handle é
   da TIC, não do mesh: o pin prova a seleção VIVA depois do pick de prim
   E de mesh, com o audit sem nada pendurado.
4. **A ORIGEM DO TOQUE** — o clear-on-empty-tap só vale para toques
   NASCIDOS no viewport: o `viewportTapClearsSelection` mantém as duas
   verificações de origem (press dentro do view + release dentro do view)
   e a verificação de reclamação (claimedMask); o `dividerInput` passa a
   MATAR o arm do deselect quando o toque nasce NA PEGA (redimensionamento
   ≠ tap). Painel, resize e layout NÃO limpam NEM selecionam — o pin
   aferes as 4 situações (painel / release fora / reclamado / o contraste
   honesto do tap no vazio).
5. **A PASTA AUDIO + A DISCIPLINA DO LOG** — `Project::kDirAudio` (o
   literal vira constante) e o `Project::create` nasce com `audio/` criada
   (o catálogo lista pasta REAL desde o 1.º dia); o `SafStorage::listDir`
   distingue AUSÊNCIA de ERRO: o `resolveDir` ganha o out-param `absent` —
   pasta ausente = `elog::info("saf: listDir %s — pasta ausente (ok, lista
   vazia)")` (I honesto), erro real = E. O E fantasma
   «pasta ausente no projeto SAF: audio» morreu (projects antigos sem
   audio/ ficam em I — nunca E).

## 3. As provas (os 8 pins + as 3 mutações)

**PINS (a família nova `selecao_unica` + o pin de audio):**

| Pin | O que prova |
|---|---|
| `uieditor_selecao_unica_audit_id_pendurado_nao_e_selecao` | o CONTRATO dos únicos escritores (selectTic repõe selElement; clearSelection limpa tudo) + o audit (vivo fica; morto limpa e DIZ; inválido não mexe; órfãos do multi saem por compação COM a ordem preservada) |
| `uieditor_selecao_unica_apagar_limpa_o_highlight_no_draw` | apagar a TIC selecionada POR TRÁS da UI → o draw da hierarquia limpa o id pendurado à ENTRADA e o highlight MORRE JUNTO (ZERO quads âmbar — a «Sub» que subiu ao índice 0 NÃO acende; a prova é VISUAL, pelo batch de sólidos na cor accentDim) |
| `uieditor_selecao_unica_layout_nao_mexe_na_selecao` | o round-trip serialize/parse do layout.json NÃO muda a seleção (o layout persiste PAINÉIS, nunca identidades) |
| `uieditor_selecao_unica_clear_so_com_toque_do_viewport` | a ORIGEM do toque: nascido no PAINEL não limpa; nascido no viewport e solto FORA não limpa; RECLAMADO pelo gizmo nem arma; o CONTRASTE honesto (tap nascido e solto no vazio LIMPA) |
| `uieditor_selecao_unica_vazio_limpa_os_tres_leitores` | o tap no vazio limpa OS TRÊS leitores em harmonia: linha (estado), inspector («Nada selecionado» = scene.get nulo + pickerGuardBlocked), gizmo (O MESMO scene.get nulo) — zero contradição |
| `ui_tap_hierarquia_selecao_viva_no_frame_seguinte` | UI REAL com gestos: tap numa linha da hierarquia → a seleção é O TIC da linha, VIVO, COM NOME; no frame seguinte o gizmo e o inspector leem O MESMO TIC (o plano do inspector tem conteúdo — nunca «Nada selecionado») |
| `assetpick_selecao_unica_troca_prim_mesh_mantem_selecao` | a TIC selecionada troca de PRIMITIVA (primPending) e de MESH (menuKind 1) — o MESMO handle vivo, os leitores em harmonia, zero órfãos no audit |
| `regress_selecao_unica_audio` | projeto NOVO: a estrutura nasce COM audio/ e a varredura do catálogo corre com ZERO linhas E (o audio nem sequer aparece); projeto ANTIGO sem audio/: o listing é I honesto («pasta ausente (ok, lista vazia)») — NUNCA o E fantasma |

**MUTAÇÕES (vermelho → verde, cada uma caçada pelo pin certo):**

| Mutação | O que quebra | Quem a caça |
|---|---|---|
| **M1** — o highlight por ÍNDICE PRÓPRIO (o anti-pattern do device: um índice de linha ARMAZENADO no draw, não derivado do estado) | depois de apagar a TIC da linha 0, a «Sub» que SOBE ao índice 0 acende âmbar (o highlight sobrevive ao modelo) | `uieditor_selecao_unica_apagar_limpa_o_highlight_no_draw` → `hierQuadsAmbar == 0` VERMELHO ×2 |
| **M2** — o clear SEM verificação de origem (o arm ignora view+claimed; o release dispara em qualquer sítio) | o toque nascido no PAINEL limpa a seleção; o gesto reclamado pelo gizmo limpa | `uieditor_selecao_unica_clear_so_com_toque_do_viewport` VERMELHO ×3 + `uieditor_desselecionar_viewport_3d_tap_nao_drag` VERMELHO (o caso reclamado) |
| **M3** — o draw NÃO valida o id pendurado (o audit sai do drawHierarchy) | o id morto SOBREVIVE ao draw da hierarquia (o estado fica sujo para os leitores seguintes) | `uieditor_selecao_unica_apagar_limpa_o_highlight_no_draw` → `!st.selected.valid()` VERMELHO ×2 |

## 4. Os gates locais (todos verdes)

scope (o diff inteiro dentro do ci/scope.txt) · docs-lint 81 · gmesh-docs
25 · ui-vocab · theme · hierarchy · gizmo D20 (2.07%/2.63% ≤ 4%/6%) ·
projects-ui · reload · glyph · jni-parity · check_main · link-parity 122
TUs · release-identity versionCode 68.

## 5. Os números finais

- **test_core: 0 falhas** (a suíte inteira, com os 8 pins novos; a
  sentinela r039 correu VERDE com o disco limpo — o flake de DISCO da
  sandbox está documentado, o CI é o árbitro)
- **c33: 797/797** (o 17.4 atualizado à lei nova: o conjunto da
  multi-seleção leva TICs VIVAS — o audit compacta os órfãos; o chip não
  conta fantasmas)

## 6. NÃO VERIFICADO (honesto)

- **O sign-off no device** — o dono reproduz o vc 67 (troca de prim/mesh
  com a TIC selecionada; guarda-restauro de layout; apagar a TIC
  selecionada; abrir o catálogo num projeto novo) e vê: SEM linha âmbar
  sem nome, SEM «Nada selecionado» com linha acesa, o gizmo sempre em
  harmonia, e ZERO linhas E de audio ao abrir o catálogo.
- **O «nome em branco» do device** — a cura mata a CLASSE (nenhum leitor
  pode divergir: um estado, três leitores, audit antes de todos; a linha
  só acende DERIVADA de um TIC vivo). O caminho exato que produziu a linha
  âmbar com nome em branco no vc 67 não ficou reproduzido na sandbox — se
  o device tornar a mostrar, o log novo («seleção: audit do frame limpou
  id pendurado») DIZ que leitor viu o quê.
- **A troca de mesh por UNDO/REDO** — o undo/redo seleciona o handle
  devolvido (selectTic); a sobrevivência da seleção nos snapshots do undo
  não é pin deste hotfix (o undo é dono do seu contrato).
