// tests/test_main.cpp — runner da suíte do core (CI Linux).
//
// 0.10-M (PASSO 5A): o CI apanhou um SEGFAULT cujo teste não dizia o
// NOME no output do ctest (o banner morria a meio). O handler de SIGSEGV/
// SIGBUS/SIGFPE abaixo imprime o teste EM CURSO + o backtrace bruto
// (backtrace_symbols; -rdynamic dá os símbolos) — a próxima falha diz
// ficheiro+função, não um banner órfão.
#include "TestFramework.h"
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <execinfo.h>

namespace {

void testCrashHandler(int sig) {
    // ASYNC-SIGNAL-SAFETY: só write() e backtrace (o backtrace não é
    // oficialmente async-safe, mas é o que existe — morrendo de qualquer
    // forma, o risco é aceitável e o ganho é a CAUSA no log do CI)
    const char* nome = ::test::currentName();
    static char buf[256];
    int n = std::snprintf(buf, sizeof(buf),
                          "\n[CRASH sinal %d no teste '%s'] backtrace:\n",
                          sig, nome ? nome : "?");
    ::write(2, buf, static_cast<size_t>(n));
    void* bt[32];
    const int m = ::backtrace(bt, 32);
    ::backtrace_symbols_fd(bt, m, 2);
    ::_exit(70 + sig);
}

} // namespace

int main() {
    std::signal(SIGSEGV, testCrashHandler);
    std::signal(SIGBUS, testCrashHandler);
    std::signal(SIGFPE, testCrashHandler);
    std::signal(SIGILL, testCrashHandler);
    std::printf("== testes do core — G.One VV 0.7.7 ==\n");
    for (const auto& c : ::test::cases()) {
        const int before = ::test::failures();
        std::printf("%-46s ...", c.name);
        std::fflush(stdout);
        ::test::setCurrentName(c.name);
        c.fn();
        ::test::setCurrentName(nullptr);
        std::printf("\r%-46s %s\n", c.name,
                    before == ::test::failures() ? "OK" : "FALHOU");
        std::fflush(stdout);
    }
    const int f = ::test::failures();
    std::printf("\n%d teste(s) com falha\n", f);
    return f == 0 ? 0 : 1;
}
