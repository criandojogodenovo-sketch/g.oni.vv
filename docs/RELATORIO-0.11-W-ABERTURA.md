# 0.11-W — ABERTURA (Render Systems)

## §0 Estado de entrada

- 0.11-T fechada (vc 72, run 38573108989) — 3.5a (LOD + skinned cull)
- 0.11-T-FIX Perna 1 fechada (vc 73, run 38597261135 VERDE à primeira no
  fc51ff2, todos os jobs: core ✓ · c33 ✓ · APK arm64 assinado ✓ · symbol
  gate ✓) — **Perna 2: ADIADA pelo dono** (o mandato desta campanha é a
  0.11-W; a Perna 2 — job streaming-stress a 8 workers + diagnóstico por
  subtração — fica declarada no BACKLOG 0.11-T-FIX-Perna3 com o teto de
  workers a proteger o device entretanto)
- Baseline do dono no C33: 5 FPS · 6 TICs · bl 5164/5178 (a vista era
  quase vertical — a investigação do culling é obrigatória, §2/§4)
- A cena: 6 TICs com o MESMO mesh + material — a evidência métrica: 
  5178 = 6 × 863 EXATO (as tabelas somam um múltiplo perfeito do nº de
  blocos de UM .gmesh; meshes diferentes não produzem este alinhamento).
  A confirmação visual final (imagem do dono) fica no §3 do pin 3.5b —
  o pré-requisito do instancing.
- Próxima sentinela livre: R-047 (a R-045 é da 3.5a, a R-046 é do
  T-FIX Perna 1 — ambas com relatório próprio; o registo documental em
  docs/REGRESSOES.md continua como dívida declarada da 3.5a/T-FIX)
- versionCode: 73 (app/build.gradle:29) — o par do §14 do
  RELATORIO-0.9.6.md
- Fixture local (o equivalente da casa): as cenas sintéticas geradas em
  código do test_core/c33_virtual (o MESMO padrão de todas as campanhas:
  o city de 72 primitivas do SAF, o corredor de 50 personagens da 3.5a).
  O .gmesh de 203 MB do dono NÃO está no repo — os pins da 3.5b geram
  .gmesh equivalentes (mesmo mesh + N TICs) e o pin C33 real é do dono
  (o §3 de cada sub-fase). Sem ele o FPS final é o que o dono vê.

## §1 Objetivo desta campanha

Três sistemas que atacam o gargalo do render:
- 3.5b — GPU Instancing (N objetos iguais → 1 draw call)
- 3.75a — Streaming progressivo (camadas no .gmesh, sem freeze)
- 3.75b — Impostores (PNG 3D por distância)

## §2 Ordem de implementação

1. 3.5b (instancing) — o ganho imediato na cena do dono
2. 3.75a (streaming progressivo) — tira o freeze do import
3. 3.75b (impostores) — para mundos grandes

A investigação do culling (§1.8 do prompt — bl 5164/5178 com uma vista
quase vertical significa que o frustum cortou só 14 blocos de 5178)
corre ANTES do fecho da 3.5b: dump dos planos do frustum + dos AABBs de
blocos + do far da câmara, colado no §4 do relatório 3.5b. Se o culling
estiver partido, o fix é desta campanha; se estiver bem, o número
explica-se com números.

## §3 Cláusula calma

Zero features fora das 3 sub-fases. Zero mudança à linguagem V.ONI.
Zero refactor de UI alheia. Todos os campos novos são opcionais com
default que reproduz a 0.11-T byte a byte.

## §4 Alvo final

30+ FPS no C33 com a cena dos 6 modelos de 203 MB, sem crash e
sem freeze no import. Medido com o bench R-017. PNG do HUD.
