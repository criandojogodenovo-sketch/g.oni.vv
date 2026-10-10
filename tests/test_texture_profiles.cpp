// tests/test_texture_profiles.cpp — PASSO 5A: os TRÊS PERFIS de texturas
// (redução e compressão SEPARADAS), o override por asset, o teto do device,
// o caminho normal-map sem perda de canais, os mips em espaço linear, o
// .gtext v2 (flags), o cache POR PERFIL (a mesma textura nos 3 perfis = 3
// tamanhos) e o passe de texturas PARALELO do conversor.
//
// Pins do dono (0.10-M PASSO 5A):
//   · mesma textura nos 3 perfis = 3 tamanhos distintos
//   · override por asset VENCE o perfil
//   · textura gigante não crasha (GL_MAX_TEXTURE_SIZE → mip mais baixo)
//   · ETC2 verde em device sem ASTC
//   · normal maps sem perda de canais
//   · mips filtrados em espaço linear
#include "TestFramework.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "assets/AssetConverter.h"
#include "assets/GltfImporter.h"
#include "assets/GOwnFormats.h"
#include "assets/MipGen.h"
#include "assets/PngLoader.h"
#include "assets/TexturePipeline.h"
#include "assets/TexturePolicy.h"
#include "render/ThumbPng.h"     // encodePngRgb — gerador de PNGs sintéticos
#include "core/FsStorage.h"      // o conversor streaming quer staging REAL
#include "FakeStorage.h"         // os testes do pipeline (raiz em memória)

using namespace vv;

namespace {

std::vector<u8> readFixture(const char* name) {
    std::string path = std::string(FIXTURE_DIR) + "/" + name;
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        return {};
    }
    std::vector<u8> bytes;
    u8 buf[4096];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
        bytes.insert(bytes.end(), buf, buf + n);
    }
    std::fclose(f);
    return bytes;
}

std::string b64encode(const u8* data, size_t n) {
    static const char* tbl =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    size_t i = 0;
    for (; i + 2 < n; i += 3) {
        const u32 v = (static_cast<u32>(data[i]) << 16) |
                      (static_cast<u32>(data[i + 1]) << 8) | data[i + 2];
        out += tbl[(v >> 18) & 63];
        out += tbl[(v >> 12) & 63];
        out += tbl[(v >> 6) & 63];
        out += tbl[v & 63];   // o 4.º — sem ele o b64 sai truncado (3/4)
    }
    if (i < n) {
        u32 v = static_cast<u32>(data[i]) << 16;
        if (i + 1 < n) v |= static_cast<u32>(data[i + 1]) << 8;
        out += tbl[(v >> 18) & 63];
        out += tbl[(v >> 12) & 63];
        out += (i + 1 < n) ? tbl[(v >> 6) & 63] : '=';
        out += '=';
    }
    return out;
}

// PNG sintético RGB (thumb::encodePngRgb — zlib real); padrão dividido em
// 4 quadrantes (a média dos mips fica aferível)
std::vector<u8> makePngQuadrantes(u32 w, u32 h) {
    std::vector<u8> rgb(static_cast<size_t>(w) * h * 3u);
    const u32 hw = w / 2u, hh = h / 2u;
    for (u32 y = 0; y < h; ++y) {
        for (u32 x = 0; x < w; ++x) {
            u8* p = &rgb[(static_cast<size_t>(y) * w + x) * 3u];
            if (y < hh) {
                p[0] = x < hw ? 255 : 0;
                p[1] = x < hw ? 255 : 0;
                p[2] = 0;
            } else {
                p[0] = 0;
                p[1] = x < hw ? 0 : 255;
                p[2] = x < hw ? 0 : 255;
            }
        }
    }
    return thumb::encodePngRgb(rgb.data(), w, h);
}

struct PipeOut {
    bool ok = false;
    CompressedImage comp;
    TextureLoadInfo info;
    std::string err;
};

PipeOut runPipe(TexturePipeline& p, const std::vector<u8>& png,
                const char* name, TexJob job = TexJob{}) {
    PipeOut r;
    r.ok = p.process(png.data(), png.size(), name, r.comp, r.info, r.err,
                     job);
    return r;
}

