// voni/VoniTykers.cpp — o runtime dos linkers & tykers (ver header).
//
// O QUE VIVE AQUI: o registo de RFs por script (com a rejeição de CICLOS e
// o teto de profundidade 256), e os HANDLERS dos 13 componentes — a tabela
// que liga nome→handler vive no REGISTO CENTRAL (VoniRegistry.cpp), que é
// também a fonte das Docs de cada um (Docs obrigatória por entrada).
//
// ERROS: todos legíveis, SEM linha (a Vm acrescenta a linha do componente);
// NUNCA exceção para fora — os handlers devolvem false + err.
#include "voni/VoniTykers.h"
#include "voni/VoniRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <set>
#include <sstream>

namespace voni {
namespace tykers {

namespace {

// caminho como texto ("jogador.pos") — id de nó para o grafo dos RFs
std::string pathText(const std::vector<std::string>& p) {
    std::string s;
    for (size_t i = 0; i < p.size(); ++i) {
        if (i) {
            s += '.';
        }
        s += p[i];
    }
    return s;
}

// lê a posição (Vec3) de um TIC
bool ticPos(Host& h, const std::string& tic, f32 out[3], std::string& err) {
    Value v;
    std::string herr;
    if (!h.getProp(tic, {"pos"}, v, herr)) {
        err = herr.empty() ? ("TIC '" + tic + "' não existe") : herr;
        return false;
    }
    if (v.t != Type::Vec3) {
        err = "a posição de '" + tic + "' não é Vec3";
        return false;
    }
    out[0] = v.v3[0];
    out[1] = v.v3[1];
    out[2] = v.v3[2];
    return true;
}

bool setTicPos(Host& h, const std::string& tic, const f32 p[3],
               std::string& err) {
    std::string herr;
    if (!h.setProp(tic, {"pos"}, Value::ofVec3(p[0], p[1], p[2]), herr)) {
        err = herr;
        return false;
    }
    return true;
}

// o nome do TIC de um lado do link (com .pos aceito — escrever a posição é
// o mesmo que tocar no TIC); outros sufixos são propriedades, não posição
bool sideTic(const std::vector<std::string>& side, const char* comp,
             std::string& out, std::string& err) {
    if (side.empty()) {
        err = std::string(comp) + ": lado do linker vazio (erro interno)";
        return false;
    }
    if (side.size() == 1 || (side.size() == 2 && side[1] == "pos")) {
        out = side[0];
        return true;
    }
    err = std::string(comp) + ": este lado do linker tem de ser um TIC — '" +
          pathText(side) + "' é propriedade";
    return false;
}

// o ALVO do comportamento: o ponto do point(x,y,z) SE definido; senão a
// posição do destino (TIC solto ou propriedade Vec3 do caminho)
bool targetPos(Ctx& c, f32 out[3], std::string& err) {
    if (c.st.hasPoint) {
        out[0] = c.st.point[0];
        out[1] = c.st.point[1];
        out[2] = c.st.point[2];
        return true;
    }
    const std::vector<std::string>& d = c.link.destino;
    if (d.empty()) {
        err = "destino do linker vazio (erro interno)";
        return false;
    }
    Value v;
    std::string herr;
    if (!c.host.getProp(d[0], std::vector<std::string>(d.begin() + 1,
                                                       d.end()),
                        v, herr)) {
        err = herr.empty() ? ("o destino '" + pathText(d) + "' não existe")
                           : herr;
        return false;
    }
    if (v.t == Type::Tic) {
        // cadeia através de um valor TIC — resolve a posição DELE
        return ticPos(c.host, v.tic, out, err);
    }
    if (v.t != Type::Vec3) {
        err = "o destino '" + pathText(d) + "' não é uma posição (Vec3)";
        return false;
    }
    out[0] = v.v3[0];
    out[1] = v.v3[1];
    out[2] = v.v3[2];
    return true;
}

// extrai um arg numérico já avaliado
bool argNum(const Comp& m, size_t i, f64& out, std::string& err) {
    if (i >= m.vals.size()) {
        err = m.name + ": falta o " + std::to_string(i + 1) + "º argumento";
        return false;
    }
    const Value& v = m.vals[i];
    if (v.t == Type::Int) {
        out = (f64)v.i;
        return true;
    }
    if (v.t == Type::Num) {
        out = v.n;
        return true;
    }
    err = m.name + ": o " + std::to_string(i + 1) +
          "º argumento tem de ser um número";
    return false;
}

// aplica o limit(min,max) à distância |p - alvo|; se p cai EM CIMA do
// alvo (sem direção), empurra para fora na direção de onde veio (from)
void clampLimit(TykerState& st, const f32 target[3], const f32 from[3],
                f32 p[3]) {
    if (!st.hasLimit) {
        return;
    }
    f32 d[3] = {p[0] - target[0], p[1] - target[1], p[2] - target[2]};
    const f32 len = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (len >= 1e-6f) {
        if (len < st.limMin) {
            const f32 k = st.limMin / len;
            p[0] = target[0] + d[0] * k;
            p[1] = target[1] + d[1] * k;
            p[2] = target[2] + d[2] * k;
        } else if (len > st.limMax) {
            const f32 k = st.limMax / len;
            p[0] = target[0] + d[0] * k;
            p[1] = target[1] + d[1] * k;
            p[2] = target[2] + d[2] * k;
        }
        return;
    }
    // p == alvo: usa a direção DE ONDE VEIO (a posição antiga) para manter
    // a distância mínima (o follow() quer colar; o limit segura)
    f32 f[3] = {from[0] - target[0], from[1] - target[1], from[2] - target[2]};
    const f32 flen = std::sqrt(f[0] * f[0] + f[1] * f[1] + f[2] * f[2]);
    if (flen >= 1e-6f && st.limMin > 0.0f) {
        const f32 k = st.limMin / flen;
        p[0] = target[0] + f[0] * k;
        p[1] = target[1] + f[1] * k;
        p[2] = target[2] + f[2] * k;
    }
}

// a paleta FECHADA dos nomes de cor (colorpars)
bool colorByName(const std::string& n, f32 rgb[3]) {
    struct Named { const char* n; f32 r, g, b; };
    static const Named kPal[] = {
        {"vermelho", 1, 0, 0},   {"verde", 0, 1, 0},
        {"azul", 0, 0, 1},       {"amarelo", 1, 1, 0},
        {"branco", 1, 1, 1},     {"preto", 0, 0, 0},
        {"cinza", 0.5f, 0.5f, 0.5f},
    };
    for (const Named& p : kPal) {
        if (n == p.n) {
            rgb[0] = p.r;
            rgb[1] = p.g;
            rgb[2] = p.b;
            return true;
        }
    }
    return false;
}

int hexDigit(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// "#RRGGBB" → rgb 0..1
bool colorByHex(const std::string& s, f32 rgb[3]) {
    if (s.size() != 7 || s[0] != '#') {
        return false;
    }
    int v[6];
    for (int i = 0; i < 6; ++i) {
        v[i] = hexDigit(s[1 + i]);
        if (v[i] < 0) {
            return false;
        }
    }
    rgb[0] = (v[0] * 16 + v[1]) / 255.0f;
    rgb[1] = (v[2] * 16 + v[3]) / 255.0f;
    rgb[2] = (v[4] * 16 + v[5]) / 255.0f;
    return true;
}

} // namespace

// ---------------------------------------------------------------------------
// Registry — linkers por RF + REJEIÇÃO DE CICLOS (DFS com teto 256)
// ---------------------------------------------------------------------------
bool Registry::addLinker(const std::vector<std::string>& origem,
                         const std::vector<std::string>& destino,
                         const std::string& rf, u32 line, std::string& err) {
    (void)line;
    const std::string a = pathText(origem);
    const std::string b = pathText(destino);

    // auto-link: a→a é o ciclo mais curto
    if (a == b) {
        err = "ciclo: '" + a + "' já está ligado a si próprio no RF '" + rf +
              "' — o linker não pode ligar um objeto a ele mesmo";
        return false;
    }

    std::vector<Link>& links = rfs[rf];

    // ciclo DIRETO: a→b quando b→a já existe no MESMO RF
    for (const Link& l : links) {
        if (pathText(l.origem) == b && pathText(l.destino) == a) {
            err = "ciclo: '" + a + "'→'" + b + "' e '" + b + "'→'" + a +
                  "' no RF '" + rf + "' — um dos dois linkers tem de sair";
            return false;
        }
    }

    links.push_back(Link{origem, destino, line});

    // ciclos LONGOS (a→b→c→a): DFS com teto de profundidade 256 — a MESMA
    // guarda NÃO deixa a cadeia crescer sem fim (abort legível, nunca crash)
    {
        std::map<std::string, std::vector<std::string>> adj;
        for (const Link& l : links) {
            adj[pathText(l.origem)].push_back(pathText(l.destino));
        }
        std::set<std::string> visiting;   // no caminho corrente
        std::set<std::string> done;       // já explorado sem ciclo
        // DFS recursivo com o contador de profundidade — devolve false +
        // preenche err se achar ciclo OU exceder a profundidade
        struct Walk {
            Registry& r;
            std::map<std::string, std::vector<std::string>>& adj;
            std::set<std::string>& visiting;
            std::set<std::string>& done;
            const std::string& rf;
            bool run(const std::string& node, u32 depth, std::string& err) {
                if (depth > r.kMaxDepth) {
                    err = "o RF '" + rf + "' excede a profundidade " +
                          std::to_string(r.kMaxDepth) +
                          " (cadeia de linkers demasiado longa)";
                    return false;
                }
                if (done.count(node)) {
                    return true;
                }
                if (visiting.count(node)) {
                    err = "ciclo detetado no RF '" + rf + "' — a cadeia de " +
                          "linkers volta a '" + node + "'";
                    return false;
                }
                visiting.insert(node);
                auto it = adj.find(node);
                if (it != adj.end()) {
                    for (const std::string& nxt : it->second) {
                        if (!run(nxt, depth + 1, err)) {
                            return false;
                        }
                    }
                }
                visiting.erase(node);
                done.insert(node);
                return true;
            }
        } w{*this, adj, visiting, done, rf};
        for (const auto& kv : adj) {
            if (!w.run(kv.first, 1, err)) {
                // REJEITA o linker que fechou o ciclo: remove-o
                links.pop_back();
                return false;
            }
        }
    }
    return true;
}

const std::vector<Link>* Registry::findRf(const std::string& rf) const {
    auto it = rfs.find(rf);
    return it == rfs.end() ? nullptr : &it->second;
}

bool Registry::changeSide(const std::string& rf, bool destino,
                          const std::vector<std::string>& path) {
    auto it = rfs.find(rf);
    if (it == rfs.end()) {
        return false;
    }
    for (Link& l : it->second) {
        if (destino) {
            l.destino = path;
        } else {
            l.origem = path;
        }
    }
    return true;
}

void Registry::clear() {
    rfs.clear();
}

// ---------------------------------------------------------------------------
// COMPONENTES · contínuos
// ---------------------------------------------------------------------------

// follow() cola · follow(d) guarda a distância d · follow(d,suav) suaviza
// (suav = taxa por segundo; pos += (alvo-op)·(1-e^(-suav·dt)))
bool compFollow(Ctx& c, const Comp& m, std::string& err) {
    f64 d = 0.0, suav = 0.0;
    if (m.vals.size() >= 1 && !argNum(m, 0, d, err)) {
        return false;
    }
    if (m.vals.size() >= 2 && !argNum(m, 1, suav, err)) {
        return false;
    }
    std::string origem;
    if (!sideTic(c.link.origem, "follow", origem, err)) {
        return false;
    }
    f32 op[3], tp[3];
    if (!ticPos(c.host, origem, op, err)) {
        return false;
    }
    if (!targetPos(c, tp, err)) {
        return false;
    }
    f32 dir[3] = {op[0] - tp[0], op[1] - tp[1], op[2] - tp[2]};
    const f32 len = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] +
                              dir[2] * dir[2]);
    f32 desired[3];
    if (len < 1e-6f) {
        // já em cima do alvo: mantém (não há direção para guardar a distância)
        desired[0] = op[0];
        desired[1] = op[1];
        desired[2] = op[2];
    } else {
        const f32 k = (f32)d / len;
        desired[0] = tp[0] + dir[0] * k;
        desired[1] = tp[1] + dir[1] * k;
        desired[2] = tp[2] + dir[2] * k;
    }
    f32 np[3];
    if (suav > 0.0) {
        const f32 kk = (f32)(1.0 - std::exp(-suav * c.dt));
        np[0] = op[0] + (desired[0] - op[0]) * kk;
        np[1] = op[1] + (desired[1] - op[1]) * kk;
        np[2] = op[2] + (desired[2] - op[2]) * kk;
    } else {
        np[0] = desired[0];
        np[1] = desired[1];
        np[2] = desired[2];
    }
    clampLimit(c.st, tp, op, np);
    return setTicPos(c.host, origem, np, err);
}

// look(): a origem fica virada para o alvo (rot em GRAUS, yaw+pitch)
bool compLook(Ctx& c, const Comp& m, std::string& err) {
    (void)m;
    std::string origem;
    if (!sideTic(c.link.origem, "look", origem, err)) {
        return false;
    }
    f32 op[3], tp[3];
    if (!ticPos(c.host, origem, op, err)) {
        return false;
    }
    if (!targetPos(c, tp, err)) {
        return false;
    }
    const f32 dx = tp[0] - op[0], dy = tp[1] - op[1], dz = tp[2] - op[2];
    const f32 yaw = std::atan2(dx, dz) * (f32)(180.0 / 3.14159265358979);
    const f32 horiz = std::sqrt(dx * dx + dz * dz);
    const f32 pitch = std::atan2(dy, horiz) * (f32)(180.0 / 3.14159265358979);
    std::string herr;
    if (!c.host.setProp(origem, {"rot"},
                        Value::ofVec3(pitch, yaw, 0.0f), herr)) {
        err = herr;
        return false;
    }
    return true;
}

// orbit(d,vel): círculo no plano XZ à altura do alvo; vel em GRAUS/segundo
bool compOrbit(Ctx& c, const Comp& m, std::string& err) {
    f64 d = 0.0, vel = 0.0;
    if (!argNum(m, 0, d, err) || !argNum(m, 1, vel, err)) {
        return false;
    }
    std::string origem;
    if (!sideTic(c.link.origem, "orbit", origem, err)) {
        return false;
    }
    f32 tp[3];
    if (!targetPos(c, tp, err)) {
        return false;
    }
    c.st.orbitAngle += (f32)(vel * c.dt);
    if (c.st.orbitAngle > 360.0f) {
        c.st.orbitAngle -= 360.0f;
    }
    if (c.st.orbitAngle < 0.0f) {
        c.st.orbitAngle += 360.0f;
    }
    const f32 a = c.st.orbitAngle * (f32)(3.14159265358979 / 180.0);
    f32 op[3];
    ticPos(c.host, origem, op, err);   // de onde veio (para o limit)
    f32 np[3] = {tp[0] + std::cos(a) * (f32)d, tp[1],
                 tp[2] + std::sin(a) * (f32)d};
    clampLimit(c.st, tp, op, np);
    return setTicPos(c.host, origem, np, err);
}

// copy(propriedade): copia do destino para a origem a cada frame
bool compCopy(Ctx& c, const Comp& m, std::string& err) {
    std::string origem, destino;
    if (!sideTic(c.link.origem, "copy", origem, err) ||
        !sideTic(c.link.destino, "copy", destino, err)) {
        return false;
    }
    Value v;
    std::string herr;
    if (!c.host.getProp(destino, {m.raw1}, v, herr)) {
        err = herr.empty() ? ("copy: o destino '" + destino + "' não tem '" +
                              m.raw1 + "'")
                           : herr;
        return false;
    }
    if (!c.host.setProp(origem, {m.raw1}, v, herr)) {
        err = herr;
        return false;
    }
    return true;
}

// map(): no-op SEM mapa (reservado — a spec manda aceitar e não fazer nada)
bool compMap(Ctx& c, const Comp& m, std::string& err) {
    (void)c;
    (void)m;
    (void)err;
    return true;
}

// ---------------------------------------------------------------------------
// COMPONENTES · pontuais
// ---------------------------------------------------------------------------

// Change(origem|destino)to(alvo): reescreve o lado dos links do RF
bool compChange(Ctx& c, const Comp& m, std::string& err) {
    if (m.changePath.empty()) {
        err = "Change: falta o alvo — Change(origem)to(alvo)";
        return false;
    }
    if (!c.reg.changeSide(c.st.rf, m.changeDestino, m.changePath)) {
        err = "Change: o RF '" + c.st.rf + "' desapareceu (erro interno)";
        return false;
    }
    return true;
}

// point() limpa · point(x,y,z) fixa o alvo absoluto
bool compPoint(Ctx& c, const Comp& m, std::string& err) {
    if (m.vals.empty()) {
        c.st.hasPoint = false;
        return true;
    }
    if (m.vals.size() != 3) {
        err = "point: 0 argumentos (limpa o ponto) ou 3 — point(x,y,z)";
        return false;
    }
    f64 x = 0, y = 0, z = 0;
    if (!argNum(m, 0, x, err) || !argNum(m, 1, y, err) ||
        !argNum(m, 2, z, err)) {
        return false;
    }
    c.st.hasPoint = true;
    c.st.point[0] = (f32)x;
    c.st.point[1] = (f32)y;
    c.st.point[2] = (f32)z;
    return true;
}

// colorpars(cor)(nome|#RRGGBB): 'cor' tinge o material da origem; outros
// nomes ficam guardados no tyker para o shading() futuro
bool compColorpars(Ctx& c, const Comp& m, std::string& err) {
    f32 rgb[3] = {1, 1, 1};
    if (m.color2.size() == 7 && m.color2[0] == '#') {
        if (!colorByHex(m.color2, rgb)) {
            err = "colorpars: '" + m.color2 + "' não é um hex válido "
                  "(#RRGGBB)";
            return false;
        }
    } else if (!colorByName(m.color2, rgb)) {
        err = "colorpars: cor '" + m.color2 + "' não existe (nomes: "
              "vermelho verde azul amarelo branco preto cinza — ou #RRGGBB)";
        return false;
    }
    if (m.raw1 == "cor") {
        std::string origem, herr;
        if (!sideTic(c.link.origem, "colorpars", origem, err)) {
            return false;
        }
        if (!c.host.setProp(origem, {"cor"},
                            Value::ofVec3(rgb[0], rgb[1], rgb[2]), herr)) {
            err = herr;
            return false;
        }
        return true;
    }
    // outro parâmetro: guardado (o shading() futuro consome)
    c.st.colorParams[m.raw1] = m.color2;
    return true;
}

// play(): toca a animação do linker no TIC dono do script
bool compPlay(Ctx& c, const Comp& m, std::string& err) {
    (void)m;
    // a ponta que é nome de ANIMAÇÃO (não é TIC da cena)
    const std::vector<std::string>* animSide = nullptr;
    if (c.link.origem.size() == 1 && !c.host.ticExists(c.link.origem[0])) {
        animSide = &c.link.origem;
    } else if (c.link.destino.size() == 1 &&
               !c.host.ticExists(c.link.destino[0])) {
        animSide = &c.link.destino;
    }
    if (!animSide) {
        err = "play(): o tyker '" + c.st.name + "' não tem animação no " +
              "linker — liga uma: linker(tic)to(animação)=RF(…)";
        return false;
    }
    std::string herr;
    if (!c.host.importAnim((*animSide)[0], herr)) {
        err = herr.empty() ? ("play(): animação '" + (*animSide)[0] +
                              "' não encontrada")
                           : herr;
        return false;
    }
    return true;
}

// limit(min,max): distância mín/máx da origem ao alvo (follow/orbit)
bool compLimit(Ctx& c, const Comp& m, std::string& err) {
    f64 mn = 0, mx = 0;
    if (!argNum(m, 0, mn, err) || !argNum(m, 1, mx, err)) {
        return false;
    }
    if (mn > mx) {
        err = "limit: o min tem de ser menor ou igual ao max";
        return false;
    }
    c.st.hasLimit = true;
    c.st.limMin = (f32)mn;
    c.st.limMax = (f32)mx;
    return true;
}

// delay(s): consumido no ARRANQUE (define QUANDO o tyker ativa) — aqui é
// no-op de confirmação (a ativação já esperou os segundos)
bool compDelay(Ctx& c, const Comp& m, std::string& err) {
    (void)c;
    (void)m;
    (void)err;
    return true;
}

// shading(): reservado — aceita e não faz nada
bool compShading(Ctx& c, const Comp& m, std::string& err) {
    (void)c;
    (void)m;
    (void)err;
    return true;
}

} // namespace tykers
} // namespace voni
