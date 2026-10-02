#pragma once
// assets/ObjImporter.h — parser Wavefront OBJ → MeshData (F5-B; 0.8.10: STREAMING).
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
// 0.8.10 — STREAMING: o ObjStreamParser consome LINHA-A-LINHA (nunca o
// ficheiro inteiro em RAM — o import alimenta-o por chunks de 4-8 MB com
// um assembler de linhas). parseObj(texto) fica como WRAPPER do mesmo
// código (caminho único, testes antigos intactos).
//
// LIMITES documentados: índices u16 (falha > 65535 vértices ÚNICOS usados,
// erro explícito); sem MTL (material fica como NOME); winding do ficheiro
// é preservado (OBJs do mercado já saem CCW).
//
// GL-free: parser puro sobre linhas — testável no CI Linux.
#include <string>
#include <vector>
#include "assets/Assets.h"

namespace vv {

// 0.8.10 — parser OBJ STREAMING: feedLine() por linha (SEM '\n'/'\r' —
// o assembler do chamador trata), finish() no fim. A RAM de pico são os
// arrays crus + o MeshData (nunca o ficheiro inteiro).
class ObjStreamParser {
public:
    // uma linha do OBJ (pode ser parcial de chunk — o chamador junta).
    // false = erro de sintaxe (err preenchido; a mensagem tem a linha)
    bool feedLine(const char* s, size_t len, std::string& err);

    // fecha o último grupo e valida (vértices/índices não vazios)
    bool finish(MeshData& out, std::string& err);

    u64 lines() const { return lines_; }

private:
    std::vector<Vec3> positions_;
    std::vector<Vec3> normals_;
    std::vector<Vec2> uvs_;
    std::vector<Vertex> vertices_;   // dedup de cantos
    std::vector<u16> indices_;
    std::vector<MeshData::Group> groups_;
    std::string curGroup_;
    std::string curMaterial_;
    bool groupOpen_ = false;
    u64 lines_ = 0;
    // hash de cantos (implementação no .cpp — membro opaco)
    struct Impl;
    Impl* impl_ = nullptr;
    bool ensureImpl();
    void destroyImpl();
public:
    ObjStreamParser();
    ~ObjStreamParser();
    ObjStreamParser(const ObjStreamParser&) = delete;
    ObjStreamParser& operator=(const ObjStreamParser&) = delete;
};

// false se OBJ inválido (sintaxe quebrada, índice fora do range, excesso de
// vértices); `err` descreve a causa (linha quando aplicável).
// 0.8.10: WRAPPER — alimenta o ObjStreamParser linha-a-linha (o MESMO
// caminho do import streaming; zero divergência).
bool parseObj(const char* text, size_t len, MeshData& out, std::string& err);

} // namespace vv
