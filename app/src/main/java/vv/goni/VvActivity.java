package vv.goni;

import android.app.NativeActivity;
import android.content.ContentValues;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Environment;
import android.provider.DocumentsContract;
import android.provider.MediaStore;
import android.provider.Settings;
import android.util.Log;
import android.view.View;   // 0.9.0 (spec I): setImmersive usa View.SYSTEM_UI_FLAG_*
import android.content.pm.ActivityInfo;   // 0.9.1: setRequestedOrientation
import android.graphics.Color;            // 0.9.1: EditText do IME transparente
import android.text.InputType;            // 0.9.1: inputType multi-linha
import android.view.KeyEvent;             // 0.9.1: teclas do IME
import android.view.inputmethod.EditorInfo;             // 0.9.1
import android.view.inputmethod.InputConnection;        // 0.9.1
import android.view.inputmethod.InputConnectionWrapper; // 0.9.1
import android.view.inputmethod.InputMethodManager;     // 0.9.1
import android.widget.EditText;           // 0.9.1: o host do IME
import android.widget.FrameLayout;        // 0.9.1: layout 1x1 do host

/**
 * F5.2 — ponte Java mínima do ARMAZENAMENTO (sucessora da exceção SAF).
 *
 * F5.3 — HANDSHAKE INVERTIDO: é ESTA activity que se registra no nativo
 * (nativeRegisterActivity) — nunca o contrário. O android_main corre no
 * thread do glue (pthread), que NÃO está anexado à VM: GetEnv devolvia
 * JNI_EDETACHED e a ponte morria com "env/activity indisponíveis" — causa
 * única de todas as features Java-dependentes falharem desde a 0.6.0
 * (docs/HANDSHAKE_AUDIT.md). O thread da UI (aqui) está SEMPRE anexado.
 *
 * Sequência garantida (0.6.4 — causa raiz do UnsatisfiedLinkError no
 * RMX3624, Android 13): o android.app.NativeActivity NÃO faz
 * System.loadLibrary — o framework carrega a lib com dlopen(RTLD_LOCAL)
 * DIRETO (loadNativeCode_native em android_app_NativeActivity.cpp) e um
 * dlopen cru NUNCA chama o JNI_OnLoad nem coloca a lib no mapa de
 * resolução do JVM. Consequência na 0.6.3: o RegisterNatives do
 * JNI_OnLoad nunca correu no device e a busca por nome não via a lib →
 * "No implementation found" em TODA chamada nativa (o hospedeiro testava
 * o C++ diretamente — a RESOLUÇÃO não era modelada). FIX: o static
 * initializer abaixo carrega a lib NO Java — JNI_OnLoad corre (registo)
 * e a lib entra no mapa do JVM; o dlopen do framework depois reaproveita
 * a MESMA lib já carregada (idempotente). onResume reforça o registo
 * (idempotente — re-caches e substitui o GlobalRef).
 *
 * Defesa em profundidade (native): se MESMO assim o registo do JNI_OnLoad
 * falhar por qualquer razão de classloader, o 1º nativeRegisterActivity
 * re-tenta o RegisterNatives via GetObjectClass (sem FindClass) — ver
 * ensureNativesRegistered em platform/StorageBridge.cpp.
 *
 * FLUXO All Files Access (o mesmo do Godot e de outros editores):
 *   1. o editor pergunta in-app ("Precisa de acesso a todos os ficheiros…");
 *   2. "Permitir" → JNI chama openAllFilesSettings() AQUI — lança
 *      Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION com a URI
 *      "package:<pkg>" (abre a janela de permissões DESTE app);
 *   3. o utilizador ativa o interruptor manualmente e volta;
 *   4. onActivityResult reencaminha o resultado por JNI (fila p/ o thread
 *      da engine) → nativo verifica Environment.isExternalStorageManager()
 *      → concedido = File API POSIX direta (Downloads, Documents, etc.);
 *      recusado = modo app-private (getExternalFilesDir).
 *
 * O SAF tree picker (ACTION_OPEN_DOCUMENT_TREE e afins) foi REMOVIDO — mais
 * complexo e menos familiar (docs/SAF_EXCEPTION.md registra a decisão). A
 * superfície Java sobrevive PORQUE NativeActivity não reencaminha
 * onActivityResult nem lança intents de settings sem uma subclasse.
 */
