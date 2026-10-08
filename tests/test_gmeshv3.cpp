// tests/test_gmeshv3.cpp — 0.10-M (PASSO 2) — O FORMATO v3 SOB PROVA:
// round-trip exato float32 (a quantização 16-bit do v1 morreu), blocos
// por grupo com materiais, as fixtures v1/v2 REAIS que continuam a abrir,
// o meta lido SEM alocar os dados (contagens acima de 2^32 e offsets
// acima de 4 GB), a corrupção apanhada pelos CRC32 e o mmap de 64 bits.
#include "TestFramework.h"

#include "assets/GOwnFormats.h"
#include "platform/FileApi.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

using namespace vv;

// o mini-escritor LE (o Writer do GOwnFormats.cpp é interno ao TU)
struct BW {
    std::vector<u8>& b;
    explicit BW(std::vector<u8>& v) : b(v) {}
    void u8_(u8 v) { b.push_back(v); }
    void u16_(u16 v) { b.push_back(static_cast<u8>(v & 0xFF));
                       b.push_back(static_cast<u8>(v >> 8)); }
    void u32_(u32 v) { u16_(static_cast<u16>(v & 0xFFFF));
                       u16_(static_cast<u16>(v >> 16)); }
    void u64_(u64 v) { u32_(static_cast<u32>(v & 0xFFFFFFFFull));
                       u32_(static_cast<u32>(v >> 32)); }
    void f32_(f32 v) { u32 raw; std::memcpy(&raw, &v, 4); u32_(raw); }
    void bytes_(const u8* p, size_t n) { b.insert(b.end(), p, p + n); }
};

// esfera determinística (a família do round-trip do wiring010)
void esfera(MeshData& m, int seg = 16, int ring = 12) {
    m.name = "esfera";
    for (int r = 0; r <= ring; ++r) {
        const f32 phi = 3.14159265f * f32(r) / f32(ring);
        for (int s = 0; s <= seg; ++s) {
            const f32 th = 6.28318530f * f32(s) / f32(seg);
            Vertex v;
            v.pos = Vec3{std::sin(phi) * std::cos(th), std::cos(phi),
                         std::sin(phi) * std::sin(th)};
            v.normal = v.pos;
            v.uv = Vec2{f32(s) / f32(seg), f32(r) / f32(ring)};
            m.vertices.push_back(v);
        }
    }
    for (int r = 0; r < ring; ++r) {
        for (int s = 0; s < seg; ++s) {
            const u16 a = u16(r * (seg + 1) + s);
            const u16 b = u16(a + seg + 1);
            m.indices.push_back(a);
            m.indices.push_back(b);
            m.indices.push_back(u16(a + 1));
            m.indices.push_back(b);
            m.indices.push_back(u16(b + 1));
            m.indices.push_back(u16(a + 1));
        }
    }
}

