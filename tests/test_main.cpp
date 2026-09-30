// tests/test_main.cpp — runner da suíte do core (CI Linux).
#include "TestFramework.h"

int main() {
    std::printf("== testes do core — G.One VV 0.6.10 ==\n");
    for (const auto& c : ::test::cases()) {
        const int before = ::test::failures();
        c.fn();
        std::printf("%-46s %s\n", c.name,
                    before == ::test::failures() ? "OK" : "FALHOU");
    }
    const int f = ::test::failures();
    std::printf("\n%d teste(s) com falha\n", f);
    return f == 0 ? 0 : 1;
}
