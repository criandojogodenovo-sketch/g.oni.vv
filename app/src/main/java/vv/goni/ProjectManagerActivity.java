package vv.goni;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.GradientDrawable;
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
 * F5.4 → 0.7.6 — ECRÃ DE PROJETOS REESTRUTURADO.
 *
 * Estrutura final (spec 0.7.6):
 *   ┌──────────────────────────────────────────────────────┐
 *   │ G.One VV                                             │
 *   │ [ Novo projeto ] [ Importar projeto ]   ← TOPO, contorno #8AB4F8
 *   │ Meus projetos                          ← cabeçalho   │
 *   │  nome do projeto                                      │
 *   │  dd/MM/yyyy HH:mm (última edição)     ← UMA entrada  │
 *   │  …                                                    │
 *   └──────────────────────────────────────────────────────┘
 *
 * REGRAS 0.7.6 cumpridas aqui:
 *   • UMA entrada de criação ([Novo projeto] — o botão duplicado de fundo
 *     e o prefixo interno "primary:" da lista MORRERAM);
 *   • a lista mostra SÓ nome + data da última edição (o URI nunca aparece;
 *     ProjectsFormat.folderLabel limpa "primary:" quando a pasta precisa
 *     de ser referida nos diálogos);
 *   • botões de criação SEM gradiente cinza do tema do sistema: contorno
 *     #8AB4F8 sobre o fundo escuro (a cor de marca do editor — a mesma
 *     exceção documentada do Theme central);
 *   • nenhum emoji.
 *
 * REGRAS 0.8.6 (Theme uniforme + página inicial limpa):
 *   • os TOKENS da marca vivem em CONSTANTES únicas (BRAND/BG/TEXT/TEXT_DIM/
 *     SURFACE/LINE) — os hex inline espalhados MORRERAM; são o espelho Java
 *     do Theme.h da engine (mono + brand);
 *   • os AlertDialogs herdam o tema CLARO do manifest → agora correm num
 *     ContextThemeWrapper ESCURO (Theme_DeviceDefault_Dialog): título,
 *     mensagem e botões coerentes com o resto da app;
 *   • hierarquia visual: título de marca + subtítulo discreto, ações no
 *     topo, lista com NOME (claro) + data (discreto) e empty-state com CTA.
 *
 * Fluxos: [Novo projeto] → nome (diálogo) → picker SAF da pasta DESTE
 * projeto; [Importar projeto] → picker SAF direto (pasta que JÁ é um
 * projeto .goni — o nome vem da própria pasta). Toque abre; long-press
 * remove da lista / apaga de verdade (diálogos de sempre).
 */
public class ProjectManagerActivity extends Activity {
    private static final String TAG = "GONI";

    // request code do picker SAF — NÃO colide com kReqAllFiles (4301)
    static final int REQ_PICK_TREE = 4302;
    // 0.7.6: o picker do IMPORTAR (pasta que já existe) — code próprio para
    // o onActivityResult saber qual foi
    static final int REQ_PICK_TREE_IMPORT = 4303;

    // 0.8.6 — TOKENS do Theme (espelho Java do ui/Theme.h — UMA fonte de
    // verdade por plataforma; nada de hex inline espalhado):
    //   brand   #8AB4F8 (a exceção documentada ao mono — ações/contorno)
    //   bg      #0B0E13 (fundo da app — o mesmo BG do Theme)
    //   text    #E6E6E6 / textDim #8A939B (texto primário/discreto)
    //   surface #1E222A (campos/inputs)  line #232A31 (divisores)
    private static final int BRAND = 0xFF8AB4F8;
    private static final int BG = 0xFF0B0E13;
    private static final int TEXT = 0xFFE6E6E6;
    private static final int TEXT_DIM = 0xFF8A939B;
    private static final int SURFACE = 0xFF1E222A;
    private static final int LINE = 0xFF232A31;

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
        root.setBackgroundColor(BG);
        root.setPadding(dp(16), dp(14), dp(16), dp(12));

