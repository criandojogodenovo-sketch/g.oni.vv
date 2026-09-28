#pragma once
// core/Json.h — JSON mínimo do engine (F3, zero dependências).
//
// Escrito para o formato de cena .goni, mas é um parser JSON geral pequeno:
// null/bool/number/string/array/object, escapes padrão (\n \" \\ \t \r \b \f
// e \uXXXX → UTF-8), profundidade máxima 64 (anti-bomba).
//
// Objeto preserva a ORDEM dos membros (vector de pares) — o dump volta
// determinístico, o que mantém os round-trips dos testes estáveis.
//
// GL-free / Android-free: compila e roda na suíte do CI Linux.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>
#include "core/Types.h"

namespace vv {

class Json {
public:
    enum class Type : u8 { Null, Bool, Number, String, Array, Object };

    Type type = Type::Null;

    bool        boolean = false;
    double      number  = 0.0;
    std::string string;
    std::vector<Json>                        items;     // Array
    std::vector<std::pair<std::string, Json>> members;  // Object (ordem preservada)

    static Json makeNumber(double v) {
        Json j;
        j.type = Type::Number;
        j.number = v;
        return j;
    }
    static Json makeString(const std::string& s) {
        Json j;
        j.type = Type::String;
        j.string = s;
        return j;
    }
    static Json makeBool(bool b) {
        Json j;
        j.type = Type::Bool;
        j.boolean = b;
        return j;
    }
    static Json makeArray() {
        Json j;
        j.type = Type::Array;
        return j;
    }
    static Json makeObject() {
        Json j;
        j.type = Type::Object;
        return j;
    }

    void addMember(const std::string& key, Json value) {
        members.emplace_back(key, std::move(value));
    }
    void addItem(Json value) { items.push_back(std::move(value)); }

    // Primeiro membro com a chave (Object); nullptr se não existir.
    const Json* find(const char* key) const {
        if (type != Type::Object) {
            return nullptr;
        }
        for (const auto& kv : members) {
            if (kv.first == key) {
                return &kv.second;
            }
        }
        return nullptr;
    }

    // Versão mutável (migrações alteram/insère campos no documento).
    Json* find(const char* key) {
        if (type != Type::Object) {
            return nullptr;
        }
        for (auto& kv : members) {
            if (kv.first == key) {
                return &kv.second;
            }
        }
        return nullptr;
    }

    // ---- parser -------------------------------------------------------------
    // false se inválido (texto truncado, token inesperado, profundidade > 64).
    static bool parse(const char* text, size_t len, Json& out) {
        Parser p{text, text + len};
        p.skipWs();
        if (!p.parseValue(out, 0)) {
            return false;
        }
        p.skipWs();
        return p.pos >= p.end;   // tem de consumir tudo
    }

    // ---- dump ---------------------------------------------------------------
    // Números com %.9g (garante round-trip exato de f32 → double → texto).
    std::string dump() const {
        std::string s;
        dumpTo(s);
        return s;
    }

private:
    struct Parser {
        const char* pos;
        const char* end;

