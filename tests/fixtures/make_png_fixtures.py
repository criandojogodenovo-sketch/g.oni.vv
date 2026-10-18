#!/usr/bin/env python3
# Gera fixtures PNG pequenos para os testes do CI (tests/fixtures/).
# Formatos cobertos: RGB 8-bit (16x16 vermelho) e RGBA 8-bit (8x8 com
# gradiente de alpha) — stb_image normaliza ambos para RGBA.
import struct, zlib, os

def chunk(kind: bytes, data: bytes) -> bytes:
    return (struct.pack(">I", len(data)) + kind + data +
            struct.pack(">I", zlib.crc32(kind + data) & 0xFFFFFFFF))

def png(width, height, color_type, raw_rows):
    ihdr = struct.pack(">IIBBBBB", width, height, 8, color_type, 0, 0, 0)
    idat = zlib.compress(b"".join(raw_rows), 9)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr) +
            chunk(b"IDAT", idat) + chunk(b"IEND", b""))

os.makedirs("tests/fixtures", exist_ok=True)

# 1) RGB 16x16 vermelho puro (color type 2)
rows = [b"\x00" + bytes([255, 0, 0] * 16) for _ in range(16)]
open("tests/fixtures/red16.png", "wb").write(png(16, 16, 2, rows))

# 2) RGBA 8x8 com gradiente de alpha (color type 6) — R=64 G=128 B=192
rows = []
for y in range(8):
    row = b"\x00"
    for x in range(8):
        row += bytes([64, 128, 192, (x + y) * 15 + 17])
    rows.append(row)
open("tests/fixtures/alpha8.png", "wb").write(png(8, 8, 6, rows))

# 3) F5.1-A: RGB 4096x4096 sólido verde (gate 4K com compressão — o PNG
# sólido deflate para alguns KB; decodifica para 64 MB de RGBA)
rows = [b"\x00" + bytes([20, 180, 60] * 4096) for _ in range(4096)]
open("tests/fixtures/green4096.png", "wb").write(png(4096, 4096, 2, rows))

# 4) F5.1-A: RGB 256x256 sólido azul (borda exata do gate <256)
rows = [b"\x00" + bytes([30, 40, 220] * 256) for _ in range(256)]
open("tests/fixtures/blue256.png", "wb").write(png(256, 256, 2, rows))

# 5) F5.1-B: RGBA 4x4 sólido amarelo p/ textura embutida base64 no glTF
rows = [b"\x00" + bytes([255, 220, 40, 255] * 4) for _ in range(4)]
open("tests/fixtures/yellow4.png", "wb").write(png(4, 4, 6, rows))

print("fixtures:",
      os.path.getsize("tests/fixtures/red16.png"), "B;",
      os.path.getsize("tests/fixtures/alpha8.png"), "B;",
      os.path.getsize("tests/fixtures/green4096.png"), "B;",
      os.path.getsize("tests/fixtures/blue256.png"), "B;",
      os.path.getsize("tests/fixtures/yellow4.png"), "B")
