// voni/VoniCompile.cpp — PEG → AST (0.9.2 §1).
//
// ÚNICO TU que inclui o peglib (vendor/peglib/peglib.h v1.8.6 — pin): o
// resto do voni/ não sabe que o parser existe. A GRAMÁTICA vive em
// VoniGrammar.cpp (ficheiro único 🔶 — mudar lá muda a linguagem sem
// tocar aqui); este ficheiro só LÊ a gramática ativa (default ou override
// de setGrammar) e registra as AÇÕES que constroem a AST de VoniAst.h.
//
// REGRAS DE OURO DAS AÇÕES (peglib v1.8.6):
//   • Literais puros NÃO empurram valores; regras CAPTURADAS (<'…'>) dão
//     sv.token(). Tudo o que a ação precisa de DISTINGUIR tem captura ou
//     regra própria na gramática (AssignOp/AddOp/MulOp/CmpOp/BoolLit/
//     JumpStmt/NotUnary/NegExpr — ver notas na gramática).
//   • Opção/estrela ausentes NÃO empurram nada — os índices do sv mudam
//     consoante a presença (FnDef 3 ou 4, LastStmt 2 ou 3, ReturnStmt 0/1).
//   • sv.line_info().first = linha do INÍCIO da regra (1-based) — a linha
//     que os erros da spec §12 reportam.
//   • Valores atravessam std::any; o passthrough é `return sv[0];`.
//
// ERROS: duas camadas —
//   • SINTAXE: Result::error_info.error_pos → linha contada no fonte;
//   • VALIDAÇÃO (depois do parse, com linhas — spec §§4-5): maiúscula em
//     nome de utilizador, palavra reservada, redeclaração, fn duplicada,
//     central main duplicado, evento duplicado, tipo desconhecido.
#include "voni/Voni.h"
#include "voni/VoniAst.h"
#include "voni/VoniInternal.h"
#include "voni/VoniRegistry.h"

#include "vendor/peglib/peglib.h"

#include <cctype>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace voni {

// ---------------------------------------------------------------------------
// tipos de valor que circulam entre as ações (privados deste TU)
// ---------------------------------------------------------------------------
namespace {

struct Tok {           // terminal com linha (identificadores e operadores)
    std::string text;
    u32         line;
};

struct NumV {          // NumLit
    bool isFloat = false;
    i64  i = 0;
    f64  n = 0.0;
};

struct StrV {          // StrLit / BoolLit (bool como texto "true"/"false")
    std::string s;
    u32         line;
    bool        isBool = false;
};

using PathToks = std::vector<Tok>;
using Exprs    = std::vector<ExprP>;

struct CallTailV {     // '(' args ')' OU string solta (View P "texto")
    bool hasArgs = false;
    Exprs args;
    bool hasStr = false;
    std::string str;
    u32  line = 1;
};

struct BlockV {
    Block block;
};

struct ParamV {
    std::vector<Param> ps;
};

struct FnDefV {
    FnDef def;
};

struct CmEventV {      // on moment / allmoments (antes da validação)
    bool  isOn = false;
    u32   line = 1;
    Block body;
};

struct CentralV {      // central main cru (antes da validação)
    u32 line = 1;
    std::vector<CmEventV> events;
};

struct AndCaseV {
    AndCase ac;
};

struct OptCaseV {
    OptCase oc;
};

struct NotExistV {     // notexist{…} (+ and(…) colado?)
    Block otherwise;
    bool hasChain = false;
    std::vector<AndCaseV> cases;
};

struct CounterV {      // with n+=1
    std::string name;
    u32 line = 1;
};

// ---- 0.9.5: LINKERS & TYKERS ---------------------------------------------
struct LinkerDeclV {   // linker(A)to(B)=RF(nome)
    LinkerDecl decl;
};

struct CompArgV {       // um argumento de componente: Expr OU cor #RRGGBB
    ExprP       expr = nullptr;
    std::string color;
    bool        isColor = false;
};

using CompTailV = std::vector<CompArgV>;

struct TykerCompV {     // componente (inclui a forma Change…to…)
    TykerComp comp;
};

struct TykerFindV {     // find(rf)
    std::string rf;
    u32 line = 1;
};

struct TykerDefV {      // tyker(nome){ … } cru
    TykerDef def;
};

struct RawProgram {    // script cru (antes da validação)
    Block top;
    std::vector<FnDef> fns;
    std::vector<CentralV> centrals;
    std::vector<LinkerDecl> linkers;   // 0.9.5
    std::vector<TykerDef> tykers;      // 0.9.5
};

u32 lineOf(const peg::SemanticValues& sv) {
    return static_cast<u32>(sv.line_info().first);
}

ExprP mkBin(ExprP a, Expr::Bin op, ExprP b, u32 line) {
    ExprP e = Expr::make(Expr::Kind::Binary, line);
    e->bin = op;
    e->a = std::move(a);
    e->b = std::move(b);
    return e;
}

// o NOME de um statement "perdido" dentro de um tyker (para o erro que
// ensina dizer O QUE foi rejeitado — "'View P' é código de script")
std::string strayNameOf(const Stmt& s) {
    switch (s.kind) {
        case Stmt::Kind::VarDecl: return "v++/v#";
        case Stmt::Kind::Assign: {
            std::string p;
            for (size_t i = 0; i < s.path.size(); ++i) {
                if (i) {
                    p += '.';
                }
                p += s.path[i];
            }
            return p.empty() ? "atribuição" : p;
        }
        case Stmt::Kind::IfChain:  return "exist";
        case Stmt::Kind::Option:   return "option";
        case Stmt::Kind::Repeat:   return "repeat";
        case Stmt::Kind::Last:     return "last";
        case Stmt::Kind::Continue: return "continue";
        case Stmt::Kind::Resume:   return "resume";
        case Stmt::Kind::Return:   return "return";
        case Stmt::Kind::PathCall: {
            std::string p;
            for (size_t i = 0; i < s.path.size(); ++i) {
                if (i) {
                    p += '.';
                }
                p += s.path[i];
            }
            return p.empty() ? "comando" : p;
        }
    }
    return "código de script";
}

} // namespace

// ---------------------------------------------------------------------------
// o compilador
// ---------------------------------------------------------------------------
namespace {

struct Compiler {
    peg::parser parser;

