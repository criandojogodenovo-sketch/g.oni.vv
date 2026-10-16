# RELATÓRIO 0.9.4 — FASE 9: G0 (BLOQUEADORES FUNCIONAIS) + G1 (OS 6 PONTOS QUEBRADOS) + G2 (ALINHAMENTO AOS MOCKS)

> Três grupos, três commits, CI verde em cada. Só apresentação, EXCETO G1-6
> (a única mudança de lógica autorizada) e a lógica mínima dos fixes G0.
> Todos os pontos com a linha de CAUSA (ficheiro + função) ANTES do fix.

## 1. OBJETIVO

Fechar a verificação da 0.9.3→0.9.4 da checklist do dono: os bloqueadores
funcionais (script editor fechava ao digitar, esqueleto base, Docs
inalcançáveis, botões mortos), os 6 pontos quebrados (toolbar, acentos,
material, ecrã de projetos, layout.json, seleção) e o alinhamento aos
mocks aprovados (hierarquia, Inspector, barra única, barra de toque).

## 2. FONTE DE VERDADE

- Sintomas: a checklist do dono no device (screenshots + logs) e o output
  do `adb logcat` citado em cada causa;
- O device virtual `c33_virtual` (FASE 9 = UI replay 1600×720/1536×720,
  insets reais, /tmp read-only, lifecycle EGL TERM/INIT);
- A suíte host (759 casos) + 6 gates (check_main, link_parity 102 TUs,
  jni_parity, projects_ui_check, reload_concurrency, glyph_source_check);
- CI: runs colados na secção 10 (verdes nos 3 commits).

## 3. AS 7 PERGUNTAS EMBUTIDAS — RESPOSTAS DIRETAS

**(1) Onde estava a interrupção das Docs (existiam mas eram inalcançáveis)?**
Em `ui/SettingsPage.cpp`, no walk do `scrollTap`: o `case kBitDocs` só
fazia `hy += kRowH` — a linha "Ver docs da V.ONI" NUNCA era re-despachada
para o toque (morta desde a 0.9.2, quando a página ganhou o scroll de
linhas). Fix no G0: o walk espelha o draw e o toque volta a despachar
(a linha abre o DocsScreen; afervado no harness 9.6).

**(2) Que botões estavam mortos (inventário G0-4) e o que foi decidido?**
Dois MORTOS: o [sliders] da top bar (`Toolbar.cpp kTbSlidersId` →
`slidersPressed` NUNCA consumido desde a 0.9.0) e o [⚙ settings] do
viewport (`ViewportChrome.cpp kVpSettingsId` → `vpSettingsMenu` setado,
NADA lia). DECISÃO: REMOVIDOS (regra do dono: cada botão ou é funcional
ou sai). O snap chip FUNCIONA (cicla o valor — virou botão de ÍMAN na
G1-1/G2-9). O ⋮ decorativo ao lado do Ordenar (Java) não fazia nada →
removido na G1-4.

**(3) Porque é que #FFFFFF mostrava um quadrado ESCURO (G1-3)?**
`MeshRenderer::tint` é `f32[3]` passado DIRETO às APIs de cor `f32[4]`
(`panelRounded`/`imageQuad`) — o 4.º componente (ALFA) era lido FORA do
array: lixo, quase sempre <1 → escurecimento. Fix: `tint4[4]` explícito
com alfa 1 nos 4 sítios (textura/cor base/prévia/swatch).

**(4) O que era o quadrado cinzento de ~190px no ecrã de projetos (G1-4a)?**
O `bigLogo` 96dp (≈190px @2x) do EMPTY-STATE: `ProjectManagerActivity
.onCreate` acrescentava a `emptyBox` DEPOIS do grid no FrameLayout
(z-order acima) e o `refresh()` só escondia `emptyTitle`/`emptySub`/`grid`
— com projetos o logo ficava VISÍVEL, FIXO em coords de ecrã (não rolava),
tapando os cards. Fix: o CAIXA inteiro (`emptyBox`) desliga com projetos.

**(5) Onde/when é emitido o "ui: pick bloqueado (sem seleção)" (G1-6)?**
`platform/main.cpp` no `frame()` (consumo do `g_editor.pickBlockedHint`,
logo após o draw do Inspector): o Inspector seta o flag quando um toque em
linha de picker (mesh/tex/prim) foi bloqueado pelo `pickerGuardBlocked`
(TIC sem MeshRenderer/alvo morto). É HONESTO (hint de UI), não é o bug do
pick — o bug do pick era outro: `CamGizmo.cpp pickSceneTic` só media a
distância ao CENTRO projetado (44px) — tocar o CORPO de um cubo grande NÃO
selecionava. Fix: AABB do mesh projetado (8 cantos pela matriz world),
44px como piso, o mais próximo da câmara vence.

