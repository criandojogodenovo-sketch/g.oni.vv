#pragma once
// core/Bench.h — BENCHMARKS REAIS (FASE 0.9.6 · G6 · R-017).
//
// O CONTRATO (o que a spec exige e este ficheiro implementa):
//   * TODO o número do relatório é MEDIDO — nunca hardcodado. O que não
//     pôde ser medido imprime "não medido" (honestidade R-017);
//   * o bloco colável tem 9 LINHAS FIXAS (format() é a fonte única do
//     texto — o botão "Copiar relatório" copia EXATAMENTE isto);
//   * verts/draw calls saem da MESMA fonte da barra de estado (o main
//     injeta o total do frame — DrawStats somado no sítio de sempre);
//   * as amostras de FPS são os dt REAIS do loop (nenhum gerador).
//
// Camadas (ver ARCHITECTURE.md): este ficheiro é a parte PURA (dados +
// funções puras + leituras de /proc — testável no CI Linux); a máquina de
// fases (cenas/import/probe) vive em platform/main.cpp (coberta pelo
// c33_virtual, que inclui o main.cpp real do device).
//
// GL-free · Android-free: compila no host dos testes.
#include "core/Types.h"

#include <string>
#include <vector>

namespace vv::bench {

// ---- um valor medido (sabe se o foi — "não medido" é um estado) ----------
struct Measured {
    bool     ok = false;   // false → imprimir "não medido"
    double   value = 0.0;  // o NÚMERO MEDIDO (nunca um placeholder)
};

// ---- agregação de amostras de FPS (dt reais em segundos) ------------------
struct FpsAgg {
    bool   ok = false;
    double avg = 0.0;      // média das amostras
    double min = 0.0;      // pior amostra
    double p1 = 0.0;       // 1% low (percentil 1 das amostras de fps)
};

// média/mínimo/1%-low de amostras de FPS (1/dt por frame). Amostras com
// dt<=0 são ignoradas (o pause do runner não conta). n<10 → ok=false
// (agregação de menos de ~10 frames não representa nada).
FpsAgg aggregate(const std::vector<double>& fpsSamples);

// pico de RSS do processo em KB (VmHWM de /proc/self/status — o kernel
// mantém o máximo desde o arranque; 0 = não medível — não-Android/Linux)
u64 readPeakRssKb();

// ---- o relatório (a fonte única do bloco colável) -------------------------
struct Report {
    // identidade (buildinfo::g_version/g_versionCode no fill)
    char version[32] = "";
    u32  versionCode = 0;
    // device/Android/APK (JNI benchDeviceInfo — "" no host = não medido)
    char device[64] = "";
    int  sdk = 0;                  // 0 = não medido
    Measured apkMb;               // tamanho do APK instalado
    Measured apkShaOk;            // value=1 quando o sha256 chegou
    char   apkSha[20] = "";       // 12 hex + "…" (curto, o bloco é 1 linha)

    // arranques (marcas do lifecycle — ver LifecycleMarks abaixo)
    Measured warmStartMs;          // onResume → 1ª apresentação
    Measured coldStartMs;          // onCreate → 1ª apresentação

    // as duas cenas (amostras dt + verts/draw calls DO frame final)
    FpsAgg  def;                   // cena default (a que estava aberta)
    Measured defVerts, defDc;
    FpsAgg  scene;                 // cena bench (TICs fixos + mesh importado)
    Measured sceneVerts, sceneDc;

    Measured importMs;             // convert::importFile do GLB de referência
    Measured importScale;          // scale do nó lido no TIC instanciado

    Measured texMs;                // compressão 512×512 (mesma máquina do
                                   // pipeline de produção: ASTC→ETC2)
    char texFormat[24] = "";       // "" = não medido

    u32  audioOk = 0, audioTotal = 0;   // probe (0/0 = não medido)
    char audioBackend[16] = "";

    Measured rssPeakMb;            // VmHWM no fim do bench
    Measured projMb;               // tamanho do projeto no storage
};

// O BLOCO DE 9 LINHAS (template FIXO — R-017: aferível linha a linha):
//   Benchmarks · G.One VV <version> (<vc>) · <device> · Android <sdk>
//   warm start: <ms> ms · cold start: <ms> ms
//   cena default: <avg> fps (min <min>, 1% low <p1>) · <v> verts · <dc> draw calls
//   cena bench (mesh importado): <avg> fps (min <min>, 1% low <p1>) · <v> verts · <dc> draw calls
//   import glTF ref: <ms> ms (escala <s>)
//   texturas: <fmt> (<ms> ms)
//   áudio: <ok>/<total> ok · <backend>
//   memória: pico RSS <mb> MB
//   APK: <mb> MB · sha256 <sha> · projeto: <pmb> MB
// Valores não medidos imprimem "não medido" no sítio do número.
std::string format(const Report& r);

// formata UM valor medido ("%.1f") ou "não medido" (para compor linhas
// fora do bloco — o log do bench usa; teste afere o contrato)
std::string fmtOr(const Measured& m, const char* unitlessFmt);

// ---- GLB de referência (determinístico) ------------------------------------
// Um container GLB VÁLIDO com: grelha 16×16 (256 verts / 450 tris) com
// normais + uvs, e UM nó com scale 2.5 (a "escala" do relatório é o
// round-trip deste valor pelo importer → Transform3D do TIC instanciado).
// Zero aleatoriedade — dois builds quaisquer produzem bytes idênticos.
void makeReferenceGlb(std::vector<u8>& out);

// ---- marcas de arranque (o lifecycle mediu; o bench lê) --------------------
// StorageBridge::nativeRegisterActivity marca "onCreate"/"onResume"; a
// APRESENTAÇÃO marca-se a cada swap (markFirstFrame é barato e idempotente
// — registra a primeira do processo E a primeira após cada onResume). O
// bench reporta a última medição do processo (quem nunca saiu do
// foreground não tem warm — e o relatório DIZ "não medido", não inventa).
void markCreate();     // nativeRegisterActivity("onCreate")
void markResume();     // nativeRegisterActivity("onResume")
void markFirstFrame(); // CADA apresentação (swap) — a 1ª e a 1ª pós-resume
Measured latestColdMs();   // create → 1ª apresentação (desde o arranque)
Measured latestWarmMs();   // resume → 1ª apresentação após o último onResume

// reset das marcas (SÓ testes — o precedente resetForTest do VoniRegistry:
// a suíte partilha o processo e os testes de handshake/lifecycle chamam o
// nativeRegisterActivity com origens reais)
void resetMarksForTest();

} // namespace vv::bench
