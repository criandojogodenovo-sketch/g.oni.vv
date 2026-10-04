// platform/AudioOutDevice.cpp — AAudio REAL + AudioTrack JNI (0.8.11; hotfix 0.9.3).
//
// PORQUE dlopen (a decisão documentada): o minSdk é 24 e o AAudio só existe
// a partir da API 26 — o header <aaudio/AAudio.h> do NDK fica VAZIO quando
// se compila com __ANDROID_API__=24 (todas as declarações estão atrás de
// #if __ANDROID_API__ >= 26) e a ligação direta de -laaudio nem sequer
// resolve. O CAMINHO ROBUSTO é o mesmo do Oboe do Google: carregar a
// libaaudio.so EM RUNTIME (dlopen/dlsym) com typedefs próprios — o MESMO
// binário corre na API 24/25 (dlopen falha → FALLBACK AudioTrack limpo) e
// na 26+ (AAudio nativo). Nenhum enum do AAudio é inventado: os valores
// abaixo são a ABI congelada da API 26 e a sanidade é verificada DEPOIS do
// openStream (canais/rate reais lidos do stream; desacordo = falha do
// start = fallback, nunca crash).
//
// HOTFIX 0.9.3 (REG-002/R-006) — o que mudou neste TU (Tarefa 3, o fix
// robusto do AAudio como FALLBACK do Oboe):
//   1. StartGate atómico: start() chamado 2× NÃO abre um 2º stream (a
//      fuga de stream vivo era a porta de entrada do SIGSEGV no Unisoc —
//      tombstones 00-17/21-31 da app antiga);
//   2. stream_ protegido por mtx_: o error callback pode correr AO MESMO
//      TEMPO que o stop() da main — quem tira o stream do slot primeiro
//      fecha; o outro vê vazio (nunca duplo close, nunca use-after-free);
//   3. errCb FECHA o stream morto (o padrão que o Oboe segue internamente:
//      error-callback → close; o AAudio documenta que o stream morto tem
//      de ser fechado) e abre o portão (um start futuro pode tentar);
//   4. TODOS os retornos verificados e logados (requestStop/close/
//      requestPause/requestStart) — nunca assumidos;
//   5. falha = return false → o main decide o fallback (o editor segue
//      SEM SOM, nunca crasha por causa do áudio).
//
// FALLBACK AudioTrack (JNI): se o probe mandar (ou o Oboe E o AAudio
// recusarem), o MESMO misturador alimenta um AudioTrack em MODE_STREAM
// com uma thread de escrita própria. 0.9.3: também ganhou o StartGate
// (o arranque duplo criava uma 2ª thread de escrita + track vazado).
//
// Este TU só compila no ANDROID (CMake: if(ANDROID)) — o OboeBackend.cpp
// (o primário) é TU COMUM e é compilado-verificado nos DOIS lados.
#include "platform/AudioOut.h"
#include "platform/AudioStartGate.h"

#include <dlfcn.h>

#include <android/api-level.h>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <mutex>
#include <thread>
#include <vector>

#include <jni.h>

#include "platform/EngineLog.h"
#include "platform/JniAttach.h"

