#pragma once
// components/Environment.h — o TIC WORLDENVIRONMENT (0.12-Y 3.Y-C).
//
// A ordem do dono: «Novo preset no "+": cria um TIC que permite carregar
// um HDRI (mapa 360) para iluminar a cena e definir a cor do céu/névoa
// (fog).»
//
// O componente é DADOS (o padrão AudioPlayer — "estrutura primeiro"): o
// main lê o 1.º Environment ATIVO por frame e aplica:
//   • o CLEAR da cena = skyColor (o céu);
//   • a névoa exp2 no lit (uFogColor/uFogDensity — a fonte única
//     render/Pbr.h; density 0 = morta = o byte a byte do lit de sempre);
//   • o AMBIENT escala pelo multiplicador (o HDRI "ilumina" pela média
//     CPU — ambientFromAvg; 1.0 = o kAmbient de sempre).
//
// O HDRI: hdriPath é um ref relativo (o equirectangular 2:1); a MÉDIA CPU
// (averageColor abaixo) alimenta o ambient — o fundo visual do céu é o
// skyColor (a leitura honesta do lit-lite: "iluminar a cena" = a média,
// "a cor do céu" = o clear).
//
// GL-free: puras testáveis no CI (averageColor + ambientFromAvg).
#include "core/Component.h"
#include "core/Types.h"
#include <string>

namespace vv {

class Environment : public Component {
public:
    f32 skyColor[4] = {0.040f, 0.045f, 0.065f, 1.0f};  // o céu (o clear)
    f32 fogColor[4] = {0.040f, 0.045f, 0.065f, 1.0f};  // a névoa
    f32 fogDensity = 0.0f;    // 0 = MORTA (o lit de sempre — o contrato)
    f32 ambient = 1.0f;       // o multiplicador do ambient (o HDRI escala)
    std::string hdriPath;     // o ref do equirect ("" = sem — o céu é a cor)
    bool active = true;       // o 1.º ATIVO da cena manda

    // a clamp dos campos (o padrão AudioPlayer::clampFields)
    void clampFields() {
        if (fogDensity < 0.0f) fogDensity = 0.0f;
        if (fogDensity > 1.0f) fogDensity = 1.0f;
        if (ambient < 0.0f) ambient = 0.0f;
        if (ambient > 8.0f) ambient = 8.0f;
        for (int i = 0; i < 4; ++i) {
            if (skyColor[i] < 0.0f) skyColor[i] = 0.0f;
            if (skyColor[i] > 1.0f) skyColor[i] = 1.0f;
            if (fogColor[i] < 0.0f) fogColor[i] = 0.0f;
            if (fogColor[i] > 1.0f) fogColor[i] = 1.0f;
        }
    }
};

// ---- as FONTES PURAS (o padrão render/Pbr.h — o draw e os pins usam a
// MESMA conta) ------------------------------------------------------------

// a média ARGB dos pixels (o downsample do HDRI: a cada N-ésimo pixel —
// o passo dá no máx ~4096 amostras; RGB 0..1). rgba = 4 canais u8.
inline void averageColor(const u8* rgba, u32 w, u32 h, f32 out[3]) {
    out[0] = out[1] = out[2] = 0.0f;
    if (!rgba || w == 0 || h == 0) {
        return;
    }
    const u64 total = static_cast<u64>(w) * h;
    u64 step = total / 4096u + 1u;   // o teto de amostras (o C33 paga 4k)
    u64 n = 0;
    for (u64 i = 0; i < total; i += step) {
        const u8* p = rgba + i * 4u;
        out[0] += p[0] / 255.0f;
        out[1] += p[1] / 255.0f;
        out[2] += p[2] / 255.0f;
        ++n;
    }
    if (n > 0) {
        out[0] /= static_cast<f32>(n);
        out[1] /= static_cast<f32>(n);
        out[2] /= static_cast<f32>(n);
    }
}

// o multiplicador do ambient a partir da LUMINÂNCIA média do HDRI (o
// padrão honesto: um HDRI escuro ilumina menos — a luminância relativa
// (0.2126, 0.7152, 0.0722) × kHdriGain, o piso 0.25 nunca apaga a cena)
inline f32 ambientFromAvg(const f32 avg[3]) {
    const f32 lum = 0.2126f * avg[0] + 0.7152f * avg[1] + 0.0722f * avg[2];
    const f32 v = lum * 4.0f;   // o ganho (um HDRI médio ~0.3 → 1.2)
    return v < 0.25f ? 0.25f : (v > 4.0f ? 4.0f : v);
}

} // namespace vv