// monta um v3 SINTÉTICO (header+meta+tabela) para os testes de contagens
// grandes — os DADOS não existem (é o ponto: ler sem alocar)
bool sinteticoV3(u64 verts, u64 tris, u64 blocos, u64 dataOffset0,
                 std::vector<u8>& out) {
    GMeshV3Meta meta;
    meta.attrCount = 3;
    meta.attrs[0] = GMeshV3Attr{kAttrPosition, kAttrF32, 0, 0, 0};
    meta.attrs[1] = GMeshV3Attr{kAttrNormal, kAttrF32, 0, 0, 0};
    meta.attrs[2] = GMeshV3Attr{kAttrUv0, kAttrF32, 0, 0, 0};
    meta.vertexCount = verts;
    meta.indexCount = tris * 3;
    meta.blockCount = blocos;
    meta.materialCount = 1;
    meta.aabbMin = Vec3{-1, -1, -1};
    meta.aabbMax = Vec3{1, 1, 1};
    meta.blockVertexCap = 2000000000u;   // cap configurável (u32 inteiro)
    const u64 dataStart = 32 + kGmeshV3MetaBytes;
    const u64 stride = 32;   // 12+12+8
    u64 cursor = dataStart;
    std::vector<GMeshV3Block> entries;
    for (u64 i = 0; i < blocos; ++i) {
        const u64 iv = verts / blocos + (i == 0 ? verts % blocos : 0);
        const u64 ii = tris * 3 / blocos + (i == 0 ? (tris * 3) % blocos : 0);
        cursor += (16 - cursor % 16) % 16;
        GMeshV3Block e;
        e.dataOffset = dataOffset0 + i;   // FORA do ficheiro (de propósito)
        e.vertexCount = iv;
        e.indexCount = ii;
        e.aabbMin = meta.aabbMin;
        e.aabbMax = meta.aabbMax;
        e.materialIndex = 0;
        e.indexType = iv > 65535 ? 1u : 0u;
        e.dataSize = iv * stride + ii * (e.indexType ? 4ull : 2ull);
        e.crc32 = 0x12345678;
        // N.B. o cursor NÃO avança: os dados vivem nos offsets míticos
        // (5 GB) — o ficheiro sintético tem SÓ header+meta+tabela
        entries.push_back(e);
    }
    cursor += (16 - cursor % 16) % 16;
    meta.blockTableOffset = cursor;
    cursor += blocos * kGmeshV3BlockEntryBytes;
    meta.materialTableOffset = cursor;
    const std::string matName = "laca";
    cursor += 2 + matName.size();
    const u64 payloadSize = cursor - 32;
    meta.tableCrc32 = 0;
    BW o(out);
    std::vector<u8> hdr;
    {
        BW hw(hdr);
        // o mesmo writeHeader da casa (via writeHeader não é público —
        // escreve o header à mão, igual ao formato)
        const char* magic = "GMES";
        hw.bytes_(reinterpret_cast<const u8*>(magic), 4);
        hw.u16_(3);
        hw.u16_(0x1A2B);
        hw.u32_(16);
        hw.u64_(payloadSize);
        hw.u64_(0);   // checksum preenchido abaixo
        hw.u32_(0);
    }
    o.bytes_(hdr.data(), hdr.size());
    std::vector<u8> mb;
    {
        BW mw(mb);
        mw.u32_(meta.attrCount);
        mw.u32_(meta.flags);
        mw.u32_(meta.blockVertexCap);
        mw.u32_(0);   // tableCrc32 (preenchido abaixo)
        mw.u64_(meta.vertexCount);
        mw.u64_(meta.indexCount);
        mw.u64_(meta.blockCount);
        mw.u64_(meta.materialCount);
        mw.f32_(meta.aabbMin.x); mw.f32_(meta.aabbMin.y);
        mw.f32_(meta.aabbMin.z);
        mw.f32_(meta.aabbMax.x); mw.f32_(meta.aabbMax.y);
        mw.f32_(meta.aabbMax.z);
        mw.u64_(meta.blockTableOffset);
        mw.u64_(meta.materialTableOffset);
        for (u32 i = 0; i < kGmeshV3MaxAttrs; ++i) {
            const GMeshV3Attr& a = meta.attrs[i];
            mw.u8_(a.semantic); mw.u8_(a.storage);
            mw.u8_(a.normalized); mw.u8_(a.reserved);
            mw.u32_(a.reserved2);
        }
        mw.u32_(0);
        mw.u32_(0);
    }
    o.bytes_(mb.data(), mb.size());
    std::vector<u8> tb;
    {
        BW tw(tb);
        for (const GMeshV3Block& e : entries) {
            tw.u64_(e.dataOffset);
            tw.u64_(e.dataSize);
            tw.u64_(e.vertexCount);
            tw.u64_(e.indexCount);
            tw.f32_(e.aabbMin.x); tw.f32_(e.aabbMin.y);
            tw.f32_(e.aabbMin.z);
            tw.f32_(e.aabbMax.x); tw.f32_(e.aabbMax.y);
            tw.f32_(e.aabbMax.z);
            tw.u32_(e.materialIndex);
            tw.u32_(e.indexType);
            tw.u32_(e.crc32);
            tw.u32_(0);
            tw.u64_(0);
        }
    }
    meta.tableCrc32 = gcrc32(tb.data(), tb.size());
    for (size_t i = 0; i < kGmeshV3MetaBytes; ++i) {
        out[32 + i] = mb[i];
    }
    // o CRC da tabela entra nos metadados (offset 32+12 = tableCrc32)
    const u32 crc32v = meta.tableCrc32;
    out[32 + 12] = static_cast<u8>(crc32v & 0xFF);
    out[32 + 13] = static_cast<u8>((crc32v >> 8) & 0xFF);
    out[32 + 14] = static_cast<u8>((crc32v >> 16) & 0xFF);
    out[32 + 15] = static_cast<u8>((crc32v >> 24) & 0xFF);
    // o checksum dos metadados → no header (offset 20)
    const u64 chk = gfnv1a(out.data() + 32, kGmeshV3MetaBytes);
    for (int i = 0; i < 8; ++i) {
        out[20 + i] = static_cast<u8>((chk >> (8 * i)) & 0xFF);
    }
    while (out.size() < meta.blockTableOffset) o.u8_(0);
    o.bytes_(tb.data(), tb.size());
    // a tabela de materiais (1 nome)
    o.u16_(static_cast<u16>(matName.size()));
    o.bytes_(reinterpret_cast<const u8*>(matName.data()), matName.size());
    while (out.size() % 4 != 0) o.u8_(0);
    return true;
}

} // namespace

