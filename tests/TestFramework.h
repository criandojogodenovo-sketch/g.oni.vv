#pragma once
// tests/TestFramework.h — micro-framework de testes (zero dependências).
#include <cstdio>
#include <vector>

namespace test {

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

struct Registrar {
    Registrar(const char* name, void (*fn)()) { cases().push_back({name, fn}); }
};

} // namespace test

#define TEST(name)                                            \
    static void test_##name();                                \
    static ::test::Registrar reg_##name(#name, &test_##name); \
    static void test_##name()

#define EXPECT(expr) ::test::expect((expr), #expr, __FILE__, __LINE__)
