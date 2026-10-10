// render/BlockMesh.cpp — 0.10-M (PASSO 4): a implementação do render por
// blocos (abertura por faixas, frustum por bloco, lazy load, LRU com
// orçamentos, hull de bounds).
//
// A ABERTURA é pura até ao último byte: peek (192 B) → meta validada →
// faixa da tabela (blockCount×80 B) → faixa dos materiais. NENHUM dado de
// bloco é lido — o `import` que aplica um mesh de 972 MB ao TIC custa
// ~192 B + tabela (~75 KB para 920 blocos) + materiais.
#include "render/BlockMesh.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "core/ProjectStorage.h"
#include "platform/EngineLog.h"
#include "platform/FileApi.h"
#include "platform/Log.h"
#include "render/Mesh.h"
#include "render/Renderer.h"
#include "render/Texture.h"

namespace vv {

// teto da faixa de materiais lida de uma vez (nomes 4-alinhados — 1 MB é
// ~10k materiais de ~100 chars: muito além de qualquer modelo real; um
// ficheiro que precise de mais é recusado com erro legível)
constexpr size_t kMaterialsRangeMax = 1ull * 1024 * 1024;

u64 blockMeshVramBytes(const MeshData& block) {
    u64 n = static_cast<u64>(block.vertices.size()) * sizeof(Vertex) +
            static_cast<u64>(block.indices.size()) * 2u;
    if (block.skinned()) {
        n += static_cast<u64>(block.vertices.size()) * 20u;   // 4 u8 + 4 f32
    }
    return n;
}

BlockMesh::~BlockMesh() {
    close();
}

void BlockMesh::close() {
    destroyGpu();
    entries_.clear();
    blocks_.clear();
    materials_.clear();
    storage_ = nullptr;
    path_.clear();
    opened_ = false;
    frame_ = 0;
    evictedTotal_ = 0;
    uploadFails_ = 0;
    uploadDead_ = false;
    last_ = Stats{};
    churnWarned_ = false;
}

void BlockMesh::destroyGpu() {
    for (Entry& e : entries_) {
        if (e.gpu) {
            e.gpu->destroy();
            e.gpu.reset();
        }
        e.vramBytes = 0;
    }
}

bool BlockMesh::open(ProjectStorage& st, const std::string& relPath,
                     std::string& err) {
    close();
    const auto t0 = std::chrono::steady_clock::now();
    err.clear();

    // ---- 0.10.6 (SAF-SEAM) · A FONTE DA ABERTURA: a MESMA cascata do ------
    // conversor — openReadFd / mapFd64; se o provider recusar o mmap, o
    // pread das faixas NO MESMO fd; se o fd não sair (ou recusar o pread —
    // o pipe de um provider de cloud), os RANGES do readBytesAt de sempre.
    // NUNCA um caminho POSIX sob content://: o fd vem do bridge (no device
    // é o VvActivity.bridgeOpenFd). O fd/mapping vivem SÓ durante as 3
    // leituras da abertura (peek 192 B + tabela + materiais) — o lazy de
    // sempre é o readBytesAt (1 leitura por bloco — o contrato do PASSO 4
    // intocado, os pins da cache também).
    u64 fileBytes = 0;                  // o tamanho VISTO (nas linhas de falha)
    bool fileBytesKnown = false;
    int fd = -1;                        // o fd da cascata (fecha no fim)
    u8* map = nullptr;                  // o mmap POR FD (se o provider deu)
    unsigned long long mapLen = 0;
    const char* fonte = "ranges";       // o que as 3 leituras usaram (log)
    {
        std::string ferr;
        if (st.openReadFd(relPath, &fd, ferr)) {
            struct stat fst{};
            if (::fstat(fd, &fst) == 0 && fst.st_size > 0) {
                fileBytes = static_cast<u64>(fst.st_size);
                fileBytesKnown = true;
            }
            // fstat=0 (pipe/stream do provider) NÃO é o tamanho do
            // documento — o tamanho VISTO por este fd não conta para as
            // decisões de truncatura/quarentena (senão um ficheiro BOM
            // servido por pipe virava «truncado»); o pread decide a
            // seguir, e os ranges reabrem a visão REAL do documento
            if (fileBytesKnown) {
                map = static_cast<u8*>(fileapi::mapFd64(fd, 0, fileBytes,
                                                        &mapLen));
                if (map != nullptr) {
                    fonte = "mmap-fd";
                    // o mapping segura a própria referência (o contrato do
                    // mapFd64) — o fd fecha JÁ, como o conversor faz
                    ::close(fd);
                    fd = -1;
                } else if (fd >= 0) {
                    fonte = "pread-fd";   // o degradado: faixas NO MESMO fd
                }
                // o recuso do mapa é LOGADO pelo mapFd64 com o errno REAL
                // do SO (ENODEV de pipe, EOPNOTSUPP/EPERM de alguns FUSE)
            } else if (::fstat(fd, &fst) == 0 && fst.st_size == 0) {
                elog::warn("blocos: o fd de '%s' veio com 0 B (pipe/stream "
                           "do provider?) — o pread decide, e a seguir os "
                           "ranges",
                           relPath.c_str());
            }
        } else {
            // storage sem fds (default do ProjectStorage): os RANGES do
            // readBytesAt — o caminho de sempre do PASSO 4
            elog::info("blocos: '%s' sem fd (%s) — as 3 leituras vão pelos "
                       "RANGES do storage",
                       relPath.c_str(), ferr.c_str());
        }
    }
    if (!fileBytesKnown) {
        // o tamanho pela via do storage (Fs/Fake têm statBytes; o SAF puro
        // não — a seguir as sondas dos materiais cobrem a cauda)
        u64 sb = 0;
        if (st.statBytes(relPath, sb)) {
            fileBytes = sb;
            fileBytesKnown = true;
        }
    }
    const auto sizeText = [&]() -> std::string {
        return fileBytesKnown ? std::to_string(fileBytes) + " B"
                              : std::string("tamanho desconhecido (sem "
                                            "stat sob este storage)");
    };

    // a leitura de UMA faixa pela FONTE ativa (mapa → pread → ranges).
    // Em falha o `err` diz QUAL das 3 leituras, o errno e o tamanho visto
    // — a linha de sempre confundia «não encontrado» com «faixa
    // ilegível» SEM dizer qual nem porque (truncava a causa).
    auto readRange = [&](const char* what, u64 off, size_t len,
                         std::vector<u8>& out) -> bool {
        out.clear();
        if (len == 0) {
            return true;
        }
        if (map != nullptr) {
            if (off + len > mapLen) {
                err = relPath + ": a leitura da " + what + " falhou — a "
                      "faixa [" + std::to_string(off) + ", " +
                      std::to_string(off + len) +
                      ") vai além do mmap (" + std::to_string(mapLen) +
                      " B)";
                return false;
            }
            out.assign(map + off, map + off + len);
            return true;
        }
        if (fd >= 0) {
            out.resize(len);
            size_t got = 0;
            while (got < len) {
                const ssize_t r = ::pread(fd, out.data() + got, len - got,
                                          static_cast<off_t>(off + got));
                if (r <= 0) {
                    break;
                }
                got += static_cast<size_t>(r);
            }
            if (got == len) {
                return true;
            }
            const int e = errno;   // o errno AGORA (ESPIPE num pipe, EIO…)
            elog::error("blocos: '%s' — a leitura da %s [%llu, %llu) falhou "
                        "no fd — errno=%d (%s); ficheiro visto com %s; a "
                        "degradar para RANGES pelo storage",
                        relPath.c_str(), what,
                        static_cast<unsigned long long>(off),
                        static_cast<unsigned long long>(off + len), e,
                        std::strerror(e), sizeText().c_str());
            ::close(fd);
            fd = -1;   // o pipe não serve para pread — os ranges reabrem
            fonte = "ranges";
        }
        if (!st.readBytesAt(relPath, off, len, out)) {
            err = relPath + ": a leitura da " + what + " [" +
                  std::to_string(off) + ", " + std::to_string(off + len) +
                  ") falhou pelo storage — ficheiro visto com " +
                  sizeText() +
                  (fileBytesKnown && off + len > fileBytes
                       ? " (faixa além do fim — truncado?)"
                       : " (a causa do storage está nas linhas acima)");
            return false;
        }
        return true;
    };

    // 0.10.6 (SAF-SEAM) — A QUARENTENA do asset corrompido: a validação
    // da tabela/materiais falhou numa abertura CUJO peek validou o v3 (o
    // ficheiro DIZ que tem a tabela em X e NÃO TEM — truncado ou o CRC
    // divergiu). O asset sai do catálogo (rename .corrupt — os bytes
    // ficam para forense) e a mensagem manda REIMPORTAR: nunca o swap
    // silencioso, o pick mantém o estado anterior. Um peek que NÃO valida
    // (v1/v2/garbage) NÃO entra em quarentena — pode ser um v1 VÁLIDO a
    // caminho do mesh único (o blockHull só chega aqui com peek verde; a
    // chamada direta do v1 é o caminho de sempre do teste).
    auto quarentena = [&](const std::string& causa) -> void {
        const std::string corrupt = relPath + ".corrupt";
        const bool moved = st.rename(relPath, corrupt);
        if (moved) {
            elog::error("blocos: '%s' ASSET CORROMPIDO — em quarentena "
                        "'%s' (os bytes ficam; %s; ficheiro visto com %s) "
                        "— REIMPORTA o ficheiro original",
                        relPath.c_str(), corrupt.c_str(), causa.c_str(),
                        sizeText().c_str());
        } else {
            elog::error("blocos: '%s' ASSET CORROMPIDO (%s; ficheiro visto "
                        "com %s) — a quarentena .corrupt NÃO saiu (storage "
                        "sem rename); REIMPORTA o ficheiro original",
                        relPath.c_str(), causa.c_str(), sizeText().c_str());
        }
        err = relPath + ": asset corrompido, reimporta — " + causa;
    };

    // (1) o PEEK de sempre (192 B) — o MESMO guard do load inteiro
    std::vector<u8> peek;
    if (!readRange("faixa do PEEK (192 B)", 0,
                   kGHeaderBytes + kGmeshV3MetaBytes, peek)) {
        elog::error("blocos: '%s' FALHOU no PEEK — %s", relPath.c_str(),
                    err.c_str());
        if (map != nullptr) {
            fileapi::unmapFile64(map, mapLen);
        }
        if (fd >= 0) {
            ::close(fd);
        }
        return false;
    }
    if (!gmeshV3PeekMeta(peek.data(), peek.size(), meta_, err)) {
        // v1/v2/garbage — o mesh único decide (SEM quarentena: um v1
        // válido de <192 B morreria renomeado)
        err = relPath + ": o PEEK de 192 B falhou — " + err +
              " (ficheiro visto com " + sizeText() +
              "; o caminho do mesh único decide)";
        elog::error("blocos: '%s' FALHOU no PEEK — %s", relPath.c_str(),
                    err.c_str());
        if (map != nullptr) {
            fileapi::unmapFile64(map, mapLen);
        }
        if (fd >= 0) {
            ::close(fd);
        }
        return false;
    }

    // (2) a TABELA pela faixa EXATA (nunca os dados)
    {
        const u64 tableLen =
            meta_.blockCount * static_cast<u64>(kGmeshV3BlockEntryBytes);
        if (tableLen > kMaterialsRangeMax * 64ull) {
            // defesa: blocos demais para a faixa declarada (920 blocos =
            // 73,6 KB; o teto absurdo trava lixo antes de alocar)
            err = "tabela de blocos absurda (" +
                  std::to_string(meta_.blockCount) + " entradas)";
            elog::error("blocos: '%s' FALHOU — %s", relPath.c_str(),
                        err.c_str());
            if (map != nullptr) {
                fileapi::unmapFile64(map, mapLen);
            }
            if (fd >= 0) {
                ::close(fd);
            }
            return false;
        }
        // o tamanho VISTO contra a faixa pedida: TRUNCADO — corrupção
        // CONFIRMADA (o peek validou o v3; o fim do ficheiro não acompanha)
        if (fileBytesKnown &&
            meta_.blockTableOffset + tableLen > fileBytes) {
            quarentena("a tabela de blocos acaba além do fim (faixa [" +
                       std::to_string(meta_.blockTableOffset) + ", " +
                       std::to_string(meta_.blockTableOffset + tableLen) +
                       ") > " + std::to_string(fileBytes) +
                       " B — ficheiro truncado)");
            if (map != nullptr) {
                fileapi::unmapFile64(map, mapLen);
            }
            if (fd >= 0) {
                ::close(fd);
            }
            return false;
        }
        std::vector<u8> tableBytes;
        if (!readRange("faixa da TABELA", meta_.blockTableOffset,
                       static_cast<size_t>(tableLen), tableBytes)) {
            elog::error("blocos: '%s' FALHOU na TABELA — %s",
                        relPath.c_str(), err.c_str());
            if (map != nullptr) {
                fileapi::unmapFile64(map, mapLen);
            }
            if (fd >= 0) {
                ::close(fd);
            }
            return false;
        }
        if (!gmeshV3ReadTableOnly(tableBytes.data(), tableBytes.size(),
                                  meta_, blocks_, err)) {
            quarentena("a validação da TABELA falhou (" + err + ")");
            if (map != nullptr) {
                fileapi::unmapFile64(map, mapLen);
            }
            if (fd >= 0) {
                ::close(fd);
            }
            return false;
        }
    }

    // (3) os MATERIAIS pela faixa deles: com o tamanho visto é a cauda
    //     EXATA; sem tamanho (SAF puro), as sondas de sempre
    {
        std::vector<u8> matBytes;
        bool got = false;
        if (fileBytesKnown) {
            if (fileBytes >= meta_.materialTableOffset) {
                const u64 tail = fileBytes - meta_.materialTableOffset;
                if (tail > kMaterialsRangeMax) {
                    err = "tabela de materiais absurda (" +
                          std::to_string(tail) + " B — o .gmesh v3 da casa "
                          "escreve nomes)";
                    elog::error("blocos: '%s' FALHOU — %s", relPath.c_str(),
                                err.c_str());
                    if (map != nullptr) {
                        fileapi::unmapFile64(map, mapLen);
                    }
                    if (fd >= 0) {
                        ::close(fd);
                    }
                    return false;
                }
                got = readRange("faixa dos MATERIAIS",
                                meta_.materialTableOffset,
                                static_cast<size_t>(tail), matBytes);
                if (!got) {
                    elog::error("blocos: '%s' FALHOU nos MATERIAIS — %s",
                                relPath.c_str(), err.c_str());
                }
            } else {
                // o meta diz que os materiais começam além do fim — o
                // ficheiro foi CORTADO (o peek estava verde)
                quarentena("a tabela de materiais começa além do fim (" +
                           std::to_string(meta_.materialTableOffset) +
                           " B > " + std::to_string(fileBytes) +
                           " B — ficheiro truncado)");
                if (map != nullptr) {
                    fileapi::unmapFile64(map, mapLen);
                }
                if (fd >= 0) {
                    ::close(fd);
                }
                return false;
            }
        } else {
            // SAF (sem stat): 1 MB → 64 KB → 4 KB → 256 B; o reader para
            // em materialCount nomes — bytes a mais no fim são inofensivos
            static const size_t kProbes[] = {1024u * 1024u, 64u * 1024u,
                                             4u * 1024u, 256u};
            for (size_t probe : kProbes) {
                if (st.readBytesAt(relPath, meta_.materialTableOffset,
                                   probe, matBytes)) {
                    got = true;
                    break;
                }
            }
            if (!got) {
                // 0.10.6 (SAF-SEAM) — A COSTURA QUE FALTAVA: um provider SEM
                // tamanho (o fd veio PIPE/stream, o stat não existe sob
                // content://) e uma cauda de materiais MENOR que a sonda
                // mínima (256 B) MATAVA a abertura — as sondas exigem o
                // comprimento EXATO e a cauda é quase sempre curta
                // («cidade» = 8 B). A tabela é AUTO-DESCRITIVA: nome a
                // nome (u16 comprimento + nome + pad a 4 do offset
                // ABSOLUTO — o MESMO alinhamento do escritor). A leitura
                // incremental monta os bytes EXATOS sem saber o tamanho;
                // uma leitura que falha a meio deixa o READER decidir
                // (truncado → QUARENTENA com a causa certa, não «sondas»)
                u64 moff = meta_.materialTableOffset;
                for (u64 i = 0; i < meta_.materialCount; ++i) {
                    std::vector<u8> two;
                    if (!st.readBytesAt(relPath, moff, 2, two)) {
                        break;
                    }
                    const u16 nlen = static_cast<u16>(two[0]) |
                                     static_cast<u16>(
                                         static_cast<u16>(two[1]) << 8);
                    matBytes.insert(matBytes.end(), two.begin(), two.end());
                    moff += 2;
                    if (nlen > 0) {
                        std::vector<u8> nm;
                        if (!st.readBytesAt(relPath, moff, nlen, nm)) {
                            break;
                        }
                        matBytes.insert(matBytes.end(), nm.begin(), nm.end());
                        moff += nlen;
                    }
                    const u64 pad = (4 - moff % 4) % 4;
                    if (pad > 0) {
                        std::vector<u8> pz;
                        if (!st.readBytesAt(relPath, moff,
                                            static_cast<size_t>(pad), pz)) {
                            // o pad FINAL pode cair além do fim de um
                            // ficheiro truncado — o READER diz «tabela de
                            // materiais truncada» (a QUARENTENA certa)
                            break;
                        }
                        matBytes.insert(matBytes.end(), pz.begin(), pz.end());
                        moff += pad;
                    }
                    if (i + 1 == meta_.materialCount) {
                        got = true;   // os materialCount nomes montados
                    }
                }
                if (got) {
                    elog::info("blocos: materiais de '%s' lidos NOME A NOME "
                               "(%u nome(s) — sem tamanho do ficheiro sob "
                               "este provider, a sonda mínima não caberia)",
                               relPath.c_str(),
                               static_cast<unsigned>(meta_.materialCount));
                } else if (!matBytes.empty()) {
                    got = true;   // PARCIAL: o reader valida (ou quarentena)
                    elog::warn("blocos: '%s' — materiais INCOMPLETOS na "
                               "leitura nome a nome (o reader decide)",
                               relPath.c_str());
                }
            }
            if (!got) {
                err = relPath + ": a leitura da faixa dos MATERIAIS falhou "
                      "pelas sondas (sem stat sob este storage — a causa do "
                      "storage está nas linhas acima)";
                elog::error("blocos: '%s' FALHOU nos MATERIAIS — %s",
                            relPath.c_str(), err.c_str());
            }
        }
        if (got && !gmeshV3ReadMaterialsOnly(matBytes.data(),
                                              matBytes.size(), meta_,
                                              materials_, err)) {
            quarentena("a validação dos MATERIAIS falhou (" + err + ")");
            got = false;
        }
        if (!got) {
            if (map != nullptr) {
                fileapi::unmapFile64(map, mapLen);
            }
            if (fd >= 0) {
                ::close(fd);
            }
            return false;
        }
    }

    if (map != nullptr) {
        fileapi::unmapFile64(map, mapLen);
    }
    if (fd >= 0) {
        ::close(fd);
    }
    storage_ = &st;
    path_ = relPath;
    entries_.resize(blocks_.size());   // Entry move-only: resize constrói
    opened_ = true;

    // a linha contrato do LOAD (o par das fases do conversor): a abertura
    // É o load do runtime por blocos — a tabela, nunca os dados. A FONTE
    // da abertura diz-se ao lado (o par do «asset: v3 fonte=…» do
    // conversor: mmap-fd = o fd do bridge mapeado; pread-fd = o provider
    // recusou o mapa e as faixas saíram NO MESMO fd; ranges = readBytesAt)
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - t0)
                          .count();
    const u64 dataBytes = [&]() {
        u64 n = 0;
        for (const GMeshV3Block& b : blocks_) {
            n += b.dataSize;
        }
        return n;
    }();
    elog::info(
        "gmesh: fase=load ms=%.1f (por BLOCOS: tabela de %u blocos, %zu B "
        "lidos, fonte=%s; dados=%llu B por carregar em lazy)",
        ms, static_cast<unsigned>(blocks_.size()),
        kGHeaderBytes + kGmeshV3MetaBytes +
            blocks_.size() * kGmeshV3BlockEntryBytes,
        fonte, static_cast<unsigned long long>(dataBytes));
    return true;
}

