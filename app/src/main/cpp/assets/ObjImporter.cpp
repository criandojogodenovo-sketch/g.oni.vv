// assets/ObjImporter.cpp — parser Wavefront OBJ (F5-B).
//
// Tokenizador de linha simples sobre o buffer em memória. Sem alocações por
// token além das strings de nomes (grupo/material) — os vértices saem
// direto para MeshData via dedup de cantos (hash em vetor aberto).
#include "assets/ObjImporter.h"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <unordered_map>

namespace vv {

namespace {

struct CornerKey {
    i32 v, t, n;
    bool operator==(const CornerKey& o) const {
        return v == o.v && t == o.t && n == o.n;
    }
};

struct CornerHash {
    size_t operator()(const CornerKey& k) const {
        // mistura barata (FNV-1a sobre os 3 i32)
        size_t h = 1469598103934665603ull;
        const i32 vals[3] = {k.v, k.t, k.n};
        for (const i32 x : vals) {
            h ^= static_cast<size_t>(static_cast<u32>(x));
            h *= 1099511628211ull;
        }
        return h;
    }
};

struct Parser {
    const char* pos;
    const char* end;
    u32 line = 1;

    void skipSpaces() {
        while (pos < end && *pos == ' ') ++pos;   // OBJ usa espaço simples
    }
    bool atEnd() const { return pos >= end; }
    char peek() const { return *pos; }
    void bumpLine() { ++line; }
};

// lê a palavra-chave da linha (até espaço ou newline)
bool readKeyword(Parser& p, std::string& kw) {
    kw.clear();
    while (!p.atEnd() && *p.pos != '\n' && *p.pos != ' ') {
        kw.push_back(*p.pos++);
    }
    return !kw.empty();
}

// lê o resto da linha como float (retorna false se não for número)
bool readFloat(Parser& p, f32& out) {
    p.skipSpaces();
    char* tail = nullptr;
    if (p.atEnd()) {
        return false;
    }
    const char* start = p.pos;
    // strtof sobre o restante da linha: o buffer pode não ter '\0' —
    // copia o token para um buffer pequeno
    char buf[64];
    size_t n = 0;
    while (!p.atEnd() && *p.pos != ' ' && *p.pos != '\n' && n + 1 < sizeof(buf)) {
        buf[n++] = *p.pos++;
    }
    buf[n] = '\0';
    out = std::strtof(buf, &tail);
    if (tail == buf || (tail && *tail != '\0')) {
        p.pos = start;   // reposiciona no início do token inválido
        return false;
    }
    return true;
}

// lê um índice de canto de face: inteiro com sinal (1-based; negativo relativo)
bool readCornerIndex(Parser& p, i32& out) {
    p.skipSpaces();
    if (p.atEnd() || *p.pos == '\n') {
        return false;
    }
    bool neg = false;
    if (*p.pos == '-') {
        neg = true;
        ++p.pos;
    } else if (*p.pos == '+') {
        ++p.pos;
    }
    if (p.atEnd() || *p.pos < '0' || *p.pos > '9') {
        return false;
    }
    i64 v = 0;
    while (!p.atEnd() && *p.pos >= '0' && *p.pos <= '9') {
        v = v * 10 + (*p.pos - '0');
        if (v > 100000000) {
            return false;   // absurdo — proteção de overflow
        }
        ++p.pos;
    }
    out = static_cast<i32>(neg ? -v : v);
    return true;
}

} // namespace

bool parseObj(const char* text, size_t len, MeshData& out, std::string& err) {
    out = MeshData{};
    if (!text || len == 0) {
        err = "obj vazio";
        return false;
    }

    // normaliza CRLF → LF (OBJs salvos no Windows são comuns; o parser
    // abaixo só entende '\n'). Sem '\r' no buffer, nada é copiado.
    std::string normalized;
    const char* parseText = text;
    size_t parseLen = len;
    if (std::memchr(text, '\r', len) != nullptr) {
        normalized.reserve(len);
        for (size_t i = 0; i < len; ++i) {
            if (text[i] != '\r') {
                normalized.push_back(text[i]);
            }
        }
        parseText = normalized.data();
        parseLen = normalized.size();
    }

    Parser p{parseText, parseText + parseLen};

    // fontes brutas do ficheiro
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> uvs;
    std::unordered_map<CornerKey, u16, CornerHash> cornerMap;

    std::string curGroup;       // "g"/"o" atual (default "")
    std::string curMaterial;    // "usemtl" atual (default "")
    bool groupOpen = false;     // já abrimos o grupo atual em out.groups?

    auto closeGroup = [&](u32 firstIndex) {
        if (groupOpen && !out.groups.empty()) {
            out.groups.back().indexCount = firstIndex - out.groups.back().firstIndex;
            groupOpen = false;
        }
    };
    auto openGroup = [&](u32 firstIndex) {
        MeshData::Group g;
        g.name = curGroup;
        g.material = curMaterial;
        g.firstIndex = firstIndex;
        g.indexCount = 0;
        out.groups.push_back(std::move(g));
        groupOpen = true;
    };

    auto resolveCorner = [&](i32 rawV, i32 rawT, i32 rawN, u16& outIdx) {
        auto fix = [&](i32 raw, size_t count) -> i64 {
            const i64 idx = raw;
            return idx > 0 ? idx - 1 : static_cast<i64>(count) + idx;
        };
        const i64 vi = fix(rawV, positions.size());
        if (vi < 0 || vi >= static_cast<i64>(positions.size())) {
            err = "indice de vertice fora do range";
            return false;
        }
        i64 ti = -1, ni = -1;
        if (rawT != 0) {
            ti = fix(rawT, uvs.size());
            if (ti < 0 || ti >= static_cast<i64>(uvs.size())) {
                err = "indice de uv fora do range";
                return false;
            }
        }
        if (rawN != 0) {
            ni = fix(rawN, normals.size());
            if (ni < 0 || ni >= static_cast<i64>(normals.size())) {
                err = "indice de normal fora do range";
                return false;
            }
        }
        const CornerKey key{static_cast<i32>(vi), static_cast<i32>(ti),
                            static_cast<i32>(ni)};
        const auto it = cornerMap.find(key);
        if (it != cornerMap.end()) {
            outIdx = it->second;
            return true;
        }
        // limite do engine: u16 — conta VÉRTICES ÚNICOS de saída (um ficheiro
        // pode ter 100k "v" mas se só 3 são usados, o mesh tem 3)
        if (out.vertices.size() >= 65536) {
            err = "mesh excede 65535 vertices (limite u16 do engine)";
            return false;
        }
        Vertex vtx;
        vtx.pos = positions[static_cast<size_t>(vi)];
        vtx.normal = ni >= 0 ? normals[static_cast<size_t>(ni)] : Vec3{0.0f, 1.0f, 0.0f};
        vtx.uv = ti >= 0 ? uvs[static_cast<size_t>(ti)] : Vec2{0.0f, 0.0f};
        outIdx = static_cast<u16>(out.vertices.size());
        out.vertices.push_back(vtx);
        cornerMap.emplace(key, outIdx);
        return true;
    };

    while (!p.atEnd()) {
        if (*p.pos == '\n') {
            ++p.pos;
            p.bumpLine();
            continue;
        }
        p.skipSpaces();
        if (p.atEnd()) {
            break;
        }
        if (*p.pos == '\n') {
            continue;
        }
        if (*p.pos == '#') {   // comentário: consome a linha
            while (!p.atEnd() && *p.pos != '\n') ++p.pos;
            continue;
        }

        std::string kw;
        if (!readKeyword(p, kw)) {
            ++p.pos;
            continue;
        }

        if (kw == "v") {
            Vec3 v;
            if (!readFloat(p, v.x) || !readFloat(p, v.y) || !readFloat(p, v.z)) {
                err = "v invalido na linha " + std::to_string(p.line);
                return false;
            }
            positions.push_back(v);
        } else if (kw == "vn") {
            Vec3 n;
            if (!readFloat(p, n.x) || !readFloat(p, n.y) || !readFloat(p, n.z)) {
                err = "vn invalido na linha " + std::to_string(p.line);
                return false;
            }
            normals.push_back(n);
        } else if (kw == "vt") {
            Vec2 t;
            f32 wIgnored = 0.0f;
            if (!readFloat(p, t.x) || !readFloat(p, t.y)) {
                err = "vt invalido na linha " + std::to_string(p.line);
                return false;
            }
            readFloat(p, wIgnored);   // vt pode ter w — ignorado
            uvs.push_back(t);
        } else if (kw == "f") {
            // canto = v[/vt][/vn]; consome o token inteiro do canto
            i32 cornerV[64];
            i32 cornerT[64];
            i32 cornerN[64];
            int nCorners = 0;
            while (true) {
                p.skipSpaces();
                if (p.atEnd() || *p.pos == '\n' || nCorners >= 64) {
                    break;
                }
                if (!readCornerIndex(p, cornerV[nCorners])) {
                    err = "face invalida na linha " + std::to_string(p.line);
                    return false;
                }
                cornerT[nCorners] = 0;
                cornerN[nCorners] = 0;
                if (!p.atEnd() && *p.pos == '/') {
                    ++p.pos;
                    if (!p.atEnd() && *p.pos != '/') {
                        if (!readCornerIndex(p, cornerT[nCorners])) {
                            err = "vt da face invalido na linha " + std::to_string(p.line);
                            return false;
                        }
                    }
                    if (!p.atEnd() && *p.pos == '/') {
                        ++p.pos;
                        if (!readCornerIndex(p, cornerN[nCorners])) {
                            err = "vn da face invalido na linha " + std::to_string(p.line);
                            return false;
                        }
                    }
                }
                ++nCorners;
            }
            if (nCorners < 3) {
                err = "face com menos de 3 cantos na linha " + std::to_string(p.line);
                return false;
            }
            if (!groupOpen) {
                openGroup(static_cast<u32>(out.indices.size()));
            }
            // fan: (0, i, i+1) — preserva o winding do ficheiro
            u16 idx0, idxA, idxB;
            if (!resolveCorner(cornerV[0], cornerT[0], cornerN[0], idx0)) {
                return false;
            }
            for (int i = 1; i + 1 < nCorners; ++i) {
                if (!resolveCorner(cornerV[i], cornerT[i], cornerN[i], idxA) ||
                    !resolveCorner(cornerV[i + 1], cornerT[i + 1], cornerN[i + 1], idxB)) {
                    return false;
                }
                out.indices.push_back(idx0);
                out.indices.push_back(idxA);
                out.indices.push_back(idxB);
            }
        } else if (kw == "g" || kw == "o") {
            // novo grupo: fecha o atual e troca o nome
            closeGroup(static_cast<u32>(out.indices.size()));
            p.skipSpaces();   // keyword reader para no espaço após "g"/"o"
            std::string name;
            while (!p.atEnd() && *p.pos != '\n') {
                name.push_back(*p.pos++);
            }
            while (!name.empty() && name.back() == ' ') name.pop_back();
            curGroup = name;
        } else if (kw == "usemtl") {
            closeGroup(static_cast<u32>(out.indices.size()));
            p.skipSpaces();   // keyword reader para no espaço após "usemtl"
            std::string name;
            while (!p.atEnd() && *p.pos != '\n') {
                name.push_back(*p.pos++);
            }
            while (!name.empty() && name.back() == ' ') name.pop_back();
            curMaterial = name;
        } else {
            // mtllib / s / outros: consome a linha (reconhecidos, ignorados)
            while (!p.atEnd() && *p.pos != '\n') ++p.pos;
        }
        // consome o fim da linha (keyword readers param antes do '\n')
        if (!p.atEnd() && *p.pos == '\n') {
            ++p.pos;
            p.bumpLine();
        }
    }
    closeGroup(static_cast<u32>(out.indices.size()));

    if (out.vertices.empty() || out.indices.empty()) {
        err = "obj sem triangulos";
        return false;
    }
    return true;
}

} // namespace vv
