#!/usr/bin/env python3
"""0.9.3 — CHECK ESTRUTURAL DO FIX REG-001/R-005 (hotfix
ConcurrentModification no ProjectManagerActivity.reload()).

O CI não tem instrumentação Android (a tela não instancia no host); o que
afervamos é a ESTRUTURA DO CÓDIGO REAL — a mesma cultura do
projects_ui_check.py/jni_parity.py: asserts determinísticos sobre o fonte
que a suíte JVM não consegue ver. Se alguém reverter o fix (voltar ao
padrão antigo — a main thread a mexer em `all` enquanto a io itera), ESTE
check fica VERMELHO (é um dos ganchos da prova de mutação).

AS REGRAS (todas obrigatórias):
  1. reload() só pede: delega no ReloadGate (nunca mexe em `all` direto);
  2. CONFINAMENTO: a lambda do io.execute NUNCA referencia all/shown/
     missingUris (a causa raiz do crash era exatamente isso — iterar `all`
     na thread de fundo enquanto a main a limpava);
  3. a publicação (all.clear+addAll) acontece DENTRO de main.post (main
     thread) e substitui de uma vez;
  4. o loop do estado-em-falta itera o SNAPSHOT local (`fresh`) com
     try/catch POR ENTRADA (projeto corrompido/inacessível = saltado);
  5. o load (VvProjects.load) está dentro de try/catch (corrompido →
     lista vazia + log, nunca crash);
  6. Log.i com contadores no fim do reload (ok/em-falta);
  7. o portão ReloadGate existe, é puro (zero imports de Android) e tem a
     semântica single-flight + trailing (compareAndSet + queued) com
     abertura de emergência (abandon/safeRun);
  8. THUMBS mantém o teto (LruCache 24 — Problema 3);
  9. a memória do arranque é logada (Debug.getMemoryInfo — Problema 3)
     no Gestor E no editor (VvActivity).
"""
import re
import sys
import pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
ACT = ROOT / "app/src/main/java/vv/goni/ProjectManagerActivity.java"
VVA = ROOT / "app/src/main/java/vv/goni/VvActivity.java"
GATE = ROOT / "app/src/main/java/vv/goni/ReloadGate.java"


def strip_comments(src: str) -> str:
    src = re.sub(r"/\*.*?\*/", "", src, flags=re.S)
    src = re.sub(r"//[^\n]*", "", src)
    return src


def fail(msg):
    print(f"  FALHOU  {msg}")
    return 1


def method_span(code: str, header: str):
    """devolve (ini, fim) do corpo do método cuja assinatura contém header"""
    i = code.find(header)
    if i < 0:
        return None
    j = code.find("{", i)
    depth = 0
    k = j
    while k < len(code):
        if code[k] == "{":
            depth += 1
        elif code[k] == "}":
            depth -= 1
            if depth == 0:
                return (j, k)
        k += 1
    return None


def lambda_span(code: str, opener: str):
    """devolve (ini, fim) do bloco {..} que segue o opener (ex.: io.execute)"""
    i = code.find(opener)
    if i < 0:
        return None
    j = code.find("{", i)
    depth = 0
    k = j
    while k < len(code):
        if code[k] == "{":
            depth += 1
        elif code[k] == "}":
            depth -= 1
            if depth == 0:
                return (j, k)
        k += 1
    return None


