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
    b.quad(cl.x, cl.y, cl.w, cl.h, cl.u0, cl.v0, cl.u1, cl.v1,
           color[0], color[1], color[2], color[3]);
    return true;
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

void UiContext::panel(f32 x, f32 y, f32 w, f32 h, const f32 color[4]) {
    const u32 fv = solids_.vertexCount();
    if (emitTo(solids_, x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f, color)) {
        recordRun(solids_, 0u, fv, 6u);   // 0.7.4: z-order real
    }
}

void UiContext::frame(f32 x, f32 y, f32 w, f32 h, f32 t, const f32 color[4]) {
    panel(x, y, w, t, color);
    panel(x, y + h - t, w, t, color);
    panel(x, y + t, t, h - 2.0f * t, color);
    panel(x + w - t, y + t, t, h - 2.0f * t, color);
}

void UiContext::label(f32 xBaseline, f32 yBaseline, const char* text, const f32 color[4]) {
    if (!font_ || !font_->ok() || !text) {
        return;
    }
    // 0.7.4: glifos ESCALADOS por textScale_ (paridade do viewport 2D —
    // o mini-canvas mostra o texto na MESMA proporção do Play)
    const f32 k = textScale_;
    f32 penX = xBaseline;
    for (const char* p = text; *p; ++p) {
        const char c = *p;
        if (c < static_cast<char>(FontAtlas::kFirstChar) ||
            c >= static_cast<char>(FontAtlas::kFirstChar + FontAtlas::kNumChars)) {
            penX += font_->height() * 0.30f * k;
            continue;
        }
        const Glyph& g = font_->glyph(c);
        emitTo(glyphs_, penX + g.xoff * k, yBaseline + g.yoff * k,
               g.w * k, g.h * k, g.u0, g.v0, g.u1, g.v1, color);
        penX += g.xadv * k;
    }
}

// F4.2/B2: mede; se exceder maxW trunca com "..." (ASCII — o atlas da F1 não
// tem U+2026) pelo maior prefixo que caiba. Sem fonte → no-op (igual label).
void UiContext::labelFitted(f32 xBaseline, f32 yBaseline, const char* text,
                            const f32 color[4], f32 maxW) {
    if (!hasFont() || !text) {
        return;
    }
    if (fontWidth(text) <= maxW) {
        label(xBaseline, yBaseline, text, color);
        return;
    }
    char buf[256];
    textfit::ellipsize(text, maxW,
                       [this](const char* s) { return fontWidth(s); },
                       buf, sizeof(buf));
    if (buf[0]) {
        label(xBaseline, yBaseline, buf, color);
    }
}

// 0.7.6 — a CAPTURA de gesto do botão, extraída (a toolbar desenha os
// próprios widgets com ícones e precisa da MESMA semântica sem o desenho).
bool UiContext::widgetHit(u64 id, f32 x, f32 y, f32 w, f32 h) {
    bool pressed = false;

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

bool UiContext::button(u64 id, f32 x, f32 y, f32 w, f32 h, const char* text) {
    const bool pressed = widgetHit(id, x, y, w, h);
    const bool down = input_ && input_->down(0);
    const bool held = (active_ == id && down);
    const f32* bg  = held ? theme::ACCENT : theme::PANEL;
    const f32* txt = held ? theme::BG     : theme::TEXT;
    panel(x, y, w, h, bg);
    frame(x, y, w, h, 1.0f, theme::LINE);

    if (font_ && font_->ok() && text) {
        // F4.2/B2: o texto do botão nunca sai do rect — nomes longos de TIC
        // na Hierarchy eram cortados pela borda do botão
        char fit[256];
        const char* shown = text;
        if (font_->widthOf(text) > w - 8.0f) {
            textfit::ellipsize(text, w - 8.0f,
                               [this](const char* s) { return font_->widthOf(s); },
                               fit, sizeof(fit));
            shown = fit;
        }
        const f32 tw = font_->widthOf(shown);
        // F5.0-fix: baseline centrada com as métricas REAIS do bloco de
        // texto (topo = baseline − ascent, fundo = baseline + descent) —
        // antes era a aproximação 0.30*altura, que com a fonte a 28 px
        // deixava os glifos descerem para a linha de baixo.
        const f32 asc = font_->ascent();
        const f32 desc = font_->descent();
        const f32 baseline = y + (h - asc - desc) * 0.5f + asc;
        label(x + (w - tw) * 0.5f, baseline, shown, txt);
    }
    return pressed;
}

bool UiContext::slider(u64 id, f32 x, f32 y, f32 w, f32 h, f32 minV, f32 maxV, f32& value) {
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
    if (slot == kNoScroll) {
        return;   // sem slots — ignora a região (não devia acontecer: 2 usos)
    }

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
        const f32 th = font_->height();
        labelFitted(r.x + 12.0f, r.y + kStatusH * 0.5f + th * 0.30f, text,
                    theme::TEXT, r.w - 24.0f);
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
