#include "render/Material.h"
#include "render/Texture.h"
#include "render/Pbr.h"   // 0.12-Y: a fonte única das fórmulas PBR-lite
#include "assets/GOwnFormats.h"   // 0.11-M: o GMeshMatProps do GTB2 (fromGMesh)
#include "platform/Log.h"
#include <GLES3/gl3.h>

namespace vv {

namespace {

// Luz direcional FIXA no shader (PLACEHOLDER — luzes configuráveis pós-F4).
// Tema mono: albedo cinza claro, ambient escuro — nada de cor.
// F5-E: aUV/vUV (location 2, F5) + uTex/uHasTex — a textura albedo é
// opcional: sem ela o resultado é IDÊNTICO ao F2 (uHasTex=0).
// 0.7.0: uTint — cor por TIC (gestão de TICs). Multiplicativo sobre a cor
// FINAL; default (1,1,1) definido a CADA draw (setTint) = resultado 0.6.x
// byte a byte. O tema mono fica INTACTO: a cor só muda quando o utilizador
// mexe nos sliders R/G/B do Inspector.
// 0.8.2 (F7): SKINNING no VERTEX SHADER — uSkin liga o ramo de bones;
// aJoints/aWeights (locations 3/4) só existem no VAO de um mesh SKINADO
// (mesh estático: atributos não ligados → valor constante, uSkin=0 ignora).
// A matriz efetiva é a soma ponderada das 4 influências (a MESMA conta do
// skinVertex de CPU — o teste de CI aferiu as duas contra o mesmo valor).
// 0.11-M (FASE 2): uUvRot/uUvOff — o KHR_texture_transform no slot
// baseColor (mat2 column-major + offset; identidade = o uv de sempre).
constexpr char kVsSrc[] = R"(#version 300 es
layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in vec2 aUV;
layout(location = 3) in vec4 aJoints;
layout(location = 4) in vec4 aWeights;
uniform mat4 uVP;
uniform mat4 uModel;
uniform int uSkin;
// 0.11-S (FASE 3): os bones vivem num UBO std140 — 128 mat4 = 8 KB cabem no
// mínimo garantido do ES 3.0 (MAX_UNIFORM_BLOCK_SIZE ≥ 16 KB); o array
// uniform antigo [64] não garantia espaço para 128 no limite mínimo.
layout(std140) uniform Bones { mat4 uBones[128]; };
// 0.11-W (3.5b): as matrizes POR INSTÂNCIA no MESMO padrão — 64 mat4 = 4 KB
// (o orçamento declarado do GE8320; acima, o chamador divide em draws).
// uInstOn = 0 (o de sempre) NUNCA lê o bloco — o ramo é morto e o caminho
// pré-3.5b é byte a byte.
uniform int uInstOn;
layout(std140) uniform Instances { mat4 uInst[64]; };
uniform mat2 uUvRot;
uniform vec2 uUvOff;
out vec3 vNormal;
out vec2 vUV;
out vec3 vWorldPos;   // 0.12-Y: a névoa/especular precisam da posição
void main() {
    vec4 pos = vec4(aPos, 1.0);
    vec3 nrm = aNormal;
    // 0.11-W (3.5b): a matriz efetiva da instância = uInst[gl_InstanceID] ×
    // uModel (§3.5b.3 — o model do TIC dono × a transformação do bloco no
    // .gmesh, que é identidade na geometria baked do import).
    mat4 m = uModel;
    if (uInstOn != 0) {
        m = uInst[gl_InstanceID] * uModel;
    }
    if (uSkin != 0) {
        mat4 b = uBones[int(aJoints.x)] * aWeights.x
               + uBones[int(aJoints.y)] * aWeights.y
               + uBones[int(aJoints.z)] * aWeights.z
               + uBones[int(aJoints.w)] * aWeights.w;
        pos = b * pos;
        nrm = mat3(b) * nrm;
    }
    vNormal = mat3(m) * nrm;
    vUV = uUvRot * aUV + uUvOff;
    vWorldPos = (m * pos).xyz;
    gl_Position = uVP * m * pos;
})";

// 0.11-M (FASE 2): uBaseColor (o factor multiplicativo do glTF), uEmissive
// (self-glow), uAlphaMode/uAlphaCutoff (MASK = discard abaixo do cutoff;
// BLEND = alpha no outColor) e uTwoSided (o verso com a normal invertida).
// 0.12-Y (3.Y-C): O LIT PBR-LITE — uNormalTex (unit 1) com a perturbação
// por DERIVADAS DE ECRÃ (dFdx/dFdy do ES 3.0 — a TBN sem atributos novos
// no mesh: o mesh antigo fica byte a byte), uRoughMetalTex (unit 2, o
// glTF empacota roughness no G e metallic no B), os FATORES
// uRoughness/uMetallic (o default glTF 1/1 — mas SÓ entram com texturas
// ou fatores explícitos; o caminho de sempre passa nullptr = os defaults
// preservam o lit antigo) e a NÉVOA (uFogColor/uFogDensity — a exp2 da
// fonte única render/Pbr.h; density 0 = morta = byte a byte).
constexpr char kFsSrc[] = R"(#version 300 es
precision mediump float;
in vec3 vNormal;
in vec2 vUV;
in vec3 vWorldPos;
out vec4 outColor;
uniform sampler2D uBaseColorTex;   // 0.12-Y-fix: o ALBEDO é o slot 0 — o nome é o contrato
uniform float uHasTex;
uniform vec3 uTint;
uniform vec4 uBaseColor;
uniform vec3 uEmissive;
uniform int uAlphaMode;
uniform float uAlphaCutoff;
uniform int uTwoSided;
uniform sampler2D uNormalTex;
uniform float uHasNormal;
uniform sampler2D uRoughMetalTex;
uniform float uHasRoughMetal;
uniform float uRoughness;
uniform float uMetallic;
uniform vec3 uFogColor;
uniform float uFogDensity;
uniform float uAmbientScale;   // o WorldEnvironment (o HDRI ilumina)
uniform vec3 uEye;
// 0.12-Y-E: a DIRECTIONAL do céu (o sol do ciclo dia/noite). O main manda
// SEMPRE: a identidade de sempre sem céu procedural (o kLightDir + branco
// = o lit byte a byte), o sol graduado com o ciclo ligado.
uniform vec3 uLightDir;   // PARA a luz (a direção do sol — sunDirFromTime)
uniform vec3 uLightCol;   // a cor JÁ com a intensidade (o luar à noite)
const vec3 kAmbient  = vec3(0.16);
const vec3 kAlbedo   = vec3(0.58);
void main() {
    vec3 n = normalize(vNormal);
    if (uTwoSided == 1 && !gl_FrontFacing) {
        n = -n;   // o verso de um material doubleSided ilumina com a normal dele
    }
    // 0.12-Y: o NORMAL MAP por derivadas de ecrã (a TBN do mesh sem
    // atributos tangent — a mesma conta do render/Pbr.h nas fontes puras)
    if (uHasNormal > 0.5) {
        vec3 tng = normalize(dFdx(vWorldPos));
        vec3 btg = normalize(cross(n, tng));
        tng = cross(btg, n);
        vec3 tn = texture(uNormalTex, vUV).xyz * 2.0 - 1.0;
        n = normalize(tng * tn.x + btg * tn.y + n * tn.z);
    }
    float rough = uRoughness;
    float metal = uMetallic;
    if (uHasRoughMetal > 0.5) {
        vec2 rm = texture(uRoughMetalTex, vUV).gb;   // o pacote glTF
        rough = rm.x;
        metal = rm.y;
    }
    float diff = max(dot(n, uLightDir), 0.0);
    vec4 texel = vec4(1.0);
    if (uHasTex > 0.5) {
        texel = texture(uBaseColorTex, vUV);
    }
    vec3 alb = kAlbedo * texel.rgb * uBaseColor.rgb;
    float alpha = uBaseColor.a * texel.a;
    if (uAlphaMode == 1 && alpha < uAlphaCutoff) {
        discard;   // MASK: abaixo do cutoff o fragmento NÃO EXISTE (depth intacto)
    }
    // 0.12-Y: o PBR-lite (as fórmulas da fonte única render/Pbr.h)
    float shin = 4.0 + (1.0 - clamp(rough, 0.0, 1.0)) * (1.0 - clamp(rough, 0.0, 1.0)) * 252.0;
    vec3 vdir = normalize(uEye - vWorldPos);
    vec3 h = normalize(uLightDir + vdir);
    float spec = pow(max(dot(n, h), 0.0), shin);
    float mdiff = 1.0 - clamp(metal, 0.0, 1.0) * 0.85;
    vec3 c = kAmbient * uAmbientScale + uLightCol * (alb * mdiff * diff + spec * mix(vec3(0.04), alb, metal)) + uEmissive;
    // a névoa exp2 (density 0 = morta — o byte a byte do lit de sempre)
    float fog = 1.0 - exp(-pow(length(uEye - vWorldPos) * uFogDensity, 2.0));
    c = mix(c, uFogColor, clamp(fog, 0.0, 1.0));
    outColor = vec4(c * uTint, uAlphaMode == 2 ? alpha : 1.0);
})";

GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = GL_FALSE;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[512];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        LOGE("LitMaterial: falha ao compilar shader: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

} // namespace

bool LitMaterial::init() {
    destroy();
    const GLuint vs = compileShader(GL_VERTEX_SHADER, kVsSrc);
    const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kFsSrc);
    if (!vs || !fs) {
        return false;
    }
    prog_ = glCreateProgram();
    glAttachShader(prog_, vs);
    glAttachShader(prog_, fs);
    glLinkProgram(prog_);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = GL_FALSE;
    glGetProgramiv(prog_, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE) {
        char log[512];
        glGetProgramInfoLog(prog_, sizeof(log), nullptr, log);
        LOGE("LitMaterial: falha ao linkar programa: %s", log);
        return false;
    }
    locVP_ = glGetUniformLocation(prog_, "uVP");
    locModel_ = glGetUniformLocation(prog_, "uModel");
    // 0.12-Y-fix (a ordem do dono): o albedo liga-se pelo nome do CONTRATO
    // uBaseColorTex (o slot 0 do PBR); o fallback uTex é o retro-guarda do
    // lit antigo — os dois caminhos chegam ao MESMO unit 0.
    locTex_ = glGetUniformLocation(prog_, "uBaseColorTex");
    if (locTex_ < 0) {
        locTex_ = glGetUniformLocation(prog_, "uTex");
    }
    locHasTex_ = glGetUniformLocation(prog_, "uHasTex");
    locTint_ = glGetUniformLocation(prog_, "uTint");
    locSkin_ = glGetUniformLocation(prog_, "uSkin");        // 0.8.2
    // 0.11-S (FASE 3): o bloco UBO dos bones — o INDEX (não location) e o
    // binding fixo do programa (o buffer entra por glBindBufferBase POR draw)
    boneBlockIdx_ = static_cast<i32>(glGetUniformBlockIndex(prog_, "Bones"));
    if (boneBlockIdx_ >= 0) {
        glUniformBlockBinding(prog_, static_cast<GLuint>(boneBlockIdx_),
                              kBoneBinding);
    }
    // 0.11-W (3.5b): o bloco das instâncias (binding 1 — o MESMO padrão)
    instBlockIdx_ = static_cast<i32>(glGetUniformBlockIndex(prog_, "Instances"));
    if (instBlockIdx_ >= 0) {
        glUniformBlockBinding(prog_, static_cast<GLuint>(instBlockIdx_),
                              kInstBinding);
    }
    locInstOn_ = glGetUniformLocation(prog_, "uInstOn");
    locBaseColor_ = glGetUniformLocation(prog_, "uBaseColor");   // 0.11-M
    locAlphaMode_ = glGetUniformLocation(prog_, "uAlphaMode");
    locAlphaCutoff_ = glGetUniformLocation(prog_, "uAlphaCutoff");
    locTwoSided_ = glGetUniformLocation(prog_, "uTwoSided");
    locEmissive_ = glGetUniformLocation(prog_, "uEmissive");
    locUvRot_ = glGetUniformLocation(prog_, "uUvRot");
    locUvOff_ = glGetUniformLocation(prog_, "uUvOff");
    // 0.12-Y (3.Y-C): o PBR-lite + a névoa (os locations — o -1 honesto
    // quando o link não os tem; o draw de sempre passa os defaults)
    locNormalTex_ = glGetUniformLocation(prog_, "uNormalTex");
    locHasNormal_ = glGetUniformLocation(prog_, "uHasNormal");
    locRoughMetalTex_ = glGetUniformLocation(prog_, "uRoughMetalTex");
    locHasRoughMetal_ = glGetUniformLocation(prog_, "uHasRoughMetal");
    locRoughness_ = glGetUniformLocation(prog_, "uRoughness");
    locMetallic_ = glGetUniformLocation(prog_, "uMetallic");
    locFogColor_ = glGetUniformLocation(prog_, "uFogColor");
    locFogDensity_ = glGetUniformLocation(prog_, "uFogDensity");
    locAmbientScale_ = glGetUniformLocation(prog_, "uAmbientScale");
    locEye_ = glGetUniformLocation(prog_, "uEye");
    // 0.12-Y-E: a directional do céu (os locations — o -1 honesto quando o
    // link não os tem)
    locLightDir_ = glGetUniformLocation(prog_, "uLightDir");
    locLightCol_ = glGetUniformLocation(prog_, "uLightCol");
    LOGI("LitMaterial: lit difusa fixa + ambient + uBaseColorTex (unit 0 — "
         "o albedo slot 0) + uTint "
         "+ skin (UBO Bones[128]) + instancing (UBO Instances[64]) + props "
         "do material glTF (alpha/factors/twoSided/uvTransform) pronto");
    return true;
}