void pushF32(std::vector<u8>& b, f32 v) {
    u8 tmp[4];
    std::memcpy(tmp, &v, 4);
    b.insert(b.end(), tmp, tmp + 4);
}

void pushU16(std::vector<u8>& b, u16 v) {
    u8 tmp[2];
    std::memcpy(tmp, &v, 2);
    b.insert(b.end(), tmp, tmp + 2);
}

// triângulo: POSITION (3×f32×3) + índices u16 — devolve offsets/lengths
std::vector<u8> triBuffer(u32& posOff, u32& posLen, u32& idxOff, u32& idxLen) {
    std::vector<u8> b;
    posOff = 0;
    pushF32(b, 0); pushF32(b, 0); pushF32(b, 0);
    pushF32(b, 1); pushF32(b, 0); pushF32(b, 0);
    pushF32(b, 0); pushF32(b, 1); pushF32(b, 0);
    posLen = 36;
    idxOff = 36;
    pushU16(b, 0); pushU16(b, 1); pushU16(b, 2);
    idxLen = 6;
    return b;
}

// glTF (o JSON de um GLB) com N imagens embutidas (data:) + materiais:
// 0..N-1 com baseColor na imagem i e um material extra com normalTexture na
// imagem nNormal — o buffer é o BIN chunk do GLB (o caminho streaming da
// casa; a fonte é um ficheiro GLB real no disco)
std::string gltfComTexturasFull(u32 nImgs, u32 nNormal, u32 bufLen) {
    std::string imgs, mats, texs;
    for (u32 i = 0; i < nImgs; ++i) {
        texs += (i ? "," : "");
        texs += "{\"source\":" + std::to_string(i) + "}";
    }
    for (u32 i = 0; i < nImgs; ++i) {
        mats += (i ? "," : "");
        mats += "{\"name\":\"mat" + std::to_string(i) + "\","
                "\"pbrMetallicRoughness\":{\"baseColorTexture\":{\"index\":" +
                std::to_string(i) + "}}}";
    }
    if (nImgs > 0) {
        mats += ",{\"name\":\"matNormal\",\"normalTexture\":{\"index\":" +
                std::to_string(nNormal) + "}}";
    }
    return std::string(
               "{\"asset\":{\"version\":\"2.0\"},"
               "\"buffers\":[{\"byteLength\":") +
           std::to_string(bufLen) + "}],"
           "\"bufferViews\":["
           "{\"buffer\":0,\"byteOffset\":0,\"byteLength\":36},"
           "{\"buffer\":0,\"byteOffset\":36,\"byteLength\":6}],"
           "\"accessors\":["
           "{\"bufferView\":0,\"componentType\":5126,\"count\":3,\"type\":"
           "\"VEC3\",\"min\":[0,0,0],\"max\":[1,1,0]},"
           "{\"bufferView\":1,\"componentType\":5123,\"count\":3,\"type\":"
           "\"SCALAR\"}],"
           "\"materials\":[" + mats + "],"
           "\"textures\":[" + texs + "],"
           "\"images\":[" + imgs + "],"
           "\"meshes\":[{\"name\":\"tri\",\"primitives\":[{"
           "\"attributes\":{\"POSITION\":0},\"indices\":1,\"material\":0}]}],"
           "\"nodes\":[{\"name\":\"raiz\",\"mesh\":0}],"
           "\"scenes\":[{\"nodes\":[]}]}";
}

} // namespace

