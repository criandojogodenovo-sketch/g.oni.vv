# RELATÓRIO F5.5 — Sub-fase: All Files Access de verdade (fix do C33 0.6.9)

## 1. Objetivo

Fazer o All Files Access funcionar de verdade no device: no C33 (0.6.9), a
G.One VV NÃO aparecia na lista "Acesso a todos os ficheiros" (enquanto o
Godot aparecia "Permitida") e o interruptor na página da app não concedia
nada. Causa: o manifest não declarava
`android.permission.MANAGE_EXTERNAL_STORAGE` — logo o sistema não tinha o que
conceder e `Environment.isExternalStorageManager()` era sempre `false`.
Objetivos da sub-fase: (a) declarar a permissão no manifest; (b) manter o
fluxo existente (diálogo in-app → "Permitir" →
`ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION` → voltar → re-verificar
`isExternalStorageManager()` no onResume → prosseguir para import/export);
(c) logar `storage: all-files granted=1/0` no engine.log a cada transição
(visível no log viewer); (d) documentar a decisão e a implicação Play Policy
em `docs/`; (e) testes CI da lógica de re-verificação e do fluxo com stubs.

## 2. Estado inicial (HEAD de entrada)

- HEAD de entrada: `4ac04fc` (fecho da campanha 0.6.7→0.6.9 — release
  0.6.9, versionCode 18, 321 testes, 3 releases com APK assinado).
- Fluxo All Files Access já implementado desde a F5.2 (diálogo, intent,
  re-verificação por `onActivityResult`, fallback app-private), handshake
  invertido F5.3 a funcionar (log do C33 mostrava
  `handshake=1 supported=1 manager=0`) — ou seja: a ponte Java estava VIVA,
  o sistema SUPORTAVA, mas o `manager` nunca virava 1 porque **não havia
  permissão declarada para conceder**.
- `docs/SAF_EXCEPTION.md` documentava (erradamente) como opção de design:
  "Nenhuma permissão no manifest: MANAGE_EXTERNAL_STORAGE NÃO é declarado".

## 3. Arquitetura escolhida

A causa raiz é uma linha de manifest — a arquitetura do fluxo (F5.2/F5.3)
estava correta e foi MANTIDA intacta. A F5.5 acrescentou apenas a camada
que faltava:

- **Manifest** — `<uses-permission android:name="android.permission.
  MANAGE_EXTERNAL_STORAGE" />`: é a declaração que (1) faz a app aparecer na
  lista "Acesso a todos os ficheiros" das definições do sistema, (2) dá ao
  interruptor uma permissão para conceder, e (3) permite que
  `isExternalStorageManager()` retorne `true` após a concessão. A permissão
  NÃO é pedida no arranque nem na instalação (não é uma permissão runtime
  clássica — só o ecrã especial do sistema a concede).
- **Re-verificação no RESUME (nativo)** — caso novo tratado: nem todo o
  ecrã de settings OEM termina com `setResult` (o `onActivityResult` pode
  nunca chegar; a volta também pode ser pelos recents). O handler novo de
  `APP_CMD_RESUME` no `onAppCmd` (thread da engine): (1) drena a fila
  `PendingResult` primeiro — o `onActivityResult` corre ANTES do
  `onResume` no lifecycle Java, por isso o retorno normal já foi tratado
  pelo `onStorageResult`; (2) se o fluxo ainda está `PendingSettings`,
  chama `storage::resumeRecheck` — política PURA em `StoragePerm.h`
  (GL-free, afervel no CI) — com a verificação FRESCA de
  `isExternalStorageManager()` (via JNI, thread anexado). Concedido →
  `Granted` + ação pendente retomada (import prossegue sem re-pedir);
  recusado → `Idle` + modo app-private + toast claro, SEM relançar as
  definições (nenhum loop — a próxima tentativa é sempre do utilizador).
- **Log de transição** — `storage: all-files granted=1/0 (<onde>)` emitido
  pela helper `logAllFilesGranted` do `main.cpp` com deteção de mudança
  (`g_grantedLogged`): força linha nova em TODAS as transições que
  importam (boot, retorno das definições, re-verificação no resume) e nas
  tentativas de import/export apenas quando o valor observado MUDA
  (conceder/revogar fora do fluxo da app) — sem spam, visível no log
  viewer in-app.