void LitMaterial::destroy() {
    if (prog_) { glDeleteProgram(prog_); prog_ = 0; }
    if (uboBones_) { glDeleteBuffers(1, &uboBones_); uboBones_ = 0; }
    if (uboInst_) { glDeleteBuffers(1, &uboInst_); uboInst_ = 0; }
    locVP_ = locModel_ = locTex_ = locHasTex_ = locTint_ = -1;
    locSkin_ = -1;             // 0.8.2
    boneBlockIdx_ = -1;        // 0.11-S
    instBlockIdx_ = -1;        // 0.11-W (3.5b)
    locInstOn_ = -1;
    locBaseColor_ = locAlphaMode_ = locAlphaCutoff_ = locTwoSided_ = -1;
    locEmissive_ = locUvRot_ = locUvOff_ = -1;   // 0.11-M
    locNormalTex_ = locHasNormal_ = locRoughMetalTex_ = locHasRoughMetal_ = -1;
    locRoughness_ = locMetallic_ = -1;           // 0.12-Y
    locFogColor_ = locFogDensity_ = locEye_ = -1;
    locAmbientScale_ = -1;
    locLightDir_ = locLightCol_ = -1;   // 0.12-Y-E
}

void LitMaterial::use() const {
    glUseProgram(prog_);
}

