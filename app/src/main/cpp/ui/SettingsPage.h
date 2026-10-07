#pragma once
// ui/SettingsPage.h — PÁGINA DE SETTINGS 0.9.0 (spec I):
//
//   ┌──────────────────────────────────────────────────────┐
//   │ [← back 56]  Settings  20sp                          │
//   │ (scroll)                                              │
//   │ ▾ GERAL          versão/build só-leitura · Repor      │
//   │                    layout · Imersivo (toggle)         │
//   │ ▾ ÁUDIO           volume geral (0..100)               │
//   │ ▾ PERMISSÕES      All Files (estado+botão) · Mic      │
//   │ ▾ DIAGNÓSTICO     Ver logs · Export logs · Probe      │
//   │                    áudio · dumps com badge ANTIGO     │
//   │ ▾ DOCS            (a linha entra na 0.9.2 — spec I)   │
//   │ ▾ SOBRE           sha256 da .so · licenças            │
//   └──────────────────────────────────────────────────────┘
//
// Linhas 48dp (spec A); secções colapsáveis 48dp com chevron (título 14sp);
// toggles PERSISTEM (layout.json p/ imersivo; settings.goni p/ áudio);
// BACK devolve ao editor com a seleção INTACTA (nada fecha a seleção).
#include "ui/EditorLayout.h"
#include "ui/Icons.h"
#include "ui/SafeArea.h"
#include "ui/ScrollMath.h"
#include "ui/Theme.h"
#include "core/Types.h"

#include <vector>
#include <string>

namespace vv {

class UiContext;
struct InputState;

namespace editor {

struct EditorState;

namespace settings {

// 0.9.6.18 (HOTFIX D4b): o rect do BOTÃO de uma actionRow — FONTE ÚNICA
// exportada (o draw, o re-despacho do scrollTap E os testes partilham-no).
// OUTLINE COMPACTO: largura = texto + padding (teto metade da linha),
// altura dos controlos de linha (28dp — a altura do toggle da casa),
// alinhado à direita. A assinatura pede o ui (mede o texto) e o texto
// (o walk tem de calcular o MESMO rect que o draw)
UiRect actionBtnRect(UiContext& ui, f32 x, f32 y, f32 w, const char* btn);

// ---- ids (faixa 5800..5899 — nova, sem colisões) ---------------------------
constexpr u64 kBackId     = 5800;   // ← voltar (56dp)
constexpr u64 kSectionBase = 5810;  // +bit da secção (colapsar)
constexpr u64 kResetLayoutId = 5820; // [Repor layout]
constexpr u64 kImmersiveId  = 5821;  // toggle Imersivo
constexpr u64 kVolumeId     = 5822;  // volume geral (slider 0..1)
constexpr u64 kAllFilesId   = 5823;  // All Files (estado + botão)
constexpr u64 kMicId        = 5824;  // Mic (estado + botão)
constexpr u64 kViewLogsId   = 5825;  // [Ver logs]
constexpr u64 kExportLogsId = 5826;  // [Export logs]
constexpr u64 kProbeId      = 5827;  // [Probe áudio]
constexpr u64 kDumpsId      = 5828;  // dumps com badge ANTIGO
constexpr u64 kTextWindowId = 5830;  // 0.9.1: abrir a janela de texto (IME)
                                     // (5829 é o literal da linha "reconverter"
                                     // — IDs únicos por frame, immediate-mode)
constexpr u64 kRunBenchId    = 5831;  // 0.9.6 (G6): benchmarks (R-017)
constexpr u64 kCopyBenchId   = 5832;  // 0.9.6 (G6): copiar o bloco de 9 linhas
constexpr u64 kLayoutExpId   = 5833;  // 0.9.6.5 (GRUPO B): exportar layout
constexpr u64 kLayoutAudId   = 5834;  // 0.9.6.5 (GRUPO B): auditoria do ecrã
constexpr u64 kScrollId     = 49;    // região de scroll da página

// ---- bits das secções (colapsáveis — PERSISTE via layout.json) --------------
constexpr u32 kBitGeral = 1u << 0;
constexpr u32 kBitAudio = 1u << 1;
constexpr u32 kBitPerm  = 1u << 2;
constexpr u32 kBitDiag  = 1u << 3;
constexpr u32 kBitDocs  = 1u << 4;
constexpr u32 kBitSobre = 1u << 5;

// ---- contexto (o main injeta; TUDO só-leitura exceto o que devolve) ---------
struct Ctx {
    const char* version = "";        // "0.9.0 (vc 43)"
    const char* git = "";            // PASSO 1: o commit curto (o Sobre;
                                     // vivia na status bar removida)
    const char* soSha = "";          // sha256 da .so (identidade 0.8.10)
    const char* storageMode = "";    // "saf" / "files"
    bool keepSource = true;
    f32  audioMaster = 1.0f;
    bool immersive = false;          // toggle Imersivo (JNI)
    bool allFilesGranted = false;
    bool micGranted = false;
    u32  dumpCount = 0;              // dumps (badge ANTIGO fica no viewer)
};

// ---- devoluções ---------------------------------------------------------------
enum Result {
    kNone = 0,
    kBackPressed,      // volta ao editor (seleção intacta)
    kResetLayout,      // Repor layout (defaults)
    kToggleImmersive,  // liga/desliga o imersivo
    kAllFilesPressed,  // abrir as definições de All Files
    kMicPressed,       // pedir permissão de mic
    kViewLogs,         // abrir o viewer de logs
    kExportLogs,       // export para Downloads
    kProbeAudio,       // diagnóstico de áudio
    kOpenTextWindow,   // 0.9.1: abrir a janela de texto (portrait + IME)
    kOpenDocs,         // 0.9.2: abrir o ecrã de Docs da V.ONI
    kToggleKeepSource, // fonte manter/largar
    kReconvert,        // reconverter assets
    kRunBench,         // 0.9.6 (G6): correr os benchmarks (R-017)
    kCopyBench,        // 0.9.6 (G6): copiar o relatório (bloco de 9 linhas)
    kExportLayout,     // 0.9.6.5 (GRUPO B): exportar o layout (PNG+JSON)
    kAuditScreen,      // 0.9.6.5 (GRUPO B): auditoria do ecrã (validador+log)
};

// desenha a PÁGINA inteira (full-screen na banda do viewport) e processa os
// toques; devolve a ação do frame. st.settingsMenu fecha no kBackPressed.
Result draw(UiContext& ui, const InputState& in, EditorState& st, const Ctx& ctx);

// alturas PURAS (fonte única — desenho e testes): secção 48 + linha 48
constexpr f32 kSectionH = 48.0f;
constexpr f32 kRowH = 48.0f;

} // namespace settings
} // namespace editor
} // namespace vv
