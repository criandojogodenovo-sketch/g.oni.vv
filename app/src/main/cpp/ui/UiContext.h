#pragma once
// ui/UiContext.h — UI immediate-mode em C++ (tema mono, landscape).
// Fluxo: beginFrame → widgets (panel/label/button) → endFrame (submete batches).
// Layout F1: toolbar topo (exatamente 3 botões) + viewport + status line inferior.
// F4.1: primitivo de scroll — beginScroll(id, region, contentHeight) /
// endScroll() com drag-to-scroll, clamp e indicador (matemática em
// ui/ScrollMath.h, GL-free e testada no CI); quads desenhados dentro da
// região são RECORTADOS por interseção (sem glScissor — quad batch único).
#include "render/QuadBatch.h"
#include "render/Renderer.h"
#include "ui/FontAtlas.h"
#include "ui/ScrollMath.h"
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

// UiRect vive em ui/ScrollMath.h (matemática GL-free partilhada)

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

    // F4.1: região de scroll reutilizável (immediate-mode; estado por id em
    // slots fixos — o offset persiste entre frames). Entre begin/end, os quads
    // de panel/label são recortados à região e os BOTÕES só desenham (o tap é
    // re-despachado pelo painel via scrollTap — drag em qualquer sítio =
    // scroll; sliders mantêm a prioridade de captura).
    void beginScroll(u64 id, const UiRect& region, f32 contentHeight);
    void endScroll();
    f32  scrollOffset() const;   // offset da região aberta (após beginScroll)
    bool scrollTap(f32& x, f32& y);   // consome o tap re-despachado (1 frame)

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
    // emite um quad recortado pelo clip_ (panel/label passam por aqui)
    bool emitTo(QuadBatch& b, f32 x, f32 y, f32 w, f32 h,
                f32 u0, f32 v0, f32 u1, f32 v1, const f32 color[4]);

    Renderer*         renderer_ = nullptr;
    const InputState* input_ = nullptr;
    FontAtlas*        font_ = nullptr;
    f32               sw_ = 0.0f;
    f32               sh_ = 0.0f;
    u64               active_ = 0;   // botão pressionado (immediate mode)
    QuadBatch         solids_;
    QuadBatch         glyphs_;

    // ---- F4.1: scroll -------------------------------------------------------
    struct ScrollSlot {
        u64          id = 0;
        bool         used = false;
        scroll::State st;
        UiRect       region{};
        f32          contentH = 0.0f;
    };
    static constexpr u32 kMaxScrollSlots = 8;
    static constexpr i32 kNoScroll = -1;
    ScrollSlot scrollSlots_[kMaxScrollSlots];
    i32        scrollCur_    = kNoScroll;   // região aberta neste frame
    bool       inScroll_     = false;       // entre begin/endScroll
    bool       scrollPending_= false;       // press edge à espera de claim
    u64        pendingId_    = 0;
    UiRect     clip_         = {0.0f, 0.0f, 1e9f, 1e9f};
    bool       tapValid_     = false;
    f32        tapX_ = 0.0f, tapY_ = 0.0f;
};

} // namespace vv
