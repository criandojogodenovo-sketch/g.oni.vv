# RELATÓRIO 0.7.2 — Import robusto (navegador + galeria + aplicar)

## 1. Objetivo

Terceira sub-fase do F6: (a) navegador de ficheiros in-app que navega
QUALQUER pasta do armazenamento (com o All Files Access), listando
.obj/.gltf/.glb/.png com subir/descer; (b) a GALERIA (DCIM/Camera,
Pictures) incluída nas raízes navegáveis; (c) mostrar sempre ONDE procura
(o caminho no topo) e a pasta vazia/sem acesso com mensagem COM O CAMINHO
(nunca um toast cego); (d) após importar com um TIC selecionado, perguntar
"aplicar ao TIC?" (textura/mesh) e aplicar pelo mesmo caminho do seletor.

## 2. Estado inicial (HEAD de entrada)

- HEAD de entrada: `c677c0e` (fecho da 0.7.1 — relatório; código no
  `61471cd`; suíte 372 OK; release 0.7.1 com versionCode 21).
- O import da 0.6.x era uma LISTA FIXA: varria Download e Documents,
  mostrava até 8 candidatos num overlay sem navegação; não entrava em
  subpastas nem na galeria; a pasta vazia dizia apenas "nenhum ficheiro
  suportado" (toast sem caminho); importar não oferecia aplicar.

## 3. Arquitetura escolhida

- **`FileApi::listDirEntries`** (a extensão POSIX do módulo da F5.2):
  diretorias PRIMEIRO (todas — navegar é o objetivo), depois os ficheiros
  suportados; ambos ordenados por nome case-insensitive; `d_type` com
  fallback `stat` (DT_UNKNOWN é comum); opendir falho → false + errno no
  engine.log (o chamador mostra o caminho).
- **As raízes vivem no FileApi** (`kBrowserRoots`): Raiz/Download/Docs +
  **Camera (DCIM/Camera) e Pictures** — a galeria como raiz de um toque.
- **O CAMINHO como cidadão de primeira classe**: `browserPathLabel`
  (FONTE ÚNICA) devolve o caminho inteiro quando cabe e, quando não cabe,
  corta o INÍCIO guardando o FIM (busca binária pela maior cauda que
  caiba — o dono quer ver "...DCIM/Camera", não
  "storage/emulated/0/D..."); `browserEmptyMessage` traz o caminho
  ("(vazio) <path>" / "(sem acesso) <path>").
- **O overlay NAVEGADOR** (`drawFileBrowser`, ui/UiEditor): painel 92%
  (cap 900px); caminho no TOPO; raízes numa linha fixa; "^ Subir"; lista
  com scroll (id 45; diretorias "/ nome", ficheiros "mesh:/tex:"); o
  Estado do browser (cwd/entries/failed) vive no MAIN (FileApi é
  device/CI — o estado é I/O, a UI é pura).
- **Aplicar-após-import**: o `browserImportFile` copia (readAll →
  writeBytes → refreshCatalog) e, se há TIC selecionado COM MeshRenderer,
  arma o diálogo; "Sim" aplica pelo MESMO `applyAssetPick` do seletor
  (nenhum código de aplicação duplicado); "Nao" deixa só importado.

## 4. Implementação (por ficheiro, commit `e3a601e`)

- **`platform/FileApi.h/.cpp`** — `DirEntry`, `listDirEntries`,
  `parentPath`, `kBrowserRoots` (com a galeria).
- **`ui/UiEditor.h/.cpp`** — `browserPathLabel`, `browserEmptyMessage`,
  `drawFileBrowser`, `drawApplyDialog`.
- **`ui/EditorLayout.h`** — ids do browser (raízes 6850..6854, subir
  6860, linhas 6870+, fechar 6899, aplicar 6900/6901, scroll 45).
- **`ui/EditorUi.h/.cpp`** — `EditorState.fileBrowser/applyAsk`;
  `closeAllOverlays` fecha o browser.
- **`platform/main.cpp`** — `FileBrowserState`/`ApplyAskState`,
  `browserOpen` (lista + log com contagem/falha), `browserImportFile`
  (cópia + pergunta), `attemptImport` abre o NAVEGADOR na Download (o
  fluxo all-files/diálogo intacto), dispatch das raízes/subir/entradas +
  o diálogo aplicar no frame; reset na reentrada.
- **`README.md`** — escopo 0.7.2 + roteiro de verificação C33.

## 5. Decisões técnicas relevantes

- **O navegador SUBSTITUI a lista fixa no Importar…** (o overlay antigo
  `drawImportMenu` fica — continua afervelido pelos testes da F5.2 — mas o
  main já não o abre); o fluxo de PERMISSÃO não mudou um byte;
