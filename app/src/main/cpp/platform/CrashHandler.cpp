// platform/CrashHandler.cpp — crash dump legível no device (F5.1-hotfix,
// parte 1.2). Ver CrashHandler.h para o desenho.
//
// POR QUE _Unwind_Backtrace e não backtrace(): o backtrace() da bionic só
// existe na API 33+ (C33 é API 31/32 — não existia no device-alvo).
// _Unwind_Backtrace (libunwind do toolchain) existe desde sempre e compila
// igual no hospedeiro (libgcc) — o mesmo código corre no device e no CI.
//
// POR QUE dladdr: dá ficheiro .so + símbolo dinâmico + base — o offset
// (pc - dli_fbase) é o que permite mapear a frame à função/linha sem
// ndk-stack (addr2line -e libgoni_vv.so <offset> resolve no CI quando
// houver PC).
#define _GNU_SOURCE 1

#include "platform/CrashHandler.h"

#include <cxxabi.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <link.h>
#include <signal.h>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <ucontext.h>
#include <unistd.h>
#include <unwind.h>

namespace vv {
namespace crash {

namespace {

constexpr int kMaxFrames = 64;

char g_dir[512] = {0};

// ---------------------------------------------------------------- unwind ----
struct FrameCtx {
    void** pcs;
    int    max;
    int    n;
};

_Unwind_Reason_Code unwindCallback(struct _Unwind_Context* ctx, void* data) {
    FrameCtx* fc = static_cast<FrameCtx*>(data);
    if (fc->n >= fc->max) {
        return _URC_END_OF_STACK;
    }
    uintptr_t ip = _Unwind_GetIP(ctx);
    if (ip != 0) {
        fc->pcs[fc->n++] = reinterpret_cast<void*>(ip);
    }
    return _URC_NO_REASON;
}

int captureFramesImpl(void** pcs, int max) {
    FrameCtx fc{pcs, max, 0};
    _Unwind_Backtrace(&unwindCallback, &fc);
    return fc.n;
}

// ------------------------------------------------------------------ dump ----
void dprint(int fd, const char* s) {
    if (s) {
        ::dprintf(fd, "%s", s);
    }
}

// escreve uma frame no formato tombstone-like: "#NN pc <off> <lib> (<sym>+<d>)"
void writeFrame(int fd, int idx, void* pc) {
    Dl_info info{};
    if (::dladdr(pc, &info) && info.dli_fname) {
        // nome curto do módulo (basename)
        const char* slash = std::strrchr(info.dli_fname, '/');
        const char* lib = slash ? slash + 1 : info.dli_fname;
        const unsigned long off =
            reinterpret_cast<unsigned long>(pc) -
            reinterpret_cast<unsigned long>(info.dli_fbase);
        if (info.dli_sname) {
            const unsigned long soff =
                reinterpret_cast<unsigned long>(pc) -
                reinterpret_cast<unsigned long>(info.dli_saddr);
            ::dprintf(fd, "#%02d pc 0x%lx  %s (%s+0x%lx)\n",
                      idx, off, lib, info.dli_sname, soff);
        } else {
            ::dprintf(fd, "#%02d pc 0x%lx  %s\n", idx, off, lib);
        }
    } else {
        ::dprintf(fd, "#%02d pc %p  <desconhecido>\n", idx, pc);
    }
}

} // namespace

const char* signalName(int sig) {
    switch (sig) {
        case SIGSEGV: return "SIGSEGV";
        case SIGABRT: return "SIGABRT";
        case SIGBUS:  return "SIGBUS";
        case SIGFPE:  return "SIGFPE";
        default:      return "SIGNAL";
    }
}

int captureFrames(void** pcs, int max) {
    return captureFramesImpl(pcs, max);
}

int writeDumpFromFrames(const char* path, const char* sigName, int sig,
                        void* faultAddr, void* const* pcs, int n) {
    if (!path || !path[0]) {
        return -1;
    }
    const int fd = ::open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd < 0) {
        return -2;
    }
    dprint(fd, "G.One VV — crash dump (legível sem ndk-stack)\n");
    if (sigName) {
        ::dprintf(fd, "signal: %s (%d)\n", sigName, sig);
    }
    if (faultAddr) {
        ::dprintf(fd, "si_addr: %p\n", faultAddr);
    }
    // pc/lr/sp do contexto — só no device arm64 (no hospedeiro não há uctx)
#ifdef __aarch64__
    // (preenchido pelo handler — ver abaixo; aqui o dump "de teste" omite)
#endif
    ::dprintf(fd, "frames: %d\n", n > 0 ? n : 0);
    for (int i = 0; i < n; ++i) {
        writeFrame(fd, i, pcs[i]);
    }
    ::dprintf(fd, "nota: offset 0x… = pc - base do módulo; addr2line -e "
                  "libgoni_vv.so <offset> resolve a linha no CI.\n");
    ::close(fd);
    return 0;
}

namespace {

void crashHandler(int sig, siginfo_t* info, void* uctx) {
    // 1. dump próprio: <dir>/crash-<unixtime>.dump
    char path[600];
    ::snprintf(path, sizeof(path), "%s/crash-%ld.dump",
               g_dir[0] ? g_dir : "/data/local/tmp", static_cast<long>(::time(nullptr)));

    void* pcs[kMaxFrames];
    FrameCtx fc{pcs, kMaxFrames, 0};
    _Unwind_Backtrace(&unwindCallback, &fc);

    const int fd = ::open(path, O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (fd >= 0) {
        dprint(fd, "G.One VV — crash dump (legível sem ndk-stack)\n");
        ::dprintf(fd, "signal: %s (%d)\n", signalName(sig), sig);
        if (info) {
            ::dprintf(fd, "si_addr: %p  si_code: %d\n", info->si_addr, info->si_code);
        }
        // registos do contexto (arm64) — pc do FAULT (a frame 0 do unwind
        // aponta para o handler; o pc real do crash está no ucontext)
#ifdef __aarch64__
        const ucontext_t* uc = static_cast<const ucontext_t*>(uctx);
        if (uc) {
            ::dprintf(fd, "pc: 0x%llx  lr: 0x%llx  sp: 0x%llx\n",
                      static_cast<unsigned long long>(uc->uc_mcontext.pc),
                      static_cast<unsigned long long>(uc->uc_mcontext.regs[30]),
                      static_cast<unsigned long long>(uc->uc_mcontext.sp));
            // resolve o pc do FAULT com dladdr — a linha mais importante do dump
            writeFrame(fd, -1, reinterpret_cast<void*>(uc->uc_mcontext.pc));
        }
#elif defined(__x86_64__)
        const ucontext_t* uc = static_cast<const ucontext_t*>(uctx);
        if (uc) {
            const unsigned long rip =
                static_cast<unsigned long>(uc->uc_mcontext.gregs[REG_RIP]);
            ::dprintf(fd, "pc: 0x%lx\n", rip);
            writeFrame(fd, -1, reinterpret_cast<void*>(rip));
        }
#endif
        ::dprintf(fd, "frames: %d\n", fc.n);
        for (int i = 0; i < fc.n; ++i) {
            writeFrame(fd, i, pcs[i]);
        }
        ::dprintf(fd, "log: %s/engine.log (boot progress [boot N/6] lá dentro)\n",
                  g_dir[0] ? g_dir : "?");
        ::dprintf(fd, "nota: offset 0x… = pc - base do módulo; addr2line -e "
                      "libgoni_vv.so <offset> resolve a linha no CI.\n");
        ::close(fd);
    }

    // 2. marca no engine.log (append atómico — O_APPEND; open/write cru,
    //    async-signal-safe)
    if (g_dir[0]) {
        char logPath[600];
        ::snprintf(logPath, sizeof(logPath), "%s/engine.log", g_dir);
        const int lfd = ::open(logPath, O_WRONLY | O_APPEND | O_CREAT, 0644);
        if (lfd >= 0) {
            ::dprintf(lfd, "CRASH %s (%d) — dump: %s\n", signalName(sig), sig, path);
            ::close(lfd);
        }
    }

    // 3. re-raise com handler padrão — o sistema gera o tombstone normal
    ::signal(sig, SIG_DFL);
    ::raise(sig);
}

} // namespace

void install(const char* logsDir) {
    if (!logsDir || !logsDir[0]) {
        return;
    }
    std::snprintf(g_dir, sizeof(g_dir), "%s", logsDir);

    // stack alternativo: SIGSEGV por stack-overflow não teria stack para o
    // próprio handler — sigaltstack dá uma pilha de emergência
    // (64KB fixo — SIGSTKSZ não é constante de compilação no glibc 2.34+)
    static char altStack[64 * 1024];
    stack_t ss;
    ss.ss_sp = altStack;
    ss.ss_size = sizeof(altStack);
    ss.ss_flags = 0;
    ::sigaltstack(&ss, nullptr);

    struct sigaction sa{};
    sa.sa_sigaction = &crashHandler;
    sa.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&sa.sa_mask);
    ::sigaction(SIGSEGV, &sa, nullptr);
    ::sigaction(SIGABRT, &sa, nullptr);
    ::sigaction(SIGBUS,  &sa, nullptr);
    ::sigaction(SIGFPE,  &sa, nullptr);
}

} // namespace crash
} // namespace vv