public class VvActivity extends NativeActivity {
    // DEVE espelhar platform/StoragePerm.h (vv::storage::kReqAllFiles)
    static final int REQ_ALL_FILES = 4301;

    static {
        // F5.4 — CARREGAMENTO DA LIB NO JAVA (fix do UnsatisfiedLinkError do
        // RMX3624). O meta-data android.app.lib_name serve APENAS para o
        // framework decidir QUE lib abrir com dlopen(RTLD_LOCAL) — isso não
        // corre o JNI_OnLoad nem registra a lib no JVM. O System.loadLibrary
        // de verdade corre AQUI (inicialização da classe, antes de onCreate):
        // chama o JNI_OnLoad (RegisterNatives) e coloca a lib no mapa de
        // resolução — nativeRegisterActivity/nativeOnActivityResult passam a
        // resolver sempre, no onCreate e no onResume.
        System.loadLibrary("goni_vv");
    }

    // F5.3 — registo da activity no nativo (handshake invertido). A origem
    // ("onCreate"/"onResume") entra no engine.log para o log viewer do C33
    // mostrar a sequência completa do handshake.
    private static native void nativeRegisterActivity(
            VvActivity activity, String origin);

    private static native void nativeOnActivityResult(
            int requestCode, int resultCode, Uri uri, int flags);

    // F5.4 — Gestor de Projetos: entrega ao native o projeto escolhido no
    // ecrã inicial (URI da pasta SAF + nome) — o boot do android_main está
    // À ESPERA na fila ProjectSlot (timeout 3s). Chamado UMA vez, do
    // onCreate, após o handshake (a fila só faz sentido com a ponte viva).
    private static native void nativeOpenProject(String treeUri, String name);

    // 0.8.10 — IDENTIDADE DA BUILD: entrega BuildConfig + build_info.txt
    // (git/epoch/sha256 da .so, escrito pelo CI) ao native — vive nos crash
    // dumps (nome+header), no banner do boot log e no badge ANTIGO do viewer.
    private static native void nativeSetBuildInfo(String version, int versionCode,
                                                  String git, String soSha, long epoch);

    // 0.9.1 — IME DO SISTEMA: o InputConnection do EditText encaminha para
    // AQUI (fila ime:: no nativo; a engine consome por frame). text = texto
    // commitado (UTF-8); keyCode/action = tecla (DEL 67, ENTER 66, DPAD) —
    // só ACTION_DOWN é encaminhado (a engine não repete edges).
    private static native void nativeOnImeText(String text);
    private static native void nativeOnImeKey(int keyCode, int action);

