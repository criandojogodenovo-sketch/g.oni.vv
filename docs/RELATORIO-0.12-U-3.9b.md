# RELATORIO-0.12-U-3.9b.md — spawn(tipo, x, y, z) → TIC

0.12-U · Bloco A (Redução de Trabalho) · sub-fase 3.9b · versionCode
98 → **99** · o CI é o árbitro.

## §1 · O que mudou

1. **A entrada no registo** (`VoniRegistry.cpp` — a FONTE ÚNICA):
   `spawn` = `Kind::Funcao`, argc 4/4, Docs/equiv/exemplo obrigatórios
   (a referência pública regenera: 15181 → 15534 bytes).
2. **O contrato do Host** (`Voni.h`): `spawnTic(tipo, x, y, z, outName,
   err)` NOVO com default **«spawn(): indisponível neste hospedeiro»** —
   o padrão do stopAnim/blendAnim: o hospedeiro sem cena responde
   honesto (o erro sobe com a LINHA — a Vm acrescenta), nunca crash.
3. **O despacho no VM** (`VoniVm.cpp`): `evalCall` → `evalSpawn` — o
   TIPO é NOME CRU (Txt entre aspas OU identificador — o padrão dos
   slots de nome do tykerwire: copy/colorpars/blend; avaliar `esfera`
   mataria com «'esfera' não existe» antes do Host); x/y/z são números
   avaliados (erro legível se não forem); o argc afere-se pelos
   minArgs/maxArgs da ENTRADA; o Host devolve o NOME ÚNICO e o script
   recebe `Value::ofTic(nome)` — usável em `.pos`, `distance`, etc.
4. **A engine real** (`VoniEngineHost.cpp`): a lista VIVA é **esfera** e
   **box** (a decisão 0.8.10 — o `primFromName` do render é defesa de
   LOAD do .goni antigo, NÃO porta para o script criar tipos mortos; o
   erro ENSINA «tipos: esfera box»); criação via `Scene::create` (nasce
   **ACTIVE** — F1) + visible (o default do Tic) + `Transform3D` na pos
   (updateWorld) + `MeshRenderer` com `primOn` + `primDefaults(kind)` (o
   mesmo corpo do preset Mesh do `core/Presets.cpp`); o NOME ÚNICO é o
   `uniqueSpawnName` (base, base.001, base.002… limite 999 — a MESMA
   convenção do `createTicFromPreset`/`uniqueTicName` do uiEditor: o
   find é por nome, o colidido é fantasma).

## §2 · A prova (os pins)

9 pins NOVOS:
- `tests/test_voni.cpp` (6 VM): `spawn_cria_tic_e_devolve` (o nome no
  log + a pos + o @+ TIC) · `spawn_tipos_validos_e_registo` (aspas E
  nome cru) · `spawn_tipo_desconhecido_erro_com_linha` (o «dragon» E o
  «cilindro» REMOVIDO — linha certa + a lista no erro) ·
  `spawn_forma_errada_ensina_a_sintaxe` (argc errado, argumento não
  número, tipo não-nome) · `spawn_nome_unico_na_serie` (esfera,
  esfera.001, esfera.002 + box na base própria) · `spawn_tic_usavel_
  pos_distance` (escreve pos, lê, entra na distance) ·
  `spawn_host_sem_cena_erro_honesto` (o default do Host);
- `tests/test_wiring092.cpp` (2 ENGINE REAL): `voni_spawn_cria_tic_real_
  na_cena` (ACTIVE/VISIBLE/Transform3D na pos/prim kind Sphere e Box/
  esfera.001/exported com os nomes únicos) ·
  `voni_spawn_tipo_desconhecido_mata_a_run_com_linha` (o popError com a
  lista + **NADA nasceu** na cena).

**Sentinela R-053** (`regress_spawn`, tests/test_sentinels.cpp): (1) a
entrada é Funcao argc 4/4 com Docs completa; (2) o default do Host é
honesto (o host do R-052, que não implementa spawn, é a prova viva);
(3) a cadeia completa registo→VM→Host→script — série única + o TIC
devolvido usável + o tipo desconhecido morre com «esfera box».

