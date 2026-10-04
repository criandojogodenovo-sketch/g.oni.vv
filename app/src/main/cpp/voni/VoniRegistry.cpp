// voni/VoniRegistry.cpp — O REGISTO CENTRAL (ver header). METADE 2: TODA a
// superfície ensinável da V.ONI vive AQUI (Linguagem §§3-8 + Comandos §9 +
// Linker/Tyker/Componente da 0.9.5) — UMA SÓ FONTE ALIMENTA TUDO: lista de
// comandos · tabela de equivalências (equiv) · erros-que-ensinam
// (kForeign + a sintaxe nas mensagens) · tooltips/toque (desc) · Docs ·
// completamento (prefixMatch + skeleton) · copiar-referência
// (fullReferenceMarkdown). O VoniDocs passou a VISTA; o teste R-013 afere
// a BIJEÇÃO registo↔Docs↔erros↔referência.
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
// AS ENTRADAS — TODA a linguagem. Docs OBRIGATÓRIA por entrada (o teste do
// registo afere campo a campo); equiv = a linha Python/JS; skeleton = o
// texto do Tab ("" = sem esqueleto — o Tab indenta).
// ---------------------------------------------------------------------------
static const Entry kEntries[] = {
    // ---- LINGUAGEM (§§3-8) ------------------------------------------------
    { "central main", Kind::Linguagem,
      "central main { on moment { } allmoments { } }",
      "Bloco de arranque do TIC: contém on moment e allmoments.",
      "central main {\n  on moment { View P \"comecou\" }\n  allmoments { }\n}",
      "o arranque do script (setup + update)",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "on moment", Kind::Linguagem,
      "on moment { }",
      "Corre 1× no arranque do TIC (depois do top-level).",
      "central main { on moment { View P \"uma vez\" } }",
      "setup() / start()",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "allmoments", Kind::Linguagem,
      "allmoments { }",
      "Corre a cada frame enquanto o TIC está ativo.",
      "central main { allmoments { move(0, 0, 0.1) } }",
      "update() por frame",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "v++ (declarar)", Kind::Linguagem,
      "v++nome=valor",
      "Declara variável com o tipo inferido do valor.",
      "v++vida=100",
      "x = valor (tipo inferido)",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "v# (declarar com tipo)", Kind::Linguagem,
      "v#nome:Tipo=valor",
      "Declara variável com tipo explícito.",
      "v#velocidade:Num=5.5",
      "x: tipo = valor",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "@+ (exportar)", Kind::Linguagem,
      "v#@+nome:Tipo=valor · v++@+nome=valor",
      "Logo depois do declarador: a variável aparece no Inspector.",
      "v#@+velocidade:Num=5.5",
      "pública / editável no editor",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "repeat", Kind::Linguagem,
      "repeat(n) { }",
      "Repete o bloco n vezes.",
      "repeat(3) { View P \"tres vezes\" }",
      "for _ in range(n):",
      "repeat(n){ }", 9, CompKind::Nenhum, 0, 0 },
    { "last", Kind::Linguagem,
      "last(condição) { }",
      "Repete enquanto a condição for verdadeira.",
      "last(vida > 0) { vida-=1 }",
      "while condição:",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "with n+=1", Kind::Linguagem,
      "last(condição) with n+=1 { }",
      "Contador de iterações do last; n usável na condição e no corpo.",
      "last(n < 10) with n+=1 { }",
      "enumerate / contador",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "continue", Kind::Linguagem,
      "continue",
      "Salta o resto da iteração atual.",
      "last(true) { continue }",
      "continue (igual)",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "resume", Kind::Linguagem,
      "resume",
      "Sai do loop (o 'break' da V.ONI).",
      "last(true) { resume }",
      "break",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "exist", Kind::Linguagem,
      "exist(condição) { } notexist{ }",
      "Corre o bloco se a condição for verdadeira (o 'if' da V.ONI).",
      "exist(vida == 0) { View P \"fim\" }",
      "if:",
      "exist(){ } notexist{ }", 8, CompKind::Nenhum, 0, 0 },
    { "notexist", Kind::Linguagem,
      "notexist{ } · notexist{ } and( cond(ação) stopand )",
      "O 'senão' do exist; com and(…) colado faz a cadeia de casos.",
      "exist(a) { } notexist{ View P \"default\" }",
      "else:",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "and(…) stopand", Kind::Linguagem,
      "and( cond(ação) stopand cond(ação) stopand )",
      "Casos do notexist: 1ª condição verdadeira corre a ação e para.",
      "exist(x) { }\nnotexist{ View P \"nada\" } and(\n  x2(View P \"dois\") stopand\n}",
      "elif: (cadeia de casos)",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "option", Kind::Linguagem,
      "option(valor) { and valor(ação) stopand notoption{ } }",
      "Escolhe caso por valor; notoption é o default.",
      "option(nivel) {\n  and 1(View P \"facil\") stopand\n  notoption{ View P \"outro\" }\n}",
      "switch/case",
      "option(){ and valor(ação) stopand notoption{ } }", 10,
      CompKind::Nenhum, 0, 0 },
    { "fn", Kind::Linguagem,
      "fn nome(a:Num):Num { return a }",
      "Função do utilizador (nome em minúsculas).",
      "fn dobro(n:Num):Num { return n*2 }",
      "def (Python) / function (JS)",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "return", Kind::Linguagem,
      "return valor",
      "Devolve o valor da fn.",
      "fn um():Int { return 1 }",
      "return (igual)",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "operadores", Kind::Linguagem,
      "a+b · a == b · a and b · not a",
      "Aritmética + - * /, comparação == != < > <= >=, lógicos and or not.",
      "exist(vida > 0 and not fim) { }",
      "iguais (and/or/not em vez de &&/||/!)",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "tipos", Kind::Linguagem,
      "v#nome:Tipo=valor",
      "Int Num Txt Bool Vec2 Vec3 TIC (booleanos: true/false).",
      "v#vida:Int=100 · v#@+velocidade:Num=5.5",
      "int/float/str/bool",
      "", 0, CompKind::Nenhum, 0, 0 },

    // ---- COMANDOS (§9 — lista fechada) ------------------------------------
    { "View P", Kind::Comando,
      "View P \"texto\"",
      "Escreve texto no log da engine (prefixo voni:).",
      "View P \"ola mundo\"",
      "print()",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "View Object", Kind::Comando,
      "View Object",
      "Ainda não existe: a consola é que o usará (erro legível por agora).",
      "View Object  // 'consola ainda não existe'",
      "console de objetos (futuro)",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "move", Kind::Comando,
      "move(x, y, z)",
      "Move o TIC dono do script (sem animação).",
      "move(0, 0, 0.5)",
      "transform.position += (x,y,z)",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "Import.Animation", Kind::Comando,
      "Import.Animation(\"nome da animação\")",
      "Importa a animação pelo nome para o TIC dono.",
      "Import.Animation(\"correr\")",
      "carregar/tocar um clip",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "transition.for", Kind::Comando,
      "nomedacena.transition.for(\"cena de destino\")",
      "Transição de cena (NÃO se chama importar cenas).",
      "cena1.transition.for(\"cena2\")",
      "mudar de cena/nível",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "Deltatime.Increment", Kind::Comando,
      "Deltatime.Increment(variável, valor_por_segundo)",
      "Acrescenta valor×dt à variável a cada execução (frame).",
      "allmoments { Deltatime.Increment(tempo, 1) }",
      "x += v * dt",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "Explode.TIC.et", Kind::Comando,
      "Explode.TIC.et",
      "Faz o TIC dono desaparecer.",
      "Explode.TIC.et",
      "visible = False",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "Explode.TIC.er", Kind::Comando,
      "Explode.TIC.er",
      "Faz o TIC dono aparecer.",
      "Explode.TIC.er",
      "visible = True",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "Search", Kind::Comando,
      "Search.alvo.propriedade",
      "Pesquisa dentro da cena via RTTI (leitura de propriedades).",
      "v++px=Search.jogador.pos.x",
      "find()/get_node + atributo",
      "", 0, CompKind::Nenhum, 0, 0 },
    { "propriedades de TIC", Kind::Comando,
      "tic.propriedade · tic.pos.x=5",
      "Leitura/escrita por pontos: pos, rot, escala, cor (Vec3 com .x/.y/.z), "
      "name, visible, active.",
      "jogador.pos.x=5",
      "atributos do objeto",
      "", 0, CompKind::Nenhum, 0, 0 },

    // ---- LINKER (a forma de declaração — 0.9.5) ---------------------------
    { "linker", Kind::Linker,
      "linker(A)to(B)=RF(nome)",
      "Liga A a B e regista o link no RF(nome); A e B podem ser objeto, "
      "TIC, propriedade ou animação.",
      "linker(jogador)to(cubo)=RF(principal)",
      "uma referência entre duas coisas",
      "linker()to()=RF()", 7, CompKind::Nenhum, 0, 0 },

    // ---- TYKER (a forma de declaração) + find ------------------------------
    { "tyker", Kind::Tyker,
      "tyker(nome){ find(RF) componentes… }",
      "Bloco de comportamento sobre os links de um RF; corre a cada frame "
      "enquanto o script está ativo.",
      "tyker(seguelo){ find(principal) follow(2) }",
      "um comportamento que corre por frame",
      "tyker(nome){ find(RF) }", 22, CompKind::Nenhum, 0, 0 },
    { "find", Kind::Tyker,
      "find(nome-do-RF)",
      "Liga o tyker ao RF declarado nos linkers; é SEMPRE o 1º componente "
      "e é obrigatório.",
      "tyker(seguelo){ find(principal) look() }",
      "o tyker liga-se ao grupo de links",
      "", 0, CompKind::Nenhum, 0, 0 },

    // ---- COMPONENTES CONTÍNUOS (correm a cada frame) ----------------------
    { "follow", Kind::Componente,
      "follow() · follow(d) · follow(d,suav)",
      "A origem segue o destino: follow() cola, follow(d) guarda a "
      "distância d, suav suaviza a perseguição por segundo.",
      "linker(jogador)to(cubo)=RF(p)\ntyker(s){ find(p) follow(2) }",
      "perseguir (lerp contínuo)",
      "", 0, CompKind::Continuo, 0, 2 },
    { "look", Kind::Componente,
      "look()",
      "A origem roda para ficar virada para o destino (rotação em graus).",
      "tyker(s){ find(p) look() }",
      "look_at()",
      "", 0, CompKind::Continuo, 0, 0 },
    { "orbit", Kind::Componente,
      "orbit(d,vel)",
      "A origem orbita o destino à distância d, avançando vel graus por "
      "segundo (círculo no plano XZ).",
      "tyker(s){ find(p) orbit(3, 90) }",
      "girar em volta",
      "", 0, CompKind::Continuo, 2, 2 },
    { "copy", Kind::Componente,
      "copy(propriedade)",
      "Copia a propriedade do destino para a origem a cada frame "
      "(ex.: copy(pos), copy(cor)).",
      "tyker(s){ find(p) copy(escala) }",
      "espelhar um atributo",
      "", 0, CompKind::Continuo, 1, 1 },
    { "map", Kind::Componente,
      "map()",
      "Reservado: mapeia valores entre as pontas; sem mapa definido é "
      "no-op (não faz nada, não dá erro).",
      "tyker(s){ find(p) map() }",
      "remapear valores (reservado)",
      "", 0, CompKind::Continuo, 0, 0 },

    // ---- COMPONENTES PONTUAIS (disparam 1× na ativação) ------------------
    { "Change", Kind::Componente,
      "Change(origem)to(alvo) · Change(destino)to(alvo)",
      "Muda um lado dos links do RF para o novo alvo (o RF é partilhado: "
      "todos os tykers desse RF passam a ver a mudança).",
      "tyker(s){ find(p) Change(destino)to(cubo2) }",
      "reescrever o alvo da ligação",
      "", 0, CompKind::Pontual, 0, 0 },
    { "point", Kind::Componente,
      "point() · point(x,y,z)",
      "point(x,y,z) fixa um ponto absoluto como alvo do follow/look/orbit; "
      "point() limpa o ponto e volta ao destino vivo.",
      "tyker(s){ find(p) point(0, 1, 0) follow(2) }",
      "alvo fixo (coordenadas)",
      "", 0, CompKind::Pontual, 0, 3 },
    { "colorpars", Kind::Componente,
      "colorpars(cor)(nome|#RRGGBB)",
      "Define um parâmetro de cor: o parâmetro 'cor' tinge o material do "
      "TIC de origem (nome da paleta ou hex); outros nomes ficam guardados "
      "para o shading() futuro.",
      "tyker(s){ find(p) colorpars(cor)(#FF0000) }",
      "cor do material",
      "", 0, CompKind::Pontual, 1, 1 },
    { "play", Kind::Componente,
      "play()",
      "Toca a animação do linker (a ponta que é nome de animação, não de "
      "TIC) no TIC dono do script.",
      "linker(ator)to(correr)=RF(a)\ntyker(t){ find(a) play() }",
      "tocar uma animação",
      "", 0, CompKind::Pontual, 0, 0 },
    { "limit", Kind::Componente,
      "limit(min,max)",
      "Limita a distância da origem ao alvo entre min e max (aplica-se ao "
      "follow e ao orbit).",
      "tyker(s){ find(p) follow(2) limit(1, 10) }",
      "clamp da distância",
      "", 0, CompKind::Pontual, 2, 2 },
    { "delay", Kind::Componente,
      "delay(segundos)",
      "Adia a ativação do tyker: os componentes pontuais só disparam — e "
      "os contínuos só começam — passados os segundos.",
      "tyker(s){ find(p) delay(2) follow(1) }",
      "esperar N segundos",
      "", 0, CompKind::Pontual, 1, 1 },

    // ---- RESERVADO ---------------------------------------------------------
    { "shading", Kind::Componente,
      "shading()",
      "Reservado: aceita e não faz nada (o futuro sombreado consumirá os "
      "parâmetros do colorpars).",
      "tyker(s){ find(p) shading() }",
      "sombreado (reservado)",
      "", 0, CompKind::Reservado, 0, 0 },
};

