# RELATÓRIO 0.9.1 — ORIENTAÇÃO + IME DO SISTEMA

**Repo:** github.com/criandojogodenovo-sketch/g.oni.vv · **Base:** 0.9.0 (f3e4f4a)
**Entrega:** 0.9.1 · versionCode 44

---

## 1. OBJETIVO

Janelas de texto pesado (editor de script 0.9.2 e futuros editores de código) abrem em **portrait** via JNI e usam o **IME do sistema** (texto/teclas por JNI → fila → engine), com o resize seguro pelo caminho contentRect/resize (o lifecycle fix cobre term/init). O teclado in-app mantém-se para renomear rápido em landscape. Zero features fora do escopo (CLÁUSULA CALMA).

## 2. FONTE DE VERDADE

Spec 0.9.1 do prompt da campanha (5 pontos + critérios de aceitação). Não há V.ONI nesta sub-fase.

## 3. IMPLEMENTAÇÃO

| Ponto | Onde | Essência |
|---|---|---|
| 1. Orientação | `platform/ImeQueue.{h,cpp}` + `StorageBridge.{h,cpp}` + `VvActivity.setOrientation` | O ESTADO vive na engine (`ime::setOrientation` — devolve true se mudou, LOGA "orientacao: portrait pedida (…)" a cada mudança; repetição não re-dispara JNI). O executor é a Java (`setRequestedOrientation` via `jniSetOrientation`, runOnUiThread). Abrir janela de texto = portrait; fechar = landscape (o par INSEPARÁVEL é afervado nos 2 lados: estado `ime::orientation()` + `void_calls` do fake JNI). |
| 2. IME | `VvActivity` (host + InputConnectionWrapper) + natives `nativeOnImeText`/`nativeOnImeKey` + fila `ime::` | EditText invisível 1x1 transparente criado no onCreate; o InputConnection encaminha commitText/deleteSurroundingText/sendKeyEvent(DEL 67, ENTER 66, DPAD)/performEditorAction para os natives (tabela RegisterNatives, 4→6) SEM acumular texto no host (buffer ÚNICO na engine). A fila (mutex, teto 256) é consumida por frame no thread da engine. SHOW/HIDE POR CONTA DA ENGINE (`jniImeShow`/`jniImeHide` → InputMethodManager). |
| 3. Resize seguro | lifecycle 0.6.7 (existente) + testes novos | A rotação passa pelo TERM_WINDOW/INIT_WINDOW reais: `applyContentRect` → rects corretos em 720×1536; re-upload de TUDO no contexto novo (a regressão "sem glifos brancos" = re-bake do atlas — aferida nos testes). A JANELA sobrevive (estado da engine, não da GPU) e o IME continua a escrever após rodar. |
| 4. Coexistência | gate no `textwin::applyEvent` + anyOverlayOpen | Teclado in-app (renomear/texto/alvo, landscape) intacto; eventos do IME com a janela fechada não vingam; com a janela aberta o gate modal cobre o editor (nada desenha/interage atrás). |
| 5. Janela de texto | `ui/TextWindow.{h,cpp}` + entrada em Settings→Diagnóstico | A SEMENTE do editor de script (0.9.2 traz coloração/Run/Stop/parse): full-screen, barra 56dp com back 56dp, título 20sp + hint 12sp, linhas com as MÉTRICAS REAIS da fonte, scroll SEGUE O FIM (setOffset APÓS beginScroll — o slot tem de existir), caret piscante 0.6s, DEL apaga 1 CODE POINT UTF-8, escrita append-only v0. `st.textWin` no `anyOverlayOpen`. |

**Decisões documentadas:** (a) a janela vive em Diagnóstico enquanto não há componente Script num TIC (0.9.2) — é o host honesto para o dono VERIFICAR o portrait+IME no C33; (b) abrir a janela FECHA o Settings (os dois backs são 56dp no mesmo canto — o modal único evita duplo-dispatch do mesmo toque); (c) fechar descarta o rascunho (0.9.2 persistirá no componente Script).

## 4. FIXES REAIS (apanhados pelos testes novos desta release)

1. **BOTÕES DA PÁGINA DE SETTINGS MORTOS (0.9.0):** dentro da região de scroll o `widgetHit` não captura (`scroll::buttonCaptures=false`) — o re-despacho do `scrollTap` só via os HEADERS de secção; "Ver logs/Export/Probe/reconverter/All Files/Mic/Repor layout/Imersivo" não acionavam no device. FIX: o walk do scrollTap espelha o draw LINHA A LINHA (rect do botão partilhado `actionBtnRect` — fonte única).
2. **COLISÃO de IDs:** a linha "reconverter" usa o literal 5829; `kTextWindowId` inicial também — IDs únicos por frame em immediate-mode → 5830.
3. **javac do CI:** `VvActivity.setImmersive` não pode override (`Activity.setImmersive(boolean)` é FINAL) → `setImmersiveMode` + GetMethodID a seguir; `VvProjects.Entry.uri` era `final` e a RECUPERAÇÃO de projetos re-aponta a pasta → deixa de ser final; faltava `import android.view.View`.

## 5. TESTES (suíte 667 → 681)

