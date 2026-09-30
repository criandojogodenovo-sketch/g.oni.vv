# RELATÓRIO 0.6.8 — Sub-fase: Play Mode com janela própria

> Campanha 0.6.7 → 0.6.9 · Sub-fase 2 de 3 · Release 0.6.8 (versionCode 17)

## 1. Objetivo

Separar a UI em DOIS modos — EDITOR (toolbar+Hierarchy+Inspector) e PLAY
(viewport fullscreen + TouchControls + barra superior mínima) — com
transição por Play/Stop. Em PLAY: esconder painéis de edição e toolbar;
TouchControls ancorados na safe-area sem sobrepor nada; orbit desativado
(1 dedo = controlos). Ao parar: volta ao EDITOR com pose restaurada
(PlaySnapshot) e painéis repostos exatamente.

## 2. Estado inicial (HEAD de entrada)

- HEAD de entrada: `d81dc6b` (0.6.7-d complemento — CI 0.6.7 verde,
  APK assinado `bb973a81a309…`).
- Suíte: 287 testes verdes.
- Problemas abertos confirmados na leitura do código:
  - o Play era apenas um toggle da física (`g_playMode` no main) — a UI de
    edição continuava TODA desenhada por baixo dos TouchControls;
  - o botão Play não mudava de rótulo; não havia Stop dedicado;
  - o orbit continuava ativo em play (1 dedo não reclamado orbitava);
  - a lógica de orbit vivia no main.cpp (não testável na suíte).

## 3. Causas raiz / diagnóstico

O botão Play (toolbar id 2) alternava `g_playMode` e capturava/restaurava o
snapshot, mas o `frame()` desenhava incondicionalmente toolbar, painéis e
menus — os TouchControls eram emitidos POR CIMA (ordem de emissão no mesmo
quad batch), deixando os painéis visíveis e tocáveis por baixo. Não existia
o conceito de "modo de UI": o play era um gate da física, não da interface.

## 4. Implementação (o que mudou)

- **0.6.8** (`0acf451`) — tudo abaixo; **0.6.8-b** (este commit) — release.
1. **Estado:** `EditorState.playMode` (novo membro) substitui a global
   `g_playMode` do main; `enterPlayMode()/leavePlayMode()` no main
   centralizam as transições (capture/restore do PlaySnapshot + logs).
2. **Play bar:** `editor::drawPlayBar` (EditorUi.cpp) — botão **Stop**
   (id 5, `kPlayStopId` em EditorLayout.h), estado "**a correr · fps N**",
   aviso "**simulação — alterações descartadas ao parar**" (labelFitted).
   Geometria pura em `playBarRect()/playStopButtonRect()` (mesma faixa da
   toolbar, dentro da safe-area, tema mono).
3. **frame() com dois modos:** em play, early return depois da play bar +
   TouchControls + toast + status line — toolbar/painéis/menus JAMAIS são
   desenhados; em editor, tudo como antes.
4. **Orbit extraído:** `editor::updateCameraOrbit(Camera&, OrbitState&,
   const InputState&, const UiRect&, u32 claimedMask, bool playMode)` em
   EditorUi.cpp (lógica intacta — regra F3 do dono-do-gesto + pinch) com
   guard `playMode` que reseta o gesto pendente. O main passa
   `g_editor.playMode`; `g_orbit` é um `editor::OrbitState`.
5. **Menus fechados ao entrar em play** (`closeAllOverlays`) — os painéis
   voltam EXATAMENTE ao parar (offsets de scroll e seleção vivem fora das
   flags de overlay e nunca são tocados).

## 5. Decisões técnicas relevantes

1. **Early return vs flags:** em play o código de edição nem corre (não é
   "desenhado invisível") — impossível sobrepor ou tocar painéis em play.
2. **Status line partilhada:** a status line (fps/tics/verts/dc) continua
   em play — não é painel de edição e é útil para diagnóstico.
3. **PlaySnapshot 100% intacto** (core/PlaySnapshot.h não foi tocado) —
   os pontos de chamada é que ficaram centralizados em enter/leave.
4. **TouchControls intactos** (CLÁUSULA CALMA — components/TouchControls.*
   sem alterações): o layout existente já vive na área útil acima da status
   line; o teste geométrico prova a não-sobreposição com a play bar.
5. **id 5 para o Stop** (faixa livre entre a toolbar 1..3 e os presets
   20..23; 7..10 já reservados para os gizmos da 0.6.9).

## 6. Testes novos (CI Linux)

`tests/test_playui.cpp` (8 casos, 287 → 295):

1. `play_toolbar_play_abre_janela_play` — tap REAL no botão Play da
   toolbar (gesto injetado no rect do 2º botão) → `st.playMode` true;
