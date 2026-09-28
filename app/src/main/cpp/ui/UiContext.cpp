#include "ui/UiContext.h"
#include <cstring>

namespace vv {

namespace {
constexpr u32 kMaxQuadsUi = 4096;
constexpr f32 kBtnW   = 240.0f;
constexpr f32 kBtnH   = 56.0f;
constexpr f32 kBtnGap = 16.0f;
constexpr f32 kPad    = 16.0f;
} // namespace

void UiContext::init() {
    solids_.reserve(kMaxQuadsUi);
    glyphs_.reserve(kMaxQuadsUi);
}

void UiContext::beginFrame(Renderer* renderer, const InputState* input,
                           f32 screenW, f32 screenH) {
    renderer_ = renderer;
    input_ = input;
    sw_ = screenW;
    sh_ = screenH;
    solids_.clear();
    glyphs_.clear();
}

void UiContext::panel(f32 x, f32 y, f32 w, f32 h, const f32 color[4]) {
    solids_.quad(x, y, w, h, 0.0f, 0.0f, 1.0f, 1.0f,
                 color[0], color[1], color[2], color[3]);
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
    f32 penX = xBaseline;
    for (const char* p = text; *p; ++p) {
        const char c = *p;
        if (c < static_cast<char>(FontAtlas::kFirstChar) ||
            c >= static_cast<char>(FontAtlas::kFirstChar + FontAtlas::kNumChars)) {
            penX += font_->height() * 0.30f;
            continue;
        }
        const Glyph& g = font_->glyph(c);
        glyphs_.quad(penX + g.xoff, yBaseline + g.yoff, g.w, g.h,
                     g.u0, g.v0, g.u1, g.v1,
                     color[0], color[1], color[2], color[3]);
        penX += g.xadv;
    }
}

bool UiContext::button(u64 id, f32 x, f32 y, f32 w, f32 h, const char* text) {
    bool pressed = false;

    const bool down = input_ && input_->down(0);
    f32 px = -1.0f, py = -1.0f;
    if (input_) {
        input_->pos(0, px, py);
    }
    const bool inside = (px >= x && px < x + w && py >= y && py < y + h);

    if (down && inside && active_ == 0) {
        active_ = id;
    }
    if (active_ == id && !down) {
        if (inside) {
            pressed = true;
        }
        active_ = 0;
    }

    const bool held = (active_ == id && down);
    const f32* bg  = held ? theme::ACCENT : theme::PANEL;
    const f32* txt = held ? theme::BG     : theme::TEXT;
    panel(x, y, w, h, bg);
    frame(x, y, w, h, 1.0f, theme::LINE);

    if (font_ && font_->ok() && text) {
        const f32 tw = font_->widthOf(text);
        const f32 th = font_->height();
        // baseline ≈ centro + 0.30*altura (aproximação do ascent do atlas)
        label(x + (w - tw) * 0.5f, y + h * 0.5f + th * 0.30f, text, txt);
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

    if (down && inside && active_ == 0) {
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

void UiContext::toolbar(bool outClicks[3]) {
    static const char* kNames[3] = { "Menu", "Play", "Settings" };

    const UiRect r = toolbarRect();
    panel(r.x, r.y, r.w, r.h, theme::PANEL);
    panel(r.x, r.y + r.h - 1.0f, r.w, 1.0f, theme::LINE);   // separador inferior

    for (u32 i = 0; i < 3; ++i) {
        const f32 bx = kPad + static_cast<f32>(i) * (kBtnW + kBtnGap);
        const f32 by = (kToolbarH - kBtnH) * 0.5f;
        outClicks[i] = button(1 + i, bx, by, kBtnW, kBtnH, kNames[i]);
    }
}

void UiContext::statusLine(const char* text) {
    const UiRect r = statusRect();
    panel(r.x, r.y, r.w, r.h, theme::PANEL);
    panel(r.x, r.y, r.w, 1.0f, theme::LINE);   // separador superior

    if (font_ && font_->ok() && text) {
        const f32 th = font_->height();
        label(12.0f, r.y + kStatusH * 0.5f + th * 0.30f, text, theme::TEXT);
    }
}

void UiContext::endFrame() {
    if (!renderer_) {
        return;
    }
    renderer_->submit(solids_, renderer_->whiteTexture());
    if (font_ && font_->ok()) {
        renderer_->submit(glyphs_, font_->texture());
    }
}

} // namespace vv
