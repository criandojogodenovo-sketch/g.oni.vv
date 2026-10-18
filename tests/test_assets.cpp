// tests/test_assets.cpp — F5-D: decode PNG (stb_image) + gate 4K→2K.
// Fixtures reais em tests/fixtures/ (RGB 16x16 e RGBA 8x8, geradas por
// make_png_fixtures.py); o gate é testado com imagens em memória.
#include "TestFramework.h"
#include <cstdio>
#include <string>
#include "assets/PngLoader.h"

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

} // namespace

TEST(png_decode_fixture_rgb_vermelho) {
    const std::vector<u8> bytes = readFixture("red16.png");
    EXPECT(bytes.size() > 8u);
    RawImage img;
    std::string err;
    EXPECT(loadPng(bytes.data(), bytes.size(), img, err));
    EXPECT(err.empty());
    EXPECT(img.width == 16u && img.height == 16u);
    EXPECT(img.ok());
    // todos os pixels = vermelho puro, alpha 255 (RGB → RGBA)
    for (u32 i = 0; i < 16u * 16u; ++i) {
        const u8* px = &img.rgba[i * 4];
        EXPECT(px[0] == 255 && px[1] == 0 && px[2] == 0 && px[3] == 255);
    }
}

TEST(png_decode_fixture_rgba_alpha) {
    const std::vector<u8> bytes = readFixture("alpha8.png");
    EXPECT(bytes.size() > 8u);
    RawImage img;
    std::string err;
    EXPECT(loadPng(bytes.data(), bytes.size(), img, err));
    EXPECT(img.width == 8u && img.height == 8u);
    // cor constante (64,128,192) + alpha gradiente (x+y)*15+17
    for (u32 y = 0; y < 8; ++y) {
        for (u32 x = 0; x < 8; ++x) {
            const u8* px = &img.rgba[(y * 8u + x) * 4];
            EXPECT(px[0] == 64 && px[1] == 128 && px[2] == 192);
            EXPECT(px[3] == static_cast<u8>((x + y) * 15 + 17));
        }
    }
}

TEST(png_invalido_falha_com_erro) {
    RawImage img;
    std::string err;
    EXPECT(!loadPng(nullptr, 0, img, err));
    const u8 lixo[] = "isto definitivamente nao e um png";
    EXPECT(!loadPng(lixo, sizeof(lixo), img, err));
    EXPECT(!err.empty());
    // PNG truncado (só a assinatura)
    const u8 trunc[] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    EXPECT(!loadPng(trunc, sizeof(trunc), img, err));
}

namespace {

RawImage fillImg(u32 w, u32 h, u8 r, u8 g, u8 b, u8 a) {
    RawImage img;
    img.width = w;
    img.height = h;
    img.rgba.resize(static_cast<size_t>(w) * h * 4);
    for (size_t i = 0; i < img.rgba.size(); i += 4) {
        img.rgba[i] = r;
        img.rgba[i + 1] = g;
        img.rgba[i + 2] = b;
        img.rgba[i + 3] = a;
    }
    return img;
}

} // namespace

TEST(gate_1k_2k_passam_sem_aviso) {
    std::string warn;
    RawImage k1 = fillImg(1024, 1024, 10, 20, 30, 255);
    EXPECT(!downscaleTo2K(k1, warn));
    EXPECT(warn.empty());
    EXPECT(k1.width == 1024u && k1.height == 1024u);
    RawImage k2 = fillImg(2048, 2048, 10, 20, 30, 255);
    EXPECT(!downscaleTo2K(k2, warn));
    EXPECT(warn.empty());
    // 2048×1024 (não-quadrado no limite) também passa
    RawImage k3 = fillImg(2048, 1024, 1, 2, 3, 4);
    EXPECT(!downscaleTo2K(k3, warn));
    EXPECT(warn.empty());
}

