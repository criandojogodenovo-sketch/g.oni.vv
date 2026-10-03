import vv.goni.ProjectsFormat;

/**
 * 0.7.6 — testes HOST da formatação da tela de projetos (javac + java no CI,
 * zero Android). Aferem a spec: o prefixo interno "primary:" NUNCA aparece no
 * texto visível; a data vem formatada.
 *
 * Correr: javac -d build-java \
 *            app/src/main/java/vv/goni/ProjectsFormat.java \
 *            tests-java/ProjectsFormatTest.java
 *         java -cp build-java ProjectsFormatTest
 */
public final class ProjectsFormatTest {

    private static int failures = 0;

    private static void check(boolean cond, String what) {
        if (!cond) {
            ++failures;
            System.out.println("  FALHOU  " + what);
        }
    }

    private static void checkEq(String got, String want, String what) {
        boolean ok = got.equals(want);
        if (!ok) {
            ++failures;
            System.out.println("  FALHOU  " + what + ": got=\"" + got
                    + "\" want=\"" + want + "\"");
        }
    }

    public static void main(String[] args) {
        // ---- folderLabel: o prefixo interno de armazenamento SAI ------------
        checkEq(ProjectsFormat.folderLabel(
                "content://com.android.externalstorage.documents/tree/primary%3ADownload%2FGOneVV"),
                "Download/GOneVV", "uri com primary: + caminho decodifica e perde o prefixo");
        checkEq(ProjectsFormat.folderLabel(
                "content://com.android.externalstorage.documents/tree/primary%3AMeuJogo"),
                "MeuJogo", "uri com primary: simples");
        checkEq(ProjectsFormat.folderLabel(
                "content://com.android.externalstorage.documents/tree/13A3-2C1D%3AJogoSD"),
                "JogoSD", "cartao SD (ID interno sai)");
        checkEq(ProjectsFormat.folderLabel(
                "content://com.android.externalstorage.documents/tree/GOne"),
                "GOne", "sem prefixo fica intacto");
        checkEq(ProjectsFormat.folderLabel(""), "", "vazio");
        checkEq(ProjectsFormat.folderLabel(null), "", "null");
        // NENHUM resultado contém o prefixo interno (a regra da spec)
        for (String uri : new String[]{
                "content://x/tree/primary%3AA",
                "content://x/tree/primary%3AA%2FB%2FC",
                "content://x/tree/home%3AA"}) {
            check(!ProjectsFormat.folderLabel(uri).startsWith("primary:")
                            && !ProjectsFormat.folderLabel(uri).startsWith("home:"),
                    "sem prefixo interno em " + uri);
        }

        // ---- dateLabel: sempre "dd/MM/yyyy HH:mm" (ou vazia) ---------------
        String d = ProjectsFormat.dateLabel(1748736000000L);   // 2025-06-01 00:00 UTC
        check(d.length() == 16 && d.charAt(2) == '/' && d.charAt(5) == '/'
                        && d.charAt(13) == ':',
                "formato dd/MM/yyyy HH:mm: \"" + d + "\"");
        checkEq(ProjectsFormat.dateLabel(0L), "", "epoch-0 devolve vazia");
        checkEq(ProjectsFormat.dateLabel(-5L), "", "negativo devolve vazia");
        // determinística: mesmo millis → mesmo rótulo
        check(ProjectsFormat.dateLabel(1748736000000L).equals(d), "deterministica");

        // ---- 0.9.0 — relativeTime (spec F: "há 2 h" no card) ----------------
        final long NOW = 1_800_000_000_000L;   // fixo: testes determinísticos
        checkEq(ProjectsFormat.relativeTime(NOW - 30_000L, NOW), "agora",
                "relativo: <60s = agora");
        checkEq(ProjectsFormat.relativeTime(NOW - 5 * 60_000L, NOW), "há 5 min",
                "relativo: minutos");
        checkEq(ProjectsFormat.relativeTime(NOW - 2 * 3_600_000L, NOW), "há 2 h",
                "relativo: horas (o exemplo da spec)");
        checkEq(ProjectsFormat.relativeTime(NOW - 3 * 86_400_000L, NOW), "há 3 d",
                "relativo: dias");
        checkEq(ProjectsFormat.relativeTime(NOW - 2 * 7 * 86_400_000L, NOW),
                "há 2 sem", "relativo: semanas");
        check(!ProjectsFormat.relativeTime(NOW - 40 * 7 * 86_400_000L, NOW)
                .startsWith("há "), "relativo: velho → data absoluta");
        checkEq(ProjectsFormat.relativeTime(NOW, NOW), "agora",
                "relativo: exatamente agora");
        checkEq(ProjectsFormat.relativeTime(NOW + 60_000L, NOW), "agora",
                "relativo: futuro clampado (nunca negativo)");
        checkEq(ProjectsFormat.relativeTime(0L, NOW), "", "relativo: epoch-0 vazia");
        checkEq(ProjectsFormat.relativeTime(-1L, NOW), "", "relativo: negativa vazia");

        // ---- 0.9.0 — compareEntries (dropdown Ordenar) -----------------------
        // 0 = Última Edição DESC (o padrão da spec)
        check(ProjectsFormat.compareEntries("a", 200L, 0L, "b", 100L, 0L, 0) < 0,
                "sort 0: editado mais recentemente PRIMEIRO");
        check(ProjectsFormat.compareEntries("a", 100L, 0L, "b", 100L, 0L, 0) < 0,
                "sort 0: desempate por nome (a < b)");
        // 1 = Nome A-Z
        check(ProjectsFormat.compareEntries("alpha", 0L, 0L, "Beta", 0L, 0L, 1) < 0,
                "sort 1: A-Z case-insensitive");
        // 2 = Nome Z-A
        check(ProjectsFormat.compareEntries("alpha", 0L, 0L, "Beta", 0L, 0L, 2) > 0,
                "sort 2: Z-A invertido");
        // 3 = Criado DESC
        check(ProjectsFormat.compareEntries("a", 0L, 50L, "b", 0L, 900L, 3) > 0,
                "sort 3: criado mais recente primeiro");

        // ---- 0.9.0 — sortLabel (o texto do dropdown) --------------------------
        checkEq(ProjectsFormat.sortLabel(0), "Última Edição", "label padrão");
        checkEq(ProjectsFormat.sortLabel(1), "Nome (A-Z)", "label A-Z");
        checkEq(ProjectsFormat.sortLabel(2), "Nome (Z-A)", "label Z-A");
        checkEq(ProjectsFormat.sortLabel(3), "Criado (recente)", "label criado");
        checkEq(ProjectsFormat.sortLabel(99), "Última Edição",
                "label desconhecido → padrão");

        System.out.println(failures == 0
                ? "ProjectsFormatTest: OK"
                : "ProjectsFormatTest: " + failures + " falha(s)");
        if (failures > 0) {
            System.exit(1);
        }
    }
}
