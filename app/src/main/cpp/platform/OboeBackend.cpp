// platform/OboeBackend.cpp — OBOE, o CAMINHO PRIMÁRIO do áudio (hotfix
// 0.9.3, REG-002/R-006).
//
// PORQUE OBOE (a decisão da Tarefa 2, com evidência): o AAudio cru tem
// bugs de vendor no Unisoc/Spreadtrum do realme RMX3624 — o SIGSEGV
// (SEGV_ACCERR) dentro de AAudio_createStreamBuilder dos tombstones
// 00-17/21-31 da app antiga (com.goni.runtime) acontecia no
// EditorActivity.onResume → startAudio(). O Oboe é a biblioteca oficial
// do Google para áudio de baixa latência: usa AAudio na API 27+ (com
// recuperação de erro), usa OpenSL ES na API <=26 e em devices
// problemáticos (fallback automático — a razão de ser da biblioteca),
// e devolve Result verificado em TODAS as chamadas. LICENÇA Apache 2.0
// (documentada no RELATORIO-0.9.3 e no cabeçalho do CMake da app).
// VERSÃO PINADA: 1.9.3 (>=1.8.0 exigido; ver FetchContent no CMake).
//
// AS REGRAS DO FIX (todas afervadas pelo sentinela regress_audio_lifecycle
// e pelo replay FASE 8 do c33_virtual):
//   1. StartGate atómico — start() chamado 2× (o padrão do tombstone:
//      onResume repetido) NÃO abre um 2º stream: o 2º chamador recebe
//      log + true (idempotência) e o hardware fica intocado;
//   2. TODOS os oboe::Result são verificados e logados (openStream,
//      requestStart, requestPause, requestStop, close) — nunca assumidos;
//   3. o callback de ERRO segue o contrato do oboe: onErrorBeforeClose
//      só LOGA; onErrorAfterClose larga a referência (o oboe JÁ fechou),
//      marca erro e abre o portão (um start futuro pode tentar);
//   4. falha de arranque = editor SEM SOM (return false — o main decide
//      o fallback AAudio/AudioTrack), NUNCA crash;
//   5. onAudioReady: SEM locks, SEM alocação — só puxa o misturador
//      (o padrão do AudioOut desde 0.8.11; a thread é de tempo real).
//
// TU COMUM (o padrão StorageBridge/tests-stub-jni.h): no APK compila
// contra o oboe REAL (FetchContent, alvo `oboe`); na suíte do host
// compila contra tests/stub/oboe/Oboe.h (tests/stub vem primeiro no
// include path) — o MESMO código de produção vigiado pelo CI.
#include "platform/AudioOut.h"
#include "platform/AudioStartGate.h"
#include "platform/EngineLog.h"

#include <oboe/Oboe.h>

#include <cstring>
#include <memory>
#include <mutex>

#include "core/Types.h"

namespace vv::audioout {
namespace {

class OboeBackend final : public Backend, public oboe::AudioStreamCallback {
public:
    ~OboeBackend() override { stop(); }

    // ---- Backend ----------------------------------------------------------
    bool start(u32 sampleRate, u16 channels) override {
        // REG-002: NUNCA arranques duplos — o 2º chamador NÃO toca no
        // hardware (a fuga de stream vivo era a porta do SIGSEGV Unisoc)
        if (!gate_.tryEnter()) {
            elog::warn("audio(oboe): start ignorado — stream ja ativo "
                       "(porta R-006)");
            return true;   // idempotente: o stream que toca continua
        }
        oboe::AudioStreamBuilder b;
        b.setDirection(oboe::Direction::Output)
            ->setPerformanceMode(oboe::PerformanceMode::LowLatency)
            ->setSharingMode(oboe::SharingMode::Exclusive)
            ->setFormat(oboe::AudioFormat::Float)
            ->setChannelCount(static_cast<int>(channels))
            ->setSampleRate(static_cast<int32_t>(sampleRate))
            ->setCallback(this);
        std::shared_ptr<oboe::AudioStream> s;
        const oboe::Result ro = b.openStream(s);
        if (ro != oboe::Result::OK || !s) {
            elog::error("audio(oboe): openStream FALHOU (%s) — sem som, "
                        "o editor segue", oboe::convertToText(ro));
            gate_.release();
            return false;
        }
        const oboe::Result rs = s->requestStart();
        if (rs != oboe::Result::OK) {
            elog::error("audio(oboe): requestStart FALHOU (%s) — sem som",
                        oboe::convertToText(rs));
            const oboe::Result rc = s->close();
            if (rc != oboe::Result::OK) {
                elog::warn("audio(oboe): close pos-falha (%s)",
                           oboe::convertToText(rc));
            }
            gate_.release();
            return false;
        }
        {
            std::lock_guard<std::mutex> lk(mtx_);
            stream_ = std::move(s);
            errored_ = false;
        }
        // log informativo OBRIGATÓRIO (Tarefa 2.5): o que o Oboe ESCOLHEU
        // (rate/ch/perf do stream REAL — o device manda)
        const std::shared_ptr<oboe::AudioStream> snap = streamSnapshot();
        elog::info("audio(oboe): stream ATIVO rate=%d ch=%d burst=%d "
                   "perf=%d share=%d api=%d",
                   snap->getSampleRate(), snap->getChannelCount(),
                   snap->getFramesPerBurst(),
                   static_cast<int>(snap->getPerformanceMode()),
                   static_cast<int>(snap->getSharingMode()),
                   static_cast<int>(snap->getAudioApi()));
        return true;
    }