- **Gate CI** — o job `verify-entry-symbols` passou a exigir a permissão no
  manifest BINÁRIO do APK (`aapt2 dump xmltree`): a presença aferida é a
  que o sistema Android realmente vê, não a do XML fonte.

## 4. Implementação (por ficheiro, commit `f5.5-a`)

1. `app/src/main/AndroidManifest.xml` — `uses-permission` nova com
   comentário da causa raiz e da decisão Play Policy.
2. `platform/StoragePerm.h` — `resumeRecheck(PermFlow&, supported,
   isManager)`: guard de estado (`PendingSettings`) + transição única via
   `onSettingsReturn(supported && isManager)`; contrato documentado (o
   chamador drena a fila ANTES).
3. `platform/main.cpp` —
   - `case APP_CMD_RESUME` no `onAppCmd` (drain → guard → verificação
     fresca → `resumeRecheck` → log → `resumePendingAfterReturn`);
   - `logAllFilesGranted()` + `g_grantedLogged` (transições);
   - `resumePendingAfterReturn()` (cauda do retorno: toast claro + ação
     retomada 1× via `takePendingAction`) e `finishStorageReturn()`
     (transição + cauda, usada pelo `onStorageResult`);
   - boot passa a logar `granted=1/0 (boot)`;
   - `storageGrantedNow` loga variações detetadas por tentativa.
4. `tests/test_storageperm.cpp` — 5 testes novos (ver §6).
5. `.github/workflows/release.yml` — gate 4 do manifest binário.
6. `docs/SAF_EXCEPTION.md` — "Consequências" corrigido, decisão Play
   Policy revista, secção F5.5 nova.
7. `README.md` — título, "Escopo F5.5" e roteiro de verificação C33 F5.5.

## 5. Decisões técnicas relevantes

- **Sem bump de versão** (versionCode 18 / "0.6.9" mantidos): a tarefa
  especifica CLÁUSULA CALMA (manifest + docs + lógica de verificação) e
  1–2 commits; um bump arrastaria banners/README/artifact. O APK desta
  sub-fase distingue-se pelo commit e pelo sha256 (§11) — instalar por
  cima (`adb install -r`) é o fluxo normal.
- **Re-verificação no RESUME é nativa** (via JNI do thread da engine), sem
  tocar em Java: a `VvActivity` continua INTACTA — o `onResume` Java já
  reforça o handshake (`nativeRegisterActivity`), que é tudo o que o
  nativo precisa para chamar `Environment.isExternalStorageManager()`.
- **Drain antes do recheck**: garante que o caminho normal
  (`onActivityResult` → fila → `onStorageResult`) processa primeiro e o
  resume nunca duplica a ação retomada (`takePendingAction` consome 1×; o
  resultado tardio, se chegar, encontra `Granted` e não retoma nada).
- **Recusa no resume NÃO relança nada**: fica `Idle`/app-private com toast
  claro — a semântica "sem nag" da F5.2 mantém-se (o `resumeRecheck` em
  `PendingSettings` apenas decide o retorno pendente; nunca abre
  definições por iniciativa própria).
- **Nenhum gate removido**: o `verify-entry-symbols` só GANHOU a aferição
  da permissão (item 4 do gate do manifest binário); todos os outros
  (hasCode, ProjectManagerActivity, VvActivity, lib_name, símbolos de
  entrada, jni_parity) intactos.

## 6. Testes novos (CI Linux)

`tests/test_storageperm.cpp` — 321 → **326** (+5), sobre a política pura
`storage::resumeRecheck` (stub = a própria máquina `PermFlow`, como no
resto da suíte):

1. `storage_resume_concede_import_prossegue` — conceder → resume → import
   prossegue: `Granted` + modo all files + ação retomada 1× + tentativa
   seguinte sem diálogo.
