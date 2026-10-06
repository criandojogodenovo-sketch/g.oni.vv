# RELATÓRIO 0.9.6.10 — GRUPO UI: A REESCRITA DA APRESENTAÇÃO (spec G · grafite+âmbar+vidro)

> A ordem expressa do dono: a camada de UI em scope de REESCRITA (a
> atual era inaceitável — a imagem 2 como anti-exemplo: ícones soltos
> sobre a grelha, blocos cegantes), SEM tocar na lógica V.ONI, seleção,
> física, TICs, render ou storage — só apresentação e organização.
> A imagem 1 (o desktop G.ONI do dono) é o ALVO de estrutura, densidade
> e inventário; o azul dela é o nosso âmbar.

## 1 · O pedido e o método

As DUAS imagens foram LIDAS (VLM + sonda por-pixel) antes de mexer: a
imagem 1 deu o inventário de 8 regiões; a imagem 2 (o device real)
deu o ANTI-EXEMPLO — o [+] gigante e a tecla Mover como BLOCOS BRANCOS
SÓLIDOS (o accent mono #F5F5F5 em fills), os ícones undo/redo/save, o
«Mover» e a toolbar inferior a FLUTUAR sobre a grelha sem painel-mãe, a
barra branca órfã na hierarquia. Depois: região a região (topo →
hierarquia/ficheiros → viewport → assets/consola → inspector → status),
commit + suíte verde a cada uma — 7 commits (c6df876 · fc0db42 ·
d338ca9 · d95d7ff · 24c26dd · 8079f59 · a50ff89).

## 2 · A tabela de tokens aplicada (spec G — a decisão do dono)

| token | valor | uso |
|---|---|---|
| bg | #0E0E10 α1 | fundo/clear da app (o grafite) |
| surface | #161618 α0.80 | o VIDRO 80% — painéis dokados |
| surface2 | #202023 α0.86 | vidro denso — os PAIS flutuantes, premido |
| border | #2E2E32 α0.55 | hairline de grafite |
| glassEdge | #FFFFFF α0x1F | **o BORDO do vidro** (a assinatura — Δ28 sobre o céu) |
| glassTop | #FFFFFF α0x0A | o highlight do topo do vidro |
| text1 | #ECECEE | texto primário (15,3:1 sobre surface) |
| text2 | #A6A6AD | texto secundário (7,5:1) |
| accent | **#FFB020** | O ÂMBAR — a identidade (9,9:1) |
| accentPress | #E09A00 | premido (feedback) |
| accentInk | #0E0E10 | tinta sobre âmbar (10,5:1) |
| **accentDim** | **#4A3714** | o FILL DA SELEÇÃO (âmbar 25% s/ grafite; text1 9,6:1) |
| danger | #E5484D | semântica (4,6:1) |
| warn | **#FF8A3D** | LARANJA — hue 24° ≠ âmbar 39° (7,7:1) |
| ok | #46A758 | semântica (6,0:1) |
| axisX/Y/Z/dim | #E9493B/#5ACB5F/#4F92F5/#9E9E9E | os EIXOS (conteúdo; MORAM no Theme — gate R-030) |

AUDITORIA (o padrão honesto, PURA no CI): sobre surface SÓLIDA e sobre
o VIDRO REAL (α0.80 sobre o bg — o que está de facto por trás dos
painéis dokados) TODO o texto ≥4,5:1 e todo o componente ≥3:1. O caso
patológico (cena BRANCA PURA por trás do vidro 80%): text1 8,2 ✓ e
accent 5,3 ✓ (o que PODE flutuar); text2 4,0 e danger 2,5 NÃO flutuam
sobre a cena viva — A REGRA DA CASA (documentada no Theme.h e vigiada na
sentinela): texto secundário e severidade vivem em painéis dokados.

## 3 · Região → ficheiro → teste

| região | ficheiro | teste |
|---|---|---|
| TEMA (tabela+gate+ícone) | ui/Theme.h · ui/UiContext.h · scripts/theme_hex_check.py · scripts/gen_app_icon.py · ProjectManagerActivity.java | theme_tabela_spec_g_exata · theme_contraste_auditoria_spec · regress_identidade_grafite_ambar (R-028 recalibrada) · o GATE R-030 no CI · FASE 13.9 (o mono morto/o clear/o vidro nos píxeis) |
| TOPO (logo/menu 6 secções/stop/plataforma) | ui/Toolbar.{h,cpp} · drawFileMenu (EditorUi.cpp) · main.cpp (dispatch 1..14) | topbar_unica_56dp · FASE 13.1 (a toolbar povoada; o pixel do painel = o composto do vidro) · menu_sheet_ancorado (recalibrado ao cabeçalho de secção) |
| HIERARQUIA+FICHEIROS | drawHierarchy (seleção accentDim+barra) · BottomPanel.cpp (a árvore res:// + o browser) · main.cpp (refreshFilesTree) | hierarquia_* de sempre · FASE 13.7 ao device (validador 0/0) · consola_chips (recalibrado às tabs) |
| VIEWPORT (painel-mãe) | ui/ViewportChrome.{h,cpp} (a strip/o rail/a toolbar/o +) | vpchrome_toolbar_ancorada · FASE 13.6 (a invariância 1.0↔2.0) · 13.7 (o validador — a colisão do [+] com o stack de 5 colunas apanhada e morta por construção) · 13.9 (o vidro do rail no píxel) |
| ASSETS+CONSOLA | BottomPanel.cpp (as 4 tabs; o comando) · main.cpp (propósito 9: limpar/ajuda/play/stop/snap) | consola_chips_filtram (as tabs) · layout round-trip (0..4) |
| INSPECTOR | drawInspector (X/Y/Z coloridos; as tabs Inspector/Nós) | inspector_* de sempre · 13.6/13.7 (os pisos 48dp das tabs apanhados ao vivo pelo validador) |
| STATUS BAR | drawStatusBar (BottomPanel.cpp) · main.cpp (StatusBarData real) | toast_tokens · FASE 13.2 (o validador verde) |
| E5 caret + E1 header | ui/ScriptEditor.{h,cpp} | **regress_caret_na_banda_dos_glifos (R-029 NOVA)** · regress_barra_simbolos_ime (R-027: o Stop regista SEMPRE) · FASE 13.8 (o título FICA e não sangra — o contrato E1) |

## 4 · As PNGs comparadas (P-05)

- `docs/antes-0.9.6.9-mono.png` — o ANTES (o export do harness com o
  tema do Grupo F e o chrome solto).
- `docs/depois-0.9.6.10-grafite-ambar.png` — o DEPOIS (o MESMO export,
  re-vestido; 1536×720 @1.0).
- `docs/depois-0.9.6.10-device.png` — o DEPOIS ao TAMANHO do device
  (1600×720 @2.0 = 776×336dp; o validador 0/0).
- `docs/lado-a-lado-0.9.6.10.png` — **o ANTES | o DEPOIS | A IMAGEM 1**
  lado a lado (4704×720).
- `docs/icon-gone-512.png` — o ícone novo (G âmbar + 4 setas #ECECEE
  sobre grafite; a prova por-pixel: 9469 px âmbar sobre 52749 grafite;
  round==square byte-a-byte).

A LEITURA do lado-a-lado pelo VLM (glm-5v): o DEPOIS é «a MESMA família
de editor» que a referência (top bar/hierarquia/viewport central/
inspector/dock — o critério (a) da aceitação) e os QUATRO elementos
flutuantes estão «visibly enclosed within a dark semi-transparent glass
panel with distinct visible edges» — o critério (b); no ANTES eles
apareciam «as loose floating icons» (a citação exata do anti-exemplo).

## 5 · O que ainda NÃO bate certo com a imagem 1 (a lista honesta)

1. **A tab de modo «2D»**: a referência tem 3D/2D/UI/Áudio — o motor
   NÃO tem modo 2D (uma tab morta seria funcionalidade falsa); ficam as
   3 reais (3D/UI/ÁUDIO).
2. **O seletor de plataforma**: a referência tem um dropdown com alvos;
   o nosso chip «Android» INFORMA o alvo REAL (Mobile/Android) ao toque
   — não há outros alvos para escolher (zero fingimento).
3. **A segunda tab da viewport** («Viewport 3D» ao lado de «Cena»): o
   motor tem UMA vista; a strip traz a tab «Cena» (a ativa) + os chips
   de estado reais (Perspetiva/Global) — a segunda tab seria decorativa.
4. **Miniaturas de MESH 3D**: as TEXTURAS desenham a textura em si
   (imageQuad real); meshes e áudio ficam com o ícone de tipo — uma
   pré-visualização 3D por asset precisaria de um renderer de preview
   (fora do scope «não tocar no render»).
5. **A MEMÓRIA na status bar**: a referência mostra «Memória: N MB» —
   não há API de memória da app sem JNI novo (fora do scope); a linha
   traz versão/projeto/FPS/objetos/estado/Mobile First (tudo real).
6. **«Sombras/Colisor» toggles do Mesh**: a referência mostra 3 toggles;
   o MeshRenderer real não expõe flags de sombra/colisor — o Inspector
   mantém as propriedades REAIS (visível/cor/transform/malha/textura).
7. **A densidade de conteúdo**: a referência é um desktop com uma cena
   rica (hierarquia cheia, inspector povoado, assets com thumbs) — o
   nosso export mostra a cena de teste do harness; a DENSIDADE dos
   componentes é a da referência (headers/tabs/caixas X-Y-Z/contagens).
8. **O seletor «Cena ▾» ao lado do Menu**: fica (a troca de cenas real);
   a referência tem-na na menubar — a nossa mobile-first funde-a no botão
   próprio (o menu ≡ tem a secção CENA com guardar/carregar).

## 6 · As sentinelas e as mutações

- **R-028 → regress_identidade_grafite_ambar** (recalibrada à spec G):
  a rampa grafite R==G com B−R ≤8 (o tinge frio do text2 #A6A6AD é 7 —
  NÃO é azul); o AZUL de verdade morto (B−R > 25 em NENHUM chrome); o
  âmbar quente R>G>B; o warn laranja ≠ âmbar; as DUAS auditorias do
  vidro (a real ≥4,5; a patológica só para o que flutua); os aliases
  bit-a-bit. MUTAÇÃO R-028g (o azul #2196F3 de volta na tabela E no
  alias): 4 testes vermelhos (16 asserções) — a 1ª tentativa SÓ na
  tabela morreu no static_assert (a fonte única é load-bearing).
- **R-029 NOVA (E5)**: regress_caret_na_banda_dos_glifos. MUTAÇÃO
  R-029a (a fórmula antiga de volta): a sentinela VERMELHA.
- **R-030 NOVA (o gate do dono)**: theme_hex_check no CI. MUTAÇÃO
  R-030a (o hex órfão plantado): o gate VERMELHO com a linha exata.
- **R-027 (recalibrada)**: o alvo do Stop regista SEMPRE no audit (o
  botão existia em editor mas NÃO constava do orçamento do device).

## 7 · O estado da suíte e do CI

core **844/0** (843+1) + c33_virtual **440/440** + os gates
check_main/link_parity(115)/jni_parity(7)/glyph_source/docs_lint(52)/
projects_ui/**theme_hex(R-030)**/release_identity VERDES. O CI: verde
nos commits c6df876/fc0db42/d95d7ff/24c26dd/8079f59/a50ff89; o commit
intermédio d338ca9 (E5+E1) ficou VERMELHO na FASE 13.8 — o contrato do
título mudou com o E1 (o título deixa de SUMIR) e a recalibração do
cheque chegou no commit SEGUINTE (d95d7ff, verde) — a sequência honesta
está no histórico.

## 8 · Os NÃO VERIFICADO honestos (só no device se confirmam)

1. O FEEL do âmbar aceso (o Stop em play, o warn laranja) ao olho do
   dono — os píxeis e os contrastes estão afervados, o gosto não.
2. A MINIATURA REAL das texturas no device (o resolver do GpuAssets no
   APK — o harness desenha com o stub; o caminho é o mesmo do canvas).
3. O teclado no campo de comando (o propósito 9 abre o IME como os
   outros; a mediana de escrita é a de sempre).
4. O «⋯» do script editor no portrait REAL do C33 (a geometria está
   afervada ao 720×1600@2.0 do harness).
