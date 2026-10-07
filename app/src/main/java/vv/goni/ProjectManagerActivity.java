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

    // ---- TOKENS 0.9.6.10 (espelho Java do ui/Theme.h — spec G:
    // GRAFITE+ÂMBAR+VIDRO, A REESCRITA DA APRESENTAÇÃO por ordem do dono).
    // O mono morreu no device (os fills brancos do accent eram BLOCOS
    // CEGANTES — o anti-exemplo do dono): a Paleta A OFICIAL é grafite
    // #0E0E10 + âmbar #FFB020 + vidro 80%. O ACCENT_INK continua ESCURO
    // (10,5:1 sobre o âmbar). Os CARDS mantêm o véu do vidro (GLA α0.80).
    static final int BG = 0xFF0E0E10;        // bg (o grafite)
    static final int SURFACE = 0xFF161618;   // surface (RGB p/ texto/estado)
    static final int SURFACE2 = 0xFF202023;  // surface-2 (premido)
    static final int BORDER = 0xFF2E2E32;    // border
    static final int TEXT1 = 0xFFECECEE;     // text-1
    static final int TEXT2 = 0xFFA6A6AD;     // text-2 (7,5:1)
    static final int ACCENT = 0xFFFFB020;    // accent — O ÂMBAR (a spec G)
    static final int ACCENT_PRESS = 0xFFE09A00;
    static final int ACCENT_INK = 0xFF0E0E10; // tinta SOBRE âmbar (10,5:1)
    static final int ACCENT_DIM = 0xFF4A3714; // o fill da seleção (âmbar 25%)
    static final int SURFACE_GLA = 0xCC161618; // vidro: α0.80 (cards/sheets)
    static final int SURFACE2_GLA = 0xDB202023; // vidro denso: α0.86
    static final int DANGER = 0xFFE5484D;
    static final int WARN = 0xFFFF8A3D;      // LARANJA (nunca == âmbar)
    static final int OK = 0xFF46A758;

    private final List<VvProjects.Entry> all = new ArrayList<>();   // fonte (só a MAIN thread mexe — REG-001)
    private final List<VvProjects.Entry> shown = new ArrayList<>(); // filtro+ordem (idem)
    private final List<String> missingUris = new ArrayList<>();     // estado F (idem)

    private GridView grid;
    private LinearLayout emptyBox;   // FASE 9 (G1-4a): o CAIXA inteiro desliga
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
    // REG-001/R-005 (hotfix 0.9.3): o PORTÃO do reload — 1 de cada vez +
    // coalesce trailing; a thread de fundo NUNCA toca em all/shown/missingUris
    private final ReloadGate reloadGate = new ReloadGate(this::doReload);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        sortMode = getPreferences(MODE_PRIVATE).getInt("sortMode", 0);

        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        root.setBackgroundColor(BG);
        root.setPadding(dp(16), dp(8), dp(16), dp(8));

        // FASE 9 (G1-4b): cabeçalho NUMA LINHA — ícone+título à esquerda;
        // pesquisa ao centro (flex); ordenar SÓ ÍCONE (texto no menu);
        // "Novo projeto" preenchido azul; "Importar projeto" só contorno
        root.addView(buildTopBar());

        // corpo: grelha + empty-state sobrepostos
        FrameLayout body = new FrameLayout(this);
        grid = new GridView(this);
        // FASE 9 (G1-4c): GRELHA ADAPTÁVEL — colunas = largura útil ÷ 180dp
        // (AUTO_FIT + columnWidth faz EXATAMENTE essa divisão na largura
        // REAL do ecrã — nada de 2 fixo, nada específico de telemóvel)
        grid.setNumColumns(GridView.AUTO_FIT);
        grid.setColumnWidth(dp(180));
        grid.setHorizontalSpacing(dp(16));
        grid.setVerticalSpacing(dp(16));
        grid.setStretchMode(GridView.STRETCH_COLUMN_WIDTH);
        grid.setSelector(new ColorDrawable(Color.TRANSPARENT)); // sem halo
        // mínimo 2 colunas (ecrãs estreitos — a regra do mock)
        grid.getViewTreeObserver().addOnGlobalLayoutListener(() -> {
            if (grid.getNumColumns() > 0 && grid.getNumColumns() < 2) {
                grid.setNumColumns(2);
            }
        });
        adapter = new CardsAdapter();
        grid.setAdapter(adapter);
        body.addView(grid, new FrameLayout.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT,
                ViewGroup.LayoutParams.MATCH_PARENT));

        // EMPTY STATE (spec F/M): ícone + convite a criar o 1º projeto.
        // FASE 9 (G1-4a): o CAIXA INTEIRO vive no FrameLayout DEPOIS do
        // grid (z-order ACIMA); a versão antiga só escondia emptyTitle /
        // emptySub — o bigLogo 96dp cinzento (~190px @2x) ficava VISÍVEL
        // com projetos, FIXO em coords de ecrã (não rolava com o grid) e
        // TAPAVA os cards — o "quadrado cinzento fantasma" do dono.
        // 0.9.6.18 (HOTFIX D6): o bigLogo deixa de ser o PNG (gone_logo) —
        // é o glifo da marca DESENHADO pela ÚNICA função (UiIcons.drawBrand,
        // o espelho Java do Brand.cpp — a paridade dos 4 sítios; 96dp ≥ 32
        // = a versão COMPLETA, setas com pontas), SEM fundo (âmbar sobre o
        // vidro do ecrã, nunca âmbar sobre âmbar)
        emptyBox = new LinearLayout(this);
        emptyBox.setOrientation(LinearLayout.VERTICAL);
        emptyBox.setGravity(Gravity.CENTER);
        View bigLogo = new BrandView(this, ACCENT, /*lodFull=*/true);
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
        // Problema 3 (hotfix 0.9.3): memória no ARRANQUE logada (evidência
        // p/ o dono comparar com o FinalizerWatchdog dos tombstones antigos)
        logStartupMemory("gestor: onCreate");
        reload();
    }

    /**
     * Problema 3 — uso de memória no arranque (Debug.getMemoryInfo + heap
     * Java): uma linha informativa; NUNCA bloqueia nem crasha o arranque.
     */
    private void logStartupMemory(String where) {
        try {
            final android.os.Debug.MemoryInfo mi = new android.os.Debug.MemoryInfo();
            android.os.Debug.getMemoryInfo(mi);
            final Runtime rt = Runtime.getRuntime();
            Log.i(TAG, where + ": memoria — dalvikPss=" + mi.dalvikPss
                    + "KB nativePss=" + mi.nativePss + "KB totalPss=" + mi.getTotalPss()
                    + "KB | heap java " + (rt.totalMemory() >> 10) + "KB/"
                    + (rt.maxMemory() >> 10) + "KB (livre "
                    + (rt.freeMemory() >> 10) + "KB)");
        } catch (Throwable t) {
            Log.w(TAG, where + ": Debug.getMemoryInfo indisponivel", t);
        }
    }

    // ---- FASE 9 (G1-4b): O CABEÇALHO NUMA LINHA -----------------------------
    // [logo 32][G.One VV] [pesquisa flex] [ordenar 48] [Novo projeto FILL]
    // [Importar projeto CONTORNO] — tudo em dp; em paisagem 20:9 (806dp
    // úteis) cabe sem rolar e sobra altura para uma fila de cards inteira
    private View buildTopBar() {
        LinearLayout bar = new LinearLayout(this);
        bar.setOrientation(LinearLayout.HORIZONTAL);
        bar.setGravity(Gravity.CENTER_VERTICAL);
        bar.setPadding(0, dp(4), 0, dp(4));

        // 0.9.6.18 (HOTFIX D6): o logo do cabeçalho é o GLIFO DESENHADO
        // pela ÚNICA função (a paridade da marca — o mesmo BrandView do
        // empty-state; 32dp = a versão completa), não um PNG solto
        View logo = new BrandView(this, ACCENT, /*lodFull=*/true);
        bar.addView(logo, new LinearLayout.LayoutParams(dp(32), dp(32)));
        TextView title = new TextView(this);
        title.setText("G.One VV");
        title.setTextColor(TEXT1);
        title.setTextSize(16);
        title.setTypeface(null, android.graphics.Typeface.BOLD);
        LinearLayout.LayoutParams tp = new LinearLayout.LayoutParams(
                ViewGroup.LayoutParams.WRAP_CONTENT, dp(48));
        tp.leftMargin = dp(8);
        bar.addView(title, tp);

        // pesquisa ao CENTRO (flex — o que sobrar da linha é dela)
        search = new EditText(this);
        search.setSingleLine(true);
        search.setTextSize(14);
        search.setTextColor(TEXT1);
        search.setHintTextColor(TEXT2);
        search.setHint("pesquisar projetos");
        search.setPadding(dp(12), 0, dp(12), 0);
        GradientDrawable sf = new GradientDrawable();
        sf.setColor(SURFACE_GLA);   // 0.9.6.9: o campo também é vidro (α0.88)
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
        LinearLayout.LayoutParams sp = new LinearLayout.LayoutParams(
                0, dp(48), 1f);
        sp.leftMargin = dp(12);
        sp.rightMargin = dp(8);
        bar.addView(search, sp);

        // ORDENAR: SÓ ÍCONE (48dp) — o texto vive no MENU; o ⋮ decorativo
        // ao lado foi REMOVIDO (não fazia nada: o clique era no botão todo)
        sortBtn = new TextView(this, null, 0);
        sortBtn.setGravity(Gravity.CENTER);
        sortBtn.setCompoundDrawablesWithIntrinsicBounds(
                UiIcons.drawable(UiIcons.SORT, TEXT2, 24, density()), null,
                null, null);
        sortBtn.setContentDescription("ordenar");
        sortBtn.setOnClickListener(v -> showSortMenu());
        bar.addView(sortBtn, new LinearLayout.LayoutParams(
                dp(48), dp(48)));

        // [Novo projeto] — preenchido azul (fill accent, raio 8dp)
        Button24 novo = new Button24(this, "Novo projeto",
                UiIcons.PLUS, ACCENT, ACCENT_PRESS, false);
        novo.setOnClickListener(v -> askNewProject());
        LinearLayout.LayoutParams np = new LinearLayout.LayoutParams(
                0, dp(56), 1f);
        np.leftMargin = dp(12);
        bar.addView(novo.view(), np);

        // [Importar projeto] — SÓ CONTORNO (bordo accent 1dp, fundo
        // transparente, texto/ícone accent — o par do preenchido)
        Button24 imp = new Button24(this, "Importar projeto",
                UiIcons.UPLOAD, 0, 0, true);
        LinearLayout.LayoutParams ip = new LinearLayout.LayoutParams(
                0, dp(56), 1.15f);
        ip.leftMargin = dp(8);
        imp.setOnClickListener(v -> pickFolder(REQ_PICK_TREE_IMPORT));
        bar.addView(imp.view(), ip);
        return bar;
    }

    // ---- botão primário 56dp (fill accent ou CONTORNO, raio 8) ------------
    private static final class Button24 {
        private final android.widget.Button b;

        Button24(Activity a, String label, int icon, int fill, int fillPress,
                 boolean outline) {
            b = new android.widget.Button(a);
            b.setText(label);
            b.setAllCaps(false);
            b.setTextSize(15);
            b.setTextColor(outline ? ACCENT : ACCENT_INK);
            b.setPadding(dp2(a, outline ? 10 : 16), 0,
                         dp2(a, outline ? 10 : 16), 0);
            GradientDrawable n = new GradientDrawable();
            if (outline) {
                // FASE 9 (G1-4b): SÓ CONTORNO — fundo transparente + bordo
                // accent 1dp (o par do preenchido)
                n.setColor(Color.TRANSPARENT);
                n.setStroke(dp2(a, 1), ACCENT);
            } else {
                n.setColor(fill);
            }
            n.setCornerRadius(dp2(a, 8));
            GradientDrawable p = new GradientDrawable();
            if (outline) {
                p.setColor(SURFACE2);
                p.setStroke(dp2(a, 1), ACCENT);
            } else {
                p.setColor(fillPress);
            }
            p.setCornerRadius(dp2(a, 8));
            StateListDrawable st = new StateListDrawable();
            st.addState(new int[]{android.R.attr.state_pressed}, p);
            st.addState(new int[]{}, n);
            b.setBackground(st);
            b.setCompoundDrawablesWithIntrinsicBounds(
                    UiIcons.drawable(icon, outline ? ACCENT : ACCENT_INK, 24,
                                     a.getResources()
                                             .getDisplayMetrics().density),
                    null, null, null);
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
        // FASE 9 (G1-4a): o CAIXA do empty-state inteiro (logo INCLUÍDO)
        // desliga com projetos — o bigLogo era o quadrado fantasma
        emptyBox.setVisibility(empty ? View.VISIBLE : View.GONE);
        grid.setVisibility(empty ? View.GONE : View.VISIBLE);
        adapter.notifyDataSetChanged();
    }

    // =========================================================================
    // RELOAD — REG-001/R-005 (hotfix 0.9.3)
    //
    // O BUG (23 crashes num dia, vv.goni@RMX3624): a versão antiga fazia
    // all.clear()+all.addAll(VvProjects.load()) na MAIN thread (onCreate +
    // onResume + pós-operações) enquanto a lambda do io.execute iterava `all`
    // na thread de background (pool-2-thread-1) → a iteração do ArrayList
    // lançava a exceção de modificação concorrente (stack trace exato:
    // lambda$reload$4, ProjectManagerActivity.java:355) e o processo morria;
    // reabrir morria outra vez (loop).
    //
    // O PADRÃO SEGURO (as 4 regras, afervadas pelos sentinelas da JVM):
    //   1. reload() só pede; o PORTÃO ReloadGate deixa passar 1 de cada
    //      vez (coalesce trailing — a Tempestade onCreate+onResume+… nunca
    //      põe 2 loads a correr em paralelo sobre os mesmos dados);
    //   2. a thread de fundo constrói lista NOVA LOCAL e NUNCA toca em
    //      all/shown/missingUris (o snapshot `fresh` é dela);
    //   3. a publicação acontece na MAIN thread e substitui `all` DE UMA
    //      VEZ (clear+addAll) — mutação confinada a uma thread;
    //   4. cada entrada é lida dentro de try/catch: projeto corrompido/
    //      inacessível = saltado com Log.w + contadores, NUNCA crash.
    // =========================================================================
    private void reload() {
        Log.i(TAG, "projetos: reload pedido");
        reloadGate.request();
    }

    /** chamado 1× de cada vez pelo portão; todo o trabalho pesado na io */
    private void doReload() {
        try {
            io.execute(() -> {
                // (1) CARREGAR — lista NOVA local; projects.json corrompido
                //     ou ilegível vira lista vazia COM log (VvProjects.load é
                //     defensivo por dentro); uma falha aqui NUNCA mata o app
                final List<VvProjects.Entry> fresh = new ArrayList<>();
                try {
                    final List<VvProjects.Entry> loaded = VvProjects.load(this);
                    if (loaded != null) {
                        fresh.addAll(loaded);
                    }
                } catch (Throwable t) {
                    Log.e(TAG, "projetos: leitura da lista FALHOU — lista "
                            + "vazia (a tela segue)", t);
                }
                // (2) 1ª PUBLICAÇÃO (main thread): a lista aparece LOGO
                //     (mesmo timing da versão antiga — o estado EM FALTA é
                //     que é assíncrono, como sempre foi)
                main.post(() -> {
                    all.clear();
                    all.addAll(fresh);   // cópia de referências: `fresh` não volta a ser escrito
                    refresh();
                });
                // (3) ESTADO EM FALTA (spec F): a pasta ainda existe? —
                //     iteramos o SNAPSHOT LOCAL (nunca `all`); falha de
                //     query = projeto em falta (card de recuperação)
                final List<String> gone = new ArrayList<>();
                for (VvProjects.Entry e : fresh) {
                    try {
                        if (!VvProjects.projectFolderExists(this,
                                Uri.parse(e.uri))) {
                            gone.add(e.uri);
                        }
                    } catch (Throwable t) {
                        Log.w(TAG, "projetos: pasta de '" + e.name
                                + "' inacessivel — marcada em falta", t);
                        gone.add(e.uri);
                    }
                }
                // (4) 2ª PUBLICAÇÃO (main thread): em-falta + miniaturas +
                //     contadores + o portão liberta (ou acorda o trailing)
                main.post(() -> {
                    missingUris.clear();
                    missingUris.addAll(gone);
                    refresh();
                    // pede as miniaturas dos cards visíveis
                    for (VvProjects.Entry e : shown) {
                        requestThumb(e);
                    }
                    Log.i(TAG, "projetos: reload ok — " + fresh.size()
                            + " projeto(s), " + gone.size() + " em falta");
                    reloadGate.finished();
                });
            });
        } catch (Throwable t) {
            // a fila recusou (activity a morrer) — o portão abre SEM crash
            Log.e(TAG, "projetos: reload não agendado (fila encerrada?)", t);
            reloadGate.abandon();
        }
    }

    @Override
    protected void onResume() {
        super.onResume();
        reload();   // a lista/miniaturas mudaram (edição guardada no editor)
    }

    // ---- MINIATURAS (spec F): thumb.png do projeto → cache em memória ------
    // 0.9.6.19 (HOTFIX D16): a LEITURA REPETE — o engine escreve o thumb.png
    // no thread de jobs DEPOIS do finish() (a captura é off-thread por
    // desenho, D7): o primeiro openInputStream do gestor podia chegar ANTES
    // da escrita terminar e o card ficava em iniciais ATÉ ao próximo onResume
    // (a falha nunca se repetia na mesma sessão). Agora: repetições com
    // espera por card — o thumb que chega tarde é apanhado na mesma sessão;
    // cada tentativa loga (o dono segue a cura no logcat).
    private static final long[] THUMB_RETRY_MS = {250, 500, 1000};
    private final java.util.HashMap<String, Integer> thumbRetries =
            new java.util.HashMap<>();

    private void requestThumb(VvProjects.Entry e) {
        if (THUMBS.get(e.uri) != null) {
            thumbRetries.remove(e.uri);   // chegou — o contador morre
            return;
        }
        if (missingUris.contains(e.uri)) {
            return;   // projeto EM FALTA não tem thumb para ler (spec F)
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
                bmp = null;   // sem thumb (ainda) → fallback/retry (não é erro)
            }
            if (bmp == null) {
                // 0.9.6.18 (HOTFIX D7): o WARN honesto — o save nunca
                // bloqueou e o card NUNCA mostra o ícone da app: mostra as
                // iniciais (o aviso é o caminho do fallback assertado)
                Log.w(TAG, "projetos: thumb.png indisponível para '"
                        + e.name + "' — card usa iniciais (fallback)");
            }
            final Bitmap fb = bmp;
            main.post(() -> {
                synchronized (loadingThumbs) {
                    loadingThumbs.remove(e.uri);
                }
                if (fb != null) {
                    THUMBS.put(e.uri, fb);
                    thumbRetries.remove(e.uri);
                    adapter.notifyDataSetChanged();
                    return;
                }
                // 0.9.6.19 (D16): a leitura REPETE — a escrita do engine
                // (thread de jobs, D7) pode chegar DEPOIS do onResume
                final int attempt = thumbRetries.containsKey(e.uri)
                        ? thumbRetries.get(e.uri) : 0;
                if (attempt < THUMB_RETRY_MS.length) {
                    thumbRetries.put(e.uri, attempt + 1);
                    Log.i(TAG, "projetos: thumb de '" + e.name
                            + "' lê outra vez em " + THUMB_RETRY_MS[attempt]
                            + "ms (tentativa " + (attempt + 1) + "/"
                            + THUMB_RETRY_MS.length + ")");
                    main.postDelayed(() -> requestThumb(e),
                            THUMB_RETRY_MS[attempt]);
                    return;   // o card fica com as iniciais nesta passagem
                }
                thumbRetries.remove(e.uri);   // esgotou — fica o fallback
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
            // card: vidro + raio 8dp + bordo 1dp (spec F); premido=vidro denso
            GradientDrawable nrm = new GradientDrawable();
            nrm.setColor(SURFACE_GLA);   // 0.9.6.9: o CARD é vidro (α0.88)
            nrm.setCornerRadius(dp(8));
            nrm.setStroke(dp(1), BORDER);
            GradientDrawable prs = new GradientDrawable();
            prs.setColor(SURFACE2_GLA);  // premido: vidro denso (α0.92)
            prs.setCornerRadius(dp(8));
            prs.setStroke(dp(1), BORDER);
            StateListDrawable bg = new StateListDrawable();
            bg.addState(new int[]{android.R.attr.state_pressed}, prs);
            bg.addState(new int[]{}, nrm);
            card.setBackground(bg);
            card.setPadding(dp(8), dp(8), dp(8), dp(8));

            // ---- MINIATURA 16:9 (largura = coluna REAL da grelha) ----
            // FASE 9 (G1-4c): colunas ADAPTÁVEIS (AUTO_FIT ÷ 180dp) — a
            // largura vem do nº de colunas REAL, não de um /2 fixo
            int cols = grid.getNumColumns();
            if (cols < 2) {
                cols = 2;
            }
            int colW = (grid.getWidth() > 0 ? grid.getWidth()
                      : parent.getWidth()) / cols - dp(16);
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
                    // 0.9.6.18 (HOTFIX D7): o DEFAULT é o tile de INICIAIS
                    // (máx 2) + matiz sóbrio de hash sobre grafite (paleta
                    // fixa de 6) — o dono: «O ícone da app NUNCA é conteúdo
                    // de card» (o gone_logo aqui era a galeria de
                    // placeholders); o card sem thumb.png é DISTINTO dos
                    // outros pelo hash do nome
                    thumb.addView(new InitialsThumbView(
                            ProjectManagerActivity.this, e.name),
                            new FrameLayout.LayoutParams(
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

    /**
     * 0.9.6.18 (HOTFIX D6) — o VIEW da marca: desenha o glifo pela ÚNICA
     * função (UiIcons.drawBrand). O mesmo view no cabeçalho (32dp) e no
     * empty-state (96dp) — a paridade da marca no lado Java.
     */
    private static final class BrandView extends View {
        private final int color;
        private final boolean lodFull;

        BrandView(Activity a, int color, boolean lodFull) {
            super(a);
            this.color = color;
            this.lodFull = lodFull;
        }

        @Override
        protected void onDraw(Canvas c) {
            super.onDraw(c);
            UiIcons.drawBrand(c, getWidth() / 2f, getHeight() / 2f,
                    Math.min(getWidth(), getHeight()), color, lodFull);
        }
    }

    /**
     * 0.9.6.18 (HOTFIX D7) — o FALLBACK determinístico do card sem
     * thumb.png: tile de INICIAIS do nome (máx 2) + matiz sóbrio de hash
     * sobre grafite (paleta FIXA de 6 matizes). NUNCA o ícone da app
     * (a regra do dono); os cards distintos entre si pelo hash do nome.
     * 0.9.6.18b — a CLASSE DEIXA DE SER static (a densidade vem da
     * activity por herança do contexto interno) e SEM membros static no
     * corpo (o nível de linguagem 1.8 do AGP não os leva em classe
     * interna); o bordo é STROKE com o dp da casa (o FILL pintava a laje
     * TODA de BORDER e o setStroke não existe em Paint — era
     * setStrokeWidth).
     */
    private final class InitialsThumbView extends View {
        // a paleta fixa (6 matizes SÓBRIOS — saturação contida, todos
        // legíveis sobre o grafite da casa; NUNCA cores vivas de placeholder)
        private final int[] HUES = {
                0xFFE09A00,   // âmbar (a cor da casa, escurecida)
                0xFFC4573B,   // terracota
                0xFF3E8E7E,   // azul-petróleo
                0xFF5B7FA6,   // azul-ardósia
                0xFF6E8B3D,   // musgo
                0xFF8B6FA0,   // malva
        };
        private final String initials;
        private final int hue;

        InitialsThumbView(Activity a, String name) {
            super(a);
            this.initials = initialsOf(name);
            // o matiz: hash POSITIVO do nome sobre a paleta FIXA (o mesmo
            // nome → sempre o mesmo matiz — determinístico, sem aleatório)
            this.hue = HUES[Math.abs(name.hashCode()) % HUES.length];
        }

        /** as iniciais (0.9.6.19 · HOTFIX D18): 2+ palavras → a inicial das
         *  DUAS primeiras; 1 palavra → a PRIMEIRA + a ÚLTIMA letra
         *  (projetoyygf → "PF", prooksnsn → "PN" — todas saíam "PR" com a
         *  regra antiga das duas primeiras letras; o pin: projetos com
         *  prefixo comum distinguem-se por iniciais ou matiz); 1 letra →
         *  ela própria; vazio → "·" (o tile nunca fica mudo) */
        private String initialsOf(String name) {
            if (name == null || name.trim().isEmpty()) {
                return "·";
            }
            String[] words = name.trim().split("\\s+");
            if (words.length >= 2) {
                return (words[0].substring(0, 1)
                        + words[1].substring(0, 1)).toUpperCase();
            }
            String w = words[0];
            if (w.length() == 1) {
                return w.toUpperCase();
            }
            return (w.substring(0, 1)
                    + w.substring(w.length() - 1)).toUpperCase();
        }

        @Override
        protected void onDraw(Canvas c) {
            super.onDraw(c);
            final android.graphics.Paint p = new android.graphics.Paint(
                    android.graphics.Paint.ANTI_ALIAS_FLAG);
            // o tile: grafite da casa + bordo 1dp + iniciais BOLD no matiz
            p.setStyle(android.graphics.Paint.Style.FILL);
            p.setColor(0xFF2E2E32);   // o SURFACE2 da casa
            c.drawRect(0, 0, getWidth(), getHeight(), p);
            p.setStyle(android.graphics.Paint.Style.STROKE);
            final float sw = dp(1);   // o dp da CASA (o dp2 local morreu — duplicava a derivação)
            p.setStrokeWidth(sw);
            p.setColor(0xFF4A3714);   // o BORDER da casa
            c.drawRect(sw / 2f, sw / 2f, getWidth() - sw / 2f,
                    getHeight() - sw / 2f, p);
            p.setColor(hue);
            p.setTextAlign(android.graphics.Paint.Align.CENTER);
            p.setFakeBoldText(true);
            p.setTextSize(Math.min(getWidth(), getHeight()) * 0.38f);
            final float base = getHeight() / 2f
                    - (p.descent() + p.ascent()) / 2f;
            c.drawText(initials, getWidth() / 2f, base, p);
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
        // FASE 9 (G1-4b): "Última edição" em minúscula (depois do nome
        // próprio); o TEXTO vive só aqui — o botão é ícone
        pm.getMenu().add("Última edição");
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
        g.setColor(SURFACE_GLA);   // 0.9.6.9: o campo de texto é vidro (α0.88)
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
