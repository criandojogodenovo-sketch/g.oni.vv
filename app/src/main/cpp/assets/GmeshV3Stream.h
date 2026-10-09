#pragma once
// assets/GmeshV3Stream.h — 0.10-M (PASSO 3): O CONVERSOR EM STREAMING E
// PARALELO (glTF/GLB → .gmesh v3).
//
// O que a spec do dono manda e ESTE ficheiro faz:
//   • lê POR PRIMITIVA do buffer já mmapado — NUNCA o modelo inteiro na
//     RAM (o pico é a maior primitiva em voo + um bloco por worker; a
//     fórmula declarada vive no relatório do PASSO 3 e no §streaming dos
//     docs do formato);
//   • AGRUPA POR MATERIAL e corta em blocos de ≤ blockVertexCap (a
//     rotina de corte é a MESMA classe do v3CutGroup: fronteira de
//     triângulo, pool ascendente, sem soldadura);
//   • MANTÉM AS TRANSFORMAÇÕES DOS NÓS: a matriz-mundo do nó é baked nos
//     floats (M·pos, R·normal — a MESMA multiplicação do merge antigo, e
//     a MESMA que a verificação reavalia bit a bit);
//   • POOL DE THREADS (nº de núcleos − 1) decodifica primitivas em
//     paralelo; os blocos appendam-se a um temporário em disco e UM
//     ASSEMBLY ordena a saída (material → nó → bloco; os bytes finais são
//     DETERMINÍSTICOS independentemente do timing das threads);
//   • SEM PERDA: posições/normais/UVs em float32 exatamente como na
//     origem (a única operação é a transformação do nó, quando não é
//     identidade); sem soldar, sem simplificar; o conjunto de triângulos
//     do ficheiro é o conjunto da fonte;
//   • VERIFICAÇÃO OBRIGATÓRIA: relê o v3 escrito e compara POR TRIÂNGULO
//     com a origem reavaliando o MESMO bake (bit a bit) — loga
//     `gmesh: v3 blocos=<n> verts=<v> tris=<t> verificado=1` ou o
//     PRIMEIRO desvio (e falha — nunca silencioso);
//   • PROGRESSO por fase (corte, verificação) com CANCELAMENTO — corre
//     na thread do import (a UI nunca bloqueia);
//   • ERROS só de armazenamento/RAM com mensagem clara — a string
//     «limite de vértices» MORREU (não existe neste caminho).
//
// FRONTEIRAS (decisões do PASSO 3, no relatório):
//   • modelo com PELE (skins) → o chamador usa o caminho de sempre (o
//     merge preserva joints/weights; o streaming não tem skin);
//   • .obj gigante → fora daqui (o OBJ ≤65535 segue o caminho vigente;
//     o OBJ sem teto está no BACKLOG como PASSO 3-b).
//
// GL-free / Android-free (mmap via fileapi::mapFile64) — testável no CI.
#include <string>
#include <vector>

#include "assets/AssetConverter.h"   // Output/Stats
#include "assets/GltfImporter.h"     // GltfModel (a assinatura abaixo)
#include "assets/GOwnFormats.h"
#include "core/Json.h"
#include "core/ProjectStorage.h"

namespace vv {

// o resultado do conversor (o relatório cita os números)
struct V3StreamResult {
    u64 blocks = 0;
    u64 verts = 0;      // a soma das pools dos blocos (o ficheiro contém
                        // isto — a duplicação entre blocos é do corte)
    u64 tris = 0;
    bool verified = false;
    std::string firstDeviation;   // vazio = verificado sem desvios
    double cutMs = 0.0;           // fase corte (paralelo)
    double assembleMs = 0.0;      // fase assembly (temporário → ficheiro)
    double verifyMs = 0.0;        // fase verificação (re-corte + compare)
};

// O POOL do corte: núcleos−1, e NUNCA 0 — um host single-core (ou com
// hardware_concurrency()==0, "desconhecido") cai para 1 worker e o corte
// corre SEQUENCIAL no próprio thread do chamador: mais lento, MAS CORRETO
// (a mutação M3 do relatório vigia: pool a 0 sem fallback = vermelho).
u32 v3StreamWorkerCount(u32 hardwareThreads);

// 0.10-M (SAF-STREAM) — A FONTE ALTERNATIVA do bin: quando não há mmap
// (o fd do provider RECUSOU o mapa — pipe/FUSE sem mmap; ou os bytes
// vieram embutidos no JSON — data: URI), cada TAREFA carrega o SEU SPAN
// por ranges (o «pread de ranges» que o dono manda preservar — a
// degradação honesta, NUNCA o legado por causa do storage). O contrato:
//   fn(user, off, len, out, err) carrega [off, off+len) DO BIN (offsets
//   desde o INÍCIO do buffer 0 — o chamador soma o binBase do GLB);
//   true = out tem exatamente len bytes.
// A higiene de RSS (MADV_DONTNEED) fica DESLIGADA neste caminho — em heap
// o madvise ZERA páginas de vizinhos (a lição F3 do PASSO 3); o pico é o
// span da tarefa em voo (a mesma fórmula do mmap, sem o drop).
struct V3RangeSource {
    bool (*fn)(void* user, u64 off, u64 len, std::vector<u8>& out,
               std::string& err) = nullptr;
    void* user = nullptr;
};

// converte as primitivas do modelo (parseGltf em modo streaming —
// primRefs SEM geometria materializada) para assets/<stem>.gmesh v3.
//   `bin`      — o buffer 0 JÁ mmapado pelo chamador (GLB: o ficheiro
//                inteiro com a base NO INÍCIO DO CHUNK BIN; .gltf: o
//                irmão .bin); os offsets dos accessors são desde aqui.
//                nullptr + `ranges` = o caminho dos RANGES (SAF-STREAM:
//                o fd recusou o mmap ou os bytes vêm do JSON — cada
//                tarefa carrega o seu span).
//   `doc`      — o documento glTF parseado (accessors/bufferViews).
//   `binDroppable` — false quando `bin` é HEAP (data: URI exportado pelo
//                parse): o MADV_DONTNEED só é legítimo em mmap de ficheiro
//                (a lição F3 — em heap zera páginas de vizinhos).
// Escreve o ficheiro final por openWriteStream/writeStreamChunk do
// storage (nunca o ficheiro inteiro em RAM). onProgress(done,total) é
// chamado por fase com o total de triângulos; devolve false = CANCELADO
// (o output parcial é removido). Falha → false + err LEGÍVEL.
bool convertGltfToV3(const GltfModel& model, const Json& doc,
                     const u8* bin, u64 binLen, u64 fileBytes,
                     const std::string& stem, ProjectStorage& st,
                     convert::Output& out, convert::Stats& stats,
                     V3StreamResult& result, std::string& err,
                     bool (*onProgress)(void*, u64, u64) = nullptr,
                     void* user = nullptr,
                     const V3RangeSource* ranges = nullptr,
                     bool binDroppable = true);

} // namespace vv