def main():
    bad = 0
    src = ACT.read_text(encoding="utf-8")
    vva = strip_comments(VVA.read_text(encoding="utf-8"))
    gate = strip_comments(GATE.read_text(encoding="utf-8"))
    code = strip_comments(src)

    if not GATE.is_file():
        print("  FALHOU  ReloadGate.java ausente (o portão do REG-001)")
        return 1

    # ---- 1) reload() delega no portão ------------------------------------
    m = method_span(code, "private void reload()")
    if m is None:
        return fail("reload() não encontrado")
    body = code[m[0]:m[1]]
    if "reloadGate.request()" not in body:
        bad = fail("R-005.1: reload() tem de delegar no ReloadGate.request()")
    for forbidden in ("all.clear()", "all.addAll", "shown.clear()",
                      "missingUris.clear()"):
        if forbidden in body:
            bad = fail(f"R-005.1: reload() mexe na lista da UI direto "
                       f"({forbidden}) — só o portão + doReload")

    # ---- 2) CONFINAMENTO: a lambda io.execute NUNCA toca nas listas UI ----
    m = method_span(code, "private void doReload()")
    if m is None:
        return fail("doReload() não encontrado (o esqueleto do reload)")
    do_reload = code[m[0]:m[1]]
    lspan = lambda_span(do_reload, "io.execute(() -> {")
    if lspan is None:
        return fail("io.execute(() -> { não encontrado no doReload()")
    io_lambda = do_reload[lspan[0]:lspan[1]]
    # os blocos main.post ANINHADOS são a publicação na MAIN thread (legí-
    # timos); a regra do confinamento aplica-se ao RESTO da lambda io
    somente_io = io_lambda
    while True:
        pspan = lambda_span(somente_io, "main.post(() -> {")
        if pspan is None:
            break
        somente_io = (somente_io[:pspan[0]]
                      + "/* main.post */" + somente_io[pspan[1] + 1:])
    for ident in ("all.", "shown.", "missingUris.", "adapter."):
        if ident in somente_io:
            bad = fail(f"R-005.2: a lambda do io.execute referencia '{ident}' "
                       f"fora do main.post — a thread de fundo NUNCA toca "
                       f"nas listas da UI (era exatamente o mecanismo do "
                       f"crash)")
    # o snapshot local é o que itera
    if "for (VvProjects.Entry e : fresh)" not in somente_io:
        bad = fail("R-005.2: o estado-em-falta itera o SNAPSHOT local (fresh)")

    # ---- 3) a publicação DENTRO de main.post ------------------------------
    pub1 = lambda_span(io_lambda, "main.post(() -> {")
    if pub1 is None:
        return fail("main.post(() -> { não encontrado no doReload()")
    pub1_body = io_lambda[pub1[0]:pub1[1]]
    if "all.clear();" not in pub1_body or "all.addAll(fresh)" not in pub1_body:
        bad = fail("R-005.3: a 1ª publicação (main.post) substitui `all` "
                   "de uma vez (clear+addAll(fresh))")
    if "reloadGate.finished()" not in do_reload:
        bad = fail("R-005.3: o portão é libertado no fim da publicação "
                   "(reloadGate.finished())")
    if "reloadGate.abandon()" not in do_reload:
        bad = fail("R-005.3: a abertura de emergência (abandon) falta no "
                   "catch do agendamento")

    # ---- 4) try/catch POR ENTRADA no loop do estado-em-falta ---------------
    if "catch (Throwable t)" not in somente_io:
        bad = fail("R-005.4: cada entrada lida dentro de try/catch "
                   "(corrompido = saltado, nunca crash)")

    # ---- 5) o load dentro de try/catch ------------------------------------
    if "VvProjects.load(this)" not in somente_io:
        bad = fail("R-005.5: o load corre NA thread io (VvProjects.load)")
    parte_load = somente_io.split("VvProjects.load")[0]
    apos_load = somente_io[somente_io.find("VvProjects.load"):]
    if "try {" not in parte_load[-200:] or "catch (Throwable t)" not in apos_load[:600]:
        bad = fail("R-005.5: o load está protegido por try/catch")

    # ---- 6) contadores no log ----------------------------------------------
    if "reload ok" not in do_reload and "reload ok" not in code:
        bad = fail("R-005.6: Log.i com contadores (ok/em-falta) no fim")

    # ---- 7) o portão: semântica single-flight + trailing + emergência ------
    if "running.compareAndSet(false, true)" not in gate:
        bad = fail("R-005.7: ReloadGate sem compareAndSet single-flight")
    if "queued.set(true)" not in gate:
        bad = fail("R-005.7: ReloadGate sem coalesce trailing (queued)")
    if "queued.compareAndSet(true, false)" not in gate:
        bad = fail("R-005.7: ReloadGate sem despacho do trailing (finished)")
    if "abandon" not in gate or "safeRun" not in gate:
        bad = fail("R-005.7: ReloadGate sem abertura de emergência "
                   "(abandon/safeRun)")
    # PURO: zero Android
    gate_raw = GATE.read_text(encoding="utf-8")
    for imp in ("android.", "java.awt", "javax."):
        if imp in gate_raw.split("import")[1] if "import" in gate_raw else False:
            bad = fail(f"R-005.7: ReloadGate tem de ser Java puro (import {imp})")
    if re.search(r"^\s*import\s+", gate_raw, re.M):
        for linha in re.findall(r"^\s*import\s+(.+)$", gate_raw, re.M):
            if not linha.startswith("java.util.concurrent"):
                bad = fail(f"R-005.7: import não-puro no ReloadGate: {linha}")

    # ---- 8) teto das miniaturas mantido ------------------------------------
    if "new LruCache<>(24)" not in code:
        bad = fail("P3.2: o teto do THUMBS (LruCache 24) foi removido")

    # ---- 9) memória do arranque logada --------------------------------------
    if "logStartupMemory" not in code or "Debug.getMemoryInfo" not in code:
        bad = fail("P3.3: o Gestor não loga a memória do arranque")
    if "Debug.getMemoryInfo" not in vva:
        bad = fail("P3.3: o editor (VvActivity) não loga a memória do arranque")

    if bad == 0:
        print("reload_concurrency_check: OK — o fix R-005 está intacto "
              "(portão + confinamento + try/catch por entrada + contadores)")
    else:
        print("reload_concurrency_check: VERMELHO — o fix R-005 foi "
              "enfraquecido (ver acima)")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