TEST(gate_4k_reduz_para_2k_com_aviso) {
    std::string warn;
    RawImage k4 = fillImg(4096, 4096, 100, 150, 200, 255);
    EXPECT(downscaleTo2K(k4, warn));
    EXPECT(k4.width == 2048u && k4.height == 2048u);
    EXPECT(k4.ok());
    // box 2×2 de pixels constantes = o próprio valor
    const u8* px = &k4.rgba[(1024u * 2048u + 512u) * 4];
    EXPECT(px[0] == 100 && px[1] == 150 && px[2] == 200 && px[3] == 255);
    EXPECT(!warn.empty());
    EXPECT(warn.find("4096x4096") != std::string::npos);
    EXPECT(warn.find("2048x2048") != std::string::npos);
    EXPECT(warn.find("gate 2K") != std::string::npos);
}

TEST(gate_multiplas_reducoes_ate_caber) {
    std::string warn;
    // 8192×4096 → 4096×2048 → 2048×1024 (2 iterações)
    RawImage g = fillImg(8192, 4096, 5, 6, 7, 8);
    EXPECT(downscaleTo2K(g, warn));
    EXPECT(g.width == 2048u && g.height == 1024u);
    const u8* px = &g.rgba[(512u * 2048u + 256u) * 4];
    EXPECT(px[0] == 5 && px[1] == 6 && px[2] == 7 && px[3] == 8);
}

TEST(gate_box_media_exata_por_quadrantes) {
    // 4096×4096 com quadrantes 2×2 distintos: (0,10,20,30) → média 15
    RawImage img;
    img.width = 4096;
    img.height = 4096;
    img.rgba.resize(static_cast<size_t>(4096) * 4096 * 4);
    for (u32 y = 0; y < 4096; ++y) {
        for (u32 x = 0; x < 4096; ++x) {
            u8* px = &img.rgba[(static_cast<size_t>(y) * 4096u + x) * 4];
            px[0] = static_cast<u8>(((x % 2) * 2 + (y % 2)) * 10);   // 0/10/20/30
            px[1] = 0;
            px[2] = 0;
            px[3] = 255;
        }
    }
    std::string warn;
    EXPECT(downscaleTo2K(img, warn));
    EXPECT(img.width == 2048u);
    // pixel (1,1) da saída = média de (2,2)=30? — padrão: (x%2)*2+(y%2) →
    // bloco 2×2 contém {0,10,20,30} → média 15 em qualquer posição
    for (int probe = 0; probe < 8; ++probe) {
        const u32 x = static_cast<u32>(probe) * 137u;
        const u32 y = static_cast<u32>(probe) * 211u;
        const u8* px = &img.rgba[(static_cast<size_t>(y) * 2048u + x) * 4];
        EXPECT(px[0] == 15);
        EXPECT(px[3] == 255);
    }
}

// ---- F5-D/2: interface TextureCompressor (passthrough da F5) -----------------
#include "assets/TextureCompressor.h"

TEST(compressor_passthrough_identidade) {
    PassthroughCompressor pc;
    EXPECT(std::string(pc.name()) == "passthrough");

    RawImage in = fillImg(32, 16, 200, 100, 50, 255);
    CompressedImage out;
    std::string err;
    EXPECT(pc.compress(in, out, err));
    EXPECT(err.empty());
    EXPECT(out.ok());
    EXPECT(out.format == CompressedFormat::RGBA8);
    EXPECT(out.width == 32u && out.height == 16u);
    EXPECT(out.data == in.rgba);   // bytes saem como entraram
    // entrada não mutada
    EXPECT(in.width == 32u && in.height == 16u && in.ok());

    // caminho do pipeline: decode PNG → compressor → formato aceitável
    const std::vector<u8> bytes = readFixture("alpha8.png");
    RawImage img;
    EXPECT(loadPng(bytes.data(), bytes.size(), img, err));
    std::string warn;
    downscaleTo2K(img, warn);   // 8x8: sem efeito
    CompressedImage comp;
    EXPECT(pc.compress(img, comp, err));
    EXPECT(comp.width == 8u && comp.height == 8u);
    EXPECT(comp.data.size() == 8u * 8u * 4u);
}

