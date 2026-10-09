#!/usr/bin/env python3
# scripts/gmesh_docs_check.py — 0.10-M (PASSO 1) — O GATE DOS DOCS DE
# FORMATO: docs/GMESH_formato.md e docs/GTEX_formato.md são a FONTE DE
# VERDADE dos formatos próprios; este gate afere que os docs e o CÓDIGO
# não divergem:
#   (1) os dois docs existem;
#   (2) cada símbolo citado entre backticks existe no fonte C++
#       (o contrato não documenta fantasmas — a regra do hierarchy-check
#       aplicada aos formatos);
#   (3) as ÂNCORAS numéricas/byte dos docs batem com o código (o header
#       de 32 bytes, a marca endian 0x1A2B, o teto 65535, os magics);
#   (4) as sentinelas do formato (R-038/R-039 quando existirem) vivem no
#       fonte — a lista SENTINELS cresce com os passos 2 e 3.
# UM desvio = CI VERMELHO = release bloqueada. Mudar o formato sem
# atualizar os docs (ou vice-versa) = vermelho.
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
CPP = REPO / "app" / "src" / "main" / "cpp"
DOCS = [REPO / "docs" / "GMESH_formato.md", REPO / "docs" / "GTEX_formato.md"]

# tokens entre backticks que NÃO são símbolos do fonte (GL/NDK API,
# vendors, plataformas, nomes de ficheiro, literais de formato) — a
# allowlist é EXPLícita: acrescentar aqui é declarar que o token é
# externo ao repo.
EXTERNAL = {
    "GMES", "GVTX", "GANM", "GTC",          # magics (verificados como ÂNCORAS)
    "u8", "u16", "u32", "u64", "f32", "i32",  # typedefs de core/Types.h
    "gltf_<hash16>.png", "cache_<hash16>_<fmt>.gtc",
    "assets/<stem>[_i].gtext", "assets/<stem>[_i].gmesh",
    "assets/<stem>.gm", "assets/<stem>.gmesh", "<nome>.gmesh",
    "textures/cache", "textures/gltf_<hash16>.png",
    "assets/", "source/", "textures/",
    "Xms", "N", "M",                                    # literais de log/erro
    "glCompressedTexImage2D", "glTexImage2D", "glGenerateMipmap",
    "GL_COMPRESSED_RGB8_ETC2", "GL_COMPRESSED_RGBA8_ETC2_EAC",
    "GL_COMPRESSED_RGBA_ASTC_4x4_KHR", "GL_COMPRESSED_RGBA_ASTC_6x6_KHR",
    "vendor/etcpak", "vendor/astc-encoder", "vendor/stb/stb_image.h",
    "stb_image", "etcpak", "astc-encoder",
    "glAstcSupported",                        # render/Texture.cpp (device-only)
    "arm64", "Linux", "CI", "RGBA", "RGBA8", "ETC2", "ASTC", "LDR",
    "sRGB", "PNG", "glTF", "GLB", "OBJ", "UTF-8", "ASCII", "LE",
    "FNV-1a", "float32", "std::vector<u16>", "std::string",
    "MeshData::indices", "MeshData::data", "CompressedImage::data",
    "readBytes", "engine.log", "AABB", "KHR",
}