bool BlockMesh::createHull(Mesh& out) const {
    if (!opened_) {
        return false;
    }
    // 8 cantos do AABB global; 12 triângulos (36 índices) — um Mesh VÁLIDO
    // cujos bounds são EXATAMENTE o AABB do meta (o create calcula-os dos
    // vértices — os cantos são o próprio AABB)
    Vertex v[8];
    int vi = 0;
    for (int c = 0; c < 8; ++c) {
        v[vi].pos = Vec3{c & 1 ? meta_.aabbMax.x : meta_.aabbMin.x,
                         c & 2 ? meta_.aabbMax.y : meta_.aabbMin.y,
                         c & 4 ? meta_.aabbMax.z : meta_.aabbMin.z};
        v[vi].normal = Vec3{0.0f, 1.0f, 0.0f};
        v[vi].uv = Vec2{0.0f, 0.0f};
        ++vi;
    }
    static const u16 kHullIdx[36] = {
        0, 1, 3, 0, 3, 2,   // -Z
        5, 4, 6, 5, 6, 7,   // +Z
        4, 0, 2, 4, 2, 6,   // -X
        1, 5, 7, 1, 7, 3,   // +X
        2, 3, 7, 2, 7, 6,   // +Y
        4, 5, 1, 4, 1, 0    // -Y
    };
    return out.create(v, 8, kHullIdx, 36);
}

