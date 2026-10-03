package vv.goni;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.database.Cursor;
import android.net.Uri;
import android.provider.DocumentsContract;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONObject;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.List;

/**
 * F5.4 — lista de projetos do Gestor (estilo Godot) + criação da estrutura.
 *
 * A LISTA vive em projects.json dentro de getExternalFilesDir (app-private,
 * SEM permissões): a lista em si não depende do handshake nem de All Files
 * Access — é Java puro a ler/escrever um ficheiro pequeno.
 *
 * CADA projeto aponta para a PRÓPRIA pasta escolhida no SAF
 * (ACTION_OPEN_DOCUMENT_TREE + takePersistableUriPermission): pastas
 * diferentes, projetos que nunca se misturam. O URI persistido continua
 * válido entre arranques (permissão persistente).
 *
 * A estrutura do projeto (scenes/, meshes/, textures/) é criada AQUI na
 * escolha (o que já existir é respeitado); o project.goni é escrito pelo
 * native no primeiro open (Project::openOrCreate — fonte única do formato).
 */
public final class VvProjects {
    private static final String TAG = "GONI";

    // extras do Intent que abre o editor COM um projeto (consumidos pela
    // VvActivity e reencaminhados ao native por nativeOpenProject)
    public static final String EXTRA_PROJECT_URI = "goni.project.uri";
    public static final String EXTRA_PROJECT_NAME = "goni.project.name";

    /**
     * entrada da lista: nome + URI da pasta SAF + data de criação +
     * data da ÚLTIMA EDIÇÃO (0.7.6 — a lista mostra nome + data da última
     * edição; editedAt atualiza a cada abertura do projeto, ausente no
     * projects.json antigo → cai no createdAt).
     */
    public static final class Entry {
        // 0.9.0: name deixa de ser final (o RENOMEAR escreve-o; o URI da
        // pasta e a data de criação continuam imutáveis — identidade)
        public String name;
        public final String uri;
        public final long createdAt;
        public long editedAt;

        public Entry(String name, String uri, long createdAt) {
            this.name = name;
            this.uri = uri;
            this.createdAt = createdAt;
            this.editedAt = createdAt;
        }

        JSONObject toJson() throws Exception {
            JSONObject o = new JSONObject();
            o.put("name", name);
            o.put("uri", uri);
            o.put("createdAt", createdAt);
            o.put("editedAt", editedAt > 0L ? editedAt : createdAt);
            return o;
        }

        static Entry fromJson(JSONObject o) {
            Entry e = new Entry(o.optString("name", "projeto"),
                                o.optString("uri", ""),
                                o.optLong("createdAt", 0L));
            e.editedAt = o.optLong("editedAt", e.createdAt);
            return e;
        }
    }

    private VvProjects() {
    }

    static File file(Context c) {
        File dir = c.getExternalFilesDir(null);
        return new File(dir != null ? dir : c.getFilesDir(), "projects.json");
    }

