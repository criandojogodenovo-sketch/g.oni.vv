package vv.goni;

import android.app.NativeActivity;
import android.content.ContentValues;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;
import android.provider.Settings;

/**
 * F5.2 — ponte Java mínima do ARMAZENAMENTO (sucessora da exceção SAF).
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

    private static native void nativeOnActivityResult(
            int requestCode, int resultCode, Uri uri, int flags);

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
}
