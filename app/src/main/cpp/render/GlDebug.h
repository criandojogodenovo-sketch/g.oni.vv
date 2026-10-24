#pragma once
// render/GlDebug.h — 0.12-Z FASE 0 (DIAGNÓSTICO): a VARA DE MEDIR do
// contexto GL. NÃO muda comportamento de render nenhum — só OBSERVA:
//
//   1. CONTADORES de recursos GL (programas/buffers/VAOs/texturas/FBOs)
//      criados e destruídos por camada (Renderer/Grid/Sky/Particles/Font/
//      Mesh/Texture) — o boot e o TERM logam o resumo (o dono vê quantas
//      vezes cada coisa nasce/morre ao sair e voltar ao app).
//   2. SNAPSHOT do ESTADO GL à entrada e à saída de CADA passe (viewport,
//      scissor, blend, depth test/write/func, cull, FBO ligado) — um passe
//      que vaza estado aparece como diff à saída (o log só na MUDANÇA).
//   3. glGetError depois de cada passe (gate próprio) e aviso de uniform
//      location -1 na init dos shaders.
//   4. OS INTERRUPTORES POR PASSE (nas Definições → Diagnóstico): master
//      desligado por defeito = render normal byte a byte. Com o master ON,
//      só os passes ligados desenham — "tudo off menos a grelha" tem de
//      mostrar SEMPRE a grelha (o dono liga um a um até a sombra aparecer
//      e reporta QUAL passe a cria).
//
// Tudo em RUNTIME (sem macro de build): os toggles são o modo debug, de
// sessão (nunca serializados), e o custo com master OFF é zero branches
// além de UM bool por passe. GLES 3.0 only; no hospedeiro o stub responde.
#include "core/Types.h"
#include "platform/EngineLog.h"
#include <GLES3/gl3.h>
#include <cstdio>
#include <cstring>

namespace vv::gldbg {

// ---- OS INTERRUPTORES (sessão — nunca serializados; default = normal) ----
struct Masks {
    bool master   = false;   // OFF: o render corre EXATAMENTE como sempre
    bool sky      = true;    // fundo/céu (o gradiente procedural)
    bool worldEnv = true;    // o Environment (céu/clear/luz de jogo)
    bool geometry = true;    // as malhas dos TICs (pass 3D)
    bool grid     = true;    // a grelha de chão
    bool gizmos   = true;    // o gizmo do editor
    bool post     = true;    // pós-processamento: NÃO EXISTE no motor (0 FBOs
                             // — verificado no F0); o switch é no-op DOCUMEN-
                             // TADO e fica para o dono ver o inventário
    bool ui       = true;    // painéis do editor (a toolbar do topo e as
                             // Definições ficam SEMPRE vivas — a via de
                             // escape para reconfigurar)
    bool uiGlass  = true;    // o "vidro" da UI (glassTopLine/ramo de vidro)
};
inline Masks g_masks;
inline bool g_stateLog = false;   // log do estado à entrada/saída dos passes
inline bool g_errorLog = false;   // glGetError por passe

// ---- CONTADORES (por kind; o resumo vive no engine.log) ------------------
enum class Kind : u32 { Program = 0, Buffer, Vao, Texture, Fbo, Count_ };
inline u32 g_created[static_cast<u32>(Kind::Count_)] = {0, 0, 0, 0, 0};
inline u32 g_destroyed[static_cast<u32>(Kind::Count_)] = {0, 0, 0, 0, 0};

inline void onCreated(Kind k, u32 n) {
    g_created[static_cast<u32>(k)] += n;
}
inline void onDestroyed(Kind k, u32 n) {
    g_destroyed[static_cast<u32>(k)] += n;
}
inline u32 createdOf(Kind k) {
    return g_created[static_cast<u32>(k)];
}
inline u32 destroyedOf(Kind k) {
    return g_destroyed[static_cast<u32>(k)];
}
inline void resetCounters() {
    for (u32 i = 0; i < static_cast<u32>(Kind::Count_); ++i) {
        g_created[i] = 0;
        g_destroyed[i] = 0;
    }
}
inline void summaryLine(char* out, u32 cap) {
    std::snprintf(out, cap,
                  "prog +%u/-%u buf +%u/-%u vao +%u/-%u tex +%u/-%u "
                  "fbo +%u/-%u",
                  g_created[0], g_destroyed[0], g_created[1], g_destroyed[1],
                  g_created[2], g_destroyed[2], g_created[3], g_destroyed[3],
                  g_created[4], g_destroyed[4]);
}

// ---- O ESTADO GL POR PASSE ----------------------------------------------
struct PassState {
    GLint viewport[4];
    GLboolean scissor;
    GLboolean blend;
    GLboolean depthTest;
    GLboolean cullFace;
    GLint depthMask;
    GLint depthFunc;
    GLint fbo;
};

inline PassState capture() {
    PassState s;
    glGetIntegerv(GL_VIEWPORT, s.viewport);
    s.scissor    = glIsEnabled(GL_SCISSOR_TEST);
    s.blend      = glIsEnabled(GL_BLEND);
    s.depthTest  = glIsEnabled(GL_DEPTH_TEST);
    s.cullFace   = glIsEnabled(GL_CULL_FACE);
    glGetIntegerv(GL_DEPTH_WRITEMASK, &s.depthMask);
    glGetIntegerv(GL_DEPTH_FUNC, &s.depthFunc);
    GLint fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
    s.fbo = fbo;
    return s;
}

inline bool differs(const PassState& a, const PassState& b) {
    return std::memcmp(&a, &b, sizeof(PassState)) != 0;
}

inline void describe(const PassState& s, char* out, u32 cap) {
    std::snprintf(out, cap,
                  "vp(%d,%d,%d,%d) scissor%d blend%d depth%d(w%d,fn0x%x) "
                  "cull%d fbo%d",
                  s.viewport[0], s.viewport[1], s.viewport[2], s.viewport[3],
                  s.scissor ? 1 : 0, s.blend ? 1 : 0, s.depthTest ? 1 : 0,
                  s.depthMask, s.depthFunc, s.cullFace ? 1 : 0, s.fbo);
}

// a FRONTEIRA do passe: chama-se no FIM de cada passe com o estado à
// entrada; loga SÓ quando o passe alterou o estado (o vazamento é o sinal)
inline void passBoundary(const char* name, const PassState& in,
                         const PassState& out) {
    if (!g_stateLog) {
        return;
    }
    if (!differs(in, out)) {
        return;
    }
    char a[128];
    char b[128];
    describe(in, a, sizeof(a));
    describe(out, b, sizeof(b));
    elog::warn("gldebug: passe %s ALTEROU estado: %s -> %s", name, a, b);
}

inline void checkErrors(const char* where) {
    if (!g_errorLog) {
        return;
    }
    GLenum e = glGetError();
    while (e != GL_NO_ERROR) {
        elog::warn("gldebug: GL error 0x%04x em %s", static_cast<u32>(e),
                   where);
        e = glGetError();
    }
}

// o aviso do uniform: -1 é legal em GLES (optimizado fora) mas o dono pediu
// o aviso — o log é 1× por init, nunca por frame
inline void checkUniformLoc(i32 loc, const char* name, const char* pass) {
    if (loc < 0) {
        elog::info("gldebug: uniform %s = -1 no shader do passe %s "
                   "(optimizado fora ou nome errado)", name, pass);
    }
}

} // namespace vv::gldbg