// ---- a tabela de redução (a política pura, linha a linha) -------------------
TEST(perfil_tabela_tetos) {
    using P = TexPerfil;
    // Qualidade: SEM redução em qualquer classe
    EXPECT(texProfileCapMaxDim(512, 512, P::Qualidade) == 0u);
    EXPECT(texProfileCapMaxDim(1024, 1024, P::Qualidade) == 0u);
    EXPECT(texProfileCapMaxDim(2048, 2048, P::Qualidade) == 0u);
    EXPECT(texProfileCapMaxDim(4096, 4096, P::Qualidade) == 0u);
    EXPECT(texProfileCapMaxDim(5120, 5120, P::Qualidade) == 0u);
    // Equilibrado: 512/1024 mantém · 2048→1K · 4096→2K · 5120+→2K
    EXPECT(texProfileCapMaxDim(512, 512, P::Equilibrado) == 0u);
    EXPECT(texProfileCapMaxDim(1024, 512, P::Equilibrado) == 0u);
    EXPECT(texProfileCapMaxDim(2048, 2048, P::Equilibrado) == 1024u);
    EXPECT(texProfileCapMaxDim(3000, 100, P::Equilibrado) == 2048u);
    EXPECT(texProfileCapMaxDim(4096, 4096, P::Equilibrado) == 2048u);
    EXPECT(texProfileCapMaxDim(5120, 2048, P::Equilibrado) == 2048u);
    // Mobile: 512/1024 mantém · tudo o resto →1K (agressiva)
    EXPECT(texProfileCapMaxDim(512, 512, P::Mobile) == 0u);
    EXPECT(texProfileCapMaxDim(1024, 1024, P::Mobile) == 0u);
    EXPECT(texProfileCapMaxDim(2048, 2048, P::Mobile) == 1024u);
    EXPECT(texProfileCapMaxDim(4096, 4096, P::Mobile) == 1024u);
    EXPECT(texProfileCapMaxDim(8192, 4096, P::Mobile) == 1024u);
}

TEST(perfil_nomes_e_parse) {
    EXPECT(std::string(texPerfilName(TexPerfil::Qualidade)) == "Qualidade");
    EXPECT(std::string(texPerfilName(TexPerfil::Equilibrado)) == "Equilibrado");
    EXPECT(std::string(texPerfilName(TexPerfil::Mobile)) == "Mobile");
    EXPECT(texPerfilFromInt(0) == TexPerfil::Qualidade);
    EXPECT(texPerfilFromInt(1) == TexPerfil::Equilibrado);
    EXPECT(texPerfilFromInt(2) == TexPerfil::Mobile);
    EXPECT(texPerfilFromInt(3) == TexPerfil::Qualidade);   // podre → default
    EXPECT(texPerfilFromInt(-1) == TexPerfil::Qualidade);
}

// ---- O PIN: mesma textura nos 3 perfis = 3 tamanhos distintos ---------------
TEST(perfis_tres_tamanhos_distintos) {
    FakeStorage st;
    TextureCache cache(st);
    HardwareCompressor hw;
    hw.setAstcSupported(true);   // ASTC 4x4 (o padrão da casa)
    const std::vector<u8> png = readFixture("green4096.png");
    ASSERT(png.size() > 8u);

    TexturePipeline pipe(hw, cache);
    pipe.setMaxTextureSize(0);   // sem teto device neste teste

    pipe.setPerfil(TexPerfil::Qualidade);
    const PipeOut q = runPipe(pipe, png, "t4k");
    ASSERT(q.ok);
    EXPECT(q.comp.width == 4096u && q.comp.height == 4096u);
    EXPECT(!q.info.reduced);
    EXPECT(q.comp.format == CompressedFormat::ASTC_4x4);
    EXPECT(q.info.srgb && !q.info.normalMap);
    EXPECT((q.comp.flags & kTexFlagSrgb) != 0);

    pipe.setPerfil(TexPerfil::Equilibrado);
    const PipeOut e = runPipe(pipe, png, "t4k");
    ASSERT(e.ok);
    EXPECT(e.comp.width == 2048u && e.comp.height == 2048u);
    EXPECT(e.info.reduced);
    EXPECT(e.info.reducedFromW == 4096u);
    EXPECT(e.comp.format == CompressedFormat::ASTC_4x4);

    pipe.setPerfil(TexPerfil::Mobile);
    const PipeOut m = runPipe(pipe, png, "t4k");
    ASSERT(m.ok);
    EXPECT(m.comp.width == 1024u && m.comp.height == 1024u);
    EXPECT(m.info.reduced);

    // 3 tamanhos distintos, sempre comprimidos
    EXPECT(q.comp.data.size() > e.comp.data.size());
    EXPECT(e.comp.data.size() > m.comp.data.size());
    EXPECT(q.comp.data.size() != e.comp.data.size());
    EXPECT(e.comp.data.size() != m.comp.data.size());

    // o cache guardou TRÊS entradas distintas (uma por perfil)
    std::vector<std::string> cacheFiles;
    ASSERT(st.listDir(TextureCache::kDir, cacheFiles));
    EXPECT(cacheFiles.size() == 3u);
}

