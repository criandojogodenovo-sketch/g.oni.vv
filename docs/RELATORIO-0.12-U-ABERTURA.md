# RELATORIO-0.12-U-ABERTURA.md — Campanha 0.12-U · V.ONI Bloco A (Redução de Trabalho)

Atualizado: 2026-10-19 · versionCode de partida: **97** · último commit da
0.12-E: `22f5d78` (CI run 39160170296, 5 jobs verdes).

Cláusula calma: só o que está no prompt da campanha. Nada de features fora
das sub-fases listadas. Regra de convivência: o Bloco A NÃO se declara
fechado até o platformer do critério correr.

## §1 · O estado real (VERIFICA §0.2 — as 4 verificações)

| # | Verificação | Resultado |
|---|---|---|
| 1 | Campanha E (UI 2.5D) fechou com CI verde? | **VERDE** — a cadeia E-1 (vc 95) → E-2 (vc 96) → E-3 (vc 97, `43650d8`) está no log; o `versionCode 97` confirmado no `app/build.gradle`; o run do HEAD (`22f5d78`) fechou **success** nos 5 jobs (C33 virtual, testes do core, entry symbols, APK release arm64 assinado; keystore skip esperado) |
| 2 | git status limpo? | **LIMPO** — o clone local acusava 630 ficheiros «modificados» com `0 insertions(+), 0 deletions(-)`: só bits de modo (100644→100755) do umask da sandbox, ZERO mudança de conteúdo; `core.filemode=false` local normalizou e o status ficou vazio. `main` == `origin/main` (0/0) |
| 3 | Gates verdes (docs-lint, R-013, scope-check)? | **VERDE** — o job "C33 virtual (dispositivo + sentinelas + gates)" do run 39160170296 fechou success (é onde os gates correm); o test_core do run também success. Prova local adicional: o R-013 regenerado (`voni_refgen`) confere byte a byte (ver #4) |
| 4 | VONI_referencia.md e llms-full.txt iguais ao gerado? | **IGUAIS** — o `voni_refgen` compilado localmente (g++ 17) regenerou a referência e o `diff` contra os dois ficheiros commitados deu idêntico: 11908 bytes cada, byte a byte |

As quatro verificações passam. A campanha 0.12-U arranca do estado real
acima — nenhum reparo prévio foi preciso (o status sujo era phantom de
sandbox, documentado aqui e não no repo).

## §2 · As 7 features do Bloco A

O prompt manda três sub-fases; o conteúdo são SEITE features (a fonte
única de cada uma é o REGISTO CENTRAL — cada entrada nova é uma linha em
`app/src/main/cpp/voni/VoniRegistry.cpp`, com Docs/equiv/exemplo
obrigatórios; a referência pública regenera a cada sub-fase e o R-013
afere byte a byte):

| # | Feature | Sub-fase | Contrato |
|---|---|---|---|
| 1 | **View P universal** | 3.9a | `View P` aceita expressão de TODOS os 7 tipos (Int Num Txt Bool Vec2 Vec3 TIC) via `valueText()` — o texto entre aspas de sempre continua igual (compat byte a byte) |
| 2 | **Matemática inline — 13 funções** | 3.9a | `abs, min, max, clamp, lerp, floor, ceil, round, sqrt, sin, cos, random, random_int` — chamadas como expressão; `sin`/`cos` em GRAUS |
| 3 | **distance** | 3.9a | `distance(TIC\|Vec3, TIC\|Vec3) → Num` — aceita nome de TIC (resolve a pos via Host) ou Vec3 |
| 4 | **spawn** | 3.9b | `spawn(tipo, x, y, z) → TIC` — `Host::spawnTic`; erro com linha se o tipo não existir (tipos vivos: `esfera`, `box` — a decisão 0.8.10); o TIC nasce active/visible/nome único |
| 5 | **every(s)** | 3.9c | corre o bloco a cada `s` segundos (acumulador por nó AST) |
| 6 | **after(s)** | 3.9c | corre o bloco UMA vez passados `s` segundos |
| 7 | **when(cond)** | 3.9c | corre o bloco UMA vez quando a condição fica verdadeira (subida) |

Anatomia por sub-fase (o formato de commit e a política de mutação são os
do prompt — pin verde → quebra fix → pin vermelho com a mensagem esperada
→ outputs colados no relatório → reposto → verde):

- **3.9a — View P universal + matemática + distance**: pins ×15;
  sentinela `regress_voni_matematica` (R-052); mutações M-3.9a1, M-3.9a2,
  M-3.9a3; fecho vc 97 → 98.
- **3.9b — spawn(tipo, x, y, z) → TIC**: pins ×7; sentinela
  `regress_spawn` (R-053); mutações M-3.9b1, M-3.9b2; fecho vc 98 → 99.
- **3.9c — every(s) / after(s) / when(cond)**: nova sintaxe no AST +
  parser (o estado vive POR NÓ AST); pins ×11; sentinela
  `regress_voni_timers` (R-054); mutações M-3.9c1, M-3.9c2, M-3.9c3;
  fecho vc 99 → 100.

## §3 · O critério do platformer (o juiz final)

A regra de convivência da campanha: **não declarar o Bloco A fechado até
o platformer do critério correr**. O critério é:

> UM ÚNICO script `.voni` de platformer que usa AS SETE FEATURES em cadeia
> — `spawn` ergue a plataforma e a moeda, a matemática (`clamp`, `lerp`,
> `abs`, `random_int`…) move o jogador e decide o salto, `distance` mede a
> chegada, `when` apanha a queda e repõe, `after` fecha a porta e `every`
> faz a moeda pulsar — corre num pin da suíte contra um Host controlado
> (dt simulado, frame a frame) e a sequência observável (o log do `View P`
> e os estados que o pin lê) tem de bater a esperada, do arranque ao
> objetivo. O pin corre no `test_core` local E no CI.

O Bloco A só fecha no FECHO com este pin verde — ele é o juiz final; os
pins e sentinelas das sub-fases são a prova por peça, o platformer é a
prova do todo.

## §4 · As regras fixas da campanha (do prompt, sem reescrita)

1. **O registo é fonte única**: cada entrada nova é uma linha em
   `VoniRegistry.cpp`; `VONI_referencia.md` + `llms-full.txt` regeneram a
   cada sub-fase (R-013 afere byte a byte).
2. **REGRA CTX**: o commit de cada sub-fase TEM de incluir o STATE.md
   atualizado (§1.2 — o gate `docs_lint_state.sh` é o árbitro; o teto do
   pin `ctx_state_n` é <300 linhas — o índice comprime, o conteúdo não
   mente).
3. **Formato de commit**: `<sub-fase>: <frase>` + o que mudou + pin +
   sentinela R-0NN nova + versionCode + relatório próprio
   (`docs/RELATORIO-0.12-U-<SUBFASE>.md`).
4. **Prova de mutação obrigatória** (política #5 do REGRESSOES.md):
   pin verde → quebra fix → pin vermelho com a mensagem esperada →
   outputs colados → reposto → verde.
5. **FECHO**: o relatório de fecho da campanha
   (RELATORIO-0.12-U-FECHO.md — nasce NO fecho) + README +
   ARCHITECTURE §5 (as entradas novas) + BACKLOG + referência
   regenerada + CI verde. PARAR.
