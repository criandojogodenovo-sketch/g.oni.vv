#!/usr/bin/env python3
# scripts/jni_parity.py — PARIDADE JNI (F5.4, TAREFA de CI do dono).
#
# A classe de bug que o dono pediu para nunca mais passar despercebida:
# um método nativo declarado na VvActivity.java SEM implementação
# resolvível no lado C++ = UnsatisfiedLinkError em runtime (RMX3624,
# 0.6.3). Este gate afere as 3 camadas:
#
#   1. VvActivity.java  — TODO `native` declarado tem de estar na tabela
#      RegisterNatives do StorageBridge.cpp com a MESMA assinatura JNI
#      (o RegisterNatives casa a string da assinatura inteira);
#   2. VvActivity.java  — TEM de conter o static { System.loadLibrary(...) }:
#      o android.app.NativeActivity carrega a lib com dlopen(RTLD_LOCAL)
#      cru (loadNativeCode_native), que NÃO corre o JNI_OnLoad e NÃO
#      coloca a lib no mapa de resolução do JVM — sem o loadLibrary no
#      Java, TODO método nativo morre com "No implementation found";
#   3. (opcional, CI release) dynsyms do .so real — cada native tem de
#      ter o símbolo Java_vv_goni_VvActivity_<nome> exportado
#      (uso: jni_parity.py <caminho-para-dynsyms.txt>).
#
# Saída limpa + exit 0 = paridade OK; exit 1 com a causa exata = regressão.
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
JAVA = os.path.join(ROOT, "app", "src", "main", "java", "vv", "goni", "VvActivity.java")
CPP = os.path.join(ROOT, "app", "src", "main", "cpp", "platform", "StorageBridge.cpp")

# tipo Java (na declaração do native) → descritor JNI
JNI_TYPES = {
    "void": "V",
    "boolean": "Z",
    "int": "I",
    "long": "J",
    "float": "F",
    "double": "D",
    "String": "Ljava/lang/String;",
    "Uri": "Landroid/net/Uri;",
    "VvActivity": "Lvv/goni/VvActivity;",
    "Intent": "Landroid/content/Intent;",
}


def java_type_to_jni(t: str) -> str:
    t = t.strip()
    if t in JNI_TYPES:
        return JNI_TYPES[t]
    sys.exit(f"ERRO: tipo Java sem mapeamento em jni_parity.py: '{t}' "
             f"(adicionar a JNI_TYPES se a VvActivity ganhar tipos novos)")


def parse_java_natives(src: str):
    """devolve {nome: assinatura} dos métodos `native` da VvActivity.java"""
    if "System.loadLibrary(" not in src:
        sys.exit("ERRO: VvActivity.java SEM System.loadLibrary — o dlopen do "
                 "NativeActivity não corre o JNI_OnLoad nem registra a lib no "
                 "JVM; TODO método nativo morreria com UnsatisfiedLinkError "
                 "(causa raiz do RMX3624 na 0.6.3).")
    if "static {" not in src:
        sys.exit("ERRO: VvActivity.java sem static initializer — o "
                 "System.loadLibrary tem de correr no static init (antes do "
                 "onCreate), senão a 1ª chamada nativa antecede o load.")
    natives = {}
    for m in re.finditer(r"\bnative\s+([\w.]+)\s+(\w+)\s*\(([^)]*)\)\s*;", src):
        ret, name, params = m.group(1), m.group(2), m.group(3).strip()
        sig = "("
        if params:
            for p in params.split(","):
                p = p.strip()
                if not p:
                    continue
                parts = p.split()
                if len(parts) < 2:
                    sys.exit(f"ERRO: parâmetro sem tipo+nome no native {name}: '{p}'")
                sig += java_type_to_jni(parts[-2])
        sig += ")" + java_type_to_jni(ret)
        natives[name] = sig
    if not natives:
        sys.exit("ERRO: nenhum método native encontrado na VvActivity.java "
                 "(a ponte Java↔native desapareceu?)")
    return natives


def strip_cpp_comments(src: str) -> str:
    """remove // e /* */ para o regex da tabela não partir com comentários
    inline (as entradas documentam as assinaturas — o parser tem de as
    ignorar e continuar a afervelar o código)"""
    src = re.sub(r"/\*.*?\*/", " ", src, flags=re.S)
    src = re.sub(r"//[^\n]*", " ", src)
    return src


def parse_cpp_table(src: str):
    """devolve {nome: assinatura} da tabela kNativeMethods do StorageBridge.cpp"""
    table = {}
    src = strip_cpp_comments(src)
    for m in re.finditer(
            r'\{\s*const_cast<char\*>\(\s*"(\w+)"\s*\)\s*,\s*'
            r'const_cast<char\*>\(\s*"([^"]+)"\s*\)', src):
        name, sig = m.group(1), m.group(2)
        if name in table:
            sys.exit(f"ERRO: native '{name}' duplicado na tabela RegisterNatives")
        table[name] = sig
    return table


def main():
    java_src = open(JAVA, encoding="utf-8").read()
    cpp_src = open(CPP, encoding="utf-8").read()

    java_natives = parse_java_natives(java_src)
    cpp_table = parse_cpp_table(cpp_src)

    errors = []

    # 1) TODO native da Java tem de estar na tabela, com a MESMA assinatura
    for name, sig in sorted(java_natives.items()):
        if name not in cpp_table:
            errors.append(f"native '{name}' da VvActivity.java AUSENTE na "
                          f"tabela RegisterNatives (UnsatisfiedLinkError garantido)")
        elif cpp_table[name] != sig:
            errors.append(f"native '{name}': assinatura divergente — "
                          f"Java='{sig}' tabela='{cpp_table[name]}'")

    # 2) entrada EXTRA na tabela sem declaração Java = NoSuchMethodError no
    #    RegisterNatives (falha o registo inteiro)
    for name in sorted(cpp_table):
        if name not in java_natives:
            errors.append(f"tabela RegisterNatives tem '{name}' que NÃO é "
                          f"declarado na VvActivity.java (NoSuchMethodError "
                          f"no RegisterNatives — registo inteiro falha)")

    # 3) símbolos exportados do .so real (quando o CI entrega o dynsyms)
    if len(sys.argv) > 1:
        with open(sys.argv[1], encoding="utf-8", errors="replace") as f:
            syms = f.read()
        for name in sorted(java_natives):
            sym = f"Java_vv_goni_VvActivity_{name}"
            if sym not in syms:
                errors.append(f"símbolo '{sym}' AUSENTE no .dynsym do .so "
                              f"(visibilidade/--gc-sections?)")

    if errors:
        print("PARIDADE JNI FALHOU:")
        for e in errors:
            print(f"  - {e}")
        sys.exit(1)

    print(f"PARIDADE JNI OK — {len(java_natives)} native(s) com nome+assinatura "
          f"casando Java↔RegisterNatives"
          + (f" e símbolos Java_vv_goni_VvActivity_* exportados no .so"
             if len(sys.argv) > 1 else ""))
    for name, sig in sorted(java_natives.items()):
        print(f"  {name} {sig}")


if __name__ == "__main__":
    main()