// ---- o PIN: override por asset VENCE o perfil -------------------------------
TEST(override_por_asset_vence_o_perfil) {
    FakeStorage st;
    TextureCache cache(st);
    HardwareCompressor hw;
    hw.setAstcSupported(true);
    const std::vector<u8> png = readFixture("green4096.png");
    ASSERT(png.size() > 8u);

    TexturePipeline pipe(hw, cache);
    pipe.setPerfil(TexPerfil::Mobile);   // o perfil mandaria →1024

    // override 0 = NUNCA reduzir: a 4K sobrevive ao Mobile
    pipe.setOverride("especial", 0);
    const PipeOut keep = runPipe(pipe, png, "especial", TexJob{"especial"});
    ASSERT(keep.ok);
    EXPECT(keep.comp.width == 4096u && keep.comp.height == 4096u);
    EXPECT(!keep.info.reduced);
    EXPECT(keep.info.overrideDim == 0u);

    // override 512 = teto duro mesmo no Qualidade (que não reduz)
    pipe.setPerfil(TexPerfil::Qualidade);
    pipe.setOverride("mini", 512);
    const PipeOut mini = runPipe(pipe, png, "mini", TexJob{"mini"});
    ASSERT(mini.ok);
    EXPECT(mini.comp.width == 512u && mini.comp.height == 512u);
    EXPECT(mini.info.reduced);
    EXPECT(mini.info.overrideDim == 512u);

    // sem override → o perfil fala (Mobile reduz)
    pipe.setPerfil(TexPerfil::Mobile);
    const PipeOut normal = runPipe(pipe, png, "outro", TexJob{"outro"});
    ASSERT(normal.ok);
    EXPECT(normal.comp.width == 1024u);
    EXPECT(normal.info.overrideDim == 0u);
}

// ---- o PIN: textura gigante não crasha (GL_MAX_TEXTURE_SIZE) ----------------
TEST(gigante_nao_crasha_teto_do_device) {
    FakeStorage st;
    TextureCache cache(st);
    HardwareCompressor hw;
    hw.setAstcSupported(true);
    const std::vector<u8> png = readFixture("green4096.png");
    ASSERT(png.size() > 8u);

    TexturePipeline pipe(hw, cache);
    pipe.setPerfil(TexPerfil::Qualidade);   // o perfil não reduz…
    pipe.setMaxTextureSize(1024);           // …o device limita (C33 fake)

    const PipeOut r = runPipe(pipe, png, "gigante");
    ASSERT(r.ok);                           // NUNCA erro, nunca crash
    EXPECT(r.comp.width == 1024u && r.comp.height == 1024u);
    EXPECT(r.info.reduced);
    EXPECT(r.info.reducedFromW == 4096u);
    EXPECT(r.comp.ok());                    // o blob é válido e abre
    // e o override «nunca reduzir» NÃO vence a FÍSICA do device:
    pipe.setOverride("forca", 0);
    const PipeOut f = runPipe(pipe, png, "forca", TexJob{"forca"});
    ASSERT(f.ok);
    EXPECT(f.comp.width == 1024u);   // o teto do device manda mesmo assim
}

// ---- o PIN: ETC2 verde em device sem ASTC ------------------------------------
TEST(etc2_verde_sem_astc_com_perfil) {
    FakeStorage st;
    TextureCache cache(st);
    HardwareCompressor hw;
    hw.setAstcSupported(false);   // o device sem a extensão KHR
    const std::vector<u8> png = readFixture("green4096.png");
    ASSERT(png.size() > 8u);

    TexturePipeline pipe(hw, cache);
    pipe.setPerfil(TexPerfil::Mobile);
    const PipeOut r = runPipe(pipe, png, "semastc");
    ASSERT(r.ok);
    EXPECT(r.comp.format == CompressedFormat::ETC2_RGB);   // verde opaco
    EXPECT(r.comp.width == 1024u && r.comp.height == 1024u);
    EXPECT(r.comp.mips.size() == 11u);   // 1024 → 1
    // e a variante ETC2 tem CHAVE PRÓPRIA no cache (não colide com ASTC):
    // outro pipeline COM astc → miss e ASTC 4x4
    HardwareCompressor hw2;
    hw2.setAstcSupported(true);
    TextureCache cache2(st);
    TexturePipeline pipe2(hw2, cache2);
    pipe2.setPerfil(TexPerfil::Mobile);
    const PipeOut r2 = runPipe(pipe2, png, "semastc");
    ASSERT(r2.ok);
    EXPECT(r2.comp.format == CompressedFormat::ASTC_4x4);   // variante a1
    EXPECT(!r2.info.cacheHit);
}

