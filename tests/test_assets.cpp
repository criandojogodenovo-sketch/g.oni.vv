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
    EXPECT(warn.find("F5.1") != std::string::npos);
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
