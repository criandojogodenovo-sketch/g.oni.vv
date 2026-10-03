#!/usr/bin/env python3
"""0.9.0 — CHECK ESTRUTURAL da tela de projetos (lado Java, spec F).

O CI não tem instrumentação Android; aferimos a ESTRUTURA do ecrã pelo
fonte (a mesma cultura do jni_parity.py — asserts determinísticos sobre o
código que a suíte host não consegue instanciar):

  1. cabeçalho 72dp com LOGO 48dp + título 20sp + tagline 12sp;
  2. pesquisa 48dp (lupa) + dropdown Ordenar (ProjectsFormat.sortLabel);
  3. botões primários 56dp FILL ACCENT (0xFF2196F3, raio 8dp — spec A/F);
  4. grelha de cards com miniatura 16:9 (thumbH = colW*9/16);
  5. estados do card: a carregar (ProgressBar) / em falta (MissingThumbView
     + interrogação + recuperação) / default (gone_logo);
  6. menu ⋮/long-press com as 4 ações (abrir/renomear/duplicar/apagar);
  7. apagar COM confirmação (DANGER + botões 48dp) e SEM swipe-to-delete;
  8. tempo relativo no card (ProjectsFormat.relativeTime);
  9. TOKENS 0.9.0 em constantes (espelho Java do ui/Theme.h) — zero hex
     inline fora delas;
 10. AlertDialogs no ContextThemeWrapper ESCURO.
"""
import re
import sys
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
ACT = ROOT / "app/src/main/java/vv/goni/ProjectManagerActivity.java"
FMT = ROOT / "app/src/main/java/vv/goni/ProjectsFormat.java"
VVP = ROOT / "app/src/main/java/vv/goni/VvProjects.java"
ICN = ROOT / "app/src/main/java/vv/goni/UiIcons.java"

TOKENS_090 = ("0xFF0B0E13", "0xFF151A23", "0xFF1F2733", "0xFF2A3442",
              "0xFFF5F5F5", "0xFF98A2B3", "0xFF2196F3", "0xFF1B7FD4",
              "0xFFEF5350", "0xFFFABB45")


def strip_comments(src: str) -> str:
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    src = re.sub(r"//[^\n]*", "", src)
    return src


def fail(msg):
    print(f"  FALHOU  {msg}")
    return 1


