# etcpak (vendored) — G.One VV F5.1-A

Fonte: https://github.com/wolfpld/etcpak (tag 2.1) — BSD-2 (LICENSE.txt intacto).
Autores: AUTHORS.txt intacto.

Ficheiros vendored (subconjunto necessário para ETC2 — sem app/CLI/DXT):
- ProcessRGB.cpp/.hpp  — codificadores ETC1/ETC2 RGB + ETC2 RGBA (EAC alpha)
- Tables.cpp/.hpp      — tabelas de quantização/modificadores
- Dither.cpp/.hpp      — dithering opcional (não usado pelo engine)
- ForceInline.hpp, Math.hpp, ProcessCommon.hpp, Vector.hpp (headers)

Integração (ver CMakeLists do app e dos testes):
- O SIMD é auto-detectado: NEON no arm64 (NDK) via `__ARM_NEON`;
  SSE4.1 no hospedeiro/CI x86_64 via flag `-msse4.1` só nestes TUs.
- `-w` suprime warnings upstream (shift-overflow benigno em
  compressBlockTH, conhecido do autor).
- Etc2Compressor (assets/Etc2Compressor.cpp) chama CompressEtc2Rgb /
  CompressEtc2Rgba por nível de mip com padding 4×4 por replicação de
  borda; entrada RGBA8 em memória (byte 0 = R) — formato nativo do engine.
- `heuristics=false` (usado nos testes) restringe a blocos
  individual/diff — determinístico e simples de verificar.
