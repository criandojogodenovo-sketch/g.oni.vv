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
// + payloadSize 8 + checksum 8 + reserved 4) — kGHeaderBytes é a constante
// única que leitores e escritores partilham
constexpr size_t kGHeaderBytes = 32;

void writeHeader(Writer& w, const char* magic, u64 payloadSize, u64 checksum) {
    w.bytes_(reinterpret_cast<const u8*>(magic), 4);
    w.u16_(1);          // version
    w.u16_(0x1A2B);     // endianMark
    w.u32_(8);          // align (eco informativo)
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
    if (h.version != 1) {
        err = "versão " + std::to_string(h.version) +
              " desconhecida (a engine lê a 1)";
        return false;
    }
    if (h.payloadSize + kGHeaderBytes > len) {
        err = "payload truncado: header diz " +
              std::to_string(h.payloadSize) + " B mas só há " +
              std::to_string(len - kGHeaderBytes) + " B";
        return false;
    }
    const u64 got = gfnv1a(bytes + kGHeaderBytes,
                           static_cast<size_t>(h.payloadSize));
    if (got != h.checksum) {
        err = "CHECKSUM CORROMPIDO: payload diz 0x" +
              std::to_string(h.checksum) + ", recalculado 0x" +
              std::to_string(got) + " — o ficheiro foi danificado";
        return false;
    }
    return true;
}

// ---- .gmesh -------------------------------------------------------------------
// payload: verts u32, indices u32, bounds f32×6, groups u32, [skin u8],
// depois verts×(pos u16×3 + nrm u16×3 + uv u16×2 [+ joints u8×4 + w u16×4]),
// indices×u16, groups×(nome str, material str, first u32, count u32),
// skin verts×(joints u8×4, weights u16×4)
bool writeGMesh(const MeshData& m, std::vector<u8>& out, std::string& err) {
    out.clear();
    if (m.vertices.empty() || m.indices.empty()) {
        err = "geometria vazia — nada a converter";
        return false;
    }
    if (m.vertices.size() > 65535) {
        err = "mesh com " + std::to_string(m.vertices.size()) +
              " vértices — o limite do engine é 65535 (índices u16)";
        return false;
    }
    const bool skinned = m.vertices.size() * 4 == m.skinJoints.size();
    // bounds (o leitor dequantiza por aqui — e o runtime lê o AABB SEM
    // tocar nos vértices)
    Vec3 mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
    for (const Vertex& v : m.vertices) {
        mn.x = mn.x < v.pos.x ? mn.x : v.pos.x;
        mn.y = mn.y < v.pos.y ? mn.y : v.pos.y;
        mn.z = mn.z < v.pos.z ? mn.z : v.pos.z;
        mx.x = mx.x > v.pos.x ? mx.x : v.pos.x;
        mx.y = mx.y > v.pos.y ? mx.y : v.pos.y;
        mx.z = mx.z > v.pos.z ? mx.z : v.pos.z;
    }
    std::vector<u8> payload;
    Writer w(payload);
    w.u32_(static_cast<u32>(m.vertices.size()));
    w.u32_(static_cast<u32>(m.indices.size()));
    w.f32_(mn.x); w.f32_(mn.y); w.f32_(mn.z);
    w.f32_(mx.x); w.f32_(mx.y); w.f32_(mx.z);
    w.u32_(static_cast<u32>(m.groups.size()));
    w.u8_(skinned ? 1 : 0);
    for (const Vertex& v : m.vertices) {
        w.u16_(quantF(v.pos.x, mn.x, mx.x - mn.x));
        w.u16_(quantF(v.pos.y, mn.y, mx.y - mn.y));
        w.u16_(quantF(v.pos.z, mn.z, mx.z - mn.z));
        w.u16_(quantS(v.normal.x));
        w.u16_(quantS(v.normal.y));
        w.u16_(quantS(v.normal.z));
        w.u16_(quantU(v.uv.x));
        w.u16_(quantU(v.uv.y));
        if (skinned) {
            const size_t vi = &v - m.vertices.data();
            for (int k = 0; k < 4; ++k) {
                w.u8_(m.skinJoints[vi * 4 + static_cast<size_t>(k)]);
            }
            for (int k = 0; k < 4; ++k) {
                w.u16_(quantU(m.skinWeights[vi * 4 + static_cast<size_t>(k)]));
            }
        }
    }
    for (const u16 idx : m.indices) {
        w.u16_(idx);
    }
    for (const MeshData::Group& g : m.groups) {
        w.str_(g.name);
        w.str_(g.material);
        w.u32_(g.firstIndex);
        w.u32_(g.indexCount);
    }
    // header comum + payload
    Writer o(out);
    writeHeader(o, "GMES", payload.size(), gfnv1a(payload.data(), payload.size()));
    o.bytes_(payload.data(), payload.size());
    return true;
}

bool readGMesh(const u8* bytes, size_t len, MeshData& out, std::string& err) {
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
