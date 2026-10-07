// ui/Strings.cpp — a implementação da tabela localizada (HOTFIX D4).
//
// PT é a língua base do app (todo o vocabulário da casa é PT; o default é
// PT mesmo sem locale conhecido — o host/CI vive aqui). EN existe porque o
// dono a pediu EXPLICITAMENTE: «"Settings" só em locale EN».
#include "ui/Strings.h"
#include <cstring>

namespace vv {
namespace strings {

namespace {

const char* g_lang = "pt";

struct Row {
    Key      key;
    const char* pt;
    const char* en;
};

// A TABELA (a fonte ÚNICA — nenhum draw tem literal próprio destas
// strings; o gate ui_vocab_check.py caça o literal fora daqui)
const Row kTable[] = {
    {Key::SettingsTitle, "Definições",  "Settings"},
    {Key::OpenSettings,  "Definições",  "Settings"},
    {Key::ResetLayout,   "Repor layout", "Reset layout"},
};

const char* pick(const Row& r) {
    if (std::strcmp(g_lang, "en") == 0) {
        return r.en;
    }
    return r.pt;   // PT é o default (locale desconhecido → a língua base)
}

} // namespace

const char* tr(Key k) {
    for (const Row& r : kTable) {
        if (r.key == k) {
            return pick(r);
        }
    }
    return "";   // chave desconhecida — nunca acontece (a tabela é fechada)
}

const char* locale() {
    return g_lang;
}

void setLocale(const char* lang2) {
    if (lang2 && std::strlen(lang2) >= 2) {
        // normaliza o código de 2 letras (AConfiguration devolve "pt"/"PT")
        char a = lang2[0] >= 'A' && lang2[0] <= 'Z'
                     ? static_cast<char>(lang2[0] - 'A' + 'a') : lang2[0];
        char b = lang2[1] >= 'A' && lang2[1] <= 'Z'
                     ? static_cast<char>(lang2[1] - 'A' + 'a') : lang2[1];
        static char buf[3] = {0, 0, 0};
        buf[0] = a;
        buf[1] = b;
        buf[2] = '\0';
        // só pt/en têm tabela — qualquer outra coisa volta ao default PT
        g_lang = (std::strcmp(buf, "en") == 0 || std::strcmp(buf, "pt") == 0)
                     ? buf : "pt";
    }
}

const char* setLocaleForTest(const char* lang2) {
    const char* prev = g_lang;
    setLocale(lang2);
    return prev;
}

} // namespace strings
} // namespace vv