TEST(compressor_entrada_invalida_falha) {
    PassthroughCompressor pc;
    RawImage vazio;
    CompressedImage out;
    std::string err;
    EXPECT(!pc.compress(vazio, out, err));
    EXPECT(!err.empty());
    EXPECT(!out.ok());
}

// ---- F5.1-A: compressão ETC2/ASTC + mips em CPU ------------------------------
#include "assets/MipGen.h"
#include <cstring>
#include <set>

TEST(mips_helpers_contagens_e_tamanhos) {
    EXPECT(mipCountFor(64, 64) == 7u);    // 64,32,16,8,4,2,1
    EXPECT(mipCountFor(8, 8) == 4u);      // 8,4,2,1
    EXPECT(mipCountFor(70, 66) == 7u);    // 70 → 35..1 (7 níveis)
    EXPECT(mipCountFor(1, 1) == 1u);
    EXPECT(mipCountFor(2048, 4) == 12u);  // 2048..1

    // ETC2 RGB: 8 bytes por bloco 4×4; RGBA EAC: 16
    EXPECT(mipBytesFor(CompressedFormat::ETC2_RGB, 64, 64) == 2048u);
    EXPECT(mipBytesFor(CompressedFormat::ETC2_RGBA, 64, 64) == 4096u);
    // dimensões não múltiplas de 4: teto por bloco
    EXPECT(mipBytesFor(CompressedFormat::ETC2_RGB, 66, 70) ==
           static_cast<size_t>(17) * 18 * 8);
    // ASTC: 16 bytes por bloco (4×4 ou 6×6)
    EXPECT(mipBytesFor(CompressedFormat::ASTC_4x4, 64, 64) == 4096u);
    EXPECT(mipBytesFor(CompressedFormat::ASTC_6x6, 64, 64) ==
           static_cast<size_t>(11) * 11 * 16);
    EXPECT(mipBytesFor(CompressedFormat::RGBA8, 8, 8) == 256u);

    EXPECT(formatIsCompressed(CompressedFormat::RGBA8) == false);
    EXPECT(formatIsCompressed(CompressedFormat::ETC2_RGB) == true);
    EXPECT(std::string(formatName(CompressedFormat::ETC2_RGB)) == "ETC2 RGB");
    EXPECT(std::string(formatName(CompressedFormat::ETC2_RGBA)) == "ETC2 EAC");
    EXPECT(std::string(formatName(CompressedFormat::ASTC_4x4)) == "ASTC 4x4");
}

TEST(mip_chain_dims_e_box) {
    RawImage in = fillImg(8, 8, 200, 100, 50, 255);
    std::vector<RawImage> chain;
    genMipChainRGBA(in, chain);
    EXPECT(chain.size() == 4u);   // 8,4,2,1
    EXPECT(chain[0].width == 8u && chain[0].height == 8u);
    EXPECT(chain[1].width == 4u && chain[2].width == 2u &&
           chain[3].width == 1u && chain[3].height == 1u);
    // sólido: todos os níveis mantêm a cor
    for (const RawImage& m : chain) {
        for (size_t i = 0; i < m.rgba.size(); i += 4) {
            EXPECT(m.rgba[i] == 200 && m.rgba[i + 1] == 100 &&
                   m.rgba[i + 2] == 50 && m.rgba[i + 3] == 255);
        }
    }
    // dims ímpares: 5x3 → 2x1 → 1x1 (último px replica)
    RawImage odd = fillImg(5, 3, 10, 20, 30, 40);
    std::vector<RawImage> chain2;
    genMipChainRGBA(odd, chain2);
    EXPECT(chain2.size() == 3u);
    EXPECT(chain2[1].width == 2u && chain2[1].height == 1u);
    // média box 2×2: pixels (0..1)x(0..1) de cor constante → igual
    EXPECT(chain2[1].rgba[0] == 10 && chain2[1].rgba[1] == 20 &&
           chain2[1].rgba[2] == 30 && chain2[1].rgba[3] == 40);
}