namespace vv::audioout {

namespace {

// a JavaVM chega do android_main (o glue tem activity->vm)
JavaVM* g_vm = nullptr;

} // namespace

// (void* para o AudioOut.h compilar no host sem <jni.h>; o host ignora —
// a versão host em AudioOut.cpp é um no-op)
void setVm(void* vm) {
    g_vm = static_cast<JavaVM*>(vm);
}

namespace {

// env do thread CHAMADOR com attach explícito (a MESMA política do
// StorageBridge: tabela pura jni::planAttach, attach NOMEADO e PERMANENTE,
// cada falha logada com o código — nenhum JNIEnv* assumido não-nulo)
JNIEnv* attachedEnv() {
    if (!g_vm) {
        return nullptr;   // sem VM (android_main ainda não correu)
    }
    JNIEnv* env = nullptr;
    const jint rc = g_vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
    const jni::AttachAction plan = jni::planAttach(rc);
    if (plan == jni::AttachAction::Use) {
        return env;
    }
    if (plan == jni::AttachAction::Attach) {
        JavaVMAttachArgs args {};
        args.version = JNI_VERSION_1_6;
        args.name    = "goni-audio";
        args.group   = nullptr;
        const jint arc = g_vm->AttachCurrentThread(&env, &args);
        if (arc != jni::kJniOk || !env) {
            elog::error("audio: AttachCurrentThread('goni-audio') FALHOU rc=%d",
                        static_cast<int>(arc));
            return nullptr;
        }
        return env;   // SEM Detach: a thread de áudio vive até ao fim
    }
    elog::error("audio: GetEnv FALHOU rc=%d (%s)", static_cast<int>(rc),
                jni::attachActionLabel(plan));
    return nullptr;
}

// limpa exceção pendente e devolve true se havia (log do lado do chamador)
bool clearPendingException(JNIEnv* env) {
    if (env && env->ExceptionCheck()) {
        env->ExceptionClear();
        return true;
    }
    return false;
}

// ---- AAudioLoader (dlopen/dlsym — os typedefs são NOSSOS porque o header
// oficial está vazio a compilar contra a API 24; a ABI é opaca: handles são
// ponteiros para structs opacos e os enums têm valores congelados) -----------
struct AAudioStreamBuilder;   // opaco (o real vive na libaaudio.so)
struct AAudioStream;

typedef int32_t aaudio_result_t;
typedef int32_t aaudio_stream_state_t;
typedef int32_t aaudio_format_t;
typedef int32_t aaudio_performance_mode_t;
typedef int32_t aaudio_data_callback_result_t;

typedef aaudio_data_callback_result_t (*AAudioDataCallbackProc)(
        AAudioStream*, void*, void*, int32_t);
typedef void (*AAudioErrorCallbackProc)(AAudioStream*, void*, aaudio_result_t);

// valores da ABI da API 26 (congelados; ver comentário do topo)
constexpr aaudio_result_t kAaudioOk = 0;
constexpr aaudio_format_t kAaudioPcmFloat = 4;              // ENCODING float
constexpr aaudio_performance_mode_t kAaudioPerfLowLatency = 12;
constexpr aaudio_stream_state_t kAaudioStateStarting = 2;
constexpr aaudio_stream_state_t kAaudioStateStarted = 3;
constexpr aaudio_data_callback_result_t kAaudioCbContinue = 0;

typedef const char* (*fn_convertResultToText)(aaudio_result_t);
typedef aaudio_result_t (*fn_createStreamBuilder)(AAudioStreamBuilder**);
typedef void (*fn_builderSetFormat)(AAudioStreamBuilder*, aaudio_format_t);
typedef void (*fn_builderSetSampleRate)(AAudioStreamBuilder*, int32_t);
typedef void (*fn_builderSetChannelCount)(AAudioStreamBuilder*, int32_t);
typedef void (*fn_builderSetPerformanceMode)(AAudioStreamBuilder*,
                                             aaudio_performance_mode_t);
typedef void (*fn_builderSetDataCallback)(AAudioStreamBuilder*,
                                          AAudioDataCallbackProc, void*);
typedef void (*fn_builderSetErrorCallback)(AAudioStreamBuilder*,
                                           AAudioErrorCallbackProc, void*);
typedef aaudio_result_t (*fn_builderOpen)(AAudioStreamBuilder*, AAudioStream**);
typedef aaudio_result_t (*fn_builderDelete)(AAudioStreamBuilder*);
typedef aaudio_result_t (*fn_requestStart)(AAudioStream*);
typedef aaudio_result_t (*fn_requestPause)(AAudioStream*);
typedef aaudio_result_t (*fn_requestStop)(AAudioStream*);
typedef aaudio_result_t (*fn_close)(AAudioStream*);
typedef aaudio_stream_state_t (*fn_getState)(AAudioStream*);
typedef int32_t (*fn_getChannelCount)(AAudioStream*);
typedef int32_t (*fn_getSampleRate)(AAudioStream*);
typedef int64_t (*fn_getFramesRead)(AAudioStream*);
typedef int32_t (*fn_getXRunCount)(AAudioStream*);

struct AAudioLoader {
    void* lib = nullptr;
    fn_convertResultToText convertResultToText = nullptr;
    fn_createStreamBuilder createStreamBuilder = nullptr;
    fn_builderSetFormat builderSetFormat = nullptr;
    fn_builderSetSampleRate builderSetSampleRate = nullptr;
    fn_builderSetChannelCount builderSetChannelCount = nullptr;
    fn_builderSetPerformanceMode builderSetPerformanceMode = nullptr;
    fn_builderSetDataCallback builderSetDataCallback = nullptr;
    fn_builderSetErrorCallback builderSetErrorCallback = nullptr;
    fn_builderOpen builderOpen = nullptr;
    fn_builderDelete builderDelete = nullptr;
    fn_requestStart requestStart = nullptr;
    fn_requestPause requestPause = nullptr;
    fn_requestStop requestStop = nullptr;
    fn_close close = nullptr;
    fn_getState getState = nullptr;
    fn_getChannelCount getChannelCount = nullptr;
    fn_getSampleRate getSampleRate = nullptr;
    fn_getFramesRead getFramesRead = nullptr;
    fn_getXRunCount getXRunCount = nullptr;
    bool ok() const { return lib && createStreamBuilder && builderOpen &&
                             requestStart && requestPause && requestStop &&
                             close && getState; }
};

// carrega 1× (a 1ª chamada de createAAudio); devolve nullptr se o device
// não tem AAudio (API < 26: a lib nem existe — fallback AudioTrack limpo)
AAudioLoader* aaudioLoader() {
    static AAudioLoader* loader = nullptr;
    static bool tried = false;
    if (tried) {
        return loader;
    }
    tried = true;
    const int api = android_get_device_api_level();
    if (api < 26) {
        elog::info("audio: API %d < 26 — AAudio indisponivel (fallback "
                   "AudioTrack, documentado)", api);
        return nullptr;
    }
    void* lib = ::dlopen("libaaudio.so", RTLD_NOW);
    if (!lib) {
        elog::warn("audio: dlopen(libaaudio.so) FALHOU na API %d — %s",
                   api, ::dlerror() ? ::dlerror() : "?");
        return nullptr;
    }
    auto* l = new AAudioLoader();
    l->lib = lib;
    auto sym = [lib](const char* name) -> void* { return ::dlsym(lib, name); };
    l->convertResultToText =
        reinterpret_cast<fn_convertResultToText>(sym("AAudio_convertResultToText"));
    l->createStreamBuilder =
        reinterpret_cast<fn_createStreamBuilder>(sym("AAudio_createStreamBuilder"));
    l->builderSetFormat =
        reinterpret_cast<fn_builderSetFormat>(sym("AAudioStreamBuilder_setFormat"));
    l->builderSetSampleRate = reinterpret_cast<fn_builderSetSampleRate>(
        sym("AAudioStreamBuilder_setSampleRate"));
    l->builderSetChannelCount = reinterpret_cast<fn_builderSetChannelCount>(
        sym("AAudioStreamBuilder_setChannelCount"));
    l->builderSetPerformanceMode = reinterpret_cast<fn_builderSetPerformanceMode>(
        sym("AAudioStreamBuilder_setPerformanceMode"));
    l->builderSetDataCallback = reinterpret_cast<fn_builderSetDataCallback>(
        sym("AAudioStreamBuilder_setDataCallback"));
    l->builderSetErrorCallback = reinterpret_cast<fn_builderSetErrorCallback>(
        sym("AAudioStreamBuilder_setErrorCallback"));
    l->builderOpen =
        reinterpret_cast<fn_builderOpen>(sym("AAudioStreamBuilder_openStream"));
    l->builderDelete =
        reinterpret_cast<fn_builderDelete>(sym("AAudioStreamBuilder_delete"));
    l->requestStart =
        reinterpret_cast<fn_requestStart>(sym("AAudioStream_requestStart"));
    l->requestPause =
        reinterpret_cast<fn_requestPause>(sym("AAudioStream_requestPause"));
    l->requestStop =
        reinterpret_cast<fn_requestStop>(sym("AAudioStream_requestStop"));
    l->close = reinterpret_cast<fn_close>(sym("AAudioStream_close"));
    l->getState = reinterpret_cast<fn_getState>(sym("AAudioStream_getState"));
    l->getChannelCount =
        reinterpret_cast<fn_getChannelCount>(sym("AAudioStream_getChannelCount"));
    l->getSampleRate =
        reinterpret_cast<fn_getSampleRate>(sym("AAudioStream_getSampleRate"));
    l->getFramesRead =
        reinterpret_cast<fn_getFramesRead>(sym("AAudioStream_getFramesRead"));
    l->getXRunCount =
        reinterpret_cast<fn_getXRunCount>(sym("AAudioStream_getXRunCount"));
    if (!l->ok()) {
        elog::error("audio: libaaudio.so sem os símbolos esperados — "
                    "fallback AudioTrack");
        ::dlclose(lib);
        delete l;
        return nullptr;
    }
    loader = l;
    elog::info("audio: libaaudio.so carregada (dlopen, API %d)", api);
    return loader;
}

// ---- AAudio ------------------------------------------------------------------
class AAudioBackend final : public Backend {
public:
    ~AAudioBackend() override { stop(); }

