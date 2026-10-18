#pragma once
// voni/VoniAst.h — nós da AST da V.ONI (construída pelo VoniCompile a
// partir da gramática PEG; interpretada pelo VoniVm).
//
// TODOS os nós carregam a LINHA (1-based) — spec §12: "erros com linha no
// editor e no log". A linha vem do sv.line_info() da regra que criou o nó.
//
// Estilo: structs simples com Kind enum (sem variant/herança — o padrão do
// codebase: dados planos, debuggable, sem surpresas de lifetime). Só os
// membros do Kind ativo são usados.
#include "voni/Voni.h"

#include <memory>
#include <string>
#include <vector>

namespace voni {

// ---------------------------------------------------------------------------
// expressões
// ---------------------------------------------------------------------------
struct Expr;
// shared_ptr (não unique_ptr): o PEG faz backtrack e DESCARTA valores de
// ações de ramos que afinal falham — unique_ptr perdido = leak; shared_ptr
// descartado é simplesmente libertado.
using ExprP = std::shared_ptr<Expr>;

struct Expr {
    enum class Kind : u8 {
        Lit = 0,     // literal Int/Num/Txt/Bool
        Var,         // nome de variável (minúscula) — resolve: globais→fn-frame
        Path,        // caminho com pontos: jogador.pos.x / Search.a.b
        Call,        // chamada de fn do utilizador: soma(1,2)
        Unary,       // -x / not x
        Binary,      // x + y / x == y / x and y …
    };

    Kind        kind = Kind::Lit;
    u32         line = 1;

    // Lit
    Value       lit;
    // Var
    std::string name;
    // Path (≥1 segmento; segs[0] resolve a variável global | TIC | engine)
    std::vector<std::string> segs;
    // Call
    std::vector<ExprP> args;
    // Unary
    enum class Un : u8 { Neg, Not };
    Un          un = Un::Neg;
    ExprP       a;
    // Binary
    enum class Bin : u8 { Add, Sub, Mul, Div, Eq, Ne, Lt, Gt, Le, Ge, And, Or };
    Bin         bin = Bin::Add;
    ExprP       b;

    static ExprP make(Kind k, u32 line) {
        ExprP e(new Expr);
        e->kind = k;
        e->line = line;
        return e;
    }
};

// ---------------------------------------------------------------------------
// declarações e statements
// ---------------------------------------------------------------------------
struct Stmt;
using StmtP = std::shared_ptr<Stmt>;
using Block = std::vector<StmtP>;

struct Param {
    std::string name;
    Type        type = Type::None;
    u32         line = 1;
};

struct FnDef {
    std::string name;
    u32         line = 1;
    std::vector<Param> params;
    bool        hasRet = false;
    Type        ret = Type::None;
    Block       body;
};

// caso de and() (§7): condição + ação
struct AndCase {
    ExprP       cond;   // pode ser null → era a forma nua (cond=var implícita)
    std::string bare;   // forma nua: nome de condição (variável bool)
    u32         line = 1;
    Block       action;
};

// caso de option (§7): valor + ação
struct OptCase {
    ExprP       value;
    u32         line = 1;
    Block       action;
};

struct Stmt {
    enum class Kind : u8 {
        VarDecl = 0,   // v++ / v# (§4)
        Assign,        // x=1 · x+=1 · jogador.pos.x=5 (§§8-9)
        IfChain,       // exist{} notexist{} and(c(a) stopand) (§7)
        Option,        // option(c){ and v(a) stopand … notoption{} } (§7)
        Repeat,        // repeat(n){ } (§6)
        Last,          // last(c){ } · last(c) with n+=1 { } (§6)
        Continue,      // §6
        Resume,        // §6
        Return,        // §8
        PathCall,      // comandos/chamadas por caminho (§9)
    };

    Kind        kind = Kind::VarDecl;
    u32         line = 1;

