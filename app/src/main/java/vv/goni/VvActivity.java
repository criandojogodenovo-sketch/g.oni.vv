package vv.goni;

import android.app.NativeActivity;
import android.content.ContentValues;
import android.content.Intent;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.provider.MediaStore;

/**
 * F5.1-C — EXCEÇÃO DOCUMENTADA à regra "zero Java" (docs/SAF_EXCEPTION.md).
 *
 * O SAF (ACTION_OPEN_DOCUMENT_TREE / ACTION_OPEN_DOCUMENT /
 * ACTION_CREATE_DOCUMENT) devolve o resultado por
 * Activity.onActivityResult — e NativeActivity NÃO reencaminha esse
 * resultado ao nativo. Sem esta subclasse não é possível escolher pastas
 * do armazenamento. Superfície Java mínima: abrir os pickers do sistema e
 * reencaminhar (request, result, uri, flags) por JNI.
 */
public class VvActivity extends NativeActivity {
    // DEVE espelhar platform/Saf.h (vv::saf::kReq*)
    static final int REQ_PICK_TREE = 4201;
    static final int REQ_IMPORT = 4202;
    static final int REQ_EXPORT = 4203;

    private static native void nativeOnActivityResult(
            int requestCode, int resultCode, Uri uri, int flags);

    @Override
    protected void onActivityResult(int requestCode, int resultCode, Intent data) {
        super.onActivityResult(requestCode, resultCode, data);
        Uri uri = (data != null) ? data.getData() : null;
        int flags = (data != null) ? data.getFlags() : 0;

        // persistência da permissão: SEM takePersistableUriPermission a
        // concessão morre com o processo (o objetivo é a pasta sobreviver
        // entre arranques do editor). Falha silenciosa — o nativo trata
        // URI sem persistência como utilizável só nesta sessão.
        if (uri != null
                && (flags & Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION) != 0) {
            try {
                getContentResolver().takePersistableUriPermission(uri,
                        Intent.FLAG_GRANT_READ_URI_PERMISSION |
                        Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            } catch (SecurityException ignored) {
            }
        }
        nativeOnActivityResult(requestCode, resultCode, uri, flags);
    }

    void openTreePicker(int requestCode) {
        try {
            startActivityForResult(
                    new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE), requestCode);
        } catch (Exception e) {
            nativeOnActivityResult(requestCode, RESULT_CANCELED, null, 0);
        }
    }

    void openImportPicker(int requestCode) {
        try {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("*/*");   // obj/gltf/glb/png — decisão por extensão no nativo
            startActivityForResult(i, requestCode);
        } catch (Exception e) {
            nativeOnActivityResult(requestCode, RESULT_CANCELED, null, 0);
        }
    }

    void openExportPicker(int requestCode, String suggestedName) {
        try {
            Intent i = new Intent(Intent.ACTION_CREATE_DOCUMENT);
            i.addCategory(Intent.CATEGORY_OPENABLE);
            i.setType("application/octet-stream");
            if (suggestedName != null) {
                i.putExtra(Intent.EXTRA_TITLE, suggestedName);
            }
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
