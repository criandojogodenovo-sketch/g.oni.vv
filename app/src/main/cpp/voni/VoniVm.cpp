// voni/VoniVm.cpp — o INTÉRPRETE V.ONI (0.9.2 §§3-12) + utilitários de Value
// + a API da classe Script.
//
// MODELO (spec §3): a "central" (VoniSystem na engine) agenda
//   top-level 1×  →  on moment 1×  →  allmoments a cada frame
// O runStart/runFrame aqui implementam exatamente essa ordem.
//
// SANDBOX (spec §12):
//   • ESTRUTURAL: este ficheiro não inclui NADA da engine (só Voni.h +
//     Host) — não há ficheiros/rede/sistema para chamar. O isolamento é
//     do tipo "não existe a função", não "a função está bloqueada".
//   • BUDGET: kDefaultTickBudget instruções por tick; exceder aborta com
//     "budget de iterações excedido" (loop infinito legível, nunca hang).
//   • PROFUNDIDADE: 256 chamadas de fn → abort legível (nunca stack
//     overflow, nunca crash).
//   • Todos os erros carregam LINHA e sobem como Error — NUNCA exceção
//     C++ para fora da API (a única exceção interna é o VmFail apanhado
//     no boundary).
//
// DECISÕES 🔶 (isoladas aqui, documentadas no relatório):
//   • Int/Int → Int (divisão inteira); qualquer lado Num → Num
//   • `+` com Txt+Txt concatena
//   • Num aceita Int por ALARGAMENTO (coerção segura); Int NÃO aceita Num
//   • Deltatime.Increment: alvo Int soma truncado, alvo Num soma exato
//   • VarDecl re-executada RE-INICIALIZA (a validação de compile já impede
//     2 declarações do mesmo nome; 1 declaração num bloco por-frame só
//     repõe o valor)
//   • contadores `with` começam a 0 e incrementam no FIM da iteração
//     (continue incluído); params de fn fazem shadow às globais
#include "voni/Voni.h"
#include "voni/VoniAst.h"
#include "voni/VoniInternal.h"
#include "voni/VoniRegistry.h"
#include "voni/VoniTykers.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace voni {

// ---------------------------------------------------------------------------
// Value/Type utils
// ---------------------------------------------------------------------------
std::string typeName(Type t) {
    switch (t) {
        case Type::Int:  return "Int";
        case Type::Num:  return "Num";
        case Type::Txt:  return "Txt";
        case Type::Bool: return "Bool";
        case Type::Vec2: return "Vec2";
        case Type::Vec3: return "Vec3";
        case Type::Tic:  return "TIC";
        default:         return "?";
    }
}

bool typeFromName(const char* s, Type& out) {
    if (!s) {
        return false;
    }
    struct Entry { const char* n; Type t; };
    static const Entry kMap[] = {
        {"Int", Type::Int}, {"Num", Type::Num},   {"Txt", Type::Txt},
        {"Bool", Type::Bool}, {"Vec2", Type::Vec2}, {"Vec3", Type::Vec3},
        {"TIC", Type::Tic},
    };
    for (const Entry& e : kMap) {
        if (std::strcmp(s, e.n) == 0) {
            out = e.t;
            return true;
        }
    }
    return false;
}

bool Value::vecComponent(u32 idx, f32& out) const {
    if (t == Type::Vec2 && idx < 2) {
        out = v2[idx];
        return true;
    }
    if (t == Type::Vec3 && idx < 3) {
        out = v3[idx];
        return true;
    }
    return false;
}

bool Value::setVecComponent(u32 idx, f32 v) {
    if (t == Type::Vec2 && idx < 2) {
        v2[idx] = v;
        return true;
    }
    if (t == Type::Vec3 && idx < 3) {
        v3[idx] = v;
        return true;
    }
    return false;
}

std::string valueText(const Value& v) {
    char buf[64];
    switch (v.t) {
        case Type::Int:
            std::snprintf(buf, sizeof(buf), "%lld", (long long)v.i);
            return buf;
        case Type::Num:
            std::snprintf(buf, sizeof(buf), "%g", v.n);
            return buf;
        case Type::Txt:
            return v.s;
        case Type::Bool:
            return v.b ? "true" : "false";
        case Type::Vec2:
            std::snprintf(buf, sizeof(buf), "(%g, %g)", v.v2[0], v.v2[1]);
            return buf;
        case Type::Vec3:
            std::snprintf(buf, sizeof(buf), "(%g, %g, %g)", v.v3[0], v.v3[1],
                          v.v3[2]);
            return buf;
        case Type::Tic:
            return v.tic;
        default:
            return "?";
    }
}

// ---------------------------------------------------------------------------
// Script: ctor/dtor/move
// ---------------------------------------------------------------------------
Script::Script() : impl_(new Impl) {}
Script::~Script() { delete impl_; }
Script::Script(Script&& o) noexcept : impl_(o.impl_) { o.impl_ = nullptr; }
Script& Script::operator=(Script&& o) noexcept {
    if (this != &o) {
        delete impl_;
        impl_ = o.impl_;
        o.impl_ = nullptr;
    }
    return *this;
}

// ---------------------------------------------------------------------------
// a VM (interna)
// ---------------------------------------------------------------------------
namespace {

// exceção leve de controlo — NUNCA sai da API (apanhada no boundary)
struct VmFail {};

enum class Flow { None, Continue, Resume, Return };

struct Frame {
    std::unordered_map<std::string, Value> locals;
};

struct Vm {
    Host&           host;
    Script::Impl&   st;
    Error           err;
    u64             budget = Script::kDefaultTickBudget;
    u32             depth = 0;
    f64             dt = 0.0;

    Flow            flow = Flow::None;
    Value           retVal;
    u32             flowLine = 0;      // linha do continue/resume/return
    std::vector<Frame> frames;         // params de fn
    // `with n+=1`: se existir uma GLOBAL Int com o nome, o contador é
    // ALIAS dela (o loop incrementa a variável — o valor fica depois do
    // loop, "conta iterações" §6); senão é local ao loop (nasce 0)
    struct Counter {
        std::string name;
        i64         value = 0;
        Value*      alias = nullptr;   // ponteiro estável (unordered_map)
    };
    std::vector<Counter> counters;

    Vm(Host& h, Script::Impl& s) : host(h), st(s) {}

    [[noreturn]] void fail(u32 line, const std::string& msg) {
        if (err.ok) {
            err = Error::fail(line, msg);
        }
        throw VmFail{};
    }

