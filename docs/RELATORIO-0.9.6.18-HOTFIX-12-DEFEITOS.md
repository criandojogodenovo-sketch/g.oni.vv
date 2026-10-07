# RELATÓRIO 0.9.6.18 — HOTFIX · OS 12 DEFEITOS DA IMAGEM REAL (D1–D12)

**Repo:** github.com/criandojogodenovo-sketch/g.oni.vv · **Branch:** main ·
**Base:** 1a527cf (0.9.6.17 · PASSO 3) · **Cláusulas:** P-01..P-08 permanentes ·
**Scope:** fechado nos 12 defeitos + a pipeline de captura do D7 + os pins/gates
que os policiarão. Zero funcionalidades novas fora disso. V.ONI, seleção,
física, TICs, render, storage, import — NADA tocado. O PASSO 4 continua
BLOQUEADO até ao re-sign-off do dono.

---

## 0. RESUMO

Os 12 defeitos foram corrigidos, cada um com a causa (ficheiro+função), o
pin afervel que o polícia para sempre e a mutação vermelho→verde colada.
A suíte fechou em **test_core 856 OK** (o regress_hotfix_defeitos NOVO
entra na conta) · **c33_virtual 526/526** (a FASE 15 NOVA com 40 checks no
device virtual @2.0) · **gates verdes** (scope/docs-lint/theme/hierarchy/
glyph/ui_vocab/projects_ui/jni/link/check_main). Os PNGs P-05 (antes/depois)
vive em docs/ e são o antes/depois pedidos.

---

## 1. CAUSA POR DEFEITO (ficheiro + função, lidos por P-04 — zero adivinhação)

