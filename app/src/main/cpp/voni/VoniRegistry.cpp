// voni/VoniRegistry.cpp — O REGISTO CENTRAL (ver header). METADE 1 povoa
// LINKER/TYKER/COMPONENTE; a METADE 2 migra LINGUAGEM+COMANDO para aqui e
// o VoniDocs passa a DERIVAR deste vetor (uma só fonte alimenta tudo).
//
// A TABELA kComponents liga nome→handler (os handlers vivem no VoniTykers).
// ADICIONAR COMPONENTE = 1 handler + 1 linha AQUI (com Docs) — zero parser.
#include "voni/VoniRegistry.h"
#include "voni/VoniTykers.h"

#include <cstring>
#include <mutex>

namespace voni {
namespace reg {

// ---------------------------------------------------------------------------
// AS ENTRADAS (METADE 1): as formas linker/tyker/find + os 13 componentes.
// Docs OBRIGATÓRIA por entrada (sintaxe/descrição/exemplo) — o teste do
// registo afere campo a campo; a METADE 2 acrescenta equiv/skeleton.
// ---------------------------------------------------------------------------
static const Entry kEntries[] = {
    // ---- LINKER (a forma de declaração) ----------------------------------
    { "linker", Kind::Linker,
      "linker(A)to(B)=RF(nome)",
      "Liga A a B e regista o link no RF(nome); A e B podem ser objeto, "
      "TIC, propriedade ou animação.",
      "linker(jogador)to(cubo)=RF(principal)",
      "", "", 0, CompKind::Nenhum, 0, 0 },

    // ---- TYKER (a forma de declaração) + find -----------------------------
    { "tyker", Kind::Tyker,
      "tyker(nome){ find(RF) componentes… }",
      "Bloco de comportamento sobre os links de um RF; corre a cada frame "
      "enquanto o script está ativo.",
      "tyker(seguelo){ find(principal) follow(2) }",
      "", "", 0, CompKind::Nenhum, 0, 0 },
    { "find", Kind::Tyker,
      "find(nome-do-RF)",
      "Liga o tyker ao RF declarado nos linkers; é SEMPRE o 1º componente "
      "e é obrigatório.",
      "tyker(seguelo){ find(principal) look() }",
      "", "", 0, CompKind::Nenhum, 0, 0 },

    // ---- COMPONENTES CONTÍNUOS (correm a cada frame) ---------------------
    { "follow", Kind::Componente,
      "follow() · follow(d) · follow(d,suav)",
      "A origem segue o destino: follow() cola, follow(d) guarda a "
      "distância d, suav suaviza a perseguição por segundo.",
      "linker(jogador)to(cubo)=RF(p)\ntyker(s){ find(p) follow(2) }",
      "", "", 0, CompKind::Continuo, 0, 2 },
    { "look", Kind::Componente,
      "look()",
      "A origem roda para ficar virada para o destino (rotação em graus).",
      "tyker(s){ find(p) look() }",
      "", "", 0, CompKind::Continuo, 0, 0 },
    { "orbit", Kind::Componente,
      "orbit(d,vel)",
      "A origem orbita o destino à distância d, avançando vel graus por "
      "segundo (círculo no plano XZ).",
      "tyker(s){ find(p) orbit(3, 90) }",
      "", "", 0, CompKind::Continuo, 2, 2 },
    { "copy", Kind::Componente,
      "copy(propriedade)",
      "Copia a propriedade do destino para a origem a cada frame "
      "(ex.: copy(pos), copy(cor)).",
      "tyker(s){ find(p) copy(escala) }",
      "", "", 0, CompKind::Continuo, 1, 1 },
    { "map", Kind::Componente,
      "map()",
      "Reservado: mapeia valores entre as pontas; sem mapa definido é "
      "no-op (não faz nada, não dá erro).",
      "tyker(s){ find(p) map() }",
      "", "", 0, CompKind::Continuo, 0, 0 },

    // ---- COMPONENTES PONTUAIS (disparam 1× na ativação) ------------------
    { "Change", Kind::Componente,
      "Change(origem)to(alvo) · Change(destino)to(alvo)",
      "Muda um lado dos links do RF para o novo alvo (o RF é partilhado: "
      "todos os tykers desse RF passam a ver a mudança).",
      "tyker(s){ find(p) Change(destino)to(cubo2) }",
      "", "", 0, CompKind::Pontual, 0, 0 },
    { "point", Kind::Componente,
      "point() · point(x,y,z)",
      "point(x,y,z) fixa um ponto absoluto como alvo do follow/look/orbit; "
      "point() limpa o ponto e volta ao destino vivo.",
      "tyker(s){ find(p) point(0, 1, 0) follow(2) }",
      "", "", 0, CompKind::Pontual, 0, 3 },
    { "colorpars", Kind::Componente,
      "colorpars(cor)(nome|#RRGGBB)",
      "Define um parâmetro de cor: o parâmetro 'cor' tinge o material do "
      "TIC de origem (nome da paleta ou hex); outros nomes ficam guardados "
      "para o shading() futuro.",
      "tyker(s){ find(p) colorpars(cor)(#FF0000) }",
      "", "", 0, CompKind::Pontual, 1, 1 },
    { "play", Kind::Componente,
      "play()",
      "Toca a animação do linker (a ponta que é nome de animação, não de "
      "TIC) no TIC dono do script.",
      "linker(ator)to(correr)=RF(a)\ntyker(t){ find(a) play() }",
      "", "", 0, CompKind::Pontual, 0, 0 },
    { "limit", Kind::Componente,
      "limit(min,max)",
      "Limita a distância da origem ao alvo entre min e max (aplica-se ao "
      "follow e ao orbit).",
      "tyker(s){ find(p) follow(2) limit(1, 10) }",
      "", "", 0, CompKind::Pontual, 2, 2 },
    { "delay", Kind::Componente,
      "delay(segundos)",
      "Adia a ativação do tyker: os componentes pontuais só disparam — e "
      "os contínuos só começam — passados os segundos.",
      "tyker(s){ find(p) delay(2) follow(1) }",
      "", "", 0, CompKind::Pontual, 1, 1 },

    // ---- RESERVADO ---------------------------------------------------------
    { "shading", Kind::Componente,
      "shading()",
      "Reservado: aceita e não faz nada (o futuro sombreado consumirá os "
      "parâmetros do colorpars).",
      "tyker(s){ find(p) shading() }",
      "", "", 0, CompKind::Reservado, 0, 0 },
};

// ---------------------------------------------------------------------------
// o dispatch: nome → handler (VoniTykers). A MESMA tabela que o compilador
// consulta para validar nomes/argc — UMA fonte.
// ---------------------------------------------------------------------------
namespace {
struct CompRow {
    const char*     name;
    tykers::Handler fn;
};
const CompRow kComponents[] = {
    {"follow",     &tykers::compFollow},
    {"look",       &tykers::compLook},
    {"orbit",      &tykers::compOrbit},
    {"copy",       &tykers::compCopy},
    {"map",        &tykers::compMap},
    {"Change",     &tykers::compChange},
    {"point",      &tykers::compPoint},
    {"colorpars",  &tykers::compColorpars},
    {"play",       &tykers::compPlay},
    {"limit",      &tykers::compLimit},
    {"delay",      &tykers::compDelay},
    {"shading",    &tykers::compShading},
};

// instalações de teste (installForTest) — guardadas à parte, consultadas
// PRIMEIRO; resetForTest limpa. Mutex: o registo é read-only em produção,
// só os testes escrevem (e o CI corre os executáveis em série — o mutex é
// defesa de princípio, não de conteúdo).
struct TestInstall {
    Entry      entry;
    tykers::Handler fn;
};
std::vector<TestInstall>& testInstalls() {
    static std::vector<TestInstall> v;
    return v;
}
std::mutex& testMutex() {
    static std::mutex m;
    return m;
}
} // namespace

const std::vector<Entry>& all() {
    // o vetor estático completo (kEntries é array C — empacota uma vez)
    static const std::vector<Entry> kAll(std::begin(kEntries),
                                         std::end(kEntries));
    return kAll;
}

const Entry* find(const std::string& name) {
    for (const Entry& e : all()) {
        if (name == e.name) {
            return &e;
        }
    }
    std::lock_guard<std::mutex> lk(testMutex());
    for (const TestInstall& t : testInstalls()) {
        if (name == t.entry.name) {
            return &t.entry;
        }
    }
    return nullptr;
}

const Entry* findComponent(const std::string& name) {
    const Entry* e = find(name);
    return (e && e->kind == Kind::Componente) ? e : nullptr;
}

const Entry* prefixMatch(const std::string& prefix) {
    if (prefix.empty()) {
        return nullptr;
    }
    for (const Entry& e : all()) {
        if (std::strncmp(e.name, prefix.c_str(), prefix.size()) == 0) {
            return &e;
        }
    }
    std::lock_guard<std::mutex> lk(testMutex());
    for (const TestInstall& t : testInstalls()) {
        if (std::strncmp(t.entry.name, prefix.c_str(), prefix.size()) == 0) {
            return &t.entry;
        }
    }
    return nullptr;
}

tykers::Handler handlerFor(const std::string& name) {
    std::lock_guard<std::mutex> lk(testMutex());
    for (const TestInstall& t : testInstalls()) {
        if (name == t.entry.name) {
            return t.fn;
        }
    }
    for (const CompRow& r : kComponents) {
        if (name == r.name) {
            return r.fn;
        }
    }
    return nullptr;
}

bool installForTest(const char* name, tykers::Handler handler,
                    const char* syntax, const char* desc,
                    const char* example) {
    std::lock_guard<std::mutex> lk(testMutex());
    TestInstall t;
    t.entry = Entry{name, Kind::Componente, syntax, desc, example,
                    "", "", 0, CompKind::Continuo, 0, 9};
    t.fn = handler;
    testInstalls().push_back(t);
    return true;
}

void resetForTest() {
    std::lock_guard<std::mutex> lk(testMutex());
    testInstalls().clear();
}

} // namespace reg
} // namespace voni
