# AUDIT_SAF_JNI.md — Auditoria e correção da ponte JNI/SAF (F5.1-hotfix, parte 2)

Data: 2026-09-29 · Base: 0.6.0 (5b6d607) → fix em 0.6.1
Âmbito: AndroidManifest.xml · vv/goni/VvActivity.java · vv/goni/SafIo.java ·
platform/SafBridge.cpp · platform/SafIoJni.cpp · platform/Saf.h

## 1. Evidência recolhida (APK 0.6.0 real, artifact goni-vv-0.6.0-release-signed)

| Verificação | Resultado |
|---|---|
| (a) manifest → subclasse correta | **OK** — `package="vv.goni"`, `<activity android:name="vv.goni.VvActivity">`, `android.app.lib_name=goni_vv`, `hasCode` presente (true), extraído do AndroidManifest.xml BINÁRIO dentro do APK |
| (b) pacote Java vs símbolo C++ | **OK** — `vv.goni.VvActivity` ⇔ `Java_vv_goni_VvActivity_nativeOnActivityResult` (o nome `Java_com_goni_runtime_*` citado no briefing era uma suposição; o pacote real é `vv.goni` e bate certo) |
| (c) métodos nativos registados | **PARCIAL — fix aplicado** — `nativeOnActivityResult` existia como símbolo exportado no .so (verificado com `nm -D`) mas era resolvido LAZIAMENTE por nome, sem registo explícito nem log; `classes.dex` contém `vv/goni/VvActivity`, `vv/goni/SafIo`, `nativeOnActivityResult` e `Landroid/app/NativeActivity;` (strings verificadas no dex) |

Conclusão da parte estática: **a fiação manifest/dex/símbolos do 0.6.0 está correta** —
o crash de arranque é RUNTIME. Os defeitos abaixo são os que a auditoria encontrou
no código da ponte; todos corrigidos e cada um agora loga OK/FAIL no `engine.log`.

## 2. Defeitos encontrados e correções

### D1 — Exceções JNI pendentes esquecidas (gravidade: ALTA — candidato ao crash)
`initJava()` chamava `GetMethodID` ×3 e `loadAppClass()` chamava `loadClass` sem
`ExceptionCheck/ExceptionClear`. Basta um lookup falhar (dex/assinatura) para uma
exceção ficar PENDENTE; qualquer chamada JNI seguinte com exceção pendente tem
comportamento indefinido (ART aborta em builds com checks). **Fix:** cada
lookup/chamada agora verifica, limpa e loga (`jni: … NÃO encontrada` /
`loadAppClass(…) FALHOU`).

### D2 — Registo nativo lazy sem log (gravidade: MÉDIA — diagnóstico impossível)
`nativeOnActivityResult` resolvido por nome no 1º uso. Falha → `UnsatisfiedLinkError`
sem nenhuma linha nossa no log. **Fix:** `JNI_OnLoad` novo com `RegisterNatives`
explícito (`(IIILandroid/net/Uri;I)V`); falha de registo MORRE NO ARRANQUE com
a causa escrita no `engine.log` (o `JNI_OnLoad` ativa o `elog` com o diretório
fallback do device — corre antes do `android_main`). O símbolo `Java_vv_goni_…`
continua exportado (gate `verify-entry-symbols` estendido no commit 6).

### D3 — Handler SAF corria no thread da UI (gravidade: ALTA — crash no 1º picker)
`onActivityResult` chega no thread Java (UI). O handler antigo fazia logo aí
`g_resources.releaseAll()` + `g_gpu.releaseAll()` + `loadActiveScene(resolveTex)`
→ **chamadas GLES sem contexto EGL corrente** (o contexto vive no thread da
engine) + corrida de estado com o frame em curso. **Fix:** `PendingResult`
(saf.h, 1 slot + mutex, GL-free) — o JNI só enfileira; o loop da engine faz
`pollResult()` e processa no thread certo, com EGL corrente. Testes CI:
round-trip e sobreposição da fila (test_saf.cpp).

### D4 — Guardas nulos insuficientes no `safInitJniBackend` (gravidade: MÉDIA)
`GetObjectClass(loader)` sem `loader != null`; `midLoader`/`midLoad` nunca
verificados. Qualquer falha de JVM/classloader → UB. **Fix:** todos os passos
com guarda + log (`jni/saf: …`), exceções sempre limpas.

### D5 — `SafJniBackend::init/initSingles` falhavam em silêncio (gravidade: BAIXA)
Se `GetStaticMethodID` falhasse não havia forma de saber QUAL método. **Fix:**
lookup por tabela com log individual (`SafIo.ioExists OK` / `NÃO encontrada
(assinatura)`).

## 3. Estado depois do fix

Cada passo da ponte escreve no `engine.log` (Android/data/vv.goni/files/logs/):

```
jni: JNI_OnLoad — G.One VV 0.6.1 (registo explícito de nativos)
jni: JNI_OnLoad — nativeOnActivityResult registado OK (1 método)
jni: initJava — cache de activity/classes/métodos
jni: VvActivity.openTreePicker OK / openImportPicker OK / openExportPicker OK
jni: vv.goni.SafIo carregada (classloader da activity)
jni/saf: init backend JNI
jni/saf: SafIo.ioExists OK … (5 métodos) … singles ok
[boot 2/6] storage OK …
[boot 6/6] scene OK → editor …
```

Se o arranque morrer ANTES destas linhas, o `crash-<ts>.dump` no mesmo
diretório mostra a função C++ exata (offset) — sem PC, sem logcat.
