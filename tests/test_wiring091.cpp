// tests/test_wiring091.cpp — 0.9.1: ORIENTAÇÃO (portrait nas janelas de
// texto) + IME DO SISTEMA (fila Java→engine) + JANELA DE TEXTO (a semente
// do editor de script 0.9.2).
//
// Cobertura nova desta release (a suíte antiga continua a cobrir o resto):
//   IME queue (puro): pushText/poll round-trip · FIFO · texto vazio ignora ·
//                     pushKey(None) ignora · mapa de keycodes · teto 256
//   ORIENTAÇÃO:       estado inicial landscape · portrait muda (true) ·
//                     repetição NÃO muda (false) · volta landscape (true)
//   TEXTWIN (puro):   open limpa · texto append · ENTER quebra linha ·
//                     DEL apaga 1 CODE POINT UTF-8 (acento morre inteiro) ·
//                     setas v0 ignoradas · eventos com a janela FECHADA
//                     ignorados (não contaminam o teclado in-app)
//   TEXTWIN (draw):   janela 720×1536 PORTRAIT desenha · back 56dp devolve
//                     1 · caret pisca (0.6s on/off) · buffer com muitas
//                     linhas roda para o FIM · lineCount
//   MODAL:            st.textWin.open entra no anyOverlayOpen (nada do
//                     editor desenha/interage atrás da janela)
//   SETTINGS:         a linha Diagnóstico→"editor de texto (IME)" devolve
//                     kOpenTextWindow (secções acima colapsadas p/ visibilidade)
// Os casos do CAMINHO DO DEVICE (JNI ponte+fila+INIT re-upload na rotação)
// estão no TU do test_wiring087 (único que inclui platform/main.cpp).
#include "TestFramework.h"

#include "core/Handle.h"
#include "core/Scene.h"
#include "platform/ImeQueue.h"
#include "platform/InputState.h"
#include "ui/EditorUi.h"
#include "ui/FontAtlas.h"
#include "ui/ScrollMath.h"
#include "ui/SettingsPage.h"
#include "ui/TextWindow.h"
#include "ui/Theme.h"
#include "ui/Toolbar.h"
#include "ui/UiContext.h"
#include "ui/UiEditor.h"   // anyOverlayOpen

#include <cmath>
#include <cstdio>
#include <string>

using namespace vv;
using namespace vv::editor;

namespace {

constexpr f32 kSW = 1600.0f;
constexpr f32 kSH = 720.0f;
constexpr f32 kPW = 720.0f;    // o C33 em PORTRAIT (a rotação do device)
constexpr f32 kPH = 1536.0f;

bool nearEqF(f32 a, f32 b, f32 eps = 1.0f) {
    return std::fabs(a - b) <= eps;
}

struct Env {
    FontAtlas  font;
    UiContext  ui;
    InputState input;
    EditorState st;

    Env() {
        const char* fp = FONT_FIXTURE;
        font.loadFromPaths(&fp, 1, 28.0f);
        ui.init();
        ui.setFont(&font);
        ui.setSafeArea(safe::Insets{});
    }

    // frame de JANELA DE TEXTO em PORTRAIT (a janela é modal — nada mais
    // desenha; o main desenha-a topmost depois de tudo)
    void twFrame(f32 dt = 0.0f) {
        ui.beginFrame(nullptr, &input, kPW, kPH);
        textwin::draw(ui, input, st.textWin, kPW, kPH, dt);
        ui.endFrame();
        input.clearEdges();
    }

    void tap(f32 x, f32 y) {
        input.injectDown(0, x, y);
        ui.beginFrame(nullptr, &input, kPW, kPH);
        textwin::draw(ui, input, st.textWin, kPW, kPH, 0.0f);
        ui.endFrame();
        input.injectUp(0);
        ui.beginFrame(nullptr, &input, kPW, kPH);
        textwin::draw(ui, input, st.textWin, kPW, kPH, 0.0f);
        ui.endFrame();
        input.clearEdges();
    }
};

} // namespace

