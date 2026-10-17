# RELATORIO-0.12-A-INVEST-RAM.md — A-4: o salto anormal do 3.º edifício (RAM vs GPU)

Campanha 0.12-A · sub-fase A-4 · 2026-10-17

O PROBLEMA: o dono mede 4,1M → 8,2M → 12,3M tri e obtém 77 ms → 143 ms →
333 ms por frame — o dobro de triângulos devia custar ~o dobro (a parte
linear) e custa 1,86× e depois 2,33×, com o 3.º salto a desabar o FPS.
A pergunta da campanha: **RAM ou GPU/CPU?**

REGRAS DA SUB-FASE: sem fix, sem alterações de produção — só instrumentação
sob `#ifdef GONI_BENCH_RAM` (removível, OFF por default).

---

## 1. A instrumentação entregue (duas peças)

### 1.1 O sampler do device (main.cpp, sob o ifdef)

Com o APK construído com `-DGONI_BENCH_RAM=ON` (o CMake do app recebe a
flag; sem ela o código não existe no binário — zero custo), o frame loga
1×/segundo no engine.log a TABELA completa do dono:

```
bench_ram: tri=<N> fps=<F> frame_ms=<MS> rss=<MB> pico=<MB> livre=<MB>
           dc=<DC> bl=<BL> blocos=<TOTAL> carregados=<N> a_transmitir=<N>
```

- `tri` = índices desenhados ÷ 3 (st3d + grid + UI, a MESMA soma da
  statusLine R-017);
- `rss`/`pico` = VmRSS/VmHWM de `/proc/self/status` (o pico desde o arranque);
- `livre` = MemAvailable de `/proc/meminfo`;
- `a_transmitir` = blocos visíveis à espera de vaga (o contador de pending
  da 3.75a — se esta linha crescer e NÃO esvaziar, o conjunto visível é
  maior do que a cache consegue segurar).

### 1.2 O bench da curva no hospedeiro (alvo opcional `bench_ram`)

`tests/bench_ram_curve.cpp` — constrói-se SÓ com
`cmake -DGONI_BENCH_RAM=ON … && make bench_ram` (a opção é OFF por default;
não entra no test_core nem no APK). Gera .gmesh sintéticos com os 8 tamanhos
da curva (50k…12,3M tri, blocos de 600 verts ≈ 598 tri, o MESMO formato do
import), abre-os pela TABELA e desenha-os com os ORÇAMENTOS DE PRODUÇÃO
(64 MB RAM + 192 MB VRAM) através do rasterizador-stub da casa COM O
FRAMEBUFFER OFF (glDrawElements só conta — **nenhum custo de GPU no número**;
o que mede é a parte CPU do caminho: cull + sort + batching + LRU +
re-materialização). Cada tamanho corre 12 frames em ORBIT + 5 frames com a
CÂMARA PARADA (o discriminador do mecanismo).

---

## 2. A TABELA (hospedeiro x86_64, -O1, 2026-10-17)

```
      tri   blocos   open_ms  frame0_ms   orbit_ms  parado_ms       bl   vram_MB   evc_orb   evc_par    rss_MB
    50000       84       0.1        6.3       0.01       0.01       84       1.8         0         0       9.2
   200000      335       0.1       25.3       0.05       0.05      335       7.3         0         0      21.4
   500000      837       0.2       63.0       0.12       0.12      837      18.2         0         0      45.9
  1000000     1673       0.5      128.7       0.26       0.24     1673      36.4         0         0      85.3
  2000000     3345       0.9      281.7       0.49       0.48     3345      72.7         0         0      89.7
  4100000     6857       1.8      580.4       1.11       1.03     6857     149.0         0         0     178.5
  8200000    13713       3.5     1140.0     376.63     378.48    13713     192.0      9758      9758     352.2
 12300000    20569       5.2     1673.3     899.60     898.40    20569     192.0     23470     23470     484.3
```