// ---- frustum (Gribb-Hart sobre as colunas do VP; coluna-major) -----------
// plano = linha3 ± linha_i do clip; fora = a·x+b·y+c·z+d < 0
void BlockMesh::frustumPlanes(const Mat4& vp, f32 out[6][4]) {
    const f32* m = vp.m;
    // linhas da matriz (coluna-major): linha r = (m[r], m[4+r], m[8+r], m[12+r])
    for (int p = 0; p < 6; ++p) {
        const int i = p >> 1;             // 0..2
        const f32 sign = (p & 1) ? -1.0f : 1.0f;   // par: +, ímpar: -
        f32 a = m[3] + sign * m[i];
        f32 b = m[7] + sign * m[4 + i];
        f32 c = m[11] + sign * m[8 + i];
        f32 d = m[15] + sign * m[12 + i];
        const f32 len = std::sqrt(a * a + b * b + c * c);
        if (len > 1e-12f) {
            const f32 inv = 1.0f / len;
            a *= inv;
            b *= inv;
            c *= inv;
            d *= inv;
        }
        out[p][0] = a;
        out[p][1] = b;
        out[p][2] = c;
        out[p][3] = d;
    }
}

bool BlockMesh::blockVisible(const Mat4& model, const GMeshV3Block& blk,
                             const f32 planes[6][4]) {
    // o AABB do bloco (8 cantos) transformado pelo model — testado DIRETO
    // contra cada plano: conservative-exact para OBB (rodado incluído)
    Vec3 c[8];
    for (int k = 0; k < 8; ++k) {
        const Vec3 local{k & 1 ? blk.aabbMax.x : blk.aabbMin.x,
                         k & 2 ? blk.aabbMax.y : blk.aabbMin.y,
                         k & 4 ? blk.aabbMax.z : blk.aabbMin.z};
        // model * local (coluna-major)
        const f32* m = model.m;
        c[k] = Vec3{m[0] * local.x + m[4] * local.y + m[8] * local.z + m[12],
                    m[1] * local.x + m[5] * local.y + m[9] * local.z + m[13],
                    m[2] * local.x + m[6] * local.y + m[10] * local.z +
                        m[14]};
    }
    for (int p = 0; p < 6; ++p) {
        const f32 a = planes[p][0], b = planes[p][1], c2 = planes[p][2],
                  d = planes[p][3];
        int outside = 0;
        for (int k = 0; k < 8; ++k) {
            if (a * c[k].x + b * c[k].y + c2 * c[k].z + d < 0.0f) {
                ++outside;
            }
        }
        if (outside == 8) {
            return false;   // o bloco INTEIRO está fora deste plano
        }
    }
    return true;
}

