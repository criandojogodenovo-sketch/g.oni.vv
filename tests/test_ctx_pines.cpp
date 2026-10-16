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
#include <regex>
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
    // (o gate corre via `bash` — o bit de execução do checkout não é
    //  precondição do pin; o blob tem 100755 e o CI chama-o igual)
    int code = 0;
    std::string out = runCapture("bash " + gate + " d0c765d^ d0c765d", code);
    EXPECT_MSG(code != 0, "o gate tem de ficar vermelho no 3.5b "
                          "(código sem STATE.md); saída: %s", out.c_str());
    EXPECT(out.find(frase) != std::string::npos);

    // F2: o fix caaef02 (código, sem STATE.md) → VERMELHO
    out = runCapture("bash " + gate + " caaef02^ caaef02", code);
    EXPECT_MSG(code != 0, "o gate tem de ficar vermelho no fix caaef02; "
                          "saída: %s", out.c_str());
    EXPECT(out.find(frase) != std::string::npos);

    // F3: o commit de docs (abertura da 0.11-W) → ISENTO (verde)
    out = runCapture("bash " + gate + " cf6279a^ cf6279a", code);
    EXPECT_MSG(code == 0, "commit de docs é isento; saída: %s",
               out.c_str());

    // F4: gama vazia → VERDE
    out = runCapture("bash " + gate + " caaef02 caaef02", code);
    EXPECT_MSG(code == 0, "gama vazia é verde; saída: %s", out.c_str());
}

// ---------------------------------------------------------------------------
// 0.11-CTX CTX-2 — os pins do CODEMAP.md (o mapa do código; índice, não
// verdade). Os paths citados têm de existir (o pin corre git ls-files) e
// seguem UMA convenção (relativa à raiz do repo).
// ---------------------------------------------------------------------------

namespace {

// O §3 do CODEMAP (as receitas) — texto entre "## §3" e "## §4".
std::string codemapSection(const std::string& text, const std::string& from,
                           const std::string& to) {
    const size_t a = text.find(from);
    if (a == std::string::npos) return "";
    const size_t b = text.find(to, a);
    return text.substr(a, b == std::string::npos ? std::string::npos : b - a);
}

bool lsFilesHas(const std::string& root, const std::string& rel) {
    const std::string cmd = "git -C " + root + " ls-files --error-unmatch -- "
                            "'" + rel + "' >/dev/null 2>&1";
    int code = 0;
    runCapture(cmd, code);
    return code == 0;
}

}  // namespace

// ctx_codemap_existe — o mapa existe na raiz.
TEST(ctx_codemap_existe) {
    std::string text;
    EXPECT_MSG(readFile(ctxRoot() + "/CODEMAP.md", text),
               "CODEMAP.md tem de existir na raiz do repo");
    EXPECT(!text.empty());
}

// ctx_codemap_curto — <400 linhas (o mapa não é a arquitetura; essa vive
// no ARCHITECTURE.md).
TEST(ctx_codemap_curto) {
    std::string text;
    ASSERT(readFile(ctxRoot() + "/CODEMAP.md", text));
    const long lines =
        static_cast<long>(std::count(text.begin(), text.end(), '\n'));
    EXPECT_MSG(lines < 400, "CODEMAP.md com %ld linhas — o teto é 400",
               lines);
}

// ctx_codemap_seccoes — as 6 secções do contrato (§1..§6) presentes.
TEST(ctx_codemap_seccoes) {
    std::string text;
    ASSERT(readFile(ctxRoot() + "/CODEMAP.md", text));
    for (int n = 1; n <= 6; ++n) {
        const std::string marker = "## §" + std::to_string(n);
        EXPECT_MSG(text.find(marker) != std::string::npos,
                   "CODEMAP.md sem a secção '%s'", marker.c_str());
    }
}

