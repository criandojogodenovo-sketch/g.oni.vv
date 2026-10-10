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
#include "math/Math.h"   // 0.10-M (EXT): Vec3/Quat do NodeOut

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
    // 0.9.6.4 (GRUPO A/R-022 · A3): texturas que FALHARAM ou foram
    // ignoradas (imagem podre, bufferView fora do buffer, mime não-PNG,
    // orçamento) — o import SEGUE sem elas; o toast diz «SEM N textura(s)»
    // e a causa de cada uma vive no engine.log. NUNCA silencioso.
    u32 texWarn = 0;
    // 0.9.6.4 (GRUPO A/R-021): irmãos copiados do diretório original
    // (.bin/texturas de um .gltf separado) — o log lista um por um.
    u32 siblings = 0;
    // ---- PASSO 5A: as duas operações contam SEPARADAS (a tabela do
    // relatório lê daqui): redução de resolução e normal maps
    u32 texReduced = 0;   // texturas cuja RESOLUÇÃO desceu (perfil/override/teto)
    u32 texNormal = 0;    // texturas no caminho RGBA8 (normal maps — sem perda)
    // 0.9.6.12 (A2/R-014 · a spec 2d): primitivas LARGADAS por bufferView
    // fora do buffer (exporter malformado) — o import SEGUE com o resto;
    // o toast diz «K primitiva(s) fora» (nunca silencioso)
    u32 primWarn = 0;
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

    // ---- 0.10-M (EXT · IMPORT DE NÓS COMO SUB-ÁRVORE) --------------------
    // PREENCHIDO SÓ pelo import com `expandNodes=true` (vazio = o default
    // FUNDIDO de sempre — um .gmesh por fonte, ZERO mudanças nesse caminho).
    // Um registo por NÓ COM MESH (≥1 primitiva que sobreviveu ao corte),
    // NA ORDEM dos nós do glTF; `mesh` = índice em `Output::meshes` (a
    // PEÇA daquele nó — "assets/<stem>_<nomeDoNó>.gmesh"). O main cria o
    // TIC por registo (gltfExpandInstantiate): nome do glTF preservado,
    // `translation/rotation/scale` = a transformação MUNDO do nó no
    // Transform3D do TIC (o engine compõe flat — TransformSystem não
    // resolve pais — por isso a MUNDO: a peça desenha no sítio EXATO; a
    // geometria da peça fica CRUA no espaço local do nó). Nós com shear
    // no mundo (escala não-uniforme do pai + rotação do filho) trazem a
    // transformação baked na geometria e TRS de identidade — honesto, no
    // log. `node` = índice no array de nós do glTF (diag).
    struct NodeOut {
        std::string name;                 // do glTF ("n<i>" se anónimo)
        Vec3 translation{0.0f, 0.0f, 0.0f};
        Quat rotation{0.0f, 0.0f, 0.0f, 1.0f};
        Vec3 scale{1.0f, 1.0f, 1.0f};
        i32 mesh = -1;   // índice em Output::meshes (-1 = sem peça)
        i32 node = -1;   // índice no GltfModel::nodes (diag)
    };
    std::vector<NodeOut> expandNodes;    // vazio = import fundido
};

// converte UMA fonte escolhida no navegador (caminho ABSOLUTO do FileApi)
// para os formatos próprios do projeto. `pipeline` (texturas) opcional —
// null cai no passthrough RGBA. onProgress(user, done, total) por chunk
// da cópia; devolve false = CANCELADO (a cópia parcial é removida).
// Falha → false + err LEGÍVEL (nunca crash, nunca silêncio).
// 0.10-M (EXT): `expandNodes` = true → o import EXPANDIDO (um .gmesh
// PEÇA por nó com mesh + Output::expandNodes p/ criar os TICs; o default
// false = o FUNDIDO de sempre — byte a byte idêntico ao caminho vigente).
bool importFile(const std::string& srcAbs, const std::string& srcName,
                ProjectStorage& st, TexturePipeline* pipeline,
                Output& out, Stats& stats, std::string& err,
                bool (*onProgress)(void*, u64, u64) = nullptr,
                void* user = nullptr, bool expandNodes = false);

// ---- RECONVERSÃO (botão "reconverter"): lê a FONTE guardada em source/ ----
// Mesma conversão a partir da cópia do projeto (chunks/ranges contra o
// storage quando FS real; no SAF lê inteiro com guarda de orçamento).
// 0.10-M (EXT): `expandNodes` segue o MESMO setting do import (a
// reconversão honra o modo que o dono escolheu nas Definições).
bool reconvertFile(const std::string& sourceRel, ProjectStorage& st,
                   TexturePipeline* pipeline, Output& out, Stats& stats,
                   std::string& err,
                   bool (*onProgress)(void*, u64, u64) = nullptr,
                   void* user = nullptr, bool expandNodes = false);

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

// ---- 0.9.6.4 (GRUPO A/R-021) — OS IRMÃOS DO .gltf SEPARADO ------------------
// Coleta os URIs externos (buffers[].uri + images[].uri, sem data:) do JSON
// de um .gltf; copyGltfSiblings copia-os DO DIRETÓRIO ORIGINAL para
// source/<subcaminho> (URI-decode %20 incluído; streaming pelo storage — FS
// e SAF; um log por irmão; irmão AUSENTE no original = ERRO QUE NOMEIA O
// FICHEIRO). Expostos p/ as sentinelas R-021.
bool collectGltfSiblingUris(const char* json, size_t jsonLen,
                            std::vector<std::string>& uris, std::string& err);
bool copyGltfSiblings(const char* json, size_t jsonLen,
                      const std::string& srcDir, ProjectStorage& st,
                      Stats& stats, std::string& err,
                      bool (*onProgress)(void*, u64, u64) = nullptr,
                      void* user = nullptr);

// ---- 0.9.6.4 (GRUPO A/R-022) — INTEGRIDADE DA CÓPIA -------------------------
// Verifica que a cópia em `rel` é BYTE A BYTE igual à fonte `srcAbs`, em
// chunks (nunca o ficheiro inteiro em RAM quando há caminho real):
//   • FsStorage/raiz real  → ChunkReader nos DOIS lados + memcmp por chunk;
//   • SAF (content://)     → readBytes com guarda de orçamento (256 MB);
//     além do orçamento LOGA HONESTO e devolve true (a conversão lê a
//     FONTE — a verificação protege o reconvert posterior).
// false + err «cópia truncada: ...» (o chamador REMOVE a cópia — sem
// estado parcial). Exposto p/ a sentinela regress_glb_copia_verificada.
bool verifyCopyChunked(const std::string& srcAbs, ProjectStorage& st,
                       const std::string& rel, std::string& err);

} // namespace convert
} // namespace vv
