#pragma once
// ui/UiContext.h — UI immediate-mode em C++ (tema mono, landscape).
// Fluxo: beginFrame → widgets (panel/label/button) → endFrame (submete batches).
// Layout F1: toolbar topo (exatamente 3 botões) + viewport + status line inferior.
#include "render/QuadBatch.h"
#include "render/Renderer.h"
#include "ui/FontAtlas.h"
#include "platform/InputState.h"

namespace vv {

// Tokens do tema mono — ÚNICA paleta permitida na F1 (cinza/branco/preto).
namespace theme {
constexpr f32 BG[4]     = {0.0784314f, 0.0784314f, 0.0784314f, 1.0f}; // #141414
constexpr f32 PANEL[4]  = {0.1176471f, 0.1176471f, 0.1176471f, 1.0f}; // #1E1E1E
constexpr f32 LINE[4]   = {0.1803922f, 0.1803922f, 0.1803922f, 1.0f}; // #2E2E2E
constexpr f32 TEXT[4]   = {0.9019608f, 0.9019608f, 0.9019608f, 1.0f}; // #E6E6E6
constexpr f32 ACCENT[4] = {0.9607843f, 0.9607843f, 0.9607843f, 1.0f}; // #F5F5F5
}

struct UiRect {
    f32 x, y, w, h;
};

class UiContext {
public:
    void init();   // reserva capacidade dos batches
    void setFont(FontAtlas* font) { font_ = font; }

    void beginFrame(Renderer* renderer, const InputState* input, f32 screenW, f32 screenH);
    void endFrame();   // submete solids + glyphs ao renderer

    // widgets
    void panel(f32 x, f32 y, f32 w, f32 h, const f32 color[4]);
    void frame(f32 x, f32 y, f32 w, f32 h, f32 thickness, const f32 color[4]);
    void label(f32 xBaseline, f32 yBaseline, const char* text, const f32 color[4]);
    bool button(u64 id, f32 x, f32 y, f32 w, f32 h, const char* text);

    // F3: slider horizontal immediate-mode (Inspector do Transform3D).
    // Escreve em `value` (clamp [minV,maxV]); devolve true se mudou este frame.
    // Partilha o mesmo active_ dos botões — um widget interativo por gesto.
    bool slider(u64 id, f32 x, f32 y, f32 w, f32 h, f32 minV, f32 maxV, f32& value);

    // F3: accessors usados pelos painéis do editor (EditorUi).
    bool hasFont() const { return font_ && font_->ok(); }
    f32  fontWidth(const char* text) const { return font_ ? font_->widthOf(text) : 0.0f; }
    f32  fontHeight() const { return font_ ? font_->height() : 0.0f; }
    f32  screenWidth() const { return sw_; }
    f32  screenHeight() const { return sh_; }

    // layout landscape F1
    void toolbar(bool outClicks[3]);   // exatamente 3 botões: Menu, Play, Settings
    void statusLine(const char* text); // fps + contagem de TICs

    UiRect toolbarRect() const {
        return {0.0f, 0.0f, sw_, kToolbarH};
    }
    UiRect statusRect() const {
        return {0.0f, sh_ - kStatusH, sw_, kStatusH};
    }
    UiRect viewportRect() const {
        return {0.0f, kToolbarH, sw_, sh_ - kToolbarH - kStatusH};
    }

    static constexpr f32 kToolbarH = 88.0f;
    static constexpr f32 kStatusH  = 40.0f;

private:
    Renderer*         renderer_ = nullptr;
    const InputState* input_ = nullptr;
    FontAtlas*        font_ = nullptr;
    f32               sw_ = 0.0f;
    f32               sh_ = 0.0f;
    u64               active_ = 0;   // botão pressionado (immediate mode)
    QuadBatch         solids_;
    QuadBatch         glyphs_;
};

} // namespace vv
