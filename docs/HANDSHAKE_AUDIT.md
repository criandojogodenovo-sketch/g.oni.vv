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
  `lib_name=goni_vv` — a aferição da TAREFA 1 passa a correr em todo build.
- Suíte do core ganha `test_handshake.cpp`: handshake simulado (o "stub
  Java" chama o export JNI diretamente contra um JNIEnv falso controlável),
  attach de threads, fluxo de permissão completo e regressão das mensagens
  (causa verdadeira vs sistema).

## 5. Verificação (0.6.3)

- **Suíte do core**: `tests/test_handshake.cpp` — a ponte JNI INTEIRA
  (StorageBridge.cpp) compila na suíte com o fake JNI controlável
  (`tests/stub/jni.h`); o teste chama `nativeRegisterActivity` diretamente
  (o "stub Java" da TAREFA 5): registo onCreate, re-registo onResume
  (GlobalRefs substituídos), registo parcial (método crítico ausente),
  attach EDETACHED → `AttachCurrentThread('goni-engine')`, attach com
  falha (rc no log), fluxo de permissão completo (diálogo → intent 4301 →
  onActivityResult → fila → Granted + ação retomada), regressão das
  mensagens honestas (causa ponte ≠ sistema) e `JNI_OnLoad` com 2 nativos.
  254→264 testes.
- **CI**: `check_main.sh` (sintaxe) + `link_parity.sh` (ligação dos 68 TUs
  da app) verdes; gate NOVO `aapt2 dump xmltree` afere
  hasCode/VvActivity/lib_name em todo build; `verify-entry-symbols` exige
  também `Java_vv_goni_VvActivity_nativeRegisterActivity` no `.dynsym`.
- **Aceitação no C33** (log viewer mostra a sequência):
  `java: onCreate → nativeRegisterActivity` →
  `native: activity registada` →
  `storage: All Files Access — handshake=1 supported=1 manager=…`

## 6. F5.4 (0.6.4) — a 3ª camada: JNI_OnLoad NUNCA correu no device

**Evidência (RMX3624, Android 13, 0.6.3, logcat):**

```
java.lang.UnsatisfiedLinkError: No implementation found for void
vv.goni.VvActivity.nativeRegisterActivity(vv.goni.VvActivity, java.lang.String)
(tried Java_vv_goni_VvActivity_nativeRegisterActivity and Ja…)
    at vv.goni.VvActivity.onCreate(VvActivity.java:63)
    …
    at vv.goni.VvActivity.onResume(VvActivity.java:76)
```

A 0.6.3 corrigiu a DIREÇÃO do handshake (a activity regista-se no native) e
o ATTACH dos threads — mas o registo dos nativos dependia do `JNI_OnLoad`,
e o `JNI_OnLoad` nunca chegou a correr no device. Cadeia causal:

1. `android.app.NativeActivity` NÃO faz `System.loadLibrary` — o framework
   carrega a lib com `dlopen(RTLD_LAZY | RTLD_LOCAL)` DIRETO dentro de
   `loadNativeCode_native` (`android_app_NativeActivity.cpp`). O meta-data
   `android.app.lib_name` só diz AO framework QUE lib abrir.
2. Um `dlopen` cru não invoca `JNI_OnLoad` (isso é feito pelo caminho
   `Runtime.loadLibrary0 → nativeLoad` do ART, apenas em
   `System.loadLibrary`).
3. Logo o `RegisterNatives` do `JNI_OnLoad` nunca correu no device; e a
   busca por nome (`Java_vv_goni_VvActivity_nativeRegisterActivity`) não
   encontra a lib: ela não está no mapa de libraries do JVM (só
   `System.loadLibrary` lá a coloca) nem no escopo global (`RTLD_LOCAL`).
4. A declaração comentada na VvActivity ("super.onCreate() faz
   System.loadLibrary → JNI_OnLoad corre os RegisterNatives") era uma
   premissa FALSA — o hospedeiro testava o C++ diretamente contra o fake
   JNI e a RESOLUÇÃO (dlopen vs loadLibrary) não era modelada. Auditorias
   estáticas (manifest, dex, .dynsym) estavam todas certas e provam apenas
   potencial — não resolução.

**FIX (2 camadas + gate):**

- **1ª camada (primária)**: `static { System.loadLibrary("goni_vv"); }` no
  static initializer da `VvActivity` — corre na instanciação da classe
  (ANTES do onCreate), chama o `JNI_OnLoad` de verdade (RegisterNatives) e
  coloca a lib no mapa de resolução do JVM. O `dlopen` do framework depois
  reaproveita a MESMA lib (idempotente). Bónus: o `FindClass` do `JNI_OnLoad`
  passa a correr com o classloader da APP (quem chama o loadLibrary).
- **2ª camada (defesa)**: `ensureNativesRegistered()` idempotente no
  StorageBridge — o 1º `nativeRegisterActivity` re-tenta o
  `RegisterNatives` via `GetObjectClass(activity)` (não depende de
  FindClass nem de classloader). E o `JNI_OnLoad` ficou TOLERANTE:
  `FindClass`/`RegisterNatives` a falhar NÃO devolvem `JNI_ERR` (isso
  mataria o loadLibrary = app morta no arranque) — logam e adiam para a 2ª
  camada.
- **Gate (CI, pedido do dono)**: `scripts/jni_parity.py` afere em todo
  build que (a) todo `native` da VvActivity.java está na tabela
  RegisterNatives com a MESMA assinatura, (b) o static
  `System.loadLibrary` existe, e (c) no job de release, os símbolos
  `Java_vv_goni_VvActivity_*` estão exportados no `.dynsym` do .so real.
  **Na 1ª execução o gate apanhou um bug latente real**: a tabela registava
  `nativeOnActivityResult` com `(IIILandroid/net/Uri;I)V` (um `I` a mais —
  a Java declara `(int, int, Uri, int)`), o que teria feito o
  `RegisterNatives` falhar em runtime logo que o fix do loadLibrary
  ativasse o `JNI_OnLoad`. Corrigido para `(IILandroid/net/Uri;I)V`.

**Verificação (C33 E RMX3624)**: o logcat deixa de ter
`UnsatisfiedLinkError`; o engine.log (log viewer in-app) mostra a sequência
completa `jni: JNI_OnLoad — … registado(s)` → `java: onCreate →
nativeRegisterActivity` → `native: activity registada` → `jni: handshake
OK` → `storage: All Files Access — handshake=1 …`.