void LitMaterial::setVP(const Mat4& vp) const {
    if (locVP_ >= 0) {
        glUniformMatrix4fv(locVP_, 1, GL_FALSE, vp.m);
    }
}

void LitMaterial::setModel(const Mat4& model) const {
    if (locModel_ >= 0) {
        glUniformMatrix4fv(locModel_, 1, GL_FALSE, model.m);
    }
}

// F5-E: liga a textura albedo (unit 0 — o uBaseColorTex) e ativa uHasTex;
// nullptr desativa — sem textura o shader produz EXATAMENTE o resultado
// da F2. 0.12-Y-fix: o bind do albedo É o slot baseColor (o unit 0 é o
// MESMO que o PBR-lite espera — o normal 1 e o rough/metal 2 nunca colidem).
void LitMaterial::setTexture(const Texture* tex) const {
    if (locHasTex_ >= 0) {
        glUniform1f(locHasTex_, tex ? 1.0f : 0.0f);
    }
    if (tex && locTex_ >= 0) {
        tex->bind(0);   // o albedo/baseColor no unit 0 (o contrato)
        glUniform1i(locTex_, 0);
    }
}

// 0.7.0 — cor por TIC: uTint multiplicativo. nullptr = branco (o
// comportamento de sempre — uniforms GL nascem a 0, por isso o default é
// definido EXPLICITAMENTE em cada draw, nunca deixado ao acaso).
void LitMaterial::setTint(const f32 rgb[3]) const {
    if (locTint_ < 0) {
        return;
    }
    if (rgb) {
        glUniform3f(locTint_, rgb[0], rgb[1], rgb[2]);
    } else {
        glUniform3f(locTint_, 1.0f, 1.0f, 1.0f);
    }
}

// 0.8.2 (F7) — ramo de bones: uSkin liga/desliga (A CADA draw, como o
// uTint — uniforms nascem a 0 e 0 = caminho estático, que é o default
// correto); setBones faz upload do bloco (cap 128 = kMaxBones do UBO).
void LitMaterial::setSkin(bool on) const {
    if (locSkin_ >= 0) {
        glUniform1i(locSkin_, on ? 1 : 0);
    }
}