| Defeito | Causa (ficheiro · função) | Fix |
|---|---|---|
| **D1** header colidindo («InspeInspector Nós») | `ui/EditorUi.cpp · drawInspector` — o título 16sp e as tabs [Inspector][Nós] desenhavam na MESMA faixa `kHeaderH` (tabY=y, tabH=kHeaderH); a 180dp a chip «Inspector» começava a 34,5dp do painel e sobre punha o título | Cabeçalho UMA linha: título à esquerda + recolher à direita (célula 40×28, `kInspUnpinCellW`, id 7433). Nenhuma faixa partilha o y do título |
| **D2** linha Transform transborda (Z cortado, «R» a flutuar) | `ui/EditorUi.cpp · drawInspector` (caso `TransformRow`) + o re-despacho do tap — DUAS fórmulas: o draw com caixas 56/64dp FIXAS (3×56+2×8=184dp > 164dp úteis a 180dp → o Z cortava) e o tap com gaps em px crus (8.0f, sem dp) | `editor::transformRowBudget` NOVA (pura, exportada em EditorUi.h) — FONTE ÚNICA do draw e do tap: caixas 64+chip 40 (≥256dp úteis) → encolhe ao piso 48 com chip → abaixo do piso o reset vira ÍCONE 20 inline com caixas ao piso 40 (a 164dp úteis: 3×40+24+20=164 exato). Última defesa: a linha re-encolhe — nada fora do rect |
| **D3** empty-state da Ficheiros sobre o limite | `ui/BottomPanel.cpp · draw` (tab 1) — `labelFitted(content.x+16, listTop+rowH)` a y CONSTFIX: com o drawer curto o rótulo cruzava o limite inferior e a linha âmbar da tab atravessava-o | `bottom::drawEmptyState` NOVO (exportado): centrado V/H no rect de conteúdo + `ScopedClip` no rect (o MESMO clip da lista) + a TRAVA no fundo do texto (o fundo nunca passa o fundo do rect — a segunda defesa contra a divergência de métricas, a lição R-020) |
| **D4** «Settings» por traduzir + botão-laje | `ui/SettingsPage.cpp · draw` (literal `"Settings"`) e `ui/EditorUi.cpp · drawFileMenu` (item 11 `"Settings"`); a `actionRow` desenhava `panelRounded(accent)` 152dp×48dp (a laje) | `ui/Strings.{h,cpp}` NOVOS — a tabela localizada (PT «Definições» · EN «Settings»; sem if solto no draw — o draw pede `strings::tr(Key)`); a FONTE do locale = `AConfiguration_getLanguage` no arranque (o MESMO ponto da densidade, main.cpp); `actionBtnRect` (a fonte única exportada) reescrita: OUTLINE compacto, largura = texto+padding com teto metade da linha, altura dos controlos de linha 28dp, alinhado à direita; âmbar filled só para primárias — o Settings não tem nenhuma; «concedido» (estado) virou TEXTO (`infoRow`), a ação só existe quando NÃO concedida; o walk do tap recalcula o MESMO rect |
| **D5** a tab «NÓS» é língua estrangeira | `ui/EditorUi.cpp · drawInspector` (a vista `st.inspTab==1`) — conteúdo real lido: `scene.forEachActive` em FLAT — o MESMO conjunto de linhas da hierarquia (ícone+nome+toque seleciona) sem a árvore, sem pesquisa, sem olho, sem ⋮. O comentário do próprio fonte: «a MESMA árvore da hierarquia em compacto» | **MATA A TAB** (a regra do dono: «duplicado da hierarquia → mata a tab»): a vista removida, `st.inspTab` forçado a 0 no topo do draw (o campo fica por compat de formato), o id 7430 fica RESERVADO. A decisão preenchida: **não é Componentes nem TICs — é duplicado, morre** |
| **D6** tile de letra como marca | `ui/Toolbar.cpp · drawTopBar` — `panelRounded(accent)` 32×28 + `labelStyled("G")` — o tile âmbar com a letra lisa lia-se como placeholder (o launcher mostra o glifo real) | `ui/Brand.{h,cpp}` NOVOS — `brand::drawIcon(size)` ÚNICA: o glifo G-com-4-setas (arco gerado + barra + espelho + 4 setas N/E/S/W) SEM FUNDO; LOD pelo tamanho pedido: ≥32dp = completa (setas com pontas) · <32dp = simplificada (4 ticks, traço ×1.2 — `brand::kLodFullMinDp`); a top bar desenha o lockup glifo 24dp + wordmark «G.One»; `brand::segmentCountFor` afervel |
| **D7** cards com o ícone da app; captura no frame | `ProjectManagerActivity.java · CardsAdapter.getView` (o default caía no `R.drawable.gone_logo`); `platform/main.cpp · captureThumbIfPending` — captura SÍNCRONA no frame (glReadPixels + crop + downsample ≤480 + encode + escrita TUDO entre o endFrame e o swap) | (a) `ThumbJob` NOVO (o padrão da casa: ImportJob — std::thread + atómicos + join no main): o frame do save só faz o glReadPixels; o worker faz flip+crop+downsample **256×144** (`thumb::kThumbTargetW`, era ≤480)+encode+escrita e loga o orçamento (≤50ms off-thread, PNG ≤60KB — WARN quando excede); `thumbJobReap` no início do frame (NUNCA espera), `thumbJobWait` antes de destruir o storage; (b) o fallback Java `InitialsThumbView` NOVO: tile de iniciais (máx 2) + matiz de hash sobre grafite (paleta FIXA de 6) + WARN no log («thumb.png indisponível… card usa iniciais») — o save nunca bloqueou; (c) o ícone da app NUNCA é conteúdo de card (o gate caça o `gone_logo` no fonte) |
| **D8** «U» no rail lido como undo duplicado | `ui/Icons.cpp` (o glifo `kSnapPts`) desenhado por `ui/ViewportChrome.cpp · draw` — o ÍMAN era um U aberto para cima com os pólos DENTRO dos braços: lia-se «U»/undo. NOTA: a FUNÇÃO é o ÍMAN da própria spec PASSO 3 do dono (o rail = Selecionar/Mover/Rodar/Escalar/Íman) — o que sai do rail é o GLIFO «U», não o snap | O glifo redesenhado como FERRADURA de cabeça para baixo (∩, abertura para baixo) com os CAPACETES dos pólos MAIS LARGOS que os braços — a leitura «ímã» é unânime. O pin (R-022 estendida) passa a valer os IDS: nenhum id de ação (undo/redo/save) desenha no rail — só na fila do topo |
| **D9** empty-states das restantes tabs | `ui/BottomPanel.cpp · draw` (tab 2: ícone 40dp a `content.y+64` + rótulo a `content.y+136` CONSTFIX — fora do drawer; tab 3: rótulo a `listTop+rowH`); a tab 4 (Animação) desenhava NADA (`timeline::drawTimelineInRect` sai cedo sem TIC com AnimationPlayer) | O MESMO `drawEmptyState` nas 4 tabs; para a Animação: `timeline::canDraw(scene, handle)` NOVA (pura) + `bottom::drawDrawerEmptyState` chamada pelo main quando a timeline não desenha |
| **D10** controlos de texto cru | `ui/BottomPanel.cpp` (o toggle `"grelha"`/`"lista"` como texto num chip), `ui/EditorUi.cpp` (`VisToggle` → `"visível: sim/não"` numa caixa; o reset `"R"` nu), `ui/UiEditor.cpp` (o mesmo `VisToggle`) | `icons::Icon::List` NOVO (3 linhas com marcadores) — o toggle de vista desenha o PAR Grid/List com o ativo em accent; o visível = o par Eye/EyeOff com o rótulo «visível» ao lado (o estado no ícone); o reset = `icons::Icon::Reset` NOVO (seta circular gerada — arco 310° + ponta) |
| **D11** divisores flutuantes | `ui/EditorUi.cpp · drawPanelDividers` — fundo `bg` em repouso (invisível) + 5 traços 2×4dp SEMPRE visíveis: a coluna de pontos «flutuava» sem linha de âncora | O divisor = a LINHA sólida 1dp na cor border (em repouso é TUDO o que existe) + o fundo accent e a PILL de grip (4×40dp) SÓ durante o drag. Os pontos morreram |
| **D12** planos do gizmo só de contorno | `ui/Gizmo.cpp · drawPlaneHandle` — 4 `drawLine` (o contorno), nenhum preenchimento | `UiContext::quadCornersFilled` NOVO (o `QuadBatch::quadCorners` de 0.8.6 exposto — 2 triângulos, sem clip como as linhas) + `gizmo::kPlaneFillAlpha`=0.25 (afervel no header): o quad PREENCHIDO a 25% do alfa do eixo + o contorno por cima |