    void spend(u32 line) {
        if (budget == 0) {
            fail(line, std::string("budget de iterações excedido — loop ") +
                           "infinito? (limite por frame: " +
                           std::to_string(Script::kDefaultTickBudget / 1000) +
                           "k instruções)");
        }
        --budget;
    }

    // ---- resolução de nomes ----------------------------------------------
    // (contadores `with` tratados à parte — counterValue/isCounter; os
    // frames de fn e as globais vivem em findVar)
    i64 counterValue(const std::string& name) {
        for (auto it = counters.rbegin(); it != counters.rend(); ++it) {
            if (it->name == name) {
                return it->alias ? it->alias->i : it->value;
            }
        }
        return 0;
    }

    bool isCounter(const std::string& name) const {
        for (const auto& c : counters) {
            if (c.name == name) {
                return true;
            }
        }
        return false;
    }

    Value* findVar(const std::string& name) {
        for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
            auto f = it->locals.find(name);
            if (f != it->locals.end()) {
                return &f->second;
            }
        }
        auto g = st.globals.find(name);
        if (g != st.globals.end()) {
            return &g->second;
        }
        return nullptr;
    }

    const Program& program() const { return *st.program; }

    // ---- tipos ------------------------------------------------------------    
    bool coerceInto(Type declared, Value& v, u32 line, const char* what) {
        if (declared == Type::None) {
            return true;   // inferido
        }
        if (declared == Type::Num && v.t == Type::Int) {
            v = Value::ofNum((f64)v.i);   // alargamento seguro Int→Num
            return true;
        }
        if (v.t == declared) {
            return true;
        }
        fail(line, std::string(what) + ": espera " + typeName(declared) +
                       " mas veio " + typeName(v.t));
    }

    // ---- expressões ---------------------------------------------------------
    Value eval(const Expr& e) {
        spend(e.line);
        switch (e.kind) {
            case Expr::Kind::Lit:
                return e.lit;

            case Expr::Kind::Var:
                return evalVar(e);

            case Expr::Kind::Path:
                return evalPath(e);

            case Expr::Kind::Call:
                return evalCall(e);

            case Expr::Kind::Unary:
                return evalUnary(e);

            case Expr::Kind::Binary:
                return evalBinary(e);
        }
        fail(e.line, "expressão desconhecida (erro interno)");
    }

    // resolução de um NOME nu (Var ou Path de 1 segmento — o PathExpr põe
    // o nome em segs[0], não em e.name): contador → TIC → erro
    Value evalVarByName(const std::string& name, u32 line) {
        if (isCounter(name)) {
            return Value::ofInt(counterValue(name));
        }
        if (Value* v = findVar(name)) {
            return *v;
        }
        if (host.ticExists(name)) {
            return Value::ofTic(name);
        }
        fail(line, std::string("'") + name + "' não existe (variável, função ou TIC?)");
    }

    Value evalVar(const Expr& e) { return evalVarByName(e.name, e.line); }

    Value evalPath(const Expr& e) {
        const std::vector<std::string>& segs = e.segs;
        if (segs.empty()) {
            fail(e.line, "caminho vazio (erro interno)");
        }
        // Search.alvo.propriedade (§9 — RTTI)
        if (segs[0] == "Search") {
            if (segs.size() < 3) {
                fail(e.line, std::string("Search precisa de alvo e propriedade — ") +
                                 "Search.alvo.propriedade");
            }
            std::vector<std::string> chain(segs.begin() + 2, segs.end());
            Value out;
            std::string herr;
            if (!host.search(segs[1], chain, out, herr)) {
                fail(e.line, herr.empty() ? "Search: alvo não encontrado"
                                          : herr);
            }
            return out;
        }
        if (segs.size() == 1) {
            return evalVarByName(segs[0], e.line);   // identificador nu
        }
        // base = variável (deve ser TIC ou vetor) …
        if (!isCounter(segs[0])) {
            if (Value* v = findVar(segs[0])) {
                std::vector<std::string> rest(segs.begin() + 1, segs.end());
                return valuePath(*v, rest, e.line, segs[0]);
            }
        }
        // … senão é um TIC da cena (§9: jogador.pos.x)
        Value out;
        std::string herr;
        std::vector<std::string> chain(segs.begin() + 1, segs.end());
        if (!host.getProp(segs[0], chain, out, herr)) {
            fail(e.line, herr.empty() ? "'" + segs[0] + "' não existe"
                                      : herr);
        }
        return out;
    }

    // caminho sobre um VALOR (TIC → propriedades; Vec → componentes)
    Value valuePath(const Value& base, const std::vector<std::string>& rest,
                    u32 line, const std::string& displayName) {
        if (rest.empty()) {
            return base;
        }
        Value cur = base;
        for (const std::string& seg : rest) {
            if (cur.t == Type::Tic) {
                Value out;
                std::string herr;
                std::vector<std::string> one{seg};
                if (!host.getProp(cur.tic, one, out, herr)) {
                    fail(line, herr.empty() ? "propriedade '" + seg +
                                                   "' não existe"
                                             : herr);
                }
                cur = out;
                continue;
            }
            if (cur.t == Type::Vec2 || cur.t == Type::Vec3) {
                u32 idx = 9;
                if      (seg == "x") idx = 0;
                else if (seg == "y") idx = 1;
                else if (seg == "z") idx = 2;
                f32 c = 0;
                if (idx != 9 && cur.vecComponent(idx, c)) {
                    cur = Value::ofNum((f64)c);
                    continue;
                }
                fail(line, std::string("'") + displayName + "' é " + typeName(cur.t) +
                               " — componente '" + seg + "' não existe");
            }
            fail(line, std::string("'") + displayName + "' é " + typeName(cur.t) +
                           " — não tem propriedade '" + seg + "'");
        }
        return cur;
    }

    Value evalCall(const Expr& e) {
        const FnDef* fn = findFn(e.name);
        if (!fn) {
            fail(e.line, std::string("'") + e.name + "' não é uma função");
        }
        std::vector<Value> args;
        args.reserve(e.args.size());
        for (const ExprP& a : e.args) {
            args.push_back(eval(*a));
        }
        Value out;
        callFn(*fn, args, e.line, out);
        return out;
    }

    const FnDef* findFn(const std::string& name) const {
        for (const FnDef& f : program().fns) {
            if (f.name == name) {
                return &f;
            }
        }
        return nullptr;
    }

