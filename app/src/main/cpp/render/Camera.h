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
#include <cmath>

namespace vv {

// ---- 0.12-Y (3.Y-A): A FONTE ÚNICA da matemática do PAN ---------------------
// PURAS (o padrão gridFadeForDist — desenha e os pins aferem a MESMA
// função): os eixos do PLANO DA CÂMARA + a escala px→mundo no foco.
//   right = (cos yaw, 0, −sin yaw) — horizontal, ortogonal ao yaw;
//   camUp = cross(right, forward) = (−sp·sy, cp, −sp·cy) — o "cima do
//   ecrã" projetado no mundo (a pitch ±89° fica horizontal — top-down);
//   worldPerPixel = 2·dist·tan(fovY/2)/viewH — o tamanho real de 1 px no
//   plano do target (o pan à velocidade do dedo em QUALQUER zoom).
inline f32 camWorldPerPixel(f32 dist, f32 fovY, f32 viewHPx) {
    return viewHPx > 0.0f
               ? 2.0f * dist * std::tan(fovY * 0.5f) / viewHPx
               : 0.0f;
}
inline Vec3 camRightVec(f32 yaw) {
    return Vec3{std::cos(yaw), 0.0f, -std::sin(yaw)};
}
inline Vec3 camUpVec(f32 yaw, f32 pitch) {
    const f32 cp = std::cos(pitch);
    const f32 sp = std::sin(pitch);
    return Vec3{-sp * std::sin(yaw), cp, -sp * std::cos(yaw)};
}

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
    // 0.12-Y (3.Y-A): o PAN — desloca o TARGET no plano da câmara (agarra
    // a cena): dxPx para a direita empurra a cena para a direita (o alvo
    // anda para a ESQUERDA no eixo right); dyPx para baixo (o y do ecrã
    // cresce para baixo) empurra a cena para baixo (o alvo SOBE no camUp).
    // A escala px→mundo é a fonte única camWorldPerPixel (a velocidade do
    // dedo constante em QUALQUER zoom/distância). viewHPx ≤ 0 = nada.
    void pan(f32 dxPx, f32 dyPx, f32 viewHPx);

    f32 pitchClamped(f32 p) const;
    f32 distClamped(f32 d) const;
};

} // namespace vv