    /** carrega a lista (defensivo: ficheiro ausente/corrompido → lista vazia) */
    public static List<Entry> load(Context c) {
        List<Entry> out = new ArrayList<>();
        try {
            File f = file(c);
            if (!f.isFile()) {
                return out;
            }
            byte[] buf = new byte[(int) Math.max(1, f.length())];
            FileInputStream in = new FileInputStream(f);
            int n = in.read(buf);
            in.close();
            if (n <= 0) {
                return out;
            }
            JSONArray arr = new JSONArray(new String(buf, 0, n, "UTF-8"));
            for (int i = 0; i < arr.length(); i++) {
                Entry e = Entry.fromJson(arr.getJSONObject(i));
                if (e.uri != null && !e.uri.isEmpty()) {
                    out.add(e);
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "projects.json ilegível — lista vazia", e);
        }
        return out;
    }

    public static void save(Context c, List<Entry> list) {
        try {
            JSONArray arr = new JSONArray();
            for (Entry e : list) {
                arr.put(e.toJson());
            }
            FileOutputStream os = new FileOutputStream(file(c));
            os.write(arr.toString().getBytes("UTF-8"));
            os.close();
        } catch (Exception e) {
            Log.e(TAG, "projects.json não gravou", e);
        }
    }

    /** índice do projeto com este URI (duplicados não entram na lista) */
    public static int indexOfUri(List<Entry> list, String uri) {
        for (int i = 0; i < list.size(); i++) {
            if (uri != null && uri.equals(list.get(i).uri)) {
                return i;
            }
        }
        return -1;
    }

    /** cria scenes/ meshes/ textures/ na pasta escolhida (respeita os que já existem) */
    public static void createStructure(Context c, Uri treeUri) {
        try {
            Uri root = DocumentsContract.buildDocumentUriUsingTree(treeUri,
                    DocumentsContract.getTreeDocumentId(treeUri));
            String[] dirs = {"scenes", "meshes", "textures"};
            for (String dir : dirs) {
                if (childExists(c, root, dir)) {
                    continue;
                }
                try {
                    DocumentsContract.createDocument(c.getContentResolver(),
                            root,
                            DocumentsContract.Document.MIME_TYPE_DIR,
                            dir);
                    Log.i(TAG, "projetos: pasta criada na escolha: " + dir);
                } catch (Exception e) {
                    Log.e(TAG, "projetos: criar " + dir + " falhou", e);
                }
            }
        } catch (Exception e) {
            Log.e(TAG, "projetos: createStructure FALHOU", e);
        }
    }

    private static boolean childExists(Context c, Uri parentDocUri, String name) {
        try {
            Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(
                    parentDocUri, DocumentsContract.getDocumentId(parentDocUri));
            Cursor cur = c.getContentResolver().query(children,
                    new String[]{DocumentsContract.Document.COLUMN_DISPLAY_NAME},
                    null, null, null);
            if (cur == null) {
                return false;
            }
            boolean found = false;
            while (cur.moveToNext()) {
                if (name.equals(cur.getString(0))) {
                    found = true;
                    break;
                }
            }
            cur.close();
            return found;
        } catch (Exception e) {
            return false;
        }
    }

    /** nome da pasta escolhida (best-effort — preenche o diálogo) */
    public static String folderDisplayName(Context c, Uri treeUri) {
        Cursor cur = null;
        try {
            Uri doc = DocumentsContract.buildDocumentUriUsingTree(treeUri,
                    DocumentsContract.getTreeDocumentId(treeUri));
            cur = c.getContentResolver().query(doc,
                    new String[]{DocumentsContract.Document.COLUMN_DISPLAY_NAME},
                    null, null, null);
            if (cur != null && cur.moveToFirst()) {
                String n = cur.getString(0);
                if (n != null && !n.isEmpty()) {
                    return n;
                }
            }
        } catch (Exception ignored) {
        } finally {
            if (cur != null) {
                cur.close();
            }
        }
        return "projeto";
    }

    /**
     * abre o editor COM este projeto (extras → nativeOpenProject no boot).
     * 0.7.6: marca a data da ÚLTIMA EDIÇÃO (a lista mostra nome + data —
     * editar/abrir refresca a linha sem tocar na pasta do projeto).
     */
    public static void launchEditor(Activity a, Entry e) {
        List<Entry> ps = load(a);
        int i = indexOfUri(ps, e.uri);
        if (i >= 0) {
            ps.get(i).editedAt = System.currentTimeMillis();
            save(a, ps);
        }
        Intent it = new Intent(a, VvActivity.class);
        it.putExtra(EXTRA_PROJECT_URI, e.uri);
        it.putExtra(EXTRA_PROJECT_NAME, e.name);
        a.startActivity(it);
    }

    // ---- 0.9.0 (spec F/L): RENOMEAR · DUPLICAR · THUMBNAIL · EM FALTA ----

    /**
     * 0.6.7 — APAGA o projeto de verdade: a PASTA escolhida no SAF é
     * removida via File API (DocumentsContract.deleteDocument no URI de
     * documento da raiz — a mesma File API de escrita que criou a
     * estrutura) e a entrada sai da lista (projects.json).
     *
     * CHAMADO SÓ APÓS o diálogo de confirmação explícito do
     * ProjectManagerActivity ("Apagar projeto X? Não pode ser desfeito") —
     * este método NÃO pergunta nada.
     *
     * Devolve true se a pasta foi apagada (a entrada sai da lista mesmo se
     * a pasta já não existir — sem pasta não há projeto). A permissão
     * persistente do URI é libertada no ProjectManagerActivity.
     */
    public static boolean deleteProject(Context c, Entry e) {
        // 1) apagar a PASTA (DocumentContract = File API do SAF; o URI de
        //    árvore vira o URI de documento da raiz — o mesmo salto do
        //    createStructure)
        boolean folderGone = false;
        try {
            Uri tree = Uri.parse(e.uri);
            Uri root = DocumentsContract.buildDocumentUriUsingTree(tree,
                    DocumentsContract.getTreeDocumentId(tree));
            folderGone = DocumentsContract.deleteDocument(
                    c.getContentResolver(), root);
        } catch (Exception ex) {
            // providers podem recusar pastas raiz partilhadas — o log diz
            // a causa; a entrada da lista é removida à mesma (pasta que a
            // app não consegue apagar não devia continuar listada como
            // projeto utilizável)
            Log.e(TAG, "projetos: apagar a pasta de '" + e.name + "' falhou", ex);
        }
        // 2) remover da LISTA (a pasta sumir = o projeto sumiu)
        List<Entry> ps = load(c);
        int i = indexOfUri(ps, e.uri);
        if (i >= 0) {
            ps.remove(i);
            save(c, ps);
        }
        return folderGone;
    }

    /** nome do ficheiro de miniatura dentro da pasta do projeto (o engine
     *  escreve-o a CADA GUARDAR — captura da viewport; o Java lê e faz cache) */
    public static final String THUMB_FILE = "thumb.png";

    /** URI de documento do thumb.png (content://…/tree/…/document/…:thumb.png) */
    public static Uri thumbUri(Context c, Uri treeUri) {
        return DocumentsContract.buildDocumentUriUsingTree(treeUri,
                DocumentsContract.getTreeDocumentId(treeUri) + ":" + THUMB_FILE);
    }

    /** o thumb.png EXISTE na pasta do projeto? (em falta → card default) */
    public static boolean hasThumb(Context c, Uri treeUri) {
        try {
            return queryExists(c, thumbUri(c, treeUri));
        } catch (Exception e) {
            return false;
        }
    }

    /** o documento raiz da árvore do projeto AINDA existe? (spec F:
     *  "projeto em falta" — pasta apagada fora da app → card de estado) */
    public static boolean projectFolderExists(Context c, Uri treeUri) {
        try {
            Uri root = DocumentsContract.buildDocumentUriUsingTree(treeUri,
                    DocumentsContract.getTreeDocumentId(treeUri));
            return queryExists(c, root);
        } catch (Exception e) {
            return false;
        }
    }

    private static boolean queryExists(Context c, Uri docUri) {
        Cursor cur = null;
        try {
            cur = c.getContentResolver().query(docUri, null, null, null, null);
            return cur != null && cur.getCount() > 0;
        } catch (Exception e) {
            return false;   // SecurityException/FileNotFoundException → em falta
        } finally {
            if (cur != null) {
                cur.close();
            }
        }
    }

    /**
     * RENOMEAR (spec L: menu ⋮ do card): muda o nome na LISTA e renomeia a
     * PASTA no armazenamento (best-effort — providers podem recusar; se a
     * pasta não renomear, só o nome da lista muda e o log diz porquê).
     */
    public static void renameProject(Context c, Entry e, String newName) {
        if (newName == null || newName.trim().isEmpty()) {
            return;
        }
        List<Entry> ps = load(c);
        int i = indexOfUri(ps, e.uri);
        if (i < 0) {
            return;
        }
        // renomear a PASTA (best-effort)
        try {
            Uri tree = Uri.parse(e.uri);
            Uri root = DocumentsContract.buildDocumentUriUsingTree(tree,
                    DocumentsContract.getTreeDocumentId(tree));
            DocumentsContract.renameDocument(c.getContentResolver(), root,
                    newName);
        } catch (Exception ex) {
            Log.e(TAG, "projetos: renomear a pasta de '" + e.name + "' falhou "
                    + "(só o nome da lista muda)", ex);
        }
        ps.get(i).name = newName.trim();
        save(c, ps);
    }

    /**
     * DUPLICAR (spec L): copia TODO o conteúdo da pasta do projeto para uma
     * sub-pasta-irmã "<nome> copia" escolhida pelo picker SAF (O utilizador
     * escolhe o destino — a app NÃO cria pastas à revelia; o método devolve
     * o URI da nova árvore ou null se cancelado/falhou). A cópia é byte-a-
     * byte pela File API do SAF (DocumentsContract + streams).
     */
    public interface CopyProgress {
        void onProgress(int done, int total);
    }

    /** copia a árvore de `src` para DENTRO de `dstTree` (sub-pasta `folderName`) */
    public static boolean copyTreeInto(Context c, Uri srcTree, Uri dstTree,
                                       String folderName, CopyProgress prog) {
        try {
            Uri srcRoot = DocumentsContract.buildDocumentUriUsingTree(srcTree,
                    DocumentsContract.getTreeDocumentId(srcTree));
            Uri dstRoot = DocumentsContract.buildDocumentUriUsingTree(dstTree,
                    DocumentsContract.getTreeDocumentId(dstTree));
            // conta primeiro (para o progresso)
            java.util.List<Uri> files = listTreeDeep(c, srcRoot);
            int done = 0;
            // cria a pasta destino
            Uri dst = ensureChildDir(c, dstRoot, folderName);
            if (dst == null) {
                return false;
            }
            copyDirDeep(c, srcRoot, dst, files, new int[]{done}, prog);
            return true;
        } catch (Exception e) {
            Log.e(TAG, "projetos: duplicar falhou", e);
            return false;
        }
    }

    private static java.util.List<Uri> listTreeDeep(Context c, Uri rootDoc)
            throws Exception {
        java.util.List<Uri> out = new ArrayList<>();
        collectFiles(c, rootDoc, out);
        return out;
    }

    private static void collectFiles(Context c, Uri dirDoc,
                                     java.util.List<Uri> out) throws Exception {
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(
                dirDoc, DocumentsContract.getDocumentId(dirDoc));
        Cursor cur = c.getContentResolver().query(children, new String[]{
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_MIME_TYPE}, null, null, null);
        if (cur == null) {
            return;
        }
        java.util.List<Uri> subDirs = new ArrayList<>();
        while (cur.moveToNext()) {
            String id = cur.getString(0);
            String mime = cur.getString(1);
            Uri doc = DocumentsContract.buildDocumentUriUsingTree(dirDoc, id);
            if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                subDirs.add(doc);
            } else {
                out.add(doc);
            }
        }
        cur.close();
        for (Uri sd : subDirs) {
            collectFiles(c, sd, out);
        }
    }