2. `storage_resume_recusa_toast_claro_sem_loop` — recusar → resume →
   `Idle` + app-private + ação abortada; só NOVA tentativa do utilizador
   reabre o diálogo; o pedido de intent nunca fica pendente da recusa.
3. `storage_resume_sem_pendente_nao_mexe` — guard: `Idle` (arranque frio),
   `Granted` e `DialogOpen` não mexem em nada; API < 30 no resume recusa
   (nunca concede o impossível).
4. `storage_resume_drain_primeiro_resultado_tratado_nao_duplica` — o
   retorno chegou pela fila (caminho normal): o recheck que se segue
   devolve `false` e o import NUNCA corre 2×.
5. `storage_resume_resultado_tardio_nao_duplica` — o resume decide sem
   resultado; o `onActivityResult` tardio chega depois: `onSettingsReturn`
   idempotente e a ação já foi consumida (o export NUNCA corre 2×).

A presença da permissão no manifest é verificada a) no CI pelo gate do
manifest binário do APK e b) no device pelo roteiro (§12, item 1).

## 7. Commits da sub-fase (branch main)

| Commit  | Assunto |
|---------|---------|
| d17f9ed | f5.5-a: ALL FILES ACCESS DE VERDADE — MANAGE_EXTERNAL_STORAGE declarado + re-verificação no resume + log granted=1/0 + gate CI do manifest binário + 5 testes + docs |
| (este)  | f5.5-b: RELATÓRIO F5.5 (relatório 1-16 + sha256 do APK assinado) |

## 8. HEAD da sub-fase

- HEAD de fecho: o commit `f5.5-b` (este relatório) sobre `d17f9ed`.
- Versão mantida: versionCode 18, versionName "0.6.9" (sem bump — §5).

## 9. Suíte de testes

- **326 testes, 326 OK, 0 falhas** (baseline: 321 — +5 novos).
- Gates locais verdes antes do push: `check_main.sh` OK (main.cpp com o
  `APP_CMD_RESUME` novo compila contra os stubs); `link_parity.sh` OK
  (71 TUs); `jni_parity.py` OK (3 natives, zero alterações Java).

## 10. CI

- `core-tests` (Linux): ctest (326) + check_main + link_parity +
  jni_parity.
- `build-release` (NDK r26): `assembleRelease` + `apksigner verify`.
- `verify-entry-symbols`: `nm -D` + `jni_parity.py dynsyms.txt` + gate do
  manifest binário (AGORA com a permissão aferida).
- CI da sub-fase (run **36730097048**, HEAD `d17f9ed`): core-tests
  **verde**, build-release **verde** (APK assinado), verify-entry-symbols
  **verde** — incluindo o gate novo
  `android.permission.MANAGE_EXTERNAL_STORAGE` presente no manifest binário.

## 11. APK

- `app-release.apk` (arm64-v8a), versionCode 18, versionName "0.6.9",
  assinado com a keystore dos secrets — artifact
  `goni-vv-0.6.9-release-signed` do workflow `release` (run 36730097048,
  artifact id 11104266590).
- APK CUMULATIVO: cobre 0.6.7 (lifecycle GL + gestão de projetos) + 0.6.8
  (play mode) + 0.6.9 (gizmos) + **F5.5 (All Files Access de verdade)**.
- Distinguir do APK 0.6.9 original: mesma versão — usar o sha256 abaixo.
- sha256 do APK assinado:
  `1df094b0a696cc9da0023bf8a73bb12163b1e7fa2b81500ae176771a2d89f503`
- Verificação extra local do manifest binário do artifact: a string
  `android.permission.MANAGE_EXTERNAL_STORAGE` está presente no
  `AndroidManifest.xml` compilado (pool UTF-16LE) — o mesmo que o gate CI
  aferiu.

## 12. Verificação no device (roteiro C33 — resumo)

1. Definições → Privacidade → "Acesso a todos os ficheiros": a **G.One VV
   aparece na lista** (antes não aparecia).
2. Menu → Importar… → diálogo → Permitir → página da app nas definições →
   ativar o interruptor → voltar → toast "acesso concedido — File API
   direta" e a lista de Download/Documents abre SEM re-pedir (import
   retomado — pelo `onActivityResult` ou, se o ecrã não devolver
   resultado, pela re-verificação no resume).
