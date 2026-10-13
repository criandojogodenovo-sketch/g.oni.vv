#pragma once
// render/Material.h — material mínimo do pipeline 3D (F2).
// Shader lit simples: difusa direcional FIXA no shader + ambient.
// PLACEHOLDER DE LUZ — luzes configuráveis são uma fase própria pós-F4.
// F5-E: o material ACEITA uma textura (albedo) opcional — quando presente,
// o shader multiplica o albedo pelo texel (uv do vértice, F5). Sem
// textura, o comportamento F2 é mantido byte a byte.
// 0.11-M (FASE 2): o shader lit ganha as PROPRIEDADES DO MATERIAL glTF —
// o baseColorFactor (multiplicativo), o alphaMode (OPAQUE/MASK com
// alphaCutoff/BLEND), o doubleSided (normal do verso), o emissiveFactor
// (a luz PBR GGX completa é a FASE 5 — o emissive é self-glow, não
// precisa de luz) e o KHR_texture_transform no slot baseColor.
#include "core/Types.h"
#include "math/Math.h"
#include <cmath>

namespace vv {

class Texture;   // render/Texture.h (F5-D) — recurso GL não-dono
struct GMeshMatProps;   // assets/GOwnFormats.h — as props do GTB2 (fwd)

// ---- 0.11-M (FASE 2): o transform UV do KHR_texture_transform (PURO) ------
// A fórmula OFICIAL da extensão: uv' = rotate(uv·scale, rotation) + offset
//   u' = cos(r)·sx·u − sin(r)·sy·v + tx
//   v' = sin(r)·sx·u + cos(r)·sy·v + ty
// Afervel sem GL (o pin do teste manda nestes DOIS — o shader tem de
// produzir O MESMO valor).
inline void khrTextureTransformApply(f32 u, f32 v, f32 rotation, f32 sx,
                                     f32 sy, f32 tx, f32 ty, f32 out[2]) {
    const f32 c = std::cos(rotation);
    const f32 s = std::sin(rotation);
    out[0] = c * sx * u - s * sy * v + tx;
    out[1] = s * sx * u + c * sy * v + ty;
}

// a MESMA matriz em COLUMN-MAJOR (o layout do glUniformMatrix2fv):
// col0 = (m00, m10), col1 = (m01, m11) com M aplicado como uv' = M·uv + t
inline void khrTextureTransformMat2(f32 rotation, f32 sx, f32 sy, f32 out[4]) {
    const f32 c = std::cos(rotation);
    const f32 s = std::sin(rotation);
    out[0] = c * sx;    // m00
    out[1] = s * sx;    // m10
    out[2] = -s * sy;   // m01
    out[3] = c * sy;    // m11
}

// 0.11-M (FASE 2) — as PROPRIEDADES de UM draw (por material do .gmesh).
// Defaults = o comportamento de SEMPRE (byte a byte: albedo branco,
// opaco, front-facing, sem emissive, uv identidade) — o caminho antigo
// passa nullptr e o shader produz o MESMO pixels de antes.
struct DrawProps {
    f32 baseColor[4] = {1.0f, 1.0f, 1.0f, 1.0f};
    u8  alphaMode = 0;        // 0=OPAQUE, 1=MASK (discard), 2=BLEND
    f32 alphaCutoff = 0.5f;
    bool twoSided = false;
    f32 emissive[3] = {0.0f, 0.0f, 0.0f};
    f32 uvRot[4] = {1.0f, 0.0f, 0.0f, 1.0f};   // mat2 column-major
    f32 uvOff[2] = {0.0f, 0.0f};

    // o SLOT 0 (baseColor) do GTB2 → DrawProps. Os factores
    // metallic/roughness e os transforms dos slots 1-4 esperam o lit
    // PBR da 0.11-L (documentado no relatório — o GTB2 já os transporta).
    static DrawProps fromGMesh(const GMeshMatProps& p);
};

class LitMaterial {
public:
    bool init();      // compila/linka o programa, cacheia uniforms
    void destroy();

    void use() const;                 // glUseProgram
    void setVP(const Mat4& vp) const;
    void setModel(const Mat4& model) const;
    // F5-E: liga a textura albedo (unit 0) e ativa uHasTex; nullptr desativa
    void setTexture(const Texture* tex) const;
    // 0.7.0 — COR POR TIC: tint multiplicativo da cor final (uTint).
    // nullptr OU {1,1,1} = comportamento 0.6.x byte a byte. A CADA draw
    // (uniforms GL nascem a 0 — nunca deixar o uTint por definir).
    void setTint(const f32 rgb[3]) const;

    // 0.8.2 (F7) — SKINNING: liga o ramo de bones do vertex shader (uSkin)
    // e faz upload das matrizes (uBones[64]); nullptr/0 desliga — o mesh
    // estático segue o caminho de sempre (atributos 3/4 não ligados).
    void setSkin(bool on) const;
    void setBones(const Mat4* bones, u32 count) const;

    // 0.11-M (FASE 2) — as props do material (A CADA draw, como o uTint —
    // uniforms nascem a 0 e o default É explícito: nullptr = o default
    // de sempre = o comportamento byte a byte dos draws pré-FASE 2)
    void setProps(const DrawProps* p) const;

    // tem de casar com o SkeletonComp (limite do array no shader)
    static constexpr u32 kMaxBones = 64;

    bool ok() const { return prog_ != 0; }

private:
    u32 prog_ = 0;
    i32 locVP_ = -1;
    i32 locModel_ = -1;
    i32 locTex_ = -1;
    i32 locHasTex_ = -1;
    i32 locTint_ = -1;   // 0.7.0: uTint (cor por TIC)
    i32 locSkin_ = -1;   // 0.8.2: uSkin (ramo de bones)
    i32 locBones_ = -1;  // 0.8.2: uBones[64]
    // 0.11-M (FASE 2): as props do material
    i32 locBaseColor_ = -1;   // uBaseColor (vec4)
    i32 locAlphaMode_ = -1;   // uAlphaMode (int)
    i32 locAlphaCutoff_ = -1; // uAlphaCutoff (float)
    i32 locTwoSided_ = -1;    // uTwoSided (int)
    i32 locEmissive_ = -1;    // uEmissive (vec3)
    i32 locUvRot_ = -1;       // uUvRot (mat2)
    i32 locUvOff_ = -1;       // uUvOff (vec2)
};

// F3: alias pedido pela spec — MeshRenderer guarda `Material*`. A F3 tem um
// único material (o lit da F2, intacto); quando surgir o segundo material
// (fase de luzes), Material vira base comum e o alias é removido.
using Material = LitMaterial;

} // namespace vv