    private static Uri ensureChildDir(Context c, Uri parentDoc, String name)
            throws Exception {
        if (childExists(c, parentDoc, name)) {
            return findChild(c, parentDoc, name);
        }
        return DocumentsContract.createDocument(c.getContentResolver(), parentDoc,
                DocumentsContract.Document.MIME_TYPE_DIR, name);
    }

    private static Uri findChild(Context c, Uri parentDoc, String name) {
        try {
            Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(
                    parentDoc, DocumentsContract.getDocumentId(parentDoc));
            Cursor cur = c.getContentResolver().query(children, new String[]{
                    DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                    DocumentsContract.Document.COLUMN_DISPLAY_NAME}, null, null, null);
            if (cur == null) {
                return null;
            }
            Uri found = null;
            while (cur.moveToNext()) {
                if (name.equals(cur.getString(1))) {
                    found = DocumentsContract.buildDocumentUriUsingTree(
                            parentDoc, cur.getString(0));
                    break;
                }
            }
            cur.close();
            return found;
        } catch (Exception e) {
            return null;
        }
    }

    private static void copyDirDeep(Context c, Uri srcDir, Uri dstDir,
                                    java.util.List<Uri> allFiles, int[] done,
                                    CopyProgress prog) throws Exception {
        // copia os ficheiros DIRETOS de srcDir para dstDir
        Uri children = DocumentsContract.buildChildDocumentsUriUsingTree(
                srcDir, DocumentsContract.getDocumentId(srcDir));
        Cursor cur = c.getContentResolver().query(children, new String[]{
                DocumentsContract.Document.COLUMN_DOCUMENT_ID,
                DocumentsContract.Document.COLUMN_MIME_TYPE,
                DocumentsContract.Document.COLUMN_DISPLAY_NAME}, null, null, null);
        if (cur == null) {
            return;
        }
        java.util.List<Uri> subSrc = new ArrayList<>();
        java.util.List<String> subNames = new ArrayList<>();
        while (cur.moveToNext()) {
            String id = cur.getString(0);
            String mime = cur.getString(1);
            String name = cur.getString(2);
            Uri doc = DocumentsContract.buildDocumentUriUsingTree(srcDir, id);
            if (DocumentsContract.Document.MIME_TYPE_DIR.equals(mime)) {
                subSrc.add(doc);
                subNames.add(name);
            } else {
                Uri dstFile = DocumentsContract.createDocument(
                        c.getContentResolver(), dstDir,
                        mime != null ? mime : "application/octet-stream", name);
                if (dstFile != null) {
                    copyFile(c, doc, dstFile);
                    done[0]++;
                    if (prog != null) {
                        prog.onProgress(done[0], allFiles.size());
                    }
                }
            }
        }
        cur.close();
        for (int i = 0; i < subSrc.size(); i++) {
            Uri subDst = ensureChildDir(c, dstDir, subNames.get(i));
            if (subDst != null) {
                copyDirDeep(c, subSrc.get(i), subDst, allFiles, done, prog);
            }
        }
    }