// ---------------------------------------------------------------------------
// PALAVRAS ESTRANGEIRAS (erros-que-ensinam): o utilizador que sabe Python/JS
// escreve 'if' — o erro de sintaxe ENSINA o equivalente V.ONI em vez de
// "expressão inesperada" seco. O teste R-013 afere que cada dica aponta a
// uma ENTRADA REAL do registo (bijeção).
// ---------------------------------------------------------------------------
struct Foreign {
    const char* word;    // o que a pessoa escreveu
    const char* entry;   // a entrada do registo que resolve
    const char* teach;   // a linha que ensina
};

static const Foreign kForeign[] = {
    {"if",        "exist",   "'if' não existe na V.ONI — chama-se exist: exist(condição){ }"},
    {"else",      "notexist","'else' não existe — é o notexist: exist(c){ } notexist{ }"},
    {"elif",      "and(…) stopand",
                           "'elif' não existe — a cadeia é notexist{ } and( cond(ação) stopand )"},
    {"while",     "last",    "'while' não existe — chama-se last: last(condição){ }"},
    {"for",       "repeat",  "'for' não existe — chama-se repeat: repeat(n){ }"},
    {"break",     "resume",  "'break' não existe — chama-se resume"},
    {"switch",    "option",  "'switch' não existe — chama-se option(valor){ }"},
    {"case",      "option",  "'case' não existe — os casos do option: and valor(ação) stopand"},
    {"def",       "fn",      "'def' não existe — chama-se fn: fn nome(a:Num):Num { }"},
    {"function",  "fn",      "'function' não existe — chama-se fn: fn nome(a:Num):Num { }"},
    {"print",     "View P",  "'print' não existe — o log é o View P: View P \"texto\""},
    {"echo",      "View P",  "'echo' não existe — o log é o View P: View P \"texto\""},
    {"True",      "tipos",   "'True' com maiúscula não existe — é true (minúsculas)"},
    {"False",     "tipos",   "'False' com maiúscula não existe — é false (minúsculas)"},
    {"None",      "tipos",   "'None' não existe — usa um valor dos 7 tipos (Int Num Txt Bool Vec2 Vec3 TIC)"},
    {"null",      "tipos",   "'null' não existe — usa um valor dos 7 tipos (Int Num Txt Bool Vec2 Vec3 TIC)"},
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
// só os testes escrevem.
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

// ---------------------------------------------------------------------------
// erros-que-ensinam: a palavra estrangeira no ponto do erro de sintaxe
// ---------------------------------------------------------------------------
const Foreign* findForeign(const std::string& word) {
    for (const Foreign& f : kForeign) {
        if (word == f.word) {
            return &f;
        }
    }
    return nullptr;
}

const char* foreignTeach(const std::string& word, const char** entryName) {
    const Foreign* f = findForeign(word);
    if (!f) {
        return nullptr;
    }
    if (entryName) {
        *entryName = f->entry;
    }
    return f->teach;
}

// ---------------------------------------------------------------------------
// A REFERÊNCIA PÚBLICA — gerada DO REGISTO (a mesma fonte de tudo): alimenta
// o VONI_referencia.md, o llms-full.txt e o botão copiar-referência do
// editor. O teste de sincronia (CI) afere que os ficheiros commitados
// correspondem EXATAMENTE a esta saída.
// ---------------------------------------------------------------------------
std::string fullReferenceMarkdown() {
    std::string out;
    out += "# V.ONI — Referência da linguagem\n\n";
    out += "A linguagem de scripting FECHADA da G.One VV. Esta referência ";
    out += "é GERADA do registo central do motor (uma só fonte alimenta a ";
    out += "Docs, o editor que ensina e os erros legíveis).\n\n";
    out += "Regras de ouro: NÃO EXISTE if/else/switch/break (exist/notexist";
    out += "/option/resume) · nomes de utilizador em MINÚSCULAS (maiúsculas";
    out += " são da engine) · booleanos true/false · sandbox sem ";
    out += "ficheiros/rede · erros SEMPRE com linha, nunca crash.\n\n";

    struct Section { Kind k; const char* title; };
    const Section sections[] = {
        {Kind::Linguagem,  "Linguagem"},
        {Kind::Comando,    "Comandos"},
        {Kind::Linker,     "Linkers"},
        {Kind::Tyker,      "Tykers"},
        {Kind::Componente, "Componentes"},
    };
    for (const Section& s : sections) {
        out += "## ";
        out += s.title;
        out += "\n\n";
        for (const Entry& e : all()) {
            if (e.kind != s.k) {
                continue;
            }
            out += "### ";
            out += e.name;
            out += "\n";
            out += "- **Sintaxe**: `";
            out += e.syntax;
            out += "`\n- **Descrição**: ";
            out += e.desc;
            out += "\n- **Equivalência**: ";
            out += e.equiv;
            out += "\n- **Exemplo**:\n```\n";
            out += e.example;
            out += "\n```\n\n";
        }
    }
    return out;
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
