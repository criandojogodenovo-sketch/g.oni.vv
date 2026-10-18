// platform/EngineLog.cpp — log duplo logcat + ficheiro com rotação
// (F5.1-hotfix, parte 1.1). Ver EngineLog.h para o desenho.
//
// Rotação: quando engine.log atinge maxBytes, fecha e desloca
//   engine.log.2 → (apagado), engine.log.1 → engine.log.2,
//   engine.log → engine.log.1, e reabre um engine.log novo.
// O custo por linha é um lseek(SEEK_END) para saber o tamanho — o log do
// boot tem dezenas de linhas, nunca no caminho quente do frame.
#include "platform/EngineLog.h"

#include <algorithm>
#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef __ANDROID__
#include <android/log.h>
#endif

namespace vv::elog {

namespace {

std::mutex g_mu;
int        g_fd = -1;
std::string g_dir;            // "" = inativo
long       g_maxBytes = kDefaultMaxBytes;
int        g_backups = kDefaultBackups;

void closeLocked() {
    if (g_fd >= 0) {
        ::close(g_fd);
        g_fd = -1;
    }
}

bool openLocked() {
    if (g_dir.empty()) {
        return false;
    }
    closeLocked();
    char path[512];
    std::snprintf(path, sizeof(path), "%s/engine.log", g_dir.c_str());
    // O_APPEND: cada write é atómico (vários threads, sem interleave)
    g_fd = ::open(path, O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (g_fd < 0) {
        // F5.2: falha de fopen do PRÓPRIO log tem de dizer porquê — só
        // logcat (o ficheiro é que falhou); o self-check do boot (FileApi)
        // registra a mesma causa em linhas "self-check: … errno=…".
        const int e = errno;
#ifdef __ANDROID__
        __android_log_print(ANDROID_LOG_WARN, "GONI_VV",
                            "elog: open(%s) FALHOU — errno=%d (%s)",
                            path, e, std::strerror(e));
#else
        std::fprintf(stderr, "elog: open(%s) FALHOU — errno=%d (%s)\n",
                     path, e, std::strerror(e));
#endif
    }
    return g_fd >= 0;
}

void rotateLocked() {
    if (g_dir.empty()) {
        return;
    }
    closeLocked();
    char oldPath[512];
    char newPath[512];
    // .(backups-1) → apagado; .n → .(n+1); ativo → .1
    std::snprintf(oldPath, sizeof(oldPath), "%s/engine.log.%d",
                  g_dir.c_str(), g_backups);
    ::remove(oldPath);
    for (int i = g_backups - 1; i >= 1; --i) {
        std::snprintf(oldPath, sizeof(oldPath), "%s/engine.log.%d",
                      g_dir.c_str(), i);
        std::snprintf(newPath, sizeof(newPath), "%s/engine.log.%d",
                      g_dir.c_str(), i + 1);
        ::rename(oldPath, newPath);
    }
    std::snprintf(oldPath, sizeof(oldPath), "%s/engine.log", g_dir.c_str());
    std::snprintf(newPath, sizeof(newPath), "%s/engine.log.1", g_dir.c_str());
    ::rename(oldPath, newPath);
    openLocked();
}

void androidWrite(char level, const char* line) {
#ifdef __ANDROID__
    const int prio = (level == 'E') ? ANDROID_LOG_ERROR
                   : (level == 'W') ? ANDROID_LOG_WARN
                                    : ANDROID_LOG_INFO;
    __android_log_write(prio, "GONI_VV", line);
#else
    (void)level;
    (void)line;
#endif
}

} // namespace

bool init(const char* logsDir, long maxBytes, int backups) {
    std::lock_guard<std::mutex> lk(g_mu);
    g_maxBytes = maxBytes > 0 ? maxBytes : kDefaultMaxBytes;
    g_backups = backups > 0 ? backups : kDefaultBackups;
    if (!logsDir || !logsDir[0]) {
        g_dir.clear();
        closeLocked();
        return false;
    }
    // cria o diretório (e os pais que faltarem — externalDataPath existe,
    // mas o subdir logs/ não)
    std::string path(logsDir);
    for (size_t i = 1; i <= path.size(); ++i) {
        if (i == path.size() || path[i] == '/') {
            std::string sub = path.substr(0, i);
            ::mkdir(sub.c_str(), 0755);
        }
    }
    g_dir = path;
    return openLocked();
}

void shutdown() {
    std::lock_guard<std::mutex> lk(g_mu);
    closeLocked();
    g_dir.clear();
}

bool active() {
    std::lock_guard<std::mutex> lk(g_mu);
    return g_fd >= 0;
}

const char* dir() {
    std::lock_guard<std::mutex> lk(g_mu);
    return g_dir.c_str();
}

const char* androidFallbackDir() {
#ifdef __ANDROID__
    // applicationId fixo da app (build.gradle: applicationId 'vv.goni').
    // Usado ANTES do android_main ter os paths da activity (JNI_OnLoad):
    // getExternalFilesDir(null) == /storage/emulated/0/Android/data/vv.goni/files
    return "/storage/emulated/0/Android/data/vv.goni/files/logs";
#else
    return "";
#endif
}

void writeLine(char level, const char* line) {
    androidWrite(level, line);
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_fd < 0) {
        return;
    }
    // timestamp estilo logcat (MM-DD HH:MM:SS.mmm) — curto e ordenável
    struct timeval tv;
    ::gettimeofday(&tv, nullptr);
    struct tm tmv;
    ::localtime_r(&tv.tv_sec, &tmv);
    char prefix[48];
    std::snprintf(prefix, sizeof(prefix),
                  "%02d-%02d %02d:%02d:%02d.%03d %c/GONI: ",
                  tmv.tm_mon + 1, tmv.tm_mday,
                  tmv.tm_hour, tmv.tm_min, tmv.tm_sec,
                  static_cast<int>(tv.tv_usec / 1000), level);

