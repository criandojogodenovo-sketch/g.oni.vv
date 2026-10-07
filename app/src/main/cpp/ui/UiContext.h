#pragma once
// ui/UiContext.h — UI immediate-mode em C++ (tema mono, landscape).
// Fluxo: beginFrame → widgets (panel/label/button) → endFrame (submete batches).
// Layout F1: toolbar topo (exatamente 3 botões) + viewport + status line inferior.
// F4.1: primitivo de scroll — beginScroll(id, region, contentHeight) /
// endScroll() com drag-to-scroll, clamp e indicador (matemática em
// ui/ScrollMath.h, GL-free e testada no CI); quads desenhados dentro da
// região são RECORTADOS por interseção (sem glScissor — quad batch único).
// F4.2: safe-area — o main lê android_app->contentRect e injeta os Insets
// (setSafeArea); toolbarRect/statusRect/viewportRect passam a viver DENTRO
// do contentRect (matemática em ui/SafeArea.h, GL-free e testada) — com a
// altura real dos painéis o overflow do Inspector é detetado e o scroll
// ativa no C33 (B1).
#include "render/QuadBatch.h"
#include "render/Renderer.h"
#include "render/Texture.h"   // 0.7.0: quads texturados da UI criável
#include "ui/FontAtlas.h"
#include "ui/LayoutDump.h"   // 0.9.6.5 (GRUPO B): o registo do layout auditado
#include "ui/ScrollMath.h"
#include "ui/SafeArea.h"
#include "ui/TextFit.h"
#include "ui/Theme.h"     // 0.9.0: design system (tabela obrigatória spec A)
#include "platform/InputState.h"
#include <cmath>
#include <string>
#include <vector>

namespace vv {

// Tokens do tema — 0.9.6.10 (GRUPO UI): ALIASES da tabela única
// (ui/Theme.h, spec G grafite+âmbar+vidro). A pele inteira da app muda num
// só sítio: BG=bg #0E0E10 α1, PANEL=surface #161618 α0.80 (VIDRO 80%),
// LINE=border #2E2E32 α0.55, TEXT=text1 #ECECEE, ACCENT=accent #FFB020
// (O ÂMBAR — a identidade spec G), WARN=warn #FF8A3D (laranja ≠ âmbar).
// O struct completo com text2/accentInk/accentDim/danger/ok/scrim/
// glassEdge/glassTop/eixos + a auditoria do vidro (blendOver/
// contrastOnGlass) vive em Theme.h. OS ALPHAS VÊM CONOSCO (o pass UI
// desenha com blend desde a F1).
namespace theme {
// (arrays C++ não se copiam em constexpr — MESMAS expressões do Theme.h para
// igualdade bit-a-bit, LIGADAS por static_assert: mudar Theme.h sem aqui =
// build MORRE — a fonte única continua sendo Theme.h)
constexpr f32 BG[4]     = {14.0f / 255.0f, 14.0f / 255.0f, 16.0f / 255.0f, 1.0f};       // bg #0E0E10
constexpr f32 PANEL[4]  = {22.0f / 255.0f, 22.0f / 255.0f, 24.0f / 255.0f, 0.80f};    // surface #161618 α0.80
constexpr f32 LINE[4]   = {46.0f / 255.0f, 46.0f / 255.0f, 50.0f / 255.0f, 0.55f};    // border #2E2E32 α0.55
constexpr f32 TEXT[4]   = {236.0f / 255.0f, 236.0f / 255.0f, 238.0f / 255.0f, 1.0f};  // text1 #ECECEE
constexpr f32 ACCENT[4] = {255.0f / 255.0f, 176.0f / 255.0f, 32.0f / 255.0f, 1.0f};  // = accent (O ÂMBAR)
constexpr f32 WARN[4]   = {255.0f / 255.0f, 138.0f / 255.0f, 61.0f / 255.0f, 1.0f};   // warn #FF8A3D (laranja)
static_assert(BG[0] == theme::kTheme.bg[0] && BG[2] == theme::kTheme.bg[2],
              "BG desincronizado de Theme.h — fonte única violada");
static_assert(PANEL[1] == theme::kTheme.surface[1] &&
                  PANEL[2] == theme::kTheme.surface[2] &&
                  PANEL[3] == theme::kTheme.surface[3],
              "PANEL desincronizado de Theme.h — fonte única violada");
static_assert(LINE[0] == theme::kTheme.border[0] && LINE[1] == theme::kTheme.border[1],
              "LINE desincronizado de Theme.h — fonte única violada");
static_assert(TEXT[0] == theme::kTheme.text1[0] && TEXT[2] == theme::kTheme.text1[2],
              "TEXT desincronizado de Theme.h — fonte única violada");
static_assert(WARN[1] == theme::kTheme.warn[1] && WARN[2] == theme::kTheme.warn[2],
              "WARN desincronizado de Theme.h — fonte única violada");
static_assert(ACCENT[0] == theme::kTheme.accent[0] &&
                  ACCENT[1] == theme::kTheme.accent[1] &&
                  ACCENT[2] == theme::kTheme.accent[2] &&
                  ACCENT[3] == theme::kTheme.accent[3],
              "ACCENT desincronizado de Theme.h — fonte única violada");
}

// UiRect vive em ui/ScrollMath.h (matemática GL-free partilhada)

class UiContext {
public:
    void init();   // reserva capacidade dos batches
    void setFont(FontAtlas* font) { font_ = font; }