    bool start(u32 sampleRate, u16 channels) override {
        // REG-002: NUNCA arranques duplos — o 2º chamador NÃO toca no
        // hardware (idempotência: o stream que toca continua)
        if (!gate_.tryEnter()) {
            elog::warn("audio: start ignorado — stream AAudio ja ativo "
                       "(porta R-006)");
            return true;
        }
        loader_ = aaudioLoader();
        if (!loader_) {
            gate_.release();   // API < 26 / dlopen falhou → quem chamou decide
            return false;
        }
        AAudioStreamBuilder* b = nullptr;
        if (loader_->createStreamBuilder(&b) != kAaudioOk || !b) {
            elog::error("audio: AAudio_createStreamBuilder FALHOU");
            gate_.release();
            return false;
        }
        loader_->builderSetFormat(b, kAaudioPcmFloat);
        loader_->builderSetSampleRate(b, static_cast<int32_t>(sampleRate));
        loader_->builderSetChannelCount(b, static_cast<int32_t>(channels));
        loader_->builderSetPerformanceMode(b, kAaudioPerfLowLatency);
        loader_->builderSetDataCallback(b, &AAudioBackend::dataCb, this);
        loader_->builderSetErrorCallback(b, &AAudioBackend::errCb, this);
        AAudioStream* s = nullptr;
        const aaudio_result_t rc = loader_->builderOpen(b, &s);
        loader_->builderDelete(b);
        if (rc != kAaudioOk || !s) {
            elog::error("audio: AAudio openStream FALHOU (%s)", resultText(rc));
            gate_.release();
            return false;
        }
        // SANIDADE pós-open (a ABI é opaca: desacordo = falha = fallback,
        // nunca crash) — o stream real manda nos canais/rate
        const int32_t ch = loader_->getChannelCount(s);
        const int32_t sr = loader_->getSampleRate(s);
        if (ch != static_cast<int32_t>(channels) || sr <= 0) {
            elog::error("audio: stream AAudio aberto com ch=%d rate=%d "
                        "(pedidos %d/%u) — recusa", ch, sr,
                        static_cast<int>(channels), sampleRate);
            loader_->close(s);
            gate_.release();
            return false;
        }
        const aaudio_result_t rs = loader_->requestStart(s);
        if (rs != kAaudioOk) {
            elog::error("audio: AAudio requestStart FALHOU (%s)",
                        resultText(rs));
            loader_->close(s);
            gate_.release();
            return false;
        }
        {
            std::lock_guard<std::mutex> lk(mtx_);
            stream_ = s;
            errored_ = false;
        }
        elog::info("audio: stream AAudio ATIVO rate=%d ch=%d (perf=low-latency)",
                   sr, ch);
        return true;
    }

