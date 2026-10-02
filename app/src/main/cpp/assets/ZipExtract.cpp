// assets/ZipExtract.cpp — parser ZIP próprio + inflate zlib (0.8.10).
//
// Formato consumido (o subset que os zips reais usam):
//   EOCD (fim do ficheiro, sig 0x06054b50): nº de entradas + offset do
//     central directory; procurado nos últimos 64 KB (comment ≤ 65 535 B);
//   Central directory entry (sig 0x02014b50): nome, método (0/8), CRC32,
//     comprimido/não-comprimido, offset do local header;
//   Local file header (sig 0x04034b50): 30 B fixos + nome + extra — o
//     DATA segue; o extra do local pode DIVERGIR do central (lê-se do
//     local, como manda a spec).
// Bits 0-2 dos flags = encriptado → rejeitado (erro legível).
// Zip64: rejeitado com erro legível (0xFFFFFFFF sentinelas — archives
// >4 GB não são o caso do device; documentado).
#include "assets/ZipExtract.h"

#include <zlib.h>

#include <cstdio>
#include <cstring>

#include "platform/EngineLog.h"
#include "platform/FileApi.h"

namespace vv::zip {

namespace {

constexpr u32 kSigEocd = 0x06054b50u;
constexpr u32 kSigCde  = 0x02014b50u;
constexpr u32 kSigLfh  = 0x04034b50u;

u16 rd16(const u8* p) {
    return static_cast<u16>(p[0] | (p[1] << 8));
}
u32 rd32(const u8* p) {
    return static_cast<u32>(p[0]) | (static_cast<u32>(p[1]) << 8) |
           (static_cast<u32>(p[2]) << 16) | (static_cast<u32>(p[3]) << 24);
}

// lê um range EXATO de uma FILE*
bool readAt(FILE* f, u64 off, void* dst, size_t n) {
    if (std::fseek(f, static_cast<long>(off), SEEK_SET) != 0) {
        return false;
    }
    return n == 0 || std::fread(dst, 1, n, f) == n;
}

struct Cde {
    std::string name;
    u16 method = 0;
    u16 flags = 0;
    u32 crc = 0;
    u64 compSize = 0;
    u64 uncompSize = 0;
    u64 lfhOffset = 0;
};

// encontra o EOCD (procura do fim; comment variável)
bool findEocd(FILE* f, u64 fileSize, u64& cdOffset, u32& cdCount,
              std::string& err) {
    const size_t scan = fileSize < 65558 ? static_cast<size_t>(fileSize)
                                         : 65558;
    std::vector<u8> tail(scan);
    if (!readAt(f, fileSize - scan, tail.data(), scan)) {
        err = "leitura do fim do archive falhou";
        return false;
    }
    // procura o EOCD do fim para o início (comment final variável)
    const size_t start = scan >= 22 ? scan - 22 : 0;
    for (size_t i = start + 1; i-- > 0;) {
        if (rd32(tail.data() + i) == kSigEocd) {
            const u8* e = tail.data() + i;
            if (i + 22 > scan) {
                continue;
            }
            cdCount = rd16(e + 10);
            cdOffset = rd32(e + 16);
            if (cdCount == 0xFFFF || cdOffset == 0xFFFFFFFFu) {
                err = "archive ZIP64 (>4 GB) nao suportado ainda";
                return false;
            }
            return true;
        }
    }
    err = "nao e um .zip valido (EOCD ausente)";
    return false;
}

// um ficheiro extraído STREAMING (stored direto; deflate por inflate)
bool extractOne(FILE* f, ProjectStorage& st, const Cde& cde,
                const std::string& destRel, u64& totalOut,
                bool (*onProgress)(void*, u64, u64), void* user, u64 fileSize,
                u64& filePos, ExtractStats& stats, std::string& err) {
    // local header: 30 B + nameLen + extraLen → data
    u8 lfh[30];
    if (!readAt(f, cde.lfhOffset, lfh, 30) || rd32(lfh) != kSigLfh) {
        err = "local header corrompido: " + cde.name;
        return false;
    }
    const u16 nameLen = rd16(lfh + 26);
    const u16 extraLen = rd16(lfh + 28);
    const u64 dataOff = cde.lfhOffset + 30 + nameLen + extraLen;

    // bomb-guard POR ENTRADA (declaração do central directory)
    if (cde.uncompSize > kMaxEntryOut) {
        err = "archive: bomb-guard entrada '" + cde.name + "' declara " +
              std::to_string(cde.uncompSize / (1024 * 1024)) +
              " MB (teto " + std::to_string(kMaxEntryOut / (1024 * 1024)) +
              " MB)";
        return false;
    }
    if (totalOut + cde.uncompSize > kMaxTotalOut) {
        err = "archive: bomb-guard total excederia 2 GB (ratio=" +
              std::to_string(
                  fileSize > 0 ? static_cast<u64>(
                                     (totalOut + cde.uncompSize) / fileSize)
                               : 0ull) +
              ")";
        return false;
    }

    st.makeDirs(destRel.substr(0, destRel.rfind('/')));
    const int h = st.openWriteStream(destRel);
    if (h <= 0) {
        err = "storage recusou a escrita de: " + destRel;
        return false;
    }
    bool ok = true;
    u32 crc = 0;
    if (cde.method == 0) {   // STORED: cópia direta por chunks
        std::vector<u8> buf(kZipChunk);
        u64 left = cde.compSize;
        u64 off = dataOff;
        while (left > 0 && ok) {
            const size_t want = left < buf.size() ? static_cast<size_t>(left)
                                                  : buf.size();
            if (!readAt(f, off, buf.data(), want)) {
                err = "leitura curta em: " + cde.name;
                ok = false;
                break;
            }
            if (!st.writeStreamChunk(h, buf.data(), want)) {
                err = "escrita falhou em: " + destRel;
                ok = false;
                break;
            }
            crc = static_cast<u32>(crc32(crc, buf.data(),
                                         static_cast<uInt>(want)));
            totalOut += want;
            off += want;
            left -= want;
            filePos = off;
            if (onProgress && !onProgress(user, filePos, fileSize)) {
                stats.canceled = true;
                err = "cancelado";
                ok = false;
            }
        }
    } else if (cde.method == 8) {   // DEFLATE: inflate streaming por chunks
        z_stream zs{};
        if (inflateInit2(&zs, -15) != Z_OK) {   // raw deflate (sem header zlib)
            st.closeWriteStream(h);
            err = "inflate init falhou";
            return false;
        }
        std::vector<u8> in(kZipChunk);
        std::vector<u8> out(kZipChunk);
        u64 left = cde.compSize;
        u64 off = dataOff;
        while (left > 0 && ok) {
            const size_t want = left < in.size() ? static_cast<size_t>(left)
                                                 : in.size();
            if (!readAt(f, off, in.data(), want)) {
                err = "leitura curta em: " + cde.name;
                ok = false;
                break;
            }
            zs.next_in = in.data();
            zs.avail_in = static_cast<uInt>(want);
            do {
                zs.next_out = out.data();
                zs.avail_out = static_cast<uInt>(out.size());
                const int rc = inflate(&zs, Z_NO_FLUSH);
                if (rc != Z_OK && rc != Z_STREAM_END && rc != Z_BUF_ERROR) {
                    err = "deflate corrompido em: " + cde.name;
                    ok = false;
                    break;
                }
                const size_t got = out.size() - zs.avail_out;
                if (got > 0) {
                    if (!st.writeStreamChunk(h, out.data(), got)) {
                        err = "escrita falhou em: " + destRel;
                        ok = false;
                        break;
                    }
                    crc = static_cast<u32>(
                        crc32(crc, out.data(), static_cast<uInt>(got)));
                    totalOut += got;
                }
                if (rc == Z_STREAM_END) {
                    break;
                }
            } while (zs.avail_in > 0 && ok);
            off += want;
            left -= want;
            filePos = off;
            if (onProgress && !onProgress(user, filePos, fileSize)) {
                stats.canceled = true;
                err = "cancelado";
                ok = false;
            }
        }
        inflateEnd(&zs);
    } else {
        err = "metodo de compressao " + std::to_string(cde.method) +
              " desconhecido em: " + cde.name;
        ok = false;
    }
    st.closeWriteStream(h);
    if (!ok) {
        // SEM ESTADO PARCIAL: a entrada meio-extraída sai
        if (st.remove(destRel)) {
            elog::info("archive: entrada incompleta '%s' removida (sem "
                       "estado parcial)", destRel.c_str());
        }
        return false;
    }
    // CRC do conteúdo conferido contra o central directory (integridade)
    if (crc != cde.crc) {
        err = "CRC errado em '" + cde.name + "' (archive danificado)";
        if (st.remove(destRel)) {
            elog::info("archive: entrada com CRC errado '%s' removida",
                       destRel.c_str());
        }
        return false;
    }
    ++stats.files;
    stats.totalOut = totalOut;
    elog::info("archive: extract %s ok (%llu B)", destRel.c_str(),
               static_cast<unsigned long long>(cde.uncompSize));
    return true;
}

} // namespace

bool entryNameSafe(const std::string& name) {
    if (name.empty() || name.front() == '/' || name.front() == '\\') {
        return false;   // absoluto
    }
    if (name.find('\\') != std::string::npos) {
        return false;   // separador windows → caminho estranho, rejeita
    }
    if (name.compare(0, 2, "./") == 0) {
        return false;
    }
    // ".." como SEGMENTO (não como prefixo de nome legítimo tipo "..a")
    size_t i = 0;
    while (i < name.size()) {
        const size_t slash = name.find('/', i);
        const std::string seg =
            name.substr(i, slash == std::string::npos ? std::string::npos
                                                      : slash - i);
        if (seg == ".." || seg.empty()) {
            return false;   // segmento vazio ("a//b") também é lixo
        }
        if (slash == std::string::npos) {
            break;
        }
        i = slash + 1;
    }
    return true;
}

bool extractArchive(const std::string& srcAbs, ProjectStorage& st,
                    const std::string& destDir, ExtractStats& stats,
                    std::string& err,
                    bool (*onProgress)(void*, u64, u64), void* user) {
    stats = ExtractStats{};
    err.clear();
    FILE* f = std::fopen(srcAbs.c_str(), "rb");
    if (!f) {
        err = "archive ilegivel: " + srcAbs + " (" + fileapi::errnoText() + ")";
        return false;
    }
    bool ok = false;
    do {
        std::fseek(f, 0, SEEK_END);
        const long sz = std::ftell(f);
        std::fseek(f, 0, SEEK_SET);
        if (sz <= 0) {
            err = "archive vazio";
            break;
        }
        const u64 fileSize = static_cast<u64>(sz);
        stats.archiveBytes = fileSize;
        u64 cdOffset = 0;
        u32 cdCount = 0;
        if (!findEocd(f, fileSize, cdOffset, cdCount, err)) {
            break;
        }
        elog::info("archive: open %s entries=%u", srcAbs.c_str(), cdCount);
        stats.entries = cdCount;
        ok = true;
        u64 totalOut = 0;
        u64 filePos = 0;
        // central directory: lê uma entrada de cada vez (nomes podem ser
        // longos — buffer por entrada, nunca o CD inteiro em RAM)
        for (u32 i = 0; i < cdCount && ok; ++i) {
            u8 cde[46];
            if (!readAt(f, cdOffset, cde, 46) || rd32(cde) != kSigCde) {
                err = "central directory corrompido (entrada " +
                      std::to_string(i) + ")";
                ok = false;
                break;
            }
            Cde e;
            e.flags = rd16(cde + 8);
            e.method = rd16(cde + 10);
            e.crc = rd32(cde + 16);
            e.compSize = rd32(cde + 20);
            e.uncompSize = rd32(cde + 24);
            const u16 nameLen = rd16(cde + 28);
            const u16 extraLen = rd16(cde + 30);
            const u16 commentLen = rd16(cde + 32);
            e.lfhOffset = rd32(cde + 42);
            if (nameLen == 0 || nameLen > 4096) {
                err = "nome de entrada invalido (comprimento " +
                      std::to_string(nameLen) + ")";
                ok = false;
                break;
            }
            std::vector<u8> nm(nameLen);
            if (!readAt(f, cdOffset + 46, nm.data(), nameLen)) {
                err = "nome de entrada truncado";
                ok = false;
                break;
            }
            e.name.assign(reinterpret_cast<const char*>(nm.data()), nameLen);
            cdOffset += 46 + nameLen + extraLen + commentLen;
            // encriptado → recusa legível
            if (e.flags & 0x0007) {
                err = "entrada encriptada nao suportada: " + e.name;
                ok = false;
                break;
            }
            // ZIP-SLIP: nome fora do destino → REJEITA (nunca escreve fora)
            if (!entryNameSafe(e.name)) {
                ++stats.rejected;
                elog::warn("archive: zip-slip rejeitado %s", e.name.c_str());
                continue;
            }
            const std::string destRel = destDir + "/" + e.name;
            // archive ANINHADO → ignora (não extrai recursivamente) + log
            const size_t dot = e.name.rfind('.');
            std::string low = dot == std::string::npos ? "" : e.name.substr(dot);
            for (char& c : low) {
                if (c >= 'A' && c <= 'Z') {
                    c = static_cast<char>(c - 'A' + 'a');
                }
            }
            if (low == ".zip" || low == ".rar") {
                ++stats.nested;
                elog::info("archive: aninhado '%s' ignorado (extrai "
                           "manualmente depois)", e.name.c_str());
                continue;
            }
            if (!e.name.empty() && e.name.back() == '/') {   // diretório
                st.makeDirs(destRel);
                ++stats.dirs;
                continue;
            }
            if (!extractOne(f, st, e, destRel, totalOut, onProgress, user,
                            fileSize, filePos, stats, err)) {
                ok = false;
                break;
            }
        }
        if (ok) {
            // rácio do bomb-guard no LOG (a prova de que o teto vigia)
            const u64 ratio =
                fileSize > 0 ? totalOut / fileSize : 0ull;
            elog::info("archive: extraido %u ficheiro(s) para %s "
                       "(bomb-guard ratio=%llu)",
                       stats.files, destDir.c_str(),
                       static_cast<unsigned long long>(ratio));
        }
    } while (false);
    std::fclose(f);
    return ok;
}

} // namespace vv::zip