    void beginFrame(Renderer* renderer, const InputState* input, f32 screenW, f32 screenH);
    void endFrame();   // submete solids + glyphs ao renderer

    // 0.7.4 — ESCALA DE TEXTO do contexto (paridade editor↔Play): o
    // viewport 2D do editor desenha o canvas ESCALADO (scale-to-fit);
    // sem isto os glifos ficavam a 28 px CRUS sobre rects a 0.4× — texto
    // proporcionalmente maior no editor que no Play (truncagens e
    // centralizações diferentes: o caso do Menu no C33). Com a escala
    // ativa, fontWidth/fontHeight/textMetrics e a EMISSÃO de glifos são
    // todos multiplicados — o mini-canvas fica EXATAMENTE o Play reduzido.
    // Define-se SÓ à volta do desenho do canvas no viewport 2D (RAII
    // abaixo); todo o resto do editor fica a 1.0.
    void setTextScale(f32 s) { textScale_ = (s > 0.05f ? s : 1.0f); }
    f32  textScale() const { return textScale_; }

    // widgets
    void panel(f32 x, f32 y, f32 w, f32 h, const f32 color[4]);
    // 0.9.0 — CANTOS CURVOS (spec A: raios 8dp cards/botões, 4dp campos/
    // chips): painel com cantos arredondados por ESCADARIA de quads (4 degraus
    // por canto — zero shaders, zero blur, zero custo exponencial; afervável
    // nos batches como o panel de sempre). radius é CLAMPADO a min(w,h)/2.
    // Os degraus cobrem o círculo por DENTRO (nunca sangram para fora do rect
    // — a auditoria de rects 0.9.0 conta o bounding box, que fica EXATO).
    void panelRounded(f32 x, f32 y, f32 w, f32 h, f32 radius,
                      const f32 color[4]);
    // 0.9.0 — moldura com cantos curvos: 4 arestas retas (panel) + 4 arcos
    // (polilinhas no LINE batch com a MESMA espessura do traço). Para cards
    // e botões outline do design system.
    void frameRounded(f32 x, f32 y, f32 w, f32 h, f32 t, f32 radius,
                      const f32 color[4]);
    // 0.9.0 — chip pill (raio = h/2): o campo/chip de 4dp quando h≤16,
    // cápsula completa quando pedido explicitamente.
    void panelPill(f32 x, f32 y, f32 w, f32 h, const f32 color[4]);
    // 0.8.6 — TIPOGRAFIA por elemento: escala (×28 px base) + estilo
    // (0 normal, 1 negrito, 2 itálico). O label() de sempre = styled a 1.0/0.
    void labelStyled(f32 xBaseline, f32 yBaseline, const char* text,
                     const f32 color[4], f32 fontScale, u8 style);
    // 0.8.6 — ROTAÇÃO por elemento: os quads emitidos ENTRE set/clear giram
    // à volta de (cx,cy) por rad radianos (o clip permanece axis-aligned;
    // desligar SEMPRE no fim do elemento). Aninhamento: não — um nível só.
    void setQuadXform(f32 cx, f32 cy, f32 rad) {
        xformCx_ = cx;
        xformCy_ = cy;
        xformSin_ = std::sin(rad);
        xformCos_ = std::cos(rad);
        xformActive_ = true;
    }
    void clearQuadXform() { xformActive_ = false; }
    // 0.6.9 — segmento de ecrã com espessura (gizmos 3D projetados). Sem
    // clip (emitido FORA das regiões de scroll; o gizmo vive no viewport).
    void drawLine(f32 x0, f32 y0, f32 x1, f32 y1, f32 thickness,
                  const f32 color[4]) {
        const u32 fv = solids_.vertexCount();
        solids_.line(x0, y0, x1, y1, thickness,
                     color[0], color[1], color[2], color[3]);
        if (solids_.vertexCount() > fv) {
            recordRun(solids_, 0u, fv, solids_.vertexCount() - fv);
        }
    }
    void frame(f32 x, f32 y, f32 w, f32 h, f32 thickness, const f32 color[4]);
    // 0.9.6.18 (HOTFIX D12) — quad PREENCHIDO com cantos explícitos (o
    // gizmo preenche os planos XY/XZ/YZ a ~25% alfa por baixo do contorno).
    // 6 vértices = 2 triângulos na convenção do QuadBatch::quadCorners
    // (p0/p3 = 1º canto, p1 = 2.º, p2/p4 = 3.º, p5 = 4.º — o fan p0..p3 na
    // ordem do perímetro). SEM clip (como drawLine — o gizmo vive fora de
    // regiões de scroll). Degenerado não emite (o guard é do batch).
    void quadCornersFilled(const f32 px[6], const f32 py[6],
                           const f32 color[4]) {
        const u32 fv = solids_.vertexCount();
        solids_.quadCorners(px, py, 0.0f, 0.0f, 1.0f, 1.0f,
                            color[0], color[1], color[2], color[3]);
        if (solids_.vertexCount() > fv) {
            recordRun(solids_, 0u, fv, solids_.vertexCount() - fv);
        }
    }
    void label(f32 xBaseline, f32 yBaseline, const char* text, const f32 color[4]);
    // F4.2/B2: label que NUNCA excede maxW — mede e trunca com "..." se
    // precisar (ui/TextFit.h). Todo texto dentro de painéis usa isto.
    void labelFitted(f32 xBaseline, f32 yBaseline, const char* text,
                     const f32 color[4], f32 maxW);
    // 0.9.6.6 (GRUPO C): o FIT com TIPOGRAFIA — o MESMO contrato (nunca
    // excede maxW; trunca com «…») para textos que não são CORPO: a
    // legenda 12sp da status bar. A medida e o draw partilham a MESMA
    // escala (fontScale × textK — o choke point); o labelFitted de
    // sempre é o caso corpo (delega AQUI com k relativo 1.0).
    void labelFittedStyled(f32 xBaseline, f32 yBaseline, const char* text,
                           const f32 color[4], f32 maxW, f32 fontScale,
                           u8 style);
    bool button(u64 id, f32 x, f32 y, f32 w, f32 h, const char* text);