    // 0.9.6.12g (A2-2 · FAZ 1 do dono): «Escreve a linha completa no log,
    // sem "…"» — as linhas de diagnóstico do import (a comparação inteira
    // de bufferView + o caminho do ficheiro) cabem INTEIRAS; 896 cortava
    // a linha do dono a meio dos números. O teto do logcat do Android
    // (~4 KB por linha) continua acima disto.
    char buf[2176];
    const int pre = static_cast<int>(std::strlen(prefix));
    int n = std::snprintf(buf, sizeof(buf), "%s%s\n", prefix, line);
    if (n < 0) {
        return;
    }
    // linha maior que o buffer → trunca (o log é diagnóstico, nunca crashes)
    if (n >= static_cast<int>(sizeof(buf))) {
        n = static_cast<int>(sizeof(buf)) - 1;
        buf[n - 1] = '\n';
        buf[n] = '\0';
    }
    if (::write(g_fd, buf, static_cast<size_t>(n)) < 0) {
        return;
    }
    // rotação por tamanho (a partir do fim do ficheiro agora)
    const long size = static_cast<long>(::lseek(g_fd, 0, SEEK_END));
    if (size >= g_maxBytes) {
        rotateLocked();
    }
}

namespace {

void vwrite(char level, const char* fmt, va_list ap) {
    // 0.9.6.12g (A2-2 · FAZ 1): 896 cortava a linha do import a meio dos
    // números (o dono lia «real=…» sem o resto) — 2048 carrega a linha
    // completa (o buf do writeLine é 2176 = 48 de prefixo + 2048 + \n)
    char line[2048];
    int n = std::vsnprintf(line, sizeof(line), fmt, ap);
    if (n < 0) {
        return;
    }
    if (n >= static_cast<int>(sizeof(line))) {
        line[sizeof(line) - 1] = '\0';
    }
    writeLine(level, line);
}

} // namespace

void info(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vwrite('I', fmt, ap);
    va_end(ap);
}

void warn(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vwrite('W', fmt, ap);
    va_end(ap);
}

void error(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vwrite('E', fmt, ap);
    va_end(ap);
}

// ---- F5.2: log viewer (Settings → "Ver logs") ------------------------------
//
// readTail: as ÚLTIMAS maxLines linhas do log (ativo + rotações), em ordem
// cronológica (a mais antiga primeiro, como no ficheiro). Linhas >
// kViewerLineMax chars são truncadas na leitura.
//
// 0.10-M (PASSO 3B): A ORDEM É O FICHEIRO ATIVO PRIMEIRO. A leitura antiga
// (.2 → .1 → ativo, com break ao encher maxLines) tinha o bug do «log de
// ontem»: com 1+ rotação o backup MAIS ANTIGO enchia a janela sozinho e o
// ficheiro ATIVO — o de HOJE, com o banner do boot e os erros da sessão —
// nunca era lido (o dono via conteúdo velho no viewer; o diagnóstico
// «ver causa no log» morria aí). A cura: lê-se o ativo; só quando FALTAM
// linhas se completar com o .1 e depois o .2, sempre pela CAUDA (as linhas
// mais recentes de cada backup) — o mais recente GANHA, nunca perde.
int readTail(std::vector<std::string>& out, int maxLines) {
    out.clear();
    if (maxLines <= 0) {
        return 0;
    }
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_dir.empty()) {
        return 0;
    }
    // ordem de leitura: ativo → .1 → .2 (o NOVO primeiro; cada backup só
    // entra quando faltam linhas — e entra PELA CAUDA, prependando)
    char path[512];
    std::vector<std::string> parts;
    std::snprintf(path, sizeof(path), "%s/engine.log", g_dir.c_str());
    parts.emplace_back(path);
    for (int i = 1; i <= g_backups; ++i) {
        std::snprintf(path, sizeof(path), "%s/engine.log.%d", g_dir.c_str(), i);
        parts.emplace_back(path);
    }