    // erros de tipo encontrados nas AÇÕES (a ação sabe o nome escrito;
    // o validador só vê Type::None) — linha + mensagem
    std::vector<std::pair<u32, std::string>> typeErrs;

    Compiler() : parser(activeGrammar()) {
        if (parser) {
            registerTerminals();
            registerExpressions();
            registerStatements();
            registerTykers();
            registerStructure();
        }
    }

    bool ok() { return static_cast<bool>(parser); }

    // ---- terminais --------------------------------------------------------
    void registerTerminals() {
        parser["IdentAny"] = [](const peg::SemanticValues& sv) -> Tok {
            return Tok{std::string(sv.token()), lineOf(sv)};
        };
        parser["NumLit"] = [](const peg::SemanticValues& sv) -> NumV {
            const std::string t(sv.token());
            NumV v;
            if (t.find('.') != std::string::npos) {
                v.isFloat = true;
                v.n = std::strtod(t.c_str(), nullptr);
            } else {
                v.i = std::strtoll(t.c_str(), nullptr, 10);
            }
            return v;
        };
        parser["StrLit"] = [](const peg::SemanticValues& sv) -> StrV {
            // token COM aspas (captura inteira — o <…> desliga o skip de
            // whitespace lá dentro; senão " " perdia o espaço): strip 1ª/última
            std::string t(sv.token());
            if (t.size() >= 2 && t.front() == '"' && t.back() == '"') {
                t = t.substr(1, t.size() - 2);
            }
            return StrV{std::move(t), lineOf(sv), false};
        };
        parser["BoolLit"] = [](const peg::SemanticValues& sv) -> StrV {
            return StrV{std::string(sv.token()), lineOf(sv), true};
        };
    }

    void registerExpressions() {
        // Atom: CONVERTER literais (NumV/StrV crus) em nós Lit — as regras
        // de expressão acima esperam ExprP em TODOS os filhos (o passthrough
        // puro deixaria NumV/StrV passar e o any_cast<ExprP> daria nullptr).
        parser["Atom"] = [](const peg::SemanticValues& sv) -> ExprP {
            if (const ExprP* e = std::any_cast<ExprP>(&sv[0])) {
                return *e;   // FnCall / PathExpr / ParenExpr
            }
            if (const NumV* n = std::any_cast<NumV>(&sv[0])) {
                ExprP e = Expr::make(Expr::Kind::Lit, lineOf(sv));
                e->lit = n->isFloat ? Value::ofNum(n->n) : Value::ofInt(n->i);
                return e;
            }
            if (const StrV* s = std::any_cast<StrV>(&sv[0])) {
                ExprP e = Expr::make(Expr::Kind::Lit, lineOf(sv));
                e->lit = s->isBool ? Value::ofBool(s->s == "true")
                                   : Value::ofTxt(s->s);
                return e;
            }
            return Expr::make(Expr::Kind::Lit, lineOf(sv));   // inalcançável
        };
        parser["ParenExpr"] = [](const peg::SemanticValues& sv) -> std::any {
            return sv[0];
        };

        parser["FnCall"] = [](const peg::SemanticValues& sv) -> ExprP {
            ExprP e = Expr::make(Expr::Kind::Call, lineOf(sv));
            const Tok* name = std::any_cast<Tok>(&sv[0]);
            e->name = name ? name->text : "?";
            if (sv.size() > 1) {
                if (const Exprs* args = std::any_cast<Exprs>(&sv[1])) {
                    e->args = *args;
                }
            }
            return e;
        };
        parser["PathExpr"] = [](const peg::SemanticValues& sv) -> ExprP {
            ExprP e = Expr::make(Expr::Kind::Path, lineOf(sv));
            for (const std::any& v : sv) {
                if (const Tok* t = std::any_cast<Tok>(&v)) {
                    e->segs.push_back(t->text);
                }
            }
            return e;
        };
        parser["ArgList"] = [](const peg::SemanticValues& sv) -> Exprs {
            Exprs out;
            out.reserve(sv.size());
            for (const std::any& v : sv) {
                if (const ExprP* e = std::any_cast<ExprP>(&v)) {
                    out.push_back(*e);
                }
            }
            return out;
        };

        // or/and folds (uniformes — o operador não precisa de chegar à ação)
        parser["OrExpr"] = [](const peg::SemanticValues& sv) -> ExprP {
            ExprP left = *std::any_cast<ExprP>(&sv[0]);
            for (size_t i = 1; i < sv.size(); ++i) {
                left = mkBin(std::move(left), Expr::Bin::Or,
                             *std::any_cast<ExprP>(&sv[i]), lineOf(sv));
            }
            return left;
        };
        parser["AndExpr"] = [](const peg::SemanticValues& sv) -> ExprP {
            ExprP left = *std::any_cast<ExprP>(&sv[0]);
            for (size_t i = 1; i < sv.size(); ++i) {
                left = mkBin(std::move(left), Expr::Bin::And,
                             *std::any_cast<ExprP>(&sv[i]), lineOf(sv));
            }
            return left;
        };
        parser["NotExpr"] = [](const peg::SemanticValues& sv) -> std::any {
            return sv[0];
        };
        parser["NotUnary"] = [](const peg::SemanticValues& sv) -> ExprP {
            ExprP e = Expr::make(Expr::Kind::Unary, lineOf(sv));
            e->un = Expr::Un::Not;
            e->a = *std::any_cast<ExprP>(&sv[0]);
            return e;
        };
        parser["NegExpr"] = [](const peg::SemanticValues& sv) -> ExprP {
            ExprP e = Expr::make(Expr::Kind::Unary, lineOf(sv));
            e->un = Expr::Un::Neg;
            e->a = *std::any_cast<ExprP>(&sv[0]);
            return e;
        };
        parser["UnaryExpr"] = [](const peg::SemanticValues& sv) -> std::any {
            return sv[0];
        };
        parser["Expr"] = [](const peg::SemanticValues& sv) -> std::any {
            return sv[0];
        };

        // comparação (única, não associativa — spec §8)
        parser["CmpExpr"] = [](const peg::SemanticValues& sv) -> std::any {
            if (sv.size() < 3) {
                return sv[0];
            }
            const std::string op = std::any_cast<Tok>(&sv[1])->text;
            Expr::Bin b = Expr::Bin::Eq;
            if      (op == "==") b = Expr::Bin::Eq;
            else if (op == "!=") b = Expr::Bin::Ne;
            else if (op == "<")  b = Expr::Bin::Lt;
            else if (op == ">")  b = Expr::Bin::Gt;
            else if (op == "<=") b = Expr::Bin::Le;
            else                 b = Expr::Bin::Ge;
            return mkBin(*std::any_cast<ExprP>(&sv[0]), b,
                         *std::any_cast<ExprP>(&sv[2]), lineOf(sv));
        };
        parser["CmpOp"] = [](const peg::SemanticValues& sv) -> Tok {
            return Tok{std::string(sv.token()), lineOf(sv)};
        };

        // + - e * / folds (o operador CHEGOU como Tok — regras AddOp/MulOp)
        parser["AddExpr"] = [](const peg::SemanticValues& sv) -> ExprP {
            ExprP left = *std::any_cast<ExprP>(&sv[0]);
            for (size_t i = 1; i + 1 < sv.size(); i += 2) {
                const std::string op = std::any_cast<Tok>(&sv[i])->text;
                left = mkBin(std::move(left),
                             op == "+" ? Expr::Bin::Add : Expr::Bin::Sub,
                             *std::any_cast<ExprP>(&sv[i + 1]), lineOf(sv));
            }
            return left;
        };
        parser["AddOp"] = [](const peg::SemanticValues& sv) -> Tok {
            return Tok{std::string(sv.token()), lineOf(sv)};
        };
        parser["MulExpr"] = [](const peg::SemanticValues& sv) -> ExprP {
            ExprP left = *std::any_cast<ExprP>(&sv[0]);
            for (size_t i = 1; i + 1 < sv.size(); i += 2) {
                const std::string op = std::any_cast<Tok>(&sv[i])->text;
                left = mkBin(std::move(left),
                             op == "*" ? Expr::Bin::Mul : Expr::Bin::Div,
                             *std::any_cast<ExprP>(&sv[i + 1]), lineOf(sv));
            }
            return left;
        };
        parser["MulOp"] = [](const peg::SemanticValues& sv) -> Tok {
            return Tok{std::string(sv.token()), lineOf(sv)};
        };
    }

