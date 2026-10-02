#pragma once
// assets/AssetConverter.h — IMPORT STREAMING + CONVERSÃO (0.8.10).
//
// O CAMINHO NOVO do import (um só入口, chamado pelo browserImportFile):
//   1. CÓPIA STREAMING da fonte → source/<nome> no projeto (chunks de
//      kChunkBytes — NUNCA o ficheiro inteiro em RAM; progresso por chunk
//      com CANCELAMENTO);
//   2. CONVERSÃO lendo a fonte POR RANGES/CHUNKS:
//        .obj  → ObjStreamParser linha-a-linha (qualquer tamanho)
//        .glb  → header+JSON inteiros (JSON ≤ kMaxJsonBytes) + BIN chunk
//                DEFERIDO: accessors/imagens materializam SÓ os seus
//                ranges (≤ kMaxRangeBytes cada)
//        .gltf → JSON ≤ kMaxJsonBytes + .bin irmão por range loader
//        .png  → bytes ≤ kMaxImageBytes (decode inteiro; além = erro
//                legível — foto de 64 MB+ não é textura de engine)
//   3. ESCRITA dos CONVERTIDOS em assets/:
//        meshes  → assets/<stem>[_i].gmesh (quantizado 16-bit, indexado)
//        texturas→ assets/<stem>[_i].gtext (mips ASTC/ETC2 do pipeline)
//        anims+skel → assets/<stem>.gm (clips + joints)
//   4. LOG com RÁCIO: "asset: convert <nome> -> N ficheiro(s), X B -> Y B
//      (ratio Z)" — a prova de "menor dentro da engine".
//
// ORÇAMENTO de memória (a "guarda" do prompt): a RAM de pico é o chunk +
// um range + um CompressedImage — independentemente do tamanho da fonte.
// O passe de GEOMETRIA corre e escreve ANTES do passe de TEXTURAS (os
// intermédios libertam-se entre passes).
//
// GL-free / Android-free: FileApi POSIX + ProjectStorage — CI Linux.
#include <string>
#include <vector>

#include "assets/GOwnFormats.h"
#include "core/ProjectStorage.h"

namespace vv {

class TexturePipeline;

namespace convert {

// chunk da cópia streaming (4-8 MB, a faixa do prompt)
constexpr size_t kChunkBytes = 6ull * 1024 * 1024;
// JSON do glTF inteiro (o documento tem de caber — além: erro legível)
constexpr size_t kMaxJsonBytes = 16ull * 1024 * 1024;
// PNG em RAM p/ decode (fotos normais: 2-8 MB)
constexpr size_t kMaxImageBytes = 64ull * 1024 * 1024;

struct Stats {
    u64 sourceBytes = 0;    // bytes da fonte (cópia streaming)
    u64 outputBytes = 0;    // soma dos convertidos escritos
    u32 meshes = 0;
    u32 textures = 0;
    u32 clips = 0;
    u32 joints = 0;
    u32 verts = 0;
    u32 indices = 0;
    bool canceled = false;
};

struct Output {
    std::vector<std::string> meshes;     // "assets/casa.gmesh"
    std::vector<std::string> textures;   // "assets/casa_0.gtext"
    std::string anim;                    // "assets/casa.gm" ("" sem clips)
};

// converte UMA fonte escolhida no navegador (caminho ABSOLUTO do FileApi)
// para os formatos próprios do projeto. `pipeline` (texturas) opcional —
// null cai no passthrough RGBA. onProgress(user, done, total) por chunk
// da cópia; devolve false = CANCELADO (a cópia parcial é removida).
// Falha → false + err LEGÍVEL (nunca crash, nunca silêncio).
bool importFile(const std::string& srcAbs, const std::string& srcName,
                ProjectStorage& st, TexturePipeline* pipeline,
                Output& out, Stats& stats, std::string& err,
                bool (*onProgress)(void*, u64, u64) = nullptr,
                void* user = nullptr);

// ---- RECONVERSÃO (botão "reconverter"): lê a FONTE guardada em source/ ----
// Mesma conversão a partir da cópia do projeto (chunks/ranges contra o
// storage quando FS real; no SAF lê inteiro com guarda de orçamento).
bool reconvertFile(const std::string& sourceRel, ProjectStorage& st,
                   TexturePipeline* pipeline, Output& out, Stats& stats,
                   std::string& err,
                   bool (*onProgress)(void*, u64, u64) = nullptr,
                   void* user = nullptr);

// ---- MIGRAÇÃO de projeto antigo (meshes/*.obj|gltf|glb, textures/*.png) ----
// Converte EM SILÊNCIO no primeiro load: cada fonte legada → assets/*.gmesh
// / *.gtext (+.gm); devolve o número de ficheiros convertidos (0 = nada a
// fazer). Fontes legadas NÃO são apagadas (o setting "largar a fonte" é
// quem as larga). Falha individual = log + segue (o mesh antigo continua
// a resolver pelo caminho legacy do ResourceManager).
u32 migrateLegacyAssets(ProjectStorage& st, TexturePipeline* pipeline);

// sandbox de nome (como o browserImportFile: '/'→'_' etc)
std::string sanitizeName(const std::string& in);

// stem sem extensão ("casa.obj" → "casa")
std::string stemOf(const std::string& name);

// irmão .gm de uma ref convertida ("assets/x.gmesh" → "assets/x.ggm" não:
// → "assets/x.gm"; sem '#' e só p/ refs .gmesh; senão "")
std::string ganimSiblingOf(const std::string& meshRel);

} // namespace convert
} // namespace vv
