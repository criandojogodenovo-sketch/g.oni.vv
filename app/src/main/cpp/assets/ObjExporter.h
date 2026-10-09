#pragma once
// assets/ObjExporter.h — MeshData → texto Wavefront OBJ (F5-E/round-trip).
//
// Exporta o formato INTERNO para OBJ: v/vt/vn alinhados (1 por vértice) e
// faces por grupo (f v/vt/vn). O round-trip exportObj → parseObj devolve a
// MESMA geometria (contagens e multiconjunto de vértices/triângulos — a
// ORDEM pode diferir se os índices do mesh não forem sequenciais).
// Usado pelo Menu do editor ("export OBJ" do TIC selecionado) e pelos
// testes do CI.
//
// 0.10-M (PASSO 4) — O STREAM POR BLOCOS: o export de um mesh DE BLOCOS
// (acima dos 65 535 verts do mesh único) anda pela TABELA — um bloco de
// cada vez materializado, numeração GLOBAL de v/vt/vn (baseVertex cresce
// por bloco), pico de RAM = 1 bloco (o caminho inteiro nunca existe).
#include <string>
#include <cstdint>
#include "assets/Assets.h"

namespace vv {

std::string exportObj(const MeshData& mesh);

// o acumulador do stream por blocos (PASSO 4): o chamador materializa um
// bloco de cada vez (BlockMesh::materializeBlock) e faz append; a
// numeração é GLOBAL (1-based, o OBJ manda) e o resultado é UM ficheiro
// OBJ válido com um grupo por bloco.
struct ObjStreamBlock {
    std::string out;      // o texto acumulado
    u64 baseVertex = 0;   // o índice GLOBAL do próximo vértice (-1 base)
    u32 blocks = 0;       // blocos appended
    u64 verts = 0;        // total de vértices escritos
    u64 tris = 0;         // total de triângulos escritos
};

// cabeçalho (1× por export)
void objStreamBegin(ObjStreamBlock& s, const std::string& name);

// append de UM bloco materializado (groupName = "bloco N"; material = o
// nome da tabela de materiais do .gmesh). Escreve v/vt/vn do bloco e as
// faces com a numeração global; atualiza baseVertex/verts/tris/blocks.
void objStreamAppend(ObjStreamBlock& s, const MeshData& block,
                     const std::string& groupName,
                     const std::string& material);

} // namespace vv
