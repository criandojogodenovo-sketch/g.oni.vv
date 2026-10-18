// components/SkeletonComp.cpp — composição hierárquica das matrizes de skin.
#include "components/SkeletonComp.h"

namespace vv {

Mat4 jointLocalMatrix(const SkeletonComp::Joint& j) {
    const Mat4 rs = Mat4::mul(j.rot.toMat4(),
                              Mat4::scale(j.scale.x, j.scale.y, j.scale.z));
    return Mat4::mul(Mat4::translation(j.pos.x, j.pos.y, j.pos.z), rs);
}

u32 computeSkinMatrices(const SkeletonComp& sk, Mat4* out, u32 maxBones) {
    if (!out || sk.joints.empty()) {
        return 0;
    }
    const u32 n = static_cast<u32>(sk.joints.size());
    const u32 count = n < maxBones ? n : maxBones;   // cap do shader (64)
    // world[i] = world[parent] · TRS(i) — pais-primeiro: uma passada
    Mat4 world[SkeletonComp::kMaxBones];
    for (u32 i = 0; i < count; ++i) {
        const SkeletonComp::Joint& j = sk.joints[i];
        const Mat4 local = jointLocalMatrix(j);
        if (j.parent >= 0 && static_cast<u32>(j.parent) < i) {
            world[i] = Mat4::mul(world[static_cast<u32>(j.parent)], local);
        } else {
            world[i] = local;   // raiz (ou pai fora do cap — honesto)
        }
        // skin = world · inverseBind (no BIND: world = bind → skin = I)
        out[i] = Mat4::mul(world[i], j.inverseBind);
    }
    return count;
}

} // namespace vv
