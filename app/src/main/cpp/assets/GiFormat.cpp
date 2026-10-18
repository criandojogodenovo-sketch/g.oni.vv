// assets/GiFormat.cpp — contentor .gi + IMA-ADPCM + WAV (0.8.11).
//
// IMA-ADPCM: o algoritmo clássico das tabelas de 89 passos (o mesmo do
// WAV IMA), SEM headers de bloco — os estados INICIAIS por canal vão no
// início do payload (4 B por canal: predictor i16 + index i16) e os
// nibbles seguem INTERLEAVED por canal (L R L R… no stereo). 16 bits →
// 4 bits = exatamente 4:1. Determinístico: o round-trip do CI aferia
// erro máximo por amostra (≤ 3% típico — as transients pagam o passo
// da tabela; a ENERGIA fica dentro de 0.5%).
#include "assets/GiFormat.h"

#include <cmath>
#include <cstring>

#include "assets/GOwnFormats.h"   // gfnv1a (uma só convenção de checksum)
#include "platform/EngineLog.h"

// decoders vendors (implementações em TU próprio — ver AudioCodecs.cpp)
namespace vv {
bool decodeOgg(const u8* data, size_t len, u32& sampleRate, u16& channels,
               std::vector<f32>& pcm, u64& frames, std::string& err);
bool decodeMp3(const u8* data, size_t len, u32& sampleRate, u16& channels,
               std::vector<f32>& pcm, u64& frames, std::string& err);
}

