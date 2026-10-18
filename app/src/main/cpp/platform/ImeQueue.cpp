// platform/ImeQueue.cpp — implementação da fila IME + política de orientação
// (contrato em ImeQueue.h). As funções usam UM mutex (o mesmo padrão da
// fila PendingResult/ProjectSlot: a Java empurra no thread da UI, a engine
// consome no frame). O estado de orientação é parte do MESMO módulo para o
// contrato ficar ÚNICO (uma linha de log por mudança).
#include "platform/ImeQueue.h"

#include "platform/EngineLog.h"

namespace vv::ime {

namespace {

// constexpr puro — os keycodes Android que interessam (o mapa é estável
// na ABI do Android: DEL=67, ENTER=66, DPAD=19..22)
constexpr int kKcDpadUp    = 19;
constexpr int kKcDpadDown  = 20;
constexpr int kKcDpadLeft  = 21;
constexpr int kKcDpadRight = 22;
constexpr int kKcEnter     = 66;
constexpr int kKcDel       = 67;
constexpr int kKcTab       = 61;   // 0.9.5: os esqueletos (editor que ensina)

struct Queue {
    std::mutex mu;
    std::deque<Event> events;
    Orientation orientation = Orientation::Landscape;   // a app nasce landscape
    f32 imeInsetPx = 0.0f;   // 0.9.6.8 (GRUPO E): a faixa do IME em px (0=fechado)
};

Queue& q() {
    static Queue inst;   // única por processo (a lib carrega 1×)
    return inst;
}

} // namespace

Key fromAndroidKeycode(int kc) {
    switch (kc) {
        case kKcDel:        return Key::Del;
        case kKcEnter:      return Key::Enter;
        case kKcDpadUp:     return Key::Up;
        case kKcDpadDown:   return Key::Down;
        case kKcDpadLeft:   return Key::Left;
        case kKcDpadRight:  return Key::Right;
        case kKcTab:        return Key::Tab;
        default:            return Key::None;
    }
}

void pushText(const char* utf8) {
    if (!utf8 || !*utf8) {
        return;   // commit vazio (composição limpa) = nada a escrever
    }
    Queue& qq = q();
    std::lock_guard<std::mutex> lk(qq.mu);
    Event ev;
    ev.isText = true;
    ev.text = utf8;
    qq.events.push_back(std::move(ev));
    // defesa: uma fila descontrolada (teclado acelerado) nunca cresce sem
    // teto — 256 eventos é ~16 frases; além disso o utilizador não vê
    // o texto aparecer (o device morreu?)
    while (qq.events.size() > 256) {
        qq.events.pop_front();
    }
}

void pushKey(Key k) {
    if (k == Key::None) {
        return;
    }
    Queue& qq = q();
    std::lock_guard<std::mutex> lk(qq.mu);
    Event ev;
    ev.isText = false;
    ev.key = k;
    qq.events.push_back(std::move(ev));
    while (qq.events.size() > 256) {
        qq.events.pop_front();
    }
}

bool poll(Event& out) {
    Queue& qq = q();
    std::lock_guard<std::mutex> lk(qq.mu);
    if (qq.events.empty()) {
        return false;
    }
    out = std::move(qq.events.front());
    qq.events.pop_front();
    return true;
}

u32 pending() {
    Queue& qq = q();
    std::lock_guard<std::mutex> lk(qq.mu);
    return static_cast<u32>(qq.events.size());
}

void clearForTest() {
    Queue& qq = q();
    std::lock_guard<std::mutex> lk(qq.mu);
    qq.events.clear();
    qq.orientation = Orientation::Landscape;
    qq.imeInsetPx = 0.0f;   // 0.9.6.8 (GRUPO E): o reset cobre o inset
}

Orientation orientation() {
    Queue& qq = q();
    std::lock_guard<std::mutex> lk(qq.mu);
    return qq.orientation;
}

bool setOrientation(Orientation o, const char* reason) {
    Queue& qq = q();
    std::lock_guard<std::mutex> lk(qq.mu);
    if (qq.orientation == o) {
        return false;   // sem mudança — SEM log (o device repete pedidos)
    }
    qq.orientation = o;
    elog::info("orientacao: %s pedida (%s)",
               o == Orientation::Portrait ? "portrait" : "landscape",
               (reason && *reason) ? reason : "sem motivo");
    return true;
}

// ---- 0.9.6.8 (GRUPO E) · o inset do IME ------------------------------------
// A VvActivity mede a faixa do IME (rootHeight − visibleFrame.bottom) e
// SÓ empurra MUDANÇAS (o listener dispara a cada layout — o valor é o
// mesmo enquanto o teclado está parado). O LOG é uma linha por mudança:
// o dono segue o IME abrir/fechar no engine.log com os px E os dp.
void setBottomInset(f32 px) {
    Queue& qq = q();
    std::lock_guard<std::mutex> lk(qq.mu);
    const f32 v = px > 0.0f ? px : 0.0f;
    if (qq.imeInsetPx == v) {
        return;   // sem mudança — silêncio (o listener dispara MUITO)
    }
    const bool wasClosed = qq.imeInsetPx <= 0.0f;
    const bool nowClosed = v <= 0.0f;
    qq.imeInsetPx = v;
    if (wasClosed != nowClosed) {
        elog::info("ime: %s (inset %.0f px)", nowClosed ? "fechado" : "aberto",
                   (double)v);
    } else {
        elog::info("ime: inset %.0f px (o teclado mudou de tamanho)", (double)v);
    }
}

f32 bottomInset() {
    Queue& qq = q();
    std::lock_guard<std::mutex> lk(qq.mu);
    return qq.imeInsetPx;
}

bool insetVisible() {
    return bottomInset() > 0.0f;
}

} // namespace vv::ime