    void callFn(const FnDef& fn, const std::vector<Value>& args, u32 line,
                Value& out) {
        if (args.size() != fn.params.size()) {
            fail(line, std::string("fn '") + fn.name + "' espera " +
                           std::to_string(fn.params.size()) +
                           " argumento(s), recebeu " +
                           std::to_string(args.size()));
        }
        if (depth + 1 > Script::kMaxCallDepth) {
            fail(line, std::string("profundidade de chamadas excedida (") +
                           std::to_string(Script::kMaxCallDepth) +
                           ") — recursão infinita?");
        }
        Frame fr;
        for (size_t i = 0; i < args.size(); ++i) {
            Value a = args[i];
            coerceInto(fn.params[i].type, a, line,
                       ("argumento '" + fn.params[i].name + "'").c_str());
            fr.locals[fn.params[i].name] = a;
        }
        frames.push_back(std::move(fr));
        ++depth;
        Flow f = Flow::None;
        bool ok = true;
        try {
            f = execBlock(fn.body);   // o Flow vem no RETORNO (não em membro)
        } catch (const VmFail&) {
            ok = false;
        }
        --depth;
        frames.pop_back();
        if (!ok) {
            throw VmFail{};
        }
        if (f == Flow::Return) {
            Value rv = retVal;
            if (fn.hasRet) {
                coerceInto(fn.ret, rv, flowLine,
                           ("retorno de '" + fn.name + "'").c_str());
                out = rv;
            } else if (rv.t != Type::None) {
                fail(flowLine, std::string("fn '") + fn.name +
                                   "' não declara retorno mas devolveu " +
                                   typeName(rv.t));
            } else {
                out = Value{};
            }
        } else {
            // corpo acabou sem return
            if (fn.hasRet) {
                fail(fn.line, std::string("fn '") + fn.name +
                                  "' declara retorno :" + typeName(fn.ret) +
                                  " mas acabou sem return");
            }
            out = Value{};
        }
    }

    Value evalUnary(const Expr& e) {
        Value a = eval(*e.a);
        if (e.un == Expr::Un::Neg) {
            if (a.t == Type::Int) {
                return Value::ofInt(-a.i);
            }
            if (a.t == Type::Num) {
                return Value::ofNum(-a.n);
            }
            fail(e.line, std::string("não sei negar ") + typeName(a.t) +
                             " (Int ou Num)");
        }
        // not
        if (a.t != Type::Bool) {
            fail(e.line, std::string("not precisa de true/false (veio ") +
                             typeName(a.t) + ")");
        }
        return Value::ofBool(!a.b);
    }

    Value evalBinary(const Expr& e) {
        // curto-circuito dos lógicos
        if (e.bin == Expr::Bin::And) {
            Value a = eval(*e.a);
            if (a.t != Type::Bool) {
                fail(e.line, std::string("and precisa de true/false (veio ") +
                                 typeName(a.t) + ")");
            }
            if (!a.b) {
                return Value::ofBool(false);
            }
            Value b = eval(*e.b);
            if (b.t != Type::Bool) {
                fail(e.line, std::string("and precisa de true/false (veio ") +
                                 typeName(b.t) + ")");
            }
            return Value::ofBool(b.b);
        }
        if (e.bin == Expr::Bin::Or) {
            Value a = eval(*e.a);
            if (a.t != Type::Bool) {
                fail(e.line, std::string("or precisa de true/false (veio ") +
                                 typeName(a.t) + ")");
            }
            if (a.b) {
                return Value::ofBool(true);
            }
            Value b = eval(*e.b);
            if (b.t != Type::Bool) {
                fail(e.line, std::string("or precisa de true/false (veio ") +
                                 typeName(b.t) + ")");
            }
            return Value::ofBool(b.b);
        }

        Value a = eval(*e.a);
        Value b = eval(*e.b);
        const bool numA = a.t == Type::Int || a.t == Type::Num;
        const bool numB = b.t == Type::Int || b.t == Type::Num;

        switch (e.bin) {
            case Expr::Bin::Add:
                if (a.t == Type::Txt && b.t == Type::Txt) {
                    return Value::ofTxt(a.s + b.s);
                }
                break;
            case Expr::Bin::Sub:
            case Expr::Bin::Mul:
            case Expr::Bin::Div:
                break;
            case Expr::Bin::Eq:
                return Value::ofBool(valuesEqual(a, b));
            case Expr::Bin::Ne:
                return Value::ofBool(!valuesEqual(a, b));
            case Expr::Bin::Lt:
            case Expr::Bin::Gt:
            case Expr::Bin::Le:
            case Expr::Bin::Ge: {
                if (!numA || !numB) {
                    fail(e.line, std::string("comparação < > precisa de números (veio ") +
                                     typeName(a.t) + " e " + typeName(b.t) +
                                     ")");
                }
                const f64 x = a.t == Type::Int ? (f64)a.i : a.n;
                const f64 y = b.t == Type::Int ? (f64)b.i : b.n;
                switch (e.bin) {
                    case Expr::Bin::Lt: return Value::ofBool(x < y);
                    case Expr::Bin::Gt: return Value::ofBool(x > y);
                    case Expr::Bin::Le: return Value::ofBool(x <= y);
                    default:            return Value::ofBool(x >= y);
                }
            }
            default:
                break;
        }

        // aritmética
        if (!numA || !numB) {
            fail(e.line, std::string("aritmética precisa de números (veio ") +
                             typeName(a.t) + " e " + typeName(b.t) + ")");
        }
        const bool bothInt = a.t == Type::Int && b.t == Type::Int;
        if (bothInt) {
            const i64 x = a.i, y = b.i;
            switch (e.bin) {
                case Expr::Bin::Add: return Value::ofInt(x + y);
                case Expr::Bin::Sub: return Value::ofInt(x - y);
                case Expr::Bin::Mul: return Value::ofInt(x * y);
                default:
                    if (y == 0) {
                        fail(e.line, "divisão por zero");
                    }
                    return Value::ofInt(x / y);   // 🔶 divisão inteira
            }
        }
        const f64 x = a.t == Type::Int ? (f64)a.i : a.n;
        const f64 y = b.t == Type::Int ? (f64)b.i : b.n;
        switch (e.bin) {
            case Expr::Bin::Add: return Value::ofNum(x + y);
            case Expr::Bin::Sub: return Value::ofNum(x - y);
            case Expr::Bin::Mul: return Value::ofNum(x * y);
            default:
                if (y == 0.0) {
                    fail(e.line, "divisão por zero");
                }
                return Value::ofNum(x / y);
        }
    }

