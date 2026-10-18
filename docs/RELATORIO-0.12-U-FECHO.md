# RELATORIO-0.12-U-FECHO.md — Campanha 0.12-U · V.ONI Bloco A (Redução de Trabalho)

0.12-U · FECHO · versionCode vigente: **100** (o 3.9c declarou o par no
§14 do RELATORIO-0.9.6.md; o FECHO não bumpa — nenhum build novo) · o
CI é o árbitro.

## §1 · O que a campanha entregou (as 7 features, as 3 sub-fases)

| Sub-fase | Features | vc | Pins | Sentinela | Mutações |
|---|---|---|---|---|---|
| 3.9a | View P universal · matemática (13 fn) · distance | 98 | ×15 | R-052 `regress_voni_matematica` | M-3.9a1/a2/a3 |
| 3.9b | spawn(tipo, x, y, z) → TIC | 99 | ×9 | R-053 `regress_spawn` | M-3.9b1/b2 |
| 3.9c | every(s) · after(s) · when(cond) | 100 | ×12 (+1 recalibração) | R-054 `regress_voni_timers` | M-3.9c1/c2/c3 |

- **O registo é a fonte única**: 50 → **64 entradas** (as 15 Funções +
  os 3 timers + o View P atualizado); a referência pública regenerada a
  cada sub-fase (11908 → **16566 bytes**) e o R-013 afere byte a byte.
- **As camadas respeitadas**: o spawn entra pela ÚNICA porta
  `Host::spawnTic` (o default «indisponível» honesto; a EngineHost cria
  na cena REAL — active/visible/nome único/base.001); o estado dos
  timers vive no `Script::Impl` POR NÓ AST (zero static/global).
- **A compat byte a byte**: a string do View P de sempre segue o caminho
  de sempre (a gramática tenta o CallTail primeiro); a fn do utilizador
  vence a função do registo; nenhum .goni/.gmesh tocado.
- **Os achados das mutações** (a prova a afiar a prova): o pin da
  distance nasceu cego ao eixo z (o 3-4-5 tem z=0 — o ponto no eixo z
  puro fechou a vaca); a «borda de subida» do when era INOBSERVÁVEL
  dado o portão 1× (código morto saiu — o contrato é o portão); os
  pins ganharam guardas (a mutação do View P produzia CRASH em vez de
  vermelho limpo na 1.ª passada).

## §2 · O CRITÉRIO DO PLATFORMER (o juiz final) — VERDE

A regra de convivência da campanha: «não declarar o Bloco A fechado até
o platformer do critério correr». O pin
`platformer_bloco_a_juiz_final` (tests/test_voni.cpp) corre o ÚNICO
script `.voni` contra o FakeHost com dt simulado (25 frames × 0.1 s) e
a sequência observável bateu a esperada:

```voni
v#@+dado:Int=random_int(1, 6)
v#jogador:TIC=spawn("box", 0, 1, 0)
v#plataforma:TIC=spawn("box", 0, 0, -4)
v#moeda:TIC=spawn("esfera", 0, 1, -4)
v#porta:TIC=spawn("box", 0, 0, -8)
v#vy:Num=0
v#vel:Num=0
central main {
  on moment {
    View P "fase a correr"
    View P abs(-42)
  }
  allmoments {
    vel = min(vel + 0.1, 0.5)
    jogador.pos.z = jogador.pos.z - vel
    vy = clamp(vy - 0.5, -2, 0)
    jogador.pos.y = jogador.pos.y + vy
    moeda.pos.y = lerp(moeda.pos.y, 1 + vel, 0.5)
    every(0.5) { View P "moeda pulsa" }
    after(2) { View P "porta aberta" }
    exist(jogador.pos.y < -1) {
      jogador.pos.y = 1
      vy = 0
      View P "reposicao"
    }
    when(distance(jogador, moeda) < 1.5) { View P "moeda apanhada" }
  }
}
```

O veredito do juiz (tudo verificado no pin): os 4 spawns com nome único
(`box`, `box.001`, `esfera`, `box.002`) · a física repôs **8×** (o
ciclo de queda de 3 frames — a REPOLÍTICA é `exist`, que corre sempre;
o `when` é EVENTO 1× — o refinamento do critério da ABERTURA, mesmo
conjunto de 7 features) · a moeda pulsa **5×** (o every com o resto a
carregar) · a porta abriu **1×** (o after aos 2.0 s) · a moeda foi
apanhada **1×** (o when 1×-total sobre a distance) · o jogador andou
(z < −10, a vel capped pelo min) · a moeda subiu (o lerp) · o dado do
Inspector é Int em [1, 6] (o random_int honesto) · o arranque logou
«fase a correr» e o «42» do abs.

**O Bloco A FECHA com o juiz verde** — a redução de trabalho é real: o
dono escreveu um platformer de 25 linhas SEM relógios à mão, SEM
if/else, SEM boilerplate de spawn — e a linguagem validou cada linha
com erros que ensinam.

## §3 · A verificação (o CI e os gates)

- test_core: **0 falhas** (os skips de disco/sandbox conhecidos — a
  R-039 e os medicoes precisam de ~8 GB temporários que a sandbox não
  tem; `regress_streaming_paralelo` é flake de layout da sandbox — passa
  isolado e o CI é o árbitro; o CI correu VERDE nas sub-fases 3.9a e
  3.9b — runs 39170629991 e 39171590189).
- c33_virtual: **1010 checks, 0 falhas** (a FASE 35 intacta — a campanha
  não tocou o chrome).
- Os 12 gates locais verdes (docs-lint 112 ficheiros · theme-hex ·
  ui-vocab · hierarchy · projects-ui · gmesh-docs · jni-parity ·
  glyph-source · reload-concurrency · release-identity vc 100 ·
  link-parity 123 TUs · check_main) · `ctx_state_n` (299 < 300) ·
  `ctx_codemap_n` (399 < 400) · `ctx_refs_ok` · `ctx_readme_n`.

## §4 · As entradas da linguagem (o que o README/ARCHITECTURE registam)

- **Funções** (15): abs · min · max · clamp · lerp · floor · ceil ·
  round · sqrt · sin · cos · random · random_int · distance · spawn —
  sin/cos em GRAUS; o despacho no `evalCall` consulta o REGISTO (o nome
  só existe se a tabela o disser); o argc vem da ENTRADA.
- **Timers** (3): every · after · when — reservadas; o estado por nó
  AST morre no stop().
- **View P**: universal (os 7 tipos via valueText).
- As reservadas: 20 → **23** (every/after/when; a recalibração com nota
  no relatório 3.9c).

## §5 · Os ficheiros do fecho

- `README.md` — a campanha em curso passa a 0.12-U fechada + o vc 100;
- `ARCHITECTURE.md` §5 — os números que importam atualizados;
- `BACKLOG.md` — a secção 0.12-U FEITO;
- `STATE.md` — §1: a campanha FECHA (o índice segue o código);
- `docs/RELATORIO-0.12-U-ABERTURA.md` + `-3.9a` + `-3.9b` + `-3.9c` +
  este FECHO — a história completa da campanha;
- `VONI_referencia.md` + `llms-full.txt` — regenerados (o R-013 afere).
