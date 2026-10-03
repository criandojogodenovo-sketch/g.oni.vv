package vv.goni;

import java.util.concurrent.atomic.AtomicBoolean;

/**
 * REG-001 / R-005 — o PORTÃO do reload() da tela de projetos (hotfix 0.9.3).
 *
 * O CRASH (23 ocorrências num dia no realme RMX3624, process vv.goni):
 * {@code ProjectManagerActivity.reload()} limpava e preenchia a lista
 * partilhada {@code all} na MAIN thread (chamado do onCreate E do onResume,
 * mais os pós-criar/apagar/renomear) enquanto a lambda do io.execute
 * percorria a MESMA lista na thread de background (pool-2-thread-1) — o
 * for-each do ArrayList lançava ConcurrentModificationException e o processo
 * morria; ao reabrir, morria outra vez (crash loop).
 *
 * O QUE ESTE PORTÃO GARANTE (as regras do fix):
 *   1. UM reload de cada vez — request() quando já está a correr NÃO
 *      agenda trabalho duplicado: marca apenas "há um pedido a mais";
 *   2. COALESCE TRAILING — pedidos em curso não se perdem: o ÚLTIMO
 *      estado do disco é sempre publicado (o finished() acorda um reload
 *      final se alguém pediu entretanto);
 *   3. o portão NUNCA fica fechado poruma falha — abandon() abre-o de
 *      emergência (exceção ao agendar) e o contrato do Worker é terminar
 *      SEMPRE com finished() (mesmo em falha total, com lista vazia).
 *
 * A thread de fundo que o Worker lança tem o SEU contrato próprio (o
 * padrão seguro do REG-001): construir lista NOVA local e publicar na
 * main thread de uma só vez — NUNCA tocar na lista da UI durante o load.
 * Isso vive no ProjectManagerActivity.doReload(); os sentinelas da JVM
 * (tests-java/ReloadGateTest.java, casos regress_concurrent_reload e
 * regress_corrupt_projects) afervam este portão com 10/100 threads.
 *
 * Java PURO (zero dependências de Android): compila e corre na JVM do
 * CI (o mesmo padrão do ProjectsFormat).
 */
public final class ReloadGate {

    /** o trabalho de carregar + publicar (o portão chama-o 1× de cada vez) */
    public interface Worker {
        void run();
    }

    private final AtomicBoolean running = new AtomicBoolean(false);
    private final AtomicBoolean queued = new AtomicBoolean(false);
    private final Worker worker;

    public ReloadGate(Worker worker) {
        if (worker == null) {
            throw new IllegalArgumentException("worker obrigatorio");
        }
        this.worker = worker;
    }

    /**
     * Pede um reload. Devolve true se ESTE chamador acordou o trabalho
     * (portão estava aberto); false se ficou agendado como trailing.
     * Nunca lança: uma exceção do Worker abre o portão de volta.
     */
    public boolean request() {
        if (running.compareAndSet(false, true)) {
            safeRun();
            return true;
        }
        // já está a correr: agenda um trailing (coalesce — nunca perder o
        // pedido FINAL: o estado do disco mais recente é o que conta)
        queued.set(true);
        // RECUPERAÇÃO: se finished() despachou entre o compareAndSet e o
        // set acima, o pedido teria ficado órfão (portão aberto + queued
        // a true sem ninguém para o acordar) — tenta acordar AGORA
        if (queued.compareAndSet(true, false)) {
            if (running.compareAndSet(false, true)) {
                safeRun();
                return true;
            }
            // outro chamador acordou primeiro — o pedido volta à fila
            queued.set(true);
        }
        return false;
    }

    /**
     * OBRIGATÓRIO no fim de CADA publicação (main thread): acorda o
     * trailing se houver pedidos agendados, ou abre o portão.
     */
    public void finished() {
        if (queued.compareAndSet(true, false)) {
            // há trabalho agendado — corre AGORA (o portão continua
            // fechado; esse run vai acabar noutro finished())
            safeRun();
        } else {
            running.set(false);
        }
    }

    /**
     * Emergência: o arranque nem chegou a ser agendado (exceção no
     * io.execute) — o portão abre SEM esperar por publicação nenhuma.
     */
    public void abandon() {
        queued.set(false);
        running.set(false);
    }

    public boolean isRunning() {
        return running.get();
    }

    public boolean hasQueued() {
        return queued.get();
    }

    /** o worker NUNCA pode deixar o portão fechado (tela presa p/ sempre) */
    private void safeRun() {
        try {
            worker.run();
        } catch (Throwable t) {
            running.set(false);
            queued.set(false);
            // re-lança: o chamador (a Activity) apanha TUDO e registra —
            // o portão já está aberto, a tela continua viva
            if (t instanceof RuntimeException) {
                throw (RuntimeException) t;
            }
            if (t instanceof Error) {
                throw (Error) t;
            }
            throw new IllegalStateException("worker do reload falhou", t);
        }
    }
}
