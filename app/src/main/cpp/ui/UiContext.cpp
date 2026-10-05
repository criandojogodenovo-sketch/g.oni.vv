#include "ui/UiContext.h"
#include <cstring>

namespace vv {

namespace {
constexpr u32 kMaxQuadsUi = 4096;
// (kBtnW/kBtnGap eram da toolbar de 3 botões — a barra 0.7.6 vive em
// ui/Toolbar.cpp; kPad continua usado pelo statusLine)
constexpr f32 kPad    = 16.0f;
} // namespace

void UiContext::init() {
    solids_.reserve(kMaxQuadsUi);
    glyphs_.reserve(kMaxQuadsUi);
    for (u32 i = 0; i < kMaxImageBatches; ++i) {
        images_[i].reserve(64);   // 0.7.0: poucas imagens por frame
    }
    runs_.reserve(kMaxRuns);   // 0.8.4: zero realloc de runs em steady state
}

void UiContext::beginFrame(Renderer* renderer, const InputState* input,
                           f32 screenW, f32 screenH) {
    renderer_ = renderer;
    input_ = input;
    sw_ = screenW;
    sh_ = screenH;
    textScale_ = 1.0f;   // 0.7.4: reset por frame (só o viewport 2D escala)
    solids_.clear();
    glyphs_.clear();
    for (u32 i = 0; i < kMaxImageBatches; ++i) {   // 0.7.0
        images_[i].clear();
        imageTex_[i] = 0;
    }
    imageCount_ = 0;
    runs_.clear();   // 0.7.4: runs de submissão por frame (0.8.4: dinâmico)
    xformActive_ = false;   // 0.8.6: nunca arrastar uma xform para o frame

    // 0.9.6.4 (GRUPO A · A4): o contador de frames avança AQUI — os slots
    // de scroll que não desenharem neste frame ficam STALE e podem ser
    // recolhidos pelo beginScroll de outra região (o fix dos 8 slots)
    ++frameStamp_;

    // F4.1: estado de scroll POR FRAME (os slots com offset persistem)
    scrollCur_    = kNoScroll;
    inScroll_     = false;
    scrollPending_ = false;
    pendingId_    = 0;
    tapValid_     = false;
    tapSlot_      = kNoScroll;   // F5.0-fix
    clip_         = {0.0f, 0.0f, 1e9f, 1e9f};
}

// ---- emissão com clip (F4.1) -----------------------------------------------
bool UiContext::emitTo(QuadBatch& b, f32 x, f32 y, f32 w, f32 h,
                       f32 u0, f32 v0, f32 u1, f32 v1, const f32 color[4]) {
    scroll::Clipped cl;
    if (!scroll::clipQuad(x, y, w, h, u0, v0, u1, v1, clip_, cl)) {
        return false;
    }
    if (xformActive_) {
        // 0.8.6: rotação por elemento — os cantos do rect JÁ CLIPADO giram à
        // volta do centro do ELEMENTO (o clip continua axis-aligned: nos
        // cantos de um elemento rodado dentro de um scroll o recorte é
        // aproximado — aceitável e documentado)
        const f32 ca = xformCos_, sa = xformSin_;
        const f32 dx0 = cl.x - xformCx_, dy0 = cl.y - xformCy_;
        const f32 dx1 = cl.x + cl.w - xformCx_, dy1 = cl.y + cl.h - xformCy_;
        const f32 x0 = xformCx_ + dx0 * ca - dy0 * sa;
        const f32 y0 = xformCy_ + dx0 * sa + dy0 * ca;
        const f32 x1 = xformCx_ + dx0 * ca - dy1 * sa;
        const f32 y1 = xformCy_ + dx0 * sa + dy1 * ca;
        const f32 x2 = xformCx_ + dx1 * ca - dy1 * sa;
        const f32 y2 = xformCy_ + dx1 * sa + dy1 * ca;
        const f32 x3 = xformCx_ + dx1 * ca - dy0 * sa;
        const f32 y3 = xformCy_ + dx1 * sa + dy0 * ca;
        const f32 px[6] = {x0, x1, x2, x0, x2, x3};
        const f32 py[6] = {y0, y1, y2, y0, y2, y3};
        b.quadCorners(px, py, cl.u0, cl.v0, cl.u1, cl.v1,
                      color[0], color[1], color[2], color[3]);
        return true;
    }
    b.quad(cl.x, cl.y, cl.w, cl.h, cl.u0, cl.v0, cl.u1, cl.v1,
           color[0], color[1], color[2], color[3]);
    return true;
}

// 0.8.6 — glifo com CANTOS EXPLÍCITOS (itálico). O recorte é feito pelo
// bounding rect (aproximação: glifos parcialmente fora do scroll podem
// vazar meio-pixel na borda recortada); a xform de rotação aplica-se como
// no emitTo.
void UiContext::emitGlyphCorners(QuadBatch& b, const f32 px[6], const f32 py[6],
                                 f32 u0, f32 v0, f32 u1, f32 v1,
                                 const f32 color[4]) {
    f32 mnX = px[0], mxX = px[0], mnY = py[0], mxY = py[0];
    for (u32 i = 1; i < 6; ++i) {
        mnX = px[i] < mnX ? px[i] : mnX;
        mxX = px[i] > mxX ? px[i] : mxX;
        mnY = py[i] < mnY ? py[i] : mnY;
        mxY = py[i] > mxY ? py[i] : mxY;
    }
    if (mxX < clip_.x || mnX >= clip_.x + clip_.w ||
        mxY < clip_.y || mnY >= clip_.y + clip_.h) {
        return;   // totalmente fora — não emite
    }
    if (xformActive_) {
        const f32 ca = xformCos_, sa = xformSin_;
        f32 rx[6], ry[6];
        for (u32 i = 0; i < 6; ++i) {
            const f32 dx = px[i] - xformCx_, dy = py[i] - xformCy_;
            rx[i] = xformCx_ + dx * ca - dy * sa;
            ry[i] = xformCy_ + dx * sa + dy * ca;
        }
        b.quadCorners(rx, ry, u0, v0, u1, v1,
                      color[0], color[1], color[2], color[3]);
        return;
    }
    b.quadCorners(px, py, u0, v0, u1, v1,
                  color[0], color[1], color[2], color[3]);
}

// 0.7.4 — run de submissão: extende o corrente quando é o MESMO batch+tex,
// senão abre um novo. 0.8.4: armazenamento DINÂMICO (reserve em init; zero
// realloc em steady state) — o cap fixo de 32 cortava runs em silêncio e,
// pior, empurrava o batch de GLIFOS para fora do cap do Renderer (o texto
// INTEIRO saía do ecrã no C33). Agora nada se corta; o frame anómalo é
// avisado pelo Renderer (limiar kSubWarn).
// `tex` = 0 p/ sólidos (textura branca do renderer).
void UiContext::recordRun(QuadBatch& b, u32 tex, u32 firstVertex,
                          u32 vertexCount) {
    if (!runs_.empty() && runs_.back().batch == &b &&
        runs_.back().tex == tex) {
        runs_.back().vertexCount += vertexCount;
        return;
    }
    runs_.push_back(Run{&b, tex, firstVertex, vertexCount});
}

// ---- 0.9.6.5 (GRUPO B): o registo do layout ---------------------------------

void UiContext::auditBegin(const char* screenName, f32 screenW, f32 screenH,
                            f32 insetT, f32 insetB, f32 insetL, f32 insetR,
                            f32 density) {
    audit_.clear();
    audit_.screen = screenName ? screenName : "";
    audit_.screenW = screenW;
    audit_.screenH = screenH;
    audit_.insetT = insetT;
    audit_.insetB = insetB;
    audit_.insetL = insetL;
    audit_.insetR = insetR;
    audit_.density = density;
    auditComposite_ = 0;
    auditing_ = true;
}

void UiContext::auditEnd() { auditing_ = false; }

void UiContext::auditAdd_(layout::Entry::Kind kind, u64 id, f32 x, f32 y,
                          f32 w, f32 h) {
    if (!auditing_) return;
    layout::Entry e;
    e.kind = kind;
    e.id = id;
    e.x = x;
    e.y = y;
    e.w = w;
    e.h = h;
    e.clipped = inScroll_;
    // 0.9.6.8 (GRUPO E): a tecla COMPACTA (a barra de símbolos de 40dp da
    // spec E) — a flag vive SÓ durante a chamada de buttonCompact(); a
    // 1ª entrada Button apanha-a (o label do texto de dentro não é
    // compacto — a exceção do validador é para a TECLA, não para o texto)
    if (kind == layout::Entry::Button && auditCompactNext_) {
        e.compact = true;
        auditCompactNext_ = false;
    }
    audit_.add(e);
}

void UiContext::auditLabel_(f32 xBaseline, f32 yBaseline, const char* shown,
                            f32 fullW, bool truncated, f32 k) {
    // o MESMO guard dos compostos: o label chamado POR DENTRO de um
    // button/labelFitted não re-regista (quem chama regista a entrada
    // CERTA — o button com a truncagem do SEU texto, o labelFitted com o
    // fullW do texto ORIGINAL)
    if (!auditing_ || auditComposite_ != 0 || !font_ || !font_->ok() ||
        !shown) {
        return;
    }
    layout::Entry e;
    e.kind = layout::Entry::Label;
    e.x = xBaseline;
    e.y = yBaseline - font_->ascent() * k;
    e.w = fontWidth(shown);
    e.h = (font_->ascent() + font_->descent()) * k;
    e.fullW = fullW > 0.0f ? fullW : e.w;
    e.truncated = truncated;
    e.clipped = inScroll_;
    audit_.add(e);
}

void UiContext::panel(f32 x, f32 y, f32 w, f32 h, const f32 color[4]) {
    // GRUPO B: o rect do painel DESENHADO (o bounding box — os cantos
    // curvos cobrem por dentro desde a 0.9.0; o box é o mesmo)
    if (auditing_ && auditComposite_ == 0) {
        auditAdd_(layout::Entry::Panel, 0, x, y, w, h);
    }
    const u32 fv = solids_.vertexCount();
    if (emitTo(solids_, x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color)) {
        recordRun(solids_, 0u, fv, 6u);   // 0.7.4: z-order real
    }
}

void UiContext::frame(f32 x, f32 y, f32 w, f32 h, f32 t, const f32 color[4]) {
    if (auditing_ && auditComposite_ == 0) {
        auditAdd_(layout::Entry::Frame, 0, x, y, w, h);
        ++auditComposite_;   // as 4 arestas são PARTES — uma entrada só
        panel(x, y, w, t, color);
        panel(x, y + h - t, w, t, color);
        panel(x, y + t, t, h - 2.0f * t, color);
        panel(x + w - t, y + t, t, h - 2.0f * t, color);
        --auditComposite_;
        return;
    }
    panel(x, y, w, t, color);
    panel(x, y + h - t, w, t, color);
    panel(x, y + t, t, h - 2.0f * t, color);
    panel(x + w - t, y + t, t, h - 2.0f * t, color);
}

// ---- 0.9.0 — CANTOS CURVOS (escadaria de quads; sem shaders/blur) -----------
// O canto é o quarto de círculo de raio r centrado em (x+r, y+r) etc. A
// escadaria de kCornerSteps degraus por canto COBRE o círculo por dentro:
// o degrau i (faixa [dy0, dy1) do topo) começa em dx = r − √(r²−(r−dy1)²)
// (a largura no FUNDO da faixa — a mais larga — logo o degrau cobre toda a
// faixa e nunca sai do círculo). Custo: 4 passos/canto = 21 quads no total.
namespace {
constexpr int kCornerSteps = 4;
inline f32 cornerInset(f32 r, f32 dy) {
    // dx mínimo do quarto de círculo à profundidade dy (0..r): pontos com
    // dx < inset estão FORA do círculo.
    const f32 s = r * r - (r - dy) * (r - dy);
    return r - ((s > 0.0f) ? std::sqrt(s) : 0.0f);
}
} // namespace

void UiContext::panelRounded(f32 x, f32 y, f32 w, f32 h, f32 radius,
                             const f32 color[4]) {
    if (auditing_ && auditComposite_ == 0) {
        auditAdd_(layout::Entry::Panel, 0, x, y, w, h);
    }
    ++auditComposite_;   // a escadaria toda é UMA entrada (o bounding box)
    if (w <= 0.0f || h <= 0.0f) {
        --auditComposite_;
        return;
    }
    const f32 half = (w < h ? w : h) * 0.5f;
    const f32 r = radius < half ? (radius > 0.0f ? radius : 0.0f) : half;
    if (r < 1.0f) {   // raio degenerado = rect cru
        panel(x, y, w, h, color);
        --auditComposite_;
        return;
    }
    // corpo (cruz central): coluna central inteira + 2 faixas laterais
    panel(x + r, y, w - 2.0f * r, h, color);
    panel(x, y + r, r, h - 2.0f * r, color);
    panel(x + w - r, y + r, r, h - 2.0f * r, color);
    // 4 cantos: escadaria cobrindo o quarto de círculo por DENTRO
    const f32 stepH = r / static_cast<f32>(kCornerSteps);
    for (int i = 0; i < kCornerSteps; ++i) {
        // faixa [dy0, dy1) medida do TOPO do canto; largura pela dy do FUNDO
        const f32 dy0 = static_cast<f32>(i) * stepH;
        const f32 dy1 = static_cast<f32>(i + 1) * stepH;
        const f32 in0 = cornerInset(r, dy0);   // largura no topo (menor)
        const f32 in1 = cornerInset(r, dy1);   // largura no fundo (maior)
        const f32 sh_ = dy1 - dy0;
        const f32 sx = in1;                    // começa na largura MÁXIMA
        const f32 sw = r - in1;                // até ao fim do canto
        if (sw > 0.05f) {
            // top-left
            panel(x + sx, y + dy0, sw, sh_, color);
            // top-right
            panel(x + w - r, y + dy0, r - sx, sh_, color);
            // bottom-left
            panel(x + sx, y + h - dy1, sw, sh_, color);
            // bottom-right
            panel(x + w - r, y + h - dy1, r - sx, sh_, color);
        }
        (void)in0;
    }
    --auditComposite_;
}

void UiContext::frameRounded(f32 x, f32 y, f32 w, f32 h, f32 t, f32 radius,
                             const f32 color[4]) {
    if (auditing_ && auditComposite_ == 0) {
        auditAdd_(layout::Entry::Frame, 0, x, y, w, h);
    }
    ++auditComposite_;
    if (w <= 0.0f || h <= 0.0f || t <= 0.0f) {
        --auditComposite_;
        return;
    }
    const f32 half = (w < h ? w : h) * 0.5f;
    const f32 r = radius < half ? (radius > 0.0f ? radius : 0.0f) : half;
    if (r < 1.0f) {
        frame(x, y, w, h, t, color);
        --auditComposite_;
        return;
    }
    // arestas retas (entre os cantos)
    panel(x + r, y, w - 2.0f * r, t, color);                     // topo
    panel(x + r, y + h - t, w - 2.0f * r, t, color);             // fundo
    panel(x, y + r, t, h - 2.0f * r, color);                     // esq
    panel(x + w - t, y + r, t, h - 2.0f * r, color);             // dir
    // arcos dos cantos: polilinhas no line batch (mesma espessura do traço)
    const f32 cx[4] = {x + r, x + w - r, x + w - r, x + r};
    const f32 cy[4] = {y + r, y + r, y + h - r, y + h - r};
    const f32 a0[4] = {180.0f, 270.0f, 0.0f, 90.0f};
    const int kArcSeg = 5;
    f32 px[kArcSeg + 1], py[kArcSeg + 1];
    for (int c = 0; c < 4; ++c) {
        for (int i = 0; i <= kArcSeg; ++i) {
            const f32 a = (a0[c] + 90.0f * (static_cast<f32>(i) / kArcSeg)) *
                          0.01745329252f;
            px[i] = cx[c] + (r - t * 0.5f) * std::cos(a);
            py[i] = cy[c] + (r - t * 0.5f) * std::sin(a);
        }
        for (int i = 1; i <= kArcSeg; ++i) {
            drawLine(px[i - 1], py[i - 1], px[i], py[i], t, color);
        }
    }
    --auditComposite_;
}

void UiContext::panelPill(f32 x, f32 y, f32 w, f32 h, const f32 color[4]) {
    if (auditing_ && auditComposite_ == 0) {
        auditAdd_(layout::Entry::Panel, 0, x, y, w, h);
        ++auditComposite_;
        panelRounded(x, y, w, h, h * 0.5f, color);
        --auditComposite_;
        return;
    }
    panelRounded(x, y, w, h, h * 0.5f, color);
}

void UiContext::label(f32 xBaseline, f32 yBaseline, const char* text, const f32 color[4]) {
    labelStyled(xBaseline, yBaseline, text, color, 1.0f,
                static_cast<u8>(0));   // normal (0.8.6: tudo passa pelo styled)
}

// 0.8.6 — label com TIPOGRAFIA por elemento: escala da fonte (multiplicador
// da base 28 px), negrito (cada glifo desenha 2× com +1 px — embutido, sem
// segundo atlas) e itálico (cisalhamento dos VÉRTICES TOP dos quads — a
// haste inclina, o clip permanece axis-aligned). GL-free e afervel nos
// batches como o label de sempre.
void UiContext::labelStyled(f32 xBaseline, f32 yBaseline, const char* text,
                            const f32 color[4], f32 fontScale, u8 style) {
    if (!font_ || !font_->ok() || !text) {
        return;
    }
    // 0.9.6.6 (GRUPO C): o k TOTAL = textScale_ (viewport 2D) × textK()
    // (a densidade do texto — o choke point) × fontScale (o sp RELATIVO
    // do chamador, sp/14). A 2.0 o k de sempre; a 1.0 o corpo a 14px.
    const f32 k = textScale_ * theme::textK() *
                  (fontScale > 0.05f ? fontScale : 1.0f);
    // GRUPO B: o label DESENHADO (bounding box pelas métricas reais — o
    // MESMO ascent/descent do centrado do button desde a F5.0-fix)
    auditLabel_(xBaseline, yBaseline, text, 0.0f, false, k);
    const bool bold = style == static_cast<u8>(1);
    const bool italic = style == static_cast<u8>(2);
    f32 penX = xBaseline;
    // FASE 9 (G1-2 — ACENTOS): iteração UTF-8 por CODE POINT — o atlas
    // cobre Latin-1 + Latin Ext-A + …; glifo ausente avança 0.30·altura
    // (o espaço de sempre — nunca crash, nunca byte a byte)
    for (const char* p = text; *p;) {
        u32 bytes = 1;
        const u32 cp = utf8Decode(p, &bytes);
        const Glyph* g = font_->glyphFor(cp);
        if (!g) {
            penX += font_->height() * 0.30f * k;
            p += bytes;
            continue;
        }
        const f32 gx = penX + g->xoff * k;
        const f32 gy = yBaseline + g->yoff * k;
        const f32 gw = g->w * k;
        const f32 gh = g->h * k;
        const u32 passes = bold ? 2u : 1u;
        for (u32 pass = 0; pass < passes; ++pass) {
            const f32 ox = bold ? static_cast<f32>(pass) * (k >= 1.0f ? 1.0f : 0.5f) : 0.0f;
            if (italic && gh > 0.0f) {
                // cisalhamento: os vértices TOP deslocam +0.21·gh (≈12°)
                const f32 shear = 0.21f * gh;
                const f32 px[6] = {gx + ox + shear, gx + ox, gx + ox + gw,
                                   gx + ox + shear, gx + ox + gw, gx + ox + gw + shear};
                const f32 py[6] = {gy, gy + gh, gy + gh,
                                   gy, gy + gh, gy};
                emitGlyphCorners(glyphs_, px, py, g->u0, g->v0, g->u1, g->v1, color);
            } else {
                emitTo(glyphs_, gx + ox, gy, gw, gh,
                       g->u0, g->v0, g->u1, g->v1, color);
            }
        }
        penX += g->xadv * k;
        p += bytes;
    }
}

// F4.2/B2: mede; se exceder maxW trunca com "…" (U+2026 — no atlas desde
// a FASE 9/G1-2; o textfit usa o glifo se existir) pelo maior prefixo que
// caiba. Sem fonte → no-op (igual label).
// 0.9.6.6 (GRUPO C): labelFittedStyled — o MESMO contrato do fit para
// textos que não são CORPO (a legenda 12sp da status bar, títulos): a
// medida e o draw usam a MESMA escala (fontScale relativo × textK do
// choke point) — o labelFitted de sempre é o caso corpo (k=1 relativo).
void UiContext::labelFittedStyled(f32 xBaseline, f32 yBaseline,
                                   const char* text, const f32 color[4],
                                   f32 maxW, f32 fontScale, u8 style) {
    if (!hasFont() || !text) {
        return;
    }
    const f32 k = textScale_ * theme::textK() *
                  (fontScale > 0.05f ? fontScale : 1.0f);
    const f32 fullW = font_ ? font_->widthOf(text) * k : 0.0f;
    if (fullW <= maxW) {
        // coube INTEIRO — a entrada diz a largura REAL (nunca trunca);
        // o labelStyled() de dentro regista POR ELE (não há duplo)
        labelStyled(xBaseline, yBaseline, text, color, fontScale, style);
        return;
    }
    char buf[256];
    textfit::ellipsize(
        text, maxW, [&](const char* s) { return font_->widthOf(s) * k; },
        buf, sizeof(buf));
    if (buf[0]) {
        // TRUNCOU — a entrada carrega a largura INTEIRA do texto original
        // e o flag (o validador conta a informação perdida)
        auditLabel_(xBaseline, yBaseline, buf, fullW, true, k);
        ++auditComposite_;   // o labelStyled() de dentro já não regista
        labelStyled(xBaseline, yBaseline, buf, color, fontScale, style);
        --auditComposite_;
    }
}

void UiContext::labelFitted(f32 xBaseline, f32 yBaseline, const char* text,
                            const f32 color[4], f32 maxW) {
    labelFittedStyled(xBaseline, yBaseline, text, color, maxW, 1.0f, 0);
}

// 0.7.6 — a CAPTURA de gesto do botão, extraída (a toolbar desenha os
// próprios widgets com ícones e precisa da MESMA semântica sem o desenho).
bool UiContext::widgetHit(u64 id, f32 x, f32 y, f32 w, f32 h) {
    bool pressed = false;

    // GRUPO B: o hit-rect do widget interativo (a toolbar/banner/fila do
    // browser desenham por fora e capturam por AQUI — a região tocável é
    // layout também; dentro do button() o guard evita a dupla)
    if (auditing_ && auditComposite_ == 0) {
        auditAdd_(layout::Entry::Button, id, x, y, w, h);
    }

    const bool down = input_ && input_->down(0);
    f32 px = -1.0f, py = -1.0f;
    if (input_) {
        input_->pos(0, px, py);
    }
    const bool inside = (px >= x && px < x + w && py >= y && py < y + h);

    // F4.1: dentro de região de scroll o botão só DESENHA — o scroll reclama
    // o gesto e o painel re-despacha o tap (scroll::buttonCaptures).
    if (down && inside && active_ == 0 && scroll::buttonCaptures(inScroll_)) {
        active_ = id;
    }
    if (active_ == id && !down) {
        if (inside) {
            pressed = true;
        }
        active_ = 0;
    }
    return pressed;
}

// GRUPO D (0.9.6.7) — pega de arrasto: a MESMA captura do widgetHit, sem
// o registo no audit (não é alvo de tap) e sem a semântica de clique (o
// drag é consumido pelo chamador com o input cru — o padrão da pega do
// drawer). Devolve true enquanto o dedo está na pega.
bool UiContext::dragHandle(u64 id, f32 x, f32 y, f32 w, f32 h) {
    const bool down = input_ && input_->down(0);
    f32 px = -1.0f, py = -1.0f;
    if (input_) {
        input_->pos(0, px, py);
    }
    const bool inside = (px >= x && px < x + w && py >= y && py < y + h);
    if (down && inside && active_ == 0 && scroll::buttonCaptures(inScroll_)) {
        active_ = id;
    }
    if (active_ == id && !down) {
        active_ = 0;
        return false;
    }
    return active_ == id;
}

bool UiContext::button(u64 id, f32 x, f32 y, f32 w, f32 h, const char* text) {
    // 0.9.6.6 (GRUPO C): o texto do botão mede-se pelo CONTEXTO (fontWidth/
    // textMetrics — textK incluído). ANTES media-se pelo ATLAS CRU
    // (font_->widthOf): dentro do viewport 2D (textScale_) e a densidade
    // ≠2 o texto media uma largura e DESENHAVA outra — truncava/centrava
    // errado (a classe do bug que o Grupo C fecha: medir ≠ desenhar).
    const f32 fitPad = theme::dp(8.0f);   // respiro do texto no rect (dp)
    if (auditing_ && auditComposite_ == 0) {
        auditAdd_(layout::Entry::Button, id, x, y, w, h);
        ++auditComposite_;
        const bool pressedInner = widgetHit(id, x, y, w, h);
        const bool downInner = input_ && input_->down(0);
        const bool heldInner = (active_ == id && downInner);
        const f32* bg = heldInner ? theme::ACCENT : theme::PANEL;
        const f32* txt = heldInner ? theme::BG : theme::TEXT;
        // 0.9.6.6 (GRUPO C · CANTOS SUAVIZADOS): o button() é o CHOKE POINT
        // de TODOS os botões da app — cantos 8dp (spec A: «raios 8dp
        // cards/botões») por AQUI, um só sítio. (o raio é clampado a
        // min(w,h)/2 pelo panelRounded — teclas finas ficam pill sem medo)
        panelRounded(x, y, w, h, theme::dp(theme::kRadiusCard), bg);
        frameRounded(x, y, w, h, 1.0f, theme::dp(theme::kRadiusCard),
                     theme::LINE);
        if (font_ && font_->ok() && text) {
            char fit[256];
            const char* shown = text;
            if (fontWidth(text) > w - fitPad) {
                textfit::ellipsize(text, w - fitPad,
                                   [this](const char* s) { return fontWidth(s); },
                                   fit, sizeof(fit));
                shown = fit;
            }
            const f32 tw = fontWidth(shown);
            const TextMetrics tm = textMetrics();
            const f32 baseline = y + (h - tm.ascent - tm.descent) * 0.5f +
                                 tm.ascent;
            // a entrada do TEXTO do botão (com a truncagem do nome — os
            // nomes longos de TIC da Hierarchy contam-se AQUI); o guard
            // abre SÓ para o registo (o label() de dentro segue calado)
            --auditComposite_;
            auditLabel_(x + (w - tw) * 0.5f, baseline, shown,
                        fontWidth(text), shown != text,
                        textScale_ * theme::textK());
            ++auditComposite_;
            label(x + (w - tw) * 0.5f, baseline, shown, txt);
        }
        --auditComposite_;
        return pressedInner;
    }
    const bool pressed = widgetHit(id, x, y, w, h);
    const bool down = input_ && input_->down(0);
    const bool held = (active_ == id && down);
    const f32* bg  = held ? theme::ACCENT : theme::PANEL;
    const f32* txt = held ? theme::BG     : theme::TEXT;
    // 0.9.6.6 (GRUPO C · CANTOS SUAVIZADOS): idem — 8dp no choke point
    panelRounded(x, y, w, h, theme::dp(theme::kRadiusCard), bg);
    frameRounded(x, y, w, h, 1.0f, theme::dp(theme::kRadiusCard),
                 theme::LINE);

    if (font_ && font_->ok() && text) {
        // F4.2/B2: o texto do botão nunca sai do rect — nomes longos de TIC
        // na Hierarchy eram cortados pela borda do botão
        char fit[256];
        const char* shown = text;
        if (fontWidth(text) > w - fitPad) {
            textfit::ellipsize(text, w - fitPad,
                               [this](const char* s) { return fontWidth(s); },
                               fit, sizeof(fit));
            shown = fit;
        }
        const f32 tw = fontWidth(shown);
        // F5.0-fix: baseline centrada com as métricas REAIS do bloco de
        // texto (topo = baseline − ascent, fundo = baseline + descent) —
        // antes era a aproximação 0.30*altura, que com a fonte a 28 px
        // deixava os glifos descerem para a linha de baixo.
        // (0.9.6.6: as métricas são as do CONTEXTO — a MESMA escala do draw)
        const TextMetrics tm = textMetrics();
        const f32 baseline = y + (h - tm.ascent - tm.descent) * 0.5f +
                             tm.ascent;
        label(x + (w - tw) * 0.5f, baseline, shown, txt);
    }
    return pressed;
}

// 0.9.6.8 (GRUPO E) — A TECLA COMPACTA: o MESMO button() (desenho, gesto,
// texto — TUDO igual) com a entrada de audit marcada compacta. É a tecla
// da BARRA DE SÍMBOLOS (40dp da spec do autor); o validador aplica o piso
// compacto (40dp) em vez do 48dp da casa — exceção ESTREITA vigiada pela
// sentinela R-027 (a flag só vive durante ESTA chamada)
bool UiContext::buttonCompact(u64 id, f32 x, f32 y, f32 w, f32 h,
                              const char* text) {
    auditCompactNext_ = true;
    const bool pressed = button(id, x, y, w, h, text);
    auditCompactNext_ = false;   // a flag NUNCA escapa da chamada
    return pressed;
}

bool UiContext::slider(u64 id, f32 x, f32 y, f32 w, f32 h, f32 minV, f32 maxV, f32& value) {
    // GRUPO B: o rect INTERATIVO do slider (o trilho/thumb são visuais)
    if (auditing_ && auditComposite_ == 0) {
        auditAdd_(layout::Entry::Slider, id, x, y, w, h);
        ++auditComposite_;
        const bool changedInner = sliderCore(id, x, y, w, h, minV, maxV, value);
        --auditComposite_;
        return changedInner;
    }
    return sliderCore(id, x, y, w, h, minV, maxV, value);
}

// o corpo do slider de sempre (gesto + trilho/thumb) — o slider() de cima
// é que carrega o registo do layout (GRUPO B)
bool UiContext::sliderCore(u64 id, f32 x, f32 y, f32 w, f32 h, f32 minV,
                           f32 maxV, f32& value) {
    const bool down = input_ && input_->down(0);
    f32 px = -1.0f, py = -1.0f;
    if (input_) {
        input_->pos(0, px, py);
    }
    const bool inside = (px >= x && px < x + w && py >= y && py < y + h);

    // F4.1: slider mantém a prioridade de captura DENTRO da região de scroll
    // (regra da spec: drag horizontal num slider = slider). Ao reclamar,
    // cancela o claim pendente do scroll (endScroll vê active_ != 0).
    if (down && inside && active_ == 0 && scroll::sliderCaptures(inScroll_)) {
        active_ = id;
    }
    bool changed = false;
    if (active_ == id && down) {
        // dedo capturado: segue horizontalmente (mesmo fora do track — gesto contínuo)
        const f32 t = (px - x) / (w > 1.0f ? w : 1.0f);
        const f32 nv = minV + (maxV - minV) * (t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t));
        if (nv != value) {
            value = nv;
            changed = true;
        }
    }
    if (active_ == id && !down) {
        active_ = 0;
    }