// ---- mips filtrados em ESPAÇO LINEAR (a luz certa) ---------------------------
TEST(mips_em_espaco_linear) {
    // transferência: os cantos e o meio (a fórmula oficial sRGB)
    EXPECT(::test::nearEqF(srgbToLinear(0), 0.0f));
    EXPECT(::test::nearEqF(srgbToLinear(255), 1.0f));
    EXPECT(::test::nearEqF(srgbToLinear(128), 0.2158f, 1e-3f));
    EXPECT(linearToSrgbByte(0.0f) == 0);
    EXPECT(linearToSrgbByte(1.0f) == 255);
    EXPECT(linearToSrgbByte(0.5f) == 188);   // a LUZ média de preto+branco

    // tabuleiro preto/branco 8×8: o mip nível 3 (todos os pixéis)
    RawImage tab;
    tab.width = 8;
    tab.height = 8;
    tab.rgba.assign(8u * 8u * 4u, 0);
    for (u32 y = 0; y < 8; ++y) {
        for (u32 x = 0; x < 8; ++x) {
            const u8 v = ((x / 2 + y / 2) % 2) ? 255 : 0;
            u8* p = &tab.rgba[(static_cast<size_t>(y) * 8u + x) * 4u];
            p[0] = p[1] = p[2] = v;
            p[3] = 255;
        }
    }
    std::vector<RawImage> chainL, chainB;
    genMipChainRGBA(tab, chainL, MipSpace::SrgbLinear);
    genMipChainRGBA(tab, chainB, MipSpace::Bytes);
    ASSERT(chainL.size() >= 4u);
    const RawImage& ml = chainL[3];   // 1×1
    const RawImage& mb = chainB[3];
    // em LUZ: média 0.5 linear → 188 sRGB; em BYTES: 127 (escurecido)
    EXPECT(ml.rgba[0] >= 186u && ml.rgba[0] <= 190u);
    EXPECT(mb.rgba[0] >= 126u && mb.rgba[0] <= 129u);
    EXPECT(ml.rgba[0] > mb.rgba[0]);   // a prova do 50%
}

// ---- o PIN: normal maps SEM PERDA de canais (RGBA8) --------------------------
TEST(normal_map_rgba8_sem_perda_de_canais) {
    FakeStorage st;
    TextureCache cache(st);
    HardwareCompressor hw;
    hw.setAstcSupported(true);
    const std::vector<u8> png = makePngQuadrantes(300, 300);   // não-múltiplo
    ASSERT(png.size() > 8u);

    TexturePipeline pipe(hw, cache);
    pipe.setPerfil(TexPerfil::Mobile);
    const PipeOut r = runPipe(pipe, png, "norm", TexJob{"norm", true});
    ASSERT(r.ok);
    // sem compressão (o ASTC destruiria os canais) e sem redução (≤1024)
    EXPECT(r.comp.format == CompressedFormat::RGBA8);
    EXPECT(!r.info.srgb && r.info.normalMap);
    EXPECT((r.comp.flags & kTexFlagNormal) != 0);
    EXPECT((r.comp.flags & kTexFlagSrgb) == 0);

    // a prova «sem perda»: os bytes do nível 0 são EXATAMENTE o decode
    RawImage ref;
    std::string perr;
    ASSERT(loadPng(png.data(), png.size(), ref, perr));
    ASSERT(r.comp.data.size() == ref.rgba.size());
    EXPECT(std::memcmp(r.comp.data.data(), ref.rgba.data(),
                       ref.rgba.size()) == 0);

    // Mobile REDUZ a resolução da normal 2048→1024 (a política da resolução
    // é perfil; o FORMATO é que é sem perda — os canais ficam intatos)
    const std::vector<u8> png2 = makePngQuadrantes(2048, 2048);
    ASSERT(png2.size() > 8u);
    const PipeOut r2 = runPipe(pipe, png2, "norm2", TexJob{"norm2", true});
    ASSERT(r2.ok);
    EXPECT(r2.comp.format == CompressedFormat::RGBA8);
    EXPECT(r2.comp.width == 1024u);
    EXPECT(r2.info.normalMap && r2.info.reduced);
}

