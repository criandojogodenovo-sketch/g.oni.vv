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

// 0.8.4 — layout fluido do header (ver Timeline.h): cluster direito fixo,
// "clip:" encostado ao play (encolhe 176→120 se preciso), título no resto.
HeaderLayout headerLayout(const UiRect& r) {
    const f32 by = r.y + 6.0f;
    const f32 bh = theme::dp(kHeaderH - 12.0f);
    HeaderLayout L;
    // cluster da DIREITA — offsets fixos de sempre (nada muda nos ecrãs largos)
    L.play   = {r.x + r.w - 436.0f, by, 84.0f, bh};
    L.stop   = {r.x + r.w - theme::dp(348.0f), by, theme::dp(64.0f), bh};
    L.mode   = {r.x + r.w - 280.0f, by, 108.0f, bh};
    L.slider = {r.x + r.w - 164.0f, by, 96.0f, bh};
    L.add    = {r.x + r.w - theme::dp(60.0f), by, theme::dp(48.0f), bh};
    // "clip:": à ESQUERDA do play com folga 8 (nunca sobrepõe); encolhe
    // 176→120 quando a strip é estreita e, em último caso, o TÍTULO cede
    // (o botão fica com o que sobrar, mínimo clicável 96 — o botão nunca
    // invade o play)
    f32 cw = 176.0f;
    f32 cx = L.play.x - 8.0f - cw;
    if (cx < r.x + 160.0f) {
        cw = 120.0f;
        cx = L.play.x - 8.0f - cw;
        if (cx < r.x + 140.0f) {
            cw = L.play.x - 8.0f - (r.x + 140.0f);
            if (cw < 96.0f) {
                cw = 96.0f;
            }
            cx = L.play.x - 8.0f - cw;
            if (cx < r.x + 12.0f) {
                cx = r.x + 12.0f;   // extremo absoluto (não sai da strip)
            }
        }
    }
    L.clip = {cx, by, cw, bh};
    L.title = {r.x + theme::dp(12.0f), r.y, (cx - theme::dp(8.0f)) - (r.x + theme::dp(12.0f)), theme::dp(kHeaderH)};
    return L;
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
    const UiRect r = timelineRect(ui.screenWidth(), ui.screenHeight(),
                                  ui.safeArea(), st.showInspector);
    drawTimelineInRect(ui, in, scene, st, tl, dt, r);
}

