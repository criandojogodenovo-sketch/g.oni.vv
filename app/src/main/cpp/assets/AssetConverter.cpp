// assets/AssetConverter.cpp — import streaming + conversão (0.8.10).
//
// VER AssetConverter.h para o desenho. Implementação:
//   • copyFileChunked (FileApi) para a fonte → source/;
//   • OBJ: ChunkReader + assembler de linhas → ObjStreamParser;
//   • GLB: leitura do header/chunks por fread DIRETO (o ficheiro fica
//     aberto; o JSON lê inteiro ≤ kMaxJsonBytes; o BIN fica DEFERIDO no
//     GltfRangeLoader — cada accessor/imagem faz fseek+fread do SEU range);
//   • glTF: JSON inteiro ≤ kMaxJsonBytes; .bin irmão pelo mesmo loader;
//   • PNG: readAll com guarda kMaxImageBytes → pipeline → .gtext;
//   • clips: dummy Scene+TIC + gltfAttachClips (O MESMO código do runtime);
//     esqueleto: gltfAttachSkin no mesmo dummy → joints copiados p/ o .gm.
#include "assets/AssetConverter.h"

#include <cstdio>
#include <unistd.h>
#include <cstring>

#include "assets/GltfAnim.h"
#include "assets/GltfImporter.h"
#include "assets/ObjImporter.h"
#include "assets/PngLoader.h"
#include "assets/TexturePipeline.h"
#include "components/AnimationPlayer.h"
#include "components/SkeletonComp.h"
#include "core/Json.h"
#include "core/Scene.h"
#include "platform/EngineLog.h"
#include "platform/FileApi.h"
#include "platform/StorageBridge.h"   // 0.8.12: jniCacheDir (staging SAF sem /tmp)