    // 0.9.6.8 (GRUPO E) — A TECLA COMPACTA da barra de símbolos: o MESMO
    // button() com a entrada de audit marcada `compact` (o piso do toque é
    // o 40dp da spec E em vez do 48dp da casa — o validador e a sentinela
    // R-027 vigiam o piso compacto: 39dp continua a FALHAR)
    bool buttonCompact(u64 id, f32 x, f32 y, f32 w, f32 h,
                       const char* text);

    // PASSO 1 (0.9.6.14) — O PISO DE LINHA da PRÓXIMA entrada interativa:
    // os elementos DE LINHA (top bar 36 / campos e tabs de baixo 32 /
    // cabeçalhos 28) têm o piso da ALTURA DA LINHA da spec — NÃO o 40 do
    // botão solto. O desenhista chama auditRowFloorNext(layout::kRowFloorDp)
    // ANTES do widgetHit/button; a flag vive SÓ até à 1ª entrada que a
    // apanha (o padrão buttonCompact — nunca escapa). As constantes são
    // as nomeadas do LayoutDump.h — números frouxos NÃO passam.
    void auditRowFloorNext(f32 floorDp) { auditRowFloorNext_ = floorDp; }
    // PASSO 2 (0.9.6.15): o corte do fit É O DESENHO (nomes de linha com
    // tip — spec B); o validador não avisa TextoTruncado nessa entrada.
    // O padrão da casa: flag consumida pela 1ª label registada, nunca escapa
    void auditFitByDesignNext() { auditFitByDesign_ = true; }