    // ---- statements -------------------------------------------------------
    void registerStatements() {
        parser["Block"] = [](const peg::SemanticValues& sv) -> BlockV {
            BlockV b;
            b.block.reserve(sv.size());
            for (const std::any& v : sv) {
                if (const StmtP* s = std::any_cast<StmtP>(&v)) {
                    b.block.push_back(*s);
                }
            }
            return b;
        };
        parser["Statement"] = [](const peg::SemanticValues& sv) -> std::any {
            return sv[0];
        };

        // v++ / v# (§4) — 4 formas em regras próprias (a ação sabe qual é)
        // FASE 9 (loop ASan): std::function POR VALOR — as ações do parser
        // VIVEM além do retorno de registerStatements(); capturar a lambda
        // `auto` por referência (&) deixava um ponteiro para a STACK MORTA
        // da função (use-after-return desde a 0.9.2).
        std::function<StmtP(const peg::SemanticValues&, bool, bool)>
            varDeclAction = [this](const peg::SemanticValues& sv, bool typed,
                                 bool exported) -> StmtP {
            // v++: [name, init] · v#: [name, tipoTok, init]
            StmtP s = Stmt::make(Stmt::Kind::VarDecl, lineOf(sv));
            const Tok* name = std::any_cast<Tok>(&sv[0]);
            s->varName = name ? name->text : "?";
            s->line = name ? name->line : lineOf(sv);
            size_t initIdx = 1;
            if (typed) {
                const Tok* t = std::any_cast<Tok>(&sv[1]);
                if (t) {
                    Type tt;
                    if (typeFromName(t->text.c_str(), tt)) {
                        s->varType = tt;
                    } else {
                        typeErrs.emplace_back(
                            t->line, "tipo '" + t->text +
                                         "' não existe (tipos: Int Num Txt " +
                                         "Bool Vec2 Vec3 TIC)");
                    }
                    initIdx = 2;
                }
            }
            s->exported = exported;
            s->init = *std::any_cast<ExprP>(&sv[initIdx]);
            return s;
        };
        parser["VarInfExp"] = [varDeclAction](
                const peg::SemanticValues& sv) -> StmtP {
            return varDeclAction(sv, false, true);
        };
        parser["VarInf"] = [varDeclAction](
                const peg::SemanticValues& sv) -> StmtP {
            return varDeclAction(sv, false, false);
        };
        parser["VarTypExp"] = [varDeclAction](
                const peg::SemanticValues& sv) -> StmtP {
            return varDeclAction(sv, true, true);
        };
        parser["VarTyp"] = [varDeclAction](
                const peg::SemanticValues& sv) -> StmtP {
            return varDeclAction(sv, true, false);
        };
        parser["VarDecl"] = [](const peg::SemanticValues& sv) -> std::any {
            return sv[0];
        };

        // atribuição (§§8-9): x=1 · x+=1 · jogador.pos.x=5
        parser["Assignment"] = [](const peg::SemanticValues& sv) -> StmtP {
            StmtP s = Stmt::make(Stmt::Kind::Assign, lineOf(sv));
            if (const PathToks* p = std::any_cast<PathToks>(&sv[0])) {
                for (const Tok& t : *p) {
                    s->path.push_back(t.text);
                }
            }
            const Tok* op = std::any_cast<Tok>(&sv[1]);
            s->add = op && op->text == "+=";
            s->init = *std::any_cast<ExprP>(&sv[2]);
            return s;
        };
        parser["AssignOp"] = [](const peg::SemanticValues& sv) -> Tok {
            return Tok{std::string(sv.token()), lineOf(sv)};
        };
        parser["Path"] = [](const peg::SemanticValues& sv) -> PathToks {
            PathToks out;
            out.reserve(sv.size());
            for (const std::any& v : sv) {
                if (const Tok* t = std::any_cast<Tok>(&v)) {
                    out.push_back(*t);
                }
            }
            return out;
        };
        parser["CallTail"] = [](const peg::SemanticValues& sv) -> CallTailV {
            CallTailV c;
            c.line = lineOf(sv);
            if (sv.empty()) {
                return c;   // '(' ')' sem argumentos — sem cauda útil
            }
            if (const Exprs* args = std::any_cast<Exprs>(&sv[0])) {
                c.hasArgs = true;
                c.args = *args;
            } else if (const StrV* s = std::any_cast<StrV>(&sv[0])) {
                c.hasStr = true;
                c.str = s->s;
            }
            return c;
        };
        // comando por caminho: move(1,2,3) · Import.Animation("n") …
        auto pathCallFrom = [&](const PathToks& toks, u32 line,
                                const CallTailV* tail) -> StmtP {
            StmtP s = Stmt::make(Stmt::Kind::PathCall, line);
            for (const Tok& t : toks) {
                s->path.push_back(t.text);
            }
            if (tail) {
                if (tail->hasArgs) {
                    s->hasArgs = true;
                    s->args = tail->args;
                } else if (tail->hasStr) {
                    // string solta → argumento literal único (View P "t")
                    s->hasArgs = true;
                    ExprP lit = Expr::make(Expr::Kind::Lit, tail->line);
                    lit->lit = Value::ofTxt(tail->str);
                    s->args.push_back(std::move(lit));
                }
            }
            return s;
        };
        parser["PathCall"] = [&](const peg::SemanticValues& sv) -> StmtP {
            // ViewStmt (forma espacial 'View P "texto"') já devolve StmtP
            // PRONTO — passthrough direto (o cast para PathToks daria null)
            if (const StmtP* ready = std::any_cast<StmtP>(&sv[0])) {
                return *ready;
            }
            // Path CallTail? → [PathToks, (CallTailV)?]
            const PathToks* p = std::any_cast<PathToks>(&sv[0]);
            const CallTailV* tail =
                sv.size() > 1 ? std::any_cast<CallTailV>(&sv[1]) : nullptr;
            return pathCallFrom(p ? *p : PathToks{}, lineOf(sv), tail);
        };
        parser["ViewStmt"] = [&](const peg::SemanticValues& sv) -> StmtP {
            // 'View' IdentAny CallTail? — os literais NÃO empurram valores:
            // sv = [Tok(sub), (CallTailV)?]; o "View" vem do literal.
            PathToks toks;
            toks.push_back(Tok{"View", lineOf(sv)});
            if (const Tok* sub = std::any_cast<Tok>(&sv[0])) {
                toks.push_back(*sub);
            }
            const CallTailV* tail =
                sv.size() > 1 ? std::any_cast<CallTailV>(&sv[1]) : nullptr;
            return pathCallFrom(toks, lineOf(sv), tail);
        };

        // exist/notexist + and( c(a) stopand ) (§7)
        parser["IfChain"] = [](const peg::SemanticValues& sv) -> StmtP {
            StmtP s = *std::any_cast<StmtP>(&sv[0]);
            if (sv.size() > 1) {
                if (const NotExistV* n = std::any_cast<NotExistV>(&sv[1])) {
                    s->otherwise = n->otherwise;
                    s->cases.reserve(n->cases.size());
                    for (const AndCaseV& c : n->cases) {
                        s->cases.push_back(c.ac);
                    }
                }
            }
            return s;
        };
        parser["ExistStmt"] = [](const peg::SemanticValues& sv) -> StmtP {
            StmtP s = Stmt::make(Stmt::Kind::IfChain, lineOf(sv));
            s->cond = *std::any_cast<ExprP>(&sv[0]);
            const BlockV* b = std::any_cast<BlockV>(&sv[1]);
            s->then = b ? b->block : Block{};
            return s;
        };
        parser["NotExistStmt"] = [](const peg::SemanticValues& sv) -> NotExistV {
            NotExistV n;
            const BlockV* b = std::any_cast<BlockV>(&sv[0]);
            n.otherwise = b ? b->block : Block{};
            if (sv.size() > 1) {
                if (const std::vector<AndCaseV>* cs =
                        std::any_cast<std::vector<AndCaseV>>(&sv[1])) {
                    n.hasChain = true;
                    n.cases = *cs;
                }
            }
            return n;
        };
        parser["AndChain"] = [](const peg::SemanticValues& sv)
            -> std::vector<AndCaseV> {
            std::vector<AndCaseV> out;
            out.reserve(sv.size());
            for (const std::any& v : sv) {
                if (const AndCaseV* c = std::any_cast<AndCaseV>(&v)) {
                    out.push_back(*c);
                }
            }
            return out;
        };
        parser["CaseItem"] = [](const peg::SemanticValues& sv) -> std::any {
            return sv[0];
        };
        // forma NUA do autor: cond(ação) — a condição é o identificador
        parser["BareCase"] = [](const peg::SemanticValues& sv) -> AndCaseV {
            AndCaseV v;
            v.ac.line = lineOf(sv);
            const Tok* name = std::any_cast<Tok>(&sv[0]);
            v.ac.bare = name ? name->text : "?";
            ExprP cond = Expr::make(Expr::Kind::Var, v.ac.line);
            cond->name = v.ac.bare;
            v.ac.cond = std::move(cond);
            for (size_t i = 1; i < sv.size(); ++i) {
                if (const StmtP* s = std::any_cast<StmtP>(&sv[i])) {
                    v.ac.action.push_back(*s);
                }
            }
            return v;
        };
        parser["ExprCase"] = [](const peg::SemanticValues& sv) -> AndCaseV {
            AndCaseV v;
            v.ac.line = lineOf(sv);
            v.ac.cond = *std::any_cast<ExprP>(&sv[0]);
            for (size_t i = 1; i < sv.size(); ++i) {
                if (const StmtP* s = std::any_cast<StmtP>(&sv[i])) {
                    v.ac.action.push_back(*s);
                }
            }
            return v;
        };

        // option (§7)
        parser["OptionStmt"] = [](const peg::SemanticValues& sv) -> StmtP {
            StmtP s = Stmt::make(Stmt::Kind::Option, lineOf(sv));
            s->sel = *std::any_cast<ExprP>(&sv[0]);
            for (size_t i = 1; i < sv.size(); ++i) {
                if (const OptCaseV* c = std::any_cast<OptCaseV>(&sv[i])) {
                    s->optCases.push_back(c->oc);
                } else if (const BlockV* d = std::any_cast<BlockV>(&sv[i])) {
                    s->hasDefault = true;
                    s->defaultBlock = d->block;
                }
            }
            return s;
        };
        parser["OptionCase"] = [](const peg::SemanticValues& sv) -> std::any {
            return sv[0];
        };
        parser["BareOpt"] = [](const peg::SemanticValues& sv) -> OptCaseV {
            // 'and' IdentAny '(' Statement* ')' 'stopand' → valor = Var
            OptCaseV v;
            v.oc.line = lineOf(sv);
            const Tok* name = std::any_cast<Tok>(&sv[0]);
            ExprP val = Expr::make(Expr::Kind::Var, v.oc.line);
            val->name = name ? name->text : "?";
            v.oc.value = std::move(val);
            for (size_t i = 1; i < sv.size(); ++i) {
                if (const StmtP* s = std::any_cast<StmtP>(&sv[i])) {
                    v.oc.action.push_back(*s);
                }
            }
            return v;
        };
        parser["ExprOpt"] = [](const peg::SemanticValues& sv) -> OptCaseV {
            OptCaseV v;
            v.oc.line = lineOf(sv);
            v.oc.value = *std::any_cast<ExprP>(&sv[0]);
            for (size_t i = 1; i < sv.size(); ++i) {
                if (const StmtP* s = std::any_cast<StmtP>(&sv[i])) {
                    v.oc.action.push_back(*s);
                }
            }
            return v;
        };
        parser["NotOption"] = [](const peg::SemanticValues& sv) -> BlockV {
            const BlockV* b = std::any_cast<BlockV>(&sv[0]);
            return b ? *b : BlockV{};
        };

        // loops (§6)
        parser["LoopStmt"] = [](const peg::SemanticValues& sv) -> std::any {
            return sv[0];
        };
        parser["RepeatStmt"] = [](const peg::SemanticValues& sv) -> StmtP {
            StmtP s = Stmt::make(Stmt::Kind::Repeat, lineOf(sv));
            s->count = *std::any_cast<ExprP>(&sv[0]);
            const BlockV* b = std::any_cast<BlockV>(&sv[1]);
            s->body = b ? b->block : Block{};
            return s;
        };
        parser["LastStmt"] = [](const peg::SemanticValues& sv) -> StmtP {
            // [cond, (CounterV)?, block]
            StmtP s = Stmt::make(Stmt::Kind::Last, lineOf(sv));
            s->whileCond = *std::any_cast<ExprP>(&sv[0]);
            size_t bi = 1;
            if (sv.size() > 2) {
                if (const CounterV* c = std::any_cast<CounterV>(&sv[1])) {
                    s->hasCounter = true;
                    s->counter = c->name;
                    bi = 2;
                }
            }
            const BlockV* b = std::any_cast<BlockV>(&sv[bi]);
            s->body = b ? b->block : Block{};
            return s;
        };
        parser["WithClause"] = [](const peg::SemanticValues& sv) -> CounterV {
            CounterV c;
            const Tok* t = std::any_cast<Tok>(&sv[0]);
            c.name = t ? t->text : "?";
            c.line = t ? t->line : lineOf(sv);
            return c;
        };
        parser["JumpStmt"] = [](const peg::SemanticValues& sv) -> StmtP {
            const std::string t(sv.token());
            return Stmt::make(t == "continue" ? Stmt::Kind::Continue
                                              : Stmt::Kind::Resume,
                              lineOf(sv));
        };
        parser["ReturnStmt"] = [](const peg::SemanticValues& sv) -> StmtP {
            StmtP s = Stmt::make(Stmt::Kind::Return, lineOf(sv));
            if (!sv.empty()) {
                if (const ExprP* e = std::any_cast<ExprP>(&sv[0])) {
                    s->hasValue = true;
                    s->init = *e;
                }
            }
            return s;
        };
    }