    // lê UMA linha "chave=valor" do assets/build_info.txt (vazio se ausente)
    private String assetInfo(String key) {
        try (java.io.BufferedReader r = new java.io.BufferedReader(
                new java.io.InputStreamReader(getAssets().open("build_info.txt")))) {
            String line;
            while ((line = r.readLine()) != null) {
                if (line.startsWith(key + "=")) {
                    return line.substring(key.length() + 1);
                }
            }
        } catch (Throwable ignored) {
            // sem build_info.txt (dev/local) — a identidade fica "dev"
        }
        return "";
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        // 0.8.10 — identidade ANTES de tudo (o crash handler precisa dela
        // o mais cedo possível): BuildConfig + assets/build_info.txt do CI
        try {
            nativeSetBuildInfo(BuildConfig.VERSION_NAME, BuildConfig.VERSION_CODE,
                    assetInfo("git"), assetInfo("soSha256"),
                    Long.parseLong(assetInfo("epoch").isEmpty() ? "0" : assetInfo("epoch")));
        } catch (Throwable t) {
            Log.e("GONI", "java: nativeSetBuildInfo FALHOU (identidade = dev)", t);
        }
        Log.i("GONI", "java: onCreate → nativeRegisterActivity");
        try {
            nativeRegisterActivity(this, "onCreate");
        } catch (Throwable t) {
            // nunca crashar por causa da ponte — o diagnóstico está no logcat
            // e o nativo reporta o handshake pendente no boot do engine.log
            Log.e("GONI", "java: nativeRegisterActivity FALHOU (onCreate)", t);
        }

        // 0.9.1 — host do IME (1x1px, transparente, canto): criado UMA vez;
        // o IME só abre quando a ENGINE pede (imeShow) — nunca por conta do
        // foco (o NativeActivity não tem UI; sem isto nada o abre sozinho).
        initImeHost();

        // 0.9.3 (Problema 3, hotfix): memória no ARRANQUE do editor — uma
        // linha informativa (Debug.getMemoryInfo + heap Java); o dono compara
        // com o FinalizerWatchdog dos tombstones antigos. NUNCA crasha.
        try {
            final android.os.Debug.MemoryInfo mi =
                    new android.os.Debug.MemoryInfo();
            android.os.Debug.getMemoryInfo(mi);
            final Runtime rt = Runtime.getRuntime();
            Log.i("GONI", "editor: onCreate memoria — dalvikPss=" + mi.dalvikPss
                    + "KB nativePss=" + mi.nativePss + "KB totalPss="
                    + mi.getTotalPss() + "KB | heap java " + (rt.totalMemory() >> 10)
                    + "KB/" + (rt.maxMemory() >> 10) + "KB (livre "
                    + (rt.freeMemory() >> 10) + "KB)");
        } catch (Throwable t) {
            Log.w("GONI", "editor: Debug.getMemoryInfo indisponivel", t);
        }

        // F5.4 — projeto escolhido no Gestor de Projetos (extras do Intent).
        // SÓ chega aqui quem veio do gestor: VvProjects.launchEditor põe os
        // extras; o android_main consome a fila e monta o SafStorage.
        Intent it = getIntent();
        final String treeUri = it != null ? it.getStringExtra(VvProjects.EXTRA_PROJECT_URI) : null;
        final String name = it != null ? it.getStringExtra(VvProjects.EXTRA_PROJECT_NAME) : null;
        if (treeUri != null && !treeUri.isEmpty()) {
            Log.i("GONI", "java: onCreate → nativeOpenProject ('"
                    + ((name != null && !name.isEmpty()) ? name : "projeto") + "')");
            try {
                nativeOpenProject(treeUri,
                        (name != null && !name.isEmpty()) ? name : "projeto");
            } catch (Throwable t) {
                Log.e("GONI", "java: nativeOpenProject FALHOU (ponte?)", t);
            }
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        Log.i("GONI", "java: onResume → nativeRegisterActivity (reforço)");
        try {
            nativeRegisterActivity(this, "onResume");
        } catch (Throwable t) {
            Log.e("GONI", "java: nativeRegisterActivity FALHOU (onResume)", t);
        }
    }

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        Uri uri = (data != null) ? data.getData() : null;
        int flags = (data != null) ? data.getFlags() : 0;
        nativeOnActivityResult(requestCode, resultCode, uri, flags);
    }

    // F5.2 — abre a janela de permissões DO APP (All Files Access).
    // startActivityForResult (não startActivity): o volta-chega passa pelo
    // onActivityResult → o nativo re-verifica isExternalStorageManager.
    // Falha (action sem activity no aparelho) → resultado CANCELED imediato
    // (o nativo trata como "sem concessão", modo app-private).
    void openAllFilesSettings(int requestCode) {
        try {
            Intent i = new Intent(
                    Settings.ACTION_MANAGE_APP_ALL_FILES_ACCESS_PERMISSION,
                    Uri.parse("package:" + getPackageName()));
            startActivityForResult(i, requestCode);
        } catch (Exception e) {
            nativeOnActivityResult(requestCode, RESULT_CANCELED, null, 0);
        }
    }

    // 0.8.11 — ÁUDIO: o GRAVAR do workspace precisa do MICROFONE. Devolve
    // true se JÁ concedida (a gravação arranca logo); false = o DIALOGO do
    // sistema está aberto — o nativo mostra o toast "toque Gravar de novo"
    // e o dono volta a tocar depois de conceder (o mesmo contrato humano
    // do All Files: nada de loops, nada de arranques bloqueados). Chamado
    // por JNI no thread da engine (checkSelfPermission/requestPermissions
    // são de Context/Activity — thread-safe para perguntar; o diálogo é
    // lançado da activity). API 23+ (minSdk 24 ✓).
    boolean ensureMicPermission() {
        try {
            if (checkSelfPermission(android.Manifest.permission.RECORD_AUDIO)
                    == android.content.pm.PackageManager.PERMISSION_GRANTED) {
                return true;
            }
            requestPermissions(
                    new String[]{android.Manifest.permission.RECORD_AUDIO},
                    4301 /* kReqMic, resultado ignorado — o re-toque decide */);
            return false;
        } catch (Exception e) {
            Log.e("GONI", "java: ensureMicPermission FALHOU", e);
            return false;   // sem permissão = gravação desligada, resto segue
        }
    }