    // 0.7.6 — CAPTURA DE GESTO sem desenho (a toolbar desenha os próprios
    // botões: ícones/segmented da ui/Toolbar). MESMA semântica do button():
    // press edge dentro do rect captura active_; release dentro = clique.
    // widgetActive diz se o widget tem o dedo (visual de "held").
    bool widgetHit(u64 id, f32 x, f32 y, f32 w, f32 h);
    bool widgetActive(u64 id) const { return active_ == id; }

    // GRUPO D (0.9.6.7) — PEGA DE ARRASTO (divisores dos painéis): captura
    // o gesto como widgetHit (active_), MAS sem registo no audit de layout
    // — não é um alvo de TAP (a pega do drawer de 12dp é o precedente da
    // spec E; o alvo 48dp da casa é para tap). Devolve true enquanto o
    // dedo segura a pega.
    bool dragHandle(u64 id, f32 x, f32 y, f32 w, f32 h);

    // FASE 9 (G2-7): o dedo (slot 0) está EM CIMA deste rect AGORA?
    // Leitura PURA do input (sem claim, sem gesto) — para o long-press do
    // nome da hierarquia (o contador de frames vive no EditorState).
    bool pointerDownAt(f32 x, f32 y, f32 w, f32 h) const {
        if (!input_ || !input_->down(0)) {
            return false;
        }
        f32 px = -1.0f, py = -1.0f;
        input_->pos(0, px, py);
        return px >= x && px < x + w && py >= y && py < y + h;
    }

    // F3: slider horizontal immediate-mode (Inspector do Transform3D).
    // Escreve em `value` (clamp [minV,maxV]); devolve true se mudou este
    // frame. Partilha o mesmo active_ dos botões — um widget interativo por
    // gesto.
    bool slider(u64 id, f32 x, f32 y, f32 w, f32 h, f32 minV, f32 maxV, f32& value);

    // 0.9.6.5 (GRUPO B): o corpo do slider (o slider() público carrega o
    // registo do layout à volta dele)
    bool sliderCore(u64 id, f32 x, f32 y, f32 w, f32 h, f32 minV, f32 maxV,
                    f32& value);

