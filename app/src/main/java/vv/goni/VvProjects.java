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
        public final String name;
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

    /**
     * 0.6.7 — APAGA o projeto de verdade: a PASTA escolhida no SAF é
     * removida via File API (DocumentsContract.deleteDocument no URI de
     * documento da raiz — a mesma File API de escrita que criou a
     * estrutura) e a entrada sai da lista (projects.json).
     *
     * CHAMADO SÓ APÓS o diálogo de confirmação explícita do
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
}
