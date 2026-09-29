# astc-encoder (vendored) — G.One VV F5.1-A

Fonte: https://github.com/ARM-software/astc-encoder (tag 5.3.0) — Apache-2.0
(LICENSE.txt intacto, cabeçalhos SPDX preservados).

Ficheiros vendored: os 23 TUs da biblioteca estática (ASTCENC_SRC do
CMakeLists upstream) + headers — sem CLI, testes, fuzzers ou third-party.

Integração (ver CMakeLists do app e dos testes):
- SIMD auto-detectado pelos headers (astcenc_mathlib.h): NEON no arm64
  (NDK), SSE2 no hospedeiro/CI x86_64 — nenhum define necessário.
- `-fsigned-char` nestes TUs (recomendação do build upstream para ARM).
- `-w` suprime warnings upstream.
- AstcCompressor (assets/AstcCompressor.cpp): perfil ASTCENC_PRF_LDR
  (linear — igual ao caminho RGBA8 não-comprimido no shader), qualidade
  "fast" (10.0), 1 thread, bloco 4x4/6x6, cadeia de mips completa em CPU.
- Custos: ~26 ficheiros C++ ao build (~40-60 s extra no CI) e ~0.5 MB no
  libgoni_vv.so — dentro do critério "APK cresce < ~1.5 MB".