    // F4.1: região de scroll reutilizável (immediate-mode; estado por id em
    // slots fixos — o offset persiste entre frames). Entre begin/end, os quads
    // de panel/label são recortados à região e os BOTÕES só desenham (o tap é
    // re-despachado pelo painel via scrollTap — drag em qualquer sítio =
    // scroll; sliders mantêm a prioridade de captura).
    void beginScroll(u64 id, const UiRect& region, f32 contentHeight);
    void endScroll();
    f32  scrollOffset() const;   // offset da região aberta (após beginScroll)
    // F5.0-fix: tap re-despachado POR ID — o tap nasce da região `id` e só
    // essa o consome (antes era global: a Hierarchy, desenhada primeiro,
    // comia o tap do Inspector e os botões do painel direito morriam)
    bool scrollTap(u64 id, f32& x, f32& y);

    // F5.2: define o offset de UMA região por id (viewer de logs salta para
    // o fundo ao abrir). O valor é clampado no próximo beginScroll da região.
    void scrollSetOffset(u64 id, f32 offset);

    // ---- 0.9.6.5 (GRUPO B · FERRAMENTAS DE VERIFICAÇÃO): AUDITORIA DE LAYOUT
    // Liga o REGISTO do que este frame desenha: cada widget acrescenta a
    // SUA entrada (o rect REAL — o JSON exportado sai do MESMO código que
    // desenha, não pode divergir; a lição R-020). O main liga no frame em
    // que há export/auditoria pendentes (Diagnóstico ou harness); o custo
    // com o audit DESLIGADO é um `if` por widget.
    void auditBegin(const char* screenName, f32 screenW, f32 screenH,
                    f32 insetT, f32 insetB, f32 insetL, f32 insetR,
                    f32 density);
    void auditEnd();                                   // desliga (Record fica)
    const layout::Record& auditRecord() const { return audit_; }
    bool auditing() const { return auditing_; }

    // hooks de TESTE (CI): leitura dos batches emitidos no frame — permitem
    // aos testes de hospedeiro aferir a geometria REAL desenhada pelos
    // painéis (linhas sequenciais, sem sobreposição, scroll a revelar o fundo)
    const QuadBatch& solidsForTest() const { return solids_; }
    const QuadBatch& glyphsForTest() const { return glyphs_; }
    // 0.7.0: batch de IMAGENS da UI criável (quads texturados — o elemento
    // Image). Até 4 texturas distintas por frame; ordem de submissão:
    // solids → imagens (ordem de 1ª utilização) → glifos.
    const QuadBatch& imagesForTest() const { return images_[0]; }
    u32 imageBatchCountForTest() const { return imageCount_; }

    // 0.7.0: resolver de ref relativa → Texture (o main liga ao GpuAssets;
    // null/sem textura → imageQuad devolve false e o chamador desenha o
    // PLACEHOLDER mono — moldura + diagonais)
    void setImageResolver(const Texture* (*resolve)(const std::string&)) {
        imgResolve_ = resolve;
    }
    // emite um quad TEXTURADO com a textura resolvida da ref (uv 0..1,
    // cor = tint). false = sem resolver/textura (placeholder no chamador).
    bool imageQuad(f32 x, f32 y, f32 w, f32 h, const std::string& ref,
                   const f32 color[4]);

    // 0.7.4 — SUBMISSÃO EM ORDEM (z correto sólidos↔texturas). Antes os
    // batches eram submetidos em GRUPOS FIXOS (solids→imagens→glifos):
    // um sólido desenhado DEPOIS de uma textura ficava POR BAIXO dela na
    // tela (botão sobre painel texturizado desaparecia). Agora cada emissão
    // regista um RUN (batch + textura + range de vértices) na ordem real;
    // o endFrame submete os runs SEQUENCIALMENTE. Os batches ACUMULAM como
    // sempre (solids_ íntegro p/ os testes); só a SUBMISSÃO é segmentada.
    // Glifos continuam por último (texto sempre legível — decisão do tema).
    u32 runCountForTest() const { return static_cast<u32>(runs_.size()); }
    u32 runTexForTest(u32 i) const {   // 0 = sólidos (textura branca)
        return i < runs_.size() ? runs_[i].tex : 0xFFFFFFFFu;
    }
    u32 runFirstVertexForTest(u32 i) const {
        return i < runs_.size() ? runs_[i].firstVertex : 0;
    }
    u32 runVertexCountForTest(u32 i) const {
        return i < runs_.size() ? runs_[i].vertexCount : 0;
    }

