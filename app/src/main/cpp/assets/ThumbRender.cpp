// assets/ThumbRender.cpp — o render SOFTWARE das miniaturas (PASSO 5C).
//
// TUDO AQUI É PURO (sem GL, sem storage): mesh → pixels. A câmara é uma
// ortográfica isométrica fixa (a vista 3/4 que o dono vê no viewport) com
// z-buffer de floats; o sombreamento é lambert flat com a direção de luz
// do Material (a casa tem UMA luz). O rasterizador é o clássico
// bounding-box + coordenadas baricêntricas — 256×256 × até 4096
// triângulos = trabalho de milissegundos, afervel no CI.
#include "assets/ThumbRender.h"
#include "render/ThumbPng.h"   // encodePngRgb (zlib) — o encode do 0.9.0
#include <algorithm>
#include <cmath>
#include <cstring>

namespace vv {
namespace thumbrender {

namespace {

// a luz da casa (Material.cpp kLightDir — já normalizada, PARA a luz)
constexpr float kLightDir[3] = {0.4545f, 0.7435f, 0.2891f};
constexpr float kAmbient = 0.16f;
constexpr float kAlbedo = 0.62f;

// o fundo (o grafite do chrome — o valor do painel sem os tokens da UI)
void putBg(u8* rgba) {
    rgba[0] = 0x14;
    rgba[1] = 0x14;
    rgba[2] = 0x16;
    rgba[3] = 0xFF;
}

struct Vec3F {
    float x, y, z;
};

Vec3F sub(const Vec3F& a, const Vec3F& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vec3F cross(const Vec3F& a, const Vec3F& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
            a.x * b.y - a.y * b.x};
}

float dot(const Vec3F& a, const Vec3F& b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

Vec3F norm(const Vec3F& a) {
    const float l = std::sqrt(dot(a, a));
    if (l <= 1e-9f) {
        return {0.0f, 0.0f, 0.0f};
    }
    return {a.x / l, a.y / l, a.z / l};
}

// o glifo de inicial 3×5 (o mínimo legível para as iniciais do placeholder;
// as letras que os stems do projeto realmente usam)
bool glyphPixel(char up, int gx, int gy) {
    // 3 colunas × 5 linhas por glifo (gx 0..2, gy 0..4)
    switch (up) {
        case 'A': return gy == 0 ? gx != 1 : (gy == 2 ? true : gx == 0 || gx == 2);
        case 'B': return gx == 0 || gy == 0 || gy == 2 || gy == 4 || (gx == 2 && (gy == 1 || gy == 3));
        case 'C': return gy == 0 || gy == 4 ? gx != 0 : gx == 0;
        case 'D': return gx == 0 || gy == 0 || gy == 4 || gx == 2;
        case 'E': return gx == 0 || gy == 0 || gy == 2 || gy == 4;
        case 'G': return gy == 0 || gy == 4 ? gx != 0 : (gx == 0 || (gy >= 2 && gx == 2));
        case 'H': return gx == 0 || gx == 2 || gy == 2;
        case 'I': return gy == 0 || gy == 4 ? true : gx == 1;
        case 'L': return gx == 0 || gy == 4;
        case 'M': return gx != 1 || gy == 1;
        case 'N': return gx == 0 || gx == 2 || gx == gy;
        case 'O': return gy == 0 || gy == 4 ? gx != 1 : gx == 0 || gx == 2;
        case 'P': return gx == 0 || gy == 0 || gy == 2 || (gx == 2 && gy == 1);
        case 'R': return gx == 0 || gy == 0 || gy == 2 || (gx == 2 && gy == 1) || (gx == gy && gx > 0);
        case 'S': return gy == 0 || gy == 2 || gy == 4 ? gx != 0 : gx == 2;
        case 'T': return gy == 0 ? true : gx == 1;
        case 'U': return gx == 0 || gx == 2 || gy == 4;
        case 'X': return gx == gy || gx + gy == 2;
        case '0': case '1': case '2': case '3': case '4':
        case '5': case '6': case '7': case '8': case '9':
            return gy == 0 || gy == 4 || gx == 0 || (gx == 2 && gy == 2);
        default: return gx == 0 || gx == 2 || gy == 0 || gy == 4;
    }
}

void drawInitials(const std::string& stem, u32 size, std::vector<u8>& rgba) {
    // as iniciais: 1.ª + última letra (a regra D18 — «G1» para "grelha1")
    char c0 = stem.empty() ? '?' : stem[0];
    char c1 = stem.size() > 1 ? stem[stem.size() - 1] : c0;
    c0 = static_cast<char>(std::toupper(static_cast<unsigned char>(c0)));
    c1 = static_cast<char>(std::toupper(static_cast<unsigned char>(c1)));
    const int s = static_cast<int>(size);
    const int cell = s / 8;   // o glifo 3×5 escala com a miniatura
    const int ox = s / 2 - (cell * 7) / 2;
    const int oy = s / 2 - (cell * 5) / 2;
    for (int gy = 0; gy < 5; ++gy) {
        for (int cx = 0; cx < 3; ++cx) {
            if (!glyphPixel(c0, cx, gy)) {
                continue;
            }
            for (int dy = 0; dy < cell; ++dy) {
                for (int dx = 0; dx < cell; ++dx) {
                    const int x = ox + cx * cell + dx;
                    const int y = oy + gy * cell + dy;
                    if (x < 0 || y < 0 || x >= s || y >= s) {
                        continue;
                    }
                    u8* p = &rgba[(static_cast<size_t>(y) * s + x) * 4];
                    p[0] = p[1] = p[2] = 0xB5;
                }
            }
        }
    }
    for (int gy = 0; gy < 5; ++gy) {
        for (int cx = 0; cx < 3; ++cx) {
            if (!glyphPixel(c1, cx, gy)) {
                continue;
            }
            for (int dy = 0; dy < cell; ++dy) {
                for (int dx = 0; dx < cell; ++dx) {
                    const int x = ox + cx * cell + cell * 4 + dx;
                    const int y = oy + gy * cell + dy;
                    if (x < 0 || y < 0 || x >= s || y >= s) {
                        continue;
                    }
                    u8* p = &rgba[(static_cast<size_t>(y) * s + x) * 4];
                    p[0] = p[1] = p[2] = 0xB5;
                }
            }
        }
    }
}

} // namespace

bool renderMesh(const MeshData& mesh, u32 size, std::vector<u8>& rgba) {
    rgba.assign(static_cast<size_t>(size) * size * 4, 0);
    if (!mesh.ok() || mesh.indices.size() < 3) {
        return false;
    }
    for (size_t i = 0; i < rgba.size(); i += 4) {
        putBg(&rgba[i]);
    }

    // ---- o AABB (só os vértices REFERENCIADOS — os órfãos não desenham) --
    Vec3F mn{1e30f, 1e30f, 1e30f}, mx{-1e30f, -1e30f, -1e30f};
    for (const u16 idx : mesh.indices) {
        if (idx >= mesh.vertices.size()) {
            return false;
        }
        const Vertex& v = mesh.vertices[idx];
        mn = {std::min(mn.x, v.pos.x), std::min(mn.y, v.pos.y),
              std::min(mn.z, v.pos.z)};
        mx = {std::max(mx.x, v.pos.x), std::max(mx.y, v.pos.y),
              std::max(mx.z, v.pos.z)};
    }
    const Vec3F center{(mn.x + mx.x) * 0.5f, (mn.y + mx.y) * 0.5f,
                       (mn.z + mx.z) * 0.5f};
    const Vec3F ext{std::max(mx.x - mn.x, 1e-3f), std::max(mx.y - mn.y, 1e-3f),
                    std::max(mx.z - mn.z, 1e-3f)};

    // ---- a câmara isométrica fixa (a vista 3/4 da casa) -----------------
    const Vec3F fwd = norm({-1.0f, -0.65f, -1.0f});   // PARA o mesh
    const Vec3F worldUp{0.0f, 1.0f, 0.0f};
    const Vec3F right = norm(cross(worldUp, fwd));
    const Vec3F up = cross(fwd, right);

    // o encaixe: o raio do AABB projetado nos eixos da câmara
    const float hx = std::abs(dot(right, {ext.x * 0.5f, 0, 0})) +
                     std::abs(dot(right, {0, ext.y * 0.5f, 0})) +
                     std::abs(dot(right, {0, 0, ext.z * 0.5f}));
    const float hy = std::abs(dot(up, {ext.x * 0.5f, 0, 0})) +
                     std::abs(dot(up, {0, ext.y * 0.5f, 0})) +
                     std::abs(dot(up, {0, 0, ext.z * 0.5f}));
    const float scale = (static_cast<float>(size) * 0.42f) /
                        std::max(hx, hy);

    // ---- o z-buffer + o rasterizador -------------------------------------
    std::vector<float> zbuf(static_cast<size_t>(size) * size, 1e30f);
    const size_t triCount =
        std::min<size_t>(mesh.indices.size() / 3, kMaxTrisPorThumb);
    const int s = static_cast<int>(size);
    for (size_t t = 0; t < triCount; ++t) {
        const Vertex& a = mesh.vertices[mesh.indices[t * 3]];
        const Vertex& b = mesh.vertices[mesh.indices[t * 3 + 1]];
        const Vertex& c = mesh.vertices[mesh.indices[t * 3 + 2]];
        Vec3F pa{a.pos.x - center.x, a.pos.y - center.y, a.pos.z - center.z};
        Vec3F pb{b.pos.x - center.x, b.pos.y - center.y, b.pos.z - center.z};
        Vec3F pc{c.pos.x - center.x, c.pos.y - center.y, c.pos.z - center.z};
        // a normal no MUNDO (para a luz)
        const Vec3F n = norm(cross(sub(pb, pa), sub(pc, pa)));
        const float shade = kAmbient + kAlbedo * std::max(dot(n, {kLightDir[0], kLightDir[1], kLightDir[2]}), 0.0f);
        // a projeção ortográfica (px = dot(right), py = dot(up), pz = depth);
        // o +hx/hy leva o AABB a [0, 0.84·s] e o +0.08·s CENTRA (a silhueta
        // com a mesma folga dos dois lados — 8% de cada lado)
        const float ax = (dot(right, pa) + hx) * scale + size * 0.08f;
        const float ay = (dot(up, pa) + hy) * scale;
        const float az = dot(fwd, pa);
        const float bx = (dot(right, pb) + hx) * scale + size * 0.08f;
        const float by = (dot(up, pb) + hy) * scale;
        const float bz = dot(fwd, pb);
        const float cx = (dot(right, pc) + hx) * scale + size * 0.08f;
        const float cy = (dot(up, pc) + hy) * scale;
        const float cz = dot(fwd, pc);
        // o y do ECRÃ cresce para BAIXO (o up da câmara inverte) — o
        // encaixe vertical soma o MESMO 0.08·s do horizontal
        const float sy0 = static_cast<float>(size) - ay - size * 0.08f;
        const float sy1 = static_cast<float>(size) - by - size * 0.08f;
        const float sy2 = static_cast<float>(size) - cy - size * 0.08f;
        const float minX = std::min({ax, bx, cx});
        const float maxX = std::max({ax, bx, cx});
        const float minY = std::min({sy0, sy1, sy2});
        const float maxY = std::max({sy0, sy1, sy2});
        const int x0 = std::max(0, static_cast<int>(minX));
        const int x1 = std::min(s - 1, static_cast<int>(maxX) + 1);
        const int y0 = std::max(0, static_cast<int>(minY));
        const int y1 = std::min(s - 1, static_cast<int>(maxY) + 1);
        // a área 2× do triângulo (o sinal diz a face — o w0/w1 abaixo
        // dividem por ela e ficam consistentes em AMBOS os sentidos)
        const float area = (bx - ax) * (sy2 - sy0) - (cx - ax) * (sy1 - sy0);
        if (std::abs(area) < 1e-9f) {
            continue;
        }
        for (int py = y0; py <= y1; ++py) {
            for (int px = x0; px <= x1; ++px) {
                const float fx = static_cast<float>(px) + 0.5f;
                const float fy = static_cast<float>(py) + 0.5f;
                const float w0 = ((bx - fx) * (sy2 - fy) - (cx - fx) * (sy1 - fy)) / area;
                const float w1 = ((cx - fx) * (sy0 - fy) - (ax - fx) * (sy2 - fy)) / area;
                const float w2 = 1.0f - w0 - w1;
                if (w0 < 0.0f || w1 < 0.0f || w2 < 0.0f) {
                    continue;
                }
                const float z = w0 * az + w1 * bz + w2 * cz;
                float& zRef = zbuf[static_cast<size_t>(py) * size + px];
                if (z < zRef) {
                    zRef = z;
                    u8* p = &rgba[(static_cast<size_t>(py) * size + px) * 4];
                    const u8 g = static_cast<u8>(
                        std::min(255.0f, std::max(0.0f, shade * 255.0f)));
                    p[0] = g;
                    p[1] = static_cast<u8>(std::min(
                        255.0f, std::max(0.0f, g * 0.98f + 10.0f)));
                    p[2] = static_cast<u8>(std::min(
                        255.0f, std::max(0.0f, g * 0.92f + 24.0f)));
                    p[3] = 0xFF;
                }
            }
        }
    }
    return true;
}

void placeholder(const std::string& stem, u32 size, std::vector<u8>& rgba) {
    rgba.assign(static_cast<size_t>(size) * size * 4, 0);
    for (size_t i = 0; i < rgba.size(); i += 4) {
        putBg(&rgba[i]);
    }
    // o quadro (a moldura do asset — 2 px nas bordas)
    const int s = static_cast<int>(size);
    for (int i = 0; i < s; ++i) {
        for (int b = 0; b < 2; ++b) {
            u8* top = &rgba[(static_cast<size_t>(b) * s + i) * 4];
            u8* bot = &rgba[(static_cast<size_t>(s - 1 - b) * s + i) * 4];
            u8* lft = &rgba[(static_cast<size_t>(i) * s + b) * 4];
            u8* rgt = &rgba[(static_cast<size_t>(i) * s + (s - 1 - b)) * 4];
            u8* all[4] = {top, bot, lft, rgt};
            for (u8* p : all) {
                p[0] = p[1] = p[2] = 0x3A;
                p[3] = 0xFF;
            }
        }
    }
    drawInitials(stem, size, rgba);
}

std::vector<u8> encodePng(const std::vector<u8>& rgba, u32 size) {
    if (rgba.size() != static_cast<size_t>(size) * size * 4) {
        return {};
    }
    // RGBA → RGB (o encode do ThumbPng é RGB) — drop alpha determinístico
    std::vector<u8> rgb(static_cast<size_t>(size) * size * 3);
    for (u32 y = 0; y < size; ++y) {
        for (u32 x = 0; x < size; ++x) {
            const size_t src = (static_cast<size_t>(y) * size + x) * 4;
            const size_t dst = (static_cast<size_t>(y) * size + x) * 3;
            rgb[dst] = rgba[src];
            rgb[dst + 1] = rgba[src + 1];
            rgb[dst + 2] = rgba[src + 2];
        }
    }
    return ::vv::thumb::encodePngRgb(rgb.data(), size, size);
}

} // namespace thumbrender
} // namespace vv
