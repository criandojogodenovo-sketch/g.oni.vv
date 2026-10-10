#pragma once
// assets/TexturePolicy.h — POLÍTICA DE TEXTURAS DO PASSO 5A (0.10-M).
//
// DUAS OPERAÇÕES DISTINTAS, NUNCA CONFUNDIDAS (nem no código, nem no log):
//   1. REDUÇÃO (downscale) — quantos pixéis a textura tem. Política por
//      RESOLUÇÃO, escolhida pelo PERFIL do projeto; overridable POR ASSET.
//   2. COMPRESSÃO — como os pixéis são guardados (ASTC 4x4 padrão, ETC2
//      fallback, RGBA8 para os casos sem perda). Independente do perfil.
//
// Os TRÊS PERFIS (o dono escolhe no projeto — settings.goni `texPerfil=0/1/2`;
// o default é Qualidade = o comportamento pré-5A, nada muda sem decisão):
//   Qualidade   (0) — SEM redução alguma; ASTC 4x4 quando existe.
//   Equilibrado (1) — redução SELETIVA pela tabela (só acima de 1024).
//   Mobile      (2) — redução AGRESSIVA (tudo acima de 1024 → 1K).
//
// TABELA DE REDUÇÃO (classe = a MAIOR dimensão; default, overridable por
// asset — o override VENCE o perfil):
//   512/1024 → mantém (nos três perfis — sprites e detalhes não pagam preço)
//   2048     → avalia→1K nos perfis limitados (Equilibrado e Mobile → 1024)
//   4096     → 2K (Equilibrado) ou 1K (Mobile), conforme perfil
//   5120+    → conforme perfil (Equilibrado 2K, Mobile 1K)
// A redução halva SEMPRE do ORIGINAL decodificado (fator 2, média box 2×2 —
// em espaço LINEAR para texturas de cor: ver MipGen::MipSpace), nunca de um
// blob comprimido («nunca recomprimir de comprimido»).
//
// O OVERRIDE por asset vive em `textures/overrides.goni` (uma linha por
// asset, lida no arranque do projeto): `<stem> <maxDim>` — maxDim 0 = NUNCA
// reduzir este asset; N = teto da maior dimensão. O stem é o nome do .gtext
// sem diretoria/extensão ("cidade_3").
//
// GL-free / Android-free: testável no CI (tests/test_texture_profiles.cpp).
#include <string>
#include "core/Types.h"

namespace vv {

// ---- os três perfis ---------------------------------------------------------
enum class TexPerfil : u8 {
    Qualidade = 0,
    Equilibrado = 1,
    Mobile = 2,
};

// nome EXATO para o log («(perfil Equilibrado)») — nunca traduzido/abreviado
const char* texPerfilName(TexPerfil p);

// parse do settings.goni ("texPerfil=0/1/2"); fora do range → Qualidade
TexPerfil texPerfilFromInt(int v);

// ---- a tabela de redução (a política DEFAULT) --------------------------------
// Devolve o TETO da maior dimensão no perfil dado (0 = sem redução — mantém
// a resolução original). A decisão é PURA e determinística: mesmas dims +
// mesmo perfil → mesmo teto.
u32 texProfileCapMaxDim(u32 w, u32 h, TexPerfil p);

// ---- bits do campo `flags` (CompressedImage + .gtext v2 + .gtc eco) ----------
constexpr u8 kTexFlagSrgb   = 0x1;   // conteúdo de COR (sRGB); mips filtrados
                                     // em espaço linear — ver GTEX_formato §3
constexpr u8 kTexFlagNormal = 0x2;   // normal map: RGBA8 SEM perda de canais

// ---- o job de UMA textura (o que varia POR ASSET) ----------------------------
struct TexJob {
    // stem do .gtext destino ("cidade_3") — chave do override + contexto do
    // log. Vazio = sem override possível (loads soltos do runtime).
    const char* assetKey = "";
    // true = normal map (glTF material.normalTexture): RGBA8 sem perda de
    // canais, mips em espaço de bytes (dados LINEARES), flag kTexFlagNormal.
    bool normalMap = false;
};

} // namespace vv