    // == com tipos compatíveis (Int==Num ok; tipos diferentes = false —
    // usado pelo option; o operador == usa a mesma)
    static bool valuesEqual(const Value& a, const Value& b) {
        const bool numA = a.t == Type::Int || a.t == Type::Num;
        const bool numB = b.t == Type::Int || b.t == Type::Num;
        if (numA && numB) {
            const f64 x = a.t == Type::Int ? (f64)a.i : a.n;
            const f64 y = b.t == Type::Int ? (f64)b.i : b.n;
            return x == y;
        }
        if (a.t != b.t) {
            return false;
        }
        switch (a.t) {
            case Type::Txt:  return a.s == b.s;
            case Type::Bool: return a.b == b.b;
            case Type::Vec2:
                return a.v2[0] == b.v2[0] && a.v2[1] == b.v2[1];
            case Type::Vec3:
                return a.v3[0] == b.v3[0] && a.v3[1] == b.v3[1] &&
                       a.v3[2] == b.v3[2];
            case Type::Tic:  return a.tic == b.tic;
            default:         return false;
        }
    }

    // ---- statements ---------------------------------------------------------
    Flow exec(const Stmt& s) {
        spend(s.line);
        switch (s.kind) {
            case Stmt::Kind::VarDecl: {
                Value v = eval(*s.init);
                if (s.varType != Type::None) {
                    coerceInto(s.varType, v, s.line,
                               ("variável '" + s.varName + "'").c_str());
                }
                if (s.exported &&
                    std::find(st.exportOrder.begin(), st.exportOrder.end(),
                              s.varName) == st.exportOrder.end()) {
                    st.exportOrder.push_back(s.varName);
                }
                st.globals[s.varName] = v;
                return Flow::None;
            }

            case Stmt::Kind::Assign:
                execAssign(s);
                return Flow::None;

            case Stmt::Kind::IfChain: {
                Value c = eval(*s.cond);
                if (c.t != Type::Bool) {
                    fail(s.line, std::string("exist() precisa de true/false (veio ") +
                                     typeName(c.t) + ")");
                }
                if (c.b) {
                    return execBlock(s.then);   // salta tudo (§7)
                }
                for (const AndCase& ac : s.cases) {
                    Value cc = eval(*ac.cond);
                    if (cc.t != Type::Bool) {
                        fail(ac.line, std::string("condição do and() precisa de ") +
                                          "true/false");
                    }
                    if (cc.b) {
                        // 1º verdadeiro corre e PARA (salta o default §7)
                        return execBlock(ac.action);
                    }
                }
                return execBlock(s.otherwise);   // default
            }

            case Stmt::Kind::Option: {
                Value sel = eval(*s.sel);
                for (const OptCase& oc : s.optCases) {
                    Value v = eval(*oc.value);
                    if (valuesEqual(sel, v)) {
                        return execBlock(oc.action);
                    }
                }
                if (s.hasDefault) {
                    return execBlock(s.defaultBlock);
                }
                return Flow::None;
            }

            case Stmt::Kind::Repeat: {
                Value n = eval(*s.count);
                i64 count = 0;
                if (n.t == Type::Int) {
                    count = n.i;
                } else if (n.t == Type::Num && n.n == std::floor(n.n)) {
                    count = (i64)n.n;
                } else {
                    fail(s.line, std::string("repeat() precisa de um número inteiro ") +
                                     "(veio " + valueText(n) + ")");
                }
                if (count < 0) {
                    fail(s.line, "repeat() com número negativo");
                }
                for (i64 i = 0; i < count; ++i) {
                    spend(s.line);
                    Flow f = execBlock(s.body);
                    if (f == Flow::Resume) {
                        break;
                    }
                    if (f == Flow::Return) {
                        return f;
                    }
                }
                return Flow::None;
            }

            case Stmt::Kind::Last: {
                Value* counterAlias = nullptr;
                if (s.hasCounter) {
                    if (Value* g = findVar(s.counter)) {
                        if (g->t != Type::Int) {
                            fail(s.line, std::string("contador '") +
                                             s.counter +
                                             "' tem de ser Int (veio " +
                                             typeName(g->t) + ")");
                        }
                        counterAlias = g;   // ALIAS: conta NA variável
                    }
                }
                counters.push_back(
                    Counter{s.hasCounter ? s.counter : std::string(), 0,
                            counterAlias});
                bool exited = false;
                while (true) {
                    spend(s.line);
                    Value c = eval(*s.whileCond);
                    if (c.t != Type::Bool) {
                        const u32 line = s.line;
                        counters.pop_back();
                        fail(line, "last() precisa de true/false na condição");
                    }
                    if (!c.b) {
                        break;
                    }
                    Flow f = execBlock(s.body);
                    if (f == Flow::Resume) {
                        exited = true;
                        break;
                    }
                    if (f == Flow::Return) {
                        counters.pop_back();
                        return f;
                    }
                    // passo do contador (continue INCLUIDO — a iteração
                    // contou; decisão 🔶 documentada)
                    if (s.hasCounter && !counters.empty()) {
                        if (counters.back().alias) {
                            counters.back().alias->i += 1;
                        } else {
                            counters.back().value += 1;
                        }
                    }
                }
                (void)exited;
                counters.pop_back();
                return Flow::None;
            }

            case Stmt::Kind::Continue:
                flowLine = s.line;
                return Flow::Continue;

            case Stmt::Kind::Resume:
                flowLine = s.line;
                return Flow::Resume;

            case Stmt::Kind::Return:
                flowLine = s.line;
                if (s.hasValue) {
                    retVal = eval(*s.init);
                } else {
                    retVal = Value{};
                }
                return Flow::Return;

            case Stmt::Kind::PathCall:
                runCommand(s);
                return Flow::None;
        }
        fail(s.line, "instrução desconhecida (erro interno)");
    }