**(6) O que faz o 4.º ícone da barra vertical (o "quadrado com ponto")?**
Era o DUPLICATE (rect+plus): duplica o TIC selecionado (cópia com sufixo
de nome). AÇÃO mantida; o GLIFO passou ao COPY padrão (2 quadrados
sobrepostos — G2-11). O Paste (5.º) já era a prancheta.

**(7) O que eram os "pontinhos fantasma" no canto superior direito (G2-10)?**
O TRIAD de orientação do viewport (`ViewportChrome.cpp`): projeção
ortográfica dos eixos X/Y/Z (vermelho/verde/azul) com o marco central de
4px — os "pontinhos". Não estava no mock → REMOVIDO na G2-10 (a
orientação vive no gizmo 3D e na câmara).

## 4. OS GRUPOS — PONTO A PONTO (concluído/não concluído + causa)

### GRUPO 0 (commit 4555977, CI run 37189852254 attempt 5 — verde)
| ponto | estado | causa (1 linha) |
|---|---|---|
| G0-1 script fecha ao digitar | CONCLUÍDO | 3 camadas: portrait+IME no mesmo frame (rotação matava o IME) + handle stale do TIC dono no INIT_WINDOW + modelo append-only sem caret |
| G0-2 esqueleto base | CONCLUÍDO | `scriptwin::open()` deixava o buf vazio sem fonte guardada |
| G0-3 lupa + Docs | CONCLUÍDO | a lupa NÃO EXISTIA na toolbar do editor; a linha Docs estava morta (pergunta 1) |
| G0-4 inventário de mortos | CONCLUÍDO | pergunta 2 (2 mortos removidos, snap funciona, ⋮ removido) |

### GRUPO 1 (commit aa0162a, CI run 37190933465 — verde na 1ª)
| ponto | estado | causa |
|---|---|---|
| G1-1 toolbar ancorada | CONCLUÍDO | `vpchrome::draw` chamava `centerRect(..., 0.0f, ...)` — drawerH HARDCODED 0 |
| G1-2 acentos | CONCLUÍDO | `FontAtlas` assava SÓ ASCII 32..126; iteração byte-a-byte |
| G1-3 material | CONCLUÍDO | pergunta 3 (tint f32[3]→f32[4]) + legendas `labelFitted` com clamp 64px |
| G1-4 ecrã de projetos | CONCLUÍDO | pergunta 4 (bigLogo) + header 4 linhas + `setNumColumns(2)` fixo |
| G1-5 layout.json | CONCLUÍDO | `saveLayoutNow()` corria a CADA frame (cada passo de 8dp = 1 write) |
| G1-6 seleção (lógica) | CONCLUÍDO | pergunta 5 (centro 44px); +mutação obrigatória (secção 14) |

### GRUPO 2 (commit e7ddf66, CI run — secção 10)
| ponto | estado | causa |
|---|---|---|
| G2-7 hierarquia ícones | CONCLUÍDO | o switch de tipo NÃO consultava o BodyComp |
| G2-8 Inspector | CONCLUÍDO | labels de debug + body numa linha truncada + scroll persistia entre TICs |
| G2-9 sliders/linhas | CONCLUÍDO | pergunta 2 (morto → removido na fusão) |
| G2-10 barra única + pontinhos | CONCLUÍDO | 2 faixas (104dp) → 1 (56dp); pergunta 7 (triad removido) |
| G2-11 4.º ícone | CONCLUÍDO | pergunta 6 (Duplicate→Copy glyph) |

## 5. TESTES

- Core: 744 → 751 (G0, +7 scriptwin/settings-docs) → 755 (G1) → **759**
  (G2: +corpo-por-tipo, +física-2-colunas, +inspector-ao-topo, +long-press,
  +barra-única-48px);
- Harness c33_virtual: 147 → 177 (G0) → 186 (G1 na árvore) → 200 (G1
  final com 9.9/9.10) → **209** (G2, passo 9.11);
- Sentinelas: R-001..R-006 verdes + **R-007** (script typing) + **R-008**
  (cobertura de glifos) + **R-009** (pick pelo corpo);