    // trilho + preenchimento + thumb (tema mono, sem cores novas)
    const f32 cy = y + h * 0.5f - 2.0f;
    panel(x, cy, w, 4.0f, theme::LINE);
    const f32 t = (maxV > minV) ? (value - minV) / (maxV - minV) : 0.0f;
    const f32 fillW = w * (t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t));
    if (fillW > 0.0f) {
        panel(x, cy, fillW, 4.0f, theme::TEXT);
    }
    const f32 thumbW = 10.0f;
    const f32 thumbX = x + fillW - thumbW * 0.5f;
    const f32 ty     = y + h * 0.5f - 12.0f;
    panel(thumbX < x ? x : (thumbX > x + w - thumbW ? x + w - thumbW : thumbX), ty,
          thumbW, 24.0f, theme::ACCENT);
    return changed;
}

// ---- scroll (F4.1) ----------------------------------------------------------
void UiContext::beginScroll(u64 id, const UiRect& region, f32 contentHeight) {
    // GRUPO B: a região de scroll é uma zona INTERATIVA de layout (o drag
    // vive aqui); os filhos dela nascem com clipped=true (auditAdd_ lê
    // inScroll_ — tem de ser registada ANTES do flag ligar)
    if (auditing_ && auditComposite_ == 0) {
        auditAdd_(layout::Entry::Scroll, id, region.x, region.y, region.w,
                  region.h);
    }
    // slot por id (procura; senão primeiro livre)
    i32 slot = kNoScroll;
    for (u32 i = 0; i < kMaxScrollSlots; ++i) {
        if (scrollSlots_[i].used && scrollSlots_[i].id == id) {
            slot = static_cast<i32>(i);
            break;
        }
    }
    if (slot == kNoScroll) {
        for (u32 i = 0; i < kMaxScrollSlots; ++i) {
            if (!scrollSlots_[i].used) {
                scrollSlots_[i].used = true;
                scrollSlots_[i].id = id;
                slot = static_cast<i32>(i);
                break;
            }
        }
    }
    // 0.9.6.4 (GRUPO A · A4) — A EXAUSTÃO QUE MATAVA O BROWSER: os 8 slots
    // eram DEFINITIVOS e as regiões de scroll da casa passam de 8 numa
    // sessão (hierarquia/inspector/logs/scenes/uiInsp/ficheiros/consola/
    // seletor/browser/settings/docs/script/texto/áudio) — quem chegasse
    // ao 9.º nascia MORTO ao toque (sem região = sem claim = sem tap; a
    // FASE 12.8b apanhou: 612 frames com o browser surdo). AGORA: um slot
    // cujo dono NÃO desenhou neste frame (lastFrame != frameStamp_) está
    // STALE — o overlay fechou — e é RECICLADO (o mais antigo primeiro;
    // o preço é o offset dele recomeçar a zero, nunca a região ficar
    // morta: um scroll perdido < um botão morto)
    if (slot == kNoScroll) {
        i32 stale = kNoScroll;
        u32 oldest = 0xFFFFFFFFu;
        for (u32 i = 0; i < kMaxScrollSlots; ++i) {
            if (scrollSlots_[i].used &&
                scrollSlots_[i].lastFrame != frameStamp_ &&
                scrollSlots_[i].lastFrame < oldest) {
                oldest = scrollSlots_[i].lastFrame;
                stale = static_cast<i32>(i);
            }
        }
        if (stale != kNoScroll) {
            scrollSlots_[stale] = ScrollSlot{};
            scrollSlots_[stale].used = true;
            scrollSlots_[stale].id = id;
            slot = stale;
        }
    }
    if (slot == kNoScroll) {
        return;   // sem slots E sem stale (todas as regiões vivas) — ignora
    }
    scrollSlots_[slot].lastFrame = frameStamp_;

    ScrollSlot& s = scrollSlots_[slot];
    scrollCur_ = slot;
    inScroll_  = true;
    s.region   = region;
    s.contentH = contentHeight;
    s.st.offset = scroll::clampOffset(s.st.offset, contentHeight, region.h);
    clip_      = region;

    if (!input_) {
        return;
    }

    if (s.st.active) {
        // gesto em curso: arrasta (ou termina no release)
        if (input_->down(0)) {
            f32 px, py;
            input_->pos(0, px, py);
            scroll::dragTo(s.st, px, py, contentHeight, region.h);
        } else {
            f32 px, py;
            input_->pos(0, px, py);
            if (scroll::endDrag(s.st)) {
                tapValid_ = true;
                tapX_ = px;
                tapY_ = py;
                tapSlot_ = scrollCur_;   // F5.0-fix: dono do tap
            }
            if (active_ == id) {
                active_ = 0;
            }
        }
        return;
    }

    // press edge dentro da região → claim ADIADO para endScroll (os widgets
    // desenhados entre begin/end têm prioridade — slider reivindica, botão não)
    if (input_->pressed(0) && active_ == 0) {
        f32 px, py;
        input_->pos(0, px, py);
        if (scroll::inside(region, px, py)) {
            scrollPending_ = true;
            pendingId_     = id;
        }
    }
}

