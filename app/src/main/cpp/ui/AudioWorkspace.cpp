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
// FASE UI I: os ids mudaram para o HEADER (os pins do CI aférram-nos —
// o padrão ScriptEditor.h); este ns guarda só a GEOMETRIA do orçamento.
constexpr f32 kRowH = 46.0f;
constexpr u64 kScrollId = 7700;   // faixa própria (não colide com 41..46)

// FASE UI I (a ordem do dono) — O ORÇAMENTO VERTICAL do workspace (a fonte
// única do empilhamento: a lista é a parte FLEXÍVEL e o resto é FIXO —
// antes o rótulo «CLIPS» vivia DENTRO do scroll e a reserva fixa de baixo
// (196dp) podia ESTOURAR o viewport com o piso da lista: a waveform e as
// ações desenhavam PARA ALÉM do rect — a sobreposição que o dono mandou
// matar). Os valores seguem a fase: toque ≥40dp, waveform ~80dp FIXA.
constexpr f32 kBtnH     = 44.0f;   // a barra Importar/Gravar/Play/Stop
constexpr f32 kHeaderH  = 22.0f;   // a faixa FIXA do rótulo CLIPS (fora do
                                   // scroll — nunca sob os botões nem sob
                                   // a lista)
constexpr f32 kWaveH    = 80.0f;   // a waveform FIXA (~80dp — a ordem)
constexpr f32 kActH     = 40.0f;   // as ações do clip numa linha (o toque)
constexpr f32 kListMinH = 40.0f;   // o piso da lista flexível
constexpr f32 kPadX     = 6.0f;    // o respiro lateral (o de sempre)
constexpr f32 kGap      = 8.0f;    // o vão da escala do dono
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
    if (ui.button(kAudioIdImport, x, y, btnW, kBtnH, "Importar...")) {
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
        if (ui.button(kAudioIdRecord, x, y, btnW, kBtnH, rec)) {
            if (host.onRecord) {
                host.onRecord();
            }
        }
    }
    x += btnW + 8.0f;
    if (ui.button(kAudioIdPlay, x, y, btnW, kBtnH,
                  host.previewing ? "Pausa" : "Play")) {
        if (host.onPreviewToggle) {
            host.onPreviewToggle();
        }
    }
    x += btnW + 8.0f;
    if (ui.button(kAudioIdStop, x, y, btnW, kBtnH, "Stop")) {
        if (host.onPreviewStop) {
            host.onPreviewStop();
        }
    }
    const f32 btnsBot = y + kBtnH;   // o fundo da barra (o orçamento começa)

    // ---- overlay de gravação (medidor + temporizador) -------------------
    // (fixo ENTRE a barra e o cabeçalho da lista — o orçamento desconta-o)
    f32 recH = 0.0f;
    if (host.recording) {
        recH = 62.0f;
        ui.panel(ox + 6.0f, btnsBot + 4.0f, aw - 12.0f, 54.0f, theme::PANEL);
        ui.frame(ox + 6.0f, btnsBot + 4.0f, aw - 12.0f, 54.0f, 2.0f,
                 theme::ACCENT);
        char line[96];
        std::snprintf(line, sizeof(line), "A GRAVAR... %d s (toque PARAR)",
                      host.recordSecs);
        ui.labelFitted(ox + 18.0f, btnsBot + 18.0f + th * 0.3f, line,
                       theme::TEXT, aw - 36.0f);
        // medidor: barra do nível (0..1)
        const f32 lvl = host.recordLevel < 0.0f ? 0.0f
            : (host.recordLevel > 1.0f ? 1.0f : host.recordLevel);
        const f32 mw = aw - 36.0f;
        ui.panel(ox + 18.0f, btnsBot + 40.0f, mw, 10.0f, theme::LINE);
        ui.panel(ox + 18.0f, btnsBot + 40.0f, mw * lvl, 10.0f, theme::ACCENT);
    }

    // ---- O ORÇAMENTO VERTICAL (a fonte única — de baixo para cima) ------
    // ações do clip (40) → [waveform FIXA 80 + vão] → lista flexível →
    // cabeçalho CLIPS (22, FIXO fora do scroll) → barra de ações (44).
    // A lista CEDE; a waveform só entra se a lista ficar com o piso; sem
    // lugar para a faixa do cabeçalho (viewport minúsculo) o workspace não
    // desenha (o guard honesto — nada de empilhar por cima do rect).
    const f32 actionsY = oy + ah - 4.0f - kActH;        // a linha de ações
    f32 waveY = actionsY - kGap - kWaveH;               // a waveform FIXA
    f32 listTop = btnsBot + recH + 4.0f + kHeaderH + 2.0f;
    f32 listBot = waveY - kGap;
    f32 listH = listBot - listTop;
    const u32 n = static_cast<u32>(clips.size());
    const bool wantWave = n > 0 && st.selected < n;
    bool showWave = wantWave;
    if (showWave && listH < kListMinH) {
        showWave = false;   // a lista tem prioridade sobre a pré-escuta
    }
    if (!showWave) {
        listBot = actionsY - kGap;
        listH = listBot - listTop;
    }
    if (listH < kListMinH) {
        return 0;   // viewport abaixo do orçamento mínimo — nada a empilhar
    }

    // ---- o RÓTULO CLIPS (faixa FIXA — fora do scroll: nunca sob os
    // botões nem sob a lista — a ordem literal) ---------------------------
    {
        const TextMetrics tmH = ui.textMetrics();
        ui.labelFitted(ox + 10.0f, listTop - kHeaderH
                                       + (kHeaderH - tmH.block()) * 0.5f
                                       + tmH.ascent,
                       n ? "CLIPS (audio/)" : "CLIPS (audio/) — vazio",
                       theme::kTheme.text2, aw - 20.0f);
    }

    // ---- LISTA de clips (nome + duração + codec) com scroll -------------
    // (os botões DENTRO do scroll só desenham — o scroll reclama o gesto e
    // o tap é RE-DESPACHADO depois do endScroll, o padrão da Hierarchy/
    // navegador; ver widgetHit: scroll::buttonCaptures)
    const UiRect region{ox + kPadX, listTop, aw - 2.0f * kPadX, listH};
    const f32 contentH = static_cast<f32>(n) * kRowH;
    ui.beginScroll(kScrollId, region, contentH);
    const f32 off = ui.scrollOffset();
    f32 cy = listTop - off;
    const f32 rowsTop = cy;   // onde a 1ª linha começa (coords de ecrã)
    if (n == 0) {
        // o ESTADO VAZIO da lista (legível — o text2 5,5:1 da casa, nunca
        // o border quase invisível de antes)
        const TextMetrics tmE = ui.textMetrics();
        ui.labelFitted(ox + 16.0f,
                       listTop + (listH - tmE.block()) * 0.5f + tmE.ascent,
                       "(sem clips — toca em Importar ou Gravar)",
                       theme::kTheme.text2, aw - 32.0f);
    }
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
                 cy + kRowH * 0.5f - th * 0.5f + th * 0.3f, meta,
                 theme::kTheme.text2);
        ui.button(kAudioIdClipBase + i, ox + 10.0f, cy + 2.0f, aw - 20.0f,
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

    // ---- WAVEFORM do clip selecionado (picos PCM) — ALTURA FIXA 80dp ----
    if (showWave) {
        ui.panel(ox + kPadX, waveY, aw - 2.0f * kPadX, kWaveH, theme::PANEL);
        ui.frame(ox + kPadX, waveY, aw - 2.0f * kPadX, kWaveH, 2.0f,
                 theme::ACCENT);
        const std::string& rel = clips[st.selected];
        const std::vector<f32> empty;
        const std::vector<f32>& peaks =
            host.peaksOf ? host.peaksOf(rel) : empty;
        const f32 midY = waveY + kWaveH * 0.5f;
        const f32 half = kWaveH * 0.5f - 6.0f;
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
            const TextMetrics tmW = ui.textMetrics();
            ui.labelFitted(ox + 16.0f, midY - tmW.block() * 0.5f,
                           "(waveform a carregar...)", theme::kTheme.text2,
                           aw - 32.0f);
        }
        // linha do tempo do preview (posição do cursor de reprodução)
        if (host.previewing && host.previewPos) {
            const f32 frac = host.previewPos();
            if (frac >= 0.0f && frac <= 1.0f) {
                ui.panel(wx + ww * frac, waveY + 4.0f, 2.0f, kWaveH - 8.0f,
                         theme::TEXT);
            }
        }
    }

    // ---- ações do clip: renomear · apagar · atribuir (NUMA LINHA) -------
    {
        f32 ax = ox + kPadX;
        const f32 abW = (aw - 12.0f - 2.0f * 8.0f) * 0.3333f;
        if (ui.button(kAudioIdRename, ax, actionsY, abW, kActH, "renomear")) {
            if (host.onRename) {
                host.onRename();
            }
            st.confirmDelete = false;
        }
        ax += abW + 8.0f;
        if (ui.button(kAudioIdDelete, ax, actionsY, abW, kActH, "apagar")) {
            st.confirmDelete = !st.confirmDelete;
        }
        ax += abW + 8.0f;
        if (ui.button(kAudioIdAssign, ax, actionsY, abW, kActH,
                      "atribuir a TIC")) {
            if (host.onAssign) {
                host.onAssign();
            }
            st.confirmDelete = false;
        }
    }

    // ---- confirmação de APAGAR (nunca sem confirmar — o prompt exige) ---
    // MODAL no rect da WAVEFORM (o mesmo sítio, desenhado DEPOIS — nada
    // cresce para lá do viewport; sem waveform (viewport apertado) o modal
    // cobre o fundo da lista — é modal, o dono decide APAGAR/manter já).
    if (st.confirmDelete && n > 0 && st.selected < n) {
        const f32 confY = showWave ? waveY : actionsY - kGap - kWaveH;
        const std::string& rel = clips[st.selected];
        ui.panel(ox + kPadX, confY, aw - 2.0f * kPadX, kWaveH, theme::PANEL);
        ui.frame(ox + kPadX, confY, aw - 2.0f * kPadX, kWaveH, 2.0f,
                 theme::kTheme.danger);
        char q[96];
        std::snprintf(q, sizeof(q), "apagar '%s'?",
                      rel.substr(rel.rfind('/') + 1).c_str());
        ui.labelFitted(ox + 16.0f, confY + 6.0f, q, theme::TEXT, aw - 32.0f);
        const f32 cbW = (aw - 2.0f * kPadX - kGap) * 0.5f;
        if (ui.button(kAudioIdDeleteYes, ox + kPadX, confY + 34.0f, cbW, 40.0f,
                      "APAGAR")) {
            if (host.onDelete) {
                host.onDelete(rel);
            }
            st.confirmDelete = false;
        }
        if (ui.button(kAudioIdDeleteNo, ox + kPadX + cbW + kGap, confY + 34.0f,
                      cbW, 40.0f, "manter")) {
            st.confirmDelete = false;
        }
    }
    (void)in;
    return chosen;
}

} // namespace editor
} // namespace vv
