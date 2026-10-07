# RELATORIO-0.9.6.15 — UI PASSO 2 · PAINÉIS

> Campanha UI do dono (spec «só layout; sem lógica nova»), PASSO 2 de 0–4.
> Este relatório segue as cláusulas permanentes: P-01 (tudo lido do fonte,
> zero adivinhação), P-04 (o que NÃO foi verificado fica listado), P-05
> (evidência PNG exportada), P-07 (a checklist do device fica por preencher
> — o sign-off é do dono), P-08 (o contrato `docs/LAYOUT_HIERARCHY.md`
> atualizado e o gate `hierarchy-check` verde).

## 1. O QUE O PASSO 2 PEDIA (e onde vive no código)

| Pedido da spec (PASSO 2) | Onde ficou (fonte único) |
|---|---|
| Hierarchy 18% (min 140dp) | `safe::kHierPct=0.18` + `kHierMinW=140` em `ui/SafeArea.h` — consumidos por `resolvePanels()` (a fonte única que os painéis, o centerRect, o drag e os testes partilham) |
| Inspector 22% (min 180 / max 260dp) | `safe::kInspPct=0.22` + `kInspMinW=180` + `kInspMaxW=260` (o TETO é novo no clamp do drag) |
| Sem seleção → trilho 32dp | `safe::kInspTrackW=32` + `editor::inspectorCollapsed()` (EditorUi.h — a pergunta ÚNICA) consumida por `resolvePanels(..., inspTrack)`, `drawInspector`, `dividerRects` e o `centerRect` do main |
| Viewport ≥55% | consequência dos defaults (18+22=40%); PROVADO na sentinela R-033 nos ecrãs de referência + medido na TABELA §3 |
| Pega de arrasto 24dp | `bottom::kDrawerHandleH=24` (era 12) + `editor::kDividerHitW=24` (era 20; o strip visível segue 12dp, a folga vai PARA DENTRO do painel) |
| Bottom panel default FECHADO | `BottomState.bottomTab=0` já era o arranque; o AUTO-ABRIR da 0.8.0 (o `tlVisible && bottomTab==0 → 4` no main) foi REMOVIDO — o drawer abre só por toque na tab |
| Drawer aberto ≤35% da altura | `safe::kDrawerMaxPct=0.35` dentro de `effectiveDrawerH(raw, vpH, contentH)` — o CAP DUPLO (piso do viewport 88dp E teto 35%; o MENOR manda) numa única fonte usada pelo draw E pelo `currentDrawerH` |
| Consola: chips internas 28dp | `bottom::kConChipH=28` (era 36) com a flag do validador `kHeadFloorDp` nos 5 alvos da linha (chips + auto + export) |
| Eliminar a «Consola» duplicada | a linha de chips é `Logs/Erros/Avisos` (era `Consola/Logs/Erros/Avisos`) — a vista COMPLETA do log vive na chip «Logs» (filtro −1); `consoleTab` não é persistido no layout.json, logo não há migração de estado |
| «Erros» mostra erros | o filtro por nível já existia (`lineLevel` → E/); agora a chip Erros é o índice 1 e a FASE 13.7h PROVA o caminho todo (elog::error → engine.log → tap na chip → filtro ativo) |

## 2. OS ACHADOS DO CAMINHO (o que o código mostrou)

1. **A colisão latente das miniaturas de Material** (apanhada pelo
   `test_ui` a 260dp): o plano reservava `dp(44)+sp(12)` para a linha
   (`inspThumbsH`) mas o desenho punha miniaturas de **64px CRUS** + a
   legenda ~26px ABAIXO — a legenda desenhava-se DENTRO da linha
   seguinte desde o Grupo D. A 300dp não colidia com glifos por sorte
   geométrica (a legenda centrada não alcançava a coluna dos rótulos);
   a 260dp a colisão ficou real (penetração 2.1px «cor B»×«Textura»).
   FIX: o desenho tudo em dp (miniatura 44 — a medida do PASSO 1, que no
   device @2.0 era desenhada a METADE, 32dp) e o plano reserva o que o
   draw consome: `dp(4)+dp(44)+sp(12)+dp(4)`.