    void execAssign(const Stmt& s) {
        Value v = eval(*s.init);
        const std::vector<std::string>& p = s.path;
        if (p.empty()) {
            fail(s.line, "atribuição sem destino (erro interno)");
        }
        if (p.size() == 1) {
            if (isCounter(p[0])) {
                fail(s.line, std::string("'") + p[0] +
                                 "' é o contador do loop (não se atribui)");
            }
            if (Value* slot = findVar(p[0])) {
                if (s.add) {
                    v = addValues(*slot, v, s.line);
                }
                coerceInto(slot->t == Type::None ? v.t : slot->t, v, s.line,
                           ("variável '" + p[0] + "'").c_str());
                *slot = v;
                return;
            }
            fail(s.line, std::string("variável '") + p[0] + "' não existe");
        }
        // caminho: variável TIC ou nome de TIC
        Value* base = findVar(p[0]);
        if (base) {
            if (base->t != Type::Tic) {
                // vetor? jogador… não: p[0] é VARIÁVEL; só TIC tem props
                fail(s.line, std::string("'") + p[0] + "' é " + typeName(base->t) +
                                 " — não tem propriedades");
            }
            std::vector<std::string> chain(p.begin() + 1, p.end());
            if (s.add) {
                Value cur;
                std::string herr;
                if (!host.getProp(base->tic, chain, cur, herr)) {
                    fail(s.line, herr);
                }
                v = addValues(cur, v, s.line);
            }
            std::string herr;
            if (!host.setProp(base->tic, chain, v, herr)) {
                fail(s.line, herr);
            }
            return;
        }
        std::vector<std::string> chain(p.begin() + 1, p.end());
        if (s.add) {
            Value cur;
            std::string herr;
            if (!host.getProp(p[0], chain, cur, herr)) {
                fail(s.line, herr);
            }
            v = addValues(cur, v, s.line);
        }
        std::string herr;
        if (!host.setProp(p[0], chain, v, herr)) {
            fail(s.line, herr);
        }
    }

    Value addValues(const Value& a, const Value& b, u32 line) {
        if (a.t == Type::Int && b.t == Type::Int) {
            return Value::ofInt(a.i + b.i);
        }
        const bool numA = a.t == Type::Int || a.t == Type::Num;
        const bool numB = b.t == Type::Int || b.t == Type::Num;
        if (numA && numB) {
            const f64 x = a.t == Type::Int ? (f64)a.i : a.n;
            const f64 y = b.t == Type::Int ? (f64)b.i : b.n;
            if (a.t == Type::Int && b.t == Type::Int) {
                return Value::ofInt((i64)(x + y));
            }
            return Value::ofNum(x + y);
        }
        if (a.t == Type::Txt && b.t == Type::Txt) {
            return Value::ofTxt(a.s + b.s);
        }
        if ((a.t == Type::Vec3 || a.t == Type::Vec2) && a.t == b.t) {
            Value r = a;
            const u32 n = a.t == Type::Vec2 ? 2 : 3;
            for (u32 i = 0; i < n; ++i) {
                f32 av = 0, bv = 0;
                a.vecComponent(i, av);
                b.vecComponent(i, bv);
                r.setVecComponent(i, av + bv);
            }
            return r;
        }
        fail(line, std::string("não sei somar ") + typeName(a.t) + " com " +
                       typeName(b.t));
    }

    Flow execBlock(const Block& b) {
        for (const StmtP& s : b) {
            Flow f = exec(*s);
            if (f != Flow::None) {
                return f;
            }
        }
        return Flow::None;
    }

    // ---- leitura de um caminho como L-VALUE (Deltatime.Increment) --------
    // devolve {varName} ou {ticName, chain} consoante o destino
    void lvalueOf(const Expr& e, std::string& varName, std::string& ticName,
                  std::vector<std::string>& chain) {
        if (e.kind == Expr::Kind::Var) {
            varName = e.name;
            return;
        }
        if (e.kind == Expr::Kind::Path && !e.segs.empty()) {
            if (e.segs.size() == 1) {
                varName = e.segs[0];
                return;
            }
            // variável TIC ou TIC direto
            if (Value* v = findVar(e.segs[0])) {
                if (v->t == Type::Tic) {
                    ticName = v->tic;
                    chain.assign(e.segs.begin() + 1, e.segs.end());
                    return;
                }
                fail(e.line, std::string("'") + e.segs[0] + "' é " + typeName(v->t) +
                                 " — Deltatime.Increment precisa de variável " +
                                 "numérica ou propriedade de TIC");
            }
            ticName = e.segs[0];
            chain.assign(e.segs.begin() + 1, e.segs.end());
            return;
        }
        fail(e.line, std::string("Deltatime.Increment: o 1º argumento tem de ser uma ") +
                     "variável (ex.: vida) ou propriedade (ex.: " +
                     "jogador.pos.x)");
    }

    // lê o valor ATUAL de um l-value (var ou propriedade)
    Value readLvalue(const std::string& varName, const std::string& ticName,
                     const std::vector<std::string>& chain, u32 line) {
        if (!varName.empty()) {
            if (Value* v = findVar(varName)) {
                return *v;
            }
            fail(line, std::string("variável '") + varName + "' não existe");
        }
        Value out;
        std::string herr;
        if (!host.getProp(ticName, chain, out, herr)) {
            fail(line, herr);
        }
        return out;
    }

    // escreve num l-value com verificação de tipo do slot
    void writeLvalue(const std::string& varName, const std::string& ticName,
                     const std::vector<std::string>& chain, const Value& v,
                     u32 line) {
        if (!varName.empty()) {
            if (Value* slot = findVar(varName)) {
                Value nv = v;
                coerceInto(slot->t == Type::None ? nv.t : slot->t, nv, line,
                           ("variável '" + varName + "'").c_str());
                *slot = nv;
                return;
            }
            fail(line, std::string("variável '") + varName + "' não existe");
        }
        std::string herr;
        if (!host.setProp(ticName, chain, v, herr)) {
            fail(line, herr);
        }
    }

    // ---- COMANDOS (§9 — lista FECHADA; registo em tabela) -----------------
    // (ver bloco REGISTO DE COMANDOS abaixo — a tabela kCommands)
    void runCommand(const Stmt& s);

    // helpers dos comandos
    std::vector<Value> evalArgs(const Stmt& s) {
        std::vector<Value> out;
        out.reserve(s.args.size());
        for (const ExprP& a : s.args) {
            out.push_back(eval(*a));
        }
        return out;
    }

    static bool isNum(const Value& v) {
        return v.t == Type::Int || v.t == Type::Num;
    }
    static f64 asNum(const Value& v) {
        return v.t == Type::Int ? (f64)v.i : v.n;
    }
};

// ---------------------------------------------------------------------------
// REGISTO DE COMANDOS DA ENGINE (spec §9 — lista FECHADA)
// ---------------------------------------------------------------------------
// Adicionar um comando = 1 handler + 1 linha na tabela (o parser NÃO muda —
// o PathCall já entrega qualquer caminho; é AQUI que se decide o que existe).
// 0.9.3 acrescenta o registo central de linkers/tykers com este MESMO
// padrão (ficheiro próprio).
// ---------------------------------------------------------------------------

