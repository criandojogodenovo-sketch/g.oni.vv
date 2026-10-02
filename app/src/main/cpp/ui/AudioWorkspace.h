#pragma once
// ui/AudioWorkspace.h — o WORKSPACE modo ÁUDIO (0.8.11). Ver .cpp.
//
// O HOST é a LIGAÇÃO ao device (main.cpp implementa): import/gravação/
// preview/apagar/atribuir/renomear + os dados por clip (codec, duração,
// picos). O desenho é PURO (UiContext) — afervel no CI com fake host.
//
// GEOMETRIA: o workspace substitui o VIEWPORT (o MESMO rect do editor de
// UI: entre a toolbar e a timeline, sem o painel direito quando visível) —
// a 1ª versão desenhava na ÁREA ÚTIL TODA (sobrepunha a toolbar e os
// painéis e fazia early-return no frame; apanhado na revisão). Assim como
// o editor 2D, ele É um painel do editor, não um modo de ecrã cheio.
#include <functional>
#include <string>
#include <vector>

#include "ui/ScrollMath.h"
#include "ui/UiContext.h"

namespace vv {
namespace editor {

// estado do workspace (o main é o dono; sobrevive à troca de aba)
struct AudioWorkspaceState {
    u32 selected = 0;        // clip selecionado na lista
    bool confirmDelete = false;
};

// a LIGAÇÃO ao device (o main injeta; os testes usam um fake com CAPTURES
// — std::function, não ponteiros crus: os fakes contam as chamadas). TODOS
// os campos são OPCIONAIS: o desenho nunca chama um alvo vazio.
struct AudioWorkspaceHost {
    // ações
    std::function<void()> onImport;          // abre o navegador (raiz Music)
    std::function<void()> onRecord;          // grava/para (mic → .gi ADPCM)
    std::function<void()> onPreviewToggle;   // play/pausa do clip selecionado
    std::function<void()> onPreviewStop;
    std::function<void(const std::string& rel)> onDelete;
    std::function<void()> onAssign;          // atribui ao AudioPlayer do TIC
    std::function<void()> onRename;          // renomear (teclado in-app)
    // dados por clip (o main lê do cache de GiClip)
    const char* (*codecOf)(const std::string& rel) = nullptr;   // "adpcm"
    f32 (*durationOf)(const std::string& rel) = nullptr;        // segundos
    const std::vector<f32>& (*peaksOf)(const std::string& rel) = nullptr;
    // estado vivo
    bool recording = false;
    int recordSecs = 0;
    f32 recordLevel = 0.0f;   // 0..1 (o medidor)
    bool previewing = false;
    f32 (*previewPos)() = nullptr;   // 0..1 (a linha da waveform)
};

void audioWorkspaceReset(AudioWorkspaceState& st);

// desenha o workspace no rect do VIEWPORT (o mesmo do editor de UI);
// devolve 1..N quando o clip i-1 foi TOCADO (seleção) — os botões agem
// pelo host
int drawAudioWorkspace(UiContext& ui, const InputState& in,
                       const UiRect& view, AudioWorkspaceState& st,
                       const std::vector<std::string>& clips,
                       AudioWorkspaceHost& host);

} // namespace editor
} // namespace vv
