package vv.goni;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Intent;
import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.StateListDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.util.Log;
import android.util.LruCache;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.BaseAdapter;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.GridView;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.PopupMenu;
import android.widget.ProgressBar;
import android.widget.TextView;
import android.widget.Toast;

import java.io.InputStream;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.List;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

/**
 * 0.9.0 — TELA DE PROJETOS (spec F dos mockups do autor):
 *
 *   ┌───────────────────────────────────────────────────────────────┐
 *   │ [logo G+lâmpada 48] G.One VV            (20sp)                │ 72dp
 *   │                      editor de jogos no telemóvel (12sp text2)│
 *   │ [🔍 pesquisar projetos______________] [Ordenar: Última Ed. ▾] │ 48dp
 *   │ [  ＋ Novo projeto  ] [  ⬆ Importar projeto  ]  ← fill accent │ 56dp
 *   │ Meus projetos                                       (12sp)   │
 *   │ ┌───────────────┐  ┌───────────────┐                          │
 *   │ │  16:9 thumb   │  │  16:9 thumb   │  cards r=8dp, bordo      │
 *   │ │ nome    (⋮48) │  │ nome    (⋮48) │                          │
 *   │ │ há 2 h        │  │ há 3 d        │  tempo relativo 12sp     │
 *   │ └───────────────┘  └───────────────┘                          │
 *   └───────────────────────────────────────────────────────────────┘
 *
 * REGRAS (spec F):
 *   • miniatura 16:9 — default = logo G.One; após guardar = captura da
 *     viewport (thumb.png escrito pelo ENGINE a cada save; Java lê + cache);
 *   • estados do card: normal / premido (surface2) / a carregar miniatura
 *     (spinner) / projeto em falta (fundo rachado + ? + erro legível +
 *     recuperação) / vazio com convite;
 *   • gestos: toque abre; toque longo OU ⋮ abre menu (abrir/renomear/
 *     duplicar/apagar COM confirmação); SEM swipe-to-delete;
 *   • pesquisa filtra por nome; dropdown Ordenar (Última Edição padrão);
 *   • tokens = espelho Java do ui/Theme.h 0.9.0 (tabela spec A).
 */
public class ProjectManagerActivity extends Activity {
    private static final String TAG = "GONI";

    static final int REQ_PICK_TREE = 4302;         // criar: pasta DO projeto
    static final int REQ_PICK_TREE_IMPORT = 4303;  // importar pasta .goni
    static final int REQ_PICK_TREE_DUP = 4304;     // 0.9.0: duplicar → destino
    static final int REQ_PICK_TREE_RECOVER = 4305; // 0.9.0: re-apontar pasta

    // ---- TOKENS 0.9.0 (espelho Java do ui/Theme.h — tabela spec A) --------
    static final int BG = 0xFF0B0E13;        // bg
    static final int SURFACE = 0xFF151A23;   // surface (cards)
    static final int SURFACE2 = 0xFF1F2733;  // surface-2 (premido)
    static final int BORDER = 0xFF2A3442;    // border
    static final int TEXT1 = 0xFFF5F5F5;     // text-1
    static final int TEXT2 = 0xFF98A2B3;     // text-2 (6,7:1 sobre surface)
    static final int ACCENT = 0xFF2196F3;    // accent 🔶 (flip 1 token)
    static final int ACCENT_PRESS = 0xFF1B7FD4;
    static final int DANGER = 0xFFEF5350;
    static final int WARN = 0xFFFABB45;

    private final List<VvProjects.Entry> all = new ArrayList<>();   // fonte
    private final List<VvProjects.Entry> shown = new ArrayList<>(); // filtro+ordem
    private final List<String> missingUris = new ArrayList<>();     // estado F

    private GridView grid;
    private TextView emptyTitle, emptySub;
    private EditText search;
    private TextView sortBtn;
    private CardsAdapter adapter;
    private String pendingName = "projeto";
    private VvProjects.Entry pendingDup;          // duplicar em curso
    private int sortMode = 0;                     // ProjectsFormat.sortLabel

    // cache de miniaturas (spec F: PNG no dir + cache) — chave = uri do projeto
    private static final LruCache<String, Bitmap> THUMBS =
            new LruCache<>(24);   // ~24 bitmaps 480×270
    // estado "a carregar" por uri (spinner até a thread postar)
    private final List<String> loadingThumbs = new ArrayList<>();
    private final ExecutorService io = Executors.newSingleThreadExecutor();
    private final Handler main = new Handler(Looper.getMainLooper());

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        sortMode = getPreferences(MODE_PRIVATE).getInt("sortMode", 0);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        root.setPadding(dp(16), dp(8), dp(16), dp(8));

