# RELATORIO-0.12-U-3.9c.md — every(s) / after(s) / when(cond)

0.12-U · Bloco A (Redução de Trabalho) · sub-fase 3.9c · versionCode
99 → **100** · o CI é o árbitro.

## §1 · O que mudou

1. **A sintaxe no parser** (`VoniGrammar.cpp` — o ficheiro único 🔶):
   `TimerStmt <- EveryStmt / AfterStmt / WhenStmt` com a forma comum
   `'every|after|when' '(' Expr ')' Block` — ANTES de Assignment/PathCall
   (a mesma razão dos loops). As 3 palavras entram em `kReserved` —
   `v#every:Int=1` morre com «'every' é uma palavra reservada» (a
   gramática não dá o nome ao utilizador e o statement não nasce
   partido).
2. **O AST** (`VoniAst.h`): `Stmt::Kind::Timer` + `TimerKind { Every,
   After, When }` + `timerArg` (o s ou a condição) — o corpo é o `body`
   de sempre (o campo do Repeat/Last).
3. **As ações** (`VoniCompile.cpp`): a ação partilhada `timerAction`
   construída POR VALOR (a lição FASE 9 — as ações vivem além do retorno);
   o validador caminha o corpo (`walkBlock`) e o `strayNameOf` diz
   «every»/«after»/«when» no erro do corpo do tyker (o erro DIZ o que
   entrou).
4. **O ESTADO POR NÓ AST** (`VoniInternal.h`): `Script::Impl::timerStates`
   — `unordered_map<const Stmt*, TimerState>` (acc/fired). A chave é o
   PONTEIRO do nó (o program é estável — shared_ptr; runs repetidos não
   recompilam). O `reset()` do stop() limpa — o restart recomeça do zero.
   ZERO static/global: o estado de DOIS scripts não se toca e o do MESMO
   script morre com ele.
5. **Os contratos fechados** (`VoniVm.cpp` — decisões 🔶):
   - **every(segundos)**: acumula dt; ao atingir o período dispara e o
     RESTO CARREGA (sem drift); dispara no máximo 1× por frame (um dt
     gigante nunca dispara em rajada); período ≤ 0 = erro com linha;
   - **after(segundos)**: o relógio começa no 1.º ENCONTRO da linha;
     dispara 1× e fica inativo para sempre; tempo ≤ 0 = erro com linha;
   - **when(condição)**: dispara 1× TOTAL no 1.º frame em que a condição
     está verdadeira (se já nasce verdadeira, é no 1.º frame); não-Bool =
     erro com linha; (o achado do §3: a «borda de subida» era
     INOBSERVÁVEL dado o portão 1× — saiu do código, o contrato é o
     portão);
   - o corpo NÃO é ciclo: resume/continue referem-se ao construto de
     FORA (o flow propaga) — o return devolve da fn.
6. **As 3 entradas no registo** (`VoniRegistry.cpp`, Kind::Linguagem —
   Docs/equiv/exemplo completos) + a referência pública regenerada
   (15534 → 16566 bytes; 64 entradas; o R-013 afere byte a byte).

## §2 · A prova (os pins)

12 pins NOVOS em `tests/test_voni.cpp`:
`every_cada_meio_segundo` (10×0.1s = 2 disparos) ·
`every_o_resto_carrega_sem_drift` (10×0.2s = 4 disparos EXATOS — o
contrato anti-drift) · `every_intervalo_invalido_erro_com_linha` (0 e
"a") · `after_dispara_uma_vez_e_morre` (20 frames = 1 log) ·
`after_dois_nos_independentes` (dois relógios autónomos no mesmo
allmoments — a prova do estado por nó) · `after_tempo_invalido` ·
`when_dispara_na_subida_uma_vez` (vida<3 sustentado = 1 log) ·
`when_uma_vez_no_total_mesmo_com_duas_subidas` (t 3-4 e t 7-8 — a 2.ª
subida é morta: 1× TOTAL) · `when_nasce_verdadeira_dispara_no_primeiro_
frame` · `when_tipo_errado_erro_com_linha` ·
`timers_o_estado_morre_no_stop` (a 2.ª vida dispara de novo) ·
`timers_no_corpo_do_tyker_sao_rejeitados_que_ensinam` («'every' é código
de script»).

**Sentinela R-054** (`regress_voni_timers`,
tests/test_sentinels.cpp): (1) as 3 entradas (Linguagem, Docs completas)
+ as 3 palavras reservadas; (2) o every com o resto a carregar (4 exatos
em 10×0.2s); (3) o after 1× em 20 frames; (4) o when 1× no nível
sustentado; (5) o stop() mata o estado por nó (a 2.ª vida dispara de
novo).

**Recalibração com nota**: `docs_lista_reservadas_completa` (tests/
test_voni.cpp) — o `reservedWordCount` era 20, passa a **23** (os timers
every/after/when são RESERVADOS por desenho: a gramática não dá o nome
ao utilizador e o statement não nasce partido); o pin ganha as 3
afirmações novas + a nota no sítio.

