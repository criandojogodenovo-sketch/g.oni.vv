#!/usr/bin/env python3
"""ui_vocab_check.py — GATE DO VOCABULÁRIO DE UI (0.9.6.18 · HOTFIX D5+D10).

O dono fechou o princípio: «nenhum controlo é texto cru, nenhum glifo
clipa, nenhum duplicado sobrevive». Este gate caça no FONTE os
controlos-de-texto proibidos e o vocabulário estrangeiro à unidade da
engine (TIC) — o voltar-a-meter é VERMELHO (as mutações M5/M9):

  D5 · a 2.ª tab do Inspector morreu (duplicado da hierarquia) — os
       rótulos «Nós»/«Node»/«GameObject»/«Entity» não podem voltar a
       existir como string de UI no fonte C++/Java;
  D10 · controlos de texto cru proibidos: o toggle «grelha»/«lista» é o
       par de ícones Grid/List; o estado de visibilidade é o par
       Eye/EyeOff com o rótulo «visível» ao lado (nunca «visível: sim»);
       o reset da linha Transform é o ícone Reset (nunca o «R» nu);
       «on/off» como string desenhada também é proibido.

A ALLOWLIST (explícita, o dono manda):
  * ui/Icons.cpp e UiIcons.java — a TABELA DE NOMES dos ícones
    (documentação do vocabulário; não desenha controlo nenhum);
  * teclado in-app (UiEditor) — as TECLAS J/K/L/M/N/O/P/Q/R são teclas;
  * strings de LOG (engine.log/logs do sistema) não são controlo de UI —
    o gate só caça strings nos ficheiros de DESENHO listados abaixo.
Os comentários do fonte podem DISCUTIR os padrões (a regra cita o que
proíbe) — o gate caça o fonte SEM comentários (o mesmo modelo do
projects_ui_check.py).

Uso:
  python3 scripts/ui_vocab_check.py            # a partir da raiz do repo
Exit 0 = limpo · Exit 1 = vocabulário proibido no fonte (lista ficheiro).
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent

# (ficheiro, [(padrão literal, motivo)])
# Os padrões são LITERAIS de string de UI procurados no fonte sem comentários.
TARGETS = [
    ("app/src/main/cpp/ui/EditorUi.cpp", [
        ('"Nós"', "D5: a 2.ª tab do inspector morreu (duplicado da hierarquia)"),
        ('"Node"', "D5: vocabulário estrangeiro — a unidade da engine é TIC"),
        ('"GameObject"', "D5: vocabulário estrangeiro — a unidade é TIC"),
        ('"Entity"', "D5: vocabulário estrangeiro — a unidade é TIC"),
        ('"grelha"', "D10: o toggle de vista é o par de ícones Grid/List"),
        ('"visível: ', "D10: o estado vive no par Eye/EyeOff com o rótulo «visível»"),
        ('"R"', "D10: o reset é o ícone Reset (seta circular) — o «R» nu morreu"),
        ('"on/off"', "D10: string de estado desenhada como controlo"),
    ]),
    ("app/src/main/cpp/ui/BottomPanel.cpp", [
        ('"Nós"', "D5"),
        ('"grelha"', "D10: o toggle de vista é o par de ícones Grid/List"),
        ('"lista"', "D10: o toggle de vista é o par de ícones Grid/List"),
        ('"visível: ', "D10"),
        ('"R"', "D10"),
        ('"on/off"', "D10"),
    ]),
    ("app/src/main/cpp/ui/SettingsPage.cpp", [
        ('"Settings"', "D4: o título vem da tabela localizada (ui/Strings.h)"),
        ('"Repor layout"', "D4: o botão vem da tabela localizada (ui/Strings.h)"),
    ]),
    ("app/src/main/cpp/ui/Toolbar.cpp", [
        # D6: a marca é o glifo da função única — o tile com letra única é
        # proibido (o wordmark «G.One» é texto legítimo da casa)
        ('"G"', "D6: tiles com letra única proibidos como marca — usa brand::drawIcon"),
    ]),
    ("app/src/main/cpp/ui/UiEditor.cpp", [
        ('"visível: ', "D10: o estado vive no par Eye/EyeOff (o editor de UI usa o mesmo vocabulário)"),
        ('"on/off"', "D10"),
    ]),
    ("app/src/main/java/vv/goni/ProjectManagerActivity.java", [
        ('R.drawable.gone_logo', "D6/D7: a marca desenha-se (UiIcons.drawBrand) e o ícone da app nunca é conteúdo de card"),
    ]),
]

# as exceções por ficheiro (allowlist explícita — cada uma documentada)
ALLOW = {
    # o teclado in-app do editor de UI: as TECLAS são teclas (D10 não proíbe
    # teclado; proíbe CONTROLO de texto cru)
    "app/src/main/cpp/ui/UiEditor.cpp": [
        '"R"',
    ],
}


def strip_comments(src: str) -> str:
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    src = re.sub(r"//[^\n]*", "", src)
    return src


def main() -> int:
    bad = 0
    for rel, patterns in TARGETS:
        f = ROOT / rel
        if not f.is_file():
            print(f"  FALHOU  {rel}: ficheiro ausente (o gate apodreceu)")
            return 1
        code = strip_comments(f.read_text(encoding="utf-8"))
        for pat, why in patterns:
            if pat in code:
                if pat in ALLOW.get(rel, []):
                    continue
                bad += 1
                print(f"  FALHOU  {rel}: {pat} — {why}")
    # (M12) a CAPTURA fora do hot path: em main.cpp o encodePngRgb vive
    # SÓ (1) no worker do thumb (assíncrono — D7) e (2) no export de layout
    # (diagnóstico PEDIDO — nunca corre no frame). Uma 3.ª chamada (ou o
    # encode fora do worker) = a captura voltou ao hot path do render.
    main_cpp = strip_comments(
        (ROOT / "app/src/main/cpp/platform/main.cpp").read_text(encoding="utf-8"))
    n_enc = main_cpp.count("encodePngRgb")
    worker_start = main_cpp.find("static void thumbJobWorker")
    worker_end = main_cpp.find("static void thumbJobReap")
    enc_in_worker = (worker_start >= 0 and worker_end > worker_start and
                     main_cpp[worker_start:worker_end].count("encodePngRgb") == 1)
    if n_enc != 2 or not enc_in_worker:
        bad += 1
        print(f"  FALHOU  platform/main.cpp: encodePngRgb ×{n_enc} fora do "
              f"padrão (1 no worker do thumb + 1 no export de layout) — a "
              f"captura no hot path do render é a mutação M12")

    if bad:
        print(f"UI-VOCAB CHECK: {bad} violação(ões) — o vocabulário do dono "
              f"voltou ao fonte (ver docs/RELATORIO-0.9.6.18-HOTFIX-12-DEFEITOS.md)")
        return 1
    print("UI-VOCAB CHECK: OK (D5/D10/D4/D6 — o vocabulário de UI está limpo)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