    void stop() override {
        AAudioStream* s = nullptr;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            s = stream_;
            stream_ = nullptr;
        }
        if (s && loader_) {
            const aaudio_result_t rst = loader_->requestStop(s);
            if (rst != kAaudioOk) {
                elog::warn("audio: AAudio requestStop no fim (%s)",
                            resultText(rst));
            }
            const aaudio_result_t rcl = loader_->close(s);
            if (rcl != kAaudioOk) {
                elog::warn("audio: AAudio close no fim (%s)",
                            resultText(rcl));
            }
        }
        gate_.open();
    }
    void pause() override {
        AAudioStream* s = nullptr;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            s = stream_;
        }
        if (s && loader_) {
            const aaudio_result_t r = loader_->requestPause(s);
            if (r != kAaudioOk) {
                elog::warn("audio: AAudio requestPause (%s)", resultText(r));
            }
        }
    }
    void resume() override {
        AAudioStream* s = nullptr;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            s = stream_;
        }
        if (s && loader_) {
            const aaudio_result_t r = loader_->requestStart(s);
            if (r != kAaudioOk) {
                elog::warn("audio: AAudio resume requestStart (%s) — stream "
                            "morto? (o probe decide a troca)", resultText(r));
            }
        }
    }
    bool ready() const override {
        if (errored_.load()) {
            return false;
        }
        AAudioStream* s = nullptr;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            s = stream_;
        }
        if (!s || !loader_) {
            return false;
        }
        const aaudio_stream_state_t st = loader_->getState(s);
        return st == kAaudioStateStarted || st == kAaudioStateStarting;
    }
    const char* name() const override { return "aaudio"; }
    u64 framesOut() const override {
        AAudioStream* s = nullptr;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            s = stream_;
        }
        return s && loader_->getFramesRead
            ? static_cast<u64>(loader_->getFramesRead(s)) : 0ull;
    }
    u32 xruns() const override {
        AAudioStream* s = nullptr;
        {
            std::lock_guard<std::mutex> lk(mtx_);
            s = stream_;
        }
        return s && loader_->getXRunCount
            ? static_cast<u32>(loader_->getXRunCount(s)) : 0u;
    }