// ---------------------------------------------------------------------------
// IME queue (puro)
// ---------------------------------------------------------------------------
TEST(ime_queue_text_roundtrip_fifo) {
    ime::clearForTest();
    EXPECT(ime::pending() == 0);
    ime::pushText("Ola");
    ime::pushText(" mundo");
    ime::Event ev;
    EXPECT(ime::poll(ev));
    EXPECT(ev.isText && ev.text == "Ola");
    EXPECT(ime::poll(ev));
    EXPECT(ev.isText && ev.text == " mundo");
    EXPECT(!ime::poll(ev));   // FIFO esgotado
}

TEST(ime_queue_texto_vazio_e_key_none_sao_ignorados) {
    ime::clearForTest();
    ime::pushText("");          // commit vazio (composição limpa) — nada
    ime::pushKey(ime::Key::None);
    EXPECT(ime::pending() == 0);
}

TEST(ime_queue_keycode_map) {
    EXPECT(ime::fromAndroidKeycode(67) == ime::Key::Del);    // KEYCODE_DEL
    EXPECT(ime::fromAndroidKeycode(66) == ime::Key::Enter);  // KEYCODE_ENTER
    EXPECT(ime::fromAndroidKeycode(19) == ime::Key::Up);
    EXPECT(ime::fromAndroidKeycode(22) == ime::Key::Right);
    EXPECT(ime::fromAndroidKeycode(42) == ime::Key::None);   // não mapeada
}

TEST(ime_queue_teto_256) {
    ime::clearForTest();
    for (int i = 0; i < 300; ++i) {
        ime::pushText("x");
    }
    // defesa anti-descontrolo: a fila nunca passa do teto
    EXPECT(ime::pending() <= 256);
    ime::clearForTest();
}

// ---------------------------------------------------------------------------
// Política de orientação (puro)
// ---------------------------------------------------------------------------
TEST(orientacao_estado_inicial_landscape_e_transicoes) {
    ime::clearForTest();
    EXPECT(ime::orientation() == ime::Orientation::Landscape);
    // abrir janela de texto: muda e devolve TRUE (o chamador faz o JNI)
    EXPECT(ime::setOrientation(ime::Orientation::Portrait, "teste abrir"));
    EXPECT(ime::orientation() == ime::Orientation::Portrait);
    // repetição: NÃO muda (e o JNI NÃO é re-disparado — sem spam no device)
    EXPECT(!ime::setOrientation(ime::Orientation::Portrait, "teste repetido"));
    // fechar: volta e devolve TRUE
    EXPECT(ime::setOrientation(ime::Orientation::Landscape, "teste fechar"));
    EXPECT(ime::orientation() == ime::Orientation::Landscape);
    // a mudança escreve linha de log (estado de orientação LOGADO — spec 0.9.1)
}

// ---------------------------------------------------------------------------
// TEXTWIN puro — applyEvent
// ---------------------------------------------------------------------------
TEST(textwin_open_limpa_e_apply_append_enter) {
    ime::clearForTest();
    textwin::State tw;
    tw.buf = "lixo da sessão anterior";
    textwin::open(tw);
    EXPECT(tw.open);
    EXPECT(tw.buf.empty());

    ime::Event ev;
    ev.isText = true; ev.text = "Ola";
    EXPECT(textwin::applyEvent(tw, ev));
    EXPECT(tw.buf == "Ola");
    ev.isText = false; ev.key = ime::Key::Enter;
    EXPECT(textwin::applyEvent(tw, ev));
    EXPECT(tw.buf == "Ola\n");
    ev.isText = true; ev.text = "mundo";
    EXPECT(textwin::applyEvent(tw, ev));
    EXPECT(tw.buf == "Ola\nmundo");
    EXPECT(textwin::lineCount(tw) == 2);
    EXPECT(ime::pending() == 0);
}

