#pragma once
// voni/VoniTykers.h — RUNTIME DOS LINKERS & TYKERS (0.9.5 · METADE 1).
//
// O QUE É O QUÊ (spec fechada desta entrega):
//   linker(A)to(B)=RF(nome)  — DECLARAÇÃO de topo: liga A a B e regista o
//                               link no RF(nome). A e B são CAMINHOS
//                               (objeto/TIC/propriedade/animação).
//   tyker(nome){ find(RF) comps… } — bloco de comportamento sobre os links
//                               do RF; find é OBRIGATÓRIO de primeiro.
//
// ESTE FICHEIRO é o MOTOR (estado + handlers); os METADADOS (Docs,
// sintaxe, argc) vivem no REGISTO CENTRAL (VoniRegistry) — UMA fonte
// alimenta tudo. Os handlers são apontados pela tabela do registo.
//
// SEMÂNTICA (decisões 🔶 documentadas no RELATÓRIO-0.9.5):
//   • Ciclo direto a→b + b→a no MESMO RF → erro legível no arranque do
//     script (o linker que fecha o ciclo é REJEITADO — o script não corre
//     ambíguo). Ciclos longos (a→b→c→a) são apanhados pelo mesmo DFS.
//   • Profundidade: o DFS de ciclos tem teto 256 → abort legível (nunca
//     stack overflow, nunca crash).
//   • RF em falta no find → log "voni: RF 'x' não encontrada" + O TYKER
//     NÃO CORRE (o resto do script segue — nunca fatal).
//   • Tykers ticam a CADA FRAME, DEPOIS dos allmoments (o comportamento
//     do linker "fecha" o frame).
//   • Componentes CONTÍNUOS correm por frame; PONTUAIS disparam 1× na
//     ATIVAÇÃO (depois do delay(s) se houver).
//   • Args de componentes resolvem 1× NA ATIVAÇÃO (literais ou variáveis
//     do script — ambos válidos).
//
// GL-free / engine-free: só depende de Voni.h (Host) — host-testável.
#include "voni/Voni.h"

#include <map>
#include <string>
#include <vector>

namespace voni {

namespace reg {
struct Entry;   // o REGISTO (VoniRegistry.h) — metadados por componente
}

namespace tykers {

// ---------------------------------------------------------------------------
// o link: origem → destino (caminhos de pontos; segs[0] é o nome)
// ---------------------------------------------------------------------------
struct Link {
    std::vector<std::string> origem;
    std::vector<std::string> destino;
    u32 line = 1;
};

// ---------------------------------------------------------------------------
// Registry — os RFs de UM script (Impl). "Vários tykers partilham RF":
// o find(nome) de vários tykers resolve à MESMA tabela.
// ---------------------------------------------------------------------------
struct Registry {
    // RF nome → links (ordem de declaração; map = iteração determinística)
    std::map<std::string, std::vector<Link>> rfs;

    // Regista um linker. CICLO (a→b + b→a direto, ou longo via DFS com
    // teto kMaxDepth) → false + err legível (o linker é rejeitado).
    bool addLinker(const std::vector<std::string>& origem,
                   const std::vector<std::string>& destino,
                   const std::string& rf, u32 line, std::string& err);
    const std::vector<Link>* findRf(const std::string& rf) const;
    // Change(origem|destino)to(x): reescreve o LADO de TODOS os links do RF
    // (o RF é partilhado — a mudança afeta todos os tykers que o usam).
    bool changeSide(const std::string& rf, bool destino,
                    const std::vector<std::string>& path);
    void clear();

    static constexpr u32 kMaxDepth = 256;   // teto do DFS (spec: 256)
};

// ---------------------------------------------------------------------------
// Comp — um componente RESOLVIDO na ativação (args avaliados; nomes crus
// capturados). A resolução corre no VoniVm (tem eval); os handlers aqui
// recebem o resultado pronto.
// ---------------------------------------------------------------------------
struct Comp {
    std::string name;                 // "follow" / "Change" / …
    u32         line = 1;
    const reg::Entry* meta = nullptr; // entrada do REGISTO (Docs/argc)

    std::vector<Value> vals;          // args da 1ª cauda AVALIADOS
    std::string raw1;                 // 1º arg como NOME cru (copy/colorpars)
    bool hasColor2 = false;           // 2ª cauda (colorpars)
    std::string color2;               // "#RRGGBB" ou nome de cor
    // Change(origem|destino)to(alvo)
    bool isChange = false;
    bool changeDestino = false;
    std::vector<std::string> changePath;
};

// ---------------------------------------------------------------------------
// TykerState — o estado por tyker (vive no Impl; sobrevive entre frames)
// ---------------------------------------------------------------------------
struct TykerState {
    std::string name;
    u32         line = 1;
    std::string rf;

    bool missing = false;      // RF não encontrada → NÃO CORRE (log 1×)
    bool loggedMissing = false;
    bool activated = false;    // componentes pontuais já dispararam
    f64  elapsed = 0.0;        // segundos desde o 1º frame
    f64  delayS = 0.0;         // delay(s): ativação adiada

    // params dos componentes pontuais:
    bool hasPoint = false;     // point(x,y,z): alvo absoluto
    f32  point[3] = {0, 0, 0};
    bool hasLimit = false;     // limit(min,max): distância do follow/orbit
    f32  limMin = 0, limMax = 0;
    f32  orbitAngle = 0.0f;    // orbit(d,vel): ângulo corrente (graus)

    // colorpars fora do parâmetro 'cor': guardado p/ o shading() futuro
    std::map<std::string, std::string> colorParams;

    // contínuos ativos (resolvidos na ativação; correm a cada frame).
    // Os PONTUAIS não ficam guardados: disparam 1× na ativação e pronto.
    std::vector<Comp> continuous;
};

// ---------------------------------------------------------------------------
// Ctx — o contexto de execução de UM componente sobre UM link
// ---------------------------------------------------------------------------
struct Ctx {
    Host&          host;
    Registry&      reg;
    TykerState&    st;
    const Link&    link;
    f64            dt = 0.0;
    u32            line = 1;
};

using Handler = bool (*)(Ctx&, const Comp&, std::string& err);

// ---- os handlers (implementados em VoniTykers.cpp; a TABELA que despacha
// vive no REGISTO — VoniRegistry.cpp — com as Docs de cada um) -------------
bool compFollow(Ctx& c, const Comp& m, std::string& err);
bool compLook(Ctx& c, const Comp& m, std::string& err);
bool compOrbit(Ctx& c, const Comp& m, std::string& err);
bool compCopy(Ctx& c, const Comp& m, std::string& err);
bool compMap(Ctx& c, const Comp& m, std::string& err);
bool compChange(Ctx& c, const Comp& m, std::string& err);
bool compPoint(Ctx& c, const Comp& m, std::string& err);
bool compColorpars(Ctx& c, const Comp& m, std::string& err);
bool compPlay(Ctx& c, const Comp& m, std::string& err);
bool compLimit(Ctx& c, const Comp& m, std::string& err);
bool compDelay(Ctx& c, const Comp& m, std::string& err);
bool compShading(Ctx& c, const Comp& m, std::string& err);

} // namespace tykers
} // namespace voni