private:
    const char* resultText(aaudio_result_t rc) const {
        return loader_->convertResultToText
            ? loader_->convertResultToText(rc) : "(sem texto)";
    }

    static aaudio_data_callback_result_t dataCb(AAudioStream* s, void* user,
                                                 void* data, int32_t numFrames) {
        (void)user;
        const AAudioLoader* l = aaudioLoader();
        const int32_t ch = l && l->getChannelCount ? l->getChannelCount(s) : 2;
        const int32_t sr = l && l->getSampleRate ? l->getSampleRate(s) : 44100;
        if (MixFn mix = currentMixFn()) {
            mix(static_cast<f32*>(data), static_cast<u32>(numFrames),
                static_cast<u32>(ch > 0 ? ch : 2), static_cast<u32>(sr > 0 ? sr : 44100));
        } else {
            std::memset(data, 0, sizeof(f32) * static_cast<size_t>(numFrames < 0 ? 0 : numFrames) *
                                       static_cast<size_t>(ch > 0 ? ch : 2));
        }
        return kAaudioCbContinue;
    }

    static void errCb(AAudioStream* s, void* user, aaudio_result_t error) {
        AAudioBackend* self = static_cast<AAudioBackend*>(user);
        const AAudioLoader* l = aaudioLoader();
        elog::error("audio: stream AAudio ERRO %d (%s) — disconnect?",
                    static_cast<int>(error),
                    l && l->convertResultToText ? l->convertResultToText(error)
                                                : "?");
        if (!self) {
            return;
        }
        self->errored_ = true;   // ready() passa a falso → probe vê
        // 0.9.3 (REG-002): o stream MORREU — o contrato do AAudio manda
        // FECHAR (o padrão que o Oboe segue internamente: error-callback
        // → close). Retiramo-lo do slot SOB o mutex: o stop() da main pode
        // correr AO MESMO TEMPO — quem tirar primeiro fecha, o outro vê
        // vazio (nunca duplo close, nunca use-after-free).
        AAudioStream* doomed = nullptr;
        {
            std::lock_guard<std::mutex> lk(self->mtx_);
            if (self->stream_ == s) {
                doomed = s;
                self->stream_ = nullptr;
            }
        }
        if (doomed && self->loader_) {
            const aaudio_result_t rcl = self->loader_->close(doomed);
            if (rcl != kAaudioOk) {
                elog::warn("audio: close do stream morto (%s)",
                            self->resultText(rcl));
            }
        }
        self->gate_.open();   // um start futuro pode tentar de novo
    }

    mutable std::mutex mtx_;               // guarda stream_ (errCb × stop)
    AAudioLoader* loader_ = nullptr;
    AAudioStream* stream_ = nullptr;       // protegido por mtx_
    StartGate gate_;                        // REG-002: nunca 2×
    std::atomic<bool> errored_{false};
    u32 rate_ = 44100;
};