TEST(etc2_solido_rgb_dimensoes_e_blocos_identicos) {
    Etc2Compressor c;
    c.heuristics = false;   // caminho ETC1-style determinístico

    RawImage in = fillImg(64, 64, 255, 0, 0, 255);
    CompressedImage out;
    std::string err;
    EXPECT(c.compress(in, out, err));
    EXPECT(err.empty());
    EXPECT(out.ok());
    EXPECT(in.ok());   // entrada não mutada
    EXPECT(out.format == CompressedFormat::ETC2_RGB);   // opaco → RGB8
    EXPECT(out.width == 64u && out.height == 64u);
    EXPECT(out.mips.size() == 7u);

    // nível 0: 16×16 blocos × 8 bytes = 2048
    EXPECT(out.mips[0].width == 64u && out.mips[0].height == 64u);
    EXPECT(out.mips[0].offset == 0u);
    EXPECT(out.mips[0].size == 2048u);
    // soma dos níveis = tamanho do blob (blob contíguo sem buracos)
    size_t total = 0;
    u32 prevEnd = 0;
    for (const CompressedMip& m : out.mips) {
        EXPECT(m.offset == prevEnd);
        prevEnd = m.offset + m.size;
        total += m.size;
    }
    EXPECT(total == out.data.size());

    // sólido: TODOS os blocos 8 bytes do nível 0 são idênticos
    for (u32 b = 1; b < out.mips[0].size / 8u; ++b) {
        EXPECT(std::memcmp(out.data.data() + b * 8, out.data.data(), 8) == 0);
    }
}

TEST(etc2_rgba_eac_estrutura_e_blob_alinhado) {
    Etc2Compressor c;
    c.heuristics = false;

    // alpha com gradiente → RGBA8_EAC (16 bytes por bloco)
    RawImage in;
    in.width = 64;
    in.height = 64;
    in.rgba.resize(64u * 64u * 4u);
    for (u32 y = 0; y < 64; ++y) {
        for (u32 x = 0; x < 64; ++x) {
            u8* px = &in.rgba[(y * 64u + x) * 4];
            px[0] = 40; px[1] = 80; px[2] = 120;
            px[3] = static_cast<u8>(x * 4);
        }
    }
    CompressedImage out;
    std::string err;
    EXPECT(c.compress(in, out, err));
    EXPECT(out.format == CompressedFormat::ETC2_RGBA);
    EXPECT(out.mips[0].size == 4096u);   // 16×16 blocos × 16 bytes

    // offsets alinhados ao tamanho do nível (blob contíguo, sem sobreposição)
    u32 expectOff = 0;
    for (const CompressedMip& m : out.mips) {
        EXPECT(m.offset == expectOff);
        EXPECT(m.size == mipBytesFor(out.format, m.width, m.height));
        expectOff += m.size;
    }
    EXPECT(expectOff == out.data.size());
}

TEST(etc2_pad_nao_multiplo_de_4) {
    Etc2Compressor c;
    c.heuristics = false;
    RawImage in = fillImg(66, 70, 10, 200, 30, 255);
    CompressedImage out;
    std::string err;
    EXPECT(c.compress(in, out, err));
    EXPECT(out.width == 66u && out.height == 70u);   // dims preservadas
    // teto por bloco: ceil(66/4)=17 × ceil(70/4)=18 × 8 bytes
    EXPECT(out.mips[0].size == static_cast<u32>(17 * 18 * 8));
    EXPECT(out.mips.size() == 7u);   // max(66,70)=70 → 7 níveis
}

