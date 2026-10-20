#include "render/Camera.h"
#include <cmath>

namespace vv {

Vec3 Camera::eye() const {
    const f32 cp = std::cos(pitch);
    const Vec3 dir{cp * std::sin(yaw), std::sin(pitch), cp * std::cos(yaw)};
    return target + dir * dist;
}

Mat4 Camera::view() const {
    static constexpr Vec3 kUp{0.0f, 1.0f, 0.0f};
    return Mat4::lookAt(eye(), target, kUp);
}

Mat4 Camera::proj(f32 aspect) const {
    // 0.8.9: nearZ/farZ são MEMBROS dinâmicos (setClips) — o far contém
    // sempre o AABB da cena (derivado pelo main), nunca um teto fixo.
    return Mat4::perspective(fovY, aspect, nearZ, farZ);
}

f32 Camera::pitchClamped(f32 p) const {
    if (p < kMinPitch) return kMinPitch;
    if (p > kMaxPitch) return kMaxPitch;
    return p;
}

f32 Camera::distClamped(f32 d) const {
    if (d < kMinDist) return kMinDist;
    if (d > kMaxDist) return kMaxDist;
    return d;
}

void Camera::orbit(f32 dYaw, f32 dPitch) {
    // arrastar para a direita gira a cena para a direita (câmara orbita no
    // sentido oposto); arrastar para baixo eleva a câmara
    yaw -= dYaw;
    pitch = pitchClamped(pitch + dPitch);
}

void Camera::zoomBy(f32 factor) {
    if (factor > 0.0f) {
        setDistance(dist * factor);
    }
}

// 0.12-Y (3.Y-A): o PAN — o TARGET desloca-se NO PLANO DA CÂMARA (o padrão
// do editor: agarrar a cena). A direção é a fonte única (camRightVec/
// camUpVec/camWorldPerPixel em Camera.h — os pins aferem a MESMA função):
//   • dxPx > 0 (dedo para a direita) → target anda para a ESQUERDA no
//     eixo right (a cena segue o dedo para a direita);
//   • dyPx > 0 (dedo para baixo — o y do ecrã cresce para baixo) → target
//     SOBE no camUp (a cena segue o dedo para baixo);
//   • a escala worldPerPixel = 2·dist·tan(fovY/2)/viewH mantém a
//     velocidade 1:1 com o dedo em QUALQUER distância/zoom;
//   • viewHPx ≤ 0 = degenerado (nada — nunca divide por zero).
void Camera::pan(f32 dxPx, f32 dyPx, f32 viewHPx) {
    const f32 wpp = camWorldPerPixel(dist, fovY, viewHPx);
    if (wpp <= 0.0f) {
        return;
    }
    target = target - camRightVec(yaw) * (dxPx * wpp);
    target = target + camUpVec(yaw, pitch) * (dyPx * wpp);
}

void Camera::setDistance(f32 d) {
    dist = distClamped(d);
}

} // namespace vv
