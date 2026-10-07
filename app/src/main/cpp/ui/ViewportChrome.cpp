// ui/ViewportChrome.cpp — stack vertical + toolbar inferior (FASE 9).
//
// FASE 9 (G1-1): toolbar ancorada ao RETÂNGULO DA VIEWPORT (drawerH real),
// só ícones (nome só no ativo), snap = íman, "+" no canto inferior
// direito, botão de settings REMOVIDO (morto — inventário G0-4).
#include "ui/ViewportChrome.h"
#include "ui/EditorUi.h"
#include "ui/UiContext.h"
#include "render/Camera.h"

#include <cstdio>

namespace vv {
namespace editor {
namespace vpchrome {

namespace {

f32 textBaseline(UiContext& ui, const UiRect& r) {
    if (!ui.hasFont()) {
        return r.y + r.h * 0.5f;
    }
    const TextMetrics m = ui.textMetrics();
    return r.y + (r.h - m.block()) * 0.5f + m.ascent;
}

// botão do STACK (PASSO 1): alvo 40×40, DESENHO em chip 32×32 centrado;
// ícone 20; disabled = text2 40% (mesmo alvo)
bool stackButton(UiContext& ui, u64 id, const UiRect& r, icons::Icon icon,
                 bool enabled) {
    const bool pressed = ui.widgetHit(id, r.x, r.y, r.w, r.h);
    const bool held = ui.widgetActive(id);
    const f32 inset = theme::dp(4.0f);   // o desenho 32 dentro do alvo 40
    const UiRect d = {r.x + inset, r.y + inset, r.w - 2.0f * inset,
                      r.h - 2.0f * inset};
    if (held && enabled) {
        ui.panelRounded(d.x, d.y, d.w, d.h, theme::dp(theme::kRadiusCard),
                        theme::kTheme.surface2);
    } else if (enabled) {
        // repouso: chip surface com bordo (o alvo é visível — nunca "quase
        // invisível", o problema documentado da 0.8.x)
        ui.panelRounded(d.x, d.y, d.w, d.h, theme::dp(theme::kRadiusCard),
                        theme::kTheme.surface);
        ui.frameRounded(d.x, d.y, d.w, d.h, 1.0f, theme::dp(theme::kRadiusCard),
                        theme::kTheme.border);
    } else {
        ui.panelRounded(d.x, d.y, d.w, d.h, theme::dp(theme::kRadiusCard),
                        theme::kTheme.bg);
    }
    f32 col[4] = {theme::kTheme.text1[0], theme::kTheme.text1[1],
                  theme::kTheme.text1[2], 1.0f};
    if (!enabled) {
        col[0] = theme::kTheme.text2[0];
        col[1] = theme::kTheme.text2[1];
        col[2] = theme::kTheme.text2[2];
        col[3] = 0.4f;
    }
    const f32 s = theme::dp(20.0f);   // PASSO 1: ícone 20 no chip 32
    icons::drawIcon(ui, icon, r.x + (r.w - s) * 0.5f, r.y + (r.h - s) * 0.5f,
                    s, col);
    return pressed && enabled;
}

// botão da TOOLBAR INFERIOR (0.9.6.1 · G1-2): SÓ ÍCONE — os 4 botões são
// IGUAIS (PASSO 1: alvo 40, desenho 32, só ícone 20). O NOME da ferramenta
// ativa vive na LEGENDA ACIMA da barra (o "Escalar" de 48px estendia-se
// POR CIMA dos botões vizinhos — o layout só dava largura larga ao
// Selecionar e o draw pintava a palavra em QUALQUER ativo)
bool toolButton(UiContext& ui, u64 id, const UiRect& r, icons::Icon icon,
                const char* word, bool active) {
    (void)word;   // o nome vive na legenda acima da barra (draw abaixo)
    const bool pressed = ui.widgetHit(id, r.x, r.y, r.w, r.h);
    const bool held = ui.widgetActive(id);
    const bool on = active || held;
    const f32 inset = theme::dp(4.0f);   // o desenho 32 dentro do alvo 40
    const UiRect d = {r.x + inset, r.y + inset, r.w - 2.0f * inset,
                      r.h - 2.0f * inset};
    if (on) {
        ui.panelRounded(d.x, d.y, d.w, d.h, theme::dp(theme::kRadiusCard),
                        theme::kTheme.accent);
    } else {
        ui.panelRounded(d.x, d.y, d.w, d.h, theme::dp(theme::kRadiusCard),
                        theme::kTheme.surface);
        ui.frameRounded(d.x, d.y, d.w, d.h, 1.0f, theme::dp(theme::kRadiusCard),
                        theme::kTheme.border);
    }
    const f32 s = theme::dp(20.0f);   // PASSO 1: ícone 20
    const f32 col[4] = {on ? theme::kTheme.accentInk[0] : theme::kTheme.text1[0],
                        on ? theme::kTheme.accentInk[1] : theme::kTheme.text1[1],
                        on ? theme::kTheme.accentInk[2] : theme::kTheme.text1[2],
                        1.0f};
    icons::drawIcon(ui, icon, r.x + (r.w - s) * 0.5f,
                    r.y + (r.h - s) * 0.5f, s, col);
    return pressed;
}

} // namespace

Layout layout(const UiRect& view, const ChipWidths* cw) {
    Layout L;
    L.view = view;
    // 0.9.6.1 (PASSO 0 · R-018): todos os alvos daqui são dp REAL — eram px
    // crus (o dono media botões de ferramentas com 48px de altura no device)
    const f32 stackBtn = theme::dp(kStackBtn);
    const f32 stackGap = theme::dp(kStackGap);
    const f32 margin = theme::dp(8.0f);
    // GRUPO D (0.9.6.7 — A BARRA DE TOQUE CABE NO ORÇAMENTO): o chrome
    // ADAPTa-se ao rect da viewport (o device de 800dp deixava o viewport a
    // ~200dp: o stack de 5×48+4×8+8 = 280dp TRANBORDAVA o fundo por cima
    // da toolbar e o [+] caía POR CIMA dos botões de ferramenta — o
    // kViewportMinW dos divisores garante ≥320dp de largura; a ALTURA
    // adapta-se AQUI, em colunas, SEMPRE com alvos de 48dp inteiros).
    //
    // (1) o [+] : canto inferior direito SE cabe ao lado da toolbar
    // (toolbar = 5×48 + 4×8 = 272 + margens); senão sobe para o canto
    // superior direito (onde o triad esteve até à FASE 9).
    const f32 toolbarW = 5.0f * theme::dp(kToolBtn) + 4.0f * stackGap;
    const bool plusBottom = view.w >= toolbarW + margin + theme::dp(56.0f) +
                                       margin + margin;
    L.plusTopRight = !plusBottom;
    // (2) o stack: COLUNAS suficientes para a altura útil (a faixa da
    // toolbar em baixo come 56dp + 8 de folga; o topo tem 8 de margem).
    // O menor nº de colunas que caiba — 1 coluna nos ecrãs largos (o
    // layout de sempre, ZERO mudança onde cabe), 2/3 nos curtos.
    // PASSO 1 (0.9.6.14): a strip do topo (40dp — kStripH; era 48) come a
    // altura disponível do stack
    const f32 stripH = theme::dp(kStripH);
    const f32 availH =
        view.h - margin - stripH - margin - theme::dp(kBottomH) - margin;
    // 0.9.6.10: SEM a reserva do [+] — no estreito ele vive DENTRO da
    // strip do topo (o fim direito dela), não ao lado do stack
    const f32 availW = view.w - 2.0f * margin;
    u32 cols = 1;
    bool fits = false;
    for (; cols < 5; ++cols) {
        const u32 rowsPerCol = (5u + cols - 1u) / cols;   // ceil(5/cols)
        const f32 needH = static_cast<f32>(rowsPerCol) * stackBtn +
                          static_cast<f32>(rowsPerCol - 1u) * stackGap;
        const f32 needW = static_cast<f32>(cols) * stackBtn +
                          static_cast<f32>(cols - 1u) * stackGap;
        if (needH <= availH && needW <= availW) {
            fits = true;
            break;
        }
    }
    if (!fits) {
        // nem 4 colunas couberam — a ÚLTIMA tentativa: 5 colunas de 1 linha
        // SEM a reserva do [+] (com ele no canto, a reserva pode ter sobrado)
        const f32 needW = 5.0f * stackBtn + 4.0f * stackGap;
        if (stackBtn <= availH && needW <= view.w - 2.0f * margin) {
            cols = 5;
            fits = true;
        }
    }
    // DEGRADAÇÃO HONESTA: nem o mínimo coube (viewport sub-toolbar — ex. o
    // drawer comido ao device) → o stack ESCONDE (toolbar+viewport mandam;
    // os rects ficam degenerados e o draw salta)
    L.stackVisible = fits;
    L.stackCols = fits ? cols : 1;
    // ---- 0.9.6.10 (GRUPO UI · a imagem 1) · A STRIP DO TOPO DA VIEWPORT:
    // a tab [Cena] (a vista ativa — o underline âmbar como as tabs de
    // modo) + os chips [Perspetiva] (a projeção REAL da câmara do editor)
    // e [Global] (o espaço REAL do gizmo — sempre mundial, ver Gizmo.h) —
    // INFORMAÇÃO REAL em chips, na faixa de vidro de 40dp (o pai; o [+]
    // dos ecrãs estreitos vive no fim direito DELA)
    // P-08 (0.9.6.12 · GRUPO J1 · R-022): em viewport sub-piso (só em
    // testes — o cap do drawer garante kViewportMinH em produção) a strip
    // ESCONDE (degradação honesta — a toolbar manda)
    L.stripVisible = view.h >= stripH + theme::dp(kBottomH);
    L.strip = {view.x, view.y, view.w, stripH};
    {
        const f32 chipH = theme::dp(32.0f);
        const f32 cy = view.y + (stripH - chipH) * 0.5f;
        // ---- 0.9.6.12 (GRUPO J2 · R-023 · a regra §2.5 do contrato) —
        // CHIPS MEDIDOS (o wrap-content da spec J2): a largura de cada
        // chip = o TEXTO medido + 2×8dp de padding, com o piso de 56dp.
        // Era 88/112/88dp FIXOS = 312dp num viewport de piso 288dp COM o
        // [+] a viver no fim direito da strip (plusTopRight no device):
        // o chip Global transbordava e era COBERTO pelo [+] — o dono lia
        // «Glob+». A degradação (por ordem): Perspetiva esconde primeiro
        // (a projeção é a menos acionável), depois Cena; o Global é o
        // ÚLTIMO e SÓ ellipsize quando nem ele cabe (fallback honesto da
        // spec). cw=null → as larguras fixas antigas (compat dos testes)
        const f32 pad2 = 2.0f * theme::dp(8.0f);
        const f32 chipMin = theme::dp(56.0f);
        f32 wCena = theme::dp(88.0f);
        f32 wPersp = theme::dp(112.0f);
        f32 wGlobal = theme::dp(88.0f);
        bool showCena = true, showPersp = true, showGlobal = true;
        if (cw) {
            wCena = (std::max)(chipMin, cw->cena + pad2);
            wPersp = (std::max)(chipMin, cw->persp + pad2);
            wGlobal = (std::max)(chipMin, cw->global + pad2);
            // a RESERVA do [+]: nos ecrãs estreitos ele vive no fim
            // direito da strip — os chips nunca o pisam (o pai dele é
            // desenhado DEPOIS: cobria o texto — o «Glob+» do dono).
            // (o X do [+] é o MESMO nos dois casos — inferior/superior
            // direito; só o Y difere, e o X é o que a reserva precisa)
            const f32 plusRightX =
                view.x + view.w - theme::dp(56.0f) - margin;
            const f32 right =
                L.plusTopRight ? plusRightX - theme::dp(4.0f)
                               : view.x + view.w - margin;
            f32 x = view.x + margin;
            const f32 gap = theme::dp(8.0f);
            if (x + wCena + gap + wPersp + gap + wGlobal <= right) {
                // os três cabem — o layout de sempre
            } else if (x + wCena + gap + wGlobal <= right) {
                showPersp = false;   // o primeiro a ceder
            } else if (x + wGlobal <= right) {
                showPersp = false;
                showCena = false;
            } else {
                // nem o Global cabe inteiro: ele fica SOZINHO, clamped à
                // faixa, e o labelFitted ellipsiza HONESTAMENTE («Glob…»)
                showPersp = false;
                showCena = false;
                wGlobal = (std::max)(chipMin, right - x);
            }
        }
        L.stripCena = showCena
                          ? UiRect{view.x + margin, cy, wCena, chipH}
                          : UiRect{0.0f, 0.0f, 0.0f, 0.0f};
        L.stripPersp =
            showPersp
                ? UiRect{L.stripCena.x + L.stripCena.w + theme::dp(8.0f), cy,
                         wPersp, chipH}
                : UiRect{0.0f, 0.0f, 0.0f, 0.0f};
        L.stripGlobal =
            showGlobal
                ? UiRect{(showPersp ? L.stripPersp.x + L.stripPersp.w
                                    : (showCena ? L.stripCena.x +
                                                      L.stripCena.w
                                                : view.x + margin)) +
                             theme::dp(8.0f),
                         cy, wGlobal, chipH}
                : UiRect{0.0f, 0.0f, 0.0f, 0.0f};
    }
    // ---- stack (coluna-major: undo/redo/save/dup/paste, preenchendo
    // coluna a coluna — a ordem de leitura de sempre; degenerado quando
    // ESCONDIDO — o draw salta). 0.9.6.10: desce abaixo da strip e ganha
    // o PAI de vidro (o rail esquerdo da referência) ----
    const u32 rowsPerCol = (5u + L.stackCols - 1u) / L.stackCols;
    for (u32 i = 0; i < 5; ++i) {
        if (!L.stackVisible) {
            L.stack[i] = {0.0f, 0.0f, 0.0f, 0.0f};
            continue;
        }
        const u32 col = i / rowsPerCol;
        const u32 row = i % rowsPerCol;
        L.stack[i] = {view.x + margin + theme::dp(8.0f) +
                          static_cast<f32>(col) * (stackBtn + stackGap),
                      view.y + stripH + margin +
                          static_cast<f32>(row) * (stackBtn + stackGap),
                      stackBtn, stackBtn};
    }
    if (L.stackVisible) {
        // o PAI do stack: cobre as colunas com 8dp de folga (o rail)
        const f32 panelW =
            static_cast<f32>(L.stackCols) * stackBtn +
            static_cast<f32>(L.stackCols - 1u) * stackGap + theme::dp(16.0f);
        const f32 panelH =
            static_cast<f32>(rowsPerCol) * stackBtn +
            static_cast<f32>(rowsPerCol - 1u) * stackGap + theme::dp(16.0f);
        L.stackPanel = {L.stack[0].x - theme::dp(8.0f),
                        L.stack[0].y - theme::dp(8.0f), panelW, panelH};
    } else {
        L.stackPanel = {0.0f, 0.0f, 0.0f, 0.0f};
    }
    // FASE 9 (G2-10): o TRIAD foi REMOVIDO — os "pontinhos fantasma" do
    // dono (canto sup-dir do viewport, fora do mock); a orientação vive
    // no gizmo 3D e na câmara.
    // ---- toolbar inferior: SÓ ÍCONES, âncora = canto inferior ESQUERDO
    // do rect da viewport (G1-1). Uma fileira (garantida pelo
    // kViewportMinW = 320dp ≥ 272+16 da toolbar).
    // P-08 (0.9.6.12 · GRUPO J1 · R-022 · a regra §2.2 do contrato): a
    // toolbar vive DENTRO do rect da viewport e ABAIXO da strip POR
    // CONSTRUÇÃO — o cap do drawer (safe::kViewportMinH) garante o espaço
    // em produção; o clamp é a última defesa (viewport degenerado de
    // teste): nunca acima do topo da strip, nunca fora por baixo. Era ESTE
    // o caminho do defeito 1 do dono (a toolbar sobre a top bar) — agora o
    // overlap é impossível por construção.
    const f32 botH = theme::dp(kBottomH);
    const f32 toolW = theme::dp(kToolBtn);
    f32 by = view.y + view.h - botH - theme::dp(8.0f);
    const f32 byMin = view.y + (L.stripVisible ? stripH : 0.0f);
    const f32 byMax = view.y + view.h - botH;
    if (by < byMin) {
        by = byMin;
    }
    if (by > byMax) {
        by = byMax;
    }
    // a legenda (12sp acima da barra) só se NÃO cruzar a strip
    L.legendVisible = (by - theme::dp(24.0f)) >= byMin;
    // R-022 (o achado ao vivo): no C33 (content 756dp) os pisos da
    // gangorra (200+272) deixam o viewport a 284dp < kViewportMinW — a
    // fileira com margem 16dp transbordava 4dp (o Ímã saía do rect). A
    // margem esquerda Cede (16→4dp) antes de transbordar
    const f32 rowW = 5.0f * toolW + 4.0f * theme::dp(8.0f);
    f32 lead = view.w >= rowW + theme::dp(16.0f)
                   ? theme::dp(16.0f)
                   : (std::max)(theme::dp(4.0f), view.w - rowW);
    f32 bx = view.x + lead;
    L.selectBtn = {bx, by, toolW, botH};  bx += toolW + theme::dp(8.0f);
    L.moveBtn   = {bx, by, toolW, botH};  bx += toolW + theme::dp(8.0f);
    L.rotateBtn = {bx, by, toolW, botH};  bx += toolW + theme::dp(8.0f);
    L.scaleBtn  = {bx, by, toolW, botH};  bx += toolW + theme::dp(8.0f);
    L.snapBtn   = {bx, by, toolW, botH};
    // o PAI da toolbar: da legenda (12sp + 6dp acima) até AO LIMITE do
    // fundo da viewport (flush — o padding de baixo do pai É a margem).
    // P-08 (J1): sem legenda (viewport apertado) o pai começa na barra;
    // o fundo NUNCA passa o fundo do view e a LARGURA NUNCA passa a
    // direita (o achado R-022: 8+288dp transbordava 8dp o viewport de
    // 288dp por baixo do Inspector)
    {
        const f32 top = by - (L.legendVisible ? theme::dp(22.0f)
                                              : theme::dp(4.0f));
        const f32 bottom = by + botH + theme::dp(8.0f);
        const f32 bottomLim = view.y + view.h;
        const f32 left = L.selectBtn.x - theme::dp(8.0f);
        const f32 rightLim = view.x + view.w;
        const f32 right = (std::min)(left + rowW + theme::dp(16.0f),
                                     rightLim);
        L.toolPanel = {left, top, right - left,
                       (bottom > bottomLim ? bottomLim : bottom) - top};
    }
    // "+" — inferior direito se cabe; senão o canto SUPERIOR direito
    if (plusBottom) {
        L.addTicBtn = {view.x + view.w - theme::dp(56.0f) - margin, by,
                       theme::dp(56.0f), botH};
    } else {
        // 0.9.6.10: no estreito o [+] vive DENTRO DA STRIP (o fim direito
        // dela) — nunca mais colide com as colunas do stack (a colisão
        // real que o validador apanhou no device de 288dp)
        L.addTicBtn = {view.x + view.w - theme::dp(56.0f) - margin, view.y,
                       theme::dp(56.0f), botH};
    }
    // o pai do [+]: o padding dobra PARA DENTRO (o botão mantém o sítio
    // de sempre — o canto inferior direito a 8dp da borda)
    L.plusPanel = {L.addTicBtn.x - theme::dp(4.0f), L.addTicBtn.y -
                                                      theme::dp(4.0f),
                   L.addTicBtn.w + theme::dp(8.0f),
                   L.addTicBtn.h + theme::dp(8.0f)};
    return L;
}

Actions draw(UiContext& ui, EditorState& st, toolbar::GizmoModeState& gz,
             const ChromeState& cs, const Camera& camera, f32 drawerH) {
    Actions a;
    // G1-1: o rect da viewport com o drawerH REAL — a toolbar acompanha o
    // painel de baixo (aberto = sobe; fechado = desce ao fundo da viewport)
    // GRUPO D: larguras de ESTADO (divisores) — o chrome acompanha os painéis
    const UiRect vpView = safe::centerRect(
        ui.screenWidth(), ui.screenHeight(), ui.safeArea(), drawerH,
        st.showInspector, st.hierW, st.inspW);
    // 0.9.6.12 (GRUPO J2 · R-023): os chips MEDIDOS pela fonte REAL (o
    // wrap-content da spec — o Global inteiro ou degradação por ordem)
    ChipWidths cw;
    if (ui.hasFont()) {
        cw.cena = ui.fontWidth("Cena");
        cw.persp = ui.fontWidth("Perspetiva");
        cw.global = ui.fontWidth("Global");
    }
    const Layout L = layout(vpView, &cw);

    // ---- 0.9.6.10 (GRUPO UI) · OS PAIS DE VIDRO (a regra do
    // anti-exemplo: NADA flutua sobre a grelha sem painel-mãe) — os pais
    // desenham PRIMEIRO, os botões vivem POR CIMA deles ----
    {
        // a STRIP do topo: faixa de vidro com a tab [Cena] ATIVA (o
        // underline âmbar no fundo, como as tabs de modo) + os chips de
        // estado [Perspetiva]/[Global] (informação REAL, sem toggle falso)
        // — A RECEITA COMPLETA do vidro da spec G: fill surface2 + o BORDO
        // #FFFFFF1F (glassEdge) TODO À VOLTA + o highlight #FFFFFF0A no
        // topo (o vidro sobre o céu escuro TEM de se LER — o delta de
        // 8/255 do fill só era invisível; o bordo é a assinatura)
        // P-08 (J1): escondida em viewport sub-piso (a toolbar manda)
        if (L.stripVisible) {
        ui.panelRounded(L.strip.x, L.strip.y, L.strip.w, L.strip.h, 0.0f,
                        theme::kTheme.surface2);
        ui.panelRounded(L.strip.x, L.strip.y, L.strip.w, L.strip.h, 0.0f,
                        theme::kTheme.glassEdge);
        ui.panelRounded(L.strip.x, L.strip.y + theme::dp(1.0f), L.strip.w,
                        L.strip.h - theme::dp(2.0f), 0.0f,
                        theme::kTheme.surface2);
        ui.panel(L.strip.x, L.strip.y + L.strip.h - 1.0f, L.strip.w, 1.0f,
                 theme::kTheme.glassEdge);
        const TextMetrics tmS = ui.textMetrics();
        auto chipLabel = [&](const UiRect& r, const char* txt, bool active) {
            if (r.w <= 0.0f || r.h <= 0.0f) {
                return;   // escondido pela degradação (J2) — não desenha
            }
            const bool held = active;   // o chip ativo lê-se aceso
            if (active) {
                ui.panelRounded(r.x, r.y, r.w, r.h,
                                theme::dp(theme::kRadiusField),
                                theme::kTheme.accentDim);
            } else {
                ui.panelRounded(r.x, r.y, r.w, r.h,
                                theme::dp(theme::kRadiusField),
                                theme::kTheme.surface2);
            }
            (void)held;
            if (ui.hasFont()) {
                ui.labelFitted(r.x + theme::dp(8.0f),
                               r.y + (r.h - tmS.block()) * 0.5f + tmS.ascent,
                               txt,
                               active ? theme::kTheme.text1
                                      : theme::kTheme.text2,
                               r.w - theme::dp(16.0f));
            }
        };
        chipLabel(L.stripCena, "Cena", true);      // a vista ATIVA (única)
        chipLabel(L.stripPersp, "Perspetiva", false);
        chipLabel(L.stripGlobal, "Global", false);
        }   // fim da strip visível (P-08)
        // o RAIL esquerdo (o pai do stack) — a receita do vidro spec G:
        // fill surface2 + o BORDO glassEdge à volta + highlight no topo
        if (L.stackVisible) {
            ui.panelRounded(L.stackPanel.x, L.stackPanel.y, L.stackPanel.w,
                            L.stackPanel.h, theme::dp(theme::kRadiusCard),
                            theme::kTheme.surface2);
            ui.frameRounded(L.stackPanel.x, L.stackPanel.y, L.stackPanel.w,
                            L.stackPanel.h, 1.0f,
                            theme::dp(theme::kRadiusCard),
                            theme::kTheme.glassEdge);
            // o HIGHLIGHT do topo do vidro (a spec G: #FFFFFF0A)
            ui.panelRounded(L.stackPanel.x + theme::dp(2.0f),
                            L.stackPanel.y + theme::dp(1.0f),
                            L.stackPanel.w - theme::dp(4.0f),
                            theme::dp(2.0f), theme::dp(1.0f),
                            theme::kTheme.glassTop);
        }
        // o PAI da toolbar inferior (cobre a legenda) — idem
        ui.panelRounded(L.toolPanel.x, L.toolPanel.y, L.toolPanel.w,
                        L.toolPanel.h, theme::dp(theme::kRadiusCard),
                        theme::kTheme.surface2);
        ui.frameRounded(L.toolPanel.x, L.toolPanel.y, L.toolPanel.w,
                        L.toolPanel.h, 1.0f, theme::dp(theme::kRadiusCard),
                        theme::kTheme.glassEdge);
        // o PAI do [+]
        ui.panelRounded(L.plusPanel.x, L.plusPanel.y, L.plusPanel.w,
                        L.plusPanel.h, theme::dp(theme::kRadiusCard),
                        theme::kTheme.surface2);
        ui.frameRounded(L.plusPanel.x, L.plusPanel.y, L.plusPanel.w,
                        L.plusPanel.h, 1.0f, theme::dp(theme::kRadiusCard),
                        theme::kTheme.glassEdge);
    }

    // ---- stack vertical (pulado quando o layout ESCONDE — degradação) ----
    if (L.stackVisible) {
        if (stackButton(ui, kVpUndoId, L.stack[0], icons::Icon::Undo,
                        cs.canUndo)) {
            a.undoPressed = true;
        }
        if (stackButton(ui, kVpRedoId, L.stack[1], icons::Icon::Redo,
                        cs.canRedo)) {
            a.redoPressed = true;
        }
        if (stackButton(ui, kVpSaveId, L.stack[2], icons::Icon::Save, true)) {
            a.savePressed = true;
        }
        // FASE 9 (G2-11): o 4.º ícone era DUPLICATE (rect+plus — o
        // "quadrado com ponto" do dono) → agora é o COPY padrão (2
        // quadrados sobrepostos; a AÇÃO continua duplicar — só o GLIFO)
        if (stackButton(ui, kVpDupId, L.stack[3], icons::Icon::Copy, true)) {
            a.dupPressed = true;
        }
        if (stackButton(ui, kVpPasteId, L.stack[4], icons::Icon::Paste,
                        cs.canPaste)) {
            a.pastePressed = true;
        }
    }

    // ---- toolbar inferior: modos (Selecionar = SEM gizmo; gz.mode para o
    // gizmo da 0.6.9; o st.selectMode é o cursor de seleção por toque) ----
    if (toolButton(ui, kVpSelectId, L.selectBtn, icons::Icon::Cursor,
                   "Selecionar", st.selectMode)) {
        st.selectMode = true;
        gz.mode = 0;
    }
    if (toolButton(ui, toolbar::kGizmoIds[0], L.moveBtn, icons::Icon::Move,
                   "Mover", !st.selectMode && gz.mode == 0)) {
        st.selectMode = false;
        gz.mode = 0;
    }
    if (toolButton(ui, toolbar::kGizmoIds[1], L.rotateBtn, icons::Icon::Rotate,
                   "Rodar", !st.selectMode && gz.mode == 1)) {
        st.selectMode = false;
        gz.mode = 1;
    }
    if (toolButton(ui, toolbar::kGizmoIds[2], L.scaleBtn, icons::Icon::Scale,
                   "Escalar", !st.selectMode && gz.mode == 2)) {
        st.selectMode = false;
        gz.mode = 2;
    }
    // 0.9.6.1 (G1-2) · A LEGENDA: o nome da ferramenta ATIVA numa strip
    // pequena ACIMA da barra — nunca dentro do botão (nada se sobrepõe).
    // P-08 (J1): some em viewport apertado (não cruza a strip — regra §2.2)
    {
        const char* name = st.selectMode ? "Selecionar"
                           : gz.mode == 0 ? "Mover"
                           : gz.mode == 1 ? "Rodar"
                                          : "Escalar";
        const f32 legendY = L.selectBtn.y - theme::dp(6.0f) -
                            theme::dp(12.0f);   // 12sp acima do topo da barra
        if (ui.hasFont() && L.legendVisible) {
            ui.labelStyled(L.selectBtn.x + theme::dp(2.0f), legendY, name,
                           theme::kTheme.text2,
                           theme::fontScale(theme::kFontCaption), 0);
        }
    }

    // snap: BOTÃO DE ÍMAN (G2-9 no mock, aplicado com a toolbar nova) —
    // estado ativo/inativo, SEM texto (o valor segue no tooltip do gizmo)
    {
        const bool pressed =
            ui.widgetHit(kVpSnapValId, L.snapBtn.x, L.snapBtn.y, L.snapBtn.w,
                         L.snapBtn.h);
        const bool held = ui.widgetActive(kVpSnapValId);
        const bool on = gz.snap || held;
        ui.panelRounded(L.snapBtn.x, L.snapBtn.y, L.snapBtn.w, L.snapBtn.h,
                        theme::dp(theme::kRadiusCard),
                        on ? theme::kTheme.accent : theme::kTheme.surface);
        if (!on) {
            ui.frameRounded(L.snapBtn.x, L.snapBtn.y, L.snapBtn.w, L.snapBtn.h,
                            1.0f, theme::dp(theme::kRadiusCard),
                            theme::kTheme.border);
        }
        const f32 col[4] = {on ? theme::kTheme.accentInk[0]
                               : theme::kTheme.text1[0],
                            on ? theme::kTheme.accentInk[1]
                               : theme::kTheme.text1[1],
                            on ? theme::kTheme.accentInk[2]
                               : theme::kTheme.text1[2],
                            1.0f};
        icons::drawIcon(ui, icons::Icon::Snap,
                        L.snapBtn.x + (L.snapBtn.w - theme::dp(24.0f)) * 0.5f,
                        L.snapBtn.y + (L.snapBtn.h - theme::dp(24.0f)) * 0.5f,
                        theme::dp(24.0f), col);
        if (pressed) {
            gz.snap = !gz.snap;
        }
    }

    // [+] Adicionar TIC — o plus-menu de sempre, no canto inferior DIREITO
    // da viewport (G1-1)
    {
        // 0.9.6.10 (GRUPO UI · o anti-exemplo da imagem 2): o [+] era um
        // BLOCO cheio do accent (o quadrado branco cegante do device) —
        // agora é um chip de vidro (o pai) com o ícone ÂMBAR: o accent é
        // ESTADO, não repouso (a regra spec A)
        const bool pressed =
            ui.widgetHit(kVpAddTicId, L.addTicBtn.x, L.addTicBtn.y,
                         L.addTicBtn.w, L.addTicBtn.h);
        const f32 col[4] = {theme::kTheme.accent[0], theme::kTheme.accent[1],
                            theme::kTheme.accent[2], 1.0f};
        icons::drawIcon(ui, icons::Icon::Plus,
                        L.addTicBtn.x + (L.addTicBtn.w - theme::dp(24.0f)) * 0.5f,
                        L.addTicBtn.y + (L.addTicBtn.h - theme::dp(24.0f)) * 0.5f,
                        theme::dp(24.0f), col);
        if (pressed) {
            a.addTicPressed = true;
        }
    }

    // FASE 9 (G2-10): o TRIAD de orientação foi REMOVIDO (os
    // "pontinhos fantasma" do dono — fora do mock).

    return a;
}

} // namespace vpchrome
} // namespace editor
} // namespace vv
