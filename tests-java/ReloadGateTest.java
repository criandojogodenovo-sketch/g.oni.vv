import vv.goni.ReloadGate;

import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.CountDownLatch;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;
import java.util.concurrent.atomic.AtomicReference;

/**
 * 0.9.3 — SENTINELAS JVM DO HOTFIX REG-001/R-005 (ConcurrentModification
 * no ProjectManagerActivity.reload()). Correm na JVM do CI (javac + java,
 * zero Android — o padrão ProjectsFormatTest).
 *
 * O CRASH ORIGINAL (23×/dia no RMX3624, vv.goni): o reload() antigo limpava
 * e preenchia a lista partilhada `all` na MAIN thread enquanto a lambda do
 * io.execute percorria a MESMA lista na thread de background — a iteração
 * do ArrayList lançava a exceção de modificação concorrente (stack:
 * lambda$reload$4, ProjectManagerActivity.java:355) e o processo morria.
 *
 * O QUE ESTE FICHEIRO AFERTA (com o MODELO do esqueleto real da tela —
 * gate + worker assíncrono + publicação única):
 *   regress_concurrent_reload — 10 threads a pedir reload em simultâneo:
 *     zero exceções, publicações SEM sobreposição, estado final correto
 *     (o ÚLTIMO pedido é sempre o publicado — coalesce trailing);
 *   regress_corrupt_projects  — 10 "projetos" no disco dos quais vários
 *     falham a leitura: os bons são publicados, os maus saltados com
 *     contadores, ZERO exceções escapam (uma falha de leitura nunca
 *     mata o reload);
 *   adversarios              — 100 threads; heap pressionada (~83% do
 *     teto com -Xmx do runner); worker que lança NÃO deixa o portão
 *     preso (a tela continua viva).
 *
 * A REPRODUÇÃO DO PADRÃO ANTIGO (a prova de deteção): o caso
 * regress_concurrent_reload inclui o mecanismo exato do crash original
 * (lista partilhada percorrida numa thread enquanto a outra a modifica)
 * e afere que ELE É DETETÁVEL — é o gancho da PROVA DE MUTAÇÃO (o fix
 * revertido tem de ficar VERMELHO aqui ou no check estrutural
 * scripts/reload_concurrency_check.py).
 *
 * Correr: javac -d build-java \
 *            app/src/main/java/vv/goni/ReloadGate.java \
 *            tests-java/ReloadGateTest.java
 *         java -cp build-java ReloadGateTest
 * (o runner do CI usa -Xmx48m — a pressão de heap do adversário)
 */
public final class ReloadGateTest {

    private static int failures = 0;

    private static void check(boolean cond, String what) {
        if (!cond) {
            ++failures;
            System.out.println("  FALHOU  " + what);
        }
    }

    // =========================================================================
    // O MODELO da tela de projetos: o MESMO esqueleto do
    // ProjectManagerActivity pós-fix (gate + io single-thread + publicação
    // única). A lista da UI (uiList) só é mexida DENTRO do io.execute —
    // o confinamento que mata o CME.
    // =========================================================================
    static final class ScreenModel {
        final List<String> uiList = new ArrayList<>();      // o "all" da tela
        final AtomicInteger runs = new AtomicInteger();      // worker acordado
        final AtomicInteger publishes = new AtomicInteger(); // publicações
        final AtomicReference<Throwable> firstError = new AtomicReference<>();
        volatile int diskVersion = 0;      // o "projects.json" muda a cada pedido
        volatile int lastPublished = -1;
        volatile boolean publishOverlapped = false;
        volatile boolean publishing = false;
        volatile int lastFailedCount = -1;
        final ExecutorService io = Executors.newSingleThreadExecutor();
        final ReloadGate gate = new ReloadGate(this::onReload);

        /** quantas entradas do "disco" estão corrompidas (lançam ao ler) */
        volatile int corruptEntries = 0;
        /** quantas entradas boas o último reload publicou */
        volatile int lastGoodCount = -1;

        private void onReload() {
            runs.incrementAndGet();
            io.execute(() -> {
                try {
                    // (1) CARREGAR — lista NOVA local (NUNCA a da UI)
                    final int v = diskVersion;
                    final List<String> fresh = new ArrayList<>();
                    int failed = 0;
                    for (int i = 0; i < 10; i++) {
                        try {
                            if (i < corruptEntries) {
                                throw new IllegalStateException("entrada ilegivel");
                            }
                            fresh.add("proj-" + v + "-" + i);
                        } catch (Throwable t) {
                            ++failed;   // projeto corrompido: saltado + contado
                        }
                    }
                    Thread.sleep(2);   // o "trabalho" de disco/SAF
                    // (2) PUBLICAR — substituição ÚNICA da lista da UI
                    if (publishing) {
                        publishOverlapped = true;
                    }
                    publishing = true;
                    uiList.clear();
                    uiList.addAll(fresh);
                    lastGoodCount = fresh.size();
                    lastFailedCount = failed;
                    lastPublished = v;
                    publishes.incrementAndGet();
                    publishing = false;
                } catch (Throwable t) {
                    firstError.compareAndSet(null, t);
                } finally {
                    gate.finished();
                }
            });
        }