void UiContext::endScroll() {
    if (scrollCur_ == kNoScroll) {
        inScroll_ = false;
        return;
    }
    ScrollSlot& s = scrollSlots_[scrollCur_];

    // resolve o claim pendente: nenhum widget reclamou → scroll reclama agora
    if (scrollPending_ && pendingId_ == s.id && active_ == 0 && input_) {
        f32 px, py;
        input_->pos(0, px, py);
        if (input_->down(0)) {
            scroll::beginDrag(s.st, px, py);
            active_ = s.id;
        } else {
            // tap de 1 frame (press+release no mesmo frame) → re-despacha
            tapValid_ = true;
            tapX_ = px;
            tapY_ = py;
            tapSlot_ = scrollCur_;   // F5.0-fix: dono do tap
        }
    }
    scrollPending_ = false;
    pendingId_     = 0;

    // indicador discreto (só com overflow) — ainda com o clip ativo
    if (scroll::maxOffset(s.contentH, s.region.h) > 0.0f) {
        f32 bx, by, bw, bh;
        scroll::indicator(s.region, s.contentH, s.st.offset, bx, by, bw, bh);
        emitTo(solids_, bx, by, bw, bh, 0.0f, 0.0f, 1.0f, 1.0f, theme::ACCENT);
    }

    inScroll_ = false;
    clip_     = {0.0f, 0.0f, 1e9f, 1e9f};
    scrollCur_ = kNoScroll;
}

