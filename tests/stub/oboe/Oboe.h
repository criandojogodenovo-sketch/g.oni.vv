// tests/stub/oboe/Oboe.h — FAKE da API pública do Oboe (google/oboe,
// Apache-2.0, versão PINADA 1.9.3 no APK via CMake FetchContent).
//
// PORQUE EXISTE: o MESMO TU de produção (platform/OboeBackend.cpp) tem de
// compilar e ser TESTADO no host do CI sem o NDK — o padrão da casa do
// tests/stub/jni.h (StorageBridge): no APK compila contra o oboe REAL
// (include dir do alvo FetchContent), na suíte compila contra ESTE stub
// (tests/stub vem PRIMEIRO no include path dos alvos de teste).
//
// O QUE ESPELHA: a API pública estável usada pelo OboeBackend —
// AudioStreamBuilder (setters encadeáveis + openStream(shared_ptr&)),
// AudioStream (requestStart/requestPause/requestStop/close/getters),
// AudioStreamCallback (onAudioReady/onErrorBeforeClose/onErrorAfterClose),
// Result + convertToText(Result). Os NOMES e ASSINATURAS são os do oboe
// real; os VALORES do enum Result no stub não precisam de coincidir (o
// backend nunca depende do inteiro, só de Result::OK e do texto).
//
// GANCHOS DE TESTE (só existem NO STUB — a build real nunca os vê):
//   oboe::testing::hooks()        — contadores + injeção de falhas;
//   oboe::testing::reset()        — limpa tudo entre casos;
//   oboe::testing::fireErrorOnAllStreams(Result) — dispara a sequência
//     REAL de erro do oboe (onErrorBeforeClose → close interno →
//     onErrorAfterClose) em TODOS os streams vivos, sincronamente (nos
//     testes que precisam da thread real, o próprio teste envolve numa
//     std::thread).
#pragma once

#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace oboe {

// ---- Result (nomes da API pública; ver nota do cabeçalho) ----------------
enum class Result : int32_t {
    OK = 0,
    ErrorDisconnected = -899,
    ErrorClosed = -898,
    ErrorIllegalState = -897,
    ErrorInvalidHandle = -896,
    ErrorUnimplemented = -895,
    ErrorNoFreeHandles = -894,
    ErrorNoMemory = -893,
    ErrorTimeout = -892,
    ErrorInvalidFormat = -891,
    ErrorInvalidRate = -890,
    ErrorInvalidState = -889,
    ErrorWouldBlock = -888,
    ErrorUnavailable = -887,
};

inline const char* convertToText(Result r) {
    switch (r) {
        case Result::OK: return "OK";
        case Result::ErrorDisconnected: return "ErrorDisconnected";
        case Result::ErrorClosed: return "ErrorClosed";
        case Result::ErrorIllegalState: return "ErrorIllegalState";
        case Result::ErrorInvalidHandle: return "ErrorInvalidHandle";
        case Result::ErrorUnimplemented: return "ErrorUnimplemented";
        case Result::ErrorNoFreeHandles: return "ErrorNoFreeHandles";
        case Result::ErrorNoMemory: return "ErrorNoMemory";
        case Result::ErrorTimeout: return "ErrorTimeout";
        case Result::ErrorInvalidFormat: return "ErrorInvalidFormat";
        case Result::ErrorInvalidRate: return "ErrorInvalidRate";
        case Result::ErrorInvalidState: return "ErrorInvalidState";
        case Result::ErrorWouldBlock: return "ErrorWouldBlock";
        case Result::ErrorUnavailable: return "ErrorUnavailable";
    }
    return "Unknown";
}

enum class Direction : int32_t { Output, Input };
enum class SharingMode : int32_t { Exclusive, Shared };
enum class PerformanceMode : int32_t { None, PowerSaving, LowLatency };
enum class AudioFormat : int32_t {
    Invalid = -1,
    Unspecified = 0,
    I16 = 1,
    Float = 2,
    I24 = 3,
    I32 = 4,
};
enum class Usage : int32_t {
    Unspecified = 0,
    Media = 1,
    Game = 2,
};
enum class AudioApi : int32_t { Unspecified = 0, OpenSLES = 1, AAudio = 2 };
enum class StreamState : int32_t {
    Uninitialized = 0,
    Open,
    Starting,
    Started,
    Pausing,
    Paused,
    Flushing,
    Flushed,
    Stopping,
    Stopped,
    Closing,
    Closed,
    Disconnected,
};
enum class DataCallbackResult : int32_t { Continue = 0, Stop = 1 };

