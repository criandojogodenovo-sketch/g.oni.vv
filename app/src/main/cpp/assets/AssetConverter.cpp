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
bool convertGltfCommon(const char* json, size_t jsonLen, FILE* binFile,
                       u64 binBase, u64 binLen,
                       const GltfBufferResolver& resolver,
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
    for (size_t i = 0; i < model.images.size(); ++i) {
        const GltfImage& im = model.images[i];
        if (im.bytes.empty()) {
            elog::warn("asset: textura %zu é EXTERNA ('%s') — o .gltf não "
                       "a traz; o mesh entra com material por defeito",
                       i, im.uriPath.c_str());
            continue;   // externa — o ficheiro é que a traz (ou ignora)
        }
        if (im.mime != "image/png") {
            elog::warn("asset: imagem %zu com mime '%s' ignorada (so PNG) "
                       "— o mesh entra com material por defeito",
                       i, im.mime.c_str());
            continue;
        }
        if (im.bytes.size() > kMaxImageBytes) {
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
                elog::warn("asset: textura %zu falhou (%s) — o mesh entra "
                           "com material por defeito", i, perr.c_str());
                continue;
            }
        } else {
            static PassthroughCompressor kPassthrough;
            RawImage img;
            std::string pngErr;
            if (!loadPng(im.bytes.data(), im.bytes.size(), img, pngErr)) {
                elog::warn("asset: textura %zu falhou (%s) — o mesh entra "
                           "com material por defeito", i, pngErr.c_str());
                continue;
            }
            std::string cerr2;
            if (!kPassthrough.compress(img, comp, cerr2)) {
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
        (void)ver;
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
        ok = convertGltfCommon(reinterpret_cast<const char*>(json.data()),
                               json.size(), f, binBase, binLen,
                               GltfBufferResolver{}, st, stem, pipeline, out,
                               stats, err);
    } while (false);
    std::fclose(f);
    return ok;
}

// .gltf em ficheiro: JSON inteiro + .bin irmão (range loader no irmão)
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
    // resolve .bin irmãos: lê do disco à volta da fonte (FileApi) e regista
    // o PRIMEIRO como buffer 0 do range loader (o caso comum: 1 buffer)
    struct SiblingCtx {
        FILE* f = nullptr;
        u64 len = 0;
    };
    GltfBufferResolver resolver;
    // buffers externos: carrega INTEIRO com guarda (o .bin típico é pequeno;
    // o caso ENORME é o GLB — este é o .gltf textual)
    // 0.9.6.3 (R-020 · spec PASSO 2e): se o vizinho .bin não se consegue
    // ler, a mensagem diz ISSO e sugere o GLB (nunca falha em silêncio)
    struct ExternCtx {
        bool failed = false;
        std::string uri;
    } externa;
    resolver.fn = [](void* user, const char* uri, std::vector<u8>& o) -> bool {
        auto* ctx = static_cast<ExternCtx*>(user);
        if (std::strlen(uri) > 512 || std::strstr(uri, "..") != nullptr) {
            ctx->failed = true;
            ctx->uri = uri;
            return false;
        }
        const bool ok = fileapi::readAll(uri, o) && !o.empty() &&
                        o.size() <= kMaxImageBytes * 4;
        if (!ok) {
            ctx->failed = true;
            ctx->uri = uri;
        }
        return ok;
    };
    resolver.user = &externa;
    const bool ok = convertGltfCommon(
        reinterpret_cast<const char*>(json.data()), json.size(), nullptr, 0,
        0, resolver, st, stem, pipeline, out, stats, err);
    if (!ok && externa.failed) {
        err = "o .gltf referencia ficheiros externos ('" + externa.uri +
              "') que não foi possível ler ao lado do ficheiro — o seletor "
              "do Android não dá acesso aos vizinhos; exporta o modelo "
              "como GLB (tudo embutido num só ficheiro)";
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
    st.makeDirs("source");
    const std::string srcRel = std::string("source/") + name;
    if (!copyToStorage(srcAbs, st, srcRel, stats, err, onProgress, user)) {
        return false;   // err/canceled já preenchidos
    }
    elog::info("import: fonte copiada %s -> %s (%llu B em chunks de %zu)",
               srcAbs.c_str(), srcRel.c_str(),
               static_cast<unsigned long long>(srcSize), kChunkBytes);

    // 2) CONVERSÃO pela extensão (lê da FONTE original — mesma bytes)
    const std::string ext = lowerExtOfName(name);
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
    // 3) o RÁCIO no log (a prova de "menor dentro da engine")
    const double ratio = stats.outputBytes > 0
        ? static_cast<double>(stats.sourceBytes) /
          static_cast<double>(stats.outputBytes)
        : 0.0;
    elog::info("asset: convert '%s' -> %u mesh(es) %u tex %u clip(s) — "
               "%llu B -> %llu B (ratio %.2fx)",
               name.c_str(), stats.meshes, stats.textures, stats.clips,
               static_cast<unsigned long long>(stats.sourceBytes),
               static_cast<unsigned long long>(stats.outputBytes), ratio);
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
    const bool ok = importFile(tmp, name, st, pipeline, out, stats, err,
                               onProgress, user);
    ::remove(tmp.c_str());
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
