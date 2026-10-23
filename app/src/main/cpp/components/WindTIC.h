#pragma once
// components/WindTIC.h — o TIC DE VENTO GLOBAL (0.12-Y-F2).
//
// A arquitetura do dono (a DECISÃO OBRIGATÓRIA da F2): o vento é UM
// componente de DADOS que o ParticleRenderer consome — ele NÃO renderiza
// nada. Antes do draw call das partículas, o main procura o 1.º WindTIC
// ATIVO da cena e passa `wind = windDirection * windStrength` como o
// uniform uWind ao particle shader (o desvio acumulado uWind·t no VS).
// SEM WindTIC na cena: uWind = (0,0,0) — as partículas de sempre byte a
// byte (a F1 intacta).
//
// Campos (o Inspector edita; o serializer grava):
//   windDirection — a direção do vento (NORMALIZADA no clampFields —
//                   o FS/puras só consomem direção; o zero cai no default)
//   windStrength  — a magnitude em unidades/s (0..10; 1.0 = a brisa da
//                   fábrica)
//
// GL-free: puras testáveis no CI (windVector).
#include "core/Component.h"
#include "core/Types.h"
#include <cmath>

namespace vv {

class WindTIC : public Component {
public:
    static constexpr f32 kStrengthDefault = 1.0f;
    static constexpr f32 kStrengthMax     = 10.0f;
    static constexpr f32 kDirDefault[3]   = {1.0f, 0.0f, 0.0f};

    f32 windDirection[3] = {kDirDefault[0], kDirDefault[1], kDirDefault[2]};
    f32 windStrength = kStrengthDefault;

    bool active = true;   // o 1.º ATIVO da cena manda (o padrão Environment)

    // a clamp de defesa (o padrão Environment::clampFields): a direção
    // NORMALIZA (o zero cai no default +x), a força vive em [0, kStrengthMax]
    void clampFields() {
        if (windStrength < 0.0f) windStrength = 0.0f;
        if (windStrength > kStrengthMax) windStrength = kStrengthMax;
        const f32 len = std::sqrt(windDirection[0] * windDirection[0] +
                                  windDirection[1] * windDirection[1] +
                                  windDirection[2] * windDirection[2]);
        if (len > 1e-6f) {
            windDirection[0] /= len;
            windDirection[1] /= len;
            windDirection[2] /= len;
        } else {
            windDirection[0] = kDirDefault[0];
            windDirection[1] = kDirDefault[1];
            windDirection[2] = kDirDefault[2];
        }
    }
};

// a FONTE PURA do vetor de vento (o draw e os pins usam a MESMA conta —
// o padrão Environment::ambientFromAvg): wind = dir * strength, com a
// direção JÁ normalizada pelo clampFields do componente
inline void windVector(const WindTIC& w, f32 out[3]) {
    out[0] = w.windDirection[0] * w.windStrength;
    out[1] = w.windDirection[1] * w.windStrength;
    out[2] = w.windDirection[2] * w.windStrength;
}

} // namespace vv
