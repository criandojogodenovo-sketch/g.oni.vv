#!/usr/bin/env python3
"""build_c33.py — compila o c33_virtual no hospedeiro SEM cmake (o irmão do
build_tests.py; a lição do 585217f: o gate da casa é compilar os DOIS
alvos — o build_tests.py só cobre o test_core e o CI compila também o
c33_virtual). Uso: python3 scripts/build_c33.py [--run]
"""
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CML = os.path.join(REPO, "tests", "CMakeLists.txt")
OBJ = os.path.join(REPO, "tests", "build-obj-c33")
BIN = os.path.join(OBJ, "c33_virtual")


def src_list():
    # o alvo c33_virtual = ${GONI_CORE_SRCS} + c33_virtual.cpp — SÓ o core
    # (os test_*.cpp são da suíte; o c33 tem o main próprio)
    # parse POR LINHAS (o regex .*? morre no ')' de um comentário — o
    # ferido real desta build): recolhe até à linha que fecha o set
    out = []
    inside = False
    for line in open(CML):
        code = line.split("#", 1)[0].strip()
        if not inside:
            if code.startswith("set(GONI_CORE_SRCS"):
                inside = True
            continue
        if code == ")":
            break
        m2 = re.match(r"\$\{CORE_DIR\}/([\w/-]+\.cpp)$", code)
        if m2:
            out.append(os.path.join(REPO, "app/src/main/cpp", m2.group(1)))
    # o TU do c33
    out.append(os.path.join(REPO, "tests", "c33_virtual.cpp"))
    seen = set()
    uniq = []
    for p in out:
        rp = os.path.realpath(p)
        if rp not in seen:
            seen.add(rp)
            uniq.append(rp)
    return uniq


def needs_rebuild(src, obj):
    if not os.path.exists(obj):
        return True
    # as DEPENDÊNCIAS de header da camada que o TU inclui (o mtime do TU
    # só não chega — a lição do stale): qualquer .h do cpp/ mais novo
    cdir = os.path.join(REPO, "app/src/main/cpp")
    newest = os.path.getmtime(src)
    for root, _dirs, files in os.walk(cdir):
        for f in files:
            if f.endswith(".h"):
                m = os.path.getmtime(os.path.join(root, f))
                if m > newest:
                    newest = m
    return os.path.getmtime(obj) < newest


def main():
    os.makedirs(OBJ, exist_ok=True)
    srcs = src_list()
    objs = []
    jobs = []
    for s in srcs:
        rel = os.path.relpath(s, REPO).replace("/", "_")
        o = os.path.join(OBJ, rel + ".o")
        objs.append(o)
        if needs_rebuild(s, o):
            inc = ["-DFONT_FIXTURE=\"%s/tests/fixtures/fonts/LiberationSans-Regular.ttf\"" % REPO,
                   "-DFIXTURE_DIR=\"%s/tests/fixtures\"" % REPO,
                   "-I", os.path.join(REPO, "tests/stub"),
                   "-I", os.path.join(REPO, "app/src/main/cpp"),
                   "-I", os.path.join(REPO, "app/src/main/cpp/vendor"),
                   "-I", os.path.join(REPO, "app/src/main/cpp/vendor/etcpak"),
                   "-I", os.path.join(REPO, "app/src/main/cpp/vendor/astc-encoder"),
                   "-I", os.path.join(REPO, "tests")]
            flags = ["-std=c++17", "-O1", "-g0", "-w", "-fsigned-char",
                     "-msse4.1", "-pthread", "-c", s, "-o", o]
            jobs.append((s, inc + flags))
    fails = 0

    def comp(args):
        s, cmd = args
        r = subprocess.run(["g++-14"] + cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print("FAIL", os.path.basename(s))
            print(r.stderr[-2000:])
        return r.returncode == 0

    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        for ok in ex.map(comp, jobs):
            if not ok:
                fails += 1
    if fails:
        print(f"{fails} TU(s) falharam")
        return 1
    vendor_objs = []   # os vendors JÁ ESTÃO na lista do set (o parse por linhas)
    r = subprocess.run(["g++-14", "-std=c++17", "-o", BIN] + objs + vendor_objs +
                       ["-lz", "-pthread"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        print("LINK FAIL:")
        print(r.stderr[-2000:])
        return 1
    print("OK", BIN)
    if "--run" in sys.argv:
        os.chdir(REPO)
        r = subprocess.run([BIN], capture_output=True, text=True)
        tail = r.stdout.strip().splitlines()[-8:]
        print("\n".join(tail))
        return r.returncode
    return 0


if __name__ == "__main__":
    sys.exit(main())