// ResultWithValue<T> — existe na API REAL e o getXRunCount() a devolve.
// acessores VERIFICADOS nos headers do oboe 1.9.3 (include/oboe/
// ResultWithValue.h): .value(), .error(), .isOk(), operator bool — o
// .result() NÃO existe (duas passadas do build NDK apanharam as
// divergências stub↔real; agora o stub espelha a API ao detalhe)
template <typename T>
class ResultWithValue {
public:
    explicit ResultWithValue(T value) : value_(value), error_(Result::OK) {}
    ResultWithValue(Result error) : value_{}, error_(error) {}
    T value() const { return value_; }
    Result error() const { return error_; }
    bool isOk() const { return error_ == Result::OK; }
    explicit operator bool() const { return isOk(); }

private:
    T value_;
    Result error_;
};

class AudioStream;

// ---- o registo de streams vivos (interna do stub) --------------------------
namespace detail {
inline std::vector<AudioStream*>& liveStreams() {
    static std::vector<AudioStream*> v;
    return v;
}
} // namespace detail

// ---- o callback (a interface real do oboe) -------------------------------
class AudioStreamCallback {
public:
    virtual ~AudioStreamCallback() = default;
    virtual DataCallbackResult onAudioReady(AudioStream* stream,
                                            void* audioData,
                                            int32_t numFrames) = 0;
    virtual void onErrorBeforeClose(AudioStream* stream, Result error) {}
    virtual void onErrorAfterClose(AudioStream* stream, Result error) {}
};

// ---- ganchos de teste (SÓ no stub) ----------------------------------------
namespace testing {

struct Hooks {
    // próxima chamada a openStream devolve isto em vez de OK (1×)
    Result nextOpenResult = Result::OK;
    // próximo requestStart devolve isto (1×)
    Result nextStartResult = Result::OK;
    // telemetria (os sentinelas afervam o NÃO-arranque-duplo)
    int openCount = 0;      // streams criados pelo builder
    int startCount = 0;     // requestStart que devolveram OK
    int pauseCount = 0;
    int stopCount = 0;
    int closeCount = 0;     // closes (do backend OU da sequência de erro)
    int errorCallbacks = 0; // pares onError antes/depois disparados
};
inline Hooks& hooks() {
    static Hooks h;
    return h;
}
inline void reset() {
    hooks() = Hooks{};
    detail::liveStreams().clear();   // cada teste começa sem streams vivos
}
// dispara a sequência REAL de erro do oboe em TODOS os streams vivos
// (chamada de DENTRO dos testes; o backend não sabe que existe)
void fireErrorOnAllStreams(Result error);

} // namespace testing

// ---- o registo de streams vivos aparece acima (detail) --------------------

// ---- AudioStream (fake com o contrato real) -------------------------------
class AudioStream {
public:
    AudioStream(AudioStreamCallback* cb, int32_t rate, int32_t ch,
                PerformanceMode perf, SharingMode share, AudioFormat fmt,
                Usage usage)
        : callback_(cb), rate_(rate), ch_(ch), perf_(perf), share_(share),
          fmt_(fmt), usage_(usage) {}

    Result requestStart() {
        if (closed_) {
            return Result::ErrorClosed;
        }
        const Result forced = testing::hooks().nextStartResult;
        if (forced != Result::OK) {
            testing::hooks().nextStartResult = Result::OK;
            state_ = StreamState::Open;
            return forced;
        }
        state_ = StreamState::Started;
        ++testing::hooks().startCount;
        return Result::OK;
    }
    Result requestPause() {
        if (closed_) {
            return Result::ErrorClosed;
        }
        state_ = StreamState::Paused;
        ++testing::hooks().pauseCount;
        return Result::OK;
    }
    Result requestStop() {
        if (closed_) {
            return Result::ErrorClosed;
        }
        state_ = StreamState::Stopped;
        ++testing::hooks().stopCount;
        return Result::OK;
    }
    Result close() {
        if (closed_) {
            return Result::ErrorClosed;   // o oboe real recusa duplo close
        }
        closed_ = true;
        state_ = StreamState::Closed;
        ++testing::hooks().closeCount;
        unregisterStream(this);
        return Result::OK;
    }

