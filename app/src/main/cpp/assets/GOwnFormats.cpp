// assets/GOwnFormats.cpp — leitores/escritores dos formatos próprios (0.8.10).
//
// PRINCÍPIOS (a regra da casa aplicada a bytes):
//   • NUNCA confiar no ficheiro: toda a leitura valida magic/endian/
//     versão/tamanho/checksum/contagens ANTES de tocar em arrays —
//     corrupção devolve ERRO LEGÍVEL, nunca crash, nunca lixo;
//   • DETERMINISMO: mesmos dados → mesmos bytes (o teste de pureza do CI
//     aperia o round-trip);
//   •Little-endian explícito (memcpy de u16/u32/u64/f32 — o host e o
//     arm64 do device são LE; o endianMark apanha o contrário).
#include "assets/GOwnFormats.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace vv {

u64 gfnv1a(const u8* data, size_t len) {
    u64 h = 1469598103934665603ull;
    for (size_t i = 0; i < len; ++i) {
        h ^= data[i];
        h *= 1099511628211ull;
    }
    return h;
}

namespace {

// ---- little-endian raw IO (sem dependências) --------------------------------
struct Writer {
    std::vector<u8>& b;
    explicit Writer(std::vector<u8>& out) : b(out) {}
    void u8_(u8 v) { b.push_back(v); }
    void u16_(u16 v) { b.push_back(static_cast<u8>(v & 0xFF));
                       b.push_back(static_cast<u8>(v >> 8)); }
    void u32_(u32 v) { u16_(static_cast<u16>(v & 0xFFFF));
                       u16_(static_cast<u16>(v >> 16)); }
    void u64_(u64 v) { u32_(static_cast<u32>(v & 0xFFFFFFFFull));
                       u32_(static_cast<u32>(v >> 32)); }
    void f32_(f32 v) { u32 raw; std::memcpy(&raw, &v, 4); u32_(raw); }
    void str_(const std::string& s) {   // u16 len + bytes (sem nulo)
        u16_(static_cast<u16>(s.size() > 65535 ? 65535 : s.size()));
        b.insert(b.end(), s.begin(), s.end());
    }
    void bytes_(const u8* p, size_t n) { b.insert(b.end(), p, p + n); }
};

struct Reader {
    const u8* p = nullptr;
    size_t n = 0;
    size_t i = 0;
    bool bad = false;   // lêu fora do fim — o chamador devolve erro
    explicit Reader(const u8* data, size_t len) : p(data), n(len) {}
    u8 u8_() { if (i + 1 > n) { bad = true; return 0; } return p[i++]; }
    u16 u16_() { if (i + 2 > n) { bad = true; return 0; }
                 const u16 v = static_cast<u16>(p[i] | (p[i + 1] << 8));
                 i += 2; return v; }
    u32 u32_() { const u16 lo = u16_(); const u16 hi = u16_();
                 return static_cast<u32>(lo) | (static_cast<u32>(hi) << 16); }
    u64 u64_() { const u32 lo = u32_(); const u32 hi = u32_();
                 return static_cast<u64>(lo) | (static_cast<u64>(hi) << 32); }
    f32 f32_() { const u32 raw = u32_(); f32 v; std::memcpy(&v, &raw, 4);
                 return v; }
    std::string str_() {
        const u16 len = u16_();
        if (bad || i + len > n) { bad = true; return ""; }
        std::string s(reinterpret_cast<const char*>(p + i), len);
        i += len;
        return s;
    }
    const u8* bytes_(size_t len) {
        if (bad || i + len > n) { bad = true; return nullptr; }
        const u8* r = p + i;
        i += len;
        return r;
    }
};

// ---- quantização 16-bit ------------------------------------------------------
u16 quantF(f32 v, f32 mn, f32 ext) {
    if (!(ext > 0.0f)) {
        return 0;
    }
    f32 q = (v - mn) / ext;
    if (q < 0.0f) q = 0.0f;
    if (q > 1.0f) q = 1.0f;
    return static_cast<u16>(q * 65535.0f + 0.5f);
}
f32 dequantF(u16 q, f32 mn, f32 ext) {
    return mn + (static_cast<f32>(q) / 65535.0f) * ext;
}
u16 quantS(f32 v) {   // [-1,1] → u16
    if (v < -1.0f) v = -1.0f;
    if (v > 1.0f) v = 1.0f;
    return static_cast<u16>((v * 0.5f + 0.5f) * 65535.0f + 0.5f);
}
f32 dequantS(u16 q) {
    return (static_cast<f32>(q) / 65535.0f) * 2.0f - 1.0f;
}
u16 quantU(f32 v) {   // [0,1] → u16
    if (v < 0.0f) v = 0.0f;
    if (v > 1.0f) v = 1.0f;
    return static_cast<u16>(v * 65535.0f + 0.5f);
}
f32 dequantU(u16 q) {
    return static_cast<f32>(q) / 65535.0f;
}

// header comum = SEMPRE 32 bytes (magic 4 + version 2 + endian 2 + align 4
// + payloadSize 8 + checksum 8 + reserved 4) — kGHeaderBytes vive no
// header (o conversor streaming do PASSO 3 também o usa)

void writeHeader(Writer& w, const char* magic, u64 payloadSize, u64 checksum,
                 u16 version = 1) {
    w.bytes_(reinterpret_cast<const u8*>(magic), 4);
    w.u16_(version);    // 0.10-M: o .gmesh escreve 3; os outros ficam na 1
    w.u16_(0x1A2B);     // endianMark
    w.u32_(version >= 3 ? 16 : 8);   // align (eco informativo)
    w.u64_(payloadSize);
    w.u64_(checksum);
    w.u32_(0);          // reserved (padding até 32 — o tamanho é CONTRATO)
}

} // namespace

bool gReadHeader(const u8* bytes, size_t len, const char* wantMagic,
                 GFileHeader& h, std::string& err) {
    if (!bytes || len < kGHeaderBytes) {
        err = "ficheiro próprio curto demais (" + std::to_string(len) + " B)";
        return false;
    }
    Reader r(bytes, len);
    for (int i = 0; i < 4; ++i) {
        h.magic[i] = static_cast<char>(r.u8_());
    }
    h.version = r.u16_();
    h.endianMark = r.u16_();
    h.align = r.u32_();
    h.payloadSize = r.u64_();
    h.checksum = r.u64_();
    if (std::memcmp(h.magic, wantMagic, 4) != 0) {
        err = "magic errado (esperado ";
        err += wantMagic;
        err += "): não é um ficheiro próprio válido";
        return false;
    }
    if (h.endianMark != 0x1A2B) {
        err = "endianess trocada (marca 0x" + std::to_string(h.endianMark) +
              ") — ficheiro de outra plataforma";
        return false;
    }
    if (h.version == 0) {
        err = "versão 0 inválida";
        return false;
    }
    // o RANGE de versões suportadas é DECISÃO DE CADA FORMATO (o .gmesh
    // abre 1..3 — ver kGmeshVersion*; o .gtext/.gm continuam na 1)
    if (h.payloadSize + kGHeaderBytes > len) {
        err = "payload truncado: header diz " +
              std::to_string(h.payloadSize) + " B mas só há " +
              std::to_string(len - kGHeaderBytes) + " B";
        return false;
    }
    if (h.version <= 2) {
        // v1/v2 do .gmesh: o FNV-1a cobre o PAYLOAD INTEIRO (a regra de
        // 0.8.10) — e é a regra VIGENTE do .gtext/.gm em qualquer versão
        const u64 got = gfnv1a(bytes + kGHeaderBytes,
                               static_cast<size_t>(h.payloadSize));
        if (got != h.checksum) {
            err = "CHECKSUM CORROMPIDO: payload diz 0x" +
                  std::to_string(h.checksum) + ", recalculado 0x" +
                  std::to_string(got) + " — o ficheiro foi danificado";
            return false;
        }
    }
    // v3: o checksum do header cobre SÓ os 160 B de metadados (o payload
    // inteiro obrigava a LER o ficheiro todo — inimigo do mmap/streaming);
    // a verificação dos metadados é feita pelo leitor v3 (que conhece o
    // tamanho fixo) e a integridade do resto vive nos CRC32 da tabela.
    return true;
}

