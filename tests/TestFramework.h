#pragma once
// tests/TestFramework.h — micro-framework de testes (zero dependências).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "math/Math.h"

namespace test {

// helpers de comparação partilhados (float/vetor/matriz com epsilon)
inline bool nearEqF(vv::f32 a, vv::f32 b, vv::f32 eps = 1e-4f) {
    const vv::f32 d = a - b;
    return d < eps && d > -eps;
}

inline bool vecNearF(const vv::Vec3& a, const vv::Vec3& b, vv::f32 eps = 1e-4f) {
    return nearEqF(a.x, b.x, eps) && nearEqF(a.y, b.y, eps) && nearEqF(a.z, b.z, eps);
}

inline bool matNearF(const vv::Mat4& a, const vv::Mat4& b, vv::f32 eps = 1e-4f) {
    for (int i = 0; i < 16; ++i) {
        if (!nearEqF(a.m[i], b.m[i], eps)) return false;
    }
    return true;
}

struct Case {
    const char* name;
    void (*fn)();
};

// 0.12-Y (3.Y-D): o FONTE SEM COMENTÁRIOS (o padrão do ui_vocab_check) —
// os contratos de fonte (countSub sobre o .cpp) são CÓDIGO, não prosa: um
// comentário com a expressão NÃO satisfaz (a M-Yd1 provou o falso verde).
inline std::string stripCppComments(const std::string& src) {
    std::string out;
    out.reserve(src.size());
    bool inStr = false;
    for (size_t i = 0; i < src.size(); ++i) {
        if (inStr) {
            out += src[i];
            if (src[i] == '\\' && i + 1 < src.size()) {
                out += src[++i];
            } else if (src[i] == '"') {
                inStr = false;
            }
            continue;
        }
        if (src[i] == '"') {
            inStr = true;
            out += src[i];
            continue;
        }
        if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '/') {
            while (i < src.size() && src[i] != '\n') ++i;
            if (i < src.size()) out += '\n';
            continue;
        }
        if (src[i] == '/' && i + 1 < src.size() && src[i + 1] == '*') {
            i += 2;
            while (i + 1 < src.size() &&
                   !(src[i] == '*' && src[i + 1] == '/')) {
                if (src[i] == '\n') out += '\n';
                ++i;
            }
            ++i;
            continue;
        }
        out += src[i];
    }
    return out;
}


// 0.10-M (PASSO 5A): o teste EM CURSO (o handler de crash do test_main
// diz o NOME — o banner órfão do CI não dizia nada)
inline const char*& currentName() {
    static const char* n = nullptr;
    return n;
}
inline void setCurrentName(const char* n) { currentName() = n; }

inline std::vector<Case>& cases() {
    static std::vector<Case> c;
    return c;
}

inline int& failures() {
    static int f = 0;
    return f;
}

inline void expect(bool cond, const char* expr, const char* file, int line) {
    if (!cond) {
        ++failures();
        std::printf("  FALHOU  %s:%d  %s\n", file, line, expr);
    }
}

// 0.9.6.12 (R-022): EXPECT com MENSAGEM printf-style — a falha DIZ o caso
// (ecrã/drawer/números) sem exigir que a expressão seja o diagnóstico
template <typename... Args>
inline void expectMsgF(bool cond, const char* file, int line,
                       const char* fmt, Args&&... args) {
    if (!cond) {
        ++failures();
        char buf[512];
        std::snprintf(buf, sizeof(buf), fmt, args...);
        std::printf("  FALHOU  %s:%d  %s\n", file, line, buf);
    }
}

struct Registrar {
    Registrar(const char* name, void (*fn)()) { cases().push_back({name, fn}); }
};

} // namespace test

#define TEST(name)                                            \
    static void test_##name();                                \
    static ::test::Registrar reg_##name(#name, &test_##name); \
    static void test_##name()

#define EXPECT(expr) ::test::expect((expr), #expr, __FILE__, __LINE__)
#define EXPECT_MSG(expr, ...) \
    ::test::expectMsgF((expr), __FILE__, __LINE__, __VA_ARGS__)

// ASSERT: EXPECT que ABORTA o caso (pré-condições de teste — desreferenciar
// um ponteiro nulo a meio do caso é crash, não falha limpa)
#define ASSERT(expr)                                                          \
    do {                                                                      \
        if (!(expr)) {                                                        \
            ::test::expect((expr), #expr, __FILE__, __LINE__);               \
            return;                                                           \
        }                                                                     \
    } while (0)
