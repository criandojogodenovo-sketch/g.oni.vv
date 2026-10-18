# RELATORIO-0.12-U-3.9a.md — View P universal + matemática + distance

0.12-U · Bloco A (Redução de Trabalho) · sub-fase 3.9a · versionCode
97 → **98** · o CI é o árbitro.

## §1 · O que mudou

1. **View P universal** — o comando aceita EXPRESSÃO de TODOS os 7 tipos
   (Int Num Txt Bool Vec2 Vec3 TIC) e formata via `valueText()`:
   - `app/src/main/cpp/voni/VoniGrammar.cpp` — `ViewStmt <- 'View'
     IdentAny (CallTail / Expr)?` (o CallTail tenta-se PRIMEIRO: a string
     de sempre segue o caminho de sempre — compat byte a byte; a expressão
     é a alternativa nova);
   - `app/src/main/cpp/voni/VoniCompile.cpp` — a ação do ViewStmt empacota
     a ExprP como 1 argumento do PathCall (a ação do comando não muda);
   - `app/src/main/cpp/voni/VoniVm.cpp` — `cmdViewP` avaliou e loga
     `"voni: " + valueText(v)`; o erro de forma ensina as duas formas.
2. **As 14 FUNÇÕES do registo** (`Kind::Funcao` novo):
   - `app/src/main/cpp/voni/VoniRegistry.h` — o Kind + o kindName;
   - `app/src/main/cpp/voni/VoniRegistry.cpp` — as 14 entradas (abs, min,
     max, clamp, lerp, floor, ceil, round, sqrt, sin, cos, random,
     random_int, distance) com Docs/equiv/exemplo/argc OBRIGATÓRIOS + a
     secção «Funções» no `fullReferenceMarkdown` + a entrada do View P
     atualizada;
   - `app/src/main/cpp/voni/VoniDocs.h/.cpp` — a vista `Cat::Funcao`
     (a Docs é 1:1 com o registo — o R-013 afere);
   - `app/src/main/cpp/voni/VoniVm.cpp` — o despacho no `evalCall`: a fn
     do UTILIZADOR procura-se primeiro (compat — o shadow da casa), depois
     `reg::find` + `Kind::Funcao` → `evalFuncao` (o argc afere-se pelos
     minArgs/maxArgs da ENTRADA; o erro ensina a sintaxe do registo).
3. **A matemática (decisões 🔶)**:
   - `sin`/`cos` em **GRAUS** (`kDegToRad` no VoniVm — a convenção da
     casa: a rot do engine também é graus; um só sistema de unidades);
   - `abs` preserva o tipo (Int→Int, Num→Num); min/max/clamp promovem
     (Int,Int→Int; Num misturado→Num); lerp devolve Num;
   - `floor`/`ceil` devolvem Int; `round` = meio afasta do zero
     (`std::llround` — 2.5→3, −2.5→−3);
   - `sqrt` de negativo = erro legível com linha;
   - `random()` → Num em [0,1) (`std::mt19937` semeado 1×; o sandbox NÃO
     expõe o estado — o isolamento §12 mantém-se);
   - `random_int(min,max)` → Int INCLUSIVO; limites têm de ser inteiros
     (Int, ou Num com valor inteiro exato — o contrato do repeat);
     min > max = erro legível;
   - `clamp` com lo > hi = erro legível (ensina a ordem).
4. **distance(TIC|Vec3, TIC|Vec3) → Num** — cada argumento é um TIC (a
   pos resolve via `host.getProp(tic, {"pos"})` — a cena é a verdade) ou
   um Vec3 (ex.: `jogador.pos`); outro tipo = erro que ensina.

O registo é a FONTE ÚNICA: cada função é uma linha em `VoniRegistry.cpp`;
`VONI_referencia.md` + `llms-full.txt` regenerados pelo `voni_refgen`
(11908 → 15181 bytes; o R-013 afere byte a byte — VERDE).

## §2 · A prova (os pins)

15 pins NOVOS em `tests/test_voni.cpp`:
`view_p_012u_num_bool_txt` (os 4 formatos no log + a string de sempre
byte a byte) · `view_p_012u_vec_tic_e_componentes` (TIC→nome, Vec3→
formato do valor, .x→Num) · `view_p_012u_expressao_e_concat` (concat +
aritmética + função no argumento) · `mat_abs_preserva_o_tipo` ·
`mat_min_max_com_promocao` · `mat_clamp_limita_e_erro_ordem` ·
`mat_lerp_interpolacao_linear` · `mat_floor_ceil_round_exatos` (inclui
os negativos e o meio-afasta) · `mat_sqrt_e_negativo_erro_com_linha` ·
`mat_sin_cos_em_graus` (90°→1, 180°→−1, 30°→0.5) ·
`mat_random_na_faixa_0_1` · `mat_random_int_no_intervalo_fechado` (o
dado 1–6, os negativos, o intervalo unitário, min>max e o limite
fracionário = erros) · `mat_arity_e_desconhecida_ensinam` (o argc vem da
entrada; 'gato' continua «não é uma função») · `distance_tic_e_vec3`
(TIC×TIC, Vec3×Vec3, TIC×Vec3 variável, o EIXO Z puro, o tipo errado e o
TIC fantasma = erros com linha) · `mat_fn_do_utilizador_vence_a_funcao_
do_registo` (compat).

**Sentinela R-052** (`regress_voni_matematica`, tests/test_sentinels.cpp):
(1) as 14 entradas existem com Kind::Funcao + Docs completas + argc + o
View P continua Comando; (2) sin(90)==1 e cos(180)==−1 EXATOS no log; (3)
`View P 42` e `View P true` chegam ao log; (4) a `fn abs` do utilizador
vence; (5) a referência tem «## Funções» com distance/random_int.

