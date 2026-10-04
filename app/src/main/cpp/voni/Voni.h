#pragma once
// voni/Voni.h — API PÚBLICA da linguagem V.ONI v0 (0.9.2 §3, §§9-12).
//
// A V.ONI é a linguagem de scripting FECHADA da G.One VV (spec transcrita
// na campanha 0.9.2 — ÚNICA fonte de verdade). Este header é o contrato:
//
//   voni::Error   — erro com LINHA (spec §12: "erros com linha no editor e
//                   no log"; NUNCA crash)
//   voni::Value   — os 7 tipos fechados da spec §4: Int Num Txt Bool Vec2
//                   Vec3 TIC (booleanos true/false)
//   voni::Host    — interface PURA da engine (a ponte vive em
//                   VoniEngineHost; o core NÃO inclui nada da engine —
//                   host-testável no CI Linux)
//   voni::Script  — compila (gramática isolada em VoniGrammar.cpp 🔶) e
//                   corre: top-level 1× → on moment 1× → allmoments/frame
//                   (a "central" que agenda é o VoniSystem na engine)
//
// SANDBOX (spec §12): o core NÃO tem APIs de ficheiro/rede/sistema — o
// isolamento é ESTRUTURAL (não há o que chamar). Budget de instruções por
// tick + profundidade de chamadas 256 com abort legível.
//
// GL-free / Android-free / engine-free: roda nos testes do CI Linux.
#include "core/Types.h"

#include <string>
#include <vector>

namespace voni {

// aliases do engine (vv::) importados — a V.ONI usa a mesma largura fixa
using vv::u8;
using vv::u32;
using vv::u64;
using vv::i64;
using vv::f32;
using vv::f64;

// ---------------------------------------------------------------------------
// tipos fechados (spec §4 🔶: Int, Num, Txt, Bool, Vec2, Vec3, TIC)
// ---------------------------------------------------------------------------
enum class Type : u8 {
    None = 0,
    Int,    // i64
    Num,    // f64
    Txt,    // std::string
    Bool,   // bool
    Vec2,   // 2×f32
    Vec3,   // 3×f32
    Tic,    // referência por NOME a um TIC da cena (resolvido via Host)
};

std::string typeName(Type t);              // "Int"… (nomes da spec)
bool        typeFromName(const char* s, Type& out);   // "Num" → Type::Num

// ---------------------------------------------------------------------------
// valor de run-time
// ---------------------------------------------------------------------------
struct Value {
    Type t = Type::None;
    i64       i = 0;                // Int
    f64       n = 0.0;              // Num
    std::string s;                  // Txt
    bool      b = false;            // Bool
    f32       v2[2] = {0, 0};       // Vec2
    f32       v3[3] = {0, 0, 0};    // Vec3
    std::string tic;                // TIC (nome)

    static Value ofInt(i64 v)               { Value x; x.t = Type::Int; x.i = v; return x; }
    static Value ofNum(f64 v)               { Value x; x.t = Type::Num; x.n = v; return x; }
    static Value ofTxt(std::string v)       { Value x; x.t = Type::Txt; x.s = std::move(v); return x; }
    static Value ofBool(bool v)             { Value x; x.t = Type::Bool; x.b = v; return x; }
    static Value ofVec2(f32 a, f32 b)       { Value x; x.t = Type::Vec2; x.v2[0]=a; x.v2[1]=b; return x; }
    static Value ofVec3(f32 a, f32 b, f32 c){ Value x; x.t = Type::Vec3; x.v3[0]=a; x.v3[1]=b; x.v3[2]=c; return x; }
    static Value ofTic(std::string name)    { Value x; x.t = Type::Tic; x.tic = std::move(name); return x; }

    // componente .x/.y/.z de Vec2/Vec3 (o acesso a propriedades de vetores
    // é na própria linguagem: jogador.pos.x — ver Vm). idx 0..2.
    bool vecComponent(u32 idx, f32& out) const;
    bool setVecComponent(u32 idx, f32 v);   // retorna false se não é Vec
};

// texto "1"/"1.5"/"true"/"[1,2,3]" (para o Inspector e o View P de valores)
std::string valueText(const Value& v);

// ---------------------------------------------------------------------------
// erro com LINHA (spec §12: nunca crash; linha + mensagem legível)
// ---------------------------------------------------------------------------
struct Error {
    bool   ok = true;
    u32    line = 0;      // 1-based; 0 = sem linha (não deveria acontecer)
    std::string message;  // PT, legível, sem jargão
    // 0.9.6 (G2-7e) · O BOTÃO SUBSTITUIR: quando o erro-que-ensina tem
    // equivalente de 1 token (if→exist…), o par viaja COM o erro — o
    // editor acende o botão e troca a palavra no buffer. Vazios = ensina
    // sem substituir (case/default/elif…). O 'break' traz o equivalente
    // do CONTEXTO real da run (ciclo→resume, option→stopand).
    std::string fixFrom;  // a palavra estrangeira (ex.: "if")
    std::string fixTo;    // o equivalente V.ONI (ex.: "exist")

