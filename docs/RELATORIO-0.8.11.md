# RELATÓRIO 0.8.11 — ÁUDIO: AAudio COM PROBE + FALLBACK AUDIOTRACK + FORMATO PRÓPRIO .GI (ADPCM/OGG/MP3) + TIC AUDIOPLAYER + WORKSPACE ÁUDIO + GRAVAÇÃO DE MIC

> Sub-fase 0.8.11 da campanha de estabilização. HEAD anterior: 0.8.10-c
> (ed56afa). versionCode 41 · suíte 581 → 608 testes · CLÁUSULA CALMA
> respeitada (só o áudio do prompt + testes; zero física, zero V.ONI,
> zero layout novo além da 3ª aba do G3, da secção do Inspector e dos
> 2 itens de Settings exigidos pelo próprio prompt).

## 1. Objetivo

Dar SOM à cena com a mesma disciplina das sub-fases anteriores: (1) um
MISTURADOR puro no core (vozes/loop/pitch/posicional/master) que nunca
fala com o hardware; (2) o backend AAudio com PROBE DE ESTABILIDADE
antes de confiar e FALLBACK AudioTrack DOCUMENTADO debaixo da mesma
interface; (3) formato próprio `.gi` (ADPCM IMA 4:1 para WAV,
passthrough para OGG/MP3 com decode no load pelos vendors minimp3/
stb_vorbis); (4) o TIC AudioPlayer (estrutura primeiro — clip/autoplay/
loop/volume/pitch/posicional) com Inspector e glifo de altifalante;
(5) o WORKSPACE ÁUDIO (lista/importar/gravar/preview/waveform/renomear/
apagar/atribuir); (6) gravação de microfone → .gi ADPCM com a permissão
RECORD_AUDIO pedida no primeiro GRAVAR.

## 2. Diagnóstico (o que NÃO existia — e o que a 1ª versão dos ficheiros escondia)

O áudio era AUSENTE por completo (0.8.10 fechou formatos/arquivos/identidade).
O trabalho desta sub-fase começou por uma AUDITORIA DO CÓDIGO DE 1ª
VERSÃO que encontrou (todos corrigidos ANTES do push, com testes):

| Bug da 1ª versão | Causa | Fix |
|---|---|---|
| Símbolo duplicado `setMixFn` | definido em AudioOut.cpp E AudioOutDevice.cpp (o Android liga ambos) | ÚNICO dono em AudioOut.cpp + getter `currentMixFn()` |
| `ui::requestTextInput`/`textInputDone` | funções INVOCADAS mas inexistentes (não compilava) | fluxo pelo teclado in-app existente (propósito 7) |
| `vv::jni::env()` | chamado mas inexistente | VM do glue via `audioout::setVm` + attach próprio ("goni-audio") |
| Áudio por header `<aaudio/AAudio.h>` | minSdk 24: o header fica VAZIO (API 26+) e a ligação falha | **dlopen/dlsym à Oboe** com typedefs próprios (o mesmo binário corre em 24/25 com fallback limpo) |
| AudioTrack ctor com 4 args | a assinatura real é `(attrs,fmt,buf,mode,sessionId)` | tenta 5-arg, cai no 4-arg (ExceptionClear entre) + Builders públicos API 21+ |
| IMA-ADPCM gerava +ch amostras | o encoder codificava AS PRIMEIRAS (que já iam cruas nos predictors) e o decoder emitia-as dobradas | convenção limpa: predictors crus + nibbles do RESTO; decoder com `expectedFrames` |
| `kModeAudioId = 13` | COLIDIA com o G5 inspector (13) | 15 (+ teste de não-colisão) |
| "Subir" do navegador = 6 | colidia com a 6ª raiz Music (6) | números PARAMÉTRICOS (count+1 / count+2) |
| Workspace na área TODA + early-return | sobrepunha toolbar/painéis e cortava o frame | rect do editor de UI (`centerRect`) no fluxo normal |
| Fim exato do clip | a voz ficava "playing" entre callbacks (cursor==frames só era visto no início da PRÓXIMA iteração) | deteção no fim do bloco (bug apanhado pelo CI, ver §10) |
| Botões da lista dentro do scroll | "não clicavam" (o scroll reclama o gesto — regra da casa) | re-despacho por `scrollTap` (o padrão da Hierarchy) |
| Preset Audio com MeshRenderer | o caminho genérico adicionava mesh a TODOS os presets | branch antes do MeshRenderer (estrutura pura) |

