// ui/AudioWorkspace.cpp — o WORKSPACE modo ÁUDIO (0.8.11).
//
// Ocupa o RECT DO VIEWPORT (o mesmo do editor de UI — nunca a área toda:
// toolbar/painéis/timeline ficam nos seus sítios, zero sobreposição por
// construção). Contém (o prompt): lista de clips (nome+duração+codec) ·
// Importar (abre o navegador) · Gravar/Parar (mic → .gi ADPCM) · Preview
// play/pause/stop · WAVEFORM (picos PCM, safe-area) · trim 🔶 (dívida) ·
// renomear (teclado in-app) · apagar (confirmação) · atribuir a TIC.
//
// O desenho é mono-panel com scroll (o padrão da casa). TODOS os
// ponteiros do host são OPCIONAIS (nunca desreferenciados sem guard —
// os testes usam fakes parciais).
#include "ui/AudioWorkspace.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "ui/Theme.h"

namespace vv {
namespace editor {

namespace {
constexpr u64 kIdClipBase   = 0x900000ull;   // lista de clips
constexpr u64 kIdImport     = 0x900100ull;
constexpr u64 kIdRecord     = 0x900101ull;
constexpr u64 kIdPlay       = 0x900102ull;
constexpr u64 kIdStop       = 0x900103ull;
constexpr u64 kIdRename     = 0x900104ull;
constexpr u64 kIdDelete     = 0x900105ull;
constexpr u64 kIdDeleteYes  = 0x900106ull;
constexpr u64 kIdDeleteNo   = 0x900107ull;
constexpr u64 kIdAssign     = 0x900108ull;
constexpr f32 kRowH = 46.0f;
constexpr u64 kScrollId = 7700;   // faixa própria (não colide com 41..46)
} // namespace

// o ESTADO do workspace vive no AudioWorkspaceState (main é o dono)
void audioWorkspaceReset(AudioWorkspaceState& st) {
    st.selected = 0;
    st.confirmDelete = false;
}

int drawAudioWorkspace(UiContext& ui, const InputState& in,
                       const UiRect& view, AudioWorkspaceState& st,
                       const std::vector<std::string>& clips,
                       AudioWorkspaceHost& host) {
    // devolve: 0 nada; 1..N = clip i-1 escolhido (seleção); os BOTÕES
    // agem pelo HOST (import/record/play/stop/delete/assign/rename)
    if (view.w < 100.0f || view.h < 100.0f) {
        return 0;
    }
    const f32 ox = view.x;
    const f32 oy = view.y;
    const f32 aw = view.w;
    const f32 ah = view.h;
    const f32 th = ui.fontHeight();

    // fundo dedicado (o modo ÁUDIO NÃO desenha a cena 3D — como o editor
    // de UI, o rect recebe um painel opaco)
    ui.panel(ox, oy, aw, ah, theme::BG);
    ui.frame(ox, oy, aw, ah, 1.0f, theme::LINE);

    // ---- barra de ações (topo): Importar · Gravar · Play · Stop ---------
    f32 y = oy + 6.0f;
    f32 x = ox + 6.0f;
    int chosen = 0;
    const f32 btnW = (aw - 12.0f - 3.0f * 8.0f) * 0.25f;
    if (ui.button(kIdImport, x, y, btnW, 44.0f, "Importar...")) {
        if (host.onImport) {
            host.onImport();
        }
        st.confirmDelete = false;
    }
    x += btnW + 8.0f;
    {
        char rec[32];
        std::snprintf(rec, sizeof(rec), host.recording ? "PARAR (%ds)"
                                                        : "Gravar",
                      host.recordSecs);
        if (ui.button(kIdRecord, x, y, btnW, 44.0f, rec)) {
            if (host.onRecord) {
                host.onRecord();
            }
        }
    }
    x += btnW + 8.0f;
    if (ui.button(kIdPlay, x, y, btnW, 44.0f,
                  host.previewing ? "Pausa" : "Play")) {
        if (host.onPreviewToggle) {
            host.onPreviewToggle();
        }
    }
    x += btnW + 8.0f;
    if (ui.button(kIdStop, x, y, btnW, 44.0f, "Stop")) {
        if (host.onPreviewStop) {
            host.onPreviewStop();
        }
    }
    y += 52.0f;

    // ---- overlay de gravação (medidor + temporizador) -------------------
    if (host.recording) {
        ui.panel(ox + 6.0f, y, aw - 12.0f, 54.0f, theme::PANEL);
        ui.frame(ox + 6.0f, y, aw - 12.0f, 54.0f, 2.0f, theme::ACCENT);
        char line[96];
        std::snprintf(line, sizeof(line), "A GRAVAR... %d s (toque PARAR)",
                      host.recordSecs);
        ui.labelFitted(ox + 18.0f, y + 14.0f + th * 0.3f, line, theme::TEXT,
                       aw - 36.0f);
        // medidor: barra do nível (0..1)
        const f32 lvl = host.recordLevel < 0.0f ? 0.0f
            : (host.recordLevel > 1.0f ? 1.0f : host.recordLevel);
        const f32 mw = aw - 36.0f;
        ui.panel(ox + 18.0f, y + 36.0f, mw, 10.0f, theme::LINE);
        ui.panel(ox + 18.0f, y + 36.0f, mw * lvl, 10.0f, theme::ACCENT);
        y += 62.0f;
    }

    // ---- LISTA de clips (nome + duração + codec) com scroll -------------
    // (os botões DENTRO do scroll só desenham — o scroll reclama o gesto e
    // o tap é RE-DESPACHADO depois do endScroll, o padrão da Hierarchy/
    // navegador; ver widgetHit: scroll::buttonCaptures)
    const f32 listTop = y;
    const f32 listH = ah - (y - oy) - 196.0f;   // deixa wave+ações em baixo
    const u32 n = static_cast<u32>(clips.size());
    const f32 contentH = 30.0f + static_cast<f32>(n) * kRowH;
    const UiRect region{ox + 6.0f, listTop, aw - 12.0f,
                        listH > 80.0f ? listH : 80.0f};
    ui.beginScroll(kScrollId, region, contentH);
    const f32 off = ui.scrollOffset();
    f32 cy = listTop - off;
    ui.labelFitted(ox + 10.0f, cy + 4.0f + th * 0.3f,
                   n ? "CLIPS (audio/)" : "(sem clips — Importar ou Gravar)",
                   theme::LINE, aw - 20.0f);
    cy += 30.0f;
    const f32 rowsTop = cy;   // onde a 1ª linha começa (coords de ecrã)
    for (u32 i = 0; i < n; ++i) {
        if (i == st.selected) {
            ui.panel(ox + 8.0f, cy + 2.0f, aw - 16.0f, kRowH - 4.0f,
                     theme::PANEL);
            ui.frame(ox + 8.0f, cy + 2.0f, aw - 16.0f, kRowH - 4.0f, 2.0f,
                     theme::ACCENT);
        }
        // nome (sem pasta/extensão) + meta na direita
        const std::string& rel = clips[i];
        const size_t slash = rel.rfind('/');
        const size_t dot = rel.rfind('.');
        const std::string nome =
            rel.substr(slash + 1, dot == std::string::npos
                                     ? std::string::npos
                                     : dot - slash - 1);
        char meta[64];
        std::snprintf(meta, sizeof(meta), "%s  %.1fs",
                      host.codecOf ? host.codecOf(rel) : "?",
                      host.durationOf ? host.durationOf(rel) : 0.0f);
        ui.labelFitted(ox + 16.0f, cy + kRowH * 0.5f - th * 0.5f + th * 0.3f,
                       nome.c_str(), theme::TEXT, aw * 0.6f);
        const f32 mw = ui.hasFont() ? ui.fontWidth(meta) : 80.0f;
        ui.label(ox + aw - mw - 16.0f,
                 cy + kRowH * 0.5f - th * 0.5f + th * 0.3f, meta, theme::LINE);
        ui.button(kIdClipBase + i, ox + 10.0f, cy + 2.0f, aw - 20.0f,
                  kRowH - 4.0f, "");   // só desenha (o tap vem no re-despacho)
        cy += kRowH;
    }
    ui.endScroll();
    // tap re-despachado → linha da lista (a MESMA geometria desenhada)
    {
        f32 tx = 0.0f, ty = 0.0f;
        if (ui.scrollTap(kScrollId, tx, ty)) {
            const i32 row = static_cast<i32>((ty - rowsTop + off) / kRowH);
            if (row >= 0 && static_cast<u32>(row) < n &&
                tx >= ox + 10.0f && tx < ox + aw - 10.0f) {
                st.selected = static_cast<u32>(row);
                st.confirmDelete = false;
                chosen = row + 1;
            }
        }
    }
    y = listTop + region.h + 10.0f;

    // ---- WAVEFORM do clip selecionado (picos PCM) ------------------------
    if (n > 0 && st.selected < n) {
        ui.panel(ox + 6.0f, y, aw - 12.0f, 96.0f, theme::PANEL);
        ui.frame(ox + 6.0f, y, aw - 12.0f, 96.0f, 2.0f, theme::ACCENT);
        const std::string& rel = clips[st.selected];
        const std::vector<f32> empty;
        const std::vector<f32>& peaks =
            host.peaksOf ? host.peaksOf(rel) : empty;
        const f32 midY = y + 48.0f;
        const f32 half = 40.0f;
        const f32 wx = ox + 14.0f;
        const f32 ww = aw - 28.0f;
        if (!peaks.empty()) {
            const f32 barW = ww / static_cast<f32>(peaks.size());
            for (size_t b = 0; b < peaks.size(); ++b) {
                const f32 h = half * peaks[b];
                if (h < 0.5f) {
                    continue;
                }
                ui.panel(wx + static_cast<f32>(b) * barW, midY - h,
                         barW > 1.0f ? barW - 0.5f : 0.5f, h * 2.0f,
                         theme::ACCENT);
            }
        } else {
            ui.labelFitted(ox + 16.0f, midY - th * 0.5f,
                           "(waveform a carregar...)", theme::LINE, aw - 32.0f);
        }
        // linha do tempo do preview (posição do cursor de reprodução)
        if (host.previewing && host.previewPos) {
            const f32 frac = host.previewPos();
            if (frac >= 0.0f && frac <= 1.0f) {
                ui.panel(wx + ww * frac, y + 4.0f, 2.0f, 88.0f,
                         theme::TEXT);
            }
        }
        y += 102.0f;

        // ---- ações do clip: renomear · apagar · atribuir -----------------
        f32 ax = ox + 6.0f;
        const f32 abW = (aw - 12.0f - 2.0f * 8.0f) * 0.3333f;
        if (ui.button(kIdRename, ax, y, abW, 40.0f, "renomear")) {
            if (host.onRename) {
                host.onRename();
            }
            st.confirmDelete = false;
        }
        ax += abW + 8.0f;
        if (ui.button(kIdDelete, ax, y, abW, 40.0f, "apagar")) {
            st.confirmDelete = !st.confirmDelete;
        }
        ax += abW + 8.0f;
        if (ui.button(kIdAssign, ax, y, abW, 40.0f, "atribuir a TIC")) {
            if (host.onAssign) {
                host.onAssign();
            }
            st.confirmDelete = false;
        }
        y += 46.0f;
        // confirmação de APAGAR (nunca sem confirmar — o prompt exige)
        if (st.confirmDelete) {
            char q[96];
            std::snprintf(q, sizeof(q), "apagar '%s'?",
                          rel.substr(rel.rfind('/') + 1).c_str());
            ui.panel(ox + 6.0f, y, aw - 12.0f, 52.0f, theme::PANEL);
            ui.labelFitted(ox + 16.0f, y + 8.0f + th * 0.3f, q, theme::TEXT,
                           aw - 32.0f);
            if (ui.button(kIdDeleteYes, ox + aw * 0.5f - 120.0f, y + 26.0f,
                          112.0f, 22.0f, "APAGAR")) {
                if (host.onDelete) {
                    host.onDelete(rel);
                }
                st.confirmDelete = false;
            }
            if (ui.button(kIdDeleteNo, ox + aw * 0.5f + 8.0f, y + 26.0f,
                          112.0f, 22.0f, "manter")) {
                st.confirmDelete = false;
            }
        }
    }
    (void)in;
    return chosen;
}

} // namespace editor
} // namespace vv
