#pragma once
// platform/CrashHandler.h — diagnóstico de crash (F1).
// Instala sigaction para SIGSEGV/SIGABRT e escreve goni_crash.log em
// <internalDataPath>/goni_crash.log. Verificação manual do dono.
namespace vv {

// internalDataPath vem de app->activity->internalDataPath (glue).
// Se for nulo/vazio, o handler não é instalado (não há onde escrever).
void installCrashHandler(const char* internalDataPath);

} // namespace vv
