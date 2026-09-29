// core/ProjectStorage.cpp — guarda de caminhos relativos (F5-A).
//
// Implementações de ProjectStorage chamam validRelPath ANTES de tocar o FS:
// o manifesto e os assets só referenciam caminhos dentro da raiz do projeto
// (defesa contra traversal — '../../secret' nunca sai da pasta do jogo).
#include "core/ProjectStorage.h"

namespace vv {

bool validRelPath(const std::string& rel) {
    if (rel.empty()) {
        return false;
    }
    if (rel.front() == '/' || rel.back() == '/') {
        return false;
    }
    std::string seg;
    const auto checkSeg = [&seg]() {
        return !seg.empty() && seg != "." && seg != "..";
    };
    for (const char c : rel) {
        if (c == '\\') {
            return false;   // só '/' como separador (refs portáveis)
        }
        if (c == '/') {
            if (!checkSeg()) {
                return false;
            }
            seg.clear();
            continue;
        }
        seg.push_back(c);
    }
    return checkSeg();
}

std::string joinRelPath(const std::string& root, const std::string& rel) {
    if (!validRelPath(rel)) {
        return "";
    }
    if (root.empty()) {
        return rel;
    }
    return root + "/" + rel;
}

} // namespace vv