# âncoras numéricas/byte: (doc-referência, padrão, ficheiro onde TEM de
# existir) — os números do formato não podem divergir do código.
ANCHORS = [
    ("header 32 B", r"kGHeaderBytes\s*=\s*32", "assets/GOwnFormats.h"),   # 0.10.2: a constante vive no header (a esqueleto v3 partilha)
    ("marca endian", r"0x1A2B", "assets/GOwnFormats.cpp"),
    ("teto vértices", r"65535", "assets/GOwnFormats.cpp"),
    ("FNV offset", r"1469598103934665603|0xcbf29ce484222325",
     "assets/GOwnFormats.cpp"),
    ("FNV primo", r"1099511628211|0x100000001b3", "assets/GOwnFormats.cpp"),
    ("leitor recusa versão", r"desconhecida \(o \.gmesh lê 1\.\.3\)",
     "assets/GOwnFormats.cpp"),
    ("escritor v3", r"kGmeshVersionWrite\s*=\s*3", "assets/GOwnFormats.h"),
    ("leitor mínimo", r"kGmeshVersionMinRead\s*=\s*1", "assets/GOwnFormats.h"),
    ("meta 160 B", r"kGmeshV3MetaBytes\s*=\s*160", "assets/GOwnFormats.h"),
    ("entrada 80 B", r"kGmeshV3BlockEntryBytes\s*=\s*80", "assets/GOwnFormats.h"),
    ("cap do bloco", r"kGmeshV3BlockVertexCap\s*=\s*65535", "assets/GOwnFormats.h"),
    ("gtext teto dims", r"16384", "assets/GOwnFormats.cpp"),
    ("gtext teto mips", r"nMips > 32", "assets/GOwnFormats.cpp"),
    ("gmesh teto grupos", r"nGroups > 4096", "assets/GOwnFormats.cpp"),
    ("chunk da cópia", r"kChunkBytes\s*=\s*6ull \* 1024 \* 1024",
     "assets/AssetConverter.h"),
    ("teto JSON", r"kMaxJsonBytes\s*=\s*16ull \* 1024 \* 1024",
     "assets/AssetConverter.h"),
    ("teto PNG", r"kMaxImageBytes\s*=\s*64ull \* 1024 \* 1024",
     "assets/AssetConverter.h"),
    ("gate 256 px", r"w >= 256u && h >= 256u", "assets/TextureCompressor.h"),
    ("gtc kDir", r"kDir", "assets/TextureCache.h"),
    # 0.10-M (PASSO 4): os orçamentos DECLARADOS da cache de blocos (o
    # MESMO 256 MB da casa — agora limitado por construção)
    ("orçamento RAM de blocos", r"kBlockCacheRamBudgetBytes\s*=\s*64ull \* 1024 \* 1024",
     "render/BlockMesh.h"),
    ("orçamento VRAM de blocos", r"kBlockCacheVramBudgetBytes\s*=\s*192ull \* 1024 \* 1024",
     "render/BlockMesh.h"),
]

# sentinelas por passo (cresce com os passos 2/3 — PASSO 1: só o formato
# de nome; R-038 entra no PASSO 2, R-039 no PASSO 3)
SENTINELS = []  # (nome, ficheiro) — preenchido nos passos seguintes


def fail(msg: str) -> None:
    print(f"::error::gmesh-docs-check: {msg}")
    print(f"GATE VERMELHO: {msg} — release BLOQUEADA")


def read_cpp(rel: str) -> str:
    p = CPP / rel
    return p.read_text(encoding="utf-8") if p.exists() else ""


def symbol_in_code(sym: str, corpus: str) -> bool:
    # Class::method → procura o method (a classe pode viver noutro TU);
    # senão procura o token inteiro como palavra.
    if "::" in sym and not sym.startswith("std::"):
        method = sym.split("::")[-1]
        return re.search(r"\b" + re.escape(method) + r"\b", corpus) is not None
    return re.search(r"\b" + re.escape(sym) + r"\b", corpus) is not None


def main() -> int:
    for d in DOCS:
        if not d.exists():
            fail(f"{d.name} ausente — a fonte de verdade do formato é "
                 "obrigatória (0.10-M PASSO 1)")
            return 1

    corpus = ""
    for p in sorted(CPP.rglob("*")):
        if p.suffix in (".h", ".cpp") and p.is_file():
            corpus += p.read_text(encoding="utf-8", errors="replace") + "\n"

    missing_syms, anchor_miss, sentinel_miss = [], [], []

    for d in DOCS:
        text = d.read_text(encoding="utf-8")
        for tok in re.findall(r"`([^`]+)`", text):
            for piece in tok.split():
                s = piece.strip("(){}[],.;:")
                if not s or s in EXTERNAL:
                    continue
                # só identificadores plausíveis (letras/dígitos/_/::)
                if not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_:]*", s):
                    continue
                if s.startswith(("GL_", "gl")) or "std::" in s:
                    continue
                if not symbol_in_code(s, corpus):
                    missing_syms.append(f"{d.name}: `{s}`")

    for label, pattern, rel in ANCHORS:
        if not re.search(pattern, read_cpp(rel)):
            anchor_miss.append(f"{label} ({pattern}) não encontrado em {rel}")

    for name, f in SENTINELS:
        if name not in (REPO / "tests" / f).read_text(encoding="utf-8"):
            sentinel_miss.append(f"{name} ausente em {f}")

    if missing_syms:
        fail("símbolos citados que NÃO existem no fonte:\n  " +
             "\n  ".join(sorted(set(missing_syms))))
        print()
    if anchor_miss:
        fail("âncoras do formato que NÃO batem com o código:\n  " +
             "\n  ".join(anchor_miss))
        print()
    if sentinel_miss:
        fail("sentinelas do formato apagadas:\n  " +
             "\n  ".join(sentinel_miss))
        return 1
    if missing_syms or anchor_miss:
        return 1

    print(f"gmesh-docs-check: VERDE — {len(DOCS)} docs, "
          f"símbolos e {len(ANCHORS)} âncoras batem com o código")
    return 0


if __name__ == "__main__":
    sys.exit(main())