    static Error fine() { return Error{}; }
    static Error fail(u32 line_, const std::string& msg) {
        Error e; e.ok = false; e.line = line_; e.message = msg; return e;
    }
    static Error teach(u32 line_, const std::string& msg,
                       const std::string& from, const std::string& to) {
        Error e = fail(line_, msg);
        e.fixFrom = from;
        e.fixTo = to;
        return e;
    }
};

// ---------------------------------------------------------------------------
// Host — a engine VISTA pela V.ONI (pura; a bridge é VoniEngineHost)
// ---------------------------------------------------------------------------
class Host {
public:
    virtual ~Host() = default;

    // log (View P escreve com prefixo "voni:" — spec §9)
    virtual void log(const char* line) = 0;

    // propriedades de TIC (RTTI §9: jogador.pos / Search.alvo.propriedade)
    // `ticName` = 1º segmento; `chain` = segmentos seguintes ({"pos","x"}).
    // Devolve false + preenche `err` (legível, SEM linha — a Vm acrescenta)
    // se o TIC ou a propriedade não existirem.
    virtual bool getProp(const std::string& ticName,
                         const std::vector<std::string>& chain,
                         Value& out, std::string& err) = 0;
    virtual bool setProp(const std::string& ticName,
                         const std::vector<std::string>& chain,
                         const Value& v, std::string& err) = 0;

    // existe algum TIC ativo com este nome? (resolução de caminhos)
    virtual bool ticExists(const std::string& ticName) = 0;

    // comandos do TIC anfitrião (o dono do componente Script — a "central"
    // define-o antes de cada tick)
    virtual void moveTic(f32 dx, f32 dy, f32 dz) = 0;       // move(x,y,z) — RELATIVO 🔶
    virtual void explodeTic(bool hide) = 0;                 // Explode.TIC.et/.er
    virtual bool importAnim(const std::string& name, std::string& err) = 0;

    // nomedacena.transition.for("destino") (spec §9 — NÃO se chama importar)
    // fromName tem de ser a cena ATUAL (decisão 🔶 documentada).
    virtual bool transitionTo(const std::string& fromName,
                              const std::string& toName, std::string& err) = 0;
    virtual std::string currentSceneName() = 0;

    // Search.alvo.função/propriedade — RTTI (v0: só propriedades; a forma
    // "função" devolve erro legível até existirem funções registadas 0.9.3)
    virtual bool search(const std::string& target,
                        const std::vector<std::string>& chain,
                        Value& out, std::string& err) = 0;

    // dt do frame corrente (Deltatime.Increment usa valor × dt — spec §9)
    virtual f64 frameDt() = 0;
};

// ---------------------------------------------------------------------------
// variável exportada (v#@+nome / v++@+nome → aparece no Inspector, spec §4)
// ---------------------------------------------------------------------------
struct ExportedVar {
    std::string name;
    Type        type = Type::None;
    Value       value;
};

// ---------------------------------------------------------------------------
// Script — uma instância compilada de um ficheiro .voni
// ---------------------------------------------------------------------------
class Script {
public:
    Script();
    ~Script();

    Script(Script&&) noexcept;
    Script& operator=(Script&&) noexcept;

    // compila `source` (a gramática ATIVA: default de VoniGrammar.cpp ou o
    // override de setGrammar — spec §1 🔶). Falha → ok=false + line+message.
    // Compilar NÃO executa nada (sandbox: validar antes de correr).
    static Script compile(const char* source, Error& err);

    // ---- ciclo de vida (a "central" = VoniSystem agenda assim) -----------
    // top-level 1× → on moment 1× (spec §3). Idempotente: chamar 2× sem
    // stop() devolve false (proteção da central contra double-start).
    bool runStart(Host& host, Error& err);

    // allmoments 1× (cada frame enquanto ativo — spec §3). Só depois de
    // runStart; sem allmoments no script é no-op que devolve true.
    bool runFrame(Host& host, f64 dt, Error& err);

    // parar (o próximo runStart recomeça do zero: vars re-declaradas)
    void stop();

    bool started() const;
    bool running() const;    // started && !parado && sem erro fatal

    // variáveis exportadas (Inspector; edição via setVar ANTES do próximo
    // restart aplica ao vivo 🔶 — documentado)
    const std::vector<ExportedVar> exported() const;
    bool setVar(const std::string& name, const Value& v, Error& err);

    // (debug/testes) nº de instruções executadas no último tick
    u64 lastTickInstructions() const;

    // budget de instruções por tick (spec §12; default 200000 — loops
    // infinitos abortam com erro legível "budget de iterações excedido")
    static constexpr u64 kDefaultTickBudget = 200000;
    static constexpr u32 kMaxCallDepth = 256;   // spec §12: profundidade 256

    struct Impl;
    Impl& impl() { return *impl_; }

private:
    Impl* impl_ = nullptr;
};

// gramática ativa (override de testes — spec §1 🔶 "mudar um literal no
// ficheiro de gramática muda comportamento sem tocar em C++")
void setGrammar(const char* grammarOverrideOrNull);
const char* activeGrammar();

// palavras reservadas (spec §5 — lista FIXA minúsculas). vive com a
// gramática (VoniGrammar.cpp) porque é definição da LINGUAGEM.
bool isReservedWord(const std::string& lower);
const char* const* reservedWords();   // terminada em nullptr (Docs/testes)
u32 reservedWordCount();

} // namespace voni