f32 UiContext::scrollOffset() const {
    return scrollCur_ != kNoScroll ? scrollSlots_[scrollCur_].st.offset : 0.0f;
}

// F5.0-fix: o tap é re-despachado POR ID — só a região que gerou o gesto o
// consome. Antes era global (primeiro scrollTap ganhava): a Hierarchy,
// desenhada antes do Inspector, comia o tap do painel direito e os botões
// do Inspector (mesh/tex/add TouchControls) não acionavam no device.
bool UiContext::scrollTap(u64 id, f32& x, f32& y) {
    if (!tapValid_) {
        return false;
    }
    if (tapSlot_ == kNoScroll || scrollSlots_[tapSlot_].id != id) {
        return false;   // tap de OUTRA região — NÃO consome
    }
    x = tapX_;
    y = tapY_;
    tapValid_ = false;
    return true;
}

// F5.2: offset por id (viewer de logs salta para o fundo ao abrir) — o
// valor cru fica no slot; beginScroll clampa contra o contentH da região.
void UiContext::scrollSetOffset(u64 id, f32 offset) {
    for (u32 i = 0; i < kMaxScrollSlots; ++i) {
        if (scrollSlots_[i].used && scrollSlots_[i].id == id) {
            scrollSlots_[i].st.offset = offset;
            return;
        }
    }
}