- `frame0_ms` = o 1.º frame (a materialização fria até ao teto do orçamento);
- `orbit_ms` / `parado_ms` = o frame ESTÁVEL em orbit / com a câmara parada;
- `evc_orb` / `evc_par` = EVICTIONS da LRU POR FRAME (o thrash da cache);
- `vram_MB` = os uploads residentes (o teto de produção é 192 MB);
- `rss_MB` = o pico de RSS do processo (tabela + cache CPU + transientes).

O ficheiro .gmesh do escalão 12,3M tem ≈ 470 MB (6,17M verts × 32 B +
12,3M×3 idx × 2 B); o escalão 8,2M ≈ 315 MB; o 4,1M ≈ 160 MB. A RAM do
processo NUNCA passa de ~0,5 GB — o host tinha 4 GB.

---

## 3. A análise obrigatória (§4 do prompt, com números)

### (a) O FPS cai linear com o tri? Onde quebra?

**Cai linear ATÉ ao cruzamento do orçamento da cache e desaba DEPOIS.** A
região linear é limpa: 50k→0,01 ms · 200k→0,05 · 500k→0,12 · 1M→0,26 ·
2M→0,49 · 4,1M→1,11 — cada duplicação de tri custa ~2× (o cull + o
batching escalam linear; zero crescimento superlinear no caminho CPU até
aqui). A QUEBRA é exatamente quando o conjunto VISÍVEL deixa de caber no
orçamento VRAM de 192 MB: com blocos de ≈ 20,4 KB, 192 MB seguram ≈ 9,4k
blocos; o escalão 8,2M tem 13 713 blocos visíveis e o escalão 12,3M tem
20 569 — **a partir daí o frame explode para 376 ms e 900 ms** (339× e
810× o custo linear esperado de ~1,9 ms e ~2,9 ms). A quebra NÃO é no
número de triângulos — é no número de BLOCOS VISÍVEIS contra o teto da
cache.

### (b) O RSS cresce linear?

**SIM — linear e sem penhasco.** 9,2 → 21,4 → 45,9 → 85,3 → 89,7 → 178,5 →
352,2 → 484,3 MB: o pico do processo cresce ~linear com o modelo (a tabela
cresce com os blocos, a cache CPU está retida no seu próprio orçamento de
64 MB, os transientes do gerador aparecem no frame0). NÃO há salto de RSS
no ponto onde o frame salta (o RSS do 8,2M é 1,97× o do 4,1M — proporcional
ao modelo; o FRAME é 339× o linear).

### (c) O salto do 3.º coincide com pico de RSS?

**NÃO.** O salto do frame coincide com o **TETO DO ORÇAMENTO DA CACHE**
(vram_MB crava em 192,0 nos dois escalões) e com a **TEMPESTADE DE
EVICTIONS**: 9 758 blocos/frame a 8,2M e 23 470/frame a 12,3M — contra ZERO
evictions em toda a região linear. O RSS continua a subir suavemente e
honesto no mesmo intervalo. E o discriminador decisivo: **`parado_ms` ≈
`orbit_ms` e `evc_par` == `evc_orb`** — com a câmara COMPLETAMENTE PARADA o
thrash continua idêntico. O conjunto visível é maior do que a cache; o draw
tenta materializar todo o conjunto visível a cada frame; a LRU cicliza
(recarrega do disco + re-uploads) o mesmo conjunto indefinidamente, parada
ou girando.

### (d) Veredito: RAM ou GPU/CPU?

