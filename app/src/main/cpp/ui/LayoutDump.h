#pragma once
// ui/LayoutDump.h — 0.9.6.5 (GRUPO B · FERRAMENTAS DE VERIFICAÇÃO):
// o LAYOUT EXPORTADO + O VALIDADOR.
//
// O QUE ISTO É: o registo do que um frame DESENHOU de verdade. Quando o
// audit está ligado, cada widget do UiContext (o CHOKE POINT único — os
// rects saem do MESMO código que desenha) acrescenta a sua entrada; o
// ficheiro JSON que o dono abre é a lista EXATA de painéis/labels/botões
// no ecrã, com os números reais em px. A lição R-020 (a sentinela que
// codificava o bug): nada de duplicar o layout noutro sítio que possa
// divergir — o registo É o draw.
//
// O VALIDADOR é PURO (GL-free/Android-free — a matemática vive aqui e
// corre no CI como o scroll:: e o textfit::): recebe o Record + as regras
// da casa (48dp de toque, nada fora do ecrã, interativos que não se
// pisam, texto que não sangra) e devolve os problemas com severidade.
// O QUE NÃO É regra: painéis sobre painéis (camadas legítimas — o modal
// tapa o editor), labels DENTRO de scroll cortadas na borda da região
// (é o scroll a funcionar). O relatório do Grupo B coloca os problemas
// que HOJE existem; os Grupos C-I baixam as contagens a zero.
//
// JSON: sai pelo core/Json.h (a ordem dos membros é preservada — o dump
// é determinístico, os testes de round-trip ficam estáveis).
#include "core/Types.h"
#include "core/Json.h"
#include <string>
#include <vector>

namespace vv {
namespace layout {

// ---- OS PISOS DE TOQUE (PASSO 1 · 0.9.6.14 — a tabela da spec do dono) ------
// A LEI DE OURO: DESENHO 32dp / TOQUE 40dp — nada ≥48 no editor (era o
// piso único de 48dp da 0.9.0). Os elementos DE LINHA tomam a altura da
// LINHA da spec (o alvo é a linha inteira — a barra de 36, o campo de 32,
// o cabeçalho de 28): o piso DELES é a altura da linha, NÃO o 40 do botão
// solto. As constantes vivem AQUI (o validador é a vara de medir) e os
// desenhistas flagam a entrada com auditRowFloorNext(kXxx) — os números
// NUNCA se repetem frouxos pelos ficheiros.
constexpr f32 kTouchFloorDp = 40.0f;   // botão SOLTO (desenho 32 no alvo 40)
constexpr f32 kRowFloorDp   = 36.0f;   // LINHA: top bar / listas / consola
constexpr f32 kFieldFloorDp = 32.0f;   // CAMPO: caixas X/Y/Z / tabs de baixo
constexpr f32 kHeadFloorDp  = 28.0f;   // CABEÇALHO/chips dentro do cabeçalho

// ---- A ENTRADA (um widget desenhado) ---------------------------------------
struct Entry {
    enum Kind : u8 { Panel, Frame, Label, Button, Scroll, Slider };
    Kind kind = Panel;
    u64  id = 0;          // interativos: o id do gesto (button/slider/scroll)
    f32  x = 0, y = 0, w = 0, h = 0;   // o rect DESENHADO (px, topo-esquerda)
    f32  fullW = 0;       // Label: largura do texto INTEIRO (antes do fit)
    bool truncated = false;   // labelFitted que cortou com "…"
    bool clipped = false;     // desenhado dentro de um clip de scroll
    // 0.9.6.8 (GRUPO E): a tecla da BARRA DE SÍMBOLOS — o precedente da
    // exceção VIGIADA (o piso compacto continua no validador; desde a
    // PASSO 1 o piso regular também é 40, o flag passa a ser DOCUMENTAÇÃO
    // no dump JSON — a sentinela R-027 reescrita prova o novo piso)
    bool compact = false;
    // PASSO 1 (0.9.6.14): o piso DE LINHA (kRowFloorDp/kFieldFloorDp/
    // kHeadFloorDp) — 0 = botão solto (kTouchFloorDp). O validador afere
    // a entrada pelo piso da SUA classe; a flag vive SÓ durante a chamada
    // (o padrão auditCompactNext_ — nunca escapa)
    f32 rowFloorDp = 0.0f;

