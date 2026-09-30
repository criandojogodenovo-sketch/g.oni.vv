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

    /** entrada da lista: nome + URI da pasta SAF + data de criação */
    public static final class Entry {
        public final String name;
        public final String uri;
        public final long createdAt;

        public Entry(String name, String uri, long createdAt) {
            this.name = name;
            this.uri = uri;
            this.createdAt = createdAt;
        }

        JSONObject toJson() throws Exception {
            JSONObject o = new JSONObject();
            o.put("name", name);
            o.put("uri", uri);
            o.put("createdAt", createdAt);
            return o;
        }

        static Entry fromJson(JSONObject o) {
            return new Entry(o.optString("name", "projeto"),
                             o.optString("uri", ""),
                             o.optLong("createdAt", 0L));
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

    /** abre o editor COM este projeto (extras → nativeOpenProject no boot) */
    public static void launchEditor(Activity a, Entry e) {
        Intent i = new Intent(a, VvActivity.class);
        i.putExtra(EXTRA_PROJECT_URI, e.uri);
        i.putExtra(EXTRA_PROJECT_NAME, e.name);
        a.startActivity(i);
    }
}
