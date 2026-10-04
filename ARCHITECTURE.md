# ARCHITECTURE.md — o mapa dos módulos do G.One VV

> Como este documento é mantido: escrito à mão a partir do código real
> (cada camada nomeia os ficheiros que a compõem); o gate `docs-lint`
> (R-016) impede que aqui viva placeholder ou achismo. A referência da
> LINGUAGEM é gerada do registo (`VONI_referencia.md`) — este ficheiro
> desenha a arquitetura, não a linguagem.

## 1. O diagrama de camadas (a visão de cima)

```
┌──────────────────────────────────────────────────────────────────────────┐
│ JAVA (app/src/main/java/vv/goni)                                          │
│   VvActivity (NativeActivity + JniBridge) · ProjectManagerActivity       │
│   ReloadGate · ProjectsFormat · UiIcons · VvProjects                      │
└───────────────┬──────────────────────────────────────────────────────────┘
                │ JNI (jni_* — a tabela afere-se pelo gate jni_parity)
┌───────────────▼──────────────────────────────────────────────────────────┐
│ PLATFORM (cpp/platform) — a casca Android                                 │
│   main.cpp (ANativeActivity_onCreate/android_main, o loop e o dispatch)  │
│   EglContext · FileApi/SafStorage(bridge) · ImeQueue · InputState        │
│   OboeBackend/AudioOut(Device) · BuildInfo · CrashHandler · EngineLog    │
└───────────────┬──────────────────────────────────────────────────────────┘
                │ chama para dentro; nunca o contrário
┌───────────────▼──────────────────────────────────────────────────────────┐
│ UI (cpp/ui) — o editor immediate-mode                                     │
│   EditorUi · Toolbar · ViewportChrome · BottomPanel · Hier/Inspector      │
│   ScriptEditor (+ teclado in-app) · DocsScreen · SettingsPage             │
│   AudioWorkspace · Gizmo/CamGizmo · SceneFx · Theme/Layout/SafeArea       │
│   Icons · FontAtlas · TextFit/ScrollMath · UiCanvas (canvas de jogo)     │
└───────────────┬──────────────────────────────────────────────────────────┘
                │ desenha e despacha por cima de
┌───────────────▼──────────────────────────────────────────────────────────┐
│ NÚCLEO (cpp/core + components + render + physics) — engine pura (testável│
│ em desktop SEM Android: os testes do core e o c33_virtual correm no       │
│ Linux do CI)                                                              │
│   Scene/Tic · ComponentStore/Registry · Project(+Storage/Serializer)      │
│   AnimationSystem · TransformSystem · PhysicsSystem · AssetPersist        │
│   components: Transform3D · MeshRenderer · ScriptComp · AnimationPlayer   │
│               SkeletonComp · BodyComp · CameraComp · InputMap ·           │
│               TouchControls · AudioPlayer · UiCanvas                      │
│   render: Camera · Mesh · Material · GpuAssets · DrawStats · Grid · …     │
│           (DrawStats: os contadores de verts/draw calls — a MESMA fonte  │
│            que a barra de estado do editor e o bench consomem)            │
└───────────────┬──────────────────────────────────────────────────────────┘
                │ o script manda no núcleo por
┌───────────────▼──────────────────────────────────────────────────────────┐
│ V.ONI (cpp/voni) — a linguagem de scripting do dono                       │
│                                                                           │
│   gramática → AST → COMPILADOR → VM ──▶ EngineHost ──▶ núcleo            │
│                                                                           │
│   VoniGrammar.cpp   o parser (formas fechadas; erros com linha)           │
│        │                                                                 │
│   VoniAst.h         a árvore (declarações/momentos/tykers/comandos)      │
│        │                                                                 │
│   VoniCompile.cpp   valida+compila (contra o REGISTO; erros que ENSINAM) │
│        │                                                                 │
│   VoniVm.cpp        executa (moments/on/allmoments; budget de instruções)│
│        │            + VoniTykers.cpp (runtime dos tykers: RFs, ciclos,   │
│        │              componentes follow/look/orbit/…)                    │
│        ▼                                                                 │
│   VoniEngineHost.cpp o Host: propriedades RTTI e ações sobre TICs reais   │
│        │              (posição, rotação, cor, mesh, animação, áudio…)     │
│        ▼                                                                 │
│   core/ núcleo      Transform3D/MeshRenderer/… mudam de verdade          │
│                                                                           │
│   VoniRegistry.cpp  O REGISTO CENTRAL (a única fonte da superfície):      │
│                      nome/sintaxe/1-linha/exemplo/equivalência Python-JS  │
│                      /esqueleto de Tab/argc de CADA entrada da linguagem  │
│                      (linkers, tykers, find, 13 componentes, Linguagem,   │
│                      Comandos). Desta ÚMA tabela saem: as Docs            │
│                      (VoniDocs é vista 1:1), os erros-que-ensinam        │
│                      (kForeign + replace), a strip de ajuda do editor,    │
│                      o completamento, os esqueletos de Tab, o             │
│                      copiar-referência e a referência pública gerada      │
│                      (VONI_referencia.md — o CI afere byte a byte, R-013)│
│   VoniHighlight.cpp colorização do editor (mesmas categorias do registo)  │
└──────────────────────────────────────────────────────────────────────────┘
                │ tudo o acima é afervado por
┌───────────────▼──────────────────────────────────────────────────────────┐
│ TESTES (tests/) — a suíte que substitui o IDE                              │
│   test_core (binário, ~810 casos + sentinelas R-NNN) — núcleo/voni/ui    │
│   em desktop puro (stubs de plataforma em tests/stub)                    │
│   c33_virtual (o dispositivo virtual) — repete no Linux o caminho real   │
│   de ANativeActivity/events/IME/rotação do C33 (FASEs numeradas)         │
│   voni_refgen — gera a referência pública do registo (R-013)             │
└──────────────────────────────────────────────────────────────────────────┘
```