TEST(gmeshv3_escrita_v3_e_roundtrip_exato) {
    MeshData m;
    esfera(m);
    MeshData::Group g{"corpo", "laca", 0,
                      static_cast<u32>(m.indices.size())};
    m.groups.push_back(g);

    std::vector<u8> out;
    std::string err;
    ASSERT(writeGMesh(m, out, err));
    // a versão escrita é 3 (a MUTAÇÃO M-V3a troca ISTO para 2 → vermelho)
    const u16 ver = static_cast<u16>(out[4] | (out[5] << 8));
    EXPECT_MSG(ver == kGmeshVersionWrite,
               "o escritor grava version=%u (esperado 3)", ver);

    MeshData back;
    ASSERT(readGMesh(out.data(), out.size(), back, err));
    EXPECT(back.vertices.size() == m.vertices.size());
    EXPECT(back.indices.size() == m.indices.size());
    EXPECT(back.groups.size() == 1);
    // SEM PERDA: float32 EXATO bit a bit (o v1 era quantizado ~1e-3)
    bool exact = true;
    for (size_t i = 0; i < m.vertices.size(); ++i) {
        if (std::memcmp(&back.vertices[i].pos, &m.vertices[i].pos, 12) != 0 ||
            std::memcmp(&back.vertices[i].normal, &m.vertices[i].normal,
                        12) != 0 ||
            std::memcmp(&back.vertices[i].uv, &m.vertices[i].uv, 8) != 0) {
            exact = false;
        }
    }
    EXPECT_MSG(exact, "as posições/normais/uv batem bit a bit");
    // os índices idênticos
    EXPECT(std::memcmp(back.indices.data(), m.indices.data(),
                       m.indices.size() * 2) == 0);
    // o material do grupo sobrevive
    EXPECT(back.groups[0].material == "laca");
}