TEST(etc2_gradiente_responde_e_deterministico) {
    RawImage in;
    in.width = 256;
    in.height = 256;
    in.rgba.resize(256u * 256u * 4u);
    for (u32 y = 0; y < 256; ++y) {
        for (u32 x = 0; x < 256; ++x) {
            u8* px = &in.rgba[(y * 256u + x) * 4];
            px[0] = static_cast<u8>(x);
            px[1] = static_cast<u8>(y);
            px[2] = static_cast<u8>((x + y) / 2u);
            px[3] = 255;
        }
    }
    Etc2Compressor c1, c2;
    c1.heuristics = false;
    c2.heuristics = false;
    CompressedImage o1, o2;
    std::string err;
    EXPECT(c1.compress(in, o1, err));
    EXPECT(c2.compress(in, o2, err));
    // determinístico: mesma entrada → mesmo blob
    EXPECT(o1.data.size() == o2.data.size());
    EXPECT(std::memcmp(o1.data.data(), o2.data.data(), o1.data.size()) == 0);

    // gradiente: os blocos NÃO são todos iguais (o codificador responde)
    std::set<std::string> uniq;
    for (u32 b = 0; b < o1.mips[0].size / 8u; ++b) {
        uniq.insert(std::string(reinterpret_cast<const char*>(o1.data.data()) + b * 8, 8));
    }
    EXPECT(uniq.size() > 32u);
}

TEST(astc_4x4_estrutura_e_tamanhos) {
    AstcCompressor c(4);
    RawImage in;
    in.width = 64;
    in.height = 64;
    in.rgba.resize(64u * 64u * 4u);
    for (u32 y = 0; y < 64; ++y) {
        for (u32 x = 0; x < 64; ++x) {
            u8* px = &in.rgba[(y * 64u + x) * 4];
            px[0] = static_cast<u8>(x * 4);
            px[1] = static_cast<u8>(y * 4);
            px[2] = 128;
            px[3] = 255;
        }
    }
    CompressedImage out;
    std::string err;
    EXPECT(c.compress(in, out, err));
    EXPECT(err.empty());
    EXPECT(out.ok());
    EXPECT(out.format == CompressedFormat::ASTC_4x4);
    EXPECT(out.mips.size() == 7u);
    EXPECT(out.mips[0].size == 4096u);   // 16×16 blocos × 16 bytes
    // cadeia: 4096+1024+256+64+16+16+16
    EXPECT(out.data.size() == 5488u);
    u32 expectOff = 0;
    for (const CompressedMip& m : out.mips) {
        EXPECT(m.offset == expectOff);
        expectOff += m.size;
    }
    EXPECT(expectOff == out.data.size());
}

TEST(astc_6x6_tamanho_por_bloco) {
    AstcCompressor c(6);
    EXPECT(c.blockX() == 6u);
    RawImage in = fillImg(64, 64, 5, 10, 15, 255);
    CompressedImage out;
    std::string err;
    EXPECT(c.compress(in, out, err));
    EXPECT(out.format == CompressedFormat::ASTC_6x6);
    // ceil(64/6) = 11 → 11×11 blocos × 16 bytes
    EXPECT(out.mips[0].size == static_cast<u32>(11 * 11 * 16));
    // bloco inválido → fixa em 4
    AstcCompressor c7(7);
    EXPECT(c7.blockX() == 4u);
}