## 3. Arquitetura do áudio (o misturador nunca conhece o hardware)

- **core/AudioEngine (PURO, CI)**: vozes (slots reutilizáveis por id)
  com clip/cursor/loop/volume/pitch/posicional; `mix(out, frames, ch,
  rate)` soma tudo com resample LINEAR duplo (pitch E taxa do device —
  um clip 22050 num device 44100 anda ao ritmo do relógio, aferido),
  mono→stereo duplicado, clamp [-1,1], master global; o posicional atenua
  pela distância ao listener (câmara ativa em Play, orbit no editor) com
  cursor a ANDAR mesmo mudo (o loop conta o tempo); vozes seguem a POS
  VIVA dos TICs por frame; `voiceProgress` alimenta a linha da waveform.
- **platform/AudioOut (a interface Backend)**: `start/stop/pause/resume/
  ready/name/framesOut/xruns` + `setMixFn/currentMixFn/setVm`. O TU
  comum (host+device) define o PROBE harness; o host tem fábricas stub
  `#ifndef __ANDROID__` (o device NÃO as compila — o bug do símbolo
  duplicado).
- **platform/AudioOutDevice (ANDROID)**: AAudioBackend via **AAudioLoader
  dlopen** (Oboe-style: minSdk 24 + API 26+ no mesmo binário; API < 26 →
  dlopen falha → fallback limpo; sanidade pós-open: canais/rate lidos do
  stream; erro de stream → `errored_` → ready false → probe vê). O
  fallback AudioTrackBackend (JNI): AudioFormat/AudioAttributes Builders,
  ctor 5-arg→4-arg, thread de escrita com jfloatArray global reutilizado,
  ExceptionClear em TODA a chamada; pause = flag (a thread dorme).
- **Lifecycle**: INIT_WINDOW → `audioBackendBoot()` (AAudio → se recusar
  → AudioTrack → se nenhum → sem som com log, a engine segue); APP_CMD_
  PAUSE/RESUME → pause/resume do stream (vozes intactas); TERM_WINDOW →
  stop. Reentrância do android_main: o boot volta a correr no INIT.

## 4. O PROBE (a exigência do prompt: provar antes de confiar)