2. **O valor dos sliders fora do painel**: `sliderRow`/`uiSliderRow`
   alinhavam o valor a `kPanelW` (300dp FIXO) — com o painel de 22% o
   valor desenhava-se FORA do painel. FIX: os helpers recebem a largura
   real `w` e alinham a `x+w−kPad−tw` (idêntico ao anterior em 300dp).
3. **O trilho e o modo UI**: o colapso aplica-se SÓ ao editor 3D — o
   modo UI tem o contrato próprio de paridade WYSIWYG play/edit (os
   testes `uilayout_paridade_*` e o `renderBoth` provam-no); o main
   pergunta `!uiMode && inspectorCollapsed(st)`.
4. **Botões do inspector sub-piso**: os 15 botões de linha desenhavam
   `r.h−4` (34dp @2.0 < o piso 40) — invisível enquanto não havia
   seleção no frame auditado; com o TIC real da FASE 13.7 o validador
   apanhou. FIX: o alvo é a LINHA inteira (o padrão PASSO 1 das tabs) +
   a flag `kRowFloorDp`.
5. **Nomes que truncam POR DESENHO**: os nomes de linha da hierarquia/
   Nós têm tip de long-press (spec B) — o validador agora distingue o
   corte-de-desenho (`auditFitByDesignNext()` → `"fit_por_desenho"` no
   JSON) do corte-acidental. O placeholder da pesquisa degrada por
   ordem («pesquisar TIC» → «pesquisar» no piso 140) e o convite vazio
   também (ladder medida ao vivo — no piso sai «+ cria um TIC»), o
   MESMO padrão R-023 dos chips da strip.

## 3. TABELA DE MEDIDAS (o script `scripts/passo2_medidas.cpp` inclui o
SafeArea.h REAL e corre as MESMAS funções; união da cobertura em grelha)

### PERFIL B — device RMX3624 (776×336dp de content)

| Medida | px | dp | % | Critério |
|---|---|---|---|---|
| Top bar | 72 | 36 | 10.7% alt | — |
| Tab bar de baixo | 64 | 32 | 9.5% alt | — |
| topo+tab (chrome fixo) | 136 | 68 | **20.2%** | c ≤20% (mantido do PASSO 1) |
| Hierarquia (default) | 280 | 140 | **18.0%** larg | spec: 18% (piso 140) |
| Inspector (default, c/ seleção) | 360 | 180 | **23.2%** larg | spec: 22% (piso 180) |
| TRILHO (sem seleção) | 64 | 32 | 4.1% larg | spec: trilho 32dp |
| **Viewport central (c/ seleção)** | 912 | 456 | **58.8%** | **a ≥55% ✓** (era 37.1%) |
| **Viewport central (trilho)** | 1208 | 604 | **77.8%** | a ≥55% ✓ |
| Drawer efetivo (raw 240) | 224 | 112 | **33.3%** alt | **≤35% ✓** |
| Drawer efetivo (raw 400, o máximo) | 224 | 112 | 33.3% alt | o cap manda ✓ |
| Pega do drawer | 48 | 24 | — | spec: 24dp ✓ |
| Chips da consola | 56 | 28 | — | spec: 28dp ✓ |

### PERFIL A — harness (1536×720dp)

| Medida | dp | % | Critério |
|---|---|---|---|
| Hierarquia | 276.5 | 18.0% | ✓ |
| Inspector (teto) | 260 | 16.9% | o teto 260 manda ✓ |
| Viewport (c/ seleção) | 999.5 | **65.1%** | a ≥55% ✓ |
| Viewport (trilho) | 1227.5 | 79.9% | ✓ |
| Drawer efetivo (raw 240) | 240 | 33.3% | ≤35% ✓ |
| Drawer no máximo | 248 | 34.4% | o cap manda ✓ |

### Cobertura da viewport por controlos (critério b ≤10%)
Permanece o território do **PASSO 3** (VIEWPORT): a strip/rail/toolbar
de 40dp do PASSO 1 ainda cobrem ~84% da área do viewport no device —
o PASSO 2 não toca no chrome da viewport (a spec separa os passos).

## 4. PROVAS (as suítes e as mutações)

- **Suítes**: `test_core` **0 falhas** (858 testes) · `c33_virtual`
  **467/467** (10 checks novos da 13.7h).