## 2. As regras de dependência (o que pode chamar o quê)

1. **A seta só desce.** platform → ui → núcleo → voni→núcleo (o Host).
   O núcleo NÃO conhece a UI nem a platform (por isso compila e é testado
   em desktop); a voni só toca o núcleo pelo `VoniEngineHost`.
2. **O main.cpp é o despachante.** Todo o evento Android (touch, IME,
   rotação, lifecycle) entra por `platform/main.cpp`, é traduzido em
   estado puro (`InputState`/`ImeQueue`) e consumido pela UI no frame —
   nunca a UI pergunta nada à Java diretamente.
3. **O registo é a única fonte da linguagem.** Nenhuma lista de comandos,
   erro didático, tooltip ou doc duplica o registo — tudo é vista
   (`VoniDocs::all()`, `reg::find`, `prefixMatch`, `fullReferenceMarkdown`).
   O gate R-013 afere a bijeção.
4. **DrawStats é a única fonte dos números de render.** A barra de
   estado do editor, os dumps de diagnóstico e o bench leem o MESMO
   contador (`render/DrawStats.h`) — nunca há dois contadores para o
   mesmo facto.
5. **A persistência tem um dono por camada.** SAF (árvore de documentos)
   em `platform` + `core/SafStorage`; cenas/projetos em `core/ProjectStorage`
   + `SceneSerializer`; preferências de layout em `ui/EditorLayout` (com
   debounce, G1-5); o que o device vê gravado é o que os testes afervam
   em `.goni` round-trip.

## 3. O caminho de um frame (o traço que orienta navegação)

```
android_main → InputState/ImeQueue drenados → ui (draw+hit do editor) →
voni tick (moments→allmoments→tykers, com budget) → systems
(Transform/Animation/Physics) → render submit (DrawStats acumula) →
EglContext swap
```

## 4. Onde cada preocupação transversal vive

| Preocupação | Casa | Nota |
|---|---|---|
| Insets/safe-area | `ui/SafeArea.h` + `ui/Layout` (Theme) | fonte única; desktop = 0 |
| Overlays/camadas | `ui` (fullscreenOverlayOpen) | cena < painéis < modais < teclado |
| Erros V.ONI | `voni` (Error com fixFrom/fixTo) | linha + fix + botão Substituir |
| Áudio | `platform/OboeBackend` → `core/AudioEngine` | probe + fallback AudioTrack |
| Crash | `platform/CrashHandler` | dump com build/git/so/epoch |
| Bench | `core` + Settings (Diagnóstico) | medições reais, R-017 |
| Gates de CI | `scripts/` + `.github/workflows/release.yml` | scope-check · release-identity · docs-lint · … |

## 5. Os números que importam (no momento em que se escreveu)

817 casos no test_core (com as sentinelas R-001..R-017) · 316 checks no
c33_virtual (FASE 12) · 6 gates locais + os do CI (scope-check ·
release-identity · docs-lint) · ~105 TUs no alvo da
app (link_parity). Estes números movem-se a cada versão — o valor exato
de cada época está no RELATÓRIO dessa versão.
