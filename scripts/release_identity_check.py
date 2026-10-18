#!/usr/bin/env python3
# scripts/release_identity_check.py — GATE release-identity (R-015, FASE 0.9.6 G0).
#
# O CONTRATO (task 0.9.6, G0): em cada run de release, asserta que
#   nome do artefacto == versionName no build config == versão do RELATORIO
#   a fechar. Vermelho se divergir.
#
# A causa raiz vigiada (real, colada no REGRESSOES R-015): o run verde do
# fecho 0.9.5 publicou o APK como `goni-vv-0.9.4-release-signed` — o nome
# do artifact estava LITERAL no workflow (0.9.4) enquanto o build.gradle
# já dizia 0.9.5/48: identidade errada no ficheiro que o dono baixa,
# rastreabilidade de crash-dumps turva (o dump traz a versão da build,
# o artifact dizia outra) e o checklist VERIFIED do README não casava com
# a versão instalada.
#
# Modos:
#   (sem args)  — afere o REPO: build.gradle ↔ workflow ↔ RELATORIO.
#   --artifact-name N — afere também o NOME REAL do artifact publicado
#                 (o job verify-entry-symbols passa o diretório baixado).
# Sai 0 (verde) / 1 (vermelho, mensagem ::error para o CI).
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
GRADLE = REPO / "app" / "build.gradle"
WORKFLOW = REPO / ".github" / "workflows" / "release.yml"
DOCS = REPO / "docs"


def fail(msg: str) -> None:
    print(f"::error::release-identity (R-015): {msg}")
    sys.exit(1)


def ok(msg: str) -> None:
    print(f"GATE VERDE (release-identity/R-015): {msg}")


def version_tuple(v: str):
    try:
        return tuple(int(p) for p in v.split("."))
    except ValueError:
        return ()


def main() -> None:
    artifact_name = None
    args = sys.argv[1:]
    if args and args[0] == "--artifact-name":
        if len(args) < 2:
            fail("--artifact-name exige um valor")
        artifact_name = args[1]

    # ---- 1) versionName/versionCode do BUILD CONFIG ------------------------
    gradle = GRADLE.read_text(encoding="utf-8")
    m = re.search(r"versionCode\s+(\d+)", gradle)
    if not m:
        fail("versionCode não encontrado em app/build.gradle")
    vcode = m.group(1)
    m = re.search(r"versionName\s+'([0-9.]+)'", gradle)
    if not m:
        fail("versionName não encontrado em app/build.gradle")
    vname = m.group(1)

    # ---- 2) o nome do artifact no WORKFLOW é DINÂMICO e casa com VNAME ----
    # (linhas de comentário são IGNORADAS na deteção de literais — os
    # comentários documentam a história do bug (o literal 0.9.4) de propósito;
    # o que o gate aferiu sempre foi o `name:` REAL do upload-artifact)
    wf = WORKFLOW.read_text(encoding="utf-8")
    wf_code = "\n".join(
        l for l in wf.splitlines() if not l.lstrip().startswith("#"))
    # 2a. nenhum literal goni-vv-X.Y.Z-release com versão != VNAME (apanha
    #     a reintrodução do hardcode — a causa raiz do R-015)
    for lit in re.findall(r"goni-vv-([0-9]+(?:\.[0-9]+)*)-release", wf_code):
        if lit != vname:
            fail(f"nome de artifact LITERAL 'goni-vv-{lit}-release' no "
                 f"workflow != versionName '{vname}' do build.gradle "
                 f"(era exatamente assim que o 0.9.4-publicado-como-0.9.4 "
                 f"acontecia — R-015)")
    # 2b. o upload do APK usa ${{ env.VNAME }} (dinâmico, SEM versão escrita)
    up = re.search(
        r"name:\s*goni-vv-\$\{\{\s*env\.VNAME\s*\}\}-release", wf)
    if not up:
        fail("o passo Publicar artifact do APK não usa "
             "'goni-vv-${{ env.VNAME }}-release…' — o nome do artifact tem "
             "de vir do versionName do build (R-015)")
    # 2c. o VNAME tem de ser mesmo exportado para o env do job (senão o
    #     upload publica 'goni-vv--release' com a variável VAZIA)
    if 'echo "VNAME=${VNAME}" >> "$GITHUB_ENV"' not in wf:
        fail("o passo que escreve build_info.txt não exporta "
             "'VNAME=${VNAME}' para o GITHUB_ENV — o nome do artifact "
             "sairia vazio (R-015)")

    # ---- 3) o RELATORIO da versão a fechar EXISTE e declara o MESMO par --
    rel = DOCS / f"RELATORIO-{vname}.md"
    if not rel.is_file():
        fail(f"docs/RELATORIO-{vname}.md não existe — a release {vname} "
             f"não pode fechar sem o relatório da própria versão (R-015)")
    head = rel.read_text(encoding="utf-8")
    if not re.search(rf"versionCode\s*{vcode}\b", head):
        fail(f"docs/RELATORIO-{vname}.md não declara 'versionCode {vcode}' "
             f"— o relatório a fechar tem de casar com o build config "
             f"(R-015)")
    if vname not in head:
        fail(f"docs/RELATORIO-{vname}.md não menciona a versão {vname} "
             f"(R-015)")

    # ---- 4) a versão a fechar é a MAIS RECENTE documentada ----------------
    # (apanha o "reverter o bump": versionName 0.9.4 com RELATORIO-0.9.5 já
    #  commitado = estar a RELEASE-AR uma identidade mais velha que a última
    #  fase documentada — vermelho. Os RELATORIO-F*.md são legados pré-0.6.7
    #  e não contam para a ordenação.)
    numeric = []
    for f in DOCS.glob("RELATORIO-*.md"):
        v = f.stem[len("RELATORIO-"):]
        t = version_tuple(v)
        if t:
            numeric.append((t, v))
    if numeric:
        latest = max(numeric)[1]
        if latest != vname:
            fail(f"a versão a fechar ({vname}) não é a mais recente "
                 f"documentada (última: RELATORIO-{latest}.md) — reverter o "
                 f"bump ou release-ar por baixo do relatório vigente é "
                 f"exatamente o que o R-015 vigia")

    # ---- 5) modo --artifact-name: o NOME REAL publicado -------------------
    if artifact_name is not None:
        if not re.fullmatch(
                rf"goni-vv-{re.escape(vname)}-release(-signed|-UNSIGNED)",
                artifact_name):
            fail(f"o artifact publicado chama-se '{artifact_name}' — "
                 f"esperado goni-vv-{vname}-release(-signed|-UNSIGNED) "
                 f"(versionName do build.gradle: {vname}) (R-015)")

    ok(f"versionName {vname} · versionCode {vcode} · artifact "
       f"goni-vv-{vname}-release(-signed|-UNSIGNED) · "
       f"docs/RELATORIO-{vname}.md declara o par e é o mais recente"
       + (f" · artifact real: {artifact_name}" if artifact_name else ""))


if __name__ == "__main__":
    main()
