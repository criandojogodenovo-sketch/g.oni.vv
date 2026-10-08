// assets/GltfImporter.cpp — parser glTF 2.0 (F5-C).
//
// Camadas: container GLB → JSON (core/Json) → buffers → accessors →
// primitivas → MeshData + materiais + nós. Toda leitura de memória é
// bounds-checked (byteOffset+byteLength dentro do buffer) — ficheiro
// malicioso/truncado falha com erro, nunca lê fora.
#include "assets/GltfImporter.h"

#include "platform/EngineLog.h"   // 0.9.6.3: o log das extensões usadas
#include "core/Json.h"
#include <cstring>

namespace vv {

namespace {

constexpr u32 kMagicGltf  = 0x46546C67u;   // 'glTF'
constexpr u32 kChunkJson  = 0x4E4F534Au;   // 'JSON'
constexpr u32 kChunkBin   = 0x004E4942u;   // 'BIN'

// ---- base64 -----------------------------------------------------------------
bool base64Decode(const char* src, size_t len, std::vector<u8>& out) {
    out.clear();
    auto val = [](char c) -> i32 {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    u32 acc = 0;
    int bits = 0;
    for (size_t i = 0; i < len; ++i) {
        const char c = src[i];
        if (c == '=' || c == '\n' || c == '\r' || c == ' ') {
            continue;   // padding/whitespace
        }
        const i32 v = val(c);
        if (v < 0) {
            return false;
        }
        acc = (acc << 6) | static_cast<u32>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<u8>((acc >> bits) & 0xFFu));
        }
    }
    return true;
}

// ---- leitura bounds-checked de um bufferView -------------------------------
struct ViewSpan {
    const u8* data = nullptr;
    size_t byteLength = 0;
    size_t stride = 0;   // 0 = compacto
    // 0.9.6.12g (A2-2): true = os bytes vivem no STAGING do store (buffer
    // deferido) — o chamador pode ADOTAR o buffer (takeStaged) em vez de o
    // copiar: o range grande nunca existe DUAS vezes em RAM (FAZ 5)
    bool staged = false;
};

// 0.9.6.4 (GRUPO A/R-022) — A CAUSA da falha, distinguível: «view fora dos
// limites do buffer» (ficheiro malformado) NÃO é a mesma coisa que «a
// leitura do range falhou» (I/O) — antes o mesmo «fora do buffer» enganava
// as duas. UMA SÓ rotina (resolveView) serve meshes (fatal) e imagens
// (warn + skip — a geometria não morre por uma textura).
enum class ViewFail {
    None = 0,
    NoBuffer,      // buffer do view inexistente/inválido
    BadLength,     // sem byteLength
    OutOfBounds,   // offset+len > tamanho REAL do buffer
    BeyondDeclared,// offset+len > o byteLength DECLARADO (A2-2: a regra do
                   // dono — o view tem de caber em AMBOS os tetos)
    TooBig,        // range acima do teto kMaxRangeBytes (A2-2: a mensagem
                   // passa a ser «modelo demasiado grande para a memória»)
    ReadFail,      // a leitura do range falhou (I/O)
    BadStride,     // stride menor que o elemento
};

// 0.8.10 — STORE de buffers: residentes (base64/externo) OU DEFERIDOS (o
// BIN chunk de um .glb EM FICHEIRO: ranges materializados POR DEMANDA —
// o import de 500 MB nunca carrega o BIN).
// 0.9.6.12g (A2-2): kMaxRangeBytes vive AGORA no GltfImporter.h (256 MB) —
// UMA constante partilhada com o fileRangeLoad (o teto de 64 MB daqui
// recusava o view VÁLIDO de 118 MB do scene do dono, e o teto de 16 MB
// REUSADO do kMaxJsonBytes no loader matava o dragão com mentira de I/O).

class GltfBufferStore {
public:
    void addOwned(std::vector<u8>&& b) {
        Entry e;
        e.owned = std::move(b);
        e.deferredLen = e.owned.size();
        entries_.push_back(std::move(e));
    }
    void addDeferred(u64 totalLen) {
        Entry e;
        e.deferredLen = totalLen;
        e.deferred = true;
        entries_.push_back(std::move(e));
    }
    void setRangeLoader(const GltfRangeLoader& l) { loader_ = l; }
    // 0.9.6.12 (A2): os byteLength DECLARADOS do JSON (a comparação do
    // log do dono pede declared vs real lado a lado)
    void setDeclaredLens(std::vector<u64>&& d) { declared_ = std::move(d); }
    u64 declared(size_t bi) const {
        return bi < declared_.size() ? declared_[bi] : 0;
    }

    size_t count() const { return entries_.size(); }
    u64 size(size_t bi) const {
        return bi < entries_.size() ? entries_[bi].deferredLen : 0;
    }

    // ponteiro ESTÁVEL para [off, off+len) — materializa se for deferred.
    // 0.9.6.12g (A2-2 · FAZ 5): o pool que SÓ CRESCIA (um deque com TODOS
    // os ranges materializados até ao fim do parse — o scene.bin de 212 MB
    // ia chegando à RAM inteira, view a view) é agora UM STAGING
    // reutilizável: o pico de RAM é UM range. `*staged` diz ao chamador
    // que pode ADOTAR os bytes (takeStaged) em vez de os duplicar.
    const u8* span(size_t bi, size_t off, size_t len, bool* staged = nullptr) {
        if (staged) {
            *staged = false;
        }
        if (bi >= entries_.size() || len == 0) {
            return nullptr;
        }
        Entry& e = entries_[bi];
        if (!e.deferred) {
            if (off + len > e.owned.size()) {
                return nullptr;
            }
            return e.owned.data() + off;   // residente — sem cópia
        }
        if (off + len > e.deferredLen) {
            return nullptr;
        }
        if (len > kMaxRangeBytes) {
            return nullptr;   // acima do teto honesto — o resolveView NOMEIA
        }
        if (!loader_.fn) {
            return nullptr;
        }
        stage_.clear();
        stage_.resize(static_cast<size_t>(len));
        if (!loader_.fn(loader_.user, static_cast<u32>(bi),
                        static_cast<u64>(off), static_cast<u64>(len),
                        stage_) ||
            stage_.size() != len) {
            // o valor DEVOLVIDO pela leitura é afervado: um range curto
            // (ficheiro truncado no último bloco) é falha, nunca silêncio
            stage_.clear();
            stage_.shrink_to_fit();
            return nullptr;
        }
        if (staged) {
            *staged = true;
        }
        return stage_.data();
    }

    // 0.9.6.12g (A2-2): ADOTA o staging (só válido após um span() deferred
    // bem-sucedido e antes do span() seguinte) — o accessor COMPACTO move
    // os bytes em vez de os copiar (o range de 118 MB nunca é duplicado)
    std::vector<u8> takeStaged() { return std::move(stage_); }

