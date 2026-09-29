#pragma once
// assets/ObjImporter.h — parser Wavefront OBJ → MeshData (F5-B).
//
// ESCOPO (básico, suficiente para o pipeline do engine):
//   v / vn / vt                        — geometria
//   f com todas as formas de canto:    — a, a/b, a/b/c, a//c
//     (índices 1-based; negativos = relativos ao último)
//   faces → triângulos por FAN         — quad/polygon dividido em (0,i,i+1)
//   g / o / usemtl                     — grupos nomeados com ranges de índice
//   mtllib / s / comentários           — reconhecidos e ignorados
// DEDUP: cantos (v,vt,vn) iguais recaem no MESMO vértice de saída.
//
// LIMITES documentados: índices u16 (falha > 65535 vértices únicos, erro
// explícito); sem MTL (material fica como NOME — cor/texture vem de F8);
// winding do ficheiro é preservado (OBJs do mercado já saem CCW).
//
// GL-free: parser puro sobre texto em memória — testável no CI Linux.
#include <string>
#include "assets/Assets.h"

namespace vv {

// false se OBJ inválido (sintaxe quebrada, índice fora do range, excesso de
// vértices); `err` descreve a causa (linha quando aplicável).
bool parseObj(const char* text, size_t len, MeshData& out, std::string& err);

} // namespace vv