- **test_wiring091.cpp NOVO (14 casos):** fila ime:: (round-trip/FIFO/texto vazio/Key::None/mapa de keycodes/teto 256) · política de orientação (estado inicial, mudança, sem-spam na repetição) · textwin puro (open limpa, append, ENTER, UTF-8-DEL "caf\xC3\xA9"→"caf", setas ignoradas, fechada ignora) · draw portrait 720×1536 (back 56dp devolve 1, caret 0.6s, scroll ao fim com as métricas da fonte, lineCount) · gate modal · Settings: a linha devolve kOpenTextWindow (tap no BOTÃO, draw nos 2 frames do gesto).
- **test_wiring087.cpp +2 casos device:** (1) abrir → portrait+imeShow nos `void_calls` do fake JNI + linha de log; IME escreve "Ola\nmund!" pelos natives REAIS do bridge (ENTER 66 + DEL 67); frame com a janela desenha no stub; fechar → landscape+imeHide; IME com a janela fechada não vinga. (2) ROTAÇÃO: TERM/INIT com 1280×720→720×1536, janela ABERTA — superfície renasce portrait, lifecycle re-upa, buffer SOBREVIVE, IME continua, fechar repõe landscape.
- **c33_virtual FASE 7 NOVA (14 checks: 101→115):** o replay do device com a janela de texto (abrir/portrait/IME/rotação/fechar).
- **handshake:** contagens 4→6 natives.

## 6. HARNESS (C33 virtual)

115 checks VERDE (FASE 7 incluída; sentinelas R-001..R-004 verdes; gates de padrões proibidos verdes).

## 7. ITERAÇÕES DO LOOP

1. **it1** — compilação: `u32` sem Types.h no ImeQueue.h; natives IME sem declaração nos 2 TUs de teste → corrigidos.
2. **it2** — suíte: 10 falhas: (a) contagens do handshake (4→6); (b) a MINHA expectativa do UTF-8 tinha um 'e' fantasma ("café"→"caf", não "cafe"); (c) `scrollSetOffset` ANTES do `beginScroll` é no-op (o slot tem de existir) → movido para depois (sem lag de 1 frame); (d) o teste do tap na Settings apanhou os BOTÕES MORTOS (fix 4.1 acima) e precisava do draw nos 2 frames do gesto; (e) close() passa a limpar o buffer (semântica de rascunho documentada).
3. **it3** — 0 falhas na suíte + 115/115 no harness + checks da casa (check_main, link_parity 95 TUs, jni_parity 6 natives, projects_ui_check).

## 8. SENTINELAS

regress_selection_loss · regress_tmp_staging · regress_none_slot · regress_dump_identity — **TODAS VERDES**.

## 9. COMMITS

- `0.9.0-b` ICE do GCC 13.3 do runner REPRODUZIDO localmente (GCC extraído dos .debs) + paridade de link do c33_virtual (5 TUs da 0.9.0 faltavam no CMake).
- `0.9.0-c` import android.view.View (javac).
- `0.9.0-d` core/UndoStack.cpp em falta no CMake da APP (ld.lld undefined symbols).
- `0.9.1-a` a sub-fase inteira (orientação + IME + janela de texto + fixes 4.1–4.3 + testes).

## 10. CI / APK

versionCode 44 / 0.9.1; artifact `goni-vv-0.9.1-release-signed`. O run do CI é a evidência oficial (core-tests + c33-virtual 115 + APK assinado + verify-entry-symbols). A atualização deste relatório com o run/sha256 fecha a sub-fase.

## 11. NÃO VERIFICADO (honesto)

- **VERIFIED no C33 pelo dono:** pendente — checklist no README §0.9.1 (o device real é o juiz: acentos no teclado do sistema, GBoard com composição, rotação física do aparelho).
- **wiring010 500MB local:** falha ambiental de DISCO do sandbox (68 MB livres; o fixture é 523 MB) — passa no CI (runs 37114617999/37114804784/37115220295 core-tests verdes).
- **Composição de IME:** setComposingText não é encaminhado (só o commit) — teclados que escrevem por composição longa podem mostrar o inline vazio; isolado no InputConnectionWrapper da VvActivity para iterar sem tocar na engine.
- **Scroll manual na janela de texto:** v0 segue sempre o fim (o cursor editável + scroll livre são do 0.9.2).

## 12. DECLARAÇÃO DE AUSÊNCIA DE ACHISMO

Cada afirmação tem evidência: código nomeado, teste que cobre (14+2+14 novos), contagens (681 checks de suíte, 115 do harness), iterações do loop com os bugs que apanharam (incluindo o bug real da 0.9.0 que os testes novos apanharam). O não verificado está na secção 11.

## 13. LIÇÕES

1. Compilador do runner ≠ compilador local: o ICE do GCC 13.3 só se provou extraindo o GCC EXATO dos .debs do Ubuntu — ambiente de CI é reprodutível, vale o custo.
2. Immediate-mode: um botão dentro de scroll PRECISA de re-despacho por coordenadas — o teste do tap no botão certo é obrigatório (a 0.9.0 passou com botões mortos porque ninguém tocava neles na suíte).
3. IDs de widget são globais por frame — um literal (5829) colide silenciosamente; a auditoria de unicidade tem de cobrir literals.
4. O tap tem 2 frames (press+release) — o teste que só desenha num dos dois não testa nada.

## 14. PRÓXIMOS PASSOS

0.9.2 (V.ONI v0 core — parser PEG, linguagem fechada, editor de script com coloração, Docs) → 0.9.3 (Linkers & Tykers).

## 15. VEREDITO

A release 0.9.1 entrega a política de orientação, o IME do sistema com fila JNI e a janela de texto (semente do editor de script), com a suíte e o harness verdes e as sentinelas intactas. Aguarda o VERIFIED do dono no C33.