TEST(textwin_del_apaga_um_code_point_utf8) {
    textwin::State tw;
    textwin::open(tw);
    // "café" = c,a,f,é — o 'é' é 2 bytes UTF-8 (0xC3 0xA9)
    tw.buf = "caf\xC3\xA9";
    EXPECT(tw.buf.size() == 5);   // 4 code points, 5 bytes
    ime::Event ev;
    ev.isText = false; ev.key = ime::Key::Del;
    EXPECT(textwin::applyEvent(tw, ev));
    EXPECT(tw.buf == "caf");     // os 2 bytes do 'é' saíram JUNTOS
    EXPECT(textwin::applyEvent(tw, ev));
    EXPECT(tw.buf == "ca");
    EXPECT(textwin::applyEvent(tw, ev));
    EXPECT(tw.buf == "c");
    // DEL em buffer vazio: sem crash, sem mudança
    tw.buf.clear();
    EXPECT(textwin::applyEvent(tw, ev));
    EXPECT(tw.buf.empty());
}

TEST(textwin_setas_v0_ignoradas_e_fechada_ignora_tudo) {
    textwin::State tw;
    textwin::open(tw);
    tw.buf = "abc";
    ime::Event ev;
    ev.isText = false; ev.key = ime::Key::Left;
    EXPECT(!textwin::applyEvent(tw, ev));   // cursor editável é do 0.9.2
    EXPECT(tw.buf == "abc");
    // janela FECHADA: o rascunho descarta (close limpa) e eventos do IME
    // não vingam — não contaminam o teclado in-app (a coexistência dos dois
    // inputs é por exclusão de contexto)
    textwin::close(tw);
    EXPECT(!tw.open);
    EXPECT(tw.buf.empty());
    ev.isText = true; ev.text = "invasao";
    EXPECT(!textwin::applyEvent(tw, ev));
    EXPECT(tw.buf.empty());
}

// ---------------------------------------------------------------------------
// TEXTWIN draw — portrait 720×1536, back 56dp, caret, scroll ao fim
// ---------------------------------------------------------------------------
TEST(textwin_draw_portrait_back_56dp_devolve_1) {
    Env e;
    textwin::open(e.st.textWin);
    e.st.textWin.buf = "linha um\nlinha dois";

    // frame em PORTRAIT (a janela adapta-se à superfície rodada — o resize
    // passa pelo caminho contentRect/resize de sempre)
    e.twFrame();
    EXPECT(e.st.textWin.open);

    // BACK: a célula inteira é o alvo (56dp ≥ 48dp da spec A)
    e.tap(28.0f, 28.0f);
    EXPECT(e.st.textWin.open);   // o draw SINALIZA (return 1) — quem fecha é
                                 // o main (par landscape+imeHide); aqui o
                                 // Estado continua, o main decide
}

TEST(textwin_draw_devolve_1_quando_o_back_e_tocado) {
    Env e;
    textwin::open(e.st.textWin);
    // 1º frame: armazenar o press
    e.input.injectDown(0, 28.0f, 28.0f);
    e.ui.beginFrame(nullptr, &e.input, kPW, kPH);
    const int r1 = textwin::draw(e.ui, e.input, e.st.textWin, kPW, kPH, 0.0f);
    e.ui.endFrame();
    e.input.injectUp(0);
    e.ui.beginFrame(nullptr, &e.input, kPW, kPH);
    const int r2 = textwin::draw(e.ui, e.input, e.st.textWin, kPW, kPH, 0.0f);
    e.ui.endFrame();
    e.input.clearEdges();
    EXPECT(r1 == 0);   // press não é clique
    EXPECT(r2 == 1);   // release dentro = BACK pedido
}

TEST(textwin_caret_pisca_06s) {
    Env e;
    textwin::open(e.st.textWin);
    e.st.textWin.buf = "abc";
    // blink < 0.6 → caret ON (painel accent emitido)
    e.twFrame(0.0f);
    const u32 on = e.ui.solidsForTest().vertexCount();
    EXPECT(on > 0);
    // blink 0.7 → dentro do período OFF (0.6..1.2)
    e.twFrame(0.7f);
    // os quads mudam (o caret saiu — só o fundo/barra ficam)
    const u32 off = e.ui.solidsForTest().vertexCount();
    EXPECT(off < on);
}