**RAM — a tempestade de re-materialização da cache de blocos (o orçamento
64+192 MB), NÃO a GPU e NÃO o custo de CPU do desenho.** A prova: no
hospedeiro NÃO HÁ GPU (o stub não rasteriza um único fragmento) e MESMO
ASSIM o salto superlinear reproduz-se, no MESMO ponto (o cruzamento do
orçamento) e com o MESMO mecanismo (evictions contínuas). O custo linear
do caminho CPU (cull/sort/batch) está excluído pela região limpa até 4,1M.
No DEVICE espera-se o MESMO mecanismo com blocos maiores (a cena de 203 MB
do dono tem blocos com texturas — cada eviction custa leitura + decode +
upload), o que faz a quebra chegar ANTES (com menos tri) e explica o 333 ms
do 3.º edifício. O lowmemorykiller do Android NÃO é o mecanismo esperado —
o RSS do processo não penhasca; mas só a tabela do device fecha o caso.

### O que o dono deve ver no device para FECHAR (o protocolo do C33)

1. APK com `-DGONI_BENCH_RAM` (o sampler §1.1) + importar as 3 cenas.
2. `grep bench_ram engine.log` → confirmar por escalão: rss ~linear entre
   escalões; `a_transmitir` GRANDE e a não esvaziar nos escalões 8,2M/12,3M
   (o pending eterno = o visível não cabe na cache); dc/bl a oscilar.
3. `logcat | grep -iE "lowmemorykiller|reclaim|kswapd"` → esperado vazio;
   se APARECER, o peso muda para pressão de RAM do sistema (o veredito
   passaria a "RAM do sistema", a recomendação §4 ganha prioridade máxima).
4. A predição que confirma o mecanismo: NO device, o 2.º/3.º edifício com a
   câmara parada tem o frame_ms TÃO MAU como em orbit (o MESMO thrash).

---

## 4. A recomendação para a próxima campanha (sem fix nesta — a regra A-4)

1. **O orçamento de materialização no caminho ESTÁVEL do draw** (a extensão
   natural do contrato da 3.75a): o draw de batch NÃO pode materializar
   "todo o visível" quando o visível não cabe na cache — um teto de
   materializações/frame (o padrão dos 8 loads do import, «perto primeiro»)
   desenha o que cabe, mantém o resto pendente (o impostor/LOD cobre o
   buraco) e mata o thrash: o frame volta à região linear. É UMA decisão
   de orçamento no `flushBatch`/draw, sem formato novo.
2. **O impostor/LOD como política de cobertura** (os sistemas já existem —
   3.75b e 3.5a): os blocos pendentes por causa do teto (1) desenhados como
   PNG/silhueta quando o TIC está longe — o dono já tem os dois campos no
   Inspector (a A-1 provou que a UI existe).
3. **O orçamento como setting honesto** (64+192 MB são os defaults da
   casa): subir o teto VRAM adia a quebra ao custo de RAM do device — a
   tabela §2 dá a conta (blocos visíveis × bytes/bloco vs teto).
4. **A telemetria `a_transmitir` no HUD** (o campo já existe no
   BlockFrameStats): o dono VÊ a cache a afogar sem abrir o log.

---

## 5. Os limites declarados (a honestidade da investigação)

- O bench do hospedeiro NÃO tem GPU: os tempos absolutos são da parte CPU
  (cull/sort/batch/LRU/disco) — os números do device (77/143/333) incluem
  o rasterizador real; o que a curva prova é o PONTO e o MECANISMO da
  quebra, não os milissegundos exatos do device.
- A malha sintética tem 1 tri por vértice (o gerador da casa); o device
  tem malhas com proporções diferentes — o que importa para a quebra é
  BYTES POR BLOCO × blocos visíveis vs o teto, e essa conta a tabela §2
  permite refazer para qualquer proporção.
- O sampler do device (§1.1) foi verificado por síntese de compilação
  (g++ -fsyntax-only com e sem o ifdef contra os stubs da casa) — o APK
  com a flag é build do CI/Android; sem a flag o código não existe.
- A curva correu 1× no host deste ambiente (x86_64, -O1, tmpfs em disco);
  re-corridas podem variar os ms absolutos ~10%, a FORMA (linear → cliff no
  cruzamento do orçamento) é o resultado.