// ctx_codemap_paths_uniformes — UMA convenção: nenhum caminho de camada
// cpp citado à moda relativa-a-cpp/ (voni/..., core/...) — tem de ser
// app/src/main/cpp/<camada>/... (relativa à raiz, o que o cabeçalho declara).
TEST(ctx_codemap_paths_uniformes) {
    std::string text;
    ASSERT(readFile(ctxRoot() + "/CODEMAP.md", text));
    static const std::regex bare(
        "([^A-Za-z0-9_/.-]|^)((?:voni|core|render|assets|ui|platform|"
        "components)/[A-Za-z0-9_][A-Za-z0-9_./-]*)");
    auto begin = std::sregex_iterator(text.begin(), text.end(), bare);
    auto end = std::sregex_iterator();
    long bad = 0;
    for (auto it = begin; it != end; ++it) {
        const size_t p = static_cast<size_t>(it->position(2));
        const size_t lead = 17;  // "app/src/main/cpp/" tem 17 chars
        const bool cppPrefixed =
            p >= lead && text.compare(p - lead, lead, "app/src/main/cpp/") == 0;
        if (!cppPrefixed) {
            ++bad;
            EXPECT_MSG(false, "path fora da convenção (relativo à raiz): %s",
                       it->str(2).c_str());
        }
    }
    EXPECT(bad == 0);
}

// ctx_codemap_ficheiros_existem — cada ficheiro citado (com prefixo de
// caminho do repo) existe no índice do git; os nomes nus citados no mapa
// também. O pin corre git ls-files e compara (o contrato §2.4 da CTX-2).
TEST(ctx_codemap_ficheiros_existem) {
    const std::string root = ctxRoot();
    std::string text;
    ASSERT(readFile(root + "/CODEMAP.md", text));

    // 1) os caminhos com prefixo citados no CODEMAP
    static const std::regex path(
        "((?:app/src/main/cpp|app/src/main/java|tests|docs|ci|scripts)/"
        "[A-Za-z0-9_][A-Za-z0-9_./+-]*)");
    auto begin = std::sregex_iterator(text.begin(), text.end(), path);
    auto end = std::sregex_iterator();
    std::vector<std::string> cited;
    for (auto it = begin; it != end; ++it) {
        std::string tok = it->str(1);
        while (!tok.empty() && (tok.back() == '.' || tok.back() == ',' ||
                                tok.back() == ')' || tok.back() == ':'))
            tok.pop_back();
        if (tok.find('<') != std::string::npos) continue;  // placeholder
        if (tok.find('*') != std::string::npos) continue;  // glob
        if (std::find(cited.begin(), cited.end(), tok) == cited.end())
            cited.push_back(tok);
    }
    EXPECT(cited.size() > 20);  // o mapa tem de citar o repo a sério
    for (const std::string& tok : cited) {
        bool ok = lsFilesHas(root, tok);
        if (!ok) {
            // variantes legítimas: "X.h/.cpp" → o X.h; diretório com barra
            const size_t slashDot = tok.find("/.");
            if (slashDot != std::string::npos)
                ok = lsFilesHas(root, tok.substr(0, slashDot));
            if (!ok && tok.back() == '/') ok = lsFilesHas(root, tok + ".gitkeep");
        }
        EXPECT_MSG(ok, "CODEMAP cita caminho que não existe no git: %s",
                   tok.c_str());
    }

    // 2) os nomes nus (sem prefixo) que o mapa cita a propósito
    static const char* kNus[] = {
        "VoniRegistry.h",   "VoniVm.cpp",          "VoniTykers.cpp",
        "VoniEngineHost.h", "test_main.cpp",       "TestFramework.h",
        "test_sentinels.cpp", "test_ctx_pines.cpp", "c33_virtual.cpp",
        "voni_refgen.cpp",  "glstub_fb.h",         "Tick.h",
        "SceneSerializer.cpp", "GOwnFormats.cpp",  "GmeshV3Stream.cpp",
        "BlockMesh.cpp",    "BlockMesh.h",         "ImpostorIndex.cpp",
        "Impostor.h",       "EditorLayout.h",      "Theme.h",
        "MeshRenderer.h",   "main.cpp",            "docs_lint_state.sh",
    };
    for (const char* nu : kNus) {
        const std::string cmd = "git -C " + root + " ls-files -- '*" + nu +
                                "' 2>/dev/null";
        int code = 0;
        const std::string out = runCapture(cmd, code);
        EXPECT_MSG(code == 0 && !out.empty(),
                   "CODEMAP cita o ficheiro nu '%s' e ele não está no git",
                   nu);
    }
}