        void skipWs() {
            while (pos < end && (*pos == ' ' || *pos == '\t' || *pos == '\n' || *pos == '\r')) {
                ++pos;
            }
        }
        bool eat(char c) {
            if (pos < end && *pos == c) {
                ++pos;
                return true;
            }
            return false;
        }
        bool parseValue(Json& out, int depth) {
            if (depth > 64) {
                return false;
            }
            skipWs();
            if (pos >= end) {
                return false;
            }
            switch (*pos) {
                case '{': return parseObject(out, depth);
                case '[': return parseArray(out, depth);
                case '"': out.type = Type::String; return parseString(out.string);
                case 't':
                    out = makeBool(true);
                    return literal("true");
                case 'f':
                    out = makeBool(false);
                    return literal("false");
                case 'n':
                    out.type = Type::Null;
                    return literal("null");
                default:  return parseNumber(out);
            }
        }
        bool literal(const char* lit) {
            for (const char* p = lit; *p; ++p, ++pos) {
                if (pos >= end || *pos != *p) {
                    return false;
                }
            }
            return true;
        }
        bool parseNumber(Json& out) {
            const char* start = pos;
            if (pos < end && (*pos == '-' || *pos == '+')) ++pos;
            while (pos < end && ((*pos >= '0' && *pos <= '9') || *pos == '.' ||
                                 *pos == 'e' || *pos == 'E' || *pos == '-' || *pos == '+')) {
                ++pos;
            }
            if (pos == start) {
                return false;
            }
            char buf[64];
            const size_t n = static_cast<size_t>(pos - start);
            if (n >= sizeof(buf)) {
                return false;
            }
            std::memcpy(buf, start, n);
            buf[n] = '\0';
            char* tail = nullptr;
            out = makeNumber(std::strtod(buf, &tail));
            return tail && *tail == '\0';
        }
        bool parseString(std::string& out) {
            if (!eat('"')) {
                return false;
            }
            out.clear();
            while (pos < end) {
                const char c = *pos++;
                if (c == '"') {
                    return true;
                }
                if (c != '\\') {
                    out.push_back(c);
                    continue;
                }
                if (pos >= end) {
                    return false;
                }
                const char esc = *pos++;
                switch (esc) {
                    case '"':  out.push_back('"');  break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/');  break;
                    case 'n':  out.push_back('\n'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'u': {
                        // \uXXXX → UTF-8 (plano multilingue básico; pares
                        // sobrogem não são necessários no .goni ASCII)
                        unsigned cp = 0;
                        for (int i = 0; i < 4; ++i) {
                            if (pos >= end) {
                                return false;
                            }
                            const char h = *pos++;
                            cp <<= 4;
                            if (h >= '0' && h <= '9')      cp |= static_cast<unsigned>(h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                            else return false;
                        }
                        if (cp < 0x80) {
                            out.push_back(static_cast<char>(cp));
                        } else if (cp < 0x800) {
                            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
                            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        } else {
                            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
                            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
                            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
                        }
                        break;
                    }
                    default: return false;
                }
            }
            return false;   // string sem fecho
        }
        bool parseArray(Json& out, int depth) {
            if (!eat('[')) {
                return false;
            }
            out = makeArray();
            skipWs();
            if (eat(']')) {
                return true;
            }
            while (true) {
                Json v;
                if (!parseValue(v, depth + 1)) {
                    return false;
                }
                out.items.push_back(std::move(v));
                skipWs();
                if (eat(',')) {
                    continue;
                }
                return eat(']');
            }
        }
        bool parseObject(Json& out, int depth) {
            if (!eat('{')) {
                return false;
            }
            out = makeObject();
            skipWs();
            if (eat('}')) {
                return true;
            }
            while (true) {
                skipWs();
                std::string key;
                if (!parseString(key)) {
                    return false;
                }
                skipWs();
                if (!eat(':')) {
                    return false;
                }
                Json v;
                if (!parseValue(v, depth + 1)) {
                    return false;
                }
                out.members.emplace_back(std::move(key), std::move(v));
                skipWs();
                if (eat(',')) {
                    continue;
                }
                return eat('}');
            }
        }
    };

    void dumpTo(std::string& s) const {
        switch (type) {
            case Type::Null:   s += "null"; break;
            case Type::Bool:   s += boolean ? "true" : "false"; break;
            case Type::Number: {
                char buf[32];
                std::snprintf(buf, sizeof(buf), "%.9g", number);
                s += buf;
                break;
            }
            case Type::String: dumpString(string, s); break;
            case Type::Array: {
                s += '[';
                for (size_t i = 0; i < items.size(); ++i) {
                    if (i) s += ',';
                    items[i].dumpTo(s);
                }
                s += ']';
                break;
            }
            case Type::Object: {
                s += '{';
                for (size_t i = 0; i < members.size(); ++i) {
                    if (i) s += ',';
                    dumpString(members[i].first, s);
                    s += ':';
                    members[i].second.dumpTo(s);
                }
                s += '}';
                break;
            }
        }
    }

    static void dumpString(const std::string& str, std::string& s) {
        s += '"';
        for (const char c : str) {
            switch (c) {
                case '"':  s += "\\\""; break;
                case '\\': s += "\\\\"; break;
                case '\n': s += "\\n";  break;
                case '\r': s += "\\r";  break;
                case '\t': s += "\\t";  break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                        s += buf;
                    } else {
                        s += c;
                    }
            }
        }
        s += '"';
    }
};

} // namespace vv