- **Harness puro** (`runProbe` em AudioOut.cpp — corre no CI com fakes):
  N ciclos start/20 ms/stop (o ciclo de vida duro), M ciclos pause/resume
  (o lifecycle da activity), playback longo opcional; conta failures,
  pauseFailures, disconnects (ready false a meio — headset desligado no
  device), xruns; `shouldFallback()` = crash OU falha OU disconnect
  (qualquer uma ativa o AudioTrack — a regra "não apostamos a engine num
  backend não provado").
- **No device** (Settings → "diagnostico audio (probe)"): corre contra um
  backend FRESCO (nunca o vivo — o probe mata streams de propósito),
  escreve a TABELA no engine.log (`audio: probe aaudio — ciclos 50/50 ok
  … DECISAO: AAudio OK|FALLBACK AudioTrack`) e, se mandar fallback E o
  vivo for aaudio, **troca sozinho** (`audioBackendSwitch`).
- **Fallback de arranque**: o boot já tenta AAudio→AudioTrack sozinho (a
  recusa do openStream/start no C33 de alguém com API 24/25 cai aqui).

## 5. O formato próprio .gi (o "menor dentro da engine")

- **Header comum de 32 B** (o das 0.8.10): magic GICL/versão/endianMark/
  align/payloadSize/checksum FNV-1a/reserved — TODA a leitura valida
  magic/versão/endian/tamanho/checksum/contagens ANTES de tocar arrays;
  corrupção = erro LEGÍVEL (6 casos aferidos).
- **Codec 0 = ADPCM IMA 4:1 (WAV)**: as tabelas clássicas de 89 passos;
  os estados INICIAIS (predictor+index) por canal vão CRUS no payload
  (a 1ª amostra é EXATA) e os nibbles codificam o RESTO (2 amostras por
  byte, padding conhecido); o decoder recebe `expectedFrames` do
  contentor e para EXATAMENTE em frames×channels (a contagem era o bug
  da 1ª versão). Round-trip aferido: energia 0.00004 do original em 1 s
  de senoide (limite do teste 0.005), máx/amostra 3243/32768.
- **Codecs 1/2 = OGG/MP3 passthrough**: os bytes ORIGINAIS entram no
  .gi (re-encodar ADPCM perderia qualidade sem ganhar tamanho — decisão
  do prompt); o DECODE corre no LOAD: `decodeOgg` (stb_vorbis, planar→
  interleaved, guarda 1 G frames) e `decodeMp3` (minimp3 FLOAT_OUTPUT,
  buffer com padding de 4096 — a 1ª versão cortava em off+100<len e
  perdia o último frame). Vendors num ÚNICO TU (AudioCodecs.cpp).
- **Import**: `.wav` (PCM16 mono/stereo; 8-bit/float/ >2ch → erro
  legível) → ADPCM; `.ogg/.mp3` → passthrough validado por decode;
  outra extensão → "audio .X nao suportado (aceites: .wav .ogg .mp3)".
  O clip vive em `audio/<nome>.gi`; rácio no log
  (`audio: import … ratio=X`); guarda de 256 MB (áudio "gigante" é
  lixo — o streaming de 500 MB é para geometria).

## 6. O TIC AudioPlayer + o Inspector + os glifos

- **componentes/AudioPlayer.h (dados + getters)**: clipPath ("audio/
  x.gi"), autoplay, loop, volume 0..1, pitch 0.5..2, posicional + raios;
  runtime voiceId/previewing (NUNCA serializados); clampFields de defesa
  (chamado na leitura do serializer e nos sliders).
- **Preset "+ Audio"**: Transform+AudioPlayer SEM MeshRenderer/BodyComp
  (o bug da 1ª versão: o caminho genérico dava mesh a todos); registro
  "AudioPlayer" (id 9, count 10) e removeAll cobertos.
- **Inspector (o plano — fonte única)**: secção Audio com 10 linhas
  (cabeçalho, clip ▸ seletor 5, ouvir/parar = PREVIEW, autoplay, loop,
  volume, pitch, posicional, raio interno, raio externo) — ids 5700..
  5708 (faixa nova); o PREVIEW é um FLAG que o frame do main mapeia ao
  misturador (`audioPreviewTick(Tic&)` — o MESMO caminho do Play; fim
  natural devolve o botão a "ouvir").
- **Seletor de clips (assetMenu 5)**: catálogo `audio/` completo +
  "none" + "importar…" (browser na raiz Music; SEMPRE oferece import);
  `applyAssetPick` kind 5 é PURO (escreve clipPath, mata a voz antiga).
- **Glifos do editor** (nunca em Play): ALTIFALANTE em polilinha na
  posição do TIC (amarelo tocando/cinza parado — lê voiceId) + esfera
  WIREFRAME do raio externo quando posicional (8 longitudes × 4 lat).

## 7. O workspace ÁUDIO + a gravação

- **Aba G3 "3D | UI | ÁUDIO"** (kModeAudioId=15; exclusivos; o 3D limpa
  a seleção de elemento ao voltar). O workspace desenha no RECT DO
  VIEWPORT (o mesmo do editor de UI — `centerRect` menos a timeline quando
  visível; NUNCA a área toda: toolbar/painéis ficam nos seus sítios e o
  frame segue o fluxo normal — o early-return da 1ª versão morreu).
- **Conteúdo**: barra Importar/Gravar/Play/Stop; overlay de gravação
  (temporizador + MEDIDOR de nível 0..1); LISTA de clips com scroll
  (nome limpo + codec + duração; seleção pelo re-despacho scrollTap —
  os botões dentro do scroll só desenham, a regra da casa); WAVEFORM de
  160 picos com LINHA de progresso do preview; renomear (teclado in-app
  propósito 7: o commit copia bytes→remove o velho→catálogo→os TICs com
  a ref antiga seguem a nova); apagar COM CONFIRMAÇÃO (2 toques);
  atribuir a TIC; host com std::function (fakes com captures no CI;
  TODOS os alvos opcionais — guards).
- **Gravação (device)**: worker thread com AudioRecord JNI (MIC/44100/
  MONO/PCM16, getMinBufferSize×4, read por chunks de 2048, pico→medidor);
  STOP → writeGi ADPCM → `audio/rec-<unix>.gi` + catálogo + rácio. A
  permissão RECORD_AUDIO: declarada no manifest, pedida NO 1º GRAVAR
  (VvActivity.ensureMicPermission → diálogo; false = "conceda o microfone
  e toque Gravar de novo" — o contrato humano do All Files, sem loops);
  a ponte (`jniEnsureMicPermission`) é aferida no CI contra o fake JNI.
- **HOST/CI**: o mic é SINTÉTICO (senoide 440 Hz, 10× tempo real) pela
  MESMA máquina de estados — o wiring inteiro (toggle/worker/mutex/
  medidor/STOP→.gi/catálogo) aferido sem hardware.

## 8. Implementação por ficheiro

- **NOVOS**: `assets/GiFormat.{h,cpp}` (contentor+ADPCM+WAV+import),
  `assets/AudioCodecs.cpp` (stb_vorbis+minimp3 num TU), `core/AudioEngine.
  {h,cpp}` (misturador), `platform/AudioOut.{h,cpp}` (interface+probe+
  stubs host), `platform/AudioOutDevice.cpp` (Android: AAudio dlopen +
  AudioTrack JNI), `components/AudioPlayer.h`, `ui/AudioWorkspace.{h,cpp}`,
  `vendor/minimp3/minimp3.h`, `vendor/stb_vorbis/stb_vorbis.{h,c}`,
  `tests/test_wiring011.cpp`.
- **MODIFICADOS**: `platform/main.cpp` (o coração de áudio: cache de
  clips, catálogo, preview×2, backend boot/probe/switch, gravação,
  listener, glifos, workspace no frame, import wav/ogg/mp3 com diálogo
  'a', renomear propósito 7, settings volume, pause/resume, settings
  agora LÊEM o settings.goni — o load de 0.8.10 nunca corria), `ui/
  EditorUi.{h,cpp}` (Inspector Audio, seletor 5, plus-menu Audio,
  settings +2, catálogo .audio), `ui/EditorLayout.h` (perfil/plano/kinds/
  ids do Audio), `ui/Toolbar.{h,cpp}` (G3 3D|UI|ÁUDIO, id 15), `ui/
  UiEditor.cpp` (navegador: 6 raízes paramétricas, kind 's', teclado
  propósito 7), `core/Presets.{h,cpp}` (PresetKind::Audio sem mesh),
  `core/ComponentStore.{h,cpp}` (storage AudioPlayer), `core/
  SceneSerializer.cpp` (append/fill AudioPlayer), `platform/FileApi.{h,
  cpp}` (kind 's', raiz Music), `platform/StorageBridge.{h,cpp}` (ponte
  do mic), `AndroidManifest.xml` (RECORD_AUDIO), `VvActivity.java`
  (ensureMicPermission), `app/build.gradle` (41/0.8.11), CMake×2 (TUs +
  -ldl + vendor), `.github/workflows/release.yml` (10 gates de símbolos
  de áudio), `tests/stub/jni.h` (mic_granted), `tests/test_wiring087.cpp`
  (secção 0.8.11 device), `tests/test_browser.cpp`/`test_ui.cpp`/
  `test_components.cpp` (constantes novas).

## 9. Decisões

- **dlopen em vez do header AAudio**: o minSdk 24 esvazia o
  <aaudio/AAudio.h> (guard __ANDROID_API__>=26) e a ligação direta não
  resolve; o padrão Oboe (dlopen+dlsym+typedefs próprios, enums da ABI
  congelada com sanidade pós-open) dá UM binário para 24/25/26+ com
  fallback limpo nas antigas. Falha de enum = start falha = fallback
  (nunca crash).
- **Passthrough OGG/MP3**: re-encodar para ADPCM perderia qualidade sem
  ganhar tamanho (já comprimidos); o decode fica no LOAD (RAM: só o PCM
  final; o .gi guarda os bytes crus).
- **O preview é um FLAG**: o Inspector (puro) não fala com o misturador;
  o frame do main mapeia flag→voz (o MESMO caminho do Play — zero
  caminhos paralelos).
- **std::function no host do workspace** (não ponteiros crus): os fakes
  do CI contam chamadas com captures; o custo é irrelevante (≤1 chamada
  por toque).
- **Botões na lista via scrollTap**: os widgets dentro de beginScroll só
  desenham (o scroll reclama o gesto — regra F4.1 da casa); o tap volta
  pelo re-despacho com a MESMA geometria desenhada.
- **Settings agora LÊEM o settings.goni** no post-load (o loadProject-
  Settings de 0.8.10 nunca era CHAMADO — o setting só vivia na RAM;
  bug latente morto de caminho).

## 10. Testes (581 → 608; RED→GREEN provado)

Puros (22): ADPCM contagem EXATA mono/stereo ×6 comprimentos (o bug da
1ª versão) + defesas; .gi round-trip (metadados/PCM/peaks) + 6 corrupções
legíveis (checksum recalculado para o erro chegar ao codec); import WAV
mono/stereo com rácio + 6 erros de formato; misturador cursor/loop/fim/
pitch/resample/slots/stopAll/clip inválido; posicional (pura + mudo com
cursor a andar + volta com som) + master; PROBE ×4 (ok→AAudio OK, start
falha→FALLBACK, disconnect→FALLBACK, null→crash); workspace ×2 (toques
chamam o host com geometria calculada; apagar exige confirmação; host
parcial nunca crasha); serializer round-trip + defaults + clamps; preset
estrutura + registro + removeAll; FileApi kinds + raiz Music; seletor 5
(draw com re-armação + apply none/pick/fora/sem-AudioPlayer); plano do
Inspector (+10 linhas, ids sem colisão); plus-menu 7.
Device (5, no TU do main.cpp): import wav e2e (→audio/x.gi + catálogo×2
+ rácio + diálogo 'a' + "Sim" atribui); sem AudioPlayer não pergunta;
gravação sintética e2e (toggle/worker/medidor/STOP→.gi/rácio + ponte do
mic concedida/negada no fake JNI); boot/probe/troca + pause/resume +
contrato setMixFn/currentMixFn; frame no modo ÁUDIO + glifos + preview
do Inspector pelo caminho real + fim natural.

**RED→GREEN (duplo)**: (a) o encoder volta a codificar as primeiras
amostras; (b) o misturador volta a não detetar o fim exato no fim do
bloco → **12 testes FALHAM**; restaurados → **608 OK, 0 falhas**.

Output real (colado):
```
  [import] wav 0.3s → audio/goni_w011_salto.gi (0.30s, codec=adpcm)
  [gravar] audio/rec-1790978689.gi: 1.20s 52920 frames
  wiring011_probe_backend_saudavel_aaaudio_ok    OK
  wiring011_probe_start_falha_decide_fallback    OK
  wiring011_probe_disconnect_decide_fallback     OK
  wiring011_probe_null_é_crash_e_fallback        OK
```

## 11. Riscos

- O AAudioLoader usa os VALORES da ABI congelada da API 26 (formatos/
  modos/estados) — se um enum mudasse (não mudou em 8 anos), a sanidade
  pós-open/resultado de start apanha e o fallback assume (nunca crash).
- O callback do AAudio corre na thread de áudio: o mix() é lock-free por
  desenho; o `voices_` é um vector SEM lock — vozes criadas/paradas
  pelo thread da engine enquanto o callback mistura podem ser VISTAS a
  meio (re-alocação do vector em push_back) — a janela é 1 callback
  (~10 ms) e o reboot do vector só acontece com vozes NOVAS após todas
  paradas; aceitável para jogo de telemóvel (documentado; um ring-buffer
  SPSC = melhoria futura).
- A gravação device usa read bloqueante numa thread própria — um
  AudioRecord que não arranja buffer devolve erro legível e o worker sai
  limpo (o toggle devolve o estado ao dono).

## 12. Dívida

- Trim de clips no workspace (o 🔶 do prompt): cortar início/fim por
  picos — próxima sub-fase de áudio.
- Ring-buffer SPSC para vozes (eliminar a janela de re-alocação).
- .gi stereo ADPCM com frames ímpares: o padding do último byte é
  conhecido (decoder parametrizado) — sem casos no device hoje.
- LOD/streaming de clipes longos (um .gi de 30 min carrega inteiro;
  hoje o dono usa clips de segundos/minutos).

## 13. CLÁUSULA CALMA

Só o áudio do prompt + testes: zero física, zero V.ONI, zero rede, zero
layout novo além da 3ª aba do G3 + secção do Inspector + workspace +
2 itens de Settings — TUDO exigido pelo próprio prompt 0.8.11.

## 14. Como ler o log se algo falhar no C33

- **Sem som**: procurar `audio: backend` (AAudio/AudioTrack ATIVO; se
  `NENHUM backend ligou` o device recusou ambos — o misturador segue e
  o resto da app está são); `audio: TROCA de backend` é o probe a decidir.
- **Probe**: a linha `audio: probe aaudio — …` É a tabela completa;
  `DECISAO:` fecha o veredito.
- **Import**: `audio: import <rel> codec=… ratio=…` (sucesso) ou
  `audio: import de '<path>' FALHOU — <razão>` (a causa exata).
- **Gravação**: `audio: gravacao ARRANCA` → `audio: gravado Ns …
  ratio=4.0x`; `a espera da permissao do mic` = o diálogo está aberto.
- **Clips**: `audio: load <rel> … codec=…` (o decode correu);
  `audio: preview no TIC '…'` (o Inspector) / `audio: preview '…'`
  (o workspace); `audio: clip '…' atribuido ao TIC '…'` (o "Sim"/o
  seletor).
- **Crash (se algum)**: o dump nasce com versionCode 41 no nome — a
  identidade da 0.8.10 continua a funcionar.

## 15. Gates do CI

core-tests 608 · check_main · link_parity (93 TUs com -lz/-ldl) ·
jni_parity (4 natives) · build-release ASSINADO versionCode 41 (2 passes
com build_info.txt embutido) · verify-entry-symbols com os GATES NOVOS
de ÁUDIO (writeGi/readGi/importAudioToGi/imaEncode/imaDecode/decodeOgg/
decodeMp3/runProbe/probeTable/drawAudioWorkspace no .dynsym real do APK
— o precedente FileApi: testes verdes + device sem a feature = gate
obrigatório).

## 16. Checklist device

No README (§ Verificação no Realme C33 — 0.8.11): 7 blocos — o som ouve
+ pause/resume do lifecycle, o probe com headset a desligar (DECISAO na
tabela + troca sozinha), import de .wav/.ogg/.mp3 com rácio e waveform,
GRAVAR com permissão/medidor/rácio, TIC de Audio com preview/posicional
(esfera wireframe + abafar), autoplay no Play + round-trip do projeto,
volume geral + renomear/apagar com confirmação. Zero dumps novos
continua a ser o critério global.