// ---- .gmesh -------------------------------------------------------------------
// PAYLOAD v1 (e v2 — a interpretação tolerante documentada):
// verts u32, indices u32, bounds f32×6, groups u32, [skin u8],
// depois verts×(pos u16×3 + nrm u16×3 + uv u16×2 [+ joints u8×4 + w u16×4]),
// indices×u16, groups×(nome str, material str, first u32, count u32),
// skin verts×(joints u8×4, weights u16×4)
namespace {
bool readGMeshV1Payload(const u8* bytes, size_t len, MeshData& out,
                        std::string& err) {
    out = MeshData{};
    GFileHeader h;
    if (!gReadHeader(bytes, len, "GMES", h, err)) {
        return false;
    }
    Reader r(bytes + kGHeaderBytes, static_cast<size_t>(h.payloadSize));
    const u32 nVerts = r.u32_();
    const u32 nIdx = r.u32_();
    Vec3 mn, mx;
    mn.x = r.f32_(); mn.y = r.f32_(); mn.z = r.f32_();
    mx.x = r.f32_(); mx.y = r.f32_(); mx.z = r.f32_();
    const u32 nGroups = r.u32_();
    const u8 skinned = r.u8_();
    if (r.bad || nVerts == 0 || nVerts > 65535 || nIdx == 0 ||
        (nIdx % 3) != 0 || nGroups > 4096 || skinned > 1) {
        err = "contagens inválidas no .gmesh (verts=" + std::to_string(nVerts) +
              " idx=" + std::to_string(nIdx) + " grupos=" +
              std::to_string(nGroups) + ")";
        return false;
    }
    const Vec3 ext{mx.x - mn.x, mx.y - mn.y, mx.z - mn.z};
    out.vertices.resize(nVerts);
    for (u32 i = 0; i < nVerts; ++i) {
        Vertex& v = out.vertices[i];
        v.pos.x = dequantF(r.u16_(), mn.x, ext.x);
        v.pos.y = dequantF(r.u16_(), mn.y, ext.y);
        v.pos.z = dequantF(r.u16_(), mn.z, ext.z);
        v.normal.x = dequantS(r.u16_());
        v.normal.y = dequantS(r.u16_());
        v.normal.z = dequantS(r.u16_());
        // renormaliza (a quantização encolhe um pouco — normais ~unitárias)
        const f32 nl = std::sqrt(v.normal.x * v.normal.x +
                                 v.normal.y * v.normal.y +
                                 v.normal.z * v.normal.z);
        if (nl > 1e-6f) {
            v.normal.x /= nl; v.normal.y /= nl; v.normal.z /= nl;
        }
        v.uv.x = dequantU(r.u16_());
        v.uv.y = dequantU(r.u16_());
        if (skinned) {
            for (int k = 0; k < 4; ++k) {
                out.skinJoints.push_back(r.u8_());
            }
        }
    }
    if (skinned) {
        out.skinWeights.resize(static_cast<size_t>(nVerts) * 4);
        for (u32 i = 0; i < nVerts; ++i) {
            f32 wsum = 0.0f;
            for (int k = 0; k < 4; ++k) {
                const f32 wk = dequantU(r.u16_());
                out.skinWeights[static_cast<size_t>(i) * 4 +
                                static_cast<size_t>(k)] = wk;
                wsum += wk;
            }
            // renormaliza a soma (a quantização rouba ~0.002)
            if (wsum > 1e-6f && std::fabs(wsum - 1.0f) > 1e-4f) {
                for (int k = 0; k < 4; ++k) {
                    out.skinWeights[static_cast<size_t>(i) * 4 +
                                    static_cast<size_t>(k)] /= wsum;
                }
            }
        }
    }
    out.indices.resize(nIdx);
    for (u32 i = 0; i < nIdx; ++i) {
        out.indices[i] = r.u16_();
    }
    for (u32 g = 0; g < nGroups; ++g) {
        MeshData::Group grp;
        grp.name = r.str_();
        grp.material = r.str_();
        grp.firstIndex = r.u32_();
        grp.indexCount = r.u32_();
        if (grp.firstIndex + grp.indexCount > nIdx) {
            err = "grupo '" + grp.name + "' fora do range de índices";
            return false;
        }
        out.groups.push_back(std::move(grp));
    }
    if (r.bad) {
        err = "payload .gmesh truncado (contagens não batem com os bytes)";
        return false;
    }
    out.name = "gmesh";
    return true;
}

} // namespace (o leitor v1 cru — o resto do .gmesh volta ao nível do vv)

