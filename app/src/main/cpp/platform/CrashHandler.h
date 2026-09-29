#pragma once
// platform/CrashHandler.h — crash dump PERMANENTE da engine (F5.1-hotfix,
// parte 1.2). Substitui o handler mínimo da F1.
//
// Objetivo do dono: sem PC/logcat, um crash no C33 tem de se explicar sozinho.
// O handler captura SIGSEGV/SIGABRT/SIGBUS/SIGFPE, recolhe o stacktrace
// (_Unwind_Backtrace) e resolve nomes com dladdr() — escreve
// <logsDir>/crash-<timestamp>.dump num formato LEGÍVEL sem ndk-stack:
//
//   #03 pc 0x1a2b3c  libgoni_vv.so (vv::editor::drawInspector+0x88)
//
// Ativo em TODAS as builds (release incluída) — é a única forma de o dono
// diagnosticar sem PC. Depois do dump, re-raise com handler padrão (o
// sistema gera o tombstone normal para quem tiver adb).
//
// ATENÇÃO async-signal-safety: dentro do handler só open/write/dprintf/
// dladdr — zero malloc (sem std::string/vector, sem demangle).
namespace vv {
namespace crash {

// instala os 4 handlers + stack alternativo (sigaltstack — SIGSEGV por
// stack-overflow também produz dump). null/vazio → não instala.
void install(const char* logsDir);

// ---- núcleo testável no hospedeiro (CI Linux) -------------------------------

// recolhe os PCs do stack CORRENTE (até max). Devolve o nº de frames.
// No device corre dentro do handler; nos testes, num processo filho.
int captureFrames(void** pcs, int max);

// escreve o dump a partir de frames JÁ recolhidos (formato idêntico ao
// handler). Usado pelo handler no device e pelos testes no hospedeiro.
// faultAddr pode ser null. Devolve 0 em sucesso.
int writeDumpFromFrames(const char* path, const char* sigName, int sig,
                        void* faultAddr, void* const* pcs, int n);

// nome do sinal (SIGSEGV/…) — exposto para os testes
const char* signalName(int sig);

} // namespace crash
} // namespace vv