- Gates: check_main, link_parity (102 TUs), jni_parity, projects_ui_check
  (REESCRITO p/ o cabeçalho de 1 linha + grelha adaptativa + emptyBox),
  reload_concurrency, **glyph_source_check (NOVO — gate de acentos do
  fonte, 41 padrões)**.

## 6. HARNESS FASE 9 — O QUE O DISPOSITIVO VIRTUAL AFERMA (por passo)

- 9.1-9.6 (G0): esqueleto+par portrait/IME · 20 teclas do IME sem fechar ·
  rotação → IME re-pedido + handle re-validado + teclado in-app digitável ·
  fonte grava no componente E no .goni · lupa→Docs+pesquisa filtra · linha
  Docs do Settings abre (era morta);
- 9.7 (G1-2/R-008): o atlas do boot tem ç ã Ã õ é í + …; a string de teste
  do dono "ÁUDIO Física Animação Seleção ção ÃÕ ç" EMITE glifos;
- 9.8 (G1-5): ZERO writes durante o drag de 3 passos · EXATAMENTE 1 write
  1,5s após a última alteração · a linha traz o MOTIVO · sem mudança não
  grava · APP_CMD_PAUSE faz flush imediato;
- 9.9 (G1-1): painel FECHADO e ABERTO — os 6 botões + stack DENTRO do rect
  da viewport · a toolbar SOBE ~240px com o painel · nenhum botão pisa a
  faixa do drawer · o total cabe na viewport útil · o '+' no canto inf-dir;
- 9.10 (G1-6): canto do corpo >60px do centro · pickSceneTic direto · o TAP
  pelo caminho REAL da UI seleciona · a mudança LOGA com motivo · o tap no
  vazio limpa E LOGA;
- 9.11 (G2): kToolbarH=56 · o viewport ganhou 48px · tic_static/tic_player/
  tic_rigid pelo BodyComp VIVO · Física 3 linhas em duas colunas · troca de
  TIC → Inspector ao topo.

## 7. ITERAÇÕES DO LOOP (fixes reais apanhados pelos testes novos)

1. **O log da limpeza não disparava** (G1-6): `viewportTapClearsSelection`
   limpa a seleção DENTRO antes do main ver — o handle é fotografado ANTES
   (`selAntesTap`). Apanhado pelo check 9.10 ao ficar vermelho com a mutação
   de pick ativa;
2. **SIGSEGV nos testes uieditor** (G2): taps em posições do chrome ANTIGO
   (104) → canvas null → EXPECT não pára → deref null. Apanhado pelo build
   ASan (`build-asan`); posições recalibradas (88→56);
3. **wiring010 500MB falhava por DISCO** (ambiental): 6×500MB de artefactos
   em /tmp esgotaram o sandbox — limpos; NÃO era regressão (o contador de
   falhas batia só com as 2 asserções da mutação);
4. **A tab fundida quebrava test_wiring091/092** (posições Y do chrome):
   recalibrados via `safe::kToolbarH` simbólico onde possível.

## 8. SENTINELAS PERMANENTES (docs/REGRESSOES.md)

R-001..R-006 (de sempre) + R-007 (regress_script_typing) + R-008
(regress_glyph_coverage + GATE glyph_source_check.py no CI) + R-009
(cameratic_pick_pelo_corpo_g16 + replay 9.10). Todas correm em TODAS as
runs futuras.

## 9. COMMITS (main, ordem)

- `4555977` 0.9.4-a — GRUPO 0 (G0-1..G0-4 + R-007 + FASE 9.1-9.6 + 4 bugs
  de memória do ASan: kBackLines OOB, resolveCanvasLayout stack corruption,
  kReserved sem sentinela, varDeclAction use-after-return);
- `aa0162a` 0.9.4-b — GRUPO 1 (G1-1..G1-6 + R-008/R-009 + 9.7-9.10 +
  2ª leva de acentos + gate glyph_source_check);
- `e7ddf66` 0.9.4-c — GRUPO 2 (G2-7..G2-11 + 9.11 + barra única 56dp);
- (este) 0.9.4-d — bump versionCode 47/0.9.4 + RELATÓRIO.

## 10. CI / APK

- G0: run **37189852254** (workflow_dispatch @ 4555977) attempt 5 = SUCCESS
  5/5 jobs — NOTA: as runs 37184905959 (push) e re-runs morreram 5× com
  "The runner has received a shutdown signal" (exit 143, SEM erro de
  código — flake do pool de runners do GitHub em 2026-10-04 07:45-08:15
  UTC; o job gêmeo core-tests compilava VERDE nas mesmas janelas). O
  dispatch novo + reruns até verde; nenhuma mudança de código entre
  tentativas;
