#pragma once
// platform/AudioOut.h — o BACKEND de saída de áudio (0.8.11; hotfix 0.9.3).
//
// PROVA ANTES DE COMPROMISSO (a regra do prompt): a engine NÃO casa com o
// AAudio — casa com ESTA interface. 0.9.3 (REG-002/R-006): a CADEIA é
//   Oboe (primário — google/oboe 1.9.3, Apache-2.0; AAudio na API 27+
//         com fallback OpenSL ES automático nos devices problemáticos)
//   → AAudio direto (fallback fixado: dlopen + porta StartGate +
//         close-no-error-callback — AudioOutDevice.cpp)
//   → AudioTrack JNI (fallback final de sempre)
//   → SEM SOM (o editor funciona; nunca crasha por causa do áudio).
// O PROBE (Settings → Diagnóstico) corre contra um backend FRESCO e a
// decisão (shouldFallback) troca para AudioTrack debaixo da MESMA
// interface — o misturador (core/AudioEngine) não sabe qual corre.
//
// Contrato (o callback puxa do misturador):
//   start(rate, channels) → abre o stream e começa o callback;
//   stop() → para e fecha;
//   pause()/resume() → o lifecycle da activity (fundo/recente);
//   ready() → stream vivo (o auto-fallback pergunta).
//
// REG-002 (a regra de ouro do hotfix): start() é IDEMPOTENTE — chamado
// 2× NÃO abre um 2º stream (a fuga de stream vivo era a porta de entrada
// do SIGSEGV AAudio no Unisoc). Todos os backends cumprem via
// platform/AudioStartGate.h.
//
// HOST/CI: a implementação é o AudioOutStub (tests/stub) — os testes do
// misturador/probe correm SEM hardware. DEVICE: OboeBackend.cpp (oboe
// REAL via FetchContent) e AudioOutDevice.cpp (AAudio dlopen +
// AudioTrack JNI; só compilam no Android).
#include <functional>
#include <string>

#include "core/Types.h"

namespace vv::audioout {

// o callback de mistura: (buffer f32 interleaved, frames, canais, rate)
using MixFn = std::function<void(f32*, u32, u32, u32)>;

// interface pura (o main possui um unique_ptr e troca a implementação
// consoante o resultado do probe)
class Backend {
public:
    virtual ~Backend() = default;
    virtual bool start(u32 sampleRate, u16 channels) = 0;
    virtual void stop() = 0;
    virtual void pause() = 0;
    virtual void resume() = 0;
    virtual bool ready() const = 0;
    virtual const char* name() const = 0;   // "aaudio" | "audiotrack" | "stub"
    // telemetria do stream (o diagnóstico mostra)
    virtual u64 framesOut() const { return 0; }
    virtual u32 xruns() const { return 0; }
};

// instala o callback de mistura (o main liga-o ao AudioEngine no arranque;
// os backends o puxam no callback de áudio). DEFINIDO UMA SÓ VEZ em
// AudioOut.cpp (o TU comum a todas as plataformas) — os TUs Android
// LEEM-NO por currentMixFn() (nunca definem setMixFn; a 1ª versão o
// duplicava em AudioOutDevice.cpp → símbolo duplicado na ligação Android).
void setMixFn(MixFn fn);
// o callback instalado (AudioOutDevice.cpp lê no callback AAudio/na thread
// AudioTrack — VER o global, nunca o possuir)
MixFn currentMixFn();

// a JavaVM do glue (android_main tem app->activity->vm). void* para este
// header compilar no HOST sem <jni.h>; o TU do device guarda-a como JavaVM*
// e usa-a no attach da thread de escrita do AudioTrack. Host: no-op.
void setVm(void* vm);

// ---- fábrica (definida por plataforma; device em AudioOutDevice.cpp) ------
// cria o backend PEDIDO; o auto-fallback troca se o start falhar.
// 0.9.3 (REG-002): createOboe é o PRIMÁRIO — definido em OboeBackend.cpp
// (TU COMUM: oboe REAL no APK via FetchContent; stub tests/stub/oboe no
// host da suíte — o mesmo código vigiado pelo CI)
Backend* createOboe();
Backend* createAAudio();
Backend* createAudioTrack();

// ---- PROBE DE ESTABILIDADE (a exigência do prompt, ANTES do stack) --------
// O harness é PURO (corre no CI com um backend FAKE); no device corre com
// AAudio REAL. Ciclos: 50× start/stop, 10× pause/resume, playback longo
// (opcional, segundos configuráveis), e observa disconnects (no device o
// dono DESLIGA o headset a meio; o stream reporta erro).
struct ProbeResult {
    u32 cycles = 0;            // ciclos start/stop executados
    u32 failures = 0;          // ciclos que falharam (start/stop/erro)
    u32 pauseCycles = 0;
    u32 pauseFailures = 0;
    u32 disconnects = 0;       // stream caiu a meio (device: headset out)
    u32 xruns = 0;             // underruns somados
    bool longPlayOk = false;   // playback contínuo sem erro
    bool crashed = false;      // o backend morreu (auto-fallback)
    double totalMs = 0.0;
    // a DECISÃO: taxa de falha > 0 ou crash → fallback AudioTrack
    bool shouldFallback() const {
        return crashed || failures > 0 || pauseFailures > 0 || disconnects > 0;
    }
};

// corre o probe contra um backend dado (o chamador injecta AAudio real ou
// o fake dos testes). longPlaySecs=0 salta o playback longo.
// `forceFailure`/`forceDisconnect`: ganchos dos TESTES (o fake obedece).
ProbeResult runProbe(Backend* backend, u32 cycles, u32 pauseCycles,
                     u32 longPlaySecs);

// a tabela do probe (a linha que o engine.log e o RELATÓRIO mostram)
std::string probeTable(const ProbeResult& r, const char* backendName);

} // namespace vv::audioout
