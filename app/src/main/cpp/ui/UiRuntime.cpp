// ui/UiRuntime.cpp — runtime da UI criável (0.7.0): desenho, hit-test e ações.
//
// O desenho é 100% tema mono (quads + glifos do atlas; sem cores novas — as
// cores dos elementos são DADOS do utilizador, o default é o token PANEL).
// Tudo é afervel no CI: os testes leem os batches (solids/glyphs) e aferem
// geometria, z-order e ausência de sobreposição.
#include "ui/UiRuntime.h"
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
#include "core/Scene.h"
#include "ui/UiContext.h"
#include "platform/EngineLog.h"   // 0.8.9: erros LEGÍVEIS nos guards do resolver
#include <cmath>

namespace vv {
namespace ui {

// 0.8.6: declarações antecipadas (definidas no fundo do ficheiro)
bool uiRotatedRectHit(const UiRect& r, f32 rotDeg, f32 px, f32 py);
UiRect uiGizmoCornerRect(const UiRect& r, u32 corner, f32 size);
UiRect uiGizmoRotateHandleRect(const UiRect& r, f32 size);
f32 uiGizmoSnapRot(f32 deg);

namespace {

// (0.8.6: os tokens kLine/kText duplicados foram REMOVIDOS — o desenho usa
// os tokens do tema de UiContext.h direto: UMA fonte de verdade)

bool inRect(const UiRect& r, f32 x, f32 y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

// 0.8.6 — RAII da rotação por elemento (set/clear emparelhados; o clear
// corre MESMO com break/early-return dentro do switch)
struct ScopedQuadXform {
    UiContext& ui;
    ScopedQuadXform(UiContext& u, f32 cx, f32 cy, f32 radDeg) : ui(u) {
        ui.setQuadXform(cx, cy, radDeg * 0.01745329252f);
    }
    ~ScopedQuadXform() { ui.clearQuadXform(); }
    ScopedQuadXform(const ScopedQuadXform&) = delete;
    ScopedQuadXform& operator=(const ScopedQuadXform&) = delete;
};

} // namespace

// ---- menu (composto 0.7.3 — mas a geometria já serve o hit-test 0.7.0) -----

u32 menuLineCount(const UiElement& e) {
    if (e.text.empty()) {
        return 0;
    }
    u32 n = 1;
    for (char c : e.text) {
        if (c == '\n') {
            ++n;
        }
    }
    return n;
}

bool menuLineAt(const UiElement& e, u32 row, std::string& outLabel,
                std::string& outTarget) {
    if (row >= menuLineCount(e)) {
        return false;
    }
    u32 cur = 0;
    const char* start = e.text.c_str();
    const char* end = start;
    for (const char* p = start;; ++p) {
        if (*p == '\n' || *p == '\0') {
            if (cur == row) {
                end = p;
                break;
            }
            start = p + 1;
            ++cur;
        }
    }
    std::string line(start, static_cast<size_t>(end - start));
    const size_t gt = line.find('>');
    if (gt == std::string::npos) {
        outLabel = line;          // sem '>' → o alvo é o próprio label
        outTarget = line;
    } else {
        outLabel = line.substr(0, gt);
        outTarget = line.substr(gt + 1);
    }
    return true;
}

UiRect menuItemRect(const UiElement& e, const UiRect& base, u32 row) {
    const u32 n = menuLineCount(e);
    if (n == 0) {
        return base;
    }
    // 0.7.4: espaçamento entre itens — o rowH encolhe para os (n−1) vãos
    // caberem dentro do h do menu (spacing 0 = 0.7.3 exato: h/n por linha)
    const f32 gaps = static_cast<f32>(n - 1u) * (e.spacing > 0.0f ? e.spacing : 0.0f);
    const f32 rowH = base.h > gaps ? (base.h - gaps) / static_cast<f32>(n)
                                   : base.h / static_cast<f32>(n);
    return {base.x, base.y + static_cast<f32>(row) * (rowH + e.spacing),
            base.w, rowH};
}

// ---- 0.7.4: LAYOUT RESOLVER (containers + paridade — FONTE ÚNICA) -------------

namespace {

// estado do resolver (memo de tamanhos + guard de ciclos)
struct Resolver {
    const UiCanvas* c;
    f32 sw, sh;
    safe::Insets ins;
    CanvasLayout* out;
    // 0.8.4: limite do BUFFER do chamador — o resolver NUNCA escreve out[i]
    // com i >= cap (os painéis do editor usam arrays fixos de 32; escrever
    // além era CORRUPÇÃO DE MEMÓRIA — o "ecrã preto ao adicionar UI" do C33
    // quando o canvas passa 32 elementos)
    u32 count = 0;
    std::vector<f32> sizeW, sizeH;   // tamanho EFETIVO (eixo de conteúdo auto)
    std::vector<u8>  state;          // 0 = intocado, 1 = EM CURSO (ciclo), 2 = pronto
    // 0.8.9 (CRASH-PROOF): guard de ciclo do PLACEAT. O sizeOf SEMPRE teve
    // guard (state 1 = em curso devolve o tamanho manual), mas o placeAt
    // recursivo NÃO tinha NENHUM — um parent cíclico (ex.: container com
    // "colocar em" a SI PRÓPRIO, que o seletor permitia) recursava
    // INFINITAMENTE: stack exhaustion → SIGSEGV (crash-1790830406.dump do
    // C33). placing[i]=1 marca o CAMINHO atual; repetição = ciclo → ERRO
    // LEGÍVEL no log e o elemento fica órfão (rect zerado), NUNCA crash.
    std::vector<u8>  placing;
    u32  cyclesAborted = 0;          // contagem (teste do CI + diagnóstico)
};

// limite de profundidade dos traversals recursivos do resolver (0.8.9,
// CRASH-PROOF): uma cadeia legítima nunca excede `count` elementos; a
// margem é defesa extra. Ao atingir, ERRO LEGÍVEL em vez de stack overflow.
constexpr u32 kLayoutMaxDepth = 64;

// filhos de `i` = elementos com parent == nome do container i (ordem do
// array). O chamador itera o array todo (canvases pequenos; O(n²) ok).
inline bool isChildOf(const UiElement& child, const Resolver& r, i32 ci) {
    return !child.parent.empty() &&
           child.parent == r.c->elements[static_cast<size_t>(ci)].name;
}

// tamanho EFETIVO do elemento i (containers: eixo de conteúdo AUTO —
// recursivo nos filhos; ciclo/guard devolve o tamanho manual)
// 0.8.9: depth guard explícito (o state já guarda ciclos; o cap guarda
// cadeias absurdas — qualquer traversal recursivo do core tem teto).
void sizeOf(Resolver& r, i32 i, u32 depth = 0) {
    if (i < 0 || static_cast<u32>(i) >= r.count) {
        return;   // 0.8.4: fora do buffer do chamador — nem medi-lo
    }
    if (depth > kLayoutMaxDepth) {
        ++r.cyclesAborted;
        elog::error("ui: layout — profundidade > %u no elemento %d (cadeia "
                    "de parents demasiado longa); tamanho manual",
                    kLayoutMaxDepth, static_cast<int>(i));
        return;
    }
    if (r.state[static_cast<size_t>(i)] != 0u) {
        return;   // pronto (2) ou em curso (1 = ciclo → tamanho manual)
    }
    r.state[static_cast<size_t>(i)] = 1;
    const UiElement& e = r.c->elements[static_cast<size_t>(i)];
    f32 w = e.w, h = e.h;
    if (uiElementIsContainer(e.kind)) {
        // filhos visíveis (invisíveis COLAPSAM — não ocupam lugar)
        f32 content = 0.0f;
        u32 n = 0;
        // 0.8.4: só elementos dentro do cap do chamador (os vetores têm count)
        for (u32 j = 0; j < r.count; ++j) {
            const UiElement& ch = r.c->elements[j];
            if (!isChildOf(ch, r, i) || !ch.visible) {
                continue;
            }
            sizeOf(r, static_cast<i32>(j), depth + 1);
            content += (e.kind == UiElement::Kind::VBox) ? r.sizeH[j]
                                                         : r.sizeW[j];
            ++n;
        }
        const f32 total = 2.0f * e.pad + content +
                          (n > 0 ? static_cast<f32>(n - 1) * e.spacing : 0.0f);
        if (n > 0) {   // vazio mantém o tamanho manual (placeholder)
            if (e.kind == UiElement::Kind::VBox) {
                h = total;   // VBox: h AUTO, w manual
            } else {
                w = total;   // HBox: w AUTO, h manual
            }
        }
    }
    r.sizeW[static_cast<size_t>(i)] = w;
    r.sizeH[static_cast<size_t>(i)] = h;
    r.state[static_cast<size_t>(i)] = 2;
}

// dispõe o elemento i no rect dado (recursivo: containers dispõem filhos).
// Container INVISÍVEL: os filhos continuam a ser dispostos (rects válidos)
// mas TODOS ficam shown=false — escondidos em cascata.
// 0.8.9 (CRASH-PROOF): GUARD DE CICLO + PROFUNDIDADE. O placeAt recursivo
// não tinha NENHUM guard (o comentário antigo "o resolver guarda ciclos"
// só era verdade para o sizeOf): um parent cíclico (container pai de si
// próprio, ou A→B→A) recursava infinitamente até exaurir a stack — o
// SIGSEGV do crash-1790830406.dump. Agora: elemento JÁ no caminho atual
// (placing=1) = ciclo → ERRO LEGÍVEL no log, o ciclo fica por dispor
// (rect zerado = órfão, como o passo 3 já tratava) e a app SEGUE VIVA.
void placeAt(Resolver& r, i32 i, const UiRect& rect, bool parentShown,
             i32 parentIdx, u32 depth = 0) {
    if (i < 0 || static_cast<u32>(i) >= r.count) {
        return;   // 0.8.4: fora do buffer do chamador — NUNCA escrever out[i]
    }
    if (r.placing[static_cast<size_t>(i)] != 0u) {
        ++r.cyclesAborted;
        elog::error("ui: layout — CICLO de parents no elemento '%s' "
                    "(parent='%s'); elemento deixado por dispor "
                    "(sem crash — verifique 'colocar em')",
                    r.c->elements[static_cast<size_t>(i)].name.c_str(),
                    r.c->elements[static_cast<size_t>(i)].parent.c_str());
        return;   // ciclo: ABORTA com erro legível, NÃO recursa
    }
    if (depth > kLayoutMaxDepth) {
        ++r.cyclesAborted;
        elog::error("ui: layout — profundidade > %u no elemento '%s'; "
                    "elemento deixado por dispor",
                    kLayoutMaxDepth,
                    r.c->elements[static_cast<size_t>(i)].name.c_str());
        return;
    }
    const UiElement& e = r.c->elements[static_cast<size_t>(i)];
    const bool shown = parentShown && e.visible;
    CanvasLayout& L = r.out[static_cast<size_t>(i)];
    L.rect = rect;
    L.parentIdx = parentIdx;
    L.laid = parentIdx >= 0;
    L.shown = shown;
    if (!uiElementIsContainer(e.kind)) {
        return;
    }
    r.placing[static_cast<size_t>(i)] = 1;   // 0.8.9: marca o CAMINHO atual
    const u32 placedHere = r.cyclesAborted;  // (diagnóstico: abortos deste ramo)
    // dispõe os filhos dentro do rect (ordem do array = ordem do layout)
    // 0.8.4: só elementos dentro do cap do chamador (os vetores têm count)
    const f32 sp = e.spacing;
    if (e.kind == UiElement::Kind::VBox) {
        f32 y = rect.y + e.pad;
        for (u32 j = 0; j < r.count; ++j) {
            const UiElement& ch = r.c->elements[j];
            if (!isChildOf(ch, r, i) || !ch.visible) {
                continue;
            }
            const f32 cw = r.sizeW[j];
            const f32 chh = r.sizeH[j];
            f32 x = rect.x + e.pad;
            if (e.align == UiElement::Align::Center) {
                x = rect.x + (rect.w - cw) * 0.5f;
            } else if (e.align == UiElement::Align::End) {
                x = rect.x + rect.w - e.pad - cw;
            }
            placeAt(r, static_cast<i32>(j), {x, y, cw, chh}, shown,
                    static_cast<i32>(i), depth + 1);
            y += chh + sp;
        }
    } else {   // HBox
        f32 x = rect.x + e.pad;
        for (u32 j = 0; j < r.count; ++j) {
            const UiElement& ch = r.c->elements[j];
            if (!isChildOf(ch, r, i) || !ch.visible) {
                continue;
            }
            const f32 cw = r.sizeW[j];
            const f32 chh = r.sizeH[j];
            f32 y = rect.y + e.pad;
            if (e.align == UiElement::Align::Center) {
                y = rect.y + (rect.h - chh) * 0.5f;
            } else if (e.align == UiElement::Align::End) {
                y = rect.y + rect.h - e.pad - chh;
            }
            placeAt(r, static_cast<i32>(j), {x, y, cw, chh}, shown,
                    static_cast<i32>(i), depth + 1);
            x += cw + sp;
        }
    }
    r.placing[static_cast<size_t>(i)] = 0;   // 0.8.9: sai do caminho atual
    (void)placedHere;
}

} // namespace

void resolveCanvasLayout(const UiCanvas& c, f32 sw, f32 sh,
                         const safe::Insets& ins,
                         CanvasLayout* out, u32 cap) {
    // 0.8.4 (fix do "ecrã preto ao adicionar UI" no C33): o resolver opera
    // SEMPRE dentro de cap — o buffer do chamador é autoridade (os editores
    // passam arrays fixos de 32; os runtimes passam vectors do tamanho da
    // cena). Antes, os loops abaixo iteravam n e o placeAt escrevia out[i]
    // além do fim do array = corrupção de stack/heap com 33+ elementos.
    const u32 n = static_cast<u32>(c.elements.size());
    const u32 count = n < cap ? n : cap;
    for (u32 i = 0; i < count; ++i) {
        out[i] = CanvasLayout{};   // zera (órfãos ficam com rect zerado até ao fallback)
    }
    if (n == 0 || count == 0) {
        return;
    }
    Resolver r{&c, sw, sh, ins, out, count,
                std::vector<f32>(count, 0.0f), std::vector<f32>(count, 0.0f),
                std::vector<u8>(count, 0), std::vector<u8>(count, 0), 0};
    // 1) tamanhos efetivos (memo + guard de ciclo) — até ao cap do chamador
    for (u32 i = 0; i < count; ++i) {
        sizeOf(r, static_cast<i32>(i));
    }
    // 2) posicionamento: TOPO primeiro (parent vazio OU pai inexistente),
    //    depois os filhos recursivamente via placeAt
    for (u32 i = 0; i < count; ++i) {
        const UiElement& e = c.elements[i];
        if (!e.parent.empty()) {
            const i32 pi = c.findElement(e.parent);
            if (pi >= 0 &&
                uiElementIsContainer(c.elements[static_cast<size_t>(pi)].kind)) {
                continue;   // filho legítimo — disposto pelo placeAt do pai
            }
        }
        // topo: âncoras próprias + tamanho efetivo no eixo de conteúdo
        UiRect rect = elementRect(e, sw, sh, ins);
        rect.w = r.sizeW[i];
        rect.h = r.sizeH[i];
        placeAt(r, static_cast<i32>(i), rect, true, -1);
    }
    // 3) órfãos/nao-dispostos (ciclos, pais invisíveis, pais mortos): ficam
    //    como TOPO (âncoras próprias, tamanho efetivo) — determinístico
    //    FASE 9 (loop ASan): o loop era `i < n` (TODOS os elementos) mas
    //    r.sizeW/r.sizeH/r.out têm tamanho `count` (o cap do chamador é a
    //    autoridade — 0.8.4) — com 33+ elementos e cap 32 o passo 3 lia/
    //    escrevia FORA dos buffers (stack no editor). O loop é o CAP.
    for (u32 i = 0; i < count; ++i) {
        if (!c.elements[i].parent.empty() && !r.out[i].laid) {
            const UiElement& e = c.elements[i];
            UiRect rect = elementRect(e, sw, sh, ins);
            rect.w = r.sizeW[i];
            rect.h = r.sizeH[i];
            placeAt(r, static_cast<i32>(i), rect, true, -1);
            r.out[i].laid = false;   // órfão de facto (o Inspector mostra)
        }
    }
}

// ---- desenho -------------------------------------------------------------------

// 0.7.4 — tint de textura de FUNDO: branco × alpha do elemento (a textura
// mostra as cores PRÓPRIAS; o alpha do elemento permite translucidez —
// antes o Image passava e.color ESCURO como tint e a imagem ficava quase
// preta)
static const f32* texTint(const UiElement& e) {
    static f32 t[4];
    t[0] = t[1] = t[2] = 1.0f;
    t[3] = e.color[3] < 0.0f ? 0.0f : e.color[3];
    return t;
}

bool drawElement(UiContext& uictx, const UiElement& e, const UiRect& r,
                 bool sel) {
    if (!e.visible || r.w <= 0.0f || r.h <= 0.0f) {
        return false;
    }
    const f32 line[4] = {theme::LINE[0], theme::LINE[1], theme::LINE[2], 1.0f};
    const f32 text[4] = {theme::TEXT[0], theme::TEXT[1], theme::TEXT[2], 1.0f};
    // 0.7.4 — ESCALA DO CONTEXTO (paridade editor↔Play): no viewport 2D o
    // rect chega ESCALADO e TODOS os insetes/espessuras constantes deste
    // desenho escalam com ele (10*k, 20*k, molduras*k...) — o mini-canvas é
    // o Play REDUZIDO ao pixel; no Play k = 1 (comportamento 0.7.3 exato)
    const f32 k = uictx.textScale();

    // 0.8.6 — ROTAÇÃO do elemento (não-containers): TODOS os quads emitidos
    // abaixo (fundo/textura/texto) giram à volta do centro do rect. O clip
    // dos filhos/scroll continua axis-aligned (documentado).
    const bool rotated = e.rot != 0.0f && !uiElementIsContainer(e.kind);
    const ScopedQuadXform xform(uictx, r.x + r.w * 0.5f, r.y + r.h * 0.5f,
                                rotated ? e.rot : 0.0f);

    // 0.8.6 — TEXTO COM TIPOGRAFIA do elemento (fitted + fonte/estilo):
    // caminho comum de todos os textos DO elemento (o chrome do editor
    // continua pelo labelFitted normal)
    const bool styled = e.fontScale != 1.0f ||
                        e.textStyle != UiElement::TextStyle::Normal;
    auto fText = [&](f32 x, f32 baseY, const char* s, f32 maxW) {
        if (!styled) {
            uictx.labelFitted(x, baseY, s, text, maxW);
            return;
        }
        const f32 fs = e.fontScale;
        char buf[256];
        textfit::ellipsize(s, maxW,
                           [&](const char* t) {
                               return uictx.fontWidth(t) * fs;
                           },
                           buf, sizeof(buf));
        uictx.labelStyled(x, baseY, buf, text, fs,
                          static_cast<u8>(e.textStyle));
    };
    // métricas ESCALADAS pela fonte do elemento (centragens verticais)
    auto fMetrics = [&]() {
        const TextMetrics tm = uictx.textMetrics();
        if (!styled) {
            return tm;
        }
        return TextMetrics{tm.ascent * e.fontScale, tm.descent * e.fontScale};
    };

    switch (e.kind) {
        case UiElement::Kind::Panel:
            // 0.7.4: textura de fundo OPCIONAL (tex: no Inspector — ref em
            // e.image). Com textura: quad texturizado + moldura; sem
            // resolver/textura: fill sólido honesto (nunca finge)
            if (!e.image.empty() &&
                uictx.imageQuad(r.x, r.y, r.w, r.h, e.image, texTint(e))) {
                uictx.frame(r.x, r.y, r.w, r.h, 1.0f * k, line);
                break;
            }
            uictx.panel(r.x, r.y, r.w, r.h, e.color);
            uictx.frame(r.x, r.y, r.w, r.h, 1.0f * k, line);
            break;

        case UiElement::Kind::Label: {
            // 0.7.4 — SÓ TEXTO por default: fundo TRANSPARENTE (alpha 0;
            // o fix do C33 "Label com fundo claro fixo"). Fundo OPCIONAL:
            // alpha > 0 desenha o painel com a cor COM ALPHA do elemento.
            if (e.color[3] > 0.001f) {
                uictx.panel(r.x, r.y, r.w, r.h, e.color);
            }
            // texto centrado verticalmente, fitted à largura (nunca sai)
            const TextMetrics tmE = fMetrics();
            const f32 baseline =
                r.y + (r.h - tmE.block()) * 0.5f + tmE.ascent;
            fText(r.x + 10.0f * k, baseline, e.text.c_str(), r.w - 20.0f * k);
            break;
        }

        case UiElement::Kind::Button: {
            // 0.7.4: textura de fundo OPCIONAL (tex: — como o Panel); sem
            // textura mantém o fill ESCURO default do botão (C33)
            const bool textured =
                !e.image.empty() &&
                uictx.imageQuad(r.x, r.y, r.w, r.h, e.image, texTint(e));
            if (!textured) {
                uictx.panel(r.x, r.y, r.w, r.h, e.color);
            }
            uictx.frame(r.x, r.y, r.w, r.h, 2.0f * k, text);
            const TextMetrics tmE = fMetrics();
            const f32 baseline =
                r.y + (r.h - tmE.block()) * 0.5f + tmE.ascent;
            fText(r.x + 10.0f * k, baseline, e.text.c_str(), r.w - 20.0f * k);
            break;
        }

        case UiElement::Kind::Image:
            // quad de textura do projeto (ref "image"); o resolver é
            // partilhado (o main liga ao GpuAssets). SEM imagem escolhida
            // ou carga falhada → PLACEHOLDER claro: moldura + "(sem imagem)"
            // centrado (0.7.4 — o fix do C33 do gap crítico do Image)
            if (uictx.imageQuad(r.x, r.y, r.w, r.h, e.image, texTint(e))) {
                break;   // textura emitida (batch de imagens do UiContext)
            }
            uictx.panel(r.x, r.y, r.w, r.h, e.color);
            uictx.frame(r.x, r.y, r.w, r.h, 2.0f * k, line);
            if (e.image.empty()) {
                const TextMetrics tm = uictx.textMetrics();
                const f32 tw = uictx.fontWidth("(sem imagem)");
                uictx.labelFitted(
                    r.x + (r.w - tw) * 0.5f,
                    r.y + (r.h - tm.block()) * 0.5f + tm.ascent,
                    "(sem imagem)", text, r.w - 8.0f * k);
            } else {
                // ref definida mas carga falhou — o placeholder HONESTO com
                // o nome da ref (sabe-se O QUE faltou carregar)
                const TextMetrics tm = uictx.textMetrics();
                uictx.labelFitted(r.x + 8.0f * k,
                                  r.y + (r.h - tm.block()) * 0.5f + tm.ascent,
                                  e.image.c_str(), text, r.w - 16.0f * k);
            }
            break;

        case UiElement::Kind::Menu: {
            // lista vertical de botões (uma linha por item do texto).
            // 0.7.4: espaçamento entre itens (spacing), fundo das linhas
            // ON/OFF (alpha do elemento: 0 = só texto) e alinhamento do
            // texto nas linhas (start/center/end — default start = 0.7.3)
            const u32 n = menuLineCount(e);
            const bool rowBg = e.color[3] > 0.001f;
            const TextMetrics tm = uictx.textMetrics();
            for (u32 i = 0; i < n; ++i) {
                std::string label, target;
                menuLineAt(e, i, label, target);
                const UiRect row = menuItemRect(e, r, i);
                if (rowBg) {
                    uictx.panel(row.x, row.y, row.w, row.h, e.color);
                    uictx.frame(row.x, row.y, row.w, row.h, 1.0f * k, line);
                }
                const f32 baseline =
                    row.y + (row.h - tm.block()) * 0.5f + tm.ascent;
                const f32 tw = uictx.fontWidth(label.c_str()) * e.fontScale;
                f32 tx = row.x + 10.0f * k;
                if (e.align == UiElement::Align::Center) {
                    tx = row.x + (row.w - tw) * 0.5f;
                } else if (e.align == UiElement::Align::End) {
                    tx = row.x + row.w - 10.0f * k - tw;
                }
                if (e.align == UiElement::Align::Start) {
                    fText(tx, baseline, label.c_str(), row.w - 20.0f * k);
                } else {
                    fText(tx, baseline, label.c_str(), row.w);   // já centrado
                }
            }
            break;
        }

        case UiElement::Kind::Card: {
            // panel + borda + label (título no topo)
            uictx.panel(r.x, r.y, r.w, r.h, e.color);
            uictx.frame(r.x, r.y, r.w, r.h, 2.0f * k, text);
            const TextMetrics tmE = fMetrics();
            const f32 titleH = tmE.block() + 12.0f * k;
            uictx.panel(r.x, r.y + titleH, r.w, 1.0f * k, line);
            const f32 baseline = r.y + (titleH - tmE.block()) * 0.5f + tmE.ascent;
            fText(r.x + 10.0f * k, baseline, e.text.c_str(), r.w - 20.0f * k);
            break;
        }

        case UiElement::Kind::Article: {
            // texto multilinha com wrap pela largura (greedy por palavras)
            uictx.panel(r.x, r.y, r.w, r.h, e.color);
            const TextMetrics tmE = fMetrics();   // 0.8.6: métricas do elemento
            const f32 rowH = tmE.block() + 4.0f * k;
            f32 cy = r.y + 8.0f * k;
            const char* p = e.text.c_str();
            char word[96];
            char lineBuf[256];
            lineBuf[0] = '\0';
            const f32 maxW = r.w - 20.0f * k;
            while (*p && cy + tmE.block() <= r.y + r.h) {
                // palavra seguinte
                int wl = 0;
                while (*p && *p != ' ' && *p != '\n' && wl < 95) {
                    word[wl++] = *p++;
                }
                word[wl] = '\0';
                const bool hardBreak = (*p == '\n');
                while (*p == ' ' || *p == '\n') {
                    ++p;
                }
                char candidate[256];
                if (lineBuf[0]) {
                    std::snprintf(candidate, sizeof(candidate), "%s %s",
                                  lineBuf, word);
                } else {
                    std::snprintf(candidate, sizeof(candidate), "%s", word);
                }
                if (lineBuf[0] &&
                    uictx.fontWidth(candidate) * e.fontScale > maxW) {
                    // linha cheia → emite e começa nova com a palavra
                    fText(r.x + 10.0f * k, cy + tmE.ascent, lineBuf, maxW);
                    cy += rowH;
                    std::snprintf(lineBuf, sizeof(lineBuf), "%s", word);
                } else {
                    std::snprintf(lineBuf, sizeof(lineBuf), "%s", candidate);
                }
                if (hardBreak) {
                    fText(r.x + 10.0f * k, cy + tmE.ascent, lineBuf, maxW);
                    cy += rowH;
                    lineBuf[0] = '\0';
                }
            }
            if (lineBuf[0] && cy + tmE.block() <= r.y + r.h) {
                fText(r.x + 10.0f * k, cy + tmE.ascent, lineBuf, maxW);
            }
            break;
        }

        case UiElement::Kind::VBox:
        case UiElement::Kind::HBox:
            // 0.7.4 — CONTAINERS: fundo OPCIONAL (alpha > 0) + moldura fina;
            // os FILHOS são desenhados pelo chamador (drawCanvas/viewport 2D)
            // com os rects do resolver e o CLIP do rect do pai
            if (e.color[3] > 0.001f) {
                uictx.panel(r.x, r.y, r.w, r.h, e.color);
                uictx.frame(r.x, r.y, r.w, r.h, 1.0f * k, line);
            }
            break;
    }

    if (sel) {
        // moldura de seleção do editor (mono: frame ACCENT tracejado — sem
        // tracejado no quad batch: frame contínuo fino)
        const f32 accent[4] = {0.9607843f, 0.9607843f, 0.9607843f, 1.0f};
        uictx.frame(r.x, r.y, r.w, r.h, 2.0f * k, accent);
    }
    return true;
}

// desenha o canvas inteiro (FONTE ÚNICA de layout: o resolver). Os FILHOS de
// containers desenham com o CLIP do rect do pai (nada transborda — o Play
// recorta na borda do ecrã, o editor recorta no mini-ecrã: paridade).
u32 drawCanvas(UiContext& uictx, const UiCanvas& c, f32 sw, f32 sh,
               const safe::Insets& ins) {
    std::vector<CanvasLayout> L(c.elements.size());
    resolveCanvasLayout(c, sw, sh, ins, L.data(),
                        static_cast<u32>(c.elements.size()));
    u32 drawn = 0;
    for (size_t i = 0; i < c.elements.size(); ++i) {
        const UiElement& e = c.elements[i];
        if (!L[i].shown || L[i].rect.w <= 0.0f || L[i].rect.h <= 0.0f) {
            continue;
        }
        if (L[i].parentIdx >= 0) {
            // FILHO: desenha dentro do clip do PAI (retângulo do container)
            const UiRect& pr = L[static_cast<size_t>(L[i].parentIdx)].rect;
            const UiContext::ScopedClip clip(uictx, pr);
            if (drawElement(uictx, e, L[i].rect)) {
                ++drawn;
            }
        } else {
            if (drawElement(uictx, e, L[i].rect)) {
                ++drawn;   // só os DESENHADOS contam (invisíveis não)
            }
        }
    }
    return drawn;
}

// ---- hit-test --------------------------------------------------------------------

CanvasHit hitTestCanvas(const Scene& scene, f32 x, f32 y, f32 sw, f32 sh,
                        const safe::Insets& ins) {
    CanvasHit hit;   // o ÚLTIMO que contém o ponto ganha (topo do z-order)
    scene.forEachActive([&](const Tic& t) {
        if (!t.visible) {
            return;   // canvas de TIC invisível não hit-testa (não desenha)
        }
        const UiCanvas* c =
            scene.components().uiCanvases().find(t.handle);
        if (!c) {
            return;
        }
        // 0.7.4: o hit-test usa o MESMO RESOLVER do desenho (containers
        // incluídos — paridade estrutural editor↔Play)
        std::vector<CanvasLayout> L(c->elements.size());
        resolveCanvasLayout(*c, sw, sh, ins, L.data(),
                            static_cast<u32>(c->elements.size()));
        for (size_t i = 0; i < c->elements.size(); ++i) {
            const UiElement& e = c->elements[i];
            if (!L[i].shown) {
                continue;   // invisível (ou dentro de container escondido)
            }
            const UiRect r = L[i].rect;
            // 0.8.6: elemento RODADO → hit no espaço do rect (inversa exata)
            if (!uiRotatedRectHit(r, e.rot, x, y)) {
                continue;
            }
            if (e.kind == UiElement::Kind::Button) {
                hit.valid = true;
                hit.tic = t.handle;
                hit.element = static_cast<i32>(i);
                hit.menuItem = -1;
            } else if (e.kind == UiElement::Kind::Menu) {
                const UiRect row = menuItemRect(e, r, 0);
                const UiRect last = menuItemRect(e, r, menuLineCount(e) - 1u);
                if (y >= row.y && y < last.y + last.h) {
                    // linha = floor((y − topo) / (rowH + spacing)) — a MESMA
                    // geometria do menuItemRect
                    const f32 step = row.h + e.spacing;
                    const i32 rowIdx = step > 0.0f
                        ? static_cast<i32>((y - r.y) / step) : 0;
                    if (rowIdx >= 0 &&
                        static_cast<u32>(rowIdx) < menuLineCount(e)) {
                        hit.valid = true;
                        hit.tic = t.handle;
                        hit.element = static_cast<i32>(i);
                        hit.menuItem = rowIdx;
                    }
                }
            }
        }
    });
    return hit;
}

// ---- ações declarativas ------------------------------------------------------------

namespace {

// procura um elemento por NOME em todos os canvases (o alvo das ações
// Show/Hide/Toggle pode estar em QUALQUER canvas da cena)
UiElement* findElementAnywhere(Scene& scene, const std::string& name,
                               Handle& outOwner) {
    UiElement* found = nullptr;
    scene.forEachActive([&](Tic& t) {
        if (found) {
            return;
        }
        if (UiCanvas* c = scene.components().uiCanvases().find(t.handle)) {
            const i32 idx = c->findElement(name);
            if (idx >= 0) {
                found = &c->elements[static_cast<size_t>(idx)];
                outOwner = t.handle;
            }
        }
    });
    return found;
}

} // namespace

UiActionResult applyUiAction(Scene& scene, const UiElement& e,
                             const UiActionCtx& ctx,
                             const char* targetOverride) {
    UiActionResult out;
    const std::string target =
        targetOverride ? std::string(targetOverride) : e.target;

    switch (e.action) {
        case UiElement::Action::None:
            break;

        case UiElement::Action::ShowPanel:
        case UiElement::Action::HidePanel:
        case UiElement::Action::TogglePanel: {
            Handle owner{};
            UiElement* tgt = findElementAnywhere(scene, target, owner);
            if (!tgt) {
                std::snprintf(out.toast, sizeof(out.toast),
                              "elemento '%s' nao existe", target.c_str());
                std::snprintf(out.log, sizeof(out.log),
                              "ui: acao %s — alvo '%s' nao encontrado",
                              uiActionName(e.action), target.c_str());
                out.wantToast = true;
                break;
            }
            switch (e.action) {
                case UiElement::Action::ShowPanel:   tgt->visible = true;  break;
                case UiElement::Action::HidePanel:   tgt->visible = false; break;
                default:                             tgt->visible = !tgt->visible; break;
            }
            out.acted = true;
            std::snprintf(out.toast, sizeof(out.toast), "%s: %s %s",
                          target.c_str(),
                          tgt->visible ? "mostrado" : "escondido",
                          uiActionName(e.action));
            std::snprintf(out.log, sizeof(out.log),
                          "ui: %s %s (%s)", uiActionName(e.action),
                          target.c_str(), tgt->visible ? "visivel" : "escondido");
            break;
        }

        case UiElement::Action::LoadScene:
        case UiElement::Action::TransitionScene: {
            // 0.7.1: o carregamento vive no CHAMADOR (projeto/transição);
            // aqui decide-se se a cena existe (toast honesto se não). Sem
            // callback loadScene a ação NÃO finge sucesso — o toast diz o
            // que falta. Scene.Load = troca INSTANTÂNEA; Scene.Transition =
            // fade/slide conforme o param do elemento.
            const bool trans = e.action == UiElement::Action::TransitionScene;
            if (target.empty()) {
                std::snprintf(out.toast, sizeof(out.toast), "acao sem alvo");
                out.wantToast = true;
                break;
            }
            if (ctx.sceneExists && !ctx.sceneExists(target, ctx.user)) {
                std::snprintf(out.toast, sizeof(out.toast),
                              "cena '%s' nao existe", target.c_str());
                std::snprintf(out.log, sizeof(out.log),
                              "ui: cena '%s' FALHOU — nao existe no projeto",
                              target.c_str());
                out.wantToast = true;
                break;
            }
            if (!ctx.loadScene) {
                std::snprintf(out.toast, sizeof(out.toast),
                              "cena '%s': sem carregador", target.c_str());
                std::snprintf(out.log, sizeof(out.log),
                              "ui: cena '%s' existe — loadScene nao ligado",
                              target.c_str());
                out.wantToast = true;
                break;
            }
            const SceneSwap style =
                !trans ? SceneSwap::Instant
                       : (uiElementTransition(e) == UiElement::Transition::Slide
                              ? SceneSwap::Slide
                              : SceneSwap::Fade);
            ctx.loadScene(target, style, ctx.user);
            out.acted = true;
            std::snprintf(out.toast, sizeof(out.toast),
                          trans ? "cena: %s (%s)" : "cena: %s",
                          target.c_str(),
                          style == SceneSwap::Slide ? "slide" : "fade");
            std::snprintf(out.log, sizeof(out.log),
                          "ui: cena '%s' a carregar (%s)", target.c_str(),
                          style == SceneSwap::Instant ? "instantaneo"
                          : style == SceneSwap::Slide ? "slide" : "fade");
            break;
        }

        case UiElement::Action::Spawn: {
            PresetKind kind = PresetKind::Count;
            if (target == "PlayerBody3D")        kind = PresetKind::PlayerBody3D;
            else if (target == "CharacterBody3D") kind = PresetKind::CharacterBody3D;
            else if (target == "StaticBody3D")    kind = PresetKind::StaticBody3D;
            else if (target == "RigidBody3D")     kind = PresetKind::RigidBody3D;
            if (kind == PresetKind::Count || !ctx.spawnPreset) {
                std::snprintf(out.toast, sizeof(out.toast),
                              "preset '%s' invalido", target.c_str());
                std::snprintf(out.log, sizeof(out.log),
                              "ui: spawn '%s' FALHOU — preset invalido",
                              target.c_str());
                out.wantToast = true;
                break;
            }
            const Handle h = ctx.spawnPreset(kind, ctx.user);
            if (!h.valid()) {
                std::snprintf(out.toast, sizeof(out.toast), "spawn falhou");
                out.wantToast = true;
                break;
            }
            out.acted = true;
            std::snprintf(out.toast, sizeof(out.toast), "spawn: %s",
                          target.c_str());
            std::snprintf(out.log, sizeof(out.log), "ui: spawn %s criado",
                          target.c_str());
            break;
        }
    }
    return out;
}


// ---- 0.8.6 — GIZMOS de UI (escalar/rodar) + hit-test rodado -----------------

// hit-test de um rect RODADO: o ponto é ANTI-rotacionado à volta do centro
// e testado axis-aligned (a inversa exata do que o desenho faz)
bool uiRotatedRectHit(const UiRect& r, f32 rotDeg, f32 px, f32 py) {
    if (rotDeg == 0.0f) {
        return inRect(r, px, py);
    }
    const f32 cx = r.x + r.w * 0.5f;
    const f32 cy = r.y + r.h * 0.5f;
    const f32 rad = rotDeg * -0.01745329252f;   // inversa = -ângulo
    const f32 ca = std::cos(rad), sa = std::sin(rad);
    const f32 dx = px - cx, dy = py - cy;
    const f32 lx = cx + dx * ca - dy * sa;   // ponto no espaço do rect
    const f32 ly = cy + dx * sa + dy * ca;
    return inRect(r, lx, ly);
}

// handles nos CANTOS do rect (screen px): 0=TL 1=TR 2=BL 3=BR — centrados
UiRect uiGizmoCornerRect(const UiRect& r, u32 corner, f32 size) {
    static const f32 sx[4] = {0.0f, 1.0f, 0.0f, 1.0f};
    static const f32 sy[4] = {0.0f, 0.0f, 1.0f, 1.0f};
    const f32 cx = r.x + r.w * sx[corner & 3];
    const f32 cy = r.y + r.h * sy[corner & 3];
    return {cx - size * 0.5f, cy - size * 0.5f, size, size};
}

// pega de ROTAÇÃO: acima do topo-centro, a 26 px do rect (handle "solto")
UiRect uiGizmoRotateHandleRect(const UiRect& r, f32 size) {
    const f32 cx = r.x + r.w * 0.5f;
    const f32 cy = r.y - 26.0f;
    return {cx - size * 0.5f, cy - size * 0.5f, size, size};
}

// snap 15° (o MESMO passo do snap de rotação dos gizmos 3D)
f32 uiGizmoSnapRot(f32 deg) {
    return std::round(deg / 15.0f) * 15.0f;
}

} // namespace ui
} // namespace vv