3. Export Downloads → toast "exportado: Download/GOneVV/export/…" →
   ficheiro visível no gestor em `Download/GOneVV/export/`.
4. Settings → "Ver logs" → linha `storage: all-files granted=1` após
   conceder (`granted=0` antes) — nas transições: boot, retorno das
   definições, re-verificação no resume.
5. Recusa/fallback: toast claro "acesso não ativado — modo app-private",
   sem repetições/loop; projeto SAF continua completo.
6. Regressões: 0.6.9 (gizmos), 0.6.8 (play), 0.6.7 (lifecycle/apagar/
   sair), F5.4 (gestor multi-pasta).

## 13. Riscos e mitigações

- **Play Policy** — `MANAGE_EXTERNAL_STORAGE` é permissão restrita NA
  PUBLICAÇÃO: declará-la é permitido; distribuir por APK/sideload (caso
  atual) não passa pela análise. Se publicar: justificação documentada
  (docs/SAF_EXCEPTION.md) ou migração para SAF/MediaStore (isolada atrás
  de `fileapi::*`/`storage::*`). Mitigado com a decisão escrita no doc.
- **Ecrã OEM sem `setResult`** — o `onActivityResult` pode não chegar:
  mitigado pela re-verificação no `APP_CMD_RESUME` (drena a fila e decide
  com verificação fresca; testado nos casos 4/5 do §6).
- **Morte do processo ao alternar a permissão** — alguns Androids matam a
  app quando o appop muda: inócuo — o arranque frio seguinte verifica e
  loga `granted=1` no boot; a ação pendente perde-se (o utilizador volta
  a tocar em Importar e funciona direto, sem diálogo).
- **Resumo prematuro** (split-screen/multi-window com as definições ainda
  visíveis) — o resume com recusa limpa o `PendingSettings` e a ação
  pendente; auto-cura: a concessão posterior é detetada pela próxima
  tentativa (`storageGrantedNow`) ou pelo resultado que entretanto chegou.

## 14. Dívida técnica conhecida

- O `attempts()` do `PermFlow` (diagnóstico) não é mostrado no Settings —
  apenas no comportamento; se algum dia fizer falta, é uma linha no log.
- A re-verificação no resume cobre o fluxo All Files; a concessão dada
  fora dele (definições do sistema abertas diretamente) é detetada na
  próxima tentativa (log de variação) — não há polling contínuo (por
  decisão: zero custo em repouso).

## 15. Restrições respeitadas (CLÁUSULA CALMA)

- **Só manifest + docs + lógica de verificação**: zero física, zero
  render, zero componentes — os ficheiros tocados são exatamente o
  manifest, `StoragePerm.h` (política do fluxo de permissão),
  `main.cpp` (handler de lifecycle + logging do MESMO fluxo — sem tocar
  em física/render/componentes), `test_storageperm.cpp` (testes),
  `release.yml` (gate), `docs/` e `README.md`.
- **Nenhum gate removido**: todos os gates anteriores intactos (o do
  manifest binário ganhou o item 4).
- **Tema mono, landscape intactos**: nenhuma UI mudou (os toasts e o
  diálogo são os já existentes da F5.2).
- **PlaySnapshot / ponte Java / gates F5.4**: intocados (zero Java nesta
  sub-fase).

## 16. Conclusão

A linha que faltava desde a F5.2 estava no manifest — sem a declaração, o
sistema não tinha o que conceder e o fluxo (correto por si) nunca podia
terminar em concessão. Com a permissão declarada, a re-verificação no
resume a cobrir os ecrãs OEM sem resultado, o log `granted=1/0` em cada
transição e o gate CI a afervar o manifest binário do APK, o All Files
Access passa a ser verificável de ponta a ponta: lista do sistema →
interruptor → `isExternalStorageManager()==true` → File API direta em
Download/Documents. A sub-fase fechou com 326 testes verdes, CI 100%
verde e APK assinado cumulativo — aguardando a confirmação no C33
(roteiro §12) para VERIFIED.
