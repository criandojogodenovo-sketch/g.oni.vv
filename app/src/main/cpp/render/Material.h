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
    f32 metallic = 1.0f;      // 0.12-Y (3.Y-C): os FATORES do GTB2 (o
    f32 roughness = 1.0f;     //   default glTF; o FS mistura pelo PBR-lite)

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
    // e faz upload das matrizes; nullptr/0 desliga — o mesh estático segue o
    // caminho de sempre (atributos 3/4 não ligados). 0.11-S (FASE 3): as
    // matrizes vão num UBO "Bones" (std140, 128 mat4 = 8 KB) — glBindBuffer-
    // Base + glBufferSubData POR draw (o array uniform antigo não garantia
    // espaço no mínimo ES 3.0 para 128).
    void setSkin(bool on) const;
    void setBones(const Mat4* bones, u32 count) const;

    // 0.11-M (FASE 2) — as props do material (A CADA draw, como o uTint —
    // uniforms nascem a 0 e o default É explícito: nullptr = o default
    // de sempre = o comportamento byte a byte dos draws pré-FASE 2)
    void setProps(const DrawProps* p) const;

    // 0.12-Y (3.Y-C) — O PBR-LITE A CADA draw: o normal map (unit 1) + o
    // rough/metal (unit 2, o pacote glTF roughness no G metallic no B) +
    // os fatores. nullptr nas texturas = os ramos mortos (o lit de
    // sempre). A névoa/olho em setFog (o WorldEnvironment injeta).
    void setPbr(const Texture* normalTex, const Texture* roughMetalTex,
                f32 metallic, f32 roughness) const;
    // a névoa exp2 + a posição do olho (a fonte única render/Pbr.h;
    // density 0 = morta = o byte a byte do lit de sempre)
    void setFog(const f32 rgb[3], f32 density, const Vec3& eye) const;
    // o multiplicador do ambient (o HDRI do WorldEnvironment; o setFog
    // repõe 1.0 — o boost vive SÓ no frame com Environment ativo)
    void setAmbientScale(f32 scale) const;
    // 0.12-Y-E — a DIRECTIONAL do CÉU (o sol do ciclo dia/noite): a direção
    // PARA a luz + a cor JÁ com a intensidade (o luar azul fraco à noite,
    // o alaranjado ao pôr do sol). O setFog repõe a IDENTIDADE (o kLightDir
    // de sempre + cor branca = o lit byte a byte sem Environment procedural).
    void setLight(const f32 dir[3], const f32 col[3]) const;

    // tem de casar com o SkeletonComp (limite do bloco UBO no shader)
    static constexpr u32 kMaxBones = 128;   // 0.11-S: 128 (era 64)

    // 0.11-W (3.5b) — GPU INSTANCING: o teto de matrizes por draw (bloco
    // std140 de 64 × 64 B = 4 KB — o orçamento declarado do GE8320; acima
    // disto o chamador divide em múltiplos draws, nunca falha). As matrizes
    // vão no UBO "Instances" (binding 1 — o Bones é o 0) e o vertex shader
    // indexa uInst[gl_InstanceID] quando uInstOn != 0 — SEM atributos
    // divisor (o padrão do UBO Bones da casa, provado no GE8320 pela 0.11-S).
    static constexpr u32 kMaxInstances = 64;

    // 0.11-W (3.5b) — o interruptor do ramo de instâncias (A CADA draw,
    // como o uTint/uSkin — uniforms nascem a 0 e o draw de sempre define
    // EXPLICITAMENTE false; o estado nunca vinga de um draw anterior).
    void setInstancing(bool on) const;
    // o upload do bloco (n matrizes + cauda de identidade até 64 — o MESMO
    // contrato determinístico do setBones: o upload é SEMPRE 4 KB).
    void setInstances(const Mat4* instances, u32 count) const;

    bool ok() const { return prog_ != 0; }

private:
    u32 prog_ = 0;
    i32 locVP_ = -1;
    i32 locModel_ = -1;
    i32 locTex_ = -1;    // 0.12-Y-fix: uBaseColorTex (fallback uTex) — unit 0
    i32 locHasTex_ = -1;
    i32 locTint_ = -1;   // 0.7.0: uTint (cor por TIC)
    i32 locSkin_ = -1;   // 0.8.2: uSkin (ramo de bones)
    i32 locInstOn_ = -1; // 0.11-W (3.5b): uInstOn (ramo das instâncias)
    // 0.11-S (FASE 3): o bloco de bones — o INDEX do bloco no programa
    // (glGetUniformBlockIndex; <0 = shader sem skin) + o buffer UBO criado
    // à 1.ª upload (lazy — meshes estáticos nunca pagam o glBufferData)
    i32 boneBlockIdx_ = -1;   // 0.11-S: índice do bloco "Bones" (era locBones_)
    mutable u32 uboBones_ = 0; // criado lazy no 1.º setBones (const-foo)
    mutable Mat4 upload_[128]; // 0.11-S: o palco do upload (n bones + cauda
                               // de identidade — 8 KB determinísticos)
    static constexpr u32 kBoneBinding = 0;   // o slot de binding do UBO
    // 0.11-W (3.5b): o bloco das instâncias (o MESMO padrão do Bones — o
    // índice do bloco no programa + o buffer UBO criado à 1.ª upload lazy)
    i32 instBlockIdx_ = -1;    // índice do bloco "Instances"
    mutable u32 uboInst_ = 0;  // criado lazy no 1.º setInstances
    mutable Mat4 uploadInst_[64]; // o palco do upload (n instâncias + cauda
                                  // de identidade — 4 KB determinísticos)
    static constexpr u32 kInstBinding = 1;   // o slot de binding (Bones = 0)
    // 0.11-M (FASE 2): as props do material
    i32 locBaseColor_ = -1;   // uBaseColor (vec4)
    i32 locAlphaMode_ = -1;   // uAlphaMode (int)
    i32 locAlphaCutoff_ = -1; // uAlphaCutoff (float)
    i32 locTwoSided_ = -1;    // uTwoSided (int)
    i32 locEmissive_ = -1;    // uEmissive (vec3)
    i32 locUvRot_ = -1;       // uUvRot (mat2)
    i32 locUvOff_ = -1;       // uUvOff (vec2)
    // 0.12-Y (3.Y-C): o PBR-lite + a névoa
    i32 locNormalTex_ = -1;       // uNormalTex (unit 1)
    i32 locHasNormal_ = -1;       // uHasNormal
    i32 locRoughMetalTex_ = -1;   // uRoughMetalTex (unit 2)
    i32 locHasRoughMetal_ = -1;   // uHasRoughMetal
    i32 locRoughness_ = -1;       // uRoughness
    i32 locMetallic_ = -1;        // uMetallic
    i32 locFogColor_ = -1;        // uFogColor (vec3)
    i32 locFogDensity_ = -1;      // uFogDensity (float)
    i32 locAmbientScale_ = -1;    // uAmbientScale (float — o HDRI)
    i32 locEye_ = -1;             // uEye (vec3)
    i32 locLightDir_ = -1;        // 0.12-Y-E: uLightDir (vec3 — o sol)
    i32 locLightCol_ = -1;        // 0.12-Y-E: uLightCol (vec3 — cor/intens.)
};

// F3: alias pedido pela spec — MeshRenderer guarda `Material*`. A F3 tem um
// único material (o lit da F2, intacto); quando surgir o segundo material
// (fase de luzes), Material vira base comum e o alias é removido.
using Material = LitMaterial;

} // namespace vv