// ---- .gmesh v3 (0.10-M PASSO 2) ------------------------------------------------
// A spec viva é docs/GMESH_formato.md §v3. Resumo dos bytes:
//   [header comum 32][metadados 160][dados dos blocos, 16-alinhados]
//   [tabela de blocos 80 B/entrada][materiais (u16 len + bytes, 4-alinhados)]
// O checksum do header num v3 = FNV-1a dos 160 B de metadados; cada bloco
// leva CRC32 próprio e a tabela leva CRC32 próprio (a integridade sem
// ler o ficheiro inteiro — o requisito do mmap/streaming).
u32 gcrc32(const u8* data, size_t len) {
    static u32 table[256];
    static const bool ready = []() {
        for (u32 i = 0; i < 256; ++i) {
            u32 c = i;
            for (int k = 0; k < 8; ++k) {
                c = (c & 1u) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        return true;
    }();
    (void)ready;
    u32 c = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        c = table[(c ^ data[i]) & 0xFFu] ^ (c >> 8);
    }
    return c ^ 0xFFFFFFFFu;
}

namespace {

u32 v3AttrComponents(u8 semantic) {
    switch (semantic) {
        case kAttrPosition:
        case kAttrNormal: return 3;
        case kAttrUv0:
        case kAttrUv1: return 2;
        case kAttrTangent:
        case kAttrColor:
        case kAttrBones:
        case kAttrWeights: return 4;
        default: return 0;   // semântica desconhecida → rejeitada na leitura
    }
}

u32 v3AttrBytes(const GMeshV3Attr& a) {
    const u32 n = v3AttrComponents(a.semantic);
    if (n == 0) return 0;
    const u32 sz = a.storage == kAttrF32 ? 4u : (a.storage == kAttrU16 ? 2u : 1u);
    return n * sz;
}

void v3WriteMeta(Writer& w, const GMeshV3Meta& m) {
    w.u32_(m.attrCount);
    w.u32_(m.flags);
    w.u32_(m.blockVertexCap);
    w.u32_(m.tableCrc32);
    w.u64_(m.vertexCount);
    w.u64_(m.indexCount);
    w.u64_(m.blockCount);
    w.u64_(m.materialCount);
    w.f32_(m.aabbMin.x); w.f32_(m.aabbMin.y); w.f32_(m.aabbMin.z);
    w.f32_(m.aabbMax.x); w.f32_(m.aabbMax.y); w.f32_(m.aabbMax.z);
    w.u64_(m.blockTableOffset);
    w.u64_(m.materialTableOffset);
    for (u32 i = 0; i < kGmeshV3MaxAttrs; ++i) {
        const GMeshV3Attr& a = m.attrs[i];
        w.u8_(a.semantic);
        w.u8_(a.storage);
        w.u8_(a.normalized);
        w.u8_(a.reserved);
        w.u32_(a.reserved2);
    }
    w.u32_(0);
    w.u32_(0);
}

// 0.10-M (PASSO 3B) — o reset EXPLÍCITO membro-a-membro. O «m = GMeshV3Meta{}»
// de sempre faz o GCC 13 do runner do CI dar ICE (gimplify.cc:774,
// gimple_add_tmp_var — o agregado com array de agregados como temporário);
// o reset explícito é o MESMO efeito sem o construto que parte o compiler.
void v3Reset(GMeshV3Meta& m) {
    m.flags = 0;
    m.blockVertexCap = kGmeshV3BlockVertexCap;
    m.vertexCount = 0;
    m.indexCount = 0;
    m.blockCount = 0;
    m.materialCount = 0;
    m.aabbMin = Vec3{};
    m.aabbMax = Vec3{};
    m.blockTableOffset = 0;
    m.materialTableOffset = 0;
    for (u32 i = 0; i < kGmeshV3MaxAttrs; ++i) {
        m.attrs[i] = GMeshV3Attr{};
    }
    m.attrCount = 0;
    m.tableCrc32 = 0;
}

bool v3ReadMeta(Reader& r, GMeshV3Meta& m, std::string& err) {
    m.attrCount = r.u32_();
    m.flags = r.u32_();
    m.blockVertexCap = r.u32_();
    m.tableCrc32 = r.u32_();
    m.vertexCount = r.u64_();
    m.indexCount = r.u64_();
    m.blockCount = r.u64_();
    m.materialCount = r.u64_();
    m.aabbMin.x = r.f32_(); m.aabbMin.y = r.f32_(); m.aabbMin.z = r.f32_();
    m.aabbMax.x = r.f32_(); m.aabbMax.y = r.f32_(); m.aabbMax.z = r.f32_();
    m.blockTableOffset = r.u64_();
    m.materialTableOffset = r.u64_();
    for (u32 i = 0; i < kGmeshV3MaxAttrs; ++i) {
        GMeshV3Attr& a = m.attrs[i];
        a.semantic = r.u8_();
        a.storage = r.u8_();
        a.normalized = r.u8_();
        a.reserved = r.u8_();
        a.reserved2 = r.u32_();
    }
    (void)r.u32_();
    (void)r.u32_();
    if (r.bad) {
        err = "metadados do .gmesh v3 truncados";
        return false;
    }
    return true;
}

void v3WriteBlockEntry(Writer& w, const GMeshV3Block& b) {
    w.u64_(b.dataOffset);
    w.u64_(b.dataSize);
    w.u64_(b.vertexCount);
    w.u64_(b.indexCount);
    w.f32_(b.aabbMin.x); w.f32_(b.aabbMin.y); w.f32_(b.aabbMin.z);
    w.f32_(b.aabbMax.x); w.f32_(b.aabbMax.y); w.f32_(b.aabbMax.z);
    w.u32_(b.materialIndex);
    w.u32_(b.indexType);
    w.u32_(b.crc32);
    w.u32_(0);
    w.u64_(0);
}

bool v3ReadBlockEntry(Reader& r, GMeshV3Block& b, std::string& err) {
    b.dataOffset = r.u64_();
    b.dataSize = r.u64_();
    b.vertexCount = r.u64_();
    b.indexCount = r.u64_();
    b.aabbMin.x = r.f32_(); b.aabbMin.y = r.f32_(); b.aabbMin.z = r.f32_();
    b.aabbMax.x = r.f32_(); b.aabbMax.y = r.f32_(); b.aabbMax.z = r.f32_();
    b.materialIndex = r.u32_();
    b.indexType = r.u32_();
    b.crc32 = r.u32_();
    (void)r.u32_();
    (void)r.u64_();
    if (r.bad) {
        err = "tabela de blocos do .gmesh v3 truncada";
        return false;
    }
    return true;
}

void v3WriteMaterial(Writer& w, const std::string& s, u32& cursor) {
    const u16 n = static_cast<u16>(s.size() > 65535 ? 65535 : s.size());
    w.u16_(n);
    w.bytes_(reinterpret_cast<const u8*>(s.data()), n);
    cursor += 2u + n;
    while (cursor % 4 != 0) {   // 4-alinhado (as entradas são variáveis)
        w.u8_(0);
        ++cursor;
    }
}

} // namespace

// ---- o ESCRITOR v3 PARTILHADO (0.10-M PASSO 3) ------------------------------
// A serialização da ESQUELETO (header+meta+tabela+materiais) usada pelos
// DOIS escritores: writeGMesh (abaixo) e o conversor streaming
// (GmeshV3Stream). A aritmética de offsets vive SÓ AQUI — um escritor não
// pode divergir do outro (o layout é: dados 16-alinhados, tabela a seguir,
// materiais no fim; o checksum do header cobre os 160 B de metadados).
bool gmeshV3Skeleton(GMeshV3Meta& meta,
                     std::vector<GMeshV3Block>& entries,
                     const std::vector<std::string>& materials,
                     GMeshV3Skeleton& sk, std::string& err) {
    sk = GMeshV3Skeleton{};
    if (entries.empty()) {
        err = "v3 sem blocos — nada a escrever";
        return false;
    }
    if (meta.materialCount != materials.size()) {
        err = "v3: materialCount (" + std::to_string(meta.materialCount) +
              ") != materiais dados (" + std::to_string(materials.size()) +
              ")";
        return false;
    }
    const u32 stride = meta.vertexStride();
    // ---- o layout analítico (tudo conhecido ANTES de escrever) ------
    u64 cursor = kGHeaderBytes + kGmeshV3MetaBytes;
    for (size_t i = 0; i < entries.size(); ++i) {
        GMeshV3Block& e = entries[i];
        if (e.dataSize !=
            e.vertexCount * stride +
                e.indexCount * (e.indexType == 1 ? 4ull : 2ull)) {
            err = "v3: dataSize do bloco #" + std::to_string(i) +
                  " não bate com as contagens e o layout de atributos";
            return false;
        }
        if (e.indexType == 0 && e.vertexCount > 65535) {
            err = "v3: bloco #" + std::to_string(i) + " com " +
                  std::to_string(e.vertexCount) +
                  " vértices não cabe em índices locais u16";
            return false;
        }
        cursor += (16 - cursor % 16) % 16;   // os dados alinham-se a 16
        e.dataOffset = cursor;
        cursor += e.dataSize;
    }
    cursor += (16 - cursor % 16) % 16;
    meta.blockTableOffset = cursor;
    cursor += entries.size() * kGmeshV3BlockEntryBytes;
    meta.materialTableOffset = cursor;
    for (const std::string& s : materials) {
        cursor += 2ull + (s.size() > 65535 ? 65535ull : s.size());
        cursor += (4 - cursor % 4) % 4;
    }
    meta.blockCount = entries.size();
    const u64 payloadSize = cursor - kGHeaderBytes;
    // ---- a tabela (com o CRC de cada entrada já preenchido) --------
    {
        Writer tw(sk.table);
        for (const GMeshV3Block& e : entries) v3WriteBlockEntry(tw, e);
        meta.tableCrc32 = gcrc32(sk.table.data(), sk.table.size());
    }
    // ---- os metadados (com o CRC final) + o header (com o checksum) -
    {
        Writer mw(sk.meta);
        v3WriteMeta(mw, meta);
        Writer hw(sk.header);
        writeHeader(hw, "GMES", payloadSize,
                    gfnv1a(sk.meta.data(), sk.meta.size()),
                    kGmeshVersionWrite);
    }
    // ---- os materiais (o cursor REAL — o alinhamento 4 depende dele) -
    {
        Writer mw(sk.materials);
        u32 cur = static_cast<u32>(meta.materialTableOffset);
        for (const std::string& s : materials) v3WriteMaterial(mw, s, cur);
    }
    return true;
}

u32 GMeshV3Meta::vertexStride() const {
    u32 s = 0;
    for (u32 i = 0; i < attrCount && i < kGmeshV3MaxAttrs; ++i) {
        s += v3AttrBytes(attrs[i]);
    }
    return s;
}

bool GMeshV3Meta::hasAttr(u8 semantic) const {
    return attr(semantic) != nullptr;
}

const GMeshV3Attr* GMeshV3Meta::attr(u8 semantic) const {
    for (u32 i = 0; i < attrCount && i < kGmeshV3MaxAttrs; ++i) {
        if (attrs[i].semantic == semantic) return &attrs[i];
    }
    return nullptr;
}

// 0.10-M (PASSO 3B) — o ESPião do GUARDO do load: header + meta do .gmesh v3
// validados num PEQUENO buffer (kGHeaderBytes + kGmeshV3MetaBytes = 192 B —
// o ResourceManager lê SÓ isto com readBytesAt ANTES de decidir ler o
// ficheiro inteiro). O decode dos campos é o MESMO v3ReadMeta (zero drift);
// as validações são as que não dependem do comprimento do ficheiro. O
// gReadHeader de sempre NÃO serve aqui: recusa payloadSize > len (o payload
// do v3 é o ficheiro quase inteiro) — o peek decodifica o header à mão.
bool gmeshV3PeekMeta(const u8* bytes, size_t len, GMeshV3Meta& meta,
                     std::string& err) {
    v3Reset(meta);
    if (!bytes || len < kGHeaderBytes + kGmeshV3MetaBytes) {
        err = "peek curto demais (" + std::to_string(len) + " B)";
        return false;
    }
    // o header comum, decodificado à mão (as validações de sempre)
    GFileHeader h;
    for (int i = 0; i < 4; ++i) {
        h.magic[i] = static_cast<char>(bytes[i]);
    }
    h.version = static_cast<u16>(bytes[4] | (bytes[5] << 8));
    h.endianMark = static_cast<u16>(bytes[6] | (bytes[7] << 8));
    h.align = static_cast<u32>(bytes[8]) | (static_cast<u32>(bytes[9]) << 8) |
              (static_cast<u32>(bytes[10]) << 16) |
              (static_cast<u32>(bytes[11]) << 24);
    u64 payload = 0, checksum = 0;
    for (int b = 0; b < 8; ++b) {
        payload |= static_cast<u64>(bytes[12 + b]) << (8 * b);
        checksum |= static_cast<u64>(bytes[20 + b]) << (8 * b);
    }
    // (bytes 28..31 = reserved — o padding até os 32 B do CONTRATO)
    h.payloadSize = payload;
    h.checksum = checksum;
    if (std::memcmp(h.magic, "GMES", 4) != 0) {
        err = "magic errado (esperado GMES): não é um ficheiro próprio válido";
        return false;
    }
    if (h.endianMark != 0x1A2B) {
        err = "endianess trocada (marca 0x" + std::to_string(h.endianMark) +
              ") — ficheiro de outra plataforma";
        return false;
    }
    if (h.version != 3) {
        err = "não é um .gmesh v3 (version=" + std::to_string(h.version) + ")";
        return false;
    }
    if (h.payloadSize < kGmeshV3MetaBytes) {
        err = "metadados do .gmesh v3 truncados (payload " +
              std::to_string(h.payloadSize) + " B < " +
              std::to_string(kGmeshV3MetaBytes) + ")";
        return false;
    }
    // o checksum do header num v3 cobre SÓ os metadados — com 192 B isto é
    // a validação COMPLETA do header+meta (a tabela tem o seu próprio CRC,
    // verificado no caminho inteiro)
    const u64 got = gfnv1a(bytes + kGHeaderBytes, kGmeshV3MetaBytes);
    if (got != h.checksum) {
        err = "CHECKSUM CORROMPIDO: metadados dizem 0x" +
              std::to_string(h.checksum) + ", recalculado 0x" +
              std::to_string(got) + " — o ficheiro foi danificado";
        return false;
    }
    Reader r(bytes + kGHeaderBytes, kGmeshV3MetaBytes);
    if (!v3ReadMeta(r, meta, err)) {
        return false;
    }
    if (meta.attrCount == 0 || meta.attrCount > kGmeshV3MaxAttrs) {
        err = "número de atributos inválido no .gmesh v3 (" +
              std::to_string(meta.attrCount) + ")";
        return false;
    }
    bool hasPos = false;
    for (u32 i = 0; i < meta.attrCount; ++i) {
        const GMeshV3Attr& a = meta.attrs[i];
        if (v3AttrComponents(a.semantic) == 0) {
            err = "semântica de atributo desconhecida no .gmesh v3 (" +
                  std::to_string(a.semantic) + ")";
            return false;
        }
        if (a.storage > kAttrU8) {
            err = "armazenamento de atributo desconhecido no .gmesh v3 (" +
                  std::to_string(a.storage) + ")";
            return false;
        }
        if (a.semantic == kAttrPosition) hasPos = true;
    }
    if (!hasPos) {
        err = "o layout de atributos do .gmesh v3 não tem POSITION";
        return false;
    }
    if (meta.vertexCount == 0 || meta.indexCount == 0 ||
        (meta.indexCount % 3) != 0) {
        err = "contagens inválidas no .gmesh v3 (verts=" +
              std::to_string(meta.vertexCount) + " idx=" +
              std::to_string(meta.indexCount) + ")";
        return false;
    }
    if (meta.blockCount == 0 || meta.blockVertexCap == 0) {
        err = "blocos/cap inválidos no .gmesh v3 (blocos=" +
              std::to_string(meta.blockCount) + " cap=" +
              std::to_string(meta.blockVertexCap) + ")";
        return false;
    }
    return true;
}

bool readGMeshV3Meta(const u8* bytes, size_t len, GMeshV3Meta& meta,
                     std::vector<GMeshV3Block>& blocks,
                     std::vector<std::string>& materials, std::string& err) {
    v3Reset(meta);
    blocks.clear();
    materials.clear();
    GFileHeader h;
    if (!gReadHeader(bytes, len, "GMES", h, err)) {
        return false;
    }
    if (h.version != 3) {
        err = "não é um .gmesh v3 (version=" + std::to_string(h.version) + ")";
        return false;
    }
    if (h.payloadSize < kGmeshV3MetaBytes) {
        err = "metadados do .gmesh v3 truncados (payload " +
              std::to_string(h.payloadSize) + " B < " +
              std::to_string(kGmeshV3MetaBytes) + ")";
        return false;
    }
    // o checksum do header num v3 cobre SÓ os metadados
    const u64 got = gfnv1a(bytes + kGHeaderBytes, kGmeshV3MetaBytes);
    if (got != h.checksum) {
        err = "CHECKSUM CORROMPIDO: metadados dizem 0x" +
              std::to_string(h.checksum) + ", recalculado 0x" +
              std::to_string(got) + " — o ficheiro foi danificado";
        return false;
    }
    Reader r(bytes + kGHeaderBytes, kGmeshV3MetaBytes);
    if (!v3ReadMeta(r, meta, err)) {
        return false;
    }
    if (meta.attrCount == 0 || meta.attrCount > kGmeshV3MaxAttrs) {
        err = "número de atributos inválido no .gmesh v3 (" +
              std::to_string(meta.attrCount) + ")";
        return false;
    }
    bool hasPos = false;
    for (u32 i = 0; i < meta.attrCount; ++i) {
        const GMeshV3Attr& a = meta.attrs[i];
        if (v3AttrComponents(a.semantic) == 0) {
            err = "semântica de atributo desconhecida no .gmesh v3 (" +
                  std::to_string(a.semantic) + ")";
            return false;
        }
        if (a.storage > kAttrU8) {
            err = "armazenamento de atributo desconhecido no .gmesh v3 (" +
                  std::to_string(a.storage) + ")";
            return false;
        }
        if (a.semantic == kAttrPosition) hasPos = true;
    }
    if (!hasPos) {
        err = "o layout de atributos do .gmesh v3 não tem POSITION";
        return false;
    }
    if (meta.vertexCount == 0 || meta.indexCount == 0 ||
        (meta.indexCount % 3) != 0) {
        err = "contagens inválidas no .gmesh v3 (verts=" +
              std::to_string(meta.vertexCount) + " idx=" +
              std::to_string(meta.indexCount) + ")";
        return false;
    }
    if (meta.blockCount == 0 || meta.blockVertexCap == 0) {
        // o cap é CONFIGURÁVEL pela spec (u32 inteiro); a proteção do
        // índice é o indexType de cada bloco (u16 exige ≤65535)
        err = "blocos/cap inválidos no .gmesh v3 (blocos=" +
              std::to_string(meta.blockCount) + " cap=" +
              std::to_string(meta.blockVertexCap) + ")";
        return false;
    }
    // a TABELA tem de caber no ficheiro (a multiplicação é segura: cada
    // entrada tem 80 B — um bloco nunca cabe em menos de 80 B de ficheiro)
    if (meta.blockCount > static_cast<u64>(len) ||
        meta.blockTableOffset > static_cast<u64>(len) ||
        meta.blockTableOffset + meta.blockCount * kGmeshV3BlockEntryBytes >
            static_cast<u64>(len)) {
        err = "tabela de blocos do .gmesh v3 fora do ficheiro (offset " +
              std::to_string(meta.blockTableOffset) + ", blocos " +
              std::to_string(meta.blockCount) + ")";
        return false;
    }
    if (meta.materialCount > static_cast<u64>(len) ||
        meta.materialTableOffset > static_cast<u64>(len) ||
        meta.materialTableOffset <
            meta.blockTableOffset +
                meta.blockCount * kGmeshV3BlockEntryBytes) {
        err = "tabela de materiais do .gmesh v3 fora do sítio (offset " +
              std::to_string(meta.materialTableOffset) + " antes do fim da "
              "tabela de blocos)";
        return false;
    }
    // as ENTRADAS (bounded pelo ficheiro — nunca pelos dados dos blocos)
    blocks.resize(static_cast<size_t>(meta.blockCount));
    Reader tr(bytes + meta.blockTableOffset,
              static_cast<size_t>(meta.blockCount * kGmeshV3BlockEntryBytes));
    const u8* tableStart = bytes + meta.blockTableOffset;
    for (u64 i = 0; i < meta.blockCount; ++i) {
        if (!v3ReadBlockEntry(tr, blocks[static_cast<size_t>(i)], err)) {
            return false;
        }
        const GMeshV3Block& b = blocks[static_cast<size_t>(i)];
        if (b.indexType > 1 || b.vertexCount == 0 ||
            (b.indexCount % 3) != 0 || b.materialIndex >= meta.materialCount) {
            err = "entrada de bloco inválida no .gmesh v3 (#" +
                  std::to_string(i) + ")";
            return false;
        }
        if (b.indexType == 0 && b.vertexCount > 65535) {
            err = "bloco #" + std::to_string(i) +
                  " usa índices u16 com " + std::to_string(b.vertexCount) +
                  " vértices — ilegal";
            return false;
        }
        if (b.vertexCount > meta.blockVertexCap) {
            err = "bloco #" + std::to_string(i) + " com " +
                  std::to_string(b.vertexCount) +
                  " vértices excede o cap (" +
                  std::to_string(meta.blockVertexCap) + ")";
            return false;
        }
        // N.B. os dados do bloco podem viver FORA do ficheiro na leitura
        // de metadados (o contrato do dono: header+tabela sem alocar os
        // dados — offsets de 64 bits declarados à frente dos bytes); a
        // validação do fit acontece no readGMeshV3Block, ao materializar.
        const u64 expect = b.vertexCount * meta.vertexStride() +
                           b.indexCount * (b.indexType ? 4u : 2u);
        if (expect != b.dataSize) {
            err = "bloco #" + std::to_string(i) + " com tamanho " +
                  std::to_string(b.dataSize) + " B ≠ esperado " +
                  std::to_string(expect) + " B (stride " +
                  std::to_string(meta.vertexStride()) + ")";
            return false;
        }
    }
    // o CRC32 da PRÓPRIA tabela (a integridade do índice)
    const u32 tableCrc = gcrc32(
        tableStart,
        static_cast<size_t>(meta.blockCount * kGmeshV3BlockEntryBytes));
    if (tableCrc != meta.tableCrc32) {
        err = "CHECKSUM CORROMPIDO: a tabela de blocos (CRC32 0x" +
              std::to_string(meta.tableCrc32) + ", recalculado 0x" +
              std::to_string(tableCrc) + ") — o ficheiro foi danificado";
        return false;
    }
    // os MATERIAIS (nomes — 4-alinhados, bounded pelo offset da tabela)
    materials.resize(static_cast<size_t>(meta.materialCount));
    Reader mr(bytes + meta.materialTableOffset,
              static_cast<size_t>(meta.blockTableOffset -
                                  meta.materialTableOffset));
    for (u64 i = 0; i < meta.materialCount; ++i) {
        materials[static_cast<size_t>(i)] = mr.str_();
        // o pad a 4 (o escritor alinha cada nome)
        const u64 used = mr.i % 4;
        if (used != 0) {
            const u64 skip = 4 - used;
            for (u64 k = 0; k < skip; ++k) (void)mr.u8_();
        }
        if (mr.bad) {
            err = "tabela de materiais do .gmesh v3 truncada";
            return false;
        }
    }
    return true;
}

bool readGMeshV3Block(const u8* bytes, size_t len, const GMeshV3Meta& meta,
                      const GMeshV3Block& blk, MeshData& out,
                      std::string& err) {
    out = MeshData{};
    if (blk.dataOffset > static_cast<u64>(len) ||
        blk.dataSize > static_cast<u64>(len) - blk.dataOffset) {
        err = "dados do bloco fora do ficheiro";
        return false;
    }
    const u8* data = bytes + blk.dataOffset;
    // a integridade ANTES de qualquer parse (o CRC32 do bloco)
    const u32 crc = gcrc32(data, static_cast<size_t>(blk.dataSize));
    if (crc != blk.crc32) {
        err = "CHECKSUM CORROMPIDO: bloco de " +
              std::to_string(blk.vertexCount) + " verts (CRC32 0x" +
              std::to_string(blk.crc32) + ", recalculado 0x" +
              std::to_string(crc) + ") — o ficheiro foi danificado";
        return false;
    }
    const u32 stride = meta.vertexStride();
    const u64 vb = blk.vertexCount * stride;
    const u64 ib = blk.indexCount * (blk.indexType ? 4u : 2u);
    if (vb + ib != blk.dataSize) {
        err = "tamanhos do bloco inconsistentes (stride " +
              std::to_string(stride) + ")";
        return false;
    }
    // os vértices (o cursor anda pelos ATRIBUTOS — o layout é o descrito)
    out.vertices.resize(static_cast<size_t>(blk.vertexCount));
    const u32 attrCount = meta.attrCount < kGmeshV3MaxAttrs
                              ? meta.attrCount
                              : kGmeshV3MaxAttrs;
    const bool wantSkin = (meta.flags & 1u) != 0 && meta.hasAttr(kAttrBones) &&
                          meta.hasAttr(kAttrWeights);
    for (u64 v = 0; v < blk.vertexCount; ++v) {
        const u8* p = data + v * stride;
        Vertex& vt = out.vertices[static_cast<size_t>(v)];
        for (u32 ai = 0; ai < attrCount; ++ai) {
            const GMeshV3Attr& a = meta.attrs[ai];
            f32 comp[4] = {0, 0, 0, 0};
            const u32 n = v3AttrComponents(a.semantic);
            for (u32 c = 0; c < n; ++c) {
                if (a.storage == kAttrF32) {
                    f32 f;
                    std::memcpy(&f, p, 4);
                    comp[c] = f;
                    p += 4;
                } else if (a.storage == kAttrU16) {
                    u16 q;
                    std::memcpy(&q, p, 2);
                    p += 2;
                    comp[c] = a.normalized
                                  ? static_cast<f32>(q) / 65535.0f
                                  : static_cast<f32>(q);
                } else {
                    comp[c] = a.normalized
                                  ? static_cast<f32>(*p) / 255.0f
                                  : static_cast<f32>(*p);
                    p += 1;
                }
            }
            switch (a.semantic) {
                case kAttrPosition:
                    vt.pos = Vec3{comp[0], comp[1], comp[2]};
                    break;
                case kAttrNormal:
                    vt.normal = Vec3{comp[0], comp[1], comp[2]};
                    break;
                case kAttrUv0:
                    vt.uv = Vec2{comp[0], comp[1]};
                    break;
                case kAttrBones:
                    if (wantSkin) {
                        for (u32 c = 0; c < 4; ++c) {
                            out.skinJoints.push_back(static_cast<u8>(comp[c]));
                        }
                    }
                    break;
                case kAttrWeights:
                    if (wantSkin) {
                        for (u32 c = 0; c < 4; ++c) {
                            out.skinWeights.push_back(comp[c]);
                        }
                    }
                    break;
                default:
                    break;   // tangente/UV1/cor: lidos e ignorados (o
                             // MeshData de hoje não os tem)
            }
        }
    }
    // os índices (locais ao bloco)
    const u8* ip = data + vb;
    out.indices.resize(static_cast<size_t>(blk.indexCount));
    for (u64 i = 0; i < blk.indexCount; ++i) {
        u32 idx;
        if (blk.indexType) {
            std::memcpy(&idx, ip + i * 4, 4);
        } else {
            u16 q;
            std::memcpy(&q, ip + i * 2, 2);
            idx = q;
        }
        if (idx >= blk.vertexCount) {
            err = "índice local fora da pool do bloco (" +
                  std::to_string(idx) + " ≥ " +
                  std::to_string(blk.vertexCount) + ")";
            return false;
        }
        out.indices[static_cast<size_t>(i)] = idx;
    }
    MeshData::Group g;
    g.firstIndex = 0;
    g.indexCount = static_cast<u32>(blk.indexCount);
    out.groups.push_back(g);   // nome/material preenche o CHAMADOR
    out.name = "gmesh";
    return true;
}

// ---- o ESCRITOR v3 (o único escritor desde o 0.10-M) ----------------------------

namespace {

// corta UMA faixa de índices (um grupo) em blocos de ≤ cap vértices — a
// MESMA rotina que o conversor streaming do PASSO 3 usa por primitiva.
// A pool do bloco = os vértices REFERENCIADOS (1.ª referência —
// determinístico, sem soldadura); o corte só acontece em FRONTEIRA DE
// TRIÂNGULO (um triângulo nunca fica partido por dois blocos).
bool v3CutGroup(const MeshData& m, u32 firstIndex, u32 indexCount,
                const std::string& material, u32 cap, bool skinned,
                std::vector<GMeshV3BlockIn>& outBlocks, std::string& err) {
    GMeshV3BlockIn bin;
    std::vector<u32> remap(m.vertices.size(), 0xFFFFFFFFu);
    std::vector<u32> orig;   // a origem de CADA vértice da pool (para a
                             // ordenação ascendente no fecho do bloco)
    auto flush = [&]() {
        if (bin.vertices.empty()) return;
        // a pool do bloco sai em ordem ASCENDENTE do índice original —
        // o round-trip de um mesh sem soldadura fica IDÊNTICO ao de entrada
        std::vector<u32> order(bin.vertices.size());
        for (u32 i = 0; i < order.size(); ++i) order[i] = i;
        std::sort(order.begin(), order.end(), [&](u32 a, u32 b) {
            return orig[a] < orig[b];
        });
        std::vector<u32> newPos(bin.vertices.size());
        GMeshV3BlockIn sorted;
        sorted.material = bin.material;
        sorted.use32 = bin.use32;
        sorted.vertices.resize(bin.vertices.size());
        if (skinned) {
            sorted.bones.resize(bin.bones.size());
            sorted.weights.resize(bin.weights.size());
        }
        for (u32 n = 0; n < order.size(); ++n) {
            const u32 oldPos = order[n];
            newPos[oldPos] = n;
            sorted.vertices[n] = bin.vertices[oldPos];
            if (skinned) {
                for (int c = 0; c < 4; ++c) {
                    sorted.bones[n * 4 + c] = bin.bones[oldPos * 4 + c];
                    sorted.weights[n * 4 + c] =
                        bin.weights[oldPos * 4 + c];
                }
            }
        }
        for (const u32 li : bin.indices32) {
            sorted.indices32.push_back(newPos[li]);
        }
        outBlocks.push_back(std::move(sorted));
        bin = GMeshV3BlockIn{};
        bin.material = material;
        std::fill(remap.begin(), remap.end(), 0xFFFFFFFFu);
        orig.clear();
    };
    bin.material = material;
    if ((indexCount % 3) != 0) {
        err = "grupo com " + std::to_string(indexCount) +
              " índices — não é múltiplo de 3 (triângulos)";
        return false;
    }
    for (u32 t = 0; t < indexCount; t += 3) {
        u32 tri[3];
        for (int k = 0; k < 3; ++k) {
            const u32 gi = m.indices[static_cast<size_t>(firstIndex) + t + k];
            if (gi >= m.vertices.size()) {
                err = "índice " + std::to_string(gi) +
                      " fora da pool de vértices do mesh";
                return false;
            }
            tri[k] = gi;
        }
        // quantos vértices NOVOS este triângulo traz?
        u32 fresh = 0;
        for (int k = 0; k < 3; ++k) {
            if (remap[tri[k]] == 0xFFFFFFFFu) ++fresh;
        }
        if (bin.vertices.size() + fresh > cap && !bin.vertices.empty()) {
            flush();   // fronteira de triângulo: o bloco fecha INTEIRO
        }
        for (int k = 0; k < 3; ++k) {
            const u32 gi = tri[k];
            if (remap[gi] == 0xFFFFFFFFu) {
                remap[gi] = static_cast<u32>(bin.vertices.size());
                bin.vertices.push_back(m.vertices[gi]);
                orig.push_back(gi);
                if (skinned) {
                    for (int c = 0; c < 4; ++c) {
                        bin.bones.push_back(
                            m.skinJoints[static_cast<size_t>(gi) * 4 +
                                         static_cast<size_t>(c)]);
                    }
                    for (int c = 0; c < 4; ++c) {
                        bin.weights.push_back(
                            m.skinWeights[static_cast<size_t>(gi) * 4 +
                                          static_cast<size_t>(c)]);
                    }
                }
            }
            bin.indices32.push_back(remap[gi]);
        }
    }
    flush();
    return true;
}

void v3FillAabb(GMeshV3BlockIn& b) {
    Vec3 mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
    for (const Vertex& v : b.vertices) {
        mn.x = mn.x < v.pos.x ? mn.x : v.pos.x;
        mn.y = mn.y < v.pos.y ? mn.y : v.pos.y;
        mn.z = mn.z < v.pos.z ? mn.z : v.pos.z;
        mx.x = mx.x > v.pos.x ? mx.x : v.pos.x;
        mx.y = mx.y > v.pos.y ? mx.y : v.pos.y;
        mx.z = mx.z > v.pos.z ? mx.z : v.pos.z;
    }
    b.aabbMin = mn;
    b.aabbMax = mx;
}

} // namespace

bool writeGMesh(const MeshData& m, std::vector<u8>& out, std::string& err) {
    out.clear();
    if (m.vertices.empty() || m.indices.empty()) {
        err = "geometria vazia — nada a converter";
        return false;
    }
    const bool skinned = m.skinned();
    // ---- o corte em blocos: 1 bloco POR GRUPO (sem grupos → 1 único)
    std::vector<GMeshV3BlockIn> bins;
    if (m.groups.empty()) {
        if (!v3CutGroup(m, 0, static_cast<u32>(m.indices.size()), "",
                        kGmeshV3BlockVertexCap, skinned, bins, err)) {
            return false;
        }
    } else {
        for (const MeshData::Group& g : m.groups) {
            if (!v3CutGroup(m, g.firstIndex, g.indexCount, g.material,
                            kGmeshV3BlockVertexCap, skinned, bins, err)) {
                return false;
            }
        }
    }
    for (GMeshV3BlockIn& b : bins) v3FillAabb(b);
    // ---- os atributos canónicos (float32 SEM PERDA + skin quando há)
    GMeshV3Meta meta;
    meta.flags = skinned ? 1u : 0u;
    meta.blockVertexCap = kGmeshV3BlockVertexCap;
    meta.attrs[0] = GMeshV3Attr{kAttrPosition, kAttrF32, 0, 0, 0};
    meta.attrs[1] = GMeshV3Attr{kAttrNormal, kAttrF32, 0, 0, 0};
    meta.attrs[2] = GMeshV3Attr{kAttrUv0, kAttrF32, 0, 0, 0};
    if (skinned) {
        meta.attrs[3] = GMeshV3Attr{kAttrBones, kAttrU8, 0, 0, 0};
        meta.attrs[4] = GMeshV3Attr{kAttrWeights, kAttrF32, 0, 0, 0};
        meta.attrCount = 5;
    } else {
        meta.attrCount = 3;
    }
    const u32 stride = meta.vertexStride();
    u64 totalV = 0, totalI = 0;
    Vec3 gmn{1e30f, 1e30f, 1e30f}, gmx{-1e30f, -1e30f, -1e30f};
    for (GMeshV3BlockIn& b : bins) {
        totalV += b.vertices.size();
        totalI += b.indices32.size();
        // o tipo do índice decide-se pelo TAMANHO DA POOL (a pool ≤65535
        // leva u16; acima disso — cap configurável — leva u32)
        b.use32 = b.vertices.size() > 65535;
        gmn.x = (std::min)(gmn.x, b.aabbMin.x);
        gmn.y = (std::min)(gmn.y, b.aabbMin.y);
        gmn.z = (std::min)(gmn.z, b.aabbMin.z);
        gmx.x = (std::max)(gmx.x, b.aabbMax.x);
        gmx.y = (std::max)(gmx.y, b.aabbMax.y);
        gmx.z = (std::max)(gmx.z, b.aabbMax.z);
    }
    meta.vertexCount = totalV;
    meta.indexCount = totalI;
    meta.blockCount = bins.size();
    meta.aabbMin = gmn;
    meta.aabbMax = gmx;
    // ---- os materiais (dedup em ordem de 1.ª utilização)
    std::vector<std::string> mats;
    std::vector<u32> matIdx(bins.size());
    for (size_t i = 0; i < bins.size(); ++i) {
        u32 found = 0xFFFFFFFFu;
        for (size_t k = 0; k < mats.size(); ++k) {
            if (mats[k] == bins[i].material) {
                found = static_cast<u32>(k);
                break;
            }
        }
        if (found == 0xFFFFFFFFu) {
            mats.push_back(bins[i].material);
            found = static_cast<u32>(mats.size() - 1);
        }
        matIdx[i] = found;
    }
    meta.materialCount = mats.size();
    // ---- a serialização de UM bloco (a MESMA nos dois passes abaixo) ---
    auto serializeBlock = [&](const GMeshV3BlockIn& b, std::vector<u8>& blob) {
        Writer bw(blob);
        for (const Vertex& v : b.vertices) {
            bw.f32_(v.pos.x); bw.f32_(v.pos.y); bw.f32_(v.pos.z);
            bw.f32_(v.normal.x); bw.f32_(v.normal.y); bw.f32_(v.normal.z);
            bw.f32_(v.uv.x); bw.f32_(v.uv.y);
            if (skinned) {
                const size_t vi = &v - b.vertices.data();
                for (int c = 0; c < 4; ++c) {
                    bw.u8_(b.bones[vi * 4 + static_cast<size_t>(c)]);
                }
                for (int c = 0; c < 4; ++c) {
                    bw.f32_(b.weights[vi * 4 + static_cast<size_t>(c)]);
                }
            }
        }
        if (b.use32) {
            for (const u32 idx : b.indices32) bw.u32_(idx);
        } else {
            for (const u32 idx : b.indices32) bw.u16_(static_cast<u16>(idx));
        }
    };
    // ---- as ENTRADAS da tabela (tamanhos + CRCs — o blob de cada bloco
    // serializa-se UMA vez para o CRC e é descartado; a emissão abaixo
    // reconstrói e CONFIRMA o CRC — o custo é CPU, nunca a memória)
    std::vector<GMeshV3Block> entries(bins.size());
    for (size_t i = 0; i < bins.size(); ++i) {
        const GMeshV3BlockIn& b = bins[i];
        GMeshV3Block& e = entries[i];
        const u64 vb = b.vertices.size() * stride;
        const u64 ib = b.indices32.size() * (b.use32 ? 4ull : 2ull);
        e.dataSize = vb + ib;
        e.vertexCount = b.vertices.size();
        e.indexCount = b.indices32.size();
        e.aabbMin = b.aabbMin;
        e.aabbMax = b.aabbMax;
        e.materialIndex = matIdx[i];
        e.indexType = b.use32 ? 1u : 0u;
        std::vector<u8> blob;
        serializeBlock(b, blob);
        e.crc32 = gcrc32(blob.data(), blob.size());
    }
    // ---- a ESQUELETO PARTILHADA (offsets + header + meta + tabela +
    // materiais — a MESMA rotina que o conversor streaming usa: zero
    // drift de formato entre os dois escritores)
    GMeshV3Skeleton sk;
    if (!gmeshV3Skeleton(meta, entries, mats, sk, err)) {
        return false;
    }
    // ---- a emissão (header → metadados → blocos 16-alinhados → tabela
    // → materiais; a ORDEM é a mesma do layout analítico da esqueleto)
    out = sk.header;
    out.insert(out.end(), sk.meta.begin(), sk.meta.end());
    for (size_t i = 0; i < bins.size(); ++i) {
        while (out.size() < entries[i].dataOffset) out.push_back(0);
        std::vector<u8> blob;
        serializeBlock(bins[i], blob);
        if (gcrc32(blob.data(), blob.size()) != entries[i].crc32) {
            err = "v3: o CRC do bloco #" + std::to_string(i) +
                  " divergiu entre a pré-passada e a emissão (bug interno)";
            return false;
        }
        out.insert(out.end(), blob.begin(), blob.end());
    }
    // o PAD de 16-alinhamento ANTES da tabela (a esqueleto decidiu o
    // offset; a emissão tem de o respeitar — um mesh com dataSize não
    // múltiplo de 16 deixa um vão aqui)
    while (out.size() < meta.blockTableOffset) out.push_back(0);
    out.insert(out.end(), sk.table.begin(), sk.table.end());
    out.insert(out.end(), sk.materials.begin(), sk.materials.end());
    return true;
}

// ---- o dispatcher de leitura (v1/v2 payload cru · v3 blocos) ----------------

// 0.10-M (PASSO 3B) — a estimativa PURA do MeshData único: o que o load de
// hoje materializaria (RAM) antes do upload GPU. A fórmula é o CONTRATO
// (aferida no teste sem ficheiro): verts×sizeof(Vertex) + índices×2 (u16)
// + pele 20 B/vértice quando o meta diz skinned.
u64 gmeshV3LoadEstimateBytes(const GMeshV3Meta& meta) {
    const u64 skinPerVert = (meta.flags & 1u) ? 20ull : 0ull;
    return meta.vertexCount * (sizeof(Vertex) + skinPerVert) +
           meta.indexCount * sizeof(u16);
}

// 0.10-M (PASSO 3B) — A MENSAGEM da recusa (a que o dono pediu), UMA só
// fonte de verdade: o guard CEDO do ResourceManager (o espião de 192 B) e
// o readGMesh (a rede no caminho inteiro) dizem EXATAMENTE o mesmo.
std::string gmeshV3LoadRefusalErr(const GMeshV3Meta& meta) {
    // (construída por APPENDS, sem a cadeia de temporários do operator+ —
    // o GCC 13 do runner do CI também se engasgava aí a caminho do ICE)
    const u64 estimate = gmeshV3LoadEstimateBytes(meta);
    const bool overBudget = estimate > kMeshLoadBudgetBytes;
    std::string err;
    err.reserve(384);
    err += "memória insuficiente ao carregar mesh (cura no PASSO 4: render ";
    err += "por blocos): ";
    err += std::to_string(meta.vertexCount);
    err += " vértices em ";
    err += std::to_string(meta.blockCount);
    err += " blocos (";
    err += std::to_string(meta.indexCount);
    err += " índices) — o runtime de hoje monta o mesh ÚNICO em RAM (~";
    err += std::to_string(estimate / (1024 * 1024));
    err += " MB";
    if (overBudget) {
        err += ", acima do orçamento de ";
        err += std::to_string(kMeshLoadBudgetBytes / (1024 * 1024));
        err += " MB";
    }
    err += "). O FICHEIRO ESTÁ CORRETO — a conversão verificou-o; nada foi ";
    err += "perdido. O ficheiro abre por blocos (tabela v3) quando o render ";
    err += "por blocos existir.";
    return err;
}

bool readGMesh(const u8* bytes, size_t len, MeshData& out, std::string& err) {
    out = MeshData{};
    if (len < 6) {
        err = "ficheiro próprio curto demais (" + std::to_string(len) + " B)";
        return false;
    }
    const u16 ver = static_cast<u16>(bytes[4] | (bytes[5] << 8));
    if (ver <= 2) {
        return readGMeshV1Payload(bytes, len, out, err);
    }
    if (ver > 3) {
        err = "versão " + std::to_string(ver) +
              " desconhecida (o .gmesh lê 1..3)";
        return false;
    }
    // ---- v3: os blocos montados por ordem da TABELA (1 grupo por bloco)
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> mats;
    if (!readGMeshV3Meta(bytes, len, meta, blocks, mats, err)) {
        return false;
    }
    // 0.10-M (PASSO 3B) — A PAREDE DO LOAD DE HOJE, com nome e números: o
    // runtime monta o mesh ÚNICO (u16) — o modelo INTEIRO em RAM (+ o upload
    // GPU dele). A conversão está CORRETA (verificou-a bit a bit); a causa
    // do «fail de 203 MB» É este load inteiro; a cura é o PASSO 4 (render
    // por blocos) — NÃO se contorna aqui. A mensagem é a que o dono pediu.
    if (meta.vertexCount > 65535 ||
        gmeshV3LoadEstimateBytes(meta) > kMeshLoadBudgetBytes) {
        err = gmeshV3LoadRefusalErr(meta);
        return false;
    }
    u32 baseV = 0, baseI = 0;
    for (size_t i = 0; i < blocks.size(); ++i) {
        MeshData part;
        if (!readGMeshV3Block(bytes, len, meta, blocks[i], part, err)) {
            return false;
        }
        for (const Vertex& v : part.vertices) out.vertices.push_back(v);
        for (const u16 idx : part.indices) {
            out.indices.push_back(static_cast<u16>(baseV + idx));
        }
        MeshData::Group g;
        g.name = "bloco " + std::to_string(i + 1);
        g.material = blocks[i].materialIndex < mats.size()
                         ? mats[blocks[i].materialIndex]
                         : "";
        g.firstIndex = baseI;
        g.indexCount = static_cast<u32>(part.indices.size());
        out.groups.push_back(g);
        out.skinJoints.insert(out.skinJoints.end(), part.skinJoints.begin(),
                              part.skinJoints.end());
        out.skinWeights.insert(out.skinWeights.end(),
                               part.skinWeights.begin(),
                               part.skinWeights.end());
        baseV += static_cast<u32>(part.vertices.size());
        baseI += static_cast<u32>(part.indices.size());
    }
    out.name = "gmesh";
    return true;
}

// ---- .gtext --------------------------------------------------------------------
// payload: format u8, w u32, h u32, mipCount u32, mips×(w,h,off,size),
// blob (o mesmo layout contíguo do CompressedImage)
bool writeGText(const CompressedImage& img, std::vector<u8>& out,
                std::string& err) {
    out.clear();
    if (!img.ok()) {
        err = "imagem comprimida inválida (dims/mips/blob vazios)";
        return false;
    }
    std::vector<u8> payload;
    Writer w(payload);
    w.u8_(static_cast<u8>(img.format));
    w.u32_(img.width);
    w.u32_(img.height);
    w.u32_(static_cast<u32>(img.mips.size()));
    for (const CompressedMip& mip : img.mips) {
        w.u32_(mip.width);
        w.u32_(mip.height);
        w.u32_(mip.offset);
        w.u32_(mip.size);
    }
    w.bytes_(img.data.data(), img.data.size());
    Writer o(out);
    writeHeader(o, "GVTX", payload.size(),
                gfnv1a(payload.data(), payload.size()));
    o.bytes_(payload.data(), payload.size());
    return true;
}

bool readGText(const u8* bytes, size_t len, CompressedImage& out,
               std::string& err) {
    out = CompressedImage{};
    GFileHeader h;
    if (!gReadHeader(bytes, len, "GVTX", h, err)) {
        return false;
    }
    if (h.version != 1) {
        err = "versão " + std::to_string(h.version) +
              " desconhecida (o .gtext lê a 1)";
        return false;
    }
    Reader r(bytes + kGHeaderBytes, static_cast<size_t>(h.payloadSize));
    out.format = static_cast<CompressedFormat>(r.u8_());
    const int fmtInt = static_cast<int>(out.format);
    if (fmtInt < 0 || fmtInt > 4) {
        err = "formato de textura desconhecido no .gtext";
        return false;
    }
    out.width = r.u32_();
    out.height = r.u32_();
    const u32 nMips = r.u32_();
    if (r.bad || out.width == 0 || out.height == 0 || out.width > 16384 ||
        out.height > 16384 || nMips == 0 || nMips > 32) {
        err = "dims/mips inválidos no .gtext";
        return false;
    }
    for (u32 i = 0; i < nMips; ++i) {
        CompressedMip mip;
        mip.width = r.u32_();
        mip.height = r.u32_();
        mip.offset = r.u32_();
        mip.size = r.u32_();
        if (r.bad || mip.width == 0 || mip.height == 0 ||
            mip.width > out.width || mip.height > out.height) {
            err = "mip inválido no .gtext (nível " + std::to_string(i) + ")";
            return false;
        }
        out.mips.push_back(mip);
    }
    if (r.bad) {
        err = "tabela de mips truncada no .gtext";
        return false;
    }
    const size_t blobLen = static_cast<size_t>(h.payloadSize) - r.i;
    const u8* blob = r.bytes_(blobLen);
    if (r.bad || blob == nullptr) {
        err = "blob da textura truncado no .gtext";
        return false;
    }
    // valida os ranges dos mips contra o blob
    for (const CompressedMip& mip : out.mips) {
        if (static_cast<u64>(mip.offset) + mip.size > blobLen) {
            err = "mip fora do blob no .gtext (off " +
                  std::to_string(mip.offset) + ", tam " +
                  std::to_string(mip.size) + ", blob " +
                  std::to_string(blobLen) + ")";
            return false;
        }
    }
    out.data.assign(blob, blob + blobLen);
    return true;
}

// ---- .gm -------------------------------------------------------------------
// payload: clips u32, clips×(nome str, tracks u32,
// tracks×(target u8, curve u8, element str, keys u32,
// keys×(t f32, v×4 f32, in×4 f32, out×4 f32))),
// hasSkeleton u8, joints u32, joints×(nome str, pai i32, pos/rot/scale/ibm)
bool writeGAnim(const GAnimFile& anim, std::vector<u8>& out,
                std::string& err) {
    out.clear();
    const std::vector<AnimClip>& clips = anim.clips;
    if (anim.joints.size() > SkeletonComp::kMaxBones) {
        err = "esqueleto com " + std::to_string(anim.joints.size()) +
              " joints — o shader suporta " +
              std::to_string(SkeletonComp::kMaxBones);
        return false;
    }
    std::vector<u8> payload;
    Writer w(payload);
    w.u32_(static_cast<u32>(clips.size()));
    for (const AnimClip& c : clips) {
        w.str_(c.name);
        w.u32_(static_cast<u32>(c.tracks.size()));
        for (const AnimTrack& t : c.tracks) {
            w.u8_(static_cast<u8>(t.target));
            w.u8_(static_cast<u8>(t.curve));
            w.str_(t.element);
            w.u32_(static_cast<u32>(t.keys.size()));
            for (const AnimKey& k : t.keys) {
                w.f32_(k.t);
                for (int i = 0; i < 4; ++i) w.f32_(k.v[i]);
                for (int i = 0; i < 4; ++i) w.f32_(k.tanIn[i]);
                for (int i = 0; i < 4; ++i) w.f32_(k.tanOut[i]);
            }
        }
    }
    // esqueleto opcional (fim do payload — ficheiros v1 sem a secção
    // ficam válidos: hasSkeleton=0)
    w.u8_(anim.joints.empty() ? 0 : 1);
    w.u32_(static_cast<u32>(anim.joints.size()));
    for (const SkeletonComp::Joint& j : anim.joints) {
        w.str_(j.name);
        w.u32_(static_cast<u32>(static_cast<i64>(j.parent) + 0x80000000ull));
        w.f32_(j.pos.x); w.f32_(j.pos.y); w.f32_(j.pos.z);
        w.f32_(j.rot.x); w.f32_(j.rot.y); w.f32_(j.rot.z); w.f32_(j.rot.w);
        w.f32_(j.scale.x); w.f32_(j.scale.y); w.f32_(j.scale.z);
        w.f32_(j.bindPos.x); w.f32_(j.bindPos.y); w.f32_(j.bindPos.z);
        w.f32_(j.bindRot.x); w.f32_(j.bindRot.y); w.f32_(j.bindRot.z);
        w.f32_(j.bindRot.w);
        w.f32_(j.bindScale.x); w.f32_(j.bindScale.y); w.f32_(j.bindScale.z);
        for (int r = 0; r < 16; ++r) {
            w.f32_(j.inverseBind.m[r]);
        }
    }
    Writer o(out);
    writeHeader(o, "GANM", payload.size(),
                gfnv1a(payload.data(), payload.size()));
    o.bytes_(payload.data(), payload.size());
    return true;
}

bool readGAnim(const u8* bytes, size_t len, GAnimFile& out,
               std::string& err) {
    out = GAnimFile{};
    GFileHeader h;
    if (!gReadHeader(bytes, len, "GANM", h, err)) {
        return false;
    }
    if (h.version != 1) {
        err = "versão " + std::to_string(h.version) +
              " desconhecida (o .gm lê a 1)";
        return false;
    }
    Reader r(bytes + kGHeaderBytes, static_cast<size_t>(h.payloadSize));
    const u32 nClips = r.u32_();
    if (r.bad || nClips > 256) {
        err = "número absurdo de clips no .gm (" + std::to_string(nClips) + ")";
        return false;
    }
    for (u32 c = 0; c < nClips; ++c) {
        AnimClip clip;
        clip.name = r.str_();
        const u32 nTracks = r.u32_();
        if (r.bad || nTracks > 4096) {
            err = "clip '" + clip.name + "' com tracks demais no .gm";
            return false;
        }
        for (u32 ti = 0; ti < nTracks; ++ti) {
            AnimTrack t;
            t.target = static_cast<AnimTarget>(r.u8_());
            t.curve = static_cast<AnimCurve>(r.u8_());
            if (static_cast<int>(t.target) < 0 ||
                static_cast<int>(t.target) > 8) {
                err = "alvo de track desconhecido no .gm";
                return false;
            }
            t.element = r.str_();
            const u32 nKeys = r.u32_();
            if (r.bad || nKeys > 65536) {
                err = "track com keys demais no .gm";
                return false;
            }
            for (u32 k = 0; k < nKeys; ++k) {
                AnimKey key;
                key.t = r.f32_();
                for (int i = 0; i < 4; ++i) key.v[i] = r.f32_();
                for (int i = 0; i < 4; ++i) key.tanIn[i] = r.f32_();
                for (int i = 0; i < 4; ++i) key.tanOut[i] = r.f32_();
                if (r.bad) {
                    break;
                }
                t.keys.push_back(key);
            }
            if (r.bad) {
                err = "keys truncadas no .gm (clip '" + clip.name + "')";
                return false;
            }
            t.sortKeys();
            clip.tracks.push_back(std::move(t));
        }
        out.clips.push_back(std::move(clip));
    }
    // esqueleto opcional — ficheiros truncados sem a secção ficam VÁLIDOS
    // (compatibilidade com a v1 inicial sem skeleton)
    if (r.i + 1 <= static_cast<size_t>(h.payloadSize)) {
        const u8 hasSkel = r.u8_();
        if (!r.bad && hasSkel == 1) {
            const u32 nJoints = r.u32_();
            if (r.bad || nJoints > SkeletonComp::kMaxBones) {
                err = "esqueleto inválido no .gm (" + std::to_string(nJoints) +
                      " joints)";
                return false;
            }
            for (u32 j = 0; j < nJoints; ++j) {
                SkeletonComp::Joint joint;
                joint.name = r.str_();
                const u32 parent = r.u32_();
                joint.parent = static_cast<i32>(parent) - 0x80000000;
                if (joint.parent >= static_cast<i32>(j)) {
                    err = "joint '" + joint.name + "' com pai inválido";
                    return false;
                }
                joint.pos.x = r.f32_(); joint.pos.y = r.f32_(); joint.pos.z = r.f32_();
                joint.rot.x = r.f32_(); joint.rot.y = r.f32_();
                joint.rot.z = r.f32_(); joint.rot.w = r.f32_();
                joint.scale.x = r.f32_(); joint.scale.y = r.f32_(); joint.scale.z = r.f32_();
                joint.bindPos.x = r.f32_(); joint.bindPos.y = r.f32_(); joint.bindPos.z = r.f32_();
                joint.bindRot.x = r.f32_(); joint.bindRot.y = r.f32_();
                joint.bindRot.z = r.f32_(); joint.bindRot.w = r.f32_();
                joint.bindScale.x = r.f32_(); joint.bindScale.y = r.f32_();
                joint.bindScale.z = r.f32_();
                for (int k = 0; k < 16; ++k) {
                    joint.inverseBind.m[k] = r.f32_();
                }
                if (r.bad) {
                    err = "joint truncado no .gm";
                    return false;
                }
                out.joints.push_back(std::move(joint));
            }
        }
    }
    return true;
}

} // namespace vv
