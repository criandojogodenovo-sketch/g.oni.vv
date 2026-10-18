// tests/voni_refgen.cpp — O GERADOR DA REFERÊNCIA PÚBLICA (0.9.5 · METADE 2).
//
// Escreve no stdout a referência V.ONI COMPLETA, GERADA DO REGISTO CENTRAL
// (a mesma fonte que alimenta as Docs, os erros-que-ensinam, os tooltips e
// o completamento). Os ficheiros da raiz — VONI_referencia.md e
// llms-full.txt — são a SAÍDA deste gerador (commitados para as IAs que
// pesquisam "V.ONI" os encontrarem); o teste R-013 do CI regenera e AFERE
// a sincronia byte a byte (o ficheiro desatualizado = CI vermelho).
//
// Uso: ./voni_refgen > VONI_referencia.md   (e > llms-full.txt)
#include "voni/VoniRegistry.h"

#include <cstdio>

int main() {
    const std::string md = voni::reg::fullReferenceMarkdown();
    std::fwrite(md.data(), 1, md.size(), stdout);
    return 0;
}
