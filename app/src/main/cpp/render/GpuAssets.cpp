// render/GpuAssets.cpp — 1 ref → 1 objeto GL (F5-E; texturas F5.1-A).
// 0.10-M (PASSO 4): + o caminho dos BLOCOS (BlockMesh por ref + hull).
#include "render/GpuAssets.h"
#include "assets/GOwnFormats.h"
#include "assets/TextureCompressor.h"
#include "assets/TexturePipeline.h"
#include "platform/Log.h"
#include "platform/EngineLog.h"   // 0.10-M (PASSO 3B): elog — o dono lê o FICHEIRO
#include "render/BlockMesh.h"
#include "render/Mesh.h"
#include "render/Texture.h"
#include <cctype>
#include <chrono>

namespace vv {

namespace {

// 0.8.5: extensão minúscula (a classificação local do fallback — o
// ResourceManager tem a sua; aqui só interessa gltf/glb)
std::string lowerExtOf(const std::string& path) {
    const size_t dot = path.rfind('.');
    if (dot == std::string::npos) {
        return "";
    }
    std::string e = path.substr(dot + 1);
    for (char& c : e) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return e;
}

} // namespace

Mesh* GpuAssets::mesh(const std::string& ref) {
    if (!rm_) {
        return nullptr;
    }
    const auto it = gpuMeshes_.find(ref);
    if (it != gpuMeshes_.end()) {
        return it->second.get();   // MESMO objeto GL — sem duplicar GPU
    }
    // 0.10-M (PASSO 4) — O CAMINHO DOS BLOCOS: um .gmesh v3 que o mesh
    // ÚNICO recusaria (verts > 65 535 ou estimativa > orçamento — a MESMA
    // condição do guard cedo do ResourceManager, uma só verdade) abre
    // pela TABELA e devolve o HULL de bounds. O peek é o MESMO espião de
    // 192 B do PASSO 3B (uma leitura por ref, antes do ficheiro inteiro).
    {
        const std::string ext = lowerExtOf(ref);
        if (ext == "gmesh" && rm_->storage()) {
            std::vector<u8> peek;
            if (rm_->storage()->readBytesAt(
                    ref, 0, kGHeaderBytes + kGmeshV3MetaBytes, peek)) {
                GMeshV3Meta meta;
                std::string perr;
                if (gmeshV3PeekMeta(peek.data(), peek.size(), meta, perr) &&
                    (meta.vertexCount > 65535 ||
                     gmeshV3LoadEstimateBytes(meta) > kMeshLoadBudgetBytes)) {
                    return blockHull(ref);
                }
            }
        }
    }
    std::string err;
    const MeshData* data = rm_->mesh(ref, err);
    if (!data) {
        // 0.8.5 (fix do "import não aplica" no device): glTF/GLB SEM '#'
        // com VÁRIOS meshes falhava no ResourceManager ("use path#<i>") —
        // mas o browser/catálogo/aplicar nunca geram sub-refs. WIRING: o
        // fallback do APPLY é mesh #0 (o contrato do ResourceManager fica
        // intacto p/ refs explícitas; o apply do device deixa de morrer).
        const size_t hash = ref.find('#');
        const std::string ext = lowerExtOf(ref);
        if (hash == std::string::npos && (ext == "gltf" || ext == "glb")) {
            data = rm_->mesh(ref + "#0", err);
            if (data) {
                LOGI("GpuAssets: '%s' tem múltiplos meshes — a aplicar o #0 "
                     "(use '#<i>' no .goni para outro)", ref.c_str());
            }
        }
    }
    if (!data || !data->ok()) {
        // 0.10-M (PASSO 3B) — A CAUSA CHEGA AO engine.LOG: o LOGE de sempre
        // só fala com o logcat (o dono não tem PC); o err do ResourceManager
        // (a mensagem «memória insuficiente ao carregar mesh — cura no
        // PASSO 4: render por blocos», com verts/blocos/MB) ia MORRER aqui
        // sem nunca entrar no ficheiro que o viewer/export mostram. O elog
        // escreve NOS DOIS (logcat + engine.log).
        if (!err.empty()) {
            elog::error("gpu: '%s' FALHOU ao carregar — %s", ref.c_str(),
                        err.c_str());
        } else {
            elog::error("gpu: '%s' FALHOU ao carregar (mesh vazio/inválido)",
                        ref.c_str());
        }
        return nullptr;
    }
    // 0.10-M (PASSO 3B) — a fase de RENDER (o upload GPU do mesh inteiro —
    // o último dono dos minutos do dono: parse/cut/assembly/verify no
    // conversor, load aqui atrás, render é o upload)
    const auto tUpload = std::chrono::steady_clock::now();
    auto m = std::make_unique<Mesh>();
    // 0.8.2 (F7): MeshData com skin → createSkinned (aJoints/aWeights nas
    // locations 3/4); estático → create de sempre (comportamento idêntico)
    if (data->skinned()) {
        if (!m->createSkinned(data->vertices.data(),
                              static_cast<u32>(data->vertices.size()),
                              data->indices.data(),
                              static_cast<u32>(data->indices.size()),
                              data->skinJoints.data(),
                              data->skinWeights.data())) {
            elog::error("gpu: '%s' FALHOU no upload (createSkinned)",
                        ref.c_str());
            return nullptr;
        }
    } else if (!m->create(data->vertices.data(),
                          static_cast<u32>(data->vertices.size()),
                          data->indices.data(),
                          static_cast<u32>(data->indices.size()))) {
        elog::error("gpu: '%s' FALHOU no upload (create)", ref.c_str());
        return nullptr;   // upload falhou — sem cache de objeto quebrado
    }
    elog::info("gmesh: fase=render ms=%.1f (%s, %u verts)",
               std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - tUpload)
                   .count(),
               ref.c_str(), static_cast<unsigned>(data->vertices.size()));
    Mesh* raw = m.get();
    gpuMeshes_.emplace(ref, std::move(m));
    return raw;
}