TEST(textwin_scroll_segue_o_fim_em_buffer_longo) {
    Env e;
    textwin::open(e.st.textWin);
    for (int i = 0; i < 80; ++i) {
        e.st.textWin.buf += "linha " + std::to_string(i) + "\n";
    }
    EXPECT(textwin::lineCount(e.st.textWin) == 81);
    e.twFrame();
    // a MESMA matemática do draw: lineHeight = métricas REAIS da fonte
    // (regra F5.0 — o teste não assume 28px)
    const TextMetrics m = e.ui.textMetrics();
    const f32 lh = (m.ascent + m.descent + 6.0f) > 28.0f
                       ? (m.ascent + m.descent + 6.0f) : 28.0f;
    const f32 contentH = 81.0f * lh + 16.0f;
    const f32 viewH = kPH - textwin::kTopH;
    EXPECT(contentH > viewH);   // o buffer ENCHE o corpo — o scroll ativa
    e.ui.beginScroll(textwin::kScrollId,
                     UiRect{0.0f, textwin::kTopH, kPW, viewH}, contentH);
    const f32 off = e.ui.scrollOffset();
    e.ui.endScroll();
    EXPECT(nearEqF(off, contentH - viewH, 1.0f));
}

// ---------------------------------------------------------------------------
// MODAL — a janela entra no gate anyOverlayOpen
// ---------------------------------------------------------------------------
TEST(textwin_abre_o_gate_modal_do_editor) {
    Env e;
    EXPECT(!anyOverlayOpen(e.st));
    textwin::open(e.st.textWin);
    EXPECT(anyOverlayOpen(e.st));   // nada do editor desenha/interage atrás
    textwin::close(e.st.textWin);
    EXPECT(!anyOverlayOpen(e.st));
}

// ---------------------------------------------------------------------------
// SETTINGS — a linha "editor de texto (IME)" devolve kOpenTextWindow
// ---------------------------------------------------------------------------
TEST(settings_diagnostico_linha_texto_devolve_kOpenTextWindow) {
    Env e;
    e.st.settingsMenu = true;
    // colapsa Geral/Audio/Permissoes para a linha do Diagnóstico subir para
    // dentro do viewport 1600×720 (a página é scrollable — o device rola;
    // o teste determinista encurta o caminho)
    e.st.settingsCollapsed =
        settings::kBitGeral | settings::kBitAudio | settings::kBitPerm;
    // um frame para o layout estabilizar (slots de scroll do UiContext)
    e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
    e.ui.endFrame();
    e.input.clearEdges();

    // y da linha: 8 (pad) + 3 headers colapsados ×48 + header Diag ×48 +
    // 7 linhas (logs/export/probe/CORRER BENCH/COPIAR RELATÓRIO +
    // EXPORTAR LAYOUT/AUDITORIA DO ECRÃ — os dois do G6/R-017 e os dois do
    // GRUPO B/R-024) ×48 + meia linha. O tap vai no BOTÃO COMPACTO da
    // linha (0.9.6.18 · D4b: outline compacto à direita — x no interior do
    // botão, ~w-60; o tap no DEVICE também mira o botão). O draw corre nos
    // DOIS frames do gesto (o widgetHit captura o press no frame do down).
    // 0.9.6 (G1): Settings ECRÃ CHEIO — sem a banda kToolbarH do overlayArea
    // 0.9.6.5 (GRUPO B): recalibrado às 2 linhas novas do Diagnóstico
    const f32 yRow = 56.0f + 8.0f + 3.0f * 48.0f +
                     48.0f + 7.0f * 48.0f + 24.0f;
    settings::Ctx ctx;
    ctx.version = "0.9.1 (vc 44)";
    e.input.injectDown(0, 1540.0f, yRow);   // dentro do botão compacto
    e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
    settings::draw(e.ui, e.input, e.st, ctx);
    e.ui.endFrame();
    e.input.injectUp(0);
    e.ui.beginFrame(nullptr, &e.input, kSW, kSH);
    const settings::Result r = settings::draw(e.ui, e.input, e.st, ctx);
    e.ui.endFrame();
    e.input.clearEdges();
    EXPECT(r == settings::kOpenTextWindow);
}
