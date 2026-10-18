// tests/test_editor_buffer.cpp — 0.9.6.2 (R-019): O BUFFER E O CURSOR.
//
// O BUG que o dono apanhou no device: "o cursor nunca fica dentro de
// 'on moment { }' nem de 'allmoments { }'; fica sempre fora. Não dá para
// mudar de linha nem para escrever onde se quer. Isto torna o editor
// inutilizável." A CAUSA RAIZ (leitura do código): o toque no corpo
// CALCULAVA o offset sob o dedo mas nunca o aplicava ao caret — só
// alimentava a palavra da dica; o cursor ficava no fim do buffer (scripts
// guardados abrem com caret = buf.size()) e tudo o que se digitava caía
// lá. O FIX: o toque aplica o offset (o carácter mais próximo pelas
// métricas REAIS de cada code point, descontando a coluna dos números de
// linha) e o ENTER herda a indentação da linha.
//
// ESTES testes aférram o buffer pelo caminho PÚBLICO (applyEvent — o MESMO
// que o IME do sistema e o teclado próprio usam) + as funções puras da
// geometria do cursor (caretInLineForX com métricas simuladas — largura
// fixa e largura proporcional). O caminho REAL da UI (tap → caret) é o
// FASE 12.11 do c33_virtual; o engine.log do replay mostra o log do toque.
#include "TestFramework.h"

#include <algorithm>
#include <string>

#include "ui/ScriptEditor.h"   // applyEvent/State/kSkeleton + geometria
#include "core/Scene.h"
#include "platform/ImeQueue.h"

using namespace vv;
namespace sw = editor::scriptwin;

namespace {

// abre um State com o ESQUELETO (sem cena — o TIC é opcional para o buffer)
sw::State freshSkeleton() {
    sw::State st;
    st.open = true;
    st.buf = sw::kSkeleton;
    st.caret = sw::kSkeletonCaret;
    return st;
}

ime::Event textEv(const char* t) {
    ime::Event ev;
    ev.isText = true;
    ev.text = t;
    return ev;
}

ime::Event keyEv(ime::Key k) {
    ime::Event ev;
    ev.isText = false;
    ev.key = k;
    return ev;
}

} // namespace

// (1) INSERIR no MEIO (não no fim): escrever "x" cai no caret corrente e
// o caret SEGUE a inserção — o que estava depois continua depois
TEST(editor_buffer_insere_no_caret_meio) {
    sw::State st = freshSkeleton();
    const u32 caret0 = st.caret;   // entre o "{ " e o "}" de allmoments
    ASSERT(caret0 < st.buf.size());
    EXPECT(applyEvent(st, textEv("x")));
    EXPECT(st.buf[caret0] == 'x');   // caiu NO caret (não no fim)
    EXPECT(st.caret == caret0 + 1);  // o cursor seguiu
    // inserir no MEIO preserva o resto da linha
    EXPECT(st.buf.find("allmoments { x}") != std::string::npos ||
           st.buf.find("allmoments {x}") != std::string::npos);
    // o fim do buffer NÃO recebe nada (o bug era escrever sempre no fim)
    EXPECT(st.buf.back() == '\n');
}

// (2) APAGAR: o carácter ANTES do caret; no início de linha junta-a à
// anterior (o '\n' é o carácter apagado)
TEST(editor_buffer_apagar_e_juntar_linhas) {
    sw::State st = freshSkeleton();
    // caret no INÍCIO da 3.ª linha ("  allmoments { }")
    const u32 l3 = sw::lineStartOfOffset(st.buf, st.caret);
    st.caret = l3;
    const u32 len0 = (u32)st.buf.size();
    EXPECT(applyEvent(st, keyEv(ime::Key::Del)));
    EXPECT(st.buf.size() == len0 - 1);
    // a 2.ª e a 3.ª linha estão AGORA JUNTAS na mesma linha física
    const u32 nl2 = (u32)std::count(st.buf.begin(), st.buf.end(), '\n');
    EXPECT(nl2 == 3);   // o esqueleto tinha 4; juntar → 3
    // caret no início do buffer: apagar NÃO faz nada (sem crash, sem salto)
    st.caret = 0;
    EXPECT(applyEvent(st, keyEv(ime::Key::Del)));
    EXPECT(st.caret == 0);
}

// (3) NOVA LINHA: o ENTER herda a indentação da linha corrente
TEST(editor_buffer_enter_herde_indentacao) {
    sw::State st = freshSkeleton();
    // caret dentro de "  allmoments { }" (indent 2)
    EXPECT(applyEvent(st, keyEv(ime::Key::Enter)));
    // a linha nova começa com os MESMOS 2 espaços
    const u32 caret = st.caret;
    const u32 ls = sw::lineStartOfOffset(st.buf, caret);
    const u32 le = sw::lineEndOf(st.buf, ls);
    const std::string newline = st.buf.substr(ls, le - ls);
    EXPECT(newline.size() >= 2 && newline[0] == ' ' && newline[1] == ' ');
}

