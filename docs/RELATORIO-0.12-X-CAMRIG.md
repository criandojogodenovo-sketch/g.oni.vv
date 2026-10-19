# RELATORIO-0.12-X-CAMRIG.md — camrig + camdrag (a câmara de terceira pessoa)

0.12-X · sub-fase 3.9x · versionCode 100 → **101** · o CI é o árbitro.

Cláusula calma cumprida: só o core da V.ONI + a Câmara — zero refactors,
zero mexidas na UI/Renderização (o diff toca voni/, core/VoniSystem,
platform/main.cpp e tests/docs).

## §1 · O que mudou

1. **As 2 entradas no registo** (`VoniRegistry.cpp` — a FONTE ÚNICA):
   `camrig` = Componente **Continuo** argc **2..3**; `camdrag` =
   Componente **Continuo** argc **1..1** — Docs/equiv/exemplo
   obrigatórios (o exemplo é o TRIO da campanha:
   `find(cam) camrig(8, 3, 6) camdrag(0.3) limit(3, 12)`); as Docs do
   `point` e do `limit` passam a mencionar o camrig (a referência
   pública regenera: 16566 → 17549 bytes, 64 → 66 entradas).
2. **O runtime** (`VoniTykers.cpp`): `compCamrig(d,h[,suav])` — o pivô é
   **alvo+h**; a câmara assenta à distância 3D `d` do pivô na direção
   esférica do `camYaw/camPitch` do TykerState (offset
   `(sin y·cos p, sin p, cos y·cos p)` — yaw 0/pitch 0 = a +Z do pivô);
   olha para **alvo+h/2** (o rot em GRAUS pelo canal setProp da casa, a
   convenção da CÂMARA: yaw `atan2(−dx,−dz)`, pitch `asin(dy/|d|)` — o
   −Z local aponta ao alvo); suav > 0 suaviza com o decaimento da casa
   `k = 1 − e^(−suav·dt)` (sem suav = SNAP, o padrão do follow);
   `limit(min,max)` clampa a distância **FINAL** contra o pivô, APÓS a
   suavização (o `clampLimit` existente). `compCamdrag(sens)` — consome
   o `Host::lookDelta(dx,dy)` (o consumo **ZERA** o acumulador: um 2.º
   consumidor recebe zeros — nunca dobra o gesto); `camYaw -= dx·sens`,
   `camPitch += dy·sens`; o pitch é clamped **FIXO −20..70**
   (`kCamPitchMin/Max` — constantes, decisão fechada); o yaw é 360°
   livre e NORMALIZA para [0,360) (`floor` — cobre os dois lados). O
   yaw/pitch vivem no `TykerState` (`camYaw/camPitch`, junto ao
   `orbitAngle` — VoniTykers.h). Os args resolvem 1× na ativação (o
   caminho do `resolveComp` — zero parser novo).
3. **O contrato do Host** (`Voni.h`): `lookDelta(dx, dy)` NOVO com o
   default **false** — o padrão do stopAnim/spawnTic: o hospedeiro sem
   input responde honesto e o camdrag IGNORA (o script nunca morre).
   `VoniEngineHost.h/.cpp` delega no hook injetável
   (`LookDeltaFn`); `core/VoniSystem.h/.cpp` transporta o hook
   (`setLookDeltaFn`) nas DUAS construções do EngineHost (ensureStarted
   + tick) — o editor-run e o Play partilham o mesmo canal.
4. **O input no main** (`platform/main.cpp`): o acumulador
   `g_lookDx/g_lookDy` + o rasto por slot `LookTrack` (nasce com a
   máscara do press — **se nasceu reclamado pela UI, NUNCA é olhar**); o
   `frame()` alimenta SOMA os deltas (pos−pos por slot) dos dedos que o
   TouchControls/canvas/gizmo NÃO reclamaram, SÓ em `g_editor.playMode`
   (fora do Play o rasto e o acumulador limpar); o consumo pelo camdrag
   zera (o padrão do `g_physics.frame.fwd` — ver §2).
5. **O compilador** (`VoniCompile.cpp`): o Validator rejeita
   `camrig` + (`follow` OU `look`) no MESMO tyker — a câmara que segue E
   olha é UMA; o erro ENSINA («camrig já segue e olha o alvo — o follow
   sobra no mesmo tyker») e viaja com o par `fixFrom`/`fixTo`
   (`follow`→`camrig`) do botão Substituir.
6. **Sem alvo válido = erro CEDO**: o destino sem `pos` (TIC ausente ou
   sem Transform3D) morre no 1.º frame com o erro do getProp E a linha
   (o caminho do `targetPos` → `vm.fail`) — nunca câmara parada em
   silêncio.

