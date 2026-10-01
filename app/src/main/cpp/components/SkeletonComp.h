#pragma once
// components/SkeletonComp.h — ESQUELETO de skinning (0.8.2, F7).
//
// Importado de um glTF "skin" (joints + inverseBindMatrices): cada joint é
// um TRS LOCAL animável (pelos tracks JointPos/JointRot/JointScale do
// AnimationPlayer, por NOME de joint em `element`) + a inverseBind do glTF.
// Os joints vivem PAES PRIMEIRO (o importer reordena) — a composição
// hierárquica do computeSkinMatrices é ITERATIVA (uma passada).
//
// O RENDER: o main chama computeSkinMatrices por frame (para TICs com
// MeshRenderer de mesh SKINADO) e passa as matrizes ao Renderer/LitMaterial
// (uniform uBones[64] + uSkin — o shader pesa os 4 influências por vértice
// no VERTEX SHADER; a CPU só compõe as matrizes). O LIMITE de 64 bones é o
// do shader (glTF mobile típico: <60; cap documentado).
//
// SERIALIZAÇÃO (.goni): componente "Skeleton" com os joints por valor
// (nome/pai/pos/rot/scale/ibm[16]) — os JOINTS_0/WEIGHTS_0 vivem no mesh
// (ficheiro glb), as matrizes de BIND aqui.
//
// GL-free / host-testável: computeSkinMatrices/skinVertex são puras.
#include "core/Component.h"
#include "math/Math.h"
#include <string>
#include <vector>

namespace vv {

class SkeletonComp : public Component {
public:
    struct Joint {
        std::string name;    // alvo dos tracks (JointPos/Rot/Scale)
        i32  parent = -1;    // índice dentro de joints (-1 = raiz)
        Vec3 pos{};          // TRS LOCAL (animável — o apply escreve aqui)
        Quat rot = Quat::identity();
        Vec3 scale{1.0f, 1.0f, 1.0f};
        // 0.8.3 (F7): TRS de BIND — o alvo do fade quando um blend não tem
        // track para o joint (sem isto o "bind" era o valor ANIMADO e o
        // fade congelava em vez de descansar). O attach copia o TRS do nó;
        // o .goni grava ambos (bp/br/bs; ausentes = o TRS atual — 0.8.2).
        Vec3 bindPos{};
        Quat bindRot = Quat::identity();
        Vec3 bindScale{1.0f, 1.0f, 1.0f};
        Mat4 inverseBind = Mat4::identity();   // do glTF
    };

    static constexpr u32 kMaxBones = 64;   // tem de casar com o shader (uBones)

    std::vector<Joint> joints;

    // índice do PRIMEIRO joint com o nome (-1 se não existe)
    i32 findJoint(const std::string& name) const {
        for (size_t i = 0; i < joints.size(); ++i) {
            if (joints[i].name == name) {
                return static_cast<i32>(i);
            }
        }
        return -1;
    }
};

// Composição hierárquica: world[i] = world[parent]·TRS(i) (uma passada —
// joints pais-primeiro); skin[i] = world[i]·inverseBind[i].
// Devolve o número de matrizes escritas (0 se sem joints/acima do cap).
u32 computeSkinMatrices(const SkeletonComp& sk, Mat4* out, u32 maxBones);

// TRS de um joint (a mesma receita do Transform3D)
Mat4 jointLocalMatrix(const SkeletonComp::Joint& j);

// Aplica a skin a um vértice (4 influências) — a MESMA conta do shader,
// em CPU, para o CI aferir que a POSE altera VÉRTICES.
inline Vec3 skinVertex(const Vec3& pos, const u8 joints[4], const f32 weights[4],
                       const Mat4* bones, u32 boneCount) {
    Vec3 acc{0.0f, 0.0f, 0.0f};
    for (int c = 0; c < 4; ++c) {
        if (weights[c] == 0.0f) {
            continue;
        }
        const u32 b = joints[c];
        if (b >= boneCount) {
            continue;   // índice fora (mesh de outra skin) — ignora o peso
        }
        const Mat4& t = bones[b];
        const Vec3 p{t.m[0] * pos.x + t.m[4] * pos.y + t.m[8] * pos.z + t.m[12],
                     t.m[1] * pos.x + t.m[5] * pos.y + t.m[9] * pos.z + t.m[13],
                     t.m[2] * pos.x + t.m[6] * pos.y + t.m[10] * pos.z + t.m[14]};
        acc = acc + p * weights[c];
    }
    return acc;
}

} // namespace vv