// ---- .gtext v2: flags viajam; v1 continua a abrir -----------------------------
TEST(gtext_v2_flags_e_v1_retrocompativel) {
    CompressedImage img;
    img.format = CompressedFormat::ETC2_RGB;
    img.width = 8;
    img.height = 8;
    img.flags = kTexFlagSrgb;
    img.data.assign(32, 0x5A);   // 8/4×8/4 blocos × 8 B = 32 B
    CompressedMip m;
    m.width = 8;
    m.height = 8;
    m.offset = 0;
    m.size = 32;
    img.mips.push_back(m);
    std::vector<u8> out;
    std::string err;
    ASSERT(writeGText(img, out, err));

    CompressedImage back;
    ASSERT(readGText(out.data(), out.size(), back, err));
    EXPECT(back.flags == kTexFlagSrgb);
    EXPECT(back.format == CompressedFormat::ETC2_RGB);
    EXPECT(back.width == 8u);

    // v1: o MESMO payload SEM o byte de flags + header versão 1 (o payload
    // muda → payloadSize e checksum do header têm de ser refeitos)
    std::vector<u8> v1;
    const u8* src = out.data();
    const size_t hdr = 32;   // kGHeaderBytes
    v1.insert(v1.end(), src, src + hdr);
    // header: [0..3 magic][4..5 versão u16][6..7 endian][8..11 align]
    //         [12..19 payloadSize u64][20..27 checksum u64][28..31 reserved]
    v1[4] = 1;   // versão 1 (era 2 — u16 little-endian NOS OFFSETS 4..5)
    v1[5] = 0;
    const size_t pay2 = out.size() - hdr;
    v1.push_back(src[hdr]);   // format
    v1.insert(v1.end(), src + hdr + 2, src + hdr + pay2);   // salta flags
    const u64 pay1 = pay2 - 1;
    for (int i = 0; i < 8; ++i) {   // payloadSize u64 @12..19
        v1[hdr - 20 + i] = static_cast<u8>((pay1 >> (8 * i)) & 0xFF);
    }
    const u64 sum = gfnv1a(v1.data() + hdr, static_cast<size_t>(pay1));
    for (int i = 0; i < 8; ++i) {   // checksum u64 @20..27
        v1[hdr - 12 + i] = static_cast<u8>((sum >> (8 * i)) & 0xFF);
    }
    CompressedImage back1;
    ASSERT(readGText(v1.data(), v1.size(), back1, err));
    EXPECT(back1.flags == 0u);   // v1 nunca mente: sem flags
    EXPECT(back1.width == 8u);
}

// ---- o cache POR PERFIL (3 perfis = 3 entradas; hit na repetição) ------------
TEST(cache_por_perfil_entradas_distintas) {
    FakeStorage st;
    TextureCache cache(st);
    HardwareCompressor hw;
    hw.setAstcSupported(true);
    const std::vector<u8> png = readFixture("green4096.png");
    ASSERT(png.size() > 8u);

    TexturePipeline pipe(hw, cache);
    pipe.setPerfil(TexPerfil::Equilibrado);
    const PipeOut a = runPipe(pipe, png, "k");
    ASSERT(a.ok && !a.info.cacheHit);
    const PipeOut a2 = runPipe(pipe, png, "k");
    ASSERT(a2.ok && a2.info.cacheHit);
    EXPECT(a2.comp.width == a.comp.width);   // o hit devolve O MESMO produto
    EXPECT(a2.comp.data.size() == a.comp.data.size());

    pipe.setPerfil(TexPerfil::Mobile);
    const PipeOut b = runPipe(pipe, png, "k");
    ASSERT(b.ok && !b.info.cacheHit);   // perfil novo = entrada nova
    EXPECT(b.comp.width == 1024u);

    // e a textura ALTERADA (hash novo) não reutiliza nenhuma entrada
    std::vector<u8> png2 = png;
    png2.push_back(0x00);   // (o PNG decode ignora a cauda; o hash NÃO)
    TextureCache cache2(st);
    HardwareCompressor hw2;
    hw2.setAstcSupported(true);
    TexturePipeline pipe2(hw2, cache2);
    pipe2.setPerfil(TexPerfil::Equilibrado);
    const PipeOut c = runPipe(pipe2, png2, "k");
    ASSERT(c.ok);
    EXPECT(!c.info.cacheHit);   // hash diferente → entrada nova, nunca falso-hit
}

