#pragma once
// ui/Brand.h — A MARCA (0.12-D · D-4): O «G EM CHAMA» — a identidade ÚNICA
// extraída do ícone REAL do dono (docs/iconegonevv.png — o mesmo PNG do
// launcher da Y-0 e da RAMPA fire0..fire3 do Theme da D-3).
//
// O que MORREU (a ordem do D-4: «os glifos antigos das setas/mira fora;
// identidade ÚNICA no produto todo»):
//   • as 4 SETAS a N/E/S/W (com pontas no LOD completo, ticks no
//     simplificado) — o glifo «G-com-4-setas» da 0.9.6.18 SAiu; o G em
//     chama NÃO as tem (o ícone real nunca teve);
//   • a cor MONO âmbar — a cor agora é a RAMPA DO FOGO do Theme
//     (fire0 #7A1F0A na base → fire3 #FFE08A no topo), amostrada por
//     traço pela ALTURA do traço no viewBox (o que o ícone real faz: a
//     base escura, o bico amarelo);
//   • a paleta LOCAL — nenhuma cor vive aqui (R-030: a cor é do Theme;
//     o espelho Java usa os tokens FIRE do ProjectManagerActivity).
//
// AS 3 VERSÕES (a ordem do dono) — UMA função, LOD pelo tamanho (o
// contrato de sempre):
//   • GRANDE (a splash da D-6, com o ignitar animado): drawLogo com
//     `ignite` 0..1 — 0 = apagada (o mono accentDim), 1 = a chama toda;
//     a revelação corre da BASE para o TOPO (a base apanha fogo primeiro,
//     determinística — o mesmo t dá a mesma chama; a D-6 anima o t);
//   • MÉDIO (o ícone da app): os PNGs do launcher (Y-0,
//     scripts/y0_gen_icons.py) — GERADOS do mesmo desenho; o Theme ramp
//     é a MESMA família (aferrado pelo pin fogo_rampa);
//   • PEQUENO (o cabeçalho do editor): o slot do LOGO na barra de topo
//     (Toolbar.cpp) desenha drawLogo a 24dp com ignite=1.
//
// O CONTRATO (a regra da paridade, atualizado):
//   • UMA função para TODOS os sítios C++ (derivar cor/traço/função
//     duplicada é VERMELHO no gate ui_vocab_check.py);
//   • o espelho Java vive em UiIcons.drawBrand — a MESMA geometria sem
//     setas, a MESMA rampa (os tokens FIRE), aferida pelos gates dos 2
//     lados (projects_ui_check/token list);
//   • LOD pelo TAMANHO PEDIDO: ≥32dp = traço 1.0; <32dp = traço ×1.2 (a
//     perna e a barra mantêm o peso no 20-24dp);
//   • NUNCA desenha fundo (o contraste é do chamador — âmbar/fogo sobre
//     grafite, nunca fogo sobre fogo).
//
// Geometria (viewBox 0..24 — a MESMA de sempre): o G = arco r=6.2 com a
// abertura à direita + a barra horizontal até ao centro + o espelho
// vertical (o "G" do launcher).

#include "core/Types.h"

namespace vv {
class UiContext;

namespace editor {
namespace brand {

// limiar do LOD (dp pedidos): ≥32 = traço 1.0; <32 = traço ×1.2
constexpr f32 kLodFullMinDp = 32.0f;

// desenha O LOGO (o «G em chama») no rect (x, y, size, size).
// `ignite` 0..1: a fração da rampa REVELADA da base para o topo (0 =
// apagada — o mono accentDim; 1 = a chama inteira; a D-6 anima).
// NUNCA desenha fundo (a regra «fogo sobre grafite, nunca fogo sobre
// fogo» vive no chamador).
void drawLogo(UiContext& ui, f32 x, f32 y, f32 size, f32 ignite);

// o nº de segmentos que seria desenhado (afervel): 14 (arco) + 2
// (barra/espelho) — AS SETAS MORRERAM nos dois LODs (o pin
// logo_setas_mortas aferra).
u32 segmentCountFor(f32 sizeDp);

} // namespace brand
} // namespace editor
} // namespace vv
