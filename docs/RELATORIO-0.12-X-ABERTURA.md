# RELATORIO-0.12-X-ABERTURA.md — Campanha 0.12-X · Câmera de Terceira Pessoa (camrig + camdrag)

Atualizado: 2026-10-19 · versionCode de partida: **100** · último commit da
0.12-U: `c7cf843` (0.12-U FECHO, CI **success**).

Cláusula calma: só o que está no prompt da campanha — o core da V.ONI e a
Câmara. Sem refactors, sem tocar em UI ou renderização. Regra de
convivência: nada disto se declara fechado fora do que o prompt manda.

## §1 · O estado real (VERIFICA §0.2 — as 3 verificações)

| # | Verificação | Resultado |
|---|---|---|
| 1 | O vc está no 100 ou superior? (a campanha U fechou) | **SIM** — `app/build.gradle:29` declara `versionCode 100`; o STATE.md fecha a 0.12-U («Campanha 0.12-U (V.ONI BLOCO A) — FECHADA (3.9a..3.9c + o FECHO, vc 98–100)»); o último commit é o FECHO `c7cf843` |
| 2 | Os gates (docs-lint, R-013, scope-check) verdes? | **VERDE** — docs-lint local: «VERDE — 114 ficheiros» · release-identity: «GATE VERDE … versionCode 100» · o R-013 (`regress_bijeção_da_ajuda`) OK na suíte local; o scope-check afere o diff no push (o job CI do FECHO correu **success** nos 5 jobs) · CI GitHub: run do `c7cf843` = **success** |
| 3 | O STATE.md tem a campanha anterior como fechada? | **SIM** — §1 do STATE.md lista a 0.12-U como FECHADA com o platformer do juiz final VERDE |

Prova da suíte local: `test_core` compilado com CMake 4.4.4/g++17 — 1159
testes OK, **1 falha local**: `sentinela_r039_50m_verts_ram_pico_512mb`
(errno 28 DISCO CHEIO na escrita da fonte de 1754 MB — o flake limitado
pelo disco já documentado na 0.12-E; o espaço de `/tmp` da sandbox é menor
que o pico do teste. O CI é o árbitro — o run do FECHO fechou success).
As sentinelas da U (`regress_voni_matematica`, `regress_spawn`,
`regress_voni_timers`, `platformer_bloco_a_juiz_final`,
`regress_bijeção_da_ajuda`) todas OK.

## §2 · REPORTAR ANTES DE CODAR (as 5 perguntas, com ficheiro:linha)

**1. Qual TIC/CameraComp é a câmara do jogo no Play e como o main.cpp a lê?**
O 1.º TIC ATIVO com `CameraComp` de `active==true` + `Transform3D` —
`findActiveCameraTic()` em `core/CameraUtil.cpp:9` (o invariante «uma
câmara ativa por cena» vive no mesmo ficheiro — `enforceSingleActiveCamera`).
O `main.cpp` lê-a POR FRAME no `frame()`: `gameCamTic =
findActiveCameraTic(g_scene)` só em `g_editor.playMode`
(`platform/main.cpp:6184`), tira a pose (`gameCamTr`,
`main.cpp:6186-6187`) e os parâmetros (`gameCamComp`, `main.cpp:6188`); a
pose alimenta o render (`camgizmo::gameView(*gameCamTr)`,
`main.cpp:6251`) e a base de movimento da física
(`g_physics.frame.fwd/right`, `main.cpp:6191-6206`). O camrig/camdrag
escrevem NESTA pose (pos + rot do Transform3D da câmara ativa) — o render
e a física leem-na no mesmo ciclo, sem nada a mais a ligar.

**2. O eixo da frente da câmara: +Z ou −Z? Ordem Euler do Quat::fromEuler?**
**−Z** é a direção de visão e **+Y** o up (`components/CameraComp.h:7-8`;
`camgizmo::gameForward` = `rot.rotate({0,0,-1})` — `ui/CamGizmo.h:225-227`;
`gameView` olha de `pos` para `pos+fwd` — `ui/CamGizmo.cpp:505-512`).
`Quat::fromEuler(pitchX, yawY, rollZ)` usa a ordem **YXZ** (`q = qy·qx·qz`
— `math/Math.h:204-208`), em RADIANOS; a propriedade `rot` da V.ONI é
GRAUS Euler XYZ e o EngineHost converte com `fromEuler(deg2rad(x),
deg2rad(y), deg2rad(z))` (`voni/VoniEngineHost.cpp:152-155`). Derivação
que o camrig usa (a mesma do `alignToView`, `ui/CamGizmo.h:231-236`): com
`rot = fromEuler(pitch, yaw, 0)`, o −Z local aponta
`(−cos(p)·sin(y), sin(p), −cos(p)·cos(y))` — o yaw que aponta o −Z a um
alvo é `atan2(−dx, −dz)` e o pitch `asin(dy/|d|)`. Nota honesta: o `look`
existente (0.9.5) usa `yaw = atan2(dx, dz)` — a convenção DELE para
objetos, fechada pelos pins da casa e FORA do âmbito (cláusula calma);
o camrig usa a convenção da CÂMARA (−Z), documentada aqui e nos pins.

