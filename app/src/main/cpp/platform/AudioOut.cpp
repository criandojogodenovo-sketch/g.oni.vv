// platform/AudioOut.cpp — PROBE harness + fábrica STUB (0.8.11).
//
// O harness do probe é PURO: só fala com a INTERFACE Backend — os mesmos
// counters correm no CI (fake) e no device (AAudio real). A DECISÃO de
// fallback é shouldFallback(): QUALQUER falha/crash/disconnect ativa o
// AudioTrack (a regra do dono: "não apostamos a engine num backend não
// provado" — e o fallback fica DOCUMENTADO).
//
// DEVICE: AAudioOut.cpp define createAAudio() REAL; AudioTrackOut.cpp o
// fallback JNI. Este TU define os STUBS de host (nunca ligados no device
// — as fábricas reais sobrepõem-se por linker... NÃO: um símbolo por
// build. No host, os STUBS; no device, os reais vivem noutros TUs e ESTES
// não compilam (ver CMake: AudioOut.cpp SEMPRE; AudioOutDevice.cpp só
// Android — os createAAudio/createAudioTrack stub daqui ficam no host).
#include "platform/AudioOut.h"

#include <chrono>
#include <cstdio>
#include <thread>

namespace vv::audioout {

// host: o callback fica num global do TU — ÚNICO dono do símbolo em TODAS
// as plataformas (o device LÊ por currentMixFn() de AudioOutDevice.cpp)
static MixFn g_mixHost;

void setMixFn(MixFn fn) {
    g_mixHost = std::move(fn);
}

MixFn currentMixFn() {
    return g_mixHost;
}

ProbeResult runProbe(Backend* backend, u32 cycles, u32 pauseCycles,
                     u32 longPlaySecs) {
    ProbeResult r;
    if (!backend) {
        r.crashed = true;
        return r;
    }
    const auto t0 = std::chrono::steady_clock::now();
    // ---- fase 1: ciclos start/stop (o ciclo de vida duro) ---------------
    for (u32 i = 0; i < cycles; ++i) {
        if (!backend->start(44100, 2)) {
            ++r.failures;
            continue;
        }
        // um respiro de reprodução (o callback corre de verdade no device)
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        r.xruns += backend->xruns();
        if (!backend->ready()) {
            ++r.disconnects;   // caiu a meio (headset desligado no device)
        }
        backend->stop();
        ++r.cycles;
    }
    // ---- fase 2: pause/resume do lifecycle (fundo/recente) ---------------
    if (backend->start(44100, 2)) {
        for (u32 i = 0; i < pauseCycles; ++i) {
            backend->pause();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            backend->resume();
            if (!backend->ready()) {
                ++r.pauseFailures;
            }
            ++r.pauseCycles;
        }
        // ---- fase 3: playback longo (opcional) --------------------------
        if (longPlaySecs > 0) {
            const auto tLong = std::chrono::steady_clock::now();
            while (std::chrono::duration<double>(
                       std::chrono::steady_clock::now() - tLong)
                       .count() < static_cast<double>(longPlaySecs)) {
                if (!backend->ready()) {
                    ++r.disconnects;
                    break;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            r.longPlayOk = backend->ready();
        }
        backend->stop();
    } else {
        ++r.failures;
    }
    r.totalMs = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - t0)
                    .count();
    return r;
}

std::string probeTable(const ProbeResult& r, const char* backendName) {
    char buf[320];
    std::snprintf(buf, sizeof(buf),
                  "audio: probe %s — ciclos %u/%u ok, falhas %u | "
                  "pause/resume %u/%u ok, falhas %u | disconnects %u | "
                  "xruns %u | longo %s | %s (%.0f ms)",
                  backendName, r.cycles - (r.failures > r.cycles ? r.cycles : r.failures),
                  r.cycles, r.failures, r.pauseCycles, r.pauseCycles,
                  r.pauseFailures, r.disconnects, r.xruns,
                  r.longPlayOk ? "ok" : "n/a",
                  r.shouldFallback() ? "DECISAO: FALLBACK AudioTrack"
                                     : "DECISAO: AAudio OK",
                  r.totalMs);
    return std::string(buf);
}

// ---- fábricas STUB (HOST/CI APENAS; o device NÃO as compila — o
// AudioOutDevice.cpp define as REAIS; a 1ª versão definia AMBAS em todos
// os builds → símbolo duplicado na ligação Android) --------------------------
#ifndef __ANDROID__

namespace {
class StubBackend final : public Backend {
public:
    bool start(u32, u16) override { ++starts; running = true; return true; }
    void stop() override { running = false; }
    void pause() override { paused = true; }
    void resume() override { paused = false; }
    bool ready() const override { return running; }
    const char* name() const override { return "stub"; }
    u32 starts = 0;
    bool running = false;
    bool paused = false;
};
} // namespace

Backend* createAAudio() {
    return new StubBackend();   // host: nunca usado em produção
}
Backend* createAudioTrack() {
    return new StubBackend();
}

void setVm(void* /*vm*/) {}   // host: sem JavaVM

#endif // !__ANDROID__

} // namespace vv::audioout
