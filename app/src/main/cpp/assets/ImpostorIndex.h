#pragma once
// assets/ImpostorIndex.h — o ÍNDICE dos impostores (0.11-W 3.75b, PURO).
//
// O par do ThumbIndex (PASSO 5C) para o PNG do impostor: o draw de um TIC
// além da distância precisa do PNG alfa da silhueta; ele nasce SOB DEMANDA
// (a 1.ª vez que o TIC fica longe) e vive em impostors/<stem>.png — o
// MESMO padrão do thumbs/.
//
// O ORÇAMENTO (a disciplina da casa, o MESMO contrato do thumb/R-041): a
// geração abre o .gmesh PELA TABELA (192 B + tabela — ZERO dados) e
// materializa bloco a bloco SEM cache (o materializeBlock do BlockMesh —
// o caminho do export OBJ: o pico de RAM é UM BLOCO + o acumulado), até o
// teto de triângulos do impostor (kImpostorMaxTris = o MESMO 4096 da
// thumb). NENHUMA leitura é readBytes do .gmesh inteiro.
//
// A DIFERENÇA honesta do thumb: SEM placeholder. A thumb falhada vira
// placeholder (o browser tem de mostrar ALGO); o impostor falhado NÃO
// EXISTE — o TIC segue o caminho dos blocos (a silhueta mentida seria pior
// que os blocos caros). false + err em qualquer falha.
//
// GL-free / Android-free: testável no CI com FakeStorage/FsStorage.
#include <string>
#include "assets/Assets.h"
#include "core/ProjectStorage.h"

namespace vv {

class ImpostorIndex {
public:
    // o caminho do impostor de UM asset ("assets/city.gmesh" →
    // "impostors/city.png")
    static std::string relFor(const std::string& assetRel);

    static const char* kDir;   // "impostors"

    // existe o impostor do asset? (o draw pergunta antes de gerar)
    static bool hasImpostor(ProjectStorage& st, const std::string& assetRel);

    // RENDER SOB DEMANDA: falta? renderiza do .gmesh PELA TABELA (os
    // blocos materializados UM a um até kImpostorMaxTris — o pico de RAM
    // é UM bloco) e escreve o PNG RGBA. false + err em falha (o draw
    // segue o caminho dos blocos — NUNCA bloqueia nem mente). force=true
    // re-renderiza (o asset mudou).
    static bool ensureImpostor(ProjectStorage& st,
                               const std::string& assetRel, bool force,
                               std::string& err);
};

} // namespace vv
