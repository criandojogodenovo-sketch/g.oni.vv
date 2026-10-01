// ui/Timeline.cpp — editor de timeline do AnimationPlayer (0.8.0, F7).
//
// Tudo no quad batch da UiContext (tema mono, nada sobreposto); hit-test e
// desenho partilham os MESMOS rects (o padrão do editor desde a F5.0-fix).
#include "ui/Timeline.h"
#include "components/AnimationPlayer.h"
#include "components/Transform3D.h"
#include "components/UiCanvas.h"
#include "core/Scene.h"
#include "core/ComponentStore.h"
#include "math/Math.h"
#include "ui/EditorUi.h"
#include "ui/SafeArea.h"
#include <cstdio>
#include <cmath>

namespace vv {
namespace timeline {

namespace {

inline f32 rad2deg(f32 r) { return r * 57.2957795131f; }

// alvo tem componente onde escrever? (para os itens do overlay add-track)
bool targetAvailable(const Tic& tic, AnimTarget t) {
    switch (t) {
        case AnimTarget::TicPos:
        case AnimTarget::TicRot:
        case AnimTarget::TicScale:
            return tic.getComponent<Transform3D>() != nullptr;
        case AnimTarget::UiPos:
        case AnimTarget::UiColor:
        case AnimTarget::UiAlpha: {
            const UiCanvas* ui = tic.getComponent<UiCanvas>();
            return ui && !ui->elements.empty();
        }
    }
    return false;
}

// valor ATUAL da propriedade → sample para "+key"
void sampleTarget(const Tic& tic, AnimTarget t, const std::string& element,
                  f32 out[4]) {
    out[0] = out[1] = out[2] = 0.0f;
    out[3] = 1.0f;
    if (const Transform3D* tr = tic.getComponent<Transform3D>()) {
        switch (t) {
            case AnimTarget::TicPos:
                out[0] = tr->pos.x; out[1] = tr->pos.y; out[2] = tr->pos.z;
                return;
            case AnimTarget::TicRot: {
                f32 ex = 0.0f, ey = 0.0f, ez = 0.0f;
                Quat::toEuler(tr->rot, ex, ey, ez);
                out[0] = rad2deg(ex); out[1] = rad2deg(ey); out[2] = rad2deg(ez);
                return;
            }
            case AnimTarget::TicScale:
                out[0] = tr->scale.x; out[1] = tr->scale.y; out[2] = tr->scale.z;
                return;
            default:
                break;
        }
    }
    if (t == AnimTarget::UiPos || t == AnimTarget::UiColor ||
        t == AnimTarget::UiAlpha) {
        const UiCanvas* ui = tic.getComponent<UiCanvas>();
        if (!ui || element.empty()) {
            return;
        }
        const i32 idx = ui->findElement(element);
        if (idx < 0) {
            return;
        }
        const UiElement& e = ui->elements[static_cast<size_t>(idx)];
        if (t == AnimTarget::UiPos) {
            out[0] = e.ox; out[1] = e.oy;
        } else if (t == AnimTarget::UiColor) {
            out[0] = e.color[0]; out[1] = e.color[1]; out[2] = e.color[2];
        } else {
            out[0] = e.color[3];
        }
    }
}

} // namespace

bool visible(bool playMode, bool uiMode, const Scene& scene, Handle selected) {
    if (playMode || uiMode || !selected.valid()) {
        return false;
    }
    const Tic* tic = scene.get(selected);
    if (!tic || !tic->active) {
        return false;
    }
    return tic->getComponent<AnimationPlayer>() != nullptr;
}

UiRect timelineRect(f32 sw, f32 sh, const safe::Insets& in, bool rightPanel) {
    const UiRect c = safe::centerRect(sw, sh, in, rightPanel);
    return UiRect{c.x, c.y + c.h - kTimelineH, c.w, kTimelineH};
}

void stopPreview(Scene& scene, Handle ticH, State& st) {
    playSnapshotRestore(scene, st.snap);
    st.snap = PlaySnapshot{};
    if (!ticH.valid()) {
        return;
    }
    if (Tic* tic = scene.get(ticH)) {
        if (AnimationPlayer* pl = tic->getComponent<AnimationPlayer>()) {
            pl->playing = false;
            pl->time = 0.0f;
            pl->resetDir();
        }
    }
}

void drawTimeline(UiContext& ui, const InputState& in, Scene& scene,
                  editor::EditorState& st, State& tl, f32 dt) {
    // troca de seleção (ou TIC morreu) com preview a correr → restaura a
    // pose ANTES de seguir (nunca fica um TIC a meio de um clip no editor)
    if (tl.snap.captured && tl.previewTic != st.selected) {
        stopPreview(scene, tl.previewTic, tl);
    }
    Tic* tic = scene.get(st.selected);
    if (!tic || !tic->active) {
        return;
    }
    AnimationPlayer* pl = tic->getComponent<AnimationPlayer>();
    if (!pl) {
        return;
    }
    AnimClip* clip = pl->editClip();

    const f32 sw = ui.screenWidth();
    const f32 sh = ui.screenHeight();
    const UiRect r = timelineRect(sw, sh, ui.safeArea(), st.showInspector);

    // ---- painel (strip) ------------------------------------------------------
    ui.panel(r.x, r.y, r.w, kTimelineH, theme::PANEL);
    ui.frame(r.x, r.y, r.w, kTimelineH, 2.0f, theme::ACCENT);
    ui.panel(r.x, r.y + kHeaderH - 1.0f, r.w, 1.0f, theme::LINE);
    const f32 th = ui.fontHeight();

    // ---- preview (avança com dt REAL — editor, passo fixo não corre) --------
    if (pl->playing) {
        pl->advance(dt);
        pl->apply(scene, st.selected);
    }

    // ---- header --------------------------------------------------------------
    {
        char title[96];
        std::snprintf(title, sizeof(title), "ANIM · %s · %u track%s",
                      clip->name.c_str(),
                      static_cast<u32>(clip->tracks.size()),
                      clip->tracks.size() == 1 ? "" : "s");
        ui.labelFitted(r.x + 12.0f, r.y + kHeaderH * 0.5f + th * 0.30f, title,
                       theme::TEXT, 360.0f);

        const f32 by = r.y + 6.0f;
        const f32 bh = kHeaderH - 12.0f;
        // play/pause + stop + mode
        if (ui.button(kIdPlay, r.x + r.w - 436.0f, by, 84.0f, bh,
                      pl->playing ? "pausa" : "play")) {
            if (pl->playing) {
                pl->playing = false;   // pausa: tempo fica onde está
            } else {
                if (!tl.snap.captured) {
                    playSnapshotCapture(scene, tl.snap);   // sandbox do preview
                    tl.previewTic = st.selected;
                }
                pl->playing = true;
            }
        }
        if (ui.button(kIdStop, r.x + r.w - 348.0f, by, 64.0f, bh, "stop")) {
            stopPreview(scene, st.selected, tl);
        }
        const char* modeLabels[3] = {"once", "loop", "pingpong"};
        const int modeIdx = pl->mode == AnimationPlayer::Mode::Once ? 0
                            : pl->mode == AnimationPlayer::Mode::Loop ? 1 : 2;
        if (ui.button(kIdMode, r.x + r.w - 280.0f, by, 108.0f, bh,
                      modeLabels[modeIdx])) {
            pl->mode = modeIdx == 0 ? AnimationPlayer::Mode::Loop
                     : modeIdx == 1 ? AnimationPlayer::Mode::PingPong
                                    : AnimationPlayer::Mode::Once;
        }
        // speed 0.1..3 (slider inline do header)
        if (ui.slider(kIdSpeed, r.x + r.w - 164.0f, by + 6.0f, 96.0f, bh - 12.0f,
                      0.1f, 3.0f, pl->speed)) {
            // speed já escrito pelo slider
        }
        char spd[32];
        std::snprintf(spd, sizeof(spd), "vel %.1fx", pl->speed);
        ui.labelFitted(r.x + r.w - 164.0f, r.y + 4.0f, spd, theme::LINE, 96.0f);
        if (ui.button(kIdAddTrack, r.x + r.w - 60.0f, by, 48.0f, bh, "+")) {
            tl.addTrackMenu = !tl.addTrackMenu;
        }
    }

    // ---- régua + área das keys ----------------------------------------------
    const f32 keysX = r.x + 300.0f;                 // 300 px de labels/botões
    const f32 keysW = r.w - 300.0f - 12.0f;
    const f32 rulerY = r.y + kHeaderH;
    const f32 rowsY = rulerY + kRulerH;
    const f32 dur = pl->duration();
    const f32 view = dur > 0.25f ? dur : 2.0f;      // escala mínima 2 s

    auto timeToX = [&](f32 t) { return keysX + (t / view) * keysW; };

    // régua: tick por segundo inteiro + rótulo
    {
        const f32 step = view > 8.0f ? 2.0f : 1.0f;
        for (f32 t = 0.0f; t <= view + 1e-4f; t += step) {
            const f32 x = timeToX(t);
            ui.drawLine(x, rulerY + 4.0f, x, rulerY + kRulerH - 4.0f, 1.0f,
                        theme::LINE);
            if (t > 0.0f) {
                char lab[16];
                std::snprintf(lab, sizeof(lab), "%gs", t);
                ui.labelFitted(x + 3.0f, rulerY + kRulerH - 2.0f, lab,
                               theme::LINE, 44.0f);
            }
        }
        // linha de fundo da régua
        ui.drawLine(keysX, rulerY + kRulerH - 1.0f, keysX + keysW,
                    rulerY + kRulerH - 1.0f, 1.0f, theme::LINE);
    }

    // ---- rows (máx kMaxRows visíveis) ---------------------------------------
    const u32 nTracks = static_cast<u32>(clip->tracks.size());
    const u32 shown = nTracks < kMaxRows ? nTracks : kMaxRows;
    for (u32 i = 0; i < shown; ++i) {
        AnimTrack& tr = clip->tracks[i];
        const f32 ry = rowsY + static_cast<f32>(i) * kRowH;
        const bool sel = tl.selTrack == static_cast<i32>(i);
        // row selecionada: fundo LINE (destaque suave)
        if (sel) {
            ui.panel(r.x + 2.0f, ry + 1.0f, r.w - 4.0f, kRowH - 2.0f, theme::LINE);
        }
        // label: alvo + elemento
        char lab[64];
        std::snprintf(lab, sizeof(lab), "%s%s%s", animTargetLabel(tr.target),
                      tr.element.empty() ? "" : " ", tr.element.c_str());
        ui.labelFitted(r.x + 12.0f,
                       ry + (kRowH + th * 0.5f) * 0.5f, lab, theme::TEXT, 150.0f);
        // botões da row (w fixo: 40/40/40/36)
        char curve[8];
        std::snprintf(curve, sizeof(curve), "%s",
                      tr.curve == AnimCurve::Bezier ? "bez" : "lin");
        if (ui.button(kIdCurve + i, r.x + 168.0f, ry + 4.0f, 40.0f, kRowH - 8.0f,
                      curve)) {
            tr.curve = tr.curve == AnimCurve::Bezier ? AnimCurve::Linear
                                                     : AnimCurve::Bezier;
        }
        if (ui.button(kIdAddKey + i, r.x + 212.0f, ry + 4.0f, 44.0f, kRowH - 8.0f,
                      "+key")) {
            // key NO CURSOR com o valor ATUAL da propriedade (ou substitui a
            // key que já esteja a <1/30 s do cursor)
            f32 v[4] = {0.0f, 0.0f, 0.0f, 1.0f};
            sampleTarget(*tic, tr.target, tr.element, v);
            bool replaced = false;
            for (AnimKey& k : tr.keys) {
                if (std::fabs(k.t - pl->time) < 1.0f / 30.0f) {
                    for (int c = 0; c < 4; ++c) {
                        k.v[c] = v[c];
                    }
                    replaced = true;
                    break;
                }
            }
            if (!replaced) {
                AnimKey k;
                k.t = pl->time;
                for (int c = 0; c < 4; ++c) {
                    k.v[c] = v[c];
                }
                tr.keys.push_back(k);
                tr.sortKeys();
            }
            pl->apply(scene, st.selected);
        }
        if (ui.button(kIdDelKey + i, r.x + 260.0f, ry + 4.0f, 36.0f, kRowH - 8.0f,
                      "−")) {
            // remove a key MAIS PRÓXIMA do cursor desta row
            if (!tr.keys.empty()) {
                size_t best = 0;
                f32 bestD = 1e9f;
                for (size_t k = 0; k < tr.keys.size(); ++k) {
                    const f32 d = std::fabs(tr.keys[k].t - pl->time);
                    if (d < bestD) {
                        bestD = d;
                        best = k;
                    }
                }
                tr.keys.erase(tr.keys.begin() + static_cast<long>(best));
                pl->apply(scene, st.selected);
            }
        }
        // keys (diamantes = quadrados 8×8; selecionada = 12×12 ACCENT)
        for (size_t k = 0; k < tr.keys.size(); ++k) {
            const f32 x = timeToX(tr.keys[k].t);
            if (x < keysX - 4.0f || x > keysX + keysW + 4.0f) {
                continue;   // fora da janela (view < dur? não acontece — view=dur)
            }
            const bool ksel = sel && tl.selKey == static_cast<i32>(k);
            const f32 s = ksel ? 12.0f : 8.0f;
            ui.panel(x - s * 0.5f, ry + kRowH * 0.5f - s * 0.5f, s, s,
                     ksel ? theme::ACCENT : theme::TEXT);
        }
    }
    // rows cortadas: indicador "+N tracks" (cap documentado)
    if (nTracks > kMaxRows) {
        char more[48];
        std::snprintf(more, sizeof(more), "+%u tracks",
                      nTracks - kMaxRows);
        ui.labelFitted(r.x + 12.0f, rowsY + kMaxRows * kRowH, more,
                       theme::LINE, 200.0f);
    } else if (nTracks == 0) {
        ui.labelFitted(r.x + 12.0f, rowsY + 8.0f,
                       "sem tracks — [+]", theme::LINE, 360.0f);
    }

    // ---- interação crua: SCRUB + drag de key (slot 0) -----------------------
    {
        const f32 keysY = rulerY;                  // régua + rows
        const f32 keysH = kTimelineH - kHeaderH;
        f32 px = 0.0f, py = 0.0f;
        in.pos(0, px, py);
        const bool insideKeys = px >= keysX && px < keysX + keysW &&
                                py >= keysY && py < keysY + keysH;
        if (in.pressed(0) && insideKeys) {
            // 1) key a ≤14 px na row acertada? → seleciona+drag
            bool gotKey = false;
            for (u32 i = 0; i < shown; ++i) {
                const f32 rowMid = rowsY + static_cast<f32>(i) * kRowH + kRowH * 0.5f;
                if (std::fabs(py - rowMid) > kRowH * 0.5f) {
                    continue;
                }
                const AnimTrack& tr = clip->tracks[i];
                for (size_t k = 0; k < tr.keys.size(); ++k) {
                    if (std::fabs(timeToX(tr.keys[k].t) - px) <= 14.0f) {
                        tl.selTrack = static_cast<i32>(i);
                        tl.selKey = static_cast<i32>(k);
                        tl.draggingKey = true;
                        gotKey = true;
                        break;
                    }
                }
                if (gotKey) {
                    break;
                }
            }
            // 2) senão → scrub (régua ou corpo das rows)
            if (!gotKey) {
                tl.scrubbing = true;
                tl.selKey = -1;
            }
            if (tl.scrubbing) {
                pl->time = (px - keysX) / keysW * view;
                if (pl->time < 0.0f) pl->time = 0.0f;
                if (pl->time > view) pl->time = view;
                pl->apply(scene, st.selected);
            }
        }
        if (in.down(0) && (tl.scrubbing || tl.draggingKey)) {
            if (tl.scrubbing) {
                pl->time = (px - keysX) / keysW * view;
                if (pl->time < 0.0f) pl->time = 0.0f;
                if (pl->time > view) pl->time = view;
                pl->apply(scene, st.selected);
            } else if (tl.draggingKey && tl.selTrack >= 0 &&
                       tl.selTrack < static_cast<i32>(clip->tracks.size())) {
                AnimTrack& tr = clip->tracks[static_cast<size_t>(tl.selTrack)];
                if (tl.selKey >= 0 &&
                    tl.selKey < static_cast<i32>(tr.keys.size())) {
                    AnimKey& k = tr.keys[static_cast<size_t>(tl.selKey)];
                    f32 t = (px - keysX) / keysW * view;
                    if (t < 0.0f) t = 0.0f;
                    // clamp entre vizinhas (tempo estritamente crescente)
                    const f32 lo = tl.selKey > 0
                        ? tr.keys[static_cast<size_t>(tl.selKey) - 1].t + 0.001f
                        : 0.0f;
                    const f32 hi = tl.selKey + 1 < static_cast<i32>(tr.keys.size())
                        ? tr.keys[static_cast<size_t>(tl.selKey) + 1].t - 0.001f
                        : view + 1.0f;
                    if (t < lo) t = lo;
                    if (t > hi) t = hi;
                    k.t = t;
                    pl->apply(scene, st.selected);
                }
            }
        }
        if (in.released(0)) {
            if (tl.draggingKey && tl.selTrack >= 0 &&
                tl.selTrack < static_cast<i32>(clip->tracks.size())) {
                clip->tracks[static_cast<size_t>(tl.selTrack)].sortKeys();
            }
            tl.scrubbing = false;
            tl.draggingKey = false;
        }
    }

    // ---- cursor do tempo (POR CIMA das rows; espessura 2, ACCENT) ----------
    {
        const f32 cx = timeToX(pl->time);
        ui.drawLine(cx, rulerY + 2.0f, cx, r.y + kTimelineH - 4.0f, 2.0f,
                    theme::ACCENT);
        char tc[32];
        std::snprintf(tc, sizeof(tc), "%.2fs", pl->time);
        ui.labelFitted(cx + 4.0f, rulerY + kRulerH - 2.0f, tc, theme::ACCENT,
                       64.0f);
    }

    // ---- overlay "+track" (centrado, fecha com toque fora) ------------------
    if (tl.addTrackMenu) {
        const f32 w = 300.0f;
        const f32 h = kHeaderH + 6.0f * 44.0f + 12.0f;
        const f32 x = ui.safeLeft() +
                      (sw - ui.safeLeft() - ui.safeRight() - w) * 0.5f;
        const f32 y = ui.safeTop() +
                      (sh - ui.safeTop() - ui.safeBottom() - h) * 0.5f;
        // toque fora fecha (pressedOutside é do EditorUi — refeito aqui inline)
        bool outside = false;
        if (in.pressed(0)) {
            f32 px = 0.0f, py = 0.0f;
            in.pos(0, px, py);
            outside = px < x || px >= x + w || py < y || py >= y + h;
        }
        if (outside) {
            tl.addTrackMenu = false;
        } else {
            ui.panel(x, y, w, h, theme::PANEL);
            ui.frame(x, y, w, h, 2.0f, theme::ACCENT);
            ui.label(x + 12.0f, y + kHeaderH * 0.5f + th * 0.30f,
                     "NOVO TRACK", theme::TEXT);
            static const AnimTarget kTargets[6] = {
                AnimTarget::TicPos,   AnimTarget::TicRot,  AnimTarget::TicScale,
                AnimTarget::UiPos,    AnimTarget::UiColor, AnimTarget::UiAlpha,
            };
            for (int i = 0; i < 6; ++i) {
                const AnimTarget t = kTargets[i];
                const bool avail = targetAvailable(*tic, t);
                char lab[48];
                std::snprintf(lab, sizeof(lab), "%s%s", animTargetLabel(t),
                              avail ? "" : " (sem alvo)");
                if (ui.button(kIdTrackMenu + static_cast<u64>(i), x + 12.0f,
                              y + kHeaderH + static_cast<f32>(i) * 44.0f,
                              w - 24.0f, 36.0f, lab)) {
                    if (avail) {
                        std::string element;
                        if (t == AnimTarget::UiPos || t == AnimTarget::UiColor ||
                            t == AnimTarget::UiAlpha) {
                            const UiCanvas* uic = tic->getComponent<UiCanvas>();
                            if (uic && !uic->elements.empty()) {
                                element = uic->elements.front().name;
                            }
                        }
                        pl->addTrack(t, element.c_str());
                        tl.addTrackMenu = false;
                    }
                    // sem alvo: fica aberto (o rótulo explica)
                }
            }
        }
    }
}

} // namespace timeline
} // namespace vv