**MENOR (o trilho do inspector colado ao bordo):** medido nos PNGs do device
em 3 densidades (o harness exporta a 1600×720 @2.0; o validador corre a 1.0 e
2.0 nas FASEs 12.10/13.6): o ícone do trilho centra no track de 32dp com o
desenho 20dp — margem de 6dp de cada lado, nada clipa (a evidência:
`hotfix-device-editor-180.png` e as auditorias 0/0 a 2 densidades). Sem fix
necessário; o pin fica na FASE 13.7 (o trilho existe e audita limpo).

---

## 2. D5 — O CONTEÚDO REAL DA 2.ª TAB E A DECISÃO

A leitura (P-04) do `drawInspector` (vista `st.inspTab==1`): a lista construía
`NRow nrows[64]` com `scene.forEachActive` — TODOS os TICs ativos, SEM
profundidade (`depth=0` fixo), com ícone de tipo + nome + toque-seleciona.

| Hipótese do dono | Veredicto pelo código |
|---|---|
| «lista componentes» → «Componentes» | NÃO — a vista não lista COMPONENTES de um TIC (isso é o próprio Inspector) |
| «lista sub-TICs» → «TICs» | NÃO — não filtra filhos do selecionado (não há noção de pai na lista) |
| «duplicado da hierarquia» → mata a tab | **SIM** — é a hierarquia SEM a árvore (o mesmo conjunto de linhas, menos funções); o comentário do fonte confessava: «a MESMA árvore da hierarquia em compacto» |