**Verificação**: test_core 0 falhas (os skips de disco/sandbox
conhecidos — a R-039 e os medicoes precisam de ~8 GB temporários que a
sandbox não tem; o CI é o árbitro) · c33_virtual 1010/0 · os gates
docs-lint/scope/theme-hex VERDE · `ctx_state_n` (STATE.md 297 < 300) ·
`ctx_refs_ok` · `ctx_codemap_n` (397 < 400).

## §3 · PROVA DE MUTAÇÃO (política #5 — os outputs colados)

### M-3.9a1 — os GRAUS viram radianos (`kDegToRad` → 1.0)

Quebra: `constexpr f64 kDegToRad = 0.017453292519943295;` → `1.0`.

```
$ GONI_ONLY=regress_voni_matematica ./test_core
regress_voni_matematica                        ...
  FALHOU  tests/test_sentinels.cpp:7230  sin(90) deu 'voni: 0.893997' — graus virou radianos?
  FALHOU  tests/test_sentinels.cpp:7233  h.logs[1] == "voni: -1"
regress_voni_matematica                        FALHOU
2 teste(s) com falha

$ GONI_ONLY=mat_sin_cos_em_graus ./test_core
mat_sin_cos_em_graus                           ...
  FALHOU  tests/test_voni.cpp:2174  std::fabs(ex[1].value.n - 1.0) < 1e-9
  FALHOU  tests/test_voni.cpp:2176  std::fabs(ex[3].value.n + 1.0) < 1e-9
  FALHOU  tests/test_voni.cpp:2177  std::fabs(ex[4].value.n - 0.5) < 1e-9
mat_sin_cos_em_graus                           FALHOU
3 teste(s) com falha
```

Reposto → `regress_voni_matematica` OK · `mat_sin_cos_em_graus` OK
(0 teste(s) com falha nos dois).

### M-3.9a2 — o View P de só-Txt de volta (o universal revertido)

Quebra: `cmdViewP` rejeita não-Txt (`vm.fail(s.line, "View P: o texto
vai entre aspas (veio " + typeName(v.t) + ")")`).

```
$ GONI_ONLY=regress_voni_matematica ./test_core
regress_voni_matematica                        ...
  FALHOU  tests/test_sentinels.cpp:7227  s.runStart(h, err)
  FALHOU  tests/test_sentinels.cpp:7228  h.logs.size() == 2
  FALHOU  tests/test_sentinels.cpp:7246  s.runStart(h, err)
  FALHOU  tests/test_sentinels.cpp:7247  h.logs.size() == 2
  FALHOU  tests/test_sentinels.cpp:7263  s.runStart(h, err)
regress_voni_matematica                        FALHOU

$ GONI_ONLY=view_p_012u ./test_core
view_p_012u_num_bool_txt                       ...
  FALHOU  tests/test_voni.cpp:2015  r.ok
view_p_012u_num_bool_txt                       FALHOU
view_p_012u_vec_tic_e_componentes              ...
  FALHOU  tests/test_voni.cpp:2033  st
view_p_012u_vec_tic_e_componentes              FALHOU
view_p_012u_expressao_e_concat                 ...
  FALHOU  tests/test_voni.cpp:2052  r.ok
view_p_012u_expressao_e_concat                 FALHOU
3 teste(s) com falha
```

(Nota de método: os pins levaram GUARDAS `if (!r.ok) return;` / size
checks ANTES de indexar — na 1.ª passada a mutação produzia CRASH
(SIGSEGV) em vez de vermelho limpo, porque o pin indexava `logs[0]` de um
vetor vazio. A mutação provou o pin E endureceu-o.)

Reposto → os 3 pins + a sentinela OK (0 falhas).

### M-3.9a3 — a distance perde o eixo z

Quebra: `return Value::ofNum(std::sqrt(dx * dx + dy * dy));` (o dz fora).

```
$ GONI_ONLY=distance_tic_e_vec3 ./test_core
distance_tic_e_vec3                            ...
  FALHOU  tests/test_voni.cpp:2279  std::fabs(ex[3].value.n - 2.0) < 1e-9
distance_tic_e_vec3                            FALHOU
1 teste(s) com falha
```

O ACHADO da mutação: a 1.ª versão do pin usava o 3-4-5 com **z = 0** —
a mutação passava INVISÍVEL (`distance_tic_e_vec3` OK com o bug ativo).
O pin ganhou o ponto `c = {0, 0, 2}` no eixo z puro (a distance 2 leria
0) — o pin fecha a vaca. Reposto → OK (0 falhas).

## §4 · Os ficheiros

- `app/src/main/cpp/voni/VoniGrammar.cpp` — o ViewStmt universal;
- `app/src/main/cpp/voni/VoniCompile.cpp` — a ação do ViewStmt;
- `app/src/main/cpp/voni/VoniVm.cpp` — cmdViewP universal + evalFuncao +
  resolvePos + kDegToRad + voniRng;
- `app/src/main/cpp/voni/VoniRegistry.h/.cpp` — Kind::Funcao + as 14
  entradas + a secção da referência + o View P atualizado;
- `app/src/main/cpp/voni/VoniDocs.h/.cpp` — Cat::Funcao (a vista);
- `tests/test_voni.cpp` — pins ×15;
- `tests/test_sentinels.cpp` — R-052;
- `docs/REGRESSOES.md` — R-052;
- `VONI_referencia.md` + `llms-full.txt` — regenerados (15181 bytes);
- `app/build.gradle` — versionCode 98;
- `docs/RELATORIO-0.9.6.md` — §14 declara o par novo (R-015);
- `STATE.md` — §1/§2 (297 linhas < 300);
- `CODEMAP.md` — a nota da 0.12-U no §2 (o índice segue o código).
