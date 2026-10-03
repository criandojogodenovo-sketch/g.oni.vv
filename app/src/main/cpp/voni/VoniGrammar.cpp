// voni/VoniGrammar.cpp — A GRAMÁTICA PEG DA V.ONI + A LISTA DE RESERVADAS
// (0.9.2 §1 🔶 e §5 ✅).
//
// FICHEIRO ÚNICO SEPARADO DO CÓDIGO C++ DA ENGINE: TODA a definição da
// linguagem vive NESTE ficheiro (a string da gramática + a tabela de
// palavras reservadas — ambas são DADO, não código). Mudar um literal aqui
// muda a linguagem SEM tocar em mais nenhum ficheiro C++ (o requisito 🔶
// da spec: "gramática num único ficheiro separado do código C++").
//
// Como mudar a linguagem:
//   • Em produção: editar esta string (o único sítio) e recompilar.
//   • Em testes SEM recompilar: voni::setGrammar(modificada) troca em
//     runtime — é o que o teste da spec usa ("mudar um literal no ficheiro
//     de gramática muda comportamento sem tocar em C++": o teste copia a
//     gramática, muda 'repeat' para outro literal e afere que a fonte com
//     a palavra velha deixa de parsear e a nova passa).
//
// CONVENÇÕES PEG (cpp-peglib v1.8.6 — pin em vendor/peglib):
//   • %whitespace com `*` (NÃO `+` — com `+` o fim-de-input mata os loops)
//     e inclui os comentários da spec §2: // linha e /* bloco */.
//   • %word impede 'lastx' ler-se como 'last'+'x' (e 'stopand' como
//     'stop'+'and' — 'stopand' é literal próprio).
//   • Literais da gramática NÃO empurram valores semânticos; as ações no
//     VoniCompile constroem a AST via sv[i] (filhos) e sv.token() (terminais).
//   • NOMES (variáveis/fn/params/contadores) aceitam IdentAny AQUI e a
//     VALIDAÇÃO (reservada §5 / maiúscula=engine §5) vive no VoniCompile
//     para dar erros com LINHA em vez de syntax error seco.
//
// DESAMBIGUAÇÕES DE FORMA (decisões 🔶 documentadas, isoladas aqui):
//   • CaseItem/OptionCase: `cond(ação)` com cond VALOR NUA — ex.
//     `and flag(move(1,0,0)) stopand` — colidiria com FnCall `flag(...)`.
//     A forma do AUTOR ganha: BareCase (ident + '(' ação ')') tenta-se
//     ANTES da ExprCase; uma condição que SEJA uma chamada de função não
//     é forma válida (condições são expressões; escreve-se `f(x) == 3`).
//   • Atom: FnCall antes de PathExpr (senão `x=soma(1,2)` comia só `soma`
//     e o '(' sobrante matava o parse — PEG não re-tenta alternativas
//     depois de um sucesso comprometido).
//   • CallTail é OPCIONAL: `Explode.TIC.et` não tem argumentos.
//   • BoolLit antes de PathExpr (senão 'true' lia-se como caminho).
//
// SINTAXE (spec fechada 0.9.2 §§2-9 — a ÚNICA fonte de verdade):
//   central main { on moment { } allmoments { } }   entry point (§3)
//   v++nome=valor · v#nome:Tipo=valor · @+ exporta  (§4)
//   repeat(n){ } · last(cond){ } · last(c) with n+=1 (§6)
//   continue · resume                               (§6)
//   exist(c){ } notexist{ } + and( c(a) stopand … ) (§7)
//   option(c){ and valor(ação) stopand … notoption{ } } (§7)
//   fn nome(a:Num):Num { return a+b }               (§8)
//   comandos: View P "t" · move(x,y,z) · Import.Animation("n")
//             cena.transition.for("dest") · Deltatime.Increment(v,x)
//             Explode.TIC.et/.er · Search.alvo.prop (§9)
//   NÃO EXISTE if/else/switch/case/break (spec §6 — NUNCA).
#include "voni/Voni.h"

