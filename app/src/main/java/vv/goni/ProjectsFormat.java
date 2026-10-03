package vv.goni;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.Locale;

/**
 * 0.7.6 — formatação PURA da tela de projetos (host-testável no CI).
 *
 * Zero dependências de Android (nada de android.net.Uri — o CI compila e
 * corre ISTO na JVM do hospedeiro; a lógica de UI que toca no framework
 * fica nas activities). Duas funções:
 *
 *   • folderLabel(treeUri) — o rótulo VISÍVEL da pasta de um projeto. O URI
 *     SAF de árvore traz o ID interno do documento codificado no último
 *     segmento ("primary%3ADownload%2FGOne" → "primary:Download/GOne"): o
 *     prefixo "primary:" (e congéneres "home:", IDs de cartão SD) é RUÍDO
 *     INTERNO de armazenamento que o dono via na lista — aqui sai, e o
 *     percent-encoding é descodificado.
 *
 *   • dateLabel(millis) — "dd/MM/yyyy HH:mm" para a linha da lista (nome +
 *     data da última edição — nada mais, o URI deixou de ser mostrado).
 */
public final class ProjectsFormat {

    private ProjectsFormat() {
    }

    /**
     * Rótulo da pasta a partir do URI SAF de árvore.
     *   "content://com.android.externalstorage.documents/tree/primary%3ADownload%2FGOne"
     *     → "Download/GOne"
     *   "…/tree/primary%3AGOne" → "GOne"
     *   "…/tree/1234-ABCD%3AGOne" → "GOne"   (cartão SD: ID interno sai)
     *   "…/tree/GOne" → "GOne"               (sem prefixo: fica como está)
     * Entrada vazia/nula → "".
     */
    public static String folderLabel(String treeUri) {
        if (treeUri == null || treeUri.isEmpty()) {
            return "";
        }
        // último segmento do caminho (o ID do documento da árvore)
        int slash = treeUri.lastIndexOf('/');
        String tail = slash >= 0 ? treeUri.substring(slash + 1) : treeUri;
        if (tail.isEmpty()) {
            return "";
        }
        // descodifica %XX (o segmento vem URL-encoded)
        StringBuilder sb = new StringBuilder(tail.length());
        for (int i = 0; i < tail.length(); i++) {
            char c = tail.charAt(i);
            if (c == '%' && i + 2 < tail.length()) {
                int hi = Character.digit(tail.charAt(i + 1), 16);
                int lo = Character.digit(tail.charAt(i + 2), 16);
                if (hi >= 0 && lo >= 0) {
                    sb.append((char) ((hi << 4) | lo));
                    i += 2;
                    continue;
                }
            }
            sb.append(c);
        }
        String label = sb.toString();
        // remove o prefixo interno de armazenamento até ao primeiro ':'
        // ("primary:…", "home:…", "13A3-2C1D:…") — só quando o ':' vem
        // ANTES de qualquer '/' (caminho com ':' a meio fica intacto)
        int colon = label.indexOf(':');
        int slashInLabel = label.indexOf('/');
        if (colon >= 0 && (slashInLabel < 0 || colon < slashInLabel)) {
            label = label.substring(colon + 1);
        }
        return label;
    }

    /** "dd/MM/yyyy HH:mm" (0 → "") */
    public static String dateLabel(long millis) {
        if (millis <= 0L) {
            return "";
        }
        return new SimpleDateFormat("dd/MM/yyyy HH:mm", Locale.US)
                .format(new Date(millis));
    }

    // ---- 0.9.0 (spec F): tempo RELATIVO no card ("há 2 h") + chaves de
    // ORDENAÇÃO do dropdown [Ordenar: ▾] — PURAS, host-testáveis no CI.

    /**
     * Tempo relativo da última edição (spec F: "há 2 h" 12sp text-2).
     *   agora-separação → "agora" (<60s) · "há N min" (<60min) · "há N h"
     *   (<24h) · "há N d" (<7d) · "há N sem" (<5sem) · dateLabel (mais velho).
     * millis ≤ 0 → "" (o card mostra "—").
     */
    public static String relativeTime(long millis, long now) {
        if (millis <= 0L) {
            return "";
        }
        long diff = now - millis;
        if (diff < 0L) {
            diff = 0L;
        }
        final long MIN = 60_000L, H = 3_600_000L, D = 86_400_000L;
        if (diff < MIN) {
            return "agora";
        }
        if (diff < H) {
            return "há " + (diff / MIN) + " min";
        }
        if (diff < D) {
            return "há " + (diff / H) + " h";
        }
        if (diff < 7L * D) {
            return "há " + (diff / D) + " d";
        }
        if (diff < 35L * D) {
            return "há " + (diff / (7L * D)) + " sem";
        }
        return dateLabel(millis);
    }

    /** atalho: relativo ao AGORA do sistema */
    public static String relativeTime(long millis) {
        return relativeTime(millis, System.currentTimeMillis());
    }

    /**
     * chave de ordenação do dropdown (spec F: [Ordenar: Última Edição ▾]):
     *   0 = Última Edição (editedAt DESC — padrão)
     *   1 = Nome A-Z
     *   2 = Nome Z-A
     *   3 = Criado Mais Recente (createdAt DESC)
     * Devolve <0/0/>0 (java.util.Comparator semantics) — PURA p/ o CI.
     */
    public static int compareEntries(String nameA, long editedA, long createdA,
                                     String nameB, long editedB, long createdB,
                                     int sortMode) {
        switch (sortMode) {
            case 1:
                return nameA.compareToIgnoreCase(nameB);
            case 2:
                return nameB.compareToIgnoreCase(nameA);
            case 3: {
                long d = createdB - createdA;
                return d < 0 ? -1 : (d > 0 ? 1 : 0);
            }
            default: {   // 0 = Última Edição (DESC)
                long d = editedB - editedA;
                if (d != 0) {
                    return d < 0 ? -1 : 1;
                }
                return nameA.compareToIgnoreCase(nameB);   // desempate estável
            }
        }
    }

    /** rótulo do modo de ordenação (o texto do dropdown) */
    public static String sortLabel(int sortMode) {
        switch (sortMode) {
            case 1: return "Nome (A-Z)";
            case 2: return "Nome (Z-A)";
            case 3: return "Criado (recente)";
            default: return "Última Edição";
        }
    }
}