// (4) ÍNDICE ↔ LINHA/COLUNA: as conversões da geometria (a MESMA que o
// draw e o toque usam)
TEST(editor_buffer_indice_linha_coluna) {
    const std::string s = "central main {\n  on moment { }\n  allmoments { }\n}\n";
    EXPECT(sw::lineIndexOf(s, 0) == 0);
    EXPECT(sw::lineIndexOf(s, 14) == 0);   // antes do '\n'
    EXPECT(sw::lineIndexOf(s, 15) == 1);   // início da linha 2
    EXPECT(sw::lineIndexOf(s, 31) == 2);   // "  allmoments…"
    EXPECT(sw::lineIndexOf(s, 48) == 3);   // "}"
    EXPECT(sw::lineStartOfOffset(s, 20) == 15);
    EXPECT(sw::columnOfOffset(s, 20) == 5);
    EXPECT(sw::lineEndOf(s, 15) == 30);    // o '\n' da 2.ª linha
    EXPECT(sw::lineEndOf(s, 48) == 49);    // última linha
    // além do fim: clamp (sem crash)
    EXPECT(sw::lineIndexOf(s, 9999) == 4);
    // o fim do buffer (após o último newline) é uma ÚLTIMA linha vazia
    EXPECT(sw::lineStartOfOffset(s, 9999) == 50);
}

// (5) LOCALIZAÇÃO DO TOQUE com MÉTRICAS SIMULADAS (largura FIXA 10px por
// code point): o toque a x escolhe o carácter mais próximo (meia largura
// decide o empate); nunca passa do fim da linha
TEST(editor_buffer_toque_metricas_fixas) {
    const std::string line = "abcdef";
    const auto fixed = [](const char*) { return 10.0f; };
    EXPECT(sw::caretInLineForX(line, 0.0f, fixed) == 0);
    // a fronteira decide: antes da meia largura (5px) → o próprio carácter;
    // depois → o seguinte (o toque a 9px está mais perto da fronteira 10)
    EXPECT(sw::caretInLineForX(line, 4.9f, fixed) == 0);
    EXPECT(sw::caretInLineForX(line, 9.0f, fixed) == 1);
    EXPECT(sw::caretInLineForX(line, 11.0f, fixed) == 1);
    EXPECT(sw::caretInLineForX(line, 55.0f, fixed) == 5);
    EXPECT(sw::caretInLineForX(line, 999.0f, fixed) == 6);  // fim
    // largura PROPORCIONAL (o "i" mede 4, o "m" mede 16): a decisão usa a
    // largura REAL de cada carácter, não uma coluna fixa
    const std::string prop = "mim";
    const auto widths = [&](const char* t) {
        return std::string(t) == "i" ? 4.0f : 16.0f;
    };
    EXPECT(sw::caretInLineForX(prop, 8.0f, widths) == 0);
    EXPECT(sw::caretInLineForX(prop, 12.0f, widths) == 1);
    EXPECT(sw::caretInLineForX(prop, 25.0f, widths) == 2);
    EXPECT(sw::caretInLineForX(prop, 30.0f, widths) == 3);
    // UTF-8: o acento é UM code point (o toque no acento não parte bytes)
    const std::string acc = "x\xC3\xA1z";   // x á z
    u32 calls = 0;
    const auto count = [&](const char*) { ++calls; return 10.0f; };
    sw::caretInLineForX(acc, 999.0f, count);
    EXPECT(calls == 3);   // 3 code points medidos (não 4 bytes)
}

// (6) O CURSOR NUNCA É REPÔSTO NO FIM: uma sequência de eventos (texto,
// setas, apagar) mantém o cursor onde a edição está — a única forma de ir
// ao fim é editar no fim. E as setas cruzam linhas (Left no início da linha
// recua para o fim da anterior)
TEST(editor_buffer_caret_nunca_salta_para_o_fim) {
    sw::State st = freshSkeleton();
    // caret no meio da linha 3 → escreve → o caret SEGUE (não salta)
    const u32 mid = sw::kSkeletonCaret;
    EXPECT(applyEvent(st, textEv("ab")));
    EXPECT(st.caret == mid + 2);
    // Left ×2 → volta ao ponto de partida (as setas movem o MESMO caret)
    EXPECT(applyEvent(st, keyEv(ime::Key::Left)));
    EXPECT(applyEvent(st, keyEv(ime::Key::Left)));
    EXPECT(st.caret == mid);
    // Left no INÍCIO da linha → o fim da linha ANTERIOR (cruza o '\n')
    const u32 l3 = sw::lineStartOfOffset(st.buf, st.caret);
    st.caret = l3;
    EXPECT(applyEvent(st, keyEv(ime::Key::Left)));
    EXPECT(sw::lineIndexOf(st.buf, st.caret) == 1);   // linha 2
    // Up/Down mantêm a coluna (em bytes) e clampam ao fim da linha
    st.caret = sw::lineStartOfOffset(st.buf, st.buf.size() - 1);
    EXPECT(applyEvent(st, keyEv(ime::Key::Up)));
    EXPECT(st.caret < st.buf.size());
    // o caret que está no FIM continua a receber texto no fim (a regra:
    // "a não ser que a edição seja no fim" — o fim é um sítio legítimo)
    st.caret = (u32)st.buf.size();
    EXPECT(applyEvent(st, textEv("z")));
    EXPECT(st.caret == (u32)st.buf.size());
    EXPECT(st.buf.back() == 'z');
}