u32 BlockMesh::visibleBlocks(const Mat4& model, const Mat4& vp,
                             std::vector<u32>& out) const {
    out.clear();
    if (!opened_) {
        return 0;
    }
    f32 planes[6][4];
    frustumPlanes(vp, planes);
    for (size_t i = 0; i < blocks_.size(); ++i) {
        if (blockVisible(model, blocks_[i], planes)) {
            out.push_back(static_cast<u32>(i));
        }
    }
    return static_cast<u32>(out.size());
}

// ---- lazy load -------------------------------------------------------------

bool BlockMesh::materializeBlock(size_t index, MeshData& out,
                                  std::string& err) const {
    out = MeshData{};
    if (!opened_ || !storage_ || index >= blocks_.size()) {
        err = "bloco #" + std::to_string(index) + " fora da tabela";
        return false;
    }
    const GMeshV3Block& blk = blocks_[index];
    std::vector<u8> range;
    const auto t0 = std::chrono::steady_clock::now();
    if (!storage_->readBytesAt(path_, blk.dataOffset,
                               static_cast<size_t>(blk.dataSize), range)) {
        err = "faixa do bloco #" + std::to_string(index) +
              " ilegível (offset " + std::to_string(blk.dataOffset) + ", " +
              std::to_string(blk.dataSize) + " B)";
        return false;
    }
    const bool okDec = gmeshV3MaterializeBlock(range.data(), range.size(),
                                               meta_, blk, out, err);
    const double ms = std::chrono::duration<double, std::milli>(
                          std::chrono::steady_clock::now() - t0)
                          .count();
    elog::info("gmesh: fase=load ms=%.1f (bloco %zu/%zu, %llu B, %u verts)",
               ms, index, blocks_.size(),
               static_cast<unsigned long long>(blk.dataSize),
               static_cast<unsigned>(blk.vertexCount));
    if (!okDec) {
        return false;
    }
    // o nome do grupo/material do bloco (o contrato do leitor inteiro)
    if (blk.materialIndex < materials_.size()) {
        if (!out.groups.empty()) {
            out.groups[0].name = "bloco " + std::to_string(index);
            out.groups[0].material = materials_[static_cast<size_t>(
                blk.materialIndex)];
        }
        out.name = path_ + "#bloco" + std::to_string(index);
    }
    return true;
}

