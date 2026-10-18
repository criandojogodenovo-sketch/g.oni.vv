// voni/VoniDocs.cpp — as entradas das Docs V.ONI (spec 0.9.2 §11 ✅).
//
// 0.9.5 · METADE 2: ESTE FICHEIRO É UMA VISTA. TODA a superfície ensinável
// (Linguagem §§3-8 + Comandos §9 + Linker/Tyker/Componente da 0.9.5) vive
// no REGISTO CENTRAL (VoniRegistry.cpp) — a MESMA tabela que valida os
// componentes no compile, os despacha no runtime, alimenta os
// erros-que-ensinam, os tooltips, o completamento e a referência pública.
// O teste R-013 afere a BIJEÇÃO: cada entrada do registo aparece AQUI com
// os mesmos campos, e nada aqui vem de fora do registo.
#include "voni/VoniDocs.h"
#include "voni/VoniRegistry.h"

#include <algorithm>
#include <cctype>

namespace voni {
namespace docs {

// a VISTA: docs::all() é o registo mapeado 1:1 (Kind → Cat)
const std::vector<Entry>& all() {
    static const std::vector<Entry> kAll = [] {
        std::vector<Entry> out;
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
    }();
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
