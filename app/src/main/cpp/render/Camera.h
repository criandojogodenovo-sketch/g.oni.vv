#pragma once
// render/Camera.h — câmara de orbit do viewport 3D (F2; clamps generosos F3.1).
//
// Estado mínimo: target fixo (origem do interesse), yaw/pitch/dist derivam o
// eye em torno do target. Fonte da verdade = yaw/pitch/dist → clamp e
// determinismo triviais (exigência dos testes do CI Linux).
//
// Touch (via InputState da F1, integrado em main.cpp):
//   1 dedo  = orbit (yaw/pitch em torno de target)
//   pinch   = zoom (distância eye→target)
// F3.1: clamps generosos pedidos no device — zoom 1..300 (era 1.5..40), pitch
// ±89° (era ±83°: top-down quase total sem degenerar o lookAt com up (0,1,0)),
// yaw livre. near/far recalibrados contra z-fighting: rácio far/near 900:1,
// confortável para o depth de 24 bits.
#include "core/Types.h"
#include "math/Math.h"

namespace vv {

class Camera {
public:
    // limites F3.1 — pitch ~±89° (não inverte: |pitch| < 90°), zoom 1..300
    static constexpr f32 kMinPitch  = -1.5533430f;   // −89° em radianos
    static constexpr f32 kMaxPitch  =  1.5533430f;   //  89° em radianos
    static constexpr f32 kMinDist   =  1.0f;
    static constexpr f32 kMaxDist   =  300.0f;

    // F3.1: near/far contra z-fighting — far = zoom máx × 1.5 = 450; rácio
    // 900:1 preserva a precisão do depth buffer de 24 bits (grid a y=0 e
    // bases de TICs a y=0 coexistem sem faíscas).
    static constexpr f32 zNear = 0.5f;
    static constexpr f32 zFar  = kMaxDist * 1.5f;    // 450.0f

    Vec3 target{0.0f, 0.0f, 0.0f};
    f32  yaw   = 0.60f;    // rad, azimute em torno de Y (livre)
    f32  pitch = 0.50f;    // rad, elevação (clamp [kMinPitch, kMaxPitch])
    f32  dist  = 6.0f;     // eye→target (clamp [kMinDist, kMaxDist])
    f32  fovY  = 1.0472f;  // 60° em radianos

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
