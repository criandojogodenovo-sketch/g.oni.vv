package vv.goni;

import android.content.ContentResolver;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;

/**
 * F5.1-C — I/O do SAF (DocumentFile feito à mão sobre DocumentsContract,
 * para NÃO introduzir a dependência androidx.documentfile). Chamado
 * EXCLUSIVAMENTE pelo nativo (platform/SafIoJni.cpp). Toda a operação é
 * defensiva: falha → false/null (o nativo mapeia para fallback).
 *
 * `treeUri` = URI da PASTA concedida (persistida); `rel` = caminho relativo
 * com separador '/' (a validação de traversal vive no nativo — validRelPath).
 */
public final class SafIo {
    private SafIo() {}

    // -------- helpers de resolução -------------------------------------------

    /** documentId da RAIZ da pasta concedida. */
    private static String rootDocId(ContentResolver cr, Uri treeUri) {
        return DocumentsContract.getTreeDocumentId(treeUri);
    }

    /** URI do documento da pasta treeUri (a própria raiz). */
    private static Uri rootDocUri(ContentResolver cr, Uri treeUri) {
        return DocumentsContract.buildDocumentUriUsingTree(
                treeUri, rootDocId(cr, treeUri));
    }

    private static String docName(Cursor c) {
        final int idx = c.getColumnIndex(DocumentsContract.Document.COLUMN_DISPLAY_NAME);
        return idx >= 0 ? c.getString(idx) : null;
    }

    private static String docMime(Cursor c) {
        final int idx = c.getColumnIndex(DocumentsContract.Document.COLUMN_MIME_TYPE);
        return idx >= 0 ? c.getString(idx) : null;
    }

    private static Uri childUri(ContentResolver cr, Uri treeUri, String parentId, String name) {
        // procura o filho por nome (não há lookup direto por caminho em SAF)
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(
                treeUri, parentId);
        try (Cursor c = cr.query(children,
                new String[]{DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                             DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                             DocumentsContract.Document.COLUMN_MIME_TYPE},
                null, null, null)) {
            if (c != null) {
                while (c.moveToNext()) {
                    if (name.equals(docName(c))) {
                        String id = c.getString(0);
                        return DocumentsContract.buildDocumentUriUsingTree(treeUri, id);
                    }
                }
            }
        } catch (Exception ignored) {
        }
        return null;
    }

    /** URI do documento rel, null se não existe. */
    private static Uri resolve(ContentResolver cr, Uri treeUri, String rel) {
        String parentId = rootDocId(cr, treeUri);
        Uri doc = null;
        String[] segs = rel.split("/");
        for (int i = 0; i < segs.length; ++i) {
            doc = childUri(cr, treeUri, parentId, segs[i]);
            if (doc == null) return null;
            try (Cursor c = cr.query(doc,
                    new String[]{DocumentsContract.Document.COLUMN_DOCUMENT_ID},
                    null, null, null)) {
                if (c == null || !c.moveToFirst()) return null;
                parentId = c.getString(0);
            } catch (Exception e) {
                return null;
            }
        }
        return doc;
    }

    // -------- API chamada do nativo ------------------------------------------

    public static boolean ioExists(Context ctx, String treeUri, String rel) {
        try {
            ContentResolver cr = ctx.getContentResolver();
            return resolve(cr, Uri.parse(treeUri), rel) != null;
        } catch (Exception e) {
            return false;
        }
    }