        root.addView(buildHeader());
        root.addView(buildSearchRow());
        root.addView(buildActions());
        root.addView(buildSectionLabel());

        // corpo: grelha + empty-state sobrepostos
        FrameLayout body = new FrameLayout(this);
        grid = new GridView(this);
        grid.setNumColumns(2);
        grid.setHorizontalSpacing(dp(16));
        grid.setVerticalSpacing(dp(16));
        grid.setStretchMode(GridView.STRETCH_COLUMN_WIDTH);
        grid.setSelector(new ColorDrawable(Color.TRANSPARENT)); // sem halo
        adapter = new CardsAdapter();
        grid.setAdapter(adapter);
        body.addView(grid, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));

        // EMPTY STATE (spec F/M): ícone + convite a criar o 1º projeto
        LinearLayout emptyBox = new LinearLayout(this);
        emptyBox.setOrientation(LinearLayout.VERTICAL);
        emptyBox.setGravity(Gravity.CENTER);
        ImageView bigLogo = new ImageView(this);
        bigLogo.setImageResource(R.drawable.gone_logo);
        bigLogo.setColorFilter(TEXT2);
        emptyBox.addView(bigLogo, new LinearLayout.LayoutParams(dp(96), dp(96)));
        emptyTitle = new TextView(this);
        emptyTitle.setText("Nenhum projeto ainda");
        emptyTitle.setTextColor(TEXT1);
        emptyTitle.setTextSize(16);
        emptyTitle.setGravity(Gravity.CENTER);
        emptyTitle.setPadding(0, dp(16), 0, dp(4));
        emptyBox.addView(emptyTitle);
        emptySub = new TextView(this);
        emptySub.setText("Toque em “Novo projeto” para criar o primeiro");
        emptySub.setTextColor(TEXT2);
        emptySub.setTextSize(14);
        emptySub.setGravity(Gravity.CENTER);
        emptyBox.addView(emptySub);
        body.addView(emptyBox, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.WRAP_CONTENT, Gravity.CENTER));

        root.addView(body, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f));

        setContentView(root, new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));
        reload();
    }

    // ---- cabeçalho 72dp: logo 48 + G.One VV 20sp + tagline 12sp -----------
    private View buildHeader() {
        LinearLayout h = new LinearLayout(this);
        h.setOrientation(LinearLayout.HORIZONTAL);
        h.setGravity(Gravity.CENTER_VERTICAL);
        h.setPadding(dp(4), dp(8), dp(4), dp(8));
        ImageView logo = new ImageView(this);
        logo.setImageResource(R.drawable.gone_logo);
        h.addView(logo, new LinearLayout.LayoutParams(dp(48), dp(48)));
        LinearLayout texts = new LinearLayout(this);
        texts.setOrientation(LinearLayout.VERTICAL);
        texts.setPadding(dp(12), 0, 0, 0);
        TextView title = new TextView(this);
        title.setText("G.One VV");
        title.setTextColor(TEXT1);
        title.setTextSize(20);
        title.setTypeface(null, android.graphics.Typeface.BOLD);
        texts.addView(title);
        TextView tag = new TextView(this);
        tag.setText("editor de jogos no telemóvel");
        tag.setTextColor(TEXT2);
        tag.setTextSize(12);
        texts.addView(tag);
        h.addView(texts, new LinearLayout.LayoutParams(0,
                ViewGroup.LayoutParams.WRAP_CONTENT, 1f));
        return h;
    }

    // ---- pesquisa 48dp (lupa) + dropdown Ordenar 48dp ---------------------
    private View buildSearchRow() {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setPadding(0, dp(8), 0, 0);

        search = new EditText(this);
        search.setSingleLine(true);
        search.setTextSize(14);
        search.setTextColor(TEXT1);
        search.setHintTextColor(TEXT2);
        search.setHint("pesquisar projetos");
        search.setPadding(dp(12), 0, dp(12), 0);
        GradientDrawable sf = new GradientDrawable();
        sf.setColor(SURFACE);
        sf.setStroke(dp(1), BORDER);
        sf.setCornerRadius(dp(4));       // campo = raio 4dp (spec A)
        search.setBackground(sf);
        search.setCompoundDrawablesWithIntrinsicBounds(
                UiIcons.drawable(UiIcons.LUPA, TEXT2, 24, density()), null,
                null, null);
        search.setCompoundDrawablePadding(dp(8));
        search.addTextChangedListener(new android.text.TextWatcher() {
            @Override
            public void beforeTextChanged(CharSequence s, int a, int b, int c) {
            }
            @Override
            public void onTextChanged(CharSequence s, int a, int b, int c) {
            }
            @Override
            public void afterTextChanged(android.text.Editable s) {
                refresh();
            }
        });
        row.addView(search, new LinearLayout.LayoutParams(0, dp(48), 1f));

        sortBtn = new TextView(this, null, 0);
        sortBtn.setTextSize(12);
        sortBtn.setTextColor(TEXT2);
        sortBtn.setGravity(Gravity.CENTER_VERTICAL);
        sortBtn.setPadding(dp(12), 0, dp(8), 0);
        sortBtn.setCompoundDrawablesWithIntrinsicBounds(
                UiIcons.drawable(UiIcons.SORT, TEXT2, 24, density()), null,
                UiIcons.drawable(UiIcons.DOTS, TEXT2, 24, density()), null);
        sortBtn.setCompoundDrawablePadding(dp(6));
        sortBtn.setOnClickListener(v -> showSortMenu());
        row.addView(sortBtn, new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, dp(48)));
        return row;
    }

    // ---- ações primárias 56dp fill accent (raio 8dp — spec A/F) -----------
    private View buildActions() {
        LinearLayout row = new LinearLayout(this);
        row.setOrientation(LinearLayout.HORIZONTAL);
        row.setPadding(0, dp(12), 0, dp(4));

        Button24 novo = new Button24(this, "Novo projeto",
                UiIcons.PLUS, ACCENT, ACCENT_PRESS);
        novo.setOnClickListener(v -> askNewProject());
        row.addView(novo.view(), new LinearLayout.LayoutParams(0, dp(56), 1f));

        Button24 imp = new Button24(this, "Importar projeto",
                UiIcons.UPLOAD, ACCENT, ACCENT_PRESS);
        LinearLayout.LayoutParams ip = new LinearLayout.LayoutParams(0, dp(56), 1f);
        ip.leftMargin = dp(12);
        imp.setOnClickListener(v -> pickFolder(REQ_PICK_TREE_IMPORT));
        row.addView(imp.view(), ip);
        return row;
    }

    private View buildSectionLabel() {
        TextView t = new TextView(this);
        t.setText("Meus projetos");
        t.setTextColor(TEXT2);
        t.setTextSize(12);
        t.setPadding(dp(4), dp(12), 0, dp(8));
        return t;
    }

    // ---- botão primário 56dp (fill accent, ícone + palavra, raio 8) -------
    private static final class Button24 {
        private final android.widget.Button b;

        Button24(Activity a, String label, int icon, int fill, int fillPress) {
            b = new android.widget.Button(a);
            b.setText(label);
            b.setAllCaps(false);
            b.setTextSize(15);
            b.setTextColor(TEXT1);
            b.setPadding(dp2(a, 16), 0, dp2(a, 16), 0);
            GradientDrawable n = new GradientDrawable();
            n.setColor(fill);
            n.setCornerRadius(dp2(a, 8));
            GradientDrawable p = new GradientDrawable();
            p.setColor(fillPress);
            p.setCornerRadius(dp2(a, 8));
            StateListDrawable st = new StateListDrawable();
            st.addState(new int[]{android.R.attr.state_pressed}, p);
            st.addState(new int[]{}, n);
            b.setBackground(st);
            b.setCompoundDrawablesWithIntrinsicBounds(
                    UiIcons.drawable(icon, TEXT1, 24, a.getResources()
                            .getDisplayMetrics().density), null, null, null);
            b.setCompoundDrawablePadding(dp2(a, 8));
            b.setStateListAnimator(null);   // sem elevação do Material
        }

        View view() {
            return b;
        }

        void setOnClickListener(View.OnClickListener l) {
            b.setOnClickListener(l);
        }

        static int dp2(Activity a, int v) {
            return Math.round(v * a.getResources().getDisplayMetrics().density);
        }
    }

    // ---- refresh: filtra (pesquisa) + ordena (dropdown) + estado em falta --
    private void refresh() {
        shown.clear();
        String q = search != null ? search.getText().toString().trim().toLowerCase()
                                  : "";
        for (VvProjects.Entry e : all) {
            if (!q.isEmpty() && !e.name.toLowerCase().contains(q)) {
                continue;
            }
            shown.add(e);
        }
        Collections.sort(shown, new Comparator<VvProjects.Entry>() {
            @Override
            public int compare(VvProjects.Entry a, VvProjects.Entry b) {
                return ProjectsFormat.compareEntries(
                        a.name, a.editedAt, a.createdAt,
                        b.name, b.editedAt, b.createdAt, sortMode);
            }
        });
        boolean empty = shown.isEmpty();
        emptyTitle.setVisibility(empty ? View.VISIBLE : View.GONE);
        emptySub.setVisibility(empty ? View.VISIBLE : View.GONE);
        grid.setVisibility(empty ? View.GONE : View.VISIBLE);
        sortBtn.setText("Ordenar: " + ProjectsFormat.sortLabel(sortMode));
        adapter.notifyDataSetChanged();
    }

    private void reload() {
        all.clear();
        all.addAll(VvProjects.load(this));
        // estado EM FALTA (spec F): pasta apagada fora da app → afere em
        // background (queries SAF) e volta à main thread
        final List<String> gone = new ArrayList<>();
        io.execute(() -> {
            for (VvProjects.Entry e : all) {
                if (!VvProjects.projectFolderExists(this, Uri.parse(e.uri))) {
                    gone.add(e.uri);
                }
            }
            main.post(() -> {
                missingUris.clear();
                missingUris.addAll(gone);
                refresh();
                // pede as miniaturas dos cards visíveis
                for (VvProjects.Entry e : shown) {
                    requestThumb(e);
                }
            });
        });
        refresh();
    }

    @Override
    protected void onResume() {
        super.onResume();
        reload();   // a lista/miniaturas mudaram (edição guardada no editor)
    }

    // ---- MINIATURAS (spec F): thumb.png do projeto → cache em memória ------
    private void requestThumb(VvProjects.Entry e) {
        if (THUMBS.get(e.uri) != null || missingUris.contains(e.uri)) {
            return;
        }
        synchronized (loadingThumbs) {
            if (loadingThumbs.contains(e.uri)) {
                return;
            }
            loadingThumbs.add(e.uri);
        }
        io.execute(() -> {
            Bitmap bmp = null;
            try {
                Uri tu = VvProjects.thumbUri(this, Uri.parse(e.uri));
                InputStream in = getContentResolver().openInputStream(tu);
                if (in != null) {
                    bmp = BitmapFactory.decodeStream(in);
                    in.close();
                }
            } catch (Exception ex) {
                bmp = null;   // sem thumb → default G (não é erro)
            }
            final Bitmap fb = bmp;
            main.post(() -> {
                synchronized (loadingThumbs) {
                    loadingThumbs.remove(e.uri);
                }
                if (fb != null) {
                    THUMBS.put(e.uri, fb);
                }
                adapter.notifyDataSetChanged();
            });
        });
    }

    // =========================================================================
    // ADAPTER DOS CARDS (grelha 2 colunas)
    // =========================================================================
    private final class CardsAdapter extends BaseAdapter {
        @Override
        public int getCount() {
            return shown.size();
        }

        @Override
        public Object getItem(int pos) {
            return shown.get(pos);
        }

        @Override
        public long getItemId(int pos) {
            return pos;
        }

        @Override
        public View getView(int pos, View cv, ViewGroup parent) {
            final VvProjects.Entry e = shown.get(pos);
            final boolean missing = missingUris.contains(e.uri);

            LinearLayout card = new LinearLayout(ProjectManagerActivity.this);
            card.setOrientation(LinearLayout.VERTICAL);
            // card: surface + raio 8dp + bordo 1dp (spec A/F); premido=surface2
            GradientDrawable nrm = new GradientDrawable();
            nrm.setColor(SURFACE);
            nrm.setCornerRadius(dp(8));
            nrm.setStroke(dp(1), BORDER);
            GradientDrawable prs = new GradientDrawable();
            prs.setColor(SURFACE2);
            prs.setCornerRadius(dp(8));
            prs.setStroke(dp(1), BORDER);
            StateListDrawable bg = new StateListDrawable();
            bg.addState(new int[]{android.R.attr.state_pressed}, prs);
            bg.addState(new int[]{}, nrm);
            card.setBackground(bg);
            card.setPadding(dp(8), dp(8), dp(8), dp(8));

            // ---- MINIATURA 16:9 (largura = coluna − paddings) ----
            int colW = (grid.getWidth() > 0 ? grid.getWidth()
                      : parent.getWidth()) / 2 - dp(16) - dp(16);
            int thumbH = Math.round(colW * 9f / 16f);
            FrameLayout thumb = new FrameLayout(ProjectManagerActivity.this);
            if (missing) {
                // ESTADO EM FALTA (spec F): fundo "rachado" (linhas diagonais
                // border) + ? grande + erro legível + recuperação
                thumb.addView(new MissingThumbView(ProjectManagerActivity.this));
            } else {
                Bitmap bmp = THUMBS.get(e.uri);
                if (bmp != null) {
                    ImageView iv = new ImageView(ProjectManagerActivity.this);
                    iv.setImageBitmap(bmp);
                    iv.setScaleType(ImageView.ScaleType.CENTER_CROP);
                    thumb.addView(iv, new FrameLayout.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT));
                } else if (loadingThumbs.contains(e.uri)) {
                    // ESTADO A CARREGAR: spinner centrado
                    ProgressBar spin = new ProgressBar(ProjectManagerActivity.this);
                    thumb.addView(spin, new FrameLayout.LayoutParams(
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                            ViewGroup.LayoutParams.WRAP_CONTENT,
                            Gravity.CENTER));
                } else {
                    // DEFAULT: o logo G.One (spec F: default = ícone G.One)
                    ImageView iv = new ImageView(ProjectManagerActivity.this);
                    iv.setImageResource(R.drawable.gone_logo);
                    iv.setScaleType(ImageView.ScaleType.FIT_CENTER);
                    iv.setPadding(dp(16), dp(8), dp(16), dp(8));
                    thumb.addView(iv, new FrameLayout.LayoutParams(
                            ViewGroup.LayoutParams.MATCH_PARENT,
                            ViewGroup.LayoutParams.MATCH_PARENT));
                }
            }
            card.addView(thumb, new LinearLayout.LayoutParams(
                    ViewGroup.LayoutParams.MATCH_PARENT,
                    Math.max(dp(80), thumbH)));

            // ---- nome 14sp bold + ⋮ 48dp ----
            LinearLayout row = new LinearLayout(ProjectManagerActivity.this);
            row.setOrientation(LinearLayout.HORIZONTAL);
            row.setGravity(Gravity.CENTER_VERTICAL);
            TextView name = new TextView(ProjectManagerActivity.this);
            name.setText(e.name);
            name.setTextColor(missing ? TEXT2 : TEXT1);
            name.setTextSize(14);
            name.setTypeface(null, android.graphics.Typeface.BOLD);
            name.setSingleLine(true);
            name.setEllipsize(android.text.TextUtils.TruncateAt.END);
            row.addView(name, new LinearLayout.LayoutParams(0,
                    ViewGroup.LayoutParams.WRAP_CONTENT, 1f));

            View dots = new View(ProjectManagerActivity.this) {
                @Override
                protected void onDraw(Canvas c) {
                    UiIcons.draw(c, UiIcons.DOTS, getWidth() / 2f,
                            getHeight() / 2f, dp(24), TEXT2);
                }
            };
            dots.setOnClickListener(v -> showCardMenu(e, dots));
            row.addView(dots, new LinearLayout.LayoutParams(dp(48), dp(48)));
            card.addView(row);

            // ---- tempo relativo 12sp text-2 ("há 2 h") ----
            TextView time = new TextView(ProjectManagerActivity.this);
            String rt = ProjectsFormat.relativeTime(
                    e.editedAt > 0 ? e.editedAt : e.createdAt);
            time.setText(missing ? "projeto em falta" : (rt.isEmpty() ? "—" : rt));
            time.setTextColor(missing ? WARN : TEXT2);
            time.setTextSize(12);
            time.setSingleLine(true);
            card.addView(time);

            // gestos (spec F): toque abre · toque longo → menu
            card.setOnClickListener(v -> {
                if (missing) {
                    offerRecoverMissing(e);
                } else {
                    openProject(e);
                }
            });
            card.setOnLongClickListener(v -> {
                showCardMenu(e, card);
                return true;
            });
            return card;
        }
    }

    /** miniatura do estado EM FALTA: linhas "rachadas" + ? + erro + recuperação */
    private final class MissingThumbView extends View {
        MissingThumbView(Activity a) {
            super(a);
            setBackgroundColor(SURFACE2);
        }

        @Override
        protected void onDraw(Canvas c) {
            super.onDraw(c);
            final android.graphics.Paint p = new android.graphics.Paint(
                    android.graphics.Paint.ANTI_ALIAS_FLAG);
            p.setStyle(android.graphics.Paint.Style.STROKE);
            p.setStrokeWidth(dp(1));
            p.setColor(BORDER);
            // "fundo rachado": diagonais irregulares
            for (int i = -1; i < 6; i++) {
                float x = i * getWidth() / 5f;
                c.drawLine(x, 0, x + getWidth() / 7f, getHeight(), p);
            }
            // ? grande + erro legível + recuperação (spec F)
            UiIcons.draw(c, UiIcons.QUESTION, getWidth() / 2f,
                    getHeight() * 0.40f, dp(40), WARN);
            p.setStyle(android.graphics.Paint.Style.FILL);
            p.setTextSize(dp(11));
            p.setColor(TEXT2);
            String msg = "pasta não encontrada";
            float w = p.measureText(msg);
            c.drawText(msg, (getWidth() - w) / 2f, getHeight() * 0.72f, p);
            String rec = "toque para recuperar";
            w = p.measureText(rec);
            p.setColor(ACCENT);
            c.drawText(rec, (getWidth() - w) / 2f, getHeight() * 0.86f, p);
        }
    }

    // ---- menus / diálogos (spec F + L) --------------------------------------

    private void showSortMenu() {
        PopupMenu pm = new PopupMenu(new android.view.ContextThemeWrapper(this,
                android.R.style.Theme_DeviceDefault_Dialog), sortBtn);
        pm.getMenu().add("Última Edição");
        pm.getMenu().add("Nome (A-Z)");
        pm.getMenu().add("Nome (Z-A)");
        pm.getMenu().add("Criado (recente)");
        pm.setOnMenuItemClickListener(mi -> {
            String t = mi.getTitle().toString();
            sortMode = 0;
            if (t.startsWith("Nome (A")) sortMode = 1;
            else if (t.startsWith("Nome (Z")) sortMode = 2;
            else if (t.startsWith("Criado")) sortMode = 3;
            getPreferences(MODE_PRIVATE).edit().putInt("sortMode", sortMode)
                    .apply();
            refresh();
            return true;
        });
        pm.show();
    }

    /** menu ⋮ / long-press do card (spec L): abrir/renomear/duplicar/apagar */
    private void showCardMenu(final VvProjects.Entry e, View anchor) {
        final boolean missing = missingUris.contains(e.uri);
        PopupMenu pm = new PopupMenu(new android.view.ContextThemeWrapper(this,
                android.R.style.Theme_DeviceDefault_Dialog), anchor);
        pm.getMenu().add(missing ? "Recuperar (re-escolher pasta)" : "Abrir");
        pm.getMenu().add("Renomear");
        pm.getMenu().add("Duplicar");
        pm.getMenu().add("Apagar");
        pm.setOnMenuItemClickListener(mi -> {
            String t = mi.getTitle().toString();
            if (t.startsWith("Abrir")) openProject(e);
            else if (t.startsWith("Recuperar")) recoverMissing(e);
            else if (t.equals("Renomear")) askRename(e);
            else if (t.equals("Duplicar")) askDuplicate(e);
            else if (t.equals("Apagar")) confirmDelete(e);
            return true;
        });
        pm.show();
    }

    /** [Novo projeto]: nome → picker SAF da pasta DESTE projeto */
    private void askNewProject() {
        final EditText input = themedInput(pendingName, "nome do projeto");
        new AlertDialog.Builder(dark())
                .setTitle("Nome do projeto")
                .setMessage("No passo seguinte escolha a PASTA onde este "
                        + "projeto fica (só dele).")
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

    private void askRename(final VvProjects.Entry e) {
        final EditText input = themedInput(e.name, "novo nome");
        new AlertDialog.Builder(dark())
                .setTitle("Renomear projeto")
                .setView(input)
                .setPositiveButton("OK", (d, w) -> {
                    String n = input.getText().toString().trim();
                    if (!n.isEmpty() && !n.equals(e.name)) {
                        VvProjects.renameProject(this, e, n);
                        reload();
                        Toast.makeText(this, "renomeado para “" + n + "”",
                                Toast.LENGTH_SHORT).show();
                    }
                })
                .setNegativeButton("Cancelar", null)
                .show();
        input.requestFocus();
    }

    private void askDuplicate(final VvProjects.Entry e) {
        new AlertDialog.Builder(dark())
                .setTitle("Duplicar projeto")
                .setMessage("Escolha a PASTA PAI que vai receber a cópia “"
                        + e.name + " copia”.\nTodo o conteúdo do projeto é "
                        + "copiado (cenas, meshes, texturas, áudio).")
                .setPositiveButton("Escolher pasta", (d, w) -> {
                    pendingDup = e;
                    pickFolder(REQ_PICK_TREE_DUP);
                })
                .setNegativeButton("Cancelar", null)
                .show();
    }

    /** APAGAR: card centrado (título 16sp · corpo 14sp text-2 · Cancelar/
     *  Apagar danger — spec L). SEM swipe-to-delete (spec F). */
    private void confirmDelete(final VvProjects.Entry e) {
        LinearLayout body = new LinearLayout(this);
        body.setOrientation(LinearLayout.VERTICAL);
        body.setPadding(dp(20), dp(8), dp(20), dp(8));
        TextView t = new TextView(this);
        t.setText("Apagar projeto");
        t.setTextColor(TEXT1);
        t.setTextSize(16);
        t.setTypeface(null, android.graphics.Typeface.BOLD);
        body.addView(t);
        TextView m = new TextView(this);
        m.setText("Apagar “" + e.name + "”?\n\nA pasta e TODOS os ficheiros "
                + "do projeto são apagados do armazenamento.\n"
                + "Não pode ser desfeito.");
        m.setTextColor(TEXT2);
        m.setTextSize(14);
        m.setPadding(0, dp(8), 0, dp(8));
        body.addView(m);
        LinearLayout btns = new LinearLayout(this);
        btns.setOrientation(LinearLayout.HORIZONTAL);
        android.widget.Button cancel = flatButton("Cancelar", SURFACE2, TEXT1);
        android.widget.Button del = flatButton("Apagar", DANGER, TEXT1);
        btns.addView(cancel, new LinearLayout.LayoutParams(0, dp(48), 1f));
        LinearLayout.LayoutParams lp = new LinearLayout.LayoutParams(0, dp(48), 1f);
        lp.leftMargin = dp(8);
        btns.addView(del, lp);
        body.addView(btns);
        final AlertDialog dlg = new AlertDialog.Builder(dark()).setView(body)
                .setCancelable(true).create();
        cancel.setOnClickListener(v -> dlg.dismiss());
        del.setOnClickListener(v -> {
            dlg.dismiss();
            boolean gone = VvProjects.deleteProject(this, e);
            releasePermission(e.uri);
            reload();
            Toast.makeText(this,
                    gone ? "projeto apagado (" + e.name + ")"
                         : "pasta não apagada — entrada removida da lista",
                    Toast.LENGTH_LONG).show();
        });
        dlg.show();
    }

    /** projeto em falta: recuperação = re-escolher a pasta OU remover da lista */
    private void offerRecoverMissing(final VvProjects.Entry e) {
        new AlertDialog.Builder(dark())
                .setTitle("Projeto em falta")
                .setMessage("A pasta de “" + e.name + "” não foi encontrada.\n\n"
                        + "Re-escolha a pasta para recuperar o projeto, ou "
                        + "remova a entrada da lista.")
                .setPositiveButton("Re-escolher pasta", (d, w) ->
                        recoverMissing(e))
                .setNeutralButton("Remover da lista", (d, w) -> {
                    removeFromList(e);
                    Toast.makeText(this, "entrada removida da lista",
                            Toast.LENGTH_SHORT).show();
                })
                .setNegativeButton("Cancelar", null)
                .show();
    }

    private void recoverMissing(final VvProjects.Entry e) {
        recoverTarget = e;
        pickFolder(REQ_PICK_TREE_RECOVER);
    }

    // alvo do fluxo RECUPERAR (re-apontar a pasta de um projeto em falta)
    private VvProjects.Entry recoverTarget;

    private android.widget.Button flatButton(String label, int fill, int ink) {
        android.widget.Button b = new android.widget.Button(this);
        b.setText(label);
        b.setAllCaps(false);
        b.setTextSize(15);
        b.setTextColor(ink);
        GradientDrawable g = new GradientDrawable();
        g.setColor(fill);
        g.setCornerRadius(dp(8));
        b.setBackground(g);
        b.setStateListAnimator(null);
        return b;
    }

    private EditText themedInput(String text, String hint) {
        final EditText input = new EditText(this);
        input.setSingleLine(true);
        input.setText(text);
        input.setSelection(input.getText().length());
        GradientDrawable g = new GradientDrawable();
        g.setColor(SURFACE);
        g.setStroke(dp(1), BORDER);
        g.setCornerRadius(dp(4));
        input.setBackground(g);
        input.setTextColor(TEXT1);
        input.setHintTextColor(TEXT2);
        input.setHint(hint);
        input.setPadding(dp(12), dp(10), dp(12), dp(10));
        return input;
    }

    private android.view.ContextThemeWrapper dark() {
        return new android.view.ContextThemeWrapper(this,
                android.R.style.Theme_DeviceDefault_Dialog);
    }

    // ---- pickers / resultados ----------------------------------------------

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
        if (req != REQ_PICK_TREE && req != REQ_PICK_TREE_IMPORT
                && req != REQ_PICK_TREE_DUP && req != REQ_PICK_TREE_RECOVER) {
            return;
        }
        if (res != RESULT_OK || data == null || data.getData() == null) {
            if (req != REQ_PICK_TREE_DUP) {
                Toast.makeText(this, "sem pasta — projeto não criado",
                        Toast.LENGTH_SHORT).show();
            }
            pendingDup = null;
            return;
        }
        Uri tree = data.getData();
        try {
            getContentResolver().takePersistableUriPermission(tree,
                    Intent.FLAG_GRANT_READ_URI_PERMISSION
                            | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        } catch (Exception e) {
            Log.e(TAG, "projetos: takePersistableUriPermission falhou", e);
        }

        // RECUPERAR (projeto em falta): re-aponta a entrada para a pasta
        // escolhida — sem copiar nada (a pasta JÁ é o projeto)
        if (req == REQ_PICK_TREE_RECOVER && recoverTarget != null) {
            final VvProjects.Entry tgt = recoverTarget;
            recoverTarget = null;
            List<VvProjects.Entry> ps = VvProjects.load(this);
            int i = VvProjects.indexOfUri(ps, tgt.uri);
            if (i >= 0) {
                // nome da pasta escolhida (se difere, o nome da lista segue
                // a pasta — o projeto é a pasta)
                ps.get(i).uri = tree.toString();
                ps.get(i).name = VvProjects.folderDisplayName(this, tree);
                VvProjects.save(this, ps);
                Toast.makeText(this, "projeto recuperado ("
                        + ps.get(i).name + ")", Toast.LENGTH_SHORT).show();
            }
            reload();
            return;
        }

        if (req == REQ_PICK_TREE_DUP && pendingDup != null) {
            // DUPLICAR: copia a pasta origem p/ DENTRO da escolhida (subpasta
            // "<nome> copia") — a nova entrada aponta para a SUBÁRVORE
            final VvProjects.Entry src = pendingDup;
            pendingDup = null;
            final String folderName = src.name.endsWith(" copia")
                    ? src.name : src.name + " copia";
            final android.app.ProgressDialog pd =
                    new android.app.ProgressDialog(dark());
            pd.setMessage("a duplicar “" + src.name + "”…");
            pd.setCancelable(false);
            pd.show();
            io.execute(() -> {
                boolean ok = VvProjects.copyTreeInto(this,
                        Uri.parse(src.uri), tree, folderName, null);
                final Uri newTree = ok
                        ? VvProjects.subfolderTreeUri(tree, folderName) : null;
                main.post(() -> {
                    pd.dismiss();
                    if (ok && newTree != null) {
                        VvProjects.Entry ne = VvProjects.addEntry(this,
                                folderName, newTree);
                        Toast.makeText(this, "duplicado: " + ne.name,
                                Toast.LENGTH_SHORT).show();
                    } else {
                        Toast.makeText(this, "duplicar falhou (ver log)",
                                Toast.LENGTH_LONG).show();
                    }
                    reload();
                });
            });
            return;
        }

        // criar/importar (fluxo de sempre)
        List<VvProjects.Entry> ps = VvProjects.load(this);
        int dup = VvProjects.indexOfUri(ps, tree.toString());
        if (dup >= 0) {
            Toast.makeText(this,
                    "esta pasta já é o projeto “" + ps.get(dup).name + "”",
                    Toast.LENGTH_LONG).show();
            return;
        }
        String name = pendingName;
        if (req == REQ_PICK_TREE_IMPORT) {
            name = VvProjects.folderDisplayName(this, tree);
        }
        VvProjects.createStructure(this, tree);
        VvProjects.Entry e = new VvProjects.Entry(name, tree.toString(),
                System.currentTimeMillis());
        ps.add(e);
        VvProjects.save(this, ps);
        reload();
        VvProjects.launchEditor(this, e);
    }

    private void openProject(VvProjects.Entry e) {
        VvProjects.launchEditor(this, e);
    }

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

    private void releasePermission(String uri) {
        try {
            getContentResolver().releasePersistableUriPermission(
                    Uri.parse(uri),
                    Intent.FLAG_GRANT_READ_URI_PERMISSION
                            | Intent.FLAG_GRANT_WRITE_URI_PERMISSION);
        } catch (Exception ignored) {
        }
    }

    private float density() {
        return getResources().getDisplayMetrics().density;
    }

    private int dp(int v) {
        return Math.round(v * density());
    }
}