    // 0.7.4 — RECORTES fora de regiões de scroll: o viewport 2D do editor
    // desenha o mini-ecrã e NADA pode sangrar para os painéis ao lado (o
    // Play recorta na borda física do ecrã; o editor recorta aqui —
    // paridade). Também usado pelos CONTAINERS (filhos nunca saem do pai).
    // pushClip intersecta com o clip corrente; o destrutor repõe.
    class ScopedClip {
    public:
        ScopedClip(UiContext& ui, const UiRect& r) : ui_(ui) {
            saved_ = ui.clip_;
            ui_.clip_ = scroll::intersectRects(ui_.clip_, r);
        }
        ~ScopedClip() { ui_.clip_ = saved_; }
        ScopedClip(const ScopedClip&) = delete;
        ScopedClip& operator=(const ScopedClip&) = delete;
    private:
        UiContext& ui_;
        UiRect     saved_;
    };

    // 0.7.4 — RAII da escala de texto (o viewport 2D põe/tira à volta do
    // desenho do canvas; exceção-safe)
    class ScopedTextScale {
    public:
        ScopedTextScale(UiContext& ui, f32 scale) : ui_(ui) {
            saved_ = ui_.textScale_;
            ui_.textScale_ = scale > 0.05f ? scale : 1.0f;
        }
        ~ScopedTextScale() { ui_.textScale_ = saved_; }
        ScopedTextScale(const ScopedTextScale&) = delete;
        ScopedTextScale& operator=(const ScopedTextScale&) = delete;
    private:
        UiContext& ui_;
        f32        saved_;
    };
    // offset PERSISTENTE de uma região por id (fora do begin/end — o
    // scrollOffset() só vale com a região aberta; os testes leem depois)
    f32 scrollOffsetForTest(u64 id) const {
        for (u32 i = 0; i < kMaxScrollSlots; ++i) {
            if (scrollSlots_[i].used && scrollSlots_[i].id == id) {
                return scrollSlots_[i].st.offset;
            }
        }
        return 0.0f;
    }

    // F3: accessors usados pelos painéis do editor (EditorUi).
    // 0.7.4: TODOS escalam por textScale_ (a 1.0 = comportamento antigo).
    // 0.9.6.6 (GRUPO C): e por theme::textK() — a DENSIDADE do texto entra
    // AQUI (o choke point), nunca por casa do chamador: a 2.0 o atlas é o
    // corpo (k=1, o device de sempre); a 1.0 o corpo é 14px (k=0,5 — o
    // ecrã deixa de ter texto 2× desproporcional; a invariância da
    // escala, ver Theme.h). Quem mede com o ATLAS CRU (font_->widthOf)
    // DIVERGE do que desenha — o bug do button() corrigido neste grupo.
    bool hasFont() const { return font_ && font_->ok(); }
    f32  fontWidth(const char* text) const {
        return font_ ? font_->widthOf(text) * textScale_ * theme::textK()
                    : 0.0f;
    }
    f32  fontHeight() const {
        return font_ ? font_->height() * textScale_ * theme::textK() : 0.0f;
    }
    // F5.0-fix: métricas verticais REAIS da fonte assada (fallback 28 px sem
    // fonte — o caso dos testes de hospedeiro sem atlas). O layout deriva
    // destes números as alturas de linha — nunca mais de constantes cegas.
    // (as métricas voltam JÁ na escala do texto corrente — textK incluído)
    TextMetrics textMetrics() const {
        const TextMetrics m = hasFont()
                                  ? TextMetrics{font_->ascent(),
                                                font_->descent()}
                                  : TextMetrics{};
        const f32 k = textScale_ * theme::textK();
        return TextMetrics{m.ascent * k, m.descent * k};
    }
    f32  screenWidth() const { return sw_; }
    f32  screenHeight() const { return sh_; }