TEST(auto_selecao_astc_etc2_e_gate_256) {
    HardwareCompressor hw;

    // < 256px em qualquer dimensão → RGBA8 (sem compressão, 1 mip)
    {
        RawImage in = fillImg(128, 128, 1, 2, 3, 255);
        CompressedImage out;
        std::string err;
        EXPECT(hw.compress(in, out, err));
        EXPECT(out.format == CompressedFormat::RGBA8);
        EXPECT(out.mips.size() == 1u);
        EXPECT(out.data == in.rgba);
        EXPECT(hw.lastFormat() == CompressedFormat::RGBA8);
    }
    // borda exata: 255x256 → não comprime; 256x256 → comprime
    {
        RawImage in = fillImg(255, 256, 1, 2, 3, 255);
        CompressedImage out;
        std::string err;
        EXPECT(hw.compress(in, out, err));
        EXPECT(out.format == CompressedFormat::RGBA8);
    }
    {
        RawImage in = fillImg(256, 256, 1, 2, 3, 255);
        CompressedImage out;
        std::string err;
        EXPECT(HardwareCompressor::compressible(256, 256));
        EXPECT(!HardwareCompressor::compressible(255, 256));

        // ASTC disponível → ASTC 4x4
        hw.setAstcSupported(true);
        EXPECT(hw.compress(in, out, err));
        EXPECT(out.format == CompressedFormat::ASTC_4x4);
        EXPECT(hw.lastFormat() == CompressedFormat::ASTC_4x4);

        // sem ASTC, opaco → ETC2 RGB
        hw.setAstcSupported(false);
        CompressedImage out2;
        EXPECT(hw.compress(in, out2, err));
        EXPECT(out2.format == CompressedFormat::ETC2_RGB);
        EXPECT(hw.lastFormat() == CompressedFormat::ETC2_RGB);

        // sem ASTC, com alpha → ETC2 EAC
        RawImage inA = fillImg(256, 256, 10, 20, 30, 128);
        CompressedImage out3;
        EXPECT(hw.compress(inA, out3, err));
        EXPECT(out3.format == CompressedFormat::ETC2_RGBA);
    }
    // entrada inválida falha com erro
    {
        RawImage vazio;
        CompressedImage out;
        std::string err;
        EXPECT(!hw.compress(vazio, out, err));
        EXPECT(!err.empty());
    }
}

TEST(passthrough_estrutura_um_mip) {
    PassthroughCompressor pc;
    RawImage in = fillImg(32, 16, 9, 8, 7, 6);
    CompressedImage out;
    std::string err;
    EXPECT(pc.compress(in, out, err));
    EXPECT(err.empty());
    EXPECT(out.ok());
    EXPECT(out.format == CompressedFormat::RGBA8);
    EXPECT(out.mips.size() == 1u);
    EXPECT(out.mips[0].size == 32u * 16u * 4u);
    EXPECT(out.mips[0].offset == 0u);
    EXPECT(out.data == in.rgba);
}

// ---- F5.1-A: cache em disco + pipeline (gate 4K) ------------------------------
#include "assets/TextureCache.h"
#include "assets/TexturePipeline.h"
#include "FakeStorage.h"

