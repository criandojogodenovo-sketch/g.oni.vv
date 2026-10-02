// tests/test_main.cpp — runner da suíte do core (CI Linux).
#include "TestFramework.h"

int main() {
    std::printf("== testes do core — G.One VV 0.7.7 ==\n");
    for (const auto& c : ::test::cases()) {
        const int before = ::test::failures();
        std::printf("%-46s ...", c.name);
        std::fflush(stdout);
        c.fn();
        std::printf("\r%-46s %s\n", c.name,
                    before == ::test::failures() ? "OK" : "FALHOU");
        std::fflush(stdout);
    }
    const int f = ::test::failures();
    std::printf("\n%d teste(s) com falha\n", f);
    return f == 0 ? 0 : 1;
}