    // ---- 0.9.5: LINKERS & TYKERS -------------------------------------------
    void registerTykers() {
        // linker(A)to(B)=RF(nome) → [PathToks(A), PathToks(B), Tok(rf)]
        parser["LinkerDecl"] = [](const peg::SemanticValues& sv) -> LinkerDeclV {
            LinkerDeclV v;
            v.decl.line = lineOf(sv);
            if (const PathToks* a = std::any_cast<PathToks>(&sv[0])) {
                for (const Tok& t : *a) {
                    v.decl.origem.push_back(t.text);
                }
            }
            if (const PathToks* b = std::any_cast<PathToks>(&sv[1])) {
                for (const Tok& t : *b) {
                    v.decl.destino.push_back(t.text);
                }
            }
            if (const Tok* rf = std::any_cast<Tok>(&sv[2])) {
                v.decl.rf = rf->text;
                v.decl.line = rf->line;
            }
            return v;
        };

        // find(rf) → Tok
        parser["TykerFind"] = [](const peg::SemanticValues& sv) -> TykerFindV {
            TykerFindV f;
            const Tok* rf = std::any_cast<Tok>(&sv[0]);
            f.rf = rf ? rf->text : "?";
            f.line = rf ? rf->line : lineOf(sv);
            return f;
        };

        // Change(origem|destino)to(alvo) → [Tok(lado), PathToks(alvo)]
        parser["ChangeComp"] = [](const peg::SemanticValues& sv) -> TykerCompV {
            TykerCompV v;
            v.comp.name = "Change";
            v.comp.isChange = true;
            const Tok* side = std::any_cast<Tok>(&sv[0]);
            v.comp.changeDestino = side && side->text == "destino";
            v.comp.line = side ? side->line : lineOf(sv);
            if (const PathToks* p = std::any_cast<PathToks>(&sv[1])) {
                for (const Tok& t : *p) {
                    v.comp.changePath.push_back(t.text);
                }
            }
            return v;
        };
        parser["ChangeSide"] = [](const peg::SemanticValues& sv) -> Tok {
            return Tok{std::string(sv.token()), lineOf(sv)};
        };

        // caudas de argumentos: Expr OU cor #RRGGBB
        parser["ColorLit"] = [](const peg::SemanticValues& sv) -> CompArgV {
            CompArgV a;
            a.isColor = true;
            a.color = std::string(sv.token());
            return a;
        };
        parser["CompArg"] = [](const peg::SemanticValues& sv) -> CompArgV {
            if (const CompArgV* c = std::any_cast<CompArgV>(&sv[0])) {
                return *c;   // ColorLit
            }
            CompArgV a;
            a.expr = *std::any_cast<ExprP>(&sv[0]);
            return a;
        };
        parser["CompTail"] = [](const peg::SemanticValues& sv) -> CompTailV {
            CompTailV out;
            out.reserve(sv.size());
            for (const std::any& v : sv) {
                if (const CompArgV* a = std::any_cast<CompArgV>(&v)) {
                    out.push_back(*a);
                }
            }
            return out;
        };

        // componente genérico: [Tok(nome), CompTailV, (CompTailV)?]
        // (o REGISTO decide o que existe — aqui não há nomes hardcoded)
        parser["CompCall"] = [](const peg::SemanticValues& sv) -> TykerCompV {
            TykerCompV v;
            const Tok* n = std::any_cast<Tok>(&sv[0]);
            v.comp.name = n ? n->text : "?";
            v.comp.line = n ? n->line : lineOf(sv);
            if (const CompTailV* t1 = std::any_cast<CompTailV>(&sv[1])) {
                for (const CompArgV& a : *t1) {
                    if (a.isColor) {
                        // cor na 1ª cauda: vira literal Txt "#RRGGBB"
                        ExprP lit = Expr::make(Expr::Kind::Lit, v.comp.line);
                        lit->lit = Value::ofTxt(a.color);
                        v.comp.args.push_back(std::move(lit));
                    } else {
                        v.comp.args.push_back(a.expr);
                    }
                }
            }
            if (sv.size() > 2) {
                if (const CompTailV* t2 = std::any_cast<CompTailV>(&sv[2])) {
                    v.comp.hasTail2 = true;
                    if (!t2->empty()) {
                        const CompArgV& a = (*t2)[0];
                        if (a.isColor) {
                            v.comp.isColor2 = true;
                            v.comp.color2 = a.color;
                        } else {
                            v.comp.arg2 = a.expr;
                        }
                    }
                }
            }
            return v;
        };

        // um item do corpo: find / Change / componente / statement (o
        // statement é aceito AQUI para o VALIDADOR o rejeitar com erro
        // que ENSINA — linha + o que fazer — em vez de syntax error seco)
        parser["TykerItem"] = [](const peg::SemanticValues& sv) -> std::any {
            return sv[0];
        };

        // tyker(nome){ … } → [Tok(nome), items…]
        parser["TykerDef"] = [](const peg::SemanticValues& sv) -> TykerDefV {
            TykerDefV v;
            const Tok* n = std::any_cast<Tok>(&sv[0]);
            v.def.name = n ? n->text : "?";
            v.def.line = n ? n->line : lineOf(sv);
            bool sawComp = false;   // find já não pode vir depois de 1º item
            for (size_t i = 1; i < sv.size(); ++i) {
                if (const TykerFindV* f = std::any_cast<TykerFindV>(&sv[i])) {
                    if (!sawComp && !v.def.hasFind) {
                        v.def.hasFind = true;   // o 1º lugar
                        v.def.findRf = f->rf;
                        v.def.findLine = f->line;
                    } else {
                        v.def.sawLateFind = true;
                        v.def.lateFindLine = f->line;
                    }
                } else if (const TykerCompV* c =
                               std::any_cast<TykerCompV>(&sv[i])) {
                    sawComp = true;
                    v.def.comps.push_back(c->comp);
                } else if (const StmtP* s = std::any_cast<StmtP>(&sv[i])) {
                    sawComp = true;
                    if (!v.def.hasStray) {
                        v.def.hasStray = true;
                        v.def.strayLine = (*s)->line;
                        v.def.strayName = strayNameOf(**s);
                    }
                }
            }
            return v;
        };
    }

