#pragma once
// platform/AudioStartGate.h — REG-002 / R-006 (hotfix 0.9.3): o PORTÃO DE
// ARRANQUE do backend de áudio.
//
// O BUG (tombstones 00-17/21-31 no realme RMX3624/Unisoc, app antiga
// com.goni.runtime): startAudio() chamado de novo (onResume repetido sem
// um onPause que parasse o stream) abria um SEGUNDO stream por cima do
// primeiro — o primeiro FUGIA (nunca fechado) e o driver AAudio do
// Unisoc crashava dentro de AAudio_createStreamBuilder (SIGSEGV,
// SEGV_ACCERR). As três portas de entrada daquela classe de bug:
//   • arranque duplo = fuga de stream vivo (o driver do vendor enrola);
//   • builder/stream usados depois de destruídos;
//   • retorno de chamada AAudio/Oboe assumido sem verificação.
//
// O PORTÃO: NENHUM backend arranca duas vezes. tryEnter() só passa 1×;
// o segundo chamador recebe false e NÃO TOCA NO HARDWARE (idempotência
// obrigatória — o stream que já está a tocar continua). release() abre o
// portão quando o arranque FALHOU (a próxima tentativa pode tentar);
// open() abre na paragem normal.
//
// Cabeçalho PURO (host + device): os sentinelas do CI afervam o
// comportamento com o stub (AudioOut.cpp) e com o fake do Oboe
// (tests/stub/oboe/Oboe.h) — caso regress_audio_lifecycle.
#include <atomic>

namespace vv::audioout {

class StartGate {
public:
    // tenta fechar o portão; false = JÁ FECHADO (arranque em curso ou
    // stream ativo — o chamador NÃO abre stream nenhum)
    bool tryEnter() {
        bool expected = false;
        return gate_.compare_exchange_strong(expected, true);
    }
    // o arranque FALHOU → o portão abre (o próximo start pode tentar)
    void release() { gate_.store(false, std::memory_order_release); }
    // paragem normal → o portão abre
    void open() { gate_.store(false, std::memory_order_release); }
    bool held() const { return gate_.load(std::memory_order_acquire); }

private:
    std::atomic<bool> gate_{false};
};

} // namespace vv::audioout