// ---- tabela tempo/tamanho antes→depois por perfil (a do relatório) -----------
TEST(tabela_tempo_tamanho_por_perfil) {
    FakeStorage st;
    TextureCache cache(st);
    HardwareCompressor hw;
    hw.setAstcSupported(true);
    const std::vector<u8> png = readFixture("green4096.png");
    ASSERT(png.size() > 8u);

    TexturePipeline pipe(hw, cache);
    pipe.setMaxTextureSize(0);
    const char* nomes[3] = {"Qualidade", "Equilibrado", "Mobile"};
    const TexPerfil perfis[3] = {TexPerfil::Qualidade, TexPerfil::Equilibrado,
                                 TexPerfil::Mobile};
    u64 sizes[3] = {0, 0, 0};
    double ms[3] = {0, 0, 0};
    u32 dims[3] = {0, 0, 0};
    for (int i = 0; i < 3; ++i) {
        pipe.setPerfil(perfis[i]);
        const auto t0 = std::chrono::steady_clock::now();
        const PipeOut r = runPipe(pipe, png, "tabela");
        const auto t1 = std::chrono::steady_clock::now();
        ASSERT(r.ok);
        ms[i] = std::chrono::duration<double, std::milli>(t1 - t0).count();
        sizes[i] = r.comp.data.size();
        dims[i] = r.comp.width;
        std::printf("  [tabela] green4096 | %s: 4096→%u px, %llu KB, %.0f ms\n",
                    nomes[i], dims[i],
                    static_cast<unsigned long long>(sizes[i] / 1024u), ms[i]);
        std::fflush(stdout);
    }
    // a ordem da política: Mobile < Equilibrado < Qualidade (bytes finais)
    EXPECT(dims[0] == 4096u && dims[1] == 2048u && dims[2] == 1024u);
    EXPECT(sizes[2] < sizes[1]);
    EXPECT(sizes[1] < sizes[0]);
    EXPECT(ms[0] > 0.0 && ms[1] > 0.0 && ms[2] > 0.0);
}