bool cmdViewP(Vm& vm, const Stmt& s) {
    if (!s.hasArgs || s.args.size() != 1) {
        vm.fail(s.line, "View P: escreve assim — View P \"texto\"");
    }
    Value v = vm.eval(*s.args[0]);   // avalia UMA vez (args podem ter efeitos)
    if (v.t != Type::Txt) {
        vm.fail(s.line, std::string("View P: o texto vai entre aspas (veio ") +
                           typeName(v.t) + ")");
    }
    vm.host.log(("voni: " + v.s).c_str());   // prefixo da spec §9
    return true;
}

bool cmdViewObject(Vm& vm, const Stmt& s) {
    (void)s;
    vm.fail(s.line, "View Object: consola ainda não existe");
}

bool cmdMove(Vm& vm, const Stmt& s) {
    if (!s.hasArgs || s.args.size() != 3) {
        vm.fail(s.line, "move: escreve assim — move(x, y, z) com números");
    }
    std::vector<Value> a = vm.evalArgs(s);
    if (!vm.isNum(a[0]) || !vm.isNum(a[1]) || !vm.isNum(a[2])) {
        vm.fail(s.line, "move: os 3 valores têm de ser números");
    }
    vm.host.moveTic((f32)vm.asNum(a[0]), (f32)vm.asNum(a[1]),
                    (f32)vm.asNum(a[2]));
    return true;
}

bool cmdImportAnimation(Vm& vm, const Stmt& s) {
    if (!s.hasArgs || s.args.size() != 1) {
        vm.fail(s.line,
                "Import.Animation: escreve assim — "
                "Import.Animation(\"nome da animação\")");
    }
    std::vector<Value> a = vm.evalArgs(s);
    if (a[0].t != Type::Txt) {
        vm.fail(s.line, "Import.Animation: o nome vai entre aspas");
    }
    std::string err;
    if (!vm.host.importAnim(a[0].s, err)) {
        vm.fail(s.line, err.empty()
                            ? "Import.Animation: animação '" + a[0].s +
                                  "' não encontrada"
                            : err);
    }
    return true;
}

bool cmdExplodeEt(Vm& vm, const Stmt& s) {
    if (s.hasArgs) {
        vm.fail(s.line, "Explode.TIC.et não leva argumentos");
    }
    vm.host.explodeTic(true);   // et = DESAPARECER (§9)
    return true;
}

bool cmdExplodeEr(Vm& vm, const Stmt& s) {
    if (s.hasArgs) {
        vm.fail(s.line, "Explode.TIC.er não leva argumentos");
    }
    vm.host.explodeTic(false);  // er = APARECER (§9)
    return true;
}

bool cmdDeltatimeIncrement(Vm& vm, const Stmt& s) {
    if (!s.hasArgs || s.args.size() != 2) {
        vm.fail(s.line,
                "Deltatime.Increment: escreve assim — "
                "Deltatime.Increment(variável, valor_por_segundo)");
    }
    std::string varName, ticName;
    std::vector<std::string> chain;
    vm.lvalueOf(*s.args[0], varName, ticName, chain);
    Value rate = vm.eval(*s.args[1]);
    if (!vm.isNum(rate)) {
        vm.fail(s.line, std::string("Deltatime.Increment: o 2º argumento é o valor por ") +
                        "segundo (número)");
    }
    Value cur = vm.readLvalue(varName, ticName, chain, s.line);
    if (!vm.isNum(cur)) {
        vm.fail(s.line, std::string("Deltatime.Increment: o alvo tem de ser numérico ") +
                        "(veio " + typeName(cur.t) + ")");
    }
    if (cur.t == Type::Int) {
        vm.fail(s.line, std::string("Deltatime.Increment: '") +
                        (varName.empty() ? ticName : varName) +
                        "' é Int — usa Num (o dt é fracionário e a cada " +
                        "frame truncaria para 0)");
    }
    const f64 delta = vm.asNum(rate) * vm.dt;
    vm.writeLvalue(varName, ticName, chain,
                   Value::ofNum(cur.n + delta), s.line);
    return true;
}

struct CmdEntry {
    const char* key;                    // caminho com pontos: "View.P"
    bool (*fn)(Vm&, const Stmt&);
};

// A LISTA FECHADA (§9). Nomes de TIC/`transition.for` são PADRÕES (abaixo).
static const CmdEntry kCommands[] = {
    {"View.P",              &cmdViewP},
    {"View.Object",         &cmdViewObject},
    {"move",                &cmdMove},
    {"Import.Animation",    &cmdImportAnimation},
    {"Explode.TIC.et",      &cmdExplodeEt},
    {"Explode.TIC.er",      &cmdExplodeEr},
    {"Deltatime.Increment", &cmdDeltatimeIncrement},
};

void Vm::runCommand(const Stmt& s) {
    // chave: segs unidos por '.'
    std::string key;
    for (size_t i = 0; i < s.path.size(); ++i) {
        if (i) {
            key += '.';
        }
        key += s.path[i];
    }

    // 1) comandos exatos da lista fechada
    for (const CmdEntry& c : kCommands) {
        if (key == c.key) {
            c.fn(*this, s);
            return;
        }
    }

    // 2) PADRÃO: nomedacena.transition.for("cena de destino") (§9)
    if (s.path.size() == 3 && s.path[1] == "transition" &&
        s.path[2] == "for") {
        std::vector<Value> a = evalArgs(s);
        if (a.size() != 1 || a[0].t != Type::Txt) {
            fail(s.line, std::string("transition.for: escreve assim — ") +
                             s.path[0] + ".transition.for(\"cena de " +
                             "destino\")");
        }
        std::string err;
        if (!host.transitionTo(s.path[0], a[0].s, err)) {
            fail(s.line, err);
        }
        return;
    }

    // 3) Search como INSTRUÇÃO: é expressão (§9) — guiar o autor
    if (!s.path.empty() && s.path[0] == "Search") {
        fail(s.line, std::string("Search é uma expressão — usa-a para LER valores ") +
                     "(ex.: v++p=Search.jogador.pos)");
    }

    // 3.5) 0.9.5: linker/tyker chamados como comando (a declaração sem a
    // forma completa — ex. linker(a)to(b) SEM o =RF(nome)) — o runtime
    // ENSINA a forma certa em vez de "comando não existe" seco
    if (key == "linker") {
        fail(s.line, std::string("linker declara-se no TOPO com a forma ") +
                     "completa: linker(A)to(B)=RF(nome) — o RF é obrigatório");
    }
    if (key == "tyker") {
        fail(s.line, std::string("tyker declara-se no TOPO com a forma ") +
                     "completa: tyker(nome){ find(RF) componentes… }");
    }

    // 4) função do utilizador chamada como instrução (soma(1,2) à solta)
    if (s.path.size() == 1) {
        if (const FnDef* fn = findFn(s.path[0])) {
            std::vector<Value> args = evalArgs(s);
            Value out;
            callFn(*fn, args, s.line, out);
            return;
        }
    }

    fail(s.line, std::string("comando '") + key +
                     "' não existe (a lista fechada está nas Docs)");
}