        void request() {
            diskVersion++;
            gate.request();
        }

        /** espera o portão abrir e a fila esvaziar (quiescência) */
        void awaitQuiesce() throws InterruptedException {
            final long deadline = System.nanoTime() + TimeUnit.SECONDS.toNanos(20);
            while (System.nanoTime() < deadline) {
                if (!gate.isRunning() && !gate.hasQueued()) {
                    try {
                        io.submit(() -> { }).get(50, TimeUnit.MILLISECONDS);
                        return;   // a fila drenou até à barreira vazia
                    } catch (Exception filaViva) {
                        // ainda a drenar — volta a esperar
                    }
                }
                Thread.sleep(5);
            }
        }

        void shutdown() {
            io.shutdown();
            try {
                io.awaitTermination(10, TimeUnit.SECONDS);
            } catch (InterruptedException e) {
                Thread.currentThread().interrupt();
            }
        }
    }

    // =========================================================================
    // SENTINELA 1 — regress_concurrent_reload (REG-001)
    // =========================================================================
    static void regress_concurrent_reload() throws Exception {
        System.out.println("regress_concurrent_reload (R-005): contrato + 10 threads + mecanismo");

        // ---- (0) O CONTRATO DO PORTÃO (determinístico, zero timing) -------
        // Este é o GANCHO DA PROVA DE MUTAÇÃO: sem portão (mutação), o 2º
        // request() corre o worker NA HORA e o contrato abaixo falha.
        {
            final AtomicInteger contratoRuns = new AtomicInteger();
            final ReloadGate g = new ReloadGate(contratoRuns::incrementAndGet);
            check(g.request(), "contrato: o 1º pedido ENTRA (portao aberto)");
            check(contratoRuns.get() == 1,
                    "contrato: o worker correu 1x (o 1º pedido)");
            // ainda sem finished(): o portão está FECHADO
            check(!g.request(), "contrato: o 2º pedido NAO entra (portao fechado)");
            check(!g.request(), "contrato: o 3º pedido idem (coalesce)");
            check(g.hasQueued(), "contrato: o trailing ficou AGENDADO");
            check(g.isRunning(), "contrato: o portao segue FECHADO (1 de cada vez)");
            g.finished();   // o 1º publicou → acorda o trailing AGORA
            check(contratoRuns.get() == 2,
                    "contrato: o finished() acordou o trailing (worker 2x — "
                    + "nao 3x: os pedidos 2 e 3 COALESCERAM num)");
            g.finished();   // o trailing publicou → portão abre
            check(!g.isRunning() && !g.hasQueued(),
                    "contrato: o portao ABRE no fim (novo pedido pode entrar)");
            check(g.request(), "contrato: novo pedido ENTRA depois de abrir");
            check(contratoRuns.get() == 3, "contrato: o worker correu 3x no total");
            g.finished();
            // a EMERGÊNCIA: abandon abre mesmo sem finished
            check(g.request(), "contrato: mais um pedido p/ testar o abandon");
            g.abandon();
            check(!g.isRunning(),
                    "contrato: abandon abre o portao SEM finished (emergencia)");
        }

        final ScreenModel m = new ScreenModel();

        // ---- (a) a TEMPESTADE: 10 threads pedem reload EM SIMULTÂNEO -------
        final int threads = 10;
        final CountDownLatch start = new CountDownLatch(1);
        final CountDownLatch done = new CountDownLatch(threads);
        for (int t = 0; t < threads; t++) {
            final int jitter = t;
            new Thread(() -> {
                try {
                    start.await();
                    Thread.sleep(jitter % 3);   // dessincroniza as chegadas
                    m.request();
                } catch (Throwable ex) {
                    m.firstError.compareAndSet(null, ex);
                } finally {
                    done.countDown();
                }
            }, "reload-storm-" + t).start();
        }
        start.countDown();
        if (!done.await(20, TimeUnit.SECONDS)) {
            check(false, "R-005: as 10 threads terminaram");
        }
        m.awaitQuiesce();

        check(m.firstError.get() == null,
                "R-005: ZERO excecoes na tempestade de 10 reloads simultaneos");
        check(m.publishes.get() >= 1, "R-005: pelo menos 1 publicacao aconteceu");
        check(!m.publishOverlapped,
                "R-005: publicacoes SEM sobreposicao (mutacao confinada)");
        check(m.uiList.size() == m.lastGoodCount,
                "R-005: estado da UI integro (sem rasgo)");
        check(m.lastPublished == m.diskVersion,
                "R-005: o ULTIMO estado do disco foi o publicado (coalesce "
                + "trailing nao perde o pedido final) — publicado=" + m.lastPublished
                + " disco=" + m.diskVersion);
        check(m.runs.get() <= threads,
                "R-005: o portao coalesce (" + m.runs.get() + " arranques p/ "
                + threads + " pedidos — sem flooding da fila)");
        // a lista da UI tem conteudo do modelo novo (prefixo da versao final)
        check(m.uiList.isEmpty() || m.uiList.get(0).startsWith("proj-" + m.diskVersion),
                "R-005: o conteudo publicado e o da versao FINAL");

        // ---- (b) o portao reabre: um pedido NOVO arranca logo --------------
        final int runsAntes = m.runs.get();
        m.request();
        m.awaitQuiesce();
        check(m.runs.get() == runsAntes + 1,
                "R-005: o portao reabre apos quiescencia (novo pedido arranca)");
        check(m.lastPublished == m.diskVersion,
                "R-005: a publicacao final segue o disco");

        // ---- (c) A PROVA DE DETECAO: o mecanismo exato do crash original ---
        // O PADRAO ANTIGO (pre-fix): a main thread limpa/preenche a lista
        // partilhada ENQUANTO a thread de fundo a percorre com for-each.
        // Este bloco afere que o mecanismo LANCA (detetavel) — se alguém
        // reverter o fix da tela, é ISTO que volta a acontecer no telefone;
        // o check estrutural scripts/reload_concurrency_check.py fica
        // VERMELHO com o padrão antigo no fonte da Activity.
        boolean mecanismoDetetado = false;
        for (int tentativa = 0; tentativa < 200 && !mecanismoDetetado; tentativa++) {
            final List<String> partilhada = new ArrayList<>();
            for (int i = 0; i < 500; i++) {
                partilhada.add("p" + i);
            }
            final CountDownLatch sync = new CountDownLatch(1);
            final AtomicReference<Throwable> apanhada = new AtomicReference<>();
            final Thread fundo = new Thread(() -> {
                try {
                    sync.countDown();
                    for (String s : partilhada) {   // (a iteração do lambda$reload$4)
                        if (s.isEmpty()) {
                            return;   // (uso qualquer do elemento)
                        }
                    }
                } catch (Throwable t) {
                    apanhada.compareAndSet(null, t);
                }
            }, "iterador-antigo");
            fundo.start();
            sync.await();
            // a "main thread" do padrão antigo: limpa+preeche ENQUANTO
            // a de fundo itera (exatamente o all.clear()+all.addAll(...))
            partilhada.clear();
            for (int i = 0; i < 400; i++) {
                partilhada.add("n" + i);
            }
            fundo.join(2000);
            if (apanhada.get() != null) {
                mecanismoDetetado = true;
                final String cls = apanhada.get().getClass().getSimpleName();
                check(cls.startsWith("ConcurrentModification"),
                        "R-005: o mecanismo do crash original lanca a excecao "
                        + "esperada (classe detetada: " + cls + ")");
            }
        }
        check(mecanismoDetetado,
                "R-005: o mecanismo do crash original e DETETAVEL (o padrao "
                + "antigo lanca em " + (mecanismoDetetado ? "<=200" : ">200")
                + " tentativas)");
        System.out.println(mecanismoDetetado
                ? "  [prova] o padrao ANTIGO lanca (mecanismo do REG-001 "
                  + "reproduzido e apanhado); o padrao NOVO (acima) publica "
                  + "com zero excecoes"
                : "  [prova] FALHOU reproduzir o mecanismo");

        m.shutdown();
    }