// ---- o passe de texturas PARALELO do conversor (GLB com 4 texturas) ---------
TEST(conversor_passe_texturas_paralelo_4_texturas) {
    // o conversor STREAMING precisa de staging em disco REAL (o temporário
    // do corte em .staging/) — FakeStorage (raiz /fake) não serve AQUI
    const std::string base =
        std::string("/tmp/goni_5a_") +
        std::to_string(static_cast<long>(::getpid()));
    system(("mkdir -p " + base + "/proj " + base + "/logs").c_str());
    FsStorage st((base + "/proj").c_str());
    TextureCache cache(st);
    HardwareCompressor hw;
    hw.setAstcSupported(true);

    const std::vector<u8> png = readFixture("green4096.png");
    ASSERT(png.size() > 8u);
    // 4 imagens (0..3); as 0..2 com baseColor e a 3 marcada NORMAL pelo 5.º
    // material — o perfil Mobile reduz as de cor 4096→1024 e a normal segue
    // RGBA8 sem perda (com a resolução 4096→1024 da tabela)
    const u32 nImgs = 4u;
    const std::string json = gltfComTexturasFull(nImgs, 3, 42u);

    // o JSON das imagens (b64 da fixture 4K — grande) entra AQUI: as images
    // são data: URI (o caminho das imagens embutidas; o buffer é o BIN)
    std::string imgs;
    const std::string b64 = b64encode(png.data(), png.size());
    for (u32 i = 0; i < nImgs; ++i) {
        imgs += (i ? "," : "");
        imgs += "{\"uri\":\"data:image/png;base64," + b64 + "\"}";
    }
    const size_t posImgs = json.find("\"images\":[]");
    ASSERT(posImgs != std::string::npos);
    const std::string jsonFull =
        json.substr(0, posImgs) + "\"images\":[" + imgs + "]" +
        json.substr(posImgs + std::strlen("\"images\":[]"));

    // GLB: header 12 + JSON chunk (pad 4 com espaços) + BIN chunk (pad 4)
    u32 po, pl, io, il;
    const std::vector<u8> buf = triBuffer(po, pl, io, il);
    std::vector<u8> jsonBytes(jsonFull.begin(), jsonFull.end());
    while (jsonBytes.size() % 4u != 0u) {
        jsonBytes.push_back(0x20);   // espaço (o pad do spec GLB)
    }
    std::vector<u8> bin = buf;
    while (bin.size() % 4u != 0u) {
        bin.push_back(0);
    }
    const u32 total = 12u + 8u +
                      static_cast<u32>(jsonBytes.size()) + 8u +
                      static_cast<u32>(bin.size());
    std::vector<u8> glb;
    glb.reserve(total);
    const auto u32le = [&glb](u32 v) {
        glb.push_back(static_cast<u8>(v));
        glb.push_back(static_cast<u8>(v >> 8));
        glb.push_back(static_cast<u8>(v >> 16));
        glb.push_back(static_cast<u8>(v >> 24));
    };
    glb.insert(glb.end(), {'g', 'l', 'T', 'F'});
    u32le(2);            // versão
    u32le(total);
    u32le(static_cast<u32>(jsonBytes.size()));
    glb.insert(glb.end(), {'J', 'S', 'O', 'N'});
    glb.insert(glb.end(), jsonBytes.begin(), jsonBytes.end());
    u32le(static_cast<u32>(bin.size()));
    glb.insert(glb.end(), {'B', 'I', 'N', 0});
    glb.insert(glb.end(), bin.begin(), bin.end());

    // a FONTE tem de ser um ficheiro REAL (o conversor lê-a por fileapi)
    const std::string src = base + "/modelo.glb";
    FILE* f = std::fopen(src.c_str(), "wb");
    ASSERT(f != nullptr);
    std::fwrite(glb.data(), 1, glb.size(), f);
    std::fclose(f);

    TexturePipeline pipe(hw, cache);
    pipe.setPerfil(TexPerfil::Mobile);
    convert::Output out;
    convert::Stats stats;
    std::string err;
    const bool ok = convert::importFile(src, "modelo.glb", st, &pipe, out,
                                        stats, err);
    ASSERT(ok);
    EXPECT(err.empty());
    EXPECT(stats.textures == 4u);
    EXPECT(stats.texReduced == 4u);   // as 4 são 4096 → Mobile baixa todas
    EXPECT(stats.texNormal == 1u);

    // os 4 ficheiros existem, EM ORDEM, e os de cor abriram a 1024
    ASSERT(out.textures.size() == 4u);
    for (int i = 0; i < 3; ++i) {
        std::vector<u8> bytes;
        ASSERT(st.readBytes(out.textures[static_cast<size_t>(i)], bytes));
        CompressedImage img;
        ASSERT(readGText(bytes.data(), bytes.size(), img, err));
        EXPECT(img.width == 1024u);
        EXPECT((img.flags & kTexFlagSrgb) != 0);
    }
    // a imagem 3 = normal map: RGBA8 SEM perda (os bytes batem com o decode)
    {
        std::vector<u8> bytes;
        ASSERT(st.readBytes(out.textures[3], bytes));
        CompressedImage img;
        ASSERT(readGText(bytes.data(), bytes.size(), img, err));
        EXPECT(img.format == CompressedFormat::RGBA8);
        EXPECT((img.flags & kTexFlagNormal) != 0);
        EXPECT(img.width == 1024u);   // a resolução segue o perfil; canais não
        // (a prova byte a byte da sem-perda está no teste do pipeline acima —
        //  aqui o caminho do CONVERSOR é o que se prova: normal=1 + RGBA8)
    }
    system((std::string("rm -rf ") + base).c_str());
}