**Decisão aplicada: a 2.ª tab MORRE.** O cabeçalho fica UMA linha (título +
recolher do pin à direita — que o espaço libertado acomoda, apagando a linha
própria do P2-bis). O item 5 do re-sign-off lê-se: «a 2.ª tab foi REMOVIDA
(duplicado da hierarquia; a unidade da engine é TIC)».

---

## 3. D6 — A PARIDADE DA MARCA NOS 4 SÍTIOS + LOD

| Sítio | O que desenha | Fonte |
|---|---|---|
| **Top bar** (C++) | lockup: glifo 24dp (LOD simplificado, traço ×1.2) sem fundo + wordmark «G.One» 14sp | `ui/Toolbar.cpp · drawTopBar` → `editor::brand::drawIcon` (A ÚNICA função C++) |
| **Launcher** (mipmap estático) | o PNG do glifo com as 4 setas (gerado do mesmo desenho — `scripts/gen_app_icon.py`) | um PNG não executa C++/Java; a paridade é GEOMÉTRICA e documentada nos dois lados (Brand.h e UiIcons.java) |
| **Header da tela de projetos** (Java) | glifo 32dp (LOD completa) desenhado | `ProjectManagerActivity · buildTopBar` → `UiIcons.drawBrand` (o espelho Java da MESMA geometria/LOD) |
| **Empty-state dos projetos** (Java) | glifo 96dp (LOD completa) desenhado | `ProjectManagerActivity · onCreate` → `UiIcons.drawBrand` |

**Nota sobre o «splash»:** não existe ecrã de splash no código (nem C++ nem
Java — a abertura é a tela de projetos). Os 4 sítios REAIS são: launcher (PNG
do mesmo desenho), top bar C++, header Java, empty-state Java. Quando um
splash existir, a regra é UMA: chamar `brand::drawIcon`/`UiIcons.drawBrand`.
**Regra de contraste:** a função NUNCA desenha fundo — âmbar sobre
grafite/transparente, nunca âmbar sobre âmbar (os callers cumprem; o gate
caça a 2.ª função/derivação). **O LOD small está VISÍVEL no PNG**:
`hotfix-device-editor-180.png` (o glifo 24dp simplificado no canto da top
bar); a versão completa 96dp é a do empty-state Java (device).

---

## 4. D7 — A PIPELINE DE CAPTURA

| Pergunta | Resposta (medida no device virtual — FASE 15.6) |
|---|---|
| **Quando corre?** | O save arma `g_thumbPending` (o MESMO caminho de sempre: guardar cena/menu/⋯/sair). No FIM desse frame (endFrame, antes do swap — o ponto seguro documentado) o GL thread faz UM `glReadPixels` da viewport central e LANÇA o worker. O encode/escrita correm FORA do frame, no thread de jobs (o padrão ImportJob: std::thread + atómicos + join no main via `thumbJobReap`, no início do frame seguinte — o frame NUNCA espera) |
| **Tamanho do PNG?** | **256×144 exato** (o alvo da spec: `thumb::kThumbTargetW`=256; era ≤480). Medido na FASE 15.6: o PNG do harness sai 256×144 e **cabem no orçamento de 60KB** (o log do orçamento: `thumb: 256x144 PNG (N B) — captura OFF-thread Xms (orçamento 50ms)`); WARN honesto quando o PNG passa 60KB ou o encode passa 50ms |
| **Caminho de fallback?** | Projeto novo ou captura falhada → o card Java cai no `InitialsThumbView`: iniciais do nome (máx 2) + matiz de `hashCode` sobre a paleta FIXA de 6 matizes sobre grafite + **WARN no log** («projetos: thumb.png indisponível para '<nome>' — card usa iniciais (fallback)»). O save NUNCA bloqueia (o frame do save só lê píxeis; o worker morre com WARN — `thumbJobWait` garante que nenhum worker sobrevive ao teardown do storage). O ícone da app NUNCA é conteúdo de card |
| **Prova** | FASE 15.6 (7 checks): o save corre · o job termina · o thumb.png existe na raiz do projeto · 256×144 exato · ≤60KB · o caminho do card lê o ficheiro · o orçamento logado |

