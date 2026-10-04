#pragma once
// voni/VoniRegistry.h — O REGISTO CENTRAL (0.9.5 · METADE 1 🔶).
//
// "cada linker/tyker/componente declarado no registo; adicionar componente
//  novo NÃO obriga a mudar o parser; Docs obrigatória por entrada."
//
// COMO SE ADICIONA UM COMPONENTE NOVO (o contrato do registo):
//   1. 1 handler em VoniTykers.cpp (bool compXxx(Ctx&, Comp&, err));
//   2. 1 linha NA TABELA kComponents (VoniRegistry.cpp): nome + handler +
//      argc + Docs (sintaxe/descrição/exemplo) — Docs é OBRIGATÓRIA;
//   3. NADA mais. O parser NÃO muda (o corpo do tyker é GENÉRICO: nome +
//      caudas de args; a validação de nome/argc corre contra ESTA tabela).
//   O teste parser_independente do registo PROVA isto: instala um
//   componente de teste SEM tocar na gramática e corre um tyker com ele.
//
// UMA SÓ FONTE ALIMENTA TUDO (METADE 2 — os campos já cá estão):
//   lista de comandos · tabela de equivalências (equiv) · erros-que-ensinam
//   (foreign + sintaxe nas mensagens) · tooltips/toque (desc) · Docs ·
//   completamento (prefixMatch + skeleton) · copiar-referência
//   (fullReferenceText). O teste R-013 afere a BIJEÇÃO registo↔Docs↔erros.
//
// METADE 1 povoa LINKER/TYKER/COMPONENTE; a METADE 2 migra LINGUAGEM e
// COMANDO (as 28 entradas do VoniDocs.cpp) PARA AQUI — a partir daí o
// VoniDocs DERIVA do registo (docs::all() é uma VISTA).
#include "core/Types.h"
#include "voni/VoniTykers.h"   // Ctx/Comp/Handler (os handlers do runtime)

#include <string>
#include <vector>

namespace voni {

namespace reg {

using vv::u8;

// a face de cada entrada (as categorias das Docs §11 + o registo 0.9.3)
enum class Kind : u8 {
    Linguagem = 0,   // §§3-8: central main, repeat, exist, fn, …
    Comando,         // §9: View P, move, Import.Animation, …
    Linker,          // 0.9.3: linker(A)to(B)=RF(nome)
    Tyker,           // 0.9.3: tyker(nome){ find(RF) … }
    Componente,      // 0.9.3: follow/look/orbit/copy/map/…
};

inline const char* kindName(Kind k) {
    switch (k) {
        case Kind::Linguagem:  return "Linguagem";
        case Kind::Comando:    return "Comando";
        case Kind::Linker:     return "Linker";
        case Kind::Tyker:      return "Tyker";
        case Kind::Componente: return "Componente";
    }
    return "?";
}

// a natureza de execução de um COMPONENTE (só Kind::Componente)
enum class CompKind : u8 {
    Nenhum = 0,   // não é componente (Linguagem/Comando/Linker/Tyker)
    Continuo,     // corre a cada frame: follow/look/orbit/copy/map
    Pontual,      // dispara 1× na ativação: Change/point/colorpars/play/
                  // limit/delay
    Reservado,    // no-op reservado: shading
};

struct Entry {
    const char* name;         // nome canónico (chave)
    Kind        kind;
    const char* syntax;       // forma (Docs obrigatória 🔶)
    const char* desc;         // 1 linha (Docs obrigatória 🔶)
    const char* example;      // exemplo curto (Docs obrigatória 🔶)
    // ---- METADE 2 (a fonte única precisa deles desde o início) ----------
    const char* equiv;        // equivalência Python/JS ("" = sem)
    const char* skeleton;     // texto do Tab ("" = sem esqueleto)
    u32         skeletonCaret;// offset do caret DENTRO do skeleton
    // ---- componentes -----------------------------------------------------
    CompKind    compKind = CompKind::Nenhum;
    u8          minArgs = 0;  // cauda 1 (a 2ª cauda é do colorpars)
    u8          maxArgs = 0;
};

// TODAS as entradas ( METADE 1: Linker/Tyker/Componente; METADE 2 migra as
// restantes para AQUI — docs::all() passa a derivar deste vetor).
const std::vector<Entry>& all();

// por nome exato (qualquer face); null = não existe
const Entry* find(const std::string& name);
// por nome exato, só componentes; null = não é componente
const Entry* findComponent(const std::string& name);
// 1º nome que COMEÇA por `prefix` (completamento/mini-descrição); null = nada
const Entry* prefixMatch(const std::string& prefix);

// ---- dispatch de componentes (a tabela liga nome→handler do VoniTykers) --
// Devolve null se `name` não é um componente registado. O handler RECEBE o
// Comp já resolvido (args avaliados na ativação pelo Vm).
tykers::Handler handlerFor(const std::string& name);

// ---- PROVA de independência do parser (uso exclusivo de TESTES) ----------
// Instala um componente de teste no registo (Docs incluída) — a gramática
// NÃO conhece o nome e o tyker corre na mesma. Devolve true se instalado;
// o teste consulta a entrada por find() e limpa com resetForTest.
bool installForTest(const char* name, tykers::Handler handler,
                    const char* syntax, const char* desc,
                    const char* example);
void resetForTest();   // remove as instalações de teste

} // namespace reg
} // namespace voni
