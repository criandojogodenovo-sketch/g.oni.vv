#!/usr/bin/env python3
"""glslang_check.py — 0.12-Z2 F5: o GLSL ES dos shaders passa pelo
COMPILADOR DE REFERÊNCIA (glslangValidator), além do driver real
(llvmpipe) que o raster_z2 já prova.

O QUE É: extrai as strings brutas de GLSL (`constexpr char kXxxSrc[] =
R"( … )"`) dos ficheiros de render, infere o estágio pelo NOME (Vs→vert,
Fs→frag; conteúdo como reserva: gl_Position → vert) e corre o
glslangValidator por estágio. O driver compila e LINKA em runtime (a
prova do raster); o glslang valida o PERFIL ES 3.0 estático — apanha o
que o driver aceita por tolerância (extensões, precisões frouxas).

Uso:
  python3 scripts/glslang_check.py                # glslangValidator no PATH
  GLSLANG=/caminho/glslangValidator python3 …     # binário explícito
Exit 0 = todos os shaders válidos · Exit 1 = violação (ficheiro:estágio).
"""
import os
import re
import subprocess
import sys
import tempfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(REPO, "app", "src", "main", "cpp", "render")

# os ficheiros com shaders (o inventário do CODEMAP; novo shader = entra
# AQUI — o script avisa se um ficheiro de render com R"( ficar de fora)
FILES = [
    "Renderer.cpp",
    "Renderer.h",
    "SkyRenderer.h",
    "SkyRenderer.cpp",
    "Material.cpp",
    "Grid.cpp",
    "ParticleShader.h",
]

RAW_RE = re.compile(r'constexpr char (\w+)\[\] = R"\((.*?)\)"', re.S)
# o padrão de LITERAIS CONCATENADOS (ParticleShader.h: inline constexpr
# const char* kVertexSrc = "…\n" "…\n";) — o corpo é a junção dos literais
CONCAT_RE = re.compile(
    r'(?:inline\s+)?(?:static\s+)?(?:constexpr\s+)?const\s+char\s*\*\s*'
    r'(\w+)\s*=\s*'
    r'((?:"(?:[^"\\]|\\.)*"(?:\s*//[^\n]*)*\s*)+);', re.S)
_LIT_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')


def concat_body(lits: str) -> str:
    out = []
    for m in _LIT_RE.finditer(lits):
        s = m.group(1)
        s = s.replace("\\n", "\n").replace("\\t", "\t").replace('\\"', '"')
        s = s.replace("\\\\", "\\")
        out.append(s)
    return "".join(out)


def stage_of(name: str, body: str) -> str:
    n = name.lower()
    if "vs" in n or "vert" in n:
        return "vert"
    if "fs" in n or "frag" in n:
        return "frag"
    return "vert" if "gl_Position" in body else "frag"


def main() -> int:
    exe = os.environ.get("GLSLANG", "glslangValidator")
    if not os.path.isfile(exe):
        from shutil import which
        exe = which("glslangValidator") or ""
    if not exe:
        print("GLSLANG-CHECK: glslangValidator não encontrado "
              "(GLSLANG=/caminho ou no PATH) — NÃO VERIFICADO")
        return 1
    total = 0
    bad = 0
    seen_files = set()
    for fname in FILES:
        path = os.path.join(SRC, fname)
        if not os.path.isfile(path):
            continue
        src = open(path, encoding="utf-8", errors="replace").read()
        found = [(m.group(1), m.group(2)) for m in RAW_RE.finditer(src)]
        # os literais concatenados (kVertexSrc/kFragmentSrc do ParticleShader)
        for m in CONCAT_RE.finditer(src):
            name = m.group(1)
            if any(n == name for n, _ in found):
                continue
            found.append((name, concat_body(m.group(2))))
        if not found:
            continue
        seen_files.add(fname)
        for m in found:
            name, body = m
            stage = stage_of(name, body)
            total += 1
            with tempfile.NamedTemporaryFile(
                    "w", suffix=".glsl", delete=False) as f:
                f.write(body)
                tmp = f.name
            try:
                r = subprocess.run(
                    [exe, "-S", stage, tmp],
                    capture_output=True, text=True, timeout=30)
            finally:
                os.unlink(tmp)
            if r.returncode != 0:
                bad += 1
                first = (r.stdout or r.stderr or "").strip().splitlines()
                print(f"  FALHOU  {fname}:{name} [{stage}]: "
                      + (first[0] if first else "sem saída"))
            else:
                print(f"  ok      {fname}:{name} [{stage}]")
    # a apodrecência: algum ficheiro de render com shader fora do inventário
    for fname in sorted(os.listdir(SRC)):
        if fname.endswith((".cpp", ".h")) and fname not in FILES:
            txt = open(os.path.join(SRC, fname), encoding="utf-8",
                       errors="replace").read()
            if re.search(r'constexpr char \w+\[\] = R"\(', txt):
                bad += 1
                print(f"  FALHOU  render/{fname} tem shader fora do "
                      f"inventário do script (o gate apodreceu)")
    print(f"GLSLANG-CHECK: {total} shader(s), {bad} violação(ões) "
          f"({os.path.basename(exe)})")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
