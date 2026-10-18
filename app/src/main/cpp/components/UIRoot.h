#pragma once
// components/UIRoot.h — marcador de RAIZ de UI (0.12-B, B-1).
//
// A UI da cena é uma ÁRVORE de TICs: um TIC raiz (com UIRoot) e um TIC
// FILHO por elemento de UI, cada filho com o componente Control (a base
// 2D partilhada — pos2D/tamanho/âncoras/margens/pivô/rotação/visível/
// opacidade + o payload por tipo). O UIRoot é um MARCADOR (sem dados):
// existe para o editor/runtime ACHarem a raiz sem convenção de nome.
//
// COMPOS (0.12-B · B-1):
//   • Uma cena com UI tem UM UIRoot (o editor cria/reusa o TIC "UI" —
//     ensureUiRoot em ui/UiEditor.cpp; a compat do serializer promove o
//     TIC hospedeiro de um UiCanvas antigo a UIRoot — SceneSerializer).
//   • Sem limite de filhos (a árvore vive nos TICs — Scene::create;
//     a ordem de slot = ordem de desenho/z-order, a MESMA da Hierarchy).
//   • O UIRoot não desenha nada por si (o pass UI desenha os FILHOS via
//     Control; a vista espelhada vive em ui/UiRuntime.h — collectUiTree).
//   • Serialização .goni: componente "UIRoot" (presença basta — o par
//     de sempre: registry factory + dispatcher no loadText).
//
// COMPAT (o prompt da B-1): o UiCanvas fica DEPRECATED — mantém-se por
// compat (o leitor converte .goni antigo em UIRoot + filhos); os elementos
// NOVOS nunca o usam.
#include "core/Component.h"

namespace vv {

class UIRoot : public Component {
public:
    // marcador — nenhum campo. A identidade/nome vive no Tic (como sempre);
    // a árvore vive nos filhos (Tic.parent).
};

} // namespace vv
