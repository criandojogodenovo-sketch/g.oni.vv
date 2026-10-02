// assets/ObjImporter.cpp — parser Wavefront OBJ (F5-B; 0.8.10: STREAMING).
//
// 0.8.10: o corpo do parser vive no ObjStreamParser (linha-a-linha, RAM de
// pico = arrays + mesh); parseObj(texto) alimenta-o — um SÓ caminho de
// parse para o wrapper em memória e para o import de 500 MB por chunks.
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

// cursor DENTRO de UMA linha (sem '\n' — o streaming alimenta linhas)
struct LineParser {
    const char* pos;
    const char* end;
    void skipSpaces() {
        while (pos < end && *pos == ' ') ++pos;   // OBJ usa espaço simples
    }
    bool atEnd() const { return pos >= end; }
};

// lê a palavra-chave da linha (até espaço)
bool readKeyword(LineParser& p, std::string& kw) {
    kw.clear();
    while (!p.atEnd() && *p.pos != ' ') {
        kw.push_back(*p.pos++);
    }
    return !kw.empty();
}

// lê um float da linha
bool readFloat(LineParser& p, f32& out) {
    p.skipSpaces();
    if (p.atEnd()) {
        return false;
    }
    char buf[64];
    size_t n = 0;
    while (!p.atEnd() && *p.pos != ' ' && n + 1 < sizeof(buf)) {
        buf[n++] = *p.pos++;
    }
    buf[n] = '\0';
    char* tail = nullptr;
    out = std::strtof(buf, &tail);
    return tail != buf && (tail == nullptr || *tail == '\0');
}

