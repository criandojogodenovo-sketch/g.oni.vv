#pragma once
// ui/Strings.h — A TABELA LOCALIZADA (0.9.6.18 · HOTFIX D4): «Strings do
// Settings vêm da tabela localizada: "Definições" em PT, "Settings" só em
// locale EN — SEM if solto no draw.»
//
// O CONTRATO:
//   • o draw NUNCA compara locale (o «if solto» é proibido) — pede a chave
//     à tabela e desenha o que volta;
//   • a FONTE do locale no device é AConfiguration_getLanguage (main.cpp
//     no arranque — o MESMO ponto da densidade); no host/CI o default é
//     PT (a língua base do app — todo o vocabulário da casa é PT) e os
//     testes trocam com setLocaleForTest;
//   • a allowlist técnica de inglês ("Export", "Probe", "git", "fps"…)
//     continua válida — só as strings DESTA tabela localizam;
//   • o gate ui_vocab_check.py caça "Settings" hardcoded fora da tabela.
//
// A tabela é intencionalamente PEQUENA (o hotfix não traduz o app — só
// devolve as strings do Settings ao idioma da casa). Novas chaves entram
// quando o dono mandar localizar mais.

namespace vv {
namespace strings {

// as chaves (o conjunto do HOTFIX D4 — o título da página + o item do
// menu de ficheiro que a abre + o botão de ação da linha)
enum class Key {
    SettingsTitle,   // PT «Definições» · EN «Settings»
    OpenSettings,    // PT «Definições» · EN «Settings» (item do menu ⋯)
    ResetLayout,     // PT «Repor layout» · EN «Reset layout»
};

// a string da chave no locale corrente (nunca null, nunca vazia)
const char* tr(Key k);

// o locale corrente ("pt" | "en" — o código de 2 letras, minúsculas)
const char* locale();

// o device injeta AConfiguration_getLanguage AQUI (uma vez, no arranque;
// código de 2 letras — qualquer outra coisa cai no default PT)
void setLocale(const char* lang2);

// só para os testes/gates (troca e devolve o anterior)
const char* setLocaleForTest(const char* lang2);

} // namespace strings
} // namespace vv