**3. Transform3D::pos é local ou mundo? Há helper de mundo?**
`pos` é a posição na CENA e funciona como MUNDO nas cenas 3D (TICs numa
lista plana — não há pais que transformem): o render usa `camEye =
gameCamTr->pos` diretamente (`main.cpp:6253`), o áudio idem
(`main.cpp:2459`), a física idem. O campo `world` é a matriz TRS
cacheada, recalculada pelo TransformSystem/`updateWorld()`
(`components/Transform3D.h:16-33`) — não há helper «para mundo» para pos
porque pos já é mundo na prática. O camrig lê/escreve `pos` pelo MESMO
canal dos componentes existentes (`ticPos`/`setTicPos` —
`voni/VoniTykers.cpp:37-62` → `Host::getProp/setProp({"pos"})`).

**4. O TykerState guarda estado por tyker entre frames? Onde (orbitAngle
vive onde)?**
SIM — `tykers::TykerState` (`voni/VoniTykers.h:100-124`) vive em
`Script::Impl::tykerRuns` (`voni/VoniVm.cpp:1613-1645`), criado no
runStart e vivo entre frames; morre com o Impl (o stop/restart recomeça).
`orbitAngle` vive em `TykerState.orbitAngle` (`VoniTykers.h:116`) — o
`yaw`/`pitch` do camdrag vão para o MESMO sítio (campos novos ao lado).
Os contínuos resolvem args 1× na ATIVAÇÃO (`activateTyker` →
`resolveComp`, `VoniVm.cpp:1652-1683`) e ticam por frame via `tickTykers`
(`VoniVm.cpp:1688-1723`) — o camrig encaixa exatamente aqui (CompKind::
Continuo, zero parser).

**5. O InputState expõe o arrasto do dedo como (dx, dy)? (Se não, como o
TouchControls lê o input?)**
NÃO — o `InputState` expõe `down/pressed/released/pos` POR SLOT
(`platform/InputState.h:41-44`), nunca deltas. O orbit calcula o delta à
mão: guarda o último pos no `OrbitState` e faz
`cam.orbit((x−st.x)·kSens, (y−st.y)·kSens)` (`ui/EditorUi.cpp:4620-4638`).
Os slots RECLAMADOS pela UI chegam por máscaras: `feedTouchControls`
devolve `claimed` (joystick/botão — `main.cpp:4861-4903`), `canvasClaimed`
(UI de play — `main.cpp:6045-6050`) e `gizmoClaimed` (editor só — 0 em
Play). O camdrag usa o MESMO caminho: o main soma o delta
(pos-atual−pos-anterior, por slot) dos dedos NÃO reclamados e alimenta o
hook `Host::lookDelta(dx, dy)`; o tick da V.ONI consome o acumulador (o
padrão do `g_physics.frame.fwd`: o frame alimenta, o tick do passo fixo
seguinte consome — a casa já corre assim para o input da física).

## §3 · O que a campanha entrega (as decisões fechadas do prompt)

- `camrig(d, h, suav)` — Contínuo (argc 2..3): distância 3D `d` do alvo,
  `h` acima dele (o pivô é alvo+(0,h,0)), olha para `alvo + h/2`,
  suavizada por `(1 − e^(−suav·dt))`; sem suav = snap (o padrão do
  follow).
- `camdrag(sens)` — Contínuo (argc 1..1): yaw 360° livre, pitch clamp
  FIXO −20..70 graus; delta via `Host::lookDelta(dx, dy)` (default false
  = hospedeiro sem input — o script não morre); só lê o que o
  TouchControls não reclamou; só corre em Play (o main só alimenta em
  Play).
- `limit(min, max)` — clamp final da distância d, APÓS a suavização (o
  `clampLimit` existente, contra o pivô).
- yaw/pitch vivem no `TykerState` (junto ao `orbitAngle`).
- `camrig` + (`follow` OU `look`) no mesmo tyker = **ERRO NO COMPILADOR**
  (mensagem que ensina + `fixFrom`/`fixTo` para o botão Substituir).
- Args resolvem 1× na ativação (o caminho do `resolveComp` — zero
  parser). Pitch −20..70 é FIXO. A câmara corre ANTES da física (a V.ONI
  está no TickGroup::Update, a física no Physics — `main.cpp:8507-8511` —
  a ordem já é esta; nada a mudar).
- Sem alvo válido (TIC sem Transform3D) = erro cedo com linha (o caminho
  do `ticPos` → `vm.fail`).

Plano de teste: 10 pins VM (`tests/test_voni.cpp`) + pins engine real e
round-trip `.goni` (`tests/test_wiring092.cpp`); sentinela
`regress_camrig` (R-055, `tests/test_sentinels.cpp`); mutações M-3.9x1 e
M-3.9x2 coladas no relatório de fecho; referência pública regenerada
(R-013 byte a byte); STATE.md no mesmo commit; versionCode 100 → 101.