- G1: run **37190933465** (@ aa0162a) = SUCCESS na 1ª tentativa;
- G2: run (colar na submissão) (@ e7ddf66);
- APK: `goni-vv-0.9.4-release-signed` (artifact do job APK release arm64 —
  assinado via secrets; apksigner verificado pelo CI).

## 11. NÃO VERIFICADO (honesto — como verificar no C33/RMX3624)

1. **60fps no C33 com a FASE 9 toda** — verificar: abrir o editor, animar
   o drawer, abrir Inspector/hierarquia e ler o FPS da barra de status
   (spec E) durante 30s; PASSA se manter 60 (o atlas cresceu 512KB — R8,
   C33 tem folga);
2. **Teclado GBoard real no editor de script** — o harness aferva o caminho
   do IME (fila + commitText), mas o teclado REAL do dono: abrir um script,
   digitar uma linha com acentos, fechar com back, reabrir — a fonte tem de
   estar lá;
3. **Grelha adaptativa do ecrã de projetos no device** (AUTO_FIT ÷180dp):
   rodar o gestor em portrait e landscape — 2+ colunas, 16:9, uma fila
   inteira visível em landscape 20:9 sem rolar;
4. **Long-press do nome truncado**: criar um TIC com nome comprido na
   hierarquia, segurar o dedo ~0,5s sobre o nome → toast com o nome completo;
5. **layout.json no device**: arrastar o drawer devagar e parar → UMA linha
   "layout guardado (painel de baixo)" no logcat ~1,5s depois (não 4 em 40s);
6. **Acentos na CONSOLA**: provocar um erro com acento (ex.: cena inexistente
   pela ação de UI) e ler a linha na Consola — "não existe" com o ã desenhado.

## 12. DECLARAÇÃO DE AUSÊNCIA DE ACHISMO

Cada ponto tem a causa com ficheiro+função ANTES do fix (secções 3-4);
cada fix tem teste/harness que o vigia (secções 5-6); as duas mudanças de
lógica autorizadas (G0-1 e G1-6) têm prova de mutação colada (secção 14);
o que não foi verificado no device está listado com o passo de verificação
(secção 11). Nenhuma afirmação deste relatório se baseia em "deve
funcionar".

## 13. TABELA DE RASTREABILIDADE (ponto → causa → fix → teste → harness)

| ponto | causa (ficheiro:função) | fix | teste | harness |
|---|---|---|---|---|
| G0-1 | main.cpp openScriptEditor (portrait+IME mesmo frame) + INIT_WINDOW (handle stale) + ScriptEditor (sem caret) | caret + teclado in-app + re-validação por nome + IME re-pedido | R-007 regress_script_typing | 9.1-9.4 |
| G0-2 | scriptwin::open (buf vazio) | esqueleto + cursor no interior | scriptwin esqueleto (suíte) | 9.1/9.5 |
| G0-3 | SettingsPage.cpp scrollTap case kBitDocs (nunca re-despachava) | walk espelha o draw; lupa na toolbar do editor | settings-docs-tap reescrito | 9.5/9.6 |
| G0-4 | Toolbar.cpp kTbSlidersId + ViewportChrome kVpSettingsId (mortos) | REMOVIDOS (decisão registada) | topbar sem sliders (test_toolbar) | 9.11 (barra única) |
| G1-1 | vpchrome::draw centerRect drawerH=0 | drawerH REAL; ícones; '+' à direita | vpchrome_toolbar_ancorada_g11 | 9.9 |
| G1-2 | FontAtlas ASCII 32..126 | 4 ranges + UTF-8 + cobertura exigida + strings | R-008 + textfit code points + gate | 9.7 |
| G1-3 | tint f32[3]→APIs f32[4] + clamp 64px | tint4 alfa 1 + linha reservada por célula | material_legendas_inteiras_g13 | (desenho no 9.7) |
| G1-4 | onCreate emptyBox z-order + refresh parcial + header 4 linhas + 2 colunas fixas | emptyBox inteiro + buildTopBar + AUTO_FIT ÷180dp | projects_ui_check REESCRITO | (Java — structural gate) |
| G1-5 | saveLayoutNow todo frame | debounce 1,5s + PAUSE flush + sombra | (9.8 afira o fluxo real) | 9.8 |
| G1-6 | CamGizmo.cpp pickSceneTic (centro 44px) | AABB projetado + mais próximo da câmara | R-009 cameratic_pick_pelo_corpo_g16 | 9.10 |
| G2-7 | EditorUi.cpp ícone sem BodyComp | hierIconFor (BodyComp primeiro) + Static/Rigid | hierarquia_icone_por_tipo_g27 | 9.11 |
| G2-8 | labels debug + body 1 linha + scroll persistente | TwoCol ×3 + Posição + inspPrevSelected | física-2-col + ao-topo (g28) | 9.11 |
| G2-9/10 | 2 faixas 104dp + triad fora do mock | barra única 56dp; triad removido | topbar_unica_56dp (g29) + test_toolbar | 9.11 |
| G2-11 | Duplicate rect+plus (parecia "quadrado com ponto") | glifo Copy (2 quadrados) | ícones 51 (unicidade) | — |