const Texture* GpuAssets::texture(const std::string& relPath, std::string* warn) {
    if (!rm_) {
        return nullptr;
    }
    const auto it = gpuTextures_.find(relPath);
    if (it != gpuTextures_.end()) {
        if (warn) {
            warn->clear();   // do cache não repete o aviso do gate
        }
        return it->second.get();
    }

    // 0.8.10 — FORMATO PRÓPRIO .gtext: o container já TEM os mips ASTC/ETC2
    // (o import comprimiu UMA vez) — upload direto, zero re-processo.
    if (rm_->storage()) {
        const std::string ext = lowerExtOf(relPath);
        if (ext == "gtext") {
            std::vector<u8> bytes;
            if (!rm_->storage()->readBytes(relPath, bytes) || bytes.empty()) {
                LOGE("GpuAssets: textura não encontrada: %s", relPath.c_str());
                return nullptr;
            }
            CompressedImage comp;
            std::string gerr;
            if (!readGText(bytes.data(), bytes.size(), comp, gerr)) {
                LOGE("GpuAssets: .gtext inválido (%s): %s", relPath.c_str(),
                     gerr.c_str());
                return nullptr;
            }
            LOGI("GpuAssets: textura %s %ux%u %s (gtext direto)",
                 relPath.c_str(), comp.width, comp.height,
                 formatName(comp.format));
            auto t = std::make_unique<Texture>();
            if (!t->createFromCompressed(comp)) {
                return nullptr;
            }
            Texture* raw = t.get();
            gpuTextures_.emplace(relPath, std::move(t));
            return raw;
        }
    }

    // F5.1-A: caminho canônico — PNG bytes → pipeline (cache disco +
    // compressão ETC2/ASTC com cadeia de mips em CPU) → glCompressedTexImage2D
    if (pipeline_ && rm_->storage()) {
        std::vector<u8> bytes;
        if (!rm_->storage()->readBytes(relPath, bytes) || bytes.empty()) {
            LOGE("GpuAssets: textura não encontrada: %s", relPath.c_str());
            return nullptr;
        }
        CompressedImage comp;
        TextureLoadInfo info;
        std::string perr;
        if (!pipeline_->process(bytes.data(), bytes.size(), relPath.c_str(),
                                comp, info, perr)) {
            LOGE("GpuAssets: textura %s falhou: %s", relPath.c_str(),
                 perr.c_str());
            return nullptr;
        }
        if (warn && !info.warn.empty()) {
            *warn = info.warn;
        }
        LOGI("GpuAssets: textura %s %ux%u %s via %s%s", relPath.c_str(),
             comp.width, comp.height, formatName(comp.format), info.via,
             info.warn.empty() ? "" : info.warn.c_str());
        auto t = std::make_unique<Texture>();
        if (!t->createFromCompressed(comp)) {
            return nullptr;
        }
        Texture* raw = t.get();
        gpuTextures_.emplace(relPath, std::move(t));
        return raw;
    }

    // legacy F5 (sem pipeline): RawImage → passthrough → GL
    std::string err;
    const RawImage* img = rm_->image(relPath, err, warn);
    if (!img || !img->ok()) {
        return nullptr;
    }
    static PassthroughCompressor kCompressor;
    CompressedImage comp;
    std::string cerr2;
    if (!kCompressor.compress(*img, comp, cerr2)) {
        return nullptr;
    }
    auto t = std::make_unique<Texture>();
    if (!t->createFromCompressed(comp)) {
        return nullptr;
    }
    Texture* raw = t.get();
    gpuTextures_.emplace(relPath, std::move(t));
    return raw;
}

