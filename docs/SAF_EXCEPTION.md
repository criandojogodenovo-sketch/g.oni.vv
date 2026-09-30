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
- **Nenhuma permissão no manifest**: `MANAGE_EXTERNAL_STORAGE` NÃO é
  declarado — a app nunca o pede no arranque (nem na instalação); pede-o
  APENAS via a janela de settings quando o utilizador tenta importar/exportar.

## Play Policy (se um dia publicar)

`MANAGE_EXTERNAL_STORAGE` é **permissão restrita** no Google Play: requer
submissão de justificação (vídeo demonstrando o fluxo + declaração). A
justificação da G.One VV:

> O G.One VV é um editor de jogos 3D que importa modelos (OBJ/glTF/GLB) e
> texturas (PNG) escolhidos pelo utilizador e exporta modelos criados no
> editor para pastas públicas (Download/Documents). As pastas partilhadas
> (SAF) foram testadas e abandonadas por tornarem o fluxo de import/export
> inviável no dispositivo de referência; o acesso amplo a ficheiros é
> necessário para listar ficheiros .obj/.gltf/.glb/.png nas pastas do
> utilizador sem duplicar a biblioteca de mídia (MediaStore não cobre
> navegação por extensão em pastas arbitrárias).

Caminho alternativo se o Play recusar: voltar a um picker de ficheiro único
(`ACTION_OPEN_DOCUMENT`) — a File API e a UI já estão isoladas atrás de
`fileapi::*`/`storage::*`, o custo da troca é local.

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
