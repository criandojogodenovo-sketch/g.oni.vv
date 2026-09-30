# Exceção à regra "zero Java" — All Files Access (F5.2)

## Porquê Java continua a existir (e porquê o SAF foi removido)

A regra do projeto era `android:hasCode="false"` + `NativeActivity` pura.
Duas coisas do **armazenamento** não funcionam assim:

1. **Lançar a janela de permissões e receber o volta-chega** — `startActivityForResult`
   devolve por **`Activity.onActivityResult`**, e o `NativeActivity` do framework
   não reencaminha esse resultado para o código nativo.
2. **Export dos logs por MediaStore** — `ContentResolver` não existe fora da JVM.

A exceção Java FOI introduZIDA na F5.1-C para o **SAF tree picker**
(`ACTION_OPEN_DOCUMENT_TREE` + `DocumentsContract`). No C33 (Android 12) o
fluxo SAF mostrava **"SAF indisponível" sem nunca pedir permissão** — picker
de árvore é mais complexo, menos familiar e deixou de existir na F5.2.

**Modelo atual (o mesmo do Godot e de outros editores): All Files Access.**

1. O editor **pergunta in-app**: "Precisa de acesso a todos os ficheiros para
   importar/exportar projetos" (botões **Permitir** / **Cancelar**) — na
   primeira tentativa de Importar/Export.
2. "Permitir" → o nativo chama `VvActivity.openAllFilesSettings(4301)`, que lança
   **`Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION`** com a URI
   `package:vv.goni` — abre a janela de permissões DO app.
3. O utilizador **ativa o interruptor manualmente** e volta.
4. `onActivityResult` reencaminha por JNI (fila `PendingResult` → thread da
   engine) → o nativo verifica **`Environment.isExternalStorageManager()`**:
   - `true` → modo **all files**: File API POSIX direta — import varre
     `Download/` e `Documents/` (obj/gltf/glb/png) e o export escreve em
     `Download/GOneVV/export/` (visível no gestor de ficheiros);
   - `false`/recusa/API < 30 → modo **app-private** (`getExternalFilesDir`,
     sem permissões) — o Settings mostra sempre qual modo está ativo.

## Superfície Java (mínima, 1 classe)

| Ficheiro | Responsabilidade |
|----------|------------------|
| `app/src/main/java/vv/goni/VvActivity.java` | subclasse de `NativeActivity`; `openAllFilesSettings(requestCode)` lança o intent de permissões; `onActivityResult` reencaminha `(request, result, uri, flags)` por JNI estático; `exportLogsToDownloads(relPath)` copia os logs por MediaStore |

O C++ continua sem depender de Java no core: a máquina de estado do fluxo
(`platform/StoragePerm.h`) e a File API (`platform/FileApi.cpp`) são GL-free
e testadas no CI com caminhos de `/tmp`; a ponte JNI
(`platform/StorageBridge.cpp`) existe só na build Android. O I/O de ficheiros
em si é POSIX direto — a Java só abre a janela de permissões.

## Consequências

- `android:hasCode="true"` no manifest (AGP volta a dexar a 1 classe).
- Tema mono, 3 botões, landscape e o resto da app não mudam.
- **`MANAGE_EXTERNAL_STORAGE` ESTÁ DECLARADO** (F5.5 — ver secção abaixo):
  sem a declaração, o sistema NÃO TEM O QUE CONCEDER — a app não aparecia na
  lista "Acesso a todos os ficheiros" e `isExternalStorageManager()` era
  sempre `false`. A permissão continua a NÃO ser pedida no arranque: é
  concedida apenas pelo interruptor das definições, quando o utilizador
  tenta importar/exportar.

## Play Policy (se um dia publicar)

`MANAGE_EXTERNAL_STORAGE` é **permissão restrita** no Google Play: requer
submissão de justificação (vídeo demonstrando o fluxo + declaração) e a
declaração no manifest é o pré-requisito para o fluxo ser sequer proposto à
análise. A F5.2 removeu a declaração "por cautela" com a suposição ERRADA de
que o interruptor das definições funcionaria sem ela — não funciona: a
declaração é o que faz a app APARECER na lista "Acesso a todos os ficheiros"
e o que dá ao sistema uma permissão para conceder (F5.5 corrigiu isto —
ver secção abaixo). A justificação da G.One VV:

> O G.One VV é um editor de jogos 3D que importa modelos (OBJ/glTF/GLB) e
> texturas (PNG) escolhidos pelo utilizador e exporta modelos criados no
> editor para pastas públicas (Download/Documents). As pastas partilhadas
> (SAF) foram testadas e abandonadas por tornarem o fluxo de import/export
> inviável no dispositivo de referência; o acesso amplo a ficheiros é
> necessário para listar ficheiros .obj/.gltf/.glb/.png nas pastas do
> utilizador sem duplicar a biblioteca de mídia (MediaStore não cobre
> navegação por extensão em pastas arbitrárias).

**Decisão F5.5 (documentada)**: declarar `MANAGE_EXTERNAL_STORAGE` é
PERMITIDO — a restrição do Play aplica-se à PUBLICAÇÃO, não à declaração.
Distribuir por APK/sideload (o caso atual do C33) não passa pela análise do
Play. Se um dia publicar na Play Store: (a) submeter a justificação acima
(o fluxo — diálogo → janela do sistema → toggle — é exatamente o que o Play
recomenda para apps de gestão de ficheiros/editores), ou (b) migrar para
SAF/MediaStore — a File API e a UI já estão isoladas atrás de
`fileapi::*`/`storage::*`, o custo da troca é local (a alternativa
`ACTION_OPEN_DOCUMENT` por ficheiro já foi avaliada na F5.2 e é o fallback
documentado).