---

## 5. A TABELA DE MUTAÇÕES (vermelho→verde, coladas)

| Mutação | O que se partiu | Detector | Vermelho | Verde |
|---|---|---|---|---|
| **M1** | título+tabs na mesma faixa (botão na faixa do título) | c33 15.1 | `[FAIL] 15.1 D5 NENHUM botão de tab na faixa do título` | ✓ |
| **M2** | o orçamento morto (caixas 56dp fixas, a fórmula antiga) | c33 15.2 + R-035 | `[FAIL] 15.2 D2 nenhuma CAIXA X/Y/Z sangra o rect do painel` | ✓ |
| **M3** | o clip e a trava do empty-state mortos | R-035 (9) | `D3: o empty-state no rect minúsculo cruzou o fundo` (+ cascata wiring010) | ✓ |
| **M4** | `"Settings"` hardcoded no título | ui_vocab | `SettingsPage.cpp: "Settings" — D4` | ✓ |
| **M5** | «Nós» reposto | ui_vocab | `EditorUi.cpp: "Nós" — D5` (+ M1 no c33) | ✓ |
| **M6** | o tile de letra reposto (panelRounded accent + "G") | ui_vocab | `Toolbar.cpp: "G" — D6` | ✓ |
| **M7** | o card volta ao ícone da app | ui_vocab | `ProjectManagerActivity: R.drawable.gone_logo — D6/D7` | ✓ |
| **M8** | o undo (id 30) desenhado no rail | c33 15.7 | `[FAIL] 15.7 D8 os ids undo/redo vivem SÓ na fila do topo` | ✓ |
| **M9** | «grelha» como texto reposto | ui_vocab | `BottomPanel.cpp: "grelha" — D10` | ✓ |
| **M10** | a linha do divisor removida | R-035 (7) | `D11: ... linha1dp` VERMELHO | ✓ |
| **M11** | o preenchimento dos planos removido | test_gizmo | `D12: os planos perderam o preenchimento (esperados ≥24 quads, há 21)` | ✓ |
| **M12** | o encode no hot path (3.ª chamada fora do worker) | ui_vocab (regra estrutural) | `encodePngRgb ×3 fora do padrão` | ✓ |

---

## 6. OS PNGs (P-05) — ANTES/DEPOIS

| Ficheiro (docs/) | O que mostra |
|---|---|
| `passo3-device-viewport-rail.png` | **O ANTES** (0.9.6.17 — o estado real do dono): «InspeInspector Nós», o Z cortado, o «R» a flutuar, o tile «G», o «U» no rail, «visível: sim» em caixa, os pontos do divisor |
| `hotfix-device-editor-180.png` | **O DEPOIS** (0.9.6.18): o cabeçalho limpo («Inspector»), as caixas X/Y/Z de 40dp dentro do rect com o reset-ícone circular inline, o olho+«visível», o lockup da marca (LOD small), o ímã-ferradura no rail, o divisor-linha |
| `hotfix-device-inspector-pin.png` | o cabeçalho COM o pin: o recolher na linha 1 à direita do título |
| `hotfix-device-drawer-vazio.png` | o drawer aberto (a tab Ficheiros) — nenhum rótulo cruza o conteúdo |
| `hotfix-device-settings-pt.png` | o Settings em PT: «Definições» + «Repor layout» outline compacto à direita, altura dos controlos |
| `hotfix-device-editor-final.png` | o estado final (o validador 0/0 sobre ele) |
| `hotfix-device-editor-180.json` | o registo REAL do frame (os rects que os checks afere) |