## 14. PROVAS DE MUTAÇÃO (coladas — vermelho → verde)

### G0-1 (re-validação do TIC dono desligada nas DUAS camadas)

```
== C33 VIRTUAL (mutação): 177 check(s), 4 falha(s) ==
  > 9.3 teclado in-app: toque no corpo + tecla (G0-1)
    [FAIL] o MAIN re-validou o TIC dono por NOME pos-reload (G0-1)
  > 9.4 fechar: a fonte grava no componente (G0-1)
    [FAIL] a fonte digitada FICA gravada no ScriptComp (pelo nome)
  > 9.5 Docs: lupa do editor + pesquisa filtra (G0-3)
    [FAIL] a fonte gravada SOBREVIVE no disco (round-trip .goni)
    [FAIL] script EXISTENTE reabre com a fonte guardada intacta (G0-2)
== HARNESS VERMELHO — release BLOQUEADA ==
(reposto → 177/177 verde; ficheiros mutacao-G0-1-*.txt)
```

### G1-6 (pickSceneTic com o rect do CORPO desligado — o 0.9.3 restaurado)

```
cameratic_pick_pelo_corpo_g16 ... FALHOU test_cameratic.cpp:718
  FALHOU test_cameratic.cpp:751 pickSceneTic(...) == hBig
$ ./test_core | tail -1
2 teste(s) com falha

  > 9.10 tocar no corpo seleciona o TIC (G1-6)
    [FAIL] pickSceneTic: o toque no CORPO seleciona o TIC (G1-6)
    [FAIL] o TAP no corpo do cubo SELECIONA-o (o caminho da UI)
    [FAIL] a mudança de seleção LOGA com o motivo (toque no viewport)
    [FAIL] a limpeza também LOGA (cada mudança com motivo)
== C33 VIRTUAL: 200 check(s), 4 falha(s) == HARNESS VERMELHO ==
(reposto → 759/0 core + 209/209 harness; ficheiros mutacao-G1-6-*.txt)
```

## 15. DECISÕES DA FASE

- **Barra única 56dp** (G2-10): os ~48px vão INTEIROS ao viewport central
  (`safe::kToolbarH` é a fonte única — todos os layouts derivam); as tabs
  [3D|UI|ÁUDIO] ficam ao centro com underline no fundo da barra;
- **Botões mortos**: REMOVIDOS (regra do dono), ids liberados, decisão
  registada no inventário G0-4 (commit 4555977) e executada em G1-1/G2-9;
- **"Audio" sem acento no gate**: é NOME de TIC serializado no .goni —
  acentuá-lo partiria o round-trip das cenas (documentado no script);
- **dp/nada de phone-specific**: grelha AUTO_FIT ÷180dp (mín. 2), alvo 48 /
  desenho 24 / ícone ≥20dp, tudo em dp — G.One futuro no desktop não é
  bloqueado (hover/rato/⋮ não implementados, só não impedidos);
- **Flake de runners do GitHub (2026-10-04)**: 5 mortes exit-143 SEM erro
  de código no mesmo job que compilava verde 30min antes; tratado com
  dispatch + reruns (evidência na secção 10) — nenhum "fix" de CI por
  achismo.

## 16. VEREDITO

Os três grupos estão commitados com CI verde (G0: run 37189852254 att.5;
G1: run 37190933465; G2: secção 10). A suíte fecha em **759 casos core /
209 checks do dispositivo virtual / 6 gates / sentinelas R-001..R-009**,
com as duas provas de mutação obrigatórias coladas. O que falta ao
VEREDICTO total do dono é a verificação no device físico (secção 11 —
6 passos explícitos): instalado o `goni-vv-0.9.4-release-signed`, a
checklist da FASE 9 deve passar item a item.