    void stop() override {
        std::shared_ptr<oboe::AudioStream> s;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            s = std::move(stream_);
        }
        if (s) {
            const oboe::Result rst = s->requestStop();
            if (rst != oboe::Result::OK) {
                elog::warn("audio(oboe): requestStop (%s)",
                           oboe::convertToText(rst));
            }
            const oboe::Result rcl = s->close();
            if (rcl != oboe::Result::OK) {
                elog::warn("audio(oboe): close (%s)",
                           oboe::convertToText(rcl));
            }
        }
        gate_.open();   // a paragem liberta o portão (próximo start pode)
    }

    void pause() override {
        const std::shared_ptr<oboe::AudioStream> s = streamSnapshot();
        if (!s) {
            return;
        }
        const oboe::Result r = s->requestPause();
        if (r != oboe::Result::OK) {
            elog::warn("audio(oboe): requestPause (%s)",
                       oboe::convertToText(r));
        }
    }

    void resume() override {
        const std::shared_ptr<oboe::AudioStream> s = streamSnapshot();
        if (!s) {
            return;   // sem stream (erro anterior/boot falhado) — silêncio
        }
        const oboe::StreamState st = s->getState();
        if (st == oboe::StreamState::Started ||
            st == oboe::StreamState::Starting) {
            return;   // JÁ está a tocar — resume é idempotente
        }
        const oboe::Result r = s->requestStart();
        if (r != oboe::Result::OK) {
            elog::warn("audio(oboe): resume requestStart (%s) — stream "
                       "morto? (o probe decide a troca)",
                       oboe::convertToText(r));
        }
    }

    bool ready() const override {
        if (errored_.load()) {
            return false;
        }
        const std::shared_ptr<oboe::AudioStream> s = streamSnapshot();
        if (!s) {
            return false;
        }
        const oboe::StreamState st = s->getState();
        return st == oboe::StreamState::Started ||
               st == oboe::StreamState::Starting;
    }

    const char* name() const override { return "oboe"; }

    u64 framesOut() const override {
        const std::shared_ptr<oboe::AudioStream> s = streamSnapshot();
        return s ? static_cast<u64>(s->getFramesRead()) : 0ull;
    }
    u32 xruns() const override {
        const std::shared_ptr<oboe::AudioStream> s = streamSnapshot();
        if (!s) {
            return 0u;
        }
        // ATENÇÃO API REAL (verificada nos headers 1.9.3): getXRunCount()
        // devolve ResultWithValue<int32_t> com acessor .value() — não
        // compila doutra forma (duas passadas do CI apanharam: o int32_t
        // plano e o .result() inexistente)
        const oboe::ResultWithValue<int32_t> r = s->getXRunCount();
        if (r.error() != oboe::Result::OK) {
            return 0u;
        }
        return r.value() > 0 ? static_cast<u32>(r.value()) : 0u;
    }

    // ---- oboe::AudioStreamCallback ----------------------------------------
    oboe::DataCallbackResult onAudioReady(oboe::AudioStream* os,
                                           void* audioData,
                                           int32_t numFrames) override {
        // a thread de áudio do sistema: SEM locks, SEM alocação — puxa o
        // misturador puro (o mesmo contrato do dataCb do AAudio 0.8.11)
        const int32_t ch = os->getChannelCount() > 0 ? os->getChannelCount() : 2;
        const int32_t sr = os->getSampleRate() > 0 ? os->getSampleRate() : 44100;
        const u32 frames = numFrames > 0 ? static_cast<u32>(numFrames) : 0u;
        if (MixFn mix = currentMixFn()) {
            mix(static_cast<f32*>(audioData), frames, static_cast<u32>(ch),
                static_cast<u32>(sr));
        } else if (audioData && frames > 0) {
            std::memset(audioData, 0,
                        sizeof(f32) * static_cast<size_t>(frames) *
                            static_cast<size_t>(ch));
        }
        return oboe::DataCallbackResult::Continue;
    }

    void onErrorBeforeClose(oboe::AudioStream* /*os*/,
                            oboe::Result error) override {
        // SÓ LOGAR (o contrato do oboe: aqui o stream ainda vive; nunca
        // chamar métodos que bloqueiem — o oboe fecha-o a seguir)
        elog::error("audio(oboe): erro no stream ANTES do close (%s)",
                    oboe::convertToText(error));
    }

    void onErrorAfterClose(oboe::AudioStream* /*os*/,
                           oboe::Result error) override {
        // o oboe JÁ fechou o stream: largamos a referência, marcamos o
        // erro (o ready() passa a falso — o probe/main vêem) e ABRIMOS o
        // portão (um start futuro pode tentar de novo)
        elog::error("audio(oboe): stream MORREU (%s) — sem som ate novo "
                    "start (o editor segue)", oboe::convertToText(error));
        {
            std::lock_guard<std::mutex> lk(mtx_);
            stream_.reset();
        }
        errored_ = true;
        gate_.open();
    }

private:
    std::shared_ptr<oboe::AudioStream> streamSnapshot() const {
        std::lock_guard<std::mutex> lk(mtx_);
        return stream_;
    }

    mutable std::mutex mtx_;                    // guarda stream_ (errCb × stop)
    std::shared_ptr<oboe::AudioStream> stream_; // protegido por mtx_
    StartGate gate_;                            // REG-002: nunca 2×
    std::atomic<bool> errored_{false};
};

} // namespace

// a fábrica PRIMÁRIA (o main tenta Oboe → AAudio → AudioTrack → sem som).
// Definida AQUI (TU comum): no device devolve o backend de oboe REAL; no
// host da suíte, o MESMO código contra o stub — o c33_virtual replica o
// lifecycle com ESTE backend.
Backend* createOboe() { return new OboeBackend(); }

} // namespace vv::audioout
