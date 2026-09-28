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
    return Mat4::perspective(fovY, aspect, zNear, zFar);
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

void Camera::setDistance(f32 d) {
    dist = distClamped(d);
}

} // namespace vv
