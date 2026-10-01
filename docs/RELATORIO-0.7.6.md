# RELATÓRIO 0.7.6 — Reestruturação UI/UX do editor: toolbar final de 5 grupos + ícones vetoriais + Theme central + tela de projetos

Sub-fase de **reestruturação UI/UX definitiva** (primeira das duas de
0.7.6/0.7.7): a barra superior tinha crescido organicamente (Menu, Play,
Settings, 3D, UI, Mover, Rodar, Escalar, Snap — tudo texto, tudo ao mesmo
nível) e já não escalava; a tela de projetos mantinha uma criação
duplicada com botão de gradiente cinza e mostrava o prefixo interno
`primary:` do URI SAF na lista.

## 1. Objetivo

(1) **Toolbar final de 5 grupos** com princípios declarados: texto só para
identidade/ações raras; uso frequente = ícones; agrupamento por função com
separador visual; estados mutuamente exclusivos = segmented control;
esconder o que não se aplica (grupo de transformação só com seleção);
zero emoji. (2) **8 ícones vetoriais próprios** desenhados a polilinhas
pelo line batch existente (sem parser de SVG, sem raster). (3) **Theme
central** (struct Theme; cor de marca `#8AB4F8` como exceção documentada
ao tema mono dos painéis). (4) **Tela de projetos reestruturada** (uma
criação, lista nome+data sem `primary:`, botões de contorno de marca).

## 2. Estado inicial (HEAD de entrada)

`af7a004` (0.7.5-b, fecho com relatório). Suíte: 401 OK / 0 falhas; CI
100% verde; APK 0.7.5 assinado (versionCode 25).

## 3. Arquitetura escolhida

**Toolbar como módulo próprio (`ui/Toolbar.h/.cpp`, namespace
`editor::toolbar`)**: a barra deixa de ser um método do UiContext (a
antiga `UiContext::toolbar(bool[3])` morreu) e passa a ser um módulo com
(1) **layout PURO** (`toolbar::layout(sw,sh,insets,g4Visible)`) — larguras
naturais por grupo que encolhem proporcionalmente quando não cabem,
separadores finos calculados, G5 ancorado à direita (não mexe quando o G4
aparece/desaparece); (2) **draw immediate-mode** que consome o layout.
Desenho e testes partilham a MESMA fonte de posições.

**A captura de gesto foi extraída do botão**: `UiContext::widgetHit(id,x,y,w,h)`
+ `widgetActive(id)` expõem a MESMA semântica do `button()` (press edge
dentro do rect captura; release dentro = clique) sem o desenho — a toolbar
desenha os próprios widgets (ícones/segmented/texto+caret) com o gesto
padrão. O `button()` antigo foi refactorado para usar o `widgetHit`.

**Ícones = polilinhas em código (`ui/Icons.h/.cpp`)**: cada ícone é um
array de pontos em viewBox 0..24 + uma lista de polilinhas; `drawIcon`
mapeia para o rect do botão (escala size/24) e emite cada SEGMENTO no
`UiContext::drawLine` (o line batch dos gizmos 3D) com espessura uniforme
`size/12`. O arco do Rodar é gerado por código no arranque do TU (cos/sin
não são constexpr) — a ponta da seta e o eixo central são escritos no
mesmo gerador. Nada de parser de SVG, atlas novo ou raster; SVGs fonte em
`assets/` seriam só referência de design.

**Theme central (`ui/Theme.h`)**: `struct Theme { panel, text, line (mono
INTACTO, espelha os tokens históricos), bg (#0B0E13, fundo da barra),
brand (#8AB4F8, cor de marca), brandInk (#0B0E13, ícone/texto SOBRE a
marca) }` — a toolbar/ícones leem TUDO daqui; os painéis do editor
continuam no `vv::theme` de sempre.

