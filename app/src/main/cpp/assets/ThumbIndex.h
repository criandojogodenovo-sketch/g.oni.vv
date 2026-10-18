#pragma once
// assets/ThumbIndex.h — o ÍNDICE das miniaturas (o lado do browser, PURO).
//
// O contrato do PASSO 5C:
//   · o import escreve thumbs/<stem>.png (256×256) para cada mesh —
//     `ensureThumb` é a MESMA rotina (o import força o render);
//   · o browser lê o PNG PRONTO (instantâneo — o pin: 100 assets = 100
//     thumbs carregadas+decodificadas em <500 ms, `loadRows`);
//   · thumb AUSENTE = placeholder + RENDER SOB DEMANDA (`ensureThumb`
//     renderiza do .gmesh pela tabela de blocos — o pico de RAM é UM
//     bloco, nunca o modelo — a máquina do PASSO 4).
//
// GL-free / Android-free: testável no CI com FakeStorage/FsStorage.
#include <string>
#include <vector>
#include "assets/Assets.h"
#include "core/ProjectStorage.h"

namespace vv {

class ThumbIndex {
public:
    // o caminho da thumb de UM asset ("assets/city.gmesh" → "thumbs/city.png")
    static std::string relFor(const std::string& assetRel);

    static const char* kDir;   // "thumbs"

    // existe a thumb do asset? (o browser usa para escolher PNG vs placeholder)
    static bool hasThumb(ProjectStorage& st, const std::string& assetRel);

    // RENDER SOB DEMANDA: falta? renderiza do .gmesh (a tabela de blocos +
    // o primeiro bloco por readBytesAt — o pico de RAM é UM bloco) e
    // escreve o PNG. O import chama com force=true (o asset mudou — o
    // render é sempre novo). false + err em falha (o browser segue com o
    // placeholder — NUNCA bloqueia a listagem).
    static bool ensureThumb(ProjectStorage& st, const std::string& assetRel,
                            bool force, std::string& err);

    // o custo do browser (o pin do dono): carrega+decodifica as thumbs de
    // N assets (o que falta entra como placeholder). ms = o tempo REAL
    // medido (a honestidade do bench: o que não pôde medir imprime -1).
    struct Rows {
        u32 loaded = 0;        // thumbs PNG lidas+decodificadas
        u32 placeholders = 0;  // assets sem thumb (o placeholder no sítio)
        double ms = -1.0;
    };
    static Rows loadRows(ProjectStorage& st,
                         const std::vector<std::string>& assetRels);

    // o stem da thumb ("assets/city_n3.gmesh" → "city_n3")
    static std::string stemOf(const std::string& assetRel);

private:
    // o mesh do PRIMEIRO bloco do .gmesh (as faixas do PASSO 4: peek 192 B
    // + tabela + o data do bloco 0 — NUNCA o modelo inteiro em RAM)
    static bool firstBlockMesh(ProjectStorage& st, const std::string& rel,
                               MeshData& out, std::string& err);
};

} // namespace vv