void drawTimelineInRect(UiContext& ui, const InputState& in, Scene& scene,
                        editor::EditorState& st, State& tl, f32 dt,
                        const UiRect& rectIn) {
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
    // 0.8.1: a timeline edita/reproduz o clip ATIVO (o seletor troca);
    // sem clips nasce o "edit" (0.8.0)
    AnimClip* clip = pl->activeClipPtr();
    if (!clip) {
        clip = pl->editClip();
    }

    const f32 sw = ui.screenWidth();
    const f32 sh = ui.screenHeight();
    const UiRect r = rectIn;   // 0.9.0: o rect vem do chamador (strip OU drawer)

    // ---- painel (strip) ------------------------------------------------------
    ui.panel(r.x, r.y, r.w, kTimelineH, theme::PANEL);
    ui.frame(r.x, r.y, r.w, kTimelineH, 2.0f, theme::ACCENT);
    ui.panel(r.x, r.y + theme::dp(kHeaderH) - 1.0f, r.w, 1.0f, theme::LINE);
    const f32 th = ui.fontHeight();

    // ---- preview (avança com dt REAL — editor, passo fixo não corre) --------
    if (pl->playing) {
        pl->advance(dt);
        pl->apply(scene, st.selected);
    }

    // ---- header (0.8.4: layout fluido — sem sobreposição a qualquer largura)
    {
        char title[96];
        if (pl->blendClip >= 0 &&
            static_cast<size_t>(pl->blendClip) < pl->clips.size()) {
            // 0.8.3: crossfade em curso — o título mostra a TRANSIÇÃO
            std::snprintf(title, sizeof(title), "ANIM · %s → %s",
                          clip->name.c_str(),
                          pl->clips[static_cast<size_t>(pl->blendClip)].name.c_str());
        } else {
            std::snprintf(title, sizeof(title), "ANIM · %s · %u track%s",
                          clip->name.c_str(),
                          static_cast<u32>(clip->tracks.size()),
                          clip->tracks.size() == 1 ? "" : "s");
        }
        const HeaderLayout hl = headerLayout(r);
        ui.labelFitted(hl.title.x, r.y + kHeaderH * 0.5f + th * 0.30f, title,
                       theme::TEXT, hl.title.w);

        if (ui.button(kIdPlay, hl.play.x, hl.play.y, hl.play.w, hl.play.h,
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
        if (ui.button(kIdStop, hl.stop.x, hl.stop.y, hl.stop.w, hl.stop.h,
                      "stop")) {
            stopPreview(scene, st.selected, tl);
        }
        const char* modeLabels[3] = {"once", "loop", "pingpong"};
        const int modeIdx = pl->mode == AnimationPlayer::Mode::Once ? 0
                            : pl->mode == AnimationPlayer::Mode::Loop ? 1 : 2;
        if (ui.button(kIdMode, hl.mode.x, hl.mode.y, hl.mode.w, hl.mode.h,
                      modeLabels[modeIdx])) {
            pl->mode = modeIdx == 0 ? AnimationPlayer::Mode::Loop
                     : modeIdx == 1 ? AnimationPlayer::Mode::PingPong
                                    : AnimationPlayer::Mode::Once;
        }
        // slider contextual do header: BLEND (0.8.3, quando há blend em
        // curso — arrastar assume o peso MANUAL) ou VELOCIDADE (sempre)
        if (pl->blendClip >= 0) {
            f32 w = pl->blendWeight;
            if (ui.slider(kIdBlend, hl.slider.x, hl.slider.y + 6.0f,
                          hl.slider.w, hl.slider.h - 12.0f, 0.0f, 1.0f, w)) {
                pl->blendWeight = w;        // manual: o crossfade pára de
                pl->blendDuration = 0.0f;   // animar; o peso é do utilizador
            }
            char bl[32];
            std::snprintf(bl, sizeof(bl), "blend %d%%",
                          static_cast<int>(pl->blendWeight * 100.0f + 0.5f));
            ui.labelFitted(hl.slider.x, r.y + 4.0f, bl, theme::LINE, 96.0f);
        } else {
            if (ui.slider(kIdSpeed, hl.slider.x, hl.slider.y + 6.0f,
                          hl.slider.w, hl.slider.h - 12.0f, 0.1f, 3.0f,
                          pl->speed)) {
                // speed já escrito pelo slider
            }
            char spd[32];
            std::snprintf(spd, sizeof(spd), "vel %.1fx", pl->speed);
            ui.labelFitted(hl.slider.x, r.y + 4.0f, spd, theme::LINE, 96.0f);
        }
        // 0.8.1 — seletor de CLIPS (importados de glTF + "edit"): abre a
        // lista; o clip escolhido passa a ser o ATIVO (playback e edição)
        char clipBtn[40];
        std::snprintf(clipBtn, sizeof(clipBtn), "clip: %s",
                      clip->name.c_str());
        if (ui.button(kIdClip, hl.clip.x, hl.clip.y, hl.clip.w, hl.clip.h,
                      clipBtn)) {
            tl.clipMenu = !tl.clipMenu;
            tl.addTrackMenu = false;
        }
        if (ui.button(kIdAddTrack, hl.add.x, hl.add.y, hl.add.w, hl.add.h,
                      "+")) {
            tl.addTrackMenu = !tl.addTrackMenu;
            tl.clipMenu = false;
        }
    }

    // ---- régua + área das keys ----------------------------------------------
    const f32 keysX = r.x + 300.0f;                 // 300 px de labels/botões
    const f32 keysW = r.w - 300.0f - 12.0f;
    const f32 rulerY = r.y + theme::dp(kHeaderH);
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
        const f32 ry = rowsY + static_cast<f32>(i) * theme::dp(kRowH);
        const bool sel = tl.selTrack == static_cast<i32>(i);
        // row selecionada: fundo LINE (destaque suave)
        if (sel) {
            ui.panel(r.x + theme::dp(2.0f), ry + theme::dp(1.0f), r.w - theme::dp(4.0f), theme::dp(kRowH) - theme::dp(2.0f), theme::LINE);
        }
        // label: alvo + elemento
        char lab[64];
        std::snprintf(lab, sizeof(lab), "%s%s%s", animTargetLabel(tr.target),
                      tr.element.empty() ? "" : " ", tr.element.c_str());
        ui.labelFitted(r.x + 12.0f,
                       ry + (theme::dp(kRowH) + th * 0.5f) * 0.5f, lab, theme::TEXT, theme::dp(150.0f));
        // botões da row (w fixo: 40/40/40/36)
        char curve[8];
        std::snprintf(curve, sizeof(curve), "%s",
                      tr.curve == AnimCurve::Bezier ? "bez" : "lin");
        if (ui.button(kIdCurve + i, r.x + theme::dp(160.0f), ry, theme::dp(48.0f), theme::dp(kRowH),
                      curve)) {
            tr.curve = tr.curve == AnimCurve::Bezier ? AnimCurve::Linear
                                                     : AnimCurve::Bezier;
        }
        if (ui.button(kIdAddKey + i, r.x + theme::dp(212.0f), ry, theme::dp(48.0f), theme::dp(kRowH),
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
        if (ui.button(kIdDelKey + i, r.x + theme::dp(264.0f), ry, theme::dp(48.0f), theme::dp(kRowH),
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
            ui.panel(x - s * 0.5f, ry + theme::dp(kRowH) * 0.5f - s * 0.5f, s, s,
                     ksel ? theme::ACCENT : theme::TEXT);
        }
    }
    // rows cortadas: indicador "+N tracks" (cap documentado)
    if (nTracks > kMaxRows) {
        char more[48];
        std::snprintf(more, sizeof(more), "+%u tracks",
                      nTracks - kMaxRows);
        ui.labelFitted(r.x + theme::dp(12.0f), rowsY + kMaxRows * theme::dp(kRowH), more,
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

    // ---- overlay de CLIPS (0.8.1): lista + "novo (edit)" --------------------
    if (tl.clipMenu) {
        const u32 nClips = static_cast<u32>(pl->clips.size());
        const u32 shown = nClips < 6 ? nClips : 6;
        const f32 w = 300.0f;
        const f32 h = kHeaderH + (static_cast<f32>(shown) + 1.0f) * 44.0f + 12.0f;
        const f32 x = ui.safeLeft() + (sw - ui.safeLeft() - ui.safeRight() - w) * 0.5f;
        const f32 y = ui.safeTop() + (sh - ui.safeTop() - ui.safeBottom() - h) * 0.5f;
        bool outside = false;
        if (in.pressed(0)) {
            f32 px = 0.0f, py = 0.0f;
            in.pos(0, px, py);
            outside = px < x || px >= x + w || py < y || py >= y + h;
        }
        if (outside) {
            tl.clipMenu = false;
        } else {
            ui.panel(x, y, w, h, theme::PANEL);
            ui.frame(x, y, w, h, 2.0f, theme::ACCENT);
            ui.label(x + 12.0f, y + kHeaderH * 0.5f + th * 0.30f, "CLIPS",
                     theme::TEXT);
            for (u32 i = 0; i < shown; ++i) {
                char lab[64];
                std::snprintf(lab, sizeof(lab), "%s%s (%u track%s)",
                              pl->clips[i].name.c_str(),
                              static_cast<i32>(i) == pl->activeClip ? " *" : "",
                              static_cast<u32>(pl->clips[i].tracks.size()),
                              pl->clips[i].tracks.size() == 1 ? "" : "s");
                // 0.8.3: tap = troca INSTANTÂNEA; "fade" = CROSSFADE 0.4 s
                // do clip ATIVO para este (walk→run suave)
                if (ui.button(kIdClipItem + static_cast<u64>(i), x + 12.0f,
                              y + kHeaderH + static_cast<f32>(i) * 44.0f,
                              w - 112.0f, 36.0f, lab)) {
                    pl->activeClip = static_cast<i32>(i);
                    pl->time = 0.0f;   // trocou de clip → recomeça limpo
                    pl->stopBlend();
                    pl->resetDir();
                    tl.clipMenu = false;
                    tl.selTrack = -1;
                    tl.selKey = -1;
                }
                if (static_cast<i32>(i) != pl->activeClip &&
                    ui.button(kIdFade + static_cast<u64>(i), x + w - 92.0f,
                              y + kHeaderH + static_cast<f32>(i) * 44.0f,
                              80.0f, 36.0f, "fade")) {
                    pl->crossfade(static_cast<i32>(i), 0.4f);
                    tl.clipMenu = false;
                    tl.selTrack = -1;
                    tl.selKey = -1;
                }
            }
            if (ui.button(kIdClipNew, x + 12.0f,
                          y + kHeaderH + static_cast<f32>(shown) * 44.0f,
                          w - 24.0f, 36.0f, "novo (edit)")) {
                // reutiliza um clip "edit" existente ou acrescenta
                i32 idx = -1;
                for (size_t i = 0; i < pl->clips.size(); ++i) {
                    if (pl->clips[i].name == "edit") {
                        idx = static_cast<i32>(i);
                        break;
                    }
                }
                if (idx < 0) {
                    AnimClip c;
                    c.name = "edit";
                    pl->clips.push_back(std::move(c));
                    idx = static_cast<i32>(pl->clips.size()) - 1;
                }
                pl->activeClip = idx;
                pl->time = 0.0f;
                tl.clipMenu = false;
                tl.selTrack = -1;
                tl.selKey = -1;
            }
        }
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