    // ---- estrutura --------------------------------------------------------
    void registerStructure() {
        parser["Item"] = [](const peg::SemanticValues& sv) -> std::any {
            return sv[0];
        };
        parser["Script"] = [](const peg::SemanticValues& sv) -> RawProgram {
            RawProgram p;
            for (const std::any& v : sv) {
                if (const StmtP* s = std::any_cast<StmtP>(&v)) {
                    p.top.push_back(*s);
                } else if (const FnDefV* f = std::any_cast<FnDefV>(&v)) {
                    p.fns.push_back(f->def);
                } else if (const CentralV* c =
                               std::any_cast<CentralV>(&v)) {
                    p.centrals.push_back(*c);
                } else if (const LinkerDeclV* l =
                               std::any_cast<LinkerDeclV>(&v)) {
                    p.linkers.push_back(l->decl);
                } else if (const TykerDefV* t =
                               std::any_cast<TykerDefV>(&v)) {
                    p.tykers.push_back(t->def);
                }
            }
            return p;
        };
        parser["CentralMain"] = [](const peg::SemanticValues& sv) -> CentralV {
            CentralV c;
            c.line = lineOf(sv);
            if (const std::vector<CmEventV>* evs =
                    std::any_cast<std::vector<CmEventV>>(&sv[0])) {
                c.events = *evs;
            }
            return c;
        };
        parser["CMBody"] = [](const peg::SemanticValues& sv)
            -> std::vector<CmEventV> {
            std::vector<CmEventV> out;
            for (const std::any& v : sv) {
                if (const CmEventV* e = std::any_cast<CmEventV>(&v)) {
                    out.push_back(*e);
                }
            }
            return out;
        };
        parser["CMItem"] = [](const peg::SemanticValues& sv) -> std::any {
            return sv[0];
        };
        parser["OnMoment"] = [](const peg::SemanticValues& sv) -> CmEventV {
            CmEventV e;
            e.isOn = true;
            e.line = lineOf(sv);
            const BlockV* b = std::any_cast<BlockV>(&sv[0]);
            e.body = b ? b->block : Block{};
            return e;
        };
        parser["AllMoments"] = [](const peg::SemanticValues& sv) -> CmEventV {
            CmEventV e;
            e.isOn = false;
            e.line = lineOf(sv);
            const BlockV* b = std::any_cast<BlockV>(&sv[0]);
            e.body = b ? b->block : Block{};
            return e;
        };

        parser["FnDef"] = [&](const peg::SemanticValues& sv) -> FnDefV {
            // [name, params, (RetType)?, block]
            FnDefV f;
            const Tok* name = std::any_cast<Tok>(&sv[0]);
            f.def.name = name ? name->text : "?";
            f.def.line = name ? name->line : lineOf(sv);
            if (const ParamV* ps = std::any_cast<ParamV>(&sv[1])) {
                f.def.params = ps->ps;
            }
            const BlockV* b = nullptr;
            if (sv.size() == 4) {
                const Tok* t = std::any_cast<Tok>(&sv[2]);
                if (t) {
                    Type tt;
                    if (typeFromName(t->text.c_str(), tt)) {
                        f.def.hasRet = true;
                        f.def.ret = tt;
                    } else {
                        typeErrs.emplace_back(
                            t->line, "tipo '" + t->text +
                                         "' não existe (tipos: Int Num Txt " +
                                         "Bool Vec2 Vec3 TIC)");
                    }
                }
                b = std::any_cast<BlockV>(&sv[3]);
            } else {
                b = std::any_cast<BlockV>(&sv[2]);
            }
            f.def.body = b ? b->block : Block{};
            return f;
        };
        parser["Params"] = [](const peg::SemanticValues& sv) -> ParamV {
            ParamV p;
            for (const std::any& v : sv) {
                if (const Param* pr = std::any_cast<Param>(&v)) {
                    p.ps.push_back(*pr);
                }
            }
            return p;
        };
        parser["Param"] = [&](const peg::SemanticValues& sv) -> Param {
            // IdentAny ':' IdentAny → [Tok(name), Tok(tipo)]
            Param p;
            const Tok* n = std::any_cast<Tok>(&sv[0]);
            p.name = n ? n->text : "?";
            p.line = n ? n->line : lineOf(sv);
            const Tok* t = std::any_cast<Tok>(&sv[1]);
            if (t) {
                Type tt;
                if (typeFromName(t->text.c_str(), tt)) {
                    p.type = tt;
                } else {
                    typeErrs.emplace_back(
                        t->line, "tipo '" + t->text +
                                     "' não existe (tipos: Int Num Txt Bool " +
                                     "Vec2 Vec3 TIC)");
                }
            }
            return p;
        };
    }
};

// ---------------------------------------------------------------------------
// VALIDAÇÃO (spec §§4-5 — com linhas)
// ---------------------------------------------------------------------------
struct Validator {
    Error err;
    std::vector<std::string> declared;