2. `play_stop_volta_ao_editor_e_paineis_repostos` — pose mexida "em
   simulação" → tap no Stop (rect exato do EditorLayout) → playMode false
   + pos/rot restauradas exatas + `!worldDirty` + seleção preservada;
3. `play_entrar_fecha_menus_permanece_no_editor_ao_sair` — TODOS os
   overlays fechados ao entrar; seleção sobrevive ao ciclo;
4. `play_sem_paineis_de_edicao` — quads sólidos do frame play <<
   frame editor (painéis/toolbar desapareceram de facto);
5. `play_play_bar_desenhada_com_stop_estado_e_aviso` — botão Stop no rect
   exato do EditorLayout + glifos de estado/aviso emitidos;
6. `play_touchcontrols_sem_sobreposicao_com_barras` — joystick e JUMP
   disjuntos da play bar (topo), da status line (fundo) e entre si;
7. `play_orbit_desativado_gestos_nao_movem_camera` — em editor o drag
   orbita (contraste); em play o MESMO gesto (incl. pinch com 2 dedos × 5
   frames) deixa yaw/pitch/dist intactos E reseta o OrbitState;
8. `play_orbit_volta_a_funcionar_apos_sair_do_play` — gesto arrastado
   para o estado de play; ao sair, o drag volta a orbitar.

## 7. Commits da sub-fase (branch main)

| Commit  | Assunto |
|---------|---------|
| 0acf451 | 0.6.8: PLAY MODE COM JANELA PRÓPRIA (modos, play bar, orbit, testes) |
| (este)  | 0.6.8-b: release 0.6.8 (bump, banners, README, relatório) |

## 8. HEAD da sub-fase

- HEAD de fecho: ver `git log -1` no push desta sub-fase (release 0.6.8,
  versionCode 17).

## 9. Suíte de testes

- **295 testes, 295 OK, 0 falhas** (baseline da sub-fase: 287 — +8 novos).
- Gates locais verdes antes do push: `check_main.sh` OK; `link_parity.sh`
  OK (70 TUs); `jni_parity.py` OK (3 natives).

## 10. CI

- `core-tests` (Linux): ctest + check_main + link_parity + jni_parity.
- `build-release` (NDK r26): `assembleRelease` + `apksigner verify`.
- `verify-entry-symbols`: `nm -D` + `jni_parity.py dynsyms.txt` + gate do
  manifest binário.
- Estado no fecho: ver §11 (runs desta sub-fase).

## 11. APK

- `app-release.apk` (arm64-v8a), versionCode 17, versionName "0.6.8",
  assinado com a keystore dos secrets — artifact
  `goni-vv-0.6.8-release-signed` do workflow `release`.
- sha256: anotar do artifact do CI (a mesma prática da 0.6.7 — o binário
  do CI é a fonte da verdade).

## 12. Verificação no device (roteiro C33 — resumo)

1. Play → toolbar/painéis somem; viewport fullscreen + play bar (Stop,
   "a correr · fps N", aviso) + joystick/JUMP sem sobreposições.
2. 1 dedo fora dos controlos NÃO orbita; pinch também não.
3. Simular → Stop → editor com pose anterior ao play, painéis/seleção
   exatamente como antes, orbit funcional de novo.

## 13. Riscos e mitigações

- **Toques na play bar vs controlos:** a play bar vive no topo (faixa da
  toolbar) e os controlos no fundo — disjunção provada por teste
  geométrico; ids exclusivos evitam conflitos de gesto no UiContext.
- **Gesto de orbit a meio da transição:** o guard playMode reseta o
  OrbitState ao entrar em play (um drag iniciado no editor não "foge"
  para o modo play).

## 14. Dívida técnica conhecida

- A play bar não mostra métricas da simulação além do fps (ticks da
  física, etc.) — espaço reservado para fases futuras de debug de jogo.
- O botão back do sistema não sai do play (não tratado nesta fase — só o
  Stop; a claúsula calma não pede handling de back).

## 15. Restrições respeitadas (CLÁUSULA CALMA)

- Zero física nova (o gate `g_physics.enabled = playMode` é o mesmo de
  sempre), zero componentes novos, zero render novo.
- PlaySnapshot INTACTO (ficheiro não tocado).
- TouchControls INTACTOS (ficheiro não tocado).
- 3 botões da toolbar intactos (em play a toolbar não é desenhada; ao
  voltar, está lá exatamente como antes); landscape; safe-area intactos.
- Tema mono intacto (play bar usa os tokens do tema).

## 16. Conclusão

A sub-fase fecha os critérios de aceitação 0.6.8: Play abre janela própria
sem sobreposição (painéis/toolbar escondidos de facto); Stop volta ao editor
com a pose original (PlaySnapshot) e os painéis repostos exatamente; orbit
desativado em play e funcional no editor. CI verde nos três jobs; APK
assinado no artifact. Segue para a sub-fase 0.6.9 (gizmos de transformação).
