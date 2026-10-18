#pragma once
// platform/BuildInfo.h — IDENTIDADE DA BUILD (0.8.10).
//
// O dono instalava APKs de várias versões no mesmo device e os crash dumps
// eram anónimos — não dava para saber QUE build os produziu sem adivinhar.
// Agora a identidade vem da JNI (VvActivity lê BuildConfig + o
// assets/build_info.txt que o CI escreve: git/epoch/sha256 da .so) e vive
// em TODOS os crash dumps (header + nome do ficheiro) e no BANNER do boot
// log. No host/CI os defaults marcam "dev" (os testes aferem o formato).
//
// Thread-safety: set() corre UMA vez no arranque (thread da UI, antes do
// android_main); os leitores (crash handler/banner/viewer) correm depois.
#include <cstdint>
#include <string>

#include "core/Types.h"

namespace vv::buildinfo {

extern char g_version[32];    // "0.8.10" (VERSION_NAME do BuildConfig)
extern u32  g_versionCode;    // 40 (0 = host/dev — sem APK)
extern char g_git[17];        // sha curto do commit ("" no dev)
extern char g_soSha[72];      // sha256 da libgoni_vv.so (CI em 2 passes)
extern u64  g_epoch;          // epoch da build (0 = dev)

// chamado pela JNI (nativeSetBuildInfo) no onCreate da VvActivity
void set(const char* version, u32 code, const char* git, const char* soSha,
         u64 epoch);

// a linha de BANNER do boot log (0.8.12): "boot: G.One VV <versão>
// versionCode <N> sha256 <sha256-da-.so> git <short>" — a prova de
// identidade que o log viewer mostra no arranque
std::string banner();

// badge do log viewer: true se o dump veio de OUTRA build (nome
// crash-<unix>-vc<N>.dump com N != g_versionCode, ou sem -vc = pré-0.8.10)
bool dumpIsFromOtherBuild(const std::string& dumpName);

// 0.8.12 — badge de TEXTO do viewer p/ UM dump: "" (mesma build),
// "  [ANTIGO (build N)]" (vc do nome != instalado) ou
// "  [ANTIGO (pre-0.8.10)]" (dump sem identidade no nome)
std::string dumpBadge(const std::string& dumpName);

// sufixo do nome do dump com a identidade: "-vc40" ("" se code==0)
std::string dumpSuffix();

} // namespace vv::buildinfo
