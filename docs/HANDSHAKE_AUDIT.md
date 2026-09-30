# Auditoria do handshake Java↔native (F5.3) — a causa única de 0.6.0→0.6.2

## Sintoma no C33 (log viewer, 0.6.2)

```
jni: initJava env/activity indisponíveis (vm=0x…)
storage: All Files Access — supported=0 manager=0 → modo app-private
fileapi: export … FALHOU (copied=0)
```

Todas as features Java-dependentes falham desde a 0.6.0 por ESTA cadeia.
As auditorias estáticas anteriores (manifest/dex/símbolos —
docs/AUDIT_SAF_JNI.md) estavam corretas: o defeito é o **handshake em
runtime**.

## 1. Verificação do APK REAL 0.6.2 (CI, artifact `goni-vv-0.6.2-release-signed`, HEAD `5ec2bfe`)

Download do artifact via API do GitHub + despejo do manifest binário
(AXML → texto, `pyaxmlparser`):

```xml
<manifest android:versionCode="12" android:versionName="0.6.2" package="vv.goni" …>
  <application android:label="G.One VV" android:hasCode="true" …
               android:theme="@android:01030007">   <!-- Theme.NoTitleBar.Fullscreen -->
    <activity android:name="vv.goni.VvActivity" android:exported="true"
              android:screenOrientation="6"         <!-- SENSOR_LANDSCAPE -->
              android:configChanges="0x000004A0">
      <meta-data android:name="android.app.lib_name" android:value="goni_vv"/>
      <intent-filter>…MAIN + LAUNCHER…</intent-filter>
    </activity>
  </application>
</manifest>
```

| Aferição | Resultado |
|----------|-----------|
| `android:hasCode` | **true** — dex existe e é executado |
| Activity launcher | **`vv.goni.VvActivity`** (NÃO `android.app.NativeActivity`) |
| `android.app.lib_name` | **`goni_vv`** — loadLibrary corre no `super.onCreate` |
| `screenOrientation` | 6 = `SENSOR_LANDSCAPE` (intacto) |
| tema | 0x01030007 = `Theme.NoTitleBar.Fullscreen` (intacto) |
| `classes.dex` | `Lvv/goni/VvActivity;` com `nativeOnActivityResult`, `openAllFilesSettings`, `exportLogsToDownloads` |

**VEREDICTO: o sistema instancia a VvActivity e carrega a lib.** Mas o dex
de 0.6.2 NÃO contém `onCreate`/`onResume`/`nativeRegisterActivity` — a
activity nunca avisa o nativo de que existe.

## 2. Causa raiz (confirmada no código)

`storage::initJava(app->activity->vm, app->activity->clazz)` era chamado do
**android_main**, que corre no **thread do glue** (`android_app_entry` num
pthread criado por `android_app_create` no thread da UI). Esse thread **não
está anexado à VM**:

```
JNI_GetEnv() num thread NÃO anexado → JNI_EDETACHED → env = nullptr
  → "jni: initJava — env/activity indisponíveis (vm=0x…)"
  → GlobalRef/métodos nunca cacheados
  → jniStorageApiSupported() = false   (supported=0 manager=0)
  → jniOpenAllFilesSettings() = false  ("bridge não pronto")
  → jniExportLogsToDownloads() = false (copied=0)
```

O `vm=0x…` no log era o ponteiro da VM (correto) — o que faltava era o
**env do thread chamador**. O native tentava "descobrir a activity sozinho"
a partir do thread errado.

## 3. Correção (F5.3) — handshake INVERTIDO

A UI thread do Android está SEMPRE anexada à VM (é um thread Java). Quem
se registra é a **activity**:

```
VvActivity.onCreate()   → nativeRegisterActivity(this, "onCreate")   ─┐
VvActivity.onResume()   → nativeRegisterActivity(this, "onResume")   ─┤ reforço idempotente
                                                                        ▼
  native: GetJavaVM + NewGlobalRef(activity) + cache de métodos
        → "java: onCreate → nativeRegisterActivity"
        → "native: activity registada"
```

- O registro corre em `JNI_OnLoad`-garantido: o `super.onCreate` faz
  `System.loadLibrary` ANTES do corpo do `onCreate` da subclasse — os
  `RegisterNatives` já aconteceram.
- O android_main NÃO tenta mais descobrir a activity (chamada removida);
  reporta apenas o estado do handshake no boot (pendente é normal nessa
  altura — o `onResume` reforça).
- Qualquer thread da engine que chame Java faz `AttachCurrentThread`
  (nomeado) — nenhum `JNIEnv*` assumido não-nulo (JniAttach.h + teste).
- Mensagens honestas: se o handshake falhar, o toast/log diz
  **"ponte Java indisponível (handshake)"** — nunca "sistema sem All Files
  Access" (0.6.2 mentia sobre a causa).
- Fluxo All Files SÓ pós-handshake: diálogo →
  `ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION` →
  `isExternalStorageManager()` no retorno → File API direta.

## 4. Institucionalização no CI

- `verify-entry-symbols` ganha o gate **do manifest binário** (aapt2):
  `hasCode=true`, launchable-activity `vv.goni.VvActivity`,
  `lib_name=goni_vv` — a aferição da TAREFA 1 passa a correr em TODO build.
- Suíte do core ganha `test_handshake.cpp`: handshake simulado (o "stub
  Java" chama o export JNI diretamente contra um JNIEnv falso controlável),
  attach de threads, fluxo de permissão completo e regressão das mensagens
  (causa verdadeira vs sistema).