#include <string>
#include <cstring>

namespace voni {

// A gramática como DADO (ver notas de forma no topo do ficheiro).
const char* kVoniGrammar = R"VONIPEG(
# --------------------------- V.ONI v0 (0.9.2) -------------------------------

Script      <- Item*
Item        <- CentralMain / FnDef / Statement

CentralMain <- 'central' 'main' CMBody
CMBody      <- '{' CMItem* '}'
CMItem      <- OnMoment / AllMoments
OnMoment    <- 'on' 'moment' Block
AllMoments  <- 'allmoments' Block

FnDef       <- 'fn' IdentAny Params RetType? Block
Params      <- '(' (Param (',' Param)*)? ')'
Param       <- IdentAny ':' IdentAny
RetType     <- ':' IdentAny

Block       <- '{' Statement* '}'

# statement: a ordem importa (PEG escolha ordenada) — VarDecl/IfChain/…
# antes de Assignment/PathCall (senão 'x=1' comia o caminho 'x')
Statement   <- VarDecl / IfChain / OptionStmt / LoopStmt
             / JumpStmt / ReturnStmt / Assignment / PathCall

# --- variáveis (§4): v++nome=valor · v#nome:Tipo=valor · @+ exporta --------
VarDecl     <- VarInfExp / VarInf / VarTypExp / VarTyp
VarInfExp   <- 'v' '++' '@+' IdentAny '=' Expr
VarInf      <- 'v' '++' IdentAny '=' Expr
VarTypExp   <- 'v' '#' '@+' IdentAny ':' IdentAny '=' Expr
VarTyp      <- 'v' '#' IdentAny ':' IdentAny '=' Expr

# --- condicionais (§7) ------------------------------------------------------
IfChain     <- ExistStmt NotExistStmt?
ExistStmt   <- 'exist' '(' Expr ')' Block
NotExistStmt<- 'notexist' Block AndChain?
AndChain    <- 'and' '(' CaseItem+ ')'
# forma do autor: condição nua `cond(ação)` ANTES da expr genérica (ver
# nota de desambiguação no topo) — `flag(ação)` é CONDIÇÃO flag, não FnCall
CaseItem    <- BareCase / ExprCase
BareCase    <- IdentAny '(' Statement* ')' 'stopand'
ExprCase    <- Expr '(' Statement* ')' 'stopand'

OptionStmt  <- 'option' '(' Expr ')' '{' OptionCase* NotOption? '}'
OptionCase  <- BareOpt / ExprOpt
BareOpt     <- 'and' IdentAny '(' Statement* ')' 'stopand'
ExprOpt     <- 'and' Expr '(' Statement* ')' 'stopand'
NotOption   <- 'notoption' Block

# --- loops (§6) -------------------------------------------------------------
LoopStmt    <- RepeatStmt / LastStmt
RepeatStmt  <- 'repeat' '(' Expr ')' Block
LastStmt    <- 'last' '(' Expr ')' WithClause? Block
WithClause  <- 'with' IdentAny '+=' '1'

# JumpStmt CAPTURADO (a ação precisa de saber continue vs resume)
JumpStmt    <- < 'continue' / 'resume' >
ReturnStmt  <- 'return' Expr?

# --- atribuição e caminhos (§§8-9) -----------------------------------------
# AssignOp CAPTURADO (literal puro não empurra valor — a ação precisa de
# saber = vs +=); '+=' ANTES de '=' na ordem da escolha.
Assignment  <- Path AssignOp Expr
AssignOp    <- < '+=' / '=' >
# ViewStmt: o ÚNICO comando com segmentos separados por ESPAÇO (spec §9:
# `View P "texto"` / `View Object`) — ANTES de Path (senão Path comia só
# [View] e sobrava 'P'). Os restantes comandos são cadeias de pontos.
PathCall    <- ViewStmt / Path CallTail?
ViewStmt    <- 'View' IdentAny CallTail?
Path        <- IdentAny ('.' IdentAny)*
CallTail    <- '(' ArgList? ')' / StrLit

ArgList     <- Expr (',' Expr)*

# --- expressões (§8): or < and < not < cmp < add < mul < unário < átomo ----
# NotUnary/NegExpr em regras PRÓPRIAS (senão 'not x' e 'x' davam ambas
# sv.size()==1 e a ação não distinguia). AddOp/MulOp/CmpOp CAPTURADOS
# (o operador tem de chegar à ação).
Expr        <- OrExpr
OrExpr      <- AndExpr ('or' AndExpr)*
AndExpr     <- NotExpr ('and' NotExpr)*
NotExpr     <- NotUnary / CmpExpr
NotUnary    <- 'not' NotExpr
CmpExpr     <- AddExpr (CmpOp AddExpr)?
CmpOp       <- < '==' / '!=' / '<=' / '>=' / '<' / '>' >
AddExpr     <- MulExpr (AddOp MulExpr)*
AddOp       <- < '+' / '-' >
MulExpr     <- UnaryExpr (MulOp UnaryExpr)*
MulOp       <- < '*' / '/' >
UnaryExpr   <- NegExpr / Atom
NegExpr     <- '-' UnaryExpr

Atom        <- NumLit / StrLit / BoolLit / FnCall / PathExpr / ParenExpr
FnCall      <- IdentAny '(' ArgList? ')'
PathExpr    <- IdentAny ('.' IdentAny)*
ParenExpr   <- '(' Expr ')'

NumLit      <- < [0-9]+ '.' [0-9]+ > / < [0-9]+ >
StrLit      <- < '"' (!'"' .)* '"' >
BoolLit     <- < 'true' / 'false' >

IdentAny    <- < [a-zA-Z] [a-zA-Z0-9_]* >

%whitespace  <- ( [ \t\r\n]+
                / '//' (![\n] .)*
                / '/*' (!'*/' .)* '*/'
                )*
%word        <- [a-zA-Z0-9_]
)VONIPEG";