        TextView title = new TextView(this);
        title.setText("G.One VV");
        title.setTextColor(TEXT);
        title.setTextSize(22);
        title.setPadding(dp(4), dp(2), 0, dp(2));
        root.addView(title, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        // 0.8.6 — subtítulo discreto (o launcher diz o que é, sem ruído)
        TextView subtitle = new TextView(this);
        subtitle.setText("editor de jogos no telemóvel");
        subtitle.setTextColor(TEXT_DIM);
        subtitle.setTextSize(13);
        subtitle.setPadding(dp(4), 0, 0, dp(10));
        root.addView(subtitle, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        // ---- TOPO: as DUAS ações (uma entrada de CRIAÇÃO + importar) ------
        LinearLayout actions = new LinearLayout(this);
        actions.setOrientation(LinearLayout.HORIZONTAL);
        Button novo = outlineButton("Novo projeto");
        novo.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                askNewProject();
            }
        });
        actions.addView(novo, new LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

        Button importar = outlineButton("Importar projeto");
        LinearLayout.LayoutParams ip = new LinearLayout.LayoutParams(
                0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f);
        ip.leftMargin = dp(10);
        importar.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                pickFolder(REQ_PICK_TREE_IMPORT);
            }
        });
        actions.addView(importar, ip);
        root.addView(actions, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        // ---- cabeçalho da lista -------------------------------------------
        TextView header = new TextView(this);
        header.setText("Meus projetos");
        header.setTextColor(TEXT_DIM);
        header.setTextSize(13);
        header.setPadding(dp(4), dp(14), dp(4), dp(6));
        root.addView(header, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT));

        // corpo: lista + empty-state sobrepostos (FrameLayout)
        FrameLayout body = new FrameLayout(this);
        list = new ListView(this);
        list.setDivider(new android.graphics.drawable.ColorDrawable(LINE));
        list.setDividerHeight(dp(1));
        adapter = new ArrayAdapter<String>(this,
                android.R.layout.simple_list_item_2) {
            @Override
            public View getView(int pos, View cv, ViewGroup parent) {
                // lista = SÓ nome + data da última edição (o URI sai —
                // era ele que mostrava o prefixo interno "primary:")
                android.widget.TwoLineListItem item =
                        (cv instanceof android.widget.TwoLineListItem)
                                ? (android.widget.TwoLineListItem) cv : null;
                if (item == null) {
                    item = (android.widget.TwoLineListItem) getLayoutInflater()
                            .inflate(android.R.layout.simple_list_item_2,
                                     parent, false);
                }
                VvProjects.Entry e = projects.get(pos);
                TextView l1 = item.getText1();
                l1.setText(e.name);
                l1.setTextColor(TEXT);
                l1.setTextSize(17);
                TextView l2 = item.getText2();
                String d = ProjectsFormat.dateLabel(
                        e.editedAt > 0 ? e.editedAt : e.createdAt);
                l2.setText(d.isEmpty() ? "—" : d);
                l2.setTextColor(TEXT_DIM);
                l2.setTextSize(13);
                item.setPadding(dp(6), dp(8), dp(6), dp(8));
                return item;
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
        empty.setText("Nenhum projeto ainda.\nToque em “Novo projeto” para começar.");
        empty.setTextColor(TEXT_DIM);
        empty.setTextSize(15);
        empty.setGravity(Gravity.CENTER);
        empty.setVisibility(View.GONE);
        body.addView(empty, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT,
                Gravity.CENTER));
        root.addView(body, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

        setContentView(root, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        reload();
    }

    /** botão de CONTORNO da marca (#8AB4F8 sobre fundo escuro — sem
     *  gradiente cinza do tema do sistema, spec 0.7.6) */
    private Button outlineButton(String label) {
        Button b = new Button(this);
        b.setText(label);
        b.setTextSize(15);
        b.setTextColor(BRAND);
        b.setAllCaps(false);
        GradientDrawable outline = new GradientDrawable();
        outline.setColor(Color.TRANSPARENT);          // nada de gradiente
        outline.setStroke(dp(2), BRAND);              // contorno de marca
        outline.setCornerRadius(dp(6));
        b.setBackground(outline);
        b.setPadding(dp(12), dp(10), dp(12), dp(10));
        return b;
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
            // (o texto da linha vem do getView — só o COUNT importa aqui)
            adapter.add(e.name);
        }
        adapter.notifyDataSetChanged();
        empty.setVisibility(projects.isEmpty() ? View.VISIBLE : View.GONE);
        list.setVisibility(projects.isEmpty() ? View.GONE : View.VISIBLE);
    }

    /**
     * [Novo projeto]: nome do projeto → picker SAF da pasta DESTE projeto.
     * É a ÚNICA entrada de criação (0.7.6).
     */
    private void askNewProject() {
        final EditText input = new EditText(this);
        input.setSingleLine(true);
        input.setText(pendingName);
        input.setSelection(input.getText().length());
        // contraste garantido pelos TOKENS (o wrapper escuro + estilo próprio)
        input.setBackgroundColor(SURFACE);
        input.setTextColor(TEXT);
        input.setHintTextColor(TEXT_DIM);
        input.setHint("nome do projeto");
        input.setPadding(dp(12), dp(10), dp(12), dp(10));
        // 0.8.6 — DIÁLOGOS ESCUROS: o builder corre num ContextThemeWrapper
        // com o tema DeviceDefault ESCURO (o manifest é Fullscreen claro —
        // sem o wrapper o título/mensagem/botões saíam claros e quebravam a
        // identidade). O input mantém o estilo próprio (tokens do Theme).
        new AlertDialog.Builder(new android.view.ContextThemeWrapper(this,
                android.R.style.Theme_DeviceDefault_Dialog))
                .setTitle("Nome do projeto")
                .setMessage("No passo seguinte escolha a PASTA onde este projeto fica (só dele).")
                .setView(input)
                .setPositiveButton("Continuar", (d, w) -> {
                    String n = input.getText().toString().trim();
                    pendingName = n.isEmpty() ? "projeto" : n;
                    pickFolder(REQ_PICK_TREE);
                })
                .setNegativeButton("Cancelar", null)
                .show();
        input.requestFocus();
    }

    private void pickFolder(int requestCode) {
        try {
            Intent i = new Intent(Intent.ACTION_OPEN_DOCUMENT_TREE);
            i.addFlags(Intent.FLAG_GRANT_PERSISTABLE_URI_PERMISSION
                    | Intent.FLAG_GRANT_READ_URI_PERMISSION
                    | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
            startActivityForResult(i, requestCode);
        } catch (Exception e) {
            Log.e(TAG, "projetos: picker SAF indisponível", e);
            Toast.makeText(this, "seletor de pastas indisponível",
                    Toast.LENGTH_LONG).show();
        }
    }

    @Override
    protected void onActivityResult(int req, int res, Intent data) {
        super.onActivityResult(req, res, data);
        if (req != REQ_PICK_TREE && req != REQ_PICK_TREE_IMPORT) {
            return;
        }
        final boolean importing = (req == REQ_PICK_TREE_IMPORT);
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
        // nome: pedido no diálogo (criar) ou derivado da própria pasta
        // (importar — a pasta JÁ é um projeto .goni)
        String name = pendingName;
        if (importing) {
            name = VvProjects.folderDisplayName(this, tree);
        }
        // estrutura do core/Project na pasta escolhida (respeita existentes
        // — importar uma pasta com .goni não cria duplicados)
        VvProjects.createStructure(this, tree);
        VvProjects.Entry e = new VvProjects.Entry(name, tree.toString(),
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
     *   • "Remover da lista" — só tira da lista; a pasta fica intacta;
     *   • "Apagar projeto" — diálogo de confirmação SEPARADO e explícito
     *     → remove a PASTA via File API + sai da lista + liberta a
     *     permissão persistente. 0.7.6: o rodapé do diálogo mostra a pasta
     *     SEM o prefixo interno (ProjectsFormat.folderLabel).
     */
    private void confirmRemove(int pos) {
        if (pos < 0 || pos >= projects.size()) {
            return;
        }
        final VvProjects.Entry e = projects.get(pos);
        // 0.8.6: MESMO wrapper escuro dos diálogos (identidade uniforme)
        new AlertDialog.Builder(new android.view.ContextThemeWrapper(this,
                android.R.style.Theme_DeviceDefault_Dialog))
                .setTitle(e.name)
                .setMessage(ProjectsFormat.folderLabel(e.uri))
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
        // 0.8.6: MESMO wrapper escuro dos diálogos
        new AlertDialog.Builder(new android.view.ContextThemeWrapper(this,
                android.R.style.Theme_DeviceDefault_Dialog))
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
            getContentResolver().releasePersistableUriPermission(
                    Uri.parse(uri),
                    Intent.FLAG_GRANT_READ_URI_PERMISSION
                            | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        } catch (Exception ignored) {
        }
    }

    private int dp(int v) {
        return Math.round(v * getResources().getDisplayMetrics().density);
    }
}
