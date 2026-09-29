#pragma once
// assets/ObjExporter.h — MeshData → texto Wavefront OBJ (F5-E/round-trip).
//
// Exporta o formato INTERNO para OBJ: v/vt/vn alinhados (1 por vértice) e
// faces por grupo (f v/vt/vn). O round-trip exportObj → parseObj devolve a
// MESMA geometria (contagens e multiconjunto de vértices/triângulos — a
// ORDEM pode diferir se os índices do mesh não forem sequenciais).
// Usado pelo Menu do editor ("export OBJ" do TIC selecionado) e pelos
// testes do CI.
#include <string>
#include "assets/Assets.h"

namespace vv {

std::string exportObj(const MeshData& mesh);

} // namespace vv
