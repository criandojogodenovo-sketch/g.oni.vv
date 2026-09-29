#pragma once
// platform/StorageBridge.h — ponte nativo ↔ Java do ARMAZENAMENTO (F5.2,
// device-only). Sucessora da ponte SAF (F5.1-C) com o fluxo All Files Access:
//
//   1. jniStorageApiSupported()   → Environment.isExternalStorageManager()
//                                   (API 30+; API < 30 = sem suporte);
//   2. jniOpenAllFilesSettings()  → VvActivity.openAllFilesSettings(kReqAllFiles)
//                                   (ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
//   3. o retorno chega por Java_vv_goni_VvActivity_nativeOnActivityResult →
//      fila PendingResult → pollResult() no THREAD DA ENGINE (nunca na UI —
//      regra do hotfix F5.1 mantida) → PermFlow::onSettingsReturn(granted).
//
// O lado C++ é chamado do loop principal; o I/O de ficheiros em si é POSIX
// direto (platform/FileApi.cpp) — a Java só abre a janela de permissões e
// reencaminha o resultado, mais o export de logs por MediaStore (1.4).
//
// Este TU só entra na build ANDROID (usa JNI; o CI compila a SUÍTE core sem
// ele e o check_main.sh valida a sintaxe com o stub jni.h).
#include "platform/Saf.h"            // SafResult/PendingResult/ResultHandler
#include "platform/StoragePerm.h"    // kReqAllFiles (mesmo valor no Java)

namespace vv::storage {

// handler dos resultados — o MESMO tipo da fila (vv::saf::ResultHandler);
// injetado por main.cpp ANTES de abrir a janela
using saf::ResultHandler;

void setHandler(ResultHandler fn, void* user);

// consome UM resultado diferido (fila UI → engine) e dispara o handler NO
// THREAD CHAMADOR. O loop da engine chama por frame — o processamento
// (storage/GL) nunca corre no thread da UI. false = nada pendente.
bool pollResult();

// cache de env/classe/métodos — chamar 1× no android_main com o
// app->activity (o thread do NativeActivity já está anexado à VM)
void initJava(void* vm, void* activityObject);

// 1) VERIFICAÇÃO da permissão (Environment.isExternalStorageManager()).
//    false em *supported = API < 30 (o método não existe) → fluxo fallback.
bool jniStorageApiSupported(bool* outManager);

// 2) ABERTURA da janela de permissões do sistema para a app (o resultado
//    volta por onActivityResult(kReqAllFiles) → fila → pollResult).
bool jniOpenAllFilesSettings();

// F5.1-hotfix (parte 1.4, mantida): export dos logs para Downloads/GOneVV/logs
// (MediaStore, lado Java). true = chamada Java executada; *outCount =
// ficheiros copiados (negativo = falha Java: -1 excepção, -2 API<29,
// -3 sem ficheiros).
bool jniExportLogsToDownloads(int* outCount);

} // namespace vv::storage