- **Gates locais verdes**: hierarchy-check (o contrato §2.3/§2.4/§2.5 +
  a R-033 na tabela) · docs-lint 57 ficheiros · theme · jni-parity ·
  link-parity · check_main.
- **MUTAÇÃO 1** (o cap 35% morto em `effectiveDrawerH`): `test_core`
  **30 falhas** (R-033(5), R-022 §2.3, o clamp do wiring090) → reposta →
  verde.
- **MUTAÇÃO 2** (o trilho morto — `inspTrack` ignorado): `test_core`
  **4 falhas** (R-033(2)) → reposta → verde.
- **FASE 13.7h nova** (a evidência P-05): exporta o TRILHO e a consola
  ao projeto; o JSON prova o trilho de 64px (32dp @2.0), os 5 alvos de
  56px na linha dos chips (a duplicada saiu) e o tap na chip Erros a
  armar o filtro com linhas `E/GONI:` reais no engine.log.

## 5. EVIDÊNCIA PNG (P-05)

- `docs/passo2-device-trilho-32dp.png` — o device com o inspector no
  TRILHO (sem seleção): o viewport a 77.8% da largura.
- `docs/passo2-device-inspector-aberto.png` — o device com um TIC
  selecionado: hierarquia 140 + inspector 180 + viewport 456dp (58.8%).
- `docs/passo2-device-consola-chips-28.png` — o drawer na Consola: chips
  Logs/Erros/Avisos de 28dp (a «Consola» duplicada fora) + o campo de
  comando.

## 6. O CONTRATO (P-08)

`docs/LAYOUT_HIERARCHY.md` atualizado: a árvore §1 ganhou o TRILHO (com
a fonte `editor::inspectorCollapsed`), as regras §2 ganharam as
percentagens dos painéis (§2.4), o cap DUPLO do drawer + o arranque
fechado (§2.3) e as pegas de 24dp (§2.5); a tabela §3 lista as
constantes novas; a §4 lista a R-033. O gate `hierarchy-check` compara
os símbolos e está VERDE.

## 7. CHECKLIST DO DEVICE (P-07 — POR PREENCHER PELO DONO)

- [ ] O editor abre com o drawer FECHADO (nada auto-abre a Animação)
- [ ] Sem seleção: o inspector é o trilho de 32dp à direita e a cena
      ocupa a área dele; ao tocar num TIC na hierarquia o inspector volta
- [ ] O viewport 3D ocupa ≥55% da largura com o inspector aberto
- [ ] Arrastar a pega do drawer para cima NUNCA passa ~1/3 do ecrã
- [ ] As pegas dos painéis (hierarquia|viewport|inspector) agarram com
      o dedo (24dp) e o inspector não passa de 260dp nem desce de 180dp
- [ ] A consola: 3 chips (Logs/Erros/Avisos) de 28dp — SEM a «Consola»
      repetida; a chip Erros mostra as linhas de erro do log
- [ ] A hierarquia a 140dp: a pesquisa mostra «pesquisar» e a lista
      continua usável (nomes truncam com o tip do long-press)

## 8. NÃO VERIFICADO / PARA O DONO DECIDIR (P-04, zero adivinhação)

1. **«fixar aberto» do inspector** — a linha do PASSO 2 no BACKLOG
   menciona «trilho 32dp quando sem seleção + fixar aberto»; a spec do
   passo executada aqui não definiu a INTERAÇÃO de fixar (o trilho é
   informativo: o regresso é selecionar um TIC). Se o dono quiser o
   pin (tocar no trilho fixa o painel aberto), é uma linha de
   interação nova — pedir e implementar no PASSO 3.
2. **«consola ≥60%»** — a mesma linha do BACKLOG menciona «consola
   ≥60%»; a spec executada pedia chips de 28dp/duplicada fora/Erros a
   mostrar erros. A leitura provável (a LISTA de log ≥60% da altura do
   drawer aberto na tab Consola) não foi implementada por ambiguidade —
   medir e decidir com o dono.
3. **O sign-off P-07** das FASEs 13.7/13.7h no device real (a checklist
   §7) — o harness virtual confirma a matemática; o dedo no ecrã é do
   dono.
4. **CI** — o push deste passo dispara o release.yml; o estado final
   fica no commit (a regra da casa: o relatório não declara verde sem
   o run).