    public static boolean ioMakeDirs(Context ctx, String treeUri, String relDir) {
        try {
            ContentResolver cr = ctx.getContentResolver();
            Uri base = Uri.parse(treeUri);
            String parentId = rootDocId(cr, base);
            for (String seg : relDir.split("/")) {
                Uri found = childUri(cr, base, parentId, seg);
                if (found == null) {
                    Uri parentDoc = DocumentsContract.buildDocumentUriUsingTree(base, parentId);
                    Uri created = DocumentsContract.createDocument(cr, parentDoc,
                            DocumentsContract.Document.MIME_TYPE_DIR, seg);
                    if (created == null) return false;
                    try (Cursor c = cr.query(created,
                            new String[]{DocumentsContract.Document.COLUMN_DOCUMENT_ID},
                            null, null, null)) {
                        if (c == null || !c.moveToFirst()) return false;
                        parentId = c.getString(0);
                    }
                } else {
                    try (Cursor c = cr.query(found,
                            new String[]{DocumentsContract.Document.COLUMN_DOCUMENT_ID},
                            null, null, null)) {
                        if (c == null || !c.moveToFirst()) return false;
                        parentId = c.getString(0);
                    }
                }
            }
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    public static String[] ioList(Context ctx, String treeUri, String relDir) {
        try {
            ContentResolver cr = ctx.getContentResolver();
            Uri base = Uri.parse(treeUri);
            java.util.ArrayList<String> names = new java.util.ArrayList<>();
            Uri parentDoc;
            if (relDir.isEmpty()) {
                parentDoc = rootDocUri(cr, base);
            } else {
                Uri dir = resolve(cr, base, relDir);
                if (dir == null) return null;
                parentDoc = dir;
            }
            String parentId = DocumentsContract.getDocumentId(parentDoc);
            Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(base, parentId);
            try (Cursor c = cr.query(children,
                    new String[]{DocumentsContract.Document.COLUMN_DISPLAY_NAME,
                                 DocumentsContract.Document.COLUMN_MIME_TYPE},
                    null, null, null)) {
                if (c == null) return null;
                while (c.moveToNext()) {
                    String mime = docMime(c);
                    if (!DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                        String n = docName(c);
                        if (n != null) names.add(n);
                    }
                }
            }
            java.util.Collections.sort(names);
            return names.toArray(new String[0]);
        } catch (Exception e) {
            return null;
        }
    }

    public static byte[] ioRead(Context ctx, String treeUri, String rel) {
        try {
            ContentResolver cr = ctx.getContentResolver();
            Uri doc = resolve(cr, Uri.parse(treeUri), rel);
            if (doc == null) return null;
            java.io.InputStream in = cr.openInputStream(doc);
            if (in == null) return null;
            java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
            byte[] buf = new byte[16 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            in.close();
            return out.toByteArray();
        } catch (Exception e) {
            return null;
        }
    }

    public static boolean ioWrite(Context ctx, String treeUri, String rel, byte[] data) {
        try {
            ContentResolver cr = ctx.getContentResolver();
            Uri base = Uri.parse(treeUri);
            String relDir = "";
            String name = rel;
            int slash = rel.lastIndexOf('/');
            if (slash >= 0) {
                relDir = rel.substring(0, slash);
                name = rel.substring(slash + 1);
            }
            if (!relDir.isEmpty() && !ioMakeDirs(ctx, treeUri, relDir)) return false;
            Uri dirDoc = relDir.isEmpty()
                    ? rootDocUri(cr, base) : resolve(cr, base, relDir);
            if (dirDoc == null) return false;

            // overwrite: SAF cria duplicado "name (1)" se não apagar antes
            Uri existing = childUri(cr, base, DocumentsContract.getDocumentId(dirDoc), name);
            if (existing != null) {
                try {
                    DocumentsContract.deleteDocument(cr, existing);
                } catch (Exception ignored) {
                }
            }
            Uri created = DocumentsContract.createDocument(cr, dirDoc,
                    "application/octet-stream", name);
            if (created == null) return false;
            java.io.OutputStream os = cr.openOutputStream(created);
            if (os == null) return false;
            os.write(data);
            os.flush();
            os.close();
            return true;
        } catch (Exception e) {
            return false;
        }
    }

    public static boolean ioDelete(Context ctx, String treeUri, String rel) {
        try {
            ContentResolver cr = ctx.getContentResolver();
            Uri doc = resolve(cr, Uri.parse(treeUri), rel);
            if (doc == null) return false;
            DocumentsContract.deleteDocument(cr, doc);
            return true;
        } catch (Exception e) {
            return false;
        }
    }
    // -------- documento ÚNICO (import/export de ficheiros) -------------------

    /** Nome de exibição de um documento (URI única), "" se falhar. */
    public static String ioDisplayName(Context ctx, String uri) {
        try {
            ContentResolver cr = ctx.getContentResolver();
            Uri doc = Uri.parse(uri);
            try (Cursor c = cr.query(doc,
                    new String[]{DocumentsContract.Document.COLUMN_DISPLAY_NAME},
                    null, null, null)) {
                if (c != null && c.moveToFirst()) {
                    String n = docName(c);
                    return n != null ? n : "";
                }
            }
            return "";
        } catch (Exception e) {
            return "";
        }
    }

    /** Lê um documento único (URI de ficheiro). null se falhar. */
    public static byte[] ioReadSingle(Context ctx, String uri) {
        try {
            ContentResolver cr = ctx.getContentResolver();
            java.io.InputStream in = cr.openInputStream(Uri.parse(uri));
            if (in == null) return null;
            java.io.ByteArrayOutputStream out = new java.io.ByteArrayOutputStream();
            byte[] buf = new byte[16 * 1024];
            int n;
            while ((n = in.read(buf)) > 0) out.write(buf, 0, n);
            in.close();
            return out.toByteArray();
        } catch (Exception e) {
            return null;
        }
    }

    /** Escreve um documento único (URI de ACTION_CREATE_DOCUMENT). */
    public static boolean ioWriteSingle(Context ctx, String uri, byte[] data) {
        try {
            ContentResolver cr = ctx.getContentResolver();
            java.io.OutputStream os = cr.openOutputStream(Uri.parse(uri));
            if (os == null) return false;
            os.write(data);
            os.flush();
            os.close();
            return true;
        } catch (Exception e) {
            return false;
        }
    }
}