// lê um índice de canto de face: inteiro com sinal (1-based; negativo relativo)
bool readCornerIndex(LineParser& p, i32& out) {
    p.skipSpaces();
    if (p.atEnd()) {
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

// ---- impl opaco (hash de cantos) ---------------------------------------------
struct ObjStreamParser::Impl {
    std::unordered_map<CornerKey, u16, CornerHash> cornerMap;
};

ObjStreamParser::ObjStreamParser() = default;
ObjStreamParser::~ObjStreamParser() { destroyImpl(); }
bool ObjStreamParser::ensureImpl() {
    if (!impl_) {
        impl_ = new Impl();
    }
    return impl_ != nullptr;
}
void ObjStreamParser::destroyImpl() {
    delete impl_;
    impl_ = nullptr;
}

bool ObjStreamParser::feedLine(const char* s, size_t len, std::string& err) {
    if (!s || len == 0) {
        ++lines_;
        return true;   // linha vazia — legal no OBJ
    }
    ++lines_;
    if (!ensureImpl()) {
        err = "sem memoria para o parser";
        return false;
    }
    LineParser p{s, s + len};
    p.skipSpaces();
    if (p.atEnd() || *p.pos == '#') {
        return true;   // vazia/comentário
    }
    std::string kw;
    if (!readKeyword(p, kw)) {
        return true;
    }
    if (kw == "v") {
        Vec3 v;
        if (!readFloat(p, v.x) || !readFloat(p, v.y) || !readFloat(p, v.z)) {
            err = "v invalido na linha " + std::to_string(lines_);
            return false;
        }
        positions_.push_back(v);
    } else if (kw == "vn") {
        Vec3 n;
        if (!readFloat(p, n.x) || !readFloat(p, n.y) || !readFloat(p, n.z)) {
            err = "vn invalido na linha " + std::to_string(lines_);
            return false;
        }
        normals_.push_back(n);
    } else if (kw == "vt") {
        Vec2 t;
        if (!readFloat(p, t.x) || !readFloat(p, t.y)) {
            err = "vt invalido na linha " + std::to_string(lines_);
            return false;
        }
        f32 wIgnored = 0.0f;
        readFloat(p, wIgnored);   // vt pode ter w — ignorado
        uvs_.push_back(t);
    } else if (kw == "f") {
        i32 cornerV[64];
        i32 cornerT[64];
        i32 cornerN[64];
        int nCorners = 0;
        while (true) {
            p.skipSpaces();
            if (p.atEnd() || nCorners >= 64) {
                break;
            }
            if (!readCornerIndex(p, cornerV[nCorners])) {
                err = "face invalida na linha " + std::to_string(lines_);
                return false;
            }
            cornerT[nCorners] = 0;
            cornerN[nCorners] = 0;
            if (!p.atEnd() && *p.pos == '/') {
                ++p.pos;
                if (!p.atEnd() && *p.pos != '/') {
                    if (!readCornerIndex(p, cornerT[nCorners])) {
                        err = "vt da face invalido na linha " +
                              std::to_string(lines_);
                        return false;
                    }
                }
                if (!p.atEnd() && *p.pos == '/') {
                    ++p.pos;
                    if (!readCornerIndex(p, cornerN[nCorners])) {
                        err = "vn da face invalido na linha " +
                              std::to_string(lines_);
                        return false;
                    }
                }
            }
            ++nCorners;
        }
        if (nCorners < 3) {
            err = "face com menos de 3 cantos na linha " + std::to_string(lines_);
            return false;
        }
        if (!groupOpen_) {
            MeshData::Group g;
            g.name = curGroup_;
            g.material = curMaterial_;
            g.firstIndex = static_cast<u32>(indices_.size());
            g.indexCount = 0;
            groups_.push_back(std::move(g));
            groupOpen_ = true;
        }
        // resolve+dedup um canto (v,vt,vn) → índice u16 do vértice de saída
        auto resolveCorner = [&](i32 rawV, i32 rawT, i32 rawN,
                                 u16& outIdx) -> bool {
            auto fix = [&](i32 raw, size_t count) -> i64 {
                const i64 idx = raw;
                return idx > 0 ? idx - 1 : static_cast<i64>(count) + idx;
            };
            const i64 vi = fix(rawV, positions_.size());
            if (vi < 0 || vi >= static_cast<i64>(positions_.size())) {
                err = "indice de vertice fora do range (linha " +
                      std::to_string(lines_) + ")";
                return false;
            }
            i64 ti = -1, ni = -1;
            if (rawT != 0) {
                ti = fix(rawT, uvs_.size());
                if (ti < 0 || ti >= static_cast<i64>(uvs_.size())) {
                    err = "indice de uv fora do range (linha " +
                          std::to_string(lines_) + ")";
                    return false;
                }
            }
            if (rawN != 0) {
                ni = fix(rawN, normals_.size());
                if (ni < 0 || ni >= static_cast<i64>(normals_.size())) {
                    err = "indice de normal fora do range (linha " +
                          std::to_string(lines_) + ")";
                    return false;
                }
            }
            const CornerKey key{static_cast<i32>(vi), static_cast<i32>(ti),
                                static_cast<i32>(ni)};
            const auto it = impl_->cornerMap.find(key);
            if (it != impl_->cornerMap.end()) {
                outIdx = it->second;
                return true;
            }
            // limite do engine: u16 — conta VÉRTICES ÚNICOS de saída (um
            // ficheiro pode ter 100k "v" mas se só 3 são usados, o mesh
            // tem 3)
            if (vertices_.size() >= 65536) {
                err = "mesh excede 65535 vertices (limite u16 do engine)";
                return false;
            }
            Vertex vtx;
            vtx.pos = positions_[static_cast<size_t>(vi)];
            vtx.normal = ni >= 0 ? normals_[static_cast<size_t>(ni)]
                                 : Vec3{0.0f, 1.0f, 0.0f};
            vtx.uv = ti >= 0 ? uvs_[static_cast<size_t>(ti)] : Vec2{0.0f, 0.0f};
            outIdx = static_cast<u16>(vertices_.size());
            vertices_.push_back(vtx);
            impl_->cornerMap.emplace(key, outIdx);
            return true;
        };
        // fan: (0, i, i+1) — preserva o winding do ficheiro
        u16 idx0, idxA, idxB;
        if (!resolveCorner(cornerV[0], cornerT[0], cornerN[0], idx0)) {
            return false;
        }
        for (int i = 1; i + 1 < nCorners; ++i) {
            if (!resolveCorner(cornerV[i], cornerT[i], cornerN[i], idxA) ||
                !resolveCorner(cornerV[i + 1], cornerT[i + 1],
                               cornerN[i + 1], idxB)) {
                return false;
            }
            indices_.push_back(idx0);
            indices_.push_back(idxA);
            indices_.push_back(idxB);
        }
    } else if (kw == "g" || kw == "o") {
        // novo grupo: fecha o atual e troca o nome
        if (groupOpen_ && !groups_.empty()) {
            groups_.back().indexCount =
                static_cast<u32>(indices_.size()) - groups_.back().firstIndex;
            groupOpen_ = false;
        }
        p.skipSpaces();
        std::string name;
        while (!p.atEnd()) {
            name.push_back(*p.pos++);
        }
        while (!name.empty() && name.back() == ' ') name.pop_back();
        curGroup_ = name;
    } else if (kw == "usemtl") {
        if (groupOpen_ && !groups_.empty()) {
            groups_.back().indexCount =
                static_cast<u32>(indices_.size()) - groups_.back().firstIndex;
            groupOpen_ = false;
        }
        p.skipSpaces();
        std::string name;
        while (!p.atEnd()) {
            name.push_back(*p.pos++);
        }
        while (!name.empty() && name.back() == ' ') name.pop_back();
        curMaterial_ = name;
    }
    // mtllib / s / outros: reconhecidos, ignorados
    return true;
}

bool ObjStreamParser::finish(MeshData& out, std::string& err) {
    if (groupOpen_ && !groups_.empty()) {
        groups_.back().indexCount =
            static_cast<u32>(indices_.size()) - groups_.back().firstIndex;
        groupOpen_ = false;
    }
    if (vertices_.empty() || indices_.empty()) {
        err = "obj sem triangulos";
        return false;
    }
    out = MeshData{};
    out.vertices = std::move(vertices_);
    out.indices = std::move(indices_);
    out.groups = std::move(groups_);
    return true;
}

bool parseObj(const char* text, size_t len, MeshData& out, std::string& err) {
    out = MeshData{};
    if (!text || len == 0) {
        err = "obj vazio";
        return false;
    }
    // normaliza CRLF → LF (OBJs salvos no Windows são comuns; o assembler
    // de linhas só entende '\n')
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
    // 0.8.10: o MESMO caminho do import streaming — linha-a-linha
    ObjStreamParser parser;
    const char* p = parseText;
    const char* const end = parseText + parseLen;
    while (p < end) {
        const char* nl = static_cast<const char*>(
            std::memchr(p, '\n', static_cast<size_t>(end - p)));
        const char* lineEnd = nl ? nl : end;
        if (!parser.feedLine(p, static_cast<size_t>(lineEnd - p), err)) {
            return false;
        }
        p = nl ? nl + 1 : end;
    }
    return parser.finish(out, err);
}

} // namespace vv