    bool fail(u32 line, const std::string& msg) {
        if (err.ok) {
            err = Error::fail(line, msg);
        }
        return false;
    }

    // nome de utilizador: MINÚSCULA (§5) + não reservada (§5)
    bool checkName(const std::string& name, u32 line) {
        if (name.empty()) {
            return fail(line, "nome vazio");
        }
        if (name[0] >= 'A' && name[0] <= 'Z') {
            return fail(line, "'" + name + "' começa por maiúscula — " +
                                   "maiúsculas são nomes da engine; nomes " +
                                   "de utilizador começam por minúscula");
        }
        if (isReservedWord(name)) {
            return fail(line, "'" + name + "' é uma palavra reservada");
        }
        return true;
    }

    void varDecl(const Stmt& s) {
        if (!checkName(s.varName, s.line)) {
            return;
        }
        for (const std::string& d : declared) {
            if (d == s.varName) {
                fail(s.line, "variável '" + s.varName + "' já foi declarada");
                return;
            }
        }
        declared.push_back(s.varName);
    }

    // ---- 0.9.5: LINKERS & TYKERS (validação com erros que ENSINAM) --------
    void validateLinkers(const std::vector<LinkerDecl>& ls) {
        for (const LinkerDecl& l : ls) {
            if (!err.ok) {
                return;
            }
            // o nome do RF é um nome de utilizador (minúscula, não reservada)
            checkName(l.rf, l.line);
        }
    }