// ctx_codemap_receitas — o §3 tem as receitas (>= 7), incluindo a
// CRÍTICA dos DOIS CMakeLists (a ferida da 3.75b).
TEST(ctx_codemap_receitas) {
    std::string text;
    ASSERT(readFile(ctxRoot() + "/CODEMAP.md", text));
    const std::string s3 = codemapSection(text, "## §3", "## §4");
    ASSERT(s3.size() > 0);
    long receitas = 0;
    size_t pos = 0;
    while ((pos = s3.find("\n### ", pos)) != std::string::npos) {
        ++receitas;
        ++pos;
    }
    EXPECT_MSG(receitas >= 7, "§3 com %ld receitas — o mínimo é 7",
               receitas);
    EXPECT(s3.find("FICHEIRO .cpp") != std::string::npos);
    EXPECT(s3.find("tests/CMakeLists.txt") != std::string::npos);
    EXPECT(s3.find("app/src/main/cpp/CMakeLists.txt") != std::string::npos);
}

// ---------------------------------------------------------------------------
// 0.11-CTX CTX-3 — o arquivo, o README curto, o DIARIO, o TEMPLATE e as
// referências vivas. O arquivo guarda os relatórios das campanhas
// FECHADAS (git mv — histórico preservado); os da 0.11-W (campanha em
// curso, o fecho é o BLOCO B) e os da 0.11-T-FIX (Perna 2 adiada pelo
// dono — não fechada) ficam em docs/.
// ---------------------------------------------------------------------------

// ctx_arquivo_existe — o arquivo existe e recebeu os relatórios movidos
// (as campanhas 0.6.x–0.10-M e as fases fechadas da 0.11: 72 ficheiros;
// o teto honesto do pin é 50 — a prova de que a mudança foi em massa).
TEST(ctx_arquivo_existe) {
    const std::string cmd =
        "ls " + ctxRoot() + "/docs/arquivo/RELATORIO-*.md 2>/dev/null | wc -l";
    int code = 0;
    const std::string out = runCapture(cmd, code);
    long n = -1;
    ASSERT(parseIntAfter(out, "", n) || sscanf(out.c_str(), "%ld", &n) == 1);
    EXPECT_MSG(n >= 50,
               "docs/arquivo/ com %ld relatórios — a mudança em massa "
               "não aconteceu (esperados >= 50)", n);
}

// ctx_diario_existe — o DIARIO guarda o diário por versão (o conteúdo
// longo do README antes da CTX-3, movido sem edição — as leis do fluxo
// e os checklists do device preservados).
TEST(ctx_diario_existe) {
    std::string text;
    EXPECT_MSG(readFile(ctxRoot() + "/docs/DIARIO.md", text),
               "docs/DIARIO.md tem de existir (o diário por versão)");
    const long lines =
        static_cast<long>(std::count(text.begin(), text.end(), '\n'));
    EXPECT_MSG(lines > 2000,
               "docs/DIARIO.md com %ld linhas — o diário integral tem de "
               "ter as ~4.5 mil linhas do README antigo", lines);
}

// ctx_readme_curto — o README é um ÍNDICE (<200 linhas); o conteúdo
// longo foi para o DIARIO. Se voltou a crescer, a dívida vai para o
// DIARIO, não para o README.
TEST(ctx_readme_curto) {
    std::string text;
    ASSERT(readFile(ctxRoot() + "/README.md", text));
    const long lines =
        static_cast<long>(std::count(text.begin(), text.end(), '\n'));
    EXPECT_MSG(lines < 200, "README.md com %ld linhas — o teto é 200 "
                            "(o conteúdo longo vive em docs/DIARIO.md)",
               lines);
}

// ctx_template_existe — a pirâmide de leitura existe e nomeia os dois
// níveis obrigatórios (o Nível 0 = STATE + CODEMAP + o próprio prompt).
TEST(ctx_template_existe) {
    std::string text;
    EXPECT_MSG(readFile(ctxRoot() + "/docs/TEMPLATE_PROMPT.md", text),
               "docs/TEMPLATE_PROMPT.md tem de existir");
    EXPECT(text.find("Nível 0") != std::string::npos);
    EXPECT(text.find("STATE.md") != std::string::npos);
    EXPECT(text.find("CODEMAP.md") != std::string::npos);
    EXPECT(text.find("Nível 1") != std::string::npos);
    EXPECT(text.find("Nível 2") != std::string::npos);
}