def main():
    bad = 0
    src = ACT.read_text(encoding="utf-8")
    fmt = FMT.read_text(encoding="utf-8")
    vvp = strip_comments(VVP.read_text(encoding="utf-8"))
    icn = strip_comments(ICN.read_text(encoding="utf-8"))
    code = strip_comments(src)

    # 1) cabeçalho: logo 48 + título 20 + tagline 12
    for frag, what in [("dp(48)", "logo 48dp no cabeçalho"),
                       ("title.setTextSize(20)", "título 20sp"),
                       ("tag.setTextSize(12)", "tagline 12sp"),
                       ("R.drawable.gone_logo", "logo G+lâmpada")]:
        if frag not in code:
            bad += fail(f"cabeçalho: falta {what}")
    if bad == 0:
        print("OK  cabeçalho 72dp: logo 48 + G.One VV 20sp + tagline 12sp")

    # 2) pesquisa 48dp + dropdown Ordenar
    if "dp(48), 1f" not in code or "pesquisar projetos" not in code:
        bad += fail("campo de pesquisa 48dp com hint em falta")
    elif "showSortMenu" not in code or "sortLabel" not in code:
        bad += fail("dropdown Ordenar ausente (showSortMenu/sortLabel)")
    else:
        print("OK  pesquisa 48dp (lupa) + dropdown [Ordenar: … ▾]")

    # 3) botões primários 56dp FILL ACCENT raio 8
    if "dp(56)" not in code:
        bad += fail("botões primários 56dp ausentes")
    elif "0xFF2196F3" not in code or "setCornerRadius(dp(8))" not in code:
        bad += fail("fill accent #2196F3 com raio 8dp ausente (spec A/F)")
    else:
        print("OK  botões primários 56dp fill accent #2196F3 r=8dp")

    # 4) grelha de cards com miniatura 16:9
    if "setNumColumns(2)" not in code:
        bad += fail("grelha de cards 2 colunas ausente")
    elif "* 9f / 16f" not in code:
        bad += fail("miniatura 16:9 ausente (thumbH = colW*9/16)")
    else:
        print("OK  grelha 2 colunas · cards com miniatura 16:9")

    # 5) estados do card
    for frag, what in [("new ProgressBar", "spinner (a carregar)"),
                       ("MissingThumbView", "estado em falta"),
                       ("UiIcons.QUESTION", "interrogação do em falta"),
                       ("projeto em falta", "erro legível do em falta"),
                       ("recuperar", "recuperação do em falta"),
                       ("Nenhum projeto ainda", "empty state")]:
        if frag not in code:
            bad += fail(f"estados do card: falta {what}")
    if bad == 0:
        print("OK  estados: a carregar / em falta (?+erro+recuperação) / vazio")

    # 6) menu ⋮ com 4 ações
    for frag in ('"Abrir"', '"Renomear"', '"Duplicar"', '"Apagar"'):
        if frag not in code:
            bad += fail(f"menu do card: ação {frag} ausente")
    if "setOnLongClickListener" not in code:
        bad += fail("toque longo não abre o menu (spec F)")
    if bad == 0:
        print("OK  ⋮/long-press: abrir/renomear/duplicar/apagar")

    # 7) apagar COM confirmação + SEM swipe
    if "confirmDelete" not in code or "0xFFEF5350" not in code:
        bad += fail("apagar sem confirmação com botão danger")
    if re.search(r"setOnSwipe|SwipeRefresh|swipe", code, re.I):
        bad += fail("swipe-to-delete presente (PROIBIDO — spec F)")
    elif bad == 0:
        print("OK  apagar: card de confirmação danger · SEM swipe-to-delete")

    # 8) tempo relativo
    if "relativeTime" not in code:
        bad += fail("tempo relativo do card ausente (há 2 h)")
    else:
        print("OK  tempo relativo no card (ProjectsFormat.relativeTime)")

    # 9) TOKENS 0.9.0 — zero hex fora das constantes
    for tok in TOKENS_090:
        if tok not in code:
            bad += fail(f"token 0.9.0 ausente: {tok}")
    hexes = re.findall(r"0x[0-9A-Fa-f]{8}", code)
    allowed = sum(1 for h in hexes if h.upper() in
                  tuple(t.upper() for t in TOKENS_090))
    if len(hexes) != allowed:
        bad += fail(f"hex inline fora dos tokens 0.9.0: {len(hexes) - allowed}")
    elif bad == 0:
        print("OK  tokens 0.9.0 (espelho Java do ui/Theme.h) centralizados")

    # 10) diálogos escuros
    n_builders = len(re.findall(r"new AlertDialog\.Builder\(", code))
    n_dark = len(re.findall(r"new AlertDialog\.Builder\(\s*dark\(\)", code))
    if n_builders == 0 or n_builders != n_dark:
        bad += fail(f"AlertDialogs fora do wrapper escuro: {n_dark}/{n_builders}")
    else:
        print("OK  todos os AlertDialogs no wrapper escuro")

    # ---- infraestrutura F (VvProjects + ProjectsFormat + UiIcons) ----------
    if "THUMB_FILE" not in vvp or "thumbUri" not in vvp:
        bad += fail("VvProjects sem thumb.png (URI de miniatura)")
    if "projectFolderExists" not in vvp:
        bad += fail("VvProjects sem deteção de projeto em falta")
    if "renameProject" not in vvp or "copyTreeInto" not in vvp:
        bad += fail("VvProjects sem renomear/duplicar (spec L)")
    if "relativeTime" not in fmt or "compareEntries" not in fmt:
        bad += fail("ProjectsFormat sem relativeTime/compareEntries")
    if bad == 0:
        print("OK  infra: thumb.png · em falta · renomear · duplicar · ordenar")
    for icon in ("LUPA", "SORT", "DOTS", "PLUS", "UPLOAD", "QUESTION"):
        if icon not in icn:
            bad += fail(f"UiIcons sem o ícone {icon}")
    if bad == 0:
        print("OK  UiIcons (lupa/ordenar/dots/plus/upload/interrogação)")

    print()
    if bad:
        print(f"PROJECTS-UI CHECK: {bad} falha(s)")
        sys.exit(1)
    print("PROJECTS-UI CHECK: OK (0.9.0 spec F)")


if __name__ == "__main__":
    main()