**Tela de projetos**: a lógica de rótulo/data foi isolada numa classe
**PURA host-testável** (`vv/goni/ProjectsFormat.java`, zero imports de
Android — nada de `android.net.Uri`) porque o CI não tem instrumentação:
o passo `javac` compila e corre `tests-java/ProjectsFormatTest.java` na
JVM do hospedeiro. O check ESTRUTURAL do ecrã (`scripts/projects_ui_check.py`,
mesma cultura do `jni_parity.py`) aferi o que a JVM não instancia (uma
criação, sem botão de fundo, sem URI cru na lista, contorno de marca).

## 4. Implementação (por ficheiro, commits `47bc381` + `1b1a42c`)

- **`ui/Theme.h` (NOVO)** — struct Theme; marca `#8AB4F8` e fundo
  `#0B0E13` EXATOS; mono re-exportado.
- **`ui/Icons.h/.cpp` (NOVO)** — os 8 ícones (Mover: setas cruzadas
  perpendiculares; Rodar: arco parcial 235° + ponta + eixo central;
  Escalar: seta diagonal dupla; Snap: íman geométrico angular + polos;
  Inspector: retângulo + 3 linhas; Cena: 2 losangos sobrepostos; Play:
  triângulo; Pause: 2 barras); `def/segmentCount/totalLength/drawIcon`.
- **`ui/Toolbar.h/.cpp` (NOVO)** — `GizmoModeState` (movido de
  EditorUi.h), ids (Menu 1, Play 2, Pause 3, gizmo 7/8/9/10, 3D/UI
  11/12, inspector 13, cena 14), `layout` puro, `draw` com G1 texto+caret
  vetorial (o atlas é ASCII — o ▾ são 2 traços), G2/G4/G5 `iconButton`
  (inativo: ícone de marca sobre o fundo escuro; ativo/held: fundo de
  marca + ícone escuro), G3 `segmentText`, separadores.
- **`UiContext.h/.cpp`** — `widgetHit/widgetActive` extraídos;
  `toolbar(bool[3])` removida; `button()` refactorado.
- **`EditorLayout.h`** — `kMode3dId/kModeUiId` passaram à Toolbar.h;
  `toolbarModeRect/toolbarModeEndX` removidos (a fonte única é
  `toolbar::layout`).
- **`EditorUi.h/.cpp`** — `drawModeToggle/drawGizmoToolbar` removidas;
  `EditorState.showInspector` (G5); `centerRect(sw,sh,in,rightPanel)`;
  dropdown do Menu com os itens novos (Settings entrou na 1ª linha,
  "Cenas…" saiu).
- **`SafeArea.h`** — `centerRect(sw,sh,in,rightPanel)`: com o painel
  direito escondido a área dele junta-se ao viewport central (orbit,
  gizmos e viewport 2D usam o MESMO rect).
- **`UiEditor.cpp`** — `drawUiViewport` usa o rect com painel opcional.
- **`main.cpp`** — cabeamento: Actions da toolbar (menu/cena/play/pause),
  G5 esconde o Inspector (e o rect cresce), viewport 2D no bloco da barra,
  dispatch do dropdown (1 Settings, 2 Guardar, 3 Carregar, 4 OBJ, 5
  Importar, 6 Export, 7 Sair), Cena abre o overlay CENAS de sempre.
- **`ProjectManagerActivity.java` (REESCRITO)** — topo [Novo projeto]
  [Importar projeto] (contorno #8AB4F8, `GradientDrawable` com setStroke,
  sem gradiente), cabeçalho "Meus projetos", lista 2-linhas
  (nome + `ProjectsFormat.dateLabel`), SEM botão de fundo, long-press
  intacto (rodapé do diálogo com `ProjectsFormat.folderLabel`).
- **`VvProjects.java`** — `Entry.editedAt` (fallback createdAt no
  projects.json antigo; gravado sempre); `launchEditor` marca a última
  edição.
