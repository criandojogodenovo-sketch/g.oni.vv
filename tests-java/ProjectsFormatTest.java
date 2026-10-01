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

        System.out.println(failures == 0
                ? "ProjectsFormatTest: OK"
                : "ProjectsFormatTest: " + failures + " falha(s)");
        if (failures > 0) {
            System.exit(1);
        }
    }
}