// 0.11-S (FASE 3) — o upload dos bones via UBO: n matrizes + CAUDA de
// identidade até 128 (o upload é SEMPRE de 8 KB determinísticos — nenhum
// índice do mesh, nem corrupto, sai do bounds do array no shader; o custo
// por draw é de 8 KB, nada no orçamento de 10 personagens).
void LitMaterial::setBones(const Mat4* bones, u32 count) const {
    if (!bones || count == 0 || boneBlockIdx_ < 0) {
        return;
    }
    const u32 n = count < kMaxBones ? count : kMaxBones;
    if (uboBones_ == 0) {
        glGenBuffers(1, &uboBones_);
        glBindBuffer(GL_UNIFORM_BUFFER, uboBones_);
        glBufferData(GL_UNIFORM_BUFFER,
                     static_cast<GLintptr>(kMaxBones * 64), nullptr,
                     GL_DYNAMIC_DRAW);
    }
    static const Mat4 kIdentity = Mat4::identity();
    for (u32 i = 0; i < kMaxBones; ++i) {
        upload_[i] = i < n ? bones[i] : kIdentity;
    }
    glBindBuffer(GL_UNIFORM_BUFFER, uboBones_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0,
                    static_cast<GLintptr>(kMaxBones * 64), upload_[0].m);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, kBoneBinding, uboBones_);
}

// 0.11-W (3.5b) — o interruptor do ramo de instâncias (A CADA draw, como
// o uSkin: uniforms nascem a 0 e o default EXPLÍCITO é o caminho de sempre).
void LitMaterial::setInstancing(bool on) const {
    if (locInstOn_ >= 0) {
        glUniform1i(locInstOn_, on ? 1 : 0);
    }
}

// 0.11-W (3.5b) — o upload das instâncias via UBO: n matrizes + CAUDA de
// identidade até 64 (o MESMO contrato determinístico do setBones — o upload
// é SEMPRE 4 KB; nenhum índice do draw sai do bounds do array no shader).
void LitMaterial::setInstances(const Mat4* instances, u32 count) const {
    if (!instances || count == 0 || instBlockIdx_ < 0) {
        return;
    }
    const u32 n = count < kMaxInstances ? count : kMaxInstances;
    if (uboInst_ == 0) {
        glGenBuffers(1, &uboInst_);
        glBindBuffer(GL_UNIFORM_BUFFER, uboInst_);
        glBufferData(GL_UNIFORM_BUFFER,
                     static_cast<GLintptr>(kMaxInstances * 64), nullptr,
                     GL_DYNAMIC_DRAW);
    }
    static const Mat4 kIdentity = Mat4::identity();
    for (u32 i = 0; i < kMaxInstances; ++i) {
        uploadInst_[i] = i < n ? instances[i] : kIdentity;
    }
    glBindBuffer(GL_UNIFORM_BUFFER, uboInst_);
    glBufferSubData(GL_UNIFORM_BUFFER, 0,
                    static_cast<GLintptr>(kMaxInstances * 64),
                    uploadInst_[0].m);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glBindBufferBase(GL_UNIFORM_BUFFER, kInstBinding, uboInst_);
}

// 0.11-M (FASE 2) — as props A CADA draw (o mesmo contrato do uTint:
// uniforms nascem a 0 — o default É EXPLÍCITO, nunca estado de draw
// anterior a vingar). nullptr = o default de sempre (opaco, branco,
// front-facing, sem emissive, uv identidade) = o byte a byte pré-FASE 2.
void LitMaterial::setProps(const DrawProps* p) const {
    static const DrawProps kDefaults;
    const DrawProps& d = p ? *p : kDefaults;
    if (locBaseColor_ >= 0) {
        glUniform4f(locBaseColor_, d.baseColor[0], d.baseColor[1],
                    d.baseColor[2], d.baseColor[3]);
    }
    if (locAlphaMode_ >= 0) {
        glUniform1i(locAlphaMode_, static_cast<GLint>(d.alphaMode));
    }
    if (locAlphaCutoff_ >= 0) {
        glUniform1f(locAlphaCutoff_, d.alphaCutoff);
    }
    if (locTwoSided_ >= 0) {
        glUniform1i(locTwoSided_, d.twoSided ? 1 : 0);
    }
    if (locEmissive_ >= 0) {
        glUniform3f(locEmissive_, d.emissive[0], d.emissive[1], d.emissive[2]);
    }
    if (locUvRot_ >= 0) {
        glUniformMatrix2fv(locUvRot_, 1, GL_FALSE, d.uvRot);
    }
    if (locUvOff_ >= 0) {
        glUniform2f(locUvOff_, d.uvOff[0], d.uvOff[1]);
    }
}

