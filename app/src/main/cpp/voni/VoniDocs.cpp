// voni/VoniDocs.cpp — as entradas das Docs V.ONI (spec 0.9.2 §11 ✅ — nome,
// 1 linha, sintaxe, exemplo). Dados fechados da spec §§3-9.
//
// 0.9.5 · METADE 1: as entradas de LINKER/TYKER/COMPONENTE vêm do REGISTO
// CENTRAL (VoniRegistry) — a MESMA tabela que valida os componentes no
// compile e os despacha no runtime. Docs obrigatória por entrada 🔶: o
// teste registo_docs_obrigatorias afere campo a campo. A METADE 2 migra
// também Linguagem/Comando para o registo e este ficheiro passa a VISTA.
#include "voni/VoniDocs.h"
#include "voni/VoniRegistry.h"

#include <algorithm>
#include <cctype>

namespace voni {
namespace docs {

static const std::vector<Entry> kBase = {
    // ---- LINGUAGEM (§§3-8) ------------------------------------------------
    {Cat::Linguagem, "central main",
     "Bloco de arranque do TIC: contém on moment e allmoments.",
     "central main { on moment { } allmoments { } }",
     "central main {\n  on moment { View P \"comecou\" }\n  allmoments { }\n}"},
    {Cat::Linguagem, "on moment",
     "Corre 1× no arranque do TIC (depois do top-level).",
     "on moment { }",
     "central main { on moment { View P \"uma vez\" } }"},
    {Cat::Linguagem, "allmoments",
     "Corre a cada frame enquanto o TIC está ativo.",
     "allmoments { }",
     "central main { allmoments { move(0, 0, 0.1) } }"},
    {Cat::Linguagem, "v++ (declarar)",
     "Declara variável com o tipo inferido do valor.",
     "v++nome=valor",
     "v++vida=100"},
    {Cat::Linguagem, "v# (declarar com tipo)",
     "Declara variável com tipo explícito.",
     "v#nome:Tipo=valor",
     "v#velocidade:Num=5.5"},
    {Cat::Linguagem, "@+ (exportar)",
     "Logo depois do declarador: a variável aparece no Inspector.",
     "v#@+nome:Tipo=valor · v++@+nome=valor",
     "v#@+velocidade:Num=5.5"},
    {Cat::Linguagem, "repeat",
     "Repete o bloco n vezes.",
     "repeat(n) { }",
     "repeat(3) { View P \"tres vezes\" }"},
    {Cat::Linguagem, "last",
     "Repete enquanto a condição for verdadeira.",
     "last(condição) { }",
     "last(vida > 0) { vida-=1 }"},
    {Cat::Linguagem, "with n+=1",
     "Contador de iterações do last; n usável na condição e no corpo.",
     "last(condição) with n+=1 { }",
     "last(n < 10) with n+=1 { }"},
    {Cat::Linguagem, "continue",
     "Salta o resto da iteração atual.",
     "continue",
     "last(true) { continue }"},
    {Cat::Linguagem, "resume",
     "Sai do loop (o 'break' da V.ONI).",
     "resume",
     "last(true) { resume }"},
    {Cat::Linguagem, "exist",
     "Corre o bloco se a condição for verdadeira (o 'if' da V.ONI).",
     "exist(condição) { }",
     "exist(vida == 0) { View P \"fim\" }"},
    {Cat::Linguagem, "notexist",
     "O 'senão' do exist; com and(…) colado faz a cadeia de casos.",
     "notexist{ } · notexist{ } and( cond(ação) stopand )",
     "exist(a) { } notexist{ View P \"default\" }"},
    {Cat::Linguagem, "and(…) stopand",
     "Casos do notexist: 1ª condição verdadeira corre a ação e para.",
     "and( cond(ação) stopand cond(ação) stopand )",
     "exist(x) { }\nnotexist{ View P \"nada\" } and(\n  x2(View P \"dois\") stopand\n)"},
    {Cat::Linguagem, "option",
     "Escolhe caso por valor; notoption é o default.",
     "option(valor) { and valor(ação) stopand notoption{ } }",
     "option(nivel) {\n  and 1(View P \"facil\") stopand\n  and 2(View P \"medio\") stopand\n  notoption{ View P \"outro\" }\n}"},
    {Cat::Linguagem, "fn",
     "Função do utilizador (nome em minúsculas).",
     "fn nome(a:Num):Num { return a }",
     "fn dobro(n:Num):Num { return n*2 }"},
    {Cat::Linguagem, "return",
     "Devolve o valor da fn.",
     "return valor",
     "fn um():Int { return 1 }"},
    {Cat::Linguagem, "operadores",
     "Aritmética + - * /, comparação == != < > <= >=, lógicos and or not.",
     "a+b · a == b · a and b · not a",
     "exist(vida > 0 and not fim) { }"},
    {Cat::Linguagem, "tipos",
     "Int Num Txt Bool Vec2 Vec3 TIC (booleanos: true/false).",
     "v#nome:Tipo=valor",
     "v#vida:Int=100 · v#@+velocidade:Num=5.5"},

    // ---- COMANDOS (§9 — lista fechada) ------------------------------------
    {Cat::Comando, "View P",
     "Escreve texto no log da engine (prefixo voni:).",
     "View P \"texto\"",
     "View P \"ola mundo\""},
    {Cat::Comando, "View Object",
     "Ainda não existe: a consola é que o usará (erro legível por agora).",
     "View Object",
     "View Object  // 'consola ainda não existe'"},
    {Cat::Comando, "move",
     "Move o TIC dono do script (sem animação).",
     "move(x, y, z)",
     "move(0, 0, 0.5)"},
    {Cat::Comando, "Import.Animation",
     "Importa a animação pelo nome para o TIC dono.",
     "Import.Animation(\"nome da animação\")",
     "Import.Animation(\"correr\")"},
    {Cat::Comando, "transition.for",
     "Transição de cena (NÃO se chama importar cenas).",
     "nomedacena.transition.for(\"cena de destino\")",
     "cena1.transition.for(\"cena2\")"},
    {Cat::Comando, "Deltatime.Increment",
     "Acrescenta valor×dt à variável a cada execução (frame).",
     "Deltatime.Increment(variável, valor_por_segundo)",
     "allmoments { Deltatime.Increment(tempo, 1) }"},
    {Cat::Comando, "Explode.TIC.et",
     "Faz o TIC dono desaparecer.",
     "Explode.TIC.et",
     "Explode.TIC.et"},
    {Cat::Comando, "Explode.TIC.er",
     "Faz o TIC dono aparecer.",
     "Explode.TIC.er",
     "Explode.TIC.er"},
    {Cat::Comando, "Search",
     "Pesquisa dentro da cena via RTTI (leitura de propriedades).",
     "Search.alvo.propriedade",
     "v++px=Search.jogador.pos.x"},
    {Cat::Comando, "propriedades de TIC",
     "Leitura/escrita por pontos: pos, rot, escala, cor (Vec3 com .x/.y/.z), "
     "name, visible, active.",
     "tic.propriedade · tic.pos.x=5",
     "jogador.pos.x=5"},
};

// a lista COMPLETA: base (0.9.2) + o REGISTO central (0.9.5 — linker/tyker/
// componentes). Construída UMA vez; a mesma fonte que valida e despacha.
static std::vector<Entry> buildAll() {
    std::vector<Entry> out = kBase;
    for (const reg::Entry& e : reg::all()) {
        Cat c = Cat::Linguagem;
        switch (e.kind) {
            case reg::Kind::Comando:    c = Cat::Comando; break;
            case reg::Kind::Linker:     c = Cat::Linker; break;
            case reg::Kind::Tyker:      c = Cat::Tyker; break;
            case reg::Kind::Componente: c = Cat::Componente; break;
            default:                    c = Cat::Linguagem; break;
        }
        out.push_back(Entry{c, e.name, e.desc, e.syntax, e.example});
    }
    return out;
}

const std::vector<Entry>& all() {
    static const std::vector<Entry> kAll = buildAll();
    return kAll;
}

static bool containsCI(const std::string& hay, const std::string& needle) {
    if (needle.empty()) {
        return true;
    }
    auto it = std::search(
        hay.begin(), hay.end(), needle.begin(), needle.end(),
        [](unsigned char a, unsigned char b) {
            return std::tolower(a) == std::tolower(b);
        });
    return it != hay.end();
}

std::vector<const Entry*> search(const std::string& query) {
    std::vector<const Entry*> out;
    for (const Entry& e : all()) {
        if (containsCI(e.name, query) || containsCI(e.desc, query)) {
            out.push_back(&e);
        }
    }
    return out;
}

} // namespace docs
} // namespace voni
