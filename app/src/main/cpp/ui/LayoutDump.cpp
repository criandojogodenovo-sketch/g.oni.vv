// ui/LayoutDump.cpp — 0.9.6.5 (GRUPO B): o dump JSON + o validador puro.
// Ver ui/LayoutDump.h (o contrato). AQUI só matemática e texto — zero GL,
// zero Android: o CI afere as regras com Records plantados à mão e o
// device lê o resultado do frame REAL.
#include "ui/LayoutDump.h"
#include <cstdio>
#include <cstring>

namespace vv {
namespace layout {

namespace {

// arredonda a 0.1px (dump legível + round-trip estável)
inline double r1(f32 v) {
    return double(static_cast<long long>(v * 10.0f + (v >= 0 ? 0.5f : -0.5f))) / 10.0;
}

// interseção em px² (0 = não se tocam)
inline f32 overlapArea(const Entry& a, const Entry& b) {
    const f32 x0 = a.x > b.x ? a.x : b.x;
    const f32 y0 = a.y > b.y ? a.y : b.y;
    const f32 x1 = (a.x + a.w) < (b.x + b.w) ? (a.x + a.w) : (b.x + b.w);
    const f32 y1 = (a.y + a.h) < (b.y + b.h) ? (a.y + a.h) : (b.y + b.h);
    if (x1 <= x0 || y1 <= y0) return 0.0f;
    return (x1 - x0) * (y1 - y0);
}

// A contém B (com folga de 0.5px — o filho encostado ao pai da região)
inline bool contains(const Entry& a, const Entry& b) {
    return b.x >= a.x - 0.5f && b.y >= a.y - 0.5f &&
           b.x + b.w <= a.x + a.w + 0.5f &&
           b.y + b.h <= a.y + a.h + 0.5f;
}

} // namespace

Json toJson(const Record& r) {
    Json root;
    root.type = Json::Type::Object;
    root.members.emplace_back("ecra",
                              Json::makeString(r.screen ? r.screen : ""));
    {
        Json px;
        px.type = Json::Type::Object;
        px.members.emplace_back("w", Json::makeNumber(double(r.screenW)));
        px.members.emplace_back("h", Json::makeNumber(double(r.screenH)));
        root.members.emplace_back("px", std::move(px));
    }
    {
        Json in;
        in.type = Json::Type::Object;
        in.members.emplace_back("t", Json::makeNumber(double(r.insetT)));
        in.members.emplace_back("b", Json::makeNumber(double(r.insetB)));
        in.members.emplace_back("l", Json::makeNumber(double(r.insetL)));
        in.members.emplace_back("r", Json::makeNumber(double(r.insetR)));
        root.members.emplace_back("insets", std::move(in));
    }
    root.members.emplace_back("densidade", Json::makeNumber(double(r.density)));
    Json arr;
    arr.type = Json::Type::Array;
    for (const Entry& e : r.entries) {
        Json o;
        o.type = Json::Type::Object;
        o.members.emplace_back("tipo", Json::makeString(e.kindName()));
        if (e.id != 0) {
            char idbuf[24];
            std::snprintf(idbuf, sizeof(idbuf), "%llx",
                          static_cast<unsigned long long>(e.id));
            o.members.emplace_back("id", Json::makeString(idbuf));
        }
        {
            Json rect;
            rect.type = Json::Type::Object;
            rect.members.emplace_back("x", Json::makeNumber(r1(e.x)));
            rect.members.emplace_back("y", Json::makeNumber(r1(e.y)));
            rect.members.emplace_back("w", Json::makeNumber(r1(e.w)));
            rect.members.emplace_back("h", Json::makeNumber(r1(e.h)));
            o.members.emplace_back("rect", std::move(rect));
        }
        if (e.kind == Entry::Label) {
            o.members.emplace_back("largura_texto", Json::makeNumber(r1(e.fullW)));
            if (e.truncated) {
                o.members.emplace_back("truncado", Json::makeBool(true));
            }
        }
        if (e.clipped) {
            o.members.emplace_back("recortado", Json::makeBool(true));
        }
        if (e.compact) {
            // 0.9.6.8 (GRUPO E): a tecla compacta da barra de símbolos — o
            // JSON mostra o QUE ela é (o auditor do device vê a exceção)
            o.members.emplace_back("compacto", Json::makeBool(true));
        }
        arr.items.push_back(std::move(o));
    }
    root.members.emplace_back("entradas", std::move(arr));
    return root;
}

std::vector<Problem> validate(const Record& r) {
    std::vector<Problem> ps;
    const f32 kTol = 0.5f;
    const f32 minTouch = 48.0f * (r.density > 0.05f ? r.density : 1.0f);
    // 0.9.6.8 (GRUPO E): o piso da BARRA DE SÍMBOLOS (spec do autor: 40dp).
    // A exceção é ESTREITA e vigiada: só teclas compactas; 39dp compacto
    // FALHA, botão REGULAR a 40dp FALHA (a sentinela R-027 prova os dois —
    // a POLÍTICA #5: a mudança legítima está explicada no relatório)
    const f32 minCompact = 40.0f * (r.density > 0.05f ? r.density : 1.0f);
    const f32 cx = r.contentX(), cy = r.contentY();
    const f32 cw = r.contentW(), ch = r.contentH();

    for (u32 i = 0; i < r.entries.size(); ++i) {
        const Entry& e = r.entries[i];

        if (e.interactive()) {
            // RectDegenerado — a zona morta (desenha-se mas não existe)
            if (e.w <= 0.5f || e.h <= 0.5f) {
                Problem p;
                p.rule = Problem::RectDegenerado;
                p.sev = Problem::Erro;
                p.ia = i;
                ps.push_back(p);
                continue;   // as outras regras não fazem sentido num rect vazio
            }
            // ForaDoEcra — o interativo tem de estar INTEIRO no contentRect
            // (meio fora = meio intocável; o dono não alcança o botão).
            // EXCEÇÃO: conteúdo de SCROLL (clipped) — as linhas scrolled-out
            // vivem FORA da janela por DESENHO (o clip corta-as); a região
            // do scroll é que é validada, não os filhos deslocados
            if (!e.clipped &&
                (e.x < cx - kTol || e.y < cy - kTol ||
                 e.x + e.w > cx + cw + kTol || e.y + e.h > cy + ch + kTol)) {
                Problem p;
                p.rule = Problem::ForaDoEcra;
                p.sev = Problem::Erro;
                p.ia = i;
                ps.push_back(p);
            }
            // ToquePequeno — 48dp da casa (commit 0.9.6.1-a: teclas ≥48dp;
            // toolbar 48dp; a vara de medir do RMX3624 é a mesma).
            // 0.9.6.8 (GRUPO E): tecla COMPACTA (a barra de símbolos) tem o
            // piso da spec: 40dp — a exceção registada no relatório E e
            // vigiada pela sentinela R-027 (a 39dp continua a falhar)
            {
                const f32 floor = e.compact ? minCompact : minTouch;
                if (e.w < floor - kTol || e.h < floor - kTol) {
                    Problem p;
                    p.rule = Problem::ToquePequeno;
                    p.sev = Problem::Aviso;
                    p.ia = i;
                    ps.push_back(p);
                }
            }
        }

        if (e.kind == Entry::Label) {
            // TextoTruncado — o fit cortou (a informação perdeu-se; os
            // Grupos C-I baixam a contagem alargando o que couber)
            if (e.truncated) {
                Problem p;
                p.rule = Problem::TextoTruncado;
                p.sev = Problem::Aviso;
                p.ia = i;
                ps.push_back(p);
            }
            // TextoSangra — label SEM clip fora do contentRect: o texto a
            // cortar-se na borda do ecrã (a classe do bug do C33 0.9.2 —
            // a lista que desenhava fora do sítio). DENTRO de scroll o
            // clip corta na BORDA DA REGIÃO por desenho — não é sangrar.
            if (!e.clipped &&
                (e.x < cx - kTol || e.y < cy - kTol ||
                 e.x + e.w > cx + cw + kTol || e.y + e.h > cy + ch + kTol)) {
                Problem p;
                p.rule = Problem::TextoSangra;
                p.sev = Problem::Erro;
                p.ia = i;
                ps.push_back(p);
            }
        }
    }

    // Sobreposto — interativo×interativo com interseção real (>1px²) onde
    // NENHUM contém o outro (o scroll CONTÉM os seus botões-filhos — a
    // relação pai-filho é legítima; o pisar parcial é o defeito)
    for (u32 i = 0; i < r.entries.size(); ++i) {
        const Entry& a = r.entries[i];
        if (!a.interactive()) continue;
        for (u32 j = i + 1; j < r.entries.size(); ++j) {
            const Entry& b = r.entries[j];
            if (!b.interactive()) continue;
            const f32 ov = overlapArea(a, b);
            if (ov <= 1.0f) continue;
            if (contains(a, b) || contains(b, a)) continue;
            Problem p;
            p.rule = Problem::Sobreposto;
            p.sev = Problem::Erro;
            p.ia = i;
            p.ib = j;
            ps.push_back(p);
        }
    }
    return ps;
}

std::string describe(const Record& r, const Problem& p) {
    const Entry& a = r.entries[p.ia];
    char buf[256];
    switch (p.rule) {
        case Problem::ForaDoEcra:
            std::snprintf(buf, sizeof(buf),
                          "%s %s(id %llx) fora do contentRect: "
                          "[%.0f,%.0f %.0fx%.0f] vs ecrã %.0fx%.0f+%0.f,%0.f",
                          p.sevName(), a.kindName(),
                          (unsigned long long)a.id, a.x, a.y, a.w, a.h,
                          r.contentW(), r.contentH(), r.contentX(), r.contentY());
            break;
        case Problem::Sobreposto: {
            const Entry& b = r.entries[p.ib];
            std::snprintf(buf, sizeof(buf),
                          "%s %s(id %llx) pisa %s(id %llx): "
                          "[%.0f,%.0f %.0fx%.0f] vs [%.0f,%.0f %.0fx%.0f]",
                          p.sevName(), a.kindName(),
                          (unsigned long long)a.id, b.kindName(),
                          (unsigned long long)b.id, a.x, a.y, a.w, a.h,
                          b.x, b.y, b.w, b.h);
            break;
        }
        case Problem::ToquePequeno:
            // 0.9.6.8 (GRUPO E): a mensagem diz o PISO que falhou — 48dp da
            // casa ou 40dp compacto (a barra de símbolos da spec E)
            std::snprintf(buf, sizeof(buf),
                          "aviso %s(id %llx) %.0fx%.0f < %s (%.0fpx) de toque",
                          a.kindName(), (unsigned long long)a.id, a.w, a.h,
                          a.compact ? "40dp compacto" : "48dp",
                          (a.compact ? 40.0f : 48.0f) *
                              (r.density > 0.05f ? r.density : 1.0f));
            break;
        case Problem::TextoTruncado:
            std::snprintf(buf, sizeof(buf),
                          "aviso label truncada \"…\": largura inteira %.0fpx "
                          "(x=%.0f y=%.0f)",
                          a.fullW, a.x, a.y);
            break;
        case Problem::TextoSangra:
            std::snprintf(buf, sizeof(buf),
                          "%s label SANGRA o contentRect: [%.0f,%.0f %.0fx%.0f] "
                          "vs ecrã %.0fx%.0f+%0.f,%0.f",
                          p.sevName(), a.x, a.y, a.w, a.h, r.contentW(),
                          r.contentH(), r.contentX(), r.contentY());
            break;
        case Problem::RectDegenerado:
            std::snprintf(buf, sizeof(buf),
                          "%s %s(id %llx) com rect degenerado %.0fx%.0f",
                          p.sevName(), a.kindName(),
                          (unsigned long long)a.id, a.w, a.h);
            break;
        default:
            std::snprintf(buf, sizeof(buf), "problema ?");
            break;
    }
    return std::string(buf);
}

std::string report(const Record& r, const std::vector<Problem>& ps) {
    std::string out;
    char buf[160];
    u32 erros = 0, avisos = 0;
    for (const Problem& p : ps) {
        if (p.sev == Problem::Erro) ++erros; else ++avisos;
    }
    std::snprintf(buf, sizeof(buf),
                  "AUDITORIA do ecrã \"%s\" (%.0fx%.0f px, densidade %.2f, "
                  "%zu entradas)\n",
                  r.screen ? r.screen : "?", r.screenW, r.screenH, r.density,
                  r.entries.size());
    out += buf;
    std::snprintf(buf, sizeof(buf), "problemas: %u ERRO(s), %u aviso(s)\n",
                  erros, avisos);
    out += buf;
    if (ps.empty()) {
        out += "VERDE — nenhuma regra violada neste ecrã\n";
        return out;
    }
    for (const Problem& p : ps) {
        out += "  - ";
        out += describe(r, p);
        out += "\n";
    }
    return out;
}

} // namespace layout
} // namespace vv