**Verificação**: test_core 0 falhas (os skips de disco/sandbox
conhecidos; o CI é o árbitro) · c33_virtual 1010/0 · os gates locais
verdes (docs-lint/theme-hex/ui-vocab/hierarchy/projects-ui/gmesh-docs/
jni-parity/glyph-source/reload-concurrency/release-identity vc 99/
link-parity 123 TUs/check_main) · `ctx_state_n` (298 < 300) ·
`ctx_codemap_n` (399 < 400) · `ctx_refs_ok`.

## §3 · PROVA DE MUTAÇÃO (política #5 — os outputs colados)

### M-3.9b1 — o despacho do spawn fora do evalCall

Quebra: o ramo `if (e.name == "spawn") { return evalSpawn(*fe, e); }`
removido (o spawn cai no `evalFuncao`, que não o tem → «a entrada existe
no registo mas não tem runtime»).

```
$ GONI_ONLY=spawn_cria_tic ./test_core
spawn_cria_tic_e_devolve                       ...
  FALHOU  tests/test_voni.cpp:2352  r.ok
spawn_cria_tic_e_devolve                       FALHOU
1 teste(s) com falha

$ GONI_ONLY=regress_spawn ./test_core
regress_spawn                                  ...
  FALHOU  tests/test_sentinels.cpp:7389  o default do spawnTic deixou de ser honesto ('spawn: a entrada existe no registo mas não tem runtime (erro interno)')
  FALHOU  tests/test_sentinels.cpp:7405  s.runStart(h, err)
  FALHOU  tests/test_sentinels.cpp:7406  h.spawns.size() == 2
  FALHOU  tests/test_sentinels.cpp:7407  h.logs.size() == 3
  FALHOU  tests/test_sentinels.cpp:7421  err2.message.find("esfera box") != std::string::npos
regress_spawn                                  FALHOU
```

Reposto → `spawn_cria_tic_e_devolve` OK · `regress_spawn` OK (0 falhas).

### M-3.9b2 — o NOME ÚNICO fora (a EngineHost cria com o nome cru)

Quebra: `scene_.create(uniqueSpawnName(scene_, tipo))` →
`scene_.create(tipo)`.

```
$ GONI_ONLY=voni_spawn_cria ./test_core
voni_spawn_cria_tic_real_na_cena               ...
  FALHOU  tests/test_wiring092.cpp:957  h2.valid()
  FALHOU  tests/test_wiring092.cpp:973  ex[1].value.tic == "esfera.001"
voni_spawn_cria_tic_real_na_cena               FALHOU
2 teste(s) com falha
```

Reposto → OK (0 falhas). (O pin VM `spawn_nome_unico_na_serie` apanha a
MESMA classe no fake — o engine-real prova o caminho do produto.)

## §4 · Os ficheiros

- `app/src/main/cpp/voni/Voni.h` — o contrato Host::spawnTic + default;
- `app/src/main/cpp/voni/VoniEngineHost.h/.cpp` — a implementação real
  (a lista viva + o corpo do preset Mesh + uniqueSpawnName);
- `app/src/main/cpp/voni/VoniRegistry.cpp` — a entrada spawn;
- `app/src/main/cpp/voni/VoniVm.cpp` — o despacho + evalSpawn;
- `tests/test_voni.cpp` — pins ×6 + o FakeHost::spawnTic;
- `tests/test_wiring092.cpp` — pins ×2 (engine real);
- `tests/test_sentinels.cpp` — R-053;
- `docs/REGRESSOES.md` — R-053;
- `VONI_referencia.md` + `llms-full.txt` — regenerados (15534 bytes);
- `app/build.gradle` — versionCode 99;
- `docs/RELATORIO-0.9.6.md` — §14 declara o par novo (R-015);
- `STATE.md` — §1/§2 (298 linhas < 300).