// ---------------------------------------------------------------------------
// 0.9.5 · LINKERS & TYKERS no ciclo da central
// ---------------------------------------------------------------------------
// A resolução dos args corre AQUI (a Vm tem eval — literais OU variáveis do
// script, ambas válidas 🔶). Os slots de NOME (copy/colorpars) capturam o
// identificador cru em vez de o avaliar; os slots de VALOR avaliam.
namespace tykerwire {

// o TykerDef da AST pelo nome (o estado guarda o nome; o programa é estável)
const TykerDef* findDef(const Program& p, const std::string& name) {
    for (const TykerDef& t : p.tykers) {
        if (t.name == name) {
            return &t;
        }
    }
    return nullptr;
}

// o arg como NOME cru (Var/Path de 1 segmento, ou Txt entre aspas)
bool rawName(Vm& vm, const Expr& e, std::string& out, u32 line,
             const char* comp) {
    if (e.kind == Expr::Kind::Var) {
        out = e.name;
        return true;
    }
    if (e.kind == Expr::Kind::Path && e.segs.size() == 1) {
        out = e.segs[0];
        return true;
    }
    if (e.kind == Expr::Kind::Lit && e.lit.t == Type::Txt) {
        out = e.lit.s;
        return true;
    }
    vm.fail(line, std::string(comp) + ": o argumento é um NOME (ex.: " +
                       comp + "(pos)) — veio uma expressão");
    return false;   // (vm.fail lança — isto acalma o compilador)
}

// resolve um componente da AST num Comp de runtime (args avaliados)
tykers::Comp resolveComp(Vm& vm, const TykerComp& a) {
    tykers::Comp m;
    m.name = a.isChange ? "Change" : a.name;
    m.line = a.line;
    m.isChange = a.isChange;
    m.changeDestino = a.changeDestino;
    m.changePath = a.changePath;
    m.meta = reg::findComponent(m.name);
    if (a.isChange) {
        return m;   // Change: sem args avaliados (o alvo é caminho)
    }
    // slots de NOME (copy/colorpars) × slots de VALOR (o resto)
    const bool nameSlot = (a.name == "copy" || a.name == "colorpars");
    if (nameSlot) {
        if (!a.args.empty()) {
            rawName(vm, *a.args[0], m.raw1, a.line, a.name.c_str());
        }
    } else {
        m.vals.reserve(a.args.size());
        for (const ExprP& e : a.args) {
            m.vals.push_back(vm.eval(*e));
        }
    }
    if (a.hasTail2) {
        m.hasColor2 = true;
        if (a.isColor2) {
            m.color2 = a.color2;
        } else if (a.arg2) {
            rawName(vm, *a.arg2, m.color2, a.line, "colorpars");
        }
    }
    return m;
}

} // namespace tykerwire

// ---------------------------------------------------------------------------
// arranque: linkers → RFs (ciclo = FATAL legível) + estados dos tykers
// (RF em falta = LOG + tyker desligado — a spec: "tyker não corre")
// ---------------------------------------------------------------------------
static void initTykers(Vm& vm, Script::Impl& st) {
    for (const LinkerDecl& l : st.program->linkers) {
        std::string terr;
        if (!st.rfReg.addLinker(l.origem, l.destino, l.rf, l.line, terr)) {
            vm.fail(l.line, terr);   // rejeitado: o script não corre ambíguo
        }
    }
    for (const TykerDef& t : st.program->tykers) {
        tykers::TykerState ts;
        ts.name = t.name;
        ts.line = t.line;
        ts.rf = t.findRf;
        if (!st.rfReg.findRf(t.findRf)) {
            ts.missing = true;
            // o ERRO LEGÍVEL da spec — 1× no arranque; o resto do script
            // CONTINUA (nunca fatal, nunca crash)
            vm.host.log(("voni: RF '" + t.findRf + "' não encontrada — o "
                         "tyker '" + t.name + "' não corre (linha " +
                         std::to_string(t.findLine) + ")")
                            .c_str());
        }
        // delay(s): lido no ARRANQUE (define QUANDO o tyker ativa); fica o
        // ÚLTIMO delay declarado. Args avaliados AGORA (globais já existem).
        for (const TykerComp& c : t.comps) {
            if (!c.isChange && c.name == "delay" && !c.args.empty()) {
                Value v = vm.eval(*c.args[0]);
                if (v.t == Type::Int) {
                    ts.delayS = (f64)v.i;
                } else if (v.t == Type::Num) {
                    ts.delayS = v.n;
                } else {
                    vm.fail(c.line,
                            "delay: os segundos têm de ser um número");
                }
                if (ts.delayS < 0.0) {
                    ts.delayS = 0.0;
                }
            }
        }
        st.tykerRuns.push_back(std::move(ts));
    }
}

// ---------------------------------------------------------------------------
// ativação: componentes pontuais disparam 1×; contínuos ficam na lista
// ---------------------------------------------------------------------------
static void activateTyker(Vm& vm, Script::Impl& st,
                           tykers::TykerState& ts) {
    const TykerDef* def = tykerwire::findDef(*st.program, ts.name);
    if (!def) {
        return;   // (inalcançável — o estado nasce dos defs)
    }
    const std::vector<tykers::Link>* links = st.rfReg.findRf(ts.rf);
    for (const TykerComp& astc : def->comps) {
        if (!astc.isChange && astc.name == "delay") {
            continue;   // consumido no arranque ( gating da ativação)
        }
        tykers::Comp m = tykerwire::resolveComp(vm, astc);
        tykers::Handler h = reg::handlerFor(m.name);
        if (m.meta && m.meta->compKind == reg::CompKind::Continuo) {
            ts.continuous.push_back(std::move(m));
            continue;
        }
        if (!links) {
            continue;
        }
        for (const tykers::Link& lk : *links) {
            tykers::Ctx ctx{vm.host, st.rfReg, ts, lk, 0.0, m.line};
            std::string cerr;
            if (!h || !h(ctx, m, cerr)) {
                vm.fail(m.line,
                        cerr.empty() ? ("componente '" + m.name +
                                        "' falhou")
                                     : cerr);
            }
        }
    }
}