    // 0.9.6.12g (A2-2 · FAZ 1 do dono): o tamanho do FICHEIRO em disco por
    // buffer — deferido: o stat do ficheiro (loader.fileBytes); residente:
    // os bytes que existem (real == file por construção)
    u64 diskBytes(size_t bi) const {
        if (bi >= entries_.size()) {
            return 0;
        }
        return entries_[bi].deferred ? loader_.fileBytes
                                     : entries_[bi].owned.size();
    }

private:
    struct Entry {
        std::vector<u8> owned;   // residente
        u64 deferredLen = 0;
        bool deferred = false;
    };
    std::vector<Entry> entries_;
    std::vector<u64> declared_;   // buffers[].byteLength do JSON (A2)
    GltfRangeLoader loader_{};
    std::vector<u8> stage_;   // O staging único (A2-2: pico de RAM = UM range)
};

// 0.9.6.4 (R-022): devolve a CAUSA (ViewFail) em vez de bool — o chamador
// decide o texto e a gravidade. A validação usa o TAMANHO REAL do buffer
// (GLB: o comprimento REAL do chunk BIN do header; externo/data: os bytes
// que existem) — buffers[].byteLength declarado NÃO é a autoridade.
//
// 0.9.6.12 (A2 · R-014) — O DIAGNÓSTICO COMPLETO (a spec tarefa 1): a
// falha carrega a COMPARAÇÃO INTEIRA — view/buffer/off/len/declared/real —
// e os chamadores LOGAM a linha exata «glb: view<i> buffer<b> off=<o>
// len=<l> declared=<d> real=<r>» (NUNCA truncada — é o que o dono lê no
// device). E O FIX DA CAUSA RAIZ do log 10-06 do dono (high_poly):
// o bound VAVA o accessorByteOffset DUAS VEZES — total = off + accOff +
// bLen ultrapassa o buffer por accOff quando o view acaba no FIM do BIN
// (views empacotadas estilo gltfpack, accessor com byteOffset > 0): um
// GLB 100% válido era recusado com «fora do buffer». O bound correto é o
// do VIEW (off + bLen ≤ real); o accessor dentro do view é afervado
// depois por «accessor excede o bufferView» (accOff + count*elem ≤ bLen).
struct ViewDiag {
    size_t bufferIdx = 0;
    size_t viewOff = 0;
    size_t viewLen = 0;
    size_t accOff = 0;
    u64 declared = 0;   // buffers[b].byteLength do JSON
    u64 real = 0;       // o tamanho REAL usado na validação
    u64 fileBytes = 0;  // o ficheiro EM DISCO (A2-2 · FAZ 1: «file=» — a
                        // linha completa do dono, sem «…»)
};
ViewFail resolveView(GltfBufferStore& store, const Json& bv,
                     size_t accessorByteOffset, size_t elemSize, ViewSpan& out,
                     ViewDiag* diag = nullptr) {
    ViewDiag local;
    ViewDiag& dg = diag ? *diag : local;
    const Json* jbuf = bv.find("buffer");
    if (!jbuf || jbuf->type != Json::Type::Number) {
        return ViewFail::NoBuffer;
    }
    const size_t bi = static_cast<size_t>(jbuf->number);
    size_t off = 0;
    if (const Json* o = bv.find("byteOffset"); o && o->type == Json::Type::Number) {
        off = static_cast<size_t>(o->number);
    }
    size_t bLen = 0;
    if (const Json* l = bv.find("byteLength"); l && l->type == Json::Type::Number) {
        bLen = static_cast<size_t>(l->number);
    }
    // o diag leva os QUATRO números MESMO no NoBuffer (a spec: o erro
    // nomeia o buffer — e a comparação completa aparece junto)
    dg.bufferIdx = bi;
    dg.viewOff = off;
    dg.viewLen = bLen;
    dg.accOff = accessorByteOffset;
    dg.declared = store.declared(bi);
    dg.real = bi < store.count() ? store.size(bi) : 0;
    dg.fileBytes = store.diskBytes(bi);   // A2-2 · FAZ 1: o disco, no diag
    if (bi >= store.count()) {
        return ViewFail::NoBuffer;
    }
    const u64 bufSize = store.size(bi);
    if (bLen == 0 && !bv.find("byteLength")) {
        return ViewFail::BadLength;
    }
    if (accessorByteOffset > bLen) {
        return ViewFail::OutOfBounds;
    }
    // stride do VIEW (atributos de vértice podem estar interleaved)
    size_t stride = 0;
    if (const Json* s = bv.find("byteStride"); s && s->type == Json::Type::Number) {
        stride = static_cast<size_t>(s->number);
    }
    if (stride != 0 && stride < elemSize) {
        return ViewFail::BadStride;   // stride menor que o elemento é ilegal
    }
    if (stride == 0) {
        stride = elemSize;
    }
    // 0.9.6.12 (A2): O BOUND CORRETO — o view inteiro dentro do buffer
    // (era off + accOff + bLen: dupla-contava o accessorByteOffset —
    // GLB válido com view no fim do BIN + accessor com byteOffset > 0
    // era recusado; a causa raiz do defeito 1 do dono)
    // 0.9.6.12g (A2-2): o limite é EXCLUSIVO (off+len == fim do buffer é
    // VÁLIDO — a evidência do dono: view2 a acabar em declared=212986848
    // nunca foi «fora do buffer»; o que o matava eram os TETOS de range,
    // abaixo) e o view tem de caber TAMBÉM no byteLength DECLARADO (a
    // regra do dono: off+len ≤ real E ≤ declared — um exporter que
    // declara menos do que o chunk tem é mentira distinguida AQUI)
    const size_t total = off + bLen;
    if (static_cast<u64>(total) > bufSize) {
        return ViewFail::OutOfBounds;   // view fora do buffer — recusa
    }
    if (dg.declared > 0 && static_cast<u64>(total) > dg.declared) {
        return ViewFail::BeyondDeclared;
    }
    out.byteLength = bLen - accessorByteOffset;
    out.stride = stride;
    // 0.8.10: materializa SÓ o range necessário (deferred = streaming);
    // 0.9.6.4: a falha da LEITURA é ReadFail — nunca «fora do buffer»;
    // 0.9.6.12g (A2-2): acima do teto a falha é TooBig com a mensagem
    // HONESTA de memória (o ficheiro é VÁLIDO — não cabe é que não cabe)
    if (out.byteLength > kMaxRangeBytes) {
        return ViewFail::TooBig;
    }
    out.data = store.span(bi, off + accessorByteOffset, out.byteLength,
                          &out.staged);
    if (!out.data) {
        return ViewFail::ReadFail;
    }
    return ViewFail::None;
}

const char* viewFailText(ViewFail f) {
    switch (f) {
        case ViewFail::None:       return "";
        case ViewFail::NoBuffer:   return "buffer do bufferView inexistente";
        case ViewFail::BadLength:  return "bufferView sem byteLength";
        case ViewFail::OutOfBounds:return "bufferView fora do buffer (offset+len > tamanho real)";
        case ViewFail::BeyondDeclared:
            return "bufferView acima do byteLength declarado do buffer";
        case ViewFail::TooBig:
            // 0.9.6.12g (A2-2 · FAZ 5): a mensagem HONESTA de memória —
            // antes dizia «acima do teto de 64 MB» (e «ficheiro corrompido?»
            // no erro final) para um ficheiro PERFEITAMENTE VÁLIDO
            return "modelo demasiado grande para a memória (o range passa "
                   "o teto de 256 MB)";
        case ViewFail::ReadFail:   return "leitura do range falhou (I/O)";
        case ViewFail::BadStride:  return "byteStride menor que o elemento";
    }
    return "?";
}

size_t componentSize(i32 ct) {
    switch (ct) {
        case 5121: return 1;   // u8
        case 5123: return 2;   // u16
        case 5125: return 4;   // u32
        case 5126: return 4;   // f32
        default:   return 0;
    }
}

size_t componentsOf(const std::string& type) {
    if (type == "SCALAR") return 1;
    if (type == "VEC2")   return 2;
    if (type == "VEC3")   return 3;
    if (type == "VEC4")   return 4;
    if (type == "MAT4")   return 16;   // 0.8.2 (F7): inverseBindMatrices
    return 0;
}

f32 readF32(const u8* p) {
    f32 v;
    std::memcpy(&v, p, 4);
    return v;
}

u32 readU32(const u8* p, i32 ct) {
    switch (ct) {
        case 5121: return *p;
        case 5123: { u16 v; std::memcpy(&v, p, 2); return v; }
        case 5125: { u32 v; std::memcpy(&v, p, 4); return v; }
        default:   return 0;
    }
}

} // namespace

bool decodeBase64(const char* src, size_t len, std::vector<u8>& out) {
    return base64Decode(src, len, out);
}