BlockMesh* GpuAssets::blockMeshIfOpen(const std::string& ref) const {
    const auto it = blockMeshes_.find(ref);
    return it != blockMeshes_.end() ? it->second.get() : nullptr;
}

Mesh* GpuAssets::blockHull(const std::string& ref) {
    // (1) o BlockMesh — 1 ref = 1 abertura (a tabela; NUNCA os dados)
    BlockMesh* bm = blockMeshIfOpen(ref);
    if (!bm) {
        auto nb = std::make_unique<BlockMesh>();
        std::string err;
        if (!nb->open(*rm_->storage(), ref, err)) {
            // a recusa do mesh único (PASSO 3B) já logou a CAUSA; aqui é a
            // falha da ABERTURA por blocos (corrupção de tabela/faixa) —
            // o elog chega ao engine.log que o dono LÊ
            elog::error("gpu: '%s' FALHOU ao abrir por BLOCOS — %s",
                        ref.c_str(), err.c_str());
            return nullptr;
        }
        bm = nb.get();
        blockMeshes_.emplace(ref, std::move(nb));
    }
    // (2) o HULL — o Mesh* do contrato (bounds EXATOS do meta; o render é
    // do BlockMesh: drawTics consulta MeshRenderer::blocks PRIMEIRO)
    auto hull = std::make_unique<Mesh>();
    if (!bm->createHull(*hull)) {
        elog::error("gpu: '%s' FALHOU no hull de bounds (upload GL)",
                    ref.c_str());
        return nullptr;
    }
    Mesh* raw = hull.get();
    gpuMeshes_.emplace(ref, std::move(hull));
    elog::info(
        "blocos: '%s' aplicado — %u blocos, %llu verts, %.1f MB de dados "
        "POR CARREGAR (lazy: a 1ª visibilidade de cada bloco é que lê)",
        ref.c_str(), static_cast<unsigned>(bm->table().size()),
        static_cast<unsigned long long>(bm->meta().vertexCount),
        static_cast<double>([&]() {
            u64 n = 0;
            for (const GMeshV3Block& b : bm->table()) {
                n += b.dataSize;
            }
            return n;
        }()) / (1024.0 * 1024.0));
    return raw;
}

void GpuAssets::releaseAll() {
    gpuMeshes_.clear();
    gpuTextures_.clear();
    blockMeshes_.clear();   // TERM_WINDOW: o re-open é 192 B + tabela
}

} // namespace vv