bool BlockMesh::ensureBlock(size_t index, std::string& err) {
    Entry& e = entries_[index];
    const GMeshV3Block& blk = blocks_[index];
    if (e.gpu && e.gpu->ok()) {
        return true;   // residente em VRAM — o caminho quente (a cache
                       // CPU pode ter sido evictada: só pesa no RE-upload)
    }
    if (uploadDead_) {
        return false;   // anti retry-storm: o upload morreu 3× seguidas —
                        // um open novo (close+open) limpa a flag
    }
    // a faixa do disco (se o CPU cache foi evictado, volta ao disco)
    if (!e.cpu) {
        std::shared_ptr<MeshData> data = std::make_shared<MeshData>();
        if (!materializeBlock(index, *data, err)) {
            return false;
        }
        e.cpu = std::move(data);
        e.ramBytes = blockMeshVramBytes(*e.cpu);   // MESMA estimativa (a
                                                   // cache CPU pesa o MeshData)
    }
    // o upload (o par da fase=render do mesh único)
    const auto t0 = std::chrono::steady_clock::now();
    std::unique_ptr<Mesh> gpu = std::make_unique<Mesh>();
    const MeshData& md = *e.cpu;
    bool up;
    if (md.skinned()) {
        up = gpu->createSkinned(md.vertices.data(),
                                static_cast<u32>(md.vertices.size()),
                                md.indices.data(),
                                static_cast<u32>(md.indices.size()),
                                md.skinJoints.data(), md.skinWeights.data());
    } else {
        up = gpu->create(md.vertices.data(),
                         static_cast<u32>(md.vertices.size()),
                         md.indices.data(),
                         static_cast<u32>(md.indices.size()));
    }
    if (!up) {
        err = "upload do bloco #" + std::to_string(index) + " falhou (GL)";
        if (++uploadFails_ >= 3) {
            uploadDead_ = true;
            elog::error("blocos: '%s' — upload GL falhou %u× sem TERM; "
                        "os carregamentos param ate o contexto renascer",
                        path_.c_str(), uploadFails_);
        }
        return false;
    }
    uploadFails_ = 0;
    elog::info("gmesh: fase=render ms=%.1f (bloco %zu/%zu de '%s', %u verts)",
               std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - t0)
                   .count(),
               index, blocks_.size(), path_.c_str(),
               static_cast<unsigned>(blk.vertexCount));
    e.gpu = std::move(gpu);
    e.vramBytes = e.ramBytes;
    return true;
}

