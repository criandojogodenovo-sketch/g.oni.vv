#pragma once
// platform/StorageBridge.h — ponte nativo ↔ Java do ARMAZENAMENTO (F5.2,
// device-only). Sucessora da ponte SAF (F5.1-C) com o fluxo All Files Access:
//
//   HANDSHAKE INVERTIDO (F5.3 — docs/HANDSHAKE_AUDIT.md): é a VvActivity
//   (thread da UI, SEMPRE anexado à VM) que se registra no nativo —
//   onCreate → nativeRegisterActivity(this,"onCreate"), onResume reforça.
//   O native NUNCA mais tenta descobrir a activity sozinho (o thread do
//   glue não está anexado: GetEnv = JNI_EDETACHED — a causa única de
//   todas as pontes mortas 0.6.0→0.6.2).
//
//   1. handshakeOk()             → registo Java concluído (vm + GlobalRef
//                                  + métodos da activity cacheados);
//   2. jniStorageApiSupported()  → Environment.isExternalStorageManager()
//                                  (API 30+; API < 30 = sem suporte);
//   3. jniOpenAllFilesSettings() → VvActivity.openAllFilesSettings(kReqAllFiles)
//                                  (ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION);
//   4. o retorno chega por Java_vv_goni_VvActivity_nativeOnActivityResult →
//      fila PendingResult → pollResult() no THREAD DA ENGINE (nunca na UI —
//      regra do hotfix F5.1 mantida) → PermFlow::onSettingsReturn(granted).
//
// O lado C++ é chamado do loop principal; o I/O de ficheiros em si é POSIX
// direto (platform/FileApi.cpp) — a Java só abre a janela de permissões e
// reencaminha o resultado, mais o export de logs por MediaStore (1.4).
// F5.4 (Gestor de Projetos): a ponte ganhou TAMBÉM os métodos SAF da
// VvActivity (bridgeRootDoc/bridgeOpenFd/bridgeList/bridgeCreate/
// bridgeDelete) — o I/O do projeto sobre a pasta escolhida no gestor
// passa por AQUI (core/SafStorage → SafIo → JniSafIo → Java). O SAF dos
// projetos COEXISTE com o All Files Access (import/export de assets).
//
// Este TU só entra na build ANDROID (usa JNI; a suíte core compila-o com
// o stub jni.h — tests/stub/jni.h — e o check_main.sh valida a sintaxe).
#include "platform/Saf.h"            // SafResult/PendingResult/ResultHandler
#include "platform/SafIo.h"          // SafIo/SafEntry (interface do I/O SAF)
#include "platform/ProjectSlot.h"    // fila do projeto (Activity→boot)
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

// F5.4 — fila do projeto escolhido no Gestor de Projetos: a VvActivity
// empurra em nativeOpenProject; o android_main espera (timeout) antes de
// montar o storage. O MESMO objeto serve o device e os testes.
ProjectSlot& projectSlot();

// F5.4 — implementação SafIo sobre a ponte JNI (métodos bridge da
// VvActivity). Singleton interno; sem handshake as operações falham com
// err = "ponte Java indisponível (handshake)".
SafIo* jniSafIo();

// F5.3 — true quando o handshake invertido concluiu: a VvActivity registou-
// se (nativeRegisterActivity), a VM foi guardada e o método crítico do
// fluxo All Files (openAllFilesSettings) foi encontrado. false = ponte
// Java indisponível (handshake) — as mensagens têm de reportar ESTA causa.
bool handshakeOk();

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

// F5.4 — hook da SUÍTE (nunca em produção): repõe o guard do registo
// idempotente dos nativos para um caso de teste re-exercitar o
// RegisterNatives (a função vive no StorageBridge.cpp).
void resetNativesRegistrationForTest();

} // namespace vv::storage