// ---------------------------------------------------------------------------
// o tick por frame (DEPOIS dos allmoments): contínuos sobre cada link
// ---------------------------------------------------------------------------
static void tickTykers(Vm& vm, Script::Impl& st, f64 dt) {
    for (tykers::TykerState& ts : st.tykerRuns) {
        if (ts.missing) {
            continue;
        }
        ts.elapsed += dt;
        if (!ts.activated) {
            if (ts.elapsed < ts.delayS) {
                continue;
            }
            ts.activated = true;
            activateTyker(vm, st, ts);
        }
        if (ts.continuous.empty()) {
            continue;
        }
        const std::vector<tykers::Link>* links = st.rfReg.findRf(ts.rf);
        if (!links || links->empty()) {
            continue;
        }
        for (const tykers::Comp& m : ts.continuous) {
            vm.spend(m.line);   // o budget cobre os tykers (nunca hang)
            tykers::Handler h = reg::handlerFor(m.name);
            for (const tykers::Link& lk : *links) {
                tykers::Ctx ctx{vm.host, st.rfReg, ts, lk, dt, m.line};
                std::string cerr;
                if (!h || !h(ctx, m, cerr)) {
                    vm.fail(m.line,
                            cerr.empty() ? ("componente '" + m.name +
                                            "' falhou")
                                         : cerr);
                }
            }
        }
    }
}

} // namespace

// ---------------------------------------------------------------------------
// Script API — o ciclo da "central" (§3)
// ---------------------------------------------------------------------------
bool Script::runStart(Host& host, Error& err) {
    err = Error::fine();
    Impl& st = *impl_;
    if (!st.program) {
        err = Error::fail(0, "script não compilado");
        return false;
    }
    if (st.started) {
        err = Error::fail(0, std::string("script já iniciado (stop() ") +
                                 "antes de reiniciar)");
        return false;
    }
    st.started = true;
    st.running = true;

    Vm vm(host, st);
    vm.dt = host.frameDt();
    bool ok = true;
    try {
        Flow f = vm.execBlock(st.program->top);
        if (ok && f != Flow::None) {
            vm.fail(vm.flowLine,
                    f == Flow::Return
                        ? std::string("'return' fora de fn")
                        : std::string("'continue'/'resume' fora de loop"));
            ok = false;
        }
        if (ok && st.program->central.hasOn) {
            Flow f2 = vm.execBlock(st.program->central.onMoment);
            if (f2 != Flow::None) {
                vm.fail(vm.flowLine,
                        f2 == Flow::Return
                            ? std::string("'return' fora de fn")
                            : std::string("'continue'/'resume' fora de loop"));
                ok = false;
            }
        }
        // 0.9.5: linkers → RFs (ciclo = FATAL legível, o linker rejeitado)
        // + estados dos tykers (RF em falta = log + tyker desligado)
        if (ok) {
            initTykers(vm, st);
        }
    } catch (const VmFail&) {
        ok = false;
    }
    st.lastInstr = Script::kDefaultTickBudget - vm.budget;
    if (!ok) {
        err = vm.err;
        st.fatal = vm.err;
        st.running = false;
        return false;
    }
    return true;
}

bool Script::runFrame(Host& host, f64 dt, Error& err) {
    err = Error::fine();
    Impl& st = *impl_;
    if (!st.program) {
        err = Error::fail(0, "script não compilado");
        return false;
    }
    if (!st.started) {
        err = Error::fail(0, "runFrame sem runStart");
        return false;
    }
    if (!st.running) {
        err = st.fatal;   // repete o erro fatal (a central loga 1×)
        return false;
    }

    Vm vm(host, st);
    vm.dt = dt;
    bool ok = true;
    try {
        // allmoments SE existir (spec §3)…
        if (st.program->central.hasAll) {
            Flow f = vm.execBlock(st.program->central.allMoments);
            if (f != Flow::None) {
                vm.fail(vm.flowLine,
                        f == Flow::Return
                            ? std::string("'return' fora de fn")
                            : std::string("'continue'/'resume' fora de loop"));
                ok = false;
            }
        }
        // …e OS TYKERS SEMPRE (0.9.5): um script só com linkers/tykers, sem
        // central main, comporta-se na mesma — a decisão 🔶 DEPOIS dos
        // allmoments (o comportamento do linker "fecha" o frame)
        if (ok && !st.tykerRuns.empty()) {
            tickTykers(vm, st, dt);
        }
    } catch (const VmFail&) {
        ok = false;
    }
    st.lastInstr = Script::kDefaultTickBudget - vm.budget;
    if (!ok) {
        err = vm.err;
        st.fatal = vm.err;
        st.running = false;
        return false;
    }
    return true;
}

void Script::stop() { impl_->reset(); }
bool Script::started() const { return impl_->started; }
bool Script::running() const { return impl_->running; }
u64  Script::lastTickInstructions() const { return impl_->lastInstr; }

const std::vector<ExportedVar> Script::exported() const {
    std::vector<ExportedVar> out;
    const Impl& st = *impl_;
    for (const std::string& name : st.exportOrder) {
        auto it = st.globals.find(name);
        if (it != st.globals.end()) {
            ExportedVar v;
            v.name = name;
            v.type = it->second.t;
            v.value = it->second;
            out.push_back(std::move(v));
        }
    }
    return out;
}

bool Script::setVar(const std::string& name, const Value& v, Error& err) {
    err = Error::fine();
    Impl& st = *impl_;
    auto it = st.globals.find(name);
    if (it == st.globals.end()) {
        err = Error::fail(0, "variável '" + name + "' não existe");
        return false;
    }
    Value nv = v;
    if (it->second.t != Type::None && nv.t != it->second.t) {
        // coerção Int→Num (alargamento) apenas
        if (it->second.t == Type::Num && nv.t == Type::Int) {
            nv = Value::ofNum((f64)nv.i);
        } else {
            err = Error::fail(0, "'" + name + "' é " +
                                       typeName(it->second.t) +
                                       " (veio " + typeName(nv.t) + ")");
            return false;
        }
    }
    it->second = nv;
    return true;
}

} // namespace voni
