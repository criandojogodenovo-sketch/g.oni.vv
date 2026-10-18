#pragma once
// voni/VoniHighlight.h — CLASSIFICAÇÃO DE TOKENS para a coloração do editor
// de script (0.9.2 §10 🔶).
//
// O CONTRATO da spec: "O parser só expõe a CLASSE de cada token; as cores
// vivem no Theme". Este módulo é esse "parser" do ponto de vista do editor:
// recebe uma LINHA (ou o fonte todo) e devolve a classe de cada token. As
// CORES (paleta fechada #B39DDB/#8AB4F8/#F5F5F5/#81C784/#FFD54F/#757575)
// vivem em ui/Theme.h — AQUI não há um único valor de cor.
//
// Classe de um token:
//   Reserved — palavras reservadas da linguagem (§5) + v/fn/return/true/false
//   Engine   — identificadores que começam por MAIÚSCULA (View, Search, …)
//   User     — identificadores minúsculos do utilizador
//   Number   — 5 · 5.5
//   Str      — "texto"
//   Comment  — // linha e /* bloco */ (o estado do bloco atravessa linhas)
//
// GL-free / puro — host-testável.
#include "core/Types.h"

#include <string>
#include <vector>

namespace vv {
class FontAtlas;   // (o editor desenha; aqui só se classifica)
}

namespace voni {
namespace hl {

// aliases do engine (vv::) — igual ao Voni.h
using vv::u8;
using vv::u32;

enum class Cls : u8 {
    None = 0,
    Reserved,   // roxo (no Theme)
    Engine,     // azul
    User,       // branco
    Number,     // amarelo
    Str,        // verde
    Comment,    // cinzento
};

struct Token {
    u32 begin = 0;   // offset em bytes UTF-8 no fonte
    u32 len   = 0;
    Cls cls   = Cls::None;
};

// estado do comentário de BLOCO entre linhas (o editor processa linha a
// linha e mantém este estado entre chamadas)
struct BlockCommentState {
    bool inBlock = false;
};

// classifica UM token-run de uma linha (sem quebrar UTF-8 — os offsets são
// em bytes e os caracteres multibyte caem em User/None sem os partir).
// `line` = a linha; `st` = estado do bloco /* */ (entra e sai atualizado).
// devolve os tokens da linha (Comment de bloco cobre a linha toda se o
// bloco não fecha nela).
std::vector<Token> classifyLine(const std::string& line,
                                BlockCommentState& st);

// util p/ o editor/tests: a classe de uma palavra isolada
Cls wordClass(const std::string& word);

} // namespace hl
} // namespace voni
