# Rascunho — post de introdução (r/gameenginedevs)

> Post de APRESENTAÇÃO do projeto. Tom: engenheiro a falar com engenheiros.
> Público que conhece Unity/Godot e sabe o que custa um editor. Publicar
> com o APK/link do repo quando o dono decidir (o repo é público; o post
> é da conta do dono — este ficheiro é o rascunho).

**Título sugerido:** "I'm building a 3D game engine that only runs on a
$100 phone — and I write all the code on a phone too"

---

Hey folks. I've been building **G.One VV**, a 3D game engine + editor
for Android, and it's at a stage where the basics actually work, so I
wanted to share it and get feedback.

The constraints I gave myself, because they shaped everything:

- **Target device: a Realme C33** — Android 13, PowerVR GE8320, 4 GB RAM.
  60 fps is the goal, on THIS phone, not on a flagship.
- **All code is written on a phone.** No desktop, no IDE. I edit C++/Java
  in a text editor on Android and push over HTTPS.
- **The CI is the only compiler.** Nothing builds locally. Every push
  runs a 5-job GitHub Actions workflow: core tests on Linux, a "virtual
  device" replay harness, keystore, signed arm64 APK, and an
  entry-symbols gate. If CI is green, the owner installs that exact APK.

What's actually in it:

- Native Activity + OpenGL ES renderer (meshes, materials, textures,
  skeletal animation with blending, glTF import that streams so 500 MB
  archives don't blow up RAM)
- An **immediate-mode editor UI** written from scratch in C++17 — no
  WebView, no XML layouts, no Qt. Hierarchy, inspector, timeline,
  script editor with its own on-screen keyboard.
- **Oboe/AAudio with an AudioTrack fallback** and a probe that picks
  the working path at runtime
- **Its own scripting language, V.ONI** — and here's the fun part: the
  keywords are **Portuguese**. `central main { on moment { } }`,
  `exist(cond){ }`, `tyker(nome){ find(rf) follow() }`. The editor
  teaches it: type `if` and the error bar says *"if doesn't exist in
  V.ONI — it's called `exist`"* with a one-tap Replace button. Every
  error has a line number and a fix.
- A regression system I take seriously: every major bug gets an
  `R-NNN` sentinel test in the repo, **with a pasted mutation proof**
  (revert the fix → suite goes red → restore → green). ~810 unit tests
  plus ~295 replay checks in the virtual device harness.

What it does NOT have (the honest list):

- No shadows, no PBR, no lighting model beyond basic tinting. Yet.
- No desktop editor, no export to desktop — it's Android-only,
  landscape-locked, arm64-only.
- The asset formats are custom (`.gmesh`, `.gtext`, `.gm`, `.gi`) which
  was a deliberate "own the pipeline" decision, but it means no
  editor-on-the-planet can open them except this one.
- No physics beyond the custom 2-collision-kind system that's in there
  for gameplay basics.
- I have no GPU in CI, so rendering claims are verified on the real
  phone by a human (me) — the CI verifies everything around the GPU.

The workflow questions I expect, answered up front:

- *"Why write code on a phone?"* — It's the constraint I have. It
  forced good habits: small files, full-word names, tests instead of
  IDE vigilance. I'm not recommending it; I'm reporting from it.
- *"Why a new language?"* — The owner (it's built for one person's
  games) thinks in Portuguese, and existing embeddable languages don't
  teach you their dialect when you err. V.ONI's whole design goal is
  errors that teach, with a single registry feeding the docs, the
  error messages, the autocomplete skeletons and the public reference
  doc (CI checks the generated reference byte-for-byte against the
  registry).

Happy to go deep on any part — the replay harness, the immediate-mode
UI on a GPU with zero desktop debugging, the Oboe fallback chain, or
the "phone-only" workflow. AMA.