## §2 · O TIMING do input (a ordem frame ↔ V.ONI tick)

O bloco que alimenta o `lookDelta` corre DENTRO do `frame()`
(`main.cpp`, depois das máscaras de claim) e o `g_systems.run` (o tick
da V.ONI + a física) corre ANTES dele no loop
(`main.cpp:8625-8632`) — o delta alimentado no frame N é consumido pelo
1.º passo fixo do frame N+1. **É de propósito — a casa já corre assim**:

1. O `g_physics.frame.fwd/right` (o input da FÍSICA) é alimentado no
   MESMO sítio do `frame()` (`main.cpp:6265-6278`) e consumido pelo passo
   fixo seguinte — o camdrag tem EXATAMENTE a latência do movimento, os
   dois sincronizados (a ABERTURA §2.5 fechou isto com ficheiro:linha).
2. Tragar o feed para ANTES do `g_systems.run` trocaria a frescura da
   MÁSCARA: um dedo que pressiona o joystick entre dois frames seria
   marcado com a máscara do frame ANTERIOR (que não o conhecia) e
   LEAKARIA 1 frame de delta de olhar — o bug que a regra «o dedo nasce
   com a máscara do press» existe para matar.
3. O consumo ZERAR o acumulador auto-protege: steps fixos múltiplos no
   mesmo loop → só o 1.º consome; 2 tykers com camdrag → só o 1.º recebe
   (o pin `camdrag_consumo_zera_e_sem_input_e_noop` (3) prova com dois
   tykers e UM gesto: exatamente um rodou).

## §3 · A prova (os pins)

14 pins NOVOS:
- `tests/test_voni.cpp` (10 VM — o FakeHost ganhou o canal lookDelta
  com o flag hasLookInput):
  `camrig_assenta_distancia_altura_e_olhar` (snap (8,3) → (0,3,8) +
  pitch −10.62°/yaw 0 — o −Z a alvo+h/2) ·
  `camrig_suaviza_sem_ultrapassar` (o 1.º passo = d0·(1−k) EXATO,
  monótona 5 frames, converge em 600) ·
  `camdrag_yaw_360_livre_e_normalizado` (−90 → +X; +360 e −630 → o
  MESMO sítio; o rot.yaw 90 ao alvo) ·
  `camdrag_pitch_clamp_fixo_menos_20_a_70` (+200 → teto 70 e FICA,
  −400 → chão −20, o rot.pitch −70 no teto) ·
  `camrig_limit_clampa_a_distancia_final` (snap 20→12 e 2→3; APÓS a
  suavização de (40,0,0) a distância final é 12, não ~36.2) ·
  `camdrag_consumo_zera_e_sem_input_e_noop` (o frame sem gesto não
  repete; o hospedeiro sem input vive 10 frames; DOIS tykers com UM
  gesto → exatamente um rodou) ·
  `camrig_follow_ou_look_no_mesmo_tyker_erro_que_ensina` (follow E look
  rejeitados com fixFrom/fixTo + a linha; o TRIO compila;
  follow+look sem camrig continua legal) ·
  `camrig_camdrag_args_ensinam_a_sintaxe` (argc 0/1/4 do camrig e 0 do
  camdrag ensinam a sintaxe do registo; o Txt no número morre na
  ativação com «tem de ser um número») ·
  `camrig_sem_alvo_valido_erro_cedo_com_linha` (o destino fantasma e a
  câmara fantasma morrem no 1.º frame com a linha) ·
  `camrig_compat_012u_intacto` (follow(2)/orbit(2,90)/every(0.5)
  byte a byte + a forma das entradas novas e velhas no registo);
- `tests/test_wiring092.cpp` (4 MOTOR REAL):
  `voni_motor_camrig_assenta_a_camara_real` (o Transform3D da câmara
  real assentado + o Quat::toEuler confirma −10.62°/0) ·
  `voni_motor_camdrag_roda_pelo_hook_real` (o VoniSystem.setLookDeltaFn
  ponta a ponta — UM gesto roda, o consumo zera, o hospedeiro sem hook
  é no-op) · `voni_motor_camrig_sem_transform_erro_cedo` (o popError
  com «não tem Transform3D» e a câmara intocada) ·
  `voni_goni_roundtrip_preserva_o_camrig` (o TRIO pelo caminho REAL do
  .goni: saveActiveScene → open → loadActiveScene → o fonte byte a
  byte → o script carregado corre e converge no motor real).