    // VarDecl
    std::string varName;
    Type        varType = Type::None;   // None = inferido (v++)
    bool        exported = false;       // @+ (Inspector)
    ExprP       init;

    // Assign (path: [x] = variável · [jogador,pos,x] = propriedade)
    std::vector<std::string> path;
    bool        add = false;            // +=

    // IfChain: se cond → then; senão casos L→R (1º verdadeiro corre a ação
    // e salta o default); nenhum → otherwise (§7 — semântica FECHADA)
    ExprP       cond;
    Block       then;
    Block       otherwise;
    std::vector<AndCase> cases;

    // Option: sel == value → action; senão notoption (§7)
    ExprP       sel;
    std::vector<OptCase> optCases;
    bool        hasDefault = false;
    Block       defaultBlock;

    // Repeat / Last
    ExprP       count;                  // Repeat
    ExprP       whileCond;              // Last
    bool        hasCounter = false;     // Last with n+=1
    std::string counter;
    Block       body;

    // Return
    bool        hasValue = false;

    // PathCall (§9): argumentos entre '(' ')' (ou a string solta do View P)
    bool        hasArgs = false;
    std::vector<ExprP> args;

    static StmtP make(Kind k, u32 line) {
        StmtP s(new Stmt);
        s->kind = k;
        s->line = line;
        return s;
    }
};

// central main { on moment { } allmoments { } } (§3)
struct CentralMain {
    bool hasOn = false;
    u32  onLine = 1;
    Block onMoment;
    bool hasAll = false;
    u32  allLine = 1;
    Block allMoments;
};

// ---------------------------------------------------------------------------
// LINKERS & TYKERS (0.9.5 · METADE 1 — declarações de TOPO como as fn)
// ---------------------------------------------------------------------------

// linker(A)to(B)=RF(nome) — A e B são caminhos (objeto/TIC/propriedade/
// animação); o link é registado no RF(nome) no arranque do script.
struct LinkerDecl {
    std::vector<std::string> origem;
    std::vector<std::string> destino;
    std::string rf;
    u32 line = 1;
};

// um componente dentro do corpo de um tyker (ainda NÃO avaliado — a
// resolução dos args corre na ATIVAÇÃO, onde as variáveis já existem)
struct TykerComp {
    std::string name;              // "follow" / "Change" / …
    u32 line = 1;
    std::vector<ExprP> args;       // 1ª cauda (Exprs — números, vars, nomes)
    bool hasTail2 = false;         // 2ª cauda (só colorpars)
    ExprP arg2 = nullptr;          // 2ª cauda como Expr (nome de cor)
    std::string color2;            // 2ª cauda como #RRGGBB (ColorLit)
    bool isColor2 = false;
    // Change(origem|destino)to(alvo)
    bool isChange = false;
    bool changeDestino = false;
    std::vector<std::string> changePath;
};

// tyker(nome){ find(RF) comps… } — o corpo SÓ aceita componentes; a
// gramática aceita statements genéricos para o VALIDADOR os rejeitar com
// erro QUE ENSINA (linha + o que fazer em vez de syntax error seco).
struct TykerDef {
    std::string name;
    u32 line = 1;
    bool hasFind = false;          // find(RF) presente como 1º componente
    std::string findRf;
    u32 findLine = 1;
    bool sawLateFind = false;      // find fora do 1º lugar (erro legível)
    u32 lateFindLine = 0;
    bool hasStray = false;         // statement que não é componente (erro)
    u32 strayLine = 0;
    std::string strayName;         // o nome do statement rejeitado
    std::vector<TykerComp> comps;
};

// script inteiro: top-level 1× (§3) + fns + central main + linkers/tykers
struct Program {
    Block top;
    std::vector<FnDef> fns;
    std::vector<LinkerDecl> linkers;   // 0.9.5: declarações de topo
    std::vector<TykerDef> tykers;      // 0.9.5: blocos de comportamento
    bool hasCentral = false;
    u32  centralLine = 1;
    CentralMain central;
};

} // namespace voni
