#pragma once
// platform/ImeQueue.h — IME DO SISTEMA (0.9.1) + POLÍTICA DE ORIENTAÇÃO.
//
//   0.9.1 — INPUT DE TEXTO REAL. Até aqui a engine só tinha o teclado
//   in-app (renomear rápido em landscape, propósito 0..8 — mantido).
//   Editores de TEXTO/CÓDIGO (editor de script 0.9.2 e futuros) usam o
//   IME DO SISTEMA:
//
//     Java (VvActivity)                          Engine (C++)
//     ─────────────────────                      ─────────────
//     EditText invisível ──InputConnection──►  nativeOnImeText/nativeOnImeKey
//     (show/hide POR CONTA DA ENGINE:          ──► ESTA fila (thread UI →
//       jniImeShow/jniImeHide)                    thread da engine, consumido
//                                                 por frame)
//
//   POLÍTICA DE ORIENTAÇÃO (0.9.1 §1): janelas de TEXTO PESADO pedem
//   PORTRAIT via JNI (setRequestedOrientation); ao fechar, volta a
//   LANDSCAPE. O ESTADO fica aqui (não no Java): afervável na suíte,
//   LOGADO a cada mudança ("orientação: portrait pedida (…)") — o log
//   viewer do C33 mostra a sequência.
//
//   GL-free / Android-free: testável no hospedeiro (test_wiring091).
//   O lado JNI (Java↔fila) vive no StorageBridge.cpp — o padrão da casa
//   (fila PendingResult/ProjectSlot: a Java só empurra, a engine consome
//   no momento certo, NUNCA processa no thread da UI).
#include "core/Types.h"

#include <cstdint>
#include <deque>
#include <mutex>
#include <string>

namespace vv::ime {

// teclas do IME que a engine conhece (o resto do teclado chega como TEXTO
// commitado — o contrato do InputConnection é isso; DEL/ENTER/setas são os
// eventos de tecla que vêm por sendKeyEvent/deleteSurroundingText)
enum class Key : std::uint8_t {
    None = 0,
    Del,      // backspace (KEYCODE_DEL 67 ou deleteSurroundingText)
    Enter,    // KEYCODE_ENTER 66 / performEditorAction
    Up, Down, Left, Right,   // DPAD 19..22 (v0: ignorados pela janela de
                             // texto — reservados para o cursor 0.9.2)
};

// evento da fila: texto commitado OU tecla
struct Event {
    bool        isText = false;
    std::string text;            // UTF-8 (isText)
    Key         key = Key::None; // (!isText)
};

// mapa puro do keycode Android → Key (puro p/ a suíte; None = ignorar)
Key fromAndroidKeycode(int kc);

// ---- fila (thread UI → engine) --------------------------------------------
void pushText(const char* utf8);   // texto commitado (pode ser vazio — ignora)
void pushKey(Key k);               // tecla (None = ignora)
bool poll(Event& out);             // consome UM evento; false = fila vazia
u32  pending();                    // diagnóstico/testes
void clearForTest();               // reset entre casos (só suíte)

// ---- política de orientação (estado + log) ---------------------------------
enum class Orientation : std::uint8_t { Landscape = 0, Portrait = 1 };

// estado ATUAL pedido pela engine (afervável na suíte)
Orientation orientation();
// grava o pedido; devolve true se MUDOU (o chamador faz o JNI). O log é
// AQUI — uma linha por mudança, com o motivo (janela de texto aberta/fechada)
bool setOrientation(Orientation o, const char* reason);

} // namespace vv::ime
