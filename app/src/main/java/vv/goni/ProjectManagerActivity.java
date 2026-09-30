package vv.goni;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.ContentResolver;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.util.Log;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;

import java.util.ArrayList;
import java.util.List;

/**
 * F5.4 — ECRÃ INICIAL da app: Gestor de Projetos (estilo Godot).
 *
 * ANTES: a app abria direto no editor, com o projeto fixo no
 * getExternalFilesDir. AGORA: lista de projetos → toque abre no editor;
 * "+" escolhe a PASTA DESTE projeto via SAF (ACTION_OPEN_DOCUMENT_TREE).
 *
 * CADA projeto pode escolher uma pasta DIFERENTE — o SAF não limita a app
 * a uma única pasta: cada escolha gera um URI próprio, guardado com
 * takePersistableUriPermission para AQUELE projeto. Vários projetos nunca
 * se misturam na mesma pasta, MESMO SEM All Files Access.
 *
 * O All Files Access mantém-se como está (F5.2): serve só para
 * import/export de assets soltos DENTRO de um projeto já aberto
 * (Download/Documents). As duas coisas COEXISTEM.
 *
 * A lista (nome + URI) vive em projects.json no app-private (VvProjects)
 * — não depende do handshake nem de permissões. O flow SAF→native passa
 * pelo handshake: gestor → VvActivity(extras) → nativeOpenProject.
 *
 * UI programática (sem res/) — mono, escura, mesma linguagem visual do
 * editor. landscape travado pelo manifest (contrato da app).
 */
public class ProjectManagerActivity extends Activity {
    private static final String TAG = "GONI";

    // request code do picker SAF — NÃO colide com kReqAllFiles (4301)
    static final int REQ_PICK_TREE = 4302;

    private final List<VvProjects.Entry> projects = new ArrayList<>();
    private ArrayAdapter<String> adapter;
    private ListView list;
    private TextView empty;
    private String pendingName = "projeto";

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(0xFF101418);
        root.setPadding(dp(16), dp(14), dp(16), dp(12));

