#pragma once
// platform/EngineLog.h — log da engine DUPLICO (F5.1-hotfix, parte 1.1).
//
// O dono não tem PC/logcat: o diagnóstico tem de viver EM FICHEIROS no
// armazenamento. Este wrapper faz DUAS coisas em cada chamada:
//   1. __android_log_write (logcat, como sempre — útil em dev com adb);
//   2. append a <dir>/engine.log com rotação por tamanho (3 ficheiros de
//      1MB: engine.log ativo + engine.log.1 + engine.log.2; o mais velho
//      é descartado).
//
// O ficheiro é o que sobrevive ao crash: cada passo crítico do arranque
// escreve um marcador "[boot N/6]" (main.cpp) e o CrashHandler escreve
// crash-<timestamp>.dump no MESMO diretório (parte 1.2).
//
// Compila no hospedeiro (CI Linux) — __android_log_write só sob __ANDROID__.
// Thread-safe (mutex); NÃO usar dentro do signal handler (o handler usa
// open/write cru — ver CrashHandler.cpp).

namespace vv::elog {

// limites da rotação (defaults: 3 ficheiros de 1MB — ativo + 2 backups)
constexpr long kDefaultMaxBytes = 1024L * 1024L;
constexpr int  kDefaultBackups  = 2;

// caminho relativo no Downloads público usado pelo export (parte 1.4).
// Environment.DIRECTORY_DOWNLOADS == "Download" — constante única aqui
// para o CI poder aferir o valor exato que o Java recebe por JNI.
constexpr const char* kDownloadsRelPath = "Download/GOneVV/logs";

// ativa o log em ficheiro no diretório dado (cria o diretório se faltar).
// null/vazio → fica SÓ logcat (nunca crasha). Pode ser chamado 2× (o boot
// chama com o fallback do JNI_OnLoad e depois com o path canónico) — o fd
// é reaberto. Devolve true se o ficheiro ficou ativo.
bool init(const char* logsDir, long maxBytes = kDefaultMaxBytes,
          int backups = kDefaultBackups);

// fecha o fd (fim do processo; não obrigatório)
void shutdown();

// true se o append a ficheiro está ativo
bool active();

// diretório corrente ("" se inativo) — o CrashHandler usa para os dumps
const char* dir();

// diretório fallback usado NO DEVICE quando o boot ainda não chamou init
// (JNI_OnLoad corre antes do android_main): /storage/emulated/0/Android/
// data/vv.goni/files/logs. No hospedeiro devolve "".
const char* androidFallbackDir();

// níveis (o dono lê o ficheiro cru — prefixo I/W/E como no logcat)
void info(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void warn(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void error(const char* fmt, ...) __attribute__((format(printf, 1, 2)));

// núcleo sem formatação (o CrashHandler e os testes usam diretamente);
// level = 'I' | 'W' | 'E'. Escreve "<ts> <level>/GONI: <line>\n".
void writeLine(char level, const char* line);

} // namespace vv::elog
