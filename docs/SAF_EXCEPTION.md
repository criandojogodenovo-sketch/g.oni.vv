# Exceção à regra "zero Java" — SAF (F5.1-C)

## Porquê

A regra do projeto era `android:hasCode="false"` + `NativeActivity` pura.
O **Storage Access Framework** não funciona assim: `ACTION_OPEN_DOCUMENT_TREE`,
`ACTION_OPEN_DOCUMENT` e `ACTION_CREATE_DOCUMENT` devolvem o resultado por
**`Activity.onActivityResult`**, e o `NativeActivity` do framework não
reencaminha esse resultado para o código nativo. Não existe caminho 100%
nativo para escolher pastas do armazenamento (o objetivo da F5.1-C:
import/export de assets SEM PC).

## Superfície Java (mínima, ~2 classes)

| Ficheiro | Responsabilidade |
|----------|------------------|
| `app/src/main/java/vv/goni/VvActivity.java` | subclasse de `NativeActivity`; abre os pickers do sistema; `takePersistableUriPermission` no resultado; reencaminha `(request, result, uri, flags)` por JNI estático |
| `app/src/main/java/vv/goni/SafIo.java` | I/O via `DocumentsContract` (read/write/list/mkdir/exists/delete sobre a URI concedida) — chamado do nativo por JNI; **sem** dependência androidx (DocumentFile re-implementado com o framework) |

O C++ continua a não depender de Java no core: `core/SafStorage` fala com
uma interface `SafBackend` injetável (testada no CI com fake); a
implementação JNI (`platform/SafIoJni.cpp`) existe só na build Android.

## Consequências

- `android:hasCode="true"` no manifest (AGP volta a dexar as 2 classes).
- O AGP 8.5 já emitia um `classes.dex` mínimo; o impacto no APK é ~2-3 KB.
- Tema mono, 3 botões, landscape e o resto da app não mudam.

## F5.1-hotfix (0.6.1) — ampliação da exceção

`VvActivity.exportLogsToDownloads(relPath)` usa MediaStore (API 29+) para
copiar `getExternalFilesDir("logs")` → `Downloads/GOneVV/logs/` — chamado
por JNI do nativo (botão Settings → "Exportar logs"). Continua a MESMA
superfície Java mínima (sem permissões novas; ContentResolver é thread-safe
e não toca na UI). Detalhes: docs/AUDIT_SAF_JNI.md.