## Histórico

- **F5.1-C (0.6.0)**: exceção criada para o SAF (VvActivity + SafIo).
- **F5.1-hotfix (0.6.1)**: `exportLogsToDownloads` via MediaStore +
  auditoria/fix da ponte JNI (docs/AUDIT_SAF_JNI.md).
- **F5.2 (0.6.2)**: SAF tree picker REMOVIDO (SafIo.java e SafIoJni.cpp
  apagados); All Files Access documentado como caminho primário; log viewer
  in-app (Settings → "Ver logs") funcional sem export.
- **F5.3 (0.6.3)**: HANDSHAKE INVERTIDO — a VvActivity registra-se no
  native (`nativeRegisterActivity` no onCreate + onResume); a causa única
  de todas as pontes mortas 0.6.0→0.6.2 era o `initJava` do thread do glue
  (`GetEnv` = `JNI_EDETACHED`); attach de threads nomeado + mensagens
  honestas (docs/HANDSHAKE_AUDIT.md com a evidência do APK 0.6.2 real).

## F5.4 (0.6.4) — o SAF VOLTA, pela porta certa: Gestor de Projetos

A remoção do tree picker na F5.2 resolvia o fluxo de PERMISSÕES, mas criava
uma limitação de produto: o projeto vivia SEMPRE no app-private
(getExternalFilesDir). A F5.4 reintroduz o SAF com um papel EXATO e
complementar — os dois fluxos COEXISTEM, não se substituem:

- **SAF (ACTION_OPEN_DOCUMENT_TREE) — SÓ no ecrã inicial**: escolher a
  pasta de CADA projeto no Gestor de Projetos. Cada escolha gera um URI
  próprio com takePersistableUriPermission — projetos diferentes em pastas
  diferentes, sem depender de All Files Access. O I/O do projeto corre por
  core/SafStorage (ProjectStorage sobre a árvore SAF; fds via
  ParcelFileDescriptor.detachFd).
- **All Files Access (MANAGE_APP_ALL_FILES_ACCESS_PERMISSION) — só para
  import/export de assets soltos DENTRO de um projeto já aberto**
  (varrimento de Download/Documents, export para Download/GOneVV/export):
  exatamente o fluxo F5.2, intacto.

A lista de projetos (projects.json) vive no app-private — não depende
nem do handshake nem de permissões. Sem handshake, o boot cai no modo
app-private com a mensagem honesta "ponte Java indisponível (handshake)".

## F5.5 — All Files Access DE VERDADE (fix do C33 0.6.9)

No C33 com o 0.6.9, a G.One VV NÃO aparecia na lista "Acesso a todos os
ficheiros" (enquanto o Godot aparecia "Permitida") e o interruptor na
página da app não concedia nada. **Causa**: o manifest não declarava
`android.permission.MANAGE_EXTERNAL_STORAGE` — removido na F5.2 por cautela
Play Policy (a secção "Consequências" acima chegou a documentar como opção
que a permissão NÃO era declarada). Sem a declaração, o sistema não tem o
que conceder e `Environment.isExternalStorageManager()` é sempre `false` —
o fluxo de settings nunca pode funcionar, por muito correto que esteja.

O que mudou (só manifest + lógica de verificação + docs — CLÁUSULA CALMA,
zero física/render/componentes):

1. **Manifest**: `<uses-permission android:name="android.permission.
   MANAGE_EXTERNAL_STORAGE" />` — a declaração é o que faz a app APARECER
   na lista "Acesso a todos os ficheiros" e o que dá ao interruptor uma
   permissão para conceder. A app continua a NÃO pedir nada no arranque.
2. **Fluxo mantido** (exatamente como na F5.2): diálogo in-app → "Permitir"
   → `ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION` (URI `package:`) →
   voltar → re-verificar `isExternalStorageManager()` → prosseguir para
   import/export.
3. **Re-verificação no RESUME** (`APP_CMD_RESUME` do nativo — o
   "re-verificar no onResume" do fluxo): o caminho normal é o
   `onActivityResult` (corre ANTES do `onResume`), mas nem todo o ecrã de
   settings OEM termina com `setResult` — o resume drena a fila primeiro e,
   se o fluxo ainda está `PendingSettings`, verifica fresco e decide ali
   (`storage::resumeRecheck` em `platform/StoragePerm.h`, política pura
   afervel no CI). Recusa → toast claro, SEM relançar definições (sem
   loop). Ação retomada sempre 1× (nem o resultado tardio duplica).
4. **Log de transição**: `storage: all-files granted=1/0 (...)` no
   engine.log a cada transição — boot, retorno das definições,
   re-verificação no resume e variações detetadas por tentativa — visível
   no log viewer in-app (Settings → "Ver logs").
5. **Gate CI**: o job `verify-entry-symbols` passou a exigir a permissão no
   manifest BINÁRIO do APK (aapt2 dump) — regressão impossível de passar
   despercebida.

Implicação Play Policy: ver secção "Play Policy" acima — declarar é
permitido; publicar exige justificação ou migração para SAF/MediaStore.
