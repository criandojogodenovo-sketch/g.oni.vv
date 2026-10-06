#!/usr/bin/env python3
"""0.9.0 — CHECK ESTRUTURAL da tela de projetos (lado Java, spec F).

O CI não tem instrumentação Android; aferimos a ESTRUTURA do ecrã pelo
fonte (a mesma cultura do jni_parity.py — asserts determinísticos sobre o
código que a suíte host não consegue instanciar):

  1. FASE 9 (G1-4b): cabeçalho NUMA LINHA (buildTopBar) — logo+título,
     pesquisa flex, ordenar SÓ ÍCONE (texto no menu), Novo FILL +
     Importar CONTORNO, tudo 56/48dp;
  2. FASE 9 (G1-4a): o CAIXA do empty-state inteiro desliga com projetos
     (emptyBox.setVisibility — o bigLogo era o quadrado fantasma);
  3. FASE 9 (G1-4c): grelha ADAPTÁVEL (AUTO_FIT + columnWidth 180dp,
     mínimo 2) e miniatura 16:9 pela coluna REAL;
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

TOKENS_F = ("0xFF141414", "0xFF1E1E1E", "0xFF262626", "0xFF2E2E2E",
            "0xFFE6E6E6", "0xFFA6A6A6", "0xFFF5F5F5", "0xFFDADADA",
            "0xFF141414", "0xE01E1E1E", "0xEB262626",
            "0xFFEF5350", "0xFFFABB45")   # 0.9.6.9 spec F: mono+vidro


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

    # 1) FASE 9 (G1-4b): cabeçalho NUMA LINHA
    for frag, what in [("buildTopBar", "cabeçalho numa linha (buildTopBar)"),
                       ("R.drawable.gone_logo", "logo G com 4 setas"),
                       ("dp(32), dp(32)", "logo 32dp compacto"),
                       ("title.setTextSize(16)", "título 16sp"),
                       ("pesquisar projetos", "hint da pesquisa"),
                       ("setContentDescription(\"ordenar\")",
                        "ordenar como ícone (descrição de acesso)")]:
        if frag not in code:
            bad += fail(f"cabeçalho: falta {what}")
    if "buildHeader" in code or "buildSearchRow" in code or \
       "buildActions" in code:
        bad += fail("cabeçalho antigo em várias linhas ainda presente")
    if bad == 0:
        print("OK  cabeçalho NUMA LINHA: logo+título · pesquisa · "
              "ordenar-ícone · Novo/Importar")

    # 1b) o ⋮ decorativo do Ordenar REMOVIDO (só o SORT à esquerda)
    if re.search(r"sortBtn\.setCompoundDrawablesWithIntrinsicBounds\("
                 r"\s*UiIcons\.drawable\(UiIcons\.SORT[^)]*\), null,\s*"
                 r"\n?\s*UiIcons\.drawable\(UiIcons\.DOTS", code):
        bad += fail("⋮ decorativo ainda ao lado do Ordenar (removido na FASE 9)")
    else:
        print("OK  Ordenar: SÓ o ícone (o ⋮ decorativo foi removido)")

    # 2) FASE 9 (G1-4a): o quadrado fantasma — o CAIXA inteiro desliga
    if "emptyBox.setVisibility" not in code:
        bad += fail("emptyBox.setVisibility ausente (o logo fantasma ficava)")
    elif "private LinearLayout emptyBox" not in code:
        bad += fail("campo emptyBox ausente")
    else:
        print("OK  quadrado fantasma: o emptyBox INTEIRO (logo incluído) "
              "desliga com projetos")

    # 2b) botões: Novo FILL + Importar CONTORNO, 56dp
    if "dp(56)" not in code:
        bad += fail("botões 56dp ausentes")
    elif "ACCENT_PRESS, false" not in code or "0, 0, true" not in code:
        bad += fail("Novo FILL / Importar CONTORNO ausentes (G1-4b)")
    elif "outline ? ACCENT : ACCENT_INK" not in code:
        bad += fail("variante contorno do botão ausente (a tinta do fill é "
                    "ACCENT_INK desde o mono — branco sobre branco morreu)")
    else:
        print("OK  Novo projeto FILL mono · Importar projeto SÓ CONTORNO")

    # 3) FASE 9 (G1-4c): grelha adaptável
    if "GridView.AUTO_FIT" not in code or "setColumnWidth(dp(180))" not in code:
        bad += fail("grelha adaptável ausente (AUTO_FIT ÷ 180dp)")
    elif "getNumColumns() < 2" not in code:
        bad += fail("mínimo de 2 colunas ausente")
    elif "* 9f / 16f" not in code:
        bad += fail("miniatura 16:9 ausente (thumbH = colW*9/16)")
    elif "/ cols - dp(16)" not in code:
        bad += fail("largura do card pela coluna REAL ausente")
    else:
        print("OK  grelha ADAPTÁVEL (útil÷180dp, mín 2) · miniatura 16:9 "
              "pela coluna real")

    # 3b) rótulo do menu em minúscula
    if '"Última edição"' not in code:
        bad += fail('menu sem "Última edição" (minúscula — G1-4b)')
    else:
        print("OK  ordenar: \"Última edição\" em minúscula no menu")

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

    # 9) TOKENS spec F — zero hex fora das constantes
    for tok in TOKENS_F:
        if tok not in code:
            bad += fail(f"token spec F ausente: {tok}")
    hexes = re.findall(r"0x[0-9A-Fa-f]{8}", code)
    allowed = sum(1 for h in hexes if h.upper() in
                  tuple(t.upper() for t in TOKENS_F))
    if len(hexes) != allowed:
        bad += fail(f"hex inline fora dos tokens spec F: {len(hexes) - allowed}")
    elif bad == 0:
        print("OK  tokens spec F (mono+vidro — espelho Java do ui/Theme.h) "
              "centralizados")

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
    print("PROJECTS-UI CHECK: OK (0.9.6.9 spec F: mono+vidro)")


if __name__ == "__main__":
    main()