// ---- AudioTrack (JNI — o fallback DOCUMENTADO) -------------------------------
// Thread própria de escrita: AudioTrack.write(float[]) em MODE_STREAM.
class AudioTrackBackend final : public Backend {
public:
    ~AudioTrackBackend() override { stop(); }

    bool start(u32 sampleRate, u16 channels) override {
        // REG-002: o arranque duplo criava uma 2ª thread de escrita + track
        // vazado — o portão impede (o AudioTrack segue a MESMA regra)
        if (!gate_.tryEnter()) {
            elog::warn("audio: start ignorado — AudioTrack ja ativo "
                       "(porta R-006)");
            return true;
        }
        JNIEnv* env = attachedEnv();
        if (!env) {
            elog::error("audio: AudioTrack sem JNIEnv (VM não registada?)");
            gate_.release();
            return false;
        }
        // ---- AudioFormat.Builder (encoding float + rate + máscara) -------
        jclass fmtB = env->FindClass("android/media/AudioFormat$Builder");
        if (!fmtB || clearPendingException(env)) {
            elog::error("audio: AudioFormat$Builder não resolvida");
            gate_.release();
            return false;
        }
        jmethodID fbCtor = env->GetMethodID(fmtB, "<init>", "()V");
        jmethodID fbEnc = env->GetMethodID(
            fmtB, "setEncoding", "(I)Landroid/media/AudioFormat$Builder;");
        jmethodID fbRate = env->GetMethodID(
            fmtB, "setSampleRate", "(I)Landroid/media/AudioFormat$Builder;");
        jmethodID fbMask = env->GetMethodID(
            fmtB, "setChannelMask", "(I)Landroid/media/AudioFormat$Builder;");
        jmethodID fbBuild =
            env->GetMethodID(fmtB, "build", "()Landroid/media/AudioFormat;");
        if (!fbCtor || !fbEnc || !fbRate || !fbMask || !fbBuild) {
            clearPendingException(env);
            elog::error("audio: métodos do AudioFormat.Builder não achados");
            gate_.release();
            return false;
        }
        jobject fmtBuilder = env->NewObject(fmtB, fbCtor);
        fmtBuilder = env->CallObjectMethod(
            fmtBuilder, fbEnc, 4 /*ENCODING_PCM_FLOAT*/);
        fmtBuilder = env->CallObjectMethod(
            fmtBuilder, fbRate, static_cast<jint>(sampleRate));
        fmtBuilder = env->CallObjectMethod(
            fmtBuilder, fbMask,
            channels == 2 ? 12 /*CHANNEL_OUT_STEREO*/ : 4 /*MONO*/);
        jobject fmt = env->CallObjectMethod(fmtBuilder, fbBuild);
        if (clearPendingException(env) || !fmt) {
            elog::error("audio: AudioFormat.Builder falhou (rate %u ch %u)",
                        sampleRate, channels);
            gate_.release();
            return false;
        }
        // ---- AudioAttributes.Builder (USAGE_MEDIA) ------------------------
        jclass attrB = env->FindClass("android/media/AudioAttributes$Builder");
        jmethodID abCtor = env->GetMethodID(attrB, "<init>", "()V");
        jmethodID abUsage = env->GetMethodID(
            attrB, "setUsage", "(I)Landroid/media/AudioAttributes$Builder;");
        jmethodID abBuild =
            env->GetMethodID(attrB, "build", "()Landroid/media/AudioAttributes;");
        if (!attrB || !abCtor || !abUsage || !abBuild ||
            clearPendingException(env)) {
            elog::error("audio: AudioAttributes$Builder não resolvida");
            gate_.release();
            return false;
        }
        jobject attrBuilder = env->NewObject(attrB, abCtor);
        attrBuilder = env->CallObjectMethod(attrBuilder, abUsage,
                                            1 /*USAGE_MEDIA*/);
        jobject attrs = env->CallObjectMethod(attrBuilder, abBuild);
        if (clearPendingException(env) || !attrs) {
            elog::error("audio: AudioAttributes.Builder falhou");
            gate_.release();
            return false;
        }
        // ---- AudioTrack: ctor 5-arg (API 21) e, se não houver, o 4-arg ----
        jclass trkCls = env->FindClass("android/media/AudioTrack");
        if (!trkCls || clearPendingException(env)) {
            elog::error("audio: AudioTrack não resolvida");
            gate_.release();
            return false;
        }
        // 200 ms de buffer (10 ms por escrita × 20)
        const jint bufBytes = static_cast<jint>(sampleRate) / 5 *
                              static_cast<jint>(channels) *
                              static_cast<jint>(sizeof(f32));
        jmethodID trkCtor = env->GetMethodID(
            trkCls, "<init>",
            "(Landroid/media/AudioAttributes;Landroid/media/AudioFormat;III)V");
        if (trkCtor) {
            track_ = env->NewGlobalRef(env->NewObject(
                trkCls, trkCtor, attrs, fmt, bufBytes,
                1 /*MODE_STREAM*/, 0 /*sessionId gerado*/));
        } else {
            clearPendingException(env);
            trkCtor = env->GetMethodID(
                trkCls, "<init>",
                "(Landroid/media/AudioAttributes;Landroid/media/AudioFormat;II)V");
            if (!trkCtor) {
                clearPendingException(env);
                elog::error("audio: nenhum ctor AudioTrack utilizavel");
                gate_.release();
                return false;
            }
            track_ = env->NewGlobalRef(env->NewObject(
                trkCls, trkCtor, attrs, fmt, bufBytes, 1 /*MODE_STREAM*/));
        }
        if (clearPendingException(env) || !track_) {
            elog::error("audio: NewObject AudioTrack FALHOU");
            gate_.release();
            return false;
        }
        jmethodID play = env->GetMethodID(trkCls, "play", "()V");
        if (play) {
            env->CallVoidMethod(track_, play);
            clearPendingException(env);
        }
        writeF_ = env->GetMethodID(trkCls, "write", "([FII)I");
        stopM_ = env->GetMethodID(trkCls, "stop", "()V");
        releaseM_ = env->GetMethodID(trkCls, "release", "()V");
        if (!writeF_ || !stopM_ || !releaseM_) {
            clearPendingException(env);
            elog::error("audio: métodos write/stop/release não achados");
            env->DeleteGlobalRef(track_);
            track_ = nullptr;
            gate_.release();
            return false;
        }
        running_ = true;
        writer_ = std::thread([this, sampleRate, channels] {
            writerLoop(sampleRate, channels);
        });
        elog::info("audio: AudioTrack ATIVO (fallback JNI, rate %u ch %u)",
                   sampleRate, channels);
        return true;
    }