    StreamState getState() const { return state_; }
    int32_t getSampleRate() const { return rate_ > 0 ? rate_ : 48000; }
    int32_t getChannelCount() const { return ch_ > 0 ? ch_ : 2; }
    int32_t getFramesPerBurst() const { return 192; }
    AudioFormat getFormat() const { return fmt_; }
    SharingMode getSharingMode() const { return share_; }
    PerformanceMode getPerformanceMode() const { return perf_; }
    Usage getUsage() const { return usage_; }
    AudioApi getAudioApi() const { return AudioApi::AAudio; }
    int64_t getFramesRead() const { return 0; }
    // a assinatura REAL: ResultWithValue (ver nota da classe acima)
    ResultWithValue<int32_t> getXRunCount() const {
        return ResultWithValue<int32_t>(0);
    }

    // (só o stub: o estado p/ o fireErrorOnAllStreams filtrar os mortos)
    bool closedSt() const { return closed_; }

private:
    AudioStreamCallback* callback_;
    int32_t rate_;
    int32_t ch_;
    PerformanceMode perf_;
    SharingMode share_;
    AudioFormat fmt_;
    Usage usage_;
    StreamState state_ = StreamState::Open;
    bool closed_ = false;

    static void unregisterStream(AudioStream* s) {
        auto& v = detail::liveStreams();
        for (size_t i = 0; i < v.size(); ++i) {
            if (v[i] == s) {
                v.erase(v.begin() + static_cast<long>(i));
                return;
            }
        }
    }
    friend void testing::fireErrorOnAllStreams(Result);
};

inline void testing::fireErrorOnAllStreams(Result error) {
    // cópia: o close() do meio da sequência mexe no registo
    const std::vector<AudioStream*> victims = detail::liveStreams();
    for (AudioStream* s : victims) {
        if (s->closed_ || !s->callback_) {
            continue;
        }
        // a sequência REAL do oboe: before → close INTERNO → after
        s->callback_->onErrorBeforeClose(s, error);
        (void)s->close();
        s->callback_->onErrorAfterClose(s, error);
        ++testing::hooks().errorCallbacks;
    }
}

// ---- AudioStreamBuilder (setters encadeáveis como no real) ----------------
class AudioStreamBuilder {
public:
    AudioStreamBuilder* setDirection(Direction d) { dir_ = d; return this; }
    AudioStreamBuilder* setSampleRate(int32_t r) { rate_ = r; return this; }
    AudioStreamBuilder* setChannelCount(int c) { ch_ = c; return this; }
    AudioStreamBuilder* setFormat(AudioFormat f) { fmt_ = f; return this; }
    AudioStreamBuilder* setSharingMode(SharingMode s) { share_ = s; return this; }
    AudioStreamBuilder* setPerformanceMode(PerformanceMode p) { perf_ = p; return this; }
    AudioStreamBuilder* setUsage(Usage u) { usage_ = u; return this; }
    AudioStreamBuilder* setCallback(AudioStreamCallback* cb) { cb_ = cb; return this; }

    Result openStream(std::shared_ptr<AudioStream>& out) {
        const Result forced = testing::hooks().nextOpenResult;
        if (forced != Result::OK) {
            testing::hooks().nextOpenResult = Result::OK;
            out.reset();
            return forced;
        }
        if (dir_ != Direction::Output || !cb_) {
            out.reset();
            return Result::ErrorIllegalState;
        }
        out.reset(new AudioStream(cb_, rate_, ch_, perf_, share_, fmt_, usage_));
        detail::liveStreams().push_back(out.get());
        ++testing::hooks().openCount;
        return Result::OK;
    }

private:
    Direction dir_ = Direction::Output;
    int32_t rate_ = 0;   // 0 = Unspecified (o device escolhe)
    int ch_ = 0;
    AudioFormat fmt_ = AudioFormat::Unspecified;
    SharingMode share_ = SharingMode::Shared;
    PerformanceMode perf_ = PerformanceMode::None;
    Usage usage_ = Usage::Unspecified;
    AudioStreamCallback* cb_ = nullptr;
};

} // namespace oboe