**Verificação**: test_core 0 falhas (os skips de disco/sandbox
conhecidos; o CI é o árbitro) · c33_virtual 1010/0 · os gates locais
verdes · `ctx_state_n` (299 < 300) · `ctx_codemap_n` (399 < 400) ·
`ctx_refs_ok`.

## §3 · PROVA DE MUTAÇÃO (política #5 — os outputs colados)

### M-3.9c1 — o resto do every não carrega (drift)

Quebra: `ts.acc -= period;` → `ts.acc = 0.0;`.

```
$ GONI_ONLY=every_o_resto ./test_core
every_o_resto_carrega_sem_drift                ...
  FALHOU  tests/test_voni.cpp:2524  h.logs.size() == 4
every_o_resto_carrega_sem_drift                FALHOU
1 teste(s) com falha

$ GONI_ONLY=regress_voni_timers ./test_core
regress_voni_timers                            ...
  FALHOU  tests/test_sentinels.cpp:7476  o every disparou 3 vezes em 10×0.2s (o contrato é 4 — o resto do período deixou de carregar?)
regress_voni_timers                            FALHOU
1 teste(s) com falha
```

Reposto → verde (0 falhas nos dois).

### M-3.9c2 — o portão 1× do after fora

Quebra: o `if (ts.fired) return Flow::None;` do after → `if (false)`.

```
$ GONI_ONLY=after_dispara_uma_vez ./test_core
after_dispara_uma_vez_e_morre                  ...
  FALHOU  tests/test_voni.cpp:2551  h.logs.size() == 1
after_dispara_uma_vez_e_morre                  FALHOU
1 teste(s) com falha

$ GONI_ONLY=regress_voni_timers ./test_core
regress_voni_timers                            ...
  FALHOU  tests/test_sentinels.cpp:7494  o after disparou 10 vezes (o contrato é 1× e morre)
  FALHOU  tests/test_sentinels.cpp:7529  h.logs.size() == 1
  FALHOU  tests/test_sentinels.cpp:7535  o after da 2.ª vida disparou 10 vezes (o estado por nó sobreviveu ao stop — viveu num static?)
regress_voni_timers                            FALHOU
```

Reposto → verde.

### M-3.9c3 — o portão 1×-total do when fora

Quebra: o `ts.fired = true;` do when removido.

```
$ GONI_ONLY=when_uma_vez_no_total ./test_core
when_uma_vez_no_total_mesmo_com_duas_subidas   ...
  FALHOU  tests/test_voni.cpp:2637  h.logs.size() == 1
when_uma_vez_no_total_mesmo_com_duas_subidas   FALHOU
1 teste(s) com falha

$ GONI_ONLY=when_dispara_na_subida ./test_core
when_dispara_na_subida_uma_vez                 ...
  FALHOU  tests/test_voni.cpp:2600  h.logs.size() == 1
when_dispara_na_subida_uma_vez                 FALHOU
1 teste(s) com falha

$ GONI_ONLY=regress_voni_timers ./test_core
regress_voni_timers                            ...
  FALHOU  tests/test_sentinels.cpp:7512  o when disparou 8 vezes (o portão 1×-total morreu — a condição verdadeira está a re-disparar?)
regress_voni_timers                            FALHOU
1 teste(s) com falha
```

Reposto → verde (0 falhas).

**O ACHADO da mutação**: a 1.ª versão do when tinha borda de subida
(`c.b && !ts.prev`) + portão 1× — e a mutação «a borda fora» passava
INVISÍVEL: dado o portão, a borda era INOBSERVÁVEL (o 1.º disparo é o
mesmo frame nos dois designs; o portão mata os seguintes). Código morto
é dívida — a borda saiu, o contrato é o portão 1×-total, e a mutação
M-3.9c3 passou a ser «o portão fora», provada pelos pins da subida
única (8 logs) E das duas subidas (2 logs).

## §4 · Os ficheiros

- `app/src/main/cpp/voni/VoniGrammar.cpp` — o TimerStmt + as reservadas;
- `app/src/main/cpp/voni/VoniAst.h` — o Kind::Timer + TimerKind;
- `app/src/main/cpp/voni/VoniCompile.cpp` — as ações + o validador + o
  strayNameOf;
- `app/src/main/cpp/voni/VoniInternal.h` — o TimerState por nó + o reset;
- `app/src/main/cpp/voni/VoniVm.cpp` — execTimer (os 3 contratos);
- `app/src/main/cpp/voni/VoniRegistry.cpp` — as 3 entradas;
- `tests/test_voni.cpp` — pins ×12;
- `tests/test_sentinels.cpp` — R-054;
- `docs/REGRESSOES.md` — R-054;
- `VONI_referencia.md` + `llms-full.txt` — regenerados (16566 bytes);
- `app/build.gradle` — versionCode 100;
- `docs/RELATORIO-0.9.6.md` — §14 declara o par novo (R-015);
- `STATE.md` — §1/§2 (299 linhas < 300).