// ---- LRU -------------------------------------------------------------------

void BlockMesh::evictOverBudget() {
    // a contabilidade VIVA (o pin afere contra ISTO — nada declarado a
    // mais: o que passa do orçamento sai AGORA, neste frame)
    u64 ram = 0, vram = 0;
    for (const Entry& e : entries_) {
        if (e.cpu) {
            ram += e.ramBytes;
        }
        if (e.gpu) {
            vram += e.vramBytes;
        }
    }
    // ordem LRU: os menos-vistos primeiro (desempate pelo índice — estável)
    std::vector<size_t> order;
    order.reserve(entries_.size());
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].gpu) {
            order.push_back(i);
        }
    }
    std::sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        return entries_[a].lastSeen != entries_[b].lastSeen
                   ? entries_[a].lastSeen < entries_[b].lastSeen
                   : a < b;
    });

    // VRAM primeiro (o pool caro): destroy do Mesh mantém o CPU cache
    for (size_t i : order) {
        if (vram <= vramBudget_) {
            break;
        }
        Entry& e = entries_[i];
        if (e.gpu) {
            vram -= e.vramBytes;
            e.gpu->destroy();
            e.gpu.reset();
            e.vramBytes = 0;
            ++evictedTotal_;
        }
    }
    // RAM depois — TODAS as cópias CPU contam (com OU sem GPU vivo: um
    // bloco vivo em VRAM sem a cache CPU paga o disco no RE-upload, mas o
    // ORÇAMENTO é o contrato e a contabilidade soma ambas as pool)
    std::vector<size_t> ramOrder;
    ramOrder.reserve(entries_.size());
    for (size_t i = 0; i < entries_.size(); ++i) {
        if (entries_[i].cpu) {
            ramOrder.push_back(i);
        }
    }
    std::sort(ramOrder.begin(), ramOrder.end(), [&](size_t a, size_t b) {
        return entries_[a].lastSeen != entries_[b].lastSeen
                   ? entries_[a].lastSeen < entries_[b].lastSeen
                   : a < b;
    });
    for (size_t i : ramOrder) {
        if (ram <= ramBudget_) {
            break;
        }
        Entry& e = entries_[i];
        ram -= e.ramBytes;
        e.cpu.reset();
        e.ramBytes = 0;
        ++evictedTotal_;   // RAM também conta (as duas pool evictam)
    }

    // o aviso de churn: working set VISÍVEL maior que o orçamento — o
    // mesmo bloco entra/sai por frame (1× por open, com os números)
    if (!churnWarned_) {
        u64 visBytes = 0;
        for (const Entry& e : entries_) {
            if (e.gpu && e.lastSeen == frame_) {
                visBytes += e.vramBytes;
            }
        }
        if (visBytes > vramBudget_) {
            churnWarned_ = true;
            elog::warn(
                "blocos: o conjunto visivel (%.1f MB) excede o orcamento "
                "VRAM (%.1f MB) — havera troca por frame (abaixe a "
                "detalhe ou aumente o orcamento)",
                static_cast<double>(visBytes) / (1024.0 * 1024.0),
                static_cast<double>(vramBudget_) / (1024.0 * 1024.0));
        }
    }
}