    bool interactive() const {
        return kind == Button || kind == Scroll || kind == Slider;
    }
    const char* kindName() const {
        switch (kind) {
            case Panel:  return "panel";
            case Frame:  return "frame";
            case Label:  return "label";
            case Button: return "botao";
            case Scroll: return "scroll";
            case Slider: return "slider";
        }
        return "?";
    }
};

// ---- O REGISTO (o ecrã auditado) -------------------------------------------
struct Record {
    const char* screen = "";
    f32 screenW = 0.0f, screenH = 0.0f;
    f32 insetT = 0.0f, insetB = 0.0f, insetL = 0.0f, insetR = 0.0f;
    f32 density = 1.0f;          // px por dp (o 48dp multiplica por aqui)
    std::vector<Entry> entries;

    void clear() { *this = Record{}; }

    void add(const Entry& e) {
        // o cap é LIMIAR DE AVISO, não corte (a lição dos runs do
        // UiContext 0.8.4: nada se perde em silêncio); 4096 entradas é um
        // ecrã inteiro de labels com folga — acima disso é bug de caller
        if (entries.size() < 4096) {
            entries.push_back(e);
        }
    }

    // o rect útil do ecrã (contentRect — os interativos vivem aqui dentro)
    f32 contentX() const { return insetL; }
    f32 contentY() const { return insetT; }
    f32 contentW() const { return screenW - insetL - insetR; }
    f32 contentH() const { return screenH - insetT - insetB; }
};

// JSON determinístico: {screen,w,h,insets,density,entries:[{kind,id,x,y,w,
// h,fullW,trunc,clip}…]}. Os números saem arredondados a 0.1px (o dump fica
// legível e o round-trip dos testes estável).
Json toJson(const Record& r);

// ---- O VALIDADOR (as regras da casa) ----------------------------------------
struct Problem {
    enum Rule : u8 {
        ForaDoEcra,     // interativo fora do contentRect (inacessível)
        Sobreposto,     // dois interativos a pisarem-se (nenhum contém o outro)
        ToquePequeno,   // interativo abaixo do piso da SUA classe (40/36/32/28)
        TextoTruncado,  // label que o fit cortou com "…" (perdeu informação)
        TextoSangra,    // label SEM clip que sai do contentRect (o bug do C33)
        RectDegenerado  // interativo com rect ≤ 0 (zona morta)
    };
    enum Sev : u8 { Erro, Aviso };
    Rule rule = ForaDoEcra;
    Sev sev = Erro;
    u32 ia = 0, ib = 0;   // entradas envolvidas (ib só no Sobreposto)

    const char* ruleName() const {
        switch (rule) {
            case ForaDoEcra:    return "fora_do_ecra";
            case Sobreposto:    return "sobreposto";
            case ToquePequeno:  return "toque_pequeno";
            case TextoTruncado: return "texto_truncado";
            case TextoSangra:   return "texto_sangra";
            case RectDegenerado:return "rect_degenerado";
        }
        return "?";
    }
    const char* sevName() const { return sev == Erro ? "ERRO" : "aviso"; }
};

// as regras, UMA A UMA, com limiar de tolerância de 0.5px (o anti-ruído
// do arredondamento de float→px nos cantos). PISOS (PASSO 1): botão solto
// 40dp · linha 36 · campo 32 · cabeçalho 28 — a tabela da spec do dono.
std::vector<Problem> validate(const Record& r);

// a linha humana-legível (o log do device e o auditoria.txt do projeto)
std::string describe(const Record& r, const Problem& p);

// o bloco de relatório inteiro (o que o Auditoria do ecrã escreve/loga):
// header + N problemas + contagem por regra; verde quando vazio
std::string report(const Record& r, const std::vector<Problem>& ps);

} // namespace layout
} // namespace vv