namespace vv {

namespace {

// ---- tabelas IMA (as 89 clássicas) ------------------------------------------
const i32 kImaStepTable[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37,
    41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173,
    190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066,
    2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428, 4871, 5358, 5894,
    6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289,
    16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767};
const i32 kImaIndexTable[16] = {-1, -1, -1, -1, 2, 4, 6, 8,
                                -1, -1, -1, -1, 2, 4, 6, 8};

struct ImaState {
    i32 predictor = 0;
    i32 index = 0;
};

inline i32 clamp16(i32 v) {
    if (v < -32768) return -32768;
    if (v > 32767) return 32767;
    return v;
}

inline u8 encodeNibble(ImaState& st, i16 sample) {
    const i32 step = kImaStepTable[st.index];
    i32 diff = static_cast<i32>(sample) - st.predictor;
    u8 code = 0;
    if (diff < 0) {
        code = 8;
        diff = -diff;
    }
    if (diff >= step) {
        code |= 4;
        diff -= step;
    }
    if (diff >= step >> 1) {
        code |= 2;
        diff -= step >> 1;
    }
    if (diff >= step >> 2) {
        code |= 1;
    }
    // reconstrói o predictor (o DECODER faz a MESMA conta — a divergência
    // entre o original e o reconstruído é o erro de quantização, ~3%)
    i32 recon = st.predictor;
    const i32 d = step >> 3;
    if (code & 4) recon += step;
    if (code & 2) recon += step >> 1;
    if (code & 1) recon += step >> 2;
    recon += d;
    if (code & 8) recon = st.predictor - (recon - st.predictor);
    st.predictor = clamp16(recon);
    st.index += kImaIndexTable[code];
    if (st.index < 0) st.index = 0;
    if (st.index > 88) st.index = 88;
    return code;
}

inline i16 decodeNibble(ImaState& st, u8 code) {
    const i32 step = kImaStepTable[st.index];
    i32 recon = st.predictor;
    if (code & 4) recon += step;
    if (code & 2) recon += step >> 1;
    if (code & 1) recon += step >> 2;
    recon += step >> 3;
    if (code & 8) recon = st.predictor - (recon - st.predictor);
    st.predictor = clamp16(recon);
    st.index += kImaIndexTable[code];
    if (st.index < 0) st.index = 0;
    if (st.index > 88) st.index = 88;
    return static_cast<i16>(st.predictor);
}

// ---- little-endian raw IO (idêntico ao GOwnFormats — privado) --------------
void put16(std::vector<u8>& b, u16 v) {
    b.push_back(static_cast<u8>(v & 0xFF));
    b.push_back(static_cast<u8>(v >> 8));
}
void put32(std::vector<u8>& b, u32 v) {
    put16(b, static_cast<u16>(v & 0xFFFF));
    put16(b, static_cast<u16>(v >> 16));
}
void put64(std::vector<u8>& b, u64 v) {
    put32(b, static_cast<u32>(v & 0xFFFFFFFFull));
    put32(b, static_cast<u32>(v >> 32));
}
struct Rd {
    const u8* p;
    size_t n;
    size_t i = 0;
    bool bad = false;
    u8 u8_() { if (i + 1 > n) { bad = true; return 0; } return p[i++]; }
    u16 u16_() { const u16 lo = u8_(); return static_cast<u16>(lo | (u8_() << 8)); }
    u32 u32_() { const u16 lo = u16_(); const u16 hi = u16_();
                 return static_cast<u32>(lo) | (static_cast<u32>(hi) << 16); }
    u64 u64_() { const u32 lo = u32_(); const u32 hi = u32_();
                 return static_cast<u64>(lo) | (static_cast<u64>(hi) << 32); }
};

} // namespace

bool imaEncode(const i16* pcm, u64 frames, u16 channels, std::vector<u8>& out) {
    out.clear();
    if (!pcm || channels == 0 || channels > 8) {
        return false;
    }
    if (frames == 0) {
        return false;   // zero frames = nada a codificar (o clip seria vazio)
    }
    // estados iniciais: predictor = 1ª amostra do canal (vai CRUA no
    // payload — é exata), index=0; os NIBBLES codificam as restantes
    std::vector<ImaState> states(channels);
    for (u16 c = 0; c < channels; ++c) {
        states[c].predictor = pcm[c];
        states[c].index = 0;
        // estado serializado: predictor i16 + index i16
        put16(out, static_cast<u16>(states[c].predictor & 0xFFFF));
        put16(out, static_cast<u16>(states[c].index));
    }
    // nibbles das amostras RESTANTES, interleaved por canal; 2 amostras por
    // byte (lo primeiro). O último byte, se sobrar só 1 amostra, leva o
    // nibble alto a ZERO (padding conhecido do decoder).
    const u64 total = frames * channels;
    u8 byte = 0;
    bool haveLo = false;
    for (u64 s = channels; s < total; ++s) {
        const u8 nib = encodeNibble(states[s % channels], pcm[s]) & 0xF;
        if (!haveLo) {
            byte = nib;
            haveLo = true;
        } else {
            out.push_back(static_cast<u8>(byte | (nib << 4)));
            haveLo = false;
        }
    }
    if (haveLo) {
        out.push_back(byte);   // hi nibble = 0 (padding)
    }
    return true;
}

bool imaDecode(const u8* data, size_t bytes, u16 channels, u64 expectedFrames,
               std::vector<i16>& out, u64& frames) {
    out.clear();
    frames = 0;
    if (!data || channels == 0 || channels > 8) {
        return false;
    }
    if (bytes < static_cast<size_t>(channels) * 4) {
        return false;
    }
    Rd rd{data, bytes};
    std::vector<ImaState> states(channels);
    for (u16 c = 0; c < channels; ++c) {
        states[c].predictor = static_cast<i16>(rd.u16_());
        states[c].index = static_cast<i16>(rd.u16_());
        if (states[c].index < 0 || states[c].index > 88) {
            return false;
        }
        // a 1ª amostra do canal é o próprio predictor (o encoder pô-la crua)
        out.push_back(static_cast<i16>(states[c].predictor));
    }
    // total alvo: expectedFrames*channels (0 = tudo o que houver nos nibbles)
    const u64 total = expectedFrames > 0 ? expectedFrames * channels
                                         : 0xFFFFFFFFFFFFFFFFull;
    const size_t nibbleBytes = bytes - static_cast<size_t>(channels) * 4;
    for (size_t b = 0; b < nibbleBytes && out.size() < total; ++b) {
        const u8 byte = data[static_cast<size_t>(channels) * 4 + b];
        // lo nibble (sempre uma amostra, se ainda falta)
        out.push_back(
            decodeNibble(states[out.size() % channels], byte & 0xF));
        // hi nibble (o último byte pode ter padding a 0)
        if (out.size() < total) {
            out.push_back(
                decodeNibble(states[out.size() % channels], byte >> 4));
        }
        if (out.size() > (1u << 28)) {
            return false;   // 268M amostras = lixo; aborta
        }
    }
    if (expectedFrames > 0 && out.size() != total) {
        return false;   // o payload NÃO produz as frames anunciadas
    }
    frames = channels > 0 ? out.size() / channels : 0;
    return true;
}

bool parseWavPcm16(const u8* bytes, size_t len, u32& sampleRate,
                   u16& channels, std::vector<i16>& out, std::string& err) {
    out.clear();
    sampleRate = 0;
    channels = 0;
    if (!bytes || len < 44) {
        err = "wav curto demais";
        return false;
    }
    if (std::memcmp(bytes, "RIFF", 4) != 0 ||
        std::memcmp(bytes + 8, "WAVE", 4) != 0) {
        err = "não é um RIFF/WAVE";
        return false;
    }
    size_t off = 12;
    bool haveFmt = false, haveData = false;
    u16 bits = 0;
    u16 audioFormat = 1;
    while (off + 8 <= len) {
        const char id[5] = {static_cast<char>(bytes[off]),
                            static_cast<char>(bytes[off + 1]),
                            static_cast<char>(bytes[off + 2]),
                            static_cast<char>(bytes[off + 3]), 0};
        const u32 size = static_cast<u32>(bytes[off + 4]) |
                         (static_cast<u32>(bytes[off + 5]) << 8) |
                         (static_cast<u32>(bytes[off + 6]) << 16) |
                         (static_cast<u32>(bytes[off + 7]) << 24);
        if (off + 8 + size > len) {
            err = "chunk wav truncado";
            return false;
        }
        if (std::strcmp(id, "fmt ") == 0 && size >= 16) {
            audioFormat = static_cast<u16>(bytes[off + 8] |
                                           (bytes[off + 9] << 8));
            channels = static_cast<u16>(bytes[off + 10] |
                                        (bytes[off + 11] << 8));
            sampleRate = static_cast<u32>(bytes[off + 12]) |
                         (static_cast<u32>(bytes[off + 13]) << 8) |
                         (static_cast<u32>(bytes[off + 14]) << 16) |
                         (static_cast<u32>(bytes[off + 15]) << 24);
            bits = static_cast<u16>(bytes[off + 22] |
                                    (bytes[off + 23] << 8));
            haveFmt = true;
        } else if (std::strcmp(id, "data") == 0) {
            if (!haveFmt) {
                err = "wav sem fmt antes do data";
                return false;
            }
            if (audioFormat != 1) {
                err = "wav não-PCM (format " + std::to_string(audioFormat) +
                      ") — usa PCM 16-bit";
                return false;
            }
            if (bits != 16) {
                err = "wav com " + std::to_string(bits) +
                      " bits — a engine importa PCM 16-bit";
                return false;
            }
            const size_t nSamples = size / 2;
            out.resize(nSamples);
            if (nSamples > 0) {
                std::memcpy(out.data(), bytes + off + 8, nSamples * 2);
            }
            haveData = true;
        }
        off += 8 + size + (size & 1);   // chunks alinhados a 2
    }
    if (!haveFmt || !haveData) {
        err = "wav sem fmt/data";
        return false;
    }
    if (channels < 1 || channels > 2) {
        err = "wav com " + std::to_string(channels) +
              " canais — mono/stereo apenas";
        return false;
    }
    if (sampleRate < 4000 || sampleRate > 192000) {
        err = "wav com sample rate invalido (" +
              std::to_string(sampleRate) + ")";
        return false;
    }
    return true;
}

// ---- contentor ---------------------------------------------------------------
bool writeGi(const GiWriteIn& in, std::vector<u8>& out, std::string& err) {
    out.clear();
    // payload codec-específico
    std::vector<u8> payload;
    if (in.codec == GiCodec::Adpcm) {
        if (!in.pcm16 || in.frames == 0) {
            err = "ADPCM sem PCM";
            return false;
        }
        if (!imaEncode(in.pcm16, in.frames, in.channels, payload)) {
            err = "codificacao ADPCM falhou";
            return false;
        }
    } else {
        if (!in.raw || in.rawSize == 0) {
            err = "passthrough sem bytes";
            return false;
        }
        payload.assign(in.raw, in.raw + in.rawSize);
    }
    // payload do contentor: metadados + dados
    std::vector<u8> body;
    body.push_back(static_cast<u8>(in.codec));
    put32(body, in.sampleRate);
    put16(body, in.channels);
    put64(body, in.frames);
    const u8 nameLen = in.name.size() > 255
        ? 255 : static_cast<u8>(in.name.size());
    body.push_back(nameLen);
    body.insert(body.end(), in.name.begin(),
                in.name.begin() + nameLen);
    body.insert(body.end(), payload.begin(), payload.end());
    // header comum (32 B) + body
    out.reserve(32 + body.size());
    const char magic[4] = {'G', 'I', 'C', 'L'};
    out.insert(out.end(), magic, magic + 4);
    put16(out, 1);          // version
    put16(out, 0x1A2B);     // endianMark
    put32(out, 8);          // align
    put64(out, body.size());
    const u64 checksum = gfnv1a(body.data(), body.size());
    put64(out, checksum);
    put32(out, 0);          // reserved
    out.insert(out.end(), body.begin(), body.end());
    return true;
}

void GiClip::peaks(u32 bars, std::vector<f32>& out) const {
    out.clear();
    if (bars == 0 || frames == 0) {
        return;
    }
    out.resize(bars, 0.0f);
    const u64 framesPerBar = (frames + bars - 1) / bars;
    for (u64 f = 0; f < frames; ++f) {
        const u32 bar = static_cast<u32>(f / framesPerBar);
        if (bar >= bars) {
            break;
        }
        for (u16 c = 0; c < channels; ++c) {
            const f32 v = std::fabs(pcm[static_cast<size_t>(f) * channels + c]);
            if (v > out[bar]) {
                out[bar] = v;
            }
        }
    }
}

bool readGi(const u8* bytes, size_t len, GiClip& out, std::string& err) {
    out = GiClip{};
    if (!bytes || len < 32) {
        err = ".gi curto demais";
        return false;
    }
    if (std::memcmp(bytes, "GICL", 4) != 0) {
        err = "magic errado — não é um .gi";
        return false;
    }
    Rd rd{bytes, len};
    rd.i = 4;
    const u16 version = rd.u16_();
    const u16 endian = rd.u16_();
    rd.u32_();   // align
    const u64 payloadSize = rd.u64_();
    const u64 checksum = rd.u64_();
    rd.u32_();   // reserved
    if (rd.bad) {
        err = ".gi header truncado";
        return false;
    }
    if (version != 1) {
        err = ".gi versão " + std::to_string(version) + " desconhecida";
        return false;
    }
    if (endian != 0x1A2B) {
        err = ".gi endianess trocada";
        return false;
    }
    if (payloadSize + 32 > len) {
        err = ".gi payload truncado";
        return false;
    }
    if (gfnv1a(bytes + 32, static_cast<size_t>(payloadSize)) != checksum) {
        err = ".gi CHECKSUM CORROMPIDO";
        return false;
    }
    Rd b{bytes + 32, static_cast<size_t>(payloadSize)};
    const u8 codecByte = b.u8_();
    if (codecByte > 2) {
        err = ".gi codec desconhecido (" + std::to_string(codecByte) + ")";
        return false;
    }
    out.codec = static_cast<GiCodec>(codecByte);
    out.sampleRate = b.u32_();
    out.channels = b.u16_();
    out.frames = b.u64_();
    const u8 nameLen = b.u8_();
    if (b.bad || out.channels < 1 || out.channels > 2 ||
        out.sampleRate < 4000 || out.sampleRate > 192000) {
        err = ".gi metadados invalidos";
        return false;
    }
    out.name.assign(reinterpret_cast<const char*>(bytes + 32 + b.i), nameLen);
    b.i += nameLen;
    const size_t dataLen = static_cast<size_t>(payloadSize) - b.i;
    const u8* data = bytes + 32 + b.i;
    // decode por codec → PCM f32
    std::vector<i16> pcm16;
    std::string derr;
    if (out.codec == GiCodec::Adpcm) {
        u64 frames = 0;
        if (!imaDecode(data, dataLen, out.channels, out.frames, pcm16,
                       frames)) {
            err = ".gi ADPCM corrompido";
            return false;
        }
        out.frames = frames;
        out.pcm.resize(pcm16.size());
        for (size_t i = 0; i < pcm16.size(); ++i) {
            out.pcm[i] = static_cast<f32>(pcm16[i]) / 32768.0f;
        }
    } else if (out.codec == GiCodec::Ogg) {
        if (!decodeOgg(data, dataLen, out.sampleRate, out.channels, out.pcm,
                       out.frames, derr)) {
            err = ".gi OGG falhou: " + derr;
            return false;
        }
    } else {
        if (!decodeMp3(data, dataLen, out.sampleRate, out.channels, out.pcm,
                       out.frames, derr)) {
            err = ".gi MP3 falhou: " + derr;
            return false;
        }
    }
    if (out.pcm.size() != static_cast<size_t>(out.frames) * out.channels) {
        err = ".gi frames não batem com o PCM";
        return false;
    }
    return true;
}

bool importAudioToGi(const u8* bytes, size_t len, const std::string& name,
                     GiImportOut& out, std::string& err) {
    out = GiImportOut{};
    if (!bytes || len == 0) {
        err = "fonte de audio vazia";
        return false;
    }
    out.sourceBytes = len;
    // codec pela extensão do NOME
    const size_t dot = name.rfind('.');
    std::string ext = dot == std::string::npos ? "" : name.substr(dot + 1);
    for (char& c : ext) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    const std::string stem = dot == std::string::npos
        ? name : name.substr(0, dot);
    GiWriteIn wi;
    wi.name = stem;
    if (ext == "wav") {
        std::vector<i16> pcm;
        if (!parseWavPcm16(bytes, len, wi.sampleRate, wi.channels, pcm, err)) {
            return false;
        }
        wi.codec = GiCodec::Adpcm;
        wi.pcm16 = pcm.data();
        wi.frames = wi.channels > 0 ? pcm.size() / wi.channels : 0;
        if (!writeGi(wi, out.gi, err)) {
            return false;
        }
        out.codec = GiCodec::Adpcm;
    } else if (ext == "ogg") {
        // passthrough: valida decodificando (o import não guarda lixo)
        GiClip probe;
        std::vector<u8> tmp;
        wi.codec = GiCodec::Ogg;
        wi.raw = bytes;
        wi.rawSize = len;
        wi.sampleRate = 44100;   // placeholder — o decode real diz
        wi.channels = 1;
        wi.frames = 0;
        // escreve com metadados PROVISÓRIOS, decodifica para aferir e
        // reescreve com os reais (o passthrough não encoda nada)
        u32 sr = 0;
        u16 ch = 0;
        std::vector<f32> pcm;
        u64 frames = 0;
        if (!decodeOgg(bytes, len, sr, ch, pcm, frames, err)) {
            return false;
        }
        wi.sampleRate = sr;
        wi.channels = ch;
        wi.frames = frames;
        if (!writeGi(wi, out.gi, err)) {
            return false;
        }
        out.codec = GiCodec::Ogg;
    } else if (ext == "mp3") {
        u32 sr = 0;
        u16 ch = 0;
        std::vector<f32> pcm;
        u64 frames = 0;
        if (!decodeMp3(bytes, len, sr, ch, pcm, frames, err)) {
            return false;
        }
        wi.codec = GiCodec::Mp3;
        wi.raw = bytes;
        wi.rawSize = len;
        wi.sampleRate = sr;
        wi.channels = ch;
        wi.frames = frames;
        if (!writeGi(wi, out.gi, err)) {
            return false;
        }
        out.codec = GiCodec::Mp3;
    } else {
        err = "audio ." + ext + " não suportado (aceites: .wav .ogg .mp3)";
        return false;
    }
    out.sampleRate = wi.sampleRate;
    out.channels = wi.channels;
    out.frames = wi.frames;
    out.duration = wi.sampleRate > 0
        ? static_cast<f32>(static_cast<f64>(wi.frames) / wi.sampleRate)
        : 0.0f;
    return true;
}

const char* giCodecName(GiCodec c) {
    switch (c) {
        case GiCodec::Adpcm: return "adpcm";
        case GiCodec::Ogg:   return "ogg";
        case GiCodec::Mp3:   return "mp3";
    }
    return "?";
}

} // namespace vv