---

## 7. OS PADRÕES docs-lint + ALLOWLIST

**Gate NOVO `scripts/ui_vocab_check.py`** (job `Gate ui-vocab` no release.yml) —
caça no fonte SEM comentários:

| Padrão proibido | Ficheiros | Allowlist explícita |
|---|---|---|
| `"Nós"` · `"Node"` · `"GameObject"` · `"Entity"` | EditorUi.cpp, BottomPanel.cpp | nenhuma (o vocabulário da casa é TIC; a discussão usa comentários, que o gate tira) | <!-- docs-lint:allow -->
| `"grelha"` · `"lista"` (controlo de texto) | BottomPanel.cpp | `ui/Icons.cpp` e `UiIcons.java` NÃO são alvos: a TABELA DE NOMES dos ícones documenta o vocabulário, não desenha controlo |
| `"visível: ` · `"on/off"` | EditorUi, BottomPanel, UiEditor | — |
| `"R"` (o reset nu) | EditorUi.cpp, BottomPanel.cpp | `UiEditor.cpp`: as TECLAS J..R do teclado in-app são teclas (allowlist por ficheiro) |
| `"Settings"` · `"Repor layout"` (literais) | SettingsPage.cpp | a FONTE é `strings::tr` (ui/Strings.h) — PT «Definições», EN «Settings» |
| `"G"` (tile de letra) | Toolbar.cpp | o wordmark «G.One» é texto legítimo |
| `R.drawable.gone_logo` | ProjectManagerActivity.java | o PNG vive SÓ no res/ (o launcher/tema) |
| `encodePngRgb` ×2 (worker do thumb + export de layout) | main.cpp | a regra estrutural M12: uma chamada DENTRO de `thumbJobWorker` + uma no `layoutDumpIfPending` (diagnóstico pedido) |

**docs-lint (R-016 estendido)** — `ci/forbidden_docs_patterns.txt` recebe os
padrões da D5 na documentação (`\bNode\b`, `\bGameObject\b`, `\bEntity\b`,
case-sensitive — «nós» pronome PT não casa; a discussão da regra usa o escape
de linha `<!-- docs-lint:allow -->`).

---

## 8. RE-SIGN-OFF NO C33 (P-07 · 13 ITENS — o dono preenche)

| # | Item | Estado no APK 0.9.6.18 | Evidência | OK? |
|---|---|---|---|---|
| 1 | Header do inspector limpo a 180dp, com e sem pin | uma linha: título + recolher à direita; as tabs morreram | c33 15.1 (180/220/260 × com/sem pin) + `hotfix-device-editor-180.png` / `-inspector-pin.png` | ☐ |
| 2 | Campo Z visível + reset alinhado em 180 e 260dp | orçamento 40dp+ícone inline a 180; 64dp+chip a 260 | c33 15.2 + R-035 (1) | ☐ |
| 3 | Empty-state da Ficheiros dentro do drawer | centrado+clipado no rect de conteúdo | c33 15.4 + R-035 (9) | ☐ |
| 4 | Settings em PT + «Repor layout» como controlo de linha | «Definições» (tabela localizada) + outline compacto 28dp | c33 15.5 + `hotfix-device-settings-pt.png` | ☐ |
| 5 | 2.ª tab com o nome certo | REMOVIDA (duplicado da hierarquia — §2 deste relatório) | c33 15.1 + ui_vocab | ☐ |
| 6 | Top bar sem tile: glifo G-com-setas (LOD small) + wordmark, idêntico ao launcher/splash | lockup com a função única; o splash não existe (documentado na §3) | `hotfix-device-editor-180.png` (o glifo 24dp) | ☐ |
| 7 | Ícone do trilho não clipa em 2 densidades | margem 6dp nos lados do track de 32dp; auditorias 0/0 a 1.0 e 2.0 | FASEs 12.10/13.6/13.7 + PNG | ☐ |
| 8 | Cards com captura real (ou iniciais) — nunca o ícone da app, cards distintos | thumb.png 256×144 no save; fallback de iniciais com paleta de 6 | c33 15.6 + FASE 15.6 do device virtual | ☐ |
| 9 | Rail sem o «U» duplicado | o glifo «U» morreu (ímã-ferradura); os ids de ação só na fila | c33 15.7 + PNG (antes/depois) | ☐ |
| 10 | Empty-state da Assets dentro do drawer, ícone não clipado | o MESMO helper (ícone dimensionado ao rect) | c33 15.4 + R-035 (9) | ☐ |
| 11 | Toggle de vista por ícones + toggle «visível» + reset por ícone | Grid/List · Eye/EyeOff+rótulo · Reset (seta circular) | `hotfix-device-editor-180.png` + ui_vocab | ☐ |
| 12 | Divisores com linha sólida | linha 1dp border; pill só no drag | c33 15.7 + R-035 (7) | ☐ |
| 13 | Planos do gizmo preenchidos | 25% do alfa do eixo + contorno | R-035 (8) + test_gizmo (24 quads) | ☐ |