// a conversão GTB2 → DrawProps (o SLOT 0/baseColor — o único que o lit de
// hoje amostra; os outros 4 esperam o lit PBR da 0.11-L, o GTB2 transporta)
DrawProps DrawProps::fromGMesh(const GMeshMatProps& p) {
    DrawProps d;
    d.alphaMode = p.alphaMode;
    d.alphaCutoff = p.alphaCutoff;
    d.twoSided = p.doubleSided != 0;
    for (int i = 0; i < 4; ++i) {
        d.baseColor[i] = p.baseColor[i];
    }
    for (int i = 0; i < 3; ++i) {
        d.emissive[i] = p.emissive[i];
    }
    d.metallic = p.metallic;       // 0.12-Y: os FATORES do GTB2 entram
    d.roughness = p.roughness;
    khrTextureTransformMat2(p.uvRotation[0], p.uvScale[0][0], p.uvScale[0][1],
                            d.uvRot);
    d.uvOff[0] = p.uvOffset[0][0];
    d.uvOff[1] = p.uvOffset[0][1];
    return d;
}

// ---- 0.12-Y (3.Y-C): o PBR-lite A CADA draw (o MESMO contrato do uTint/
// uProps — uniforms nascem a 0, o default É EXPLÍCITO: nullptr = o lit de
// sempre byte a byte: sem normal, sem rough/metal textura, fatores 1.0
// do glTF com o albedo difuso cheio — o comportamento pré-0.12-Y) ———
void LitMaterial::setPbr(const Texture* normalTex, const Texture* roughMetalTex,
                         f32 metallic, f32 roughness) const {
    if (locHasNormal_ >= 0) {
        glUniform1f(locHasNormal_, normalTex ? 1.0f : 0.0f);
    }
    if (normalTex && locNormalTex_ >= 0) {
        normalTex->bind(1);   // o normal no unit 1 (o albedo é o 0)
        glUniform1i(locNormalTex_, 1);
    }
    if (locHasRoughMetal_ >= 0) {
        glUniform1f(locHasRoughMetal_, roughMetalTex ? 1.0f : 0.0f);
    }
    if (roughMetalTex && locRoughMetalTex_ >= 0) {
        roughMetalTex->bind(2);   // o rough/metal no unit 2
        glUniform1i(locRoughMetalTex_, 2);
    }
    if (locRoughness_ >= 0) {
        glUniform1f(locRoughness_, roughness);
    }
    if (locMetallic_ >= 0) {
        glUniform1f(locMetallic_, metallic);
    }
}

void LitMaterial::setFog(const f32 rgb[3], f32 density, const Vec3& eye) const {
    if (locFogColor_ >= 0 && rgb) {
        glUniform3f(locFogColor_, rgb[0], rgb[1], rgb[2]);
    }
    if (locFogDensity_ >= 0) {
        glUniform1f(locFogDensity_, density);
    }
    if (locAmbientScale_ >= 0) {
        glUniform1f(locAmbientScale_, 1.0f);   // o default de sempre
    }
    if (locEye_ >= 0) {
        glUniform3f(locEye_, eye.x, eye.y, eye.z);
    }
}

// 0.12-Y: o AMBIENT do WorldEnvironment (o HDRI ilumina — o default de
// sempre é 1.0 e é definido a CADA setFog; este é o "boost" do ambiente)
void LitMaterial::setAmbientScale(f32 scale) const {
    if (locAmbientScale_ >= 0) {
        glUniform1f(locAmbientScale_, scale);
    }
}

// 0.12-Y-E: a DIRECTIONAL do céu (o sol do ciclo — a cor JÁ graduada com
// a intensidade; o par identidade é o kLightDir de sempre + branco)
void LitMaterial::setLight(const f32 dir[3], const f32 col[3]) const {
    if (locLightDir_ >= 0 && dir) {
        glUniform3f(locLightDir_, dir[0], dir[1], dir[2]);
    }
    if (locLightCol_ >= 0 && col) {
        glUniform3f(locLightCol_, col[0], col[1], col[2]);
    }
}

} // namespace vv