**Sentinela R-055** (`regress_camrig`, tests/test_sentinels.cpp — o
Host055 próprio com pos/rot em GRAUS + lookDelta): (1) as 2 entradas
com kind/argc/Docs (o exemplo do trio); (2) as convenções — snap (8,3)
→ (0,3,8) + pitch −10.62°, o 1.º passo suavizado = d0·(1−k) exato, o
limit(3,12) a segurar a distância FINAL em 12; (3) o camdrag — yaw
−90/+360, pitch +200→70 e −400→−20, o consumo a zerar, o host sem
input vivo; (4) o conflito com fixFrom/fixTo; (5) a referência com as
secções `### camrig`/`### camdrag`.

**Verificação**: test_core 0 falhas (os skips de disco/sandbox
conhecidos: `sentinela_r039` — errno 28 na sandbox; o CI é o árbitro)
· os gates locais verdes · `ctx_state_vc_bate` OK (101/101) ·
`ctx_state_n` (299 < 300) · R-013 (`regress_bijeção_da_ajuda`) OK
byte a byte · os 12 pins irmãos da 0.12-U (R-052/R-053/R-054,
platformer) continuam verdes.

## §4 · PROVA DE MUTAÇÃO (política #5 — os outputs colados)

### M-3.9x1 — o delta do olhar removido do caminho (o camdrag deixa de consumir)

Quebra: as 2 linhas do consumo em `compCamdrag`
(`c.st.camYaw -= dx * (f32)sens;` / `c.st.camPitch += dy * (f32)sens;`)
comentadas — o input do olhar alimenta o hook mas NUNCA chega à câmara
(o main.cpp não compila no test_core; o fim do caminho do input dentro
do alcance dos pins é o consumo).

```
$ GONI_ONLY=camdrag_ ./test_core
camdrag_yaw_360_livre_e_normalizado            ...
  FALHOU  tests/test_voni.cpp:1912  test::nearEqF(cam->pos[0], 4.0f, 1e-3f)
  FALHOU  tests/test_voni.cpp:1913  test::nearEqF(cam->pos[2], 0.0f, 1e-3f)
  FALHOU  tests/test_voni.cpp:1918  test::nearEqF(cam->pos[0], 4.0f, 1e-3f)
  FALHOU  tests/test_voni.cpp:1922  test::nearEqF(cam->pos[0], 4.0f, 1e-3f)
  FALHOU  tests/test_voni.cpp:1924  test::nearEqF(cam->rot[1], 90.0f, 1e-2f)
camdrag_yaw_360_livre_e_normalizado            FALHOU
camdrag_pitch_clamp_fixo_menos_20_a_70         ...
  FALHOU  tests/test_voni.cpp:1946  test::nearEqF(cam->pos[1], 4.0f * std::sin(70.0f * ...), 1e-2f)
  FALHOU  tests/test_voni.cpp:1948  test::nearEqF(cam->pos[2], 4.0f * std::cos(70.0f * ...), 1e-2f)
  FALHOU  tests/test_voni.cpp:1953  ...  FALHOU  tests/test_voni.cpp:1958  ...
  FALHOU  tests/test_voni.cpp:1960  ...  FALHOU  tests/test_voni.cpp:1968  test::nearEqF(cam->rot[0], -70.0f, 1e-2f)
camdrag_pitch_clamp_fixo_menos_20_a_70         FALHOU
camdrag_consumo_zera_e_sem_input_e_noop        ...
  FALHOU  tests/test_voni.cpp:2053  test::nearEqF(cam->pos[0], 4.0f, 1e-3f)
  FALHOU  tests/test_voni.cpp:2057  test::nearEqF(cam->pos[0], 4.0f, 1e-3f)
  FALHOU  tests/test_voni.cpp:2107  c1Rodou != c2Rodou
camdrag_consumo_zera_e_sem_input_e_noop        FALHOU
18 teste(s) com falha

$ GONI_ONLY=voni_motor_camdrag ./test_core
voni_motor_camdrag_roda_pelo_hook_real         ...
  FALHOU  tests/test_wiring092.cpp:1078  test::nearEqF(tr->pos.x, 4.0f, 1e-3f)
  FALHOU  tests/test_wiring092.cpp:1079  test::nearEqF(tr->pos.z, 0.0f, 1e-3f)
  FALHOU  tests/test_wiring092.cpp:1085  test::nearEqF(tr->pos.x, 4.0f, 1e-3f)
voni_motor_camdrag_roda_pelo_hook_real         FALHOU

$ GONI_ONLY=regress_camrig ./test_core
regress_camrig                                 ...
  FALHOU  tests/test_sentinels.cpp:7766  test::nearEqF(cam->pos[0], 4.0f, 1e-3f)
  FALHOU  tests/test_sentinels.cpp:7769  test::nearEqF(cam->pos[0], 4.0f, 1e-3f)
  FALHOU  tests/test_sentinels.cpp:7773  o pitch passou do teto 70 (pos.y=0.000 — o clamp FIXO morreu?)
  FALHOU  tests/test_sentinels.cpp:7781  test::nearEqF(cam->pos[1], 4.0f * std::sin(-20.0f * ...), 1e-2f)
  FALHOU  tests/test_sentinels.cpp:7786  test::nearEqF(cam->pos[1], 4.0f * std::sin(-20.0f * ...), 1e-2f)
regress_camrig                                 FALHOU
```