TEST(gmeshv3_multi_grupo_blocos_por_material) {
    // 3 grupos (2 materiais) → 3 blocos; a bridge devolve 3 grupos com os
    // materiais da tabela
    MeshData m;
    esfera(m);
    const u32 per = static_cast<u32>(m.indices.size()) / 3;
    m.groups.push_back({"a", "madeira", 0, per});
    m.groups.push_back({"b", "metal", per, per});
    m.groups.push_back({"c", "madeira", per * 2,
                        static_cast<u32>(m.indices.size()) - per * 2});
    std::vector<u8> out;
    std::string err;
    ASSERT(writeGMesh(m, out, err));
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> mats;
    ASSERT(readGMeshV3Meta(out.data(), out.size(), meta, blocks, mats, err));
    EXPECT(blocks.size() == 3);
    EXPECT(meta.blockCount == 3);
    EXPECT(mats.size() == 2);   // madeira deduplicada
    // a pool é POR BLOCO (a semântica do v3): vértices partilhados entre
    // grupos viajam DUPLICADOS por bloco (nunca soldados) — o total é a
    // soma das pools, o conjunto de TRIÂNGULOS é idêntico ao de entrada
    EXPECT(meta.vertexCount >= m.vertices.size());
    EXPECT(meta.indexCount == m.indices.size());
    MeshData back;
    ASSERT(readGMesh(out.data(), out.size(), back, err));
    EXPECT(back.groups.size() == 3);
    EXPECT(back.groups[0].material == "madeira");
    EXPECT(back.groups[1].material == "metal");
    EXPECT(back.groups[2].material == "madeira");
    EXPECT(back.indices.size() == m.indices.size());
}

TEST(gmeshv3_v1_e_v2_fixtures_reais_abrem) {
    // as fixtures geradas COM O ESCRITOR v1 (antes do 0.10-M) e commitadas:
    // a retrocompatibilidade é contra BYTES REAIS, não contra invenção
    const std::string root = REPO_ROOT;
    std::vector<u8> bytes;
    ASSERT(fileapi::readAll(root + "/tests/fixtures/gmesh_v1_esfera.gmesh",
                            bytes));
    MeshData v1;
    std::string err;
    ASSERT((readGMesh(bytes.data(), bytes.size(), v1, err)) || (std::fprintf(stderr, "v1: %s\n", err.c_str()), 0));
    EXPECT(v1.vertices.size() == 221);
    EXPECT(v1.indices.size() == 1152);
    EXPECT(v1.groups.size() == 1);
    EXPECT(v1.groups[0].material == "laca");

    // a variante v2 (os MESMOS bytes com version=2 — a interpretação
    // tolerante documentada no GMESH_formato.md; a v2 nunca existiu no
    // histórico do repo, a decisão está no relatório do PASSO 2)
    bytes.clear();
    ASSERT(fileapi::readAll(root + "/tests/fixtures/gmesh_v2_esfera.gmesh",
                            bytes));
    MeshData v2;
    ASSERT((readGMesh(bytes.data(), bytes.size(), v2, err)) || (std::fprintf(stderr, "v2: %s\n", err.c_str()), 0));
    EXPECT(v2.vertices.size() == 221);
    EXPECT(v2.indices.size() == 1152);
    EXPECT(v2.groups[0].name == "corpo");
}

TEST(gmeshv3_meta_sem_alocar_dados_contagens_grandes) {
    // contagens acima de 2^32 e offsets acima de 4 GB — o META lê
    // header+tabela SEM os dados existirem (o contrato do dono)
    std::vector<u8> out;
    ASSERT(sinteticoV3(5000000000ull /*>2^32*/, 2500000000ull,
                       4 /*blocos*/, 5000000000ull /*offset >4 GB*/, out));
    EXPECT(out.size() < 1000);   // só header+meta+tabela+materiais
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> mats;
    std::string err;
    ASSERT((readGMeshV3Meta(out.data(), out.size(), meta, blocks, mats, err)) || (std::fprintf(stderr, "meta: %s\n", err.c_str()), 0));
    EXPECT(meta.vertexCount == 5000000000ull);
    EXPECT(meta.indexCount == 7500000000ull);
    EXPECT(meta.blockCount == 4);
    EXPECT(blocks[0].dataOffset == 5000000000ull);   // >4 GB intacto
    EXPECT(blocks[0].dataSize == blocks[0].vertexCount * 32ull +
                                    blocks[0].indexCount * 4ull);
    EXPECT(mats.size() == 1 && mats[0] == "laca");
}