    // layout landscape F1 (0.7.6: a toolbar em si vive em ui/Toolbar.h —
    // 5 grupos com ícones; aqui ficam só as primitivas e os rects)
    void statusLine(const char* text); // fps + contagem de TICs

    // F4.2: safe-area do sistema (nav/status bar). Insets default = 0 →
    // comportamento antigo (superfície inteira). O main injeta os Insets
    // derivados de android_app->contentRect; TODOS os rects abaixo (e os
    // painéis do EditorUi, que leem safeArea()) ficam dentro do contentRect.
    void setSafeArea(const safe::Insets& in) { safe_ = in; }
    const safe::Insets& safeArea() const { return safe_; }
    f32 safeLeft()   const { return safe_.left; }
    f32 safeTop()    const { return safe_.top; }
    f32 safeRight()  const { return safe_.right; }
    f32 safeBottom() const { return safe_.bottom; }
    // GRUPO D: a largura do CONTENT RECT (o «content width» que o clamp dos
    // divisores usa — a garantia do orçamento do viewport 3D)
    f32 contentWidthPx()  const { return sw_ - safe_.left - safe_.right; }
    f32 contentHeightPx() const { return sh_ - safe_.top - safe_.bottom; }

    // rects do layout — delegam em safe::* (fonte única, testada no CI)
    UiRect toolbarRect() const {
        return safe::toolbarRect(sw_, sh_, safe_);
    }
    UiRect statusRect() const {
        return safe::statusRect(sw_, sh_, safe_);
    }
    UiRect viewportRect() const {
        return safe::viewportRect(sw_, sh_, safe_);
    }

    // re-export: as constantes agora vivem em ui/SafeArea.h (fonte única)
    static constexpr f32 kToolbarH = safe::kToolbarH;
    static constexpr f32 kStatusH  = safe::kStatusH;

private:
    // emite um quad recortado pelo clip_ (panel/label passam por aqui)
    bool emitTo(QuadBatch& b, f32 x, f32 y, f32 w, f32 h,
                f32 u0, f32 v0, f32 u1, f32 v1, const f32 color[4]);
    // 0.8.6: glifo com CANTOS EXPLÍCITOS (itálico) — o clip é aproximado
    // pelo bounding rect do glifo e os cantos seguem a MESMA xform de rotação
    void emitGlyphCorners(QuadBatch& b, const f32 px[6], const f32 py[6],
                          f32 u0, f32 v0, f32 u1, f32 v1, const f32 color[4]);
    // 0.7.4: regista um RUN de submissão (ver runs_ abaixo). `tex` = 0 p/
    // sólidos (textura branca); `b` decide o batch (solids_ ou images_[i]).
    // Extende o run corrente quando é do MESMO batch+textura (batching
    // natural); caso contratório abre um run novo.
    void recordRun(QuadBatch& b, u32 tex, u32 firstVertex, u32 vertexCount);

    Renderer*         renderer_ = nullptr;
    const InputState* input_ = nullptr;
    FontAtlas*        font_ = nullptr;
    f32               sw_ = 0.0f;
    f32               sh_ = 0.0f;
    f32               textScale_ = 1.0f;   // 0.7.4: viewport 2D (paridade)
    safe::Insets      safe_{};       // F4.2: insetos do sistema (default 0)
    u64               active_ = 0;   // botão pressionado (immediate mode)
    QuadBatch         solids_;
    QuadBatch         glyphs_;