bool parseGltf(const char* json, size_t len, const std::vector<u8>& bin,
               const GltfBufferResolver& resolver, GltfModel& out,
               std::string& err, const GltfRangeLoader* rangeLoader,
               bool streamMeshes) {
    out = GltfModel{};
    if (!json || len == 0) {
        err = "glTF: json vazio";
        return false;
    }
    Json doc;
    if (!Json::parse(json, len, doc) || doc.type != Json::Type::Object) {
        err = "glTF: JSON inválido";
        return false;
    }
    const Json* jasset = doc.find("asset");
    if (!jasset || !jasset->find("version")) {
        err = "glTF: sem asset.version";
        return false;
    }

    // ---- 0.9.6.3 (R-020) · EXTENSÕES DE COMPRESSÃO: mensagem CLARA --------
    // A falha em silêncio do dono: um glb com Draco/meshopt/KTX2 falhava
    // DEPOIS com erros obscuros ("POSITION inválido") — o ficheiro usa
    // compressão que a engine não decodifica. Agora DETETA-A ANTES e diz
    // qual é. extensionsRequired = obrigatórias (sem elas o modelo é
    // ilegível); extensionsUsed = log informativo.
    {
        static const char* const kUnsupported[] = {
            "KHR_draco_mesh_compression",
            "EXT_meshopt_compression",
            "KHR_texture_basisu",
            "KHR_mesh_quantization",
        };
        if (const Json* jr = doc.find("extensionsRequired");
            jr && jr->type == Json::Type::Array) {
            for (const Json& e : jr->items) {
                if (e.type != Json::Type::String) {
                    continue;
                }
                for (const char* bad : kUnsupported) {
                    if (e.string == bad) {
                        err = std::string("o ficheiro usa compressão ") +
                              bad + ", que ainda não é suportada — "
                              "exporta o modelo sem Draco/meshopt/KTX2 "
                              "(ou converte para malha simples)";
                        return false;
                    }
                }
            }
        }
        if (const Json* ju = doc.find("extensionsUsed");
            ju && ju->type == Json::Type::Array) {
            for (const Json& e : ju->items) {
                if (e.type == Json::Type::String) {
                    elog::info("asset: glTF usa a extensão '%s' "
                               "(informativo)", e.string.c_str());
                }
            }
        }
    }

    // ---- buffers ------------------------------------------------------------
    GltfBufferStore store;
    std::vector<u64> declaredLens;   // A2: buffers[].byteLength do JSON
    if (rangeLoader && rangeLoader->fn) {
        store.setRangeLoader(*rangeLoader);
    }
    if (const Json* jb = doc.find("buffers"); jb && jb->type == Json::Type::Array) {
        for (const Json& b : jb->items) {
            u64 decl = 0;
            if (const Json* bl = b.find("byteLength");
                bl && bl->type == Json::Type::Number) {
                decl = static_cast<u64>(bl->number);
            }
            declaredLens.push_back(decl);
            const Json* uri = b.find("uri");
            if (!uri || uri->type != Json::Type::String || uri->string.empty()) {
                // sem URI = buffer do GLB (BIN chunk). 0.8.10: com RANGE
                // LOADER o buffer é DEFERIDO (streaming do ficheiro); sem
                // loader, residente como sempre.
                if (rangeLoader && rangeLoader->fn && rangeLoader->binLen > 0) {
                    store.addDeferred(rangeLoader->binLen);
                    continue;
                }
                if (bin.empty()) {
                    err = "glTF: buffer sem URI fora de .glb";
                    return false;
                }
                store.addOwned(std::vector<u8>(bin));
                continue;
            }
            const std::string& u = uri->string;
            constexpr char kDataPfx[] = "data:application/octet-stream;base64,";
            if (u.compare(0, sizeof(kDataPfx) - 1, kDataPfx) == 0) {
                std::vector<u8> decoded;
                if (!base64Decode(u.c_str() + (sizeof(kDataPfx) - 1),
                                  u.size() - (sizeof(kDataPfx) - 1), decoded)) {
                    err = "glTF: data: URI base64 inválida";
                    return false;
                }
                store.addOwned(std::move(decoded));
            } else if (rangeLoader && rangeLoader->fn &&
                       rangeLoader->binLen > 0 && u == rangeLoader->deferUri) {
                // 0.9.6.12g (A2-2 · FAZ 5): o .gltf com irmão .bin GRANDE —
                // o buffer externo do URI conhecido é DEFERIDO (ranges do
                // ficheiro no disco; o scene.bin de 212 MB do dono nunca
                // mais é lido INTEIRO para a RAM). Os OUTROS URIs (segundo
                // .bin, texturas externas) seguem pelo resolver como sempre.
                store.addDeferred(rangeLoader->binLen);
            } else if (resolver.fn) {
                std::vector<u8> ext;
                if (!resolver.fn(resolver.user, u.c_str(), ext) || ext.empty()) {
                    err = "glTF: buffer externo não resolvido: " + u;
                    return false;
                }
                store.addOwned(std::move(ext));
            } else {
                err = "glTF: buffer externo sem resolver: " + u;
                return false;
            }
        }
    }
    store.setDeclaredLens(std::move(declaredLens));

    // ---- bufferViews --------------------------------------------------------
    const Json* jviews = doc.find("bufferViews");
    if (jviews && jviews->type != Json::Type::Array) {
        err = "glTF: bufferViews inválido";
        return false;
    }

    // ---- images (F5.1-B: texturas embutidas base64 ou bufferView) ----------
    // 0.9.6.4 (GRUPO A/R-022 · A3): uma imagem que FALHA NÃO MATA O IMPORT —
    // antes, um bufferView de imagem fora do buffer (ou uma leitura falhada)
    // derrubava o glTF INTEIRO com a geometria boa («GLB com texturas
    // falha»). Agora: WARN com a CAUSA (a rotina ÚNICA resolveView devolve
    // ViewFail distinguindo limites de I/O) + imagem marcada broken — o
    // passe de texturas conta a falha (stats.texWarn) e o dono vê o toast.
    if (const Json* ji = doc.find("images"); ji && ji->type == Json::Type::Array) {
        u32 imgIdx = 0;
        for (const Json& im : ji->items) {
            GltfImage gi;
            const Json* uri = im.find("uri");
            if (uri && uri->type == Json::Type::String && !uri->string.empty()) {
                const std::string& u = uri->string;
                constexpr char kPngDataPfx[] = "data:image/png;base64,";
                if (u.compare(0, sizeof(kPngDataPfx) - 1, kPngDataPfx) == 0) {
                    if (!base64Decode(u.c_str() + (sizeof(kPngDataPfx) - 1),
                                      u.size() - (sizeof(kPngDataPfx) - 1),
                                      gi.bytes)) {
                        // data: URI podre = imagem falhada, não import morto
                        gi.broken = true;
                        elog::warn("asset: glTF imagem %u: data: URI base64 "
                                   "inválida — textura ignorada (o mesh "
                                   "segue com material por defeito)", imgIdx);
                    } else {
                        gi.mime = "image/png";
                        elog::info("asset: glTF imagem %u: uri data: (%llu B)",
                                   imgIdx,
                                   static_cast<unsigned long long>(
                                       gi.bytes.size()));
                    }
                } else if (u.compare(0, 5, "data:") == 0) {
                    gi.mime = "desconhecido";   // mime não-PNG — ignora (sem falha)
                    elog::info("asset: glTF imagem %u: uri data: mime não-PNG "
                               "(%s) — fora do escopo", imgIdx, u.c_str());
                } else {
                    gi.uriPath = u;             // textura EXTERNA
                    elog::info("asset: glTF imagem %u: uri externa '%s' (o "
                               "irmão é copiado para source/ no import)",
                               imgIdx, u.c_str());
                }
            } else if (const Json* bv = im.find("bufferView");
                       bv && bv->type == Json::Type::Number) {
                // textura EMBUTIDA no binário (GLB): bytes do bufferView
                if (!jviews) {
                    // sem bufferViews declarados a imagem é ilegível — mas o
                    // glTF pode ser só geometria: NÃO é fatal (R-022)
                    gi.broken = true;
                    elog::warn("asset: glTF imagem %u: sem bufferViews no "
                               "ficheiro — textura ignorada", imgIdx);
                } else {
                    const i32 vi = static_cast<i32>(bv->number);
                    if (vi < 0 || vi >= static_cast<i32>(jviews->items.size())) {
                        gi.broken = true;
                        elog::warn("asset: glTF imagem %u: bufferView %d fora "
                                   "do range (%zu views) — textura ignorada "
                                   "(o mesh segue)", imgIdx, vi,
                                   jviews->items.size());
                    } else {
                        const Json& jv = jviews->items[static_cast<size_t>(vi)];
                        u64 vOff = 0, vLen = 0;
                        if (const Json* o = jv.find("byteOffset");
                            o && o->type == Json::Type::Number) {
                            vOff = static_cast<u64>(o->number);
                        }
                        if (const Json* l = jv.find("byteLength");
                            l && l->type == Json::Type::Number) {
                            vLen = static_cast<u64>(l->number);
                        }
                        ViewSpan span;
                        ViewDiag diag;
                        const ViewFail vf =
                            resolveView(store, jv, 0, 1, span, &diag);
                        if (vf != ViewFail::None) {
                            // A CAUSA DISTINTA + A COMPARAÇÃO COMPLETA
                            // (A2, tarefa 1/3: a MESMA rotina, a MESMA
                            // linha com os números — nunca dois checks a
                            // divergir nem um log truncado; A2-2: file=)
                            char linha[256];
                            std::snprintf(
                                linha, sizeof(linha),
                                "glb: view%d buffer%zu off=%llu len=%llu "
                                "declared=%llu real=%llu file=%llu",
                                vi, diag.bufferIdx,
                                static_cast<unsigned long long>(diag.viewOff),
                                static_cast<unsigned long long>(diag.viewLen),
                                static_cast<unsigned long long>(diag.declared),
                                static_cast<unsigned long long>(diag.real),
                                static_cast<unsigned long long>(diag.fileBytes));
                            elog::warn("import: %s", linha);
                            gi.broken = true;
                            elog::warn("asset: glTF imagem %u: bufferView %d "
                                       "(off %llu, len %llu) FALHOU: %s — "
                                       "textura ignorada (o mesh segue com "
                                       "material por defeito)",
                                       imgIdx, vi,
                                       static_cast<unsigned long long>(vOff),
                                       static_cast<unsigned long long>(vLen),
                                       viewFailText(vf));
                        } else {
                            gi.bytes.assign(span.data,
                                            span.data + span.byteLength);
                            if (const Json* mt = im.find("mimeType");
                                mt && mt->type == Json::Type::String) {
                                gi.mime = mt->string;
                            }
                            elog::info("asset: glTF imagem %u: bufferView %d "
                                       "(off %llu, len %llu, mime %s)",
                                       imgIdx, vi,
                                       static_cast<unsigned long long>(vOff),
                                       static_cast<unsigned long long>(vLen),
                                       gi.mime.c_str());
                        }
                    }
                }
            }
            out.images.push_back(std::move(gi));
            ++imgIdx;
        }
    }

    // ---- textures (fontes de imagem; F5.1-B) --------------------------------
    std::vector<i32> textureSources;
    if (const Json* jt = doc.find("textures"); jt && jt->type == Json::Type::Array) {
        textureSources.reserve(jt->items.size());
        for (const Json& t : jt->items) {
            i32 src = -1;
            if (const Json* s = t.find("source"); s && s->type == Json::Type::Number) {
                if (s->number >= 0 &&
                    s->number < static_cast<f64>(out.images.size())) {
                    src = static_cast<i32>(s->number);
                }
            }
            textureSources.push_back(src);
        }
    }

    // ---- materials (básico) --------------------------------------------------
    if (const Json* jm = doc.find("materials"); jm && jm->type == Json::Type::Array) {
        for (const Json& m : jm->items) {
            GltfMaterial gm;
            if (const Json* n = m.find("name"); n && n->type == Json::Type::String) {
                gm.name = n->string;
            }
            if (const Json* pbr = m.find("pbrMetallicRoughness")) {
                if (const Json* bc = pbr->find("baseColorFactor");
                    bc && bc->type == Json::Type::Array && bc->items.size() == 4) {
                    for (int i = 0; i < 4; ++i) {
                        gm.baseColor[i] = static_cast<f32>(bc->items[i].number);
                    }
                }
                if (const Json* bt = pbr->find("baseColorTexture");
                    bt && bt->type == Json::Type::Object) {
                    if (const Json* ix = bt->find("index");
                        ix && ix->type == Json::Type::Number &&
                        ix->number >= 0 &&
                        ix->number < static_cast<f64>(textureSources.size())) {
                        gm.baseColorTex =
                            textureSources[static_cast<size_t>(ix->number)];
                    }
                }
            }
            out.materials.push_back(std::move(gm));
        }
    }

    // ---- leitor de accessor (bounds-checked) ---------------------------------
    // 0.9.6.12 (A2 · R-014): o estado da ÚLTIMA validação de view — o
    // chamador da primitiva decide degradar (OutOfBounds = exporter
    // malformado: larga a primitiva com W) ou matar o import (o resto)
    ViewFail lastViewFail = ViewFail::None;
    ViewDiag lastViewDiag;
    i32 lastViewIdx = -1;
    auto viewDiagLine = [&](i32 viewIdx, const ViewDiag& dg) {
        // 0.9.6.12g (A2-2 · FAZ 1): a linha COMPLETA — os quatro números
        // + file= (o ficheiro em disco) — nunca truncada (o buffer subiu
        // para 256 e o EngineLog agora carrega linhas de 2048)
        char buf[256];
        std::snprintf(buf, sizeof(buf),
                      "glb: view%d buffer%zu off=%zu len=%zu acc=%zu "
                      "declared=%llu real=%llu file=%llu",
                      viewIdx, dg.bufferIdx, dg.viewOff, dg.viewLen,
                      dg.accOff, static_cast<unsigned long long>(dg.declared),
                      static_cast<unsigned long long>(dg.real),
                      static_cast<unsigned long long>(dg.fileBytes));
        return std::string(buf);
    };
    auto readAccessor = [&](i32 accIdx, std::vector<u8>& rawElems, size_t& elemCount,
                            size_t& compCount, size_t& compSize) -> bool {
        if (!jviews) {
            err = "glTF: accessor sem bufferViews";
            return false;
        }
        const Json* ja = doc.find("accessors");
        if (!ja || accIdx < 0 || accIdx >= static_cast<i32>(ja->items.size())) {
            err = "glTF: accessor fora do range";
            return false;
        }
        const Json& a = ja->items[accIdx];
        const Json* jbv = a.find("bufferView");
        const i32 viewIdx =
            (jbv && jbv->type == Json::Type::Number) ? static_cast<i32>(jbv->number) : -1;
        size_t byteOff = 0;
        if (const Json* bo = a.find("byteOffset"); bo && bo->type == Json::Type::Number) {
            byteOff = static_cast<size_t>(bo->number);
        }
        if (const Json* c = a.find("count"); c && c->type == Json::Type::Number) {
            elemCount = static_cast<size_t>(c->number);
        } else {
            err = "glTF: accessor sem count";
            return false;
        }
        i32 ct = 5126;
        if (const Json* t = a.find("componentType"); t && t->type == Json::Type::Number) {
            ct = static_cast<i32>(t->number);
        }
        compSize = componentSize(ct);
        std::string atype = "SCALAR";
        if (const Json* t = a.find("type"); t && t->type == Json::Type::String) {
            atype = t->string;
        }
        compCount = componentsOf(atype);
        if (compSize == 0 || compCount == 0) {
            err = "glTF: accessor com tipo/componente não suportado";
            return false;
        }
        if (viewIdx < 0) {
            err = "glTF: accessor sem bufferView (sparse não suportado)";
            return false;
        }
        if (viewIdx >= static_cast<i32>(jviews->items.size())) {
            err = "glTF: bufferView fora do range";
            return false;
        }
        const size_t elemSize = compSize * compCount;
        ViewSpan span;
        ViewDiag diag;
        const ViewFail vf =
            resolveView(store, jviews->items[static_cast<size_t>(viewIdx)],
                        byteOff, elemSize, span, &diag);
        lastViewFail = vf;
        lastViewDiag = diag;
        lastViewIdx = viewIdx;
        if (vf != ViewFail::None) {
            // GEOMETRIA falhou = import falha (a spec A3: textura falha
            // tolera-se, geometria NÃO) — mas com a CAUSA exata (R-022:
            // «fora do buffer» deixou de cobrir também as falhas de I/O)
            // E COM A COMPARAÇÃO COMPLETA (A2, a tarefa 1: os quatro
            // números NUNCA truncados — o dono lê off/len/declared/real;
            // A2-2 acrescenta file= o disco)
            const std::string line = viewDiagLine(viewIdx, diag);
            elog::error("import: %s", line.c_str());
            // 0.9.6.12g (A2-2): o suffixo é HONESTO por causa — um range
            // acima do teto NÃO é corrupção (o ficheiro é válido) e uma
            // leitura curta não é corrupção (o disco falhou)
            const char* sufixo =
                vf == ViewFail::TooBig
                    ? " — o ficheiro é VÁLIDO mas o range não cabe na "
                      "memória (divide a malha ou importa um modelo mais "
                      "leve)"
                    : vf == ViewFail::ReadFail
                          ? " (a leitura do disco falhou ou o ficheiro "
                            "mudou durante o import)"
                          : " (ficheiro corrompido?)";
            const std::string nomeado =
                vf == ViewFail::NoBuffer
                    ? std::string("glTF: bufferView refere o buffer ") +
                          std::to_string(diag.bufferIdx) + ", que não "
                          "existe (o ficheiro declara " +
                          std::to_string(store.count()) + " buffer(s))"
                    : std::string("glTF: accessor (view ") +
                          std::to_string(viewIdx) + ") falhou: " +
                          viewFailText(vf) + sufixo;
            err = nomeado + " — " + line;
            return false;
        }
        if (span.stride == elemSize) {
            // compacto: cópia direta — ou ADOTAR o staging (A2-2 · FAZ 5:
            // o accessor compacto que ocupa o view INTEIRO, o perfil
            // scene/dragão do dono com accOff=0, move os bytes em vez de
            // os duplicar — o range de 118 MB nunca existe duas vezes)
            if (elemCount * elemSize > span.byteLength) {
                err = "glTF: accessor excede o bufferView";
                return false;
            }
            if (span.staged && elemCount * elemSize == span.byteLength) {
                rawElems = store.takeStaged();
            } else {
                rawElems.resize(elemCount * elemSize);
                std::memcpy(rawElems.data(), span.data, elemCount * elemSize);
            }
        } else {
            // interleaved: copia elemento a elemento respeitando o stride
            rawElems.resize(elemCount * elemSize);
            for (size_t e = 0; e < elemCount; ++e) {
                if ((e * span.stride) + elemSize > span.byteLength) {
                    err = "glTF: accessor excede o bufferView (stride)";
                    return false;
                }
                std::memcpy(rawElems.data() + e * elemSize,
                            span.data + e * span.stride, elemSize);
            }
        }
        return true;
    };

    // ---- meshes/primitives ----------------------------------------------------
    const Json* jmeshes = doc.find("meshes");
    if (!jmeshes || jmeshes->type != Json::Type::Array || jmeshes->items.empty()) {
        err = "glTF: sem meshes";
        return false;
    }
    for (const Json& jm : jmeshes->items) {
        if (streamMeshes) {
            // 0.10-M (PASSO 3): a geometria NÃO se materializa aqui — o
            // conversor streaming lê os accessors por bloco (mmap). O que
            // resta do parse (nós/materiais/anims/skins/imagens) é pequeno.
            continue;
        }
        MeshData md;
        if (const Json* n = jm.find("name"); n && n->type == Json::Type::String) {
            md.name = n->string;
        }
        i32 meshMatIdx = -1;   // primeiro material das primitivas (F5.1-B)
        const Json* jprims = jm.find("primitives");
        if (!jprims || jprims->type != Json::Type::Array) {
            err = "glTF: mesh sem primitives";
            return false;
        }
        // 0.9.6.12 (A2 · a spec 2d): bufferView que GENUINAMENTE excede o
        // buffer (exporter malformado) = DEGRADAR, não matar: larga a
        // primitiva desse accessor com W + a linha completa da comparação
        // e importa o resto. A queda conta em out.primsDropped (o toast
        // do import diz — nunca silencioso)
        auto dropPrim = [&](const char* what) {
            ++out.primsDropped;
            out.primDropCause =
                "glTF: a primitiva foi largada (" + std::string(what) +
                " — " + viewFailText(lastViewFail) + ") — " +
                viewDiagLine(lastViewIdx, lastViewDiag);
            elog::warn("import: primitiva %zu LARGADA (o view do %s está "
                       "fora do buffer) — %s (o RESTO do modelo entra; o "
                       "toast diz o total)",
                       out.primsDropped, what, out.primDropCause.c_str());
        };
        // 0.9.6.12g (A2-2): DEGRADA (não mata) o exporter malformado — o
        // view fora do buffer REAL ou acima do DECLARADO (a mesma classe:
        // o ficheiro mente; o resto do modelo pode entrar). TooBig/ReadFail
        // continuam FATAIS na geometria (sem dados não há malha)
        auto viewDegradavel = [&]() {
            return lastViewFail == ViewFail::OutOfBounds ||
                   lastViewFail == ViewFail::BeyondDeclared;
        };
        for (size_t pi = 0; pi < jprims->items.size(); ++pi) {
            const Json& jp = jprims->items[pi];
            if (const Json* mode = jp.find("mode");
                mode && mode->type == Json::Type::Number &&
                static_cast<i32>(mode->number) != 4) {
                continue;   // só TRIANGLES (linhas/pontos/tiras = fora do escopo)
            }
            const Json* jattrs = jp.find("attributes");
            if (!jattrs || jattrs->type != Json::Type::Object) {
                err = "glTF: primitiva sem attributes";
                return false;
            }
            std::vector<Vec3> pos;
            std::vector<Vec3> nor;
            std::vector<Vec2> uv;
            const Json* jpos = jattrs->find("POSITION");
            if (!jpos || jpos->type != Json::Type::Number) {
                err = "glTF: primitiva sem POSITION";
                return false;
            }
            {
                std::vector<u8> raw;
                size_t count = 0, cc = 0, cs = 0;
                if (!readAccessor(static_cast<i32>(jpos->number), raw, count, cc, cs) ||
                    cc != 3 || cs != 4) {
                    if (viewDegradavel()) {
                        dropPrim("POSITION");
                        continue;   // degrada a PRIMITIVA (a spec 2d)
                    }
                    err = err.empty() ? "glTF: POSITION inválido" : err;
                    return false;
                }
                pos.resize(count);
                std::memcpy(pos.data(), raw.data(), count * sizeof(Vec3));
            }
            if (const Json* jn = jattrs->find("NORMAL");
                jn && jn->type == Json::Type::Number) {
                std::vector<u8> raw;
                size_t count = 0, cc = 0, cs = 0;
                if (!readAccessor(static_cast<i32>(jn->number), raw, count, cc, cs) ||
                    cc != 3 || cs != 4) {
                    if (viewDegradavel()) {
                        dropPrim("NORMAL");
                        continue;
                    }
                    err = err.empty() ? "glTF: NORMAL inválido" : err;
                    return false;
                }
                nor.resize(count);
                std::memcpy(nor.data(), raw.data(), count * sizeof(Vec3));
            }
            if (const Json* jt = jattrs->find("TEXCOORD_0");
                jt && jt->type == Json::Type::Number) {
                std::vector<u8> raw;
                size_t count = 0, cc = 0, cs = 0;
                if (!readAccessor(static_cast<i32>(jt->number), raw, count, cc, cs) ||
                    cc != 2 || cs != 4) {
                    if (viewDegradavel()) {
                        dropPrim("TEXCOORD_0");
                        continue;
                    }
                    err = err.empty() ? "glTF: TEXCOORD_0 inválido" : err;
                    return false;
                }
                uv.resize(count);
                std::memcpy(uv.data(), raw.data(), count * sizeof(Vec2));
            }
            // 0.8.2 (F7): JOINTS_0 (VEC4 u8/u16) + WEIGHTS_0 (VEC4 f32) —
            // só quando AMBOS existem (skin parcial não vale nada)
            std::vector<u8> jnt;
            std::vector<f32> wgt;
            const Json* jj = jattrs->find("JOINTS_0");
            const Json* jw = jattrs->find("WEIGHTS_0");
            if (jj && jw && jj->type == Json::Type::Number &&
                jw->type == Json::Type::Number) {
                std::vector<u8> rawJ;
                size_t cntJ = 0, ccJ = 0, csJ = 0;
                if (readAccessor(static_cast<i32>(jj->number), rawJ, cntJ, ccJ, csJ) &&
                    ccJ == 4 && (csJ == 1 || csJ == 2)) {
                    jnt.resize(cntJ * 4);
                    for (size_t e = 0; e < cntJ; ++e) {
                        for (int c = 0; c < 4; ++c) {
                            const u32 v = readU32(rawJ.data() + (e * 4 + c) * csJ,
                                                  csJ == 1 ? 5121 : 5123);
                            jnt[e * 4 + c] =
                                v > 255u ? 255u : static_cast<u8>(v);
                        }
                    }
                }
                std::vector<u8> rawW;
                size_t cntW = 0, ccW = 0, csW = 0;
                if (readAccessor(static_cast<i32>(jw->number), rawW, cntW, ccW, csW) &&
                    ccW == 4 && csW == 4) {
                    wgt.resize(cntW * 4);
                    std::memcpy(wgt.data(), rawW.data(), cntW * 4 * sizeof(f32));
                }
            }

            // funde a primitiva no MeshData do mesh (rebase de índices)
            const u16 base = static_cast<u16>(md.vertices.size());
            if (md.vertices.size() + pos.size() > 65536) {
                err = "glTF: mesh fundido excede 65535 vértices (o teto do caminho de "
                          "mesh única; o formato v3 já não tem teto — o "
                          "corte em blocos entra no PASSO 3 do 0.10-M)";
                return false;
            }
            MeshData::Group grp;
            grp.name = "primitive " + std::to_string(pi);
            if (const Json* jmat = jp.find("material");
                jmat && jmat->type == Json::Type::Number &&
                static_cast<size_t>(jmat->number) < out.materials.size()) {
                grp.material = out.materials[static_cast<size_t>(jmat->number)].name;
                if (meshMatIdx < 0) {
                    meshMatIdx = static_cast<i32>(jmat->number);
                }
            }
            grp.firstIndex = static_cast<u32>(md.indices.size());

            for (size_t v = 0; v < pos.size(); ++v) {
                Vertex vtx;
                vtx.pos = pos[v];
                vtx.normal = v < nor.size() ? nor[v] : Vec3{0.0f, 1.0f, 0.0f};
                vtx.uv = v < uv.size() ? uv[v] : Vec2{0.0f, 0.0f};
                md.vertices.push_back(vtx);
                if (jnt.size() == pos.size() * 4 && wgt.size() == pos.size() * 4) {
                    for (int c = 0; c < 4; ++c) {
                        md.skinJoints.push_back(jnt[v * 4 + c]);
                        md.skinWeights.push_back(wgt[v * 4 + c]);
                    }
                }
            }

            const Json* jidx = jp.find("indices");
            if (jidx && jidx->type == Json::Type::Number) {
                std::vector<u8> raw;
                size_t count = 0, cc = 0, cs = 0;
                if (!readAccessor(static_cast<i32>(jidx->number), raw, count, cc, cs) ||
                    cc != 1) {
                    if (viewDegradavel()) {
                        dropPrim("índices");
                        continue;
                    }
                    err = err.empty() ? "glTF: índices inválidos" : err;
                    return false;
                }
                i32 ct = 5123;
                if (const Json* ja = doc.find("accessors");
                    ja && static_cast<size_t>(jidx->number) < ja->items.size()) {
                    if (const Json* t = ja->items[static_cast<size_t>(jidx->number)]
                                             .find("componentType");
                        t && t->type == Json::Type::Number) {
                        ct = static_cast<i32>(t->number);
                    }
                }
                for (size_t i = 0; i < count; ++i) {
                    const u32 idx = readU32(raw.data() + i * cs, ct);
                    if (idx >= pos.size()) {
                        err = "glTF: índice fora do range da primitiva";
                        return false;
                    }
                    md.indices.push_back(static_cast<u16>(base + idx));
                }
            } else {
                // primitiva NÃO indexada: gera 0..count-1
                for (size_t i = 0; i < pos.size(); ++i) {
                    md.indices.push_back(static_cast<u16>(base + i));
                }
            }
            grp.indexCount =
                static_cast<u32>(md.indices.size()) - grp.firstIndex;
            md.groups.push_back(std::move(grp));
        }
        if (md.ok()) {
            out.meshes.push_back(std::move(md));
            out.meshMaterial.push_back(meshMatIdx);   // alinhado com meshes
        }
    }
    // 0.9.6.12 (A2): TODAS as primitivas caíram? o import falha COM a
    // comparação completa (a spec: fixture (ii) — a mensagem com os quatro
    // números) — nunca um «sem meshes» seco que esconde a causa
    if (out.meshes.empty() && out.primsDropped > 0) {
        err = "glTF: TODAS as primitivas ficaram fora dos limites do "
              "buffer — " + out.primDropCause;
        return false;
    }

    // ---- scene/nodes (hierarquia simples) --------------------------------------
    if (const Json* jnodes = doc.find("nodes"); jnodes && jnodes->type == Json::Type::Array) {
        // 0.10-M (PASSO 3): no modo streaming as refs de primitiva são
        // por NÓ (a transformação mundo é que baked por bloco)
        std::vector<i32> meshRemapStream(jmeshes->items.size(), -1);
        if (streamMeshes) {
            const Json* jaccessors = doc.find("accessors");
            for (size_t m = 0; m < jmeshes->items.size(); ++m) {
                const Json& jm = jmeshes->items[m];
                const Json* jprims = jm.find("primitives");
                if (!jprims || jprims->type != Json::Type::Array) continue;
                for (const Json& jp : jprims->items) {
                    if (const Json* mode = jp.find("mode");
                        mode && (mode->type != Json::Type::Number ||
                                 static_cast<i32>(mode->number) != 4)) {
                        continue;   // só TRIANGLES (a regra da casa)
                    }
                    const Json* jattrs = jp.find("attributes");
                    if (!jattrs) continue;
                    GltfPrimRef ref;
                    ref.mesh = static_cast<i32>(m);
                    if (const Json* j = jattrs->find("POSITION");
                        j && j->type == Json::Type::Number) {
                        ref.posAcc = static_cast<i32>(j->number);
                    }
                    if (ref.posAcc < 0 || !jaccessors ||
                        ref.posAcc >= static_cast<i32>(jaccessors->items.size())) {
                        continue;   // sem POSITION não há primitiva
                    }
                    const Json& jpa = jaccessors->items[static_cast<size_t>(ref.posAcc)];
                    ref.posCount = jpa.find("count")
                                       ? static_cast<u32>(jpa.find("count")->number)
                                       : 0;
                    if (const Json* j = jattrs->find("NORMAL");
                        j && j->type == Json::Type::Number) {
                        ref.nrmAcc = static_cast<i32>(j->number);
                    }
                    if (const Json* j = jattrs->find("TEXCOORD_0");
                        j && j->type == Json::Type::Number) {
                        ref.uvAcc = static_cast<i32>(j->number);
                    }
                    if (const Json* j = jp.find("indices");
                        j && j->type == Json::Type::Number) {
                        ref.idxAcc = static_cast<i32>(j->number);
                    }
                    if (const Json* jmat = jp.find("material");
                        jmat && jmat->type == Json::Type::Number) {
                        ref.material = static_cast<i32>(jmat->number);
                    }
                    meshRemapStream[m] = 1;
                    out.primRefs.push_back(ref);
                }
            }
            // o nó de CADA ref: os nós apontam o mesh ORIGINAL — como o
            // mesmo mesh pode ser partilhado por vários nós (instancing),
            // a expansão nó×primitiva acontece AQUI por nó
            std::vector<GltfPrimRef> expanded;
            for (const Json& jn : jnodes->items) {
                i32 meshIdx = -1;
                if (const Json* m = jn.find("mesh");
                    m && m->type == Json::Type::Number) {
                    const i32 mi = static_cast<i32>(m->number);
                    if (mi >= 0 && mi < static_cast<i32>(meshRemapStream.size()) &&
                        meshRemapStream[static_cast<size_t>(mi)] == 1) {
                        meshIdx = mi;
                    }
                }
                if (meshIdx < 0) continue;
                for (const GltfPrimRef& r : out.primRefs) {
                    if (r.mesh == meshIdx) {
                        GltfPrimRef c = r;
                        c.node = static_cast<i32>(expanded.size()) * 0 +
                                 static_cast<i32>(&jn - jnodes->items.data());
                        expanded.push_back(c);
                    }
                }
            }
            out.primRefs = std::move(expanded);
        }
        out.nodes.reserve(jnodes->items.size());
        // mapeia o índice do mesh ORIGINAL → posição em out.meshes (meshes
        // sem primitivas TRIANGLES não entram no modelo)
        std::vector<i32> meshRemap(jmeshes->items.size(), -1);
        {
            size_t kept = 0;
            for (size_t m = 0; m < jmeshes->items.size(); ++m) {
                const Json& jm = jmeshes->items[m];
                bool anyTriangles = false;
                if (const Json* jprims = jm.find("primitives");
                    jprims && jprims->type == Json::Type::Array) {
                    for (const Json& jp : jprims->items) {
                        if (const Json* mode = jp.find("mode");
                            !mode || (mode->type == Json::Type::Number &&
                                      static_cast<i32>(mode->number) == 4)) {
                            anyTriangles = true;
                            break;
                        }
                    }
                }
                meshRemap[m] = anyTriangles ? static_cast<i32>(kept++) : -1;
            }
        }
        for (const Json& jn : jnodes->items) {
            GltfNode gn;
            if (const Json* n = jn.find("name"); n && n->type == Json::Type::String) {
                gn.name = n->string;
            }
            if (const Json* m = jn.find("mesh"); m && m->type == Json::Type::Number) {
                const i32 mi = static_cast<i32>(m->number);
                if (mi >= 0 && mi < static_cast<i32>(meshRemap.size())) {
                    gn.mesh = meshRemap[static_cast<size_t>(mi)];
                }
            }
            if (const Json* t = jn.find("translation");
                t && t->type == Json::Type::Array && t->items.size() == 3) {
                gn.translation = Vec3{static_cast<f32>(t->items[0].number),
                                      static_cast<f32>(t->items[1].number),
                                      static_cast<f32>(t->items[2].number)};
            }
            if (const Json* r = jn.find("rotation");
                r && r->type == Json::Type::Array && r->items.size() == 4) {
                gn.rotation = Quat{static_cast<f32>(r->items[0].number),
                                   static_cast<f32>(r->items[1].number),
                                   static_cast<f32>(r->items[2].number),
                                   static_cast<f32>(r->items[3].number)};
            }
            if (const Json* s = jn.find("scale");
                s && s->type == Json::Type::Array && s->items.size() == 3) {
                gn.scale = Vec3{static_cast<f32>(s->items[0].number),
                                static_cast<f32>(s->items[1].number),
                                static_cast<f32>(s->items[2].number)};
            }
            out.nodes.push_back(std::move(gn));
        }
        // 2ª passada: children → parent (por índice de nó)
        for (size_t i = 0; i < jnodes->items.size(); ++i) {
            const Json* jch = jnodes->items[i].find("children");
            if (!jch || jch->type != Json::Type::Array) {
                continue;
            }
            for (const Json& c : jch->items) {
                if (c.type != Json::Type::Number) {
                    continue;
                }
                const size_t child = static_cast<size_t>(c.number);
                if (child < out.nodes.size() && child != i) {
                    out.nodes[child].parent = static_cast<i32>(i);
                }
            }
        }
    }

    if (!out.ok()) {
        err = err.empty() ? "glTF: nenhum mesh com triângulos" : err;
        return false;
    }

    // ---- animations (0.8.1, F7): channels/samplers → GltfAnimation --------
    // O converter para CLIPS do AnimationPlayer vive em assets/GltfAnim
    // (o parser fica GL-free puro; interpolação LINEAR é a suportada — STEP
    // tolerada como linear, CUBICSPLINE extrai o valor do MEIO [in,val,out]).
    if (const Json* janims = doc.find("animations");
        janims && janims->type == Json::Type::Array) {
        for (size_t ai = 0; ai < janims->items.size(); ++ai) {
            const Json& ja = janims->items[ai];
            GltfAnimation anim;
            if (const Json* n = ja.find("name"); n && n->type == Json::Type::String &&
                !n->string.empty()) {
                anim.name = n->string;
            } else {
                anim.name = "anim " + std::to_string(ai);
            }
            // samplers: input (SCALAR f32) + output (VEC3/VEC4 f32)
            if (const Json* jsam = ja.find("samplers");
                jsam && jsam->type == Json::Type::Array) {
                for (const Json& js : jsam->items) {
                    GltfAnimSampler s;
                    bool ok = true;
                    if (const Json* in = js.find("input");
                        in && in->type == Json::Type::Number) {
                        std::vector<u8> raw;
                        size_t count = 0, cc = 0, cs = 0;
                        if (readAccessor(static_cast<i32>(in->number), raw, count,
                                         cc, cs) &&
                            cc == 1 && cs == 4) {
                            s.times.resize(count);
                            std::memcpy(s.times.data(), raw.data(),
                                        count * sizeof(f32));
                        } else {
                            ok = false;
                        }
                    } else {
                        ok = false;
                    }
                    if (ok) {
                        if (const Json* outj = js.find("output");
                            outj && outj->type == Json::Type::Number) {
                            std::vector<u8> raw;
                            size_t count = 0, cc = 0, cs = 0;
                            if (readAccessor(static_cast<i32>(outj->number), raw,
                                             count, cc, cs) &&
                                (cc == 3 || cc == 4) && cs == 4) {
                                s.components = static_cast<u32>(cc);
                                s.values.resize(count * cc);
                                std::memcpy(s.values.data(), raw.data(),
                                            count * cc * sizeof(f32));
                            } else {
                                ok = false;
                            }
                        } else {
                            ok = false;
                        }
                    }
                    if (ok) {
                        // CUBICSPLINE: [in, valor, out] por key → fica o MEIO
                        std::string interp = "LINEAR";
                        if (const Json* ip = js.find("interpolation");
                            ip && ip->type == Json::Type::String) {
                            interp = ip->string;
                        }
                        if (interp == "CUBICSPLINE" &&
                            s.values.size() == s.times.size() * 3u * s.components) {
                            std::vector<f32> mid(s.times.size() * s.components);
                            for (size_t k = 0; k < s.times.size(); ++k) {
                                std::memcpy(&mid[k * s.components],
                                            &s.values[(k * 3 + 1) * s.components],
                                            s.components * sizeof(f32));
                            }
                            s.values = std::move(mid);
                        }
                        // STEP: tolerada como linear (dívida documentada)
                        anim.samplers.push_back(std::move(s));
                    }
                }
            }
            // channels: (nó, path) → sampler
            if (const Json* jch = ja.find("channels");
                jch && jch->type == Json::Type::Array) {
                for (const Json& jc : jch->items) {
                    GltfAnimChannel ch;
                    if (const Json* js = jc.find("sampler");
                        js && js->type == Json::Type::Number) {
                        ch.sampler = static_cast<i32>(js->number);
                    }
                    const Json* jt = jc.find("target");
                    if (jt && jt->type == Json::Type::Object) {
                        if (const Json* n = jt->find("node");
                            n && n->type == Json::Type::Number) {
                            ch.node = static_cast<i32>(n->number);
                        }
                        if (const Json* p = jt->find("path");
                            p && p->type == Json::Type::String) {
                            if (p->string == "rotation") {
                                ch.path = GltfAnimChannel::Path::Rotation;
                            } else if (p->string == "scale") {
                                ch.path = GltfAnimChannel::Path::Scale;
                            } else {
                                ch.path = GltfAnimChannel::Path::Translation;
                            }
                        }
                    }
                    anim.channels.push_back(std::move(ch));
                }
            }
            if (!anim.channels.empty() && !anim.samplers.empty()) {
                out.animations.push_back(std::move(anim));
            }
        }
    }

    // ---- skins (0.8.2, F7): joints + inverseBindMatrices ------------------
    // Os joints são REORDENADOS pais-primeiro (a composição hierárquica do
    // computeSkinMatrices fica iterativa); nodeToJoint mapeia nó→índice.
    // IBM ausente → identidade (skin em bind-space direto — tolerado).
    if (const Json* jskins = doc.find("skins");
        jskins && jskins->type == Json::Type::Array) {
        for (const Json& jsk : jskins->items) {
            GltfSkin skin;
            if (const Json* n = jsk.find("name"); n && n->type == Json::Type::String) {
                skin.name = n->string;
            }
            const Json* jjoints = jsk.find("joints");
            if (!jjoints || jjoints->type != Json::Type::Array ||
                jjoints->items.empty()) {
                continue;
            }
            // nós-alvo (índices) na ordem do glTF
            std::vector<i32> nodes;
            nodes.reserve(jjoints->items.size());
            for (const Json& jn : jjoints->items) {
                if (jn.type == Json::Type::Number) {
                    const i32 ni = static_cast<i32>(jn.number);
                    if (ni >= 0 && ni < static_cast<i32>(out.nodes.size())) {
                        nodes.push_back(ni);
                    } else {
                        nodes.push_back(-1);
                    }
                } else {
                    nodes.push_back(-1);
                }
            }
            // IBM (MAT4 f32, uma por joint) — acessor compartilhado com o
            // resto do parser (bounds-checked)
            std::vector<Mat4> ibm(nodes.size(), Mat4::identity());
            bool ibmOk = false;
            if (const Json* jibm = jsk.find("inverseBindMatrices");
                jibm && jibm->type == Json::Type::Number) {
                std::vector<u8> raw;
                size_t count = 0, cc = 0, cs = 0;
                if (readAccessor(static_cast<i32>(jibm->number), raw, count, cc, cs) &&
                    cc == 16 && cs == 4) {
                    ibmOk = true;
                    const size_t n = count < nodes.size() ? count : nodes.size();
                    for (size_t k = 0; k < n; ++k) {
                        std::memcpy(ibm[k].m, raw.data() + k * 64, 64);
                    }
                }
            }
            (void)ibmOk;
            // mapeia nó → índice NA LISTA ORIGINAL (para reordenar)
            std::vector<i32> nodeToIdx(out.nodes.size(), -1);
            for (size_t k = 0; k < nodes.size(); ++k) {
                if (nodes[k] >= 0) {
                    nodeToIdx[static_cast<size_t>(nodes[k])] = static_cast<i32>(k);
                }
            }
            // reordena PAIS PRIMEIRO: adiciona joints cujo pai (de nó) já
            // está adicionado (ou não é joint); ≤N voltas (cadeias de nós)
            std::vector<bool> added(nodes.size(), false);
            skin.nodeToJoint.assign(out.nodes.size(), -1);
            for (size_t pass = 0; pass < nodes.size(); ++pass) {
                bool progressed = false;
                for (size_t k = 0; k < nodes.size(); ++k) {
                    if (added[k] || nodes[k] < 0) {
                        continue;
                    }
                    const GltfNode& gn = out.nodes[static_cast<size_t>(nodes[k])];
                    const i32 parentIdx =
                        (gn.parent >= 0 && gn.parent < static_cast<i32>(nodeToIdx.size()))
                            ? nodeToIdx[static_cast<size_t>(gn.parent)]
                            : -1;
                    if (parentIdx < 0 || added[static_cast<size_t>(parentIdx)]) {
                        GltfJoint j;
                        j.name = gn.name.empty()
                            ? ("joint " + std::to_string(k))
                            : gn.name;
                        j.parent = parentIdx >= 0
                            ? skin.nodeToJoint[static_cast<size_t>(nodes[static_cast<size_t>(parentIdx)])]
                            : -1;
                        j.pos = gn.translation;
                        j.rot = gn.rotation;
                        j.scale = gn.scale;
                        j.inverseBind = ibm[k];
                        skin.nodeToJoint[static_cast<size_t>(nodes[k])] =
                            static_cast<i32>(skin.joints.size());
                        skin.joints.push_back(std::move(j));
                        added[k] = true;
                        progressed = true;
                    }
                }
                if (!progressed) {
                    break;   // ciclo/cadeia estranha — o que entrou, entrou
                }
            }
            if (!skin.joints.empty()) {
                out.skins.push_back(std::move(skin));
            }
        }
    }
    return true;
}

