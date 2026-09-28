#pragma once
// render/Camera.h — câmara de orbit do viewport 3D (F2).
//
// Estado mínimo: target fixo (origem do interesse), yaw/pitch/dist derivam o
// eye em torno do target. Fonte da verdade = yaw/pitch/dist → clamp e
// determinismo triviais (exigência dos testes do CI Linux).
//
// Touch (via InputState da F1, integrado em main.cpp):
//   1 dedo  = orbit (yaw/pitch em torno de target)
//   pinch   = zoom (distância eye→target)
// Pitch é limitado para a câmara nunca inverter (up (0,1,0) degenera em ±90°).
#include "core/Types.h"
#include "math/Math.h"

namespace vv {

class Camera {
public:
    // limites — pitch ~±83° (não inverte), zoom entre 1.5 e 40 unidades
    static constexpr f32 kMinPitch  = -1.45f;
    static constexpr f32 kMaxPitch  =  1.45f;
    static constexpr f32 kMinDist   =  1.5f;
    static constexpr f32 kMaxDist   = 40.0f;

    Vec3 target{0.0f, 0.0f, 0.0f};
    f32  yaw   = 0.60f;    // rad, azimute em torno de Y
    f32  pitch = 0.50f;    // rad, elevação (clamp [kMinPitch, kMaxPitch])
    f32  dist  = 6.0f;     // eye→target (clamp [kMinDist, kMaxDist])
    f32  fovY  = 1.0472f;  // 60° em radianos
    f32  zNear = 0.1f;
    f32  zFar  = 200.0f;

    Vec3 eye() const;                 // target + dist * dir(yaw, pitch)
    Mat4 view() const;                // lookAt(eye(), target, up (0,1,0))
    Mat4 proj(f32 aspect) const;      // perspective(fovY, aspect, zNear, zFar)

    // rotação em radianos (a sensibilidade px→rad fica no input do main)
    void orbit(f32 dYaw, f32 dPitch);
    // fator multiplicativo da distância (>1 afasta, <1 aproxima)
    void zoomBy(f32 factor);
    void setDistance(f32 d);

    f32 pitchClamped(f32 p) const;
    f32 distClamped(f32 d) const;
};

} // namespace vv
