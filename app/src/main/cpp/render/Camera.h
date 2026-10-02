#pragma once
// render/Camera.h — câmara de orbit do viewport 3D (F2; clamps generosos F3.1;
// ESPAÇO SEM TETOS 0.8.9).
//
// Estado mínimo: target fixo (origem do interesse), yaw/pitch/dist derivam o
// eye em torno do target. Fonte da verdade = yaw/pitch/dist → clamp e
// determinismo triviais (exigência dos testes do CI Linux).
//
// Touch (via InputState da F1, integrado em main.cpp):
//   1 dedo  = orbit (yaw/pitch em torno de target)
//   pinch   = zoom (distância eye→target)
//
// 0.8.9 (prompt "ESPAÇO SEM TETOS"):
//   • zoom 0.01 → 100 000 (era 1..300): ver o modelo inteiro de longe ou
//     aproximar ao detalhe — o clamp só existe para o float não degenerar;
//   • near/far DINÂMICOS: deixaram de ser constantes de compilação — o main
//     recalcula-os por frame a partir da distância e do AABB da cena com
//     margem (setClips). Defaults mantêm o comportamento antigo (0.5/450)
//     para quem (tests incluídos) não chama setClips.
//   • NOTA DOCUMENTADA: jitter de float32 acima de ~100 000 unidades é
//     limite conhecido; origin rebasing é feature FUTURA (não implementar).
#include "core/Types.h"
#include "math/Math.h"

namespace vv {

class Camera {
public:
    // limites — pitch ~±89° (não inverte: |pitch| < 90°)
    static constexpr f32 kMinPitch  = -1.5533430f;   // −89° em radianos
    static constexpr f32 kMaxPitch  =  1.5533430f;   //  89° em radianos
    // 0.8.9: zoom amplo — 0.01 (perto do detalhe) até 100 000 (horizonte).
    static constexpr f32 kMinDist   = 0.01f;
    static constexpr f32 kMaxDist   = 100000.0f;

    // 0.8.9: near/far DINÂMICOS (setClips). Defaults = valores F3.1 (o
    // rácio 900:1 anti-z-fighting do grid de sempre). O main deriva-os por
    // frame: near = clamp(dist*0.05, 0.01, …), far = max(dist*1.5,
    // diagonalDaCena*2+10, 450) — o far SEMPRE contém o AABB da cena.
    static constexpr f32 kDefaultNear = 0.5f;
    static constexpr f32 kDefaultFar  = 450.0f;

    Vec3 target{0.0f, 0.0f, 0.0f};
    f32  yaw   = 0.60f;    // rad, azimute em torno de Y (livre)
    f32  pitch = 0.50f;    // rad, elevação (clamp [kMinPitch, kMaxPitch])
    f32  dist  = 6.0f;     // eye→target (clamp [kMinDist, kMaxDist])
    f32  fovY  = 1.0472f;  // 60° em radianos
    f32  nearZ = kDefaultNear;   // 0.8.9: dinâmico (setClips)
    f32  farZ  = kDefaultFar;    // 0.8.9: dinâmico (setClips)

    Vec3 eye() const;                 // target + dist * dir(yaw, pitch)
    Mat4 view() const;                // lookAt(eye(), target, up (0,1,0))
    Mat4 proj(f32 aspect) const;      // perspective(fovY, aspect, nearZ, farZ)

    // 0.8.9: define os planos de corte dinâmicos (chamado por frame pelo
    // main com dist + AABB da cena; determinístico — os testes aferem).
    void setClips(f32 nearClip, f32 farClip) {
        nearZ = nearClip > 0.0f ? nearClip : kDefaultNear;
        farZ  = farClip > nearZ ? farClip : nearZ * 2.0f;
    }

    // rotação em radianos (a sensibilidade px→rad fica no input do main)
    void orbit(f32 dYaw, f32 dPitch);
    // fator multiplicativo da distância (>1 afasta, <1 aproxima)
    void zoomBy(f32 factor);
    void setDistance(f32 d);

    f32 pitchClamped(f32 p) const;
    f32 distClamped(f32 d) const;
};

} // namespace vv
