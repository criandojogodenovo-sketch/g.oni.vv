# Rascunho — post técnico (r/cpp)

> Post TÉCNICO para r/cpp. Tom: engenheiro, detalhes concretos, zero
> marketing. O ângulo: "o que fazer C++17 SEM desktop e SEM IDE exige".
> Publicar da conta do dono quando decidir — este ficheiro é o rascunho.

**Título sugerido:** "I write C++17 on a phone: my CI is the only
compiler I have (Android game engine, 810 tests, a virtual-device
replay harness and mutation-proven regression sentinels)"

---

I build [G.One VV], a 3D game engine + editor for Android (arm64,
minSdk 24, C++17, ~104 translation units) and I do it entirely from a
phone — no desktop, no IDE, no local toolchain. I want to talk about
the engineering that makes that survivable, because it's mostly test
and CI architecture, not heroics.

**The setup.** A text editor on Android, git over HTTPS, and a 5-job
GitHub Actions workflow. Nothing compiles locally, ever. That means
the feedback loop is push → CI → logs. To make that loop mean
something I had to move about three habits left of where most C++
projects keep them:

1. **Every push runs the full triad.** ~810 unit tests (a ~100-line
   hand-rolled framework, no gtest — fewer deps, and I can read all of
   it), ~295 "virtual device" replay checks, and 6 static gates
   (parity of TUs between CMake targets, JNI table parity, glyph
   source checks for i18n, etc).
2. **The virtual device.** `c33_virtual` is a Linux binary that
   re-implements the ANativeActivity surface: it feeds real event
   sequences (touches, IME commits, rotation, lifecycle) into the same
   `platform/main.cpp` dispatch the phone uses, and asserts on drawn
   output and state. So "rotate the phone while the keyboard is open"
   is a CI test, not a hope.
3. **Mutation-proven sentinels.** Every regression that matters becomes
   an `R-NNN` test, and the discipline requires PASTING THE PROOF:
   revert the fix → N tests red → restore → green. A sentinel whose
   mutation doesn't go red is documentation, not a test. This caught
   real bugs — e.g. my first R-014 mutation attempt failed for the
   wrong reason (assertion ordering), which meant the test wasn't
   proving what I claimed. I redid it.

**Things that surprised me, in no order:**

- **Target-scoped CI builds cut flake exposure.** When GitHub runner
  shutdown-signal flakes started killing builds 14 times in a row
  (exit 143, zero code errors, the same code compiling fine in the
  sibling job), I didn't write retry folklore — I made each CI job
  compile only the target it tests. Half the build time per job, same
  coverage, less exposure to reclaim. Flakes dropped to noise.
- **A hand-written recursive-descent parser is fine, actually.** The
  V.ONI grammar is ~1 file of closed forms. The trick was moving ALL
  semantic knowledge (names, arity, docs, Python/JS equivalents,
  TAB-expand skeletons) into a **central registry** the parser
  validates against. Adding a language feature = 1 handler + 1 registry
  row; the parser doesn't change. There's a test that proves it by
  installing a fake component at runtime.
- **Immediate-mode UI without a debugger.** The editor UI is
  self-drawn C++ (no WebView/XML). When you can't break on a draw
  call, you want: input state as pure data you can snapshot, one draw
  path with deterministic ordering, and replay tests asserting on
  geometry. The hardest bugs (an inset-cropping issue that had list
  coordinates drawn in content-space inside a clip-only scissor since
  0.9.2) were found by asserting coordinates, not by looking.
- **The OOB bugs ASan catches that phones won't.** A `f32[3]` tint
  passed to a `f32[4]` API read garbage alpha for months. A `{4,3}`
  index array reading past the end "worked fine" on one GPU. Build
  with -fsanitize=address in CI on desktop before every release; it's
  the cheapest microscope I own.

**What I'd do differently:** honestly, vendor less. I hand-rolled the
test framework, the JSON parser, the text-shaper fallbacks, ZIP
central-directory parsing — each was right for "I can read everything
I ship", but the total surface is large for one person on a phone.
The counter-argument (and why I keep doing it): every line I can't
read is a line I can't fix from a 6.5" screen.

**Current gaps, so nobody has to point them out:** no shadows/PBR,
custom asset formats only, Android-only, no local ASan on the target
GPU, and the bench numbers from the real device are human-collected
(working on automating that next).

Repo link in my profile / happy to elaborate on any subsystem.