// ---------------------------------------------------------------------------
// PALAVRAS RESERVADAS (spec §5 ✅ — lista FIXA, minúsculas, nesta ordem no
// relatório). Usar uma como nome de variável/fn/param → erro claro
// "'x' é uma palavra reservada" (a validação corre no VoniCompile, com linha).
//
// Nomes de componentes de tyker (follow/look/orbit/copy/map/point/colorpars/
// play/limit/delay/shading/Change/find) são reservados SÓ DENTRO de bloco
// tyker (0.9.3) — em 0.9.2 NÃO estão nesta lista (fora de tyker podem ser
// nomes de variáveis, spec §5).
// ---------------------------------------------------------------------------
static const char* const kReserved[] = {
    "exist", "notexist", "option", "and", "stopand", "notoption",
    "repeat", "last", "with", "continue", "resume", "move",
    "linker", "to", "tyker",
    "central", "main", "on", "moment", "allmoments",
};

bool isReservedWord(const std::string& lower) {
    for (const char* const* p = kReserved; *p; ++p) {
        if (lower == *p) {
            return true;
        }
    }
    return false;
}

const char* const* reservedWords() { return kReserved; }

u32 reservedWordCount() {
    u32 n = 0;
    while (kReserved[n]) {
        ++n;
    }
    return n;
}

// ---------------------------------------------------------------------------
// gramática ativa (override de testes — setGrammar(nullptr) volta ao default)
// ---------------------------------------------------------------------------
static std::string g_grammarOverride;

void setGrammar(const char* grammarOverrideOrNull) {
    if (grammarOverrideOrNull && *grammarOverrideOrNull) {
        g_grammarOverride = grammarOverrideOrNull;
    } else {
        g_grammarOverride.clear();
    }
}

const char* activeGrammar() {
    return g_grammarOverride.empty() ? kVoniGrammar : g_grammarOverride.c_str();
}

} // namespace voni