    private static void copyFile(Context c, Uri src, Uri dst) throws Exception {
        java.io.InputStream in = c.getContentResolver().openInputStream(src);
        java.io.OutputStream out = c.getContentResolver().openOutputStream(dst, "w");
        if (in == null || out == null) {
            if (in != null) in.close();
            if (out != null) out.close();
            throw new java.io.IOException("stream nulo ao copiar");
        }
        byte[] buf = new byte[64 * 1024];
        int n;
        while ((n = in.read(buf)) > 0) {
            out.write(buf, 0, n);
        }
        in.close();
        out.close();
    }

    /** adiciona a entrada duplicada à lista (mesma pasta de destino) */
    public static Entry addEntry(Context c, String name, Uri treeUri) {
        List<Entry> ps = load(c);
        int dup = indexOfUri(ps, treeUri.toString());
        if (dup >= 0) {
            return ps.get(dup);   // já existe (duplicou p/ a mesma pasta)
        }
        Entry e = new Entry(name, treeUri.toString(),
                System.currentTimeMillis());
        ps.add(e);
        save(c, ps);
        return e;
    }

    /**
     * URI de ÁRVORE sintética para uma SUBPASTA da árvore concedida (o
     * duplicar cria "<nome> copia" DENTRO da pasta pai escolhida). O grant
     * do picker cobre TODA a subárvore — o URI sintético apenas RE-ENRAÍZA
     * o segmento /tree/ no id do documento da subpasta, e TODA a File API
     * da engine (bridgeRootDoc → getTreeDocumentId) resolve a partir dele.
     */
    public static Uri subfolderTreeUri(Uri parentTree, String folderName) {
        String parentDoc = DocumentsContract.getTreeDocumentId(parentTree);
        String childDoc = parentDoc + ":" + folderName;
        String s = parentTree.toString();
        int i = s.indexOf("/tree/") + "/tree/".length();
        if (i < "/tree/".length()) {
            return parentTree;   // uri inesperado — devolve intacto
        }
        int j = s.indexOf('/', i);
        if (j < 0) {
            j = s.length();
        }
        return Uri.parse(s.substring(0, i) + Uri.encode(childDoc)
                + s.substring(j));
    }
}