void BlockMesh::setBudgets(u64 ramBudgetBytes, u64 vramBudgetBytes) {
    ramBudget_ = ramBudgetBytes;
    vramBudget_ = vramBudgetBytes;
}

// ---- o frame ----------------------------------------------------------------

DrawStats BlockMesh::draw(Renderer& r, const Mat4& model, const Mat4& vp,
                          const Texture* tex, const f32* tint,
                          const Mat4* bones, u32 boneCount, Stats& st) {
    DrawStats total{};
    st = Stats{};
    if (!opened_) {
        return total;
    }
    ++frame_;

    st.totalBlocks = static_cast<u32>(blocks_.size());
    f32 planes[6][4];
    frustumPlanes(vp, planes);

    std::string err;
    for (size_t i = 0; i < blocks_.size(); ++i) {
        if (!blockVisible(model, blocks_[i], planes)) {
            continue;   // CULL: o AABB do bloco está todo fora do frustum
        }
        ++st.visible;
        Entry& e = entries_[i];
        const bool wasResident = e.gpu != nullptr;
        if (!ensureBlock(i, err)) {
            // honesto: o bloco falhou (corrupção/faixa) — loga e segue;
            // os OUTROS blocos não morrem por causa dele
            elog::error("blocos: '%s' bloco %zu FALHOU — %s", path_.c_str(),
                        i, err.c_str());
            err.clear();
            continue;
        }
        e.lastSeen = frame_;
        st.drawn++;
        if (!wasResident) {
            ++st.loadedThisFrame;
        }
        total = total + r.drawMesh(*e.gpu, model, vp, tex, tint, bones,
                                   boneCount);
    }

    evictOverBudget();

    // a contabilidade de saída (o que o HUD/log espelham — pós-eviction)
    for (const Entry& e : entries_) {
        if (e.gpu) {
            ++st.resident;
            st.vramBytes += e.vramBytes;
        }
        if (e.cpu) {
            st.ramBytes += e.ramBytes;
        }
    }
    st.evictedTotal = static_cast<u32>(evictedTotal_);
    last_ = st;
    return total;
}

// (o uploadDead limpa no close() — um open novo merece novas tentativas)

} // namespace vv
