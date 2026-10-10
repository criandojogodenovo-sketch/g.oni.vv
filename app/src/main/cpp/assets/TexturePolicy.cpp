// assets/TexturePolicy.cpp — as tabelas da política de texturas (PASSO 5A).
//
// A política vive AQUI (e só aqui): os perfis, a tabela de redução por
// resolução e os nomes exatos do log. O TexturePipeline consome; os testes
// aferem cada linha da tabela (tests/test_texture_profiles.cpp).
#include "assets/TexturePolicy.h"

namespace vv {

const char* texPerfilName(TexPerfil p) {
    switch (p) {
        case TexPerfil::Equilibrado: return "Equilibrado";
        case TexPerfil::Mobile:      return "Mobile";
        case TexPerfil::Qualidade:   break;
    }
    return "Qualidade";
}

TexPerfil texPerfilFromInt(int v) {
    if (v == 1) return TexPerfil::Equilibrado;
    if (v == 2) return TexPerfil::Mobile;
    return TexPerfil::Qualidade;   // default do projeto (e valor podre)
}

u32 texProfileCapMaxDim(u32 w, u32 h, TexPerfil p) {
    const u32 m = w > h ? w : h;
    switch (p) {
        case TexPerfil::Qualidade:
            return 0;   // SEM redução — a resolução original é o contrato
        case TexPerfil::Equilibrado:
            // 512/1024 mantém · 2048 avalia→1K · 4096→2K · 5120+→2K
            if (m <= 1024u) return 0;
            if (m <= 2048u) return 1024u;
            return 2048u;
        case TexPerfil::Mobile:
            // 512/1024 mantém · 2048→1K · 4096→1K · 5120+→1K (agressiva)
            if (m <= 1024u) return 0;
            return 1024u;
    }
    return 0;
}

} // namespace vv