Reposto → `camdrag_yaw_360_livre_e_normalizado` OK ·
`camdrag_pitch_clamp_fixo_menos_20_a_70` OK ·
`camdrag_consumo_zera_e_sem_input_e_noop` OK ·
`voni_motor_camdrag_roda_pelo_hook_real` OK · `regress_camrig` OK
(0 falhas nos 3 filtros).

### M-3.9x2 — o conflito camrig↔follow ignorado no compilador

Quebra: o gate do Validator `if (hasCamrig)` → `if (hasCamrig &&
false)` — o compilador aceita o camrig a dividir o tyker com
follow/look.

```
$ GONI_ONLY=camrig_follow ./test_core
camrig_follow_ou_look_no_mesmo_tyker_erro_que_ensina ...
  FALHOU  tests/test_voni.cpp:2120  !err.ok
  FALHOU  tests/test_voni.cpp:2121  err.message.find("camrig") != std::string::npos
  FALHOU  tests/test_voni.cpp:2122  err.message.find("follow") != std::string::npos
  FALHOU  tests/test_voni.cpp:2123  err.message.find("sobra") != std::string::npos
  FALHOU  tests/test_voni.cpp:2124  err.fixFrom == "follow"
  FALHOU  tests/test_voni.cpp:2125  err.fixTo == "camrig"
  FALHOU  tests/test_voni.cpp:2126  err.line == 2
  FALHOU  tests/test_voni.cpp:2135  !err.ok
  FALHOU  tests/test_voni.cpp:2136  err.message.find("look") != std::string::npos
  FALHOU  tests/test_voni.cpp:2137  err.fixFrom == "look"
  FALHOU  tests/test_voni.cpp:2138  err.fixTo == "camrig"
camrig_follow_ou_look_no_mesmo_tyker_erro_que_ensina FALHOU
11 teste(s) com falha

$ GONI_ONLY=regress_camrig ./test_core
regress_camrig                                 ...
  FALHOU  tests/test_sentinels.cpp:7818  err.fixTo == "camrig"
  FALHOU  tests/test_sentinels.cpp:7826  !err.ok
  FALHOU  tests/test_sentinels.cpp:7827  err.fixFrom == "look"
  FALHOU  tests/test_sentinels.cpp:7828  err.fixTo == "camrig"
regress_camrig                                 FALHOU
```

Reposto → `camrig_follow_ou_look_no_mesmo_tyker_erro_que_ensina` OK ·
`regress_camrig` OK (0 falhas nos 2 filtros).

## §5 · Os ficheiros

- `app/src/main/cpp/voni/VoniRegistry.cpp` — as 2 entradas (a fonte
  única) + as Docs do point/limit atualizadas;
- `app/src/main/cpp/voni/VoniTykers.h/.cpp` — compCamrig/compCamdrag +
  camYaw/camPitch no TykerState + as constantes FIXAS do pitch;
- `app/src/main/cpp/voni/VoniCompile.cpp` — o Validator do conflito
  (fixFrom/fixTo);
- `app/src/main/cpp/voni/Voni.h` — Host::lookDelta com o default false;
- `app/src/main/cpp/voni/VoniEngineHost.h/.cpp` — a delegação no hook
  (LookDeltaFn);
- `app/src/main/cpp/core/VoniSystem.h/.cpp` — setLookDeltaFn + o
  transporte nas 2 construções do EngineHost;
- `app/src/main/cpp/platform/main.cpp` — o acumulador + o LookTrack +
  o feed por frame (só Play, só dedos não reclamados);
- `app/build.gradle` — versionCode 101;
- `tests/test_voni.cpp` — pins ×10 + o FakeHost::lookDelta;
- `tests/test_wiring092.cpp` — pins ×4 (motor real + .goni);
- `tests/test_sentinels.cpp` — R-055;
- `docs/REGRESSOES.md` — R-055;
- `VONI_referencia.md` + `llms-full.txt` — regenerados (17549 bytes);
- `docs/RELATORIO-0.9.6.md` — §14 declara o par novo (R-015);
- `STATE.md` — §1/§2/§8 (299 linhas < 300);
- `docs/RELATORIO-0.12-X-ABERTURA.md` — a abertura (já commitada).