namespace {

std::vector<u8> readFixture(const std::string& name) {
    // (a versão const char* está acima; esta recebe std::string p/ variar)
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

} // namespace

TEST(cache_hash_distingue_conteudo) {
    const u8 a[] = {1, 2, 3, 4};
    const u8 b[] = {1, 2, 3, 5};
    const u8 a2[] = {1, 2, 3, 4};
    const u64 ha = TextureCache::hashBytes(a, sizeof(a));
    EXPECT(ha == TextureCache::hashBytes(a2, sizeof(a2)));
    EXPECT(ha != TextureCache::hashBytes(b, sizeof(b)));
    EXPECT(TextureCache::hashBytes(nullptr, 0) ==
           0xcbf29ce484222325ull);   // FNV offset basis
    // nome de ficheiro determinístico
    EXPECT(TextureCache::fileNameFor(1, CompressedFormat::ETC2_RGB) ==
           TextureCache::fileNameFor(1, CompressedFormat::ASTC_4x4));
}

TEST(cache_store_load_roundtrip_hit) {
    FakeStorage st;
    TextureCache cache(st);

    CompressedImage img;
    img.format = CompressedFormat::ETC2_RGB;
    img.width = 8;
    img.height = 8;
    img.data.resize(32);
    for (size_t i = 0; i < img.data.size(); ++i) {
        img.data[i] = static_cast<u8>(i * 7);
    }
    CompressedMip m;
    m.width = 8; m.height = 8; m.offset = 0; m.size = 32;
    img.mips.push_back(m);

    const u64 hash = 0x1234567890abcdefull;
    std::string err;
    EXPECT(cache.store(hash, img, err));
    EXPECT(err.empty());
    // o ficheiro apareceu em textures/cache/
    std::vector<std::string> files;
    EXPECT(st.listDir(TextureCache::kDir, files));
    EXPECT(files.size() == 1u);

    CompressedImage out;
    EXPECT(cache.load(hash, out, err));
    EXPECT(err.empty());
    EXPECT(out.format == img.format);
    EXPECT(out.width == 8u && out.height == 8u);
    EXPECT(out.data == img.data);
    EXPECT(out.mips.size() == 1u);
    EXPECT(out.mips[0].size == 32u);
    EXPECT(cache.hits() == 1u);
}

TEST(cache_miss_corrompido_e_hash_divergente) {
    FakeStorage st;
    TextureCache cache(st);

    // 1) ausente → miss
    CompressedImage out;
    std::string err;
    EXPECT(!cache.load(42, out, err));
    EXPECT(!err.empty());
    EXPECT(cache.misses() == 1u);

    // 2) ficheiro corrompido → miss (nunca crash)
    CompressedImage img;
    img.format = CompressedFormat::ETC2_RGBA;
    img.width = 4; img.height = 4;
    img.data.assign(64, 0xAB);
    CompressedMip m;
    m.width = 4; m.height = 4; m.offset = 0; m.size = 64;
    img.mips.push_back(m);
    const u64 hash = 77;
    EXPECT(cache.store(hash, img, err));
    const std::string rel =
        std::string(TextureCache::kDir) + "/" + TextureCache::fileNameFor(hash, img.format);
    st.files[rel] = "lixo que não é um cache";   // corrompe por cima
    EXPECT(!cache.load(hash, out, err));
    EXPECT(!err.empty());

    // 3) truncado → miss
    std::vector<u8> truncado(img.data.size() + 28 + 16);
    st.files[rel].assign(reinterpret_cast<const char*>(truncado.data()), 30);
    EXPECT(!cache.load(hash, out, err));

    // 4) hash divergente (header incoerente) → miss
    EXPECT(cache.load(hash + 1, out, err) == false);
    EXPECT(cache.hits() == 0u);
}

TEST(cache_png_alterado_invalida_entrada_antiga_permanece) {
    FakeStorage st;
    TextureCache cache(st);
    HardwareCompressor hw;   // ETC2 (hospedeiro sem ASTC marcado)
    hw.setAstcSupported(false);

    const std::vector<u8> png = readFixture("blue256.png");
    EXPECT(png.size() > 8u);

    // 1ª carga: miss → comprime → store
    TexturePipeline pipe(hw, cache);
    CompressedImage out;
    TextureLoadInfo info;
    std::string err;
    const u64 h1 = TextureCache::hashBytes(png.data(), png.size());
    EXPECT(pipe.process(png.data(), png.size(), "blue256.png", out, info, err));
    EXPECT(err.empty());
    EXPECT(!info.cacheHit);
    EXPECT(info.format == CompressedFormat::ETC2_RGB);
    EXPECT(std::string(info.via) == "compress");
    EXPECT(pipe.lastHash() == h1);

    // 2ª carga do MESMO PNG: hit do disco, blob idêntico
    CompressedImage out2;
    TextureLoadInfo info2;
    EXPECT(pipe.process(png.data(), png.size(), "blue256.png", out2, info2, err));
    EXPECT(info2.cacheHit);
    EXPECT(std::string(info2.via) == "cache");
    EXPECT(out2.data == out.data);
    EXPECT(cache.hits() == 1u && cache.misses() == 1u);

    // PNG ALTERADO (um byte muda) → hash novo → miss → nova entrada
    std::vector<u8> png2 = png;
    png2[png2.size() - 1] ^= 0xFF;
    CompressedImage out3;
    TextureLoadInfo info3;
    EXPECT(pipe.process(png2.data(), png2.size(), "blue256b.png", out3, info3, err));
    EXPECT(!info3.cacheHit);
    EXPECT(cache.misses() == 2u);
    // 2 ficheiros no cache (o antigo permanece — limpeza é manual)
    std::vector<std::string> files;
    EXPECT(st.listDir(TextureCache::kDir, files));
    EXPECT(files.size() == 2u);
    // e o original ainda faz hit
    CompressedImage out4;
    TextureLoadInfo info4;
    EXPECT(pipe.process(png.data(), png.size(), "blue256.png", out4, info4, err));
    EXPECT(info4.cacheHit);
}

TEST(pipeline_4k_com_compressao_nao_reduz) {
    FakeStorage st;
    TextureCache cache(st);
    HardwareCompressor hw;
    hw.setAstcSupported(false);   // ETC2 (garantido) — 4K mantém

    const std::vector<u8> png = readFixture("green4096.png");
    EXPECT(png.size() > 8u);

    TexturePipeline pipe(hw, cache);
    CompressedImage out;
    TextureLoadInfo info;
    std::string err;
    EXPECT(pipe.process(png.data(), png.size(), "green4096.png", out, info, err));
    EXPECT(err.empty());
    EXPECT(info.warn.empty());          // com compressão NÃO há aviso
    EXPECT(out.width == 4096u && out.height == 4096u);   // SEM downscale
    EXPECT(out.format == CompressedFormat::ETC2_RGB);
    // critério: 4K ETC2 RGB ≈ 8 MB — 2048 blocos/linha × 8 bytes
    EXPECT(out.mips[0].size == 4096u * 4096u / 2u);
    EXPECT(out.mips[0].size == 8388608u);
    EXPECT(out.mips.size() == 13u);     // 4096 → 1
    // hit na 2ª carga
    CompressedImage out2;
    TextureLoadInfo info2;
    EXPECT(pipe.process(png.data(), png.size(), "green4096.png", out2, info2, err));
    EXPECT(info2.cacheHit);
    EXPECT(out2.data.size() == out.data.size());
}

TEST(pipeline_sem_compressao_4k_reduz_2k_com_aviso) {
    FakeStorage st;
    TextureCache cache(st);
    PassthroughCompressor passthrough;   // fallback: nunca comprime

    const std::vector<u8> png = readFixture("green4096.png");
    TexturePipeline pipe(passthrough, cache);
    CompressedImage out;
    TextureLoadInfo info;
    std::string err;
    EXPECT(pipe.process(png.data(), png.size(), "green4096.png", out, info, err));
    EXPECT(err.empty());
    EXPECT(out.width == 2048u && out.height == 2048u);   // gate 2K
    EXPECT(!info.warn.empty());                          // COM aviso
    EXPECT(out.format == CompressedFormat::RGBA8);
    EXPECT(std::string(info.via) == "raw");
    EXPECT(cache.misses() == 1u);
    // o RGBA do fallback TAMBÉM fica em cache — 2ª carga é hit
    CompressedImage out2;
    TextureLoadInfo info2;
    EXPECT(pipe.process(png.data(), png.size(), "green4096.png", out2, info2, err));
    EXPECT(info2.cacheHit);
    EXPECT(out2.width == 2048u);   // vem do cache já reduzido
    EXPECT(cache.hits() == 1u);
}

TEST(pipeline_pequena_256_borda_exata_e_png_invalido) {
    FakeStorage st;
    TextureCache cache(st);
    HardwareCompressor hw;
    TexturePipeline pipe(hw, cache);

    // 256×256 → comprime (borda exata do gate)
    const std::vector<u8> blue = readFixture("blue256.png");
    CompressedImage out;
    TextureLoadInfo info;
    std::string err;
    EXPECT(pipe.process(blue.data(), blue.size(), "blue256.png", out, info, err));
    EXPECT(out.format == CompressedFormat::ETC2_RGB);
    EXPECT(out.width == 256u);

    // PNG inválido → erro claro, nada em cache
    const u8 lixo[] = "isto nao e um png";
    CompressedImage out2;
    TextureLoadInfo info2;
    EXPECT(!pipe.process(lixo, sizeof(lixo), "lixo.png", out2, info2, err));
    EXPECT(err.find("lixo.png") != std::string::npos);
    // vazio → erro
    EXPECT(!pipe.process(nullptr, 0, "vazio.png", out2, info2, err));
}