// ctx_refs_ok — nenhuma referência a docs/RELATORIO-*.md quebrada FORA
// de docs/arquivo/: cada referência citada num documento vivo tem de
// existir no caminho citado OU no arquivo (docs/arquivo/<ficheiro> — a
// convenção da mudança, documentada no README e no DIARIO). Uma ref que
// não existe em lado nenhum = ponteiro morto = vermelho.
//
// EXCEÇÃO (o fix do R-015 do fecho): as LINHAS DE ANOTAÇÃO GitHub
// (::error::, ::warning::) são a TRANSCRIÇÃO do output de um gate colada
// como prova de mutação — ex. o §11 do RELATORIO-0.9.6.md cita
// «::error::release-identity (R-015): docs/RELATORIO-0.9.9.md não
// existe», onde o ficheiro citado não existe POR DESENHO (a MUTAÇÃO C
// simulava o bump sem relatório e o gate É SUPOSTO gritar). Transcrição
// de gate não é citação; evidência não se falsifica para satisfazer um
// scanner — o scanner aprende a lê-la. Uma citação VIVA nunca começa
// uma linha por "::".
TEST(ctx_refs_ok) {
    const std::string root = ctxRoot();
    // os documentos vivos: a raiz de leitura + docs/ recursivo, exceto
    // o arquivo (o histórico congelado não é reescrito — §0.6: não apagar)
    std::string cmd =
        "cd " + root + " && ls README.md ESTUDOS.md ARCHITECTURE.md "
        "STATE.md CODEMAP.md BACKLOG.md VONI_referencia.md llms.txt "
        "llms-full.txt 2>/dev/null; "
        "find docs -name '*.md' -not -path 'docs/arquivo/*' 2>/dev/null";
    int code = 0;
    const std::string listing = runCapture(cmd, code);
    ASSERT(!listing.empty());
    static const std::regex refRe(
        "docs/(arquivo/)?(RELATORIO-[A-Za-z0-9._-]+\\.md)");
    long refs = 0, broken = 0;
    size_t pos = 0;
    while (pos <= listing.size()) {
        const size_t nl = listing.find('\n', pos);
        if (nl == std::string::npos && pos >= listing.size()) break;
        const std::string file = listing.substr(
            pos, nl == std::string::npos ? std::string::npos : nl - pos);
        if (file.empty()) break;
        pos = nl + 1;
        std::string text;
        if (!readFile(root + "/" + file, text)) continue;
        // as linhas de anotação (1.º caractere não-branco = "::") saem
        // do texto antes da varredura — ver o EXCEÇÃO no cabeçalho do pin
        std::string filtrado;
        filtrado.reserve(text.size());
        size_t ls = 0;
        while (ls < text.size()) {
            const size_t le = text.find('\n', ls);
            const std::string linha = text.substr(
                ls, le == std::string::npos ? std::string::npos : le - ls);
            const size_t primeiro = linha.find_first_not_of(" \t");
            const bool anotacao =
                primeiro != std::string::npos &&
                linha.compare(primeiro, 2, "::") == 0;
            if (!anotacao) {
                filtrado.append(linha);
                filtrado.append(1, '\n');
            }
            if (le == std::string::npos) break;
            ls = le + 1;
        }
        auto begin =
            std::sregex_iterator(filtrado.begin(), filtrado.end(), refRe);
        auto end = std::sregex_iterator();
        for (auto it = begin; it != end; ++it) {
            ++refs;
            const bool citaArquivo = it->str(1).size() > 0;
            const std::string rel = it->str(2);       // RELATORIO-X.md
            const std::string citado = citaArquivo
                                           ? "docs/arquivo/" + rel
                                           : "docs/" + rel;
            const std::string alternativo = citaArquivo
                                                ? "docs/" + rel
                                                : "docs/arquivo/" + rel;
            std::ifstream direto(root + "/" + citado);
            if (direto.is_open()) { direto.close(); continue; }
            std::ifstream arq(root + "/" + alternativo);
            if (arq.is_open()) { arq.close(); continue; }
            ++broken;
            EXPECT_MSG(false,
                       "%s cita %s e o ficheiro não está nem em docs/ "
                       "nem em docs/arquivo/ (ponteiro morto)",
                       file.c_str(), citado.c_str());
        }
    }
    EXPECT_MSG(refs > 0, "a varredura não apanhou NENHUMA referência a "
                         "RELATORIO — o pin deixou de varrer a sério");
    EXPECT(broken == 0);
}
