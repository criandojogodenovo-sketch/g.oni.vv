#pragma once
// voni/VoniDocs.h — CONTEÚDO DAS DOCS V.ONI (0.9.2 §11 ✅).
//
// "Settings → Docs com pesquisa (lupa); entrada por comando/linker/tyker/
//  componente com: nome, descrição de 1 linha, sintaxe, exemplo curto."
//
// As entradas vivem em tabela estática (DADO, não código) — a UI (Docs
// Screen) pesquisa por nome/descrição e renderiza. As categorias
// kLinker/kTyker/kComponente existem JÁ (a pesquisa e o agrupamento são
// genéricos) mas 0.9.3 é que povoa — aqui entram COMANDOS (§9) e
// LINGUAGEM (§§3-8).
#include "core/Types.h"

#include <cstring>
#include <string>
#include <vector>

namespace voni {
namespace docs {

// aliases do engine (vv::)
using vv::u8;

enum class Cat : u8 {
    Comando = 0,    // §9: View P, move, Import.Animation, …
    Linguagem,      // §§3-8: central main, repeat, exist, fn, v++, …
    Linker,         // 0.9.3 (spec 8): linker(A)to(B)=RF(nome)
    Tyker,          // 0.9.3: tyker(nome){ find(RF) … }
    Componente,     // 0.9.3: follow/look/orbit/…
};

inline const char* catName(Cat c) {
    switch (c) {
        case Cat::Comando:    return "Comando";
        case Cat::Linguagem:  return "Linguagem";
        case Cat::Linker:     return "Linker";
        case Cat::Tyker:      return "Tyker";
        case Cat::Componente: return "Componente";
    }
    return "?";
}

struct Entry {
    Cat         cat;
    const char* name;     // nome canónico (chave de pesquisa)
    const char* desc;     // 1 linha
    const char* syntax;   // forma
    const char* example;  // exemplo curto
};

// TODAS as entradas (0.9.2: Comando + Linguagem; Linker/Tyker/Componente
// vazias até 0.9.3 — a estrutura já as aceita)
const std::vector<Entry>& all();

// pesquisa case-insensitive por substring no nome OU descrição; query vazia
// devolve tudo. Puro — o ecrã de Docs chama por frame? NÃO: o ecrã guarda o
// resultado por query (ver DocsScreen); esta função é barata mas sem cache.
std::vector<const Entry*> search(const std::string& query);

} // namespace docs
} // namespace voni
