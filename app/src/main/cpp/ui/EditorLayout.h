#pragma once
// ui/EditorLayout.h — layout PURO dos painéis do editor (F4.1), sem GL —
// host-testável. Fonte ÚNICA das alturas de conteúdo que alimentam o scroll:
// o desenho (EditorUi.cpp) e os testes de CI partilham estes números.
//
// Coordenadas de CONTEÚDO: origem no topo da lista (debaixo do cabeçalho do
// painel); o scroll converte para ecrã com screenY = contentY − offset.
#include "core/Types.h"
#include <cmath>
#include "core/Scene.h"
#include "components/Transform3D.h"
#include "components/MeshRenderer.h"
#include "components/InputMap.h"
#include "components/BodyComp.h"
#include "components/TouchControls.h"

namespace vv {
namespace editor {

// constantes partilhadas pelo desenho e pela medição (antes no anon ns do .cpp)
constexpr f32 kPad       = 12.0f;
constexpr f32 kHeaderH   = 48.0f;
constexpr f32 kRowH      = 52.0f;
constexpr f32 kSliderRow = 36.0f;
constexpr f32 kMenuW     = 340.0f;

// altura do conteúdo da Hierarchy: uma linha por TIC ativo
inline f32 hierarchyContentHeight(u32 ticCount) {
    return static_cast<f32>(ticCount) * kRowH;
}

// altura do conteúdo do Inspector — DEVE espelhar a ordem e as alturas de
// drawInspector (testes do CI aferem os totais das receitas dos presets)
inline f32 inspectorContentHeight(const Tic& tic) {
    f32 cy = 0.0f;
    cy += 30.0f;                                   // nome do TIC
    if (tic.getComponent<Transform3D>()) {
        cy += 26.0f + 9.0f * kSliderRow;           // cabeçalho + 9 sliders
    }
    if (tic.getComponent<MeshRenderer>()) {
        cy += 26.0f + 26.0f;                       // mesh: origem + tex (F5-E)
    }
    if (tic.getComponent<InputMap>()) {
        cy += 26.0f;                               // input: fonte
    }
    if (tic.getComponent<BodyComp>()) {
        cy += 26.0f + kSliderRow;                  // body: tipo·forma·chão + velx
    }
    if (tic.getComponent<InputMap>()) {
        if (!tic.getComponent<TouchControls>()) {
            cy += 42.0f;                           // botão "add TouchControls"
        } else {
            cy += 26.0f;                           // label "tc: stick + jump"
        }
    }
    return cy;
}

// topo (coords de CONTEÚDO) da linha "mesh:" — o tap re-despachado do
// scroll usa isto p/ abrir o seletor de assets (F5-E); tex = topo + 26.
inline f32 inspectorMeshTop(const Tic& tic) {
    f32 cy = 30.0f;                                // nome do TIC
    if (tic.getComponent<Transform3D>()) {
        cy += 26.0f + 9.0f * kSliderRow;
    }
    return cy + 2.0f;                              // +2 = offset do botão
}

// topo do botão "add TouchControls" em coords de conteúdo (fundo = topo + 34;
// o conteúdo acaba 6 px abaixo do botão — margem)
inline f32 inspectorAddTcTop(f32 contentH) { return contentH - 40.0f; }

// Hierarchy: índice da linha sob um tap em coords de ECRÃ (com o offset do
// scroll aplicado) — -1 se fora da lista
inline i32 hierarchyRowAtTap(f32 tapY, f32 listTop, f32 offset, u32 ticCount) {
    if (ticCount == 0) {
        return -1;
    }
    // floor antes do cast: tap acima do topo (negativo) NÃO trunca para 0
    const i32 row = static_cast<i32>(std::floor((tapY - listTop + offset) / kRowH));
    if (row < 0 || static_cast<u32>(row) >= ticCount) {
        return -1;
    }
    return row;
}

} // namespace editor
} // namespace vv