- **`ProjectsFormat.java` (NOVO)** — `folderLabel` (descodifica %XX,
  remove o prefixo interno até ':': `primary:`, `home:`, IDs de cartão)
  e `dateLabel` (dd/MM/yyyy HH:mm).
- **`tests-java/ProjectsFormatTest.java` (NOVO)** — casos do `primary:`
  (com caminho, simples, cartão SD, sem prefixo, vazio/null) + formato de
  data determinístico.
- **`scripts/projects_ui_check.py` (NOVO)** — check estrutural (strip de
  comentários; asserts de criação única/sem botão extra/sem URI cru/sem
  literal `primary:`/contorno de marca).
- **`.github/workflows/release.yml`** — passos "Testes da tela de
  projetos (JVM host)" (javac+java) e "Estrutura da tela de projetos";
  artifact `goni-vv-0.7.6-release-signed`.
- **`tests/test_toolbar.cpp` (NOVO)** + atualização de
  `test_ui/test_playui/test_uieditor` (Env com `toolbar::draw`; o teste
  antigo "toolbar sem sobreposição em 3 botões" foi substituído).

## 5. Decisões técnicas relevantes

1. **Snap é um toggle do grupo G4, não um 4º estado de transformação** —
   mover/rodar/escalar são mutuamente exclusivos (segmented), mas o snap
   liga/desliga o snapping do modo ativo (como em todos os editores
   convencionais). A spec lista os 4 no grupo; a exclusividade aplica-se
   aos 3 modos.
2. **O Menu manteve TODAS as ações de ficheiro** (Guardar/Carregar/Export
   OBJ/Importar/Export Downloads/Sair) além de Settings — a spec nomeia
   "Settings, Guardar, Sair" como cabeçalho; REMOVER o resto seria
   regressão de funcionalidade (o "Cenas…" é o único que saiu — ganhou o
   próprio dropdown no G1).
3. **Pause no G2 é defensivo**: em play a barra não se desenha (a play bar
   com Stop é o caminho); se algum dia a barra existir em play, o pause
   sai do play. Mantido pelos dois ícones da spec.
4. **O caret ▾ é desenhado com 2 traços** (line batch) — o atlas é ASCII;
   nada de glifos fora dele.
5. **`editedAt` em vez de espiar o ficheiro**: a data da última edição
   atualiza-se ao ABRIR o projeto (projects.json, app-private) — sem
   tocar na pasta SAF por render de lista (query a providers a cada frame
   seria caro/frágil).
6. **A validação local do Java é um espelho python** (o sandbox só tem
   JRE) — o CI é que compila o Java de verdade; foi isso que apanhou a
   variável duplicada do hotfix `1b1a42c` (o javac do CI viu o que o
   espelho não podia ver).

## 6. Testes novos (CI Linux)

`test_toolbar.cpp` (7): 5 grupos sem sobreposição em 2 larguras ±G4 +
separadores + G5 ancorado; transformação condicional (sem seleção/modo UI
o toque no sítio do G4 não faz nada; com seleção o MESMO toque ativa);
segmented exclusivos (3D|UI limpa a seleção de elemento ao sair; modos
exclusivos; snap toggle sem mexer no modo); ícones dentro do rect a
24/32px + espessura uniforme + sem segmentos degenerados + viewBox 0..24
+ peso visual em banda comum; Theme central (cores EXATAS de
#8AB4F8/#0B0E13 + o batch prova que a barra LÊ do struct: fundo=bg,
ativo=brand, ícone inativo=brand); ações G1/G2/G5; dropdown do Menu 0.7.6
(1=Settings, 3=Carregar, 7=Sair; o clique fecha).

Fora da suíte C++: `ProjectsFormatTest` (JVM host, 13 asserts) +
`projects_ui_check.py` (8 asserts estruturais) no CI.

## 7. Commits da sub-fase (branch main)

- `47bc381` — 0.7.6-a: implementação completa (toolbar/ícones/Theme/
  tela de projetos/testes/CI).
