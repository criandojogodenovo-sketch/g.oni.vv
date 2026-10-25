#!/usr/bin/env python3
"""build_tests.py — compila o test_core da casa SEM cmake (a sandbox não tem).

Extrai a lista de fontes do tests/CMakeLists.txt (o alvo test_core) e
compila em paralelo com g++-14. Os objetos ficam em tests/build-obj/.
Uso: python3 scripts/build_tests.py [--link-only]
"""
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CML = os.path.join(REPO, "tests", "CMakeLists.txt")
OBJ = os.path.join(REPO, "tests", "build-obj")
OBJ = os.environ.get("GONI_OBJ_DIR", OBJ)  # F4-Z2: o dir do ASan separado
BIN = os.path.join(OBJ, "test_core")

def src_list():
    txt = open(CML).read()
    # do add_executable(test_core até ao PRÓXIMO alvo (o c33_virtual) —
    # o parêntese de fecho partilha linha com a última fonte)
    m = re.search(r"add_executable\(test_core(.*?)add_executable\(c33_virtual",
                  txt, re.S)
    body = "\n".join(l.split("#", 1)[0] for l in m.group(1).splitlines())
    # os ${CORE_DIR}/x.cpp e os *.cpp relativos a tests/
    out = []
    for tok in re.findall(r"\$\{CORE_DIR\}/([\w/-]+\.cpp)|([\w./-]+\.cpp)", body):
        if tok[0]:
            out.append(os.path.join(REPO, "app/src/main/cpp", tok[0]))
        else:
            out.append(os.path.join(REPO, "tests", tok[1]))
    # dedupe no CAMINHO REAL (o ../app/... relativo e o ${CORE_DIR}/...
    # absoluto são o MESMO ficheiro — o linker não perdoa duplicados)
    seen = set()
    uniq = []
    for p in out:
        rp = os.path.realpath(p)
        if rp not in seen:
            seen.add(rp)
            uniq.append(rp)
    return uniq

def compile_one(src):
    rel = src[len(REPO):].lstrip(os.sep).replace(os.sep, "_")
    obj = os.path.join(OBJ, rel + ".o")
    if os.path.exists(obj) and os.path.getmtime(obj) >= os.path.getmtime(src):
        return obj, None
    cmd = ["g++-14", "-std=c++17", "-O1", "-DNDEBUG",
           *os.environ.get("GONI_SAN", "").split(),  # F4-Z2: ASan/UBSan
           f"-DREPO_ROOT=\"{REPO}\"",
           f"-DFONT_FIXTURE=\"{REPO}/tests/fixtures/fonts/LiberationSans-Regular.ttf\"",
           f"-DFIXTURE_DIR=\"{REPO}/tests/fixtures\"",
           "-I" + os.path.join(REPO, "app/src/main/cpp"),
           "-I" + os.path.join(REPO, "tests/stub"),
           "-I" + os.path.join(REPO, "app/src/main/cpp/vendor/etcpak"),
           "-I" + os.path.join(REPO, "app/src/main/cpp/vendor/astc-encoder"),
           "-c", src, "-o", obj]
    os.makedirs(os.path.dirname(obj), exist_ok=True)
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        return obj, r.stderr[-4000:]
    return obj, None

def main():
    link_only = "--link-only" in sys.argv
    srcs = src_list()
    print(f"fontes: {len(srcs)}")
    os.makedirs(OBJ, exist_ok=True)
    fails = 0
    if not link_only:
        with ThreadPoolExecutor(max_workers=os.cpu_count()) as ex:
            for obj, err in ex.map(compile_one, srcs):
                if err:
                    fails += 1
                    print(f"FAIL {os.path.basename(obj)}:\n{err}")
                    if fails >= 3:
                        sys.exit(1)
        if fails:
            sys.exit(1)
    objs = []
    for s in srcs:
        rel = s[len(REPO):].lstrip(os.sep).replace(os.sep, "_")
        objs.append(os.path.join(OBJ, rel + ".o"))
    cmd = ["g++-14", "-std=c++17", "-O1", "-o", BIN] + objs \
        + ["-lz", "-lpthread"] \
        + os.environ.get("GONI_SAN", "").split()  # F4-Z2: runtime ASan/UBSan
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        print("LINK FAIL:\n" + r.stderr[-6000:])
        sys.exit(1)
    print(f"OK {BIN}")

if __name__ == "__main__":
    main()
