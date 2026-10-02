#pragma once
// assets/ZipExtract.h — EXTRAÇÃO DE ARCHIVES (0.8.10, passo 1 de 2).
//
// PASSO 1 — EXTRAIR (utilitário, SEM conversão): o .zip escolhido no
// navegador extrai para extracted/<nome-do-archive>/ DENTRO do projeto,
// preservando a estrutura de pastas; os ficheiros ficam CRUS
// (obj/gltf/glb/png/wav/ogg/mp3… como estavam). A extração NUNCA aciona
// importação nem conversão — o passo 2 (importar) é MANUAL, pelo fluxo de
// sempre, a partir da pasta extracted/.
//
// IMPLEMENTAÇÃO: parser PRÓPRIO de central-directory (o formato ZIP é
// estável há 30 anos: EOCD → entries → local headers) + inflate do ZLIB
// do NDK/host (método 8; método 0 = stored passa direto). Streaming por
// chunks: nunca o archive nem a entrada inteira em RAM. SEM vendoring
// (a alternativa miniz foi pesada à toa — o parser próprio são ~300
// linhas e o inflate é do sistema).
//
// SEGURANÇA (requisitos do dono):
//   • ZIP-SLIP: entradas com "../", caminho absoluto ou '\' → REJEITADAS
//     + log "archive: zip-slip rejeitado <entry>" (nunca escreve fora);
//   • BOMB-GUARD: teto TOTAL extraído (kMaxTotalOut) e por ENTRADA
//     (kMaxEntryOut) → erro LEGÍVEL "archive: bomb-guard ratio=<x>";
//   • ARCHIVES ANINHADOS: entradas .zip/.rar dentro do archive são
//     IGNORADAS + log (não se extrai recursivamente);
//   • CRC por entrada conferido no fim (mismatch = erro legível).
//
// RAR: SEM unrar vendido (licença restritiva + peso) → o browser dá o
// erro LEGÍVEL "formato não suportado ainda — usa .zip". A tabela de
// decisões vive no RELATÓRIO 0.8.10.
//
// GL-free / Android-free: FILE* + zlib + ProjectStorage — CI Linux.
#include <string>
#include <vector>

#include "core/ProjectStorage.h"

namespace vv::zip {

// chunk da leitura/decompressão streaming
constexpr size_t kZipChunk = 4ull * 1024 * 1024;
// bomb-guards: NADA de extração sem teto (zip-bomb morre com erro legível)
constexpr u64 kMaxTotalOut = 2ull * 1024 * 1024 * 1024;   // 2 GB no total
constexpr u64 kMaxEntryOut = 1ull * 1024 * 1024 * 1024;   // 1 GB por entrada

struct ExtractStats {
    u32 entries = 0;     // entradas no central directory
    u32 files = 0;       // ficheiros extraídos
    u32 dirs = 0;        // diretórios criados
    u32 rejected = 0;    // zip-slip rejeitados
    u32 nested = 0;      // archives aninhados ignorados
    u64 archiveBytes = 0;
    u64 totalOut = 0;    // bytes extraídos (p/ o rácio do bomb-guard)
    bool canceled = false;
};

// extrai srcAbs → destDir (relativo ao storage, ex. "extracted/casa").
// progresso por chunk (done/total do ARQUIVO lido); false no callback =
// cancelar. Toda a falha sai em `err` LEGÍVEL (nunca crash, nunca
// silêncio). Escreve pelo WRITE STREAM do storage (mesma regra dos 500 MB).
bool extractArchive(const std::string& srcAbs, ProjectStorage& st,
                    const std::string& destDir, ExtractStats& stats,
                    std::string& err,
                    bool (*onProgress)(void*, u64, u64) = nullptr,
                    void* user = nullptr);

// sanidade de nome de entrada (zip-slip): true se SEGURO (relativo, sem
// "..", sem absoluto, sem '\') — afervel no CI diretamente
bool entryNameSafe(const std::string& name);

} // namespace vv::zip
