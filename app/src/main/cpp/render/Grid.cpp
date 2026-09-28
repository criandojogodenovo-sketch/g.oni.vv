#include "render/Grid.h"
#include "platform/Log.h"
#include <GLES3/gl3.h>

namespace vv {

namespace {

// Quad do grid: 4 vértices (triangle strip), cantos em [-1,1]². O XZ mundial
// real = XZ da câmara + canto × extent (uniform) — o quad SEGUE a câmara,
// mas as linhas são ancoradas ao MUNDO (coords absolutas no fragment), por
// isso o padrão não desliza quando a câmara anda.
constexpr f32 kQuadCorners[4][2] = {
    {-1.0f, -1.0f}, {1.0f, -1.0f}, {-1.0f, 1.0f}, {1.0f, 1.0f},
};

constexpr char kVsSrc[] = R"(#version 300 es
layout(location = 0) in vec2 aCorner;
uniform mat4 uVP;
uniform vec3 uCenter;    // XZ da câmara (plano y=0)
uniform float uExtent;   // meia-extensão do quad em unidades de mundo
out vec2 vWorld;
void main() {
    vec2 xz = uCenter.xz + aCorner * uExtent;
    vWorld = xz;
    gl_Position = uVP * vec4(xz.x, 0.0, xz.y, 1.0);
})";

constexpr char kFsSrc[] = R"(#version 300 es
precision highp float;
in vec2 vWorld;
uniform vec3 uCamPos;
uniform vec2 uFade;      // x = início do fade radial, y = fim (da câmara)
uniform float uStep;     // passo das linhas (unidades de mundo)
out vec4 outColor;
// tema mono (o mesmo do grid de linhas da F2)
const vec3 kLineCol = vec3(0.30);
const vec3 kAxisCol = vec3(0.55);

// cobertura AA de uma linha por coordenada de célula: 1.0 no centro da
// linha, 0.0 a partir de 1 px de distância (fwidth faz o AA geométrico)
float lineAA(float c) {
    float fw = fwidth(c);
    float g = abs(fract(c - 0.5) - 0.5) / max(fw, 1e-4);
    return 1.0 - min(g, 1.0);
}

void main() {
    vec2 cell = vWorld / uStep;
    // anti-moiré: célula com menos de ~2 px na tela dissolve suavemente —
    // sub-amostragem vira ruído/cintilação, melhor desaparecer
    vec2 fw = fwidth(cell);
    float cellPx = 1.0 / max(max(fw.x, fw.y), 1e-4);
    float level = smoothstep(0.8, 2.5, cellPx);
    float line = max(lineAA(cell.x), lineAA(cell.y)) * level;

    // eixos X (z=0) e Z (x=0) ancorados na origem do mundo — linha única
    // nunca moira: AA por fwidth mantém-na com 1 px a qualquer distância
    float axX = 1.0 - min(abs(vWorld.y) / max(fwidth(vWorld.y), 1e-4), 1.0);
    float axZ = 1.0 - min(abs(vWorld.x) / max(fwidth(vWorld.x), 1e-4), 1.0);
    float axis = max(axX, axZ);

    float strength = max(line, axis);
    vec3 col = mix(kLineCol, kAxisCol, axis);

    // fade radial pela distância XZ à câmara — o grid perde-se no fundo
    // antes de qualquer borda do quad ("espaço infinito" do viewport)
    float d = length(vWorld - uCamPos.xz);
    float fade = 1.0 - smoothstep(uFade.x, uFade.y, d);
    float a = strength * fade;
    if (a <= 0.003) discard;
    outColor = vec4(col, a);
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
        LOGE("Grid: falha ao compilar shader: %s", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

} // namespace

bool Grid::init(f32 extent, f32 step) {
    destroy();
    if (extent <= 0.0f || step <= 0.0f || step > extent) {
        LOGE("Grid: parâmetros inválidos (extent=%f step=%f)", extent, step);
        return false;
    }
    if (extent < 2000.0f) {
        // guard da spec F3.1: com menos de 2000 a borda aparece no zoom máx
        LOGI("Grid: extent %.0f < 2000 — a borda pode aparecer no zoom máx", extent);
    }
    step_ = step;

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
        LOGE("Grid: falha ao linkar programa: %s", log);
        return false;
    }
    locVP_     = glGetUniformLocation(prog_, "uVP");
    locCenter_ = glGetUniformLocation(prog_, "uCenter");
    locCam_    = glGetUniformLocation(prog_, "uCamPos");
    locFade_   = glGetUniformLocation(prog_, "uFade");
    locStep_   = glGetUniformLocation(prog_, "uStep");

    // geometria inteira: um quad de 4 vértices (era uma malha de linhas)
    glGenVertexArrays(1, &vao_);
    glGenBuffers(1, &vbo_);
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadCorners), kQuadCorners,
                 GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(f32),
                          reinterpret_cast<const void*>(0));
    glBindVertexArray(0);

    vertexCount_ = 4;
    LOGI("Grid: pronto (quad + shader procedural, %u vértices, extent %.0f, step %.2f)",
         vertexCount_, extent, step);
    return true;
}

void Grid::destroy() {
    if (vbo_)  { glDeleteBuffers(1, &vbo_); vbo_ = 0; }
    if (vao_)  { glDeleteVertexArrays(1, &vao_); vao_ = 0; }
    if (prog_) { glDeleteProgram(prog_); prog_ = 0; }
    locVP_ = locCenter_ = locCam_ = locFade_ = locStep_ = -1;
    vertexCount_ = 0;
}

DrawStats Grid::draw(const Mat4& vp, const Vec3& camPos, f32 focusDist) const {
    if (!vao_ || vertexCount_ == 0) {
        return {};
    }
    glUseProgram(prog_);
    if (locVP_ >= 0) {
        glUniformMatrix4fv(locVP_, 1, GL_FALSE, vp.m);
    }
    if (locCenter_ >= 0) {
        glUniform3f(locCenter_, camPos.x, 0.0f, camPos.z);   // plano y=0
    }
    if (locCam_ >= 0) {
        glUniform3f(locCam_, camPos.x, camPos.y, camPos.z);
    }
    if (locStep_ >= 0) {
        glUniform1f(locStep_, step_);
    }
    // fade adaptativo ao zoom: no zoom de trabalho (6) recolhe-se para
    // ~12..42 (o feel do grid de linhas da F2); no zoom máx (300) abre até
    // ~330..780 — sempre muito antes da borda do quad (extent 3000)
    const f32 focus = focusDist > 0.0f ? focusDist : 6.0f;
    const f32 s1 = focus * 1.1f;
    const f32 start = s1 > 12.0f ? s1 : 12.0f;
    const f32 e1 = focus * 1.5f;
    const f32 end = start + (e1 > 30.0f ? e1 : 30.0f);
    if (locFade_ >= 0) {
        glUniform2f(locFade_, start, end);
    }
    // transparente: blend com fade, depth test ligado (respeita os TICs),
    // depth write desligado (nunca oculta nada); sem cull — o quad é visto
    // de cima e de baixo (pitch ±89°)
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glBindVertexArray(vao_);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glBindVertexArray(0);
    glEnable(GL_CULL_FACE);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    return {vertexCount_, 1};
}

} // namespace vv