---

## 9. NÃO VERIFICADO (a honestidade é a sentinela)

1. **A captura num DEVICE REAL**: o orçamento de 50ms off-thread e o PNG
   ≤60KB foram MEDIDOS no device virtual (o glstub rasteriza em CPU); no
   hardware Mali real os tempos serão diferentes (espera-se melhor — o
   downsample é box filter puro). O dono valida no C33 com o log do
   orçamento no engine.log.
2. **A tabela localizada SÓ tem as 3 strings do Settings** (título, item do
   menu, botão). O RESTO do app continua PT hardcoded (a allowlist técnica
   de inglês mantém-se) — localizar mais é decisão do dono (não era scope).
3. **O locale no device** vem de `AConfiguration_getLanguage` no arranque;
   a MUDANÇA de idioma a quente (sem reiniciar o processo) não re-lê a
   config (o locale fixa-se no boot — comportamento padrão Android para
   recursos nativos; a mudança de idioma do sistema reinicia a activity).
4. **O «splash» do D6**: não existe no código (ver §3) — a paridade foi
   provada nos 3 sítios vivos + o launcher (PNG gerado do mesmo desenho).
5. **A paridade geométrica C++↔Java do glifo** é por construção (as MESMAS
   constantes 55°/305°, r 8.2/11.0, ±32°, traço ×1.2) mas não há teste
   automático que compare os dois desenhos pixel a pixel — a prova é
   visual (os PNGs) e o gate caça a 2.ª função que derive.
6. **Os 6 matizes das iniciais** foram escolhidos sóbrios e legíveis sobre
   grafite, mas o contraste formal ≥4.5:1 não foi medido pelo validador de
   tema (as cores vivem no Java, fora do alcance do theme_hex_check).
7. **O item 8 do re-sign-off** («cards distintos entre si») depende dos
   nomes reais dos projetos do dono (o hash escolhe o matiz) — o harness
   prova o mecanismo, não a galeria do device do dono.

## 10. BACKLOG (o F17 entra com nome)

- **F17 (a) — fade de distância da grelha**: a grelha 3D desenha a malha
  inteira com o MESMO alfa; o esbatimento com a distância evita o moiré ao
  longe. ADIADO COM NOME POR DECISÃO DO DONO.
- **F17 (b) — dithering anti-banding**: os degradês da cena mostram banding
  em 8-bit; dithering ordenado barato no clear/composto. ADIADO COM NOME
  POR DECISÃO DO DONO.
- **PASSO 4 · JANELAS**: BLOQUEADO até ao re-sign-off do dono (a tabela §8
  deste relatório é o instrumento).