    // 0.7.4 — runs de submissão em ORDEM (z-order sólidos↔texturas):
    // cada entrada aponta o batch + textura + range de vértices; o
    // endFrame submete-as SEQUENCIALMENTE (antes: grupos fixos → sólidos
    // sempre por baixo das imagens). 0.8.4: armazenamento DINÂMICO — o cap
    // fixo de 32 cortava runs em silêncio e empurrava os GLIFOS fora do cap
    // do Renderer (texto inteiro a sumir no C33 com browser/timeline
    // carregados). kMaxRuns passa a ser LIMIAR DE AVISO (o Renderer loga
    // acima de kSubWarn); reserve(64) = zero realloc em steady state.
    struct Run {
        const QuadBatch* batch;
        u32              tex;          // 0 = sólidos (branca)
        u32              firstVertex;
        u32              vertexCount;
    };
    static constexpr u32 kMaxRuns = 64;   // limiar de aviso (não corta)
    std::vector<Run>     runs_;           // dinâmico — NUNCA descarta

    // 0.7.0 — imagens da UI criável: até 4 batches (um por textura)
    static constexpr u32 kMaxImageBatches = 4;
    QuadBatch         images_[kMaxImageBatches];
    u32               imageTex_[kMaxImageBatches] = {};
    u32               imageCount_ = 0;
    const Texture*    (*imgResolve_)(const std::string&) = nullptr;

    // ---- F4.1: scroll -------------------------------------------------------
    struct ScrollSlot {
        u64          id = 0;
        bool         used = false;
        u32          lastFrame = 0;   // 0.9.6.4: o último beginScroll dele
        scroll::State st;
        UiRect       region{};
        f32          contentH = 0.0f;
    };
    static constexpr u32 kMaxScrollSlots = 8;
    static constexpr i32 kNoScroll = -1;
    ScrollSlot scrollSlots_[kMaxScrollSlots];
    // 0.9.6.4 (GRUPO A · A4): contador de frames p/ RECOLHER slots de
    // regiões que não desenharam (overlay fechado). Os 8 slots eram
    // DEFINITIVOS: Settings+Docs+Script+Texto+Áudio+Logs+Consola+Ficheiros
    // numa sessão e o 9.º scroll (o BROWSER, o seletor...) nascia MORTO ao
    // toque — a FASE 12.8b apanhou-o ao vivo (612 frames com região morta)
    u32        frameStamp_   = 0;
    i32        scrollCur_    = kNoScroll;   // região aberta neste frame
    bool       inScroll_     = false;       // entre begin/endScroll
    bool       scrollPending_= false;       // press edge à espera de claim
    u64        pendingId_    = 0;
    UiRect     clip_         = {0.0f, 0.0f, 1e9f, 1e9f};
    bool       tapValid_     = false;
    f32        tapX_ = 0.0f, tapY_ = 0.0f;
    i32        tapSlot_      = kNoScroll;   // F5.0-fix: slot dono do tap

    // 0.8.6: transformação de quads por elemento (rotação à volta do centro)
    bool xformActive_ = false;
    f32  xformCx_ = 0.0f, xformCy_ = 0.0f;
    f32  xformSin_ = 0.0f, xformCos_ = 1.0f;

    // ---- 0.9.6.5 (GRUPO B): o registo do layout --------------------------------
    // auditAdd_ é o CHOKE POINT de todas as entradas; auditComposite_ é o
    // guard dos compostos (frame/panelRounded/button chamam panel() por
    // dentro — o widget de TOPO regista UMA entrada, não as partes)
    void auditAdd_(layout::Entry::Kind kind, u64 id, f32 x, f32 y, f32 w, f32 h);
    void auditLabel_(f32 xBaseline, f32 yBaseline, const char* shown,
                     f32 fullW, bool truncated, f32 k);
    layout::Record audit_;
    bool           auditing_ = false;
    u32            auditComposite_ = 0;
    // 0.9.6.8 (GRUPO E): viva SÓ durante buttonCompact() — a 1ª entrada
    // Button apanha-a (auditAdd_); nunca escapa da chamada
    bool           auditCompactNext_ = false;
    f32            auditRowFloorNext_ = 0.0f;   // PASSO 1: piso de linha
    bool           auditFitByDesign_ = false;   // PASSO 2: corte por desenho
};

} // namespace vv