    void validateTykers(const std::vector<TykerDef>& ts) {
        for (size_t i = 0; i < ts.size(); ++i) {
            const TykerDef& t = ts[i];
            if (!err.ok) {
                return;
            }
            if (!checkName(t.name, t.line)) {
                return;
            }
            for (size_t j = 0; j < i; ++j) {
                if (ts[j].name == t.name) {
                    fail(t.line, "tyker '" + t.name + "' já foi definido");
                    return;
                }
            }
            if (!t.hasFind) {
                fail(t.line, "o tyker '" + t.name +
                            "' precisa de find(RF) como 1º componente — ex.: "
                            "tyker(" + t.name + "){ find(nome) … }");
                return;
            }
            if (!checkName(t.findRf, t.findLine)) {
                return;
            }
            if (t.sawLateFind) {
                fail(t.lateFindLine,
                     "'find' só pode ser o 1º componente do tyker");
                return;
            }
            if (t.hasStray) {
                fail(t.strayLine,
                     "o corpo do tyker '" + t.name +
                         "' só aceita componentes — '" + t.strayName +
                         "' é código de script (pertence ao central main)");
                return;
            }
            for (const TykerComp& c : t.comps) {
                if (!err.ok) {
                    return;
                }
                checkComp(c);
            }
        }
    }

    // um componente contra o REGISTO CENTRAL (o parser é genérico — é AQUI
    // que se decide o que existe; a mensagem ENSINA com a sintaxe do registo)
    void checkComp(const TykerComp& c) {
        if (c.isChange) {
            if (c.changePath.empty()) {
                fail(c.line, "Change: falta o alvo — Change(origem)to(alvo)");
            }
            return;
        }
        if (c.name == "find") {
            fail(c.line, "'find' só pode ser o 1º componente do tyker");
            return;
        }
        const reg::Entry* e = reg::findComponent(c.name);
        if (!e) {
            if (c.name == "to") {
                fail(c.line,
                     "'to' vem sempre colado a um Change — "
                     "Change(origem)to(alvo)");
            } else if (c.name == "linker" || c.name == "tyker") {
                fail(c.line,
                     "'" + c.name + "' declara-se no TOPO do script "
                     "(fora do tyker)");
            } else {
                fail(c.line, "componente '" + c.name +
                                 "' não existe (a lista fechada está nas "
                                 "Docs)");
            }
            return;
        }
        if (c.args.size() < e->minArgs || c.args.size() > e->maxArgs) {
            fail(c.line, std::string(c.name) + ": escreve assim — " +
                             e->syntax);
            return;
        }
        if (c.hasTail2 && c.name != "colorpars") {
            fail(c.line, "só o colorpars tem 2ª cauda — '" + c.name +
                         "'(…)(…) não existe");
            return;
        }
        if (c.name == "colorpars" && !c.hasTail2) {
            fail(c.line, std::string("colorpars: escreve assim — ") +
                             e->syntax);
        }
    }

    void walkBlock(const Block& b) {
        for (const StmtP& s : b) {
            if (!err.ok) {
                return;
            }
            walkStmt(*s);
        }
    }

