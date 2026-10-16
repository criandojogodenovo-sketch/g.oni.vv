// tests/test_ctx_pines.cpp — os pins da campanha 0.11-CTX (o contexto
// enxuto: STATE.md/CODEMAP.md são ÍNDICES lidos sempre; o código é a
// verdade). Um pin por regra do prompt da campanha; os que fecham nas
// sub-fases CTX-2/CTX-3 entram nos commits delas.
//
// A raiz do repo vem do REPO_ROOT (o padrão da casa, 0.9.5 R-013). Para
// as MUTAÇÕES locais (M-CTX1a/2a/2b/3a/3b — o relatório cola a prova), o
// env GONI_ROOT_OVERRIDE aponta para uma CÓPIA do repo com o documento
// mutado; o pin corre contra a cópia e tem de ficar vermelho.
//
// O pin ctx_state_gate_fecha aferre o gate do §1.2 CONTRA COMMITS REAIS
// do histórico: d0c765d (3.5b: código sem STATE.md → vermelho),
// caaef02 (fix: código sem STATE.md → vermelho), cf6279a (docs: → isento)
// e a gama vazia (verde). O CI tem o histórico completo (fetch-depth 0).
#include "TestFramework.h"

#include <sys/wait.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {

std::string ctxRoot() {
    if (const char* e = std::getenv("GONI_ROOT_OVERRIDE")) return e;
#ifdef REPO_ROOT
    return REPO_ROOT;
#else
    return ".";
#endif
}

bool readFile(const std::string& path, std::string& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f.is_open()) return false;
    out.assign(std::istreambuf_iterator<char>(f),
               std::istreambuf_iterator<char>());
    return true;
}

std::vector<std::string> splitLines(const std::string& text) {
    std::vector<std::string> lines;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t nl = text.find('\n', pos);
        if (nl == std::string::npos) {
            if (pos < text.size()) lines.push_back(text.substr(pos));
            break;
        }
        lines.push_back(text.substr(pos, nl - pos));
        pos = nl + 1;
    }
    return lines;
}

// popen capturado (o gate STATE.md é um script bash — a saída importa:
// a frase "sub-fase declarada sem atualização do STATE.md" é o contrato)
std::string runCapture(const std::string& cmd, int& exitCode) {
    std::string out;
    FILE* p = ::popen(cmd.c_str(), "r");
    if (!p) {
        exitCode = -1;
        return "<popen falhou>";
    }
    char buf[512];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, p)) > 0) out.append(buf, n);
    int st = ::pclose(p);
    exitCode = st == -1 ? -1 : WEXITSTATUS(st);
    return out;
}

bool parseIntAfter(const std::string& text, const std::string& marker,
                   long& value) {
    size_t p = text.find(marker);
    if (p == std::string::npos) return false;
    p = text.find_first_not_of(" \t", p + marker.size());
    if (p == std::string::npos || !std::isdigit(static_cast<unsigned char>(text[p])))
        return false;
    value = std::strtol(text.c_str() + p, nullptr, 10);
    return true;
}

}  // namespace

// ctx_state_existe — o índice existe e é legível na raiz.
TEST(ctx_state_existe) {
    std::string text;
    EXPECT_MSG(readFile(ctxRoot() + "/STATE.md", text),
               "STATE.md tem de existir na raiz do repo");
    EXPECT(!text.empty());
}

// ctx_state_curto — <300 linhas (o índice não é o arquivo; se crescer,
// a dívida vai para o DIARIO/arquivo, não para o STATE).
TEST(ctx_state_curto) {
    std::string text;
    ASSERT(readFile(ctxRoot() + "/STATE.md", text));
    const long lines =
        static_cast<long>(std::count(text.begin(), text.end(), '\n'));
    EXPECT_MSG(lines < 300, "STATE.md com %ld linhas — o teto é 300",
               lines);
    EXPECT(lines > 0);
}

// ctx_state_seccoes — as 9 secções do contrato (§1..§9) presentes.
TEST(ctx_state_seccoes) {
    std::string text;
    ASSERT(readFile(ctxRoot() + "/STATE.md", text));
    for (int n = 1; n <= 9; ++n) {
        const std::string marker = "## §" + std::to_string(n);
        EXPECT_MSG(text.find(marker) != std::string::npos,
                   "STATE.md sem a secção '%s'", marker.c_str());
    }
}

// ctx_state_vc_bate — o vc do cabeçalho do STATE.md = o versionCode do
// build.gradle (o índice não mente sobre a versão corrente).
TEST(ctx_state_vc_bate) {
    std::string state, gradle;
    ASSERT(readFile(ctxRoot() + "/STATE.md", state));
    ASSERT(readFile(ctxRoot() + "/app/build.gradle", gradle));
    long vcState = -1, vcGradle = -1;
    ASSERT(parseIntAfter(state, "vc:", vcState));
    ASSERT(parseIntAfter(gradle, "versionCode", vcGradle));
    EXPECT_MSG(vcState == vcGradle,
               "STATE.md diz vc=%ld mas build.gradle diz %ld", vcState,
               vcGradle);
}

// ctx_state_gate_fecha — o gate do §1.2 existe e decide CERTO contra o
// histórico REAL: código sem STATE.md = vermelho (com a frase do
// contrato); docs = isento; gama vazia = verde. É a prova de que a regra
// "o STATE.md viaja com o código da sub-fase" está vigiada.
TEST(ctx_state_gate_fecha) {
    const std::string gate = ctxRoot() + "/scripts/docs_lint_state.sh";
    std::string probe;
    EXPECT_MSG(readFile(gate, probe), "falta %s", gate.c_str());
    ASSERT(probe.size() > 0);
    const char* frase = "sub-fase declarada sem atualização do STATE.md";

    // F1 (M-CTX1b viva): o commit 3.5b (código, sem STATE.md) → VERMELHO
    int code = 0;
    std::string out = runCapture(gate + " d0c765d^ d0c765d", code);
    EXPECT_MSG(code != 0, "o gate tem de ficar vermelho no 3.5b "
                          "(código sem STATE.md); saída: %s", out.c_str());
    EXPECT(out.find(frase) != std::string::npos);

    // F2: o fix caaef02 (código, sem STATE.md) → VERMELHO
    out = runCapture(gate + " caaef02^ caaef02", code);
    EXPECT_MSG(code != 0, "o gate tem de ficar vermelho no fix caaef02; "
                          "saída: %s", out.c_str());
    EXPECT(out.find(frase) != std::string::npos);

    // F3: o commit de docs (abertura da 0.11-W) → ISENTO (verde)
    out = runCapture(gate + " cf6279a^ cf6279a", code);
    EXPECT_MSG(code == 0, "commit de docs é isento; saída: %s",
               out.c_str());

    // F4: gama vazia → VERDE
    out = runCapture(gate + " caaef02 caaef02", code);
    EXPECT_MSG(code == 0, "gama vazia é verde; saída: %s", out.c_str());
}