TEST(gmeshv3_corrupcao_apanhada_pelos_crc) {
    MeshData m;
    esfera(m);
    std::vector<u8> out;
    std::string err;
    ASSERT(writeGMesh(m, out, err));
    // (a) um byte dos DADOS do bloco
    std::vector<u8> bad = out;
    bad[bad.size() / 2] ^= 0xFF;
    MeshData back;
    EXPECT(!readGMesh(bad.data(), bad.size(), back, err));
    EXPECT(err.find("CHECKSUM CORROMPIDO") != std::string::npos);
    // (b) um byte da TABELA
    bad = out;
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> mats;
    ASSERT(readGMeshV3Meta(out.data(), out.size(), meta, blocks, mats, err));
    bad[meta.blockTableOffset] ^= 0xFF;
    EXPECT(!readGMesh(bad.data(), bad.size(), back, err));
    EXPECT(err.find("CHECKSUM CORROMPIDO") != std::string::npos);
    // (c) os METADADOS
    bad = out;
    bad[32 + 40] ^= 0xFF;
    EXPECT(!readGMesh(bad.data(), bad.size(), back, err));
    EXPECT(err.find("CHECKSUM CORROMPIDO") != std::string::npos);
}

TEST(gmeshv3_versao_desconhecida_rejeitada) {
    MeshData m;
    esfera(m);
    std::vector<u8> out;
    std::string err;
    ASSERT(writeGMesh(m, out, err));
    out[4] = 9;   // version = 9 (a recusa de versões futuras)
    MeshData back;
    EXPECT(!readGMesh(out.data(), out.size(), back, err));
    EXPECT(err.find("desconhecida") != std::string::npos);
}

TEST(gmeshv3_mmap_de_64_bits_le_o_ficheiro) {
    // o caminho do dono: o ficheiro mapeado (mmap, offset 64-bit) lê-se
    // como os bytes em memória — zero cópias
    const std::string root = REPO_ROOT;
    MeshData m;
    esfera(m);
    std::vector<u8> out;
    std::string err;
    ASSERT(writeGMesh(m, out, err));
    const std::string path = "/tmp/goni_v3_mmap.gmesh";
    {
        FILE* f = std::fopen(path.c_str(), "wb");
        ASSERT(f != nullptr);
        std::fwrite(out.data(), 1, out.size(), f);
        std::fclose(f);
    }
    unsigned long long mapped = 0;
    void* ptr = fileapi::mapFile64(path.c_str(), 0, out.size(), &mapped);
    ASSERT(ptr != nullptr);
    EXPECT(mapped >= out.size());
    MeshData back;
    ASSERT((readGMesh(static_cast<const u8*>(ptr), out.size(), back, err)) || (std::fprintf(stderr, "mmap read: %s\n", err.c_str()), 0));
    EXPECT(back.vertices.size() == m.vertices.size());
    // e uma leitura com OFFSET de 64 bits (a partir do payload) também abre
    unsigned long long mapped2 = 0;
    void* p2 = fileapi::mapFile64(path.c_str(), 32, out.size() - 32,
                                  &mapped2);
    ASSERT(p2 != nullptr);
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> mats;
    // (o meta espera o header — aqui só prova que o mapeamento parcial
    // devolve os bytes certos)
    EXPECT(std::memcmp(p2, out.data() + 32, 16) == 0);
    fileapi::unmapFile64(p2, mapped2);
    fileapi::unmapFile64(ptr, mapped);
    std::remove(path.c_str());
}

TEST(gmeshv3_orfaos_nao_sobrevivem_ao_corte) {
    // vértices não referenciados não entram no ficheiro (documentado no
    // formato: o corte guarda o conjunto de triângulos)
    MeshData m;
    esfera(m);
    const Vertex orphan{Vec3{99, 99, 99}, Vec3{0, 1, 0}, Vec2{0, 0}};
    m.vertices.push_back(orphan);
    m.vertices.push_back(orphan);
    m.vertices.push_back(orphan);
    std::vector<u8> out;
    std::string err;
    ASSERT(writeGMesh(m, out, err));
    GMeshV3Meta meta;
    std::vector<GMeshV3Block> blocks;
    std::vector<std::string> mats;
    ASSERT(readGMeshV3Meta(out.data(), out.size(), meta, blocks, mats, err));
    EXPECT(meta.vertexCount == m.vertices.size() - 3);
}