    void walkStmt(const Stmt& s) {
        switch (s.kind) {
            case Stmt::Kind::VarDecl:
                varDecl(s);
                break;
            case Stmt::Kind::IfChain:
                walkBlock(s.then);
                walkBlock(s.otherwise);
                for (const AndCase& c : s.cases) {
                    walkBlock(c.action);
                }
                break;
            case Stmt::Kind::Option:
                for (const OptCase& c : s.optCases) {
                    walkBlock(c.action);
                }
                walkBlock(s.defaultBlock);
                break;
            case Stmt::Kind::Repeat:
            case Stmt::Kind::Last:
                if (s.hasCounter) {
                    checkName(s.counter, s.line);
                }
                walkBlock(s.body);
                break;
            default:
                break;
        }
    }
};

} // namespace

// ---------------------------------------------------------------------------
// API Script::compile
// ---------------------------------------------------------------------------
Script Script::compile(const char* source, Error& err) {
    err = Error::fine();
    Script sc;
    if (!source) {
        err = Error::fail(0, "fonte nula");
        return sc;
    }

    Compiler cc;
    if (!cc.ok()) {
        err = Error::fail(0, "gramática V.ONI inválida (erro interno)");
        return sc;
    }

    // 1× parse: valor + Result com error_info (linha contada do offset)
    RawProgram out;
    const size_t n = std::strlen(source);
    peg::Definition::Result r =
        cc.parser["Script"].parse_and_get_value(source, n, out);
    if (!r.ret) {
        u32 line = 1;
        if (r.error_info.error_pos) {
            for (const char* p = source;
                 p < r.error_info.error_pos && (size_t)(p - source) < n;
                 ++p) {
                if (*p == '\n') {
                    ++line;
                }
            }
        }
        // 0.9.5 · ERROS-QUE-ENSINAM (METADE 2): quem sabe Python/JS escreve
        // 'if'/'while'/'break' — varre-se a LINHA DO ERRO por palavras
        // estrangeiras (a tabela vive no REGISTO — kForeign) e a mensagem
        // ENSINA o equivalente V.ONI em vez de "expressão inesperada" seco.
        // (A palavra no error_pos pode ter ficado ATRÁS do ponto fatal — o
        // 'if (x) { }' morre no '{' — por isso a LINHA inteira.)
        std::string teach;
        {
            u32 li = 1;
            const char* ls = source;
            const char* le = source;
            while (li < line && le < source + n) {
                if (*le == '\n') {
                    ++li;
                    ls = le + 1;
                }
                ++le;
            }
            while (le < source + n && *le != '\n') {
                ++le;
            }
            const char* p = ls;
            while (p < le && teach.empty()) {
                if (std::isalpha(static_cast<unsigned char>(*p)) ||
                    *p == '_') {
                    const char* e2 = p + 1;
                    while (e2 < le &&
                           (std::isalnum(static_cast<unsigned char>(*e2)) ||
                            *e2 == '_')) {
                        ++e2;
                    }
                    const std::string word(p, static_cast<size_t>(e2 - p));
                    if (const char* t = reg::foreignTeach(word, nullptr)) {
                        teach = t;
                    }
                    p = e2;
                } else {
                    ++p;
                }
            }
        }
        if (!teach.empty()) {
            err = Error::fail(line, teach);
        } else {
            err = Error::fail(line, "erro de sintaxe — expressão inesperada");
        }
        return sc;
    }

    // erros de tipo apanhados nas ações (a ação tem o nome escrito)
    if (!cc.typeErrs.empty()) {
        err = Error::fail(cc.typeErrs[0].first, cc.typeErrs[0].second);
        return sc;
    }

    // ---- validação §§4-5 --------------------------------------------------
    Validator v;
    for (const FnDef& f : out.fns) {
        if (!v.checkName(f.name, f.line)) {
            break;
        }
        for (const Param& p : f.params) {
            if (!v.checkName(p.name, p.line)) {
                break;
            }
        }
    }
    for (size_t i = 0; v.err.ok && i < out.fns.size(); ++i) {
        for (size_t j = i + 1; j < out.fns.size(); ++j) {
            if (out.fns[i].name == out.fns[j].name) {
                v.fail(out.fns[j].line,
                       "fn '" + out.fns[j].name + "' já foi definida");
            }
        }
    }
    if (out.centrals.size() > 1) {
        v.fail(out.centrals[1].line, "central main já foi definido");
    }
    // 0.9.5: linkers e tykers (nomes, find-primeiro, componentes contra o
    // REGISTO — erros com linha que ENSINAM)
    if (v.err.ok) {
        v.validateLinkers(out.linkers);
    }
    if (v.err.ok) {
        v.validateTykers(out.tykers);
    }
    if (v.err.ok && !out.centrals.empty()) {
        const CentralV& c = out.centrals[0];
        bool sawOn = false, sawAll = false;
        for (const CmEventV& e : c.events) {
            if (e.isOn) {
                if (sawOn) {
                    v.fail(e.line, "'on moment' duplicado no central main");
                }
                sawOn = true;
            } else {
                if (sawAll) {
                    v.fail(e.line, "'allmoments' duplicado no central main");
                }
                sawAll = true;
            }
        }
    }
    if (v.err.ok) {
        v.walkBlock(out.top);
    }
    for (const FnDef& f : out.fns) {
        if (!v.err.ok) {
            break;
        }
        v.walkBlock(f.body);
    }
    if (v.err.ok && !out.centrals.empty()) {
        for (const CmEventV& e : out.centrals[0].events) {
            if (!v.err.ok) {
                break;
            }
            v.walkBlock(e.body);
        }
    }
    if (!v.err.ok) {
        err = v.err;
        return sc;
    }

    // ---- montar o Program limpo ------------------------------------------
    sc.impl().program = std::make_shared<Program>();
    Program& p = *sc.impl().program;
    p.top = std::move(out.top);
    p.fns = std::move(out.fns);
    p.linkers = std::move(out.linkers);   // 0.9.5
    p.tykers = std::move(out.tykers);     // 0.9.5
    if (!out.centrals.empty()) {
        p.hasCentral = true;
        p.centralLine = out.centrals[0].line;
        for (const CmEventV& e : out.centrals[0].events) {
            if (e.isOn) {
                p.central.hasOn = true;
                p.central.onLine = e.line;
                p.central.onMoment = e.body;
            } else {
                p.central.hasAll = true;
                p.central.allLine = e.line;
                p.central.allMoments = e.body;
            }
        }
    }
    return sc;
}

} // namespace voni