- `1b1a42c` — 0.7.6-b: hotfix Java (variável `slash` duplicada no
  `folderLabel` — o javac do CI apanhou; renomeada `slashInLabel`).

## 8. HEAD da sub-fase

`1b1a42c` + este relatório (commit do relatório fecha a sub-fase).

## 9. Suíte de testes

**407 OK / 0 falhas** (401 → 407: +7 do test_toolbar; o antigo
`uieditor_toolbar_sem_sobreposicao_em_2_larguras` foi substituído pelo
novo teste de 5 grupos). Contagem do run 36824812418.

## 10. CI

Run **36824812418** 100% verde: core-tests (407 + check_main +
link_parity com 79 TUs + jni_parity + JVM host + check estrutural),
build-release (APK assinado), verify-entry-symbols. (O run inicial
36823205794 do `47bc381` falhou no javac — causa e fix no item 5.6.)

## 11. APK

`goni-vv-0.7.6-release-signed` (artifact 11144263669 do run 36824812418),
versionCode **26**, versionName **0.7.6**.
sha256 do APK assinado:
`da1f5ed089b7db8f70a471f643da3ca6823a069e0b4d25b1fe243b6cf37a9ea6`

## 12. Verificação no device (roteiro C33 — resumo)

Roteiro completo no README (secção 0.7.6): tela de projetos (topo com
contorno de marca, lista nome+data SEM `primary:`, uma criação); toolbar
em 5 grupos com ícones nítidos; G4 some sem seleção; segmented
exclusivos; dropdowns do Menu/Cena; G5 esconde o Inspector e o viewport
cresce; regressões 0.7.4/0.7.5 (paridade, overlays, teclado).

## 13. Riscos e mitigações

- **Larguras de ecrã extremas**: o solver proporcional garante
  sem-sobreposição a qualquer largura ≥ ~700 px (landscape travado; C33 é
  1600). O piso de 18 px do ícone mantém legibilidade.
- **Ícones a 24 px**: geometria simples + traço 2 px (size/12) — o teste
  aferiu viewBox/comprimentos; a legibilidade fina é matéria do C33.
- **`importar projeto` numa pasta sem estrutura**: o `createStructure`
  respeita existentes e o `openOrCreate` do native nunca duplica
  (bridgeFindFile tri-estado, 0.6.4) — a pasta ganha a estrutura que
  faltar.

## 14. Dívida técnica conhecida

- O G2 pause é visualmente um par (pause|play) mas funcionalmente o
  caminho de play→stop é o da play bar (o pause fica defensivo).
- O teste de peso visual dos ícones usa uma BANDA (24..85 u) — contornos
  ocos (Inspector/Cena) têm mais caminho que barras (Pause); uma métrica
  de "massa percetível" ficaria para um ciclo de design.

## 15. Restrições respeitadas (CLÁUSULA CALMA)

Só toolbar/ícones/tela-de-projetos/Theme + testes. Zero física nova, zero
scripting, zero UI de jogo. Tema mono dos painéis intacto (a marca
#8AB4F8 vive SÓ em ícones/ativos — exceção documentada em ui/Theme.h).
Nada sobreposto (layout dinâmico aferido em 2 larguras ±G4). Nenhum emoji
(o caret ▾ é vetorial). Bump 0.7.6/versionCode 26.

## 16. Conclusão

A barra deixa de ser uma lista plana de texto e passa a ter GRUPOS por
função com ícones próprios nítidos; o que não se aplica some (G4); os
estados exclusivos são segmented com fundo de marca; o Theme central dá
uma fonte única de cor; a tela de projetos fica com UMA criação, lista
limpa (nome + data) sem o prefixo interno `primary:` e botões de contorno
de marca. Suíte 401→407 + testes Java host + check estrutural no CI;
release assinada verde. Pronta para o C33 — e o terreno está limpo para a
0.7.7 (TIC de câmara), que usa o line batch e a cor de marca agora
estabelecidos.
