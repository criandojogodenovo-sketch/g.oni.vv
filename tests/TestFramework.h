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
