#pragma once
// ui/Brand.h — A MARCA (0.9.6.18 · HOTFIX D6): o glifo "G com 4 setas" numa
// ÚNICA função com LOD. O dono: «Tile âmbar com "G" liso = leitura de
// placeholder; o launcher mostra o ícone real. Slot de marca = lockup:
// glifo (G com 4 setas, âmbar) SEM FUNDO sobre grafite/vidro + wordmark
// "G.One". Tiles com letra única proibidos como marca.»
//
// O CONTRATO (a regra da paridade):
//   • UMA função para TODOS os sítios C++ (top bar hoje; splash/avatars
//     quando existirem chamam ESTA — derivar cor/traço/função duplicada é
//     VERMELHO no gate ui_vocab_check.py);
//   • LOD pelo TAMANHO PEDIDO: ≥32dp = a versão COMPLETA (4 setas com
//     pontas); <32dp = a SIMPLIFICADA (G + 4 ticks, traço ~20% mais grosso
//     — a perna da letra e as pontas das setas desaparecem primeiro em
//     20-24dp; o traço grosso mantém o peso visual do launcher);
//   • COR: a cor vem do CHAMADOR (âmbar do tema) — âmbar sobre
//     grafite/transparente, NUNCA âmbar sobre âmbar (a função não desenha
//     fundo nenhum);
//   • o launcher (mipmap estático) é GERADO do mesmo desenho
//     (scripts/gen_app_icon.py — a paridade é documentada, um PNG não
//     executa C++); o espelho Java vive em UiIcons.drawBrand (a MESMA
//     geometria, LOD e contrato — aferido por jni_parity dos 2 lados).
//
// Geometria (viewBox 0..24): o G = arco r=6.2 com a abertura à direita +
// a barra horizontal até ao centro + o espelho vertical (o "G" do
// launcher); as 4 setas apontam N/E/S/W FORA do arco (raio 9.2..11.6).

#include "core/Types.h"

namespace vv {
class UiContext;

namespace editor {
namespace brand {

// limiar do LOD (dp pedidos): ≥32 = completa (setas com pontas);
// <32 = simplificada (ticks, traço ×1.2)
constexpr f32 kLodFullMinDp = 32.0f;

// desenha o glifo da marca no rect (x, y, size, size) na cor dada.
// NUNCA desenha fundo (o contraste é responsabilidade do sítio — a regra
// «âmbar sobre grafite, nunca âmbar sobre âmbar» vive no chamador).
void drawIcon(UiContext& ui, f32 x, f32 y, f32 size, const f32 color[4]);

// o nº de segmentos da versão que seria desenhada (afervel — o pin do
// LOD: a simplificada tem MENOS segmentos que a completa em qualquer size)
u32 segmentCountFor(f32 sizeDp);

} // namespace brand
} // namespace editor
} // namespace vv