        TextView title = new TextView(this);
        title.setText("G.One VV — Projetos");
        title.setTextColor(0xFFF2F2F2);
        title.setTextSize(22);
        title.setPadding(dp(4), dp(2), 0, dp(10));
        root.addView(title, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        // corpo: lista + empty-state sobrepostos (FrameLayout)
        FrameLayout body = new FrameLayout(this);
        list = new ListView(this);
        list.setDivider(new android.graphics.drawable.ColorDrawable(0xFF232A31));
        list.setDividerHeight(dp(1));
        adapter = new ArrayAdapter<String>(this,
                android.R.layout.simple_list_item_1) {
            @Override
            public View getView(int pos, View cv, ViewGroup parent) {
                TextView tv = (TextView) super.getView(pos, cv, parent);
                tv.setTextColor(0xFFE6E6E6);
                tv.setTextSize(16);
                tv.setPadding(dp(10), dp(12), dp(10), dp(12));
                return tv;
            }
        };
        list.setAdapter(adapter);
        list.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> p, View v, int pos, long id) {
                openProject(pos);
            }
        });
        list.setOnItemLongClickListener(new AdapterView.OnItemLongClickListener() {
            @Override
            public boolean onItemLongClick(AdapterView<?> p, View v, int pos, long id) {
                confirmRemove(pos);
                return true;
            }
        });
        body.addView(list, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));

        empty = new TextView(this);
        empty.setText("Nenhum projeto.\nToque em “+ Novo projeto” para escolher a pasta.");
        empty.setTextColor(0xFF8A939B);
        empty.setTextSize(15);
        empty.setGravity(Gravity.CENTER);
        empty.setVisibility(View.GONE);
        body.addView(empty, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.CENTER));
        root.addView(body, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

        Button add = new Button(this);
        add.setText("+ Novo projeto");
        add.setTextSize(16);
        add.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                askNewProject();
            }
        });
        root.addView(add, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        TextView hint = new TextView(this);
        hint.setText("A pasta de cada projeto é escolhida no seletor do sistema "
                + "(SAF) — pastas diferentes, projetos separados.");
        hint.setTextColor(0xFF6E7780);
        hint.setTextSize(12);
        hint.setPadding(dp(4), dp(6), dp(4), 0);
        root.addView(hint, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        setContentView(root, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        reload();
    }

    @Override
    protected void onResume() {
        super.onResume();
        reload();   // a lista pode ter mudado (remoções no editor / volta)
    }

    private void reload() {
        projects.clear();
        projects.addAll(VvProjects.load(this));
        adapter.clear();
        for (VvProjects.Entry e : projects) {
            adapter.add(e.name + "\n" + shortUri(e.uri));
        }
        adapter.notifyDataSetChanged();
        empty.setVisibility(projects.isEmpty() ? View.VISIBLE : View.GONE);
        list.setVisibility(projects.isEmpty() ? View.GONE : View.VISIBLE);
    }

    /**
     * 0.6.7 — "+" → nome do projeto → picker SAF da pasta DESTE projeto.
     *
     * FIX DO BUG DO NOME INVISÍVEL: o tema da app é
     * Theme.NoTitleBar.Fullscreen (claro — o manifest aplica-o à activity) e
     * o AlertDialog herdava esse tema claro → painel BRANCO. O EditText já
     * tinha texto quase-branco (0xFFE6E6E6) = texto branco sobre fundo
     * branco: durante a digitação o utilizador NÃO VIA o que escrevia. O
     * campo agora tem FUNDO ESCURO explícito (0xFF1E222A) + cor de texto
     * clara + hint — legível em QUALQUER tema de diálogo do sistema.
     */
    private void askNewProject() {
        final EditText input = new EditText(this);
        input.setSingleLine(true);
        input.setText(pendingName);
        input.setSelection(input.getText().length());
        // 0.6.7: contraste garantido — fundo escuro + texto claro + hint
        // (o diálogo herda o tema CLARO do manifest; sem fundo explícito o
        // texto claro desaparecia no branco do painel)
        input.setBackgroundColor(0xFF1E222A);
        input.setTextColor(0xFFE6E6E6);
        input.setHintTextColor(0xFF8A939B);
        input.setHint("nome do projeto");
        input.setPadding(dp(12), dp(10), dp(12), dp(10));
        new AlertDialog.Builder(this)
                .setTitle("Nome do projeto")
                .setMessage("No passo seguinte escolha a PASTA onde este projeto fica (só dele).")
                .setView(input)
                .setPositiveButton("Continuar", (d, w) -> {
                    String n = input.getText().toString().trim();
                    pendingName = n.isEmpty() ? "projeto" : n;
                    pickFolder();
                })
                .setNegativeButton("Cancelar", null)
                .show();
        // foco + teclado já abertos: digitar logo
        input.requestFocus();
    }

    private void pickFolder() {
        try {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            i.addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
                    | Intent.FLAG_GRANT_READ_URI_PERMISSION
                    | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            startActivityForResult(i, REQ_PICK_TREE);
        } catch (Exception e) {
            Log.e(TAG, "projetos: picker SAF indisponível", e);
            Toast.makeText(this, "seletor de pastas indisponível",
                    Toast.LENGTH_LONG).show();
        }
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (req != REQ_PICK_TREE) {
            return;
        }
        if (res != RESULT_OK || data == null || data.getData() == null) {
            Toast.makeText(this, "sem pasta — projeto não criado",
                    Toast.LENGTH_SHORT).show();
            return;
        }
        Uri tree = data.getData();
        try {
            // permissão PERSISTENTE para ESTE projeto (sobrevive a arranques)
            getContentResolver().takePersistableUriPermission(tree,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION
                            | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        } catch (Exception e) {
            Log.e(TAG, "projetos: takePersistableUriPermission falhou", e);
        }
        List<VvProjects.Entry> ps = VvProjects.load(this);
        int dup = VvProjects.indexOfUri(ps, tree.toString());
        if (dup >= 0) {
            Toast.makeText(this,
                    "esta pasta já é o projeto “" + ps.get(dup).name + "”",
                    Toast.LENGTH_LONG).show();
            return;
        }
        // estrutura do core/Project na pasta escolhida (respeita existentes)
        VvProjects.createStructure(this, tree);
        VvProjects.Entry e = new VvProjects.Entry(pendingName, tree.toString(),
                System.currentTimeMillis());
        ps.add(e);
        VvProjects.save(this, ps);
        reload();
        VvProjects.launchEditor(this, e);   // entra no editor; volta → lista
    }

    private void openProject(int pos) {
        if (pos < 0 || pos >= projects.size()) {
            return;
        }
        VvProjects.launchEditor(this, projects.get(pos));
    }

    /**
     * long-press: DUAS ações distintas (0.6.7):
     *   • "Remover da lista" — só tira da lista; a pasta fica intacta
     *     (voltar a adicionar a mesma pasta reabre o projeto);
     *   • "Apagar projeto" — diálogo de confirmação SEPARADO e explícito
     *     ("Apagar projeto X? Não pode ser desfeito") → remove a PASTA via
     *     File API (DocumentsContract.deleteDocument) + sai da lista +
     *     liberta a permissão persistente.
     */
    private void confirmRemove(int pos) {
        if (pos < 0 || pos >= projects.size()) {
            return;
        }
        final VvProjects.Entry e = projects.get(pos);
        new AlertDialog.Builder(this)
                .setTitle(e.name)
                .setMessage(shortUri(e.uri))
                .setNeutralButton("Remover da lista", (d, w) -> {
                    removeFromList(e);
                })
                .setPositiveButton("Apagar projeto", (d, w) -> {
                    confirmDelete(e);
                })
                .setNegativeButton("Cancelar", null)
                .show();
    }

    /** remove da LISTA (a pasta escolhida NÃO é apagada) */
    private void removeFromList(VvProjects.Entry e) {
        List<VvProjects.Entry> ps = VvProjects.load(this);
        int i = VvProjects.indexOfUri(ps, e.uri);
        if (i >= 0) {
            ps.remove(i);
            VvProjects.save(this, ps);
            releasePermission(e.uri);
        }
        reload();
    }

    /** 0.6.7 — confirmação EXPLÍCITA antes de apagar a pasta de verdade */
    private void confirmDelete(VvProjects.Entry e) {
        new AlertDialog.Builder(this)
                .setTitle("Apagar projeto")
                .setMessage("Apagar projeto “" + e.name + "”?\n\n"
                        + "A PASTA e TODOS os ficheiros do projeto são "
                        + "apagados do armazenamento.\nNão pode ser desfeito.")
                .setPositiveButton("Apagar", (d, w) -> {
                    boolean gone = VvProjects.deleteProject(this, e);
                    releasePermission(e.uri);
                    reload();
                    Toast.makeText(this,
                            gone ? "projeto apagado (" + e.name + ")"
                                 : "pasta não apagada — entrada removida da lista",
                            Toast.LENGTH_LONG).show();
                })
                .setNegativeButton("Cancelar", null)
                .show();
    }

    private void releasePermission(String uri) {
        try {
            // liberta a permissão persistente do URI removido
            getContentResolver().releasePersistableUriPermission(
                    Uri.parse(uri),
                    Intent.FLAG_GRANT_READ_URI_PERMISSION
                            | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        } catch (Exception ignored) {
        }
    }

    private static String shortUri(String uri) {
        try {
            Uri u = Uri.parse(uri);
            List<String> seg = u.getPathSegments();
            return seg.isEmpty() ? u.toString() : seg.get(seg.size() - 1);
        } catch (Exception e) {
            return uri;
        }
    }

    private int dp(int v) {
        return Math.round(v * getResources().getDisplayMetrics().density);
    }
}
