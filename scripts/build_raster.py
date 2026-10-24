#!/usr/bin/env python3
"""build_raster.py — 0.12-Z2 FASE 0: compila o RASTER HARNESS no hospedeiro
com GLES 3.0 REAL (Mesa/llvmpipe via EGL pbuffer — a prova do dono: pixels
lidos com glReadPixels, não contadores do stub).

O QUE MUDA vs build_c33.py:
  - os includes começam em tests/stub-real (o GLES3/gl3.h + EGL/egl.h REAIS
    de Khronos; o EGL/egl.h traz o shim eglCreateWindowSurface→pbuffer — o
    ÚNICO desvio do canónico, documentado no próprio header);
  - a ligação é -lEGL -lGLESv2 (as libs reais deste ambiente, Mesa 25.0.7)
    com rpath para o prefixo local dos debs extraídos;
  - o executável raster_z2 corre APENAS no hospedeiro do programador com
    Xvfb + DISPLAY (não entra no CI/ctest — o CI mantém os gates de sempre).

Uso: python3 scripts/build_raster.py [--run]
"""
import os
import re
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CML = os.path.join(REPO, "tests", "CMakeLists.txt")
OBJ = os.path.join(REPO, "tests", "build-obj-raster")
BIN = os.path.join(OBJ, "raster_z2")
MESA = "/home/z/mesa-debs/root/usr/lib/x86_64-linux-gnu"

# 0.12-Z2 F1 — o build TSAN (a prova de threads do dono: «Verificar com
# TSan no host»): os MESMOS TUs com -fsanitize=thread; os objetos saem em
# build-obj-raster-tsan (nunca misturados com os de sempre) e o binário
# liga com o runtime do TSan. Uso: python3 scripts/build_raster.py --tsan
TSAN = "--tsan" in sys.argv
if TSAN:
    OBJ = os.path.join(REPO, "tests", "build-obj-raster-tsan")
    BIN = os.path.join(OBJ, "raster_z2_tsan")


def src_list():
    # idêntico ao build_c33.py: o alvo c33_virtual = ${GONI_CORE_SRCS}
    # (a mesma árvore de fontes; o main.cpp do device entra via o TU do
    # harness, que o inclui — 1 TU, como o c33_virtual e o wiring087)
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
    out.append(os.path.join(REPO, "tests", "raster_z2.cpp"))
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
    cdir = os.path.join(REPO, "app/src/main/cpp")
    newest = os.path.getmtime(src)
    for root, _dirs, files in os.walk(cdir):
        for f in files:
            if f.endswith(".h") or f.endswith(".cpp"):
                m = os.path.getmtime(os.path.join(root, f))
                if m > newest:
                    newest = m
    # 0.12-Z2-F1BIS: o SHIM (tests/stub-real) também manda — a build perdia
    # mudanças do header de EGL (o binário velho corria com o shim velho)
    sdir = os.path.join(REPO, "tests/stub-real")
    for root, _dirs, files in os.walk(sdir):
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
            is_harness = s.endswith("raster_z2.cpp")
            inc = ["-DFONT_FIXTURE=\"%s/tests/fixtures/fonts/LiberationSans-Regular.ttf\"" % REPO,
                   "-DFIXTURE_DIR=\"%s/tests/fixtures\"" % REPO,
                   "-DREPO_ROOT=\"%s\"" % REPO,
                   "-I", os.path.join(REPO, "tests/stub-real"),
                   "-I", os.path.join(REPO, "tests/stub"),
                   "-I", os.path.join(REPO, "app/src/main/cpp"),
                   "-I", os.path.join(REPO, "app/src/main/cpp/vendor"),
                   "-I", os.path.join(REPO, "app/src/main/cpp/vendor/etcpak"),
                   "-I", os.path.join(REPO, "app/src/main/cpp/vendor/astc-encoder")]
            cmd = ["g++-14", "-std=c++17", "-O1", "-g", "-DNDEBUG",
                   "-fno-omit-frame-pointer"]
            if TSAN:
                cmd.append("-fsanitize=thread")
            if is_harness:
                cmd.append("-rdynamic")
            cmd += cmd_defines(s) + inc + ["-c", s, "-o", o]
            jobs.append((cmd, o, s))
    fails = []

    def run(job):
        cmd, o, s = job
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            fails.append((s, r.stderr[-4000:]))
        return o

    with ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        list(ex.map(run, jobs))
    if fails:
        for s, err in fails:
            print("FALHOU:", s, "\n", err, file=sys.stderr)
        sys.exit(1)

    link = ["g++-14", "-std=c++17", "-O1", "-g"]
    if TSAN:
        link.append("-fsanitize=thread")
    link += ["-o", BIN] + objs + [
        "-L" + MESA, "-Wl,-rpath," + MESA,
        "-lEGL", "-lGLESv2", "-lz", "-lpthread"]
    r = subprocess.run(link, capture_output=True, text=True)
    if r.returncode != 0:
        print("LINK FALHOU:\n", r.stderr[-6000:], file=sys.stderr)
        sys.exit(1)
    print("OK:", BIN)
    if "--run" in sys.argv:
        os.execv(BIN, [BIN])


def cmd_defines(src):
    # os defines por TU (o mesmo conjunto do c33; nada extra — o harness
    # lê o ambiente do runtime: DISPLAY/LIBGL_ALWAYS_SOFTWARE)
    return []


if __name__ == "__main__":
    main()