void UiContext::statusLine(const char* text) {
    const UiRect r = statusRect();
    panel(r.x, r.y, r.w, r.h, theme::PANEL);
    panel(r.x, r.y, r.w, 1.0f, theme::LINE);   // separador superior

    if (font_ && font_->ok() && text) {
        // 0.9.6.6 (GRUPO C): 12sp CAPTION (spec E — a linha de estado é
        // legenda) com baseline CENTRADA pelas métricas do contexto e o
        // FIT de sempre (nunca sai do rect). ANTES: label de CORPO no
        // ATLAS CRU (bloco 29px) numa banda de 24dp com o offset
        // «+0.30·altura» — SANGRAVA o fundo do ecrã (o ERRO medido do
        // Grupo B). Com o textK a 1.0 o bloco é 14px (cabe na banda de
        // 24px); a 2.0 é 28px (cabe na banda de 48px).
        labelFittedStyled(
            r.x + theme::dp(12.0f),
            theme::centeredBaseline(textMetrics().ascent,
                                    textMetrics().descent, r.y, r.h,
                                    theme::kFontCaption),
            text, theme::TEXT, r.w - theme::dp(24.0f),
            theme::fontScale(theme::kFontCaption), 0);
    }
}

// 0.7.0 — quad texturado da UI criável (elemento Image): resolve a ref,
// aloca (ou cria) o batch da textura e emite o quad (uv 0..1, cor = tint).
// 0.7.4:Button/Panel também passam por aqui (textura de fundo) e o quad é
// registado num RUN com a textura — a submissão fica na ORDEM REAL
// (z-order sólidos↔texturas correto).
bool UiContext::imageQuad(f32 x, f32 y, f32 w, f32 h, const std::string& ref,
                          const f32 color[4]) {
    if (!imgResolve_ || ref.empty()) {
        return false;
    }
    const Texture* tex = imgResolve_(ref);
    if (!tex || !tex->ok()) {
        return false;
    }
    const u32 id = tex->handle();
    u32 slot = imageCount_;
    for (u32 i = 0; i < imageCount_; ++i) {
        if (imageTex_[i] == id) {
            slot = i;
            break;
        }
    }
    if (slot == imageCount_) {
        if (imageCount_ >= kMaxImageBatches) {
            return false;   // cap de texturas por frame (documentado)
        }
        imageTex_[slot] = id;
        ++imageCount_;
    }
    const u32 fv = images_[slot].vertexCount();
    if (emitTo(images_[slot], x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color)) {
        recordRun(images_[slot], id, fv, 6u);
        return true;
    }
    return false;
}

