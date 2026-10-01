#!/usr/bin/env python3
"""0.7.6 — CHECK ESTRUTURAL da tela de projetos (lado Java).

O CI não tem instrumentação Android; aferimos a ESTRUTURA do ecrã pelo
fonte (a mesma cultura do jni_parity.py — asserts determinísticos sobre
o código que a suíte host não consegue instanciar):

  1. UMA entrada de criação: exatamente UM call-site de askNewProject();
  2. sem botão extra no fundo (o "root.addView(add" antigo morreu);
  3. a lista mostra nome + data da última edição (ProjectsFormat.dateLabel)
     e NÃO o URI cru (shortUri não constrói linhas da lista);
  4. o texto visível nunca contém o prefixo interno "primary:";
  5. os botões do topo usam o CONTORNO de marca #8AB4F8 (setStroke) sobre
     fundo escuro — nada do gradiente cinza do tema do sistema;
  6. o rótulo da pasta nos diálogos passa por ProjectsFormat.folderLabel.
"""
import re
import sys
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
ACT = ROOT / "app/src/main/java/vv/goni/ProjectManagerActivity.java"
FMT = ROOT / "app/src/main/java/vv/goni/ProjectsFormat.java"


def strip_comments(src: str) -> str:
    """código SEM comentários (/*…*/ e //…) — os checks de LITERAIS só
    devem olhar para código; a documentação pode citar 'primary:'."""
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
    code = strip_comments(src)

    # 1) UMA entrada de criação (call-sites — a definição não conta)
    n = len(re.findall(r"(?<!void )askNewProject\(\)", code))
    if n != 1:
        bad += fail(f"call-sites de askNewProject(): {n} (quer 1 — criação única)")
    else:
        print("OK  criação: uma única entrada ([Novo projeto])")

    # 2) sem botão extra no fundo
    if re.search(r"root\.addView\(add\b", code) or 'add.setText("+ Novo projeto")' in code:
        bad += fail("botão de criação duplicado no fundo (o antigo '+ Novo projeto')")
    else:
        print("OK  sem botão extra no fundo da tela")

    # 3) lista = nome + data (sem URI cru)
    if "shortUri" in code:
        bad += fail("shortUri ainda existe — a lista mostrava o URI com 'primary:'")
    else:
        print("OK  sem shortUri (o URI cru não constrói linhas)")
    if "ProjectsFormat.dateLabel" not in code:
        bad += fail("a linha da lista não usa ProjectsFormat.dateLabel (nome + data)")
    else:
        print("OK  lista usa nome + data da última edição (ProjectsFormat.dateLabel)")

    # 4) nenhum literal de UI com o prefixo interno (SÓ código — docs citam)
    if re.search(r"['\"]primary:", code):
        bad += fail("literal 'primary:' no texto visível")
    else:
        print("OK  nenhum literal 'primary:' no texto visível")

    # 5) botões de contorno de marca
    if "0xFF8AB4F8" not in code or "setStroke" not in code:
        bad += fail("botões sem contorno de marca #8AB4F8 (setStroke ausente)")
    elif re.search(r"\.setBackground\(null\)", code):
        bad += fail("botão com background do sistema (gradiente cinza)")
    else:
        print("OK  botões com contorno #8AB4F8 (setStroke) sobre fundo escuro")

    # 6) diálogos referem a pasta por folderLabel (o limpador do prefixo)
    if "ProjectsFormat.folderLabel" not in code:
        bad += fail("o rodapé do diálogo não passa por folderLabel")
    else:
        print("OK  pasta nos diálogos via ProjectsFormat.folderLabel")

    # fonte do folderLabel: as regras do prefixo existem e são testadas
    if "indexOf(':')" not in strip_comments(fmt):
        bad += fail("ProjectsFormat.folderLabel não trata o prefixo interno")
    else:
        print("OK  folderLabel trata o prefixo interno de armazenamento")

    print("PROJECTS UI CHECK:", "OK" if bad == 0 else f"{bad} falha(s)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
