// platform/BuildInfo.cpp — identidade da build (0.8.10). Ver BuildInfo.h.
#include "platform/BuildInfo.h"

#include <cstdio>
#include <cstring>

namespace vv::buildinfo {

char g_version[32] = "dev";
u32  g_versionCode = 0;
char g_git[17] = "";
char g_soSha[72] = "";
u64  g_epoch = 0;

void set(const char* version, u32 code, const char* git, const char* soSha,
         u64 epoch) {
    if (version) {
        std::snprintf(g_version, sizeof(g_version), "%s", version);
    }
    g_versionCode = code;
    if (git) {
        std::snprintf(g_git, sizeof(g_git), "%s", git);
    }
    if (soSha) {
        std::snprintf(g_soSha, sizeof(g_soSha), "%s", soSha);
    }
    g_epoch = epoch;
}

std::string banner() {
    // 0.8.12 — formato exigido pelo dono: "boot: G.One VV <versão>
    // versionCode <N> sha256 <…>" (o sha256 REAL da .so vem da JNI — o CI
    // em 2 passes escreve build_info.txt). Os campos que faltam (git,
    // epoch) seguem-se quando existem; no host/dev só a versão + code.
    char buf[200];
    if (g_soSha[0]) {
        std::snprintf(buf, sizeof(buf),
                      "boot: G.One VV %s versionCode %u sha256 %s%s%s",
                      g_version, g_versionCode, g_soSha,
                      g_git[0] ? " git " : "", g_git);
    } else {
        std::snprintf(buf, sizeof(buf),
                      "boot: G.One VV %s versionCode %u%s%s",
                      g_version, g_versionCode,
                      g_git[0] ? " git " : "", g_git);
    }
    return std::string(buf);
}

std::string dumpSuffix() {
    if (g_versionCode == 0) {
        return "";
    }
    char buf[16];
    std::snprintf(buf, sizeof(buf), "-vc%u", g_versionCode);
    return std::string(buf);
}

bool dumpIsFromOtherBuild(const std::string& dumpName) {
    if (g_versionCode == 0) {
        return false;   // host/dev: tudo é "da mesma" build
    }
    const size_t p = dumpName.rfind("-vc");
    if (p == std::string::npos) {
        return true;   // dump pré-0.8.10 (sem identidade) = ANTIGO
    }
    u32 vc = 0;
    for (size_t i = p + 3; i < dumpName.size() &&
                           dumpName[i] >= '0' && dumpName[i] <= '9'; ++i) {
        vc = vc * 10 + static_cast<u32>(dumpName[i] - '0');
    }
    return vc != g_versionCode;
}

// 0.8.12 — badge do log viewer para UM dump: "" (mesma build),
// " [ANTIGO (build N)]" (vc do nome != instalado) ou " [ANTIGO (pré-0.8.10)]"
// (dump sem identidade). O dono com o dump velho no ecrã vê DE IMEDIATO
// que não é desta build — os offsets idênticos deixam de ser mistério.
std::string dumpBadge(const std::string& dumpName) {
    if (!dumpIsFromOtherBuild(dumpName)) {
        return "";
    }
    const size_t p = dumpName.rfind("-vc");
    if (p == std::string::npos) {
        return "  [ANTIGO (pre-0.8.10)]";
    }
    u32 vc = 0;
    bool any = false;
    for (size_t i = p + 3; i < dumpName.size() &&
                           dumpName[i] >= '0' && dumpName[i] <= '9'; ++i) {
        vc = vc * 10 + static_cast<u32>(dumpName[i] - '0');
        any = true;
    }
    char buf[48];
    if (any) {
        std::snprintf(buf, sizeof(buf), "  [ANTIGO (build %u)]", vc);
    } else {
        std::snprintf(buf, sizeof(buf), "  [ANTIGO (build ?)]");
    }
    return std::string(buf);
}

} // namespace vv::buildinfo
