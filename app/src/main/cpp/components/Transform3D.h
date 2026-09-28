#pragma once
// components/Transform3D.h — transform do TIC (F3, semântica Unreal).
//
// pos + rot (Quat) + scale. `world` é a matriz TRS cacheada, recalculada
// pelo TransformSystem a cada passo fixo (TickGroup::Update) e consumida
// pelo pass 3D via MeshRenderer. Edições do Inspector também atualizam
// `world` na hora (feedback imediato, sem esperar o próximo passo).
#include "core/Component.h"
#include "math/Math.h"

namespace vv {

class Transform3D : public Component {
public:
    Vec3 pos   = Vec3{0.0f, 0.0f, 0.0f};
    Quat rot   = Quat::identity();
    Vec3 scale = Vec3{1.0f, 1.0f, 1.0f};

    Mat4 world = Mat4::identity();   // cache do TRS — mantida por TransformSystem

    // TRS: v' = T·R·S·v (escala primeiro, depois rotação, depois translação).
    Mat4 computeMatrix() const {
        const Mat4 rs = Mat4::mul(rot.toMat4(),
                                  Mat4::scale(scale.x, scale.y, scale.z));
        return Mat4::mul(Mat4::translation(pos.x, pos.y, pos.z), rs);
    }

    // Recalcula e guarda o cache (usado por TransformSystem e pelo Inspector).
    void updateWorld() { world = computeMatrix(); }
};

} // namespace vv