namespace vv {
namespace convert {

std::string sanitizeName(const std::string& in) {
    std::string s = in;
    for (char& ch : s) {
        if (ch == '/' || ch == '\\' || ch == ':') {
            ch = '_';
        }
    }
    return s;
}

std::string stemOf(const std::string& name) {
    const size_t dot = name.rfind('.');
    if (dot == std::string::npos || dot == 0) {
        return name;
    }
    return name.substr(0, dot);
}

// ---- 0.9.6.4 (GRUPO A/R-021) · OS IRMÃOS DO .gltf ---------------------------
// Um .gltf «separado» referencia os .bin e as texturas por URI RELATIVA
// («scene.bin», «textures/albedo.png»). A falha histórica do device
// («buffer externo não resolvido: scene.bin»): o resolver lia o URI contra
// o CWD do processo (que no Android é «/») — os testes passavam porque
// escreviam o .bin no CWD do CI. O caminho novo: os irmãos são LIDOS do
// DIRETÓRIO ORIGINAL (o browser dá caminhos POSIX reais do All Files
// Access) e COPIADOS para source/ (subcaminho a subcaminho) — o projeto
// fica autossuficiente e o reconvert volta a funcionar sem a pasta original.

namespace {

// URI-decode («tex%20albedo.png» → «tex albedo.png»); %XX inválido fica
// literal (o glTF do mundo real traz %20 nos nomes com espaço)
std::string uriDecode(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        if (in[i] == '%' && i + 2 < in.size()) {
            auto hex = [](char c) -> int {
                if (c >= '0' && c <= '9') return c - '0';
                if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                return -1;
            };
            const int hi = hex(in[i + 1]);
            const int lo = hex(in[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out.push_back(static_cast<char>((hi << 4) | lo));
                i += 2;
                continue;
            }
        }
        out.push_back(in[i]);
    }
    return out;
}

// o URI é EXTERNO (não data:)? e é um candidato a IRMÃO local?
// http://, file://, content:// não têm irmão local — o ERRO nomeia o URI.
bool uriHasScheme(const std::string& u) {
    const size_t colon = u.find(':');
    if (colon == std::string::npos || colon == 0) {
        return false;
    }
    for (size_t i = 0; i < colon; ++i) {
        const char c = u[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                        (c >= '0' && c <= '9') || c == '+' || c == '-' ||
                        c == '.';
        if (!ok) {
            return false;   // «C:\x» ou «a b:c» — não é esquema
        }
    }
    return true;
}

// caminho do irmão no DIRETÓRIO ORIGINAL (subcaminhos relativos mantêm a
// estrutura; «..» é recusado — zip-slip de irmãos não entra no projeto)
bool siblingPathOf(const std::string& dir, const std::string& uri,
                   std::string& outPath, std::string& outRel,
                   std::string& err) {
    if (uri.empty()) {
        err = "uri vazia";
        return false;
    }
    if (uri.compare(0, 5, "data:") == 0) {
        err = "data: (embutida)";
        return false;   // não é irmão — o chamador filtra antes
    }
    if (uriHasScheme(uri)) {
        err = "o .gltf referencia '" + uri + "', que está FORA da pasta "
              "(endereço absoluto) — exporta o modelo como GLB (tudo "
              "embutido) ou põe o ficheiro ao lado do .gltf";
        return false;
    }
    if (uri.find("..") != std::string::npos) {
        err = "o .gltf referencia '" + uri + "' com «..» — caminho fora da "
              "pasta não é copiado (segurança); exporta como GLB";
        return false;
    }
    const std::string dec = uriDecode(uri);
    if (dec.empty() || dec.front() == '/' || dec.back() == '/') {
        err = "uri de irmão inválida: '" + uri + "'";
        return false;
    }
    outRel = dec;
    const std::string sep = (!dir.empty() && dir.back() == '/') ? "" : "/";
    outPath = dir + sep + dec;
    return true;
}

} // namespace

// COLETA os URIs externos de um .gltf (buffers[].uri + images[].uri, sem
// data:). false + err se o JSON não parseia. público p/ a sentinela R-021.
bool collectGltfSiblingUris(const char* json, size_t jsonLen,
                            std::vector<std::string>& uris, std::string& err) {
    uris.clear();
    Json doc;
    if (!Json::parse(json, jsonLen, doc) || doc.type != Json::Type::Object) {
        err = "glTF: JSON inválido";
        return false;
    }
    auto pushUri = [&uris](const Json* uri) {
        if (!uri || uri->type != Json::Type::String ||
            uri->string.empty()) {
            return;
        }
        if (uri->string.compare(0, 5, "data:") == 0) {
            return;   // embutida — não é irmão
        }
        for (const std::string& have : uris) {
            if (have == uri->string) {
                return;   // dedup (mesma textura em N materiais)
            }
        }
        uris.push_back(uri->string);
    };
    if (const Json* jb = doc.find("buffers");
        jb && jb->type == Json::Type::Array) {
        for (const Json& b : jb->items) {
            pushUri(b.find("uri"));
        }
    }
    if (const Json* ji = doc.find("images");
        ji && ji->type == Json::Type::Array) {
        for (const Json& im : ji->items) {
            pushUri(im.find("uri"));
        }
    }
    return true;
}

// COPIA os irmãos do DIRETÓRIO ORIGINAL para source/<subcaminho> (streaming
// pelo storage — funciona em FS e SAF); um irmão por LOG; irmão AUSENTE no
// original = ERRO QUE NOMEIA O FICHEIRO (a spec A1). `srcDir` = a pasta do
// ficheiro .gltf original (caminho POSIX real do browser).
// [definido APÓS copyToStorage — vê o bloco seguinte]



namespace {

std::string lowerExtOfName(const std::string& name) {
    const size_t dot = name.rfind('.');
    if (dot == std::string::npos) {
        return "";
    }
    std::string e = name.substr(dot + 1);
    for (char& c : e) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return e;
}

// cópia STREAMING da fonte (POSIX/FileApi) → rel no STORAGE, por chunks
// com progresso/cancelamento (nunca o ficheiro inteiro em RAM; o storage
// escreve com o SEU write stream — FsStorage FILE*, SafStorage fd, default
// acumulado com teto). false + err legível em falha; stats.canceled em cancel.
bool copyToStorage(const std::string& srcAbs, ProjectStorage& st,
                   const std::string& rel, Stats& stats, std::string& err,
                   bool (*onProgress)(void*, u64, u64), void* user) {
    fileapi::ChunkReader rd;
    if (!rd.open(srcAbs.c_str(), kChunkBytes)) {
        err = "fonte ilegivel: " + srcAbs + " (" + fileapi::errnoText() + ")";
        return false;
    }
    st.makeDirs(rel.substr(0, rel.rfind('/')));
    const int h = st.openWriteStream(rel);
    if (h <= 0) {
        err = "storage recusou a escrita de: " + rel;
        return false;
    }
    bool ok = true;
    while (rd.next()) {
        if (rd.last > 0 && !st.writeStreamChunk(h, rd.buf.data(), rd.last)) {
            err = "escrita falhou a meio de: " + rel + " (quota/teto?)";
            ok = false;
            break;
        }
        if (onProgress && !onProgress(user, rd.done, rd.total)) {
            stats.canceled = true;
            err = "cancelado";
            ok = false;
            break;
        }
    }
    st.closeWriteStream(h);
    if (ok && rd.done < rd.total) {
        err = "leitura curta da fonte: " + srcAbs;
        ok = false;
    }
    if (!ok) {
        // SEM ESTADO PARCIAL: o ficheiro meio-escrito sai do projeto
        // (a próxima tentativa começa limpa — o requisito do prompt)
        if (st.remove(rel)) {
            elog::info("import: copia incompleta '%s' removida (sem estado "
                       "parcial)", rel.c_str());
        }
    }
    return ok;
}

// escreve bytes no storage + soma ao orçamento de saída
bool writeAsset(ProjectStorage& st, const std::string& rel,
                const std::vector<u8>& bytes, u64& outBytes,
                std::string& err) {
    if (!st.writeBytes(rel, bytes.data(), bytes.size())) {
        err = "gravacao falhou: " + rel;
        return false;
    }
    outBytes += bytes.size();
    return true;
}

// ---- OBJ streaming: ChunkReader → linhas → parser ---------------------------
bool convertObjStream(const std::string& srcAbs, ProjectStorage& st,
                      const std::string& stem, Output& out, Stats& stats,
                      std::string& err,
                      bool (*onProgress)(void*, u64, u64), void* user) {
    fileapi::ChunkReader rd;
    if (!rd.open(srcAbs.c_str(), kChunkBytes)) {
        err = "fonte ilegivel: " + srcAbs + " (" + fileapi::errnoText() + ")";
        return false;
    }
    ObjStreamParser parser;
    std::string line;   // assembler (linhas podem cruzar chunks)
    u64 reported = 0;
    while (rd.next()) {
        const u8* p = rd.buf.data();
        size_t n = rd.last;
        size_t start = 0;
        for (size_t i = 0; i < n; ++i) {
            if (p[i] == '\n') {
                line.append(reinterpret_cast<const char*>(p + start), i - start);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                if (!parser.feedLine(line.data(), line.size(), err)) {
                    err += " [obj streaming]";
                    return false;
                }
                line.clear();
                start = i + 1;
            }
        }
        line.append(reinterpret_cast<const char*>(p + start), n - start);
        if (onProgress && rd.done != reported) {
            reported = rd.done;
            if (!onProgress(user, rd.done, rd.total)) {
                stats.canceled = true;
                err = "cancelado";
                return false;
            }
        }
    }
    if (!line.empty()) {   // última linha sem '\n'
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!parser.feedLine(line.data(), line.size(), err)) {
            err += " [obj streaming]";
            return false;
        }
    }
    MeshData mesh;
    if (!parser.finish(mesh, err)) {
        return false;
    }
    stats.verts = static_cast<u32>(mesh.vertices.size());
    stats.indices = static_cast<u32>(mesh.indices.size());
    std::vector<u8> bytes;
    if (!writeGMesh(mesh, bytes, err)) {
        return false;
    }
    const std::string rel = std::string("assets/") + stem + ".gmesh";
    if (!writeAsset(st, rel, bytes, stats.outputBytes, err)) {
        return false;
    }
    out.meshes.push_back(rel);
    stats.meshes = 1;
    elog::info("asset: %s verts=%u idx=%u (streaming %llu B em %llu linhas)",
               rel.c_str(), stats.verts, stats.indices,
               static_cast<unsigned long long>(rd.total),
               static_cast<unsigned long long>(parser.lines()));
    return true;
}

// ---- range loader sobre uma FILE* (GLB em ficheiro / .bin irmão) -----------
struct FileRangeCtx {
    FILE* f = nullptr;
    u64 base = 0;      // offset do INÍCIO do buffer no ficheiro
    const char* debugName = "";
};
bool fileRangeLoad(void* user, u32 bufferIndex, u64 offset, u64 len,
                   std::vector<u8>& outv) {
    (void)bufferIndex;   // o loader é construído por buffer (base já certa)
    FileRangeCtx* ctx = static_cast<FileRangeCtx*>(user);
    if (!ctx || !ctx->f || len > kMaxJsonBytes) {
        return false;
    }
    outv.resize(static_cast<size_t>(len));
    if (std::fseek(ctx->f, static_cast<long>(ctx->base + offset), SEEK_SET) != 0) {
        return false;
    }
    return std::fread(outv.data(), 1, outv.size(), ctx->f) == outv.size();
}

// lê um range EXATO de uma FILE* (header/JSON)
bool readExact(FILE* f, u64 off, size_t len, std::vector<u8>& outv) {
    outv.resize(len);
    if (std::fseek(f, static_cast<long>(off), SEEK_SET) != 0) {
        return false;
    }
    return len == 0 || std::fread(outv.data(), 1, len, f) == len;
}

} // namespace

// ---- 0.9.6.4 (GRUPO A) — irmãos + integridade (definições públicas) --------

bool copyGltfSiblings(const char* json, size_t jsonLen,
                      const std::string& srcDir, ProjectStorage& st,
                      Stats& stats, std::string& err,
                      bool (*onProgress)(void*, u64, u64), void* user) {
    std::vector<std::string> uris;
    if (!collectGltfSiblingUris(json, jsonLen, uris, err)) {
        return false;
    }

    for (const std::string& uri : uris) {
        std::string path;
        std::string rel;
        std::string serr;
        if (!siblingPathOf(srcDir, uri, path, rel, serr)) {
            if (serr == "data: (embutida)") {
                continue;   // filtrado (não devia chegar — defesa)
            }
            err = serr;
            return false;
        }
        if (!fileapi::isFile(path)) {
            err = "o .gltf referencia '" + uri + "' que NÃO EXISTE ao lado "
                  "do ficheiro (" + srcDir + ") — copia o ficheiro em falta "
                  "para essa pasta ou exporta o modelo como GLB (tudo "
                  "embutido)";
            elog::error("import: irmao '%s' AUSENTE em '%s'", uri.c_str(),
                        srcDir.c_str());
            return false;
        }
        const std::string dstRel = std::string("source/") + rel;
        // subpastas do subcaminho («textures/x.png» → source/textures/)
        const size_t slash = dstRel.rfind('/');
        if (slash != std::string::npos) {
            st.makeDirs(dstRel.substr(0, slash));
        }
        // 0.9.6.4: o irmão que JÁ VIVE em source/ (reconvert) não volta a
        // ser copiado sobre si mesmo — a cópia auto-referencial corrompia
        const std::string sibSrcReal = fileapi::realPath(path);
        const std::string sibDstReal =
            st.root().rfind("content://", 0) == 0
                ? std::string()
                : fileapi::realPath(joinRelPath(st.root(), dstRel));
        if (!sibSrcReal.empty() && !sibDstReal.empty() &&
            sibSrcReal == sibDstReal) {
            elog::info("import: irmao '%s' JA vive em %s — copia saltada",
                       uri.c_str(), dstRel.c_str());
            ++stats.siblings;
            continue;
        }
        if (!copyToStorage(path, st, dstRel, stats, err, onProgress, user)) {
            err = "irmao '" + uri + "': " + err;
            return false;
        }
        u64 sz = 0;
        fileapi::fileSize(path.c_str(), sz);
        elog::info("import: irmao '%s' copiado (%llu B) -> %s (do diretorio "
                   "original)", uri.c_str(),
                   static_cast<unsigned long long>(sz), dstRel.c_str());
        ++stats.siblings;
    }
    if (uris.empty()) {
        elog::info("import: o .gltf nao referencia irmaos externos (tudo "
                   "embutido)");
    }
    return true;
}

// R-022 · A2 — INTEGRIDADE DA CÓPIA EM CHUNKS: a cópia do GLB para source/
// é verificada contra a FONTE byte a byte (a defesa contra a cópia
// truncada que no device se disfarçava de «bufferView da imagem fora do
// buffer»); FsStorage compara por chunks (qualquer tamanho), SAF lê a
// cópia com guarda de orçamento e além dele DIZ (nunca mente «verificado»).
namespace {
// 0x0F → "0f" (a evidência da posição exata do byte que difere)
std::string byteHex(u8 v) {
    char buf[3];
    std::snprintf(buf, sizeof(buf), "%02x", v);
    return std::string(buf);
}
} // namespace
bool verifyCopyChunked(const std::string& srcAbs, ProjectStorage& st,
                       const std::string& rel, std::string& err) {
    err.clear();
    const std::string& root = st.root();
    const bool contentRoot = root.rfind("content://", 0) == 0;
    u64 srcSize = 0;
    if (!fileapi::fileSize(srcAbs.c_str(), srcSize)) {
        err = "fonte ilegivel: " + srcAbs;
        return false;
    }
    // 0.9.6.4: a comparação por chunks exige um CAMINHO REAL da cópia —
    // raiz content:// (SAF) ou storage de memória (FakeStorage do harness)
    // NÃO têm; nesses casos a cópia é lida pelo storage (com guarda de
    // orçamento — e além dele DIZ, nunca mente «verificado»)
    std::string copyAbs;
    u64 copySize = 0;
    if (!contentRoot) {
        copyAbs = joinRelPath(root, rel);
    }
    const bool realCopy =
        !copyAbs.empty() && fileapi::fileSize(copyAbs.c_str(), copySize);
    if (realCopy) {
        // caminho REAL → comparação por chunks (nunca inteiro em RAM)
        if (copySize != srcSize) {
            err = "cópia truncada: '" + rel + "' tem " +
                  std::to_string(copySize) + " B, a fonte tem " +
                  std::to_string(srcSize) + " B";
            return false;
        }
        fileapi::ChunkReader a;
        fileapi::ChunkReader b;
        if (!a.open(srcAbs.c_str(), kChunkBytes) ||
            !b.open(copyAbs.c_str(), kChunkBytes)) {
            err = "cópia truncada: abertura falhou (" + srcAbs + " / " +
                  copyAbs + ")";
            return false;
        }
        u64 at = 0;
        while (a.next() && b.next()) {
            const size_t na = a.last;
            const size_t nb = b.last;
            if (na != nb) {
                err = "cópia truncada: '" + rel + "' difere da fonte na "
                      "posição " + std::to_string(at) + " B (chunk de " +
                      std::to_string(na) + " B vs " + std::to_string(nb) +
                      " B)";
                return false;
            }
            const u8* pa = a.buf.data();
            const u8* pb = b.buf.data();
            for (size_t i = 0; i < na; ++i) {
                if (pa[i] != pb[i]) {
                    err = "cópia truncada: '" + rel + "' difere da fonte na "
                          "posição " + std::to_string(at + i) +
                          " B (fonte 0x" + byteHex(pa[i]) + ", cópia 0x" +
                          byteHex(pb[i]) + ")";
                    return false;
                }
            }
            at += na;
            if (na == 0) {
                break;   // fim dos dois
            }
        }
        if (a.done != b.done || a.done != srcSize) {
            err = "cópia truncada: '" + rel + "' leitura desigual (fonte " +
                  std::to_string(a.done) + " B, cópia " +
                  std::to_string(b.done) + " B)";
            return false;
        }
        elog::info("import: integridade da cópia OK — %s == %s (%llu B em "
                   "chunks de %zu)", rel.c_str(), srcAbs.c_str(),
                   static_cast<unsigned long long>(srcSize), kChunkBytes);
        return true;
    }
    // SEM caminho real (SAF/FakeStorage): readBytes com GUARDA de
    // orçamento (a cópia é um convertido-fonte típico de <64 MB; um GLB
    // gigante diz HONESTO que não foi verificado)
    if (srcSize > kStreamAccumMax) {
        elog::info("import: integridade da cópia NÃO verificada — %s tem "
                   "%llu B e o storage sem caminho real compara até %zu MB "
                   "(a conversão lê a FONTE; a cópia serve o reconvert)",
                   rel.c_str(), static_cast<unsigned long long>(srcSize),
                   kStreamAccumMax / (1024 * 1024));
        return true;
    }
    std::vector<u8> copy;
    if (!st.readBytes(rel, copy) || copy.size() != srcSize) {
        err = "cópia truncada: '" + rel + "' tem " +
              std::to_string(copy.size()) + " B, a fonte tem " +
              std::to_string(srcSize) + " B (storage sem caminho real)";
        return false;
    }
    fileapi::ChunkReader a;
    if (!a.open(srcAbs.c_str(), kChunkBytes)) {
        err = "fonte ilegivel: " + srcAbs;
        return false;
    }
    u64 at = 0;
    while (a.next() && a.last > 0) {
        if (at + a.last > copy.size()) {
            err = "cópia truncada: '" + rel + "' acaba na posição " +
                  std::to_string(copy.size()) + " B, a fonte continua até " +
                  std::to_string(srcSize) + " B (SAF)";
            return false;
        }
        const u8* pa = a.buf.data();
        const u8* pc = copy.data() + at;
        for (size_t i = 0; i < a.last; ++i) {
            if (pa[i] != pc[i]) {
                err = "cópia truncada: '" + rel + "' difere da fonte na posição "
                      + std::to_string(at + i) + " B (SAF)";
                return false;
            }
        }
        at += a.last;
    }
    if (at != srcSize) {
        err = "cópia truncada: leitura curta da fonte (" +
              std::to_string(at) + "/" + std::to_string(srcSize) + " B)";
        return false;
    }
    elog::info("import: integridade da cópia OK (SAF) — %s == fonte (%llu "
               "B)", rel.c_str(), static_cast<unsigned long long>(srcSize));
    return true;
}

namespace {

// ---- PNG → .gtext ------------------------------------------------------------
bool convertPng(const std::string& srcAbs, ProjectStorage& st,
                const std::string& stem, TexturePipeline* pipeline,
                Output& out, Stats& stats, std::string& err) {
    std::vector<u8> bytes;
    if (!fileapi::readAll(srcAbs.c_str(), bytes) || bytes.empty()) {
        err = "leitura falhou: " + srcAbs + " (" + fileapi::errnoText() + ")";
        return false;
    }
    if (bytes.size() > kMaxImageBytes) {
        err = "textura de " + std::to_string(bytes.size() / (1024 * 1024)) +
              " MB excede o orçamento de " +
              std::to_string(kMaxImageBytes / (1024 * 1024)) +
              " MB (usa uma versão menor)";
        return false;
    }
    stats.sourceBytes = bytes.size();
    CompressedImage comp;
    if (pipeline) {
        TextureLoadInfo info;
        std::string perr;
        if (!pipeline->process(bytes.data(), bytes.size(), srcAbs.c_str(),
                               comp, info, perr)) {
            err = "textura falhou: " + perr;
            return false;
        }
    } else {
        static PassthroughCompressor kPassthrough;
        RawImage img;
        if (!loadPng(bytes.data(), bytes.size(), img, err)) {
            return false;
        }
        std::string cerr2;
        if (!kPassthrough.compress(img, comp, cerr2)) {
            err = "textura falhou: " + cerr2;
            return false;
        }
    }
    std::vector<u8> gtext;
    if (!writeGText(comp, gtext, err)) {
        return false;
    }
    const std::string rel = std::string("assets/") + stem + ".gtext";
    if (!writeAsset(st, rel, gtext, stats.outputBytes, err)) {
        return false;
    }
    out.textures.push_back(rel);
    stats.textures = 1;
    elog::info("asset: %s %ux%u %s (%llu B -> %llu B)",
               rel.c_str(), comp.width, comp.height, formatName(comp.format),
               static_cast<unsigned long long>(bytes.size()),
               static_cast<unsigned long long>(gtext.size()));
    return true;
}

// ---- glTF/GLB → .gmesh(.gtext das imagens) + .gm ----------------------------
// `binFile`: FILE* aberto com o buffer 0 em [binBase, binBase+binLen).
// `resolver`: p/ .gltf com .bin EXTERNO (resolve do diretório da fonte).
// `siblingImageDir` (0.9.6.4/R-021): pasta REAL onde vivem as texturas
// EXTERNAS do .gltf (o diretório original no import; source/ no reconvert
// com FS real) — as imagens uri-externas são LIDAS daí e entram no passe
// de texturas como embutidas. null (GLB, ou SAF sem caminho real) = as
// externas ficam no warn honesto de sempre.
bool convertGltfCommon(const char* json, size_t jsonLen, FILE* binFile,
                       u64 binBase, u64 binLen,
                       const GltfBufferResolver& resolver,
                       const char* siblingImageDir,
                       ProjectStorage& st, const std::string& stem,
                       TexturePipeline* pipeline, Output& out, Stats& stats,
                       std::string& err) {
    GltfRangeLoader loader;
    FileRangeCtx ctx;
    ctx.f = binFile;
    ctx.base = binBase;
    ctx.debugName = stem.c_str();
    loader.fn = binFile ? &fileRangeLoad : nullptr;
    loader.user = &ctx;
    loader.binLen = binLen;

    GltfModel model;
    if (!parseGltf(json, jsonLen, {}, resolver, model, err,
                   binFile ? &loader : nullptr)) {
        err = "glTF invalido: " + err;
        return false;
    }
    if (model.meshes.empty()) {
        err = "glTF sem meshes";
        return false;
    }
    // ---- 0.9.6.3 (R-020) · O LOG DO PARSE (a spec PASSO 1 pede: formato,
    // nós, malhas, primitivas, vértices, índices, materiais, texturas) ----
    {
        u32 nPrims = 0;
        for (const MeshData& md : model.meshes) {
            nPrims += static_cast<u32>(md.groups.size());
        }
        u32 nTexOk = 0;
        for (const GltfImage& im : model.images) {
            if (!im.bytes.empty()) {
                ++nTexOk;
            }
        }
        elog::info("asset: glTF '%s' (%llu B) — parse ok: %u nó(s), "
                   "%u mesh(es), %u primitiva(s), %u material(is), "
                   "%u/%u textura(s) embutida(s), %u animação(ões), "
                   "%u skin(s)",
                   stem.c_str(),
                   static_cast<unsigned long long>(stats.sourceBytes),
                   static_cast<u32>(model.nodes.size()),
                   static_cast<u32>(model.meshes.size()), nPrims,
                   static_cast<u32>(model.materials.size()), nTexOk,
                   static_cast<u32>(model.images.size()),
                   static_cast<u32>(model.animations.size()),
                   static_cast<u32>(model.skins.size()));
    }
    // ---- 0.9.6.3 (R-020) · MERGE COM AS TRANSFORMS DOS NÓS ---------------
    // A spec: "Importa TODAS as malhas e primitivas, aplicando as
    // transformações dos nós, e junta num só modelo." ANTES: cada mesh saía
    // CRU (o TRS dos nós era ignorado — o modelo ficava fora do sítio /
    // invisível no device) e multi-mesh partia-se em <stem>_N.gmesh (o TIC
    // só recebia UMA parte). AGORA: um passe pela hierarquia compõe a
    // matriz-mundo de cada nó e TODAS as primitivas entram num ÚNICO
    // .gmesh (os grupos preservam o material por primitiva).
    MeshData merged;
    u32 nPrimMerged = 0;
    {
        const size_t nNodes = model.nodes.size();
        std::vector<Mat4> world(nNodes, Mat4::identity());
        std::vector<Mat4> worldRot(nNodes, Mat4::identity());
        std::vector<bool> done(nNodes, false);
        // a matriz-mundo por nó (cadeia de pais composta da raiz para baixo)
        for (size_t i = 0; i < nNodes; ++i) {
            if (done[i]) {
                continue;
            }
            // sobe a cadeia até uma raiz (pai -1 ou já computado)
            std::vector<u32> chain;
            i32 k = static_cast<i32>(i);
            while (k >= 0 && !done[static_cast<size_t>(k)]) {
                chain.push_back(static_cast<u32>(k));
                k = model.nodes[static_cast<size_t>(k)].parent;
            }
            Mat4 pw = k >= 0 ? world[static_cast<size_t>(k)]
                             : Mat4::identity();
            Mat4 rw = k >= 0 ? worldRot[static_cast<size_t>(k)]
                             : Mat4::identity();
            for (size_t c = chain.size(); c-- > 0;) {
                const GltfNode& nd =
                    model.nodes[chain[c]];
                const Mat4 local =
                    Mat4::mul(Mat4::translation(nd.translation.x,
                                                nd.translation.y,
                                                nd.translation.z),
                              Mat4::mul(nd.rotation.toMat4(),
                                        Mat4::scale(nd.scale.x, nd.scale.y,
                                                    nd.scale.z)));
                pw = Mat4::mul(pw, local);
                rw = Mat4::mul(rw, nd.rotation.toMat4());
                world[chain[c]] = pw;
                worldRot[chain[c]] = rw;
                done[chain[c]] = true;
            }
        }
        // funde cada (nó com mesh) → grupo por primitiva, vértices em MUNDO
        for (size_t n = 0; n < nNodes; ++n) {
            const GltfNode& nd = model.nodes[n];
            if (nd.mesh < 0 ||
                nd.mesh >= static_cast<i32>(model.meshes.size())) {
                continue;
            }
            const MeshData& src = model.meshes[static_cast<size_t>(nd.mesh)];
            const u16 base = static_cast<u16>(merged.vertices.size());
            if (merged.vertices.size() + src.vertices.size() > 65535) {
                err = "glTF: o modelo fundido excede 65535 vértices "
                      "(limite u16 do .gmesh)";
                return false;
            }
            for (const Vertex& pv : src.vertices) {
                Vertex v = pv;  // pos/normal/uv copiados
                const f32 px = pv.pos.x, py = pv.pos.y, pz = pv.pos.z;
                const Mat4& M = world[n];
                v.pos = Vec3{
                    M.m[0] * px + M.m[4] * py + M.m[8] * pz + M.m[12],
                    M.m[1] * px + M.m[5] * py + M.m[9] * pz + M.m[13],
                    M.m[2] * px + M.m[6] * py + M.m[10] * pz + M.m[14]};
                const Mat4& R = worldRot[n];
                const f32 nx = pv.normal.x, ny = pv.normal.y,
                          nz = pv.normal.z;
                v.normal = Vec3{R.m[0] * nx + R.m[4] * ny + R.m[8] * nz,
                                R.m[1] * nx + R.m[5] * ny + R.m[9] * nz,
                                R.m[2] * nx + R.m[6] * ny + R.m[10] * nz};
                merged.vertices.push_back(v);
            }
            for (const u16 idx : src.indices) {
                merged.indices.push_back(static_cast<u16>(base + idx));
            }
            for (const MeshData::Group& g : src.groups) {
                MeshData::Group grp = g;
                grp.firstIndex += base;
                grp.name = nd.name.empty() ? g.name
                                           : nd.name + " · " + g.name;
                merged.groups.push_back(grp);
                ++nPrimMerged;
            }
            // skin do mesh de origem (se houver) viaja com o merge
            merged.skinJoints.insert(merged.skinJoints.end(),
                                     src.skinJoints.begin(),
                                     src.skinJoints.end());
            merged.skinWeights.insert(merged.skinWeights.end(),
                                      src.skinWeights.begin(),
                                      src.skinWeights.end());
        }
    }
    // ---- passe de SAÍDA: um .gmesh único (ou o cru, se não há nós) -------
    if (!merged.vertices.empty()) {
        stats.verts = static_cast<u32>(merged.vertices.size());
        stats.indices = static_cast<u32>(merged.indices.size());
        // os LIMITES FINAIS no log (a spec PASSO 2f: sem auto-escala — o
        // aviso diz se está grande/pequeno, o dono decide)
        Vec3 mn{1e9f, 1e9f, 1e9f};
        Vec3 mx{-1e9f, -1e9f, -1e9f};
        for (const Vertex& v : merged.vertices) {
            mn.x = (std::min)(mn.x, v.pos.x);
            mn.y = (std::min)(mn.y, v.pos.y);
            mn.z = (std::min)(mn.z, v.pos.z);
            mx.x = (std::max)(mx.x, v.pos.x);
            mx.y = (std::max)(mx.y, v.pos.y);
            mx.z = (std::max)(mx.z, v.pos.z);
        }
        const f32 dims[3] = {mx.x - mn.x, mx.y - mn.y, mx.z - mn.z};
        const f32 maxDim = (std::max)(dims[0], (std::max)(dims[1], dims[2]));
        elog::info("asset: limites finais min(%.3f %.3f %.3f) max(%.3f "
                   "%.3f %.3f) — dimensões %.3f x %.3f x %.3f (unidades)",
                   (double)mn.x, (double)mn.y, (double)mn.z, (double)mx.x,
                   (double)mx.y, (double)mx.z, (double)dims[0],
                   (double)dims[1], (double)dims[2]);
        if (maxDim > 10000.0f) {
            elog::warn("asset: o modelo importado mede %.1f unidades — "
                       "grande de mais para a câmara padrão (aproxima ou "
                       "reduz a escala; NADA foi escalado automaticamente)",
                       (double)maxDim);
        } else if (maxDim > 0.0f && maxDim < 0.001f) {
            elog::warn("asset: o modelo importado mede %.4f unidades — "
                       "pequeno de mais para a câmara padrão (afasta ou "
                       "aumenta a escala; NADA foi escalado automaticamente)",
                       (double)maxDim);
        }
        std::vector<u8> bytes;
        if (!writeGMesh(merged, bytes, err)) {
            return false;
        }
        char nm[96];
        std::snprintf(nm, sizeof(nm), "assets/%s.gmesh", stem.c_str());
        if (!writeAsset(st, nm, bytes, stats.outputBytes, err)) {
            return false;
        }
        out.meshes.push_back(nm);
        stats.meshes++;
        elog::info("asset: %s verts=%u idx=%u prims=%u%s — registado na "
                   "lista como %s", nm, stats.verts, stats.indices,
                   nPrimMerged, merged.skinned() ? " (skin)" : "", nm);
    } else {
        // sem nós com mesh (glTF atípico): o caminho antigo, mesh a mesh
        for (size_t i = 0; i < model.meshes.size(); ++i) {
            MeshData mesh = model.meshes[i];
            stats.verts = static_cast<u32>(mesh.vertices.size());
            stats.indices = static_cast<u32>(mesh.indices.size());
            std::vector<u8> bytes;
            if (!writeGMesh(mesh, bytes, err)) {
                return false;
            }
            char nm[96];
            if (model.meshes.size() == 1) {
                std::snprintf(nm, sizeof(nm), "assets/%s.gmesh",
                              stem.c_str());
            } else {
                std::snprintf(nm, sizeof(nm), "assets/%s_%zu.gmesh",
                              stem.c_str(), i);
            }
            if (!writeAsset(st, nm, bytes, stats.outputBytes, err)) {
                return false;
            }
            out.meshes.push_back(nm);
            stats.meshes++;
            elog::info("asset: %s verts=%u idx=%u — registado na lista "
                       "como %s", nm, stats.verts, stats.indices, nm);
        }
    }
    // ---- passe de TEXTURAS: imagens embutidas → .gtext (uma a uma) ------
    // 0.9.6.3 (R-020 · spec PASSO 2b): FALHA PARCIAL NÃO ESCONDE O MODELO —
    // uma textura que falha é um AVISO no log e o mesh entra com material
    // por defeito (antes: UMA textura má derrubava o import inteiro)
    // 0.9.6.4 (GRUPO A): cada falha CONTA (stats.texWarn) — o toast do
    // import diz «SEM N textura(s)» (nunca silencioso); texturas EXTERNAS
    // de um .gltf são LIDAS do diretório dos irmãos (R-021) quando existe.
    for (size_t i = 0; i < model.images.size(); ++i) {
        GltfImage im = model.images[i];   // cópia: as externas ganham bytes
        if (im.broken) {
            // a causa já foi logada pelo parse (R-022: bufferView/data:)
            ++stats.texWarn;
            continue;
        }
        if (im.bytes.empty() && !im.uriPath.empty()) {
            // EXTERNA: lê do diretório dos irmãos (import: a pasta original;
            // reconvert FS: source/) — o irmão foi COPIADO para source/
            if (siblingImageDir && uriHasScheme(im.uriPath) == false &&
                im.uriPath.find("..") == std::string::npos) {
                const std::string dec = uriDecode(im.uriPath);
                const std::string sep =
                    (siblingImageDir[strlen(siblingImageDir) - 1] == '/')
                        ? "" : "/";
                const std::string p =
                    std::string(siblingImageDir) + sep + dec;
                if (fileapi::isFile(p) &&
                    fileapi::readAll(p, im.bytes) &&
                    im.bytes.size() <= kMaxImageBytes) {
                    im.mime = "image/png";   // o engine só consome PNG
                    elog::info("asset: textura externa '%s' LIDA do diretorio"
                               " dos irmaos (%llu B)",
                               im.uriPath.c_str(),
                               static_cast<unsigned long long>(
                                   im.bytes.size()));
                } else {
                    ++stats.texWarn;
                    elog::warn("asset: textura externa '%s' nao foi lida em "
                               "'%s' — o mesh entra com material por defeito "
                               "(copia o ficheiro ao lado do .gltf)",
                               im.uriPath.c_str(), siblingImageDir);
                    continue;
                }
            } else {
                ++stats.texWarn;
                elog::warn("asset: textura %zu e EXTERNA ('%s') e o diretorio"
                           " dos irmaos nao esta disponivel — o mesh entra "
                           "com material por defeito",
                           i, im.uriPath.c_str());
                continue;
            }
        }
        if (im.mime != "image/png") {
            ++stats.texWarn;
            elog::warn("asset: imagem %zu com mime '%s' ignorada (so PNG) "
                       "— o mesh entra com material por defeito",
                       i, im.mime.c_str());
            continue;
        }
        if (im.bytes.size() > kMaxImageBytes) {
            ++stats.texWarn;
            elog::warn("asset: textura embutida %zu de %zu MB excede o "
                       "orçamento — ignorada (o mesh entra com material "
                       "por defeito)",
                       i, im.bytes.size() / (1024 * 1024));
            continue;
        }
        CompressedImage comp;
        if (pipeline) {
            TextureLoadInfo info;
            std::string perr;
            if (!pipeline->process(im.bytes.data(), im.bytes.size(),
                                   im.mime.c_str(), comp, info, perr)) {
                ++stats.texWarn;
                elog::warn("asset: textura %zu falhou (%s) — o mesh entra "
                           "com material por defeito", i, perr.c_str());
                continue;
            }
        } else {
            static PassthroughCompressor kPassthrough;
            RawImage img;
            std::string pngErr;
            if (!loadPng(im.bytes.data(), im.bytes.size(), img, pngErr)) {
                ++stats.texWarn;
                elog::warn("asset: textura %zu falhou (%s) — o mesh entra "
                           "com material por defeito", i, pngErr.c_str());
                continue;
            }
            std::string cerr2;
            if (!kPassthrough.compress(img, comp, cerr2)) {
                ++stats.texWarn;
                elog::warn("asset: textura %zu falhou (%s) — o mesh entra "
                           "com material por defeito", i, cerr2.c_str());
                continue;
            }
        }
        std::vector<u8> gtext;
        if (!writeGText(comp, gtext, err)) {
            return false;
        }
        char nm[96];
        std::snprintf(nm, sizeof(nm), "assets/%s_%zu.gtext", stem.c_str(), i);
        if (!writeAsset(st, nm, gtext, stats.outputBytes, err)) {
            return false;
        }
        out.textures.push_back(nm);
        stats.textures++;
    }
    // ---- clips + esqueleto → .gm (O MESMO código do runtime) -------------
    {
        Scene dummy;
        const Handle h = dummy.create("conv");
        AnimationPlayer* pl = dummy.get(h)->addComponent<AnimationPlayer>();
        const u32 nClips = pl ? gltfAttachClips(dummy, h, model) : 0;
        GAnimFile anim;
        if (pl) {
            anim.clips = pl->clips;
        }
        stats.clips = nClips;
        if (!model.skins.empty()) {
            Scene dummy2;
            const Handle h2 = dummy2.create("skel");
            dummy2.get(h2)->addComponent<MeshRenderer>();
            const u32 nJoints = gltfAttachSkin(dummy2, h2, model);
            if (const SkeletonComp* sk =
                    dummy2.get(h2)->getComponent<SkeletonComp>()) {
                anim.joints = sk->joints;
            }
            stats.joints = nJoints;
        }
        if (!anim.clips.empty() || !anim.joints.empty()) {
            std::vector<u8> bytes;
            if (!writeGAnim(anim, bytes, err)) {
                return false;
            }
            const std::string rel =
                std::string("assets/") + stem + ".gm";
            if (!writeAsset(st, rel, bytes, stats.outputBytes, err)) {
                return false;
            }
            out.anim = rel;
        }
    }
    return true;
}

// GLB em ficheiro: header + chunks por fread; JSON inteiro; BIN deferred
bool convertGlbFile(const std::string& srcAbs, ProjectStorage& st,
                    const std::string& stem, TexturePipeline* pipeline,
                    Output& out, Stats& stats, std::string& err) {
    FILE* f = std::fopen(srcAbs.c_str(), "rb");
    if (!f) {
        err = "fonte ilegivel: " + srcAbs;
        return false;
    }
    bool ok = false;
    do {
        u8 hdr[12];
        if (std::fread(hdr, 1, 12, f) != 12) {
            err = "GLB curto demais (header)";
            break;
        }
        u32 magic = 0, ver = 0, total = 0;
        std::memcpy(&magic, hdr, 4);
        std::memcpy(&ver, hdr + 4, 4);
        std::memcpy(&total, hdr + 8, 4);
        if (magic != 0x46546C67u) {
            err = "magic GLB errado (não é um .glb)";
            break;
        }
        // primeiro chunk TEM de ser JSON
        u8 ch[8];
        if (std::fread(ch, 1, 8, f) != 8) {
            err = "GLB sem chunk header";
            break;
        }
        u32 chLen = 0, chType = 0;
        std::memcpy(&chLen, ch, 4);
        std::memcpy(&chType, ch + 4, 4);
        if (chType != 0x4E4F534Au) {
            err = "primeiro chunk do GLB não é JSON";
            break;
        }
        if (chLen > kMaxJsonBytes) {
            err = "JSON do GLB tem " + std::to_string(chLen / (1024 * 1024)) +
                  " MB — o orçamento e " +
                  std::to_string(kMaxJsonBytes / (1024 * 1024)) + " MB";
            break;
        }
        std::vector<u8> json;
        if (!readExact(f, 12 + 8, chLen, json)) {
            err = "JSON do GLB truncado";
            break;
        }
        // chunk BIN (opcional): header a seguir (com padding do JSON)
        u64 binBase = 0;
        u64 binLen = 0;
        {
            const u64 jsonEnd = 12 + 8 + chLen;
            // 0.9.6.4 (R-022 · A2): binStart ALINHADO A 4 — o spec pede o
            // padding DENTRO do chunk JSON, mas exportadores do mundo real
            // escrevem chLen sem padding e zeros até ao próximo header; o
            // alinhamento é o que torna os DOIS legíveis (e é a prova da
            // mutação «binStart sem alinhamento»)
            const u64 padded = (jsonEnd + 3) & ~3ull;
            if (std::fseek(f, static_cast<long>(padded), SEEK_SET) == 0) {
                u8 bh[8];
                if (std::fread(bh, 1, 8, f) == 8) {
                    u32 blen = 0, btype = 0;
                    std::memcpy(&blen, bh, 4);
                    std::memcpy(&btype, bh + 4, 4);
                    if (btype == 0x004E4942u) {
                        binBase = padded + 8;
                        binLen = blen;
                    }
                }
            }
        }
        // 0.9.6.4 (R-022 · A2) — O LOG DO LAYOUT DOS CHUNKS: a evidência
        // que o device não tinha (o dono via só «bufferView da imagem fora
        // do buffer»); json/bin/binStart alinhado 4/byteLength — os
        // bufferViews de cada imagem são logados pelo parse (images)
        u64 fileBytes = 0;
        fileapi::fileSize(srcAbs.c_str(), fileBytes);
        elog::info("glb: layout — header ver=%u total=%u · chunk JSON %u B "
                   "(start 12) · chunk BIN %llu B (binStart %llu, alinhado "
                   "4: %s) · ficheiro %llu B",
                   ver, total, chLen,
                   static_cast<unsigned long long>(binLen),
                   static_cast<unsigned long long>(binBase),
                   binLen > 0
                       ? ((binBase % 4) == 0 ? "sim" : "NÃO (corrompido?)")
                       : "n/a (sem BIN)",
                   static_cast<unsigned long long>(fileBytes));
        ok = convertGltfCommon(reinterpret_cast<const char*>(json.data()),
                               json.size(), f, binBase, binLen,
                               GltfBufferResolver{}, nullptr, st, stem,
                               pipeline, out, stats, err);
    } while (false);
    std::fclose(f);
    return ok;
}

// .gltf em ficheiro: JSON inteiro + .bin irmão (range loader no irmão)
// 0.9.6.4 (GRUPO A/R-021): os URIs externos resolvem contra o DIRETÓRIO DO
// FICHEIRO (parentPath) — ANTES liam contra o CWD do processo, que no
// Android é «/»: era a causa exata do «buffer externo não resolvido:
// scene.bin» (no CI passava porque os testes escreviam o .bin no CWD).
// As TEXTURAS externas vêm do MESMO diretório (siblingImageDir) — os irmãos
// já foram copiados para source/ pelo importFile; o reconvert (que lê de
// source/) resolve igualmente contra source/.
bool convertGltfFile(const std::string& srcAbs, ProjectStorage& st,
                     const std::string& stem, TexturePipeline* pipeline,
                     Output& out, Stats& stats, std::string& err) {
    std::vector<u8> json;
    if (!fileapi::readAll(srcAbs.c_str(), json) || json.empty()) {
        err = "leitura falhou: " + srcAbs + " (" + fileapi::errnoText() + ")";
        return false;
    }
    if (json.size() > kMaxJsonBytes) {
        err = "JSON do glTF tem " + std::to_string(json.size() / (1024 * 1024)) +
              " MB — o orçamento e " +
              std::to_string(kMaxJsonBytes / (1024 * 1024)) + " MB";
        return false;
    }
    // o DIRETÓRIO do ficheiro — a base de TODA a resolução de irmãos
    const std::string srcDir = fileapi::parentPath(srcAbs);
    // buffers externos: lê do DIRETÓRIO DA FONTE (URI-decode + guarda de
    // tamanho; o irmão Ausente dá a mensagem que NOMEIA o ficheiro)
    // 0.9.6.3 (R-020 · spec PASSO 2e) + 0.9.6.4 (R-021: base = srcDir)
    struct ExternCtx {
        std::string baseDir;   // a pasta do ficheiro .gltf em conversão
        bool failed = false;
        std::string uri;
        std::string cause;
    } externa;
    externa.baseDir = srcDir;
    GltfBufferResolver resolver;
    resolver.fn = [](void* user, const char* uri, std::vector<u8>& o) -> bool {
        auto* ctx = static_cast<ExternCtx*>(user);
        if (std::strlen(uri) > 512 || std::strstr(uri, "..") != nullptr) {
            ctx->failed = true;
            ctx->uri = uri;
            ctx->cause = "uri invalida";
            return false;
        }
        // URI-decode («tex%20albedo.bin» → «tex albedo.bin») — o nome NO
        // DISCO é o decodificado (o copyGltfSiblings copia-o assim)
        std::string dec;
        for (const char* p = uri; *p; ++p) {
            if (*p == '%' && p[1] && p[2]) {
                auto hex = [](char c) -> int {
                    if (c >= '0' && c <= '9') return c - '0';
                    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                    return -1;
                };
                const int hi = hex(p[1]);
                const int lo = hex(p[2]);
                if (hi >= 0 && lo >= 0) {
                    dec.push_back(static_cast<char>((hi << 4) | lo));
                    p += 2;
                    continue;
                }
            }
            dec.push_back(*p);
        }
        const std::string path = ctx->baseDir +
                                 (ctx->baseDir.back() == '/' ? "" : "/") +
                                 dec;
        const bool exists = fileapi::isFile(path);
        const bool ok = exists && fileapi::readAll(path, o) && !o.empty() &&
                        o.size() <= kMaxImageBytes * 4;
        if (!ok) {
            ctx->failed = true;
            ctx->uri = uri;
            ctx->cause = !exists ? "nao existe no diretorio do ficheiro"
                                 : "ilegivel ou grande demais";
            o.clear();
        }
        return ok;
    };
    resolver.user = &externa;
    const bool ok = convertGltfCommon(
        reinterpret_cast<const char*>(json.data()), json.size(), nullptr, 0,
        0, resolver, srcDir.c_str(), st, stem, pipeline, out, stats, err);
    if (!ok && externa.failed) {
        err = "o .gltf referencia o buffer externo '" + externa.uri +
              "' que nao foi lido (" + externa.cause + " — '" + srcDir +
              "') — copia o ficheiro em falta para essa pasta ou exporta o "
              "modelo como GLB (tudo embutido)";
    }
    return ok;
}

} // namespace

bool importFile(const std::string& srcAbs, const std::string& srcNameIn,
                ProjectStorage& st, TexturePipeline* pipeline,
                Output& out, Stats& stats, std::string& err,
                bool (*onProgress)(void*, u64, u64), void* user) {
    out = Output{};
    stats = Stats{};
    err.clear();
    const std::string name = sanitizeName(srcNameIn);
    if (name.empty()) {
        err = "nome de ficheiro vazio";
        return false;
    }
    const std::string stem = stemOf(name);
    if (stem.empty()) {
        err = "ficheiro sem nome";
        return false;
    }
    u64 srcSize = 0;
    if (!fileapi::fileSize(srcAbs.c_str(), srcSize)) {
        err = "fonte ilegivel: " + srcAbs + " (" + fileapi::errnoText() + ")";
        return false;
    }
    stats.sourceBytes = srcSize;

    // 1) CÓPIA STREAMING → source/<nome> (pelo write stream do storage —
    // FsStorage FILE* real, SafStorage fd real; nunca inteiro em RAM)
    // 0.9.6.4 (GRUPO A): a cópia é SALTA quando a fonte JÁ É o destino (o
    // reconvert de uma fonte que vive em source/ copiava o ficheiro POR
    // CIMA DE SI MESMO — a cópia saía corrompida em cascata)
    st.makeDirs("source");
    const std::string srcRel = std::string("source/") + name;
    const std::string srcAbsReal = fileapi::realPath(srcAbs);
    const std::string dstAbsReal =
        st.root().rfind("content://", 0) == 0
            ? std::string()
            : fileapi::realPath(joinRelPath(st.root(), srcRel));
    const bool selfCopy = !srcAbsReal.empty() && !dstAbsReal.empty() &&
                          srcAbsReal == dstAbsReal;
    if (selfCopy) {
        elog::info("import: a fonte JA vive em %s — copia saltada (o "
                   "reconvert le o ficheiro no lugar)", srcRel.c_str());
    } else {
        if (!copyToStorage(srcAbs, st, srcRel, stats, err, onProgress,
                           user)) {
            return false;   // err/canceled já preenchidos
        }
        elog::info("import: fonte copiada %s -> %s (%llu B em chunks de %zu)",
                   srcAbs.c_str(), srcRel.c_str(),
                   static_cast<unsigned long long>(srcSize), kChunkBytes);
    }

    // 1b) 0.9.6.4 (GRUPO A/R-021) — OS IRMÃOS DO .gltf: lê o JSON da fonte,
    // coleta buffers[].uri/images[].uri externos e COPIA-OS do diretório
    // ORIGINAL para source/<subcaminho> (o projeto fica autossuficiente e
    // o reconvert funciona sem a pasta original). Irmão AUSENTE = erro que
    // NOMEIA o ficheiro (a spec A1); a cópia do .gltf sai com ele (sem
    // estado parcial de um import que falhou).
    const std::string ext = lowerExtOfName(name);
    if (ext == "gltf") {
        std::vector<u8> json;
        if (!fileapi::readAll(srcAbs.c_str(), json) || json.empty() ||
            json.size() > kMaxJsonBytes) {
            err = "leitura do .gltf falhou (JSON vazio/ilegivel/maior que " +
                  std::to_string(kMaxJsonBytes / (1024 * 1024)) + " MB)";
            return false;
        }
        if (!copyGltfSiblings(
                reinterpret_cast<const char*>(json.data()), json.size(),
                fileapi::parentPath(srcAbs), st, stats, err, onProgress,
                user)) {
            if (!selfCopy && st.remove(srcRel)) {
                elog::info("import: copia '%s' removida (import falhou — "
                           "sem estado parcial)", srcRel.c_str());
            }
            return false;
        }
    }

    // 2) CONVERSÃO pela extensão (lê da FONTE original — mesma bytes)
    st.makeDirs("assets");
    bool ok = false;
    if (ext == "obj") {
        ok = convertObjStream(srcAbs, st, stem, out, stats, err, nullptr,
                              nullptr);
    } else if (ext == "glb") {
        ok = convertGlbFile(srcAbs, st, stem, pipeline, out, stats, err);
    } else if (ext == "gltf") {
        ok = convertGltfFile(srcAbs, st, stem, pipeline, out, stats, err);
    } else if (ext == "png") {
        ok = convertPng(srcAbs, st, stem, pipeline, out, stats, err);
    } else {
        err = "formato não suportado ainda: ." + ext +
              " (aceites: .obj .gltf .glb .png)";
        ok = false;
    }
    if (!ok) {
        return false;
    }
    // 2b) 0.9.6.4 (GRUPO A/R-022 · A2) — INTEGRIDADE DA CÓPIA do GLB: a
    // cópia em source/ é conferida contra a fonte byte a byte (em chunks);
    // uma cópia truncada NÃO fica no projeto (o reconvert lê-a depois — o
    // erro «cópia truncada» é a causa REAL, nunca um «bufferView fora do
    // buffer» disfarçado). Depois da conversão (esta lê a FONTE; a cópia
    // serve o futuro).
    if (ext == "glb" && !selfCopy) {
        std::string verr;
        if (!verifyCopyChunked(srcAbs, st, srcRel, verr)) {
            err = verr;
            elog::error("import: %s", verr.c_str());
            st.remove(srcRel);   // a cópia má sai — sem estado parcial
            return false;
        }
    }
    // 3) o RÁCIO no log (a prova de "menor dentro da engine") — com os
    // AVISOS de textura e irmãos EXPLÍCITOS (0.9.6.4: nunca silencioso)
    const double ratio = stats.outputBytes > 0
        ? static_cast<double>(stats.sourceBytes) /
          static_cast<double>(stats.outputBytes)
        : 0.0;
    elog::info("asset: convert '%s' -> %u mesh(es) %u tex %u clip(s) — "
               "%llu B -> %llu B (ratio %.2fx)%s%s",
               name.c_str(), stats.meshes, stats.textures, stats.clips,
               static_cast<unsigned long long>(stats.sourceBytes),
               static_cast<unsigned long long>(stats.outputBytes), ratio,
               stats.texWarn > 0
                   ? (" — SEM " + std::to_string(stats.texWarn) +
                      " textura(s) (avisos acima)").c_str()
                   : "",
               stats.siblings > 0
                   ? (" — " + std::to_string(stats.siblings) +
                      " irmao(s) copiado(s)").c_str()
                   : "");
    return true;
}

// 0.8.12 — STAGING SEM /tmp (o fix da migração morta no C33: o log do
// device dizia fileapi: mkdir falhou em /tmp errno=30 (Read-only file
// system) e a seguir staging falhou — o HOST de testes tem /tmp
// escrevível, o Android NÃO). O caminho por esta ordem (FALLBACK em
// cascata — cada tentativa falhada LOGA a causa):
//   1) .staging/ DENTRO do projeto — quando a raiz do storage é um
//      CAMINHO de ficheiros REAL e escrevível (FsStorage do device com
//      all-files): caminho absoluto real p/ o importFile (streaming por
//      chunks), criado RECURSIVAMENTE pelo makeDirs do writeAll, removido
//      no fim;
//   2) CACHE DIR DA APP via JNI (VvActivity.cacheDirPath → getCacheDir)
//      — quando a raiz é um URI content:// do SAF (sem fopen) ou a
//      escrita em .staging/ falhou: o cache dir é o único sítio
//      GUARANTIDO escrevível no Android sem permissões;
//   3) erro LEGÍVEL com os caminhos reais tentados — JAMAIS /tmp.
// Nunca existe literal de caminho /tmp em código de staging/assets/
// migração — o gate do CI (grep) vigia isso para sempre.
bool stagingWrite(ProjectStorage& st, const void* data, size_t n,
                  std::string& tmpPath, std::string& err) {
    char nm[64];
    std::snprintf(nm, sizeof(nm), "/goni_reconvert_%d.tmp",
                  static_cast<int>(::getpid()));
    const std::string& root = st.root();
    const bool isContentUri = root.rfind("content://", 0) == 0;
    // 1) .staging/ do projeto (caminho de ficheiros real)
    if (!root.empty() && !isContentUri) {
        const std::string p = joinRelPath(root, ".staging") + nm;
        if (fileapi::writeAll(p.c_str(), data, n)) {
            tmpPath = p;
            return true;
        }
        elog::warn("asset: staging em '%s' FALHOU (%s) — a tentar o cache "
                   "dir da app",
                   p.c_str(), fileapi::errnoText().c_str());
    }
    // 2) cache dir da app via JNI (getCacheDir — escrevível SEM permissões)
    const std::string cache = storage::jniCacheDir();
    if (!cache.empty()) {
        const std::string p = cache + "/.staging" + nm;
        if (fileapi::writeAll(p.c_str(), data, n)) {
            tmpPath = p;
            return true;
        }
        elog::warn("asset: staging no cache dir '%s' FALHOU (%s)",
                   p.c_str(), fileapi::errnoText().c_str());
    }
    err = "staging falhou: nem .staging/ do projeto (raiz '" + root +
          "') nem o cache dir da app aceitaram a escrita — /tmp nunca "
          "(read-only no Android, errno=30)";
    return false;
}

bool reconvertFile(const std::string& sourceRel, ProjectStorage& st,
                   TexturePipeline* pipeline, Output& out, Stats& stats,
                   std::string& err,
                   bool (*onProgress)(void*, u64, u64), void* user) {
    // lê a FONTE guardada no projeto: se o storage tem raiz REAL (FsStorage)
    // usa o caminho absoluto (streaming por chunks); se não (SAF), lê pelo
    // readBytes com GUARDA de orçamento (erro legível além do teto — o
    // caminho 100% streaming é o import do browser, que é onde os 500 MB
    // entram). RECONVERTER fontes gigantes em SAF fica como dívida.
    const std::string abs = joinRelPath(st.root(), sourceRel);
    const std::string name = sourceRel.substr(sourceRel.rfind('/') + 1);
    if (!abs.empty() && fileapi::fileSize(abs.c_str(), stats.sourceBytes)) {
        return importFile(abs, name, st, pipeline, out, stats, err,
                          onProgress, user);
    }
    std::vector<u8> bytes;
    if (!st.readBytes(sourceRel, bytes) || bytes.empty()) {
        err = "fonte não encontrada: " + sourceRel;
        return false;
    }
    if (bytes.size() > kStreamAccumMax) {
        err = "fonte de " + std::to_string(bytes.size() / (1024 * 1024)) +
              " MB excede o orçamento de re-conversao (" +
              std::to_string(kStreamAccumMax / (1024 * 1024)) + " MB)";
        return false;
    }
    stats.sourceBytes = bytes.size();
    // 0.8.12 — escreve num ficheiro temporário REAL (.staging/ do projeto
    // ou cache dir da app — NUNCA /tmp) e reusa o importFile (streaming);
    // falha = erro LEGÍVEL com os caminhos REAIS tentados
    std::string tmp;
    if (!stagingWrite(st, bytes.data(), bytes.size(), tmp, err)) {
        elog::error("asset: staging de reconversao FALHOU — %s", err.c_str());
        return false;
    }
    elog::info("asset: staging em '%s' (%zu B — projeto/cache, sem /tmp)",
               tmp.c_str(), bytes.size());
    // 0.9.6.4 (GRUPO A/R-021) — RECONVERT DE .gltf EM SAF: os IRMÃOS são
    // stageados AO LADO do .gltf (lidos de source/<uri> pelo storage — o
    // import copiou-os para lá); sem o irmão o reconvert FALHA dizendo o
    // nome (o ficheiro original deixou de ser preciso — o projeto é
    // autossuficiente ou diz o que falta)
    std::vector<std::string> stagedSiblings;
    const size_t dotGltf = name.rfind(".gltf");
    if (dotGltf != std::string::npos && dotGltf + 5 == name.size()) {
        std::vector<std::string> uris;
        std::string jerr;
        if (collectGltfSiblingUris(
                reinterpret_cast<const char*>(bytes.data()), bytes.size(),
                uris, jerr)) {
            const std::string stageDir =
                tmp.substr(0, tmp.rfind('/'));
            for (const std::string& uri : uris) {
                std::string path;
                std::string rel;
                std::string serr;
                if (!siblingPathOf(stageDir, uri, path, rel, serr)) {
                    continue;   // data:/esquema — não é irmão de ficheiro
                }
                std::vector<u8> sib;
                if (!st.readBytes(std::string("source/") + rel, sib) ||
                    sib.empty()) {
                    err = "a fonte '" + sourceRel + "' referencia o irmao '" +
                          uri + "' que NAO vive em source/ do projeto — "
                          "reimporta o ficheiro ORIGINAL (o import copia os "
                          "irmaos)";
                    elog::error("asset: %s", err.c_str());
                    ::remove(tmp.c_str());
                    for (const std::string& s : stagedSiblings) {
                        ::remove(s.c_str());
                    }
                    return false;
                }
                const std::string sibPath =
                    stageDir + "/" + rel;   // decode já feito no rel
                const size_t slash = sibPath.rfind('/');
                if (slash != std::string::npos) {
                    fileapi::makeDirs(sibPath.substr(0, slash));
                }
                if (!fileapi::writeAll(sibPath, sib.data(), sib.size())) {
                    err = "staging do irmao '" + uri + "' FALHOU (" +
                          fileapi::errnoText() + ")";
                    ::remove(tmp.c_str());
                    for (const std::string& s : stagedSiblings) {
                        ::remove(s.c_str());
                    }
                    return false;
                }
                stagedSiblings.push_back(sibPath);
                elog::info("asset: irmao '%s' stageado (%zu B) ao lado da "
                           "fonte", uri.c_str(), sib.size());
            }
        }
    }
    const bool ok = importFile(tmp, name, st, pipeline, out, stats, err,
                               onProgress, user);
    ::remove(tmp.c_str());
    for (const std::string& s : stagedSiblings) {
        ::remove(s.c_str());   // os irmãos stageados saem com a fonte
    }
    // a pasta .staging/ fica (tamanho ~0; o próximo reconvert reusa) —
    // remover a pasta seria competir com reconverts concorrentes do
    // migrateLegacyAssets no MESMO load
    return ok;
}

u32 migrateLegacyAssets(ProjectStorage& st, TexturePipeline* pipeline) {
    u32 converted = 0;
    const char* dirs[2] = {"meshes", "textures"};
    for (int d = 0; d < 2; ++d) {
        std::vector<std::string> files;
        if (!st.listDir(dirs[d], files)) {
            continue;
        }
        for (const std::string& f : files) {
            const std::string rel = std::string(dirs[d]) + "/" + f;
            const std::string abs = joinRelPath(st.root(), rel);
            if (abs.empty()) {
                continue;
            }
            const std::string stem = stemOf(f);
            // já convertido? (assets/<stem>.gmesh ou .gtext)
            const char* wantExt = d == 0 ? ".gmesh" : ".gtext";
            const std::string wantRel =
                std::string("assets/") + stem + wantExt;
            if (st.exists(wantRel)) {
                continue;
            }
            Output out;
            Stats stats;
            std::string err;
            elog::info("asset: migracao '%s' (projeto antigo — silenciosa)",
                       rel.c_str());
            // reconvertFile cobre FS real (streaming) E SAF (readBytes com
            // guarda + staging) — um só caminho de reconversão
            if (!reconvertFile(rel, st, pipeline, out, stats, err)) {
                // falha individual NÃO mata o load (o caminho legacy
                // continua a resolver no ResourceManager)
                elog::warn("asset: migracao de '%s' FALHOU — %s (o original "
                           "continua a carregar)", rel.c_str(), err.c_str());
                continue;
            }
            ++converted;
        }
    }
    if (converted > 0) {
        elog::info("asset: migracao converteu %u ficheiro(s) legado(s) em "
                   "silencio (formatos proprios)", converted);
    }
    return converted;
}

std::string ganimSiblingOf(const std::string& meshRel) {
    const size_t dot = meshRel.rfind(".gmesh");
    if (dot == std::string::npos ||
        dot + 6 != meshRel.size()) {
        return "";
    }
    return meshRel.substr(0, dot) + ".gm";
}

} // namespace convert
} // namespace vv