    void stop() override {
        running_ = false;
        if (writer_.joinable()) {
            writer_.join();
        }
        JNIEnv* env = attachedEnv();
        if (env && track_) {
            env->CallVoidMethod(track_, stopM_);
            env->CallVoidMethod(track_, releaseM_);
            clearPendingException(env);
            env->DeleteGlobalRef(track_);
        }
        track_ = nullptr;
        if (arr_) {
            if (JNIEnv* e2 = attachedEnv()) {
                e2->DeleteGlobalRef(arr_);
            }
            arr_ = nullptr;
            arrSize_ = 0;
        }
        gate_.open();   // REG-002: a paragem liberta o portão
    }
    void pause() override { paused_ = true; }
    void resume() override { paused_ = false; }
    bool ready() const override { return running_.load() && track_ != nullptr; }
    const char* name() const override { return "audiotrack"; }
    u64 framesOut() const override { return framesOut_; }

private:
    void writerLoop(u32 sampleRate, u16 channels) {
        const u32 frames = sampleRate / 100;   // 10 ms por escrita
        std::vector<f32> buf(static_cast<size_t>(frames) * channels);
        while (running_.load()) {
            if (paused_.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                continue;
            }
            if (MixFn mix = currentMixFn()) {
                mix(buf.data(), frames, channels, sampleRate);
            } else {
                std::fill(buf.begin(), buf.end(), 0.0f);
            }
            JNIEnv* env = attachedEnv();
            if (!env || !track_) {
                break;   // VM morta: para a thread sem tocar em Java
            }
            if (!arr_ || arrSize_ < buf.size()) {
                if (arr_) {
                    env->DeleteGlobalRef(arr_);
                    arr_ = nullptr;
                }
                arr_ = static_cast<jfloatArray>(env->NewGlobalRef(
                    env->NewFloatArray(static_cast<jsize>(buf.size()))));
                arrSize_ = buf.size();
                if (!arr_) {
                    clearPendingException(env);
                    continue;
                }
            }
            env->SetFloatArrayRegion(arr_, 0, static_cast<jsize>(buf.size()),
                                      buf.data());
            env->CallIntMethod(track_, writeF_, arr_, 0,
                               static_cast<jint>(buf.size()));
            clearPendingException(env);   // write bloqueado = desiste da writes
            framesOut_ += frames;
        }
    }

    jobject track_ = nullptr;
    std::thread writer_;
    std::atomic<bool> running_{false};
    std::atomic<bool> paused_{false};
    std::atomic<u64> framesOut_{0};
    StartGate gate_;   // REG-002 (R-006): nunca 2×
    jfloatArray arr_ = nullptr;   // buffer reutilizado (global ref da thread)
    size_t arrSize_ = 0;
    jmethodID writeF_ = nullptr;
    jmethodID stopM_ = nullptr;
    jmethodID releaseM_ = nullptr;
};

} // namespace

Backend* createAAudio() { return new AAudioBackend(); }
Backend* createAudioTrack() { return new AudioTrackBackend(); }

} // namespace vv::audioout