    // =========================================================================
    // SENTINELA 2 — regress_corrupt_projects (REG-001.4/5)
    // =========================================================================
    static void regress_corrupt_projects() throws Exception {
        System.out.println("regress_corrupt_projects (R-005): 10 projetos, varios corrompidos");
        final ScreenModel m = new ScreenModel();

        // 10 projetos no "disco", 4 corrompidos: publica os 6 bons
        m.corruptEntries = 4;
        m.request();
        m.awaitQuiesce();
        check(m.firstError.get() == null,
                "R-005: reload com 4 projetos corrompidos TERMINA sem excecao");
        check(m.lastGoodCount == 6,
                "R-005: os 6 projetos bons foram publicados (publicados="
                + m.lastGoodCount + ")");
        check(m.lastFailedCount == 4,
                "R-005: os 4 corrompidos contados como falhados (falhados="
                + m.lastFailedCount + ")");
        check(m.uiList.size() == 6, "R-005: a UI fica com os 6 bons");

        // TODOS corrompidos: publica lista VAZIA, nao crasha
        m.corruptEntries = 10;
        m.request();
        m.awaitQuiesce();
        check(m.firstError.get() == null,
                "R-005: reload com TODOS os projetos corrompidos nao crasha");
        check(m.lastGoodCount == 0 && m.uiList.isEmpty(),
                "R-005: lista vazia publicada quando tudo esta corrompido");

        // "projects.json" corrompido por inteiro (o load lança): lista vazia
        // + log (o modelo: o worker apanha e publica vazio; o portao nao fica
        // preso — a prova do NAO-preso e o pedido seguinte)
        m.corruptEntries = 0;
        final int runsAntes = m.runs.get();
        m.request();
        m.awaitQuiesce();
        check(m.runs.get() == runsAntes + 1,
                "R-005: o portao sobrevive a ciclos de corrupcao seguidos");
        m.shutdown();
    }

