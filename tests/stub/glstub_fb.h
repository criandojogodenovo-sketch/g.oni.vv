// tests/stub/glstub_fb.h — 0.9.6.5 (GRUPO B · FERRAMENTAS DE VERIFICAÇÃO):
// o FRAMEBUFFER REAL do C33 virtual.
//
// O QUE ISTO É: com glstub::fb.enabled = true, o stub de GL deixa de ser
// no-op e passa a RASTERIZAR a sério num framebuffer RGBA8 + depth f32 —
// as MESMAS regras do Mali do device, pela MESMA API que a engine usa:
//
//   [objetos] ids ÚNICOS e nunca reutilizados (texturas/buffers/VAOs/
//             programas — como um GL real; o modo no-op mantém os ids
//             1..n de sempre, zero impacto nos testes existentes);
//   [dados]   glBufferData COPIA os bytes por buffer (ARRAY/ELEMENT pelo
//             alvo); glTexImage2D guarda os pixéis (GL_R8 = atlas de
//             cobertura/branca; GL_RGBA = albedo); glCompressedTexImage2D
//             marca FLAT (não decodifica ASTC/ETC2 no virtual — amostra
//             branca; a GEOMETRIA e o TEXTO são o que o layout precisa);
//   [uniforms] glGetUniformLocation devolve um slot POR NOME, por
//             programa; o rasterizador conhece os uniforms da engine
//             PELO NOME (uProj do pass de UI; uVP·uModel [+uSkin/uBones]
//             do lit) — o mesmo conhecimento que o Mali tem por compilação;
//   [regras]  viewport/scissor/depth(GL_LESS+mask)/cull(back)/blend
//             (SRC_ALPHA/ONE_MINUS_SRC_ALPHA — o único par que a engine
//             usa) — a fronteira 3D→UI da 0.7.8 e o confinamento por
//             scissor da 0.9.6.1 rasterizam EXATAMENTE como no device;
//   [shaders] os DOIS programas da engine, fiéis aos fontes reais:
//             UI (Renderer.cpp): outColor = vColor * texture(uTex, vUV).r
//               — mono: a amostra é o canal R (cobertura do glifo;
//                 branca/textura branca = 1); posições em px, orto y-down;
//             lit (Material.cpp): n = mat3(uModel)·(skin? skin·n);
//               diff = max(dot(n, kLight), 0); alb = 0.58·(hasTex?
//               texel.rgb : 1); c = 0.16 + alb·diff; out.rgb = c·uTint;
//               out.a = 1 — a luz direcional FIXA do shader original;
//   [leitura] glReadPixels devolve o framebuffer EM GL (origem no FUNDO,
//             RGBA) — o pipeline de thumb do main.cpp (glReadPixels →
//             flip → crop → downsample → PNG) passa a correr a sério no CI.
//
// DEFAULT OFF (glstub::fb.enabled = false): TODAS as funções GL mantêm o
// comportamento no-op de sempre — a suíte existente não vê UM byte de
// diferença. O glstub::reset() limpa os objetos/estado do fb (como um
// contexto que morre) mas MANTÉM `enabled` (configuração do device, como
// o astcLdr — o harness liga na FASE 12.12, a AUDITORIA de layout).
//
// A sentinela regress_fb_rasterizador_soft (test_sentinels.cpp) aferiu o
// rasterizador PIXEL A PIXEL (blend/scissor/depth/cull); a FASE 12.12 LEU
// OS PNGs de volta com o loadPng da engine (stb_image) — "PNGs LIDOS".
#pragma once
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace glstub {
namespace fb {

inline bool enabled = false;

// ---- objetos ---------------------------------------------------------------
struct TexObj {
    unsigned id = 0;
    int w = 0, h = 0;
    int channels = 0;      // 1 (GL_R8) / 4 (GL_RGBA); 0 = FLAT (comprimida)
    std::vector<uint8_t> px;
};
struct BufObj {
    unsigned id = 0;
    std::vector<uint8_t> bytes;
};
struct Attrib {
    bool on = false;
    unsigned buf = 0;      // o ARRAY bind do MOMENTO do setup (real GL)
    int size = 0;          // componentes (2/3/4)
    int stride = 0;        // bytes entre vértices
    int off = 0;           // byte offset no buffer
};
struct VaoObj {
    unsigned id = 0;
    Attrib attribs[8];
    unsigned elemBuf = 0;  // o ELEMENT bind gravado enquanto o VAO corria
};
struct UniSlot {           // por localização (= por nome consultado)
    char kind = 0;         // 'M' mat4 | 'F' 1f | 'D' 2f | 'T' 3f | 'I' 1i
    float m[16] = {};
    float f = 0.0f;
    float v2[2] = {};
    float v3[3] = {};
    int i = 0;
};
struct ProgObj {
    unsigned id = 0;
    std::vector<std::string> names;   // loc → nome (a CHAVE do rasterizador)
    std::vector<UniSlot> slots;
    std::vector<float> bones;         // uBones (count>1): n*16
    bool bonesSet = false;
};

inline unsigned nextId_ = 0;          // UM contador; ids nunca reutilizados
inline std::vector<TexObj> texs_;
inline std::vector<BufObj> bufs_;
inline std::vector<VaoObj> vaos_;
inline std::vector<ProgObj> progs_;

// ---- estado corrente --------------------------------------------------------
inline unsigned curTex_ = 0;
inline unsigned curArrayBuf_ = 0;
inline unsigned curElemBuf_ = 0;
inline unsigned curVao_ = 0;
inline unsigned curProg_ = 0;
inline float clearColor_[4] = {0.0f, 0.0f, 0.0f, 0.0f};
inline int viewport_[4] = {0, 0, 0, 0};
inline bool scissorOn_ = false;
inline int scissor_[4] = {0, 0, 0, 0};
inline bool depthOn_ = false;
inline bool cullOn_ = false;
inline bool blendOn_ = false;
inline bool depthMask_ = true;

// ---- framebuffer (linha 0 = FUNDO; convenção GL — glReadPixels direto) ------
inline bool hasFb_ = false;
inline int fbW_ = 0, fbH_ = 0;
inline std::vector<uint8_t> color_;   // RGBA8
inline std::vector<float> depth_;     // f32 (GL_LESS)

inline TexObj* findTex_(unsigned id) {
    for (auto& t : texs_) if (t.id == id) return &t;
    return nullptr;
}
inline BufObj* findBuf_(unsigned id) {
    for (auto& b : bufs_) if (b.id == id) return &b;
    return nullptr;
}
inline VaoObj* findVao_(unsigned id) {
    for (auto& v : vaos_) if (v.id == id) return &v;
    return nullptr;
}
inline ProgObj* findProg_(unsigned id) {
    for (auto& p : progs_) if (p.id == id) return &p;
    return nullptr;
}
inline void allocFb_() {
    if (viewport_[2] <= 0 || viewport_[3] <= 0) return;
    if (hasFb_ && fbW_ == viewport_[2] && fbH_ == viewport_[3]) return;
    fbW_ = viewport_[2];
    fbH_ = viewport_[3];
    color_.assign(static_cast<size_t>(fbW_) * fbH_ * 4, 0);
    depth_.assign(static_cast<size_t>(fbW_) * fbH_, 1.0f);
    hasFb_ = true;
}

// ---- eventos chamados pelos ramos fb das funções GL (gl3.h) ------------------
inline unsigned genObjects() { return ++nextId_; }

inline void delTexture(unsigned id) {
    for (size_t i = 0; i < texs_.size(); ++i) {
        if (texs_[i].id == id) { texs_.erase(texs_.begin() + i); return; }
    }
}
inline void delBuffer(unsigned id) {
    for (size_t i = 0; i < bufs_.size(); ++i) {
        if (bufs_[i].id == id) { bufs_.erase(bufs_.begin() + i); return; }
    }
}
inline void delVao(unsigned id) {
    for (size_t i = 0; i < vaos_.size(); ++i) {
        if (vaos_[i].id == id) { vaos_.erase(vaos_.begin() + i); return; }
    }
}
inline void onGenVao(unsigned id) { vaos_.push_back(VaoObj{id, {}, 0}); }
// 0.9.6.5: os REGISTOS que faltavam no rascunho — sem estes, bufferData/
// texImage2D procuravam objetos que NUNCA existiram (bufs_/texs_ vazios:
// o fetch dos vértices devolvia zeros e NADA rasterizava — a sonda do
// Grupo B apanhou ao vivo)
inline void onGenTexture(unsigned id) { texs_.push_back(TexObj{id, 0, 0, 0, {}}); }
inline void onGenBuffer(unsigned id) { bufs_.push_back(BufObj{id, {}}); }

inline void bindTexture(unsigned id) { curTex_ = id; }

inline void texImage2D(int internalformat, int w, int h, int format,
                       const void* pixels) {
    TexObj* t = findTex_(curTex_);
    if (!t || w <= 0 || h <= 0) return;
    if (internalformat == 0x8229 /*GL_R8*/ || format == 0x1903 /*GL_RED*/) {
        t->w = w; t->h = h; t->channels = 1;
        t->px.assign(static_cast<size_t>(w) * h, 0);
        if (pixels) std::memcpy(t->px.data(), pixels, t->px.size());
    } else {   // GL_RGBA — o caminho do albedo (Texture.cpp)
        t->w = w; t->h = h; t->channels = 4;
        t->px.assign(static_cast<size_t>(w) * h * 4, 0);
        if (pixels) std::memcpy(t->px.data(), pixels, t->px.size());
    }
}

inline void compressedTexImage2D(int w, int h) {
    // ASTC/ETC2: não decodifica no virtual — FLAT (amostra branca)
    TexObj* t = findTex_(curTex_);
    if (!t) return;
    t->w = w > 0 ? w : 1;
    t->h = h > 0 ? h : 1;
    t->channels = 0;
    t->px.clear();
}

inline void bindBuffer(unsigned target, unsigned id) {
    if (target == 0x8892 /*ARRAY*/) curArrayBuf_ = id;
    else if (target == 0x8893 /*ELEMENT*/) curElemBuf_ = id;
}

inline void bufferData(unsigned target, intptr_t size, const void* data) {
    const unsigned id = (target == 0x8892) ? curArrayBuf_ : curElemBuf_;
    BufObj* b = findBuf_(id);
    if (!b || size <= 0 || !data) return;
    b->bytes.assign(static_cast<size_t>(size), 0);
    std::memcpy(b->bytes.data(), data, b->bytes.size());
}

inline void bindVao(unsigned id) {
    curVao_ = id;
    VaoObj* v = findVao_(id);
    if (v) v->elemBuf = curElemBuf_;   // real GL: ELEMENT é estado do VAO
}

inline void enableAttrib(int loc) {
    VaoObj* v = findVao_(curVao_);
    if (v && loc >= 0 && loc < 8) v->attribs[loc].on = true;
}
inline void attribPointer(int loc, int size, int stride, const void* off) {
    VaoObj* v = findVao_(curVao_);
    if (!v || loc < 0 || loc >= 8) return;
    Attrib& a = v->attribs[loc];
    a.on = true;
    a.buf = curArrayBuf_;
    a.size = size;
    a.stride = stride;
    a.off = static_cast<int>(reinterpret_cast<intptr_t>(off));
}

inline void useProgram(unsigned id) { curProg_ = id; }
inline void onCreateProgram(unsigned id) {
    progs_.push_back(ProgObj{id, {}, {}, {}, false});
}
inline void delProgram(unsigned id) {
    for (size_t i = 0; i < progs_.size(); ++i) {
        if (progs_[i].id == id) { progs_.erase(progs_.begin() + i); return; }
    }
}
inline int getUniformLocation(unsigned prog, const char* name) {
    ProgObj* p = findProg_(prog);
    if (!p || !name) return -1;
    for (size_t i = 0; i < p->names.size(); ++i) {
        if (p->names[i] == name) return static_cast<int>(i);
    }
    p->names.push_back(name);
    p->slots.push_back(UniSlot{});
    return static_cast<int>(p->names.size() - 1);
}
inline void uniformMatrix4fv(int loc, int count, const float* m) {
    ProgObj* p = findProg_(curProg_);
    if (!p || !m) return;
    if (count > 1) {   // uBones (ARRAY — aplicado por uSkin)
        p->bones.assign(static_cast<size_t>(count) * 16u, 0.0f);
        std::memcpy(p->bones.data(), m, sizeof(float) * 16u * size_t(count));
        p->bonesSet = true;
        return;
    }
    if (loc < 0 || loc >= int(p->slots.size())) return;
    UniSlot& s = p->slots[size_t(loc)];
    s.kind = 'M';
    std::memcpy(s.m, m, sizeof(float) * 16);
}
inline void uniform1f(int loc, float v) {
    ProgObj* p = findProg_(curProg_);
    if (!p || loc < 0 || loc >= int(p->slots.size())) return;
    UniSlot& s = p->slots[size_t(loc)];
    s.kind = 'F'; s.f = v;
}
// 0.9.6.5: uFade do Grid (vec2) — o fwidth do fade precisa dele
inline void uniform2f(int loc, float x, float y) {
    ProgObj* p = findProg_(curProg_);
    if (!p || loc < 0 || loc >= int(p->slots.size())) return;
    UniSlot& s = p->slots[size_t(loc)];
    s.kind = 'D'; s.v2[0] = x; s.v2[1] = y;
}
inline void uniform3f(int loc, float x, float y, float z) {
    ProgObj* p = findProg_(curProg_);
    if (!p || loc < 0 || loc >= int(p->slots.size())) return;
    UniSlot& s = p->slots[size_t(loc)];
    s.kind = 'T'; s.v3[0] = x; s.v3[1] = y; s.v3[2] = z;
}
inline void uniform1i(int loc, int v) {
    ProgObj* p = findProg_(curProg_);
    if (!p || loc < 0 || loc >= int(p->slots.size())) return;
    UniSlot& s = p->slots[size_t(loc)];
    s.kind = 'I'; s.i = v;
}

inline void setViewport(int x, int y, int w, int h) {
    viewport_[0] = x; viewport_[1] = y; viewport_[2] = w; viewport_[3] = h;
    allocFb_();   // o 1.º glViewport traz o tamanho da superfície
}
inline void setScissor(int x, int y, int w, int h) {
    scissor_[0] = x; scissor_[1] = y; scissor_[2] = w; scissor_[3] = h;
}
// 0.9.6.5: os setters das capacidades (o gl3.h chama-os nos ramos fb de
// glEnable/glDisable/glDepthMask — os MESMOS switches que o Mali tem)
inline void setScissorEnabled(bool on) { scissorOn_ = on; }
inline void setDepthEnabled(bool on) { depthOn_ = on; }
inline void setCullEnabled(bool on) { cullOn_ = on; }
inline void setBlendEnabled(bool on) { blendOn_ = on; }
inline void setDepthMask(bool on) { depthMask_ = on; }
inline void setClearColor(float r, float g, float b, float a) {
    clearColor_[0] = r; clearColor_[1] = g;
    clearColor_[2] = b; clearColor_[3] = a;
}

inline void clear(unsigned mask) {
    if (!hasFb_) return;
    int x0 = 0, y0 = 0, x1 = fbW_, y1 = fbH_;
    if (scissorOn_) {
        x0 = scissor_[0] < 0 ? 0 : scissor_[0];
        y0 = scissor_[1] < 0 ? 0 : scissor_[1];
        x1 = scissor_[0] + scissor_[2]; if (x1 > fbW_) x1 = fbW_;
        y1 = scissor_[1] + scissor_[3]; if (y1 > fbH_) y1 = fbH_;
    }
    if (mask & 0x00004000 /*COLOR*/) {
        const uint8_t r = uint8_t(clearColor_[0] * 255.0f + 0.5f);
        const uint8_t g = uint8_t(clearColor_[1] * 255.0f + 0.5f);
        const uint8_t b = uint8_t(clearColor_[2] * 255.0f + 0.5f);
        const uint8_t a = uint8_t(clearColor_[3] * 255.0f + 0.5f);
        for (int y = y0; y < y1; ++y) {
            uint8_t* row = &color_[static_cast<size_t>(y) * fbW_ * 4];
            for (int x = x0; x < x1; ++x) {
                uint8_t* p = row + size_t(x) * 4;
                p[0] = r; p[1] = g; p[2] = b; p[3] = a;
            }
        }
    }
    if (mask & 0x00000100 /*DEPTH*/) {
        for (int y = y0; y < y1; ++y) {
            float* row = &depth_[static_cast<size_t>(y) * fbW_];
            for (int x = x0; x < x1; ++x) row[x] = 1.0f;
        }
    }
}

// ---- o RASTERIZADOR ----------------------------------------------------------
inline void mat4ByVec(const float* m, float x, float y, float z, float w,
                      float* out) {
    out[0] = m[0] * x + m[4] * y + m[8] * z + m[12] * w;
    out[1] = m[1] * x + m[5] * y + m[9] * z + m[13] * w;
    out[2] = m[2] * x + m[6] * y + m[10] * z + m[14] * w;
    out[3] = m[3] * x + m[7] * y + m[11] * z + m[15] * w;
}
inline void mat3ByVec(const float* m, float x, float y, float z, float* out) {
    out[0] = m[0] * x + m[4] * y + m[8] * z;
    out[1] = m[1] * x + m[5] * y + m[9] * z;
    out[2] = m[2] * x + m[6] * y + m[10] * z;
}

// cobertura R8 bilinear clamp-to-edge; FLAT/comprimida = 1 (branca)
inline float sampleR(const TexObj* t, float u, float v) {
    if (!t || t->channels != 1 || t->w < 1 || t->h < 1 || t->px.empty()) {
        return 1.0f;
    }
    auto at = [&](int x, int y) -> float {
        if (x < 0) x = 0; if (x >= t->w) x = t->w - 1;
        if (y < 0) y = 0; if (y >= t->h) y = t->h - 1;
        return float(t->px[size_t(y) * t->w + size_t(x)]) / 255.0f;
    };
    const float fx = u * t->w - 0.5f, fy = v * t->h - 0.5f;
    const int x0 = int(fx), y0 = int(fy);
    const float tx = fx - float(x0), ty = fy - float(y0);
    const float a = at(x0, y0), b = at(x0 + 1, y0);
    const float c = at(x0, y0 + 1), d = at(x0 + 1, y0 + 1);
    return (a * (1.0f - tx) + b * tx) * (1.0f - ty) +
           (c * (1.0f - tx) + d * tx) * ty;
}

// albedo .rgb bilinear; FLAT = branco; R8 no lit = (r,0,0) como GL
inline void sampleRGB(const TexObj* t, float u, float v, float* out) {
    if (!t || t->channels == 0) { out[0] = out[1] = out[2] = 1.0f; return; }
    if (t->channels != 4) {
        const float r = sampleR(t, u, v);
        out[0] = r; out[1] = 0.0f; out[2] = 0.0f;
        return;
    }
    if (t->w < 1 || t->h < 1 || t->px.empty()) {
        out[0] = out[1] = out[2] = 1.0f;
        return;
    }
    auto at = [&](int x, int y, int c) -> float {
        if (x < 0) x = 0; if (x >= t->w) x = t->w - 1;
        if (y < 0) y = 0; if (y >= t->h) y = t->h - 1;
        return float(t->px[(size_t(y) * t->w + size_t(x)) * 4 + size_t(c)]) / 255.0f;
    };
    const float fx = u * t->w - 0.5f, fy = v * t->h - 0.5f;
    const int x0 = int(fx), y0 = int(fy);
    const float tx = fx - float(x0), ty = fy - float(y0);
    for (int c = 0; c < 3; ++c) {
        const float a = at(x0, y0, c), b = at(x0 + 1, y0, c);
        const float d = at(x0, y0 + 1, c), e = at(x0 + 1, y0 + 1, c);
        out[c] = (a * (1.0f - tx) + b * tx) * (1.0f - ty) +
                 (d * (1.0f - tx) + e * tx) * ty;
    }
}

// fetch genérico por atributo (floats do buffer do VAO)
struct VertFetch {
    const VaoObj* vao = nullptr;
    const BufObj* bufOf[8] = {};

    float attr(int loc, int comp, int vertexIndex, float def) const {
        if (!vao || loc < 0 || loc > 7) return def;
        const Attrib& a = vao->attribs[loc];
        if (!a.on || !a.buf || comp >= a.size || a.stride <= 0) return def;
        const BufObj* b = bufOf[loc];
        if (!b) return def;
        const size_t byteOff = size_t(vertexIndex) * size_t(a.stride) +
                               size_t(a.off) + size_t(comp) * 4u;
        if (byteOff + 4 > b->bytes.size()) return def;
        float f = 0.0f;
        std::memcpy(&f, b->bytes.data() + byteOff, 4);
        return f;
    }
};

// o programa em uso resolvido por NOME: uProj → UI; uCenter → GRID;
// uVP+uModel → lit (a ORDEM importa: o grid TAMBÉM tem uVP, mas nunca
// uModel/uCenter — o lit nunca tem uCenter)
struct ProgramKind {
    enum K { Unknown, Ui, Lit, Grid };
    K kind = Unknown;
    const float* proj = nullptr;    // UI
    const float* vp = nullptr;      // lit
    const float* model = nullptr;   // lit
    const float* tint = nullptr;    // lit (uTint; nullptr = branco)
    float hasTex = 0.0f;            // lit (uHasTex)
    int skin = 0;                   // lit (uSkin)
    const float* bones = nullptr;   // lit (uBones)
    // grid (render/Grid.cpp — a grelha adaptativa 0.8.9)
    const float* gridVp = nullptr;  // uVP
    float center[3] = {};           // uCenter (xz da câmara)
    float extent = 3000.0f;         // uExtent
    float camPos[3] = {};           // uCamPos
    float fade[2] = {12.0f, 42.0f}; // uFade
    float step = 1.0f;              // uStep
};

inline ProgramKind resolveProgram() {
    ProgramKind pk;
    ProgObj* p = findProg_(curProg_);
    if (!p) return pk;
    auto slotByName = [&](const char* n) -> int {
        for (size_t i = 0; i < p->names.size(); ++i) {
            if (p->names[i] == n) return int(i);
        }
        return -1;
    };
    const auto hasMat = [&](int loc) -> bool {
        return loc >= 0 && loc < int(p->slots.size()) &&
               p->slots[size_t(loc)].kind == 'M';
    };
    const int locProj = slotByName("uProj");
    if (hasMat(locProj)) {
        pk.kind = ProgramKind::Ui;
        pk.proj = p->slots[size_t(locProj)].m;
        return pk;
    }
    // 0.9.6.5: o GRID antes do lit (o grid tem uVP SEM uModel; a chave é
    // o uCenter vec3 — nenhum outro programa o consulta)
    const int locCenter = slotByName("uCenter");
    if (locCenter >= 0 && locCenter < int(p->slots.size()) &&
        p->slots[size_t(locCenter)].kind == 'T') {
        const int locGvp = slotByName("uVP");
        if (hasMat(locGvp)) {
            pk.kind = ProgramKind::Grid;
            pk.gridVp = p->slots[size_t(locGvp)].m;
            std::memcpy(pk.center, p->slots[size_t(locCenter)].v3,
                        sizeof(float) * 3);
            const int locExt = slotByName("uExtent");
            if (locExt >= 0 && locExt < int(p->slots.size()) &&
                p->slots[size_t(locExt)].kind == 'F') {
                pk.extent = p->slots[size_t(locExt)].f;
            }
            const int locCam = slotByName("uCamPos");
            if (locCam >= 0 && locCam < int(p->slots.size()) &&
                p->slots[size_t(locCam)].kind == 'T') {
                std::memcpy(pk.camPos, p->slots[size_t(locCam)].v3,
                            sizeof(float) * 3);
            }
            const int locFade = slotByName("uFade");
            if (locFade >= 0 && locFade < int(p->slots.size()) &&
                p->slots[size_t(locFade)].kind == 'D') {
                pk.fade[0] = p->slots[size_t(locFade)].v2[0];
                pk.fade[1] = p->slots[size_t(locFade)].v2[1];
            }
            const int locStep = slotByName("uStep");
            if (locStep >= 0 && locStep < int(p->slots.size()) &&
                p->slots[size_t(locStep)].kind == 'F') {
                pk.step = p->slots[size_t(locStep)].f;
            }
            return pk;
        }
    }
    const int locVP = slotByName("uVP");
    const int locModel = slotByName("uModel");
    if (hasMat(locVP) && hasMat(locModel)) {
        pk.kind = ProgramKind::Lit;
        pk.vp = p->slots[size_t(locVP)].m;
        pk.model = p->slots[size_t(locModel)].m;
        const int locTint = slotByName("uTint");
        if (locTint >= 0 && locTint < int(p->slots.size()) &&
            p->slots[size_t(locTint)].kind == 'T') {
            pk.tint = p->slots[size_t(locTint)].v3;
        }
        const int locHasTex = slotByName("uHasTex");
        if (locHasTex >= 0 && locHasTex < int(p->slots.size()) &&
            p->slots[size_t(locHasTex)].kind == 'F') {
            pk.hasTex = p->slots[size_t(locHasTex)].f;
        }
        const int locSkin = slotByName("uSkin");
        if (locSkin >= 0 && locSkin < int(p->slots.size()) &&
            p->slots[size_t(locSkin)].kind == 'I') {
            pk.skin = p->slots[size_t(locSkin)].i;
        }
        if (p->bonesSet) pk.bones = p->bones.data();
        return pk;
    }
    return pk;
}

// um vértice em JANELA (px/py desde o FUNDO, como GL; z ndc; 1/w)
struct RVertex {
    float x = 0, y = 0, z = 0, iw = 1;
    float u = 0, v = 0;
    float cr = 1, cg = 1, cb = 1, ca = 1;   // cor (UI)
    float nx = 0, ny = 0, nz = 1;           // normal EM MUNDO (lit)
};

inline void ndcToWindow(const float* clip, float* outX, float* outY,
                        float* outZ, float* outIW) {
    if (clip[3] == 0.0f) {
        *outX = *outY = *outZ = 0.0f;
        *outIW = 1e9f;
        return;
    }
    const float iw = 1.0f / clip[3];
    *outX = (clip[0] * iw * 0.5f + 0.5f) * float(viewport_[2]) + float(viewport_[0]);
    *outY = (clip[1] * iw * 0.5f + 0.5f) * float(viewport_[3]) + float(viewport_[1]);
    *outZ = clip[2] * iw;
    *outIW = iw;
}

inline void rasterTriangle(const RVertex& a, const RVertex& b, const RVertex& c,
                           const ProgramKind& pk, const TexObj* tex) {
    if (!hasFb_) return;
    const float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    if (cullOn_ && area <= 0.0f) return;   // janela y-para-cima: front=CCW
    if (area == 0.0f) return;

    int minX = fbW_, minY = fbH_, maxX = 0, maxY = 0;
    {
        const float xs[3] = {a.x, b.x, c.x};
        const float ys[3] = {a.y, b.y, c.y};
        for (float v : xs) {
            const int lo = int(v) - 1, hi = int(v) + 1;
            if (lo < minX) minX = lo;
            if (hi > maxX) maxX = hi;
        }
        for (float v : ys) {
            const int lo = int(v) - 1, hi = int(v) + 1;
            if (lo < minY) minY = lo;
            if (hi > maxY) maxY = hi;
        }
    }
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX > fbW_) maxX = fbW_;
    if (maxY > fbH_) maxY = fbH_;
    if (scissorOn_) {
        const int sx0 = scissor_[0] < 0 ? 0 : scissor_[0];
        const int sy0 = scissor_[1] < 0 ? 0 : scissor_[1];
        int sx1 = scissor_[0] + scissor_[2]; if (sx1 > fbW_) sx1 = fbW_;
        int sy1 = scissor_[1] + scissor_[3]; if (sy1 > fbH_) sy1 = fbH_;
        if (sx0 > minX) minX = sx0;
        if (sy0 > minY) minY = sy0;
        if (sx1 < maxX) maxX = sx1;
        if (sy1 < maxY) maxY = sy1;
    }
    if (minX >= maxX || minY >= maxY) return;

    const float invArea = 1.0f / area;
    for (int py = minY; py < maxY; ++py) {
        for (int px = minX; px < maxX; ++px) {
            const float fx = float(px) + 0.5f, fy = float(py) + 0.5f;
            const float l0 = ((c.x - b.x) * (fy - b.y) -
                              (fx - b.x) * (c.y - b.y)) * invArea;
            const float l1 = ((a.x - c.x) * (fy - c.y) -
                              (fx - c.x) * (a.y - c.y)) * invArea;
            const float l2 = 1.0f - l0 - l1;
            if (l0 < 0.0f || l1 < 0.0f || l2 < 0.0f) continue;

            // z de janela é AFFIM EM ECRÃ (gl_FragCoord.z); depth GL_LESS
            const float z = a.z * l0 + b.z * l1 + c.z * l2;
            if (z < -1.0f || z > 1.0f) continue;
            const float dz = z * 0.5f + 0.5f;
            const size_t pi = size_t(py) * fbW_ + size_t(px);
            if (depthOn_ && dz >= depth_[pi]) continue;

            // atributos PERSPECTIVE-CORRECT: lerp(attr/w) / lerp(1/w)
            const float wa = a.iw, wb_ = b.iw, wc = c.iw;
            const float iws = l0 * wa + l1 * wb_ + l2 * wc;
            const float corr = (std::fabs(iws) < 1e-9f) ? 0.0f : 1.0f / iws;
            const float u = (l0 * wa * a.u + l1 * wb_ * b.u + l2 * wc * c.u) * corr;
            const float v = (l0 * wa * a.v + l1 * wb_ * b.v + l2 * wc * c.v) * corr;

            float outR = 1.0f, outG = 1.0f, outB = 1.0f, outA = 1.0f;
            if (pk.kind == ProgramKind::Ui) {
                // Renderer.cpp: outColor = vColor * texture(uTex, vUV).r
                const float cr = (l0 * wa * a.cr + l1 * wb_ * b.cr + l2 * wc * c.cr) * corr;
                const float cg = (l0 * wa * a.cg + l1 * wb_ * b.cg + l2 * wc * c.cg) * corr;
                const float cbv = (l0 * wa * a.cb + l1 * wb_ * b.cb + l2 * wc * c.cb) * corr;
                const float ca = (l0 * wa * a.ca + l1 * wb_ * b.ca + l2 * wc * c.ca) * corr;
                const float cov = sampleR(tex, u, v);
                outR = cr * cov;
                outG = cg * cov;
                outB = cbv * cov;
                outA = ca * cov;
            } else {
                // Material.cpp: c = 0.16 + alb·diff; out.rgb = c·uTint; a=1
                float nx = (l0 * wa * a.nx + l1 * wb_ * b.nx + l2 * wc * c.nx) * corr;
                float ny = (l0 * wa * a.ny + l1 * wb_ * b.ny + l2 * wc * c.ny) * corr;
                float nz = (l0 * wa * a.nz + l1 * wb_ * b.nz + l2 * wc * c.nz) * corr;
                const float nl = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (nl > 1e-6f) { nx /= nl; ny /= nl; nz /= nl; }
                const float kLx = 0.4545f, kLy = 0.7435f, kLz = 0.2891f;
                float diff = nx * kLx + ny * kLy + nz * kLz;
                if (diff < 0.0f) diff = 0.0f;
                float alb[3] = {0.58f, 0.58f, 0.58f};
                if (pk.hasTex > 0.5f) {
                    float texel[3];
                    sampleRGB(tex, u, v, texel);
                    alb[0] *= texel[0];
                    alb[1] *= texel[1];
                    alb[2] *= texel[2];
                }
                const float tint[3] = {pk.tint ? pk.tint[0] : 1.0f,
                                       pk.tint ? pk.tint[1] : 1.0f,
                                       pk.tint ? pk.tint[2] : 1.0f};
                outR = (0.16f + alb[0] * diff) * tint[0];
                outG = (0.16f + alb[1] * diff) * tint[1];
                outB = (0.16f + alb[2] * diff) * tint[2];
                outA = 1.0f;
            }

            uint8_t* dst = &color_[pi * 4];
            float dr = float(dst[0]) / 255.0f;
            float dg = float(dst[1]) / 255.0f;
            float db = float(dst[2]) / 255.0f;
            float da = float(dst[3]) / 255.0f;
            if (blendOn_) {   // SRC_ALPHA / ONE_MINUS_SRC_ALPHA (o par único)
                dr = outR * outA + dr * (1.0f - outA);
                dg = outG * outA + dg * (1.0f - outA);
                db = outB * outA + db * (1.0f - outA);
                da = outA + da * (1.0f - outA);
            } else {
                dr = outR; dg = outG; db = outB; da = outA;
            }
            if (dr < 0.0f) dr = 0.0f; if (dr > 1.0f) dr = 1.0f;
            if (dg < 0.0f) dg = 0.0f; if (dg > 1.0f) dg = 1.0f;
            if (db < 0.0f) db = 0.0f; if (db > 1.0f) db = 1.0f;
            if (da < 0.0f) da = 0.0f; if (da > 1.0f) da = 1.0f;
            dst[0] = uint8_t(dr * 255.0f + 0.5f);
            dst[1] = uint8_t(dg * 255.0f + 0.5f);
            dst[2] = uint8_t(db * 255.0f + 0.5f);
            dst[3] = uint8_t(da * 255.0f + 0.5f);
            if (depthOn_ && depthMask_) depth_[pi] = dz;
        }
    }
}

// ---- 0.9.6.5: o GRID (render/Grid.cpp) -----------------------------------
// helpers do fragment (smoothstep/clamp do GLSL, literais do shader real)
inline float ssSmooth(float e0, float e1, float x) {
    if (e1 <= e0) return x < e0 ? 0.0f : 1.0f;
    float t = (x - e0) / (e1 - e0);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return t * t * (3.0f - 2.0f * t);
}
inline float ssFract(float x) { return x - std::floor(x); }
// lineAA do Grid.cpp: cobertura 1.0 no centro da linha, 0 a 1 px (fwidth)
inline float gridLineAA(float c, float fw) {
    if (fw < 1e-4f) fw = 1e-4f;
    float g = std::fabs(ssFract(c - 0.5f) - 0.5f) / fw;
    if (g > 1.0f) g = 1.0f;
    return 1.0f - g;
}

// O fragment da grelha adaptativa, com o fwidth ANALÍTICO: vWorld é lido
// por interpolação perspective-correct do plano do triângulo e a derivada
// sai da avaliação do MESMO plano no pixel vizinho — exatamente o que o
// GPU calcula nos quads 2×2 (a matemática do plano não termina na aresta).
// a.u/a.v carregam o (wx, wz) MUNDIAL do vértice (o drawCore calculou).
inline void rasterGridTriangle(const RVertex& a, const RVertex& b,
                               const RVertex& c, const ProgramKind& pk) {
    if (!hasFb_) return;
    const float area = (b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y);
    if (area == 0.0f) return;

    int minX = fbW_, minY = fbH_, maxX = 0, maxY = 0;
    {
        const float xs[3] = {a.x, b.x, c.x};
        const float ys[3] = {a.y, b.y, c.y};
        for (float v : xs) {
            const int lo = int(v) - 1, hi = int(v) + 2;
            if (lo < minX) minX = lo;
            if (hi > maxX) maxX = hi;
        }
        for (float v : ys) {
            const int lo = int(v) - 1, hi = int(v) + 2;
            if (lo < minY) minY = lo;
            if (hi > maxY) maxY = hi;
        }
    }
    if (minX < 0) minX = 0;
    if (minY < 0) minY = 0;
    if (maxX > fbW_) maxX = fbW_;
    if (maxY > fbH_) maxY = fbH_;
    if (scissorOn_) {
        const int sx0 = scissor_[0] < 0 ? 0 : scissor_[0];
        const int sy0 = scissor_[1] < 0 ? 0 : scissor_[1];
        int sx1 = scissor_[0] + scissor_[2]; if (sx1 > fbW_) sx1 = fbW_;
        int sy1 = scissor_[1] + scissor_[3]; if (sy1 > fbH_) sy1 = fbH_;
        if (sx0 > minX) minX = sx0;
        if (sy0 > minY) minY = sy0;
        if (sx1 < maxX) maxX = sx1;
        if (sy1 < maxY) maxY = sy1;
    }
    if (minX >= maxX || minY >= maxY) return;

    const float invArea = 1.0f / area;
    const float step = pk.step > 1e-6f ? pk.step : 1e-6f;
    for (int py = minY; py < maxY; ++py) {
        for (int px = minX; px < maxX; ++px) {
            const float fx = float(px) + 0.5f, fy = float(py) + 0.5f;
            float l0 = ((c.x - b.x) * (fy - b.y) -
                        (fx - b.x) * (c.y - b.y)) * invArea;
            float l1 = ((a.x - c.x) * (fy - c.y) -
                        (fx - c.x) * (a.y - c.y)) * invArea;
            float l2 = 1.0f - l0 - l1;
            if (l0 < 0.0f || l1 < 0.0f || l2 < 0.0f) continue;

            const float z = a.z * l0 + b.z * l1 + c.z * l2;
            if (z < -1.0f || z > 1.0f) continue;
            const float dz = z * 0.5f + 0.5f;
            const size_t pi = size_t(py) * fbW_ + size_t(px);
            // Grid::draw: depth test LIGADO (respeita os TICs), write OFF
            if (depthOn_ && dz >= depth_[pi]) continue;

            // vWorld perspective-correct (u/v = wx/wz do vértice)
            const float wa = a.iw, wb_ = b.iw, wc = c.iw;
            const float iws = l0 * wa + l1 * wb_ + l2 * wc;
            const float corr = (std::fabs(iws) < 1e-9f) ? 0.0f : 1.0f / iws;
            const float wx = (l0 * wa * a.u + l1 * wb_ * b.u + l2 * wc * c.u) * corr;
            const float wz = (l0 * wa * a.v + l1 * wb_ * b.v + l2 * wc * c.v) * corr;

            // derivadas: os barycentricos em (fx+1, fy) e (fx, fy+1)
            struct WorldPt { float x, z; };
            auto baryAt = [&](float lx, float ly, float& o0, float& o1) {
                o0 = ((c.x - b.x) * (ly - b.y) - (lx - b.x) * (c.y - b.y)) * invArea;
                o1 = ((a.x - c.x) * (ly - c.y) - (lx - c.x) * (a.y - c.y)) * invArea;
            };
            float d0x, d1x, d0y, d1y;
            baryAt(fx + 1.0f, fy, d0x, d1x);
            baryAt(fx, fy + 1.0f, d0y, d1y);
            auto worldOf = [&](float o0, float o1) -> WorldPt {
                const float o2 = 1.0f - o0 - o1;
                const float iw = o0 * wa + o1 * wb_ + o2 * wc;
                const float cr = (std::fabs(iw) < 1e-9f) ? 0.0f : 1.0f / iw;
                return WorldPt{(o0 * wa * a.u + o1 * wb_ * b.u + o2 * wc * c.u) * cr,
                               (o0 * wa * a.v + o1 * wb_ * b.v + o2 * wc * c.v) * cr};
            };
            const WorldPt wdx = worldOf(d0x, d1x);
            const WorldPt wdy = worldOf(d0y, d1y);
            const float dwxDx = wdx.x - wx, dwzDx = wdx.z - wz;
            const float dwxDy = wdy.x - wx, dwzDy = wdy.z - wz;

            // fwidth(c) = |dc/dx| + |dc/dy| (por componente, como o GLSL)
            const float cellx = wx / step, cellz = wz / step;
            const float fwX = std::fabs(dwxDx) / step + std::fabs(dwxDy) / step;
            const float fwZ = std::fabs(dwzDx) / step + std::fabs(dwzDy) / step;
            const float fwMax = fwX > fwZ ? fwX : fwZ;
            const float cellPx = 1.0f / (fwMax > 1e-4f ? fwMax : 1e-4f);
            const float level = ssSmooth(0.8f, 2.5f, cellPx);
            const float line = (gridLineAA(cellx, fwX) > gridLineAA(cellz, fwZ)
                                    ? gridLineAA(cellx, fwX)
                                    : gridLineAA(cellz, fwZ)) * level;

            // eixos X (z=0) e Z (x=0) — linha única, AA por fwidth
            const float fwWz = std::fabs(dwzDx) + std::fabs(dwzDy);
            const float fwWx = std::fabs(dwxDx) + std::fabs(dwxDy);
            float axX = 1.0f - std::fabs(wz) / (fwWz > 1e-4f ? fwWz : 1e-4f);
            if (axX < 0.0f) axX = 0.0f;
            float axZ = 1.0f - std::fabs(wx) / (fwWx > 1e-4f ? fwWx : 1e-4f);
            if (axZ < 0.0f) axZ = 0.0f;
            const float axis = axX > axZ ? axX : axZ;

            const float strength = line > axis ? line : axis;
            if (strength <= 0.0f) continue;
            // col = mix(kLineCol 0.30, kAxisCol 0.55, axis)
            const float colC = 0.30f + (0.55f - 0.30f) * axis;

            // fade radial pela distância XZ à câmara
            const float ddx = wx - pk.camPos[0], ddz = wz - pk.camPos[2];
            const float d = std::sqrt(ddx * ddx + ddz * ddz);
            const float fade = 1.0f - ssSmooth(pk.fade[0], pk.fade[1], d);
            const float alpha = strength * fade;
            if (alpha <= 0.003f) continue;   // discard do shader

            uint8_t* dst = &color_[pi * 4];
            float dr = float(dst[0]) / 255.0f;
            float dg = float(dst[1]) / 255.0f;
            float db = float(dst[2]) / 255.0f;
            float da = float(dst[3]) / 255.0f;
            if (blendOn_) {   // Grid::draw liga SRC_ALPHA/ONE_MINUS
                dr = colC * alpha + dr * (1.0f - alpha);
                dg = colC * alpha + dg * (1.0f - alpha);
                db = colC * alpha + db * (1.0f - alpha);
                da = alpha + da * (1.0f - alpha);
            } else {
                dr = colC; dg = colC; db = colC; da = alpha;
            }
            if (dr > 1.0f) dr = 1.0f; if (dg > 1.0f) dg = 1.0f;
            if (db > 1.0f) db = 1.0f; if (da > 1.0f) da = 1.0f;
            dst[0] = uint8_t(dr * 255.0f + 0.5f);
            dst[1] = uint8_t(dg * 255.0f + 0.5f);
            dst[2] = uint8_t(db * 255.0f + 0.5f);
            dst[3] = uint8_t(da * 255.0f + 0.5f);
            // depth write OFF (o glDepthMask(GL_FALSE) do Grid::draw já
            // chega aqui pelo depthMask_ — nada escrito)
        }
    }
}

// núcleo COMUM aos dois draws (arrays/elementos); idx(i) dá o índice do
// vértice i (u16 do ELEMENT do VAO, ou first+i do arrays)
template <typename IdxOf>
inline void drawCore(unsigned mode, GLsizei count, IdxOf idxOf) {
    // 0.9.6.5: TRIANGLE_STRIP a pedido do Grid (a engine usa TRIANGLES na
    // UI/malhas e STRIP no quad da grelha — nada mais)
    const bool strip = (mode == 0x0005 /*GL_TRIANGLE_STRIP*/);
    if (mode != 0x0004 /*GL_TRIANGLES*/ && !strip) return;
    if (!hasFb_ || count < 3) return;
    VaoObj* vao = findVao_(curVao_);
    if (!vao) return;
    ProgramKind pk = resolveProgram();
    if (pk.kind == ProgramKind::Unknown) return;

    VertFetch vf;
    vf.vao = vao;
    for (int loc = 0; loc < 8; ++loc) vf.bufOf[loc] = findBuf_(vao->attribs[loc].buf);
    const TexObj* tex = findTex_(curTex_);

    // lit: mm = uVP × uModel (UMA vez por draw) — e o skin por vértice
    float mm[16] = {};
    if (pk.kind == ProgramKind::Lit) {
        for (int col = 0; col < 4; ++col) {
            for (int row = 0; row < 4; ++row) {
                float s = 0.0f;
                for (int k = 0; k < 4; ++k) {
                    s += pk.vp[k * 4 + row] * pk.model[col * 4 + k];
                }
                mm[col * 4 + row] = s;
            }
        }
    }
    auto skinMatrix = [&](float jx, float jy, float jz, float jw,
                          float wx, float wy, float wz, float ww,
                          float* out16) -> bool {
        if (!pk.skin || !pk.bones) return false;
        const float wgt[4] = {wx, wy, wz, ww};
        const int js[4] = {int(jx), int(jy), int(jz), int(jw)};
        float acc[16] = {0.0f};
        for (int wi = 0; wi < 4; ++wi) {
            if (wgt[wi] == 0.0f) continue;
            const float* src = pk.bones + size_t(js[wi]) * 16u;
            for (int e = 0; e < 16; ++e) acc[e] += src[e] * wgt[wi];
        }
        std::memcpy(out16, acc, sizeof(float) * 16);
        return true;
    };

    RVertex rv[3];
    // TRIANGLES: (0,1,2)(3,4,5)…; STRIP: (0,1,2)(1,3,2)(2,3,4)… com o
    // winding alternado do strip (o grid desenha com cull OFF, mas a
    // decomposição é a correta à mesma)
    for (GLsizei ti = 0; ti + 2 < count; ti += strip ? 1 : 3) {
        GLsizei ix[3];
        if (strip) {
            if ((ti & 1) == 0) { ix[0] = ti; ix[1] = ti + 1; ix[2] = ti + 2; }
            else               { ix[0] = ti + 1; ix[1] = ti; ix[2] = ti + 2; }
        } else {
            ix[0] = ti; ix[1] = ti + 1; ix[2] = ti + 2;
        }
        for (int k = 0; k < 3; ++k) {
            const int vi = idxOf(ix[k]);
            RVertex& r = rv[k];
            if (pk.kind == ProgramKind::Ui) {
                const float x = vf.attr(0, 0, vi, 0.0f);
                const float y = vf.attr(0, 1, vi, 0.0f);
                float clip[4];
                mat4ByVec(pk.proj, x, y, 0.0f, 1.0f, clip);
                ndcToWindow(clip, &r.x, &r.y, &r.z, &r.iw);
                r.u = vf.attr(1, 0, vi, 0.0f);
                r.v = vf.attr(1, 1, vi, 0.0f);
                r.cr = vf.attr(2, 0, vi, 1.0f);
                r.cg = vf.attr(2, 1, vi, 1.0f);
                r.cb = vf.attr(2, 2, vi, 1.0f);
                r.ca = vf.attr(2, 3, vi, 1.0f);
            } else if (pk.kind == ProgramKind::Grid) {
                // Grid.cpp VS: xz = uCenter.xz + aCorner*uExtent; o FRAGMENT
                // quer o vWorld — viaja no u/v (interpolado perspective-
                // correct pelo rasterGridTriangle)
                const float cx = vf.attr(0, 0, vi, 0.0f);
                const float cz = vf.attr(0, 1, vi, 0.0f);
                r.u = pk.center[0] + cx * pk.extent;
                r.v = pk.center[2] + cz * pk.extent;
                float clip[4];
                mat4ByVec(pk.gridVp, r.u, 0.0f, r.v, 1.0f, clip);
                ndcToWindow(clip, &r.x, &r.y, &r.z, &r.iw);
            } else {
                // lit: aPos/aNormal/aUV (+aJoints/aWeights se skinado);
                // vNormal = mat3(uModel)·(skin? skin·n) — normal EM MUNDO
                float px = vf.attr(0, 0, vi, 0.0f);
                float py = vf.attr(0, 1, vi, 0.0f);
                float pz = vf.attr(0, 2, vi, 0.0f);
                float nx = vf.attr(1, 0, vi, 0.0f);
                float ny = vf.attr(1, 1, vi, 0.0f);
                float nz = vf.attr(1, 2, vi, 1.0f);
                r.u = vf.attr(2, 0, vi, 0.0f);
                r.v = vf.attr(2, 1, vi, 0.0f);
                float skin[16];
                if (skinMatrix(vf.attr(3, 0, vi, 0.0f), vf.attr(3, 1, vi, 0.0f),
                               vf.attr(3, 2, vi, 0.0f), vf.attr(3, 3, vi, 0.0f),
                               vf.attr(4, 0, vi, 0.0f), vf.attr(4, 1, vi, 0.0f),
                               vf.attr(4, 2, vi, 0.0f), vf.attr(4, 3, vi, 0.0f),
                               skin)) {
                    float wpos[4];
                    mat4ByVec(skin, px, py, pz, 1.0f, wpos);
                    px = wpos[0]; py = wpos[1]; pz = wpos[2];
                    float ns[3];
                    mat3ByVec(skin, nx, ny, nz, ns);
                    nx = ns[0]; ny = ns[1]; nz = ns[2];
                }
                float nrmW[3];
                mat3ByVec(pk.model, nx, ny, nz, nrmW);
                r.nx = nrmW[0]; r.ny = nrmW[1]; r.nz = nrmW[2];
                float clip[4];
                mat4ByVec(mm, px, py, pz, 1.0f, clip);
                ndcToWindow(clip, &r.x, &r.y, &r.z, &r.iw);
            }
        }
        if (pk.kind == ProgramKind::Grid) {
            rasterGridTriangle(rv[0], rv[1], rv[2], pk);
        } else {
            rasterTriangle(rv[0], rv[1], rv[2], pk, tex);
        }
    }
}

inline void drawArrays(unsigned mode, GLint first, GLsizei count) {
    drawCore(mode, count, [&](GLsizei i) { return int(first) + int(i); });
}

inline void drawElements(unsigned mode, GLsizei count) {
    // os índices vivem no ELEMENT do VAO (Mesh::draw passa nullptr)
    drawCore(mode, count, [&](GLsizei i) -> int {
        const VaoObj* v = findVao_(curVao_);
        if (!v) return 0;
        const BufObj* eb = findBuf_(v->elemBuf);
        if (!eb || eb->bytes.size() < 2u * size_t(count)) return -1;
        uint16_t idx = 0;
        std::memcpy(&idx, eb->bytes.data() + size_t(i) * 2u, 2);
        return int(idx);
    });
}

inline void readPixels(int x, int y, int w, int h, void* dst) {
    if (!hasFb_ || !dst || w <= 0 || h <= 0) return;
    for (int row = 0; row < h; ++row) {
        const int sy = y + row;   // GL: origem no FUNDO-esquerda
        uint8_t* out = static_cast<uint8_t*>(dst) + size_t(row) * size_t(w) * 4u;
        if (sy < 0 || sy >= fbH_) {
            std::memset(out, 0, size_t(w) * 4u);
            continue;
        }
        for (int col = 0; col < w; ++col) {
            const int sx = x + col;
            uint8_t* d = out + size_t(col) * 4u;
            if (sx < 0 || sx >= fbW_) { std::memset(d, 0, 4); continue; }
            const size_t pi = size_t(sy) * fbW_ + size_t(sx);
            d[0] = color_[pi * 4];
            d[1] = color_[pi * 4 + 1];
            d[2] = color_[pi * 4 + 2];
            d[3] = color_[pi * 4 + 3];
        }
    }
}

// reset dos OBJETOS/estado (contexto que morre); `enabled` MANTÉM-SE
inline void resetState() {
    texs_.clear();
    bufs_.clear();
    vaos_.clear();
    progs_.clear();
    curTex_ = 0;
    curArrayBuf_ = 0;
    curElemBuf_ = 0;
    curVao_ = 0;
    curProg_ = 0;
    scissorOn_ = false;
    depthOn_ = false;
    cullOn_ = false;
    blendOn_ = false;
    depthMask_ = true;
    scissor_[0] = scissor_[1] = scissor_[2] = scissor_[3] = 0;
    clearColor_[0] = clearColor_[1] = clearColor_[2] = clearColor_[3] = 0.0f;
    hasFb_ = false;
    fbW_ = 0;
    fbH_ = 0;
    color_.clear();
    depth_.clear();
    viewport_[0] = viewport_[1] = viewport_[2] = viewport_[3] = 0;
}

}  // namespace fb
}  // namespace glstub
