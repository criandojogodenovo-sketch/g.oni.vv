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
    char buf[160];
    std::snprintf(buf, sizeof(buf),
                  "boot: goni-vv %s (versionCode %u%s%s%s)",
                  g_version, g_versionCode, g_git[0] ? ", git " : "", g_git,
                  g_soSha[0] ? ", so ok" : "");
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

} // namespace vv::buildinfo