- **O caminho corta o INÍCIO, não o fim**: é onde o utilizador está;
  quando nem "..." cabe, devolve vazio (nunca transborda — aferido com
  medida fake determinística);
- **Aplicar usa o `applyAssetPick`**: um único caminho de aplicação
  (seletor do Inspector E aplicar-após-import) — a lição da F6 (o wiring
  duplicado era o bug do C33 0.6.9).

## 6. Testes novos (CI Linux)

`tests/test_browser.cpp` (8 casos) — `listDirEntries` (diretorias
primeiro ordenadas, ficheiros suportados com `.PNG` case-insensitive,
subpastas, pasta inexistente → false), `parentPath` (pai; raiz fica na
raiz), raízes incluem a GALERIA, rótulo do caminho (inteiro quando cabe;
corta o início guardando o fim com prefixo "..."; nunca excede a largura;
nem "..." cabe → vazio), mensagem de vazio COM O CAMINHO (e a de sem
acesso), overlay NAVEGADOR (raízes 1..5 / subir 6 / entradas 7+; a
mesma geometria do draw; pasta vazia desenha a mensagem; fora fecha),
diálogo APLICAR (Nao não mexe no MeshRenderer; Sim liga texture+texPath
com o log "material: textura aplicada <ref>"), e2e do import (listar →
ler → gravar no projeto (FakeStorage) → catálogo → aplicar no TIC).

## 7. Commits da sub-fase (branch main)

- `e3a601e` — 0.7.2-a: FileApi browser + overlay NAVEGADOR + aplicar +
  wiring do main + testes (372→380).
- 0.7.2-b — este RELATÓRIO 0.7.2 + sha256 do APK assinado do run da
  sub-fase.

## 8. HEAD da sub-fase

`e3a601e` (0.7.2-a — o código; o commit do relatório vem imediatamente
por cima).

## 9. Suíte de testes

**380 OK / 0 falhas** (baseline 372; +8 em test_browser.cpp).

## 10. CI

Run `36772231260` (commit `e3a601e`) — **100% verde**: core-tests (380 OK
+ check_main + link_parity 75 TUs + jni_parity), build-release (APK
assinado), verify-entry-symbols (símbolos + manifest binário).

## 11. APK

`goni-vv-0.7.2-release-signed` (artifact do run `36772231260`),
versionCode 22 / versionName 0.7.2, arm64-v8a, **APK CUMULATIVO** (cobre
0.7.0/0.7.1 + 0.6.7→0.6.10). sha256 do `app-release.apk`:
```
4545587cb7ca8e72054e5763203fadaaaab4a95cd75aafb09ef46885d5929912
```

## 12. Verificação no device (roteiro C33 — resumo)

1. Menu → Importar… → NAVEGADOR na Download com o caminho no topo;
2. "Camera"/"Pictures" (galeria de um toque); "Raiz" mostra tudo;
3. pastas entram/saem ("^ Subir"); raiz fica na raiz;
4. pasta vazia/sem acesso → mensagem COM O CAMINHO dentro do overlay;
5. importar com TIC selecionado → "Aplicar ao TIC?" → Sim liga a
   textura/mesh já; Nao deixa nos seletores; sem seleção → só o toast;
6. regressões 0.7.1/0.7.0/0.6.x.

## 13. Riscos e mitigações

- **Árvore enorme na raiz** (milhares de ficheiros): a lista mostra 8
  linhas com scroll e só os suportados (o resto nem entra no vetor);
- **Caminhos muito compridos**: o rótulo corta o início (busca binária)
  e a mensagem trunca a 160 chars;
- **Import durante o Play**: o browser é editor-only (overlay fechado ao
  entrar em play — `closeAllOverlays`).

## 14. Dívida técnica conhecida

- O navegador não mostra pré-visualizações (thumbnails) — só nomes
  (mono; F8 pode trazer ícones);
- Não há pesquisa/filtro por nome na lista (a galeria grande pede
  scroll — aceitável para a fase).

## 15. Restrições respeitadas (CLÁUSULA CALMA)

Só import/navegação + testes: zero física, zero scripting (V.ONI=F9.1),
zero componentes de gameplay. O overlay é tema mono dentro da safe-area;
o fluxo de permissão All Files Access ficou INTACTO (diálogo +
re-verificação no resume + log de transição).

## 16. Conclusão

A 0.7.2 fecha o ciclo do asset no device: do ficheiro em QUALQUER pasta
(incluindo a galeria) ao TIC com a textura/mesh aplicada, sem sair do
editor e com o caminho sempre visível (o "onde procura" deixou de ser um
mistério). O aplicar-após-import reutiliza o caminho único de aplicação
— a mesma disciplina que a F6 impôs ao seletor. Resta a 0.7.3 para fechar
a campanha: o joystick editável (a UI do Player como instância) e os
compostos Menu/Card/Article.
