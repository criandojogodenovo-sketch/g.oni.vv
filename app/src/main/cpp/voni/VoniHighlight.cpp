// voni/VoniHighlight.cpp — tokenizer de classes p/ o editor (ver header).
// Detetor simples linha-a-linha: palavras, números, strings, comentários.
// A lista de RESERVADAS vem do VoniGrammar.cpp (a definição da linguagem
// vive TODA num sítio); 'v', 'fn', 'return', 'true', 'false' são palavras
// ESTRUTURAIS (não estão na lista §5 de reservadas para nomes — podem ser
// nomes de variável! — mas colorem-se como linguagem no editor).
#include "voni/VoniHighlight.h"
#include "voni/Voni.h"

#include <cctype>

namespace voni {
namespace hl {

namespace {

bool isIdentStart(unsigned char c) { return std::isalpha(c) || c == '_'; }
bool isIdentChar(unsigned char c) {
    return std::isalnum(c) || c == '_';
}

// palavras ESTRUTURAIS da linguagem (colorem Reserved; as da lista §5
// adicionalmente são proibidas como nomes — ver Vm/Validator)
bool isStructuralWord(const std::string& w) {
    return w == "v" || w == "fn" || w == "return" || w == "true" ||
           w == "false";
}

} // namespace

Cls wordClass(const std::string& word) {
    if (isReservedWord(word) || isStructuralWord(word)) {
        return Cls::Reserved;
    }
    if (!word.empty() && word[0] >= 'A' && word[0] <= 'Z') {
        return Cls::Engine;
    }
    return Cls::User;
}

std::vector<Token> classifyLine(const std::string& line,
                                BlockCommentState& st) {
    std::vector<Token> out;
    const size_t n = line.size();
    size_t i = 0;

    // continuar dentro de um bloco /* */ aberto numa linha anterior
    if (st.inBlock) {
        size_t end = line.find("*/");
        if (end == std::string::npos) {
            Token t;
            t.begin = 0;
            t.len = static_cast<u32>(n);
            t.cls = Cls::Comment;
            out.push_back(t);
            return out;   // continua em bloco
        }
        Token t;
        t.begin = 0;
        t.len = static_cast<u32>(end + 2);
        t.cls = Cls::Comment;
        out.push_back(t);
        i = end + 2;
        st.inBlock = false;
    }

    while (i < n) {
        const unsigned char c = static_cast<unsigned char>(line[i]);

        // espaços — avança (o editor desenha o fundo; sem token)
        if (std::isspace(c)) {
            ++i;
            continue;
        }

        // comentário de linha
        if (c == '/' && i + 1 < n && line[i + 1] == '/') {
            Token t;
            t.begin = static_cast<u32>(i);
            t.len = static_cast<u32>(n - i);
            t.cls = Cls::Comment;
            out.push_back(t);
            break;
        }

        // comentário de bloco (abre)
        if (c == '/' && i + 1 < n && line[i + 1] == '*') {
            size_t end = line.find("*/", i + 2);
            Token t;
            t.begin = static_cast<u32>(i);
            if (end == std::string::npos) {
                t.len = static_cast<u32>(n - i);
                t.cls = Cls::Comment;
                out.push_back(t);
                st.inBlock = true;   // atravessa para as próximas linhas
                break;
            }
            t.len = static_cast<u32>(end + 2 - i);
            t.cls = Cls::Comment;
            out.push_back(t);
            i = end + 2;
            continue;
        }

        // string
        if (c == '"') {
            size_t j = i + 1;
            while (j < n && line[j] != '"') {
                ++j;
            }
            Token t;
            t.begin = static_cast<u32>(i);
            t.len = static_cast<u32>((j < n ? j + 1 : n) - i);
            t.cls = Cls::Str;
            out.push_back(t);
            i = j < n ? j + 1 : n;
            continue;
        }

        // número
        if (std::isdigit(c)) {
            size_t j = i;
            while (j < n && std::isdigit(static_cast<unsigned char>(line[j]))) {
                ++j;
            }
            if (j < n && line[j] == '.' && j + 1 < n &&
                std::isdigit(static_cast<unsigned char>(line[j + 1]))) {
                ++j;
                while (j < n &&
                       std::isdigit(static_cast<unsigned char>(line[j]))) {
                    ++j;
                }
            }
            Token t;
            t.begin = static_cast<u32>(i);
            t.len = static_cast<u32>(j - i);
            t.cls = Cls::Number;
            out.push_back(t);
            i = j;
            continue;
        }

        // identificador
        if (isIdentStart(c)) {
            size_t j = i + 1;
            while (j < n &&
                   isIdentChar(static_cast<unsigned char>(line[j]))) {
                ++j;
            }
            Token t;
            t.begin = static_cast<u32>(i);
            t.len = static_cast<u32>(j - i);
            t.cls = wordClass(line.substr(i, j - i));
            out.push_back(t);
            i = j;
            continue;
        }

        // pontuação/operadores — sem classe (cor neutra do texto)
        ++i;
    }
    return out;
}

} // namespace hl
} // namespace voni