void UiContext::endFrame() {
    if (!renderer_) {
        return;
    }
    // 0.8.7 — GESTO ÓRFÃO NUNCA ATRAVESSA O FRAME: um widget que desaparece
    // a meio do gesto (overlay fechado antes do release, painel escondido,
    // mode switch) deixava o active_ PRESO para sempre — e como TODO o
    // widget exige active_ == 0 para capturar um press novo, a UI inteira
    // morria ("a engine trava" do C33: render continua, nada responde).
    // No FIM do frame (todos os widgets já tiveram a sua hipótese de fired
    // no release), sem dedo em cima, qualquer active_ sobrevivente é órfão
    // por definição — morre AQUI. O reset no beginFrame NÃO serve: matava o
    // active_ ANTES do widget o ver no frame do release (o clique nunca
    // disparava). Com dedo em cima o active_ fica (gesto legítimo em curso).
    if (input_ && !input_->down(0)) {
        active_ = 0;
    }
    // 0.7.4 — submissão na ORDEM REAL de emissão (runs): sólidos e texturas
    // intercalam-se conforme foram desenhados; os GLIFOS ficam por último
    // (texto sempre legível — a regra do tema desde a 0.7.0).
    for (const Run& r : runs_) {
        renderer_->submit(*r.batch,
                           r.tex == 0u ? renderer_->whiteTexture()
                                       : r.tex,
                           r.firstVertex, r.vertexCount);
    }
    if (font_ && font_->ok()) {
        renderer_->submit(glyphs_, font_->texture());
    }
}

} // namespace vv
