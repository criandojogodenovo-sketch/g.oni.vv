// assets/TextureCache.cpp — persistência do blob comprimido (F5.1-A).
//
// Chave do ficheiro = hash do PNG (o formato é derivado do conteúdo e
// vive DENTRO do header). Inteiros little-endian EXPLÍCITO (byte a byte)
// — o formato em disco não depende da ordem da máquina. Corrupção ou
// truncamento → load() devolve miss com erro, nunca crash.
#include "assets/TextureCache.h"
#include <cstdio>
#include <cstring>

namespace vv {

const char* TextureCache::kDir = "textures/cache";

namespace {

constexpr u8 kMagic[4] = {'G', 'V', 'T', 'C'};
constexpr u8 kVersion = 1;
constexpr size_t kHeaderSize = 28;   // até à tabela de mips

void putU32(std::vector<u8>& v, u32 x) {
    v.push_back(static_cast<u8>(x));
    v.push_back(static_cast<u8>(x >> 8));
    v.push_back(static_cast<u8>(x >> 16));
    v.push_back(static_cast<u8>(x >> 24));
}

void putU64(std::vector<u8>& v, u64 x) {
    for (int i = 0; i < 8; ++i) {
        v.push_back(static_cast<u8>(x >> (8 * i)));
    }
}

u32 getU32(const u8* p) {
    return static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8) |
           (static_cast<u32>(p[2]) << 16) | (static_cast<u32>(p[3]) << 24);
}

u64 getU64(const u8* p) {
    u64 v = 0;
    for (int i = 7; i >= 0; --i) {
        v = (v << 8) | p[i];
    }
    return v;
}

} // namespace

u64 TextureCache::hashBytes(const u8* data, size_t len) {
    // FNV-1a 64
    u64 h = 0xcbf29ce484222325ull;
    for (size_t i = 0; i < len; ++i) {
        h ^= data[i];
        h *= 0x100000001b3ull;
    }
    return h;
}

std::string TextureCache::fileNameFor(u64 hash, CompressedFormat) {
    char buf[40];
    std::snprintf(buf, sizeof(buf), "cache_%016llx.gtc",
                  static_cast<unsigned long long>(hash));
    return buf;   // (2º parâmetro reservado — a chave é só o hash do PNG)
}

bool TextureCache::load(u64 hash, CompressedImage& out,
                        std::string& err) const {
    out = CompressedImage{};
    const std::string rel = std::string(kDir) + "/" + fileNameFor(hash, out.format);
    std::vector<u8> bytes;
    if (!st_.readBytes(rel, bytes) || bytes.empty()) {
        ++misses_;
        err = "cache: miss";
        return false;
    }
    const u8* p = bytes.data();
    if (bytes.size() < kHeaderSize || std::memcmp(p, kMagic, 4) != 0) {
        ++misses_;
        err = "cache: magic/tamanho inválido";
        return false;
    }
    if (p[4] != kVersion) {
        ++misses_;
        err = "cache: versão desconhecida";
        return false;
    }
    const auto format = static_cast<CompressedFormat>(p[5]);
    const u32 w = getU32(p + 8);
    const u32 h = getU32(p + 12);
    const u32 mipCount = getU32(p + 16);
    const u64 storedHash = getU64(p + 20);
    if (storedHash != hash || w == 0 || h == 0 || mipCount == 0 ||
        mipCount > 32u) {
        ++misses_;
        err = "cache: header incoerente";
        return false;
    }
    const size_t mipTableBytes = static_cast<size_t>(mipCount) * 16u;
    if (bytes.size() < kHeaderSize + mipTableBytes) {
        ++misses_;
        err = "cache: truncado (tabela de mips)";
        return false;
    }

    out.format = format;
    out.width = w;
    out.height = h;
    out.mips.resize(mipCount);
    for (u32 i = 0; i < mipCount; ++i) {
        const u8* e = p + kHeaderSize + static_cast<size_t>(i) * 16u;
        CompressedMip& m = out.mips[i];
        m.width = getU32(e);
        m.height = getU32(e + 4);
        m.offset = getU32(e + 8);
        m.size = getU32(e + 12);
        if (m.size == 0 || m.width == 0 || m.height == 0 ||
            m.offset != (i == 0 ? 0u : out.mips[i - 1].offset + out.mips[i - 1].size)) {
            ++misses_;
            err = "cache: mips incoerentes";
            return false;
        }
    }
    // blob = tudo depois da tabela; o último mip tem de terminar nele
    const CompressedMip& last = out.mips[mipCount - 1];
    const size_t blobStart = kHeaderSize + mipTableBytes;
    if (static_cast<size_t>(last.offset) + last.size != bytes.size() - blobStart) {
        ++misses_;
        err = "cache: blob truncado";
        return false;
    }
    out.data.assign(bytes.begin() + static_cast<std::ptrdiff_t>(blobStart),
                    bytes.end());
    ++hits_;
    err.clear();
    return true;
}

bool TextureCache::store(u64 hash, const CompressedImage& img,
                         std::string& err) {
    err.clear();
    if (!img.ok()) {
        err = "cache: imagem inválida para store";
        return false;
    }
    std::vector<u8> bytes;
    bytes.reserve(kHeaderSize + img.mips.size() * 16u + img.data.size());
    bytes.insert(bytes.end(), kMagic, kMagic + 4);
    bytes.push_back(kVersion);
    bytes.push_back(static_cast<u8>(img.format));
    bytes.push_back(0);
    bytes.push_back(0);
    putU32(bytes, img.width);
    putU32(bytes, img.height);
    putU32(bytes, static_cast<u32>(img.mips.size()));
    putU64(bytes, hash);
    for (const CompressedMip& m : img.mips) {
        putU32(bytes, m.width);
        putU32(bytes, m.height);
        putU32(bytes, m.offset);
        putU32(bytes, m.size);
    }
    bytes.insert(bytes.end(), img.data.begin(), img.data.end());

    if (!st_.makeDirs(kDir)) {
        err = "cache: falha ao criar " + std::string(kDir);
        return false;
    }
    const std::string rel = std::string(kDir) + "/" + fileNameFor(hash, img.format);
    if (!st_.writeBytes(rel, bytes.data(), bytes.size())) {
        err = "cache: falha ao escrever " + rel;
        return false;
    }
    return true;
}

} // namespace vv