    constexpr long kWindow = 256L * 1024L;
    for (const std::string& p : parts) {
        if (static_cast<int>(out.size()) >= maxLines) {
            break;
        }
        FILE* f = std::fopen(p.c_str(), "rb");
        if (!f) {
            continue;   // backup ausente = normal (sem rotação ainda)
        }
        std::fseek(f, 0, SEEK_END);
        const long size = std::ftell(f);
        const long start = size > kWindow ? size - kWindow : 0;
        std::fseek(f, start, SEEK_SET);
        std::string data;
        if (size > start) {
            data.resize(static_cast<size_t>(size - start));
            const size_t got = std::fread(&data[0], 1, data.size(), f);
            data.resize(got);
        }
        std::fclose(f);
        // começa numa linha inteira (janela pode ter cortado a 1ª a meio)
        if (start > 0) {
            const size_t nl = data.find('\n');
            if (nl != std::string::npos) {
                data.erase(0, nl + 1);
            } else {
                data.clear();
            }
        }
        // separa linhas (mantém todas as deste ficheiro; o corte por
        // maxLines é feito ABAIXO, pela CAUDA — as linhas mais recentes)
        size_t pos = 0;
        std::vector<std::string> lines;
        while (pos < data.size()) {
            size_t nl = data.find('\n', pos);
            std::string line = data.substr(pos, nl == std::string::npos
                                                  ? std::string::npos
                                                  : nl - pos);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (!line.empty()) {
                if (static_cast<int>(line.size()) > kViewerLineMax) {
                    line.resize(kViewerLineMax);
                }
                lines.push_back(line);
            }
            if (nl == std::string::npos) {
                break;
            }
            pos = nl + 1;
        }
        // faltam maxLines - out.size() linhas: entra a CAUDA deste ficheiro
        // (o fim dele é o que LIGA ao já lido — ordem cronológica certa),
        // PREPENDADA no resultado (este ficheiro é MAIS ANTIGO que o lido)
        const int missing = maxLines - static_cast<int>(out.size());
        const size_t first =
            lines.size() > static_cast<size_t>(missing)
                ? lines.size() - static_cast<size_t>(missing) : 0;
        out.insert(out.begin(), lines.begin() + static_cast<long>(first),
                   lines.end());
    }
    return static_cast<int>(out.size());
}

bool listDumps(std::vector<std::string>& out) {
    out.clear();
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_dir.empty()) {
        return false;
    }
    DIR* d = ::opendir(g_dir.c_str());
    if (!d) {
        return false;
    }
    while (dirent* e = ::readdir(d)) {
        const std::string n = e->d_name;
        if (n.rfind("crash-", 0) == 0 &&
            n.size() > 5 && n.compare(n.size() - 5, 5, ".dump") == 0) {
            out.push_back(n);
        }
    }
    ::closedir(d);
    // mais RECENTE primeiro (timestamp decrescente do nome crash-<unixtime>)
    std::sort(out.begin(), out.end(), std::greater<std::string>());
    return true;
}

} // namespace vv::elog