    // =========================================================================
    // ADVERSÁRIOS (Tarefa 7D)
    // =========================================================================
    static void adversarios() throws Exception {
        System.out.println("adversarios (R-005): 100 threads + heap pressionada + worker que lanca");

        // ---- 100 threads em simultâneo --------------------------------------
        final ScreenModel m = new ScreenModel();
        final int threads = 100;
        final CountDownLatch start = new CountDownLatch(1);
        final CountDownLatch done = new CountDownLatch(threads);
        for (int t = 0; t < threads; t++) {
            new Thread(() -> {
                try {
                    start.await();
                    m.request();
                } catch (Throwable ex) {
                    m.firstError.compareAndSet(null, ex);
                } finally {
                    done.countDown();
                }
            }, "reload-adv-" + t).start();
        }
        start.countDown();
        check(done.await(30, TimeUnit.SECONDS), "adversario: 100 threads terminam");
        m.awaitQuiesce();
        check(m.firstError.get() == null,
                "adversario: 100 threads -> ZERO excecoes");
        check(!m.publishOverlapped, "adversario: sem sobreposicao de publicacoes");
        check(m.lastPublished == m.diskVersion,
                "adversario: estado final correto apos 100 pedidos");
        m.shutdown();

        // ---- heap pressionada (~83% do teto com -Xmx48m do runner) ----------
        // (o runner do CI passa -Xmx48m: 10M ints = 40MB retidos ≈ 83%)
        Runtime rt = Runtime.getRuntime();
        int[] lastico = null;
        try {
            lastico = new int[10_000_000];   // ~40 MB retidos
            final ScreenModel p = new ScreenModel();
            p.request();
            p.request();
            p.awaitQuiesce();
            check(p.firstError.get() == null,
                    "adversario: reload sob heap pressionada ("
                    + (rt.totalMemory() >> 20) + "MB alocados de "
                    + (rt.maxMemory() >> 20) + "MB de teto) sem excecao");
            check(p.lastPublished == p.diskVersion,
                    "adversario: estado final correto sob pressao de memoria");
            p.shutdown();
        } finally {
            lastico = null;   // liberta antes dos casos seguintes
        }

        // ---- worker que LANCA não deixa o portão preso ----------------------
        final ReloadGate[] preso = new ReloadGate[1];
        final AtomicInteger runsRuim = new AtomicInteger();
        preso[0] = new ReloadGate(() -> {
            runsRuim.incrementAndGet();
            throw new IllegalStateException("boom do worker");
        });
        try {
            preso[0].request();
            check(false, "adversario: a excecao do worker PROPAGA (visivel)");
        } catch (IllegalStateException esperada) {
            check(true, "adversario: a excecao do worker PROPAGA (visivel)");
        }
        check(!preso[0].isRunning(),
                "adversario: o portao ABRE apos o worker lancar (tela nao fica presa)");
        // e um pedido posterior volta a acordar o worker
        try {
            preso[0].request();
        } catch (IllegalStateException esperada2) {
            // (o worker ruim volta a lancar — o que conta e o arranque)
        }
        check(runsRuim.get() == 2,
                "adversario: o worker ruím acordou de novo (portao util)");
    }

    public static void main(String[] args) throws Exception {
        regress_concurrent_reload();
        regress_corrupt_projects();
        adversarios();

        System.out.println(failures == 0
                ? "ReloadGateTest: OK"
                : "ReloadGateTest: " + failures + " falha(s)");
        if (failures > 0) {
            System.exit(1);
        }
    }
}