    // 0.8.12 — STAGING SEM /tmp: o cache dir da app (getCacheDir) — o único
    // sítio GUARANTIDO escrevível no Android sem permissões (o /tmp é
    // READ-ONLY no device, errno=30 — a causa exata da migração morta no
    // C33: "fileapi: mkdir falhou em '/tmp'" → "staging falhou"). Chamado
    // por JNI (stagingAbsPath do AssetConverter) no thread da engine.
    // 0.9.0 (spec I) — MODO IMERSIVO: esconde as barras do sistema (status +
    // nav) com immersive sticky; false = volta ao normal. Chamado por JNI a
    // partir do toggle de Settings (Geral → Imersivo). API 24+: os flags
    // SYSTEM_UI_FLAG_* (deprecados na 30 mas FUNCIONAIS até lá; a app mira
    // o C33 com Android 12/13 — sem WindowInsetsController necessário).
    // NOME: setImmersiveMode (0.9.0-fix CI) — Activity.setImmersive(boolean)
    // é FINAL na plataforma e não pode ser override (javac morria).
    public void setImmersiveMode(boolean on) {
        try {
            final View decor = getWindow().getDecorView();
            runOnUiThread(() -> {
                if (on) {
                    decor.setSystemUiVisibility(
                            View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY
                            | View.SYSTEM_UI_FLAG_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                            | View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
                    Log.i("GONI", "java: imersivo ON (immersive sticky)");
                } else {
                    decor.setSystemUiVisibility(View.SYSTEM_UI_FLAG_VISIBLE);
                    Log.i("GONI", "java: imersivo OFF");
                }
            });
        } catch (Exception e) {
            Log.e("GONI", "java: setImmersive FALHOU", e);
        }
    }

    String cacheDirPath() {
        try {
            final java.io.File dir = getCacheDir();
            return dir != null ? dir.getAbsolutePath() : "";
        } catch (Exception e) {
            Log.e("GONI", "java: cacheDirPath FALHOU", e);
            return "";   // o nativo dá o erro LEGÍVEL (nunca /tmp)
        }
    }

    // ================= 0.9.1 — ORIENTAÇÃO + IME DO SISTEMA ==================
    //
    // POLÍTICA (0.9.1 §1): janelas de TEXTO PESADO (editor de script e
    // futuros editores de código) pedem PORTRAIT; ao fechar, LANDSCAPE.
    // O ESTADO vive no nativo (platform/ImeQueue) — aqui é só o executor
    // (setRequestedOrientation precisa da Activity). O frame do device
    // rotaciona → surfaceChanged → INIT_WINDOW (o lifecycle 0.6.7 re-upa
    // tudo — a regressão "sem glifos brancos" é testada na suíte).
    public void setOrientation(boolean portrait) {
        runOnUiThread(() -> {
            try {
                setRequestedOrientation(portrait
                        ? ActivityInfo.SCREEN_ORIENTATION_SENSOR_PORTRAIT
                        : ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
                Log.i("GONI", "java: orientação pedida = "
                        + (portrait ? "portrait" : "landscape"));
            } catch (Throwable t) {
                Log.e("GONI", "java: setOrientation FALHOU", t);
            }
        });
    }

    // o HOST do IME: EditText 1x1 TRANSPARENTE. O InputConnection é
    // encaminhado ao NATIVO e o EditText NUNCA acumula texto (cada commit
    // devolve true SEM chamar super) — o buffer da janela é da engine, não
    // da Java (uma única fonte de verdade, zero duplicação). Isolado AQUI
    // (🔶: mudar o contrato do IME mexe SÓ nesta classe + ImeQueue.h).
    private EditText imeHost;

    private void initImeHost() {
        if (imeHost != null) return;
        imeHost = new EditText(this) {
            @Override
            public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
                final InputConnection base = super.onCreateInputConnection(outAttrs);
                outAttrs.inputType = InputType.TYPE_CLASS_TEXT
                        | InputType.TYPE_TEXT_FLAG_MULTI_LINE;
                outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_EXTRACT_UI
                        | EditorInfo.IME_ACTION_NONE;
                return new InputConnectionWrapper(base, true) {
                    @Override
                    public boolean commitText(CharSequence text, int newCursorPosition) {
                        try {
                            nativeOnImeText(text != null ? text.toString() : "");
                        } catch (Throwable t) {
                            Log.e("GONI", "ime: nativeOnImeText FALHOU", t);
                        }
                        return true;   // SEM super: o host fica VAZIO
                    }

                    @Override
                    public boolean deleteSurroundingText(int beforeLength,
                                                         int afterLength) {
                        // teclados suaves mandam DEL por AQUI (composição)
                        for (int i = 0; i < beforeLength; i++) {
                            nativeOnImeKey(KeyEvent.KEYCODE_DEL, 1);
                        }
                        return true;
                    }

                    @Override
                    public boolean sendKeyEvent(KeyEvent event) {
                        if (event.getAction() == KeyEvent.ACTION_DOWN) {
                            nativeOnImeKey(event.getKeyCode(), 0);
                        }
                        return true;   // SEM super: o host não re-age
                    }

                    @Override
                    public boolean performEditorAction(int actionCode) {
                        // GBoard/teclados suaves: ENTER do IME vem por aqui
                        nativeOnImeKey(KeyEvent.KEYCODE_ENTER, 1);
                        return true;
                    }
                };
            }
        };
        imeHost.setAlpha(0f);
        imeHost.setBackgroundColor(Color.TRANSPARENT);
        imeHost.setCursorVisible(false);
        final FrameLayout.LayoutParams lp =
                new FrameLayout.LayoutParams(1, 1);
        addContentView(imeHost, lp);
        Log.i("GONI", "java: host do IME criado (1x1 transparente)");
    }

    // SHOW/HIDE POR CONTA DA ENGINE (0.9.1 §2): o chamador nativo é o
    // frame da engine (janela de texto aberta/fechada) — o IME nunca abre
    // sozinho nem fica preso quando a janela fecha.
    public void imeShow() {
        runOnUiThread(() -> {
            try {
                initImeHost();
                imeHost.requestFocus();
                final InputMethodManager imm = (InputMethodManager) getSystemService(
                        android.content.Context.INPUT_METHOD_SERVICE);
                final boolean ok = imm != null && imm.showSoftInput(
                        imeHost, InputMethodManager.SHOW_IMPLICIT);
                Log.i("GONI", "java: imeShow pedido ("
                        + (ok ? "aceite" : "rejeitado") + ")");
            } catch (Throwable t) {
                Log.e("GONI", "java: imeShow FALHOU", t);
            }
        });
    }

    public void imeHide() {
        runOnUiThread(() -> {
            try {
                if (imeHost != null) {
                    final InputMethodManager imm = (InputMethodManager) getSystemService(
                            android.content.Context.INPUT_METHOD_SERVICE);
                    if (imm != null) {
                        imm.hideSoftInputFromWindow(
                                imeHost.getWindowToken(), 0);
                    }
                    imeHost.clearFocus();
                }
                Log.i("GONI", "java: imeHide pedido");
            } catch (Throwable t) {
                Log.e("GONI", "java: imeHide FALHOU", t);
            }
        });
    }

    // F5.1-hotfix (parte 1.4) — EXPORT DOS LOGS para o Downloads PÚBLICO.
    // Copia TODOS os ficheiros de getExternalFilesDir("logs") (engine.log,
    // rotações .1/.2 e crash-*.dump) para Downloads/<relPath>/ via MediaStore
    // — API 29+ (C33 é Android 12), SEM permissão WRITE_EXTERNAL_STORAGE.
    // Chamado POR JNI do thread nativo (ContentResolver é thread-safe; não
    // toca na UI). relPath vem do nativo (vv::elog::kDownloadsRelPath =
    // "Download/GOneVV/logs") — constante única aferida no CI.
    // Devolve o nº de ficheiros copiados; negativo = falha (-1 excepção,
    // -2 API < 29, -3 sem ficheiros, -4 sem storage externo).
    int exportLogsToDownloads(String relPath) {
        try {
            if (Build.VERSION.SDK_INT < 29) return -2;
            if (relPath == null || relPath.isEmpty()) return -1;
            java.io.File dir = getExternalFilesDir("logs");
            java.io.File[] files = (dir != null) ? dir.listFiles() : null;
            if (files == null || files.length == 0) return -3;
            int count = 0;
            for (java.io.File f : files) {
                if (!f.isFile()) continue;
                // export repetido SUBSTITUI (query+delete pelo mesmo nome)
                deleteDownload(relPath, f.getName());
                ContentValues cv = new ContentValues();
                cv.put(MediaStore.MediaColumns.DISPLAY_NAME, f.getName());
                cv.put(MediaStore.MediaColumns.MIME_TYPE, "text/plain");
                cv.put(MediaStore.MediaColumns.RELATIVE_PATH, relPath);
                Uri uri = getContentResolver().insert(
                        MediaStore.Downloads.EXTERNAL_CONTENT_URI, cv);
                if (uri == null) continue;
                java.io.InputStream in = new java.io.FileInputStream(f);
                java.io.OutputStream os = getContentResolver().openOutputStream(uri);
                if (os == null) { in.close(); continue; }
                byte[] buf = new byte[16 * 1024];
                int n;
                while ((n = in.read(buf)) > 0) os.write(buf, 0, n);
                os.flush();
                os.close();
                in.close();
                count++;
            }
            return count;
        } catch (Exception e) {
            return -1;
        }
    }

    private void deleteDownload(String relPath, String name) {
        try {
            String sel = MediaStore.MediaColumns.RELATIVE_PATH + "=? AND " +
                         MediaStore.MediaColumns.DISPLAY_NAME + "=?";
            String[] args = { relPath, name };
            getContentResolver().delete(
                    MediaStore.Downloads.EXTERNAL_CONTENT_URI, sel, args);
        } catch (Exception ignored) {
        }
    }

    // ================= F5.4 — ponte SAF (chamados PELO NATIVO) ================
    //
    // Métodos de INSTÂNCIA (o native tem o GlobalRef desta activity):
    // recebem/devolvem URIs de documento como String; o ContentResolver/
    // DocumentsContract é daqui. Erros devolvem SENTINELA (-1/null/false)
    // SEM excepção propagada — o native loga a causa (mensagens honestas).
    //
    // Estes métodos servem APENAS o I/O do projeto (pasta escolhida no
    // gestor). O import/export de assets soltos continua pelo fluxo All
    // Files Access (openAllFilesSettings acima) — as duas coisas coexistem.

    /** tree URI → URI de DOCUMENTO da raiz (o único salto tree→doc) */
    String bridgeRootDoc(String treeUri) {
        try {
            Uri tree = Uri.parse(treeUri);
            Uri doc = DocumentsContract.buildDocumentUriUsingTree(tree,
                    DocumentsContract.getTreeDocumentId(tree));
            return doc != null ? doc.toString() : null;
        } catch (Exception e) {
            Log.e("GONI", "bridgeRootDoc FALHOU (" + treeUri + ")", e);
            return null;
        }
    }

    /** abre um documento como fd ("r"/"w"); detachFd = o fd passa a ser do
     *  native (fechar é o que persiste no provider); -1 = falha */
    int bridgeOpenFd(String docUri, String mode) {
        try {
            android.os.ParcelFileDescriptor pfd =
                    getContentResolver().openFileDescriptor(
                            Uri.parse(docUri),
                            (mode != null && !mode.isEmpty()) ? mode : "r");
            if (pfd == null) {
                return -1;
            }
            return pfd.detachFd();
        } catch (Exception e) {
            Log.e("GONI", "bridgeOpenFd FALHOU (" + docUri + ", " + mode + ")", e);
            return -1;
        }
    }

    /** filhos de um diretório — flat [uri0,name0,mime0,uri1,…]; null = falha */
    String[] bridgeList(String dirDocUri) {
        try {
            Uri dir = Uri.parse(dirDocUri);
            Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(dir,
                    DocumentsContract.getDocumentId(dir));
            java.util.ArrayList<String> out = new java.util.ArrayList<>();
            Cursor c = getContentResolver().query(children,
                    new String[]{
                            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                            DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                            DocumentsContract.Document.COLUMN_MIME_TYPE},
                    null, null, null);
            if (c == null) {
                return null;
            }
            while (c.moveToNext()) {
                Uri child = DocumentsContract.buildDocumentUriUsingTree(
                        dir, c.getString(0));
                out.add(child != null ? child.toString() : "");
                out.add(c.getString(1));
                out.add(c.getString(2));
            }
            c.close();
            return out.toArray(new String[0]);
        } catch (Exception e) {
            Log.e("GONI", "bridgeList FALHOU (" + dirDocUri + ")", e);
            return null;
        }
    }

    /** cria documento (ficheiro/pasta) num pai; devolve o URI ou null */
    String bridgeCreate(String parentDocUri, String mime, String name) {
        try {
            Uri created = DocumentsContract.createDocument(getContentResolver(),
                    Uri.parse(parentDocUri), mime, name);
            return created != null ? created.toString() : null;
        } catch (Exception e) {
            Log.e("GONI", "bridgeCreate FALHOU (" + parentDocUri + ", "
                    + mime + ", " + name + ")", e);
            return null;
        }
    }

    /**
     * F5.4-hotfix (bug "main.goni (1).json / project.goni (2) em TODO boot") —
     * pesquisa EXATA por displayName na pasta-alvo, com query FRESCA ao
     * provider (DocumentsContract child query, uma passagem). NÃO é o
     * bridgeList/listDir da primeira tentativa: é a verificação dedicada que
     * o native exige ANTES de qualquer createDocument.
     *
     * Contrato (tri-estado — o native distingue os três):
     *   URI não-vazio → documento EXISTE (reabrir este URI, escrever "wt");
     *   "" (vazia)    → a query correu e NÃO achou — ausência CONFIRMADA
     *                   (só aqui o native pode chamar bridgeCreate);
     *   null          → a query FALHOU (exceção/recusa) — existência
     *                   INDECIDIDA; o native NUNCA cria por cima de "não sei".
     *
     * Compat (cura dos projetos criados pela 0.6.4): alguns providers
     * renomeiam o displayName no createDocument (extensão canónica do mime
     * divergente — "x.goni" + application/json → "x.goni.json"). Se o nome
     * exato não existir, aceita-se o mesmo nome com ".json" acrescentado —
     * o projeto 0.6.4 abre sem criar novos duplicados. As cópias
     * "nome (1)", "nome (2)"… NUNCA são escolhidas (ambíguas — lixo a
     * apagar manualmente pelo utilizador, sem risco de escolha errada).
     */
    String bridgeFindFile(String dirDocUri, String name) {
        try {
            Uri dir = Uri.parse(dirDocUri);
            Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(
                    dir, DocumentsContract.getDocumentId(dir));
            Cursor c = getContentResolver().query(children,
                    new String[]{
                            DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                            DocumentsContract.Document.COLUMN_DISPLAY_NAME},
                    null, null, null);
            if (c == null) {
                return null;   // query recusada → indecidido
            }
            String exact = null;
            String compat = null;
            final String compatName = name + ".json";
            while (c.moveToNext()) {
                String n = c.getString(1);
                if (n == null) {
                    continue;
                }
                if (n.equals(name)) {
                    exact = DocumentsContract.buildDocumentUriUsingTree(
                            dir, c.getString(0)).toString();
                    break;   // exato ganha sempre — pode sair cedo
                }
                if (compat == null && n.equals(compatName)) {
                    compat = DocumentsContract.buildDocumentUriUsingTree(
                            dir, c.getString(0)).toString();
                }
            }
            c.close();
            String out = (exact != null) ? exact : compat;
            return (out != null) ? out : "";   // "" = ausência confirmada
        } catch (Exception e) {
            Log.e("GONI", "bridgeFindFile FALHOU (" + dirDocUri + ", " + name + ")", e);
            return null;   // exceção → indecidido
        }
    }

    /** apaga documento; false = recusado/ausente */
    boolean bridgeDelete(String docUri) {
        try {
            return DocumentsContract.deleteDocument(getContentResolver(),
                    Uri.parse(docUri));
        } catch (Exception e) {
            Log.e("GONI", "bridgeDelete FALHOU (" + docUri + ")", e);
            return false;
        }
    }

    /**
     * 0.6.7 — "Sair para projetos": termina ESTA activity e volta ao Gestor
     * de Projetos (ProjectManagerActivity está por baixo na back stack —
     * é o LAUNCHER, lançou o editor com startActivity). O processo NÃO
     * morre: os recursos GL são libertados pelo APP_CMD_TERM_WINDOW do
     * nativo (lifecycle 0.6.7) e reentrar no editor arranca um novo
     * android_main com contexto EGL novo (re-upload de tudo — sem os
     * "cubinhos"). Chamado POR JNI do thread da engine; o finish() é
     * postado para a UI thread (higiene de threading Android).
     */
    void bridgeFinish() {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                finish();
            }
        });
    }
}
