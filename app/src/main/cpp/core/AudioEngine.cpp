// core/AudioEngine.cpp — misturador (0.8.11). Ver AudioEngine.h.
#include "core/AudioEngine.h"

#include <cmath>
#include <cstring>

namespace vv {

i32 AudioEngine::play(const GiClip* clip, bool loop, f32 volume, f32 pitch) {
    if (!clip || !clip->ok()) {
        return -1;
    }
    // slot livre ou novo
    for (size_t i = 0; i < voices_.size(); ++i) {
        if (!voices_[i].playing) {
            voices_[i] = Voice{};
            voices_[i].clip = clip;
            voices_[i].loop = loop;
            voices_[i].volume = volume;
            voices_[i].pitch = pitch;
            voices_[i].playing = true;
            return static_cast<i32>(i);
        }
    }
    voices_.push_back(Voice{});
    Voice& v = voices_.back();
    v.clip = clip;
    v.loop = loop;
    v.volume = volume;
    v.pitch = pitch;
    v.playing = true;
    return static_cast<i32>(voices_.size() - 1);
}

void AudioEngine::stop(i32 voiceId) {
    if (voiceId >= 0 && voiceId < static_cast<i32>(voices_.size())) {
        voices_[static_cast<size_t>(voiceId)].playing = false;
        voices_[static_cast<size_t>(voiceId)].clip = nullptr;
    }
}

void AudioEngine::stopAll() {
    for (Voice& v : voices_) {
        v.playing = false;
        v.clip = nullptr;
    }
}

bool AudioEngine::playing(i32 voiceId) const {
    return voiceId >= 0 && voiceId < static_cast<i32>(voices_.size()) &&
           voices_[static_cast<size_t>(voiceId)].playing;
}

void AudioEngine::setParams(i32 voiceId, f32 volume, f32 pitch, bool loop) {
    if (voiceId >= 0 && voiceId < static_cast<i32>(voices_.size())) {
        Voice& v = voices_[static_cast<size_t>(voiceId)];
        v.volume = volume < 0.0f ? 0.0f : (volume > 4.0f ? 4.0f : volume);
        v.pitch = pitch < 0.25f ? 0.25f : (pitch > 4.0f ? 4.0f : pitch);
        v.loop = loop;
    }
}

void AudioEngine::setPosicional(i32 voiceId, bool on, const Vec3& pos,
                                f32 raioInterno, f32 raioExterno) {
    if (voiceId >= 0 && voiceId < static_cast<i32>(voices_.size())) {
        Voice& v = voices_[static_cast<size_t>(voiceId)];
        v.posicional = on;
        v.pos = pos;
        v.raioInterno = raioInterno;
        v.raioExterno = raioExterno;
    }
}

f32 AudioEngine::voiceProgress(i32 voiceId) const {
    if (voiceId < 0 || voiceId >= static_cast<i32>(voices_.size())) {
        return -1.0f;
    }
    const Voice& v = voices_[static_cast<size_t>(voiceId)];
    if (!v.playing || !v.clip || v.clip->frames == 0) {
        return -1.0f;
    }
    const f64 frac = v.cursor / static_cast<f64>(v.clip->frames);
    return frac < 0.0f ? 0.0f : (frac > 1.0f ? 1.0f : static_cast<f32>(frac));
}

u32 AudioEngine::countActive() const {
    u32 n = 0;
    for (const Voice& v : voices_) {
        if (v.playing) {
            ++n;
        }
    }
    return n;
}

f32 AudioEngine::attenuationOf(const Voice& v) const {
    if (!v.posicional) {
        return 1.0f;
    }
    return positionalAttenuation(listener_, v.pos, v.raioInterno,
                                 v.raioExterno);
}

void AudioEngine::mix(f32* out, u32 frames, u32 outChannels, u32 outRate) {
    if (!out || frames == 0 || outChannels == 0 || outRate == 0) {
        return;
    }
    // clear + soma por voz
    std::memset(out, 0, sizeof(f32) * frames * outChannels);
    for (Voice& v : voices_) {
        if (!v.playing || !v.clip || !v.clip->ok()) {
            continue;
        }
        const GiClip& clip = *v.clip;
        const f32 gain = v.volume * master * attenuationOf(v);
        if (gain <= 0.00001f) {
            // mudo/fora do raio: só avança o cursor (o loop conta o tempo)
            v.cursor += static_cast<f64>(frames) * v.pitch *
                        static_cast<f64>(clip.sampleRate) / outRate;
            if (v.cursor >= static_cast<f64>(clip.frames)) {
                v.cursor = v.loop
                    ? std::fmod(v.cursor, static_cast<f64>(clip.frames))
                    : static_cast<f64>(clip.frames);
                if (!v.loop) {
                    v.playing = false;
                }
            }
            continue;
        }
        // taxa do clip em frames de SAÍDA (resample linear por pitch)
        const f64 step = static_cast<f64>(clip.sampleRate) / outRate *
                         (v.pitch > 0.25f ? v.pitch : 0.25f);
        for (u32 f = 0; f < frames; ++f) {
            if (v.cursor >= static_cast<f64>(clip.frames)) {
                if (v.loop) {
                    v.cursor = std::fmod(v.cursor,
                                         static_cast<f64>(clip.frames));
                } else {
                    v.playing = false;
                    break;
                }
            }
            const u64 i0 = static_cast<u64>(v.cursor);
            const u64 i1 = i0 + 1 < clip.frames ? i0 + 1 : i0;
            const f32 t = static_cast<f32>(v.cursor - static_cast<f64>(i0));
            for (u32 c = 0; c < outChannels; ++c) {
                f32 s = 0.0f;
                if (c < clip.channels) {
                    const f32 a = clip.pcm[static_cast<size_t>(i0) *
                                              clip.channels + c];
                    const f32 b = clip.pcm[static_cast<size_t>(i1) *
                                              clip.channels + c];
                    s = a + (b - a) * t;
                } else {
                    // mono→stereo: duplica o canal 0
                    const f32 a = clip.pcm[static_cast<size_t>(i0) *
                                              clip.channels];
                    const f32 b = clip.pcm[static_cast<size_t>(i1) *
                                              clip.channels];
                    s = a + (b - a) * t;
                }
                out[f * outChannels + c] += s * gain;
            }
            v.cursor += step;
        }
        // FIM EXATO no último frame do bloco: sem isto a voz ficava
        // "playing" até ao PRÓXIMO callback (o cursor == frames só era
        // visto no início de uma iteração que já não vinha) — apanhado
        // pelo teste wiring011 (o clip de 1 s acabava e playing() dizia
        // true entre callbacks)
        if (!v.loop && v.cursor >= static_cast<f64>(clip.frames)) {
            v.playing = false;
        }
        // clamp final por frame (várias vozes podem somar >1)
        // — feito UMA vez no fim do mix (abaixo)
    }
    for (u32 i = 0; i < frames * outChannels; ++i) {
        if (out[i] > 1.0f) {
            out[i] = 1.0f;
        } else if (out[i] < -1.0f) {
            out[i] = -1.0f;
        }
    }
    mixedFrames_ += frames;
}

} // namespace vv