bool parseGlb(const u8* data, size_t len, const GltfBufferResolver& resolver,
              GltfModel& out, std::string& err) {
    out = GltfModel{};
    if (!data || len < 12) {
        err = "GLB: ficheiro demasiado curto";
        return false;
    }
    u32 magic = 0, version = 0, total = 0;
    std::memcpy(&magic, data, 4);
    std::memcpy(&version, data + 4, 4);
    std::memcpy(&total, data + 8, 4);
    if (magic != kMagicGltf) {
        err = "GLB: magic inválido (não é .glb)";
        return false;
    }
    if (version != 2) {
        err = "GLB: versão != 2 não suportada";
        return false;
    }
    if (total > len) {
        err = "GLB: declaração de tamanho excede o ficheiro";
        return false;
    }

    size_t off = 12;
    const u8* jsonPtr = nullptr;
    size_t jsonLen = 0;
    std::vector<u8> bin;
    bool binFromChunk = false;

    while (off + 8 <= len) {
        u32 chunkLen = 0, chunkType = 0;
        std::memcpy(&chunkLen, data + off, 4);
        std::memcpy(&chunkType, data + off + 4, 4);
        off += 8;
        if (off + chunkLen > len) {
            err = "GLB: chunk truncado";
            return false;
        }
        if (chunkType == kChunkJson && !jsonPtr) {
            jsonPtr = data + off;
            jsonLen = chunkLen;
        } else if (chunkType == kChunkBin && !binFromChunk) {
            bin.assign(data + off, data + off + chunkLen);
            binFromChunk = true;
        }
        // 0.9.6.4 (GRUPO A/R-022): chunks são ALINHADOS a 4 bytes (o spec
        // pede o padding DENTRO do chunk; exportadores do mundo real às
        // vezes escrevem o comprimento sem padding e zeros até ao próximo
        // header). O caminho de FICHEIRO (convertGlbFile) já alinhava —
        // este, em memória, andava off += chunkLen SEM alinhar: os dois
        // parsers divergiam no MESMO ficheiro. UMA regra só: alinhar 4.
        off += chunkLen;
        off = (off + 3) & ~static_cast<size_t>(3);
    }

    if (!jsonPtr) {
        err = "GLB: sem chunk JSON";
        return false;
    }
    // o JSON chunk pode ter padding com espaços (alinhamento de 4) — o
    // parser do engine tolera whitespace no fim, ok direto
    return parseGltf(reinterpret_cast<const char*>(jsonPtr), jsonLen, bin,
                     resolver, out, err);
}

} // namespace vv
