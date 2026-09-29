package vv.goni;

import android.app.NativeActivity;
import android.content.Intent;
import android.net.Uri;

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
}
