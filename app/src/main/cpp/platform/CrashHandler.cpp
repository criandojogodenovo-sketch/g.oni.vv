#include "platform/CrashHandler.h"
#include <csignal>
#include <cstdio>
#include <fcntl.h>
#include <ucontext.h>
#include <unistd.h>

namespace vv {

namespace {

char g_logPath[512] = {0};

const char* signalName(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        default:      return "SIGNAL";
    }
}

// Handler async-signal-safe: apenas open/dprintf/close (sem malloc do core).
void crashHandler(int sig, siginfo_t* info, void* uctx) {
    const int fd = ::open(g_logPath, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd >= 0) {
        ::dprintf(fd, "G.One VV 0.1.0 — crash log\n");
        ::dprintf(fd, "signal: %d (%s)\n", sig, signalName(sig));
        if (info) {
            ::dprintf(fd, "si_addr: %p\n", info->si_addr);
        }
#ifdef __aarch64__
        const ucontext_t* uc = static_cast<const ucontext_t*>(uctx);
        if (uc) {
            ::dprintf(fd, "pc: 0x%llx\n",
                      static_cast<unsigned long long>(uc->uc_mcontext.pc));
            ::dprintf(fd, "lr: 0x%llx\n",
                      static_cast<unsigned long long>(uc->uc_mcontext.regs[30]));
            ::dprintf(fd, "sp: 0x%llx\n",
                      static_cast<unsigned long long>(uc->uc_mcontext.sp));
        }
#endif
        ::dprintf(fd, "fase: F1 (fundação)\n");
        ::close(fd);
    }
    // Re-raise com handler padrão — o sistema gera o tombstone normal.
    ::signal(sig, SIG_DFL);
    ::raise(sig);
}

} // namespace

void installCrashHandler(const char* internalDataPath) {
    if (!internalDataPath || !internalDataPath[0]) {
        return;
    }
    std::snprintf(g_logPath, sizeof(g_logPath), "%s/goni_crash.log", internalDataPath);

    struct sigaction sa{};
    sa.sa_sigaction = &crashHandler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    ::sigaction(SIGSEGV, &sa, nullptr);
    ::sigaction(SIGABRT, &sa, nullptr);
}

} // namespace vv
